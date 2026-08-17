# K9Probe

Hardware probe for the **LineSel-cloned synthesized edit-handler path** on
knobs 4..9 (pages 2/3). This is the missing validation between the current
state (knobs 1..3 hardware-proven, knobs 4..9 build cleanly but untested)
and a full 9-knob Reverson ZDL.

## What it proves

| knob | page | handler | hardware status |
|---|---|---|---|
| 1 (params[5])  | 1 | LineSel knob1 clone (`linesel_handlers.bin` +0x60) | proven |
| 2 (params[6])  | 1 | LineSel knob2 clone (`linesel_handlers.bin` +0x0AC) | proven |
| 3 (params[7])  | 1 | AIR knob3 blob (`air_knob3_edit.bin`) | proven |
| 4 (params[8])  | 2 | synthesized LineSel clone (knob_id 5, off 32) | **open** |
| 5 (params[9])  | 2 | synthesized LineSel clone (knob_id 6, off 36) | **open** |
| 6 (params[10]) | 2 | synthesized LineSel clone (knob_id 7, off 40) | **open** |
| 7 (params[11]) | 3 | synthesized LineSel clone (knob_id 8, off 44) | **open** |
| 8 (params[12]) | 3 | synthesized LineSel clone (knob_id 9, off 48) | **open** |
| 9 (params[13]) | 3 | synthesized LineSel clone (knob_id 10, off 52) | **open** |

> **2026-08-17: imageInfo `+0x20` bug fixed in the linker.** Previously
> `effectTypeImageInfo +0x20` was written as the TOTAL param count for
> >3-param effects (9 here). Stock 9-param effects (G1on_STDELAY) advertise
> 3 = visible knob slots per edit page. The wrong value made G1on render
> no usable UI / freeze (Reverson hardware test 2026-08-16). The linker
> now writes `knob_count_override or 3`; this probe and Reverson both
> rebuild with `+0x20 = 3`. Knobs 4..9 synthesized-handler behavior is
> still unproven on hardware — this probe remains the gate for that.

Descriptor advertises the full 9-user-knob ceiling (3 pages x 3 visible
knobs, AIR-style paginated `effectTypeImageInfo`).

## Audio behavior (how you know a knob works)

The audio body adds `fxBuf * gain` to the output accumulator. Odd knobs
(Knob1/3/5/7/9) boost the **L** block, even knobs (Knob2/4/6/8) boost the
**R** block, each with a distinct step `(k+1)*0.04` per normalized unit.
So turning knob N audibly changes gain on a specific channel, with a
specific amount. Param normalization is not yet pinned down, so the value
is mapped through a wide window (abs + clamp to [0,1]) ? any nonzero knob
becomes audible.

## Build

Requires TI C6000 compiler (cl6x). On macOS the repo default path is used;
override anywhere with:

    export TI_CGT_ROOT=/path/to/ti-cgt-c6000
    python3 -B build_all.py k9probe

Artifact: `build/probes/K9Probe.ZDL` (probes are kept out of `dist/`).

## Flash + test procedure

1. Point Zoom Effect Manager at `build/probes/`, flash `K9Probe.ZDL` to the
   pedal (same USB import path as release ZDLs).
2. Load K9Probe in the Filter category, unbypass, play a note.
3. Page 1: turn Knob1..Knob3 ? L/R gain steps, no freeze.
4. Page 2: turn Knob4..Knob6 ? no freeze, gain changes.
5. Page 3: turn Knob7..Knob9 ? no freeze, gain changes.
6. Toggle bypass a few times.

**Pass** = every knob on all three pages changes the sound on its expected
channel and the pedal never freezes.

## Fail triage

* Freeze on page 2/3 knob: the synthesized clone ABI is wrong for later
  pages ? see `docs/EDIT-HANDLER-ABI.md` ?7/?8; next step is comparing the
  synth clone's compact MVK encodings against a stock 9-knob effect.
* No gain change on a knob but no freeze: the value reaches params[] but is
  near zero ? refine the normalization in the audio body.
* Effect doesn't load: descriptor/imageInfo pagination issue ? dump with
  `build/dump_zdl_descriptor.py`.
