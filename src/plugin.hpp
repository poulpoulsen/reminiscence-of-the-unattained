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
