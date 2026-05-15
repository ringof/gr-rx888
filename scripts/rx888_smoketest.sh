#!/usr/bin/env bash
#
# rx888_smoketest.sh — end-to-end "is the RX888 actually streaming?" test.
#
# Runs preflight, then captures 1 second of samples via rx888_stream and
# computes basic statistics. PASS if we got the expected sample count and
# the stream isn't all-zero.
#
# Usage:  rx888_smoketest.sh [samplerate]
#         (default samplerate: 32000000)
#
# Exit code: 0 on PASS, non-zero on FAIL.
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SAMPLERATE="${1:-32000000}"
DURATION_SEC=1
TMPFILE="$(mktemp -t rx888_smoke.XXXXXX.raw)"
trap 'rm -f "$TMPFILE"' EXIT

echo "rx888_smoketest: samplerate=${SAMPLERATE} Hz, duration=${DURATION_SEC}s"
echo

# Preflight
"$HERE/rx888_preflight.sh" || {
    echo
    echo "FAIL: preflight"
    exit 1
}

# Capture
echo
echo "[capture] streaming for ${DURATION_SEC}s..."
expected_bytes=$(( SAMPLERATE * 2 * DURATION_SEC ))
if ! timeout "$((DURATION_SEC + 2))" rx888_stream -s "$SAMPLERATE" -r > "$TMPFILE" 2>/tmp/rx888_smoke.err; then
    rc=$?
    # timeout returns 124 on SIGTERM-after-timeout, which is fine if we got bytes
    if [[ $rc -ne 124 ]]; then
        echo "  ✗ rx888_stream failed (exit $rc):"
        sed 's/^/    /' </tmp/rx888_smoke.err >&2
        exit 1
    fi
fi

actual_bytes=$(stat -c %s "$TMPFILE")
echo "  · captured $actual_bytes bytes (expected ~$expected_bytes)"

# Need at least 50% of expected to call it a real stream
if [[ $actual_bytes -lt $((expected_bytes / 2)) ]]; then
    echo "  ✗ too few bytes — stream did not run"
    exit 1
fi

# Compute stats
echo
echo "[validate] computing sample statistics..."
python3 - <<PY
import sys, numpy as np
data = np.fromfile("$TMPFILE", dtype=np.int16)
n = len(data)
print(f"  · samples : {n:,}")
print(f"  · mean    : {data.mean():+.2f}")
print(f"  · std     : {data.std():.2f}")
print(f"  · min/max : {data.min()}/{data.max()}")
print(f"  · range   : {data.ptp()}")

problems = []
if data.std() < 50:
    problems.append("stream looks dead (std too low — all zeros or stuck value?)")
if abs(data.mean()) > 5000:
    problems.append(f"large DC bias ({data.mean():+.0f}) — fixup/randomizer flag mismatch?")

if problems:
    print()
    for p in problems:
        print(f"  ✗ {p}")
    sys.exit(1)
else:
    print()
    print("  ✓ stream looks healthy")
PY

rc=$?
echo
if [[ $rc -eq 0 ]]; then
    echo "rx888_smoketest: PASS"
    exit 0
else
    echo "rx888_smoketest: FAIL"
    exit 1
fi
