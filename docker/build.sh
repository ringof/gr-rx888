#!/usr/bin/env bash
# Helper: stage librx888 sources and build the docker image.
#
# librx888 sources live outside this repo (drop-in target: rx888-tools).
# For the Dockerfile we need them in the build context, so this script
# copies them in from a path you specify.
#
# Usage:
#   docker/build.sh [path/to/librx888-staging]
#
# If no path is given, defaults to ../librx888-staging.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(dirname "$HERE")"
SRC="${1:-$REPO/../librx888-staging}"

if [[ ! -d "$SRC" ]]; then
    echo "librx888 staging dir not found: $SRC" >&2
    echo "Pass it as the first argument, e.g.:" >&2
    echo "    docker/build.sh /tmp/librx888-staging" >&2
    exit 1
fi

for f in include/librx888.h src/librx888.c src/rx888_stream.c Makefile; do
    if [[ ! -f "$SRC/$f" ]]; then
        echo "missing $SRC/$f" >&2
        exit 1
    fi
done

stage="$REPO/docker/librx888-src"
mkdir -p "$stage"
cp "$SRC/include/librx888.h"     "$stage/"
cp "$SRC/src/librx888.c"         "$stage/"
cp "$SRC/src/rx888_stream.c"     "$stage/"
cp "$SRC/Makefile"               "$stage/"

cd "$REPO"
exec docker build -f docker/Dockerfile -t gr-rx888:latest .
