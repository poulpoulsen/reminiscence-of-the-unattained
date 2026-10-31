#pragma once
#include "plugin.hpp"
#include "Marf248.hpp"

// Ritornello (interner Name Marf248LoopExpander): Loop-Expander fuer
// Sofonisba/Lucia. Wie Amilcare/Bianca ein reiner Jack-Traeger ohne eigene
// DSP-Logik: die Schleifenlogik liegt in Afg (Marf248.hpp), die Anbindung in
// Marf248::process() (liest die Eingaenge hier direkt, schreibt Ausgaenge und
// Anzeigezustand direkt hierher - Begruendung "kein Message-Passing" siehe
// Marf248TriggerExpander.hpp). modelMarf248LoopExpander ist in plugin.hpp
// deklariert.
//
// Konzept und Entscheidungen: doc/Expander-Konzept-Loop-Nachtrag_DE.md.

// Blinkende 0 in der Panel-Anzeige beim Austritt aus einer Schleife: Gesamtdauer
// und Dauer einer An- bzw. Aus-Phase. Bei 0,5 s / 0,1 s ergibt das an aus an
// aus an. Die Gesamtdauer sollte ein ungerades Vielfaches der Phase sein, damit
// das Blinken mit "an" endet.
static const float LOOP_BLINK_SECONDS = 0.5f;
static const float LOOP_BLINK_PERIOD = 0.1f;

struct Marf248LoopExpander : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		// Begin-/End-Markierung je Stage (Poly-Gate, Kanal i = Stage i+1,
		// Pegel >= 2 V = Markierung gesetzt, wie die Stage-Modifier-Gates von
		// Amilcare). Gelten fuer BEIDE Sections (stageModifier[] ist ebenfalls
		// modulweit geteilt). Paarung per Klammerung, siehe Afg::loopFlags.
		LOOP_BEGIN_POLY_INPUT,
		LOOP_END_POLY_INPUT,
		// Zaehler je End-Stage (Poly-CV, Kanal i = Stage i+1): 1 V pro Zone,
		// 0-1 V = endlos, 1-2 V = 1 Durchlauf ... 9-10 V = 9 Durchlaeufe. Nicht
		// gepatchte/fehlende Kanaele = 0 V = endlos. Gemeinsam fuer Section A
		// und B (getrennte Zaehlerstaende in den beiden Afg-Instanzen).
		LOOP_COUNT_POLY_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		// Austrittsimpuls (1 ms) bei jedem regulaeren Austritt aus einer
		// Schleife, unabhaengig von MOD_MUTE (siehe Afg::loopExitGen).
		LOOP_EXIT_A_OUTPUT,
		LOOP_EXIT_B_OUTPUT,
		// Restwert der innersten Schleife: 1 V pro verbleibendem Durchlauf
		// (einschliesslich des laufenden), 0 V = keine Schleife/endlos/
		// Pendulum. Blinkt bewusst nicht (das tut nur die Panel-Anzeige).
		LOOP_REST_A_OUTPUT,
		LOOP_REST_B_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		// Zeigt an, ob gerade ein Marf248 links oder rechts andockt (gleiche
		// Funktion wie bei Amilcare/Bianca).
		LINK_LIGHT,
		LIGHTS_LEN
	};

	// Anzeigezustand je Section (0 = A, 1 = B). Wird ausschliesslich von
	// Marf248::process() beschrieben (auch der Blink-Timer wird dort
	// heruntergezaehlt), das Widget liest nur - keine GUI-Zugriffe aus dem
	// Echtzeit-Thread, kein Schreiben aus zwei Threads (siehe process() unten:
	// dieses Modul raeumt nur auf, wenn kein Host andockt).
	//   restShown: verbleibende Durchlaeufe der innersten Schleife (0..9),
	//              0 = "-" (keine Schleife / endlos / Pendulum).
	//   blinkTimer: > 0 = Austritt gerade erfolgt, Anzeige zeigt eine 0.
	int restShown[2] = {0, 0};
	float blinkTimer[2] = {0.f, 0.f};

	Marf248LoopExpander() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configInput(LOOP_BEGIN_POLY_INPUT, "Loop Begin gate (poly, ch. 1-16 = stage 1-16, high marks the first stage of a loop)");
		configInput(LOOP_END_POLY_INPUT, "Loop End gate (poly, ch. 1-16 = stage 1-16, high marks the last stage of a loop)");
		configInput(LOOP_COUNT_POLY_INPUT, "Loop Count CV (poly, ch. 1-16 = stage 1-16 of the End stage, 1 V per zone: 0-1V = endless, 1-2V = 1 pass ... 9-10V = 9 passes)");

		configOutput(LOOP_EXIT_A_OUTPUT, "Section A Loop Exit trigger");
		configOutput(LOOP_EXIT_B_OUTPUT, "Section B Loop Exit trigger");
		configOutput(LOOP_REST_A_OUTPUT, "Section A remaining passes of innermost loop (1 V per pass, 0 V = no loop/endless)");
		configOutput(LOOP_REST_B_OUTPUT, "Section B remaining passes of innermost loop (1 V per pass, 0 V = no loop/endless)");
	}

	// Prueft jeden Frame selbststaendig, ob (noch) ein Marf248 unmittelbar
	// links oder rechts andockt (Muster wie Marf248TriggerExpander::process()).
	// Ohne Verbindung werden Outputs, Anzeige und Light zurueckgesetzt - die
	// eigentlichen Werte schreibt nur Marf248, solange es diesen Expander
	// erkennt. Beide Schreibzugriffe schliessen sich damit zeitlich aus.
	void process(const ProcessArgs& args) override {
		Module* left = getLeftExpander().module;
		Module* right = getRightExpander().module;
		bool linked = dynamic_cast<Marf248Host*>(left) || dynamic_cast<Marf248Host*>(right);

		if (!linked) {
			outputs[LOOP_EXIT_A_OUTPUT].setVoltage(0.f);
			outputs[LOOP_EXIT_B_OUTPUT].setVoltage(0.f);
			outputs[LOOP_REST_A_OUTPUT].setVoltage(0.f);
			outputs[LOOP_REST_B_OUTPUT].setVoltage(0.f);
			for (int s = 0; s < 2; s++) {
				restShown[s] = 0;
				blinkTimer[s] = 0.f;
			}
		}
		lights[LINK_LIGHT].setBrightness(linked ? 1.f : 0.f);
	}
};
