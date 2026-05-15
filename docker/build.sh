#!/usr/bin/env bash
# Build the gr-rx888 Docker image.
#
# librx888 sources live in this repo at tests/librx888/ (staging copy
# until librx888 is merged into rx888-tools upstream). The Dockerfile
# COPYs them in directly; no external staging dir required.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(dirname "$HERE")"

cd "$REPO"
exec docker build -f docker/Dockerfile -t gr-rx888:latest .
