/* -*- c++ -*- */
/*
 * Copyright 2026 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "source_impl.h"
#include <gnuradio/io_signature.h>

#include <algorithm>

namespace gr {
namespace rx888 {

using output_type = float;

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
      d_reference(reference)
{
}

source_impl::~source_impl() {}

int source_impl::work(int noutput_items,
                      gr_vector_const_void_star& /*input_items*/,
                      gr_vector_void_star& output_items)
{
    auto out = static_cast<output_type*>(output_items[0]);
    std::fill_n(out, noutput_items, 0.0f);
    return noutput_items;
}

} /* namespace rx888 */
} /* namespace gr */
