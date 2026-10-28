# Kinovice reminiscence of the unattained – Europa (Technical Documentation)

**Europa** is the fifth module of the plugin: a quad function generator with VCA.
Four independent envelope generators with four stages each (Delay, Attack, Hold,
Release), each one both a VCA and an envelope output. The interface shows only
one generator at a time, all four keep running in the background. This
documentation covers every control, every input/output, the internal signal
processing, the C++ structure and the design decisions.

The name follows the Anguissola family (like Sofonisba, Amilcare, Bianca and
Lucia): Europa is one of the sisters.

**Notice:** The *Kinovice reminiscence of the unattained* plugin is an independent work, heavily inspired by the wonderful Buchla world of modules. It is not affiliated with, authorized by, or endorsed by Buchla, Tiptop Audio, or any other manufacturer. Product and company names mentioned are trademarks of their respective owners and are used only to describe which functional concepts inspired a module.

Status: 2026-10-06. All functions described here have been tested by Kinovice in
a running Rack instance, except where explicitly marked open (see [Section 6](#sec6)).

---

<a id="toc"></a>

## Table of Contents

*Each entry has two links: the first (standard Markdown anchor) works in GitHub, Codeberg, VSCodium preview & most other viewers; "Obsidian" is the wikilink specifically for Obsidian.*

1. [Overview](#sec1) · [[#1. Overview|Obsidian]]
2. [Controls](#sec2) · [[#2. Controls|Obsidian]]
   - [Panel Layout](#sec2-1) · [[#2.1 Panel Layout|Obsidian]]
   - [Generator Selection (GEN and ACTIVATE)](#sec2-2) · [[#2.2 Generator Selection (GEN and ACTIVATE)|Obsidian]]
   - [Generator Switches (MODE, RETRIG D, LOOP)](#sec2-3) · [[#2.3 Generator Switches (MODE, RETRIG D, LOOP)|Obsidian]]
   - [Stage Controls (LEVEL, TIME, RANGE, CURVE)](#sec2-4) · [[#2.4 Stage Controls (LEVEL, TIME, RANGE, CURVE)|Obsidian]]
   - [Inputs and Outputs](#sec2-5) · [[#2.5 Inputs and Outputs|Obsidian]]
   - [OR Outputs and Quadrature Buttons](#sec2-6) · [[#2.6 OR Outputs and Quadrature Buttons|Obsidian]]
   - [Context Menu (Antialiasing, Ratio Change)](#sec2-7) · [[#2.7 Context Menu (Antialiasing, Ratio Change)|Obsidian]]
   - [Phasor Mode (Controls)](#sec2-8) · [[#2.8 Phasor Mode (Controls)|Obsidian]]
3. [Internal Signal Processing](#sec3) · [[#3. Internal Signal Processing|Obsidian]]
   - [State Machine per Generator](#sec3-1) · [[#3.1 State Machine per Generator|Obsidian]]
   - [Time Signal Path](#sec3-2) · [[#3.2 Time Signal Path|Obsidian]]
   - [Level Signal Path](#sec3-3) · [[#3.3 Level Signal Path|Obsidian]]
   - [Curve Shape](#sec3-4) · [[#3.4 Curve Shape|Obsidian]]
   - [VCA](#sec3-5) · [[#3.5 VCA|Obsidian]]
   - [EOC Output](#sec3-6) · [[#3.6 EOC Output|Obsidian]]
   - [Status LEDs](#sec3-7) · [[#3.7 Status LEDs|Obsidian]]
   - [Quadrature Coupling and OR Logic](#sec3-8) · [[#3.8 Quadrature Coupling and OR Logic|Obsidian]]
   - [Antialiasing (minBLEP)](#sec3-9) · [[#3.9 Antialiasing (minBLEP)|Obsidian]]
   - [Phasor Mode (Signal Processing)](#sec3-10) · [[#3.10 Phasor Mode (Signal Processing)|Obsidian]]
4. [Function Reference (C++)](#sec4) · [[#4. Function Reference (C++)|Obsidian]]
5. [Persistence](#sec5) · [[#5. Persistence|Obsidian]]
6. [Known Pitfalls and Open Items](#sec6) · [[#6. Known Pitfalls and Open Items|Obsidian]]
7. [Patch Ideas](#sec7) · [[#7. Patch Ideas|Obsidian]]
8. [Design Decisions & Boundaries](#sec8) · [[#8. Design Decisions & Boundaries|Obsidian]]

---

<a id="sec1"></a>

## 1. Overview
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Structure**<br>
Four generators with four stages each: **Delay, Attack, Hold, Release**. Each generator has a VCA (signal in, signal out) and also outputs the envelope separately (0–10 V). The module is 23HP wide (116.84 × 128.5 mm), with the OR column to the right of the main block.

**One generator visible, all active**<br>
The controls always show only the selected generator. A selector knob (GEN) with an activate button (ACTIVATE) switches between them. All four generators keep running independently, their jacks are always visible.

**Two modes and loop**<br>
Per generator: One-Shot or Sustain, plus a three-position Loop switch (Off / Gate / Free, effective in One-Shot only) and a Retrig switch that decides whether a retrigger starts from the current level or from the Delay level.

**Main use**<br>
Envelopes, trapezoid/AR/gate-like shapes and above all LFO operation (loop; LFO into the audio range as a bonus). Adjustable Delay and Release levels and CV control of all levels also make offset LFOs and wavefolder-like effects possible (see [Section 7](#sec7)).

**Extensions (2026-10-01)**<br>
Free-running cycle (LOOP = FREE, loop without a gate), rate CV (channel 5 of the TIME CV input, bipolar, exponential, shape-preserving), two OR outputs (maximum per pair 1+2 and 3+4) and a switchable quadrature coupling per pair (generator 2 or 4 starts at the end of the attack of generator 1 or 3). See [2.3](#sec2-3), [2.5](#sec2-5), [2.6](#sec2-6), [3.2](#sec3-2) and [3.8](#sec3-8).

**Extension (2026-10-02)**<br>
Optional antialiasing (minBLEP) for the hard level jumps of the envelope, switchable in the right-click menu, default off. See [2.7](#sec2-7) and [3.9](#sec3-9).

**Extension (2026-10-05)**<br>
The quadrature button has a third position: the **handshake**. Both generators of a pair wait for each other at the end of their attacks and couple their decays, giving a true quarter period between the two envelopes. See [2.6](#sec2-6) and [3.8](#sec3-8). The functional test in Rack is still pending (see [Section 6](#sec6)).

**Extension (2026-10-06)**<br>
**Phasor mode** per generator: the T1/GATE jack takes a **phasor** (0–10 V ramp) instead of a gate, and the four stage times become weights that divide one cycle into Delay, Attack, Hold and Release. Envelopes, phasor warps and sequencers stay locked to an external phasor with no time base of their own. Added to this are a free ratio p/q, an offset, direction detection (falling ramps also run as rise before fall), a reset, a phasor output on ENV and CV for curve, offset and ratio. See [2.8](#sec2-8) and [3.10](#sec3-10).

**Engine**<br>
Own Rack-free class `EnvGen4` (`EnvGen4.hpp`, only `<cmath>`/`<algorithm>`), four instances inside the `Europa` module (`Europa.cpp`). Sofonisba (`Afg`) is untouched.

---

<a id="sec2"></a>

## 2. Controls
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

<a id="sec2-1"></a>

### 2.1 Panel Layout
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

The panel consists of four areas from top to bottom (dimensions in mm, panel 116.84 × 128.5, i.e. 23 HP; the main block measures 101.6 mm, the OR column attaches on the right):

```
EUROPA
[ GEN knob ] [ ACTIVATE ]  |  MODE        RETRIG D      LOOP
                              SUSTAIN      ON            FREE
                                                         GATE
                              ONE-SHOT     OFF           OFF

 DELAY       ATTACK      HOLD        RELEASE      (dark name band)
 [Level]     [Level]     [Level]     [Level]
 [Time]      [Time]      [Time]      [Time]
 [Range]     [Range]     [Range]     [Range]
 [Curve]     [Curve]     [Curve]     [Curve]

 T1/GATE TRIG TIME CV LEVEL CV HOLD CV VCA IN | VCA OUT ENV EOC | OR   (outputs on dark plate, OR column on the right)
 1 ...                                                     (row for generator 1)
 2 ...
 3 ...
 4 ...
```

**Header area (y 10.5–29.5)**<br>
On the left a frame with the GEN knob (14 mm diameter, center x 14) and the ACTIVATE button (center x 31). On the right a frame with the three generator switches (center x 51.5 / 72.5 / 89.5).

**Stage columns (y 31–83.3)**<br>
Four frames, 22 mm wide, at x 13.3 / 38.3 / 63.3 / 88.3, each with a dark name band. Rows from the top: LEVEL (trimpot), TIME (knob), RANGE (switch), CURVE (trimpot).

**Jack block (y 84.6–126.3)**<br>
Four rows (one per generator) at y 93 / 102.3 / 111.6 / 120.9, nine columns spaced 10 mm apart starting at x 13. The inputs (T1/GATE, TRIG, TIME CV, LEVEL CV, HOLD CV, VCA IN) sit on the left on a light background, the outputs (VCA OUT, ENV, EOC) on the right on a dark plate. At the left of each row are the status LED, the selection LED and the generator number 1–4.

**OR column (x 101.5–114.5, y 84.6–126.3)**<br>
An additional column to the right of the main block, designed like the jack columns: a dark plate 10 mm wide inside a light frame with a narrow margin (1.5 mm at the sides, 0.8 mm top and bottom), 2.3 mm from the panel edge on the right. Two blocks one above the other, each pair with a label (OR 1+2, OR 3+4), a jack and a three-position click button with an LED for the quadrature coupling. See [2.6](#sec2-6).

**PHASOR strip (x 101.5–114.5, y 10.5–82.6)**<br>
Above the OR column, in its width: a light frame with the controls of the phasor mode for the displayed generator. From the top: PHASOR switch, OUT button, NUM (numerator), DEN (denominator), OFFSET, with the yellow pending LED beside them. The strip uses the previously empty area, so the panel stays 23 HP wide. See [2.8](#sec2-8).

**Row highlight**<br>
The row of the currently displayed generator has a blue band.

<a id="sec2-2"></a>

### 2.2 Generator Selection (GEN and ACTIVATE)
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**GEN knob**<br>
Dragging vertically preselects the number 1–4 (`pendingGen`), 4 mm of drag per step. The number in the knob is green while the preselection matches the displayed generator, amber otherwise.

**ACTIVATE button**<br>
Accepts the preselection (`displayedGen = pendingGen`). Green when preselection and display match, red otherwise. Only then do all controls switch to the new generator.

**Selection LEDs**<br>
One blue LED per row: bright for the displayed generator, dim for a merely preselected one.

`pendingGen`/`displayedGen` are deliberately **not Rack params**: undo/history, automation and preset recall should not affect pure navigation. They are stored via JSON ([Section 5](#sec5)).

<a id="sec2-3"></a>

### 2.3 Generator Switches (MODE, RETRIG D, LOOP)
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

MODE and RETRIG D are toggle switches (CKSS) with two positions: **up = on**, down = off. When a switch is up, a green field sits behind the upper text. LOOP is a three-position switch (`CKSSThree`): FREE at the top, GATE in the middle, OFF at the bottom, the active position is marked in green (OFF stays unmarked). All three switches belong to the displayed generator.

**MODE** (`MODE_PARAM`)<br>
Up: SUSTAIN, down: ONE-SHOT (default). See [3.1](#sec3-1).

**RETRIG D** (`RETRIG0_PARAM`)<br>
Up: ON. A retrigger (rising edge at T1/GATE while running) then starts from the **Delay level** instead of the current output level. The output jumps to the Delay level on retrigger (possibly audible step). Off (default): start from the current level, no click. Affects T1/GATE only, jump triggers at TRIG always start from the current level. The internal name `RETRIG0_PARAM` remains unchanged for historical reasons (formerly "Retrig 0": jump to 0 V).

**LOOP** (`LOOP_PARAM`, 0..2, default 0)<br>
Three positions, effective in One-Shot only, no function in Sustain mode. **OFF** (bottom, 0): no loop, one pass per gate. **GATE** (middle, 1): loop while the gate is held, as long as the gate stays high a new cycle starts automatically after Release (the behavior of the former switch in its ON position). **FREE** (top, 2): free-running cycle (auto-cycle), the generator starts by itself without a gate and loops continuously. A gate is not required but still acts as a retrigger. A switch saved earlier in the ON position loads in the middle (GATE). See [3.1](#sec3-1).

<a id="sec2-4"></a>

### 2.4 Stage Controls (LEVEL, TIME, RANGE, CURVE)
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**LEVEL** (trimpot, 0–10 V)<br>
Target level of the stage. All four stages have a level: `LEVEL_DELAY_PARAM` (default 0 V), `LEVEL_ATTACK_PARAM` (default 10 V), `LEVEL_HOLD_PARAM` (default 10 V), `LEVEL_RELEASE_PARAM` (default 0 V). Unipolar, see [3.3](#sec3-3). Additionally controllable via LEVEL CV.

**TIME** (knob, `TIME_PARAM`, 0..1, default 0.5)<br>
Knob position which, together with the RANGE switch, gives the stage time (exponential mapping, exactly 0 at the left stop). The tooltip and the right-click field show the time in seconds. Input: `5 ms`, `1.5 s`, `200 us`. A number without a unit takes the unit of the currently displayed value (below 1 s: ms, otherwise s). Values above the range maximum are clamped to the stop. See [3.2](#sec3-2).

**RANGE** (switch, `RANGE_PARAM`, 0..3, default TR3)<br>
Click advances to the next range (TR1 → TR2 → TR3 → TR4 → TR1), Shift+click goes backwards. Right-click opens Rack's standard menu, which contains the four ranges as a selection list (`RANGE_PARAM` is a `configSwitch` with labels). Undo/redo are supported. Ranges (all starting at 0): TR1 0–30 ms, TR2 0–300 ms, TR3 0–3 s, TR4 0–30 s.

**CURVE** (trimpot, `CURVE_PARAM`, −1..+1, default 0)<br>
Curve shape of the stage. Left = round, center = linear, right = pointed, each relative to the direction of movement. See [3.4](#sec3-4).

<a id="sec2-5"></a>

### 2.5 Inputs and Outputs
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

Nine jacks per generator, 36 in total, plus the two OR outputs (see [2.6](#sec2-6)). All jacks are always visible regardless of the displayed generator.

**T1/GATE** (`GATE_INPUT`, mono)<br>
Starts the envelope at stage 1 (Delay). One-Shot: trigger (edge), Sustain: gate (level). Schmitt trigger with 0.1 V / 1 V hysteresis.

**TRIG** (`TRIG_INPUT`, poly)<br>
Jump triggers: channel 2 = Attack, channel 3 = Hold, channel 4 = Release, channel 1 unused. Rising edge above 2 V, no hysteresis. A mono cable lands on channel 1 and does nothing. See [3.1](#sec3-1).

**TIME CV** (`TIMECV_INPUT`, poly)<br>
Channels 1–4 = time CV for Delay, Attack, Hold, Release, additive to the knob position. **Channel 5 = rate CV** (bipolar, exponential, one octave per volt, limited to ±5 V): changes the speed of the whole generator without changing the curve shape. A mono cable affects Delay only, channel 5 is only present with a cable of at least five channels. See [3.2](#sec3-2).

**LEVEL CV** (`LEVELCV_INPUT`, poly)<br>
Channels 1–4 = level CV for Delay, Attack, Hold, Release, additive to the knob (1 V = 1 V), sum limited to 0–10 V. A mono cable affects the Delay level only. See [3.3](#sec3-3).

**HOLD CV** (`HOLDCV_INPUT`, mono)<br>
Its own jack for the Hold time, independent of TIME CV channel 3. Only effective with a cable patched **and** in One-Shot (Loop included), no effect in Sustain mode. When patched, the voltage fully replaces the TIME knob of the Hold stage, only the RANGE switch (TR1–TR4) still applies: 0 V = no hold, rising up to just below 9.5 V within the chosen range, 9.5 V and above means infinite hold (ends only via a trigger at TRIG channel 4). No relation to the gate. Unpatched, the Hold stage stays unchanged (knob + TIME CV channel 3). See [3.1](#sec3-1) and [3.2](#sec3-2).

**VCA IN** (`SIGNAL_INPUT`, poly)<br>
VCA signal input, normalled to 10 V when unpatched. See [3.5](#sec3-5).

**VCA OUT** (`VCA_OUTPUT`, poly)<br>
Signal times envelope, channel count as VCA IN (mono when unpatched).

**ENV** (`ENV_OUTPUT`, mono)<br>
Envelope, 0–10 V.

**EOC** (`EOC_OUTPUT`, mono)<br>
Trigger (10 V) at the end of the cycle. See [3.6](#sec3-6).

**Phasor mode: changed assignments**<br>
When a generator is in phasor mode ([2.8](#sec2-8)), its jacks behave as follows:
- **T1/GATE:** phasor input, 0–10 V, instead of trigger/gate. With no cable the generator takes the phasor of the previous generator, provided that one is also in phasor mode (one cable into generator 1 feeds all four). Generator 1 has no predecessor, with no cable it sees 0 V.
- **TRIG:** channel 1 is the **reset** of the cycle counter (rising edge above 2 V). The stage triggers (channels 2–4) have no effect in phasor mode.
- **TIME CV:** channels 1–4 act as stage weights (like times). **Channel 5 = offset**, **channel 6 = numerator p**, **channel 7 = denominator q** (instead of the rate CV), each added to the knob, 10 V equals the full knob travel. The cable needs at least 5, 6 or 7 channels for this.
- **LEVEL CV:** channels 1–4 as before. **Channels 5–8 = curve** of Delay, Attack, Hold, Release (added, ±5 V equals the full range −1…+1, sum limited to −1…+1). The curve CV works in **both** modes, also in classic operation.
- **HOLD CV:** replaces the weight of the Hold stage, also in sustain mode, never infinite.
- **ENV:** envelope or phasor, depending on the OUT button. **VCA OUT** and the **OR outputs** always use the envelope.
- **EOC:** end of the cycle of y (see [3.10](#sec3-10)).

<a id="sec2-6"></a>

### 2.6 OR Outputs and Quadrature Buttons
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**OR 1+2, OR 3+4** (`OR_OUTPUT`, two outputs, mono)<br>
Deliver the **higher of the two ENV levels** of a pair (generators 1+2 and 3+4), 0–10 V, taken before the VCA. They work independently of which generator is currently displayed. See [3.8](#sec3-8).

**Quadrature buttons** (`QUAD_PARAM`, two params with three positions each, 0..2)<br>
Click button with an LED below the respective OR jack, default Off. Click advances (Off → Follow → Handshake → Off), Shift+click goes backwards. Right-click opens Rack's standard menu with the three named positions. Undo/redo are supported. The LED shows the position: dark = **Off**, green = **Follow**, amber = **handshake**. The buttons belong to the pair, not to the displayed generator.

**Off**<br>
The generators of the pair are independent.

**Follow**<br>
**Generator 2 (or 4) follows generator 1 (or 3)**: it starts at the end of the partner's attack and ignores its own T1/GATE. The decays of both generators run independently of each other.

**Handshake**<br>
Both generators wait for each other at the end of their attacks and couple their decays (model: the quadrature mode of a classic quad function generator). Gate at T1/GATE of generator 1, generator 2 starts at the attack end of generator 1. The exact sequence is described in [3.8](#sec3-8).

See also [3.8](#sec3-8).

<a id="sec2-7"></a>

### 2.7 Context Menu (Antialiasing, Ratio Change)
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Antialiasing (minBLEP, ENV/VCA/OR)** (`Europa::antialias`, not a Rack param)<br>
Checkbox in the module's right-click menu, **module-wide for all four generators**, default **off**. When on, the module smooths the level jumps of the envelope (a stage with time 0, the retrig jump) at ENV, at the VCA and at the OR outputs. EOC, gate logic and the quadrature start are untouched. The switch is deliberately not a param (undo, automation and preset recall should not affect it) and is stored via JSON ([Section 5](#sec5)). For how it works and its limits see [3.9](#sec3-9).

**Phasor: ratio change loose (every input wrap)** (`Europa::ratioLoose`, not a Rack param)<br>
Checkbox in the right-click menu, **module-wide for all generators in phasor mode**, default **off** (strict transfer). When on, each generator takes over a new ratio p/q at every input wrap, not only at a wrap where y lies exactly on the start of the cycle. y jumps at that moment. The jump is reported as an envelope jump and smoothed when antialiasing is on. The switch is saved via JSON ([Section 5](#sec5)), old patches load strict. Details in [3.10](#sec3-10).

<a id="sec2-8"></a>

### 2.8 Phasor Mode (Controls)
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

Phasor mode turns a generator into an **envelope whose timing is entirely controlled by an external phasor**. The generator then has no time of its own: everything is a pure mapping of the input phase, nothing can drift, and envelopes, sequences and modulations stay locked to the beat. The controls sit in the PHASOR strip above the OR column ([2.1](#sec2-1)) and belong to the displayed generator. All settings are real Rack params (five per generator). How it works is described in [3.10](#sec3-10).

**PHASOR** (`PHASOR_PARAM`, toggle switch, default off)<br>
Up: phasor mode on. The generator then behaves as described in [3.10](#sec3-10). Down (default): the generator works exactly as before, all old patches stay unchanged. Switching the mode resets the engine of this generator. The mode is per generator, so two generators can run as classic envelopes and two as phasor envelopes.

**OUT** (`PHASOR_OUT_PARAM`, click button with three positions 0..2, default 0)<br>
Selects what the ENV jack outputs in phasor mode. Click advances (envelope → linear → warped), Shift+click goes backwards, right-click opens Rack's standard menu with the three named positions. Undo/redo are supported. The LED shows the position: dark = **envelope**, green = **phasor linear**, amber = **phasor warped**. The widget is the same as the quadrature button (`EuropaQuadButton`), with its own undo name.
- **Envelope:** ENV outputs the envelope (0–10 V).
- **Phasor linear:** ENV outputs y, 0–10 V, a uniform ramp with ratio and offset applied.
- **Phasor warped:** ENV outputs `(k + curve(u)) / 4`. Each of the four segments takes one quarter, so the weights determine how fast the input phase moves through the four quarters, and the curve knobs shape the course inside each segment. This is the phasor for downstream modules and generators.

Both phasor outputs are **direction-true**: a falling input ramp gives a falling output ramp. VCA OUT and the OR outputs keep the envelope in all positions.

**NUM** and **DEN** (`RATIO_P_PARAM`, `RATIO_Q_PARAM`, snapped trimpots, 1..16, default 1)<br>
Numerator p and denominator q of the ratio p/q. 2/1 runs twice as fast as the input, 1/2 half as fast, 3/2 one and a half times as fast. The values are reduced (8/8 is 1/1). The tooltip shows the value. CV-controllable (TIME CV channels 6 and 7). When a new ratio is taken over is described in [3.10](#sec3-10).

**OFFSET** (`PHASE_OFFSET_PARAM`, 0–100 %, default 0)<br>
Phase shift of the cycle of y in fractions of a cycle, forward in the direction of travel. CV-controllable (TIME CV channel 5), the value wraps around as a phase.

**Pending LED** (`PENDING_LIGHT`, yellow)<br>
Lights while a change of p or q is waiting for a valid input wrap. In strict mode this can take several input cycles, in loose mode only until the next wrap.

**Without effect in phasor mode**<br>
MODE (sustain), RETRIG D, LOOP, the rate CV (channel 5 of the TIME CV input is the offset there) and the stage triggers at TRIG (channels 2–4). The quadrature buttons are not intended for a generator in phasor mode ([Section 6](#sec6)).

**Quick start**<br>
1. Phasor source (0–10 V ramp, rising or falling) into T1/GATE of the generator. With several generators in phasor mode one cable into generator 1 is enough.
2. Switch PHASOR on.
3. Set the times (TIME and RANGE) as **fractions** of the cycle: equal times give four equally long segments, unequal times move the segment boundaries.
4. Levels and curve as for a normal envelope.
5. Set NUM/DEN and OFFSET. A reset at TRIG channel 1 fixes the counter position.
6. Choose OUT: ENV as envelope or as phasor.

---

<a id="sec3"></a>

## 3. Internal Signal Processing
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

<a id="sec3-1"></a>

### 3.1 State Machine per Generator
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**One-Shot**<br>
A rising edge at T1/GATE starts Delay → Attack → Hold → Release, then rest (output at the Release level, EOC pulse). A trigger at TRIG jumps to the chosen stage and runs to the end of the cycle, independent of the gate.

**Sustain**<br>
Gate high starts Delay, then Attack, then **holds** at the Attack level. Hold is skipped, the gate replaces it. (If Hold was entered by a TRIG jump, the envelope holds at the Hold level at the end of Hold.) Gate low starts Release from the current level. Gate low during Delay, while the output is still at the rest level, aborts without Release and without EOC. Triggers at TRIG only act while the gate is high. Infinite hold via time CV is ignored in Sustain mode.

**Internal loop (One-Shot)**<br>
At the end of Release the gate level is checked. Gate high: Delay follows in the same sample, the time remainder carries over. Gate low: end. A short trigger alone gives a single pass, the gate must be held for the loop. The loop has no cable latency, the period is exactly the sum of the stage times. EOC fires at every cycle end, also in the loop.

**Free-running cycle (LOOP = FREE)**<br>
One-Shot only. An idle generator starts by itself through `startCycle()`, i.e. with the same behavior as a gate (Retrig switch, Delay level). After Release the next cycle begins immediately, even without a gate: `loopEnabled` and `freeCycle` are set, the loop runs on `gateHigh || freeCycle`. A gate still acts as a retrigger. If FREE is switched off (GATE or OFF position), the running cycle ends normally at the end of Release. The output does not jump and rests at the Release level afterwards. With infinite hold the generator stays put until a trigger arrives at TRIG. EOC fires at every cycle end. With all times 0 the cycle runs once per sample (degenerate case, stable).

**Patch loop**<br>
EOC → T1/GATE of the same generator. The pulse starts the next pass, pulling the cable stops after the current pass. The cable has 1 sample of latency. Without Delay, use EOC → TRIG channel 2. Also suited for chains and rings across several generators. Not useful in Sustain mode (the gate is a level).

**Retrigger**<br>
A rising edge at T1/GATE while running restarts stage 1. RETRIG D off: from the current level, on: from the Delay level.

**Hold duration via CV (One-Shot/Loop only)**<br>
If HOLD CV is patched, the voltage alone determines the length of the Hold stage, independent of the gate: 0 V means no hold (the stage runs through with time 0), rising up to infinite hold at 9.5 V. The Hold stage's TIME knob is then ineffective, the RANGE switch still sets the scale. In Sustain mode HOLD CV is ignored, there the gate determines the hold duration as before.

**Time 0**<br>
Any stage can have time 0 (knob left or CV). Such a stage is run through in the same sample, time remainders carry over into the next stage. Delay 0: Attack starts immediately. Attack 0: step to the Attack level. Release 0: immediate jump to the Release level. With time 0 in all stages, at most four stage transitions per sample are allowed (minimum cycle 1 sample) so the loop cannot lock up.

**Infinite hold**<br>
Time CV ≥ 9.5 V on a stage holds it indefinitely until a trigger at TRIG starts the next stage (One-Shot only). The output stays at the start level of the stage.

**Mode change while running**<br>
If the switch changes from Sustain to One-Shot during the hold phase, the hold ends and the envelope continues normally.

<a id="sec3-2"></a>

### 3.2 Time Signal Path
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Knob mapping**<br>
`t = max · (B^x − 1) / (B − 1)` with `x` = knob position (0..1) plus time CV/10 V (clamped to 0..1), `B = ENV_TIME_EXP_BASE = 1000` and `max` = upper limit of the range. At `x = 0` the time is exactly 0. The knob center of TR1 lies at about 0.9 ms. The inverse function `envPositionFromTime()` is used for text entry.

**Time CV**<br>
Additive to the knob position, 10 V corresponds to the full travel. The sum is clamped to 0..1. ≥ 9.5 V (the CV alone) means infinite hold (not in Sustain mode).

**Hold CV (special case)**<br>
When HOLD CV is patched in One-Shot/Loop, the Hold time is not additive but computed directly from the voltage: the same function as above, but with the knob position fixed at 0 and the voltage as the sole input (`envResolveTime(0, cv, range, true)`), so the voltage alone covers the full 0..1 knob position. Its own cache, independent of the time-knob cache.

**Rate CV (channel 5 of TIME CV)**<br>
Channel 5 of the polyphonic TIME CV input does not act on a stage time but on the speed of the whole generator. Factor = `2^(U · ENV_RATE_OCTAVES_PER_VOLT)`, with `U` limited to ±`ENV_RATE_CV_LIMIT_V` (values one octave per volt and 5 V, giving 1/32 to ×32; `envRateFromVolts()`). 0 V gives a factor of 1, positive is faster, negative slower. The rate only scales the time progress in `EnvGen4::process()` (`elapsed += dt · rate`). Because the phase of a stage follows from `elapsed` divided by the stage time, the curve shape is preserved at every rate, even when it changes in the middle of a stage (the output does not jump). EOC pulse length and cycle measurement run in real time. Infinite hold and sustain hold are unaffected, gate and triggers react as before. Without a cable, and with a mono or four-channel cable, the factor stays 1, so old patches sound unchanged. The cycle rate is limited by the sample rate (at most one cycle per sample). The factor is recomputed per generator only when the voltage changes.

**Cache**<br>
The resolved time contains a `pow()` and is recomputed per stage only when knob, CV, range or mode changes.

<a id="sec3-3"></a>

### 3.3 Level Signal Path
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Levels**<br>
Each stage has a target level, unipolar 0–10 V: knob plus LEVEL CV, sum clamped to 0–10 V. Delay level = start level of the cycle (only as a jump target with RETRIG D, otherwise the target of the Delay stage), Release level = end level and rest level.

**Rest level**<br>
At standstill the output sits at the Release level, also right after power-up. The knob acts live without a cycle running. After Release the output stays there. With the internal loop, the next cycle starts from this level.

**Interpolation**<br>
Each stage interpolates from the level at stage start (`voltageFrom`) to the target level. If a level knob or the level CV changes during a stage, the stage keeps interpolating towards the new target from the next sample on. This explains the lively effects with LFO-modulated levels.

<a id="sec3-4"></a>

### 3.4 Curve Shape
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Bipolar mapping**<br>
CURVE −1..+1. The magnitude `a = |c|` gives the curvature exponent `power = 2.5^(a · (2 + a²))` (maximum 2.5³ ≈ 15.6, linear at `c = 0`).

**Relative to the direction of movement**<br>
For a rising stage: `c < 0` (round) `f = 1 − (1 − t)^power` (fast start, gentle arrival), `c > 0` (pointed) `f = t^power` (gentle start, fast arrival). If the stage runs downward (target below start level), the sign is mirrored. "Round" thus always means: **flat at the upper level end**. Attack shoots up and flattens at the top, Release starts flat at the top and falls faster towards the end. Both knobs left give a symmetric, bell-like envelope, both right a pointed one. A test with curve −0.5 gave Attack 5.54 V at 25 % and 8.57 V at 50 %, Release 9.79 V at 25 % and 8.57 V at 50 %.

**Cache**<br>
`power` depends only on the magnitude and is recomputed when the curve changes.

<a id="sec3-5"></a>

### 3.5 VCA
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

The gain is the envelope times 0.1 (0 at 0 V, 1 at 10 V) and acts equally on all channels. The signal may be bipolar, its sign is preserved (linear VCA, not a rectifier). The gain never goes negative because all levels are limited to 0–10 V. With no cable at VCA IN, 10 V is applied, so VCA OUT equals ENV as a separate signal. The channel count of VCA OUT follows VCA IN. The VCA is only computed when VCA OUT is patched. With antialiasing on, the gain uses the smoothed envelope ([3.9](#sec3-9)), which also stays limited to 0–10 V.

With a Release level above 0 V the VCA does not fully close at rest.

<a id="sec3-6"></a>

### 3.6 EOC Output
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

The EOC pulse (10 V) fires at the end of Release. The pulse length is adaptive: the minimum of 1 ms and half the length of the last cycle, at least 1 sample. The length depends on the length of the most recently completed cycle, not on the time since the last fired EOC, so a swallowed pulse does not double the period. There is at least 1 sample of gap between two pulses (rising edge guaranteed, minimum cycle 2 samples). An event during a running pulse merges with it. An abort in Delay (Sustain) produces no EOC. With a rate other than 1 the pulse length stays in real time, the measured real cycle length is what counts.

<a id="sec3-7"></a>

### 3.7 Status LEDs
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

One status LED per row with three independent color channels (only one lit at a time): **Red** = stopped, **Green** = running (same green as ACTIVATE), **Yellow** = holding (Sustain or infinite stage time). The LED updates only every 64 samples. "Running" applies to the whole window since the last update, red only on complete idle, so a patch loop (EOC → T1) does not flash red. The LED draws with normal blending and no halo, because Rack's default screen blend mode bleaches to pastel tones on the light panel.

<a id="sec3-8"></a>

### 3.8 Quadrature Coupling and OR Logic
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**OR**<br>
Purely output-side in `Europa::process()` after the generator loop: `max(gens[0].envOut, gens[1].envOut)` and `max(gens[2].envOut, gens[3].envOut)`. The engines are untouched, the level is taken before the VCA. With antialiasing on ([3.9](#sec3-9)) the maximum compares the smoothed levels (`envAA[]`), otherwise they are identical to `envOut`.

**Start event**<br>
`EnvGen4::attackEnded` is true for exactly one sample. It is set in `finishStage()` when Attack ends, also at attack time 0 and during sustain hold. A jump trigger straight into Hold does not cause an event.

**Follower (Follow mode)**<br>
With the button active (Follow position), `EnvParams::coupled` is set for generator 2 (or 4). Its gate edge then no longer starts a cycle; the start comes through the optional `startRequest` parameter of `process()`, which `Europa::process()` derives from the partner's `attackEnded`. Generator 1 runs before generator 2 in the loop, so the event is available in the same sample. The follower's own T1/GATE is ignored; the partner's gate state (`gateHigh`) applies instead. The TRIG triggers for stages 2–4 stay active. LOOP = FREE behaves like GATE while coupling is active (loop while the partner's gate is held), so the follower does not start itself.

**Sustain mode**<br>
In Sustain mode the follower takes over the partner's gate state: it starts at the attack end, holds at the end of Attack for as long as the partner's gate is high, and begins Release as soon as that gate falls.

**Delay of the follower**<br>
The follower's start begins with its Delay stage. A delay time on generator 2 (knob or delay CV on channel 1 of TIME CV) shifts the beginning of its envelope in addition to the partner's attack end, and so sets the offset between the two curves.

**Loop**<br>
For a single pass, the LOOP switch of **both** generators must be OFF. If generator 1 loops (GATE or FREE), the two-stage cycle repeats: each attack end of generator 1 restarts generator 2.

**Together with OR**<br>
Coupled and combined through the OR output, two generators give a two-part envelope whose second part begins at the peak of the first.

**Handshake (position 2, 2026-10-05)**<br>
The model is the quadrature mode of a classic quad function generator as described in a public source (a public description). The source's figures could not be evaluated; the implementation is an independent interpretation of the described sequence. The master is generator 1 (or 3), the follower generator 2 (or 4). The four-step cycle:

1. The master starts (gate at T1/GATE), runs Delay and Attack and **holds** at its attack level.
2. With the master's attack end the follower starts (with its Delay stage) and rises.
3. When the follower reaches its attack end, it **holds** at its attack level too, and the master begins Release.
4. When the master reaches its end (minimum), the follower begins Release.
5. The end of the follower's Release is the end of the cycle. With LOOP on the master, the master then restarts.

**Period and phase**<br>
The period is the sum of master Delay and Attack, follower Delay and Attack, and master Release and follower Release. With equal times the follower is shifted against the master by exactly a quarter period (in the standalone test the deviation is 0.002 V). The OR output then gives a trapezoid with a plateau twice as wide, because one of the two is always at the top. The time controls act coupled: a longer attack on the follower lengthens the master's plateau, a longer release on the master delays the follower's release.

**Hold stage is dropped**<br>
Waiting replaces Hold, as on the original. Hold time, hold level and HOLD CV have no effect in the handshake. The hold sits at the attack level.

**Loop**<br>
The master's LOOP switch repeats the four-step cycle, corresponding to the "Pulse B → Trigger A" cable on the original: the master restarts after the follower's cycle end. GATE: only while the gate is held. FREE: without a gate. In the handshake the master has no internal loop, it always waits until the follower is done. With LOOP = OFF exactly one cycle runs.

**Follower in cycle**<br>
If the follower's LOOP switch is on GATE (the master's gate) or FREE, the follower free-runs and is not restarted by the master's attack end. The master waits at its attack end for the follower's next attack end. The follower holds at its attack end only if the master is waiting at that moment, otherwise it continues through Release without holding. While the master releases, the follower stays at the top and cycles on afterwards, matching the description of the cycle case on the original. FREE on the follower does not behave like GATE here (unlike in Follow mode).

**Sustain**<br>
The handshake applies in One-Shot only. If either generator is in Sustain mode, the Follow behavior applies automatically. If the handshake is switched off or a generator is set to Sustain while a generator is waiting, the hold ends immediately. An orphaned partner therefore cannot keep a generator stuck.

**External retrigger**<br>
If the master is retriggered from outside while the follower is still holding, the follower restarts at the master's next attack end, even with LOOP on the follower. Without this rule the two would block each other.

**Events and implementation**<br>
Besides `attackEnded` the engine reports the new event `cycleEnded` (one sample, Release has ended). Waiting uses the same hold as Sustain mode internally (`sustainHeld`), additionally marked with `hsHeld`, so the status LED is yellow meanwhile. `envQuadSetup()` in `EnvGen4.hpp` derives the handshake fields in `EnvParams` from the partner's events and returns the follower's `startRequest`. In `Europa::process()` the master runs before the follower: it sees the follower's events one sample later (about 21 µs at 48 kHz), the follower sees the master's events in the same sample.

**Not adopted**<br>
We do not reproduce hardware quirks of the source: the pulse level that drops with long decay, the threshold that flips early because of CMOS loading, the pulse shape (15 V, about 150 µs) and the trigger thresholds of the transient circuit. The thresholds here are exact and events are internal flags. The source's description of the four-channel panning patch is contradictory in itself and was not implemented.

<a id="sec3-9"></a>

### 3.9 Antialiasing (minBLEP)
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Problem**<br>
Hard envelope edges (a stage with time 0, the retrig jump) contain harmonics far above the Nyquist frequency. If the envelope runs in the audio range (for example as a square wave in the free-running cycle) and modulates an audio signal through the VCA, these harmonics fold back into the audible range as aliasing. Antialiasing is optional for this ([2.7](#sec2-7)), default off.

**What the engine reports**<br>
`EnvGen4` reports the **real level jumps** per sample (`jumpCount`, `jumpPos[]`, `jumpDelta[]`): (a) a stage with time 0 (except Delay) reaches its target instantly, the jump being target level minus start level; (b) the retrig switch jumps to the Delay level at the start. Continuous movements, including very fast ramps, are not reported. A Delay stage with time 0 does not change the level and reports nothing. The engine stays Rack-free and its output is bit-identical to the version without the reporting (compared over 8 million samples with random parameters).

**Timing of the jumps**<br>
`jumpPos` lies in (−1, 0] relative to the current sample. For the end of a time-0 stage the position follows from the time remainder (sub-sample accurate, also at rate ≠ 1). For gate and trigger events the moment inside the sample is unknown, and −0.5 is used.

**Smoothing**<br>
One `dsp::MinBlepGenerator<16, 16>` per generator in `Europa`. `Europa::process()` inserts the reported jumps and adds the correction to the engine output (`envOut + correction`). The impulse response is about 32 samples long and causal, so there is no latency. Because minBLEP overshoots slightly, the result is limited to 0–10 V. The engine states (quadrature, gates, EOC) keep working with the unsmoothed value.

**Where it takes effect**<br>
ENV, VCA (gain) and the OR outputs. Not affected: EOC. With the switch off the output equals the previous behavior.

**Phasor mode**<br>
In phasor mode `PhasorCore` reports the envelope's jumps in the same way ([3.10](#sec3-10)): segments with weight 0, reset, offset jumps, direction changes of the input and phase jumps with loose ratio transfer. The ENV output is smoothed only when it outputs the envelope. A phasor on ENV stays unsmoothed.

**Limits**<br>
- Only **jumps** are smoothed, not the **kinks** at stage transitions (these fall off with 1/f² and are much quieter, polyBLAMP has been put off for now).
- Jumps caused by **modulation** (time or level CV, moving a knob in the middle of a stage) are not reported.
- At **very high cycle frequencies** the corrections of neighboring edges overlap and the effect diminishes.
- Aliasing that is already **in the VCA input signal** cannot be removed by Europa.
- Degenerate case, all times 0 plus loop: the reports are incomplete there, which is harmless because the jumps cancel at the same position.

**Measuring**<br>
A sine (not a square wave) at VCA IN, analyzer at VCA OUT, switch off versus on. The sidebands at f_sine ± n · f_envelope remain (amplitude modulation, intended), the folded-back lines drop noticeably. See also [Section 6](#sec6).

**Test**<br>
`tests/test_antialias_spruenge.cpp` (standalone, no Rack): no jumps for a continuous envelope, attack time 0, retrig jump, release time 0 in Sustain, and 300 random runs in which the size and sign of every reported jump match the level change. Functional test in Rack on 2026-10-02: works.

<a id="sec3-10"></a>

### 3.10 Phasor Mode (Signal Processing)
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

The engine of the phasor mode is the Rack-free class `PhasorCore` (`PhasorCore.hpp`, [Section 4](#sec4)). One instance runs per generator. It uses `EnvParams` and the helper functions from `EnvGen4.hpp` (times, curve shapes, levels), while the `EnvGen4` states stay idle in phasor mode.

**Idea: a mapping, no state of time**<br>
From the input phase `x` (0–10 V, divided by 10) a fixed chain produces the generator's phase `y`, and from `y` the envelope and the phasor outputs. There are no integrators and no time of its own. The output therefore follows the input exactly, also with tempo changes, standstill and backward motion. The only real state is a cycle counter that depends solely on the input wraps. A clock input with period measurement was deliberately rejected (Section 8).

**Input and direction**<br>
`x = U / 10`, brought to 0..1 (`x − floor(x)`, so slightly overshooting phasors work as well). The direction follows from the step `d = x − xPrev` with hysteresis: `d < −0.5` is a wrap of a rising ramp, `d > 0.5` a wrap of a falling ramp, `|d| > 10⁻⁵` sets the direction by sign, smaller steps (standstill, noise) leave it unchanged. Until the first step "rising" applies.

**Falling ramp: mirroring at the input**<br>
With a falling ramp the whole chain works with `xs = 1 − x`. For the generator this is a normal rising ramp: it always runs in the order Delay → Attack → Hold → Release (rise before fall), and the EOC sits at x = 0 on a falling ramp automatically. Offset and ratio apply from the generator's point of view in the direction of travel. The phasor outputs are mirrored back at the end (see "Phasor outputs" below) and so stay direction-true. A direction change makes `xs` jump (`x ↔ 1 − x`), the envelope jumps then (reported, see below), the phasor output does not.

**Unrolled phasor and ratio p/q**<br>
A cycle counter `cycle` (0..719719, i.e. modulo 720720 = lcm(1..16)) counts the input wraps, forward only on the mirrored signal. From it follows
`y = frac(((cycle mod q) + xs) · p / q + offset)`.
p/q is reduced beforehand. The calculation runs in `double` so `y` stays exact at the cycle boundaries. Examples: 1/2 is one cycle of y over two input cycles, 3/2 is three y cycles over two input cycles, 2/1 is two y cycles per input cycle. Because `cycle` depends only on the input wraps and is defined by reset and ratio transfer, all generators stay phase-locked to each other.

**Ratio transfer (strict and loose)**<br>
The knob plus CV supplies the target (p, q), which is taken over only at an input wrap.
- **Strict (default):** only if `cycle mod q_old == 0` **and** `cycle mod q_new == 0` hold. Then y lies exactly on the start of the cycle (plus offset) before and after the change, so there is no jump. The price is the wait: with 4 → 6 every 12 input cycles, with 7 → 11 every 77. If the CV jumps back before the matching wrap, the change is lost.
- **Loose (context menu, [2.7](#sec2-7)):** immediately at every input wrap. y jumps to the value that the global counter dictates for the new ratio. The jump is reported as an envelope jump, there is no EOC.
- **Reset:** a rising edge at TRIG channel 1 sets the counter to 0 and takes over a pending ratio immediately (y jumps, reported). Without a reset the phase relationship for division is determined by the moment of power-up.
- The yellow pending LED lights while the target differs from the current ratio.

**Sampling of the ratio CV (rule of thumb)**<br>
The ratio is read only at input wraps (in strict mode only at valid ones), so the CV is sampled at the frequency of the input phasor. For a recognizable shape an LFO needs at least about four measuring points per period, i.e. at most a quarter of the input frequency. With a 1 Hz phasor, LFOs above about 0.25 Hz are too fast, and it looks as if they work only sometimes. With an 8 Hz input, 0.5–2 Hz ran cleanly (Kinovice, test in Rack). Remedy: use a faster input and divide down via q, or use the phasor output of another generator or a sequencer on the beat as the modulation source.

**CV mapping (all added to the knob)**<br>

| Target | Channel | Mapping |
|---|---|---|
| Weights (time) | TIME CV 1–4 | like time CV ([3.2](#sec3-2)) |
| Offset | TIME CV 5 | 10 V = one full cycle, wraps around as a phase |
| Numerator p | TIME CV 6 | 10 V = full span 1…16, rounded |
| Denominator q | TIME CV 7 | 10 V = full span 1…16, rounded |
| Curve | LEVEL CV 5–8 | ±5 V = −1…+1, limited |

p and q are rounded with hysteresis: the value changes only when it deviates by more than 0.6 steps from the last accepted value (0.1 steps of leeway at each rounding boundary), so that a CV sitting on the boundary does not make the pending LED flicker. Small amplitudes below about ±0.4 steps therefore do not move the ratio.

**Segments from the weights**<br>
The resolved stage times (knob, TIME CV and RANGE, as in [3.2](#sec3-2); HOLD CV replaces the Hold weight) are weights `w₁…w₄`. Normalized (`pᵢ = wᵢ / Σw`) they divide y into four segments. There is no infinite hold. A weight of 0 drops the segment (the envelope jumps there, reported), if all weights are 0 an equal distribution applies (one quarter each). Important: **the RANGE switch scales the weights** (TR1 to TR4 have different upper limits). For equal fractions the stages must share the same range, otherwise a stage in TR4 weighs a thousand times more than one in TR1.

**Envelope within a segment**<br>
Stage `k` runs from level(k−1) to level(k), the Delay segment starts from the Release level. The phase in the segment `u = (y − start) / length` is shaped with the stage's curve (as in [3.4](#sec3-4), relative to the direction of travel, i.e. stages running downward with the sign mirrored), including the curve CV. The output is `start + (target − start) · f(u)`.

**Phasor outputs**<br>
`phasorLinear = y`. `phasorWarped = (k + f(u)) / 4` with the same curve `f` (always rising, no mirroring). It is continuous and monotonic, with unequal weights it amounts to a warp of time in which each segment takes one quarter. Both are mirrored back on a falling input (`1 − value`) so that a falling input ramp gives a falling output ramp. The mirroring back exactly cancels the input mirroring, so a direction change does not make the phasor output jump.

**EOC**<br>
The EOC pulse fires when `y` crosses the cycle boundary (`y` drops by more than 0.5 against the previous sample). Pulse length as in [3.6](#sec3-6): the minimum of 1 ms and half the length of the last cycle of y, at least 1 sample. With p > q there are several EOCs per input cycle. No EOC is produced for unexplained jumps of y (reset, offset jump, direction change, phase jump with loose transfer).

**Unexplained jumps and minBLEP**<br>
Each sample the engine compares `y` with the expected value (`y` of the previous sample plus input motion times the previous ratio). If `y` deviates by more than 2 % of a cycle, the envelope has moved unexpectedly. If the difference between the actual and the expected envelope value exceeds 0.05 V, it is reported as a jump (`jumpCount`/`jumpPos`/`jumpDelta`, position within the sample unknown, hence −0.5), and `Europa::process()` smooths it with antialiasing on, like the jumps of the classic engine ([3.9](#sec3-9)). Continuous changes (knobs, slow CV) are not reported, they stay below the threshold. Segment jumps caused by weight 0 are reported separately by the engine (level difference, position sub-sample accurate from the course of y). With the strict ratio change `y` stays continuous, so there is nothing to report.

**Mode change**<br>
When the PHASOR switch is toggled, the generator's `EnvGen4` instance, its `PhasorCore` instance and the time caches are reset. In phasor mode `EnvGen4` does not compute, the quadrature start is not called.

**Limits**<br>
- `y` may move by at most 0.5 per sample: with a very high ratio `p/q` and a fast input, false wraps occur otherwise.
- An offset that changes at audio rate with large amplitude is treated as a sequence of jumps and is not smoothed as modulation.
- A ratio CV between the wraps does not act continuously: a continuous ratio would give up the fixed phase relationship to the input.
- A phasor on ENV (OUT on linear or warped) is not minBLEP-smoothed.

**Test**<br>
`tests/phasor_core_test.cpp` (standalone, no Rack, built with `-std=c++11`): ratio 1/1, 2/1 and 1/2 (envelope and EOC count), falling ramp (order and direction-true phasor output), strict ratio change, weight 0 with jump report, reset, direction change without a false EOC, all weights 0, curve on the warped phasor (pointed and round, continuous at the segment boundaries), unexplained jumps (reset, offset jump, slow offset sweep without a report, direction change) and loose against strict ratio transfer.

---

<a id="sec4"></a>

## 4. Function Reference (C++)
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

### `EnvGen4.hpp` (Rack-free)

**Constants**<br>
`ENV_NUM_STAGES = 4`, `ENV_MAX_TRANSITIONS_PER_SAMPLE = 4`, `ENV_TIME_INFINITE = 1e30f` (finite sentinel instead of INFINITY because Rack is built with `-funsafe-math-optimizations`), `ENV_TIME_INFINITE_VOLTS = 9.5`, `ENV_TIME_RANGE_MAX = {0.03, 0.3, 3, 30}`, `ENV_TIME_EXP_BASE = 1000`, `ENV_TIME_CV_SPAN_VOLTS = 10`, gate 0.1/1 V, trigger 2 V, `ENV_EOC_MAX_SECONDS = 1e-3`, `ENV_RATE_OCTAVES_PER_VOLT = 1`, `ENV_RATE_CV_LIMIT_V = 5`, `ENV_MAX_JUMPS_PER_SAMPLE = 8`.

**`envTimeFromPosition()` / `envPositionFromTime()`**<br>
Knob ↔ seconds per range.

**`envResolveTime()`**<br>
Knob + time CV → seconds or `ENV_TIME_INFINITE`.

**`envCurvePower()` / `envShapePhase()`**<br>
Curvature exponent and curve application, see [3.4](#sec3-4).

**`envRateFromVolts()`**<br>
Rate CV in volts → speed factor (> 0), see [3.2](#sec3-2).

**`struct EnvParams`**<br>
Already resolved values passed per sample: `time[4]`, `curve[4]`, `levelDelay`, `levelAttack`, `levelHold`, `levelRelease`, `sustainMode`, `loopEnabled`, `freeCycle`, `retrigZero`, `rate`, `coupled`, plus the handshake fields `handshake`, `hsMaster`, `hsPartnerWaiting`, `hsPartnerRunning`, `hsReleaseEvent`, `hsPartnerCycleEnded` (set by `envQuadSetup()`).

**`struct EnvGen4`**<br>
Outputs `envOut`, `eocOut`, `running`, `held`, `stage`, `attackEnded`, `cycleEnded`, `hsHeld`. Jump reporting for the antialiasing: `jumpCount`, `jumpPos[]`, `jumpDelta[]` (see [3.9](#sec3-9)). Methods `process(dt, params, gateVolts, trigVolts[4], startRequest = false)`, `startCycle()`, `jumpTo()`, `startRelease()`, `finishStage()`, `addJump()`, `reset()`.

**`enum EnvQuadMode` / `envQuadSetup()`**<br>
`ENV_QUAD_OFF = 0`, `ENV_QUAD_FOLLOW = 1`, `ENV_QUAD_HANDSHAKE = 2`. `envQuadSetup(p, quadMode, isFollower, pairSustain, self, partner)` sets the coupling fields of `EnvParams` for one generator of the pair and returns the `startRequest` for `process()` (follower only). It must be called after `loopEnabled`/`freeCycle` are set. With `pairSustain` the Follow behavior also applies in position 2. Testable without Rack; `tests/test_handshake.cpp` uses the same function as `Europa::process()`.

### `Europa.cpp`

**`struct Europa : Module`**<br>
Enums `ParamId`, `InputId`, `OutputId`, `LightId`. `EnvGen4 gens[4]`, `TimeCache`, rate cache (`rateCacheVolts`/`rateCacheValue`), `pendingGen`, `displayedGen`, `antialias`, `minBlep[4]` (`dsp::MinBlepGenerator<16, 16>`), `envAA[4]` (smoothed envelope), `process()`, `dataToJson()`/`dataFromJson()`, `onReset()`.

**`EnvTimeQuantity`**<br>
Display/input of the time in seconds (`getDisplayValue`, `setDisplayValue`, `getDisplayValueString`, `setDisplayValueString`), depending on the range of the same stage.

**Widgets**<br>
`EuropaWidget`, `EuropaRangeSwitch`, `EuropaQuadButton`, `GenSelectKnob`, `GenActivateButton`, `EuropaStatusLight`, `EuropaSelectLight`, `EuropaFrame`, `EuropaRowHighlight`, `EuropaOnIndicator`, `EuropaPanelBackground`. Labels via the shared `PanelLabel.hpp`. `EuropaWidget::appendContextMenu()` holds the antialiasing switch.

### `PhasorCore.hpp` (Rack-free)

**Constants and helpers**<br>
`PHASOR_CYCLE_MOD = 720720` (lcm(1..16)), `PHASOR_RATIO_MAX = 16`, `PHASOR_DIR_EPS = 1e-5` (hysteresis of the direction detection), `PHASOR_MAX_JUMPS = 2`. `enum PhasorOutMode` (`PHASOR_OUT_ENV = 0`, `PHASOR_OUT_LINEAR = 1`, `PHASOR_OUT_WARPED = 2`). `phasorGcd()` is a gcd of its own because Rack is built with C++11 and `std::gcd` only exists from C++17.

**`struct PhasorCore`**<br>
Outputs `envOut`, `phasorLinear`, `phasorWarped` (0..1, direction-true), `eocOut`, `running`, `pending`, plus the jump report `jumpCount`, `jumpPos[]`, `jumpDelta[]` (as in `EnvGen4`, [3.9](#sec3-9)). Methods `process(dt, EnvParams, ratioP, ratioQ, offset, phasorVolts, resetVolts, looseRatio = false)`, `envAt()` (envelope at a cycle phase, also used for the jump check), `addJump()`, `reset()`. State: cycle counter, current ratio, direction, previous values, EOC pulse.

### `Europa.cpp`, additions for the phasor mode

`PHASOR_PARAM`, `PHASOR_OUT_PARAM`, `RATIO_P_PARAM`, `RATIO_Q_PARAM`, `PHASE_OFFSET_PARAM` (four each, at the end of the enum), `PENDING_LIGHT` (four), members `phasor[4]`, `prevPhasorMode`, `phasorInVolts` (for the pass-through), `ratioPending`, `ratioPLast`/`ratioQLast` (hysteresis of the p/q rounding) and `ratioLoose`. `EuropaQuadButton` has an adjustable `historyName` (undo name), which the phasor OUT button uses.

---

<a id="sec5"></a>

## 5. Persistence
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Params**<br>
All settings of all four generators are real Rack params: 98 in total (4 × 24: 4 levels, 4 × time, 4 × range, 4 × curve, mode, retrig, loop each and the five phasor params phasor, phasor out, p, q, offset; plus two three-position quadrature buttons (0..2), one per pair). `LOOP_PARAM` has three positions (0..2). Rack stores them in the patch.

**JSON**<br>
`pendingGen` and `displayedGen` (clamped to 0–3), plus `antialias` and `ratioLoose` (both bool). If an entry is missing in an old patch, antialiasing stays off and the ratio transfer stays strict.

**Enum rule**<br>
New entries are only appended at the end so cable indices of existing patches stay stable. The Delay/Release level params and `LEVELCV_INPUT` were added this way. Old patches load them with a default of 0 V and sound unchanged. `QUAD_PARAM` and `OR_OUTPUT` were appended the same way. Old patches load the coupling switched off. On 2026-10-05 `QUAD_PARAM` gained its third position (range 0..2 instead of 0..1), 0 (Off) and 1 (Follow) keep their meaning. The former light group `QUAD_LIGHT` was removed: the button draws its LED itself from the param (lights are not stored in the patch, so there are no compatibility consequences). The values 0 (Off) and 1 (Gate) of `LOOP_PARAM` keep their meaning, the new value 2 stands for the free-running cycle.

The phasor mode (2026-10-06) appends `PHASOR_PARAM`, `PHASOR_OUT_PARAM`, `RATIO_P_PARAM`, `RATIO_Q_PARAM`, `PHASE_OFFSET_PARAM` (four each) and `PENDING_LIGHT` at the end. Old patches load all generators in classic mode and sound unchanged. The only exception: a poly cable with more than four channels at LEVEL CV, whose channels 5–8 now act as curve CV.

**Reset**<br>
`onReset()` resets all engines (including the `PhasorCore` instances) and the generator display.

---

<a id="sec6"></a>

## 6. Known Pitfalls and Open Items
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Delay level only acts at Delay time 0 with RETRIG D**<br>
A Delay stage with time 0 does not change the level (click-free retrigger). The Delay level is then ineffective unless the Retrig switch is on, which jumps to the Delay level at start. Otherwise Attack glides from the current level.

**Rest level above 0 V opens the VCA**<br>
A Release level above 0 V lets the VCA signal shine through at rest. For pure VCA use leave the Release level at 0 V (default).

**Mono cable at TRIG, TIME CV and LEVEL CV**<br>
TRIG: channel 1 is unused, a mono cable does nothing. TIME CV and LEVEL CV: a mono cable affects Delay only. Use a poly cable for all other stages.

**HOLD CV only works in One-Shot/Loop**<br>
In Sustain mode HOLD CV has no effect, the gate determines the hold duration as before. A patched cable makes the Hold stage's TIME knob fully ineffective in One-Shot, which is intentional, not a malfunction.

**Triggers in Sustain mode**<br>
Jump triggers only act while the gate is high, otherwise they are ignored.

**Loop in Sustain mode**<br>
The LOOP switch (including the FREE position) has no effect in Sustain, nor does EOC → T1/GATE.

**Rate CV needs a cable with at least five channels**<br>
A mono cable lands on channel 1 and acts there as the delay-time CV, not as rate. Reach channel 5 through a merge module or a poly splitter, channels 1–4 may stay empty. The range is generous at ±5 V and one octave per volt: a sine of ±5 V swings between 1/32 and ×32, a unipolar LFO (0–10 V) sits permanently at the limit (×32) from 5 V upward.

**Quadrature (Follow): loop of both generators OFF for single passes**<br>
If generator 1's LOOP switch is on GATE or FREE, it restarts by itself after Release and every attack end restarts generator 2. On a scope this looks as if generator 1 were waiting for generator 2. For a single pass set the loop of both generators to OFF. A delay time on generator 2 shifts its start in addition to generator 1's attack end.

**Handshake: Hold controls have no effect**<br>
In the handshake, waiting for the partner replaces the Hold stage. Hold time, hold level and HOLD CV do nothing there. This is intentional and not a malfunction.

**Handshake: waiting can take long**<br>
Each generator waits for its partner at the attack end. If the follower's attack is infinite (time CV ≥ 9.5 V), the master waits just as long. This matches the original. Switching the handshake off ends the hold immediately.

**Handshake: loop on the master for the four-step cycle**<br>
In the handshake the master has no internal loop. It restarts only after the follower's cycle end, and only with LOOP = GATE and the gate held, or with LOOP = FREE. LOOP of the follower on GATE or FREE, on the other hand, turns it into a free-running oscillator (see [3.8](#sec3-8)).

**Handshake in One-Shot only, one sample of latency**<br>
If either generator is in Sustain, Follow applies. The master sees the follower's events one sample later (about 21 µs at 48 kHz). That is inaudible for envelopes but not exactly zero.

**Saved patches and the panel width**<br>
The panel has grown from 20 to 23 HP. Modules sitting directly to the right of Europa in a saved patch may overlap after loading and need to be moved.

**Antialiasing only smooths jumps**<br>
The switch ([2.7](#sec2-7)) acts only on the reported level jumps of the envelope, not on the kinks at stage transitions, not on modulation jumps and not on aliasing in the VCA input signal. At very high cycle frequencies (the impulse response is about 32 samples long) the effect diminishes. To judge it, feed a sine into the VCA: a square wave at VCA IN together with the envelope's square wave gives a product of two broadband signals in which the envelope's share is hard to see.

**Attack, Release and Delay all at 0 do not give a square wave**<br>
With time 0 in Delay, Attack and Release the envelope in the free-running cycle sits permanently at the Hold level, and the valleys have zero width. The VCA then has a constant gain of 1 and passes the signal unchanged, so there is nothing to smooth. A square wave only appears with a **Delay time greater than 0** (at a Delay level of 0 V), the period then being Delay plus Hold.

**Audio rate**<br>
A patch loop has 1 sample of latency through the cable. At 48 kHz recognizable trapezoid shapes are possible up to about 5–6 kHz (about 8 samples per cycle), theoretically 24 kHz (pulse train only). The latency detunes audibly at high frequencies (about 2 % at 1 kHz, about 10 % at 5 kHz). There is no oversampling. Hard envelope jumps (time-0 stages, retrig jump) can be smoothed with minBLEP in the context menu ([2.7](#sec2-7), [3.9](#sec3-9)), without the switch they alias. The internal loop has no latency.

**Automation and MIDI map**<br>
Since all 98 params are real, MIDI map and automation work on every generator. Only the displayed generator is visible, though.

**Phasor mode: the ratio CV is read only once per input cycle**<br>
The CV at TIME CV channels 6 and 7 is sampled at the input wraps (see the rule of thumb in [3.10](#sec3-10)). An LFO above about a quarter of the input frequency acts irregularly or stays put. With a 1 Hz phasor, LFOs from 0.25 Hz upward are already too fast.

**Phasor mode: strict transfer can lose CV changes**<br>
Strict transfer applies only at wraps where the counter matches both old and new q. If the CV jumps back before a matching wrap, the change is discarded. For CV-controlled ratios (sequencer, LFO) choose the loose mode in the context menu ([2.7](#sec2-7)).

**Phasor mode: the same CV on numerator and denominator cancels out**<br>
If the same square wave is on TIME CV channels 6 and 7, p and q change together (16/16 and 1/1), the ratio stays 1/1 and nothing happens. Negative CV makes both numbers smaller: on the numerator this slows the generator down (8 → 4: 1/2), on the denominator it speeds it up (8 → 4: 2/1). This is the maths of the ratio, not a fault.

**Phasor mode: channels 5–7 of the TIME CV input need that many channels**<br>
Offset, p and q are reachable only if the cable has at least 5, 6 or 7 channels. A mono cable lands on channel 1 and acts there as the weight of the Delay stage.

**Phasor mode: RANGE scales the weights**<br>
In phasor mode the stage times are weights, and TR1…TR4 have different upper limits. For equally long segments set all four stages to the same RANGE.

**Phasor mode: reset, offset jump and direction change make the envelope jump**<br>
These events move `y` unexpectedly. The envelope jumps (audible, smoothed when antialiasing is on), and there is no EOC. The phasor output is not smoothed. With loose ratio transfer `y` jumps at every change.

**Phasor mode: quadrature not intended**<br>
A generator in phasor mode ignores the quadrature coupling. Do not run the partner of a phasor generator in follow or handshake mode (not intended and untested).

**LEVEL CV channels 5–8 act as curve CV, in both modes**<br>
A poly cable with more than four channels at LEVEL CV now changes the stages' curve. Old patches with such a cable may sound different.

**Open items**<br>
A comprehensive DSP test file for the basic functions of `EnvGen4` (standalone, see [Section 8](#sec8)). For the free-running cycle, the rate CV, the quadrature coupling and the jump reporting of the antialiasing there are standalone tests of their own in `tests/` (`test_autocycle.cpp`, `test_ratecv.cpp`, `test_quadratur.cpp`, `test_handshake.cpp`, `test_antialias_spruenge.cpp`). **The handshake is covered by the standalone test (`tests/test_handshake.cpp`, eleven cases); the functional test in Rack is still pending.** Also untested in Rack: the new button widget `EuropaQuadButton` (click, Shift+click, undo, LED). polyBLAMP for the kinks at stage transitions is put off for now. Panel figure as SVG. **Phasor mode:** `tests/phasor_core_test.cpp` (standalone) covers the engine. Confirmed in Rack (Kinovice, 2026-10-06): classic mode unchanged, the phasor modes and outputs, panel layout, the CV on p and q with constant voltage, strict and loose ratio transfer (LFO test see [3.10](#sec3-10)). **Still open in Rack:** the curve CV (LEVEL CV channels 5–8), the offset CV (TIME CV channel 5), the pass-through of the phasor between generators, the reset at TRIG channel 1 and the minBLEP report of the phasor jumps.

---

<a id="sec7"></a>

## 7. Patch Ideas
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**LFO with offset**<br>
Loop on, Delay level and Release level at the same value (e.g. 3 V), Attack level e.g. 7 V: the loop runs between two freely chosen values. Amplitude and offset are adjustable separately.

**Ducking**<br>
Delay level 10 V, Attack level 0 V: the envelope lowers instead of raising.

**Cascade**<br>
EOC of one generator into TRIG channel 2 of another: the second starts directly in Attack as soon as the first is done. EOC → T1/GATE of the same generator gives the patch loop.

**Skip a stage**<br>
A trigger at TRIG channel 3 jumps straight to the Hold level without the rise.

**External release**<br>
A trigger at TRIG channel 4 ends the envelope at any moment, e.g. by a clock pulse.

**LFO with exponential frequency control**<br>
LOOP on FREE, a poly cable with five channels at the TIME CV input, the rate CV on channel 5: the generator runs as an LFO without a gate whose frequency follows at one octave per volt. An LFO or sequencer on channel 5 modulates the speed, the curve shape stays unchanged.

**Two-stage envelope (quadrature + OR)**<br>
Button of pair 1+2 on, gate at T1/GATE of generator 1, loop of both generators OFF. Generator 2 starts as soon as generator 1 has reached its attack level. The OR output 1+2 delivers the combined curve. A delay time on generator 2 sets the offset.

**Quarter-period LFO (Handshake)**<br>
Button of pair 1+2 on Handshake (LED amber), LOOP of generator 1 on FREE, LOOP of generator 2 OFF, attack and release of both generators equally long. ENV 1 and ENV 2 deliver two trapezoidal control voltages with a 90° phase offset, e.g. for panning or counter-moving modulation. The period is the sum of all four times, unequal times distort the trapezoids. With equal times the OR output 1+2 gives a trapezoid with a plateau twice as wide.

**ADSR shape from two generators (OR)**<br>
Generator 1 in One-Shot with an Attack level of 10 V and a short Release, generator 2 in Sustain with an Attack level of 5 V, both with the same gate. The OR rises to 10 V, falls back to 5 V, holds 5 V and fades out at the end of the gate.

**Wavefolder-like effects**<br>
An LFO into LEVEL CV (channels 2–3) that swings beyond the 0–10 V limits gets clamped and folds back at the edge. With LFO frequencies and time ranges whose stage times lie in the range of the LFO period, and round or pointed curves, wavefolder-like shapes result. Add concrete settings (LFO frequency, time range, curve position) here.

**Phasor sequencer**<br>
Phasor source into T1/GATE of generator 1, PHASOR on, ratio 1/4: one envelope cycle over four input cycles. The EOC provides a clock for the downstream sequencer. With the weights you set how long Delay, Attack, Hold and Release are within the cycle, without anything drifting.

**Polyrhythmic envelopes**<br>
One phasor cable into generator 1, all four generators in phasor mode (the pass-through feeds 2–4), ratios 1/1, 3/2, 4/3, 5/4: four synchronous envelopes with different cycle lengths that coincide again at the reset.

**Warped phasor as a clock source**<br>
OUT of generator 1 on "phasor warped", ENV of generator 1 into T1/GATE of generator 2: the second generator receives a phasor with unevenly running time (swing, ritardando, accelerando), set via the weights and curves of generator 1.

**Ratio from a sequencer**<br>
Stepped CV into TIME CV channels 6 and 7, loose ratio transfer, input phasor fast enough (for example 8 Hz, divided down with q): the ratio follows the sequencer on every input cycle. An LFO should be at most a quarter of the input frequency.

**Falling ramp as backward motion**<br>
A falling phasor into T1/GATE: the envelope still starts with Attack, the phasor output falls along. This translates a backward-running beat into a normal envelope course.

**Curve by CV**<br>
An LFO or sequencer at LEVEL CV channels 5–8 reshapes the envelope or the warped phasor while it runs: round and pointed alternate on the beat, in both modes.

---

<a id="sec8"></a>

## 8. Design Decisions & Boundaries
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

All decisions agreed with Kinovice and tested on 2026-09-28 unless noted otherwise. The decisions of 2026-10-01, 2026-10-02, 2026-10-05 and 2026-10-06 are dated individually.

- **Engine path B (Kinovice):** own slim class `EnvGen4`, `Afg`/Sofonisba stays untouched. `Afg` is built around 16 stages and the shared `stageModifier[]` array, generalizing it would have risked regressions in the feature-complete Sofonisba.
- **Rack-free engine:** `EnvGen4.hpp` receives already resolved values (seconds, volts) and returns volts. This makes it testable standalone (DSP test file planned).
- **Time ranges (Kinovice):** all four ranges start at 0 (exactly 0 at the left stop), upper limits as in Sofonisba (30 ms / 300 ms / 3 s / 30 s), exponential knob mapping. The lower limits from Sofonisba are dropped.
- **Generator switching after the Lucia pattern (Kinovice):** all params (now 98) are real Rack params, the GUI only shows the selected generator. Rejected: the edit-param model with pickup/catch foreseen in the concept. Consequence: no copying and no pickup/catch needed, MIDI map and automation work on every generator.
- **`pendingGen`/`displayedGen` not a param (Kinovice):** undo, automation and preset recall should not affect pure GUI navigation (same decision as for Lucia).
- **Range as own click switch (Kinovice test):** snapped standard knobs with only 4 positions could not be moved in Rack (Rack's knob scales the drag distance to the value range). Rule of thumb from Sofonisba: small discrete state count → own widget. No custom context-menu checklist is needed here, Rack's standard menu already lists the ranges (an extra list appeared twice, Kinovice test).
- **Time value entry in seconds (Kinovice test):** without overrides of `getDisplayValue()`/`setDisplayValue()` a typed number landed as knob position and was clamped to 0/stop (same class of bug as in Sofonisba/tempo).
- **Own bipolar curve mapping, relative to the direction of movement (Kinovice):** instead of Sofonisba's Exp/Log bits. Reason: for falling stages the direction-independent curve gave a pointed exponential decay instead of the expected bell shape. Left = round (flat at top), right = pointed. Tooltip "curve (- round / + pointed)".
- **Delay and Release levels adjustable, unipolar 0–10 V (Kinovice):** previously fixed at 0 V. Enables offset LFOs, inverted envelopes and a rest level. Rejected: bipolar, because the VCA function would get negative gains.
- **Retrig switch jumps to the Delay level, renamed "RETRIG D" (Kinovice):** previously a jump to 0 V ("Retrig 0"). The internal enum name stays.
- **LEVEL CV as its own jack, path A (Kinovice):** poly, channel 1–4 = Delay/Attack/Hold/Release, additive, sum clamped to 0–10 V. Rejected: channels 5–8 on the TIME CV input (no space needed but less clear). Consequence: jack block initially with 8 columns instead of 7.
- **HOLD CV as its own, replacing jack instead of TIME CV channel 3 (Kinovice, 2026-09-28):** discovered while comparing against older patches, on the original controlled by 0–10 V CV with no relation to the gate. A dedicated jack instead of extending the existing channel, so the knob deliberately becomes ineffective once patched (a clear "knob- vs. CV-driven" split) rather than an additive mix. Mapping still goes through TR1–TR4 (exponential), not linear, for consistency with every other time input in the module. Scope limited to One-Shot/Loop, because in Sustain the hold duration is already unambiguously set by the gate, and a CV override there would have diluted the concept of Sustain. Consequence: jack block with 9 columns.
- **TRIG as a strobe with fixed targets:** comparable to the Sofonisba strobe (immediate jump), but without an address: each poly channel has its fixed stage (2–4).
- **Status LED with independent color channels and normal blending (Kinovice test):** Rack's RGB mixing did not allow clean colors, and the default blend mode bleaches on the light panel.
- **No oversampling:** main use is LFO operation, the audio range is a bonus. Oversampling would multiply the CPU load by its factor (four generators, polyphonic VCA) and was rejected.
- **Optional antialiasing for Europa only, minBLEP for jumps only (Kinovice, 2026-10-02):** After a survey of all modules in the plugin, Europa is the only one with a meaningful audio-rate case: the envelope's jumps (time-0 stages, retrig) act directly on audio through the VCA. The other modules are CV and logic tools (gates and triggers must not be filtered) or produce at most mild kinks. Implementation: the engine reports real jumps and stays Rack-free with a bit-identical output, while minBLEP sits in the module and corrects only the output, never the engine states (that would falsify the envelope). The result is limited to 0–10 V (overshoot). Default **off**, because minBLEP turns a hard jump into a short, slightly overshooting edge and that changes the character of sharp envelopes. Old patches sound unchanged. Rejected: polyBLAMP for the kinks (small gain, 1/f² roll-off, put off for now) and a Rack param instead of the context menu. Standalone test `tests/test_antialias_spruenge.cpp`, functional test in Rack on 2026-10-02: works (clean at Delay 0, at Delay 1 ms noticeably fewer harmonics in the spectrum with the switch on).
- **Free-running cycle as the third position of the LOOP switch (Kinovice, 2026-10-01):** instead of a separate switch or an additional param. Reasons: no room for another switch in the header, no new enum entries, and the values 0 and 1 keep their meaning. Works in One-Shot only, no effect in Sustain. The retrigger behavior is deliberately left unchanged, a gate during the free-running cycle triggers a restart as usual. Prompted by the analysis of a public community thread on emulating envelope generators from the Buchla world, which describes a cycle mode without a gate. The reference documentation is only the source of the idea, the implementation is independent. Standalone test `tests/test_autocycle.cpp`, functional test in Rack by Kinovice on 2026-10-01: works.
- **Rate CV on channel 5 of the TIME CV input (Kinovice, 2026-10-01):** bipolar, exponential and shape-preserving. Instead of a separate jack it sits on channel 5 of the existing polyphonic input (proposal adopted): the panel already had nine jack columns, channel patterns already exist for LEVEL CV and TRIG, and saved patches stay valid. Range one octave per volt, limited to ±5 V (proposal adopted, both values adjustable as constants). A global switch for unipolar CVs was not implemented. Standalone test `tests/test_ratecv.cpp` (factor mapping and limiting, duration ∝ 1/rate, identical curve shape, continuous output on a rate change, EOC count in free cycle, infinite hold, all times 0 at ×32), functional test in Rack on 2026-10-01: works ("most interesting modulation").
- **OR outputs (Kinovice, 2026-10-01):** two outputs, each the maximum of the ENV levels of a pair (1+2, 3+4) before the VCA. A level control is omitted because every stage already has its own level. A column of its own on the right, so the panel grows from 20 to 23 HP (22 HP was too narrow, 24 HP too wide), designed like the other jack columns (dark plate 10 mm wide, narrow light frame; `orPlateW`/`orFramePad` in `Europa.cpp`). Rejected: a tenth column in the existing grid (third tightening, labels too cramped) and a context menu instead of jacks (it would cost the individual envelope outputs of generators 2 and 4). Functional test in Rack on 2026-10-01: works, panel layout approved by Kinovice.
- **Quadrature coupling (Kinovice, 2026-10-01):** switchable per pair, generator 2 (or 4) starts at the **end of the attack** of generator 1 (or 3). Rejected as the start event: beginning of Release and end of cycle (EOC). The exact hardware behavior cannot be determined unambiguously from the reference documentation, so the choice is our own definition. The switch is a latching LED button in the OR column. The follower's own T1/GATE is ignored, the triggers for stages 2–4 stay active; in Sustain mode the follower follows the partner's gate; with coupling active FREE behaves like GATE (proposal adopted). Standalone test `tests/test_quadratur.cpp`, functional test in Rack on 2026-10-01: works as built. An apparently swapped start at first turned out to be a loop switch left on, hence the note in Section 6. Since 2026-10-05 the switch has three positions (next entry); the definition described here still applies unchanged as the "Follow" position.
- **Handshake as the third position of the quadrature button (Kinovice, 2026-10-05):** The trigger was a public description of the quadrature mode of a classic quad function generator (a public description). It showed that quadrature is a handshake of four events (attack end A → B starts, B maximum → A falls, A minimum → B falls, B end → A restarts), whereas Follow reproduced only the first step. Kinovice chose a **third position** instead of replacing Follow, so that Follow and saved patches (0/1) stay intact. Further definitions: the Hold stage is dropped in the handshake (waiting replaces it), the master has no internal loop and restarts through the follower's cycle end (corresponds to the "Pulse B → Trigger A" cable), a looping follower free-runs (the source's cycle case) and holds only if the master is waiting, in Sustain Follow applies automatically, an external retrigger of the master restarts a holding follower (otherwise mutual blocking, found in testing), the hold ends when the handshake is switched off. The button is a widget of its own (rule of thumb: small discrete state count), its LED is drawn from the param, `QUAD_LIGHT` is removed. The hardware's pulse and threshold quirks are not reproduced, the contradictory four-channel panning description of the source was not implemented. Standalone test `tests/test_handshake.cpp` (eleven cases: four-step cycle, period and quarter phase, unequal times with delay, hold level, free-running follower with and without master loop, Sustain fallback, switching off while holding, retrigger, all times 0, Follow unchanged) passes; `tests/test_quadratur.cpp` still runs unchanged. Functional test in Rack is still pending.
- **Phasor mode (Kinovice, 2026-10-06):** The occasion was the idea of producing at the ENV output a phasor that is always synchronous to an external clock or phasor (beat-accurate modulation, phasor-based sequencer). Decisions:
  - **Phasor input instead of clock-to-phasor:** measuring the period between clock edges is error-prone because of the measuring time (Kinovice's experience), a phasor input makes the module stateless with respect to time. Rejected: clock → phasor with a PLL.
  - **Four independent phasors synchronous to the input (variant B):** each generator runs over the whole input cycle with its own ratio, instead of chaining the four generators as segments of one cycle (variant A, sequencer).
  - **Mode choice per generator via a button with a param:** T1/GATE normally stays the trigger, the phasor input uses the same jack. Automatic detection of trigger, gate and phasor was rejected because the signals cannot be told apart reliably (a standing phasor looks like a gate, a slow ramp like a gate with an edge, a fast phasor like a trigger train, and a wrong guess would switch the mode in mid-cycle). `isConnected()` only reveals that a cable is plugged in.
  - **Stage times as weights:** Delay, Attack, Hold and Release divide the cycle, instead of a single rise/fall break point. This fits Europa's four stages, level and curve stay unchanged. Weight 0 drops a segment, all 0 gives an equal distribution.
  - **Ratio p/q with free knobs (1..16)**, not a fixed list. Unrolled phasor with a cycle counter modulo 720720 (lcm 1..16), p/q reduced, a reset input (TRIG channel 1, previously unused in Europa) fixes the phase relationship.
  - **Ratio transfer at the input wrap, strict as default:** y stays continuous, the price is the wait (for example every 12 cycles with 4 → 6). On request (CV modulation) additionally **loose** as a module-wide context menu item (`ratioLoose`): immediate transfer with a phase jump, smoothed with minBLEP. The occasion was Rack tests with square-wave CV in which the strict transfer lost changes.
  - **Direction:** hysteresis in the wrap detection. With a falling ramp the input is mirrored so that rise always runs before fall and the EOC sits at x = 0. The phasor output is mirrored back and stays direction-true (Kinovice's recommendation adopted). A switch "mirror on falling ramp" is dropped.
  - **ENV as phasor output instead of new jacks:** three positions (envelope, phasor linear, phasor warped), the envelope level stays reachable via VCA OUT and OR because VCA IN is normalled to 10 V. Rejected: new jacks (the panel had no free column) and a menu item instead of a param.
  - **Phasor warped = (k + curve(u)) / 4**, so the curve knobs also act on the phasor output (Kinovice's request), the linear phasor remains the clean reference.
  - **CV assignment without new jacks:** curve on LEVEL CV channels 5–8 (applies in both modes), offset, p and q on TIME CV channels 5–7 in phasor mode (channel 5 is no longer the rate CV there). Scaling like the time CV: additive, 10 V = full knob travel (curve ±5 V). p and q with 0.1 steps of hysteresis when rounding.
  - **Pass-through of the phasor:** with no cable at T1/GATE, a generator in phasor mode takes the phasor of the previous generator, because T1/GATE is mono and exists once per generator (one cable into generator 1 feeds all four).
  - **Unexplained jumps are reported to the minBLEP** (Kinovice's request after listening): reset, offset jump, direction change and phase jump with loose transfer. No EOC for these events.
  - **Panel:** PHASOR strip above the OR column in the previously empty area, 23 HP remain, five params per generator at the end of the enum. Rejected: widening the panel. The button widget of the quadrature button is reused (adjustable undo name).
  - **Finding from the Rack tests:** the ratio CV is sampled only at the frequency of the input phasor (rule of thumb: LFO at most a quarter). Additional reading points (ratio at every y wrap, a 1–2 ms settling window after the wrap) were considered and deliberately not implemented, because a faster input phasor with division via q solves the problem and the behavior would otherwise be harder to predict.
  - **Build:** Rack builds with C++11, hence a gcd of its own (`phasorGcd()`) instead of `std::gcd` (build error in the first Rack test). Standalone test `tests/phasor_core_test.cpp`. Functional test in Rack by Kinovice on 2026-10-06 (scope see [Section 6](#sec6)).
- **Documentation last (Kinovice):** all features first, then the documentation, including the concept document update. Separate documentation file for Europa because the plugin has five modules and Europa has nothing technically in common with the MARF core documentation.
