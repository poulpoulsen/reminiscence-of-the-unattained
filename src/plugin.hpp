#pragma once
#include <rack.hpp>

using namespace rack;

// Von Rack beim Plugin-Load bereitgestellt
extern Plugin* pluginInstance;

// Modul-Modelle (eines pro .cpp-Datei im src/-Ordner)
extern Model* modelMarf248;
extern Model* modelMarf248TriggerExpander;
extern Model* modelMarf248CvExpander;
// Kompaktversion "Lucia" (Session "Kompaktversion Marf248 - Panel &
// Model-Registrierung"): nutzt denselben Module-Typ wie modelMarf248
// (struct Marf248, siehe dortiger Marf248CompactWidget-Kommentar), daher
// KEINE eigene .cpp-Datei - Deklaration/Definition liegen bewusst
// zusammen mit modelMarf248 in Marf248.cpp/Marf248.hpp statt in einer
// separaten Datei, um die dort lokal definierten Widget-Helfer
// (StageSlider, RangeKnob, TrangeButton, StageCurveTrimpot, ...)
// mitzuverwenden, ohne sie zu duplizieren oder zu exportieren.
extern Model* modelMarf248Compact;

// Fuenftes Modul "Europa" (Quad Function Generator mit VCA), eigene
// Datei src/Europa.cpp, Engine in src/EnvGen4.hpp.
extern Model* modelEuropa;

// Sechstes Modul "Minerva" (polyphoner Dual-Spannungsprozessor mit
// Kanalwahl), eigene Datei src/Minerva.cpp,
// Engine in src/VoltageProc.hpp.
extern Model* modelMinerva;

// Siebtes Modul "Elena" (digitaler CV-Prozessor:
// A/B-Transformationssektionen plus sechs Slew-Limiter
// C-H mit Auto-Pulse), eigene Datei src/Elena.cpp, Engine in
// src/CvProcElena.hpp.
extern Model* modelElena;

// Achtes Modul "Anna-Maria" (Lowpass Gate nach dem Modell von Parker/D'Angelo, DAFx-13;
// vier Kanaele), eigene Datei src/AnnaMaria.cpp, Engine in src/AnnaMariaLpg.hpp.
extern Model* modelAnnaMaria;

// Neuntes Modul "Ritornello" (Loop-Expander fuer Sofonisba/Lucia, interner
// Name Marf248LoopExpander): Begin/End-Schleifen mit Zaehler, Austrittsimpuls,
// Restwert und Panel-Ziffernanzeige. Reiner Jack-Traeger wie Amilcare/Bianca,
// eigene Dateien src/Marf248LoopExpander.hpp/.cpp, Logik in Afg (Marf248.hpp).
extern Model* modelMarf248LoopExpander;
