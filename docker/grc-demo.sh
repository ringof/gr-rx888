#!/usr/bin/env bash
# Launch the HF waterfall demo flowgraph. Requires X11 forwarding from
# the host (DISPLAY env + /tmp/.X11-unix mount).
set -euo pipefail

if [[ -z "${DISPLAY:-}" ]]; then
    echo "DISPLAY not set. Run with:" >&2
    echo "  xhost +local:docker" >&2
    echo "  docker run ... -e DISPLAY=\$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix gr-rx888 grc-demo" >&2
    exit 1
fi

# Preflight first so the user gets a real error before the GUI hides it
rx888_preflight --quiet || {
    echo "Preflight failed; running with verbose output:" >&2
    rx888_preflight
    exit 1
}

# Default to the AM BCB demo — it always shows a signal, even on
# a thumbtack antenna. To open the wide-spectrum diagnostic view,
# launch with `gnuradio-companion /opt/gr-rx888/examples/hf_waterfall_demo.grc`.
exec gnuradio-companion /opt/gr-rx888/examples/am_bcb_demo.grc
