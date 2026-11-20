# Changelog

All notable changes to the plugin "reminiscence of the unattained".

## 2.1.0 – 2026-11

Patch compatibility: the parameters, inputs and outputs of Sofonisba, Lucia,
Amilcare and Bianca are unchanged. Patches saved with 2.0.0 load and play as before.

### Added – new modules
- **Europa** – quad function generator with VCA. Four independent
  Delay/Attack/Hold/Release generators, One-Shot/Sustain modes, loop with gate
  or free-running, delay and release levels, level/hold/rate CV, OR outputs per
  pair, quadrature follow and coupled-decay handshake mode, phasor mode with
  ratio p/q and offset, optional minBLEP antialiasing for level jumps.
- **Minerva** – dual voltage processor. Attenuverter, crossfader between two
  inputs with CV and offset, +5 V switch, polyphonic with individual settings for
  all 16 channels; context menu: M range, output limiting (off/soft/hard),
  DC blocker per section.
- **Elena** – control voltage processor. Two transfer-curve sections with input
  select, 0 V/10 V points and breakpoint (per channel, polyphonic), selectable
  input/output range (0–10 V, ±5 V, ±10 V, unlimited), six slew limiters (pairs
  C/D, E/F, G/H) with rate/time mode and auto-pulse, optional DC blocker.
- **Anna-Maria** – quad low-pass gate with Lowpass/Both/VCA modes, vactrol-style
  response, threshold/depth, feedback resonance, tuned V/Oct resonator, wet/dry,
  optional saturation, circuit variant selectable in the context menu.
- **Ritornello** – loop expander for Sofonisba and Lucia. Loop begin/end
  markers per stage via gate inputs, pass counter (1 V = 2 passes, up to 10),
  nested loops, EXIT trigger, DONE trigger when the overall loop is finished,
  remaining-pass CV (REST) and display per section.

### Changed
- Sofonisba/Lucia: the broadcast context menu on the nine modifier buttons now
  offers separate "Set … on all stages" and "Clear … on all stages" entries; each
  broadcast is undone in one step (Ctrl+Z).
- Expanders: the host now finds expanders through a chain of up to three
  modules per side (Amilcare, Bianca, Ritornello in any order).

### Fixed
- Broadcast changes are handed to the audio thread instead of being written
  from the GUI thread.
- Lower CPU load: expander detection uses a cached pointer comparison instead of
  a `dynamic_cast` per sample.

### Documentation
- Separate documentation (German and English) for every module in `doc/`.

## 2.0.0 – 2026-11

First release in the VCV Library: Sofonisba, Lucia, Amilcare, Bianca.
