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
pybind11. librx888 is optional during early development.

```sh
mkdir build && cd build
cmake -DWITH_RX888=OFF ..   # or ON once librx888 is installed
make -j
ctest
sudo make install
sudo ldconfig
```

## Status

- v0.1 — scaffold + block stub. No hardware I/O.
- v0.2 — librx888 link, streaming pipeline, stream tags + status messages.
- v0.3 — three example flowgraphs (`hf_receiver_demo`, `raw_recorder`,
  `spectrogram_demo`).

## License

GPL-3.0-or-later. See [`COPYING`](COPYING).
