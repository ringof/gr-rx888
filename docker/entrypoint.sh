#!/usr/bin/env bash
# Container entrypoint. Dispatches based on argv[1].
#
#   smoketest  (default) — preflight + 1-second sample capture + stats
#   grc-demo            — launch GNU Radio Companion with hf_waterfall_demo.grc
#   shell               — interactive bash inside the container
#   <anything else>     — exec it directly (rx888_stream, gnuradio-config-info, …)

set -euo pipefail

cmd="${1:-smoketest}"
shift || true

case "$cmd" in
    smoketest)
        exec rx888_smoketest "$@"
        ;;
    grc-demo)
        exec grc-demo "$@"
        ;;
    shell)
        exec bash -l "$@"
        ;;
    *)
        exec "$cmd" "$@"
        ;;
esac
