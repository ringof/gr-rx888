# gr-rx888 example flowgraphs

## `am_bcb_demo.grc` — booth default

Tunable AM Broadcast Band (540–1700 kHz) waterfall. A slider sets the
center frequency live; the flowgraph downconverts a 32 MS/s real ADC
stream to a 2 MS/s complex baseband centered on the slider value and
displays a waterfall + PSD.

Why this is the booth-default: AM BCB picks up reliably even on a
thumbtack antenna. The demo "always works."

```
rx888.source (32 MS/s real)
  → float_to_complex (imag = 0)
  → freq_shift_cc (-center_freq, retunes live)
  → rational_resampler_ccc (decim=16, internally-generated LPF)
  → qtgui_waterfall_sink_c + qtgui_freq_sink_c (±1 MHz around center)
```

Use this one inside the Docker image via `docker run ... gr-rx888 grc-demo`.

## `hf_waterfall_demo.grc` — full-spectrum diagnostic

The whole 0–16 MHz Nyquist span at 32 MS/s real. No tuning, no
decimation — useful for spotting the DC spike, image responses,
spurious lines, and verifying the hardware is generally alive.
Open from GRC manually:

```sh
gnuradio-companion /opt/gr-rx888/examples/hf_waterfall_demo.grc
```

## `halfband_coefs_fixed.csv` — pre-designed half-band taps

A ~493-tap linear-phase half-band low-pass filter (half the taps are
zero by construction, center tap = 0.5, cutoff at `fs/4`). Drop into
a GRC `variable_file_filter_taps` block when you want to cascade
cheap `decim=2` half-band stages — much cheaper at high sample rates
than a single fat `rational_resampler` with auto-generated taps.

Typical use (e.g. 32 MS/s → 2 MS/s in four halfband decim=2 stages):

```
... → freq_shift_cc → rational_resampler_ccc[decim=2, taps=halfband]
              → rational_resampler_ccc[decim=2, taps=halfband]
              → rational_resampler_ccc[decim=2, taps=halfband]
              → rational_resampler_ccc[decim=2, taps=halfband]
              → sink
```

Worth swapping in when running at 135 MS/s where the single-stage
decim is a CPU pig; not necessary for the 32 MS/s booth demo.
