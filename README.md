# gr-rx888

GNU Radio out-of-tree module for the **RX888 mk2** direct-sampling
digitizer — a 16-bit, up-to-130 MSPS ADC with an HF-tuned analog
front end, exposed as a single GR source block.

v0.2 — the block streams real samples from real hardware via
[`librx888`](https://github.com/ringof/rx888-tools). See
[`docs/PLAN.md`](docs/PLAN.md) for the design rationale, scope, and
how this module relates to ka9q-radio, rx888-tools, and the FX3
firmware.

## Block

`rx888.source` — one source block, float32 output.

| Parameter   | Type | Default | Meaning                                |
|-------------|------|---------|----------------------------------------|
| `samprate`  | real | 32e6    | ADC sample rate (Hz); firmware-supported: 32e6, 135e6 |
| `atten`     | real | 0       | RF step attenuator (dB), 0–31.5 in 0.5 dB steps       |
| `vga_gain`  | real | 0       | VGA gain (dB), AD8370 (auto low/high range)           |
| `dither`    | bool | False   | ADC dither                             |
| `rand`      | bool | True    | ADC output-randomization decode        |
| `reference` | real | 27e6    | Reference oscillator (Hz, advisory)    |

Planned for a follow-up release: stream tags (`rx_rate`, `rx_atten`,
`rx_vga_gain`, `rx_time`) and an async `status` message port
(`usb_overflow`, `host_overflow`, `device_gone`, `firmware_version`,
`clock_locked`). The block currently exposes the configuration knobs
above but does not yet emit those metadata channels.

## Build

Requires GNU Radio 3.10 (`gnuradio-dev` on Debian/Ubuntu), pybind11,
and [`librx888`](https://github.com/ringof/rx888-tools) installed
system-wide.

### Install librx888 first

```sh
sudo apt install -y libusb-1.0-0-dev pkg-config build-essential
git clone https://github.com/ringof/rx888-tools.git
cd rx888-tools
make firmware         # fetch the pinned FX3 firmware release
make
sudo make install
sudo ldconfig
pkg-config --modversion librx888   # sanity check
```

### Then build gr-rx888

```sh
mkdir build && cd build
cmake ..
make -j
ctest
sudo make install
sudo ldconfig
```

### Skip the host setup: use the Docker image

If your laptop has the wrong GR version or you want a known-good
build to ship on a USB stick for the booth, there's a self-contained
container in [`docker/`](docker/) that bundles librx888 + GR 3.10 +
this module + the helper scripts. One command runs the CLI smoke test:

```sh
docker/build.sh
docker run --rm -it --privileged -v /dev/bus/usb:/dev/bus/usb gr-rx888:latest
```

The `grc-demo` entrypoint launches the AM BCB receiver flowgraph
(audio out via host PulseAudio/PipeWire) with X11 forwarding. See
[`docker/README.md`](docker/README.md) for the full run line
(audio + X11 + USB), plus the host-side prep (`usbfs_memory_mb`,
udev rules) the container can't do for you.

## At the bench

Before the first time you wire up a flowgraph, run the preflight +
smoke test in [`scripts/`](scripts/) to confirm the device, USB3 link,
firmware load, `usbfs_memory_mb`, and udev permissions are all in order:

```sh
sudo scripts/rx888_smoketest.sh   # preflight + 1 sec capture + sanity stats
```

Pre-built flowgraphs live in [`examples/`](examples/):

- [`am_bcb_audio_demo.grc`](examples/am_bcb_audio_demo.grc) — booth
  default: tunable AM Broadcast Band receiver with **audio out**,
  waterfall, and PSD. Works on a thumbtack.
- [`am_bcb_demo.grc`](examples/am_bcb_demo.grc) — same tuner without
  the audio path. Use when no speakers are available.
- [`hf_waterfall_demo.grc`](examples/hf_waterfall_demo.grc) — full
  0–16 MHz diagnostic view; no tuning, no decimation.

See [`examples/README.md`](examples/README.md).

## Status

- **v0.1 (shipped)** — `gr_modtool` scaffold + `rx888.source` block stub.
- **v0.2 (shipped)** — links `librx888`; streams real int16 samples through
  an SPSC ring to `work()`; six block parameters wire through to the
  device; ASan / TSan / valgrind clean.
- **v0.3 (in flight)** — stream tags (`rx_rate`, `rx_atten`, etc.) and
  the async `status` message port; switch to the upcoming typed-setter
  firmware API (see [`docs/firmware-api-plan.md`](docs/firmware-api-plan.md)).

## License

GPL-3.0-or-later. See [`COPYING`](COPYING).
