# gr-rx888 in Docker

Pre-built environment with `librx888` + GNU Radio 3.10 + the `rx888.source` block. Use when:

- The host laptop has the wrong GR version
- You want a known-good build to ship on a USB stick
- You want CLI smoketest reproducibility before touching GRC

## Build the image

```sh
# librx888 sources must already be staged (drop-in for rx888-tools)
docker/build.sh /path/to/librx888-staging
```

That stages `librx888.h`, `librx888.c`, the slim `rx888_stream.c`, and the `Makefile` into `docker/librx888-src/` (gitignored), then runs `docker build`.

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

## Run the GRC waterfall demo

```sh
xhost +local:docker
docker run --rm -it --privileged \
           -v /dev/bus/usb:/dev/bus/usb \
           -e DISPLAY=$DISPLAY \
           -v /tmp/.X11-unix:/tmp/.X11-unix \
           gr-rx888:latest grc-demo
```

Opens `examples/hf_waterfall_demo.grc` in GRC. Click Execute. You should see live HF spectrum (~0–16 MHz visible at 32 MS/s real).

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
