# Anna-Maria – Documentation (Draft, English)

Anna-Maria is a quad low-pass gate for VCV Rack, strongly inspired by the wonderful world of Buchla modules. It is part of the plugin "reminiscence of the unattained", version 2.4.0.
Status as of: 2026-10-29. Status: draft.

Until the listening test in Rack, all statements about sound are measured on the model, not heard. Not yet tested in Rack are the linear gate path (section 5), the velocity target "Audio offset" (section 3a), and the tuned resonator with V/OCT, WET and DRY (section 6a). I built the resonator in Rack and tried it with a pulse and with noise; the test series is still pending. The four-channel panel is confirmed in Rack.

Anna-Maria is an independent work and has no connection with the manufacturers of the original modules. All trademarks and names belong to their owners.

Unless stated otherwise, all numerical values come from test programs run against the engine. They assume 48 kHz, no oversampling, and the vactrol mode "Reference". Sections 4 to 8 describe the Paper model, that is LOWPASS as well as BOTH and VCA with "Gate response = Paper circuit". What changes with the linear gate path is described in section 5. The measurements on the nonlinearity (section 7) name the solver, "Corrected" or "Reference", in each case.

---

## 1. Overview

Anna-Maria models a low-pass gate. It consists of a low-pass filter and/or a VCA. A control voltage (CV) sets, via a vactrol (an LED with a light-dependent resistor), how far both open. The model follows the paper *Parker/D'Angelo, DAFx-13* and the Octave simulation `buchla_lp_gate.m` (Stefano D'Angelo, MIT). In seven test cases the C++ engine agrees with the Octave reference to 1e-13 to 1e-15 (relative).

Special features of this module:

* **Four separate, monophonic channels** (LPG 1–4) and an ALL column with global elements, 31 HP in total. Each channel has its own knobs and CV inputs for ATT, velocity, THRESHOLD and FEEDBACK, as well as its own switches for MODE, FB NL and DEEP. The ALL output sums the four channels.
* **Control path after the hardware original:** ATT is a control level of 0…10 V, and velocity is a DC offset on top of it. A patched CV replaces the respective knob (section 3).
* **VCA and BOTH with a linear gate path** (default): VCA behaves like a linear VCA. BOTH puts the linear VCA and then the low-pass gate in series (section 5). The Paper model remains selectable in the menu.
* **DRIVE and VACTROL** for the whole module, both in the ALL column. DRIVE sets the level into the model and only affects the nonlinearity. VACTROL is the time factor of the vactrol (section 8).
* **Feedback** either normalized (default) or circuit-accurate (absolute).
* **V/OCT input, WET and DRY** (section 6a): Each channel has a V/OCT input. It tunes a resonator in front of the gate to a note, so a ping or pluck can be played at a pitch. A global WET knob with a polyphonic CV input sets the level of the resonator. A global DRY knob sets the level of the direct signal. With V/OCT patched, the direct signal passes through a fixed high-pass and low-pass.
* **Model variants in the context menu:**
  * Vactrol behavior (section 8, default = Reference)
  * NL solver (section 11, default = Corrected)
  * Gate path and filter stage of BOTH (section 5)
  * Velocity target (section 3a)
  * optional saturation (section 7a)

## 2. Signal flow

Each channel has two paths. The **audio path** carries the signal from IN to OUT. The **control path** turns ATT, velocity, THRESHOLD and DEPTH into the quantity with which the gate opens and closes. Both diagrams apply to all modes. Which stages act in which mode is shown in the table below the diagram.

**Audio path (one channel):**

```
 IN  (IN 1 is normalized to IN 2–4)
                        │
                        ▼
┌──────────────────────────────────────────────┐
│ 1  ÷ level factor (DRIVE)                    │
└───────────────────────┬──────────────────────┘
                        ▼
┌──────────────────────────────────────────────┐
│ 2  Linear gate part: × g                     │ ◄─ g (control path, branch b)
└───────────────────────┬──────────────────────┘
                        ▼
┌──────────────────────────────────────────────┐
│ 3  Resonator stage (only with V/OCT)         │ ◄─ V/OCT, WET, DRY
│    DRY · direct(x) + WET · resonator(x)      │
└───────────────────────┬──────────────────────┘
                        ▼
┌──────────────────────────────────────────────┐
│ 4  Filter with feedback                      │ ◄─ Rf (control path, branch a)
│    2 integrators + differentiator            │
└───────────────────────┬──────────────────────┘
                        ▼
┌──────────────────────────────────────────────┐
│ 5  Saturation tanh (menu, optional)          │
└───────────────────────┬──────────────────────┘
                        ▼
┌──────────────────────────────────────────────┐
│ 6  × level factor                            │
└───────────────────────┬──────────────────────┘
                        ▼
┌──────────────────────────────────────────────┐
│ 7  + velocity DC offset (Audio offset)       │
└───────────────────────┬──────────────────────┘
                        │
                        ▼
                  OUT (channel)
```

| Stage | Active in | What it does |
|---|---|---|
| 1 | all modes | Divides the input signal by the level factor. DRIVE determines it (section 7). The model calculates in model units; stage 6 undoes the division. |
| 2 | VCA, BOTH (Gate response = Linear) | Multiplies by the gain `g` from the control path. In VCA, processing ends after this stage; stages 3 to 5 are skipped. With "Gate response = Paper circuit" this stage does not exist. The gain then sits in the vactrol resistance Rf. |
| 3 | LOWPASS, BOTH (Lowpass stage), only with a V/OCT cable | Mixes the direct signal (with a fixed high-pass and low-pass) with the resonator tuned to the note. DRY and WET set the levels. Without a V/OCT cable the stage is off and the signal passes unchanged. In BOTH it sits after the linear gate part (section 6a). |
| 4 | LOWPASS, BOTH | The filter stage: two integrators, a differentiator and the feedback (FEEDBACK, limited by tanh with FB NL). The vactrol resistance Rf determines the cutoff frequency. When open, Rf is about 1.5 kΩ; when closed, it reaches into the MΩ range. With a V/OCT cable, the feedback is 0 and FEEDBACK sets the ring time of the resonator. |
| 5 | all modes, only if "Saturation = On" | Soft limiting of the model output with tanh (section 7a). |
| 6 | all modes | Multiplies by the level factor and restores the volt values. |
| 7 | only "Velocity target = Audio offset" | Adds velocity as a DC offset, only while the gate is open. It is cut at ±10 V (section 3a). In all other cases the stage is skipped. |

ALL OUT sums the outputs of the four channels and soft-limits the sum to ±10 V (section 3).

**Control path (one channel):**

```
 ATT (knob or CV) ──┐
                    ├─► + ─► limit 0…10 V ─► cv
 Velocity (knob/CV) ──┘                       │
 ┌────────────────────────────────────────┬───┘
 ▼                                        ▼
 Branch a: filter stage (Paper model)     Branch b: linear gate part
 + THRESHOLD (offset)                     g = cv / 10
 ▼                                        ▼
 Shelving filter                          Smoothing (vactrol times)
 ▼                                        ▼
 Curve, knee at the threshold             g ─► stage 2 in the audio path
 ▼
 LED current ◄─ DEPTH (series resistor)
 ▼
 Vactrol: low-pass on Rf
 ▼
 Rf ─► stage 4 in the audio path
```

* **Control level `cv`.** It is the sum of ATT and velocity, clamped to 0…10 V: `cv = clamp(ATT + velocity, 0 … 10 V)`. A patched CV replaces the respective knob. With "Velocity target = Audio offset", `cv = clamp(ATT, 0 … 10 V)`, and velocity goes into stage 7 of the audio path instead.
* **Branch a (Paper model).** The control level leads via THRESHOLD, shelving filter, curve and LED current to the vactrol. The vactrol smooths with two time constants (opening 2.4 ms, closing 50 ms, times VACTROL). Its resistance Rf acts in stage 4. THRESHOLD is added as an offset only after the limiting (section 4). DEPTH changes the series resistor of the LED and thereby how deeply the gate closes.
* **Branch b (linear gate part).** The gain is `g = cv / 10`, smoothed with the same vactrol time constants. It acts in stage 2 (section 5).
* **Modes.** LOWPASS uses only branch a. VCA uses only branch b (with "Gate response = Linear"). BOTH uses both: first the linear gate part, then the filter stage.

## 3. Controls and their effect

All knobs and switches except those in the ALL column apply per channel. The ALL and LPG 1–4 columns carry their labels above the elements.

**Channel column (LPG 1–4), rows from top:**

| Element | Range / default | Short description |
|---|---|---|
| IN | Audio | Input. IN 1 is normalled to IN 2–4 as long as nothing is patched there. |
| OUT | Audio | Channel output (mono). |
| ATT | 0…10 V, 0 V | Control level of the gate. A patched ATT CV replaces the knob. |
| VEL | −10…+10 V, 0 V | Velocity, DC offset on the control level. A patched velocity CV replaces the knob. |
| THRESH | 0…7.5 V, 7.5 V | Threshold of the gate (Paper model). A patched THRESHOLD CV replaces the knob. |
| FEEDBACK | 0…100 %, 0 % | Resonance. A patched FEEDBACK CV replaces the knob. |
| FB NL | off/on, off | Switches on the tanh nonlinearity in the feedback. |
| V/OCT | −6…+6 V, 0 V = C4 | Pitch of the resonator (section 6a). The jack sits in the same row as FB NL, to its right. |
| MODE | LOWPASS / BOTH / VCA, BOTH | Operating mode (section 5). |
| DEEP | off/on, off | Placeholder, no function. |

**ALL column (global):**

| Element | Range / default | Short description |
|---|---|---|
| ALL OUT | Audio | Sum of the four channels, followed by a soft limiter at ±10 V (linear up to ±7 V). The sum is not divided by the number of channels. |
| MODE | LOWPASS / BOTH / VCA, BOTH | Writes the mode into all four channel switches. Afterwards the channel switches can again be operated individually. |
| MODE CV | 0…10 V | When patched, it determines the mode of all channels and takes precedence over the switches: below 3.3 V = VCA, below 6.6 V = LOWPASS, above that BOTH. |
| DRIVE | 0–10, 5 | Level into the model (section 7). |
| VACTROL | ×0.1…×10, center = ×1 | Time factor of the vactrol (section 8). |
| DRY | 0…100 %, 30 % | Level of the direct signal while V/OCT is patched (section 6a). The knob sits to the left of WET. |
| WET | 0…100 %, 90 % | Level of the tuned resonator (section 6a). |
| WET CV | 0…10 V, polyphonic | When patched, it replaces the WET knob. Channels 1–4 of the cable control LPG 1–4. |

The ALL column no longer has a global DEEP switch. Its sixth row holds DRY (left) and WET (right), the seventh WET CV. The channel DEEP switches remain; they stay placeholders.

**Context menu (right-click):** It contains DEPTH per channel and the model settings (section 10). The "Depth" submenu has one slider for all four channels and one each for channels 1–4, default 100 %. A right-click on a channel knob or switch offers "Apply to all 4 channels".

**Effect in detail:**

* **ATT.** ATT is the control level of the gate in volts. By default the ATT knob or the patched ATT CV provides the control level `cv`. In the Paper model, the gate opens as soon as `cv` exceeds the threshold. With the linear gate path, the gain is `cv / 10`. A patched CV is not scaled (1 V per V); it replaces the knob.
* **VEL (velocity).** Velocity is a DC offset that is added to the control level, as a knob or as a CV. The sum `ATT + velocity` is limited to 0…10 V. A negative velocity reduces the range that ATT can reach. With ATT 5 V and velocity −5 V the gate is closed. Alternatively, velocity can be applied as a DC offset to the audio signal (section 3a).
* **THRESHOLD.** In the Paper model, THRESHOLD is the control level in volts at which the gate is half open. A lower value makes the gate open earlier, that is at a small ATT. At 7.5 V it only opens at a very high control level. A patched THRESHOLD CV input replaces the knob. It works at 1 V per V and is limited to 0…7.5 V. A positive CV means a higher threshold, so the gate opens less easily. Offset and threshold formula are given in section 4. In the linear VCA, THRESHOLD has no effect. In BOTH with the Lowpass stage, it acts on the filter part of that stage.
* **FEEDBACK.** FEEDBACK sets the resonance (section 6). With FEEDBACK CV, 0…10 V correspond to 0…100 %, limited. Both CV inputs (THRESHOLD, FEEDBACK) act per sample and can also be used at audio rate. This costs more CPU, because the filter coefficients then change in every sample. If a V/OCT cable is patched, FEEDBACK instead sets the ring time of the resonator (section 6a).
* **V/OCT.** The input tunes the resonator in front of the gate to the note (0 V = C4, ±6 octaves). It does not produce a tone by itself, but makes the gate ring at this pitch when excited by an impulse or noise. Without a cable the behavior is the same as without the resonator (section 6a).
* **WET.** WET sets the level of the resonator, applies to all channels, and has a polyphonic CV (section 6a).
* **DRY.** DRY sets the level of the direct signal while a V/OCT cable is patched. At 100 % the level is unchanged; at 0 % only the resonator is heard. Without a V/OCT cable, DRY has no effect (section 6a).
* **DEPTH (context menu).** DEPTH determines how deeply the gate closes far below the threshold (leakage). At 3 V below the threshold it is −31 dB (0 %) to −35 dB (100 %), at 6 V below −36 dB to −49 dB (Paper VCA). The threshold itself stays put when you turn it. Internally DEPTH corresponds to the trim. Default is 100 %.
* **FB NL.** FB NL switches on the tanh nonlinearity in the feedback (section 7). The switch acts wherever FEEDBACK acts (section 6).
* **MODE.** See section 5.
* **DRIVE.** DRIVE sets the level factor. This is the input voltage that corresponds to model amplitude 1: 5·2^(2·(5−Drive)/5) V, that is 20 V (0), 5 V (5) and 1.25 V (10). The output signal is scaled back. With the NL switched off, DRIVE does not change the sound. With NL, more drive means more overtones and less level (section 7).
* **VACTROL.** VACTROL is a time factor on both vactrol time constants (rise 2.4 ms, fall 50 ms at ×1; these are the values of the paper). Left is fast, the gate follows the CV. Right is slow and envelope-like (section 8). One knob applies to all channels. It also sets the inertia of the linear gate path (assumption, section 5).
* **Channels without a source.** A channel is not computed if neither its own IN nor IN 1 is patched. Its output then delivers 0 V. A channel without an OUT cable is skipped as long as ALL is not patched.

### 3a. Velocity target: "Control level" or "Audio offset"

In the context menu, **Velocity target** determines where velocity acts. The setting applies to all channels and modes.

* **Control level** (default, hardware original): Velocity is added to the control level, `cv = clamp(ATT + velocity, 0 … 10 V)`. The audio signal never receives a DC offset. A negative velocity therefore only makes the signal quieter in the linear VCA. In the Paper gate it shifts the opening. Nothing is clipped.
* **Audio offset:** ATT or ATT CV remains the control level, without velocity. Velocity acts as a DC offset on the signal after the gate, that is after the attenuation by ATT. The DC offset sits at the channel output, before the ALL sum.
  * Velocity 0 has no effect; the transition is smooth.
  * The signal is shifted by the velocity voltage and clipped in the direction of the shift at ±10 V (Rack range): below −10 V for negative velocity, above +10 V for positive velocity. A signal that already lies beyond the limit is not clipped further.
  * The offset only acts when the gate is open, then with its full value. The weight is `clamp(cv · 2, 0, 1)`: 0 at ATT 0 V, full from ATT 0.5 V. It is smoothed with the vactrol times so that the offset does not click. The 0.5 V is an assumption.
  * The offset is a DC offset at the output. ALL contains the offsets of all four channels.
  * In LOWPASS and in the Paper gate, ATT serves as the measure of the opening, not the actual filter state.

Examples (gate open):

* Signal after ATT ±2 V, velocity −4 V → −6 / −2 V.
* Signal ±5 V, velocity −4 V → +1 / −9 V.
* Signal ±5 V, velocity −7 V → −2 / −10 V (clipped).
* Signal ±5 V, velocity +1 V → +6 / −4 V.
* Signal ±5 V, velocity +7 V → +10 / +2 V (clipped).
* At ATT 0 V there is no offset.

## 4. Gate behavior: THRESHOLD, DEPTH and control level

This section describes the Paper model, that is LOWPASS as well as BOTH and VCA with "Gate response = Paper circuit". In BOTH with the linear gate path and the Lowpass stage, the threshold values continue to apply to the filter part.

**Operation.** Internally the model knows two quantities. *Offset* adds 10 V × offset to the control level. *Trim* is the series resistor of the LED. Together they determine the gate threshold. On the panel these are replaced by **THRESHOLD** (the threshold in volts) and **DEPTH** (the trim, in the context menu; it determines how deeply the gate closes).

The module calculates the offset from the chosen threshold:

`offset = (T(trim) + 0.05 V − THRESHOLD) / 10 V`

* The control range of THRESHOLD is 0…7.5 V, default 7.5 V.
* At DEPTH 100 % (default) the offset at 7.5 V is 0. The highest reachable threshold there is 7.41 V. So without a control voltage the gate is really closed.
* At a smaller DEPTH, the threshold at offset 0 is higher (T(trim)). THRESHOLD 7.5 V then already means a small offset.
* Outside the range the module limits the offset. The tooltip names the effective threshold.
* The conversion has been checked against the engine (36 combinations, largest deviation 0.008 V).

Old patches (OFFSET/TRIM, panel with 16 channels) are converted when loaded. Gain and threshold are kept; only the knobs look different. For patches from before the hardware signal path (ATT as gain ±400 %), the module sets ATT to 0 V on loading. Patches with a CV cable sound different, because the CV is no longer scaled.

**How the threshold comes about.** The gate opens when `control level + 10 V × offset` reaches about 9.12 V (at DEPTH 50 %). The 9.12 V is fixed by the circuit, namely by the series resistor and the behavior of the LED stage. The offset branch delivers at most 10 V. From this follows `threshold = 9.12 V − 10 V × offset`:

| Offset (internal) | Threshold | Meaning |
|---|---|---|
| 0 | +9.1 V (DEPTH 50 %) | Gate only opens at 9.1 V control level |
| 0.5 | +4.1 V | Middle position |
| 0.9 | +0.1 V | Gate just still closed at 0 V control level |
| 1.0 | −0.9 V | Gate already open at 0 V (outside the control range 0…7.5 V) |

"Threshold" is the control level at which the gate is half open. It is a reference value, not a signal level. The control level never falls below 0 V, so there is no negative threshold on the panel any more.

**Where T(DEPTH) comes from.** DEPTH is displayed as a knob travel of 0…100 % (value 0…1) and is not a voltage. It corresponds to the trimmer of the original (TP1, 20 kΩ), which changes the LED series resistor. From it the module calculates the reference threshold T in volts:

`T = 390,000 / (20,000 · DEPTH + 33,000)`

| Value | Meaning |
|---|---|
| 390,000 | Numerator (V·Ω): according to the paper, the Zener voltage D1 (3.9 V) times R5 (100 kΩ). It sets the overall scale. |
| 33,000 | R7 (33 kΩ), the fixed part of the series resistor. It remains even at DEPTH 0 %. |
| 20,000 · DEPTH | R6, the trimmer. At DEPTH 0 % it is fully out; at 100 % the full 20 kΩ are in the series resistor. |
| Result T | Control level in volts at which the knee of the LED characteristic is reached. The unit is volts because the numerator (V·Ω) is divided by Ω. |

**Direction:** More DEPTH makes the denominator larger and T smaller. The gate then opens at less control voltage, so it is more sensitive. The values are T = 11.82 V (DEPTH 0 %), 9.07 V (50 %) and 7.36 V (100 %). With the addition of 0.05 V (THR_HALF) this gives 11.86 V, 9.12 V and 7.41 V. These are the thresholds at offset 0. If THRESHOLD is above that, the offset is 0. If it is below, the difference is added to the control level as an offset.

**Physical background.** In the original, the vactrol resistance does not fall evenly with the control voltage. The LED characteristic has a knee where the current Ia = VB / (R6 + R7) is reached. Below it the LED barely glows, and the resistance stays high (rough estimate: approx. 64 kΩ). A few tenths of a volt above it, the resistance falls steeply to a few kΩ, and the gate is then fully open.

T is the control voltage at which this knee is reached. The trimmer shifts the knee current. The module converts the distance between the knee and THRESHOLD into an offset on the control level. In the model, the gate is half open at this voltage and fully open about 0.1 V above it. This is a rough estimate and not measured on the component. The assignment of the numbers to the components D1, R5, R6 and R7 is derived from the paper and not checked against the source code.

**What THRESHOLD is useful for:**

* **Placing the threshold within the envelope.** An envelope of 0…10 V at the ATT CV only opens the gate when it exceeds the threshold. A lower THRESHOLD opens earlier and longer, a higher one only in the upper part of the envelope.
* **Holding the gate open without an envelope.** At THRESHOLD 0 V the half-open point lies at control level 0. With a little ATT or positive velocity the gate is then open (sustained tone, open filter).
* **Shifting the range.** A negative velocity reduces the range of the ATT knob. THRESHOLD determines from which control level the gate opens. Together they determine where on the ATT knob the transition lies.
* **Modulated, also at audio rate.** The THRESHOLD CV input (1 V per V, 0…7.5 V) shifts the threshold per sample.

Caution: With THRESHOLD just above 0 V (±0.1 V), the threshold lies so close to control level 0 V that the gate stays half open at ATT 0 V. For the set value, the tooltip shows whether the gate is open, half open or closed at ATT 0 V.

The following tables use the internal quantities offset and trim. The conversion is THRESHOLD = T(trim) + 0.05 V − 10 V · offset.

Effective threshold (gain 50 % of the open gain), measured in VCA mode at 1 kHz:

`CV_threshold ≈ T(trim) − 10 V · offset`, with `T = 390,000 / (R6 + R7)`, `R6 = 20 kΩ · trim`, `R7 = 33 kΩ`.

| Offset \ trim | 0.0 | 0.5 | 1.0 |
|---|---|---|---|
| 0.00 | never (> 10 V) | 9.12 V | 7.40 V |
| 0.25 | 9.38 V | 6.62 V | 4.90 V |
| 0.50 | 6.88 V | 4.12 V | 2.40 V |
| 0.75 | 4.38 V | 1.62 V | −0.10 V |
| 1.00 | 1.88 V | −0.88 V | −2.60 V |

So at offset 0 and trim 0 the gate never opens within the range ±10 V. On the panel this would correspond to THRESHOLD +11.9 V, outside the control range.

**Characteristic** (offset 0.5, trim 0.5, VCA, 1 kHz):

| CV [V] | Rf | Gain | dB |
|---|---|---|---|
| −4 | 1352 kΩ | 0.0013 | −57 |
| −2 | 291 kΩ | 0.0083 | −42 |
| 0 | 143 kΩ | 0.0171 | −35 |
| 2 | 90 kΩ | 0.0271 | −31 |
| 4 | 63.5 kΩ | 0.0379 | −28 |
| ≥ 4.5 | 1.5 kΩ | 0.633 | −4 |

* **Below the threshold** (leakage range) the gain rises only slowly. Between −1 V and 4 V it is about 1.5–3 dB per volt, and steeper below that (−4 → −2 V: 16 dB). The gate is "closed", but not silent: at CV 0 V the gain is still −35 dB.
* **The opening is abrupt.** Between 10 % and 90 % of the open gain there are only 0.19 V (4.07…4.26 V). A CV signal that exceeds the threshold therefore switches the gate almost like a switch. Gentle envelopes only act in the leakage range and in this narrow transition.
* **Open gain:** VCA 0.633 (−4 dB), Lowpass 1.009, independent of offset and trim.
* The CV is limited internally to ±10 V.

Practical consequence: An envelope of 0…10 V at the ATT CV opens the gate at THRESHOLD 4.1 V only after 41 % of its rise and closes it again at 4.1 V. This holds for offset 0.5 and trim 0.5, on the panel for DEPTH 50 %. With THRESHOLD you decide where this happens.

### 4b. THRESHOLD (offset), DEPTH (trim), ATT and velocity in detail

Measured values: `tests/doku_offset_trim_att.cpp`, output `tests/doku_offset_trim_att_ausgabe.txt`.

**The principle.** Internally the gate has exactly one control quantity, the "effective control voltage". ATT, velocity, THRESHOLD and DEPTH determine how it arises and from when the gate opens:

```
ATT (knob or CV) ─┐
                  ├─ + ─► limited to 0…10 V ─► shelving ─┐
Velocity (knob/CV) ┘                                      ├─► current ─► LED ─► vactrol resistance Rf ─► filter/VCA
THRESHOLD ─► 10 V × offset (DC offset) ──────────────────┘            ▲
DEPTH ─► series resistor of the LED (shifts the threshold) ───────────┘
```

The threshold at which the gate is 50 % open lies at **control level ≈ T(trim) − 10 V·offset**. T is 11.82 V (trim 0), 9.07 V (0.5) and 7.36 V (1).

**In this model the gate is more of a switch than a regulator.** There are three ranges:

| Range | Position relative to threshold | Filter (LOWPASS) | VCA |
|---|---|---|---|
| **open** | above threshold + 0.1 V | Cutoff 12.95 kHz, gain 1.0 | Gain 0.633 |
| **transition** | ± 0.1 V around the threshold (10…90 % in 0.17…0.22 V) | Cutoff from ≈ 400 Hz to 13 kHz | Gain from 0.04 to 0.63 |
| **closed (leakage)** | below | Cutoff 383 Hz (0.5 V below), 296 Hz (2 V), 192 Hz (4 V), 39 Hz (8 V); measured values trim 0.5 | −29 dB (0.5 V below), −31 dB (2 V), −35 dB (4 V), −56 dB (8 V) |

In the "closed" range the sound moves only slowly. Near the threshold the cutoff changes by about one octave per 4 V, faster further below. When the threshold is crossed, the gate then opens in one step. Smooth filter sweeps therefore arise mainly from the **vactrol** (rise 2.4 ms, fall 50 ms, section 8) and not from a slow CV. A slow CV, for example from an LFO or an envelope, only becomes audible when it crosses the threshold.

**THRESHOLD (internally: offset).**

* **Effect on the signal:** Pure shift. The gain is identical at (CV −3 V, offset 0.5), (CV 0 V, offset 0.2) and (CV −6 V, offset 0.8) (0.00457). All three have CV + 10·offset = 2 V.
* **Without a CV cable** the offset is a manual control for the gate. At trim 0.5 the gate is open from offset ≈ 0.91 (threshold 9.07 V). Below that it is "closed", and the knob only changes the leakage:
  * LOWPASS cutoff: 26 Hz (offset 0), 64 Hz (0.2), 139 Hz (0.4), 236 Hz (0.6), 346 Hz (0.8), 404 Hz (0.9).
  * VCA: −110 dB (0), −47 dB (0.2), −38 dB (0.4), −33 dB (0.6), −30 dB (0.8), −28 dB (0.9).
  * At offset 1.0 the gate is open.
* **With a CV cable** the offset shifts the position of the threshold relative to the envelope. A high offset opens the gate earlier in the rise and closes it later in the fall; the open time becomes longer. A low offset only opens at high CV; the open time is short.
* **Dynamics:** The offset is a constant quantity and does not run through the shelving filter. Fast CV edges, on the other hand, get a short overshoot (section 8).
* **Order:** The offset voltage is added after the ±10 V limiting of the CV, so up to 20 V are effective. Velocity, by contrast, acts before the limiting, because it is part of the control level (section 3).

**DEPTH (internally: trim).**

* **Effect 1:** DEPTH shifts the threshold by up to 4.5 V (T = 11.82 → 7.36 V). A high trim makes the gate open earlier. At offset 0.5 the threshold is 6.88 / 5.32 / 4.12 / 3.17 / 2.40 V for trim 0 / 0.25 / 0.5 / 0.75 / 1.
* **Effect 2:** Trim also changes the leakage. The higher the trim, the more strongly the gate falls off far below the threshold:
  * 6 V below the threshold: −36 dB (trim 0), −41 dB (0.5), −49 dB (1).
  * LOWPASS cutoff 8 V below the threshold: 101 Hz (trim 0), 39 Hz (0.5), 26 Hz (1).
  * Near the threshold the difference is small. 1 V below it is −29 / −30 / −30 dB, 3 V below −31 / −33 / −35 dB for trim 0 / 0.5 / 1.
* **Effect 3:** The transition becomes minimally steeper with a high trim (0.22 V at trim 0, 0.17 V at trim 1).
* **Rule of thumb on the panel:** THRESHOLD sets the threshold, matched to the height of the envelope. DEPTH only determines how deeply the gate closes below it. If you have a fixed envelope of 0…10 V, first set THRESHOLD so that the threshold lies within the envelope. Then use DEPTH to choose how deeply the gate closes.

**ATT and velocity (control level).**

* **Effect on the signal:** The control level is `clamp(ATT + velocity, 0 … 10 V)`. The gate opens when this level exceeds the threshold. The control level comes from the knob or, if patched, from the CV, which replaces the knob.
* **Envelope 0…10 V at the ATT CV, threshold 4.12 V** (offset 0.5, trim 0.5):
  * The envelope opens the gate fully as soon as it reaches 4.2 V.
  * An envelope with a peak of only 4.12 V lies exactly on the threshold (−13 dB).
  * With a 2.5 V peak the gate no longer opens (−31 dB).
  * A negative velocity shifts the envelope downward. The gate then opens later in the rise and closes earlier in the fall; the open time becomes shorter.
  * For an exponentially falling envelope 10 V·e^(−t/200 ms), the geometry without vactrol gives open times of approx. 178 ms (peak 10 V), 120 ms (7.5 V) and 38 ms (5 V).
* ATT 0 V without CV holds the gate permanently at the rest position (control level 0 V plus velocity).
* The control level cannot fall below 0 V. The inversion of the old version (CV ATT negative) and the gain above 100 % no longer exist in the quad panel.
* Control levels above 10 V are limited. In the model, 10, 15 and 20 V give the same open gain.
* **Order in the module:** `ATT (knob or CV) + velocity` → limiting to 0…10 V → shelving. The offset voltage (THRESHOLD) is only added after the limiting. A negative velocity therefore does not cancel a low THRESHOLD.

**Typical settings.**

| Goal | Setting |
|---|---|
| Sustained tone, filter fixed at ≈ 400 Hz without envelope | LOWPASS, THRESHOLD just above 0 V (e.g. 0.1 V), DEPTH 50 % |
| Sustained tone, open, without envelope | THRESHOLD 0 V, ATT a few volts |
| Envelope 0…10 V should open in the middle | THRESHOLD 4.1 V, DEPTH 50 % |
| Short ping from the same envelope | Raise THRESHOLD or lower velocity: the gate is only open in the upper part |
| Longer opening | Lower THRESHOLD |
| Pluck with deep closing (0…10 V envelope) | MODE LOWPASS, DEPTH 100 %, THRESHOLD 7.5 V, Vactrol response = Conductance (section 14); model values for BOTH with State-based in section 4c |

**Open question.** The model switches from "closed" to "open" at the threshold within about 0.2 V (Zener behavior of the circuit in the model). Whether the original device switches just as steeply has not been checked. In the listening test, a comparison with a video or device with a slow CV, such as an LFO triangle, would be informative: Does the original also open abruptly?

### 4c. Pluck: deeply closing gate

The gate only closes as deeply as the control level at rest lies below the threshold. A 0…10 V envelope rests at 0 V. At THRESHOLD 4.1 V that is only 4.1 V of distance. The gate stays "ajar" (floor at approx. −20 dB), and after the short burst mainly the bass remains. That sounds like a deep thud, not like a pluck.

In the quad panel the control level cannot fall below 0 V, because ATT 0…10 V plus velocity is limited to 0…10 V. The rest position therefore lies at 0 V, and the distance to the threshold is the threshold itself. A high THRESHOLD at DEPTH 100 % then gives the greatest distance.

Measured on the model (Paper variant, BOTH, State-based, noise, envelope 0…10 V, fall 100 ms, level in 10 ms windows):

* DEPTH 100 %, THRESHOLD 7.4 V: rest −76 dB, peak −4 dB. The level after 20 / 50 / 100 / 200 / 400 ms is 0 / −9 / −13 / −21 / −36 dB.
* The default 4.1 V (offset "medium") leaves the gate "ajar" (rest −21 dB).

A slow vactrol (VACTROL right) makes the tail longer but lowers the peak. The gate does not open fully as long as the envelope is short. A fast envelope together with a fast vactrol (VACTROL left) sounds harder.

The numbers are model values, not listening results, and apply to the Paper gate path. In the linear gate path of BOTH, the gate closes completely at ATT 0 V. The measured values above apply to BOTH with State-based. The pluck found in the listening test (section 14, example 1), by contrast, runs in LOWPASS with Vactrol response = Conductance, THRESHOLD 7.5 V and DEPTH 100 %. There the rest position is also at 0 V, and the distance to the threshold is approx. 7.4 V.

### 4a. Closed gate is not silent

As in the original, the input signal also appears at the output when the gate is closed, filtered or attenuated. The table shows the gain in dB (offset 0.5, trim 0.5, feedback 0):

| Mode | CV 0 V | CV −4 V | CV −10 V |
|---|---|---|---|
| LOWPASS | 100 Hz −1.6 / 1 kHz −17 / 5 kHz −41 | −17 / −51 / −80 | −68 / −107 / −136 |
| BOTH | −0.6 / −7 / −21 | −7.5 / −28 / −53 | −42 / −80 / −109 |
| VCA | all frequencies ≈ −35 | −55 … −69 | −91 … −125 |

Only in LOWPASS and BOTH does a low-pass-filtered signal remain audible. VCA attenuates broadband. Whether the values correspond to the original device has not been checked, as there are no measurement data from the original.

The table applies to the Paper model. With the linear gate path (default in VCA and BOTH), the gate in VCA closes completely at ATT 0 V (gain 0). In BOTH the gain is `cv / 10` times the gain of the filter stage.

## 5. Modes

The mode per channel determines the circuit. It is chosen by the MODE switch, the global MODE, or MODE CV. LOWPASS is always the Paper model. For VCA and BOTH, the context menu **Gate response (VCA, BOTH)** selects:

* **Linear** (default): The gate is a linear VCA. The reference documentation of the hardware module describes the gate mode as a linear VCA and the combined mode as a combination of linear gate and low-pass. That is why the linear path is the default.
* **Paper circuit**: The Paper model (vactrol divider with a knee at THRESHOLD), as in sections 4 to 8. It switches steeply, and in VCA the open gain is only 0.63.

**Linear gate path.** The gain is `g = clamp(cv, 0 … 10 V) / 10`, so 10 V = gain 1. This is an assumption, because the reference documentation gives no numbers. The gain is smoothed with the vactrol time constants (rise 2.4 ms, fall 50 ms, times the VACTROL factor). The direction follows the state: if g rises, the fast constant applies. Whether the hardware module has vactrol inertia in gate mode is not documented (assumption).

| Mode | Gate response = Linear (default) | Gate response = Paper circuit |
|---|---|---|
| LOWPASS | Low-pass with C3 = 4.7 nF, Ra = 5 MΩ (Paper gate), unchanged | same mode |
| BOTH | First linear VCA, then filter stage (menu "BOTH filter stage") | Low-pass and VCA simultaneously (C3 = 0, Ra = 5 MΩ), as before the change |
| VCA | Pure linear VCA: `output = g · input`, no filter | Pure VCA (C3 = 0, Ra = 5 kΩ), open gain 0.633 |

Sonic classification:

* **LOWPASS:** The level stays near 1 in the passband, only the highs close. The gate acts like a filter with a CV-controlled cutoff (404 Hz at control level 4 V in the example of section 4). Feedback is always effective here.
* **BOTH (linear):** VCA and low-pass gate are in series. The loudness follows the control level linearly, and the filter additionally closes the highs. The menu **BOTH filter stage** offers two filter stages:
  * *Lowpass* (default) uses the filter stage of LOWPASS. THRESHOLD, DEPTH, FEEDBACK and FB NL act there as in LOWPASS, and the filter rings quietly after the gate closes. It is the default because I consider it more sensible and want to stay flexible.
  * *Paper BOTH* uses C3 = 0. The filter hardly filters and has no resonance; FEEDBACK and FB NL remain without effect. BOTH is then the Paper BOTH with a linear gate in front.
* **VCA (linear):** There is no filter, only level. THRESHOLD, DEPTH, FEEDBACK and FB NL have no effect. The loudness is exactly `cv / 10`.

Measured values on the model (48 kHz):

* **VCA linear, sine 100 Hz:** Control levels 0 / 0.5 / 1 / 2.5 / 5 / 7.5 / 10 V give the gain 0 / 0.05 / 0.1 / 0.25 / 0.5 / 0.75 / 1.0, independent of THRESHOLD, DEPTH and FEEDBACK. For a step 0 ↔ 10 V (VACTROL ×1) the rise reaches 63 % after 2.40 ms and 95 % after 7.19 ms. In the fall it is 37 % after 50.0 ms and 5 % after 149.8 ms.
* **BOTH linear, steady state** (THRESHOLD 7.5 V, DEPTH 100 %, FEEDBACK 0, sine 100 Hz). Gain for control levels 1 / 3 / 5 / 7 / 10 V:
  * Lowpass stage: 0.019 / 0.213 / 0.443 / 0.659 / 0.999.
  * Paper stage: 0.051 / 0.266 / 0.474 / 0.679 / 0.999.
  * For comparison, Paper BOTH without the linear path: 0.505 / 0.886 / 0.949 / 0.969 / 0.999.
  * BOTH linear with the Lowpass stage corresponds to LOWPASS × `cv / 10`.
* **Ringing of BOTH after the gate closes.** The ping consists of a 20 ms burst at 600 Hz with amplitude 1. Afterwards the input is 0 and the gate is closed (THRESHOLD 7.5 V, FEEDBACK 90 %). The Lowpass stage rings at 0.0081 (20–50 ms) and 0.0025 (100–200 ms), the Paper stage at 0.0000. The ringing is thus quiet, about 0.8 % of the burst amplitude.
* **Stability BOTH linear** (Lowpass stage, FEEDBACK 100 %, normalized, NL off): The peak is 33.7, in LOWPASS with the same input 43.7. The module's protection kicks in at 50. No NaN occurs.

Mode switching during operation:

* LOWPASS ↔ BOTH happens without a jump (deviation 6e-4, the DC gain of the Lowpass stage).
* From the pure linear VCA back to LOWPASS or BOTH, the filter starts with empty states. A short settling transient is possible.
* Switching the filter stage during operation produces neither NaN nor a jump above the normal signal.

Paper values for orientation (Gate response = Paper circuit):

| Mode | Circuit | Open gain (20 Hz) | Cutoff open (−3 dB) | at control level 4.0 V (Rf 63.5 kΩ) | at 3.0 V (Rf 74.6 kΩ) |
|---|---|---|---|---|---|
| LOWPASS | Low-pass with C3 = 4.7 nF, Ra = 5 MΩ | 1.009 | 12.95 kHz | Gain 0.98, f = 404 Hz | 346 Hz |
| BOTH | Low-pass and VCA simultaneously (C3 = 0, Ra = 5 MΩ) | 1.009 | 19.4 kHz | Gain 0.98, f = 1188 Hz | 1014 Hz |
| VCA | pure VCA (C3 = 0, Ra = 5 kΩ) | 0.639 | above 20 kHz | Gain 0.038 | 0.033 |

Open question: According to one possible reading of the circuit, the middle position of the hardware module uses a branch without a filter capacitor. According to the description, it would instead be a low-pass with quiet resonance. Neither can be decided with certainty. That is why the filter stage of BOTH is selectable.

## 6. Feedback (resonance)

### 6.0 What FEEDBACK does

In LOWPASS mode, FEEDBACK feeds part of the output back to the filter input via a differentiator. This creates a resonance, that is an emphasis around the cutoff frequency. Without feedback the filter is a soft low-pass; with feedback it "sings" at one point of the spectrum.

* **Where FEEDBACK acts:**
  * In LOWPASS always.
  * In BOTH only with "Gate response = Linear" and "BOTH filter stage = Lowpass" (both default).
  * In VCA, neither FEEDBACK nor FB NL acts. The same applies to BOTH with the Paper stage or the Paper gate, because the capacitor C3 of the feedback path is switched off there in the model (C3 = 0).
  * The knob position stays saved in the patch and is effective again when you switch back.
  * Measured: BOTH Paper and VCA give the same peak with FEEDBACK 90 % as with 0 %. The tooltip points this out.
  * With a V/OCT cable patched, FEEDBACK no longer acts on the filter but sets the ring time of the resonator (section 6a). The feedback of the filter is then 0.
* **The peak lies clearly above the cutoff frequency,** at about 2.3 to 2.9 times the −3 dB frequency without feedback. In the example below the cutoff is 407 Hz and the peak at 951…1162 Hz. This is the behavior of the model from the paper, not that of a classic resonant low-pass with the peak at the cutoff.
* **Low frequencies remain almost untouched** (−0.2 dB at 50 Hz). The feedback only raises the region around the peak.
* **The peak moves with the gate.** When the gate opens, the cutoff rises and the resonance moves up with it. This is the typical "wah" or "ping" sound. With the gate wide open (cutoff 13 kHz), the peak lies at about 19 kHz and is barely audible any more.
* **When excited, the filter rings.** An impulse, a gate edge or a noise burst makes the filter ring out at the resonance frequency, the longer the closer FEEDBACK is to 100 %. At 100 % it no longer decays.
* **FB NL off** (default): A sine stays a sine; only level and phase change. **FB NL on:** A tanh limits the feedback and distortion arises. It occurs mainly with high DRIVE and with signals near the resonance frequency (section 7).
* **In practice:** For "ping" or "wah", 75–90 %, a short gate and an input frequency near the peak, or noise, are suitable. From about 95 % the resonance turns into an almost tonal whistle that moves with the gate opening.

### 6.1 Measured values

The measurements apply only to LOWPASS mode. The resonance peak lies above the −3 dB frequency without feedback.

Operating point CV 4.05 V, f(−3 dB, without feedback) = 407 Hz, **normalized**:

| FEEDBACK | Peak | Peak frequency |
|---|---|---|
| 0 … 50 % | none (−0.2…−0.1 dB) | – |
| 75 % | +2.7 dB | 951 Hz |
| 90 % | +10.0 dB | 1105 Hz |
| 95 % | +15.8 dB | 1162 Hz |
| 99 % | +28.6 dB | 1162 Hz |

* Up to 50 % little happens sonically; the resonance only arises from 75 %. The stretch from 75 % to 99 % spans 26 dB.
* At 100 % the model lies exactly on the stability limit. An impulse does not decay; a constant oscillation of small amplitude arises (impulse 1.0 → peak 0.145). The module does not run away. A protection against overflow is built in anyway.
* The resonance frequency follows the CV with the gate.

**Normalized vs. circuit-accurate (context menu "Resonance"):**

* *Normalized:* The 0…100 % knob is scaled to the respective stability limit. The same knob position means the same resonance strength, independent of the gate state.
* *Circuit-accurate:* The 0…100 % knob corresponds to the absolute feedback 0…1.472. With the gate open (Rf small) this is practically the same as the normalized mode. With the gate almost closed, the resonance becomes weaker.

Example FEEDBACK 90 %:

| CV | Rf | normalized | absolute |
|---|---|---|---|
| 6.0 / 5.0 / 4.5 | 1.5 kΩ | 9.7 dB | 9.6 dB |
| 4.0 | 63.5 kΩ | 10.0 dB | 9.4 dB |
| 3.5 | 68.7 kΩ | 10.0 dB | 9.2 dB |
| 0.0 | 143 kΩ | 9.8 dB | 8.4 dB |
| −2.0 | 291 kΩ | 9.3 dB | 6.8 dB |
| −4.0 | 1352 kΩ | 6.8 dB | −0.4 dB (no resonance) |

In practice: As long as the gate is open or half open, both modes are nearly the same. A difference only appears in the almost closed range. There the absolute mode "swallows" the resonance.

## 6a. V/OCT, resonator and WET

The V/OCT input turns the gate into an instrument for ping and pluck at a fixed pitch. For this, a resonator tuned to the note sits in front of the gate. The gate itself remains unchanged and continues to determine how long the tone is audible.

**Structure.**

* The input of the gate is `DRY · d(x) + WET · res(x)`. `x` is the input signal, `d(x)` the direct signal after the high-pass and low-pass, and `res(x)` the tuned resonator. The resonator is fed from the unfiltered input `x`.
* With V/OCT patched, the direct signal passes through a high-pass and a low-pass, both first order. The values are fixed and have no controls:
  * High-pass: corner frequency = 0.5 × pitch of the resonator, at least 150 Hz (and at most 0.2 × sample rate).
  * Low-pass: 4 kHz (at most 0.45 × sample rate).
  * The high-pass removes the DC component and the low frequencies from the direct signal of a unipolar needle pulse (such as an envelope at IN), which would otherwise run through the open gate as a "plop". According to the code comment, 150 Hz reduces the plop by about 25 dB. The low-pass rounds off the vertical edge.
  * Both filters act only in resonator operation, that is, with V/OCT patched and in the modes with a resonance stage (see below).
* The resonator has the same level at every note. Its impulse response is a decaying sine with amplitude 1. The output has zeros at 0 Hz and at the Nyquist frequency. The peak gain is unchanged. This removes DC and Nyquist-frequency components from the resonator output.
* A soft limiter at the resonator output prevents sustained noise with a long ring time from summing up the level. It is linear up to 4 model units; above that it approaches 8 continuously.
* As long as V/OCT is patched, the feedback of the filter itself is 0. The resonance comes only from the resonator.
* The resonator only acts with a resonance stage: in LOWPASS and in BOTH with "Gate response = Linear" and "BOTH filter stage = Lowpass". In VCA and in Paper BOTH, V/OCT has no effect.
* Without a patched V/OCT cable, everything is bit-identical to the state without the resonator. DRY, high-pass and low-pass are then not active.

**Operation.**

* **V/OCT:** 0 V = C4 (261.6256 Hz), range ±6 octaves. An audio-rate signal is possible (FM).
* **FEEDBACK** (knob or CV) is, with V/OCT patched, the ring time T60 of the resonator: `T60 = 0.05 s · 60^FEEDBACK`. That is 0.05 s at 0 %, 0.39 s at 50 % and 3 s at 100 %. The tooltip shows the time.
* **WET** (knob 0…100 %, global, default 90 %): The gain of the resonator is `0.045 · k²` with k = WET. This gives:

| WET | 25 % | 50 % | 75 % | 90 % (default) | 100 % |
|---|---|---|---|---|---|
| Gain | 0.0028 | 0.0113 | 0.0253 | 0.0365 | 0.045 |
| dB | −51 | −39 | −32 | −29 | −27 |

* **DRY** (knob 0…100 %, global, default 30 %): The gain of the direct signal is `k²` with k = DRY. This gives:

| DRY | 0 % | 25 % | 30 % (default) | 50 % | 75 % | 100 % |
|---|---|---|---|---|---|---|
| Gain | 0 | 0.0625 | 0.09 | 0.25 | 0.56 | 1 |
| dB | −∞ | −24 | −21 | −12 | −5 | 0 |

  DRY acts only while V/OCT is patched. 100 % leaves the level of the direct signal unchanged (the fixed high-pass and low-pass stay active, though). At 0 % only the tuned resonator is heard. DRY has no CV input.
* **WET CV** (polyphonic): 0…10 V correspond to 0…100 % of the knob, with the same curve. A patched WET CV replaces the knob. Channels 1–4 of the cable control LPG 1–4; a monophonic cable applies to all channels. With the cable removed, the knob acts again.

**Level calibration.** The value range is tailored to a pulse as excitation. A pulse sums up to its area in the resonator (10 V over a few ms gives over 100 model units raw, limited to 8). The required gain is therefore small. With the gain 0.0225 at WET 90 % (output level about 2.5 V), I found a good sound and raised the level to about 5 V (+6 dB). That is today's curve `0.045 · k²`; the default is WET 90 %. The earlier calibration (`0.05 · k²`, default 35 %) is thereby superseded. With other material, such as a noise burst or sustained noise, the suitable value may differ.

**Accuracy (model, 48 kHz).** The measurements date from the state of 2026-10-24, that is, before the zeros in the resonator output and before the high-pass and low-pass in the direct path. They have not been repeated.


* The pitch is correct from C2 to C8 at T60 0.1 / 0.5 / 3 s to 0.0 cents (C8: +0.2 to +0.3 cents). The decay between 50 and 300 ms matches the target (−150, −30 and −5 dB).
* The ring level with noise excitation is flat across pitch (C2 to C7 within −0.8 to +0.1 dB). A level compensation for low notes is therefore not necessary.
* When the gate closes, the pitch deviates by −4 to −12 cents with a 200 ms envelope (C3 to C6, in later windows). With a 20 ms envelope it is up to about ±40 cents, but only in quiet decays (−20 to −50 dB). The signal does not glide downward.
* Stability: With audio-rate V/OCT (±2 octaves at 0.7 Hz plus ±1.5 octaves FM at 220 Hz), T60 3 s and 20 s of noise, no NaN occurs. The peak is 16.7, the module limit 50. This test dates from the state of 2026-10-24 (gain up to 0.05, no high-pass and low-pass in the direct path) and has not been repeated for the state of 2026-10-29.

**Notes on use.**

* V/OCT does not produce a tone. It only tunes the resonance. A sustained tone or LFO at IN stays audible at its own pitch; V/OCT then only changes the timbre.
* To hear the pitch, the resonator needs a broadband excitation: noise, or a short impulse or trigger. The gate should be open (ATT 10 V), FEEDBACK high (long ring time).
* With noise at IN, the pitch rises with rising voltage (confirmed).
* With V/OCT patched, the direct signal sounds thinner and quieter than without: the high-pass removes the lows, the low-pass the highs above 4 kHz, and DRY defaults to 30 %. For more direct signal, turn DRY up; for the resonator only, set DRY to 0 %.

**Compared with the earlier version.** A first version tuned the whole filter to the note by time scaling. It sounded dull: With the gate open and V/OCT at C4, the gain for 1 kHz dropped from −0.0 to −20.5 dB, and for 4 kHz from −0.5 to −42.3 dB. In addition, the tone sounded much shorter at high notes. That is why there is now the resonator in front of the gate. The direct path stays untouched, and the gate sounds as without V/OCT. The former menu item "Pitch level compensation" has been dropped, because the level is now flat across pitch by itself. An old patch with V/OCT patched sounds different (resonator instead of time scaling). A saved key `pitchComp` is ignored.

**Open.** The Rack test series (pitch C1 to C7, WET series, DRY series, WET CV, ring time, V/OCT removed, audio-rate V/OCT, gate closing, old patch) and the listening comparison with a comparison module are still pending. Possible extensions: a hold mode (constant pitch over the whole tone), an adjustable ring-time range (currently 0.05 to 3 s), a level curve for WET via the menu, and adjustable corner frequencies for the high-pass and low-pass in the direct path (currently fixed).

## 7. Nonlinearity (FB NL) and DRIVE

With FB NL = on, the feedback passes through a tanh limiter. This only acts with a sufficiently large signal. DRIVE determines how large the signal is in the model (section 3).

Overtone content (root of the sum of harmonics 2…8, relative to the fundamental), input ±5 V, FEEDBACK 90 %, **NL solver "Corrected"**:

| CV | f | Drive 0 | 2.5 | 5 | 7.5 | 10 | Fundamental Drive 0 / 10 |
|---|---|---|---|---|---|---|---|
| 8.0 V (open) | 220 Hz | 0.0002 | 0.0007 | 0.0021 | 0.0038 | 0.0041 | 5.00 / 5.00 V |
| 3.0 V | 500 Hz | 0.0228 | 0.0643 | 0.1079 | 0.1199 | 0.1040 | 6.37 / 3.89 V |
| 4.0 V | 500 Hz | 0.0282 | 0.0754 | 0.1196 | 0.1282 | 0.1066 | 5.91 / 4.08 V |
| 4.0 V | 2000 Hz | 0.0007 | 0.0025 | 0.0090 | 0.0250 | 0.0460 | 2.26 / 1.70 V |

For comparison, the solver "Reference": 0.0031 / 0.0122 / 0.0391 / 0.0724 / 0.0811 for CV 3.0 V, 500 Hz.

What you hear:

* **With the gate open and no resonance peak at the signal,** the NL remains practically inaudible (harmonic content below 0.5 %).
* **At the resonance peak** clear distortion arises (up to approx. 13 % harmonics) and the level drops (fundamental 5.9 → 4.1 V). This applies with the gate half open and a signal near the resonance frequency.
* **At 500 Hz** the harmonic content rises with DRIVE up to approx. 7.5 and then falls again. At 2 kHz it keeps rising up to Drive 10.
* **DRIVE without NL** has no effect.
* **With NL and FEEDBACK 100 %** (solver "Corrected", CV 4 V), a small oscillation remains after an impulse. The peak in the output after 5 s is 0.094 V at Drive 0, 0.030 V at Drive 5 and 0.008 V at Drive 10. With the solver "Reference" it decays (0 V).

## 7a. Saturation (add-on, menu "Saturation")

Saturation is not part of the Paper model. On scope recordings of the original, a sine has a slightly flattened peak, but the model has no saturation in the audio path. With "Saturation = On" the model output signal (in model units) runs through `tanh` and is then scaled back to volts. Default is Off.

* **DRIVE** determines the intensity (level in the model, section 3). The small-signal gain remains 1; only the course of large amplitudes changes. The saturation acts in all modes and also at feedback 0.
* Measured for a sine with 5 V peak, when the model has gain 1 (LOWPASS open 1.009):

| DRIVE | Model amplitude | Harmonics 3…9 / fundamental | Output peak |
|---|---|---|---|
| 0 | 0.25 | 0.5 % | 4.90 V |
| 2.5 | 0.50 | 2.0 % | 4.62 V |
| 5 | 1.00 | 6.7 % | 3.81 V |
| 7.5 | 2.00 | 17.3 % | 2.41 V |
| 10 | 4.00 | 30.0 % | 1.25 V |

* Only odd-order overtones arise, because the characteristic is symmetric.
* **The output level falls with increasing DRIVE.** The output peak is limited to the level factor (20 / 5 / 1.25 V at Drive 0 / 5 / 10). With high drive you should therefore raise the level afterwards.
* In VCA mode (open gain 0.63) and with a partially closed gate, the amplitude in the model is smaller and the saturation correspondingly weaker. It acts most strongly with the gate open and a loud signal.
* **Listening test in Rack:** The warm sound of a sine appears when DRIVE is at about 75 % (≈ 7.5). That corresponds to around 17 % harmonics (table). At Drive 5 the effect is too weak.
* This stage is an add-on and not a measurement of the original. The measured effect corresponds to the computational model, not necessarily to the sound of the device.

## 8. Time behavior: vactrol

The model has two time constants: opening 2.4 ms, closing 50 ms (low-pass on the resistance Rf). The **VACTROL** knob scales both values by a factor of ×0.1…×10 and applies to all channels. The center (×1) corresponds to the values of the paper and is checked bit-exactly against the reference. VACTROL also sets the inertia of the linear gate path (section 5).

Measured (State-based, Paper VCA, step −10 → +10 V): The rise from 10 to 90 % takes 3.2 ms at ×0.3, 10.5 ms at ×1 and 31.6 ms at ×3. The tables in this section apply to ×1.

The context menu **Vactrol response** offers:

* *Reference (as published):* like the Octave reference. The reference chooses the fast constant only if the target value Rf falls in this sample. With a gate-like CV this happens only in the first sample. After that the vactrol opens with the slow constant.
* *State-based:* The direction results from comparing the target with the current vactrol state. This behaves like a real LPG.
* *Conductance:* an add-on that provides a longer trailing response (see below and section 14).

Gate step −10 V → +10 V (VCA, DC at the input):

| | Reference | State-based |
|---|---|---|
| Rise 10 → 90 % | **219.7 ms** | **10.5 ms** |
| Closing 90 → 10 % | 0.1 ms | 0.1 ms |

The very fast closing at the step from +10 V to −10 V comes from the shelving filter in the control path. The steep CV edge overshoots, and the vactrol is briefly driven to extremely high resistance. With a gentler fall of the CV it closes more slowly (table below).

Short gate pulses (CV −10 → +10 V), peak gain reached relative to open:

| Length | Reference | State-based |
|---|---|---|
| 5 ms | 0.000 | 0.001 |
| 10 ms | 0.000 | 0.007 |
| 20 ms | 0.000 | 0.32 |
| 50 ms | 0.000 | 1.00 |
| 100 ms | 0.001 | 1.00 |
| 200 ms | 0.006 | – (1.00) |

With the "Reference" setting the module practically does not respond to triggers and short gates. The gate would have to be well over 200 ms long. For pluck sounds you should choose "State-based" or "Conductance". "Conductance" provides the trailing response with which the pluck in section 14 was found.

Closing time after a 100 ms gate (+10 V), State-based, depending on the rest CV (time to 50 % and 10 % of the peak gain):

| Rest CV | 50 % | 10 % | Final value (rel.) |
|---|---|---|---|
| −10 V | 0.0 ms | 0.1 ms | 0.000 |
| −4 V | 0.0 ms | 0.1 ms | 0.003 |
| −1 V | 0.4 ms | 8.8 ms | 0.020 |
| 1 V | 1.4 ms | 19.1 ms | 0.035 |
| 2 V | 2.0 ms | 25.4 ms | 0.043 |
| 3 V | 2.5 ms | 33.0 ms | 0.051 |
| 3.5 V | 2.8 ms | 37.4 ms | 0.056 |

So the "decay" is not a free parameter. It depends on where the CV falls back to. A low rest CV (≤ −4 V) switches off practically at once. A rest CV just below the threshold (3…3.5 V) produces a longer, softer decay of 33…37 ms down to −20 dB. With THRESHOLD, the position of the rest CV relative to the threshold can be changed.

## 9. Channels, inputs and old patches

**Channels and inputs.**

* There are four separate, monophonic channels. Each has its own state, its own knobs and its own switches. IN 1 is normalled to IN 2–4 as long as nothing is patched there.
* Each channel has its own V/OCT input. The WET CV input in the ALL column is polyphonic: Channels 1–4 of the cable control LPG 1–4.
* A channel without an audio source (neither its own IN nor IN 1) is not computed; its output sits at 0 V. A channel without an OUT cable is skipped as long as ALL is not patched. This saves up to three quarters of the CPU time. When reactivated, the channel starts in the settled state, so the vactrol does not have to settle.

**Old patches.**

* Old patches of the polyphonic version (16 channels) end up on channels 1–4. The param blocks of the old version remain so that the indices match. Their cables on AUDIO and CV sit on channel 1. Only channel 1 of the polyphony is read; poly inputs are not fanned out (exception: WET CV).
* Patches from before the hardware signal path get ATT 0 V, and their CV cables are no longer scaled (section 4).
* New inputs (THRESHOLD CV, FEEDBACK CV, V/OCT, WET CV) are appended at the end of the input list. Old patches thereby keep their cables.
* A patch with V/OCT patched from the time of time scaling now sounds different (section 6a).
* Patches with V/OCT patched from before 2026-10-29 also sound different: the direct signal is filtered and, with DRY at 30 %, quieter, and WET sits at a different level. DRY is a new knob and loads with the default 30 % in an old patch.

## 10. Context menu

The knobs are on the panel. The context menu contains DEPTH and the model settings.

| Entry | Options (default in bold) | Effect |
|---|---|---|
| Depth | Slider "All channels" and one each for channels 1–4 (default 100 %) | How deeply the gate closes below the threshold (section 3) |
| Resonance | **Normalized** / Circuit-accurate | Section 6 |
| Vactrol response | **Reference** / State-based / Conductance | Section 8 |
| NL solver | Reference / **Corrected** | Section 11 (model limits) |
| Circuit variant | **Paper** / Reproduction board | Component values per the paper or per a variant with changed component values (section 11a) |
| Gate response (VCA, BOTH) | **Linear** / Paper circuit | Linear gate path or Paper model (section 5) |
| Velocity target | **Control level** / Audio offset | Where velocity acts (section 3a) |
| BOTH filter stage | **Lowpass** / Paper BOTH | Filter stage of BOTH, only with the linear gate path (section 5) |
| Saturation | **Off** / On | Add-on, not in the Paper model (section 7a) |
| Oversampling | **Off** / 2x | 2× oversampling of the audio path. It doubles the CPU time and can reduce aliasing caused by the nonlinearity (to be checked in the listening test). |

The settings are saved in the patch. Older patches without the keys for Gate response, BOTH filter stage and Velocity target load with the default values (Linear, Lowpass, Control level).

## 11. Model limits and peculiarities of the reference

Three points were noticed when recalculating the reference:

1. **Paper equation (20):** It is faulty in the PDF; the code of the reference is correct. The code is what is implemented.
2. **Vactrol direction:** See section 8. With gate CV, the reference opens in about 220 ms. "State-based" is available as an option.
3. **NL solver:** In `yxden` there is `+a1·d1·t2m1`, but the derivation gives `−a1·d1·t2m1`. The NL branch therefore does not merge into the linear branch at small signal. Measured, the NL deviates from the linear branch by 52 % at 1 µV ("Reference"); with "Corrected" it is 1.4·10⁻¹². "Corrected" is available as an option and has been the module default since the listening test.

The default is NL solver = **Corrected** (decision after the listening test). For Vactrol response, Reference applies; this decision is still open. The engine itself (`AnnaMariaLpg`) still has the reference as its preset and is checked bit-exactly against Octave that way. The module explicitly sets Corrected.

Further limits:

* The parameters of the vactrol model are taken from the paper, not measured.
* Component tolerances and temperature are not modeled.
* At very high frequencies near Nyquist the trapezoidal integration acts. 2× oversampling helps.

## 11a. Comparison with a component variant

The source is the reference documentation of a circuit variant of the low-pass gate. The values were read from it and not checked against measurement data.

**Deviations from the paper:**

* R8 = 10 k instead of 4.7 k.
* In VCA mode the shunt resistor R16 = 10 k instead of 5 k, and there is an input stage with gain 2 (R11 = R12 = 15 k).
* The original also has a DEEP switch. It makes the gate close more deeply at the same knob position, roughly like a THRESHOLD a few volts higher. DEEP is not modeled. An exact reproduction would need the LED forward voltage.

**Menu "Circuit variant = Reproduction board":** It sets R8 = 10 k, the VCA shunt resistor to 10 k, and in VCA mode the input gain to 2. Default remains Paper.

Measured values (sine 1 kHz, THRESHOLD 4.1 V, DEPTH 50 %, State-based, gain in dB):

| Mode | Variant | CV −4 V | 0 V | 4 V | 8 V |
|---|---|---|---|---|---|
| LOWPASS | Paper | −27.3 | −6.8 | −2.5 | 0.0 |
| LOWPASS | Reproduction | −40.2 | −13.9 | −7.4 | 0.0 |
| BOTH | Paper | −50.7 | −16.8 | −8.9 | 0.0 |
| BOTH | Reproduction | −66.4 | −28.9 | −17.9 | 0.0 |
| VCA | Paper | −57.1 | −35.3 | −28.4 | −4.0 |
| VCA | Reproduction | −58.2 | −31.3 | −24.2 | +3.8 |

The threshold does not change. The closed gate lies 6…12 dB lower in LOWPASS and BOTH. In VCA mode the open gain is +3.8 dB instead of −4 dB; watch the level here.

**Pluck without new knobs** (Paper variant, BOTH, State-based, noise, envelope 0…10 V, fall 100 ms): DEPTH 100 % and THRESHOLD 7.4 V (offset at the lower stop) give rest −76 dB and peak −4 dB. The level after 20 / 50 / 100 / 200 / 400 ms is 0 / −9 / −13 / −21 / −36 dB. The default 4.1 V (offset "medium") leaves the gate "ajar" (rest −21 dB).

## 12. CPU load

* **Engine** (measuring machine, double, -O2, one channel): 70–90 ns per sample with moving CV. With constant CV it is 20–26 ns, because the control path and audio coefficients are cached (bit-identical to the original).
* **Rack:** One active channel needs 0.2–0.4 %. With ALL patched (four channels) it is 1.5 %, which is about 0.35 % per channel with moving CV.
* With 2× oversampling the load roughly doubles.
* The CV inputs for THRESHOLD and FEEDBACK at audio rate cost more, because the coefficients change in every sample.
* The resonator with V/OCT costs additional CPU. With audio-rate V/OCT the coefficients change in every sample, and sin, cos and pow run per sample and channel. This has not yet been measured in Rack. The high-pass and low-pass in the direct path (first order each) come on top and are likewise not measured.
* The linear gate path in VCA is cheaper than the Paper model.

## 13. License and sources

The plugin is licensed under GPL-3.0-or-later. The formulas follow `buchla_lp_gate.m` (Copyright 2014 Stefano D'Angelo, MIT; notice in `AnnaMariaLpg.hpp`) and the paper *Parker, D'Angelo: A Digital Model of the Buchla Lowpass-Gate*, DAFx-13.

## 14. Patch examples (to be checked in the listening test)

1. **Pluck / marimba** (found in the listening test):
   * Module: MODE LOWPASS, THRESHOLD 7.5 V, DEPTH 100 % (default), Vactrol response = Conductance, VACTROL ×1.
   * Envelope to the ATT CV, VEL 0: delay 700 ms, attack 5 ms, hold 0 ms, release 200 ms, peak 10 V (generated with the envelope module Europa).
   * At IN there is a sawtooth (100 Hz) or a triangle (2 kHz). The triangle sounds bell-like or marimba-like.
   * Further settings: DRIVE 1.9, FEEDBACK 63–80 %, FB NL on, Resonance = Normalized, NL solver = Corrected, Circuit variant = Paper, Saturation = On, Oversampling off.
   * "Gate response" and "BOTH filter stage" only act in BOTH and VCA and are irrelevant here.
   * Because THRESHOLD is 7.5 V, only the upper part of the envelope opens the gate (from approx. 7.4 V control level). The decay comes from the trailing response of the vactrol model (section 8).
   * For a softer decay turn VACTROL to the right, for a harder one to the left.
   * A variant with BOTH is given in section 4c (model values).
   * On one note: Patch a V/OCT input, give a short impulse or noise burst at IN, and set FEEDBACK high (section 6a). The envelope at the ATT CV stays. WET (default 90 %) and DRY (default 30 %) mix resonator and direct signal.
2. **Linear VCA:** MODE VCA, envelope or knob at ATT. The loudness follows exactly `cv / 10`. Velocity shifts the range (section 3).
3. **Filter drone:** MODE LOWPASS, THRESHOLD just above 0 V (e.g. 0.1 V), so that the gate sits at the desired cutoff without an envelope. An LFO with small amplitude goes to the ATT CV, FEEDBACK 75–90 %.
4. **Resonance distortion:** MODE LOWPASS, FB NL on, FEEDBACK 90 %, DRIVE 6–8, gate half open (control level at the threshold).
5. **DC offset shift:** Velocity target = Audio offset, ATT 10 V, signal ±5 V at IN, velocity from 0 to −4 V. The signal slides downward (+1 / −9 V). With further lowering it is clipped at the bottom at −10 V (section 3a).

## 15. Decisions (log)

My decisions, in chronological order:

* **Documentation (2026-09-30).** The documentation is public in the repo. Original names, model numbers, links and verbatim quotes from manufacturer manuals are avoided. Sources are called only "reference documentation".
* **Quad layout (2026-10-14).** Four monophonic channels instead of 16 polyphonic, 31 HP, ALL column (ALL OUT, global MODE, MODE CV, DRIVE, VACTROL, DEEP; status of this entry, the column changed on 2026-10-24 and 2026-10-29). Order per channel: IN | OUT, ATT | VEL, CV | CV, THRESH | FEEDBACK, CV | CV, FB NL, MODE | DEEP. ALL first sums and then soft-limits to ±10 V. It is not divided by 4, because the signal would otherwise be too quiet. IN 1 is passed through to IN 2–4.
* **Signal path after the hardware original (2026-10-14).** ATT is the control level 0…10 V, velocity a DC offset on top of it. The sum is limited to 0…10 V, and a patched CV replaces the respective knob (ATT and velocity). THRESHOLD CV: 1 V/V over 0…7.5 V. FEEDBACK CV: 0…10 V = 0…100 %.
* **Linear VCA (2026-10-15).** According to the reference documentation, VCA should be a linear VCA; the Paper circuit is ineffective there. BOTH also gets the linear path.
* **Order in BOTH (2026-10-15).** First VCA, then LPG, both in series. This corresponds to the statement in a video about the hardware.
* **Filter stage of BOTH (2026-10-15).** The stage is selectable via the menu ("stay flexible"). Default is Lowpass ("more sensible for me").
* **Velocity target (2026-10-15).** The menu "Velocity target" offers "Control level" (default) and "Audio offset". For "Audio offset" the following applies:
  * Velocity is a DC offset after the gate/ATT.
  * Velocity 0 has no effect.
  * Clipping takes place in the direction of the shift at ±10 V.
  * The offset only acts when the gate is open, then with its full value.
  * Rejected were: offset before the gate, fixed lower limit 0 V or −5 V, capping at +5 V or +10 V at the input.
* **V/OCT input (2026-10-20).** Each channel gets its own input for patching the pitch, without a menu item. The first version tuned the whole filter by time scaling. A level compensation for low notes was built in as an option, but stayed off by default.
* **Resonator instead of time scaling (2026-10-24).** With time scaling, I heard a clearly worse sound than without V/OCT. The listening sample "resonator in front of the gate" was the favorite ("sounds different, but round"). Time scaling and the level compensation have been dropped.
* **WET (2026-10-24).** The level of the resonator is a global knob. A polyphonic CV input replaces the knob when patched. The value range is tailored to my Rack measurement (good sound at 10 V and −45 dB). On 2026-10-29 the curve was recalibrated (see below).
* **Panel (2026-10-24).** The global DEEP switch has been removed from the panel to make room for WET and WET CV. The channel DEEP switches remain as placeholders.
* **Level of WET (2026-10-29).** With the gain 0.0225 at WET 90 %, I found a good sound and raised the level by 6 dB. The gain is now `0.045 · k²`, the default WET 90 %.
* **DRY and direct path (2026-10-29).** New global knob DRY (gain `k²`, default 30 %) for the direct signal in resonator operation. The direct signal additionally passes through a fixed high-pass (0.5 × pitch, at least 150 Hz) and a low-pass (4 kHz), first order each, without controls. The occasion was the deep "plop" that a unipolar needle pulse produces in the direct path through the open gate. The resonator is still fed from the unfiltered input.
* **Resonator output (2026-10-29).** The output now has zeros at 0 Hz and at the Nyquist frequency; the peak gain stays the same.
* **Panel (2026-10-29).** In the ALL column, DRY and WET sit side by side in one row, with WET CV below.
