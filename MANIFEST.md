title: gr-rx888 — RX888 mk II SDR Source
brief: GNU Radio source block for the RX888 mk II direct-sampling 16-bit / up-to-130 MSPS HF digitizer
tags:
  - sdr
  - hf
  - rx888
  - direct-sampling
  - librx888
author:
  - Dave Goncalves <davegoncalves@gmail.com>
copyright_owner:
  - Free Software Foundation, Inc.
license: GPL-3.0-or-later
gr_supported_version: v3.10
repo: https://github.com/ringof/gr-rx888
---
A GNU Radio out-of-tree module that exposes the RX888 mk II SDR as a
single `rx888.source` block via the `librx888` streaming library.
Streams real int16 ADC samples to float32 with int16/32768 scaling;
parameters cover sample rate, RF attenuator, AD8370 VGA, ADC dither,
output-randomization, and reference oscillator.

Pre-built example flowgraphs ship in `examples/` (AM Broadcast Band
receiver with audio out, full-spectrum diagnostic view), plus a
self-contained Docker image and a host preflight + smoketest script.
