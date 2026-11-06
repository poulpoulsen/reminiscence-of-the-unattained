# Anna-Maria – Entries for Chapter 11 (Rules of thumb)

* **Don't treat the gate as "soft"**: Between 10 % and 90 % opening there are only about 0.2 V of CV. Set the threshold directly with THRESHOLD (volts); don't expect a slow envelope to open it linearly.
* **Threshold**: THRESHOLD is the CV in volts (after CV ATT) at which the gate is half open. Default 4.1 V. DEPTH does not change the threshold, only how deep the gate closes below it.
* **Pluck**: choose *Vactrol response = State-based* in the menu. With *Reference* the gate opens in ~220 ms and barely responds to gates < 200 ms.
* **Decay**: the decay depends on the rest CV: low (≤ −4 V) = closes instantly; just below the threshold (3…3.5 V) = 33…37 ms to −20 dB.
* **Resonance (FEEDBACK knob)**: only happens from 75 %. Up to 50 % = nothing, 75 % = +2.7 dB, 90 % = +10 dB, 99 % = +29 dB. At 100 % marginal stability (oscillation remains). Applies in LOWPASS mode only, menu Resonance = Normalized and gate just below the threshold (measured at cutoff 407 Hz).
* **Normalized vs. absolute**: the same as long as the gate is open or half open; absolute loses resonance with an almost closed gate (CV −4 V: −0.4 dB instead of +6.8 dB).
* **DRIVE** only acts with FB NL = on. Without NL it is sonically neutral. Harmonics grow up to Drive ≈ 7.5 at low frequencies, up to 10 at high ones.
* **NL audible** only at the resonance peak with a half-open gate (up to ~13 % harmonics); with an open gate and no resonance < 0.5 %.
* **MODE**: LOWPASS = filter only, the only mode with feedback. BOTH = filter + VCA (pluck). VCA = level only (open 0.63 = −4 dB).
* **Level**: open, LOWPASS ≈ 1.0, VCA ≈ 0.63 (−4 dB). Raise the level afterwards if needed.
* **No CV at the input = 0 V**: with default settings the gate is then closed (threshold 4.1 V). For a drone, set THRESHOLD below 0 V (fully left = open).
* **Saturation** (menu *Saturation = On*, addition): intensity via DRIVE. Sine 5 V: Drive 5 = 6.7 % harmonics, Drive 10 = 30 %. The output level drops to the level factor (5 V → 3.8 V at Drive 5, 1.25 V at Drive 10). Strongest with an open gate and a loud signal. For a warm sine in the listening test: DRIVE ≈ 7.5.
* **Gate closed ≠ silent**: LOWPASS/BOTH let the bass through with a closed gate (CV 0 V, 100 Hz: −1.6/−0.6 dB; 5 kHz: −41/−21 dB). VCA: broadband −35 dB at CV 0 V. Practically silent only at CV −10 V.
* **Negative THRESHOLD** (down to −0.9 V at DEPTH 50 %, −2.6 V at DEPTH 100 %): the gate is open without CV, a negative CV closes it. Useful for drones with "ducking", for bipolar LFOs (opens in the positive half-wave) and, with CV ATT −100 %, for an inverted gate. Avoid thresholds between −0.1 and +0.1 V: the gate sits half open without a CV cable.
* **Why negative thresholds exist:** the gate opens at `CV + 10 V × offset ≈ 9.1 V`. At full offset (10 V), 0.9 V are already missing at the threshold, so the threshold lies at −0.9 V.
* **V/OCT is a resonator, not a filter tuning**: it rings exactly on the note, with no glide when the gate closes. Scaling the time of the whole filter damped the sound and was replaced.
* **FEEDBACK with V/OCT = decay time**: 0.05 s · 60^FEEDBACK (0 % = 0.05 s, 50 % ≈ 0.4 s, 100 % = 3 s). The gate's own resonance is then off.
* **WET and DRY levels** are tailored to a 10 V needle pulse at IN (good at about −45 dB, WET ≈ 35 %). With a noise burst or a sustained tone, readjust the level.
* **A direct two-pole form can build up at audio-rate V/OCT**: the resonator therefore works with a complex state (one rotation per sample) and is soft-limited (linear up to 4, then towards 8).
