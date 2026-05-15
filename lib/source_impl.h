/* -*- c++ -*- */
/*
 * Copyright 2026 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RX888_SOURCE_IMPL_H
#define INCLUDED_RX888_SOURCE_IMPL_H

#include <gnuradio/rx888/source.h>

#include <librx888.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

namespace gr {
namespace rx888 {

class source_impl : public source
{
private:
    double d_samprate;
    double d_atten;
    double d_vga_gain;
    bool   d_dither;
    bool   d_rand;
    double d_reference;

    rx888_t* d_dev;

    // Single-producer / single-consumer ring of int16 samples.
    // Writer: librx888 writer thread (sample_cb).
    // Reader: GR worker thread (work()).
    std::vector<int16_t> d_ring;
    std::atomic<size_t>  d_write_idx;
    std::atomic<size_t>  d_read_idx;
    size_t               d_ring_mask;  // d_ring.size() - 1, size is power of two
    std::atomic<uint64_t> d_dropped_samples;

    std::mutex                d_cv_mu;
    std::condition_variable   d_cv;
    std::atomic<bool>         d_stopping;

    static void sample_cb(const int16_t* samples, std::size_t nsamples, void* user);

public:
    source_impl(double samprate,
                double atten,
                double vga_gain,
                bool dither,
                bool rand,
                double reference);
    ~source_impl();

    bool start() override;
    bool stop()  override;

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items) override;
};

} // namespace rx888
} // namespace gr

#endif /* INCLUDED_RX888_SOURCE_IMPL_H */
