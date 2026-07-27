#pragma once
#include "plugin.hpp"

// modelMarf248 und modelMarf248TriggerExpander sind jetzt final in
// plugin.hpp deklariert (siehe dort) - die frueheren lokalen extern-
// Deklarationen an dieser Stelle sind damit ueberfluessig geworden.

// Marf248TriggerExpander: reiner Jack-Traeger ohne eigene DSP-Logik. Die
// eigentliche Verarbeitung (Clear/Reset Values ausloesen, Preset laden,
// Scale/Key setzen, Start-/End-of-Cycle ausgeben) passiert komplett in
// Marf248::process() - dieses Modul liest/schreibt nur seine eigenen
// Inputs/Outputs, wenn Marf248 links oder rechts andockt (siehe VCV
// Rack "Expander"-Mechanismus, offizielle Doku: Plugin API Guide #Expanders).
//
// Bewusst OHNE Message-Passing/Doppelpuffer: da dieses Modul keine eigene
// unabhaengige Verarbeitung hat, kann Marf248 seine Ports direkt lesen/
// schreiben, ohne Race-Conditions befuerchten zu muessen (siehe Rack-Doku:
// "you can implement its entire DSP algorithm in the [primary] module").
struct Marf248TriggerExpander : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		CLEAR_INPUT,
		RESET_VALUES_INPUT,
		// Poly-Umbau (ersetzt die vorherigen 12 Einzelbuchsen ENUMS
		// (PRESET_LOAD_INPUT, 12) - Breaking Change fuer bestehende Patches,
		// die auf die alten Einzel-Inputs gepatcht hatten): ein einzelner
		// polyphoner Eingang, Kanal p (0-11) laedt Preset p+1 sofort, ohne
		// den Arm-Mechanismus (Load-Taster) des Hauptpanels - siehe
		// Marf248::process(). Unbenutzte/unpatchte Kanaele liefern 0V
		// (getPolyVoltage() faellt automatisch darauf zurueck), loesen also
		// keinen Trigger aus.
		PRESET_LOAD_POLY_INPUT,
		SCALE_MAJOR_INPUT,
		SCALE_MINOR_INPUT,
		SCALE_CHROMATIC_INPUT,
		// Key: kontinuierliche CV statt Trigger (0-10V -> C..B), analog zur
		// Stage-Address-CV im Hauptmodul - Spannungshoehe bestimmt den Wert
		// direkt, kein Arm/Commit noetig (siehe Marf248::process()).
		KEY_CV_INPUT,
		// SEXT (Buchla-Book-Begriff fuer externe Strobe-Pulse): loest exakt
		// dieselbe Aktion aus wie der jeweilige Panel-Strobe-Taster
		// (STAGE_ADDR_STROBE_A/B_PARAM in Marf248) - inklusive der
		// bestehenden Continuous-Sperre, da Afg::applyStageAddress() im
		// Continuous-Zweig vorzeitig returned und strobePressed dort
		// ignoriert (siehe Marf248.hpp). Getrennt pro Section, analog zu
		// den getrennten Panel-Tastern.
		STROBE_A_INPUT,
		STROBE_B_INPUT,
		// Start/Stop-Gate pro Section: reine Pegel-Logik (Gate high =
		// running, Gate low = kein Einfluss mehr), unabhaengig vom
		// Start/Stop-Taster (RUN_A/B_PARAM) des Hauptmoduls - ODER-
		// verknuepft (siehe Afg::process() in Marf248.hpp: effectiveRunning
		// = running || runGateHigh). Der Taster-Zustand selbst bleibt dabei
		// unveraendert erhalten: sobald das Gate wieder auf Low faellt,
		// greift wieder genau der Zustand, den der Taster zuletzt
		// eingestellt hat. Schmitt-Trigger-Hysterese (0.1V/1V) gegen
		// Gate-Jitter am Umschaltpunkt, siehe Afg::runGateTrigger.
		RUN_GATE_A_INPUT,
		RUN_GATE_B_INPUT,
		// Stage SKIP (Poly, Kanal i = Stage i+1): Pegel-Override statt Trigger,
		// analog zu RUN_GATE_A/B_INPUT - Kanal-Spannung >= GATE_THRESHOLD (2V,
		// siehe Marf248::process()) erzwingt MOD_SKIP fuer die betreffende
		// Stage, unterhalb loescht es das Bit. Wirkt nur, solange die Buchse
		// gepatcht ist (isConnected()); dann hat das CV Vorrang und
		// ueberschreibt einen manuellen Klick auf STAGE_SKIP_PARAM in
		// Marf248::process() noch im selben Frame. Ohne Kabel bleibt exakt
		// der zuletzt per Klick gesetzte Zustand erhalten - identisches
		// Rueckfall-Verhalten wie bei RUN_GATE_A/B_INPUT.
		STAGE_SKIP_POLY_INPUT,
		// Stage FIRST/LAST/STOP/SUSTAIN/ENABLE/PULSE1/PULSE2 (je Poly, Kanal i
		// = Stage i+1): identisches Pegel-Override-Prinzip wie
		// STAGE_SKIP_POLY_INPUT oben - Kanal-Spannung >= GATE_THRESHOLD (2V)
		// erzwingt das jeweilige Bit fuer die betreffende Stage, unterhalb
		// loescht es das Bit. Nur wirksam, solange die Buchse gepatcht ist;
		// dann hat das CV Vorrang und ueberschreibt einen manuellen Klick auf
		// den jeweiligen STAGE_..._PARAM in Marf248::process() noch im
		// selben Frame. Ohne Kabel bleibt exakt der zuletzt per Klick
		// gesetzte Zustand erhalten. (SLOPE folgt weiter unten separat, siehe
		// STAGE_SLOPE_POLY_INPUT - Shape/Lin-Exp-Log bleibt als einziger
		// echter Mehrfach-Zyklus bewusst ohne CV-Override.)
		STAGE_FIRST_POLY_INPUT,
		STAGE_LAST_POLY_INPUT,
		STAGE_STOP_POLY_INPUT,
		STAGE_SUSTAIN_POLY_INPUT,
		STAGE_ENABLE_POLY_INPUT,
		STAGE_PULSE1_POLY_INPUT,
		STAGE_PULSE2_POLY_INPUT,
		// Stage SLOPE (Poly, Kanal i = Stage i+1): identisches Pegel-Override-
		// Prinzip wie oben. MOD_SLOPED ist entgegen einer frueheren Annahme
		// ein einfaches Ein/Aus-Bit (steuert nur, ob die Stage rampt oder
		// springt) - NICHT zu verwechseln mit dem Shape-Button (Lin/Exp/Log-
		// Zyklus, STAGE_SHAPE_PARAM), der bewusst weiterhin ohne CV-Override
		// bleibt, da er tatsaechlich ein 3-Zustands-Zyklus ist.
		STAGE_SLOPE_POLY_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		// Feuert, wenn die jeweilige Section (A/B) eine MOD_FIRST- bzw.
		// MOD_LAST-markierte Stage erreicht - unabhaengig von Cycle/
		// Pendulum-Richtung (kontinuierliche Flankenerkennung, siehe
		// Afg::process() in Marf248.hpp).
		START_CYCLE_A_OUTPUT,
		END_CYCLE_A_OUTPUT,
		START_CYCLE_B_OUTPUT,
		END_CYCLE_B_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		// Zeigt an, ob gerade ein Marf248 links oder rechts andockt. Ohne
		// Verbindung werden alle Outputs jeden Frame auf 0V gehalten (siehe
		// process()), damit beim Entfernen/Wegschieben des Hauptmoduls keine
		// haengengebliebene Spannung auf den Ausgaengen zurueckbleibt.
		LINK_LIGHT,
		LIGHTS_LEN
	};

	Marf248TriggerExpander() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configInput(CLEAR_INPUT, "Global Clear trigger");
		configInput(RESET_VALUES_INPUT, "Reset Values trigger");
		configInput(PRESET_LOAD_POLY_INPUT, "Load preset trigger (poly, ch. 1-12 = preset 1-12)");
		configInput(SCALE_MAJOR_INPUT, "Select Major scale trigger");
		configInput(SCALE_MINOR_INPUT, "Select Minor scale trigger");
		configInput(SCALE_CHROMATIC_INPUT, "Select Chromatic scale trigger");
		configInput(KEY_CV_INPUT, "Key select CV (0-10V = C..B)");
		configInput(STROBE_A_INPUT, "Section A external Strobe (SEXT)");
		configInput(STROBE_B_INPUT, "Section B external Strobe (SEXT)");
		configInput(RUN_GATE_A_INPUT, "Section A Start/Stop gate (high = run, OR'd with panel button)");
		configInput(RUN_GATE_B_INPUT, "Section B Start/Stop gate (high = run, OR'd with panel button)");
		configInput(STAGE_SKIP_POLY_INPUT, "Stage SKIP gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_FIRST_POLY_INPUT, "Stage FIRST gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_LAST_POLY_INPUT, "Stage LAST gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_STOP_POLY_INPUT, "Stage STOP gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_SUSTAIN_POLY_INPUT, "Stage SUSTAIN gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_ENABLE_POLY_INPUT, "Stage ENABLE gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_PULSE1_POLY_INPUT, "Stage PULSE1 gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_PULSE2_POLY_INPUT, "Stage PULSE2 gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_SLOPE_POLY_INPUT, "Stage SLOPE gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");

		configOutput(START_CYCLE_A_OUTPUT, "Section A Start of Cycle trigger");
		configOutput(END_CYCLE_A_OUTPUT, "Section A End of Cycle trigger");
		configOutput(START_CYCLE_B_OUTPUT, "Section B Start of Cycle trigger");
		configOutput(END_CYCLE_B_OUTPUT, "Section B End of Cycle trigger");
	}

	// Prueft jeden Frame selbststaendig, ob (noch) ein Marf248 unmittelbar
	// links oder rechts andockt. Ohne Verbindung werden Outputs/Light auf 0
	// gehalten - Marf248 selbst schreibt die eigentlichen Werte nur, wenn es
	// diesen Expander erkennt (siehe Marf248::process()), daher muss dieses
	// Modul sein eigenes "Aufraeumen" beim Verschieben/Entfernen des
	// Hauptmoduls uebernehmen (siehe Plugin API Guide #Expanders).
	void process(const ProcessArgs& args) override {
		Module* left = getLeftExpander().module;
		Module* right = getRightExpander().module;
		bool linked = (left && left->getModel() == modelMarf248)
			|| (right && right->getModel() == modelMarf248);

		if (!linked) {
			outputs[START_CYCLE_A_OUTPUT].setVoltage(0.f);
			outputs[END_CYCLE_A_OUTPUT].setVoltage(0.f);
			outputs[START_CYCLE_B_OUTPUT].setVoltage(0.f);
			outputs[END_CYCLE_B_OUTPUT].setVoltage(0.f);
		}
		lights[LINK_LIGHT].setBrightness(linked ? 1.f : 0.f);
	}
};
