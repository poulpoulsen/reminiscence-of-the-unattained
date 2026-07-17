# marf248-vcv-cpp

Natives VCV Rack Plugin: Port des Buchla/Tiptop 248t MARF, inkl. Trigger- und CV-Expander.

## Referenzen
- Offizielles Tiptop Audio/Buchla 248t PDF-Manual
- "The MARF Book" von maxl0rd

## Module
- **Marf248** – Hauptmodul, zwei AFG-Sektionen (A/B), 16 gemeinsame Stages
- **Marf248TriggerExpander** – 12HP, Clear/Reset/Preset-Trigger, Scale/Key-CV, Cycle-Outputs
- **Marf248CvExpander** – 6HP, Stage-Time-CV (poly), Gate-Outputs A/B (poly)

## Build
```bash
cd src
make -j4
make install
```
VCV Rack muss danach komplett neu gestartet werden (nicht nur Fenster schließen).

## Status
Siehe CHANGELOG.md.
