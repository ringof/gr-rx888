# gr-rx888 Time-Stream Implementation Plan

Working plan for landing the PPS-anchored time stream in `gr-rx888`,
consuming the PPS callback API documented in
[`rx888-tools/doc/pps_window.md`][pps-window] (branch
`claude/detect-dropped-samples-YbnYV`).

[pps-window]: https://github.com/ringof/rx888-tools/blob/claude/detect-dropped-samples-YbnYV/doc/pps_window.md

## Context

`librx888` (upstream, separate repo) is adding a PPS event callback
that fires on each closed PPS window. The event carries:

```c
typedef struct {
    uint64_t event_id;            /* monotonic counter, 0-based per start() */
    uint64_t host_monotonic_ms;   /* CLOCK_MONOTONIC ms at window close */
    uint64_t bytes_since_prev;    /* last_window_bytes at this event */
    uint64_t sample_index;        /* bytes_out/2 at the boundary, i.e. first
                                     IQ sample AFTER the PPS edge */
} rx888_pps_event_t;

typedef void (*rx888_pps_cb_t)(const rx888_pps_event_t *e, void *user);
void rx888_set_pps_callback(rx888_t *r, rx888_pps_cb_t cb, void *user);
```

Plus extended `rx888_stats_t` fields: `expected_xfer_bytes`,
`full_xfers`, `short_xfers`, `zero_xfers`, `min/max_actual_len`,
`bytes_in_window`, `last_window_bytes`.

The callback fires on librx888's writer thread with the same
re-entrancy contract as `sample_cb`: do not block, hand off and
return.

## Decisions (locked)

| Question | Choice |
|---|---|
| librx888 dependency | Hard-require new librx888 via CMake symbol check. No version literal. |
| UTC source (this PR) | Host system clock (CLOCK_REALTIME) snapshotted in the PPS callback. |
| gpsd | Separate, follow-up PR. Not in scope here. |
| Status message port | Bundled with this work. |
| GRC knob for time-tagging | None. Always emit when librx888 delivers events. |
| Short-window message | Always emit `pps_window` per PPS event (with deficit field). |
| librx888 version pin | `check_symbol_exists()`, not a pkg-config version literal. |

## Work breakdown

### 1. Mock surface — `tests/mock/librx888.{h,c}`

- Add `rx888_pps_event_t`, `rx888_pps_cb_t`,
  `rx888_set_pps_callback()` matching upstream.
- Add the new `rx888_stats_t` fields.
- Mock's synthesis thread fires a PPS event every `samplerate` int16
  samples (one per simulated second). `sample_index` set to absolute
  sample count at the boundary.
- Introspection knob (via `librx888_mock_introspect.h`) to inject a
  single short window on demand, so QA can drive the anomaly path
  deterministically.

### 2. Build — `CMakeLists.txt`

- After resolving the system librx888 include path, run
  `check_symbol_exists(rx888_set_pps_callback "librx888.h" HAVE_RX888_PPS_API)`.
- If absent, fail with:
  *"rx888-tools branch `claude/detect-dropped-samples-YbnYV` or later
  required for PPS callback API."*
- No pkg-config version literal to revisit when upstream tags.

### 3. Block — `lib/source_impl.{h,cc}`, `include/gnuradio/rx888/source.h`

**PPS event queue.** Add an SPSC ring of size 8 holding
`{event_id, sample_index, utc_sec, bytes_since_prev}`.

**Callback.** Register `pps_cb` after `rx888_start`. In the callback:
1. `clock_gettime(CLOCK_REALTIME, &ts)`, round to whole seconds.
2. Cross-check against `host_monotonic_ms` (warn once if the realtime
   offset jumps >100 ms across consecutive events — indicates an NTP
   step that would corrupt the tag stream).
3. Push event onto the SPSC ring. If the ring is full, drop and bump
   a counter; emit one `pps_overflow` status message per burst.

**Sample-index mapping.** librx888's `sample_index` counts samples
delivered to our `sample_cb`. We may have dropped some in
`sample_cb` when the sample ring was full (existing
`d_dropped_samples` counter). Maintain `d_cb_total_samples` (writer
side, monotonic) and `d_dropped_samples` (existing). The GR
absolute offset for a librx888 event at `sample_index = S` is
`S − d_dropped_samples_at_S`. We snapshot `d_dropped_samples` at
event arrival to avoid races.

**`work()`.** Before producing output:
- One-shot tags on first call at `nitems_written(0)`:
  - `rx_rate` = `samprate` (double)
  - `rx_atten` = `atten` (double, dB)
  - `rx_vga_gain` = `vga_gain` (double, dB)
- Drain PPS events whose mapped GR offset falls in
  `[r, r+to_copy)`. For each, emit:
  ```cpp
  add_item_tag(0, gr_offset,
               pmt::intern("rx_time"),
               pmt::make_tuple(pmt::from_uint64(utc_sec),
                               pmt::from_double(0.0)));
  ```
  `sample_index` is "first sample after the edge" per upstream doc, so
  no off-by-one adjustment.

**Status message port.** New async `pmt::mp("status")` output port.
Messages (PMT dicts):
- `pps_window` — emitted **every** PPS event. Keys:
  `event_id`, `utc_sec`, `bytes`, `deficit_samples`
  (= `fs − bytes_since_prev/2`; zero in the clean case).
- `host_overflow` — one per overflow burst on the sample ring.
- `pps_overflow` — one per overflow burst on the PPS event ring.
- `device_gone` — on `start()` failure or watchdog trip.
- `firmware_version`, `clock_locked` — once at startup, sourced from
  `rx888_get_stats`.

**Source header doc comment.** Update to match shipped behaviour
(remove the "ships without an active librx888 backend" stanza; it's
stale from v0.1).

### 4. GRC — `grc/rx888_source.block.yml`

Add to `outputs:`
```yaml
- domain: message
  id: status
  optional: true
```
No new parameters.

### 5. Tests — `python/rx888/qa_source_mock.py`

- `test_rx_time_tag_cadence`: run the mock for ~3 simulated seconds,
  collect tags from the source output, assert `rx_time` tags appear
  at offsets `≈ k * samprate` and UTC seconds are strictly
  monotonic.
- `test_one_shot_config_tags`: assert exactly one each of
  `rx_rate` / `rx_atten` / `rx_vga_gain` at offset 0.
- `test_short_window_message`: with the mock's short-window
  injection knob enabled, capture status-port messages, assert a
  `pps_window` dict with `deficit_samples > 0` is received.

### 6. Example — `examples/pps_demo.grc`

Minimal flowgraph: `rx888.source` → tag-debug → null sink; status
port → message-debug. Suitable for visual confirmation at the bench.

### 7. Docs

- `README.md`: flip "v0.3 (in flight)" bullets for time-stream tags
  and status port to shipped. Leave the typed-setter firmware-API
  bullet still in flight.
- `docs/PLAN.md`: status snapshot refresh.

## Out of scope (separate PRs)

- gpsd UTC source. Will add a `utc_source` block param (`system` |
  `gpsd`) and an optional libgps build-time dependency.
- Typed-setter firmware API consumption (`docs/firmware-api-plan.md`).
- FX3 UART NMEA path → device-side UTC via `0xB7 GETSTATS`.

## Open risks

- **NTP step during a long capture.** A backwards UTC step
  invalidates the tag stream after the step point. The 100 ms
  monotonic-vs-realtime sanity check warns; consumers wanting hard
  monotonicity should run with `chronyd -x` or migrate to the
  gpsd-backed path in the follow-up PR.
- **Ring-overflow skew.** If the sample ring overflows, the
  `sample_index → gr_offset` mapping shifts. Documented above:
  snapshot `d_dropped_samples` at event arrival, apply at tag time.
  Worst case is a missing tag, never a misplaced one.
- **Upstream API churn.** Pinning by symbol-existence rather than
  version means a future signature change in
  `rx888_set_pps_callback` would compile but link/run-time
  misbehave. Mitigation: the mock declares the exact signature; the
  CI build against the mock will fail loudly if the real header
  diverges. Worth a follow-up to add a pkg-config minimum version
  once upstream cuts a release tag.
