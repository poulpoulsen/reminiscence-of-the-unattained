# Anna-Maria – Documentation (Draft, English)

Quad lowpass gate for VCV Rack, heavily inspired by the wonderful Buchla world of modules. Part of the plugin "reminiscence of the unattained", version 2.4.0.
Date: 2026-10-29. Status: draft. Until the listening test in Rack, all statements about sound are **measured on the model**, not heard. The Rack tests of the linear gate part (section 5), of the velocity target "Audio offset" (section 3a) and of the V/Oct resonator (section 3b) are still pending; the four-channel panel is confirmed in Rack.

Anna-Maria is an independent work and has no connection to the makers of the modules it takes its inspiration from; all brands and names belong to their owners.

All numbers come from test programs run against the engine (sample rate 48 kHz, no oversampling, "Reference" mode for the vactrol behavior) unless stated otherwise. Sections 4 to 8 describe the paper model (LOWPASS, and BOTH and VCA with "Gate response = Paper circuit"); what changes with the linear gate part is in section 5. The nonlinearity measurements (section 7) are explicitly labeled "Corrected" or "Reference".

---

## 1. Overview

Anna-Maria emulates a lowpass gate: a lowpass filter and/or a VCA whose opening is controlled by a control voltage (CV) through a vactrol (LED + light-dependent resistor). The model follows the paper *Parker/D'Angelo, DAFx-13* and the Octave simulation `buchla_lp_gate.m` (Stefano D'Angelo, MIT). The C++ engine matches the Octave reference to 1e-13 … 1e-15 (relative, 7 test cases).

Special features of this module:

* **Four separate monophonic channels** (LPG 1–4) plus an ALL column with global elements (31 HP). Each channel has its own knobs and CV inputs for ATT, velocity, THRESHOLD and FEEDBACK and its own switches for MODE, FB NL and DEEP. The ALL output sums the four channels.
* **Control path after the hardware module:** ATT is a control level of 0…10 V, velocity a DC offset on it; a patched CV replaces the respective knob (section 3).
* **VCA and BOTH with a linear gate part** (default): VCA behaves like a linear VCA; BOTH puts a linear VCA and then the lowpass gate in series (section 5). The paper model stays selectable in the menu.
* One **DRIVE** knob for the whole module (level into the model; only affects the nonlinearity) and one **VACTROL** knob (time scale of the vactrol, section 8), both in the ALL column.
* **V/Oct resonator:** every channel has a V/OCT input. When it is patched, a resonator tuned to the note rings in front of the lowpass gate; FEEDBACK then sets its decay time, and the global WET and DRY knobs mix it with the direct signal (section 3b).
* Feedback is either **normalized** (default) or **circuit-accurate** (absolute).
* Model variants in the context menu: vactrol behavior (section 8, default = Reference), NL solver (section 11, default = Corrected), gate part and filter stage of BOTH (section 5), velocity target (section 3a) and an optional saturation (section 7a).

## 2. Signal flow

Per channel (paper model, MODE LOWPASS):

```
Audio IN ─ ÷level factor ─► [ audio path: 2 integrators + differentiator,
                              feedback (optional tanh) ] ─► ×level factor ─► Audio OUT
                                         ▲
                                         │ Rf (vactrol resistance)
ATT (knob or CV) ─┐
                  ├─ + ─ limit 0…10 V ─ + THRESHOLD (offset) ─ shelving ─ curve ─ LED current ─ vactrol (lowpass on Rf)
velocity (knob/CV) ┘
```

* The vactrol resistance Rf determines both the gain of the VCA part and the cutoff frequency of the lowpass.
* Open state: Rf ≈ 1.5 kΩ. Closed: Rf up into the MΩ range.
* **Control level:** `cv = clamp(ATT + velocity, 0 … 10 V)`. THRESHOLD acts as an offset **after** this limit (section 4).
* **Linear gate part (VCA, BOTH):** `g = cv / 10`, smoothed with the vactrol time constants. VCA: `output = g · input`, no filter. BOTH: first `g · input`, then the filter stage (section 5).
* Only the velocity target "Audio offset" is different: the control level is then `clamp(ATT, 0 … 10 V)`, and velocity is added to the signal behind the gate as a DC offset (section 3a).
* **V/Oct (patched, only with a resonance stage):** in front of the filter stage the signal of a tuned resonator is added to the (filtered) direct signal: `input of the stage = DRY · direct path + WET · resonator`. The lowpass gate still decides how long the tone is audible (section 3b).

## 3. Controls and their effect

All knobs and switches except those in the ALL column apply **per channel** (four channels). The ALL and LPG 1–4 columns carry their labels above the elements.

**Channel column (LPG 1–4), rows from the top:**

| Element | Range / default | Short effect |
|---|---|---|
| IN | audio | Input. IN 1 is normalled to IN 2–4 when nothing is patched there. |
| OUT | audio | Channel output (mono). |
| ATT | 0…10 V, 0 V | Control level of the gate. A patched ATT CV replaces the knob. |
| VEL | −10…+10 V, 0 V | Velocity, DC offset on the control level. A patched velocity CV replaces the knob. |
| THRESH | 0…7.5 V, 7.5 V | Gate threshold (paper model). A patched THRESHOLD CV replaces the knob. |
| FEEDBACK | 0…100 %, 0 % | Resonance. A patched FEEDBACK CV replaces the knob. |
| FB NL | off/on, off | Enables the tanh nonlinearity in the feedback. |
| V/OCT | −6…+6 V (0 V = C4) | Pitch of the resonator (section 3b). Without a cable the resonator is off. While patched, FEEDBACK sets the decay time. |
| MODE | LOWPASS / BOTH / VCA, BOTH | Operating mode (section 5). |
| DEEP | off/on, off | Placeholder, no function. |

**ALL column (global):**

| Element | Range / default | Short effect |
|---|---|---|
| ALL OUT | audio | Sum of the four channels, then a soft limiter at ±10 V (linear up to ±7 V). No division by the channel count. |
| MODE | LOWPASS / BOTH / VCA, BOTH | Writes the mode to all four channel switches; afterwards the channel switches can be operated individually again. |
| MODE CV | 0…10 V | When patched, it sets the mode of **all** channels and takes precedence over the switches: below 3.3 V = VCA, below 6.6 V = LOWPASS, above that BOTH. |
| DRIVE | 0–10, 5 | Level into the model (section 7). |
| VACTROL | ×0.1…×10, centre = ×1 | Time scale of the vactrol (section 8). |
| DRY | 0…100 %, 30 % | Level of the direct signal while V/OCT is patched (gain = position², 100 % = level unchanged, 0 = only the resonator is heard). |
| WET | 0…100 %, 90 % | Level of the tuned resonator (gain = 0.045 · position², shown in percent and dB). |
| WET CV | 0…10 V = 0…100 % | Polyphonic: channel 1–4 = LPG 1–4, a mono cable is used for all four. Replaces the WET knob when patched. |

**Context menu (right click):** DEPTH per channel (submenu "Depth" with one slider for all four channels and one each for channels 1–4, default 100 %) and the model settings (section 10). A right click on a channel knob or switch offers "Apply to all 4 channels".

**Effects in detail:**

* **ATT.** The control level of the gate in volts. By default, the ATT knob (or the patched ATT CV) is the source of the control level `cv`; in the paper model the gate opens when `cv` exceeds the threshold, in the linear gate part the gain is `cv / 10`. A patched CV is not scaled (1 V per V); it replaces the knob.
* **VEL (velocity).** A DC offset added to the control level (knob and CV alike). The sum `ATT + velocity` is limited to 0…10 V. A negative velocity shrinks the range ATT can reach; with ATT 5 V and velocity −5 V the gate is closed. Velocity can alternatively be set as a DC offset on the audio signal (section 3a).
* **THRESHOLD.** The **control level in volts at which the gate is half open**, in the paper model. Lower = the gate opens earlier, i.e. already at a small ATT; 7.5 V = the gate opens only at a very high control level. A patched THRESHOLD CV input replaces the knob (1 V per V, limited to 0…7.5 V; positive CV = higher threshold = gate harder to open). Internally: offset and threshold formula, section 4. In the linear VCA THRESHOLD has no effect; in BOTH with the lowpass stage it acts on its filter part.
* **FEEDBACK.** Resonance, see section 6. FEEDBACK CV: 0…10 V corresponds to 0…100 %, limited. Both CV inputs (THRESHOLD, FEEDBACK) act per sample and can be used at audio rate; this costs more CPU because the filter coefficients change in every sample.
* **DEPTH (context menu).** How deep the gate closes **far below the threshold** (leak): 3 V below −31 dB (0 %) to −35 dB (100 %), 6 V below −36 dB to −49 dB (paper VCA). The threshold itself stays put when you turn it. Internally: trim. Default 100 %.
* **FB NL.** Enables the tanh nonlinearity in the feedback (section 7). Works where FEEDBACK works (section 6).
* **MODE.** See section 5.
* **DRIVE.** Level factor: input voltage that corresponds to model amplitude 1 = 5·2^(2·(5−Drive)/5) V, i.e. 20 V (0), 5 V (5), 1.25 V (10). The output is scaled back; **with the NL off, DRIVE does not change the sound**. With NL: more drive = more harmonics and less level (section 7).
* **VACTROL.** Time scale for both vactrol time constants (rise 2.4 ms, fall 50 ms at ×1 = values of the paper). Left = fast, the gate follows the CV; right = slow, envelope-like (section 8). One knob for all channels. It also sets the lag of the linear gate part (assumption, section 5).
* **Channels without a source.** A channel is not computed when neither its own IN nor IN 1 is patched; its output then delivers 0 V. A channel without an OUT cable is skipped as long as ALL is not patched.
* **V/OCT, DRY, WET.** See section 3b.

### 3a. Velocity target: "Control level" or "Audio offset"

In the context menu, **Velocity target** sets where velocity acts. The setting applies to all channels and modes.

* **Control level** (default, hardware model): velocity is added to the control level, `cv = clamp(ATT + velocity, 0 … 10 V)`. The audio signal never sees a DC offset. A negative velocity therefore only makes the signal quieter (in the linear VCA) or shifts where the gate opens; nothing is cut off.
* **Audio offset:** ATT or the ATT CV stays the control level (without velocity). Velocity acts as a DC offset on the signal **after** the gate (i.e. after the attenuation by ATT), at the channel output before the ALL sum:
  * Velocity 0 = no effect; the transition is smooth.
  * The signal is shifted by the velocity voltage and cut at ±10 V (Rack range) in the direction of the shift: below −10 V for negative velocity, above +10 V for positive velocity. A signal already beyond the limit is not cut further.
  * The offset acts only while the gate is open, then at full value: the weight is `clamp(cv · 2, 0, 1)` (0 at ATT 0 V, full from ATT 0.5 V), smoothed with the vactrol times so that the offset does not click. The 0.5 V is an assumption.
  * The offset is a DC component at the output; ALL contains the offsets of all four channels. In LOWPASS and in the paper gate, ATT serves as the measure of opening, not the real filter state.

Examples (gate open): signal after ATT ±2 V, velocity −4 V → −6 / −2 V. Signal ±5 V, velocity −4 V → +1 / −9 V; velocity −7 V → −2 / −10 V (cut); velocity +1 V → +6 / −4 V; velocity +7 V → +10 / +2 V (cut). At ATT 0 V: no offset.

### 3b. V/Oct resonator, DRY and WET

The V/OCT input of each channel (row 6, right of FB NL) tunes a resonator that sits in front of the lowpass gate. It is **not a tuning of the filter**: an earlier version scaled the time of the whole filter, which damped the sound; it has been replaced. The gate itself sounds unchanged.

* **When it acts.** Only with a patched V/OCT cable and only in modes with a resonance stage: LOWPASS, and BOTH with "Gate response = Linear" and "BOTH filter stage = Lowpass". In VCA, in BOTH with the paper stage or the paper gate, and without a cable it is off; the sound is then bit-identical to a module without the resonator.
* **Pitch.** 0 V = C4 (261.63 Hz), range ±6 octaves around C4, limited at the top to 0.45 × sample rate. The resonator is a two-pole with an impulse response normalized to amplitude 1, `rⁿ · sin((n+1)·ω0)`: an impulse at IN gives a decaying sine exactly on the note. The pitch does not fall when the gate closes (no glide) and does not depend on gate or vactrol; the lowpass gate only decides how long and how bright the tone is heard.
* **Decay time.** While V/OCT is patched, FEEDBACK (knob or CV) sets the time in which the resonator decays to −60 dB: `T60 = 0.05 s · 60^FEEDBACK`, i.e. 0 % = 0.05 s, 50 % ≈ 0.39 s, 100 % = 3 s. The gate's own resonance is then off (feedback 0); the FEEDBACK tooltip shows the time.
* **Mix.** The input of the filter stage is `DRY · direct signal + WET · resonator`. WET and DRY are global knobs in the ALL column. The gain grows with the square of the knob position: WET 25 / 35 / 50 / 75 / 90 / 100 % corresponds to about −51 / −45 / −39 / −32 / −29 / −27 dB (default 90 %); DRY 30 % (default) corresponds to −21 dB. The range is tailored to a Rack measurement: a needle pulse adds up to its area in the resonator, and with a pulse height of 10 V it sounded good at about −45 dB (WET ≈ 35 %). Other material (noise burst, sustained tone) has a different level; adjust WET and DRY then.
* **Direct path.** While V/OCT is patched, the direct signal runs through a first-order highpass (corner = half the note frequency, at least 150 Hz) and a first-order lowpass (4 kHz). This takes the DC component and the lows out of a unipolar needle pulse (envelope at IN), which would otherwise run through the open gate as a low "thump", and rounds the vertical edge. The resonator is fed from the unfiltered input.
* **WET CV.** Polyphonic, 0…10 V = 0…100 % of the knob position (same curve as the knob); a mono cable is used for all four channels. A patched cable replaces the WET knob.
* **Limiting.** The resonator output is soft-limited: linear up to 4 (the impulse response has amplitude 1), above that smoothly towards 8. Without the limiter, sustained noise with a long decay time adds up to 70 times the input and triggers the module's protection.
* **Excitation.** Meant for short excitation: pulse, click or noise burst at IN. A sustained tone or sustained noise at IN feeds the resonator continuously.
* **Old patches.** The WET and DRY knobs and the WET CV input are appended at the end; older patches load with the default values. A patch with a V/OCT cable from the earlier version (time scaling) sounds different now, because the resonator has taken its place.

Measurements on the engine (48 kHz): pitch C2 to C8 at T60 0.1 / 0.5 / 3 s: deviation 0.0 cent (C8 +0.2…+0.3 cent). Decay between 50 and 300 ms, measured / target: −150.0 / −150.0, −30.0 / −30.0, −5.0 / −5.0 dB for T60 0.1 / 0.5 / 3 s. Without the resonator the output is bit-identical to the version before (3,385,728 samples across sample rates, modes, NL and vactrol variants). Stability test (pitch modulation ±2 octaves, FM, noise 20 s, T60 3 s): no NaN, peak 16.7 against a module limit of 50. The Rack tests (test plan section 12b) are still pending.

## 4. Gate behavior: THRESHOLD, DEPTH and control level

This section describes the paper model: LOWPASS, and BOTH and VCA with "Gate response = Paper circuit". In BOTH with the linear gate part and the lowpass stage, the threshold values continue to apply to the filter part.

**Operation.** Internally the model has two quantities, *offset* (adds 10 V × offset to the control level) and *trim* (series resistor of the LED). Together they set the gate threshold. The panel instead offers **THRESHOLD** (the threshold in volts) and **DEPTH** (trim, how deep the gate closes, in the context menu). The module calculates the offset from the chosen threshold: `Offset = (T(Trim) + 0.05 V − THRESHOLD) / 10 V`. The THRESHOLD range is 0…7.5 V, default 7.5 V. At DEPTH 100 % (default) the offset at 7.5 V is 0 (the highest reachable threshold there is 7.41 V), so the gate is really closed without a control voltage. At a lower DEPTH the threshold at offset 0 is higher (T(Trim)); THRESHOLD 7.5 V then already means a small offset. If the range is exceeded, the module limits the offset and the tooltip shows the effective threshold. The conversion has been checked against the engine (36 combinations, largest deviation 0.008 V).

Old patches (OFFSET/TRIM, panel with 16 channels) are converted on load; gain and threshold are preserved, only the knobs look different. For patches from before the hardware signal path (ATT as gain ±400 %), ATT is set to 0 V on load; patches with a CV cable sound different because the CV is no longer scaled.

**How the threshold arises.** The gate opens when `control level + 10 V × offset` reaches about 9.12 V (at DEPTH 50 %). The 9.12 V is dictated by the circuit (series resistor and behavior of the LED stage). The offset branch supplies at most 10 V. It follows that `threshold = 9.12 V − 10 V × offset`:

| Offset (internal) | Threshold | Meaning |
|---|---|---|
| 0 | +9.1 V (DEPTH 50 %) | Gate opens only at 9.1 V control level |
| 0.5 | +4.1 V | Middle position |
| 0.9 | +0.1 V | Gate just barely closed at 0 V control level |
| 1.0 | −0.9 V | Gate already open at 0 V (outside the 0…7.5 V range of the knob) |

"Threshold" here is the control level at which the gate is half open, a comparison value and not a signal level. Since the module's control level never falls below 0 V, the panel has no negative threshold any more.

**Where T(DEPTH) comes from.** DEPTH is shown as a knob position of 0…100 % (value 0…1) and is not a voltage. It is the trimmer of the original circuit (TP1, 20 kΩ), which changes the series resistor of the LED. From it the module computes the reference threshold T in volts:

`T = 390 000 / (20 000 · DEPTH + 33 000)`

| Value | Meaning |
|---|---|
| 390 000 | Numerator (V·Ω): according to the paper the zener voltage D1 (3.9 V) times R5 (100 kΩ). It sets the overall scale. |
| 33 000 | R7 (33 kΩ), the fixed part of the series resistor; it remains at DEPTH 0 %. |
| 20 000 · DEPTH | R6 = the trimmer. At DEPTH 0 % it is fully out; at 100 % the full 20 kΩ is in the series resistor. |
| Result T | Control level in volts at which the knee of the LED characteristic is reached (the result is in volts because V·Ω is divided by Ω). |

**Direction:** More DEPTH makes the denominator larger and T smaller: the gate opens at a lower control voltage, i.e. more sensitively. Values: DEPTH 0 % → T = 11.82 V, 50 % → 9.07 V, 100 % → 7.36 V. With the margin of 0.05 V (THR_HALF) this gives 11.86 V, 9.12 V and 7.41 V; these are the thresholds at offset 0. If THRESHOLD is above that, the offset is 0; below it, the difference is added to the control level as an offset.

**Physical background.** In the original, the vactrol resistance does not fall evenly with the control voltage. The LED characteristic has a knee where the current Ia = VB / (R6 + R7) is reached. Below it the LED barely glows and the resistance stays high (rough estimate about 64 kΩ). A few tenths of a volt above it, the resistance drops steeply to a few kΩ (fully open). T is the control voltage at which this knee is reached: the trimmer shifts the knee current, and the module converts the distance between the knee and THRESHOLD into an offset on the control level. In the model the gate is half open at this voltage and fully open about 0.1 V above it (rough estimate, not measured on the component). The assignment of the numbers to the components D1, R5, R6 and R7 is derived from the paper and has not been checked against the source code.

**What THRESHOLD is useful for:**

* **Put the threshold into the envelope.** An envelope of 0…10 V at the ATT CV opens the gate only once it exceeds the threshold. Lower THRESHOLD = open earlier and longer; higher = only the upper part of the envelope opens.
* **Keep the gate open without an envelope.** At THRESHOLD 0 V the half-open point is at control level 0; with some ATT or positive velocity the gate is then open (drone, open filter).
* **Shift the range.** Velocity (negative) shrinks the range of the ATT knob, THRESHOLD sets from which control level the gate opens; together they determine where on the ATT knob the transition lies.
* **Modulated, also at audio rate.** The THRESHOLD CV input (1 V per V, 0…7.5 V) shifts the threshold per sample.

Caution: with THRESHOLD just above 0 V (±0.1 V) the threshold is so close to control level 0 V that the gate stays half open at ATT 0 V. For the selected value, the tooltip says whether the gate is open, half open or closed at ATT 0 V.

The following tables use the internal quantities offset and trim; conversion THRESHOLD = T(Trim) + 0.05 V − 10 V · offset.

Effective threshold (gain 50 % of open gain), measured in VCA mode at 1 kHz:

`CV_threshold ≈ T(Trim) − 10 V · Offset`, with `T = 390 000 / (R6 + R7)`, `R6 = 20 kΩ · Trim`, `R7 = 33 kΩ`.

| Offset \ Trim | 0.0 | 0.5 | 1.0 |
|---|---|---|---|
| 0.00 | never (> 10 V) | 9.12 V | 7.40 V |
| 0.25 | 9.38 V | 6.62 V | 4.90 V |
| 0.50 | 6.88 V | 4.12 V | 2.40 V |
| 0.75 | 4.38 V | 1.62 V | −0.10 V |
| 1.00 | 1.88 V | −0.88 V | −2.60 V |

That is: with offset 0 and trim 0 the gate never opens within ±10 V (on the panel: THRESHOLD +11.9 V, outside the knob range).

**Characteristic curve** (offset 0.5, trim 0.5, VCA, 1 kHz):

| CV [V] | Rf | Gain | dB |
|---|---|---|---|
| −4 | 1352 kΩ | 0.0013 | −57 |
| −2 | 291 kΩ | 0.0083 | −42 |
| 0 | 143 kΩ | 0.0171 | −35 |
| 2 | 90 kΩ | 0.0271 | −31 |
| 4 | 63.5 kΩ | 0.0379 | −28 |
| ≥ 4.5 | 1.5 kΩ | 0.633 | −4 |

* **Below the threshold** (leak region) the gain rises only slowly: between −1 V and 4 V about 1.5–3 dB per volt, steeper below that (−4 → −2 V: 16 dB). The gate is "closed" but not silent: still −35 dB at CV 0 V.
* **The opening is abrupt**: between 10 % and 90 % of the open gain there are only **0.19 V** (4.07…4.26 V). A CV signal that crosses the threshold therefore switches almost like a switch; soft envelopes only act in the leak region and in this narrow transition.
* **Open gain**: VCA 0.633 (−4 dB), lowpass 1.009, independent of offset and trim.
* The CV is internally limited to ±10 V.

Practical consequence: an envelope of 0…10 V at the ATT CV opens the gate at THRESHOLD 4.1 V only after 41 % of its rise and closes it again at 4.1 V (offset 0.5, trim 0.5; on the panel with DEPTH 50 %). With THRESHOLD you decide where.

### 4b. THRESHOLD (offset), DEPTH (trim), ATT and velocity in detail

Measurements: `tests/doku_offset_trim_att.cpp`, output `tests/doku_offset_trim_att_ausgabe.txt`.

**The principle.** Internally the gate has exactly one control quantity, the "effective control voltage". ATT, velocity, THRESHOLD and DEPTH determine how it arises and when the gate opens:

```
ATT (knob or CV) ─┐
                  ├─ + ─► limited to 0…10 V ─► shelving ─┐
velocity (knob/CV) ┘                                       ├─► current ─► LED ─► vactrol resistance Rf ─► filter/VCA
THRESHOLD ─► 10 V × offset (DC component) ────────────────┘            ▲
DEPTH ─► series resistor of the LED (shifts the threshold) ────────────┘
```

The threshold (gate 50 % open) lies at **control level ≈ T(Trim) − 10 V·Offset**, with T = 11.82 V (trim 0), 9.07 V (0.5), 7.36 V (1).

**In this model the gate is more of a switch than a fader.** There are three regions:

| Region | Position relative to threshold | Filter (LOWPASS) | VCA |
|---|---|---|---|
| **open** | above threshold + 0.1 V | cutoff 12.95 kHz, gain 1.0 | gain 0.633 |
| **transition** | ± 0.1 V around the threshold (10…90 % within 0.17…0.22 V) | cutoff from ≈ 400 Hz to 13 kHz | gain from 0.04 to 0.63 |
| **closed (leak)** | below | cutoff 383 Hz (0.5 V below), 296 Hz (2 V), 192 Hz (4 V), 39 Hz (8 V); values for trim 0.5 | −29 dB (0.5 V below), −31 dB (2 V), −35 dB (4 V), −56 dB (8 V) |

That is: in the "closed" region the sound moves only slowly (near the threshold about one octave of cutoff per 4 V, faster further down), and when the threshold is crossed the gate opens in one step. Soft filter sweeps therefore arise mainly from the **vactrol** (rise 2.4 ms / fall 50 ms time constant, section 8), not from a slow CV. A slowly moving CV (LFO, envelope) only becomes "audible" once it crosses the threshold.

**THRESHOLD (internally: offset).**
* Effect on the signal: a pure shift. Measured: the gain at (CV −3 V, offset 0.5), (CV 0 V, offset 0.2) and (CV −6 V, offset 0.8) is identical (0.00457); all have CV + 10·offset = 2 V.
* Offset without a CV cable is a manual control for the gate. At trim 0.5 the gate is open from offset ≈ 0.91 (threshold 9.07 V). Below that it is "closed", and the knob only changes the leak: LOWPASS cutoff 26 Hz (offset 0), 64 Hz (0.2), 139 Hz (0.4), 236 Hz (0.6), 346 Hz (0.8), 404 Hz (0.9); VCA −110 dB (0), −47 dB (0.2), −38 dB (0.4), −33 dB (0.6), −30 dB (0.8), −28 dB (0.9). At 1.0: open.
* With a CV cable, offset shifts the position of the threshold relative to the envelope. High offset = the gate opens earlier in the rise and closes later in the fall (longer open time); low offset = the gate opens only at high CV (short open time).
* Dynamics: offset is a DC quantity and does **not** pass through the shelving filter; fast CV edges, on the other hand, get a short overshoot (section 8).
* The offset voltage is added after the ±10 V limiting of the CV; effectively up to 20 V can act. Velocity, in contrast, acts before the limit (it is part of the control level, section 3).

**DEPTH (internally: trim).**
* Effect 1: shifts the threshold by up to 4.5 V (T = 11.82 → 7.36 V); high trim = the gate opens earlier. At offset 0.5 the threshold is 6.88 / 5.32 / 4.12 / 3.17 / 2.40 V for trim 0 / 0.25 / 0.5 / 0.75 / 1.
* Effect 2: trim also changes the leak. The higher the trim, the more the gate falls off **far** below the threshold: 6 V below the threshold −36 dB (trim 0), −41 dB (0.5), −49 dB (1); LOWPASS cutoff 8 V below the threshold 101 Hz (trim 0), 39 Hz (0.5), 26 Hz (1). Near the threshold the difference is small (1 V below: −29 / −30 / −30 dB; 3 V below: −31 / −33 / −35 dB for trim 0 / 0.5 / 1).
* Effect 3: the transition becomes minimally steeper with high trim (0.22 V at trim 0, 0.17 V at trim 1).
* Rule of thumb on the panel: **THRESHOLD sets the threshold** (to match the height of the envelope), **DEPTH only determines how deep the gate closes below it**. If you have a fixed envelope of 0…10 V, set THRESHOLD so that the threshold lies within the envelope, and use DEPTH to choose how deep the gate closes below it.

**ATT and velocity (control level).**
* Effect on the signal: the control level is `clamp(ATT + velocity, 0 … 10 V)`. The gate opens when this level exceeds the threshold. The control level comes from the knob or, if patched, from the CV (which replaces the knob).
* For an envelope 0…10 V at the ATT CV and threshold 4.12 V (offset 0.5, trim 0.5): the envelope opens the gate fully as soon as it reaches 4.2 V; an envelope with a peak of only 4.12 V sits exactly on the threshold (−13 dB), with a 2.5 V peak the gate no longer opens (−31 dB). With negative velocity the envelope is shifted down: the gate opens later in the rise and closes earlier in the fall (shorter open time). For an exponentially decaying envelope 10 V·e^(−t/200 ms), the geometry (without vactrol) gives open times of about 178 ms (peak 10 V), 120 ms (7.5 V) and 38 ms (5 V).
* ATT 0 V without a CV holds the gate permanently at rest (control level 0 V plus velocity).
* The control level cannot fall below 0 V; the inversion of the old version (CV ATT negative) and the gain above 100 % no longer exist in the Quad panel.
* Control levels above 10 V are limited; measured on the model: 10, 15 and 20 V give the same open gain.
* Order in the module: `ATT (knob or CV) + velocity` → limit to 0…10 V → shelving; the offset voltage (THRESHOLD) is added only **after** the limit. Consequence: a negative velocity does not cancel a low THRESHOLD.

**Typical settings.**

| Goal | Setting |
|---|---|
| Drone, filter fixed at ≈ 400 Hz without envelope | LOWPASS, THRESHOLD just above 0 V (e.g. 0.1 V), DEPTH 50 % |
| Drone, open, without envelope | THRESHOLD 0 V, ATT a few volts |
| Envelope 0…10 V should open in the middle | THRESHOLD 4.1 V, DEPTH 50 % |
| Short ping from the same envelope | raise THRESHOLD or lower velocity: the gate is open only in the upper part |
| Longer opening | lower THRESHOLD |
| Pluck with deep closing (0…10 V envelope) | MODE LOWPASS, DEPTH 100 %, THRESHOLD 7.5 V, Vactrol response = Conductance (section 14); model values for BOTH with State-based in section 4c |

**Open question.** The model switches from "closed" to "open" at the threshold within about 0.2 V (zener behavior of the circuit in the model). Whether the original device switches this steeply has not been checked. In the listening test, a comparison with a video or device using a slow CV (LFO triangle) would be informative: does the original open abruptly as well?

### 4c. Pluck: a deeply closing gate

The gate closes only as deeply as the control level at rest lies below the threshold. A 0…10 V envelope rests at 0 V; at THRESHOLD 4.1 V that is only 4.1 V of distance, the gate stays "ajar" (floor at about −20 dB), and after the short burst mainly the bass remains. That sounds like a low-frequency click, not like a pluck. In the Quad panel the control level cannot fall below 0 V (ATT 0…10 V plus velocity, limited to 0…10 V); the rest position is therefore 0 V, and the distance to the threshold is the threshold itself. A high THRESHOLD at DEPTH 100 % then gives the largest distance.

Measured on the model (paper variant, BOTH, State-based, noise, envelope 0…10 V, decay 100 ms; level in 10 ms windows): DEPTH 100 %, THRESHOLD 7.4 V gives rest −76 dB, peak −4 dB, level after 20 / 50 / 100 / 200 / 400 ms: 0 / −9 / −13 / −21 / −36 dB. The default 4.1 V (offset "medium") leaves the gate "ajar" (rest −21 dB).

A slow vactrol (VACTROL to the right) makes the tail longer but lowers the peak, because the gate no longer opens fully while the envelope is short. A fast envelope together with a fast vactrol (VACTROL left) sounds harder. Note: the numbers are model values, not listening results; they apply to the paper gate part. In the linear gate part of BOTH, the gate closes completely at ATT 0 V. The measured values above apply to BOTH with State-based; the pluck found in the listening test (section 14, example 1) instead runs in LOWPASS with Vactrol response = Conductance at THRESHOLD 7.5 V and DEPTH 100 %, where the rest position is also 0 V and the distance to the threshold is about 7.4 V.

### 4a. A closed gate is not silent

As with the original, the input signal also appears at the output with the gate closed, filtered or attenuated (offset 0.5, trim 0.5, feedback 0, gain in dB):

| Mode | CV 0 V | CV −4 V | CV −10 V |
|---|---|---|---|
| LOWPASS | 100 Hz −1.6 / 1 kHz −17 / 5 kHz −41 | −17 / −51 / −80 | −68 / −107 / −136 |
| BOTH | −0.6 / −7 / −21 | −7.5 / −28 / −53 | −42 / −80 / −109 |
| VCA | all frequencies ≈ −35 | −55 … −69 | −91 … −125 |

Only in LOWPASS and BOTH does a lowpass-filtered signal remain audible; VCA attenuates broadband. Whether the values correspond to the original device has not been checked (no measurement data from the original).

This table applies to the paper model. With the linear gate part (default in VCA and BOTH) the gate closes completely in VCA at ATT 0 V (gain 0); in BOTH the gain is `cv / 10` times the gain of the filter stage.

## 5. Modes

The mode per channel (MODE switch, or global MODE or MODE CV) determines the circuit. LOWPASS is always the paper model. For VCA and BOTH, the context menu **Gate response (VCA, BOTH)** chooses:

* **Linear** (default): the gate is a linear VCA. The reference documentation of the hardware module describes the gate mode as a linear VCA and the combined mode as a combination of a linear gate and a lowpass; the linear part is the default for that reason.
* **Paper circuit**: the paper model (vactrol divider with a knee at THRESHOLD) as in sections 4 to 8; switches steeply, and in VCA only 0.63 open gain.

**Linear gate part.** `g = clamp(cv, 0 … 10 V) / 10`, i.e. 10 V = gain 1 (assumption: the reference documentation gives no numbers). The gain is smoothed with the vactrol time constants (rise 2.4 ms, fall 50 ms, times the VACTROL factor); the direction follows the state (if g rises, the fast constant applies). Whether the hardware module has vactrol lag in the gate mode is not documented (assumption).

| Mode | Gate response = Linear (default) | Gate response = Paper circuit |
|---|---|---|
| LOWPASS | Lowpass with C3 = 4.7 nF, Ra = 5 MΩ (paper gate), unchanged | same mode |
| BOTH | First a linear VCA, then the filter stage (menu "BOTH filter stage") | Lowpass and VCA at once (C3 = 0, Ra = 5 MΩ), as before the change |
| VCA | Pure linear VCA: `output = g · input`, no filter | Pure VCA (C3 = 0, Ra = 5 kΩ), open gain 0.633 |

Sonic character:

* **LOWPASS**: the level stays near 1 in the passband, only the highs close. The gate acts like a filter with CV-controlled cutoff (404 Hz at control level 4 V in the example of section 4). Feedback is always active here.
* **BOTH** (linear): VCA and lowpass gate in series. The loudness follows the control level linearly, and in addition the filter closes the highs. Menu **BOTH filter stage**: *Lowpass* (default) uses the filter stage of LOWPASS; THRESHOLD, DEPTH, FEEDBACK and FB NL act there as in LOWPASS, and the filter rings quietly after the gate closes. *Paper BOTH* uses C3 = 0: the filter hardly filters and has no resonance (FEEDBACK and FB NL without effect); BOTH is then the paper BOTH with a linear gate in front. The lowpass stage is the default because the author finds it more sensible and wants to stay flexible.
* **VCA** (linear): no filter, level only; THRESHOLD, DEPTH, FEEDBACK and FB NL have no effect. The loudness is exactly `cv / 10`.

Measurements on the model (48 kHz):

* VCA linear, sine 100 Hz: control level 0 / 0.5 / 1 / 2.5 / 5 / 7.5 / 10 V = gain 0 / 0.05 / 0.1 / 0.25 / 0.5 / 0.75 / 1.0, independent of THRESHOLD, DEPTH and FEEDBACK. Time (step 0 ↔ 10 V, VACTROL ×1): up to 63 % after 2.40 ms, 95 % after 7.19 ms; down to 37 % after 50.0 ms, 5 % after 149.8 ms.
* BOTH linear, stationary (THRESHOLD 7.5 V, DEPTH 100 %, FEEDBACK 0, sine 100 Hz), gain for control level 1 / 3 / 5 / 7 / 10 V: lowpass stage 0.019 / 0.213 / 0.443 / 0.659 / 0.999; paper stage 0.051 / 0.266 / 0.474 / 0.679 / 0.999; for comparison paper BOTH without the linear part 0.505 / 0.886 / 0.949 / 0.969 / 0.999. BOTH linear with the lowpass stage = LOWPASS × `cv / 10`.
* Ringing of BOTH after the gate closes (ping: 20 ms burst 600 Hz amplitude 1, then input 0 and gate closed, THRESHOLD 7.5 V, FEEDBACK 90 %): lowpass stage 0.0081 (20–50 ms) and 0.0025 (100–200 ms); paper stage 0.0000. The ringing is quiet (about 0.8 % of the burst amplitude).
* Stability of BOTH linear (lowpass stage, FEEDBACK 100 %, normalized, NL off): peak 33.7 against 43.7 in LOWPASS for the same input; the module's protection triggers at 50. No NaN.

Switching modes during operation: LOWPASS ↔ BOTH without a jump (deviation 6e-4, DC gain of the lowpass stage). Returning from the pure linear VCA to LOWPASS or BOTH starts the filter with empty states (a short transient is possible). Switching the filter stage during operation produces no NaN and no jump above the normal signal.

Paper values (for orientation, Gate response = Paper circuit):

| Mode | Circuit | Open gain (20 Hz) | Cutoff open (−3 dB) | at control level 4.0 V (Rf 63.5 kΩ) | at 3.0 V (Rf 74.6 kΩ) |
|---|---|---|---|---|---|
| LOWPASS | lowpass with C3 = 4.7 nF, Ra = 5 MΩ | 1.009 | 12.95 kHz | gain 0.98, f = 404 Hz | 346 Hz |
| BOTH | lowpass and VCA at once (C3 = 0, Ra = 5 MΩ) | 1.009 | 19.4 kHz | gain 0.98, f = 1188 Hz | 1014 Hz |
| VCA | pure VCA (C3 = 0, Ra = 5 kΩ) | 0.639 | above 20 kHz | gain 0.038 | 0.033 |

Open question: whether the middle position of the hardware module is a lowpass with quiet resonance or (according to one possible reading of the circuit) a branch without the filter capacitor cannot be decided with certainty; that is why the filter stage of BOTH is selectable.

## 6. Feedback (resonance)

### 6.0 What FEEDBACK does

In LOWPASS mode, FEEDBACK feeds a portion of the output back to the filter input through a differentiator. This creates a **resonance** (emphasis) around the cutoff frequency: without feedback the filter is a soft lowpass, with feedback it "sings" at one point of the spectrum.

* **Where FEEDBACK works:** in LOWPASS always. In BOTH only with "Gate response = Linear" and "BOTH filter stage = Lowpass" (both default). **In VCA, FEEDBACK has no effect** (and neither has FB NL), nor in BOTH with the paper stage or the paper gate (there the capacitor C3 of the feedback path is switched off in the model, C3 = 0). The knob position stays saved in the patch and is effective again when you switch back. Measured: BOTH paper and VCA with FEEDBACK 90 % versus 0 % give the same peak. The tooltip points this out.
* **With V/OCT patched, FEEDBACK changes its meaning:** it then sets the decay time of the resonator (section 3b), and the gate's own resonance is off.
* **The peak lies well above the cutoff frequency**, at roughly 2.3 to 2.9 times the −3 dB frequency without feedback (example below: cutoff 407 Hz, peak at 951…1162 Hz). This is the behavior of the model from the paper, not that of a classic resonant lowpass with the peak at the cutoff.
* **Low frequencies remain almost untouched** (−0.2 dB at 50 Hz). The feedback only raises the region around the peak.
* **The peak travels with the gate.** When the gate opens, the cutoff rises and the resonance moves up with it: the typical "wah" or "ping" sound. With a wide-open gate (cutoff 13 kHz) the peak is at about 19 kHz and barely audible.
* **When excited, the filter rings.** An impulse, a gate edge or a noise burst makes the filter ring out at the resonance frequency, the longer the closer to 100 %. At 100 % it no longer decays.
* **FB NL off** (default): the signal stays a sine, only level and phase change. **FB NL on**: a tanh limits the feedback, distortion arises, especially at high DRIVE and with signals near the resonance frequency (section 7).
* **In practice:** for "ping" or "wah" use 75–90 %, a short gate and an input frequency near the peak, or noise. From about 95 % the resonance becomes an almost tonal whistle that moves with the gate opening.

### 6.1 Measurements

LOWPASS mode only. The resonance peak lies above the −3 dB frequency without feedback.

Operating point CV 4.05 V, f(−3 dB, without feedback) = 407 Hz, **normalized**:

| FEEDBACK | Peak | Peak frequency |
|---|---|---|
| 0 … 50 % | none (−0.2…−0.1 dB) | – |
| 75 % | +2.7 dB | 951 Hz |
| 90 % | +10.0 dB | 1105 Hz |
| 95 % | +15.8 dB | 1162 Hz |
| 99 % | +28.6 dB | 1162 Hz |

* Up to 50 % little happens sonically; the resonance only arises from 75 %. The stretch between 75 % and 99 % covers 26 dB.
* At 100 % the model sits exactly on the stability limit: an impulse does not decay, a constant oscillation of small amplitude remains (impulse 1.0 → peak 0.145). The module does not run away (protection against overflow is built in anyway).
* The resonance frequency follows the CV with the gate.

**Normalized vs. circuit-accurate (context menu "Resonance"):**

* *Normalized*: the knob 0…100 % is scaled to the respective stability limit; the same knob position = the same resonance strength, independent of the gate state.
* *Circuit-accurate*: the knob 0…100 % corresponds to the absolute feedback 0…1.472. With the gate open (Rf small) practically the same as the normalized mode; with a nearly closed gate the resonance becomes weaker. Example FEEDBACK 90 %:

| CV | Rf | normalized | absolute |
|---|---|---|---|
| 6.0 / 5.0 / 4.5 | 1.5 kΩ | 9.7 dB | 9.6 dB |
| 4.0 | 63.5 kΩ | 10.0 dB | 9.4 dB |
| 3.5 | 68.7 kΩ | 10.0 dB | 9.2 dB |
| 0.0 | 143 kΩ | 9.8 dB | 8.4 dB |
| −2.0 | 291 kΩ | 9.3 dB | 6.8 dB |
| −4.0 | 1352 kΩ | 6.8 dB | −0.4 dB (no resonance) |

  In practice: as long as the gate is open or half open, both modes are nearly identical. The difference only appears in the almost-closed region; there the absolute mode "swallows" the resonance.

## 7. Nonlinearity (FB NL) and DRIVE

With FB NL = on, the feedback passes through a tanh limiter. It only acts when the signal is large enough; DRIVE determines how large the signal is in the model (section 3).

Harmonic content (root of the sum of harmonics 2…8, relative to the fundamental), input ±5 V, FEEDBACK 90 %, **NL solver "Corrected"**:

| CV | f | Drive 0 | 2.5 | 5 | 7.5 | 10 | Fundamental Drive 0 / 10 |
|---|---|---|---|---|---|---|---|
| 8.0 V (open) | 220 Hz | 0.0002 | 0.0007 | 0.0021 | 0.0038 | 0.0041 | 5.00 / 5.00 V |
| 3.0 V | 500 Hz | 0.0228 | 0.0643 | 0.1079 | 0.1199 | 0.1040 | 6.37 / 3.89 V |
| 4.0 V | 500 Hz | 0.0282 | 0.0754 | 0.1196 | 0.1282 | 0.1066 | 5.91 / 4.08 V |
| 4.0 V | 2000 Hz | 0.0007 | 0.0025 | 0.0090 | 0.0250 | 0.0460 | 2.26 / 1.70 V |

(For comparison, the "Reference" solver: 0.0031 / 0.0122 / 0.0391 / 0.0724 / 0.0811 for CV 3.0 V, 500 Hz.)

What you hear:

* **With the gate open and no resonance peak at the signal**, the NL stays practically inaudible (harmonic content < 0.5 %).
* **At the resonance peak** (gate half open, signal near the resonance frequency) clear distortion arises (up to ~13 % harmonics) and the level drops (fundamental 5.9 → 4.1 V).
* At 500 Hz the harmonic content rises with DRIVE up to about 7.5 and then falls again; at 2 kHz it keeps rising up to Drive 10.
* DRIVE without NL: **no** effect.
* With NL and FEEDBACK 100 % (solver "Corrected", CV 4 V) a small oscillation remains after an impulse (peak in the output after 5 s: 0.094 V at Drive 0, 0.030 V at Drive 5, 0.008 V at Drive 10). With the "Reference" solver it decays (0 V).

## 7a. Saturation (addition, menu "Saturation")

Not part of the paper model. Background: on scope recordings of the original, a sine has a slightly flattened peak, but the model has no saturation in the audio path. With "Saturation = On" the model output signal (in model units) runs through `tanh`, then is scaled back to volts. Default is Off.

* The intensity is determined by **DRIVE** (level in the model, section 3). The small-signal gain stays 1; only the shape of large amplitudes changes. It works in all modes and also at feedback 0.
* Measured for a sine with 5 V peak when the model has gain 1 (LOWPASS open 1.009):

| DRIVE | Model amplitude | Harmonics 3…9 / fundamental | Output peak |
|---|---|---|---|
| 0 | 0.25 | 0.5 % | 4.90 V |
| 2.5 | 0.50 | 2.0 % | 4.62 V |
| 5 | 1.00 | 6.7 % | 3.81 V |
| 7.5 | 2.00 | 17.3 % | 2.41 V |
| 10 | 4.00 | 30.0 % | 1.25 V |

* Only odd harmonics arise (symmetric curve).
* **The output level falls with rising DRIVE**: the output peak is limited to the level factor (20 / 5 / 1.25 V at Drive 0 / 5 / 10). At high drive you may therefore want to raise the level afterwards.
* In VCA mode (open gain 0.63) and with a partially closed gate the amplitude in the model is smaller and the saturation correspondingly weaker: it acts most strongly with an open gate and a loud signal.
* **Listening test (Kinovice, Rack):** the warm sound of a sine appears when DRIVE is at about 75 % (≈ 7.5), i.e. at roughly 17 % harmonics (table). At Drive 5 the effect is too weak.
* The stage is an addition, not a measurement of the original. The measured effect corresponds to the computational model, not necessarily to the sound of the device.

## 8. Timing: vactrol

Time constants of the model: opening 2.4 ms, closing 50 ms (lowpass on the resistance Rf). The **VACTROL** knob (one knob for all channels) scales both values by a factor of ×0.1…×10 (centre ×1 = values of the paper, verified bit-accurately against the reference). It also sets the lag of the linear gate part (section 5). Measured (State-based, paper VCA, step −10 → +10 V): rise 10 → 90 % at ×0.3 = 3.2 ms, ×1 = 10.5 ms, ×3 = 31.6 ms. The tables in this section apply to ×1.

Context menu **Vactrol response**:

* *Reference (as published)*: like the Octave reference. The reference picks the fast constant only if the target Rf is falling in that sample. With a gate-like CV this happens only in the first sample; afterwards the vactrol opens with the slow constant.
* *State-based*: direction from comparing the target with the current vactrol state. Behaves like a real LPG.

Gate step −10 V → +10 V (VCA, DC at the input):

| | Reference | State-based |
|---|---|---|
| Rise 10 → 90 % | **219.7 ms** | **10.5 ms** |
| Closing 90 → 10 % | 0.1 ms | 0.1 ms |

The very fast closing on the step from +10 V to −10 V comes from the shelving filter in the control path: the steep CV edge overshoots, and the vactrol is briefly driven to an extremely high resistance. With a gentler fall of the CV it closes more slowly (table below).

Short gate pulses (CV −10 → +10 V), peak gain reached relative to open:

| Length | Reference | State-based |
|---|---|---|
| 5 ms | 0.000 | 0.001 |
| 10 ms | 0.000 | 0.007 |
| 20 ms | 0.000 | 0.32 |
| 50 ms | 0.000 | 1.00 |
| 100 ms | 0.001 | 1.00 |
| 200 ms | 0.006 | – (1.00) |

With the Reference setting the module therefore practically does not respond to triggers/short gates (the gate has to be far longer than 200 ms). For pluck sounds please choose "State-based" or "Conductance"; "Conductance" provides the tail with which the pluck in section 14 was found.

Closing time after a 100 ms gate (+10 V), State-based, depending on the rest CV (time to 50 % / 10 % of the peak gain):

| Rest CV | 50 % | 10 % | Final value (rel.) |
|---|---|---|---|
| −10 V | 0.0 ms | 0.1 ms | 0.000 |
| −4 V | 0.0 ms | 0.1 ms | 0.003 |
| −1 V | 0.4 ms | 8.8 ms | 0.020 |
| 1 V | 1.4 ms | 19.1 ms | 0.035 |
| 2 V | 2.0 ms | 25.4 ms | 0.043 |
| 3 V | 2.5 ms | 33.0 ms | 0.051 |
| 3.5 V | 2.8 ms | 37.4 ms | 0.056 |

The "decay" is therefore not a free parameter but depends on **where** the CV falls back to. A low rest CV (≤ −4 V) switches off practically instantly; a rest CV just below the threshold (3…3.5 V) produces a longer, softer decay (33…37 ms to −20 dB). THRESHOLD changes the position of the rest CV relative to the threshold.

## 9. Channels and inputs

* Four separate monophonic channels; each has its own state, its own knobs and its own switches. IN 1 is normalled to IN 2–4 when nothing is patched there.
* A channel without an audio source (neither its own IN nor IN 1) is not computed; its output sits at 0 V. A channel without an OUT cable is skipped as long as ALL is not patched. This saves up to three quarters of the CPU time. When a channel is reactivated it starts in the settled state (no vactrol settling).
* Old patches of the polyphonic version (16 channels) land on channels 1–4; the param blocks of the old version stay in place so the indices match. Their cables on AUDIO and CV sit on channel 1; only channel 1 of the polyphony is read (poly inputs are no longer fanned out).

## 10. Context menu

The knobs are on the panel; the context menu contains DEPTH and the model settings.

| Entry | Options (default bold) | Effect |
|---|---|---|
| Depth | Slider "All channels" and one each for channels 1–4 (default 100 %) | How deep the gate closes below the threshold (section 3) |
| Resonance | **Normalized** / Circuit-accurate | Section 6 |
| Vactrol response | **Reference** / State-based / Conductance | Section 8 |
| NL solver | Reference / **Corrected** | Section 11 (model limits) |
| Circuit variant | **Paper** / Reproduction board | Component values after the paper or after a variant with changed component values (section 11a) |
| Gate response (VCA, BOTH) | **Linear** / Paper circuit | Linear gate part or paper model (section 5) |
| Velocity target | **Control level** / Audio offset | Where velocity acts (section 3a) |
| BOTH filter stage | **Lowpass** / Paper BOTH | Filter stage of BOTH, only with the linear gate part (section 5) |
| Saturation | **Off** / On | Addition, not in the paper model (section 7a) |
| Oversampling | **Off** / 2x | 2× oversampling of the audio path; doubles the CPU time, may reduce aliasing caused by the nonlinearity (to be checked in the listening test) |

Settings are saved in the patch. Older patches without the keys for Gate response, BOTH filter stage and Velocity target load with the default values (Linear, Lowpass, Control level).

## 11. Model limits and peculiarities of the reference

Three points were found when recomputing the reference:

1. **Paper equation (20)** is wrong in the PDF; the reference code is correct. The code is implemented.
2. **Vactrol direction**: see section 8. With a gate CV the reference opens in about 220 ms. Available as the option "State-based".
3. **NL solver**: `yxden` contains `+a1·d1·t2m1`; the derivation gives `−a1·d1·t2m1`. Consequence: the NL branch does **not** merge into the linear branch for small signals. Measured: deviation NL vs. linear at 1 µV: **52 %** (Reference) versus 1.4·10⁻¹² (Corrected). Available as the option "Corrected" and, since the listening test, the default in the module.

Default: NL solver = **Corrected** (decision after the listening test), Vactrol response = Reference (still open). The engine itself (`AnnaMariaLpg`) still has the reference as its preset and is verified bit-accurately against Octave that way; the module sets Corrected explicitly. Further limits:

* The parameters of the vactrol model are taken from the paper, not measured.
* Component tolerances and temperature are not modeled.
* At very high frequencies near Nyquist the trapezoidal integration takes effect; 2× oversampling helps.

## 11a. Comparison with a component variant

Source: reference documentation of a circuit variant of the lowpass gate. The values were read from it and not checked against measurement data.

* **Differences compared with the paper:** R8 = 10 k instead of 4.7 k; in VCA mode shunt resistor R16 = 10 k instead of 5 k and an input stage with gain 2 (R11 = R12 = 15 k). The original also has a DEEP switch that makes the gate close deeper at the same knob position (roughly like a THRESHOLD a few volts higher). DEEP is not modeled; an exact model would need the LED forward voltage.
* **Menu "Circuit variant = Reproduction board"** sets R8 = 10 k, VCA shunt 10 k and input gain 2 in VCA mode. Default stays Paper.

Measurements (sine 1 kHz, THRESHOLD 4.1 V, DEPTH 50 %, State-based, gain in dB):

| Mode | Variant | CV −4 V | 0 V | 4 V | 8 V |
|---|---|---|---|---|---|
| LOWPASS | Paper | −27.3 | −6.8 | −2.5 | 0.0 |
| LOWPASS | Reproduction | −40.2 | −13.9 | −7.4 | 0.0 |
| BOTH | Paper | −50.7 | −16.8 | −8.9 | 0.0 |
| BOTH | Reproduction | −66.4 | −28.9 | −17.9 | 0.0 |
| VCA | Paper | −57.1 | −35.3 | −28.4 | −4.0 |
| VCA | Reproduction | −58.2 | −31.3 | −24.2 | +3.8 |

The threshold does not change; the closed gate becomes 6…12 dB deeper in LOWPASS/BOTH. In VCA mode open is +3.8 dB instead of −4 dB (mind the level in that mode).

**Pluck without new controls** (Paper variant, BOTH, State-based, noise, envelope 0…10 V, decay 100 ms): DEPTH 100 %, THRESHOLD 7.4 V (corresponds to offset at its lower stop) gives rest −76 dB, peak −4 dB, level after 20 / 50 / 100 / 200 / 400 ms: 0 / −9 / −13 / −21 / −36 dB. The default 4.1 V (offset "medium") leaves the gate "ajar" (rest −21 dB).

## 12. CPU load

Engine (measuring machine, double, -O2, one channel): 70–90 ns per sample with a moving CV, 20–26 ns with a constant CV (cache for the control path and audio coefficients, bit-identical to the original). Measured in Rack: one active channel 0.2–0.4 %, ALL patched (four channels) 1.5 % (about 0.35 % per channel with a moving CV). With 2× oversampling about double. The CV inputs for THRESHOLD and FEEDBACK at audio rate cost more because the coefficients change in every sample. The linear gate part in VCA is cheaper than the paper model.

## 13. License and sources

Plugin: GPL-3.0-or-later. The formulas follow `buchla_lp_gate.m` (Copyright 2014 Stefano D'Angelo, MIT; notice in `AnnaMariaLpg.hpp`) and the paper *Parker, D'Angelo: A Digital Model of the Buchla Lowpass-Gate*, DAFx-13.

## 14. Patch examples (to be verified in the listening test)

1. **Pluck / marimba** (found in the listening test): MODE LOWPASS, THRESHOLD 7.5 V, DEPTH 100 % (default), Vactrol response = Conductance, VACTROL ×1. Envelope into the ATT CV, VEL 0: delay 700 ms, attack 5 ms, hold 0 ms, release 200 ms, peak 10 V (generated with the envelope module Europa). A sawtooth (100 Hz) or a triangle (2 kHz, sounds bell- or marimba-like) at IN. DRIVE 1.9, FEEDBACK 63–80 %, FB NL on, Resonance = Normalized, NL solver = Corrected, Circuit variant = Paper, Saturation = On, Oversampling off. "Gate response" and "BOTH filter stage" act only in BOTH and VCA and have no meaning here. Because THRESHOLD is 7.5 V, only the upper part of the envelope opens the gate (from about 7.4 V control level); the decay comes from the tail of the vactrol model (section 8). For a softer decay turn VACTROL to the right, for a harder one to the left. Variant with BOTH: see section 4c (model values).
2. **Linear VCA**: MODE VCA, envelope or knob at ATT: the loudness follows exactly `cv / 10`; velocity shifts the range (section 3).
3. **Filter drone**: MODE LOWPASS, THRESHOLD just above 0 V (e.g. 0.1 V), so that the gate sits at the desired cutoff without an envelope; LFO into the ATT CV with a small amplitude; FEEDBACK 75–90 %.
4. **Resonance distortion**: MODE LOWPASS, FB NL on, FEEDBACK 90 %, DRIVE 6–8, gate half open (control level at the threshold).
5. **DC offset shift**: Velocity target = Audio offset, ATT 10 V, signal ±5 V at IN, velocity from 0 to −4 V: the signal slides down (+1 / −9 V), with further lowering it is cut at −10 V at the bottom (section 3a).
6. **Tuned ping**: MODE LOWPASS, V/OCT from a keyboard or sequencer, a short envelope or impulse at IN, ATT 10 V (gate open, THRESHOLD 7.5 V), FEEDBACK 50 % (≈ 0.4 s), WET 35–90 %, DRY 30 %. Starting values, not yet confirmed in the listening test.

## 15. Decisions (log)

Decisions by the user (Kinovice); the order is chronological.

* **Quad layout (2026-10-14).** Four monophonic channels instead of 16 polyphonic ones, 31 HP, ALL column (ALL OUT, global MODE, MODE CV, DRIVE, VACTROL, DEEP). Order per channel: IN | OUT, ATT | VEL, CV | CV, THRESH | FEEDBACK, CV | CV, FB NL, MODE | DEEP. ALL: sum first, then soft-limit at ±10 V (no division by 4, which was too quiet). IN 1 is passed on to IN 2–4.
* **Signal path after the hardware module (2026-10-14).** ATT = control level 0…10 V, velocity = DC offset on it, sum limited to 0…10 V; a patched CV replaces the respective knob (ATT and velocity). THRESHOLD CV 1 V/V over 0…7.5 V, FEEDBACK CV 0…10 V = 0…100 %.
* **Linear VCA (2026-10-15).** According to the reference documentation, VCA should be a linear VCA (paper circuit ineffective there). BOTH gets the linear part as well.
* **Order in BOTH (2026-10-15).** First VCA, then LPG, both in series (matches a statement in a video about the hardware).
* **Filter stage of BOTH (2026-10-15).** Selectable in the menu ("stay flexible"), default Lowpass ("more sensible for me").
* **Velocity target (2026-10-15).** The menu "Velocity target" with "Control level" (default) and "Audio offset". Rule for "Audio offset": velocity as a DC offset after the gate/ATT; velocity 0 = no effect; cut in the direction of the shift at ±10 V; the offset acts only while the gate is open, then at full value (rejected: offset before the gate, fixed lower limit 0 V or −5 V, cut at +5 V or +10 V at the input).
* **Documentation (2026-09-30).** The documentation is public in the repo; original names, model numbers, links and verbatim quotes from manufacturer manuals are avoided, sources only as "reference documentation".
* **V/Oct as a tuned resonator (2026-10-24).** Listening sample 04 (resonator in front of the LPG) is the favourite ("sounds different, but round"); it replaces the time scaling of the filter, which damped the sound. WET as a global knob; a polyphonic WET CV input (4 channels) replaces the knob when patched. Level: Rack measurement with a pulse at IN: 30 V peak at WET −6 dB; good sound at 10 V and −45 dB; the range is tailored to that region. The menu "Pitch level compensation" is dropped because the level is flat across pitch on its own.
* **Panel (2026-10-24).** The global DEEP switch in the ALL column is gone from the panel (room for WET, DRY and WET CV); the parameters stay, and the channel DEEP switches remain placeholders.
