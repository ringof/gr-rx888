# Plan: librx888 + gr-rx888

A pair of minimal, composable artifacts that let GNU Radio (and any
other consumer with a C linker) use the RX888mk2 as a 16-bit, 130 MSPS
direct-sampling digitizer with an HF-tuned analog front end.
Independent of ka9q-radio's `rx888.so`, with technical distance kept
small enough that any future merge stays trivial.

## Goals

- **One thing, well.** A clean streaming digitizer in user space. Not
  an SDR receiver, not a multichannel radio, not an AGC-controlled HF
  system — a *fast ADC with a front end*, primitive enough that the
  application composes whatever it wants on top.
- **Recruit users from outside SDR.** Low-field NMR, atmospheric
  science, radio astronomy, time/frequency metrology — applications
  that currently roll their own digitizer integration. "Simple but
  not hard" is the contract.
- **Mirror, not adopt.** Independent of ka9q-radio at the code level,
  but with config-key vocabulary, defaults, and behavior parity
  wherever there's no positive reason to differ. No collaboration
  dependency.
- **Dovetail with existing work.** This firmware (USB vendor-command
  API is the contract surface), rx888-tools' `rx888_stream` (lineage
  source for the streaming pipeline), ka9q-radio's `rx888.c`
  (reference for *what* to do, not *how*).

## Non-goals

- No first-party SoapySDR plugin. Soapy-rx888 can sit on top of
  librx888 later if anyone wants it; not on the critical path.
- No AGC, no DC blocker, no fancy DSP inside the library or the GR
  block. Those compose downstream.
- No VHF tuner. R828D detected but not driven. Matches firmware posture.
- No solver beyond what the firmware already runs (~75 lines,
  ik1xpv lineage). Phil's 400-line AN619 solver stays off the
  critical path; future opt-in only.
- No fork or upstream-PR work against ExtIO_sddc or libsddc.

## Architecture

```
USB FX3 firmware  (this repo)
        │  USB vendor commands (stable contract)
        ▼
librx888  (rx888-tools repo, new library)
        │  C callback API
        ▼
gr-rx888  (new repo, GNU Radio OOT module)
        │  Source block: int16 → float32, stream tags, status msgs
        ▼
any GNU Radio flowgraph
```

`librx888` exposes vendor commands as typed C functions, owns the
libusb async transfer pipeline, hands raw int16 sample buffers up via
a registered callback. Only thread visible to the consumer is the
streaming thread, which librx888 starts and joins.

`gr-rx888` is a stock GR OOT module: one Source block, an SPSC ring
fed by the librx888 callback, int16→float32 conversion + un-randomize,
PMT stream tags (`rx_rate`, `rx_atten`, `rx_vga_gain`, `rx_time`),
async `status` message port (`usb_overflow`, `host_overflow`,
`device_gone`, `firmware_version`, `clock_locked`).

## Scope sizing

- **librx888**: ~1300 lines of C. Smaller than `rx888.c + ezusb.c`
  combined (~2500) because AGC, Phil's solver, ka9q config parsing,
  FX2 firmware upload, and VHF stubs are all dropped.
- **gr-rx888**: ~700 lines of C++/Python/CMake/GRC boilerplate.
  Standard GR OOT module shape.
- **Examples**: `hf_receiver_demo.grc`, `raw_recorder.grc`,
  `spectrogram_demo.grc`. Deliberately span radio, science,
  visualization.

Effort: weekend-and-some-weeks for librx888; similar for gr-rx888
with examples and docs. Not a life's work.

## Dovetail with existing work (no collaboration required)

| Source | What we take | What we don't |
|---|---|---|
| RX888 firmware (this repo) | Vendor command API, Si5351 simple solver (~75 lines, ik1xpv lineage), `TESTFX3` for firmware version readback | Firmware-side adds (e.g., new READ vendor command for PLL lock) ride normal release cadence |
| `rx888-tools` / `rx888_stream` | Streaming-pipeline code as the basis for librx888's transfer thread; udev rules; release-asset workflow | The CLI stays as a separate `rx888-tools` deliverable; library factored out beneath it |
| ka9q-radio's `rx888.c` (reference) | Config-key naming (`samprate`, `atten`, `dither`, `rand`, `reference`, etc.), default values (27 MHz ref, 64.8 MSPS), `usb_speeds[]` table | AGC thread, 400-line solver, ka9q `frontend`-shaped scaffolding, VHF stubs |
| SoapySDR / libsddc | Nothing | No active dependency, no fork. Coexist via the Spectre validation harness (PR #124) |

Every column above: code from this repo or its sibling, code lifted
with GPLv3-clean attribution, or behavioral parity that requires no
commits anywhere except ours.

## Sequencing

1. Land `librx888` in `rx888-tools` as a library extracted beneath
   `rx888_stream`. CLI keeps working; library is the new public artifact.
2. Spin up `gr-rx888` as a new repo following GR OOT conventions
   (`ringof/gr-rx888`). Three example flowgraphs ship with v0.1.
3. Documentation framed around capabilities (the digitizer), not
   applications (the SDR).
4. Validation against this firmware: the existing Spectre Docker
   harness (PR #124) on the SoapySDR seam, *plus* a direct test using
   gr-rx888 itself once it exists.

## Out of scope for v1

Revisit if demand surfaces:

- Soapy-rx888 shim on top of librx888.
- Arbitrary-rate solver beyond the firmware-lineage simple one.
- External-trigger / sub-microsecond timestamping (depends on
  firmware/hardware revision).
- VHF tuner support.
