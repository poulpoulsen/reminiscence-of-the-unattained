# Minerva – Technical Documentation

Sixth module of the **Kinovice reminiscence of the unattained** plugin: a
VCV Rack module heavily inspired by the wonderful Buchla world of modules –
a dual voltage processor (14HP). Two identical sections each combine four
voltages into one output voltage. Minerva is polyphonic; each of the 16
channels has its own control values. This file describes every control, the
math, the polyphony mechanics, the C++ structure, and the assumptions left
open by the reference documentation.

**Distinction:** Minerva covers analog-style voltage math (attenuverter,
crossfader, offset). Curve transformation and slew limiting, as found in a
digital control voltage processor, are a different function and are
implemented in this plugin as its own module, **Elena**, see
`Elena-Documentation_EN.md`.

Sources: publicly available user documentation of the hardware that inspired
this module.

**Notice:** The *Kinovice reminiscence of the unattained* plugin is an independent work, heavily inspired by the wonderful Buchla world of modules. It is not affiliated with, authorized by, or endorsed by Buchla, Tiptop Audio, or any other manufacturer. Product and company names mentioned are trademarks of their respective owners and are used only to describe which functional concepts inspired a module.

Test status (Kinovice, live in a running Rack instance): the basic function
of both sections, polyphonic operation, individual control values per
channel, the channel selector, and the +5 Vb switch (switchable individually
per channel) all work. Also tested in Rack: the M knob over −1…+2 at Vb and
Vc, ring modulation in the default mode (unlimited M range), "Limited 0…1",
output limiting Soft and Hard, and loading an older patch without the new
JSON keys. The DC blocker (section 2) is new, checked by calculation, and not yet
tested in Rack. The parabola example marked "calculated" in section 6 has
also not been tested in Rack.

---

<a id="toc"></a>

## Table of Contents

1. [Overview](#sec1) · [[#1. Overview|Obsidian]]
2. [Math and signal flow](#sec2) · [[#2. Math and signal flow|Obsidian]]
3. [Controls and panel](#sec3) · [[#3. Controls and panel|Obsidian]]
4. [Polyphony and channel selection](#sec4) · [[#4. Polyphony and channel selection|Obsidian]]
5. [Internal processing (C++)](#sec5) · [[#5. Internal processing (C++)|Obsidian]]
6. [Patch examples](#sec6) · [[#6. Patch examples|Obsidian]]
7. [Assumptions and open points](#sec7) · [[#7. Assumptions and open points|Obsidian]]
8. [Design decisions](#sec8) · [[#8. Design decisions|Obsidian]]

---

<a id="sec1"></a>

## 1. Overview
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Module** *(`Minerva.cpp`, `VoltageProc.hpp`, `modelMinerva`)*<br>
14HP, two identical sections A and B. Per section: an attenuverter (Va, knob K), a crossfader between Vb and Vc (knob M plus an M CV input, plus a +5 Vb switch on Vb), and an offset (knob Oset, 0–10 V). All three blocks are summed into the output Vout.

**Polyphony**<br>
Up to 16 channels per section. All knobs and the +5 Vb switch are stored separately per channel (128 params). A single shared channel selector determines which channel the panel controls currently show and edit.

**No state variables (exception: DC blocker)**<br>
Pure linear analog math without time behavior; the only exception is the optionally switchable DC blocker (section 2). Each sample computes the output voltage solely from the current inputs and control values.

**Two options in the module context menu**<br>
By default Minerva computes freely: M is unlimited (four-quadrant multiplication, inversion, gain above 1) and the output is not clipped. Both can be switched: **M range** (unlimited / limited to 0…1) and **output limiting** (off / soft / hard, fixed ±12 V). Both settings apply to the whole module and are stored only in JSON, not in the params. In addition, a DC blocker can be switched on per section (see section 2).

---

<a id="sec2"></a>

## 2. Math and signal flow
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

Per section and channel:

```
Vout  = Va·K + ( (Vb + 5 V·S)·(1 − M) + Vc·M ) + Voset
M     = M_knob + M_CV / 10 V                      (default: unlimited)
M     = clamp( M_knob + M_CV / 10 V , 0 , 1 )     (option: limited)
Voset = Oset_knob(0…1) · 10 V
```

Output limiting follows optionally (see below).

**Attenuverter (Va, K)**<br>
K ranges from −1 to +1, center position is 0 (Va silent). Turning left reverses the polarity of Va (K = −1: Va inverted), turning right keeps it (K = +1: unchanged). Intermediate values only scale the amount. K acts on Va only.

**Crossfader (Vb, Vc, M)**<br>
M crossfades between Vb (M = 0) and Vc (M = 1). The knob and the M CV input are added. Since M multiplies Vc and (1 − M) multiplies Vb, the crossfader is also a multiplier: with Vb unpatched the result is `Vc·M`.

**M range: unlimited (default) or limited**<br>
*Unlimited:* M may run arbitrarily far above 1 or below 0. The formula simply keeps computing:

- M > 1 (e.g. 1.5): `(1 − M) = −0.5`, so Vb is inverted and halved, and Vc is amplified by 1.5.
- M < 0 (e.g. −0.5): Vc is inverted and halved, and Vb is amplified by 1.5.

**Important: Vb and Vc behave oppositely at the ends of the knob.** Vc is weighted by M, Vb by (1 − M). Across the knob range from −1 to +2 this gives:

| M | Vout (crossfade only) | Vc | Vb |
|---|---|---|---|
| −1 | `2·Vb − Vc` | inverted | doubled |
| 0 | `Vb` | off | unchanged |
| 0.5 | `0.5·Vb + 0.5·Vc` | half | half |
| 1 | `Vc` | unchanged | off |
| +2 | `−Vb + 2·Vc` | doubled | inverted |

A signal at **Vc** is amplified at M = +2 and inverted at M = −1. A signal at **Vb** behaves exactly the other way round: amplified at M = −1, inverted at M = +2. The weights always sum to 1, so the difference between Vb and Vc is extrapolated beyond the ends. With the +5 Vb switch the factor applies to `Vb + 5 V`, so the offset is amplified or inverted along with the signal. A signal at M CV shifts M additionally (10 V = span of 1).

This makes the M control usable bipolarly: a bipolar signal at M CV with the M knob at 0 and Vb unpatched gives a true four-quadrant multiplication `Vc·M` (ring modulation).

*Limited:* M is hard-clamped to 0…1. Only a pure crossfade takes place; both factors stay positive. Useful for patches that rely on the clipping and for closeness to the original.

**+5 Vb switch (S)**<br>
Labeled ".5" / "+.5" on the original, "**+5 Vb**" in the plugin. Adds 5 V to Vb **inside** the crossfade (factor (1 − M)). With Vb unpatched this puts 5 V there, the contribution being `5 V·(1 − M)`. This makes it possible to invert and scale the M control. The Oset knob cannot replace it, since it only shifts the finished sum.

**Offset (Oset)**<br>
Adds 0–10 V to the sum.

**DC blocker (context menu, per section)**<br>
In the module context menu a DC blocker can be switched on per section ("DC blocker Section A/B"), off by default. It is a one-pole high-pass at about 10 Hz (−3 dB at 10 Hz, about −7 dB at 5 Hz, −20 dB at 1 Hz), with its own state per channel. Order at the output: signal part → DC blocker → + static part → output limiting. It sits **before Oset**: it processes only the signal part `Va·K + Vb·(1 − M) + Vc·M`. Oset and the +5 Vb part `5 V·(1 − M)` are added afterwards and remain as deliberate DC components (Kinovice decision). DC components from the input signal are removed. Since it also removes slow CV components below about 10 Hz, it is meant for audio. Switching it on or off produces a click of the size of the DC component.

**Unpatched inputs**<br>
Sit at 0 V (this also applies to Vb before S is added).

**Output limiting (Off / Soft / Hard)**<br>
It sits at the output, after the sum (and after a switched-on DC blocker), and corresponds to an op-amp at its supply rail. The level is fixed at **L = ±12 V**. Intermediate values such as M are not affected.

- **Off (default):** no limiting. Negative voltages and values above 12 V are possible.
- **Hard:** `Vout = clamp(Vout, −L, +L)`. Exactly linear below L, hard-cut above.
- **Soft:** up to the threshold **T = 0.9·L = 10.8 V** the signal stays exactly linear; above it, it blends smoothly into L:

```
|x| ≤ T:  y = x
|x| > T:  y = sign(x) · ( T + (L − T) · tanh( (|x| − T) / (L − T) ) )
```

The curve is continuous at the threshold and has the same slope there (no kink). The knee is (L − T) = 1.2 V wide. The usual 0…10 V range therefore stays completely undistorted; only real overdrive is trimmed. As a safety net the limiting is especially useful when M is unlimited and a faulty patch produces large levels.

---

<a id="sec3"></a>

## 3. Controls and panel
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Header area**<br>
Title "MINERVA". Below it the channel selector (knob with center digit 1–16) and the ACTIVATE button. Labels are grey, since blue is already Section A in the plugin's color logic.

**Sections A (blue) and B (orange)**<br>
Two stacked frames (42 mm tall each). Knob row: **K** (left), **M** (center), **OSET** (right). Right of the section title sits the **+5 Vb** switch (lit orange when active). Jack row in the order of the original: **Va, Vb, M CV, Vc, OUT**.

**Knob ranges and defaults**<br>
K: −1…+1, default 0. M: −1…+2, default 0.5 (Vb and Vc half each; 0 = Vb only, 1 = Vc only). Oset: 0…10 V (displayed in volts), default 0. +5 Vb: off. **Note:** with K = 0, Va is silent by default – K must be turned up for Va to be heard. The M knob spans −1 to +2. Values outside 0…1 (inversion, gain) only take effect with the M range unlimited; with "Limited 0…1" the sum of knob and M CV is still clamped to 0…1. The range can additionally be exceeded via M CV (10 V = span of 1).

**Knob and switch context menu**<br>
"Apply to all 16 channels" copies this knob's (or switch's) value to all 16 channels of the same section and type, as **one** undo step.

**Module context menu**<br>
- "Channels Section A" and "Channels Section B": Auto or fixed 1–16 (see section 4).
- "M range": **Unlimited** (default) / Limited 0…1.
- "Output limiting": **Off** (default) / Soft / Hard. Level fixed at ±12 V, soft threshold fixed at 10.8 V.
- "DC blocker Section A" / "DC blocker Section B": switch the DC blocker on per section (default off, see section 2).

M range and output limiting apply jointly to both sections and all channels. The DC blocker is set per section.

---

<a id="sec4"></a>

## 4. Polyphony and channel selection
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Channel count of the output**<br>
Default "Auto": the maximum channel count of Va, Vb, M CV, and Vc, at least 1. Through the module context menu a fixed channel count (1–16) can be chosen per section. This allows splitting a mono signal across up to 16 channels, each with its own control values.

**Mono inputs**<br>
A mono input is spread across all output channels.

**Channel selector (not a param)**<br>
`pendingCh` (selection) and `displayedCh` (shown channel) are deliberately **not params**: pickup/catch, history, and preset recall must not influence GUI navigation. The knob changes `pendingCh`; ACTIVATE copies it into `displayedCh`. The digit in the knob is green when selection and shown channel match, otherwise amber; ACTIVATE is green or red accordingly. Both values are stored via JSON and clamped on load.

**Widget sets**<br>
All knobs and switches exist 16 times on top of each other (one set per channel, 128 param widgets in total); `step()` hides all but the shown channel.

**Module-wide options apply to all channels**<br>
M range and output limiting cannot be set per channel. They act uniformly on all channels of both sections and, like the channel selector, are not params.

---

<a id="sec5"></a>

## 5. Internal processing (C++)
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**`VoltageProc.hpp`**<br>
Three free functions, all computing four channels at once as `float_4`:

- `VoltageProc::processParts(va, vb, vc, mCv, k, mKnob, vosetKnob, plus5, mLimited, signalPart, staticPart)` computes the sum in two parts: `signalPart = Va·K + Vb·(1 − M) + Vc·M` (signal and crossfader) and `staticPart = 5 V·plus5·(1 − M) + Voset` (deliberate DC components). With `mLimited = false` (default) M is not clamped, otherwise via `simd::clamp(m, 0, 1)`.
- `VoltageProc::process(…)` with the same inputs returns the total `signalPart + staticPart`.
- `VoltageProc::limitOutput(x, mode)` applies the output limiting (`mode`: 0 = off, 1 = soft, 2 = hard).

Constants: `M_CV_VOLTS_FULL = 10`, `VOSET_MAX_VOLTS = 10`, `PLUS5_VOLTS = 5`, `OUT_LIMIT_VOLTS = 12`, `SOFT_KNEE_RATIO = 0.9` (threshold T = 10.8 V).

**Soft limit in SIMD**<br>
Rack has no SIMD `tanh`. `tanh` is therefore replaced by the rational approximation `s(t) = t·(27 + t²) / (27 + 9t²)`, with `t` clamped to 3 beforehand (where `s = 1`, so the transition is continuous; the slope at 0 is 1, like the real `tanh`). Computed branch-free:

```
a    = abs(x)
t    = min( max(a − T, 0) / (L − T) , 3 )
s    = t·(27 + t²) / (27 + 9t²)
y    = sign(x) · ( min(a, T) + (L − T)·s )
```

Below T, `t = 0` and therefore `y = x`. From `a = T + 3·(L − T) = 14.4 V` on, `y` stays constant at L.

**`Minerva.cpp`**<br>
`struct Minerva : Module` with `processSection(s)` per section: determine channel count, read params in blocks of 4, fetch inputs via `getPolyVoltageSimd` (unpatched = 0 V), call `VoltageProc::process`, pass the result through `VoltageProc::limitOutput` when `outputClip != 0`, and write it with `setVoltageSimd`. With limiting off the call is skipped entirely.

**Param layout (128 params)**<br>
Per section three contiguous 16-blocks: K, M, Oset (indices 0–95, via `paramIndex(section, type, ch)`). The +5 Vb switches are two further 16-blocks **at the end of the enum** (indices 96–127, via `plus5Param(section, ch)`), so that the indices of the original 96 params, and with them saved patches, stayed unchanged. A `static_assert` ensures the enum and the helper function agree. M range and output limiting occupy no params.

**Inputs/outputs**<br>
Inputs per section in the order Va, Vb, M CV, Vc (section B = section A + 4). Outputs `OUT_A_OUTPUT`, `OUT_B_OUTPUT`.

**Persistence (JSON)**<br>
`pendingCh`, `displayedCh`, `channelsA`, `channelsB` (0 = Auto, 1–16 fixed), `mLimited` (bool; if the key is missing: `false`, i.e. unlimited), and `outputClip` (0 = off, 1 = soft, 2 = hard; if the key is missing: 0), each clamped on load. All control values go through the normal Rack params. In addition `dcBlock` (array of two 0/1 entries for A and B; if the key is missing in older patches, the blocker is off). The filter state is not saved and is cleared in `onReset()`.

**DC blocker (engine)**<br>
`DcBlock.hpp` (shared with Elena): `y[n] = x[n] − x[n−1] + R·y[n−1]` with `R = exp(−2π·10 Hz / fs)`, recomputed when the sample rate changes. `Minerva.cpp` gets the signal part and the static part separately from `VoltageProc::processParts` (the respective M handling, unlimited or limited, applies to both parts). The blocker filters only the signal part; then the static part (Oset and `5 V·(1 − M)`) is added back and the output limiting is applied. When switched off, the filter state is tracked (x[n−1] = signal part, y[n−1] = 0) so that no stale state acts when it is switched on.

**Namespace**<br>
All local structs live in an anonymous namespace (internal linkage), so they can never collide with same-named structs from other `.cpp` files of the plugin `.so` (One Definition Rule incident with `PanelLabel`, see the Sofonisba documentation). Only `modelMinerva` is exported. `PanelLabel` and the colors come from `PanelLabel.hpp`.

---

<a id="sec6"></a>

## 6. Patch examples
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Inverting (sine, 180°)**<br>
Bipolar sine into Va, K = −1: output = −Va. For a sine this equals a 180° phase shift. Strictly speaking it is a mirroring of the voltage, not a time shift: a rising ramp becomes a falling ramp. Arbitrary phase angles are not possible with Minerva.

**Inverted ramp (10 V → 0 V)**<br>
Ramp 0–10 V into Va, K = −1, Oset = 10 V.

**Multiplication (unipolar)**<br>
Vb unpatched, signal into Vc, second signal (0–10 V) into M CV, M knob at 0: output = `Vc · (M_CV / 10)`.

**Ring modulation (four-quadrant multiplication, tested in Rack)**<br>
M range unlimited (default). Vb unpatched, +5 Vb off, audio signal into Vc, bipolar modulator (e.g. ±5 V) into M CV, M knob at 0: output = `Vc · (M_CV / 10)`. The negative half-wave of the modulator reverses Vc. In "limited" mode it would instead be cut off at M = 0.

**Inverting the M control**<br>
Vb unpatched, +5 Vb on, Vc unpatched: output = `5 V·(1 − M)`. The M CV then acts inverted.

**Parabola from a ramp (calculated, not tested in Rack)**<br>
Section A: ramp into Va, K = −1, Oset = 10 V → inverted ramp. Section B: ramp into Vc, inverted ramp into M CV, M knob at 0 → `10 V · u · (1 − u)` (u = ramp/10 V). The peak is only 2.5 V, since Minerva has a maximum gain factor of 2 per section. For the full bell `u²(1−u)²` with a 10 V peak see `Elena-Documentation_EN.md`. Since u stays between 0 and 1, the result is the same in both M modes.

---

<a id="sec7"></a>

## 7. Assumptions and open points
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

The reference documentation contains the equation but leaves details open. The following points are **assumptions** and not documented:

**M CV scaling**<br>
The reference documentation only states that knob and CV are added. Assumed: 10 V = full stroke (M 0→1). The constant `M_CV_VOLTS_FULL` sits in one place in `VoltageProc.hpp`.

**Behavior of M outside 0…1**<br>
The reference documentation says nothing about this. Both variants (unlimited and limited) are assumptions. The real circuit probably limits via the supply voltage rather than hard at M = 1, but this is not documented. Kinovice's decision: the default is **unlimited**, because it opens up more patching possibilities (ring modulation, gain above 1, inversion of Vb or Vc as a math operation). The limited mode remains selectable via the context menu. **Note on older patches:** patches built with the earlier fixed limiting and relying on it (e.g. a ramp running beyond 10 V) behave differently under the new default. Select "M range: Limited" for those.

**Output limiting, level, and impedance**<br>
The reference documentation says nothing about the hardware's limits, and no schematic is available. The fixed ±12 V level follows the usual Eurorack supply; the soft threshold of 0.9·L (10.8 V) follows from the wish to leave the entire 0…10 V range undistorted. Both are assumptions. The default is "Off".

**+5 Vb switch on an unpatched Vb**<br>
5 V are present (the description says the switch internally connects a +5 V signal).

---

<a id="sec8"></a>

## 8. Design decisions
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

- **Name Minerva (Kinovice):** after one of the Anguissola sisters, like the other modules of the plugin.
- **Analog-style voltage math and digital-style transformation as separate modules (Kinovice):** two different functions, therefore two modules: Minerva (this file) and `Elena`.
- **Polyphonic with individual values per channel (Kinovice requirement):** all 16 channels individually adjustable, with one shared channel selector for both sections (Kinovice decision).
- **Channel selector without a param:** as with Lucia and Europa, so that pickup/catch, history, and preset recall do not influence navigation.
- **Retrofitted +5 Vb switch (Kinovice decision):** first omitted by mistake, on the grounds that Oset replaces it. That is wrong, since Oset shifts the sum while the switch acts inside the crossfade. Added **per section and channel**, appended at the end of the enum so that saved patches load unchanged.
- **Right-click "Apply to all 16 channels"** as one undo step, so that not all 16 channels have to be set individually.
- **Channel count "Auto" or fixed 1–16 per section (context menu):** Auto follows the input polyphony, fixed allows splitting a mono signal.
- **M range unlimited by default, switchable (Kinovice decision):** replaces the earlier fixed limit to 0…1. Unlimited enables four-quadrant multiplication, gain above 1, and inversion. For this the M knob spans −1 to +2 (default 0.5), so inversion and gain are possible directly at the knob without a second voltage source. The switch lives only in JSON (no param); saved patches still load, but behave differently under the default if they relied on the old limiting.
- **Optional output limiting: Off / Soft / Hard (Kinovice decision):** replaces the earlier decision "no output clipping". The default stays "Off" so that Minerva remains an exactly linear math block. The limiting serves as a safety net, especially with unlimited M.
- **Fixed level ±12 V, fixed soft threshold 0.9·L = 10.8 V (Kinovice decision):** keeps the context menu small. With 0.9 instead of 0.8 the entire 0…10 V range stays exactly linear (with 0.8, a regular 10 V signal would already have been slightly distorted from 9.6 V on).
- **Soft mode as a soft knee instead of plain `tanh`:** a plain `L·tanh(x/L)` distorts even small levels (only 0.46·L at 0.5·L) and would slightly distort multiplication or the parabola. The soft knee stays exactly linear up to T.
- **Module-wide instead of per section (Kinovice decision):** M range and output limiting apply to the whole module, which keeps the menu tidy.
- **Switchable DC blocker before Oset (Kinovice decision):** per section in the context menu, fixed at about 10 Hz, off by default. It filters only signal and crossfader; Oset and the +5 Vb part are kept because they are deliberate DC components. It sits before the output limiting. Motivation: a DC component with fed-in audio that showed up in the analyzer.
