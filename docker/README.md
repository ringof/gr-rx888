# gr-rx888 in Docker

Pre-built environment with `librx888` + GNU Radio 3.10 + the `rx888.source` block. Use when:

- The host laptop has the wrong GR version
- You want a known-good build to ship on a USB stick
- You want CLI smoketest reproducibility before touching GRC

## Build the image

```sh
docker/build.sh
```

The Dockerfile clones rx888-tools (librx888 lives there upstream)
and runs `make firmware && make install` to fetch the pinned FX3
firmware release and install librx888 system-wide inside the image.
No external paths required.

## Run the smoketest

```sh
docker run --rm -it --privileged \
           -v /dev/bus/usb:/dev/bus/usb \
           gr-rx888:latest
```

What happens:

1. Preflight checks (USB3 link, firmware loaded, usbfs_memory_mb, device-node access).
2. Captures 1 second of samples at 32 MS/s.
3. Computes mean / std / min / max / range.
4. Reports PASS or FAIL.

`--privileged` is the simple-but-blunt way to expose USB to the container. After Dayton, switch to `--device-cgroup-rule='c 189:* rmw' --device=/dev/bus/usb/<bus>/<dev>`.

## Run the GRC AM BCB receiver demo (with audio)

```sh
xhost +local:docker
docker run --rm -it --privileged \
           -v /dev/bus/usb:/dev/bus/usb \
           -v /run/user/$(id -u)/pulse:/tmp/pulse:ro \
           -e PULSE_SERVER=unix:/tmp/pulse/native \
           -e DISPLAY=$DISPLAY \
           -v /tmp/.X11-unix:/tmp/.X11-unix \
           gr-rx888:latest grc-demo
```

Opens `examples/am_bcb_audio_demo.grc` in GRC. Click Execute. You get
a waterfall, a PSD, a Tune slider across 540–1700 kHz, a Volume
slider, and **audio** of the tuned AM station out the host's default
PCM device.

### Why PulseAudio passthrough and not `--device /dev/snd`

On every modern Linux laptop the host's PulseAudio or PipeWire owns
the ALSA hardware exclusively, so a second process inside the
container that tries to open `default` directly via `--device /dev/snd`
hits **`Device or resource busy`** — exactly the EBUSY we kept seeing.

The image installs `libasound2-plugins` and a `/etc/asound.conf` that
routes ALSA's `default` through the **pulse plugin**, which forwards
audio to the PulseAudio socket. Bind-mounting `/run/user/$(id -u)/pulse`
into the container and setting `PULSE_SERVER=unix:/tmp/pulse/native`
gives the plugin the destination. The host audio server then mixes
our stream with everything else — no EBUSY.

PipeWire users get this for free: PipeWire ships a PulseAudio-compat
socket at the same path.

If you want the silent visualization only, swap the entrypoint:

```sh
docker run ... gr-rx888:latest sh -c \
  'gnuradio-companion /opt/gr-rx888/examples/am_bcb_demo.grc'
```

Or for the wide-spectrum 0–16 MHz diagnostic view:

```sh
docker run ... gr-rx888:latest sh -c \
  'gnuradio-companion /opt/gr-rx888/examples/hf_waterfall_demo.grc'
```

## Other commands

```sh
docker run --rm -it --privileged -v /dev/bus/usb:/dev/bus/usb gr-rx888 shell
# → interactive bash; rx888_stream / rx888_preflight / etc. on PATH

docker run --rm -it gr-rx888 gnuradio-config-info --version
# → 3.10.9.2
```

## Host-side prep the container can't do for you

- **`usbfs_memory_mb`** is a host kernel parameter. The preflight script tries `sudo tee` on it, but inside `--privileged` containers your host's value is what you get. If captures fail with `LIBUSB_ERROR_NO_MEM`, run on the **host**: `sudo sh -c 'echo 1000 > /sys/module/usbcore/parameters/usbfs_memory_mb'`.
- **udev rules** for non-root USB access apply to the host. With `--privileged` you don't need them inside the container.
- **X11 access** (`xhost +local:docker`) opens the host's display server; revert with `xhost -local:docker` after the demo.
- **Audio passthrough** is via the host's PulseAudio/PipeWire socket (see above), not `--device /dev/snd`. PulseAudio/PipeWire holds the ALSA hardware exclusively on every modern Linux desktop, so direct ALSA from the container EBUSYs. If you somehow need raw ALSA (headless server, no audio daemon), add `--device /dev/snd --group-add audio` instead and either edit `/etc/asound.conf` inside the container or pass an explicit hw device to `audio.sink`.
