# RX888 vendor-command plan: a saner set for future developers

**Status:** Draft for discussion. Companion to rx888-firmware's
`docs/vendor-protocol-plan.md`. Same recoverability invariant.

## Why

The existing wire protocol (13 commands + 2 planned read-only) works,
and every shipped client depends on it. We keep all of it. But the
shape of the surface — untyped argument bytes, GPIO bitmasks that
conflate orthogonal concerns, a misnamed Si5351 command that ka9q-radio
bypasses entirely — makes every new consumer reinvent the same
chip-datasheet conversion tables. This plan defines an additive,
typed, capability-discoverable command set so that ka9q-radio,
Spectre, gr-rx888, and the next consumer that hasn't been written yet
can all speak the same dialect.

Nothing here breaks deployed clients. The legacy opcodes stay wired,
documented as such, with a one-line migration note pointing at the
typed equivalent.

## What the current command set actually looks like

13 commands today (`STARTFX3=0xAA`–`HANGMAIN=0xCF`), plus
`GETCAPABILITIES=0xBB` / `GETSTATE=0xBC` in the firmware plan. Four
structural problems:

1. **`STARTADC` is misnamed and bypassed in practice.** It's Si5351
   setup, not ADC activation. ka9q-radio doesn't use it: it runs Phil
   Karn's 450-line spur-aware solver on the host, then programs the
   Si5351 directly via `I2CWFX3`. The firmware's ~75-line ik1xpv-lineage
   solver is unreachable to ka9q. Two solvers, one device, no contract.

2. **`SETARGFX3` is untyped.** `(DAT31_ATT, value)` carries no unit, no
   range, no encoding. ka9q sends `att*2` (half-dB count, 0–63);
   `AD8370_VGA` needs a 50-line `gain2val()` conversion because the
   byte format mixes a 7-bit code with a high-gain-range bit. Every
   host has to know the DAT-31 and AD8370 datasheets.

3. **`GPIOFX3` is a bitmask command for orthogonal concerns.** `DITH`,
   `RANDO`, `BIAS_HF`, `BIAS_VHF`, `VHF_EN`, `PGA_EN`, `LED_BLUE`,
   `SHDWN`, `ATT_SEL0/1` all share one register write. Hosts maintain
   shadow state to flip one bit. Race-prone, requires every host to
   mirror device state correctly, and forecloses ever moving an LED or
   repurposing a GPIO line.

4. **Configuration is conflated with state transitions.** `STARTFX3`
   has undocumented preconditions discovered by reading host code.
   The firmware plan's three-bucket model — *Configuration / Control /
   Query* — fixes this. Make it real.

## Three consumer profiles

| Consumer | Wants from firmware | Won't move to firmware |
|---|---|---|
| **ka9q-radio** | Streaming, GPIO bits, attenuator + VGA, `I2C_WRITE` escape hatch for its preferred solver | AGC loop, DC blocker, spur table, FFT — radiod's job |
| **Spectre** (SoapySDR seam) | `GET_CAPABILITIES`, typed setters, predictable units, `STOP`/`RESET` recoverability | SoapySDR-shaped abstractions, multi-channel pretense |
| **gr-rx888** | Same as Spectre minus the SoapySDR wrapper. Six typed setters → one streaming call → status messages | All DSP. The block is a digitizer source, not a receiver |

Common ground: **typed setters with documented units, capability
discovery, recoverable state machine, raw int16 stream, status
counters**. Divergence: ka9q wants the I²C/GPIO escape hatch;
Spectre/gr-rx888 prefer never touching it.

## Proposed command structure

Three buckets per the firmware plan, with concrete shape. Wire format:
vendor request byte + 16-bit `wValue` + 16-bit `wIndex` + optional
payload. All units explicit. All values clamped server-side to the
device's actual capability — the achieved value is observable via
`GET_STATE`.

### Configuration (idempotent, order-free, one command per concern)

| Cmd | Hex | Argument | Units / range |
|---|---|---|---|
| `SET_SAMPRATE` | 0xD0 | `uint64_t hz` (payload) | Hz, 1e6–130e6. Firmware runs the solver; achieved rate via `GET_STATE`. Single-rate-at-a-time — re-open the device to change rate domain. |
| `SET_REFERENCE` | 0xD1 | `uint64_t hz` (payload) | Hz, 10e6–100e6. Tells firmware what TCXO is installed. |
| `SET_RF_ATTEN` | 0xD2 | `int16_t cdB` (`wValue`) | centi-dB, 0–3150 in steps of 50. DAT-31 details internal. |
| `SET_VGA_GAIN` | 0xD3 | `int16_t cdB` (`wValue`) | centi-dB. Firmware picks AD8370 lo/hi range; reports actual via `GET_STATE`. |
| `SET_DITHER` | 0xD4 | `uint8_t on/off` (`wValue`) | |
| `SET_RAND` | 0xD5 | `uint8_t on/off` (`wValue`) | Also stamped into the stream header so hosts know whether to un-randomize. |
| `SET_BIAS` | 0xD6 | `port:uint8_t, on:uint8_t` (`wValue`) | port=0 HF, 1 VHF. One typed call instead of GPIO bit fiddling. |
| `SET_FRONT_END` | 0xD7 | `enum {HF=0, VHF=1, EXT=2}` (`wValue`) | Replaces `VHF_EN`+`ATT_SEL` bookkeeping. |

Idempotent: any of these may be issued in any order, in any state.
Firmware applies them when it next enters the streaming state; hosts
don't sequence.

### Control (explicit state transitions, no hidden config)

| Cmd | Hex | Semantics |
|---|---|---|
| `START` | 0xAA | Begin streaming. Preconditions: clock programmed (via `SET_SAMPRATE` or legacy `STARTADC`). Returns error on unmet precondition rather than silently misbehaving. |
| `STOP` | 0xAB | Halt streaming. Always succeeds. |
| `RESET` | 0xB1 | Soft reset to power-on-equivalent state. Subject to the recoverability invariant — firmware handles I²C-bus wedges, Si5351 lockup, and ADC misconfiguration internally without requiring a power cycle. |

Opcodes kept identical to today so deployed clients are unaffected.

### Query (read-only, side-effect-free, versioned JSON payload)

| Cmd | Hex | Returns |
|---|---|---|
| `GET_CAPABILITIES` | 0xBB | JSON, includes `"schema_version": 1`. Firmware version, board model, supported sample-rate set (from solver enumeration), attenuator range + step (cdB), VGA range + step (cdB), available GPIO functions, supported front-end ports. Issued once at open. |
| `GET_STATE` | 0xBC | JSON, includes `"schema_version": 1`. Current samprate (achieved), reference, atten, vga (all cdB), dither, rand, bias states, front-end selection, Si5351 lock-detect bit, last config error. Polled or on demand. |
| `GET_STATS` | 0xB3 | Counters: USB transfer errors, FIFO overruns, samples delivered. Polled at ~1 Hz for host status messages. |
| `GET_FIRMWARE_INFO` | 0xAC | Version + build hash. (Today's `TESTFX3`, renamed in docs.) |

The schema-version field is the forward-compatibility hatch.
Additive growth bumps minor; semantic changes bump major. Clients
read it before parsing.

`GET_CAPABILITIES` is what kills "every host has its own DAT-31 /
AD8370 lookup table." Ask once, get the truth, including the
firmware's enumerated supported-rate set so hosts can snap to a
representable rate before calling `SET_SAMPRATE`.

### Escape hatches (kept for power users)

| Cmd | Hex | Use case |
|---|---|---|
| `I2C_WRITE` / `I2C_READ` | 0xAE / 0xAF | Direct chip access. ka9q-radio's spur-aware solver path. Documented as "the typed setters cover the common case; reach for these only when they don't." |
| `GPIO_RAW` | 0xAD | Same disclaimer. Bypasses `SET_BIAS` / `SET_FRONT_END` bookkeeping at the host's risk. |

These stay forever. They're the safety valve and the reason the typed
setters can stay narrow.

## What the saner set kills

- **Synonym sprawl in host configs.** `att / atten / featten / rfatten`
  is a config-file concern, not a wire concern. One canonical
  `SET_RF_ATTEN` in centi-dB; application layer maps synonyms.
- **Per-host conversion tables.** `gain2val()` / `val2gain()` in
  ka9q-radio, equivalent in any future SoapySDR plugin, and what we'd
  have to write in gr-rx888 v0.2 all go away. cdB in, cdB out.
- **Shadow-state bugs.** `SET_BIAS` / `SET_FRONT_END` mean no host
  needs to remember "which GPIO bit was which" or "what was the rest
  of the mask when I last wrote it."
- **The "which solver?" question.** Firmware's solver is the contract,
  advertised through `GET_CAPABILITIES`. ka9q's spur-aware solver can
  stay as opt-in via `I2C_WRITE` for users who want it.

## What the saner set doesn't kill (intentionally)

- **All deployed clients.** Every change is additive. Legacy opcodes
  remain wired. `STARTADC` stays as documented alias.
- **The fast path.** Streaming endpoint and packet format are
  untouched. This is a *control-plane* refactor.
- **Application-level features.** AGC, DC blocking, spur notching,
  frequency planning — none belong in firmware. They stay where they
  are.

## Sequencing

Aligns with the firmware plan's four-commit cadence:

1. **Commit A** — Land `GET_CAPABILITIES` / `GET_STATE` in their final
   shape (firmware plan's commit 3), including `schema_version`. The
   keystone: every new typed setter validates against capability and
   reports through state.
2. **Commit B** — Typed configuration setters (`SET_SAMPRATE` through
   `SET_FRONT_END`). Legacy opcodes remain wired in parallel.
3. **Commit C** — Update `docs/architecture.md` (firmware plan's
   commit 4) to mark legacy opcodes as "kept, prefer the typed
   equivalent" with a migration table.
4. **Commit D** — librx888 + ka9q-radio + gr-rx888 adopt the typed
   setters; SoapySDR seam (Spectre) picks them up.

Hardware validation in the Spectre Docker harness (PR #124) and in
gr-rx888's QA against real hardware once v0.2 lands.

## Out of scope

VHF tuner driving, external trigger / sub-µs timestamping,
firmware-side filtering, multi-rate streaming. Punt list, revisit if
demand surfaces — matches the gr-rx888 `PLAN.md` stance.
