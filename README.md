# gr-rx888

GNU Radio out-of-tree module for the **RX888 mk2** direct-sampling
digitizer — a 16-bit, up-to-130 MSPS ADC with an HF-tuned analog
front end, exposed as a single GR source block.

Pre-alpha. The block scaffold is in place; the librx888 backend is
landing separately in [`rx888-tools`](https://github.com/ka9q/rx888-tools)
and is gated by the CMake option `-DWITH_RX888=ON` (off by default).
With `WITH_RX888=OFF` the block builds, installs, and emits silence so
flowgraphs can be wired up against the final API.

See [`docs/PLAN.md`](docs/PLAN.md) for the design rationale, scope,
and how this module relates to ka9q-radio, rx888-tools, and the FX3
firmware.

## Block

`rx888.source` — one source block, float32 output.

| Parameter   | Type | Default | Meaning                                |
|-------------|------|---------|----------------------------------------|
| `samprate`  | real | 64.8e6  | ADC sample rate (Hz)                   |
| `atten`     | real | 0       | RF step attenuator (dB)                |
| `vga_gain`  | real | 0       | VGA gain (dB)                          |
| `dither`    | bool | False   | ADC dither                             |
| `rand`      | bool | True    | ADC output-randomization decode        |
| `reference` | real | 27e6    | Reference oscillator (Hz)              |

Stream tags emitted on output 0: `rx_rate`, `rx_atten`, `rx_vga_gain`,
`rx_time`.

Async message port `status` carries: `usb_overflow`, `host_overflow`,
`device_gone`, `firmware_version`, `clock_locked`.

## Build

Requires GNU Radio 3.10 (`gnuradio-dev` on Debian/Ubuntu) and
pybind11, plus [librx888](https://github.com/ringof/rx888-tools)
installed system-wide.

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
this module. One command runs the CLI smoke test:

```sh
docker/build.sh
docker run --rm -it --privileged -v /dev/bus/usb:/dev/bus/usb gr-rx888:latest
```

There's also a `grc-demo` entrypoint that launches the bundled HF
waterfall flowgraph with X11 forwarding. See [`docker/README.md`](docker/README.md)
for both modes, plus the host-side prep (`usbfs_memory_mb`, udev
rules) the container can't do for you.

## At the bench

Before the first time you wire up a flowgraph, run the preflight +
smoke test in [`scripts/`](scripts/) to confirm the device, USB3 link,
firmware load, `usbfs_memory_mb`, and udev permissions are all in order:

```sh
sudo scripts/rx888_smoketest.sh   # preflight + 1 sec capture + sanity stats
```

A pre-built waterfall flowgraph is in
[`examples/hf_waterfall_demo.grc`](examples/hf_waterfall_demo.grc).

## Status

- v0.1 — scaffold + block stub. No hardware I/O.
- v0.2 — librx888 link, streaming pipeline, stream tags + status messages.
- v0.3 — three example flowgraphs (`hf_receiver_demo`, `raw_recorder`,
  `spectrogram_demo`).

## License

GPL-3.0-or-later. See [`COPYING`](COPYING).
