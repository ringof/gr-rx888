# gr-rx888 example flowgraphs

## `am_bcb_audio_demo.grc` — booth default (with audio)

Tunable AM Broadcast Band receiver. Drag the **Tune** slider across
540–1700 kHz, drag the **Volume** slider, hear the station. Waterfall
and PSD show what the tuner is on. AM BCB picks up reliably even on a
thumbtack antenna — this is the "always works" demo.

```
rx888.source (32 MS/s real)
  → float_to_complex (imag = 0)
  → freq_shift_cc (-center_freq, retunes live)
  → rational_resampler_ccc (decim=16) → 2 MS/s complex
  ├→ qtgui_waterfall_sink_c + qtgui_freq_sink_c
  └→ analog.am_demod_cf (audio_decim=40, 5 kHz LPF) → 50 kHz real
     → rational_resampler_fff (24/25) → 48 kHz
     → multiply_const_ff (volume slider)
     → audio.sink (ALSA default device)
```

Use this inside the Docker image via `docker run --device /dev/snd ... grc-demo`
(see `docker/README.md`). The `--device /dev/snd` flag is required for
the host's speakers to hear the audio — without it the flowgraph
still starts but `audio.sink` errors out.

## `am_bcb_demo.grc` — silent tuner variant

Same tuning + waterfall + PSD as the audio version, no audio output.
Use when running headless / on a host without ALSA / when you only
want the visualization.

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
