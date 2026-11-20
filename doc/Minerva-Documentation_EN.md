# Minerva – Technical Documentation

Sixth module of the plugin **reminiscence of the unattained**:
a VCV Rack module, strongly inspired by the wonderful world of Buchla modules –
a Dual Voltage Processor (14HP). Two identical sections each combine
four voltages into one output voltage. Minerva is polyphonic; each of its
16 channels has its own control values. This file describes every control,
the math, the polyphony mechanics, the C++ structure, and the assumptions
the reference documentation leaves open.

**Scope:** Minerva covers analog voltage math (attenuverter,
crossfader, offset). Curve transformation and slew limiting, as offered by a
digital control voltage processor, are a different function. The
plugin implements them in the separate module **Elena**, see
`Elena-Documentation_EN.md`.

Sources: publicly available user documentation of the hardware that inspired
this module.

**Note:** The plugin *reminiscence of the unattained* is an independent work, strongly inspired by the wonderful world of Buchla modules. It has no affiliation with Buchla, Tiptop Audio, or any other manufacturer, and is neither authorized nor endorsed by them. Product and company names mentioned are trademarks of their respective owners. They serve only to describe the functional concepts that inspired a module.

Test status (live in a running Rack): basic function of both
sections, polyphonic operation, individual control values per channel,
channel selector, and the +5 Vb switch (switchable per channel) all work.
Also verified in Rack: the M knob in the range −1…+2 on Vb and Vc, ring
modulation in the default mode (unlimited M range), "Limited 0…1",
output limiting Soft and Hard, and loading an older patch without
the new JSON keys. The DC blocker (Section 2) is new, verified by calculation,
and not yet tested in Rack. The parabola example in Section 6
has likewise only been verified by calculation.

---

<a id="toc"></a>

## Table of Contents

1. [Overview](#sec1) · [[#1. Overview|Obsidian]]
2. [Math and Signal Flow](#sec2) · [[#2. Math and Signal Flow|Obsidian]]
3. [Controls and Panel](#sec3) · [[#3. Controls and Panel|Obsidian]]
4. [Polyphony and Channel Selection](#sec4) · [[#4. Polyphony and Channel Selection|Obsidian]]
5. [Internal Processing (C++)](#sec5) · [[#5. Internal Processing (C++)|Obsidian]]
6. [Patch Examples](#sec6) · [[#6. Patch Examples|Obsidian]]
7. [Assumptions and Open Points](#sec7) · [[#7. Assumptions and Open Points|Obsidian]]
8. [Design Decisions](#sec8) · [[#8. Design Decisions|Obsidian]]

---

<a id="sec1"></a>

## 1. Overview
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Module** *(`Minerva.cpp`, `VoltageProc.hpp`, `modelMinerva`)*<br>
14HP, two identical sections A and B. Each section has an attenuverter (Va, knob K), a crossfader between Vb and Vc (knob M, M CV input, and a +5 Vb switch on Vb), and an offset (knob Oset, 0–10 V). The three blocks are summed to the output Vout.

**Polyphony**<br>
Each section processes up to 16 channels. All knobs and the +5 Vb switch are stored per channel (128 params). A shared channel selector determines which channel the panel controls show and edit.

**No state variables (exception: DC blocker)**<br>
Minerva is purely linear and has no time behavior. The only exception is the optional DC blocker (Section 2). Each sample call computes the output voltage solely from the current inputs and control values.

**Two options in the module context menu**<br>
By default Minerva computes freely: M is not limited (four-quadrant multiplication, inversion, gain above 1), and the output is not clipped. Both can be switched: **M range** (unlimited / limited to 0…1) and **Output limiting** (off / soft / hard, fixed at ±12 V). Both settings apply to the whole module and are stored only in the JSON, not in the params. In addition, a DC blocker can be enabled per section (Section 2).

---

<a id="sec2"></a>

## 2. Math and Signal Flow
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

The following applies per section and channel:

```
Vout  = Va·K + ( (Vb + 5 V·S)·(1 − M) + Vc·M ) + Voset
M     = M_Regler + M_CV / 10 V          (Standard: unbegrenzt)
M     = clamp( M_Regler + M_CV / 10 V , 0 , 1 )   (Option: begrenzt)
Voset = Oset_Regler(0…1) · 10 V
```

Output limiting follows optionally (see below).

**Attenuverter (Va, K)**<br>
K runs from −1 to +1; the center position 0 mutes Va. Turning left reverses the polarity of Va (K = −1: Va inverted), turning right keeps it (K = +1: unchanged). Intermediate values only scale the amount. K acts on Va only.

**Crossfader (Vb, Vc, M)**<br>
M crossfades between Vb (M = 0) and Vc (M = 1). The knob and the M CV input add together. Because M multiplies Vc and (1 − M) multiplies Vb, the crossfader is also a multiplier: with Vb unpatched the result is `Vc·M`.

**M range: unlimited (default) or limited**<br>
*Unlimited:* M may run arbitrarily far above 1 or below 0. The formula simply keeps computing:

- M > 1 (e.g. 1.5): `(1 − M) = −0.5`. Vb is inverted and halved, Vc is amplified by 1.5.
- M < 0 (e.g. −0.5): Vc is inverted and halved, Vb is amplified by 1.5.

**Important: Vb and Vc behave in opposite ways at the ends of the knob.** Vc is weighted by M, Vb by (1 − M). Across the knob range from −1 to +2 this gives:

| M | Vout (crossfade only) | Vc | Vb |
|---|---|---|---|
| −1 | `2·Vb − Vc` | inverted | doubled |
| 0 | `Vb` | off | unchanged |
| 0.5 | `0.5·Vb + 0.5·Vc` | half | half |
| 1 | `Vc` | unchanged | off |
| +2 | `−Vb + 2·Vc` | doubled | inverted |

A signal at **Vc** is amplified at M = +2 and inverted at M = −1. A signal at **Vb** behaves exactly the other way round: amplified at M = −1, inverted at M = +2. The weights always add up to 1. The difference between Vb and Vc is therefore extrapolated beyond the ends. With the +5 Vb switch, the factor acts on `Vb + 5 V`; the offset is amplified or inverted along with it. A signal at M CV shifts M additionally (10 V = stroke 1).

This makes M bipolar-controllable: with the M knob at 0 and Vb unpatched, a bipolar signal at M CV gives a true four-quadrant multiplication `Vc·M` (ring modulation).

*Limited:* M is hard-clamped to 0…1. Only a pure crossfade takes place; both factors stay positive. This suits patches that rely on the clipping, and is closer to the original.

**+5 Vb switch (S)**<br>
The original labels it ".5" or "+.5", the plugin labels it **"+5 Vb"**. The switch adds 5 V to Vb, **inside** the crossfade (factor (1 − M)). With Vb unpatched, 5 V are therefore present, and the contribution is `5 V·(1 − M)`. This allows the M control to be inverted and scaled. The Oset knob cannot replace this, since it only shifts the finished sum.

**Offset (Oset)**<br>
Adds 0–10 V to the sum.

**DC blocker (context menu, per section)**<br>
In the module context menu, a DC blocker can be enabled per section ("DC blocker Section A/B"), off by default. It is a one-pole high-pass at about 10 Hz (−3 dB at 10 Hz, about −7 dB at 5 Hz, −20 dB at 1 Hz) with its own state per channel. Order at the output: signal part → DC blocker → + fixed part → output limiting. The blocker sits **before Oset** and processes only the signal part `Va·K + Vb·(1 − M) + Vc·M`. Oset and the +5 Vb part `5 V·(1 − M)` are added afterwards and are deliberately kept as DC components (my decision). The blocker removes DC components from the input signal. Because it also removes slow CV components below about 10 Hz, it is intended for audio. Switching it on or off produces a click equal to the DC component.

**Unpatched inputs**<br>
They sit at 0 V. This also applies to Vb, before S is added.

**Output limiting (Off / Soft / Hard)**<br>
It sits at the output after the sum (and after an enabled DC blocker) and corresponds to an op-amp at its supply voltage. The level is fixed at **L = ±12 V**. Intermediate values such as M remain untouched.

- **Off (default):** no limiting. Negative voltages and values above 12 V are possible.
- **Hard:** `Vout = clamp(Vout, −L, +L)`. Exactly linear below L, hard-clipped above.
- **Soft:** Up to the threshold **T = 0.9·L = 10.8 V** the signal stays exactly linear, above it the signal transitions gently into L:

```
|x| ≤ T:  y = x
|x| > T:  y = sign(x) · ( T + (L − T) · tanh( (|x| − T) / (L − T) ) )
```

The curve is continuous at the threshold and has the same slope there (no kink). The knee is (L − T) = 1.2 V wide. The usual 0…10 V range thus stays completely undistorted; only true overdrive is clipped. As a safety net the limiting is especially useful when M is unlimited and a faulty patch produces large levels.

---

<a id="sec3"></a>

## 3. Controls and Panel
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Header area**<br>
Title "MINERVA". Below it are the channel selector (knob with a center digit 1–16) and the ACTIVATE button. The lettering is gray because blue already denotes Section A in the plugin's color scheme.

**Sections A (blue) and B (orange)**<br>
Two stacked frames (42 mm high each). Knob row: **K** (left), **M** (center), **OSET** (right). To the right of the section title sits the **+5 Vb** switch (lights orange when active). The jack row follows the order of the original: **Va, Vb, M CV, Vc, OUT**.

**Knob ranges and defaults**<br>
K: −1…+1, default 0. M: −1…+2, default 0.5 (Vb and Vc half each; 0 = Vb only, 1 = Vc only). Oset: 0…10 V (displayed in volts), default 0. +5 Vb: off. **Note:** With K = 0, Va is muted by default. K must be turned up for Va to be heard. Values of M outside 0…1 (inversion, gain) only take effect with an unlimited M range. With "Limited 0…1", the sum of knob and M CV is still clamped to 0…1. M CV can additionally exceed the range (10 V = stroke 1).

**Context menu for knobs and switches**<br>
"Apply to all 16 channels" copies the value of this knob (or switch) to all 16 channels of the same section and type, as **one** undo step.

**Module context menu**<br>
- "Channels Section A" and "Channels Section B": Auto or fixed 1–16 (see Section 4).
- "M range": **Unlimited** (default) / Limited 0…1.
- "Output limiting": **Off** (default) / Soft / Hard. Level fixed at ±12 V, soft threshold fixed at 10.8 V.
- "DC blocker Section A" / "DC blocker Section B": enable the DC blocker per section (default off, see Section 2).

M range and output limiting apply jointly to both sections and all channels. The DC blocker is set per section.

---

<a id="sec4"></a>

## 4. Polyphony and Channel Selection
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Output channel count**<br>
The default is "Auto": the maximum of the channel counts of Va, Vb, M CV, and Vc, at least 1. In the module context menu, a fixed channel count (1–16) can be chosen per section. This allows a mono signal to be split across up to 16 channels, each with its own control values.

**Mono inputs**<br>
A mono input is distributed to all output channels.

**Channel selector (not a param)**<br>
`pendingCh` (selection) and `displayedCh` (displayed channel) are deliberately **not params**. Pickup/catch, history, and preset recall should not affect GUI navigation. The knob changes `pendingCh`; ACTIVATE copies it to `displayedCh`. The digit in the knob is green when selection and displayed channel match, otherwise amber. ACTIVATE is then green or red, respectively. Both values are saved via JSON and clamped on load.

**Widget sets**<br>
All knobs and switches exist 16 times, stacked on top of each other (one set per channel, 128 param widgets in total). `step()` hides all but the displayed channel.

**Module-wide options apply to all channels**<br>
M range and output limiting cannot be set per channel. They apply uniformly to all channels of both sections and, like the channel selector, are not params.

---

<a id="sec5"></a>

## 5. Internal Processing (C++)
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**`VoltageProc.hpp`**<br>
Three free functions, all operating on four channels at once as `float_4`:

- `VoltageProc::processParts(va, vb, vc, mCv, k, mKnob, vosetKnob, plus5, mLimited, signalPart, staticPart)` computes the sum in two parts: `signalPart = Va·K + Vb·(1 − M) + Vc·M` (signal and crossfader) and `staticPart = 5 V·plus5·(1 − M) + Voset` (deliberate DC components). With `mLimited = false` (default), M is not clamped; otherwise it is clamped with `simd::clamp(m, 0, 1)`.
- `VoltageProc::process(…)` takes the same inputs and returns the total sum `signalPart + staticPart`.
- `VoltageProc::limitOutput(x, mode)` applies the output limiting (`mode`: 0 = Off, 1 = Soft, 2 = Hard).

Constants: `M_CV_VOLTS_FULL = 10`, `VOSET_MAX_VOLTS = 10`, `PLUS5_VOLTS = 5`, `OUT_LIMIT_VOLTS = 12`, `SOFT_KNEE_RATIO = 0.9` (threshold T = 10.8 V).

**Soft limit in SIMD**<br>
The soft limiting needs a `tanh` curve. For SIMD vectors (`float_4`), however, Rack provides no `tanh` function. Minerva uses the rational approximation `s(t) = t·(27 + t²) / (27 + 9t²)` instead. It behaves similarly to `tanh`: at 0 it has slope 1 and then approaches the value 1. At t = 3 it reaches exactly 1 (`tanh(3)` is 0.995). Without limiting it would keep rising beyond that. Therefore `t` is clamped to 3 beforehand; from there on `s = 1`, and the curve passes into the constant value without a jump. The calculation needs no branches (`if`/`else`), so all four channels of a `float_4` are computed simultaneously:

```
a    = abs(x)
t    = min( max(a − T, 0) / (L − T) , 3 )
s    = t·(27 + t²) / (27 + 9t²)
y    = sign(x) · ( min(a, T) + (L − T)·s )
```

Below T, `t = 0` and therefore `y = x`. From `a = T + 3·(L − T) = 14.4 V` on, `y` stays constant at L.

**`Minerva.cpp`**<br>
`struct Minerva : Module` with `processSection(s)` per section. The function determines the channel count, reads the params in blocks of four, and fetches the inputs via `getPolyVoltageSimd` (unpatched = 0 V). It then calls `VoltageProc::process`, passes the result through `VoltageProc::limitOutput` if `outputClip != 0`, and writes it with `setVoltageSimd`. If limiting is off, this call is skipped entirely.

**Param layout (128 params)**<br>
Per section, K, M, and Oset occupy three contiguous blocks of 16 (indices 0–95, via `paramIndex(section, type, ch)`). The +5 Vb switches form two further blocks of 16 **at the end of the enum** (indices 96–127, via `plus5Param(section, ch)`). This keeps the indices of the original 96 params, and thus saved patches, unchanged. A `static_assert` ensures that enum and helper function agree. M range and output limiting occupy no params.

**Inputs/outputs**<br>
The inputs of each section follow the order Va, Vb, M CV, Vc (section B = section A + 4). The outputs are named `OUT_A_OUTPUT` and `OUT_B_OUTPUT`.

**Persistence (JSON)**<br>
Saved are `pendingCh`, `displayedCh`, `channelsA`, `channelsB` (0 = Auto, 1–16 fixed), `mLimited` (bool; if the key is missing: `false`, i.e. unlimited), and `outputClip` (0 = Off, 1 = Soft, 2 = Hard; if the key is missing: 0). All values are clamped on load. The knob values go through the normal Rack params. In addition there is `dcBlock`, an array with two 0/1 entries for A and B. If the key is missing in older patches, the blocker is off. The filter state is not saved and is cleared in `onReset()`.

**DC blocker (engine)**<br>
`DcBlock.hpp` (shared with Elena): `y[n] = x[n] − x[n−1] + R·y[n−1]` with `R = exp(−2π·10 Hz / fs)`, recomputed when the sample rate changes. `Minerva.cpp` obtains the signal part and fixed part separately from `VoltageProc::processParts`; the respective M handling (unlimited or limited) applies to both parts. The blocker filters only the signal part. The fixed part (Oset and `5 V·(1 − M)`) is then added back, and the output limiting is applied afterwards. While the blocker is off, the filter state is carried along (x[n−1] = signal part, y[n−1] = 0), so that no stale state takes effect when it is enabled.

**Namespace**<br>
All local structs live in an anonymous namespace (internal linkage). This way they never collide with identically named structs in other `.cpp` files of the plugin `.so` (one-definition-rule incident with `PanelLabel`, see the Sofonisba documentation). Only `modelMinerva` is exported. `PanelLabel` and the colors come from `PanelLabel.hpp`.

---

<a id="sec6"></a>

## 6. Patch Examples
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

**Inverting (sine, 180°)**<br>
Bipolar sine into Va, K = −1: output = −Va. For a sine this corresponds to a phase shift of 180°. Strictly speaking it is a mirroring of the voltage, not a time shift: a ramp becomes a falling ramp. Arbitrary phase angles are not possible with Minerva.

**Inverted ramp (10 V → 0 V)**<br>
Ramp 0–10 V into Va, K = −1, Oset = 10 V.

**Multiplication (unipolar)**<br>
Vb unpatched, signal into Vc, second signal (0–10 V) into M CV, M knob at 0: output = `Vc · (M_CV / 10)`.

**Ring modulation (four-quadrant multiplication, verified in Rack)**<br>
M range unlimited (default). Vb unpatched, +5 Vb off, audio signal into Vc, bipolar modulator (e.g. ±5 V) into M CV, M knob at 0: output = `Vc · (M_CV / 10)`. The negative half-wave of the modulator inverts Vc. In "limited" mode it would instead be clipped to M = 0.

**Inverting the M control**<br>
Vb unpatched, +5 Vb on, Vc unpatched: output = `5 V·(1 − M)`. M CV then acts inverted.

**Parabola from a ramp (by calculation, not verified in Rack)**<br>
Section A: ramp into Va, K = −1, Oset = 10 V → inverted ramp. Section B: ramp into Vc, inverted ramp into M CV, M knob 0 → `10 V · u · (1 − u)` (u = ramp/10 V). The peak is only 2.5 V, since Minerva has a gain factor of at most 2 per section. The full bell `u²(1−u)²` with a 10 V peak is described in `Elena-Documentation_EN.md`. Since u stays between 0 and 1, the result is the same in both M modes.

---

<a id="sec7"></a>

## 7. Assumptions and Open Points
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

The reference documentation states the equation but leaves details open. The following points are **assumptions** and not substantiated:

**M CV scaling**<br>
The reference documentation says only that knob and CV add together. The assumption is 10 V = full stroke (M 0→1). The constant `M_CV_VOLTS_FULL` is defined in one place in `VoltageProc.hpp`.

**Behavior of M outside 0…1**<br>
The reference documentation says nothing about this. Both variants (unlimited and limited) are assumptions. The real circuit probably limits via the supply voltage rather than hard at M = 1, but this is not substantiated. My decision: the default is **unlimited**, because it opens up more patching possibilities (ring modulation, gain above 1, inversion of Vb or Vc as a computing operation). The limited mode remains selectable via the context menu. **Note on older patches:** Patches built with the earlier fixed limiting and relying on it (e.g. a ramp that runs beyond 10 V) behave differently in the new default. For them, select "M range: Limited".

**Output limiting, level, and impedance**<br>
The reference documentation says nothing about the hardware limits of the circuit, and no schematic is available. The fixed level of ±12 V follows the usual Eurorack supply. The soft threshold is 0.9·L = 10.8 V. Up to this voltage the soft mode passes the signal on unchanged; above it, it gently pushes the signal toward 12 V. A regular 0…10 V signal thus stays completely undistorted. Both values are assumptions. The default is "Off".

**+5 Vb switch on unpatched Vb**<br>
5 V are present. The description states that the switch internally adds a +5 V signal.

---

<a id="sec8"></a>

## 8. Design Decisions
*[↑ Back to table of contents](#toc) · [[#Table of Contents|Obsidian]]*

- **Name Minerva:** after one of the Anguissola sisters, like the other modules of the plugin.
- **Analog voltage math and digital transformation as separate modules:** These are two different functions, hence two modules: Minerva (this file) and `Elena`.
- **Polyphonic with individual values per channel (my requirement):** All 16 channels are individually adjustable, with a shared channel selector for both sections (my decision).
- **Channel selector without a param:** as with Lucia and Europa, so that pickup/catch, history, and preset recall do not affect navigation.
- **Retrofitted +5 Vb switch (my decision):** It was initially omitted on the grounds that Oset replaces it. That is wrong, because Oset shifts the sum, while the switch acts inside the crossfade. It was added **per section and channel**, as an addition at the end of the enum, so that saved patches load unchanged.
- **Right-click "Apply to all 16 channels"** as one undo step, so that not every one of the 16 channels has to be set individually.
- **Channel count "Auto" or fixed 1–16 per section (context menu):** Auto follows the input polyphony, fixed allows a mono signal to be split.
- **M range unlimited by default, switchable (my decision):** This replaces the earlier fixed limiting to 0…1. Unlimited enables four-quadrant multiplication, gain above 1, and inversion. For this the M knob spans −1 to +2 (default 0.5), so that inversion and gain are possible directly at the knob without a second voltage source. The switch is stored only in the JSON (no param). Saved patches still load, but behave differently in the default if they relied on the old limiting.
- **Output limiting optional: Off / Soft / Hard (my decision):** It replaces the earlier decision "no output clipping". The default remains "Off", so that Minerva stays an exactly linear computing block. The limiting serves as a safety net, especially with unlimited M.
- **Fixed level ±12 V, soft threshold fixed at 0.9·L = 10.8 V (my decision):** This keeps the context menu small. With 0.9 instead of 0.8, the complete 0…10 V range stays exactly linear; with 0.8, a regular 10 V signal would have been slightly distorted from 9.6 V on.
- **Soft mode as soft knee instead of plain `tanh`:** A plain `L·tanh(x/L)` already distorts small levels (at 0.5·L only 0.46·L) and would slightly distort multiplication or the parabola. The soft knee stays exactly linear up to T.
- **Module-wide instead of per section (my decision):** M range and output limiting apply to the whole module, which keeps the menu clear.
- **Switchable DC blocker before Oset (my decision):** It can be switched per section in the context menu, is fixed at about 10 Hz, and is off by default. It filters only signal and crossfader. Oset and the +5 Vb part are kept because they are deliberate DC components. It sits before the output limiting. The occasion was a DC component with fed-in audio that became visible in the analyzer.
