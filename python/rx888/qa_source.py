#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2026 Free Software Foundation, Inc.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

from gnuradio import gr, gr_unittest, blocks
from gnuradio.rx888 import source


class qa_source(gr_unittest.TestCase):

    def setUp(self):
        self.tb = gr.top_block()

    def tearDown(self):
        self.tb = None

    def test_instance_defaults(self):
        src = source()
        self.assertIsNotNone(src)

    def test_instance_kwargs(self):
        src = source(samprate=130e6,
                     atten=10.0,
                     vga_gain=6.0,
                     dither=True,
                     rand=False,
                     reference=27e6)
        self.assertIsNotNone(src)

    def test_emits_zeros_in_v0(self):
        # v0.1: no librx888 backend; block emits silence so flowgraphs validate.
        src = source(samprate=64.8e6)
        head = blocks.head(gr.sizeof_float, 1024)
        sink = blocks.vector_sink_f()
        self.tb.connect(src, head, sink)
        self.tb.run()
        out = sink.data()
        self.assertEqual(len(out), 1024)
        self.assertTrue(all(s == 0.0 for s in out))


if __name__ == '__main__':
    gr_unittest.run(qa_source)
