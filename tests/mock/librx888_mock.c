/*
 * librx888 mock — implements the public ABI from librx888.h plus a
 * small introspection surface (librx888_mock_introspect.h) for tests.
 *
 * No libusb, no threads doing real I/O. A single synthesis thread per
 * open handle generates int16 samples and invokes the user callback
 * at roughly the configured samplerate (or unpaced, see below).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _GNU_SOURCE
#include "librx888.h"
#include "librx888_mock_introspect.h"

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* libusb error codes the mock reports without depending on libusb */
#define MOCK_OK                 0
#define MOCK_ERR_NO_DEVICE     (-4)
#define MOCK_ERR_INVALID_PARAM (-2)

/* --------------------------- introspection state ----------------------- */

/* Single-instance: we don't expect concurrent open() in QA. */
static atomic_int      g_open_should_fail = 0;
static atomic_int      g_open_called      = 0;
static atomic_int      g_start_called     = 0;
static atomic_int      g_stop_called      = 0;
static atomic_int      g_close_called     = 0;
static atomic_int      g_tone_mode        = 0;
static atomic_ullong   g_samples_delivered = 0;
static atomic_ullong   g_clean_shutdowns   = 0;
/* Stats counters mirrored across handles; QA cares about totals, not
 * per-handle attribution (there's only ever one mock handle in flight). */
static atomic_ullong   g_ok_xfers          = 0;
static atomic_ullong   g_bad_xfers         = 0;
static atomic_ullong   g_bytes_out         = 0;

/* Last-open config: protected by g_cfg_mu for the writer; readers are
 * the test thread post-open, no concurrent writes once visible. */
static pthread_mutex_t g_cfg_mu = PTHREAD_MUTEX_INITIALIZER;
static rx888_config_t  g_last_cfg;
static int             g_cfg_valid = 0;

void mock_reset(void)
{
    atomic_store(&g_open_should_fail,  0);
    atomic_store(&g_open_called,       0);
    atomic_store(&g_start_called,      0);
    atomic_store(&g_stop_called,       0);
    atomic_store(&g_close_called,      0);
    atomic_store(&g_tone_mode,         0);
    atomic_store(&g_samples_delivered, 0);
    atomic_store(&g_clean_shutdowns,   0);
    pthread_mutex_lock(&g_cfg_mu);
    memset(&g_last_cfg, 0, sizeof(g_last_cfg));
    g_cfg_valid = 0;
    pthread_mutex_unlock(&g_cfg_mu);
}

void mock_set_open_should_fail(int yes) { atomic_store(&g_open_should_fail, yes ? 1 : 0); }
void mock_set_tone_mode(int on)         { atomic_store(&g_tone_mode, on ? 1 : 0); }
int  mock_open_was_called(void)         { return atomic_load(&g_open_called); }
int  mock_start_was_called(void)        { return atomic_load(&g_start_called); }
int  mock_stop_was_called(void)         { return atomic_load(&g_stop_called); }
int  mock_close_was_called(void)        { return atomic_load(&g_close_called); }
uint64_t mock_samples_delivered(void)   { return atomic_load(&g_samples_delivered); }
uint64_t mock_clean_shutdowns(void)     { return atomic_load(&g_clean_shutdowns); }

const rx888_config_t *mock_get_last_open_config(void)
{
    /* Single writer-then-stable-readers semantics. The mutex serialises
     * the open(); after that, reads are stable until next reset/open. */
    pthread_mutex_lock(&g_cfg_mu);
    const rx888_config_t *p = g_cfg_valid ? &g_last_cfg : NULL;
    pthread_mutex_unlock(&g_cfg_mu);
    return p;
}

/* ---------------------- mock handle + synthesis thread ----------------- */

struct rx888 {
    rx888_config_t    cfg;
    rx888_sample_cb_t cb;
    void             *user;
    pthread_t         thread;
    atomic_int        stop;
    atomic_int        started;
    atomic_int        running;  /* exported via rx888_is_running */
};

#define MOCK_CHUNK 65536  /* int16 samples per callback */

static void *gen_thread(void *arg)
{
    struct rx888 *r = arg;
    int16_t *buf = calloc(MOCK_CHUNK, sizeof(int16_t));
    if (!buf) return NULL;

    double phase = 0.0;
    /* Hard-code a 1 MHz tone at the configured rate. Phase wraps cleanly. */
    double samprate = (r->cfg.samplerate > 0) ? (double)r->cfg.samplerate : 32e6;
    double dphi = 2.0 * M_PI * 1.0e6 / samprate;

    /* Pace at roughly the configured samplerate so timing-sensitive
     * tests get a realistic stream. We don't try to be precise — close
     * enough that a 1-second flowgraph receives ~samprate samples. */
    long usec_per_chunk = (long)(1e6 * (double)MOCK_CHUNK / samprate);
    if (usec_per_chunk < 100) usec_per_chunk = 100;

    while (!atomic_load(&r->stop)) {
        if (atomic_load(&g_tone_mode)) {
            for (size_t i = 0; i < MOCK_CHUNK; i++) {
                buf[i] = (int16_t)(16000.0 * sin(phase));
                phase += dphi;
                if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
            }
        } else {
            memset(buf, 0, MOCK_CHUNK * sizeof(int16_t));
        }

        if (r->cb) {
            r->cb(buf, MOCK_CHUNK, r->user);
            atomic_fetch_add(&g_samples_delivered, MOCK_CHUNK);
            atomic_fetch_add(&g_ok_xfers, 1);
            atomic_fetch_add(&g_bytes_out, MOCK_CHUNK * sizeof(int16_t));
        }
        usleep(usec_per_chunk);
    }

    free(buf);
    return NULL;
}

/* ----------------------------- public ABI ------------------------------ */

void rx888_config_init_default(rx888_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->samplerate        = 32000000u;
    cfg->gain_high         = 1;
    cfg->queue_depth       = 32;
    cfg->req_packets       = 1024;
    cfg->ctrl_timeout_ms   = 5000;
    cfg->stream_timeout_ms = 0;
    cfg->watchdog_ms       = 3000;
}

int rx888_open(rx888_t **out, const rx888_config_t *cfg)
{
    if (!out || !cfg) return MOCK_ERR_INVALID_PARAM;

    atomic_fetch_add(&g_open_called, 1);

    /* Mirror real librx888's rejection of zeroed tuning knobs (this is
     * what catches callers that forgot to call rx888_config_init_default). */
    if (cfg->queue_depth == 0 || cfg->req_packets == 0 ||
        cfg->ctrl_timeout_ms == 0) {
        *out = NULL;
        return MOCK_ERR_INVALID_PARAM;
    }

    if (atomic_load(&g_open_should_fail)) {
        *out = NULL;
        return MOCK_ERR_NO_DEVICE;
    }

    pthread_mutex_lock(&g_cfg_mu);
    g_last_cfg = *cfg;
    g_cfg_valid = 1;
    pthread_mutex_unlock(&g_cfg_mu);

    rx888_t *r = calloc(1, sizeof(*r));
    if (!r) return MOCK_ERR_INVALID_PARAM;
    r->cfg = *cfg;
    atomic_store(&r->running, 1);
    *out = r;
    return MOCK_OK;
}

int rx888_start(rx888_t *r, rx888_sample_cb_t cb, void *user)
{
    if (!r) return MOCK_ERR_INVALID_PARAM;
    atomic_fetch_add(&g_start_called, 1);

    r->cb = cb;
    r->user = user;
    atomic_store(&r->stop, 0);

    if (pthread_create(&r->thread, NULL, gen_thread, r) != 0) {
        return MOCK_ERR_INVALID_PARAM;
    }
    atomic_store(&r->started, 1);
    return MOCK_OK;
}

void rx888_stop(rx888_t *r)
{
    if (!r) return;
    atomic_fetch_add(&g_stop_called, 1);

    if (atomic_load(&r->started)) {
        atomic_store(&r->stop, 1);
        pthread_join(r->thread, NULL);
        atomic_store(&r->started, 0);
        atomic_store(&r->running, 0);
        atomic_fetch_add(&g_clean_shutdowns, 1);
    }
}

void rx888_close(rx888_t *r)
{
    if (!r) return;
    atomic_fetch_add(&g_close_called, 1);
    rx888_stop(r);
    free(r);
}

const char *rx888_strerror(int err)
{
    switch (err) {
        case MOCK_OK:                 return "Success";
        case MOCK_ERR_NO_DEVICE:      return "No device (mock)";
        case MOCK_ERR_INVALID_PARAM:  return "Invalid parameter (mock)";
        default:                      return "Unknown (mock)";
    }
}

const char *rx888_version(void) { return "0.0.0-mock"; }

void rx888_get_stats(const rx888_t *r, rx888_stats_t *out)
{
    if (!out) return;
    out->ok_xfers   = atomic_load(&g_ok_xfers);
    out->bad_xfers  = atomic_load(&g_bad_xfers);
    out->bytes_out  = atomic_load(&g_bytes_out);
    out->in_flight  = r && atomic_load(&((rx888_t *)r)->started) ? 1 : 0;
    out->last_cb_ms = 0;
}

int rx888_is_running(const rx888_t *r)
{
    if (!r) return 1;  /* docstring: 1 before start() and while running */
    return atomic_load(&((rx888_t *)r)->running);
}
