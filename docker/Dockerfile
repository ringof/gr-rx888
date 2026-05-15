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
        ca-certificates sudo \
    && rm -rf /var/lib/apt/lists/*

# librx888 expects rx888-tools sources for ezusb.* and rx888.h. Pull at
# build time so the image is hermetic. Pin a specific commit when this
# stabilizes.
RUN git clone --depth 1 https://github.com/ringof/rx888-tools.git /opt/rx888-tools

# Drop in librx888 sources (these come from this gr-rx888 repo's
# librx888-staging/ via the build context — see docker/build.sh)
COPY docker/librx888-src/librx888.h     /opt/rx888-tools/include/librx888.h
COPY docker/librx888-src/librx888.c     /opt/rx888-tools/src/librx888.c
COPY docker/librx888-src/rx888_stream.c /opt/rx888-tools/src/rx888_stream.c
COPY docker/librx888-src/Makefile       /opt/rx888-tools/Makefile

RUN cd /opt/rx888-tools && make && make install && ldconfig

# Build gr-rx888 from this checkout
WORKDIR /opt/gr-rx888
COPY . /opt/gr-rx888
RUN rm -rf build && mkdir build && cd build \
    && cmake -DPYTHON_EXECUTABLE=/usr/bin/python3.12 .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig

# Place scripts and examples on PATH for convenience
RUN install -m 755 scripts/rx888_preflight.sh  /usr/local/bin/rx888_preflight \
 && install -m 755 scripts/rx888_smoketest.sh  /usr/local/bin/rx888_smoketest \
 && install -m 755 docker/entrypoint.sh        /usr/local/bin/rx888_entrypoint \
 && install -m 755 docker/grc-demo.sh          /usr/local/bin/grc-demo

ENTRYPOINT ["/usr/local/bin/rx888_entrypoint"]
CMD ["smoketest"]
