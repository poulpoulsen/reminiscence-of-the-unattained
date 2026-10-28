# Elena – Technical Documentation

Seventh module of the **Kinovice reminiscence of the unattained** plugin: a
VCV Rack module heavily inspired by the wonderful Buchla world of modules.
Two transformation sections (A/B, in the style of a digital transfer-curve
processor) and six slew limiters (C–H) with pulse outputs. Sections A/B
are polyphonic with individual values per channel; the slew sections process
polyphonic signals with shared controls per pair.

**Notice:** The *Kinovice reminiscence of the unattained* plugin is an independent work, heavily inspired by the wonderful Buchla world of modules. It is not affiliated with, authorized by, or endorsed by Buchla, Tiptop Audio, or any other manufacturer. Product and company names mentioned are trademarks of their respective owners and are used only to describe which functional concepts inspired a module.

**Distinction:** **Minerva** (analog voltage computation with attenuverter, crossfader and offset) and Elena (digital transfer-curve transformation with slew limiting) are two independent modules with different functions. That one was meant as a further development of the other is stated in none of the sources used; the assignment follows only from the descriptions themselves. See `Minerva-Documentation_EN.md`.

References: reference documentation. It describes sections A/B only briefly as similar to an older section; the formulas therefore come from the description of that older section.

Test status (Kinovice, live in a running Rack instance): sections A/B work
(channel selection with ACTIVATE, channel values persist across save and
load, polyphonic A1/A2 with a monophonic SEL CV, curve display and
breakpoint match the calculated values). The slew sections C–H work (each
slewer acts only on the signal at its own IN) and were measured (1 Hz
square wave, POS and NEG 300 ms each): in Time mode a ±5 V square wave
yields a trapezoid, a ±10 V square wave a triangle from −6.5 to +9.98 V
(constant slope 10 V/300 ms, the swing is limited by the half period). In
Rate mode a ±10 V square wave yields ±9.86 V (theory 9.87 V) and a zero
crossing after about 0.07 s (theory 0.069 s), the same for rising and
falling edges. The assignment of Rate and Time remains an interpretation of
the reference documentation, see section 8. The patch examples marked "calculated" in
section 7 have not been tested in Rack. The range selection per section (section 2) and the DC blocker are new, checked by calculation, and not yet tested in Rack.

---

<a id="toc"></a>

## Table of Contents

1. [Overview](#sec1) · [[#1. Overview|Obsidian]]
2. [Sections A/B: transformation](#sec2) · [[#2. Sections A/B: transformation|Obsidian]]
3. [Sections C–H: slew and auto-pulse](#sec3) · [[#3. Sections C–H: slew and auto-pulse|Obsidian]]
4. [Polyphony and channel selection](#sec4) · [[#4. Polyphony and channel selection|Obsidian]]
5. [Panel layout](#sec5) · [[#5. Panel layout|Obsidian]]
6. [Internal processing (C++)](#sec6) · [[#6. Internal processing (C++)|Obsidian]]
7. [Patch examples](#sec7) · [[#7. Patch examples|Obsidian]]
8. [Sources, assumptions, and open points](#sec8) · [[#8. Sources, assumptions, and open points|Obsidian]]
9. [Design decisions](#sec9) · [[#9. Design decisions|Obsidian]]

---

<a id="sec1"></a>

## 1. Overview
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Module** *(`Elena.cpp`, `CvProcElena.hpp`, `modelElena`)*<br>
42HP. On the left two identical transformation sections A and B, on the right three slew blocks with two slewers each (C/D, E/F, G/H).

**Sections A/B**<br>
Choice between two inputs (A1/A2) by a select knob or select CV, followed by a curve defined by a 0 V point, a 10 V point, and an optional breakpoint. All knobs are stored separately per channel.

**Sections C–H**<br>
Six independent slew limiters. Each has its own CV input, slewed output, and pulse output. The slew parameters (positive/negative slew, time/rate, auto-pulse) are shared in pairs and apply to all channels equally.

**Important:** A slewer only changes the signal at its own IN. Without an input signal the output stays at 0 V, and the knobs have no effect.

---

<a id="sec2"></a>

## 2. Sections A/B: transformation
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

All voltages are normalized to 1 (10 V = 1). Per section and channel:

```
Vin  = InSel·A2 + (1 − InSel)·A1                    (limited to 0…1)
without breakpoint:  Vout = Vin·V10 + (1 − Vin)·V0
with breakpoint:     Vin < bpx:  Vout = Vin·bpy/bpx + (1 − Vin/bpx)·V0
                     Vin > bpx:  Vout = ((Vin − bpx)·V10 + (1 − Vin)·bpy) / (1 − bpx)
```

**Input selection (SELECT, A1, A2, SEL CV)**<br>
The select knob crossfades between A1 (0) and A2 (1). If SEL CV is patched, it **replaces** the knob (0–10 V = 0…1). With A1 unpatched (0 V) the input stage simply multiplies InSel with A2, as described in the reference documentation; the section is therefore also a multiplier.

**Curve (0 V, 10 V)**<br>
"0 V" sets the output voltage at 0 V input, "10 V" the output voltage at 10 V input; in between it is interpolated linearly. If "10 V" lies below "0 V", the section inverts (within 0–10 V). Default: 0 V → 0 V, 10 V → 10 V, i.e., pass-through.

**Breakpoint (BP, BP IN, BP OUT)**<br>
The BP switch activates a knee point: BP IN sets the input voltage, BP OUT the output voltage there. The curve then consists of two straight lines through (0, V0), (bpx, bpy), and (1, V10). Without BP the two knobs have no effect. The BP IN value is internally limited to 0.001…0.999 to avoid division by zero.

**Curve display**<br>
A 34×20 mm graphic replaces the original's LED display: it shows the curve of the shown channel (green, breakpoint as a dot) and the current operating point input → output (amber).

**Range**<br>
By default the sections work unipolar in the range 0–10 V; bipolar input signals are cut off at Vin (negative parts become 0). In the module context menu ("Input range Section A/B") a different range can be chosen per section:

**Input range (context menu, per section)**<br>
**0 to 10 V** (default): as above, Vin hard-limited. **+/-5 V** and **+/-10 V**: the range is mapped to 0…1 (`(v + 5)/10` or `(v + 10)/20`) and applies to A1, A2, SEL CV **and** to the output (range → range): with +/-5 V, ±5 V go in and ±5 V come out. The curve runs unchanged; the knobs "0 V", "10 V", BP IN, and BP OUT keep their positions (0…1), their tooltips show the voltage in the selected range (e.g., −5…+5 V). With the default values the section is a pass-through in every range. **Unlimited (0-10 V scale)**: scale 0–10 V, but without limiting Vin; the curve continues linearly beyond the endpoints. With the default values a bipolar signal therefore passes through unchanged.

**Notes on the range**<br>
SEL CV follows the range but is always limited to 0…1 (crossfade weight). Unpatched inputs sit at 0 V; in the bipolar ranges 0 V corresponds to the center of the range (0.5 normalized). An unpatched A1 therefore does not act as 0 in the calculation `InSel·A2` there, but as the midpoint. The range selection does not change knob values, only their meaning in volts.

**DC blocker (context menu, per section)**<br>
In the module context menu a DC blocker can be switched on individually for the output of each A/B section ("DC blocker Section A/B"). It acts on the finished output (after the curve and the range back-calculation), with its own state per channel. It is a one-pole high-pass at about 10 Hz (details in section 6). A typical source of a DC component is half-wave rectification of bipolar audio in the 0–10 V range (the negative half becomes 0), or a "0 V" knob or breakpoint that shifts the signal. Since the blocker also removes slow CV components below about 10 Hz, it is off by default.

**Knob ranges and defaults**<br>
SELECT 0…100 % (default 0 = A1), 0 V and 10 V each 0…10 V (default 0 V and 10 V), BP IN and BP OUT each 0…10 V (default 5 V), BP off.

---

<a id="sec3"></a>

## 3. Sections C–H: slew and auto-pulse
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Structure**<br>
Six slewers C to H, each with `C IN` … `H IN`, `C OUT` … `H OUT`, `C PLS` … `H PLS`. The slew controls are shared per pair: **C/D**, **E/F**, **G/H**. Two LEDs per slewer: red (pulse) and blue (output level).

**POS and NEG**<br>
POS determines the speed of rising, NEG that of falling edges. Range 0.01…10 s, exponential (`t = 0.01 s · 1000^position`), displayed in ms or s, entered via right-click in ms or s. The time never reaches 0, so, as in the original, a true square or sawtooth never occurs.

**Slew CV (POS CV, NEG CV)**<br>
Adds to the knob position (0–10 V corresponds to the full knob travel) and is limited at the top: with the knob fully right the CV has no effect; it has the most effect with the knob fully left. The CV is polyphonic (per channel).

**TIME/RATE**<br>
The modes behave differently (assignment see section 8):

**Time**<br>
Linear slope at constant speed `10 V / t`. The knob gives the seconds for a swing from 0 to 10 V. A smaller jump takes correspondingly less time.

**Rate**<br>
Exponential catch-up, the speed is proportional to the distance between input and output. The knob gives the time to about 95 % of a jump (time constant `t/3`).

**AUTO (auto-pulse)**<br>
Per pair a button with text display (red when active), instead of the original's "hold for 2 s" operation. When active, every **direction reversal of the output** triggers a 1 ms pulse (10 V) at the PLS output and lights the red LED (with a short afterglow). Detection remembers the last extreme; only when the output retreats by more than 10 mV does the direction count as reversed. With AUTO off the pulse output stays silent. Very small signal swings (below about 10 mV) therefore do not trigger a pulse.

**LEDs**<br>
Blue: magnitude of the slew output (maximum across all channels, 10 V = full brightness). Red: pulse.

**DC blocker (context menu, per pair)**<br>
For the slew outputs a DC blocker can be switched on per pair ("DC blocker Sections C/D", "E/F", "G/H"), separately from the A/B blockers. It acts only on the output `x OUT`: the internal slew state, the auto-pulse detection, and the LEDs work with the unfiltered signal. With slow slew signals it largely removes them; it is meant for audio and is off by default.

---

<a id="sec4"></a>

## 4. Polyphony and channel selection
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**A/B: values per channel**<br>
Each of the 16 channels has its own values for SELECT, 0 V, 10 V, BP IN, BP OUT, and BP (192 params). A shared channel selector (knob with center digit 1–16 and ACTIVATE) determines which channel the knobs and the curve display show. As with Minerva, `pendingCh` and `displayedCh` are **not params** (pickup/catch, history, and preset recall must not influence GUI navigation) and are stored via JSON.

**A/B: channel count**<br>
Default "Auto": the maximum channel count of A1, A2, and SEL CV (at least 1). In the module context menu ("Channels Section A/B") a fixed number 1–16 can be chosen per section. Mono inputs are spread across all channels; unpatched inputs sit at 0 V.

**Right-click "Apply to all 16 channels"**<br>
On all A/B knobs and the BP switch: copies the value to all channels of the same section and type, as one undo step.

**C–H: polyphonic signals, global controls**<br>
The slewers process the channels of their IN individually (output and pulse channel count = channel count of the IN, at least 1); each channel has its own slew state. POS, NEG, TIME/RATE, and AUTO, however, apply once per pair for all channels. Only the slew CV acts per channel.

---

<a id="sec5"></a>

## 5. Panel layout
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Header row**<br>
Above A/B: channel selector (CHANNEL) and ACTIVATE, labels grey. Above the slew sections: title "ELENA".

**Sections A (blue) and B (orange)**<br>
Frames 54 mm wide and 96 mm tall each. From top to bottom: curve display; knob row "0 V" and "10 V"; knob row "BP IN", BP switch, "BP OUT"; knob row "SELECT" with "SEL CV"; jack row A1, A2, OUT.

**Sections C–H (teal)**<br>
One frame with three blocks (32 mm each). On the left of a block: POS and NEG (knobs, below them POS CV and NEG CV), to their right the switches TIME/RATE (top) and AUTO (bottom). On the right two rows with IN, OUT, PLS each (labeled "C IN" etc.) and two LEDs (red on top, blue below) beside the pulse jack.

---

<a id="sec6"></a>

## 6. Internal processing (C++)
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**`CvProcElena.hpp`**<br>
Free functions and a small state struct, without Rack dependencies apart from `simd` and `clamp`: `transfer(vin, v0, v10, bpOn, bpx, bpy)` (four channels as `float_4`, both segments are computed and selected via `ifelse`), `knobToSeconds`/`secondsToKnob`, `slewStep(in, out, tPos, tNeg, rateMode, dt)` (one channel, scalar), and `ReversalDetector` (direction reversal with hysteresis).

**`Elena.cpp`**<br>
`struct Elena : Module`. `processSection(s)` for A/B (channel count, blocks of 4, `getPolyVoltageSimd`, `transfer`, storing the operating point for the display), `processSlewers(dt)` for C–H (slew, auto-pulse, LEDs).

**Param layout (204 params)**<br>
A/B: `abParam(section, type, ch) = (section·6 + type)·16 + ch` with the types SEL, V0, V10, BPX, BPY, BP (indices 0–191). Slew pairs after that: `pairParam(pair, which) = 192 + pair·4 + which` with POS, NEG, MODE, AUTO (indices 192–203). New params are only appended at the end.

**Inputs/outputs/lights**<br>
Inputs: A1, A2, SEL per section (B = A + 3), then `SLEW_IN_C…H`, then per pair POS CV and NEG CV. Outputs: `OUT_A/B`, `SLEW_OUT_C…H`, `PULSE_C…H`. Lights: `PULSE_LED_C…H` (red), `CVOUT_LED_C…H` (blue).

**Persistence (JSON)**<br>
`pendingCh`, `displayedCh`, `channelsA`, `channelsB`, `rangeA`, `rangeB` (0 = 0–10 V, 1 = ±5 V, 2 = ±10 V, 3 = Unlimited; if the key is missing in older patches, the default 0 applies), each clamped, and `dcBlock` (array of five 0/1 entries for A, B, C/D, E/F, G/H; if the key is missing, the blocker is off). The filter state is not saved and is cleared in `onReset()`.

**DC blocker (engine)**<br>
`CvProcElena::dcBlockStep`: `y[n] = x[n] − x[n−1] + R·y[n−1]` with `R = exp(−2π·10 Hz / fs)`, recomputed when the sample rate changes. This gives −3 dB at 10 Hz, about −7 dB at 5 Hz, −20 dB at 1 Hz, and less than 1 dB attenuation from 20 Hz upward; a DC step decays with τ ≈ 16 ms. When switched off, the filter state is tracked (x[n−1] = input, y[n−1] = 0) so that no stale state acts when it is switched on; switching produces a click of the size of the DC component. Knobs go through the normal Rack params. The slew state is not saved and is reset in `onReset()`.

**CPU cost**<br>
The slew time (`std::pow`) is cached per pair, direction, and channel and only recomputed when the knob position (including CV) changes.

**Widgets**<br>
`ElenaKnob` (with "Apply to all 16 channels"), `ElenaToggle` (text switch based on `app::Switch`, for BP, TIME/RATE, and AUTO), `ElenaTransferDisplay`, channel selector, and ACTIVATE. All local structs live in an anonymous namespace (ODR, see `Minerva-Documentation_EN.md`); only `modelElena` is exported.

---

<a id="sec7"></a>

## 7. Patch examples
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Square → sawtooth/sine-like shape (slew)**<br>
Square wave 0–10 V (unipolar, e.g., 1 Hz) into `C IN`, scope on `C OUT`. POS and NEG equal: rounded/trapezoid shape; NEG fully left and POS turned up: sawtooth-like. Tested (slew works); the shape depends on the mode.

**Triangle from a ramp**<br>
Ramp 0–10 V into A1; 0 V = 0, 10 V = 0, BP on, BP IN = 5 V, BP OUT = 10 V: output = triangle 0→10→0 V.

**Multiplication**<br>
A1 unpatched, signal into A2, second signal (0–10 V) into SEL CV: output = `A2 · SEL_CV/10 V` (with the default curve).

**Bell `u²(1−u)²` (calculated with the formulas from `Elena.cpp`, not tested in Rack)**<br>
Needs Minerva and both Elena sections. 1) Minerva section A: ramp 0–10 V into Va, K = −1, Oset = 10 V → inverted ramp. 2) Elena section A: ramp into A2, inverted ramp into SEL CV, A1 unpatched; 0 V = 0, 10 V = 10, BP on with BP IN = 2.5 V, BP OUT = 10 V → parabola `4u(1−u)` with a 10 V peak. 3) Elena section B: parabola into A2 **and** into SEL CV, A1 unpatched, default curve → output = parabola². The output is symmetric with flat tails and a 10 V peak; the width results solely from the duration of the ramp. The input must be unipolar 0–10 V.

**What does not produce a bell (Kinovice test)**<br>
A triangle (section A with breakpoint) through two cascaded slews in Rate mode yields a rounded triangle, not a bell; with a periodic ramp the result looks like a sine. Reason: low-pass filtering only rounds the corners but does not produce flat tails.

---

<a id="sec8"></a>

## 8. Sources, assumptions, and open points
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**A/B as transfer-curve sections**<br>
The reference documentation describes A and B only briefly as sections with a similar function. The formulas come from the description of the transformation section and were recalculated for both endpoints and the breakpoint.

**Rate/Time (derived interpretation)**<br>
The reference documentation describes the two modes only vaguely: Rate tends to give a larger output range with a faster input, Time is based on the time a voltage needs to travel from 0 to 10 V. The implementation (Time = linear slope, Rate = exponential catch-up) is an interpretation. For Time it matches the description of a linear travel time for the full range (Kinovice). For Rate mode alternatives are conceivable. The implemented behavior was measured (see test status); whether the assignment matches the original remains open.

**Contradictory search-engine texts (rejected)**<br>
Two definitions found on the internet describe "Time" as a constant transition time independent of the jump and "Rate" as a given rate of change. That would **swap** the meaning of Time and Rate compared to the implementation. Kinovice decided **not** to swap the modes ("search engines can be wrong"). In addition the reference documentation's Time description (time for 0–10 V) matches the current implementation, and the origin of those texts is not documented. A swap would be a one-line change in `slewStep`.

**Select CV replaces the knob**<br>
The reference documentation says that a patched CV replaces the knob and at the same time mentions turning the knob to 0; implementation: a patched CV replaces the knob completely.

**Limiting of Vin**<br>
In the default range Vin is limited to 0…1 (0–10 V). The reference documentation does not state this explicitly; the original's curve is defined over this range. The range selection (±5 V, ±10 V, Unlimited) is an extension of the original and is not described in the reference documentation.

**Auto-pulse**<br>
The reference documentation mentions unreliability with long slews, signals above 35 Hz, and a small input range. The implementation detects the reversal at the output with a hysteresis of 10 mV; pulse length 1 ms and 10 V are assumed. Whether the pulse output emits anything without auto-pulse is not described by the reference documentation ("as always"); here it stays silent.

**Slew CV**<br>
Scaling 10 V = full knob travel (assumed; the reference documentation only says the CV is added to the knob position).

---

<a id="sec9"></a>

## 9. Design decisions
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

- **Name Elena (Kinovice):** after one of the Anguissola sisters (the choice was Elena or Anna Maria).
- **Complete CV processor as its own module (Kinovice):** A/B sections with transfer curve plus slew sections C–H, next to and independent of Minerva. Minerva stays untouched.
- **A/B adjustable per channel like Minerva, C–H global (Kinovice decision):** for slew, 16 individual knobs would add little and multiply the number of params; the signals remain polyphonic.
- **Auto-pulse via its own button instead of "hold for 2 s" (Kinovice decision):** the hold operation is impractical in Rack and the state is not visible. Deliberately deviates from the original.
- **Curve graphic instead of an LED chain:** shows the curve and operating point of the shown channel.
- **Do not swap Rate/Time (Kinovice):** search engines can be wrong; the interpretation according to the reference documentation stays, see section 8.
- **Channel selector without a param:** as with Minerva, Lucia, and Europa.
- **Right-click "Apply to all 16 channels"** and channel count "Auto/1–16" per A/B section (context menu) as with Minerva.
- **Selectable input range per section (Kinovice decision):** 0–10 V (default), ±5 V, ±10 V, and Unlimited. The range applies to the inputs **and** the output of the section (±5 V in, ±5 V out); the choice is made per section, not shared between A and B. An extension for use with bipolar audio and modulation signals, not part of the original.
- **Switchable DC blocker (Kinovice decision):** acts on the A/B outputs and the slew outputs, separately switchable per A/B section and per slew pair (five switches), in the context menu, with a fixed cutoff of about 10 Hz. Off by default, since it also removes slow CV components. Motivation: a DC component with bipolar audio that showed up in the analyzer.
- **Layout 42HP** with A/B on the left and the three slew blocks on the right.
