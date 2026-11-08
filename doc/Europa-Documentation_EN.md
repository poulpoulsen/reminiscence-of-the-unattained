# reminiscence of the unattained – Europa (Technical Documentation)

**Europa** is the fifth module of the plugin: a quad function generator with VCA.
Four independent envelope generators, each with four stages (Delay, Attack, Hold,
Release), each work simultaneously as a VCA and as an envelope output. The interface shows
only one generator at a time, while all four keep running in the background. This documentation
describes every control, every input and output, the internal signal processing,
the C++ structure and the design decisions.

The name follows the Anguissola family (like Sofonisba, Amilcare, Bianca and
Lucia): Europa is one of the sisters.

**Note:** The plugin *reminiscence of the unattained* is an independent work, strongly inspired by the wonderful world of Buchla modules. It has no connection to Buchla, Tiptop Audio or any other manufacturer, and is neither authorized nor endorsed by them. Product and company names mentioned are trademarks of their respective owners and only describe which functional concepts inspired a module.

Status: 2026-10-06. I have tested all described functions in a running Rack,
unless they are explicitly marked as open (see [Section 6](#sec6)).

---

<a id="toc"></a>

## Contents

*Each entry has two links: the first (standard Markdown anchor) for GitHub, Codeberg, VSCodium preview & most other viewers; "Obsidian" is the wikilink specifically for Obsidian.*

1. [Overview](#sec1) · [[#1. Overview|Obsidian]]
2. [Controls](#sec2) · [[#2. Controls|Obsidian]]
   - [Panel Layout](#sec2-1) · [[#2.1 Panel Layout|Obsidian]]
   - [Generator Selection (GEN and ACTIVATE)](#sec2-2) · [[#2.2 Generator Selection (GEN and ACTIVATE)|Obsidian]]
   - [Generator Switches (MODE, RETRIG D, LOOP)](#sec2-3) · [[#2.3 Generator Switches (MODE, RETRIG D, LOOP)|Obsidian]]
   - [Stage Controls (LEVEL, TIME, RANGE, CURVE)](#sec2-4) · [[#2.4 Stage Controls (LEVEL, TIME, RANGE, CURVE)|Obsidian]]
   - [Inputs and Outputs](#sec2-5) · [[#2.5 Inputs and Outputs|Obsidian]]
   - [OR Outputs and Quadrature Buttons](#sec2-6) · [[#2.6 OR Outputs and Quadrature Buttons|Obsidian]]
   - [Context Menu (Antialiasing, Ratio Change)](#sec2-7) · [[#2.7 Context Menu (Antialiasing, Ratio Change)|Obsidian]]
   - [Phasor Mode (Operation)](#sec2-8) · [[#2.8 Phasor Mode (Operation)|Obsidian]]
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
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**Structure**<br>
Four generators with four stages each: **Delay, Attack, Hold, Release**. Each generator has a VCA (signal in, signal out) and also outputs the envelope separately (0–10 V). The module is 23 HP wide (116.84 × 128.5 mm); the OR column sits to the right of the main block.

**One generator visible, all active**<br>
The controls always show only the selected generator. A selection knob (GEN) with an activate button (ACTIVATE) switches between them. All four generators keep running independently, and their jacks are always visible.

**Two modes and loop**<br>
Each generator offers One-Shot or Sustain, a three-position loop switch (Off / Gate / Free, effective in One-Shot only) and a retrigger switch. The retrigger switch determines whether a retrigger starts from the current level or from the delay level.

**Main use**<br>
Envelopes, trapezoid, AR and gate shapes, and above all LFO operation (loop, with LFOs up into the audio range as an extra). The delay and release levels, like all other levels, can be controlled via CV. This allows LFOs with offset and wavefolder-like effects (see [Section 7](#sec7)).

**Extensions (2026-10-01)**<br>
Free cycle (LOOP = FREE, loop without gate), rate CV (channel 5 on the TIME CV input, bipolar, exponential, shape-preserving), two OR outputs (maximum of each pair, 1+2 and 3+4) and a switchable quadrature coupling per pair (generator 2 or 4 starts at the end of attack of generator 1 or 3). See [2.3](#sec2-3), [2.5](#sec2-5), [2.6](#sec2-6), [3.2](#sec3-2) and [3.8](#sec3-8).

**Extension (2026-10-02)**<br>
Optional antialiasing (minBLEP) for the hard level jumps of the envelope, switchable in the right-click menu, default off. See [2.7](#sec2-7) and [3.9](#sec3-9).

**Extension (2026-10-05)**<br>
The quadrature button has a third position: the **handshake**. Both generators of a pair wait for each other at the end of attack and couple their decays. This creates a true quarter period between the two envelopes. See [2.6](#sec2-6) and [3.8](#sec3-8). The functional test in Rack is still pending (see [Section 6](#sec6)).

**Extension (2026-10-06)**<br>
**Phasor mode** per generator: The T1/GATE jack accepts a **phasor** (ramp 0–10 V) instead of a gate, and the four stage times become weights that divide one cycle into Delay, Attack, Hold and Release. Envelopes, phasor shaping and sequencers thus stay locked in time to an external phasor, without a time base of their own. Added to this are a free ratio p/q, an offset, direction detection (falling ramps also run as rise before fall), a reset, a phasor output on ENV and CV for curve, offset and ratio. See [2.8](#sec2-8) and [3.10](#sec3-10).

**Engine**<br>
A dedicated Rack-independent class `EnvGen4` (`EnvGen4.hpp`, only `<cmath>`/`<algorithm>`) runs as four instances in the module `Europa` (`Europa.cpp`). Sofonisba (`Afg`) remains untouched.

---

<a id="sec2"></a>

## 2. Controls
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

<a id="sec2-1"></a>

### 2.1 Panel Layout
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

From top to bottom, the panel consists of four areas (dimensions in mm, panel 116.84 × 128.5, i.e. 23 HP; the main block measures 101.6 mm, with the OR column attached on the right):

```
EUROPA
[ GEN-Knob ] [ ACTIVATE ]  |  MODE        RETRIG D      LOOP
                              SUSTAIN      ON            FREE
                                                         GATE
                              ONE-SHOT     OFF           OFF

 DELAY       ATTACK      HOLD        RELEASE      (dark name band)
 [Level]     [Level]     [Level]     [Level]
 [Time]      [Time]      [Time]      [Time]
 [Range]     [Range]     [Range]     [Range]
 [Curve]     [Curve]     [Curve]     [Curve]

 T1/GATE TRIG TIME CV LEVEL CV HOLD CV VCA IN | VCA OUT ENV EOC | OR   (outputs on dark plate, OR column on the right)
 1 ...                                                     (row of generator 1)
 2 ...
 3 ...
 4 ...
```

**Header area (y 10.5–29.5)**<br>
On the left is a frame with the GEN knob (14 mm diameter, center x 14) and the ACTIVATE button (center x 31). On the right follows a frame with the three generator switches (center x 51.5 / 72.5 / 89.5).

**Stage columns (y 31–83.3)**<br>
Four frames, each 22 mm wide, at x 13.3 / 38.3 / 63.3 / 88.3, each with a dark name band. Rows from top: LEVEL (trimpot), TIME (knob), RANGE (switch), CURVE (trimpot).

**Jack block (y 84.6–126.3)**<br>
Four rows (one per generator) at y 93 / 102.3 / 111.6 / 120.9, nine columns spaced 10 mm apart starting at x 13. The inputs (T1/GATE, TRIG, TIME CV, LEVEL CV, HOLD CV, VCA IN) sit on the left on a light background, the outputs (VCA OUT, ENV, EOC) on the right on a dark plate. At the left of each row are the status LED, the selection LED and the generator number 1–4.

**OR column (x 101.5–114.5, y 84.6–126.3)**<br>
An additional column to the right of the main block, designed like the jack columns: a 10 mm wide dark plate in a light frame with a narrow border (1.5 mm at the sides, 0.8 mm top and bottom) and 2.3 mm clearance to the panel edge. Two blocks one above the other, each pair with a label (OR 1+2, OR 3+4), a jack and a three-position click switch with LED for the quadrature coupling. See [2.6](#sec2-6).

**PHASOR strip (x 101.5–114.5, y 10.5–82.6)**<br>
Above the OR column, at the same width, sits a light frame with the controls of the phasor mode for the displayed generator. From top: PHASOR switch, OUT button, NUM (numerator), DEN (denominator), OFFSET, with the yellow pending LED beside them. The strip uses the previously empty area, and the panel width remains 23 HP. See [2.8](#sec2-8).

**Row highlight**<br>
The row of the displayed generator has a blue band.

<a id="sec2-2"></a>

### 2.2 Generator Selection (GEN and ACTIVATE)
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**GEN knob**<br>
Vertical dragging preselects the number 1–4 (`pendingGen`), 4 mm of drag per step. The number in the knob is green as long as the preselection matches the displayed generator, otherwise amber.

**ACTIVATE button**<br>
Applies the preselection (`displayedGen = pendingGen`). It is green when preselection and display match, otherwise red. Only then do all controls switch to the new generator.

**Selection LEDs**<br>
One blue LED per row: bright for the displayed generator, dim for the merely preselected one.

`pendingGen` and `displayedGen` are deliberately **not Rack params**: undo/history, automation and preset recall should not affect pure navigation. Both are saved via JSON ([Section 5](#sec5)).

<a id="sec2-3"></a>

### 2.3 Generator Switches (MODE, RETRIG D, LOOP)
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

MODE and RETRIG D are toggle switches (CKSS) with two positions: **up = on**, down = off. When a switch is up, a green field sits behind the upper label. LOOP is a three-position switch (`CKSSThree`): up FREE, middle GATE, down OFF. The active position is marked green (OFF stays unmarked). All three switches belong to the displayed generator.

**MODE** (`MODE_PARAM`)<br>
Up: SUSTAIN, down: ONE-SHOT (default). See [3.1](#sec3-1).

**RETRIG D** (`RETRIG0_PARAM`)<br>
Up: ON. A retrigger (rising edge on T1/GATE while running) then starts from the **delay level** instead of the current output level. At the retrigger, the output jumps to the delay level, which can be audible. Off (default): The start occurs from the current level, without a click. RETRIG D affects only T1/GATE; jump triggers on TRIG always start from the current level. The internal name `RETRIG0_PARAM` stays unchanged for historical reasons (formerly "Retrig 0": jump to 0 V).

**LOOP** (`LOOP_PARAM`, 0..2, default 0)<br>
Three positions, effective in One-Shot only, without function in Sustain mode. **OFF** (down, 0): no loop, one pass per gate. **GATE** (middle, 1): loop while the gate is held. As long as the gate stays high, a new cycle begins automatically after release (the behavior of the former switch in position ON). **FREE** (up, 2): free cycle (auto-cycle). The generator starts by itself without a gate and runs continuously in a loop. A gate is not required, but still acts as a retrigger. A switch saved earlier in position ON loads in the middle position (GATE). See [3.1](#sec3-1).

<a id="sec2-4"></a>

### 2.4 Stage Controls (LEVEL, TIME, RANGE, CURVE)
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**LEVEL** (trimpot, 0–10 V)<br>
Target level of the stage. All four stages have a level: `LEVEL_DELAY_PARAM` (default 0 V), `LEVEL_ATTACK_PARAM` (default 10 V), `LEVEL_HOLD_PARAM` (default 10 V), `LEVEL_RELEASE_PARAM` (default 0 V). The levels are unipolar (see [3.3](#sec3-3)) and can additionally be controlled via LEVEL CV.

**TIME** (knob, `TIME_PARAM`, 0..1, default 0.5)<br>
The knob position, together with the RANGE switch, determines the stage time (exponential mapping, exactly 0 at the far left). The tooltip and right-click field show the time in seconds. Input: `5 ms`, `1.5 s`, `200 us`. A number without a unit takes the unit of the value currently displayed (below 1 s: ms, otherwise s). Values above the range maximum are limited to the end stop. See [3.2](#sec3-2).

**RANGE** (switch, `RANGE_PARAM`, 0..3, default TR3)<br>
A click advances to the next range (TR1 → TR2 → TR3 → TR4 → TR1), Shift+click goes backward. Right-click opens Rack's standard menu with the four ranges as a selection list (`RANGE_PARAM` is a `configSwitch` with labels). Undo/redo are supported. Ranges (all starting at 0): TR1 0–30 ms, TR2 0–300 ms, TR3 0–3 s, TR4 0–30 s.

**CURVE** (trimpot, `CURVE_PARAM`, −1..+1, default 0)<br>
Curve shape of the stage. Left = round, middle = linear, right = pointed, each relative to the direction of movement. See [3.4](#sec3-4).

<a id="sec2-5"></a>

### 2.5 Inputs and Outputs
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

Each generator has nine jacks, 36 in total, plus the two OR outputs (see [2.6](#sec2-6)). All jacks are always visible, regardless of the displayed generator.

**T1/GATE** (`GATE_INPUT`, mono)<br>
Starts the envelope at stage 1 (Delay). One-Shot: trigger (edge), Sustain: gate (level). Schmitt trigger with 0.1 V / 1 V hysteresis.

**TRIG** (`TRIG_INPUT`, poly)<br>
Jump triggers: channel 2 = Attack, channel 3 = Hold, channel 4 = Release, channel 1 unused. Triggering occurs on a rising edge above 2 V, without hysteresis. A mono cable lands on channel 1 and does nothing. See [3.1](#sec3-1).

**TIME CV** (`TIMECV_INPUT`, poly)<br>
Channels 1–4 = time CV for Delay, Attack, Hold, Release, added to the knob position. **Channel 5 = rate CV** (bipolar, exponential, 1 octave per volt, limited to ±5 V): It changes the speed of the whole generator without altering the curve shape. A mono cable affects only Delay; channel 5 is present only with a cable of at least five channels. See [3.2](#sec3-2).

**LEVEL CV** (`LEVELCV_INPUT`, poly)<br>
Channels 1–4 = level CV for Delay, Attack, Hold, Release, added to the knob (1 V = 1 V), sum limited to 0–10 V. A mono cable affects only the delay level. See [3.3](#sec3-3).

**HOLD CV** (`HOLDCV_INPUT`, mono)<br>
Dedicated jack for the hold time, independent of TIME CV channel 3. It is effective only with a patched cable **and** in One-Shot (loop included), not in Sustain mode. With a cable, the voltage completely replaces the TIME knob of the hold stage; only the RANGE switch (TR1–TR4) remains effective: 0 V = no hold, rising to just under 9.5 V within the selected range, from 9.5 V infinite hold (ended only by a trigger on TRIG channel 4). There is no relation to the gate. Without a cable, the hold stage remains unchanged (knob + TIME CV channel 3). See [3.1](#sec3-1) and [3.2](#sec3-2).

**VCA IN** (`SIGNAL_INPUT`, poly)<br>
Signal input of the VCA, normalled to 10 V without a cable. See [3.5](#sec3-5).

**VCA OUT** (`VCA_OUTPUT`, poly)<br>
Signal times envelope, channel count as VCA IN (mono without a cable).

**ENV** (`ENV_OUTPUT`, mono)<br>
Envelope, 0–10 V.

**EOC** (`EOC_OUTPUT`, mono)<br>
Trigger (10 V) at the end of the cycle. See [3.6](#sec3-6).

**Phasor mode: changed assignment**<br>
When a generator is in phasor mode ([2.8](#sec2-8)), the following applies to its jacks:
- **T1/GATE:** phasor input, 0–10 V, instead of trigger/gate. Without a cable, the generator takes the phasor of the previous generator, provided that generator is also in phasor mode (so one cable on generator 1 feeds all four). Generator 1 has no predecessor; without a cable, 0 V is present there.
- **TRIG:** Channel 1 is the **reset** of the cycle counter (rising edge from 2 V). The stage triggers (channels 2–4) have no effect in phasor mode.
- **TIME CV:** Channels 1–4 act as stage weights (like times). **Channel 5 = offset**, **channel 6 = numerator p**, **channel 7 = denominator q** (instead of the rate CV), each added to the knob, 10 V corresponds to the full knob travel. For this the cable needs at least 5, 6 or 7 channels respectively.
- **LEVEL CV:** Channels 1–4 as before. **Channels 5–8 = curve** of Delay, Attack, Hold, Release (additive, ±5 V corresponds to the full range −1…+1, sum limited to −1…+1). The curve CV applies in **both** modes, including classic operation.
- **HOLD CV:** replaces the weight of the hold stage, also in Sustain mode, never infinite.
- **ENV:** envelope or phasor, depending on the OUT button. **VCA OUT** and the **OR outputs** always work with the envelope.
- **EOC:** end of the cycle of y (see [3.10](#sec3-10)).

<a id="sec2-6"></a>

### 2.6 OR Outputs and Quadrature Buttons
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**OR 1+2, OR 3+4** (`OR_OUTPUT`, two outputs, mono)<br>
They deliver the **higher of the two ENV levels** of a pair (generator 1+2 or 3+4), 0–10 V, tapped before the VCA. They work regardless of which generator is currently displayed. See [3.8](#sec3-8).

**Quadrature button** (`QUAD_PARAM`, two params with three positions each, 0..2)<br>
Click switch with LED below the respective OR jack, default Off. A click advances (Off → Follow → Handshake → Off), Shift+click goes backward. Right-click opens Rack's standard menu with the three named positions. Undo/redo are supported. The LED shows the position: dark = **Off**, green = **Follow**, amber = **Handshake**. The buttons belong to the pair, not to the displayed generator.

**Off**<br>
The generators of the pair are independent.

**Follow**<br>
**Generator 2 (or 4) follows generator 1 (or 3)**: It starts at the end of the partner's attack and ignores its own T1/GATE. The decays of both generators run independently of each other.

**Handshake**<br>
Both generators wait for each other at the end of attack and couple their decays (model: the quadrature mode of the inspiring envelope generator). The gate goes to T1/GATE of generator 1; generator 2 starts at the attack end of generator 1. The exact sequence is described in [3.8](#sec3-8).

<a id="sec2-7"></a>

### 2.7 Context Menu (Antialiasing, Ratio Change)
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**Antialiasing (minBLEP, ENV/VCA/OR)** (`Europa::antialias`, not a Rack param)<br>
Checkbox in the module's right-click menu, **module-wide for all four generators**, default **off**. When enabled, the module smooths the level jumps of the envelope (stage with time 0, retrigger jump) at ENV, at the VCA and at the OR outputs. EOC, gate logic and quadrature start remain unaffected. The switch is deliberately not a param (undo, automation and preset recall should not affect it) and is saved via JSON ([Section 5](#sec5)). For operation and limits see [3.9](#sec3-9).

**Phasor: loose ratio change (every input wrap)** (`Europa::ratioLoose`, not a Rack param)<br>
Checkbox in the right-click menu, **module-wide for all generators in phasor mode**, default **off** (strict adoption). When enabled, each generator adopts a new ratio p/q at every input wrap, not only at a wrap where y lies exactly on the cycle start. y jumps in the process. The jump is reported as an envelope jump and smoothed when antialiasing is enabled. The switch is saved via JSON ([Section 5](#sec5)); old patches load strict. The differences in detail are described in [3.10](#sec3-10).

<a id="sec2-8"></a>

### 2.8 Phasor Mode (Operation)
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

Phasor mode turns a generator into an **envelope whose timing is determined entirely by an external phasor**. The generator then has no time of its own: Everything is a pure mapping of the input phase, nothing can drift, and envelopes, sequences and modulations stay locked in time. The controls are in the PHASOR strip above the OR column ([2.1](#sec2-1)) and belong to the displayed generator. All settings are real Rack params (five per generator). The operation is described in [3.10](#sec3-10).

**PHASOR** (`PHASOR_PARAM`, toggle switch, default off)<br>
Up: phasor mode on. The generator then behaves as described in [3.10](#sec3-10). Down (default): The generator works exactly as before, and all old patches remain unchanged. A mode change resets the engine of this generator. The mode applies per generator, so two generators can run as classic envelopes and two as phasor envelopes.

**OUT** (`PHASOR_OUT_PARAM`, click button with three positions 0..2, default 0)<br>
Determines what the ENV jack outputs in phasor mode. A click advances (envelope → linear → shaped), Shift+click goes backward. Right-click opens Rack's standard menu with the three named positions. Undo/redo are supported. The LED shows the position: dark = **envelope**, green = **phasor linear**, amber = **phasor shaped**. The widget is the same as for the quadrature button (`EuropaQuadButton`), with its own undo name.
- **Envelope:** ENV outputs the envelope (0–10 V).
- **Phasor linear:** ENV outputs y, 0–10 V, a uniform ramp with ratio and offset.
- **Phasor shaped:** ENV outputs `(k + curve(u)) / 4`. Each of the four segments occupies a quarter. The weights therefore determine how fast the input phase moves through the four quarters, and the curve controls shape the progression within each segment. This is the phasor for downstream modules and downstream generators.

Both phasor outputs are **direction-true**: A falling input ramp yields a falling output ramp. VCA OUT and the OR outputs output the envelope in all positions.

**NUM** and **DEN** (`RATIO_P_PARAM`, `RATIO_Q_PARAM`, notched trimpots, 1..16, default 1)<br>
Numerator p and denominator q of the ratio p/q. 2/1 runs twice as fast as the input, 1/2 half as fast, 3/2 one and a half times as fast. The values are reduced (8/8 becomes 1/1). The tooltip shows the value. Both can be controlled via CV (TIME CV channels 6 and 7). When a new ratio is adopted is described in [3.10](#sec3-10).

**OFFSET** (`PHASE_OFFSET_PARAM`, 0–100 %, default 0)<br>
Phase shift of the cycle of y in fractions of a cycle, forward in the direction of travel. Controllable via CV (TIME CV channel 5); the value wraps around as a phase.

**Pending LED** (`PENDING_LIGHT`, yellow)<br>
Lights up as long as a change of p or q is waiting for a valid input wrap. In strict mode this can take several input cycles, in loose mode only until the next wrap.

**No effect in phasor mode**<br>
MODE (Sustain), RETRIG D, LOOP, the rate CV (channel 5 on the TIME CV input is the offset there) and the stage triggers on TRIG (channels 2–4). The quadrature buttons are not intended for a generator in phasor mode ([Section 6](#sec6)).

**Quick start**<br>
1. Patch a phasor source (ramp 0–10 V, rising or falling) to T1/GATE of the generator. With several generators in phasor mode, one cable on generator 1 is enough.
2. Switch PHASOR on.
3. Set the times (TIME and RANGE) as **fractions** of the cycle: Equal times yield four equally long segments, unequal times shift the segment boundaries.
4. Set levels and curve as for a normal envelope.
5. Set NUM/DEN and OFFSET. A reset on TRIG channel 1 fixes the counter position.
6. Use OUT to choose whether ENV outputs the envelope or the phasor.

---

<a id="sec3"></a>

## 3. Internal Signal Processing
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

<a id="sec3-1"></a>

### 3.1 State Machine per Generator
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**One-Shot**<br>
A rising edge on T1/GATE starts Delay → Attack → Hold → Release, followed by rest (output at the release level, EOC pulse). A trigger on TRIG jumps to the selected stage and runs on to the end of the cycle, independent of the gate.

**Sustain**<br>
Gate high starts Delay, then Attack, then **holds** at the attack level. Hold is skipped, the gate replaces it. (If a TRIG jump went into Hold, the envelope holds at the hold level at the end of Hold.) Gate low starts Release from the current level. If the gate falls during Delay while the output is still at the rest level, the cycle aborts without release and without EOC. Triggers on TRIG act only when the gate is high. An infinite hold via time CV is ignored in Sustain mode.

**Internal loop (One-Shot)**<br>
At the end of Release, the gate level is checked. Gate high: Delay follows in the same sample, and the remaining time is carried over. Gate low: end. A short trigger alone yields a single pass; for the loop, the gate must be held. The loop has no cable latency, and the period corresponds exactly to the sum of the stage times. EOC fires at every cycle end, including in the loop.

**Free cycle (LOOP = FREE)**<br>
One-Shot only. A resting generator starts by itself via `startCycle()`, that is, like with a gate (retrigger switch, delay level). After Release, the next cycle begins immediately, even without a gate: `loopEnabled` and `freeCycle` are set, and the loop runs on `gateHigh || freeCycle`. A gate still acts as a retrigger. If FREE is switched off (position GATE or OFF), the running cycle ends regularly at the end of Release. The output does not jump in the process and rests afterwards at the release level. With infinite hold, the generator stops until a trigger arrives on TRIG. EOC fires at every cycle end. If all times are 0, one pass runs per sample (degenerate case, stable).

**Patch loop**<br>
EOC → T1/GATE of the same generator. The pulse starts the next pass; a pulled cable stops after the current pass. The cable has 1 sample of latency. Without delay, use EOC → TRIG channel 2. This is also suitable for chains and rings across several generators. In Sustain mode it makes no sense (gate is a level).

**Retrigger**<br>
A rising edge on T1/GATE while running restarts stage 1. RETRIG D off: from the current level, on: from the delay level.

**Hold duration via CV (One-Shot/Loop only)**<br>
With HOLD CV patched, the voltage alone determines the length of the hold stage, independent of the gate: 0 V means no hold (the stage runs through with time 0), rising to infinite hold from 9.5 V. The TIME knob of the hold stage is then ineffective, and the RANGE switch continues to determine the scale. In Sustain mode, HOLD CV is ignored; there the gate determines the hold duration as before.

**Time 0**<br>
Every stage can have time 0 (knob at far left or CV). Such a stage is passed through in the same sample, and remaining time goes into the next stage. Delay 0: Attack starts immediately. Attack 0: step to the attack level. Release 0: immediate jump to the release level. With time 0 in all stages, a maximum of four stage transitions per sample is allowed (minimum cycle 1 sample) so that the loop does not hang.

**Infinite hold**<br>
A time CV ≥ 9.5 V holds the stage indefinitely until a trigger on TRIG starts the next stage (One-Shot only). The output stays at the start level of the stage.

**Mode change while running**<br>
If the switch changes from Sustain to One-Shot during the hold phase, the hold ends and the envelope continues normally.

<a id="sec3-2"></a>

### 3.2 Time Signal Path
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**Knob mapping**<br>
`t = max · (B^x − 1) / (B − 1)` with `x` = knob position (0..1) plus time CV/10 V (limited to 0..1), `B = ENV_TIME_EXP_BASE = 1000` and `max` = upper limit of the range. At `x = 0` the time is exactly 0. The knob center of TR1 lies at about 0.9 ms. The inverse function `envPositionFromTime()` serves text input.

**Time CV**<br>
Added to the knob position, 10 V corresponds to the full knob travel. The sum is limited to 0..1. A CV ≥ 9.5 V (the CV alone) means infinite hold (not in Sustain mode).

**Hold CV (special case)**<br>
With HOLD CV patched in One-Shot/Loop, the hold time is not added but calculated directly from the voltage: with the same function as above, but with a fixed knob position of 0 and the voltage as the sole input (`envResolveTime(0, cv, range, true)`). The voltage thus covers the full knob position 0..1. The cache is separate and independent of the time knob's cache.

**Rate CV (channel 5 of TIME CV)**<br>
Channel 5 of the polyphonic TIME CV input does not act on a stage time but on the speed of the whole generator. Factor = `2^(U · ENV_RATE_OCTAVES_PER_VOLT)`, with `U` limited to ±`ENV_RATE_CV_LIMIT_V` (values 1 octave per volt and 5 V, giving 1/32 to ×32; `envRateFromVolts()`). 0 V gives a factor of 1, positive is faster, negative slower. The rate scales only the time advance in `EnvGen4::process()` (`elapsed += dt · rate`). Since the phase of a stage follows from `elapsed` divided by the stage time, the curve shape is preserved at every rate, even with a change in the middle of a stage (the output does not jump). Pulse length at EOC and cycle measurement run in real time. Infinite hold and sustain hold remain unaffected, and gate and triggers respond unchanged. Without a cable, and with a mono or four-channel cable, the factor stays 1, and old patches sound unchanged. The cycle rate is limited by the sample rate (at most one cycle per sample). The factor is recalculated per generator only when the voltage changes.

**Cache**<br>
The resolved time contains a `pow()` and is recalculated per stage only when the knob, CV, range or mode changes.

<a id="sec3-3"></a>

### 3.3 Level Signal Path
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**Level**<br>
Each stage has a target level, unipolar 0–10 V: knob plus LEVEL CV, sum limited to 0–10 V. The delay level is the start level of the cycle (jump target only with RETRIG D, otherwise target of the delay stage); the release level is the end level and rest level.

**Rest level**<br>
At standstill the output sits at the release level, even right after power-up. The knob acts live without a cycle running. After Release the output stays there. With the internal loop, the next cycle starts from this level.

**Interpolation**<br>
Each stage interpolates from the level at stage start (`voltageFrom`) to the target level. If a level knob or the level CV changes during a stage, the stage keeps interpolating toward the new target from the next sample on. This explains the lively effects with LFO-modulated levels.

<a id="sec3-4"></a>

### 3.4 Curve Shape
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**Bipolar mapping**<br>
CURVE −1..+1. From the magnitude `a = |c|` follows the curvature exponent `power = 2.5^(a · (2 + a²))` (maximum 2.5³ ≈ 15.6, linear at `c = 0`).

**Relative to the direction of movement**<br>
For a rising stage: `c < 0` (round) `f = 1 − (1 − t)^power` (fast start, soft target), `c > 0` (pointed) `f = t^power` (soft start, fast target). If the stage runs downward (target below start level), the sign is mirrored. "Round" thus always means: **flat at the upper end of the level**. Attack shoots up and flattens at the top, Release starts flat at the top and falls faster toward the end. Both knobs left yield a symmetrical, bell-like envelope, both right a pointed one. A test with curve −0.5 gave Attack 5.54 V at 25 % and 8.57 V at 50 %, Release 9.79 V at 25 % and 8.57 V at 50 %.

**Cache**<br>
`power` depends only on the magnitude and is recalculated when the curve changes.

<a id="sec3-5"></a>

### 3.5 VCA
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

The gain is the envelope times 0.1 (0 at 0 V, 1 at 10 V) and acts equally on all channels. The signal may be bipolar, and the sign is preserved (linear VCA, no rectifier). The gain never becomes negative because all levels are limited to 0–10 V. Without a cable on VCA IN, 10 V is present, so VCA OUT then corresponds to ENV as a signal of its own. The channel count of VCA OUT follows VCA IN. The VCA is calculated only when VCA OUT is patched. With antialiasing enabled, the gain uses the smoothed envelope ([3.9](#sec3-9)), which likewise stays limited to 0–10 V.

With a release level above 0 V, the VCA does not fully close at rest.

<a id="sec3-6"></a>

### 3.6 EOC Output
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

The EOC pulse (10 V) fires at the end of Release. The pulse length is adaptive: the minimum of 1 ms and half the length of the last cycle, at least 1 sample. What matters is the length of the most recently completed cycle, not the time since the last fired EOC, so that a swallowed pulse does not double the period. There is at least 1 sample of pause between two pulses (rising edge guaranteed, minimum cycle 2 samples). An event during a running pulse merges with it. An abort in Delay (Sustain) produces no EOC. At a rate other than 1, the pulse length stays in real time, and the measured real cycle length is what counts.

<a id="sec3-7"></a>

### 3.7 Status LEDs
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

Each row has a status LED with three independent color channels (only one lights at a time): **Red** = stopped, **Green** = running (same green as ACTIVATE), **Yellow** = holding (Sustain or infinite stage time). The LED is updated only every 64 samples. "Running" applies to the whole window since the last update, red only on complete idle, so that a patch loop (EOC → T1) does not flash red. The LED draws with normal blending without a halo, because Rack's standard screen blend mode fades to pastel tones on the light panel.

<a id="sec3-8"></a>

### 3.8 Quadrature Coupling and OR Logic
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**OR**<br>
The OR logic works purely on the output side in `Europa::process()` after the generator loop: `max(gens[0].envOut, gens[1].envOut)` and `max(gens[2].envOut, gens[3].envOut)`. The engines remain untouched, and the level is tapped before the VCA. With antialiasing enabled ([3.9](#sec3-9)), the maximum compares the smoothed levels (`envAA[]`), otherwise they are identical to `envOut`.

**Start event**<br>
`EnvGen4::attackEnded` is true for exactly one sample. `finishStage()` sets it when Attack ends, also with attack time 0 and during sustain hold. A jump trigger directly into Hold triggers no event.

**Follower (Follow mode)**<br>
With the button active (position Follow), `EnvParams::coupled` is set for generator 2 (or 4). Its gate edge then starts no cycle; the start comes via the optional parameter `startRequest` of `process()`. `Europa::process()` derives it from the partner's `attackEnded`. Generator 1 runs in the loop before generator 2, so the event is present in the same sample. The follower's own T1/GATE is ignored; instead, the partner's gate state (`gateHigh`) applies. The triggers on TRIG for stages 2–4 remain active. With coupling active, LOOP = FREE behaves like GATE (loop while the partner's gate is held), so that the follower does not start itself.

**Sustain mode**<br>
In Sustain mode, the follower takes over the partner's gate state: It starts at the attack end, holds at the end of Attack as long as the partner's gate is high, and begins Release as soon as it falls.

**Delay of the follower**<br>
The follower starts with its delay stage. A delay time on generator 2 (knob or delay CV on channel 1 of TIME CV) shifts the beginning of its envelope in addition to the partner's attack end and thereby sets the offset between the two curves.

**Loop**<br>
For a single pass, the LOOP switch of **both** generators must be set to OFF. If generator 1 runs in a loop (GATE or FREE), the two-stage cycle repeats: Each attack end of generator 1 restarts generator 2.

**Together with OR**<br>
Coupled and combined via the OR output, two generators yield a two-part envelope whose second part sets in at the peak of the first.

**Handshake (position 2, 2026-10-05)**<br>
The model is the quadrature mode of the inspiring envelope generator according to a public description. The source's diagrams could not be evaluated; the implementation is an independent interpretation of the described sequence. The master is generator 1 (or 3), the follower generator 2 (or 4). The four-step cycle:

1. The master starts (gate on T1/GATE), runs through Delay and Attack and **holds** at the attack level.
2. With the master's attack end, the follower starts (with its delay stage) and rises.
3. When the follower reaches its attack end, it also **holds** at its attack level, and the master begins Release.
4. When the master reaches its end (minimum), the follower begins Release.
5. At the end of the follower's Release, the cycle ends. With LOOP on the master, the master then restarts.

**Period and phase**<br>
The period is the sum of Delay and Attack of the master, Delay and Attack of the follower, and Release of the master and Release of the follower. With equal times, the follower is shifted relative to the master by exactly a quarter period (in the standalone test the deviation is 0.002 V). The OR output then delivers a trapezoid with a plateau twice as wide, because one of the two is always at the top. The time controls act in a coupled way: A longer attack of the follower lengthens the master's plateau, and a longer release of the master shifts the follower's release.

**Hold stage omitted**<br>
Waiting replaces Hold, as on the original. Hold time, hold level and HOLD CV have no effect in handshake. Holding occurs at the attack level.

**Loop**<br>
The master's LOOP switch repeats the four-step cycle, corresponding to the "Pulse B → Trigger A" cable on the original: The master restarts after the follower's cycle end. GATE: only while the gate is held. FREE: without a gate. In handshake the master has no internal loop; it always waits until the follower is done. With LOOP = OFF, exactly one cycle runs.

**Follower in cycle**<br>
If the follower's LOOP switch is on GATE (master's gate) or FREE, the follower runs freely and is not restarted by the master's attack end. The master waits at its attack end for the follower's next attack end. The follower holds at its attack end only if the master is currently waiting; otherwise it runs on through Release without holding. During the master's Release, the follower stays up and cycles on afterwards. This corresponds to the description of the cycle case on the original. FREE on the follower does not behave like GATE here (unlike in Follow mode).

**Sustain**<br>
Handshake applies only in One-Shot. If one of the two generators is in Sustain mode, Follow behavior applies automatically. If handshake is switched off, or a generator is set to Sustain while a generator is waiting, the hold ends immediately. An orphaned partner therefore cannot keep the generator stuck.

**External retrigger**<br>
If the master is retriggered externally while the follower is still holding, the follower restarts with the master's next attack end, even with LOOP on the follower. Without this rule, the two would block each other.

**Events and implementation**<br>
Besides `attackEnded`, the engine reports the new event `cycleEnded` (one sample long, Release elapsed). Waiting internally uses the same hold as Sustain mode (`sustainHeld`), additionally marked with `hsHeld`. That is why the status LED lights yellow in the meantime. `envQuadSetup()` in `EnvGen4.hpp` sets the handshake fields in `EnvParams` from the partner's events and returns the follower's `startRequest`. The master runs in `Europa::process()` before the follower: It sees the follower's events one sample later (about 21 µs at 48 kHz); the follower sees the master's events in the same sample.

**Not adopted**<br>
We do not reproduce hardware quirks of the source: the pulse level decaying with a long decay, the threshold tipping too early due to CMOS loading, the pulse shape (15 V, approx. 150 µs) and the trigger thresholds of the transient circuit. The thresholds here are exact, and events are internal flags. The source's description of the four-way panning patch is contradictory in itself and was not implemented.

<a id="sec3-9"></a>

### 3.9 Antialiasing (minBLEP)
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**Problem**<br>
Hard edges of the envelope (stage with time 0, retrigger jump) contain harmonics far above the Nyquist frequency. If the envelope runs in the audio range (e.g. as a square wave in free cycle) and modulates an audio signal via the VCA, these harmonics fold back into the audible range as aliasing. Antialiasing is optional for this purpose ([2.7](#sec2-7)), default off.

**What the engine reports**<br>
`EnvGen4` reports the **true level jumps** per sample (`jumpCount`, `jumpPos[]`, `jumpDelta[]`): (a) A stage with time 0 (except Delay) reaches its target abruptly; the jump is target level minus start level. (b) The retrigger switch jumps to the delay level at the start. Continuous progressions, even very fast ramps, are not reported. A delay stage with time 0 does not change the level and reports nothing. The engine stays Rack-independent, and its output is bit-identical to the version without reporting (comparison over 8 million samples with random parameters).

**Timing of the jumps**<br>
`jumpPos` lies in (−1, 0] relative to the current sample. For an elapsed zero-time stage end, the position follows from the remaining time (subsample-accurate, also at rate ≠ 1). For gate and trigger events, the moment within the sample is unknown, so −0.5 applies.

**Smoothing**<br>
`Europa` contains one `dsp::MinBlepGenerator<16, 16>` per generator. `Europa::process()` inserts the reported jumps and adds the correction to the engine output (`envOut + correction`). The impulse response is about 32 samples long and causal, so no latency arises. Since minBLEP overshoots slightly, the result is limited to 0–10 V. The engine states (quadrature, gates, EOC) continue to calculate with the unsmoothed value.

**Where it acts**<br>
ENV, VCA (gain) and OR outputs. Not affected: EOC. When switched off, the output corresponds to the previous behavior.

**Phasor mode**<br>
In phasor mode, `PhasorCore` reports the envelope jumps in the same way ([3.10](#sec3-10)): segments with weight 0, reset, offset jumps, direction changes of the input and phase jumps with loose ratio adoption. ENV is smoothed only when the output carries the envelope. A phasor on ENV remains unsmoothed.

**Limits**<br>
- Only **jumps** are smoothed, not the **kinks** at stage transitions (they fall off at 1/f² and are considerably quieter; polyBLAMP has been postponed for now).
- Jumps caused by **modulation** (time or level CV, knob movement in the middle of a stage) are not reported.
- At **very high cycle frequency**, the corrections of neighboring edges overlap, and the effect diminishes.
- Aliasing already present **in the VCA input signal** cannot be removed by Europa.
- In the degenerate case (all times 0 plus loop), the reports are incomplete. This is harmless because the jumps cancel at the same position.

**For measuring**<br>
Feed a sine (not a square) into VCA IN, connect the analyzer to VCA OUT, and compare the switch off versus on. The sidebands at f_sine ± n · f_envelope remain (amplitude modulation, intended), while the folded-back lines decrease significantly. See also [Section 6](#sec6).

**Test**<br>
`tests/test_antialias_spruenge.cpp` (standalone, without Rack) checks: no jumps with a continuous envelope, attack time 0, retrigger jump, release time 0 in Sustain and 300 random runs in which the size and sign of every reported jump match the level change. Functional test in Rack on 2026-10-02: works.

<a id="sec3-10"></a>

### 3.10 Phasor Mode (Signal Processing)
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

The engine of phasor mode is the Rack-independent class `PhasorCore` (`PhasorCore.hpp`, [Section 4](#sec4)). One instance runs per generator. It uses `EnvParams` and the helper functions from `EnvGen4.hpp` (times, curve shapes, levels). The states of `EnvGen4` remain at rest in phasor mode.

**Idea: a mapping, no state of time**<br>
From the input phase `x` (0–10 V, divided by 10), a fixed chain produces the generator's phase `y`, and from `y` the envelope and the phasor outputs. There are no integrators and no time of its own. The output therefore follows the input exactly, even with tempo changes, standstill and reverse playback. The only real state is a cycle counter that depends only on the input's wraps. A clock input with period measurement was deliberately discarded (Section 8).

**Input and direction**<br>
`x = U / 10`, brought to 0..1 (`x − floor(x)`, so that slightly overshooting phasors also work). The direction follows from the step `d = x − xPrev` with hysteresis: `d < −0.5` is a wrap of a rising ramp, `d > 0.5` a wrap of a falling ramp, `|d| > 10⁻⁵` sets the direction according to the sign. Smaller steps (standstill, noise) do not change it. Until the first step, "rising" applies.

**Falling ramp: mirroring at the input**<br>
With a falling ramp, the whole chain works with `xs = 1 − x`. For the generator this is a normal rising ramp: It always runs in the order Delay → Attack → Hold → Release (rise before fall), and with a falling ramp the EOC automatically lies at x = 0. Offset and ratio apply from the generator's point of view in the direction of travel. The phasor outputs are mirrored back at the end (see "Phasor outputs" below) and thus stay direction-true. A direction change makes `xs` jump (`x ↔ 1 − x`). The envelope jumps in the process (reported, see below), the phasor output does not.

**Unwrapped phasor and ratio p/q**<br>
A cycle counter `cycle` (0..719719, i.e. modulo 720720 = lcm(1..16)) counts the input wraps, forward only on the mirrored signal. From this follows
`y = frac(((cycle mod q) + xs) · p / q + offset)`.
p/q is reduced beforehand. The calculation runs in `double` so that `y` stays exact at the cycle boundaries. Examples: 1/2 is one cycle of y over two input cycles, 3/2 three y cycles over two input cycles, 2/1 two y cycles per input cycle. Since `cycle` depends only on the input wraps and is defined via reset and ratio adoption, all generators stay phase-locked to one another.

**Ratio adoption (strict and loose)**<br>
The knob together with the CV delivers the target value (p, q), which is adopted only at an input wrap.
- **Strict (default):** only if `cycle mod q_old == 0` **and** `cycle mod q_new == 0` hold. Then y lies exactly on the cycle start (plus offset) before and after the change, so there is no jump. The price is the waiting time: for 4 → 6 every 12 input cycles, for 7 → 11 every 77. If the CV jumps back before the matching wrap, the change is lost.
- **Loose (context menu, [2.7](#sec2-7)):** immediately at every input wrap. y jumps to the value the global counter dictates for the new ratio. The jump is reported as an envelope jump, and there is no EOC.
- **Reset:** A rising edge on TRIG channel 1 sets the counter to 0 and adopts a pending ratio immediately (y jumps in the process, reported). Without a reset, the power-up moment determines the phase position when dividing.
- The yellow pending LED lights as long as the target value differs from the current ratio.

**Sampling of the ratio CV (rule of thumb)**<br>
The ratio is read only at input wraps (in strict mode only at valid ones). The CV is therefore sampled at the frequency of the input phasor. For a recognizable shape, an LFO needs at least about four sample points per period, i.e. at most a quarter of the input frequency. With a 1 Hz phasor, LFOs above about 0.25 Hz are too fast and behave as if they worked only some of the time. At 8 Hz input, 0.5–2 Hz ran cleanly (test in Rack). Remedy: use a faster input and divide down via q, or use the phasor output of another generator or a sequencer in time as the modulation source.

**CV mapping (all added to the knob)**<br>

| Target | Channel | Mapping |
|---|---|---|
| Weights (time) | TIME CV 1–4 | like time CV ([3.2](#sec3-2)) |
| Offset | TIME CV 5 | 10 V = one full cycle, wraps around as a phase |
| Numerator p | TIME CV 6 | 10 V = full travel 1…16, rounded |
| Denominator q | TIME CV 7 | 10 V = full travel 1…16, rounded |
| Curve | LEVEL CV 5–8 | ±5 V = −1…+1, limited |

p and q are rounded with hysteresis: The value changes only when it deviates by more than 0.6 steps from the last adopted value (0.1 steps of margin each at the rounding boundary). This way a CV sitting on the boundary does not make the pending LED flicker. Small amplitudes below about ±0.4 steps therefore do not move the ratio.

**Segments from the weights**<br>
The resolved stage times (knob, TIME CV and RANGE, as in [3.2](#sec3-2); HOLD CV replaces the hold weight) are weights `w₁…w₄`. Normalized (`pᵢ = wᵢ / Σw`), they divide y into four segments. There is no infinite hold. A weight of 0 omits the segment (the envelope jumps there, reported); if all weights are 0, an even distribution applies (a quarter each). Important: **The RANGE switch scales the weights** (TR1 to TR4 have different upper limits). For equal fractions, the stages must have the same range, otherwise a stage in TR4 weighs a thousand times more than one in TR1.

**Envelope within the segment**<br>
Stage `k` runs from level(k−1) to level(k); the delay segment starts from the release level. The phase within the segment `u = (y − start) / length` is shaped with the stage's curve (as in [3.4](#sec3-4), relative to the direction of movement, i.e. with mirrored sign for downward-running stages), including the curve CV. The output is `start + (target − start) · f(u)`.

**Phasor outputs**<br>
`phasorLinear = y`. `phasorWarped = (k + f(u)) / 4` with the same curve `f` (always rising, without mirroring). It is continuous and monotonic. With unequal weights it corresponds to a warping of time in which each segment occupies a quarter. Both are mirrored back with a falling input (`1 − value`), so that a falling input ramp yields a falling output ramp. The back-mirroring exactly cancels the input mirroring, so a direction change does not make the phasor output jump.

**EOC**<br>
The EOC pulse fires when `y` crosses the cycle boundary (`y` falls by more than 0.5 relative to the previous sample). The pulse length corresponds to [3.6](#sec3-6): minimum of 1 ms and half the length of the last cycle of y, at least 1 sample. With p > q there are several EOCs per input cycle. With unexplained jumps of y (reset, offset jump, direction change, phase jump with loose adoption), no EOC occurs.

**Unexplained jumps and minBLEP**<br>
Per sample, the engine compares `y` with the expected value (`y` of the previous sample plus input movement times the previous ratio). If `y` deviates by more than 2 % of a cycle, the envelope has moved unexpectedly. If the difference between the actual and expected envelope value exceeds 0.05 V, the engine reports a jump (`jumpCount`/`jumpPos`/`jumpDelta`, position within the sample unknown, hence −0.5). `Europa::process()` smooths it with antialiasing enabled, like the jumps of the classic engine ([3.9](#sec3-9)). Continuous changes (knob, slow CV) stay below the threshold and are not reported. Segment jumps caused by weight 0 are reported separately by the engine (level difference, position subsample-accurate from the course of y). With a strict ratio change, `y` stays continuous, so there is nothing to report.

**Mode change**<br>
When the PHASOR switch is toggled, the generator's `EnvGen4` instance and `PhasorCore` instance as well as the time caches are reset. In phasor mode, `EnvGen4` does not calculate, and the quadrature start is not called.

**Limits**<br>
- Per sample, `y` may move by at most 0.5. With a very high ratio `p/q` and a fast input, false wraps would otherwise occur.
- An offset that changes at audio rate and with large amplitude is treated as a series of jumps and not smoothed as modulation.
- A ratio CV between the wraps does not act continuously: A continuous ratio would give up the fixed phase relationship to the input.
- A phasor on ENV (OUT on linear or shaped) is not minBLEP-smoothed.

**Test**<br>
`tests/phasor_core_test.cpp` (standalone, without Rack, built with `-std=c++11`) checks: ratio 1/1, 2/1 and 1/2 (envelope and EOC count), falling ramp (order and direction-true phasor output), strict ratio change, weight 0 with jump report, reset, direction change without false EOC, all weights 0, curve on the shaped phasor (pointed and round, continuous at the segment boundaries), unexplained jumps (reset, offset jump, slow offset sweep without report, direction change) and loose ratio adoption compared with strict.

---

<a id="sec4"></a>

## 4. Function Reference (C++)
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

### `EnvGen4.hpp` (Rack-independent)

**Constants**<br>
`ENV_NUM_STAGES = 4`, `ENV_MAX_TRANSITIONS_PER_SAMPLE = 4`, `ENV_TIME_INFINITE = 1e30f` (finite sentinel instead of INFINITY, because Rack is built with `-funsafe-math-optimizations`), `ENV_TIME_INFINITE_VOLTS = 9.5`, `ENV_TIME_RANGE_MAX = {0.03; 0.3; 3; 30}`, `ENV_TIME_EXP_BASE = 1000`, `ENV_TIME_CV_SPAN_VOLTS = 10`, gate 0.1/1 V, trigger 2 V, `ENV_EOC_MAX_SECONDS = 1e-3`, `ENV_RATE_OCTAVES_PER_VOLT = 1`, `ENV_RATE_CV_LIMIT_V = 5`, `ENV_MAX_JUMPS_PER_SAMPLE = 8`.

**`envTimeFromPosition()` / `envPositionFromTime()`**<br>
Knob ↔ seconds per range.

**`envResolveTime()`**<br>
Knob + time CV → seconds or `ENV_TIME_INFINITE`.

**`envCurvePower()` / `envShapePhase()`**<br>
Curvature exponent and curve application, see [3.4](#sec3-4).

**`envRateFromVolts()`**<br>
Rate CV in volts → speed factor (> 0), see [3.2](#sec3-2).

**`struct EnvParams`**<br>
Values passed per sample, already resolved: `time[4]`, `curve[4]`, `levelDelay`, `levelAttack`, `levelHold`, `levelRelease`, `sustainMode`, `loopEnabled`, `freeCycle`, `retrigZero`, `rate`, `coupled`, plus the handshake fields `handshake`, `hsMaster`, `hsPartnerWaiting`, `hsPartnerRunning`, `hsReleaseEvent`, `hsPartnerCycleEnded` (set by `envQuadSetup()`).

**`struct EnvGen4`**<br>
Outputs `envOut`, `eocOut`, `running`, `held`, `stage`, `attackEnded`, `cycleEnded`, `hsHeld`. Jump report for antialiasing: `jumpCount`, `jumpPos[]`, `jumpDelta[]` (see [3.9](#sec3-9)). Methods `process(dt, params, gateVolts, trigVolts[4], startRequest = false)`, `startCycle()`, `jumpTo()`, `startRelease()`, `finishStage()`, `addJump()`, `reset()`.

**`enum EnvQuadMode` / `envQuadSetup()`**<br>
`ENV_QUAD_OFF = 0`, `ENV_QUAD_FOLLOW = 1`, `ENV_QUAD_HANDSHAKE = 2`. `envQuadSetup(p, quadMode, isFollower, pairSustain, self, partner)` sets the coupling fields of `EnvParams` for a generator of the pair and returns the `startRequest` for `process()` (follower only). The call must occur after setting `loopEnabled`/`freeCycle`. With `pairSustain`, Follow behavior also applies in position 2. The function is testable without Rack; `tests/test_handshake.cpp` uses the same function as `Europa::process()`.

### `Europa.cpp`

**`struct Europa : Module`**<br>
Enums `ParamId`, `InputId`, `OutputId`, `LightId`. `EnvGen4 gens[4]`, `TimeCache`, rate cache (`rateCacheVolts`/`rateCacheValue`), `pendingGen`, `displayedGen`, `antialias`, `minBlep[4]` (`dsp::MinBlepGenerator<16, 16>`), `envAA[4]` (smoothed envelope), `process()`, `dataToJson()`/`dataFromJson()`, `onReset()`.

**`EnvTimeQuantity`**<br>
Display and input of time in seconds (`getDisplayValue`, `setDisplayValue`, `getDisplayValueString`, `setDisplayValueString`), depending on the range of the same stage.

**Widgets**<br>
`EuropaWidget`, `EuropaRangeSwitch`, `EuropaQuadButton`, `GenSelectKnob`, `GenActivateButton`, `EuropaStatusLight`, `EuropaSelectLight`, `EuropaFrame`, `EuropaRowHighlight`, `EuropaOnIndicator`, `EuropaPanelBackground`. Labeling runs via the central `PanelLabel.hpp`. `EuropaWidget::appendContextMenu()` contains the antialiasing switch.

### `PhasorCore.hpp` (Rack-independent)

**Constants and helpers**<br>
`PHASOR_CYCLE_MOD = 720720` (lcm(1..16)), `PHASOR_RATIO_MAX = 16`, `PHASOR_DIR_EPS = 1e-5` (hysteresis of the direction detection), `PHASOR_MAX_JUMPS = 2`. `enum PhasorOutMode` (`PHASOR_OUT_ENV = 0`, `PHASOR_OUT_LINEAR = 1`, `PHASOR_OUT_WARPED = 2`). `phasorGcd()` is a custom gcd, because Rack is built with C++11 and `std::gcd` only exists from C++17.

**`struct PhasorCore`**<br>
Outputs `envOut`, `phasorLinear`, `phasorWarped` (0..1, direction-true), `eocOut`, `running`, `pending`, plus the jump report `jumpCount`, `jumpPos[]`, `jumpDelta[]` (as with `EnvGen4`, [3.9](#sec3-9)). Methods `process(dt, EnvParams, ratioP, ratioQ, offset, phasorVolts, resetVolts, looseRatio = false)`, `envAt()` (envelope at a cycle phase, also for the jump check), `addJump()`, `reset()`. State: cycle counter, current ratio, direction, previous values, EOC pulse.

### `Europa.cpp`, additions for phasor mode

`PHASOR_PARAM`, `PHASOR_OUT_PARAM`, `RATIO_P_PARAM`, `RATIO_Q_PARAM`, `PHASE_OFFSET_PARAM` (four each, at the end of the enum), `PENDING_LIGHT` (four), members `phasor[4]`, `prevPhasorMode`, `phasorInVolts` (for the pass-through), `ratioPending`, `ratioPLast`/`ratioQLast` (hysteresis of the p/q rounding) and `ratioLoose`. `EuropaQuadButton` has an adjustable `historyName` (undo name), which the phasor OUT button uses.

---

<a id="sec5"></a>

## 5. Persistence
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**Params**<br>
All settings of all four generators are real Rack params: 98 in total (4 × 24: 4 levels each, 4 × time, 4 × range, 4 × curve, mode, retrigger, loop and the five phasor params Phasor, Phasor-Out, p, q, Offset; plus two three-position quadrature buttons (0..2), one per pair). `LOOP_PARAM` has three positions (0..2). Rack saves all params in the patch.

**JSON**<br>
`pendingGen` and `displayedGen` (with clamp to 0–3), plus `antialias` and `ratioLoose` (both bool). If an entry is missing in an old patch, antialiasing stays off and ratio adoption stays strict.

**Enum rule**<br>
New entries are appended only at the end so that the cable indices of existing patches stay stable. This is how the delay and release level params and `LEVELCV_INPUT` were appended later. Old patches load them with default 0 V and sound unchanged. `QUAD_PARAM` and `OR_OUTPUT` were appended in the same way. Old patches load the coupling switched off. `QUAD_PARAM` received its third position on 2026-10-05 (value range 0..2 instead of 0..1); 0 (Off) and 1 (Follow) keep their meaning. The former light group `QUAD_LIGHT` was dropped: The button draws its LED itself from the param (lights are not saved in the patch, so there are no compatibility consequences). The values 0 (Off) and 1 (Gate) of `LOOP_PARAM` keep their meaning; the new value 2 stands for the free cycle.

Phasor mode (2026-10-06) appends `PHASOR_PARAM`, `PHASOR_OUT_PARAM`, `RATIO_P_PARAM`, `RATIO_Q_PARAM`, `PHASE_OFFSET_PARAM` (four each) and `PENDING_LIGHT` at the end. Old patches load all generators in classic mode and sound unchanged. One exception: a poly cable with more than four channels on LEVEL CV, whose channels 5–8 now act as curve CV.

**Reset**<br>
`onReset()` resets all engines (including the `PhasorCore` instances) and the generator display.

---

<a id="sec6"></a>

## 6. Known Pitfalls and Open Items
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**Delay level acts at delay time 0 only with RETRIG D**<br>
A delay stage with time 0 does not change the level (click-free retrigger). The delay level is then ineffective, unless the retrigger switch is on and jumps to the delay level at the start. Otherwise Attack glides off from the current level.

**Rest level above 0 V opens the VCA**<br>
A release level above 0 V lets the VCA signal show through at rest. For pure VCA operation, leave the release level at 0 V (default).

**Mono cable on TRIG, TIME CV and LEVEL CV**<br>
TRIG: Channel 1 is unused, and a mono cable does nothing. TIME CV and LEVEL CV: A mono cable affects only Delay. Use a poly cable for all other stages.

**HOLD CV acts only in One-Shot/Loop**<br>
In Sustain mode HOLD CV remains ineffective; there the gate determines the hold duration as before. A patched cable makes the TIME knob of the hold stage completely ineffective in One-Shot. This is intentional and not a malfunction.

**Triggers in Sustain mode**<br>
Jump triggers act only with the gate high; otherwise they are ignored.

**Loop in Sustain mode**<br>
The LOOP switch (including position FREE) has no effect in Sustain, nor does EOC → T1/GATE.

**Rate CV needs a cable with at least five channels**<br>
A mono cable lands on channel 1 and acts there as delay time CV, not as rate. Channel 5 is reached via a merge module or a poly splitter; channels 1–4 may stay empty. The range is generous at ±5 V and one octave per volt: A sine with ±5 V swings between 1/32 and ×32, and a unipolar LFO (0–10 V) sits permanently at the limit (×32) from 5 V.

**Quadrature (Follow): loop of both generators OFF for single passes**<br>
If the LOOP switch of generator 1 is on GATE or FREE, it restarts by itself after Release, and each attack end restarts generator 2. On the scope it then looks as if generator 1 were waiting for generator 2. For a single pass, set the loop of both generators to OFF. A delay time on generator 2 shifts its start in addition to the attack end of generator 1.

**Handshake: hold knob without effect**<br>
In handshake, waiting for the partner replaces the hold stage. Hold time, hold level and HOLD CV do nothing there. This is intentional and not a malfunction.

**Handshake: waiting can take long**<br>
Each generator waits at its attack end for the partner. If the follower's attack is infinite (time CV ≥ 9.5 V), the master waits correspondingly long. This matches the original. Switching off the handshake ends the hold immediately.

**Handshake: loop useful only on the master for the four-step cycle**<br>
In handshake the master has no internal loop. It restarts only after the follower's cycle end, and only with LOOP = GATE and the gate held, or with LOOP = FREE. LOOP of the follower on GATE or FREE, by contrast, turns it into a free-running oscillator (see [3.8](#sec3-8)).

**Handshake only in One-Shot, one sample of latency**<br>
If one of the two generators is in Sustain, Follow applies. The master sees the follower's events one sample later (about 21 µs at 48 kHz). With envelopes this is inaudible, but not exactly zero.

**Saved patches and panel width**<br>
The panel has grown from 20 to 23 HP. Modules that sit directly to the right of Europa in a saved patch may overlap after loading and must then be moved.

**Antialiasing smooths only jumps**<br>
The switch ([2.7](#sec2-7)) acts only on the reported level jumps of the envelope. Kinks at stage transitions, modulation jumps and aliasing in the VCA input signal remain untouched. At very high cycle frequency (the impulse response is about 32 samples long), the effect diminishes. A sine at the VCA is suitable for judging: A square at VCA IN, together with the square of the envelope, produces a product of two broadband signals in which the envelope's contribution is hard to recognize.

**Attack, Release and Delay all 0 gives no square wave**<br>
With time 0 in Delay, Attack and Release, the envelope in free cycle stays permanently at the hold level, and the valleys have zero width. The VCA then has a constant gain of 1 and passes the signal through unchanged, so there is nothing to smooth. A square wave arises only with a **delay time greater than 0** (at delay level 0 V). The period is then Delay plus Hold.

**Audio rate**<br>
A patch loop has 1 sample of latency due to the cable. At 48 kHz, recognizable trapezoid shapes are possible up to approx. 5–6 kHz (about 8 samples per cycle), theoretically 24 kHz (pulse train only). The latency audibly detunes at high frequencies (about 2 % at 1 kHz, about 10 % at 5 kHz). There is no oversampling. Hard envelope jumps (zero-time stages, retrigger jump) can be smoothed with minBLEP in the context menu ([2.7](#sec2-7), [3.9](#sec3-9)); without the switch they alias. The internal loop has no latency.

**Automation and MIDI map**<br>
Since all 98 params are real, MIDI map and automation work on every generator. However, only the displayed generator is ever visible.

**Phasor mode: the ratio CV is read only once per input cycle**<br>
The CV on TIME CV channels 6 and 7 is sampled at the input wraps (see rule of thumb in [3.10](#sec3-10)). An LFO above about a quarter of the input frequency acts irregularly or stands still. With a 1 Hz phasor, even LFOs from 0.25 Hz are too fast.

**Phasor mode: strict adoption can lose CV changes**<br>
Strict adoption applies only at wraps where the counter matches the old and new q. If the CV jumps back before a matching wrap, the change is discarded. For CV-controlled ratios (sequencer, LFO), choose the loose mode in the context menu ([2.7](#sec2-7)).

**Phasor mode: the same CV on numerator and denominator cancels out**<br>
If the same square wave sits on TIME CV channels 6 and 7, p and q change simultaneously (16/16 and 1/1), the ratio stays 1/1, and nothing happens. Negative CV makes both numbers smaller: On the numerator this slows the generator (8 → 4: 1/2), on the denominator it speeds it up (8 → 4: 2/1). This is the mathematics of the ratio, not an error.

**Phasor mode: channels 5–7 on the TIME CV input need correspondingly many channels**<br>
Offset, p and q are reachable only if the cable has at least 5, 6 or 7 channels respectively. A mono cable lands on channel 1 and acts there as the weight of the delay stage.

**Phasor mode: RANGE scales the weights**<br>
In phasor mode the stage times are weights, and TR1…TR4 have different upper limits. For equally long segments, set all four stages to the same RANGE.

**Phasor mode: reset, offset jump and direction change make the envelope jump**<br>
These events move `y` unexpectedly. The envelope jumps (audible, smoothed with antialiasing), and there is no EOC. The phasor output is not smoothed. With loose ratio adoption, `y` jumps at every change.

**Phasor mode: quadrature not intended**<br>
A generator in phasor mode ignores the quadrature coupling. Do not operate the partner of a phasor generator in Follow or Handshake mode (not intended and untested).

**LEVEL CV channels 5–8 act as curve CV, in both modes**<br>
A poly cable with more than four channels on LEVEL CV now changes the curve of the stages. Old patches with such a cable may sound different.

**Open items**<br>
A comprehensive DSP test file for the basic functions of `EnvGen4` is missing (standalone, see [Section 8](#sec8)). Dedicated standalone tests for the free cycle, the rate CV, the quadrature coupling and the jump report of the antialiasing are in `tests/` (`test_autocycle.cpp`, `test_ratecv.cpp`, `test_quadratur.cpp`, `test_handshake.cpp`, `test_antialias_spruenge.cpp`). **The handshake has been verified in the standalone test (`tests/test_handshake.cpp`, eleven cases); the functional test in Rack is still pending.** Likewise untested in Rack is the new button widget `EuropaQuadButton` (click, Shift+click, undo, LED). polyBLAMP for the kinks at stage transitions is postponed for now. The panel image as SVG is still missing. **Phasor mode:** `tests/phasor_core_test.cpp` (standalone) covers the engine. Confirmed in Rack (2026-10-06): classic mode unchanged, phasor modes and outputs, panel layout, the CV on p and q with constant voltage, and strict and loose ratio adoption (LFO test see [3.10](#sec3-10)). **Open in Rack:** the curve CV (LEVEL CV channels 5–8), the offset CV (TIME CV channel 5), the pass-through of the phasor between generators, the reset on TRIG channel 1 and the minBLEP reporting of the phasor jumps.

---

<a id="sec7"></a>

## 7. Patch Ideas
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

**LFO with offset**<br>
Loop on, delay level and release level set to the same value (e.g. 3 V), attack level e.g. 7 V: The loop runs between two freely selectable values. Amplitude and offset are adjustable separately.

**Ducking**<br>
Delay level 10 V, attack level 0 V: The envelope ducks instead of rising.

**Cascade**<br>
EOC of one generator to TRIG channel 2 of another: The second starts directly in Attack as soon as the first is done. EOC → T1/GATE of the same generator gives the patch loop.

**Skipping a stage**<br>
A trigger on TRIG channel 3 jumps directly to the hold level, without a rise.

**External release**<br>
A trigger on TRIG channel 4 ends the envelope at any moment, e.g. via a clock pulse.

**LFO with exponential frequency control**<br>
LOOP on FREE, a poly cable with five channels on the TIME CV input, the rate CV on channel 5: The generator runs without a gate as an LFO whose frequency follows at one octave per volt. An LFO or sequencer on channel 5 modulates the speed while the curve shape remains unchanged.

**Two-stage envelope (quadrature + OR)**<br>
Button of pair 1+2 on, gate on T1/GATE of generator 1, loop of both generators OFF. Generator 2 starts as soon as generator 1 has reached its attack level. The OR output 1+2 delivers the composite curve. A delay time on generator 2 sets the offset.

**Quarter-period LFO (handshake)**<br>
Button of pair 1+2 on Handshake (LED amber), LOOP of generator 1 on FREE, LOOP of generator 2 on OFF, attack and release of both generators equally long. ENV 1 and ENV 2 deliver two trapezoidal control voltages with 90° phase offset, e.g. for panning or counter-phase modulation. The period is the sum of all four times, and unequal times deform the trapezoids. With equal times, the OR output 1+2 delivers a trapezoid with a plateau twice as wide.

**ADSR shape from two generators (OR)**<br>
Generator 1 in One-Shot with attack level 10 V and short release, generator 2 in Sustain with attack level 5 V, both with the same gate. The OR rises to 10 V, falls back to 5 V, holds 5 V and fades out at gate end.

**Wavefolder-like effects**<br>
An LFO on LEVEL CV (channels 2–3) swinging beyond the 0–10 V limits is clipped and folds back at the edge. With LFO frequencies and time ranges whose stage times lie in the range of the LFO period, and round or pointed curves, wavefolder-like progressions arise. Add concrete settings (LFO frequency, time range, curve position) here.

**Phasor sequencer**<br>
Phasor source on T1/GATE of generator 1, PHASOR on, ratio 1/4: one envelope cycle over four input cycles. The EOC delivers a clock for the downstream sequencer. With the weights you determine how long Delay, Attack, Hold and Release are within the cycle, without anything shifting.

**Polyrhythmic envelopes**<br>
One phasor cable on generator 1, all four generators in phasor mode (the pass-through feeds 2–4), ratios 1/1, 3/2, 4/3, 5/4: four synchronous envelopes with different cycle durations that coincide again at the reset.

**Shaped phasor as clock source**<br>
OUT of generator 1 on "Phasor shaped", ENV of generator 1 to T1/GATE of generator 2: The second generator receives a phasor with unevenly running time (swing, ritardando, accelerando), set via the weights and curves of generator 1.

**Ratio via sequencer**<br>
Stepped CV on TIME CV channels 6 and 7, loose ratio adoption, input phasor fast enough (for example 8 Hz, divided down with q): The ratio follows the sequencer in every input cycle. An LFO should be at most a quarter of the input frequency.

**Falling ramp as reverse playback**<br>
A falling phasor on T1/GATE: The envelope still begins with Attack, and the phasor output falls along with it. This translates a backward-running clock into a normal envelope progression.

**Curve via CV**<br>
An LFO or sequencer on LEVEL CV channels 5–8 reshapes the envelope or the shaped phasor while running: Round and pointed alternate in time, in both modes.

---

<a id="sec8"></a>

## 8. Design Decisions & Boundaries
*[↑ Back to contents](#toc) · [[#Contents|Obsidian]]*

- **Engine option B:** a dedicated lean class `EnvGen4`, with `Afg`/Sofonisba left untouched. `Afg` is designed for 16 stages and the shared `stageModifier[]` array; generalizing it would have meant a regression risk in the feature-complete Sofonisba.
- **Rack-independent engine:** `EnvGen4.hpp` receives already resolved values (seconds, volts) and returns volts. This makes it testable standalone (DSP test file planned).
- **Time ranges:** All four ranges start at 0 (exactly 0 at the far left), the upper limits match Sofonisba (30 ms / 300 ms / 3 s / 30 s), and the knob mapping is exponential. The lower limits from Sofonisba are dropped.
- **Generator switching following the Lucia pattern:** All params (now 98) are real Rack params, and the GUI shows only the selected generator. Rejected: the edit-param model with pickup/catch provided in the concept. Consequence: Copying and pickup/catch are not needed, and MIDI map and automation act on every generator.
- **`pendingGen`/`displayedGen` not a param:** Undo, automation and preset recall should not affect pure GUI navigation (same decision as with Lucia).
- **Range as a dedicated click switch:** Notched standard knobs with only 4 positions could not be moved in Rack (Rack's knob scales the drag distance to the value range). Rule of thumb from Sofonisba: For a small discrete number of states, build a dedicated widget.
- **Time value input in seconds:** Without overrides of `getDisplayValue()`/`setDisplayValue()`, an entered number ended up as the knob position and was clamped to 0/end stop (same class of error as with Sofonisba/tempo).
- **Dedicated bipolar curve mapping, relative to the direction of movement:** instead of Sofonisba's exp/log bits. Reason: With falling stages, the direction-independent curve yielded a pointed exponential decay instead of the expected bell shape. Left = round (flat at top), right = pointed. Tooltip "curve (- round / + pointed)".
- **Delay and release level adjustable, unipolar 0–10 V:** previously fixed at 0 V. This enables LFOs with offset, inverted envelopes and rest levels. Rejected: bipolar, because the VCA function would then receive negative gains.
- **Retrigger switch jumps to the delay level, renamed "RETRIG D":** previously a jump to 0 V ("Retrig 0"). The internal enum name stays.
- **LEVEL CV as a dedicated jack, option A:** poly, channels 1–4 = Delay/Attack/Hold/Release, additive, sum limited to 0–10 V. Rejected: channels 5–8 on the TIME CV input (no space required, but less clear). Consequence: The jack block initially had 8 columns instead of 7.
- **HOLD CV as a dedicated, replacing jack instead of TIME CV channel 3:** discovered when comparing with older patches, on the original via CV 0–10 V without a gate relation. A dedicated jack rather than an extension of the existing channel, so that the knob becomes deliberately ineffective when patching (clear separation "knob- vs. CV-controlled") instead of an additive mix. The mapping continues via TR1–TR4 (exponential, not linear), consistent with all other time inputs in the module. The scope is limited to One-Shot/Loop: In Sustain the gate already determines the hold duration unambiguously, and a CV override there would have diluted the concept of Sustain. Consequence: The jack block has 9 columns.
- **TRIG as a strobe with fixed targets:** comparable to Sofonisba's strobe (immediate jump), but without an address: Each poly channel has its fixed stage (2–4).
- **Status LED with independent color channels and normal blending:** Rack's RGB mixing did not allow clean colors; in addition, the standard blend mode fades on the light panel.
- **No oversampling:** The main use is LFO operation, and the audio range is an extra. Oversampling would multiply the CPU load by the factor (four generators, polyphonic VCA) and was rejected.
- **Optional antialiasing only for Europa, minBLEP only for jumps:** An overview of all modules in the plugin shows: Europa is the only one with a meaningful audio-rate case, because the envelope's jumps (zero-time stages, retrigger) act directly on audio via the VCA. The other modules are CV and logic tools (gates and triggers must not be filtered) or produce at most mild kinks. Implementation: The engine reports true jumps and remains Rack-independent and bit-identical in its output. minBLEP sits in the module and corrects only the output, never the engine states (otherwise the envelope would be corrupted). The result is limited to 0–10 V (overshoot). Default is **off**, because minBLEP turns a hard jump into a short, slightly overshooting edge and this changes the character of sharp envelopes. Old patches sound unchanged. Rejected: polyBLAMP for the kinks (small gain, 1/f² decay, postponed for now) and a Rack param instead of the context menu. Standalone test `tests/test_antialias_spruenge.cpp`, functional test in Rack on 2026-10-02: works (clean at Delay 0, with Delay 1 ms and the switch on significantly fewer harmonics in the spectrum).
- **Free cycle as the third position of the LOOP switch:** instead of a dedicated switch or additional param. Reasons: There is no room in the header area for another switch, no new enum entries are needed, and the values 0 and 1 keep their meaning. FREE acts only in One-Shot and has no effect in Sustain. The retrigger behavior deliberately stays unchanged; a gate in free cycle retriggers as usual. Inspired by the analysis of a public community thread on emulating envelope generators from the Buchla world, in which a cycle mode without a gate is described. The reference documentation is only a source of ideas; the implementation is independent. Standalone test `tests/test_autocycle.cpp` and test in Rack.
- **Rate CV on channel 5 of the TIME CV input:** bipolar, exponential and shape-preserving. Instead of a dedicated jack, it sits on channel 5 of the existing polyphonic input: The panel already had nine jack columns, channel patterns already exist with LEVEL CV and TRIG, and saved patches remain valid. The range is one octave per volt, limited to ±5 V (suggestion adopted, both values adjustable as constants). A global switch for unipolar CVs was not implemented. Standalone test `tests/test_ratecv.cpp` (factor mapping and limiting, duration ∝ 1/rate, identical curve shape, continuous output on rate change, EOC count in free cycle, infinite hold, all times 0 at ×32), functional test in Rack on 2026-10-01: works ("highly interesting modulation").
- **OR outputs:** two outputs, the maximum of the ENV levels per pair (1+2, 3+4) before the VCA. A level control is omitted because each stage already has its own level. A dedicated column sits on the right, and the panel grows from 20 to 23 HP (22 HP was too narrow, 24 HP too wide). It is designed like the other jack columns (dark plate 10 mm wide, narrow light frame; `orPlateW`/`orFramePad` in `Europa.cpp`). Rejected: a tenth column in the existing grid (third narrowing, labeling too tight) and a context menu instead of jacks (that would have cost the individual envelope outputs of generators 2 and 4).
- **Quadrature coupling:** switchable per pair, generator 2 (or 4) starts at the **end of Attack** of generator 1 (or 3). Rejected as start events: beginning of Release and cycle end (EOC). The exact behavior of the hardware cannot be clearly derived from the reference documentation, so the choice is our own determination. The switch is a latching LED button in the OR column. The follower's own T1/GATE is ignored, and the triggers for stages 2–4 remain active. In Sustain mode the follower follows the partner's gate, and with coupling active FREE behaves like GATE (suggestion adopted). Standalone test `tests/test_quadratur.cpp`, functional test in Rack on 2026-10-01: works as built. An initially apparently swapped start was due to an enabled loop switch, hence the note in Section 6. The switch has had three positions since 2026-10-05 (next entry); the determination described here continues to apply unchanged as position "Follow".
- **Handshake as the third position of the quadrature button:** The occasion was a public description of the quadrature mode. It showed that quadrature is a handshake of four events (attack end A → B starts, maximum B → A falls, minimum A → B falls, end B → A restarts), while Follow reproduced only the first step. Decision for a **third position** rather than replacement, so that Follow and saved patches (0/1) are preserved. Further determinations: The hold stage is omitted in handshake (waiting replaces it). The master has no internal loop and restarts via the follower's cycle end (corresponds to the "Pulse B → Trigger A" cable). A looping follower runs freely (cycle case of the source) and holds only when the master is waiting. In Sustain, Follow applies automatically. An external retrigger of the master restarts a holding follower (otherwise mutual blocking, found in testing). The hold ends when the handshake is switched off. The button is a dedicated click widget (rule of thumb: small discrete number of states), the LED is drawn from the param, and `QUAD_LIGHT` is dropped. The pulse and threshold quirks of the hardware are not reproduced, and the contradictory four-way panning description of the source was not implemented. Standalone test `tests/test_handshake.cpp` (eleven cases: four-step cycle, period and quarter phase, unequal times with delay, hold level, free-running follower with and without master loop, sustain fallback, switching off while holding, retrigger, all times 0, Follow unchanged) passed. `tests/test_quadratur.cpp` continues to run unchanged. The functional test in Rack is still pending.
- **Phasor mode:** The occasion was the idea of producing a phasor on the ENV output that always runs in sync with an external clock or phasor (modulation locked in time, phasor-based sequencer). Decisions:
  - **Phasor input instead of clock-to-phasor:** A period measurement between clock edges is error-prone because of the measurement time; a phasor input makes the module stateless with respect to time. Rejected: clock → phasor with PLL.
  - **Four independent phasors synchronous to the input (variant B):** Each generator runs over the whole input cycle with its own ratio, instead of chaining the four generators as segments of one cycle (variant A, sequencer).
  - **Mode selection per generator via button with param:** T1/GATE remains the trigger in the normal case, and the phasor input uses the same jack. Automatic detection of trigger, gate and phasor was rejected because the signals cannot be reliably distinguished: A standing phasor looks like a gate, a slow ramp like a gate with an edge, a fast phasor like a trigger train, and a wrong guess would switch the mode in the middle of the cycle. `isConnected()` only reveals that a cable is plugged in.
  - **Stage times as weights:** Delay, Attack, Hold and Release divide the cycle, instead of a rise/fall breakpoint. This fits Europa's four stages, and levels and curve remain unchanged. Weight 0 omits a segment, and all 0 give an even distribution.
  - **Ratio p/q with free knobs (1..16)**, not as a fixed list. Unwrapped phasor with cycle counter modulo 720720 (lcm 1..16), p/q reduced, and a reset input (TRIG channel 1, previously unused in Europa) fixes the phase position.
  - **Ratio adoption at the input wrap, strict as default:** y stays continuous, and the price is the waiting time (for example every 12 cycles for 4 → 6). On request (CV modulation) there is additionally the **loose** adoption as a module-wide context menu item (`ratioLoose`): immediate adoption with a phase jump, smoothed with minBLEP. The occasion was Rack tests with square-wave CV, in which strict adoption lost changes.
  - **Direction:** Hysteresis in wrap detection. With a falling ramp the input is mirrored so that rise always runs before fall and the EOC lies at x = 0. The phasor output is mirrored back and stays direction-true. A switch "Mirror on falling ramp" is dropped.
  - **ENV as phasor output instead of new jacks:** three positions (envelope, phasor linear, phasor shaped). The envelope level remains reachable via VCA OUT and OR because VCA IN is normalled to 10 V. Rejected: new jacks (the panel had no free column) and a menu item instead of a param.
  - **Phasor shaped = (k + curve(u)) / 4:** The curve controls thus also act on the phasor output, and the linear phasor remains the clean reference.
  - **CV assignment without new jacks:** Curve on LEVEL CV channels 5–8 (applies in both modes), offset, p and q on TIME CV channels 5–7 in phasor mode (channel 5 is no longer the rate CV there). Scaling matches the time CV: additive, 10 V = full knob travel (curve ±5 V). p and q are rounded with 0.1 steps of hysteresis.
  - **Pass-through of the phasor:** Without a cable on T1/GATE, a generator in phasor mode takes the phasor of the previous generator, because T1/GATE is mono and present per generator (one cable on generator 1 feeds all four).
  - **Unexplained jumps are reported to the minBLEP:** reset, offset jump, direction change and phase jump with loose adoption. No EOC occurs on these events.
  - **Panel:** PHASOR strip above the OR column in the previously empty area, 23 HP remain, five params per generator at the end of the enum. Rejected: widening the panel. The button widget of the quadrature button is reused (adjustable undo name).
  - **Insight from the Rack tests:** The ratio CV is sampled only at the frequency of the input phasor (rule of thumb: LFO at most a quarter). Additional read points (ratio at every y wrap, 1–2 ms settling window after the wrap) were considered and deliberately not implemented. A faster input phasor with division via q solves the problem, and the behavior would otherwise be harder to predict.
  - **Build:** Rack builds with C++11, hence a custom gcd (`phasorGcd()`) instead of `std::gcd` (build error in the first Rack test). Standalone test `tests/phasor_core_test.cpp`. Functional test in Rack (scope see [Section 6](#sec6)).
- **Documentation last:** first all features, then the documentation, including the concept document update. Europa has its own documentation file because the plugin has five modules and Europa has technically nothing in common with the Sofonisba core documentation.
