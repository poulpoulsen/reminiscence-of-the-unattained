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
		// Direkter Zugriff: Trigger p laedt Preset p+1 sofort, ohne den
		// Arm-Mechanismus (Load-Taster) des Hauptpanels - siehe
		// Marf248::process().
		ENUMS(PRESET_LOAD_INPUT, 12),
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
		INPUTS_LEN
	};
	enum OutputId {
		// Feuert, wenn die jeweilige Section (A/B) eine MOD_FIRST- bzw.
		// MOD_LAST-markierte Stage erreicht - unabhaengig von Cycle/
		// Pendulum-Richtung (siehe Afg::triggerStagePulses() in Marf248.hpp).
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
		for (int p = 0; p < 12; p++) {
			configInput(PRESET_LOAD_INPUT + p, string::f("Load preset %d trigger", p + 1));
		}
		configInput(SCALE_MAJOR_INPUT, "Select Major scale trigger");
		configInput(SCALE_MINOR_INPUT, "Select Minor scale trigger");
		configInput(SCALE_CHROMATIC_INPUT, "Select Chromatic scale trigger");
		configInput(KEY_CV_INPUT, "Key select CV (0-10V = C..B)");
		configInput(STROBE_A_INPUT, "Section A external Strobe (SEXT)");
		configInput(STROBE_B_INPUT, "Section B external Strobe (SEXT)");

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
