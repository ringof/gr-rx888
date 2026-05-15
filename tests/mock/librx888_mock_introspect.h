/*
 * librx888 mock — test-only introspection header.
 *
 * The mock librx888 satisfies the public ABI declared in librx888.h
 * exactly, so the gr-rx888 source block links and runs against it
 * unmodified. This header exposes the *additional* test-only symbols
 * that QA can call to inspect / control what the mock did.
 *
 * Linked in the same .so as the public ABI, so ctypes can dlopen one
 * library and see both surfaces.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBRX888_MOCK_INTROSPECT_H
#define LIBRX888_MOCK_INTROSPECT_H

#include "librx888.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reset all introspection state. Call between tests. */
void mock_reset(void);

/* Make the next rx888_open() return LIBUSB_ERROR_NO_DEVICE (-4).
 * Useful for testing the block's start() error path. Cleared by mock_reset(). */
void mock_set_open_should_fail(int yes);

/* Read the rx888_config_t that was last passed to rx888_open().
 * Returns a pointer to mock-internal storage. NULL if no open seen yet. */
const rx888_config_t *mock_get_last_open_config(void);

/* Was rx888_open() called since the last mock_reset()? */
int mock_open_was_called(void);

/* Was rx888_start() called since the last mock_reset()? */
int mock_start_was_called(void);

/* Was rx888_stop() called since the last mock_reset()? */
int mock_stop_was_called(void);

/* Was rx888_close() called since the last mock_reset()? */
int mock_close_was_called(void);

/* Total int16 samples delivered to the user callback since open().
 * Atomic; safe to read while streaming. */
uint64_t mock_samples_delivered(void);

/* Total times rx888_start() spawned a thread that successfully joined
 * (i.e. clean shutdown). Atomic. */
uint64_t mock_clean_shutdowns(void);

/* Force the mock to deliver a synthetic tone instead of zeros. The
 * sine wave is computed at the configured samplerate; useful for
 * verifying the block's int16->float scaling and stream integrity.
 * Default: OFF (deliver zero buffers). */
void mock_set_tone_mode(int on);

#ifdef __cplusplus
}
#endif

#endif /* LIBRX888_MOCK_INTROSPECT_H */
