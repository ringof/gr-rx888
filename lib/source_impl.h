/* -*- c++ -*- */
/*
 * Copyright 2026 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RX888_SOURCE_IMPL_H
#define INCLUDED_RX888_SOURCE_IMPL_H

#include <gnuradio/rx888/source.h>

namespace gr {
namespace rx888 {

class source_impl : public source
{
private:
    double d_samprate;
    double d_atten;
    double d_vga_gain;
    bool d_dither;
    bool d_rand;
    double d_reference;

public:
    source_impl(double samprate,
                double atten,
                double vga_gain,
                bool dither,
                bool rand,
                double reference);
    ~source_impl();

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);
};

} // namespace rx888
} // namespace gr

#endif /* INCLUDED_RX888_SOURCE_IMPL_H */
