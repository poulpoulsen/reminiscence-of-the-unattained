# Kinovice Marf248 – Technical Documentation

VCV Rack C++ port of the Buchla/Tiptop 248t MARF (Multiple Arbitrary
Function Generator). This documentation describes the main module
**Marf248** and the two expander modules **Marf248TriggerExpander** (24HP)
and **Marf248CvExpander** (12HP): every control, every input/output, the
internal signal processing, the most important C++ functions, and the
known pitfalls in how the features interact.

References: Tiptop Audio 248t manual, the "MARF Book" (Buchla 248-1602
documentation, maxl0rd), Suzanne Ciani Cookbook, V2.66 firmware changelog.

Status: every feature described in this documentation has been live-tested
and confirmed by Kinovice in a running Rack instance (including Address
Memory Store/Recall, 4-channel A–D selection with zone boundaries and
fallback behavior, and backward compatibility of existing mono patches).

---

<a id="toc"></a>
## Table of Contents

*Each entry has two links: the first (standard Markdown anchor) works in GitHub, Codeberg, VSCodium preview & most other viewers; "Obsidian" is the wikilink specifically for Obsidian. Within Obsidian you can also use the first link - it just isn't always resolved reliably there, hence the second variant as a fallback.*

1. [Architecture Overview](#sec1) · [[#1. Architecture Overview|Obsidian]]
2. [Basic Concepts: Stage, Section, Modifier Bits](#sec2) · [[#2. Basic Concepts: Stage, Section, Modifier Bits|Obsidian]]
3. [Marf248 – Main Module](#sec3) · [[#3. Marf248 – Main Module|Obsidian]]
   - [Panel Layout](#sec3-1) · [[#3.1 Panel Layout|Obsidian]]
   - [Per-Stage Controls](#sec3-2) · [[#3.2 Per-Stage Controls|Obsidian]]
   - [Per-Section Controls (A/B)](#sec3-3) · [[#3.3 Per-Section Controls (A/B)|Obsidian]]
   - [Module-Wide Shared Elements](#sec3-4) · [[#3.4 Module-Wide Shared Elements|Obsidian]]
   - [Inputs/Outputs Overview](#sec3-5) · [[#3.5 Inputs/Outputs Overview|Obsidian]]
4. [Internal Signal Processing](#sec4) · [[#4. Internal Signal Processing|Obsidian]]
   - [The Afg Core (Section Sequencing)](#sec4-1) · [[#4.1 The Afg Core (Section Sequencing)|Obsidian]]
   - [Stage-Advance Logic: Forward / Reverse / Pendulum / Skip](#sec4-2) · [[#4.2 Stage-Advance Logic: Forward / Reverse / Pendulum / Skip|Obsidian]]
   - [Stage Address: Reset / Continuous / Strobe / Channel Select](#sec4-3) · [[#4.3 Stage Address: Reset / Continuous / Strobe / Channel Select|Obsidian]]
   - [Voltage Signal Path: Range, Quantize, Slew](#sec4-4) · [[#4.4 Voltage Signal Path: Range, Quantize, Slew|Obsidian]]
   - [Time Signal Path: Time Range, External Time, Time CV, Time Multiplier](#sec4-5) · [[#4.5 Time Signal Path: Time Range, External Time, Time CV, Time Multiplier|Obsidian]]
   - [Sustain / Enable / Stop](#sec4-6) · [[#4.6 Sustain / Enable / Stop|Obsidian]]
   - [Pulse Outputs: APOUT, P1/P2OUT, Start/End of Cycle](#sec4-7) · [[#4.7 Pulse Outputs: APOUT, P1/P2OUT, Start/End of Cycle|Obsidian]]
   - [Preset System (Pickup/Catch)](#sec4-8) · [[#4.8 Preset System (Pickup/Catch)|Obsidian]]
   - [Address Memory](#sec4-9) · [[#4.9 Address Memory|Obsidian]]
   - [Clock-Quantized Advance](#sec4-10) · [[#4.10 Clock-Quantized Advance|Obsidian]]
   - [Scale/Key Quantization](#sec4-11) · [[#4.11 Scale/Key Quantization|Obsidian]]
5. [Marf248 – Function Reference (C++)](#sec5) · [[#5. Marf248 – Function Reference (C++)|Obsidian]]
6. [Marf248TriggerExpander](#sec6) · [[#6. Marf248TriggerExpander|Obsidian]]
7. [Marf248CvExpander](#sec7) · [[#7. Marf248CvExpander|Obsidian]]
8. [Expander Mechanism (Inter-Module Communication)](#sec8) · [[#8. Expander Mechanism (Inter-Module Communication)|Obsidian]]
9. [Persistence (Patch Storage)](#sec9) · [[#9. Persistence (Patch Storage)|Obsidian]]
10. [Known Pitfalls in Feature Interaction](#sec10) · [[#10. Known Pitfalls in Feature Interaction|Obsidian]]
11. [Design Decisions & Scope Boundaries](#sec11) · [[#11. Design Decisions & Scope Boundaries|Obsidian]]

---

<a id="sec1"></a>
## 1. Architecture Overview
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

The project consists of three VCV Rack modules that together form a
plugin:

| Module | File(s) | Purpose |
|---|---|---|
| **Marf248** | `Marf248.hpp`, `Marf248.cpp` | Main module: two independent AFG sections (A/B) that share 16 common stages. Contains all of the DSP core logic. |
| **Marf248TriggerExpander** | `Marf248TriggerExpander.hpp/.cpp` | Optional 24HP expander: additional trigger/gate inputs and outputs (Clear, Presets, Scale/Key, Start/End of Cycle, Run gates, Advance, Address Memory, stage-modifier CV, SEXT). |
| **Marf248CvExpander** | `Marf248CvExpander.hpp/.cpp` | Optional 12HP expander: polyphonic CV modulation of stage time and slew curve, plus polyphonic per-stage gate outputs. |

**Core principle of the expander mechanism:** Neither expander has any
DSP logic of its own. They are purely "jack carriers." The main module's
`Marf248::process()` checks every sample whether a matching module is
docked to its left or right (`getLeftExpander()`/`getRightExpander()`)
and reads/writes that module's parameters, inputs, and outputs directly.
This avoids message-passing/double-buffering, but is only valid because
the expanders have no independent processing of their own (see the VCV
Rack Plugin API Guide, "Expanders" section: "you can implement its entire
DSP algorithm in the [primary] module"). Each expander also independently
checks every frame whether a Marf248 is docked, so it can reset its own
outputs/lights cleanly to 0V when removed or moved away.

The trigger-expander lookup deliberately sits right at the top of
`Marf248::process()`, before stage-address processing, since SEXT
(external strobe pulses) must already know about the trigger expander
before `applyStageAddress()` is called.

All three modules are registered in `plugin.cpp`/`plugin.hpp`
(`modelMarf248`, `modelMarf248TriggerExpander`, `modelMarf248CvExpander`).

---

<a id="sec2"></a>
## 2. Basic Concepts: Stage, Section, Modifier Bits
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

- **Stage**: one of 16 programming steps (`NUM_STAGES = 16`). Each stage
  has a voltage value, a time value, Range and Curve controls, and a
  bundle of digital modifier bits. The 16 stages are **shared module-wide**
  – Section A and Section B read the same programming but run
  independently as two separate "pointers" (`currentStage`) over it.
- **Section (A/B)**: two independent AFG instances (`Afg afgA`, `Afg afgB`
  inside `Marf248`), each with its own run direction, its own run/stop
  state, its own time multiplier, its own stage-address source, etc., but
  stepping through the same 16 stages.
- **Modifier bits** (`enum StageModifierBits`, in `Marf248.hpp`): a single
  `int stageModifier[NUM_STAGES]` array encodes all digital per-stage
  states as a bitmask. Overview:

| Bit | Meaning | Hardware precedent? |
|---|---|---|
| `MOD_QUANT_OCT` | Quantize the stage voltage to Scale/Key | yes |
| `MOD_SLOPED` | Stage ramps (slew) instead of stepping | yes |
| `MOD_STOP` | Programmed stop stage | yes |
| `MOD_SUSTAIN` | Holds the stage while the start gate is high | yes |
| `MOD_ENABLE` | Holds the stage while the start gate is low | yes |
| `MOD_FIRST` | Marks the stage as the start of a cycle | yes |
| `MOD_LAST` | Marks the stage as the end of a cycle | yes |
| `MOD_SLEW_EXP` / `MOD_SLEW_LOG` | Ramp curve shape (no bit = linear) | yes |
| `MOD_RANGE_HALF` / `MOD_RANGE_LIMITED` (+ 3-bit octave offset) | Voltage range Full/Half/Limited | yes |
| `MOD_VOLT_EXT` / `MOD_TIME_EXT` | External voltage/time source (channel select A–D) instead of the knob | yes |
| `MOD_PULSE1` / `MOD_PULSE2` | Programmed pulse outputs P1OUT/P2OUT | yes |
| `MOD_SKIP` | Stage is skipped entirely during automatic playback | **no** (pure software extension) |
| `MOD_SLEW_NORMALIZE` | Stretches the visibly-moving curve to fill the full stage time | **no** |
| `MOD_TRANGE_1/2/4` | Time Range TR1/TR2/TR4 (no bit = TR3, default) | yes (TR1–4) |

In addition, a 3-bit field (`RANGE_OFFSET_MASK`) encodes the octave offset
in Limited-range mode (5 steps: +0/+2/+4/+6/+8 V).

Two further storage structures, **independent** of the modifier-bit array,
supplement stage programming: the **preset system** (complete programming
of all 16 stages, 12 slots, see [4.8](#sec4-8)) and the new **Address
Memory** (a pure jump bookmark onto a stage index, 16 slots per section,
see [4.9](#sec4-9)) – both are conceptually distinct and should not be
confused with one another.

---

<a id="sec3"></a>
## 3. Marf248 – Main Module
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

<a id="sec3-1"></a>
### 3.1 Panel Layout
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

The panel is 120HP wide and organized into several vertical columns
(left to right):

```
[External CV A–D] [Section A Controls] [Section A Jacks] [Stage Array (16×)] [Section B Jacks] [Section B Controls] [Preset Column]
```

- **External CV column** (far left): the four module-wide shared external
  CV inputs (jacks A–D), which per-stage `MOD_VOLT_EXT`/`MOD_TIME_EXT` can
  reference.
- **Section A/B Controls column**: Time Multiplier knob/attenuverter, Run
  button, Mode/Pendulum buttons, Stage Address controls.
- **Section A/B Jacks column** (12–115mm, extended by 5mm to 115mm for the
  new channel-select CV jack): all jacks for the respective section
  (Start/Stop, Time Multiplier CV, VOUT/TOUT, P1OUT/P2OUT, APOUT, Stage
  Address CV, Stage Address channel-select CV).
- **Stage array** (center, 16 identically-structured columns): the actual
  programming for each of the 16 stages.
- **Preset column** (far right): Load/Save, Scale/Key, 12 preset slots.
- Above the stage array, centered: **Global Clear**, **Reset Values**, and
  **Auto-Fill**.

Each column is visually enclosed by a thin frame (`StageFrame` widget);
functionally related jack groups are additionally separated by thin
horizontal divider lines (`HDivider`).

<a id="sec3-2"></a>
### 3.2 Per-Stage Controls
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

Each of the 16 stage columns contains, from top to bottom:

| Element | Type | Function |
|---|---|---|
| **Voltage** (`STAGE_VOLTAGE_PARAM`) | Knob, 0–10V | Programmed output voltage of the stage. In External mode (`MOD_VOLT_EXT`), the knob position instead selects the channel A–D (4 zones of 2.5V each). The tooltip shows volts, note name (when Quantize is active), or channel name (when External is active) – see `StageVoltageQuantity`. |
| **Time** (`STAGE_TIME_PARAM`) | Knob, normalized 0–1 | Programmed stage time as a position **within** the selected Time Range bracket (TR1–4). The tooltip shows the Buchla Cookbook dial value (0–10, leading) with the exactly resolved number of seconds in parentheses afterward, e.g. `4.0 (0.132 s)` – see `StageTimeQuantity`. Right-click number entry still accepts the 0–10 dial value. In External mode (`MOD_TIME_EXT`), the position selects the channel A–D. |
| Two stage LEDs (`STAGE_LIGHT_A/B`) | Light | Indicate whether Section A or B is currently addressing this stage (green/red, side by side). |
| **Pulse 1 / Pulse 2 / Skip** | LED buttons (toggle) | Row 1: `MOD_PULSE1`/`MOD_PULSE2` fire a short trigger on the section's P1OUT/P2OUT when the stage is reached. `MOD_SKIP` (pure software extension) skips the stage entirely during automatic playback – no time, no triggers, no voltage output from it. Only affects automatic playback, not manual addressing (Reset/Strobe/Continuous). |
| **Stop / Sustain / Enable** | LED buttons (toggle) | Row 2: `MOD_STOP` = programmed stop stage (pauses playback until the section is restarted). `MOD_SUSTAIN` = holds the stage while a high level (gate) is present at the start input (ADSR-like release behavior). `MOD_ENABLE` = holds the stage while **no** high level is present (release only via a gate). Both hold states pause the dwell-time counter `elapsed`. **Watch out for the pitfall with no cable patched into START_A/B – see [Section 10](#sec10).** |
| **First / Last / Slope** | LED buttons (toggle) | Row 3: `MOD_FIRST`/`MOD_LAST` mark cycle start/end (wrap points for Forward/Reverse/Pendulum, and fire Start/End-of-Cycle triggers on the trigger expander; multiple First/Last pairs are possible, see [4.2](#sec4-2)). `MOD_SLOPED` switches between stepped (default) and ramped (slew) voltage change. |
| **Quantize / Volt-Ext / Time-Ext** | LED buttons (toggle) | Row 4: `MOD_QUANT_OCT` quantizes the target voltage to the global Scale/Key grid. `MOD_VOLT_EXT`/`MOD_TIME_EXT` switch the stage's voltage/time source to external (channel selection via the respective knob). |
| **Shape** (`STAGE_SHAPE_PARAM`) + two lights | LED button (3-state cycle) | Cycles Linear → Exponential → Logarithmic (only effective when Slope is active). Two separate LEDs (`STAGE_SHAPE_EXP_LIGHT`/`STAGE_SHAPE_LOG_LIGHT`) instead of a combined indicator; no bit set = linear. |
| **Curve** (`STAGE_CURVE_PARAM`) | Trimmer, 0–1 | Curve strength of the exponential/logarithmic ramp. Center (0.5) = reference curvature (`SLEW_CURVE_POWER = 2.5`), left more linear, right more pronounced (up to exponent ≈15.6). Can additionally be modulated by the Slew Curve CV on the CV expander. |
| **Normalize** (`STAGE_NORMALIZE_PARAM`) | LED button (toggle) | `MOD_SLEW_NORMALIZE` (pure software extension): at extreme curve settings, stretches the visibly-moving part of the curve to fill the entire stage time instead of squeezing it into a tiny time fraction. |
| **Time Range** (`STAGE_TRANGE_PARAM`, `TrangeButton`) | Large, self-drawing button (4-state cycle) | Selects TR1 (0.002–0.03s) through TR4 (2–30s); shows the current digit 1–4 directly on the button. TR3 (0.2–3s) is the default (no bit set). Right-click opens a context menu for direct selection. |
| **Range** (`STAGE_RANGE_PARAM`, `RangeKnob`) | Large knob with text display (7 detent positions) | Full (0–10V unchanged) → Half (compressed to 0–5V) → Limited +0/+2/+4/+6/+8V (2V span, equivalent to 2 octaves at 1V/oct, shifted by the chosen offset). Shows an abbreviation ("F"/"H"/"L0"…"L8") directly on the knob plus a colored pointer. Right-click opens a checklist context menu (more reliable than text entry for this snap-enabled control). |

<a id="sec3-3"></a>
### 3.3 Per-Section Controls (A/B)
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

Identical, mirrored layout for Section A and B:

**Controls column** (top to bottom):
- **Time Multiplier** (`TIME_MULT_A/B_PARAM`, 0.5×–4.0×) + **attenuverter**
  (`TIME_MULT_ATTEN_A/B_PARAM`, −1..+1) for the associated CV input. Scales
  all of this section's stage times multiplicatively, **after** the
  External/Internal time resolution has already taken place.
- **Run** (`RUN_A/B_PARAM`, LED button): toggles Start/Stop of the section
  (green LED = running). OR-combined with an external Start/Stop gate
  from the trigger expander. **Ignored while Advance is patched – see
  [4.10](#sec4-10) and [Pitfalls](#sec10).**
- **Mode** (`MODE_A/B_PARAM`, LED button): inverts the base run direction
  (off = Forward, on = Reverse).
- **Pendulum** (`PENDULUM_A/B_PARAM`, LED button): additionally enables
  back-and-forth bouncing between First and Last (see [4.2](#sec4-2)).
- **Stage Address knob** (`STAGE_ADDR_KNOB_A/B_PARAM`, trimmer 0–15):
  internal stage address (only effective in Int mode). **Never moves
  `currentStage` on its own – only Strobe/Continuous/Reset actually apply
  the address, see [Pitfalls](#sec10).**
- **Stage Address Int/Ext** (`STAGE_ADDR_MODE_A/B_PARAM`, LED button):
  selects between the internal knob and external CV
  (`STAGE_ADDR_EXT_A/B_INPUT`) as the address source.
- **Continuous** (`STAGE_ADDR_CONT_A/B_PARAM`, LED button): keeps
  `currentStage` synchronized with the address every frame and forces
  immediate full excursion – the section does not run internally while
  this is active (`running` is forced ineffective inside
  `applyStageAddress()`). An active slew ramp toward the addressed target
  keeps progressing regardless (see [4.3](#sec4-3)).
- **Strobe** (`STAGE_ADDR_STROBE_A/B_PARAM`, momentary button): a
  one-time jump to the current address; afterward, normal
  playback/slew continues. Flashes briefly (blue) as feedback.
- **Reset** (`STAGE_ADDR_RESET_A/B_PARAM`, momentary button): jumps
  immediately to the `MOD_FIRST` stage with the **lowest index**
  (`findLowestFirst()`, fallback: stage 1) – important when multiple
  First/Last segments are defined, see [4.2](#sec4-2).

**Jacks column** (top to bottom): Start trigger, Stop trigger, Time
Multiplier CV input, VOUT (voltage output), TOUT (time/interval output,
0–10V descending ramp), P1OUT/P2OUT (programmed pulses, with a red ring
light `PulseRingLight` around the jack that blinks with the pulse), APOUT
(All Pulses, fires on **every** stage change), Stage Address CV input,
**Stage Address channel-select CV input** (new, see [4.3](#sec4-3)).

**"Run/Hold/Stop" tri-color status LED** (at the same panel position):
- **Green** (`RUN_x_LIGHT`): section is running normally (or Advance mode
  is active – deliberately shown as "running", see [4.10](#sec4-10)).
- **Yellow** (`HOLD_x_LIGHT`): section is waiting in a Sustain/Enable
  stage for a start gate.
- **Red** (`STOP_x_LIGHT`): section stopped (button/pulse, a programmed
  stop stage, or Continuous addressing active).

<a id="sec3-4"></a>
### 3.4 Module-Wide Shared Elements
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

- **Global Clear** (`CLEAR_PARAM`, LED button, red flash light): resets
  all modifier bits, Range (→ Full), and Curve (→ center) on all 16
  stages. Also discards any pending preset pickups. Deliberately follows
  only the subset described in the manual (Output Pulses, voltage
  programming, operating mode, Time Range) – the Voltage/Time controls
  themselves are left untouched. **Address Memory is left untouched by
  Global Clear** (see [4.9](#sec4-9)).
- **Reset Values** (`RESET_VALUES_PARAM`, LED button, yellow flash
  light): a pure software extension with no hardware precedent. Resets
  **only** the Voltage and Time controls of all 16 stages to their
  defaults (0V and mid-time position) – modifier bits, Range, and Curve
  are left unchanged. Clearly separated from Global Clear.
- **Auto-Fill** (`AUTOFILL_PARAM`, LED button, blue flash light): a pure
  software extension with no hardware precedent, built for the phasor
  generator use case. Sets the 16 shared Voltage knobs to a linear 0–10V
  grid (step size 10/16 = 0.625V, stage 1 = 0V … stage 16 = 9.375V) and
  simultaneously forces `Range = Full` per stage. Time and modifier bits
  are left untouched (analogous to Reset Values). Because
  Voltage/Range are shared module-wide between Section A and B, there is
  deliberately only a single global button – separate A/B variants are
  not architecturally possible without a fundamental rework of the shared
  stage data structure.
- **Preset system** (`LOAD_PARAM`, `SAVE_PARAM`, `PRESET_PARAM` ×12):
  Load or Save is "armed" (latching), after which the next press on one
  of the 12 preset buttons selects the slot. A preset stores the complete
  programming of all 16 stages (Voltage/Time/Range/Curve knob values +
  all modifier bits). See [4.8](#sec4-8) (Pickup/Catch). Empty slots are
  ignored on load (no accidental reset), a short flash provides "nothing
  stored here" feedback.
- **Scale/Key** (`SCALE_PARAM`, `KEY_PARAM`): same arming principle as
  Load/Save, reusing the same 12 preset buttons as a dual assignment.
  Scale: only buttons 10/11/12 are valid (Major/Minor/Chromatic). Key:
  all 12 buttons are valid (C…B). See [4.11](#sec4-11).

<a id="sec3-5"></a>
### 3.5 Inputs/Outputs Overview
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

| Category | Inputs/Outputs | Shared? |
|---|---|---|
| Section start/stop | `START_A/B_INPUT`, `STOP_A/B_INPUT` | per section |
| External voltage sources | `EXT_A/B/C/D_INPUT` | module-wide shared |
| Stage address CV | `STAGE_ADDR_EXT_A/B_INPUT` (poly, up to 4 channels A–D) | per section |
| Stage address channel-select CV | `STAGE_ADDR_EXT_CHANNEL_A/B_INPUT` (new) | per section |
| Time Multiplier CV | `TIME_MULT_CV_A/B_INPUT` | per section |
| Section outputs | `VOUT_A/B`, `TOUT_A/B`, `APOUT_A/B`, `P1OUT_A/B`, `P2OUT_A/B` | per section |

---

<a id="sec4"></a>
## 4. Internal Signal Processing
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

<a id="sec4-1"></a>
### 4.1 The Afg Core (Section Sequencing)
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

`struct Afg` (in `Marf248.hpp`) encapsulates the **entire runtime state
and sequencing logic of a single section** (A or B). `Marf248` holds two
instances (`afgA`, `afgB`) that read the same stage data (via function
parameters) but run independently.

Key state fields:
- `currentStage`: index (0–15) of the currently addressed stage.
- `running` / `effectiveRunning`: `running` stores the persistent
  button/pulse state; `effectiveRunning = running || runGateHigh` is the
  state actually in effect for this frame (see trigger expander run
  gate). Deliberately kept **independent** of Advance mode (see
  [4.10](#sec4-10)).
- `elapsed`: dwell-time counter since entering the current stage.
- `voltageFrom` / `voltageOut`: the ramp's start value and current value
  (for seamless slew across stage boundaries).
- `reverseMode`, `pendulumMode`, `pendulumDir`, `pendulumHeld`,
  `pendulumWasOn`: run direction control (see [4.2](#sec4-2)).
- `continuousActive`: tracks whether `applyStageAddress()` ran in the
  Continuous branch this frame (see [4.3](#sec4-3)).
- `advanceModeActive`: true while `ADVANCE_A/B_INPUT` is patched (see
  [4.10](#sec4-10)).
- `statusHeld`, `statusStoppedByStage`: for the tri-color status LED.
- Several `dsp::PulseGenerator`/`dsp::SchmittTrigger` instances for edge
  detection and pulse output, including `advanceTrigger` (true edge
  detection, only processed while the jack is patched) and
  `runGateTrigger` (pure level detection with Schmitt hysteresis).

<a id="sec4-2"></a>
### 4.2 Stage-Advance Logic: Forward / Reverse / Pendulum / Skip
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

`Afg::advanceStage()` is called when `elapsed >= stageTime[currentStage]`
(automatic playback) or immediately if the current stage is marked as
Skip. Sequence:

1. **Determine the next stage**, depending on the run mode:
   - **Forward** (default): `next = currentStage + 1`; upon reaching the
     end or a `MOD_LAST` stage, wraps back to the next `MOD_FIRST` stage
     (`findFirstBefore()`).
   - **Reverse** (`reverseMode`): mirror image of Forward, decrements and
     wraps at `MOD_FIRST` to the next `MOD_LAST` stage
     (`findLastAfter()`).
   - **Pendulum** (`pendulumMode`): bounces back and forth between First
     and Last (assumes First..Last is a contiguous, non-wrapping range).
     At each turning point, the stage is **repeated once**
     (`pendulumHeld`) before the direction reverses – this makes the
     total period exactly 2×M (M = number of stages in the First–Last
     range) instead of 2×(M−1), keeping Pendulum in sync with a
     concurrently forward-running section. The starting direction when
     Pendulum is switched on is derived from `reverseMode` (Forward →
     bounce toward Last, Reverse → bounce toward First); toggling
     `reverseMode` while Pendulum is already running has no effect until
     the next off/on cycle (avoids an abrupt jump mid-bounce).
2. **Skip stages**: a loop (capped at `NUM_STAGES` attempts as a
   safety net) repeats the same stepping logic as long as the found
   stage has `MOD_SKIP` set. Skip only applies during automatic
   playback, not to manual addressing.
3. **Set the ramp start point** (`voltageFrom = voltageOut`), reset
   `elapsed` to 0, trigger `allPulseGen` (APOUT).
4. On re-entry **without** an index change (Pendulum turning-point
   repeat): an additional short retrigger pulse (`regateGen`,
   `REGATE_GAP_SECONDS = 5ms`) for the CV expander, so downstream
   envelope generators retrigger reliably.
5. `resetPulseEdgeState()` resets the edge-detection previous values for
   P1/P2/First/Last (see [4.7](#sec4-7)).

**Multiple First/Last segments:** since `findFirstBefore()`/
`findLastAfter()` wrap locally (relative to the current position) rather
than globally, the 16-stage field can be divided into several
independently-looping segments (e.g. First=1/Last=3, First=5/Last=8,
First=12/Last=16) – each segment loops on its own within its boundaries.
Switching between segments happens actively via external stage-address CV
combined with Strobe/Continuous, not automatically. Reset in this case
jumps specifically to the **very first** segment (`findLowestFirst()`,
lowest index) – not to whichever First is nearest from the current
position (which is what `findFirstBefore()` would have done, landing on a
different segment depending on `currentStage`).

<a id="sec4-3"></a>
### 4.3 Stage Address: Reset / Continuous / Strobe / Channel Select
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

`Afg::applyStageAddress()` is called **before** the actual
`Afg::process()` (order matters, since Continuous must make `running`
ineffective so the normal playback logic pauses in the same frame):

- **Reset**: jumps immediately to the `MOD_FIRST` stage with the lowest
  index (`findLowestFirst()`, fallback stage 1) – regardless of the
  current mode, regardless of Continuous/Strobe.
- **Continuous**: keeps `currentStage` synchronized with the address
  every frame (knob or external CV) and forces an immediate full
  excursion. The actual ramp progress (`elapsed += sampleTime`) keeps
  running continuously in `process()` regardless, so a slew ramp toward
  the newly addressed target still progresses smoothly – this matches the
  MARF manual ("slope time is equal to the programmed interval time",
  confirmed by the firmware changelog to apply during external/Continuous
  addressing as well) and is deliberately used, e.g., in the Cookbook
  "String Patch" for glide behavior. Start/Stop has no effect in
  Continuous mode (`running` remains ineffective).
- **Strobe**: a one-time jump to the current address (`jumpToStage()`);
  afterward, normal playback/slew continues unchanged. Deliberately
  ignored inside the Continuous branch via an early return.

The address itself comes either from the internal knob
(`STAGE_ADDR_KNOB_A/B`) or – when Ext mode is active – from external CV
(`STAGE_ADDR_EXT_A/B_INPUT`).

**4-channel A–D selection (new):** `STAGE_ADDR_EXT_A/B_INPUT` is now a
**polyphonic** input and can carry up to 4 independent address CVs
(channel A–D) simultaneously. Which channel is actually read is
determined by the new, dedicated channel-select CV
`STAGE_ADDR_EXT_CHANNEL_A/B_INPUT` (0–10V, split into 4 equally-sized
2.5V zones, the same zoning principle used for `MOD_VOLT_EXT`/
`MOD_TIME_EXT`). If the channel-select jack is **not** patched, channel
selection automatically falls back to channel A (index 0) – this keeps
existing mono patches into `STAGE_ADDR_EXT_A/B_INPUT` (which automatically
land on channel 1) working unchanged (backward compatibility, live-verified
by Kinovice). Testing the channel selection requires **Continuous** mode
in addition to Ext mode, so the section actually follows the selected
source live – Ext mode alone only supplies the address value but, just
like the internal knob, does not apply it automatically (see
[Pitfalls](#sec10)).

Additionally, an external strobe pulse ("SEXT", from the trigger expander)
can trigger the same action as the panel Strobe button (OR-combined).

<a id="sec4-4"></a>
### 4.4 Voltage Signal Path: Range, Quantize, Slew
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

Order of the signal path (inside `Afg::process()`), per sample:

1. **Range** (`applyRange()`): scales/shifts the raw slider voltage
   first (Full unchanged, Half compressed to 0–5V, Limited compressed to
   a 2V span plus octave offset).
2. **Quantize** (`quantizeToScale()`), if `MOD_QUANT_OCT` is set: snaps
   the already-scaled result to the nearest note within the selected
   scale+key (Major/Minor: 7-note grid; Chromatic: pure semitone grid,
   Key has no effect). Searches a window of ±12 semitones around the
   naively rounded value for the nearest scale member.
3. **Slew** (`applySlewCurve()`), if `MOD_SLOPED` is set: interpolates
   between `voltageFrom` and the target value `target` based on the
   normalized phase `t = elapsed / stageTime` and the selected curve
   shape:
   - Linear (default): `f(t) = t`
   - Exponential: `f(t) = 1 - (1-t)^power` (fast start, gentle target)
   - Logarithmic: `f(t) = t^power` (gentle start, fast target)

   `power` is computed per stage from the Curve knob position (plus an
   optional Slew Curve CV from the CV expander): center → `SLEW_CURVE_POWER
   = 2.5`, ends of travel → linear (power=1) or strongly curved
   (power≈15.6), a quadratically smoothed transition curve between
   `CURVE_FACTOR_CENTER` and `CURVE_FACTOR_EXTREME`.

   Without `MOD_SLOPED`, `voltageOut` jumps immediately to the target
   value (stepped behavior).
4. **Normalize** (`MOD_SLEW_NORMALIZE`, optional): at extreme curve
   settings, corrects the fact that the theoretically almost-complete
   curve (>99.9%) only reaches the target exactly at `t=1` instead of
   getting "stuck" earlier – stretches the time axis to achieve this
   (`slewExpNormalized()`).

<a id="sec4-5"></a>
### 4.5 Time Signal Path: Time Range, External Time, Time CV, Time Multiplier
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

Each stage's Time knob does **not** store a direct number of seconds, but
a normalized position 0–1 **within** the bracket selected by
`STAGE_TRANGE_PARAM` (TR1–TR4). Resolved in `Marf248::process()` (not in
`Afg`, since `Afg` only knows the final time in seconds):

1. **Time Range** (`getTimeRangeIndex()`) determines
   `[timeRangeMin, timeRangeMax]` from `TIME_RANGE_MIN[]`/`TIME_RANGE_MAX[]`
   (TR1: 0.002–0.03s … TR4: 2–30s; TR3 = 0.2–3s is the default with no bit
   set).
2. **External Time** (`MOD_TIME_EXT`): the (normalized) knob position
   selects the channel A–D (4 equally-sized zones). If the selected
   channel has no patch cable, the time falls back to the fastest value
   in the bracket (per manual specification).
3. **Internal Time**: `resolvedTime[i] = timeRangeMin + stageTime[i] *
   timeRangeSpan`.
4. **Additive stage-time CV** (CV expander, optional): added to the
   resolved time, scaled relative to the span of the currently selected
   Time Range (see [Section 7](#sec7)).
5. Clamp to `[0.001s, 30s]`.
6. **Time Multiplier** (per section, 0.5×–4.0×, modulated via CV +
   attenuverter): scales the already-resolved time **on top**, computed
   separately for Section A (`resolvedTimeA`) and B (`resolvedTimeB`),
   since these values are not shared module-wide.

<a id="sec4-6"></a>
### 4.6 Sustain / Enable / Stop
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

The three hold modifiers pause `elapsed` (the `elapsed += sampleTime`
increment and the `elapsed >= stageTime` comparison both live inside the
same conditional, which also requires `!continuousActive &&
!advanceModeActive`):

- **Stop**: pauses unconditionally.
- **Sustain**: pauses while the start input carries a high level (gate,
  threshold `GATE_THRESHOLD = 2V`, covering both a 10V trigger and a 5V
  hold level). ADSR-like release behavior.
- **Enable**: pauses while **no** high level is present – released only
  by a gate.

After the hold ends, `elapsed` continues exactly from the value at which
it was frozen – the hold time itself does not count toward stage time,
so the real elapsed time until the next stage change is correspondingly
longer. At the end of a Sustain/Enable hold (transition of `statusHeld`
from true to false, without `advanceStage()`/`jumpToStage()` being
called, since `currentStage` stays the same), `regateGen` also fires –
from the perspective of a downstream envelope generator, the stage
"starts" anyway.

If Sustain **and** Enable are set on the same stage simultaneously,
Enable effectively wins (holds) with no cable patched, since both
conditions are evaluated independently and OR-combined into `statusHeld`
– Sustain contributes nothing to holding in this case but also does not
prevent it. **Be sure to see [Section 10](#sec10) for the
counterintuitive behavior of Sustain/Enable on an unconnected start
input.**

STOP/SUSTAIN/ENABLE also take precedence over clock-quantized Advance
(see [4.10](#sec4-10)): a trigger arriving during a hold is simply not
evaluated – no queuing, no catch-up after the hold ends.

<a id="sec4-7"></a>
### 4.7 Pulse Outputs: APOUT, P1/P2OUT, Start/End of Cycle
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

- **APOUT** (`allPulseGen`): fires on **every** stage entry (including
  re-entry without an index change), a 1ms trigger.
- **P1OUT/P2OUT**: fire when the current stage has `MOD_PULSE1`/
  `MOD_PULSE2` set. Detected via **continuous edge detection** (not just
  once on entry): `resetPulseEdgeState()` resets the previous values
  (`pulse1PrevActive`, etc.) to `false` on each (re-)entry; the actual
  firing then happens **every sample** in `Afg::process()` via an edge
  comparison (`bit-active && !prevActive`). This also catches a gate set
  externally via CV (trigger expander) that only goes high mid-stage,
  instead of missing it entirely until the next stage change; toggling
  the gate multiple times during a single stage (e.g. an external clock)
  fires on every rising edge (a deliberate design choice: free-running
  clock operation rather than "only once per visit").
- **Start/End of Cycle** (only available on the trigger expander):
  analogous mechanism for `MOD_FIRST`/`MOD_LAST`, independent of run
  direction. Even on a Pendulum turning-point repeat (the stage is
  deliberately visited twice), each landing fires its own pulse.

<a id="sec4-8"></a>
### 4.8 Preset System (Pickup/Catch)
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

A preset slot (`Marf248::PresetSlot`) stores the complete programming of
all 16 stages: raw (unresolved) Voltage, Time, Range, and Curve knob
values, plus all modifier bits.

- `savePreset(slot)`: copies the current raw knob values + modifiers into
  the slot, marks it as `used`.
- `loadPreset(slot)`: modifier bits are applied **immediately** (purely
  digital state). The four continuous controls are **not** set directly –
  that would make the physical knob value "jump." Instead, **pickup/catch**
  is armed for each control (`PickupState::arm()`).

**`PickupState`** (in `Marf248.hpp`): holds a pending target value
separate from the current knob value. `resolve(currentRaw)` returns,
every frame, either the stored target value (while pickup is pending) or
the physical knob value directly. The crossing is detected via a sign
change in the difference "knob value − target value" between two
consecutive frames (this also handles the case where the knob happens to
already sit on the target value when loading → immediate pickup).

**Important distinction from Address Memory (see [4.9](#sec4-9)):** a
preset stores the **complete** stage programming; Address Memory stores
**only** a stage index as a jump target. Both systems can be used
independently of one another.

<a id="sec4-9"></a>
### 4.9 Address Memory
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

New feature (trigger expander, row 12): a pure jump-bookmark system for
the Stage Address function, independent of the preset system. 16 slots
**per section** (`AddrMemorySlot addrMemA[NUM_STAGES]`,
`addrMemB[NUM_STAGES]`, analogous to `NUM_STAGES`), each slot storing just
`{bool used; int stage;}` – **not** the Voltage/Time/Range/Curve/modifier
values a preset stores.

- **Store** (`ADDR_STORE_A/B_POLY_INPUT`, poly, 16 channels, channel i =
  slot i): a rising edge immediately writes the currently addressed stage
  (`afgA.currentStage`/`afgB.currentStage`) into slot i and marks it as
  `used` – **no arming mechanism**, identical to
  `PRESET_LOAD_POLY_INPUT`.
- **Recall** (`ADDR_RECALL_A/B_POLY_INPUT`, poly, 16 channels): a rising
  edge calls `afgA/B.jumpToStage(addrMemA/B[i].stage, stageModifier)`,
  **but only if `used` is set** – a never-written slot has no effect
  (analogous to the empty-slot protection in the preset system).

Store/Recall are processed **after** `afgA.process()`/`afgB.process()`
within the same frame, so they take effect only in the next sample frame,
but then take precedence over Continuous/Strobe/Int-Ext for that frame –
if Continuous stays active, it automatically overwrites the jump again on
the frame after next (predictable behavior, not a bug).

`performGlobalClear()` leaves Address Memory untouched (like presets);
only `onReset()` (VCV Rack "Initialize") clears both arrays. JSON
persistence: one array of 16 `{used, stage}` objects per section, always
written in full.

**Store/Recall have no panel buttons to click** – the trigger expander is
deliberately a pure jack carrier with no controls of its own; an actual
cable from a trigger/gate source is required (see [Pitfalls](#sec10)).

<a id="sec4-10"></a>
### 4.10 Clock-Quantized Advance
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

New feature (trigger expander, `ADVANCE_A/B_INPUT`, a pure edge-trigger
input, not a level like Run Gate): once patched, `advanceModeActive` takes
complete control of that section's stage change, **independent of**
`running`/`effectiveRunning` – the Start/Stop button and Run Gate are
deliberately ignored while the cable is connected (analogous to
`continuousActive`).

Behavior in detail:
- `elapsed` keeps running (capped at `stageTime[currentStage]`), so
  Slope/TOUT look as if the stage time had elapsed normally – the stage
  sits at the target value/TOUT=0 until the next trigger arrives.
- The actual stage change does **not** depend on this time value at all,
  only on the trigger: if the trigger arrives **before** the stage time
  elapses, the change happens immediately – the trigger always takes
  precedence over the time.
- Skip remains immediately effective, independent of Advance mode.
- STOP/SUSTAIN/ENABLE still take precedence (see [4.6](#sec4-6)): a
  trigger arriving during a hold is ignored, no queuing.
- **Mutually exclusive with Continuous**: while Continuous is active, the
  Advance branch stays inactive, since Continuous addresses the stage
  directly anyway.
- The tri-color RUN/STOP status LED deliberately shows **green** (running)
  while Advance mode is active, unlike Continuous, which continues to
  show red (stopped) – since `advanceModeActive` itself does not change
  `effectiveRunning`, this is evaluated separately.

On the panel, `ADVANCE_A_INPUT`/`ADVANCE_B_INPUT` sit on the trigger
expander in the existing Run Gate row (column C/D), since that space was
still free there (see [Section 6](#sec6)).

<a id="sec4-11"></a>
### 4.11 Scale/Key Quantization
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

Global (module-wide), applies to all stages with `MOD_QUANT_OCT` active,
both sections: `currentKey` (0=C…11=B) and `currentScale` (0=Major,
1=Minor, 2=Chromatic, default). `quantizeToScale()`:

- **Chromatic**: identical to the plain semitone grid
  (`quantizeToSemitone()`), Key has no effect.
- **Major/Minor**: searches a window of ±12 semitones around the naively
  rounded target value for the nearest member of the 7-note scale
  (`SCALE_MAJOR`/`SCALE_MINOR`) relative to the selected key.

Operation: arm the Scale/Key buttons, then the next press on one of the
12 preset buttons selects the value (see [3.4](#sec3-4)).

---

<a id="sec5"></a>
## 5. Marf248 – Function Reference (C++)
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

### `Marf248.hpp`

| Function | Description |
|---|---|
| `applySlewCurve(t, mod, power)` | Applies the selected slew curve shape (exp/log/linear) to the normalized phase `t`, including optional normalization. |
| `slewExpNormalized(t, power)` | Helper for `MOD_SLEW_NORMALIZE`: compresses/stretches the time axis so the curve reaches the threshold exactly at `t=1`. |
| `quantizeToSemitone(v)` | Snaps a voltage to the nearest 1V/oct semitone grid. |
| `quantizeToScale(v, key, scaleMode)` | Quantizes to a Major/Minor/Chromatic scale + key. |
| `applyRange(raw, mod)` | Applies Full/Half/Limited range scaling including octave offset. |
| `voltageToNoteName(v)` | Converts a 1V/oct voltage into a note name (Eurorack convention, 0V=C4). |
| `getTimeRangeIndex(mod)` | Resolves the exclusive `MOD_TRANGE_*` bits to an index 0–3 (TR3 = default). |
| `getRangeOffsetIndex(mod)` / `setRangeOffsetIndex(mod, idx)` | Reads/writes the 3-bit octave-offset field in Limited range mode. |
| `PickupState::arm(target, currentRaw)` | Arms pickup for a new target value (preset load). |
| `PickupState::resolve(currentRaw)` | Returns the effective value to use per frame (target or physical knob value), detects crossing. |
| `Afg::jumpToStage(stage, mod)` | Immediate jump to a specific stage (Reset/Strobe/Continuous/Address Memory recall), ramp start remains the last output value. |
| `Afg::applyStageAddress(continuous, addr, strobe, reset, mod)` | Processes Reset/Continuous/Strobe addressing, see [4.3](#sec4-3). |
| `Afg::findLowestFirst(mod)` | **New**: finds the `MOD_FIRST` stage with the lowest index (independent of `currentStage`) – used by Reset, so that with multiple First/Last segments, the very first segment is always reached. |
| `Afg::findFirstBefore(mod, idx)` / `Afg::findLastAfter(mod, idx)` | Ring search for the nearest `MOD_FIRST`/`MOD_LAST` stage for automatic wrap-around during Forward/Reverse playback. |
| `Afg::advanceStage(mod)` | Core logic for the next stage step (Forward/Reverse/Pendulum, skip handling), see [4.2](#sec4-2). |
| `Afg::resetPulseEdgeState()` | Resets the P1/P2/First/Last edge-detection previous values (on every stage (re-)entry). |
| `Afg::process(...)` | Main per-sample processing of a section: start/stop, Run Gate, Advance, Sustain/Enable/Stop logic, automatic playback, voltage/time output, all pulse outputs. Takes the already-resolved stage arrays (voltage, time, modifier, curve power) plus Key/Scale and Start/Stop/Run-Gate/Advance inputs. |

### `Marf248.cpp`

| Function/Struct | Description |
|---|---|
| `StageVoltageQuantity::getDisplayValueString()` | Tooltip text of the Voltage knob: channel name in External mode, note name with Quantize, otherwise a voltage figure. |
| `StageTimeQuantity::getDisplayValue()` / `setDisplayValue()` | **New**: converts the internal 0–1 raw value to/from the Buchla Cookbook dial convention (0–10) for display and right-click entry – the internal parameter value itself stays 0–1 (no breaking change for existing presets). |
| `StageTimeQuantity::getDisplayValueString()` | Tooltip text of the Time knob: shows the 0–10 dial value leading, followed by the number of seconds exactly resolved against the current Time Range in parentheses (e.g. `4.0 (0.132 s)`). |
| `StageRangeQuantity::getDisplayValueString()` | Tooltip text of the Range knob as plain text ("Full", "Half", "Lim +0"…"+8"). |
| `Marf248::onReset(e)` | Handles VCV Rack's "Initialize": in addition to the standard params, also fully resets modifier bits, both Afg instances, Mode/Pendulum, Stage Address, presets, **Address Memory**, and Scale/Key (fields not stored as Rack params). |
| `Marf248::savePreset(slot)` / `loadPreset(slot)` | See [4.8](#sec4-8). |
| `Marf248::performGlobalClear()` | Resets modifier bits, Range, and Curve on all stages, discards pending pickups, triggers the flash light. Shared between the panel button and the trigger-expander input. Leaves Address Memory untouched. |
| `Marf248::performResetValues()` | Resets only Voltage/Time on all stages to default. Shared between the panel button and the trigger-expander input. |
| `Marf248::performAutoFill()` | **New**: sets the 16 shared Voltage knobs to a linear 0–10V grid and forces `Range = Full` per stage, see [3.4](#sec3-4). |
| `Marf248::process(args)` | Central per-sample processing function. Sequence (see also sections 3–4): trigger-expander lookup (right at the top, for SEXT) → Global Clear/Reset Values/Auto-Fill → preset/scale/key arming logic → stage-modifier click buttons (decimated via `stageButtonDivider`) → stage-modifier poly-CV overrides from the trigger expander (every sample, undecimated) → per-stage resolution of voltage/time/curve (range, external sources, time range, additive CV, curve-power cache) → time-multiplier application → per-section stage-address processing (incl. channel-select CV) → `afgA.process()`/`afgB.process()` calls (incl. Run Gate and Advance) → output writing → trigger-expander actions (Clear/Reset/Preset/**Address Memory Store/Recall**/Scale/Key/Start-End-of-Cycle) → CV-expander gate output → tri-color status LEDs. |
| `Marf248::dataToJson()` / `dataFromJson(rootJ)` | Persists modifier bits, Mode/Pendulum/Stage-Address states, Scale/Key, all 12 preset slots, and **Address Memory** (see [Section 9](#sec9)). |
| `TrangeButton::draw()` | Self-drawing Time Range button (no SVG asset): dark rounded background + digit 1–4 in amber with a dark outline. |
| `TrangeButton::appendContextMenu()` | Right-click checklist for direct TR1–4 selection (manipulates `stageModifier` directly rather than via the momentary button's raw value). |
| `RangeKnob::draw()` | In addition to the standard `RoundBlackKnob`, draws a colored pointer (covering the original SVG pointer) and a text abbreviation of the current state in the center of the knob. |
| `RangeKnob::appendContextMenu()` | Right-click checklist for direct Range selection (standard text entry was found unreliable for this snap-enabled control in practice). |
| `PulseRingLight` | Custom light widget: draws a thin ring around the P1/P2 output jack instead of a separate dot. |
| `Marf248Widget::Marf248Widget(module)` | Builds the complete 120HP panel: frames, column geometry, all knobs/buttons/jacks/lights for external CV, both sections (including the new channel-select CV jack), and the 16 stage columns as well as the preset column (see [3.1](#sec3-1)). |

---

<a id="sec6"></a>
## 6. Marf248TriggerExpander
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

24HP expander (development history: 12HP → 18HP for the stage-modifier
poly CVs → 24HP for Advance). A pure jack carrier with no DSP logic of its
own – all processing happens in `Marf248::process()` once the module is
recognized as a direct left or right neighbor (`LINK_LIGHT` shows the
docking status).

### Inputs

| Input | Type | Function |
|---|---|---|
| `CLEAR_INPUT` | Trigger | Fires `performGlobalClear()` (same as the panel button). |
| `RESET_VALUES_INPUT` | Trigger | Fires `performResetValues()`. |
| `PRESET_LOAD_POLY_INPUT` | Poly trigger (12 channels) | Channel p (0–11) loads preset p+1 **immediately**, with no arming mechanism. Empty slots are ignored. Unused channels return 0V (no trigger). **Channels 13–16 are not evaluated** – the processing loop only runs from `p=0` to `p=11` (see [Pitfalls](#sec10)). |
| `SCALE_MAJOR_INPUT` / `SCALE_MINOR_INPUT` / `SCALE_CHROMATIC_INPUT` | Trigger | Sets `currentScale` directly (no arming required). |
| `KEY_CV_INPUT` | Continuous CV, 0–10V | Sets `currentKey` directly (0–11 = C–B), only effective when actually patched (`isConnected()` check prevents an unwanted fallback to C). |
| `STROBE_A_INPUT` / `STROBE_B_INPUT` ("SEXT") | Trigger | Triggers the same effect as the respective panel Strobe button (OR-combined), including the Continuous lock. |
| `RUN_GATE_A_INPUT` / `RUN_GATE_B_INPUT` | Level (gate), Schmitt hysteresis 0.1V/1V | OR-combined with the Run button state (`effectiveRunning = running \|\| runGateHigh`); the button's own state is preserved when the gate falls back low. **Ignored while Advance is patched on the same section.** |
| `ADVANCE_A_INPUT` / `ADVANCE_B_INPUT` | **New**, pure edge trigger | Clock-quantized Advance, see [4.10](#sec4-10). Sits in the Run Gate row (column C/D). |
| `STAGE_SKIP_POLY_INPUT`, `STAGE_FIRST_POLY_INPUT`, `STAGE_LAST_POLY_INPUT`, `STAGE_STOP_POLY_INPUT`, `STAGE_SUSTAIN_POLY_INPUT`, `STAGE_ENABLE_POLY_INPUT`, `STAGE_PULSE1_POLY_INPUT`, `STAGE_PULSE2_POLY_INPUT`, `STAGE_SLOPE_POLY_INPUT` | Poly gate (16 channels = stage 1–16) | Level override (threshold `GATE_THRESHOLD = 2V`) for the respective modifier bit. Only effective while the jack is patched (`isConnected()`); when active, the CV takes precedence and overrides a manual click processed in the same frame. Without a cable, the state last set by clicking is preserved. **Shape/Lin-Exp-Log is deliberately left without a CV override**, since it is a genuine 3-state cycle rather than a simple bit. |
| `ADDR_STORE_A_POLY_INPUT` / `ADDR_STORE_B_POLY_INPUT` | **New**, poly trigger (16 channels = slot 1–16) | Address Memory Store, see [4.9](#sec4-9). Sits in row 12. |
| `ADDR_RECALL_A_POLY_INPUT` / `ADDR_RECALL_B_POLY_INPUT` | **New**, poly trigger (16 channels = slot 1–16) | Address Memory Recall, see [4.9](#sec4-9). Sits in row 12. |

### Outputs

| Output | Function |
|---|---|
| `START_CYCLE_A/B_OUTPUT` | Fires (1ms trigger) when Section A/B reaches a `MOD_FIRST`-marked stage (continuous edge detection, independent of run direction). |
| `END_CYCLE_A/B_OUTPUT` | Same, for `MOD_LAST`. |

### Panel Layout (24HP, 4 columns spaced 29mm apart, 8.8mm row pitch)

Row 1: Clear / Reset Values. Row 2: Preset Load (poly, centered). Rows
3–4: Scale Major/Minor/Chromatic + Key CV. Rows 5–6: Start/End of Cycle A
and B. Row 7: SEXT A/B. **Row 8: Run Gate A/B (column A/B) + Advance A/B
(column C/D, new)** – thematically related (both concern run control of
the respective section), column C/D was previously free there, so no new
row was needed. Row 9: Stage Skip / First / Last (3 columns). Row 10:
Stage Stop / Sustain / Enable. Row 11: Stage Pulse1 / Pulse2 / Slope.
**Row 12 (new): Address Memory Store A / Store B / Recall A / Recall B**
(4 columns, same principle as row 8) – thanks to the row pitch tightened
to 8.8mm, this row still fits with 6.6mm of clearance before the bottom
mounting screws.

### Important implementation note: continuous vs. one-time edge detection

An earlier bug: `triggerStagePulses()` evaluated the P1/P2/First/Last
bits only **once**, exactly on the entry sample. An externally-set CV
gate that only went high mid-stage was therefore completely missed. Fix:
`resetPulseEdgeState()` now only resets the four edge-detection previous
values; the actual firing has since happened continuously, **every
sample**, in `Afg::process()` via edge detection. Deliberately **not**
decimated via `stageButtonDivider`, since a fast external CV gate could
otherwise be swallowed between two divider ticks (aliasing) – the extra
CPU cost (~10 operations/sample/section) is negligible.

---

<a id="sec7"></a>
## 7. Marf248CvExpander
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

12HP expander (originally 6HP, widened to add the Slew Curve CV column).
Also a pure jack carrier with no DSP logic of its own. Unchanged since the
previous documentation revision.

### Inputs

| Input | Function |
|---|---|
| `STAGE_TIME_CV_INPUT` (poly, 16 channels) | Additive CV per stage on the resolved stage time, **after** External/Internal time resolution, **before** the Time Multiplier (so it is also scaled by the multiplier). A shared attenuverter (`TIME_CV_ATTEN_PARAM`) scales all 16 channels together; ±5V CV at full attenuverter travel covers the full span of the currently active Time Range (TR1–4). |
| `CURVE_CV_INPUT` (poly, 16 channels) | Additive CV per stage on the Curve knob position (slew curve curvature), added before the power calculation. Its own attenuverter (`CURVE_CV_ATTEN_PARAM`), ±5V reference span. |

### Parameters

| Parameter | Function |
|---|---|
| `TIME_CV_ATTEN_PARAM` | Attenuverter for stage-time CV (−1..+1). |
| `TIME_CV_OFFSET_PARAM` | Offset trimmer (−10..+10V), subtracted from the raw CV **before** the attenuverter – makes a unipolar source (e.g. a 0–10V envelope) usable bipolarly. |
| `CURVE_CV_ATTEN_PARAM` | Attenuverter for slew-curve CV (−1..+1). |
| `CURVE_GLOBAL_PARAM` | Global bias knob (−1..+1): always affects all 16 stages, even with no cable patched into `CURVE_CV_INPUT` – unlike `TIME_CV_OFFSET_PARAM`, which has no effect without a cable. |

### Outputs

| Output | Function |
|---|---|
| `GATE_A_OUTPUT` / `GATE_B_OUTPUT` (poly, 16 channels) | Channel i is high (10V) as soon as Section A/B is currently addressing stage i – independent of the run state. Optional retrigger gap (context menu, `regateGapEnabled`, on by default): briefly pulls the gate of the currently addressed stage to 0V (~5ms) when a re-entry without an index change is detected (Pendulum turning point, Strobe onto the same address, Sustain/Enable release), so downstream envelope generators retrigger reliably. **Pitfall:** poly-to-mono conversion of these gates produces a continuously high level due to seamless channel transitions (verified via Bogaudio MONO on a scope) – for trigger purposes, use APOUT instead, see [Section 10](#sec10). |

### Context menu

Right-click on the panel: "Retrigger gap on stage re-entry (~5ms)"
(checkbox, `regateGapEnabled`, persisted via JSON).

---

<a id="sec8"></a>
## 8. Expander Mechanism (Inter-Module Communication)
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

Both expanders detect `Marf248` to their left **or** right
(`getLeftExpander()`/`getRightExpander()`, compared via `getModel() ==
modelMarf248`) and show this via a green `LINK_LIGHT`. As long as no
Marf248 is docked, the expanders reset their own outputs to 0V every
frame (preventing hanging voltages when removed or moved away).

Conversely, `Marf248::process()` looks every frame for a
`Marf248TriggerExpander` or `Marf248CvExpander` as a direct neighbor and
reads/writes that module's parameters/inputs/outputs directly via
`getParam()`/`getInput()`/`getOutput()` – with no message-passing, since
the expanders themselves have no processing of their own. Both expander
types can be docked independently of each other (none, one, or both at
once); if an expander is missing, the associated modulation simply falls
away (e.g. Curve CV/Time CV = 0, no poly-gate overrides, no Advance, no
Address Memory).

External CV inputs are generally handled such that, with unused channels
left unpatched, the behavior without an expander is preserved exactly
(0V on unpatched poly channels, `isConnected()` checks wherever 0V would
otherwise collide with a valid state, e.g. Key CV or the new Stage Address
channel-select CV).

---

<a id="sec9"></a>
## 9. Persistence (Patch Storage)
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

`Marf248::dataToJson()`/`dataFromJson()` persist, in addition to the
params automatically stored by VCV Rack:

- `stageModifier[]` (all 16 stages) as a JSON array.
- `reverseA/B`, `pendulumOnA/B`, `addrExtA/B`, `addrContOnA/B`.
- `currentKey`, `currentScale`.
- All 12 preset slots (`used` flag + Voltage/Time/Range/Curve arrays +
  modifier array per slot).
- **New:** Address Memory – one JSON array of 16 `{used, stage}` objects
  per section (`addrMemA`/`addrMemB`), always written in full (no sparse
  skipping like presets, since the data volume is trivial).

`Marf248CvExpander::dataToJson()`/`dataFromJson()` additionally persists
`regateGapEnabled`.

**Breaking change (Time Range introduction):** since TR1–4 were
introduced, the Time knob stores a normalized position 0–1 instead of
seconds directly. Old patches therefore interpret the stored raw value
differently (e.g. 0.1 used to mean 0.1s, now it means 10% position within
the selected TR bracket) – Time sliders in old patches may need to be
re-set. The display itself (dial value + seconds, see [5](#sec5)) has no
bearing on this, since only presentation, not storage, is affected.

---

<a id="sec10"></a>
## 10. Known Pitfalls in Feature Interaction
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

This section collects every misunderstanding and counterintuitive-but-
**correct** (non-bug) behavior encountered across the development
sessions so far – each with a pointer to where it lives in the code.

### 10.1 Sustain/Enable with no cable at Start behave oppositely

`startVoltage` is the raw voltage at `START_A/B_INPUT`, **regardless** of
`startConnected`. An unconnected input returns 0V in Rack, which is
treated exactly like a permanently LOW gate – not as "gate concept turned
off" or "neutral." That has **opposite** consequences for Sustain and
Enable:

- **Sustain with no cable**: **never** holds (`startGateHigh` is always
  `false`) → the stage appears to play through like a normal stage; the
  Sustain bit has no visible effect at all without a patched gate.
- **Enable with no cable**: **always** holds (`!startGateHigh` is always
  `true`) → the stage stays put forever, no matter how many Advance
  triggers arrive or how much stage time elapses – it looks like a
  hung/broken module, but is exactly the specified "waits for release via
  gate" semantics.

To test/verify: patch a cable (e.g. from a manual trigger/gate button
module) into START_A/B and toggle its gate high/low – only then does the
other, "releasing" behavior show up (Sustain then holds WHILE high, Enable
then holds WHILE low and releases on high). *(Code: `GATE_THRESHOLD`
comment block in `Afg::process()`, `Marf248.hpp`.)*

### 10.2 The Stage Address knob never moves currentStage on its own

The internal Stage Address knob (`STAGE_ADDR_KNOB_A/B_PARAM`) only
supplies a **value**, not an action. Only **Strobe** (one-time jump),
**Continuous** (continuous tracking), or **Reset** actually apply the
address – simply turning the knob (or changing the external address CV)
alone does **not** move `currentStage`. The same holds unchanged for the
new channel-select CV: Ext mode only supplies the address value through
the selected channel, but, just like the internal knob, does not apply it
automatically. Live-testing the 4-channel selection therefore additionally
requires **Continuous**, so the section actually follows the selected
source continuously.

### 10.3 Address Memory Store/Recall have no panel buttons

`ADDR_STORE/RECALL_A/B_POLY_INPUT` are pure CV/trigger jacks on the
trigger expander with **no** associated panel button to click. Store and
Recall only work via an actual cable patched from a trigger/gate source –
a mouse click is deliberately not supported here, since the trigger
expander is conceptually a pure jack carrier with no controls of its own.

### 10.4 Channel number on Store/Recall = slot, not stage

The channel number (1–16) on the `ADDR_STORE`/`ADDR_RECALL` poly input
determines **only** the slot number (the "storage bin"), and has
**nothing** to do with the currently addressed stage number. Example:
stage 12 can deliberately be stored in slot 1 – channel and stage are
completely independent numbers. A mono cable into one of these poly jacks
automatically lands on channel 1 (= slot 1).

### 10.5 Preset-Load poly: channels 13–16 are ignored

The processing loop for `PRESET_LOAD_POLY_INPUT` only runs from `p = 0`
to `p = 11` (12 presets). A trigger on channel 13–16 is **never**
evaluated (`getPolyVoltage()` for those channels isn't even queried) –
not a malfunction, simply outside the valid range. The `configInput`
tooltip text ("poly, ch. 1-12 = preset 1-12") describes this correctly.

### 10.6 Continuous + Advance + timer auto-advance are mutually exclusive

Each section has three mechanisms that can change `currentStage`: normal
timer auto-advance, Continuous addressing, and clock-quantized Advance.
All three are explicitly decoupled from one another in the code via
mutual `!continuousActive`/`!advanceModeActive` guards: Continuous always
takes precedence (timer and Advance both stay inactive while Continuous is
running), while Advance for its part completely ignores Start/Stop and
Run Gate as long as the Advance jack is patched. Anyone trying to use, say,
Run Gate and Advance simultaneously to control the same section will find
that Advance always wins.

### 10.7 CV Expander gates: poly-to-mono conversion yields a constant high level

The polyphonic gate outputs (`GATE_A/B_OUTPUT`) on the CV expander are
designed so that exactly one channel (the currently addressed stage) is
high. When converting to a mono signal (e.g. via Bogaudio MONO), the
seamless transitions between the 16 channels instead produce a
**continuously high level** rather than discrete trigger pulses (verified
on a scope) – for pure trigger purposes, **APOUT** is the right choice
instead, delivering a genuine 1ms pulse on every stage change.

### 10.8 Start/Stop has no effect during Continuous addressing

`applyStageAddress()` forces `running = false` in the Continuous branch,
regardless of the state of the Start/Stop button or pulse. A Start button
pressed while Continuous addressing is active therefore appears to have no
effect – this is intentional (Continuous addresses the stage directly, a
timer concept doesn't apply there), not broken. The `addrContOnA/B` state
is also persisted via JSON: a preset saved with Continuous active
reactivates Continuous automatically on load.

### 10.9 Multiple First/Last segments: Reset always jumps to the first segment

With multiple First/Last pairs in the 16-stage field, Reset **always**
jumps to the `MOD_FIRST` stage with the lowest index (`findLowestFirst()`),
regardless of which segment `currentStage` currently sits in. This is
deliberately different from the wrap logic of automatic playback
(`findFirstBefore()`), which instead wraps locally to the nearest First
point. Anyone wanting to switch between segments must do so actively via
external stage-address CV combined with Strobe or Continuous – a simple
Reset press always returns to the very first segment.

---

<a id="sec11"></a>
## 11. Design Decisions & Scope Boundaries
*[↑ Back to Table of Contents](#toc) · [[#Table of Contents|Obsidian]]*

- **Skip, Normalize, Reset Values, Auto-Fill, Global Clear flash light**:
  pure software extensions with no direct hardware precedent.
- **Shape (linear/exp/log cycle) is deliberately left without a CV
  override**, since it is a genuine 3-state cycle (unlike all the other
  simple on/off bits, including Slope, which can be overridden via poly
  CV).
- **`Afg` has no direct knowledge of inputs/ports** – External
  Voltage/Time sources and the Stage Address channel selection are
  resolved centrally in `Marf248::process()`, and `Afg` only receives the
  already-resolved values. This keeps the core logic independent of the
  module-wide I/O layout.
- **Stage Address CV and per-stage External Voltage are independent
  mechanisms** – address inputs deliberately live on the main module and
  not on the expander, so the core function does not depend on an
  optional module.
- **Address Memory is not a preset replacement** – it deliberately stores
  only a stage index as a jump target, not a complete programming (a
  clear separation of concepts, see [4.8](#sec4-8)/[4.9](#sec4-9)).
- **Auto-Fill is global only, not per section** – since
  `STAGE_VOLTAGE_PARAM` and `STAGE_RANGE_PARAM` are shared module-wide
  between Section A and B, a separate A/B variant would only be possible
  with a larger structural rework.
- **APOUT is a pure trigger, not a gate** – for envelopes, an AD envelope
  is a better fit than an ADSR.
- **TR3 is the implicit default** (no bit set), following the same
  pattern as the Range/Shape/Slope defaults.
- **Gate threshold is uniformly 2V** (not the manual-specified 5V for
  Enable) – a deliberate project decision, potentially subject to later
  review.
- **No SIMD**: not currently applicable given the lack of polyphony and
  heavily branching per-stage code; a future polyphony extension would
  be the natural trigger for revisiting this.
- **ART outputs and Autotune** (from the Python emulator) are explicitly
  **not** part of the C++ port.
- **Advance and Continuous are deliberately mutually exclusive** – both
  mechanisms address the stage in conceptually different ways
  (trigger-based vs. direct-addressing), and combining them would be
  semantically contradictory.
