/* -*- c++ -*- */
/*
 * Copyright 2026 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RX888_SOURCE_H
#define INCLUDED_RX888_SOURCE_H

#include <gnuradio/rx888/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
namespace rx888 {

/*!
 * \brief RX888mk2 streaming source (HF direct-sampling, 16-bit).
 * \ingroup rx888
 *
 * Wraps librx888's transfer pipeline as a GNU Radio source block.
 * Emits float32 samples; raw int16 from the ADC are scaled to
 * [-1.0, 1.0) and un-randomized when \p rand is true.
 *
 * Stream tags on output 0: rx_rate, rx_atten, rx_vga_gain, rx_time.
 * Async message port "status": usb_overflow, host_overflow,
 * device_gone, firmware_version, clock_locked.
 *
 * v0.1 ships without an active librx888 backend; the block emits
 * silence so flowgraphs validate. The librx888 link is gated by the
 * top-level CMake option WITH_RX888.
 */
class RX888_API source : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<source> sptr;

    /*!
     * \brief Construct an rx888 source.
     *
     * \param samprate  ADC sample rate in Hz (e.g. 64.8e6, 130e6).
     * \param atten     RF attenuator setting in dB (front-end step attenuator).
     * \param vga_gain  VGA gain in dB.
     * \param dither    Enable ADC dither.
     * \param rand      Enable ADC output-randomization decode (un-randomize).
     * \param reference Reference oscillator frequency in Hz (Si5351 input).
     */
    static sptr make(double samprate = 64.8e6,
                     double atten = 0.0,
                     double vga_gain = 0.0,
                     bool dither = false,
                     bool rand = true,
                     double reference = 27.0e6);
};

} // namespace rx888
} // namespace gr

#endif /* INCLUDED_RX888_SOURCE_H */
