# gr-rx888 — pre-built environment with librx888 + GNU Radio 3.10.
#
# Builds on Ubuntu 24.04 (matches host expectations for Dayton 2026
# laptops). Targets: smoketest container (CLI), demo container (GRC GUI
# via X11). Default ENTRYPOINT runs the smoketest.
#
# Build:
#   docker build -f docker/Dockerfile -t gr-rx888:latest .
#
# Run smoketest (requires the device + USB passthrough):
#   docker run --rm -it --privileged \
#              -v /dev/bus/usb:/dev/bus/usb \
#              gr-rx888:latest
#
# Run GRC GUI:
#   xhost +local:docker
#   docker run --rm -it --privileged \
#              -v /dev/bus/usb:/dev/bus/usb \
#              -e DISPLAY=$DISPLAY \
#              -v /tmp/.X11-unix:/tmp/.X11-unix \
#              gr-rx888:latest grc-demo
#
# NOTE: --privileged is the simple path for USB passthrough at the venue.
# Tighter alternative: --device-cgroup-rule='c 189:* rmw' plus the
# specific /dev/bus/usb/<bus>/<dev> after lsusb. Save that for after Dayton.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git pkg-config \
        libusb-1.0-0-dev usbutils \
        gnuradio gnuradio-dev \
        python3 python3-pip python3-numpy \
        # GTK3 bindings — gnuradio-companion (grc) uses GTK for its
        # editor; without these the GUI fails at "Namespace Gtk not
        # available" before the workspace loads.
        gir1.2-gtk-3.0 python3-gi python3-gi-cairo \
        # PyQt5 — used by qtgui_* sink/source blocks at runtime, which
        # the bundled hf_waterfall_demo.grc relies on.
        python3-pyqt5 \
        ca-certificates sudo curl \
    && rm -rf /var/lib/apt/lists/*

# librx888 lives in rx888-tools upstream. Clone, fetch the pinned
# firmware blob from the rx888-firmware release named in
# rx888-tools/firmware/VERSION, then build + install.
# Pin a specific commit when this stabilizes.
RUN git clone --depth 1 https://github.com/ringof/rx888-tools.git /opt/rx888-tools

WORKDIR /opt/rx888-tools
RUN make firmware \
    && make \
    && make install \
    && ldconfig

# Build gr-rx888 from this checkout
WORKDIR /opt/gr-rx888
COPY . /opt/gr-rx888
WORKDIR /opt/gr-rx888/build
RUN cmake -DPYTHON_EXECUTABLE=/usr/bin/python3.12 .. \
    && make -j"$(nproc)" \
    && make install \
    && ldconfig
WORKDIR /opt/gr-rx888

# Place scripts and examples on PATH for convenience. Keep the .sh
# suffix so each script's "$HERE/<other>.sh" sibling lookups resolve.
# Symlinks without the suffix give a tidier CLI ($ rx888_smoketest).
RUN install -m 755 scripts/rx888_preflight.sh  /usr/local/bin/rx888_preflight.sh \
 && install -m 755 scripts/rx888_smoketest.sh  /usr/local/bin/rx888_smoketest.sh \
 && install -m 755 docker/entrypoint.sh        /usr/local/bin/rx888_entrypoint \
 && install -m 755 docker/grc-demo.sh          /usr/local/bin/grc-demo \
 && ln -s rx888_preflight.sh /usr/local/bin/rx888_preflight \
 && ln -s rx888_smoketest.sh /usr/local/bin/rx888_smoketest

ENTRYPOINT ["/usr/local/bin/rx888_entrypoint"]
CMD ["smoketest"]
