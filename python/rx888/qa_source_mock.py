#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2026 Free Software Foundation, Inc.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# QA suite that exercises the rx888.source block against the in-tree
# mock librx888. Only meaningful when gr-rx888 is built with
# -DWITH_RX888_MOCK=ON (CMake registers this test in that mode only).
#
# Verifies:
#   - block constructs and start() succeeds (open + start are called)
#   - dB -> register conversions reach the wire correctly
#   - samples flow end-to-end through the SPSC ring to GR
#   - the int16 -> float32 / 32768 scaling produces the expected range
#   - rx888_open() failure causes start() to return false cleanly
#   - clean shutdown joins the mock's synthesis thread
#
# Introspection happens via ctypes against the same librx888.so the
# block linked against (the mock target is named rx888_mock but
# OUTPUT_NAME=rx888, so the .so is on disk as librx888.so).

import os
import ctypes
import time

from gnuradio import gr, gr_unittest, blocks
from gnuradio.rx888 import source


# ----------------------------- mock binding -------------------------------

def _load_mock():
    """Locate and dlopen the mock librx888 so we can introspect it.

    CMake passes the path through the LIBRX888_MOCK_SO env var when
    running QA; if missing, fall back to the linker's default resolution.
    """
    p = os.environ.get("LIBRX888_MOCK_SO")
    if p:
        return ctypes.CDLL(p)
    # Fall back: hope it's on LD_LIBRARY_PATH.
    return ctypes.CDLL("librx888.so")


class _MockConfig(ctypes.Structure):
    """Mirrors rx888_config_t from librx888.h."""
    _fields_ = [
        ("samplerate",    ctypes.c_uint),
        ("att",           ctypes.c_uint),
        ("gain",          ctypes.c_uint),
        ("gain_high",     ctypes.c_int),
        ("dither",        ctypes.c_int),
        ("randomizer",    ctypes.c_int),
        ("fixup_samples", ctypes.c_int),
        ("firmware_path", ctypes.c_char_p),
    ]


_mock = _load_mock()
_mock.mock_reset.restype                 = None
_mock.mock_set_open_should_fail.argtypes = [ctypes.c_int]
_mock.mock_set_open_should_fail.restype  = None
_mock.mock_get_last_open_config.restype  = ctypes.POINTER(_MockConfig)
_mock.mock_open_was_called.restype       = ctypes.c_int
_mock.mock_start_was_called.restype      = ctypes.c_int
_mock.mock_stop_was_called.restype       = ctypes.c_int
_mock.mock_close_was_called.restype      = ctypes.c_int
_mock.mock_samples_delivered.restype     = ctypes.c_ulonglong
_mock.mock_clean_shutdowns.restype       = ctypes.c_ulonglong
_mock.mock_set_tone_mode.argtypes        = [ctypes.c_int]
_mock.mock_set_tone_mode.restype         = None


# --------------------------------- tests ---------------------------------

class qa_source_mock(gr_unittest.TestCase):

    def setUp(self):
        _mock.mock_reset()
        self.tb = gr.top_block()

    def tearDown(self):
        self.tb = None
        _mock.mock_reset()

    # --- 1. wire-format / conversion correctness --------------------------

    def test_atten_dB_to_dat31_zero(self):
        src = source(samprate=32_000_000, atten=0.0)
        head = blocks.head(gr.sizeof_float, 1024)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        cfg = _mock.mock_get_last_open_config().contents
        self.assertEqual(cfg.att, 0)

    def test_atten_dB_to_dat31_ten(self):
        src = source(samprate=32_000_000, atten=10.0)
        head = blocks.head(gr.sizeof_float, 1024)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        cfg = _mock.mock_get_last_open_config().contents
        # 10 dB = 20 half-dB steps
        self.assertEqual(cfg.att, 20)

    def test_atten_dB_to_dat31_clamps_at_max(self):
        src = source(samprate=32_000_000, atten=50.0)  # over the 31.5 dB max
        head = blocks.head(gr.sizeof_float, 1024)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        cfg = _mock.mock_get_last_open_config().contents
        self.assertEqual(cfg.att, 63)  # 31.5 dB cap

    def test_vga_low_range_for_negative_dB(self):
        src = source(samprate=32_000_000, vga_gain=-5.0)
        head = blocks.head(gr.sizeof_float, 1024)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        cfg = _mock.mock_get_last_open_config().contents
        self.assertEqual(cfg.gain_high, 0)

    def test_vga_high_range_for_positive_dB(self):
        src = source(samprate=32_000_000, vga_gain=20.0)
        head = blocks.head(gr.sizeof_float, 1024)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        cfg = _mock.mock_get_last_open_config().contents
        self.assertEqual(cfg.gain_high, 1)
        self.assertGreater(cfg.gain, 0)
        self.assertLessEqual(cfg.gain, 127)

    def test_dither_flag_propagates(self):
        src = source(samprate=32_000_000, dither=True)
        head = blocks.head(gr.sizeof_float, 1024)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        cfg = _mock.mock_get_last_open_config().contents
        self.assertEqual(cfg.dither, 1)

    def test_rand_implies_fixup(self):
        src = source(samprate=32_000_000, rand=True)
        head = blocks.head(gr.sizeof_float, 1024)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        cfg = _mock.mock_get_last_open_config().contents
        self.assertEqual(cfg.randomizer, 1)
        self.assertEqual(cfg.fixup_samples, 1)

    def test_samprate_propagates(self):
        src = source(samprate=135_000_000)
        head = blocks.head(gr.sizeof_float, 1024)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        cfg = _mock.mock_get_last_open_config().contents
        self.assertEqual(cfg.samplerate, 135_000_000)

    # --- 2. lifecycle ------------------------------------------------------

    def test_full_lifecycle_called(self):
        src = source(samprate=32_000_000)
        head = blocks.head(gr.sizeof_float, 1024)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        # The block doesn't have to call close() during run — that
        # happens at destruction. But open + start + stop must all
        # have fired during the flowgraph lifecycle.
        self.assertTrue(_mock.mock_open_was_called())
        self.assertTrue(_mock.mock_start_was_called())
        self.assertTrue(_mock.mock_stop_was_called())
        self.assertGreaterEqual(_mock.mock_clean_shutdowns(), 1)

    def test_open_failure_propagates(self):
        _mock.mock_set_open_should_fail(1)
        src = source(samprate=32_000_000)
        head = blocks.head(gr.sizeof_float, 1024)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        # start() in the block returns false on rx888_open failure,
        # which propagates to GR as a failed start. The flowgraph
        # should NOT hang — it should exit quickly.
        t0 = time.monotonic()
        try:
            self.tb.run()
        except RuntimeError:
            # GR may raise on failed start — acceptable
            pass
        elapsed = time.monotonic() - t0
        self.assertLess(elapsed, 5.0, "flowgraph hung after rx888_open failure")
        # No samples should have flowed
        self.assertEqual(len(sink.data()), 0)

    # --- 3. sample flow ----------------------------------------------------

    def test_samples_flow_with_zeros(self):
        # Mock default mode is zero samples; verify the head block
        # still drains the correct count and the SPSC ring works.
        src = source(samprate=32_000_000)
        head = blocks.head(gr.sizeof_float, 8192)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        out = list(sink.data())
        self.assertEqual(len(out), 8192)
        self.assertTrue(all(v == 0.0 for v in out))
        self.assertGreaterEqual(_mock.mock_samples_delivered(), 8192)

    def test_samples_flow_with_tone(self):
        # Tone mode: mock generates a 1 MHz sine at amplitude 16000.
        # After int16/32768 scaling in the block, peaks should be near
        # 16000/32768 ≈ 0.488.
        _mock.mock_set_tone_mode(1)
        src = source(samprate=32_000_000)
        head = blocks.head(gr.sizeof_float, 32768)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        out = list(sink.data())
        self.assertEqual(len(out), 32768)
        peak = max(abs(v) for v in out)
        self.assertGreater(peak, 0.4, f"tone peak too low: {peak}")
        self.assertLess(peak, 0.6, f"tone peak too high (scaling bug?): {peak}")
        # Mean of a sine over many cycles should be near zero.
        mean = sum(out) / len(out)
        self.assertLess(abs(mean), 0.05, f"tone mean too far from 0: {mean}")


if __name__ == "__main__":
    gr_unittest.run(qa_source_mock)
