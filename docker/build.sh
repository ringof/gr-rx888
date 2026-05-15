#!/usr/bin/env bash
# Build the gr-rx888 Docker image.
#
# librx888 lives in rx888-tools upstream — the Dockerfile clones it
# inside the image and runs `make firmware && make install`.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(dirname "$HERE")"

cd "$REPO"
exec docker build -f docker/Dockerfile -t gr-rx888:latest .
