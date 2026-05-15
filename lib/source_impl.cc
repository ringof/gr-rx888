/* -*- c++ -*- */
/*
 * Copyright 2026 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "source_impl.h"

#include <gnuradio/io_signature.h>
#include <gnuradio/logger.h>

#include <algorithm>
#include <cmath>

namespace gr {
namespace rx888 {

using output_type = float;

namespace {

// dB → DAT-31 attenuator raw register code (0..63, half-dB steps).
// Clamp to the legal range; 31.5 dB max.
unsigned dB_to_dat31(double dB)
{
    double v = std::round(dB * 2.0);
    if (v < 0.0)  v = 0.0;
    if (v > 63.0) v = 63.0;
    return static_cast<unsigned>(v);
}

// dB → AD8370 (low/high range + 7-bit code). Returns {gain_code, gain_high}.
// Approximation: choose high range above ~7 dB; map dB to an approximate
// linear voltage ratio and project onto the device's roughly-linear gain
// code. Fine for v0.2; future librx888 will expose the device's own
// conversion via GET_CAPABILITIES.
struct ad8370_setting { unsigned code; int high; };
ad8370_setting dB_to_ad8370(double dB)
{
    static constexpr double VERNIER = 0.055744;
    static constexpr double PREGAIN = 7.079458;
    int high = (dB >= 7.0) ? 1 : 0;
    double linear = std::pow(10.0, dB / 20.0);
    double scale  = high ? (VERNIER * PREGAIN) : VERNIER;
    long code = std::lrint(linear / scale);
    if (code < 0)   code = 0;
    if (code > 127) code = 127;
    return { static_cast<unsigned>(code), high };
}

constexpr size_t RING_SAMPLES = 1u << 22;  // 4 Mi samples = 8 MiB

}  // namespace

source::sptr source::make(double samprate,
                          double atten,
                          double vga_gain,
                          bool dither,
                          bool rand,
                          double reference)
{
    return gnuradio::make_block_sptr<source_impl>(
        samprate, atten, vga_gain, dither, rand, reference);
}

source_impl::source_impl(double samprate,
                         double atten,
                         double vga_gain,
                         bool dither,
                         bool rand,
                         double reference)
    : gr::sync_block("rx888_source",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(1, 1, sizeof(output_type))),
      d_samprate(samprate),
      d_atten(atten),
      d_vga_gain(vga_gain),
      d_dither(dither),
      d_rand(rand),
      d_reference(reference),
      d_dev(nullptr),
      d_ring(RING_SAMPLES),
      d_write_idx(0),
      d_read_idx(0),
      d_ring_mask(RING_SAMPLES - 1),
      d_dropped_samples(0),
      d_stopping(false)
{
    if (d_reference != 27.0e6) {
        GR_LOG_WARN(d_logger,
            "rx888 firmware ignores the 'reference' parameter; ADC clock "
            "uses the on-board 27 MHz TCXO. Requested " +
            std::to_string(d_reference) + " Hz.");
    }
}

source_impl::~source_impl()
{
    if (d_dev) {
        rx888_close(d_dev);
        d_dev = nullptr;
    }
}

void source_impl::sample_cb(const int16_t* samples, std::size_t nsamples, void* user)
{
    auto* self = static_cast<source_impl*>(user);

    size_t w = self->d_write_idx.load(std::memory_order_relaxed);
    size_t r = self->d_read_idx.load(std::memory_order_acquire);
    size_t free_space = (self->d_ring.size() - 1) - ((w - r) & self->d_ring_mask);

    if (nsamples > free_space) {
        self->d_dropped_samples.fetch_add(nsamples - free_space,
                                          std::memory_order_relaxed);
        nsamples = free_space;
    }
    if (nsamples == 0) return;

    size_t first = std::min(nsamples, self->d_ring.size() - (w & self->d_ring_mask));
    std::copy_n(samples, first, self->d_ring.data() + (w & self->d_ring_mask));
    if (nsamples > first) {
        std::copy_n(samples + first, nsamples - first, self->d_ring.data());
    }
    self->d_write_idx.store(w + nsamples, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lk(self->d_cv_mu);
    }
    self->d_cv.notify_one();
}

bool source_impl::start()
{
    rx888_config_t cfg{};
    cfg.samplerate    = static_cast<unsigned>(d_samprate);
    cfg.att           = dB_to_dat31(d_atten);
    auto vga          = dB_to_ad8370(d_vga_gain);
    cfg.gain          = vga.code;
    cfg.gain_high     = vga.high;
    cfg.dither        = d_dither ? 1 : 0;
    cfg.randomizer    = d_rand   ? 1 : 0;
    cfg.fixup_samples = d_rand   ? 1 : 0;
    cfg.firmware_path = nullptr;  // assume device already loaded

    int rc = rx888_open(&d_dev, &cfg);
    if (rc != 0) {
        GR_LOG_ERROR(d_logger,
            std::string("rx888_open failed: ") + rx888_strerror(rc) +
            ". Is the device plugged into a SuperSpeed (blue) USB port "
            "and firmware loaded?");
        return false;
    }

    rc = rx888_start(d_dev, &source_impl::sample_cb, this);
    if (rc != 0) {
        GR_LOG_ERROR(d_logger,
            std::string("rx888_start failed: ") + rx888_strerror(rc));
        rx888_close(d_dev);
        d_dev = nullptr;
        return false;
    }

    GR_LOG_INFO(d_logger,
        "rx888 streaming at " + std::to_string(static_cast<long>(d_samprate)) +
        " Hz (atten=" + std::to_string(d_atten) +
        " dB, vga=" + std::to_string(d_vga_gain) +
        " dB, dither=" + (d_dither ? "on" : "off") +
        ", rand=" + (d_rand ? "on" : "off") + ")");
    return true;
}

bool source_impl::stop()
{
    d_stopping.store(true);
    d_cv.notify_all();
    if (d_dev) {
        rx888_stop(d_dev);
    }
    auto dropped = d_dropped_samples.load();
    if (dropped > 0) {
        GR_LOG_WARN(d_logger,
            "rx888 dropped " + std::to_string(dropped) +
            " samples due to host backpressure");
    }
    return true;
}

int source_impl::work(int noutput_items,
                      gr_vector_const_void_star& /*input_items*/,
                      gr_vector_void_star& output_items)
{
    auto* out = static_cast<output_type*>(output_items[0]);

    size_t r = d_read_idx.load(std::memory_order_relaxed);
    size_t w = d_write_idx.load(std::memory_order_acquire);
    size_t available = (w - r) & d_ring_mask;

    if (available == 0) {
        std::unique_lock<std::mutex> lk(d_cv_mu);
        d_cv.wait_for(lk, std::chrono::milliseconds(100), [this, &r] {
            return d_stopping.load() ||
                   d_write_idx.load(std::memory_order_acquire) != r;
        });
        if (d_stopping.load()) return WORK_DONE;
        w = d_write_idx.load(std::memory_order_acquire);
        available = (w - r) & d_ring_mask;
        if (available == 0) return 0;  // GR will retry
    }

    size_t to_copy = std::min<size_t>(available, static_cast<size_t>(noutput_items));
    constexpr float SCALE = 1.0f / 32768.0f;
    for (size_t i = 0; i < to_copy; i++) {
        out[i] = static_cast<float>(d_ring[(r + i) & d_ring_mask]) * SCALE;
    }
    d_read_idx.store(r + to_copy, std::memory_order_release);
    return static_cast<int>(to_copy);
}

}  // namespace rx888
}  // namespace gr
