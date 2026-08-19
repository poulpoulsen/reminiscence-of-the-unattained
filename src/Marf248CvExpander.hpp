#pragma once
#include "plugin.hpp"

// Marf248CvExpander: zweiter Expander fuer Marf248 (nach dem Trigger-
// Expander), diesmal mit Poly-Kabeln statt Einzel-Jacks - passt fuer
// "pro Stage"-Groessen (16 Kanaele = Stage 1-16) deutlich kompakter als
// 16 einzelne Buchsen. Gleiches Muster wie Marf248TriggerExpander: reiner
// Jack-Traeger ohne eigene DSP-Logik, Marf248::process() liest/schreibt
// die Ports dieses Moduls direkt (siehe dortige Kommentare zum Expander-
// Mechanismus).
//
// Stage Time CV (Input, poly 16ch): additive CV pro Stage auf die
// aufgeloeste Stage-Zeit (resolvedTime[i] in Marf248::process(), NACH
// External/Internal-Time-Aufloesung, VOR dem Time-Multiplier - wird also
// von multiplierA/B mitskaliert, Entscheidung von Kinovice). Ein einzelner
// geteilter Attenuverter (Trimpot) skaliert alle 16 Kanaele gemeinsam,
// analog zum Time-Multiplier-CV-Muster im Hauptmodul (5V-Referenzspanne).
// Zusaetzlicher Offset-Trimpot (TIME_CV_OFFSET_PARAM) verschiebt den
// Nullpunkt der rohen CV VOR dem Attenuverter - macht so eine rein
// unipolare Quelle bipolar nutzbar (Kinovice-Entscheidung: eigener Regler
// statt fixem Unipolar-Menue-Modus, damit der Nullpunkt frei auf die
// tatsaechliche Quelle abgestimmt werden kann).
//
// Gate A/Gate B (Output, poly 16ch): Kanal i ist high (10V), sobald die
// jeweilige Section (A/B) aktuell Stage i adressiert (afgA/afgB.currentStage)
// - unabhaengig vom Run-Zustand. Optional (Kontextmenue, siehe
// regateGapEnabled) wird bei einem Stage-Wiedereintritt OHNE Indexwechsel
// (Pendulum-Wendepunkt-Wiederholung, Strobe auf dieselbe Adresse, Sustain/
// Enable-Freigabe) kurz auf 0V zurueckgesetzt, damit nachgeschaltete
// Huellkurvengeneratoren zuverlaessig neu triggern (siehe Afg::regateGen in
// Marf248.hpp).
struct Marf248CvExpander : Module {
	enum ParamId {
		TIME_CV_ATTEN_PARAM,
		// Frei einstellbarer Nullpunkt (Offset), wird VOR dem Attenuverter
		// von der rohen CV abgezogen - macht eine rein unipolare CV-Quelle
		// (z.B. 0-10V Envelope) bipolar nutzbar: Offset auf die Mitte der
		// Quelle stellen (z.B. 5V), dann verkuerzt die untere Haelfte und
		// verlaengert die obere Haelfte (bei positivem Attenuverter) - siehe
		// Marf248.cpp, Stage-Time-CV-Scan.
		TIME_CV_OFFSET_PARAM,
		// Slew-Curve-CV (analog zur Stage-Time-CV oben, aber fuer die
		// Kruemmung der Slew-Kurve statt der Stage-Zeit): Attenuverter
		// skaliert/invertiert die eingehende Poly-CV, gleiche 5V-
		// Referenzspanne wie bei Time CV (siehe Marf248.cpp,
		// Slew-Curve-CV-Scan).
		CURVE_CV_ATTEN_PARAM,
		// Globaler Bias-Knob, wirkt IMMER auf alle 16 Stages, auch OHNE
		// gepatchtes Kabel am CURVE_CV_INPUT - im Unterschied zum
		// TIME_CV_OFFSET_PARAM oben (der nur die Nullpunktlage einer
		// tatsaechlich angeschlossenen CV verschiebt und ohne Kabel
		// wirkungslos bleibt). Kinovice-Entscheidung: echter unabhaengiger
		// globaler Versatz statt eines reinen CV-Offsets.
		CURVE_GLOBAL_PARAM,
		// Ratchet-Warp-Blend (Session "Ratcheting", siehe MOD_RATCHET/
		// ratchetWarpPhase() in Marf248.hpp): analoges Muster zu
		// CURVE_CV_ATTEN_PARAM/CURVE_GLOBAL_PARAM oben, aber fuer die
		// Verzerrung der Ratchet-Sub-Trigger-Verteilung statt der Slew-
		// Kurve. Attenuverter skaliert die eingehende Poly-CV (siehe
		// WARP_CV_INPUT unten).
		WARP_CV_ATTEN_PARAM,
		// Globaler Blend-Bias, wirkt IMMER auf alle 16 Stages (auch ohne
		// gepatchtes Kabel an WARP_CV_INPUT) - analog zu CURVE_GLOBAL_PARAM
		// oben, JETZT bipolar -1..+1 (Nachtrag, Praezisierung nach erster
		// Ratcheting-Session): positiv = "bauchig" (Sub-Trigger dicht am
		// Stage-Anfang, Luecken wachsen zum Ende hin), negativ =
		// "durchhaengend" (Sub-Trigger dicht am Stage-Ende, Luecken am
		// Anfang), 0 = linear/gleichmaessig. Siehe ratchetWarpPhase() in
		// Marf248.hpp fuer die Potenz-Abbildung (ersetzt die anfaengliche,
		// symmetrische Smootherstep-Fassung).
		WARP_GLOBAL_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		STAGE_TIME_CV_INPUT,
		// Poly (16 = Stage 1-16): additive CV auf die Curve-Knob-Position
		// jeder Stage, VOR der Power-Berechnung in Marf248::process()
		// aufaddiert (siehe dortiger Kommentar). Wirkt zusaetzlich zur
		// per Stage eingestellten Curve-Trimpot-Position, unabhaengig
		// davon, ob MOD_SLEW_EXP/LOG gerade aktiv ist (nur dann aber
		// hoerbar, da applySlewCurve() bei Linear/Stepped ohnehin nicht
		// auf die Power zugreift).
		CURVE_CV_INPUT,
		// Ratchet-Warp-Blend-CV (Session "Ratcheting"): Poly (16 = Stage
		// 1-16), additiv auf WARP_GLOBAL_PARAM aufaddiert, VOR dem Clamp auf
		// -1..+1 in Marf248::process() - analog zum Curve-CV-Muster oben,
		// jetzt mit derselben bipolaren -1..+1-Semantik wie Curve.
		WARP_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		GATE_A_OUTPUT,
		GATE_B_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LINK_LIGHT,
		LIGHTS_LEN
	};

	// Kontextmenue-Option (Rechtsklick auf das Panel): steuert, ob ein
	// Stage-Wiedereintritt ohne Indexwechsel kurz auf 0V zieht (kurze
	// Retrigger-Luecke, ~5ms) oder das Gate einfach durchgehend haelt.
	// Default = kurze Luecke (Kinovice-Entscheidung), da die meisten
	// nachgeschalteten Huellkurven sonst nicht neu retriggern.
	bool regateGapEnabled = true;

	Marf248CvExpander() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(TIME_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Stage Time CV Attenuverter");
		configParam(TIME_CV_OFFSET_PARAM, -10.f, 10.f, 0.f, "Stage Time CV Offset", "V");
		configParam(CURVE_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Slew Curve CV Attenuverter");
		configParam(CURVE_GLOBAL_PARAM, -1.f, 1.f, 0.f, "Slew Curve globaler Bias");
		configParam(WARP_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Ratchet Warp Blend CV Attenuverter");
		// Bipolar -1..+1 (Nachtrag), Prozent-Anzeige weiterhin ueber
		// displayMultiplier=100 (jetzt -100%..+100%, positiv="bauchig",
		// negativ="durchhaengend", siehe Enum-Kommentar oben).
		configParam(WARP_GLOBAL_PARAM, -1.f, 1.f, 0.f, "Ratchet Warp Blend globaler Bias", "%", 0.f, 100.f);
		configInput(STAGE_TIME_CV_INPUT, "Stage Time CV (poly, 16 = Stage 1-16)");
		configInput(CURVE_CV_INPUT, "Slew Curve CV (poly, 16 = Stage 1-16)");
		configInput(WARP_CV_INPUT, "Ratchet Warp Blend CV (poly, 16 = Stage 1-16)");
		configOutput(GATE_A_OUTPUT, "Section A Gate (poly, 16 = Stage 1-16)");
		configOutput(GATE_B_OUTPUT, "Section B Gate (poly, 16 = Stage 1-16)");
	}

	// Wie beim Trigger-Expander: eigenstaendige Nachbar-Pruefung jeden
	// Frame, damit Outputs/Light beim Entfernen/Wegschieben des Marf248
	// sauber auf 0V zurueckfallen (Marf248 selbst schreibt die Gate-Werte
	// nur, wenn es diesen Expander erkennt - siehe Marf248::process()).
	void process(const ProcessArgs& args) override {
		Module* left = getLeftExpander().module;
		Module* right = getRightExpander().module;
		bool linked = (left && left->getModel() == modelMarf248)
			|| (right && right->getModel() == modelMarf248);

		if (!linked) {
			outputs[GATE_A_OUTPUT].setChannels(16);
			outputs[GATE_B_OUTPUT].setChannels(16);
			for (int c = 0; c < 16; c++) {
				outputs[GATE_A_OUTPUT].setVoltage(0.f, c);
				outputs[GATE_B_OUTPUT].setVoltage(0.f, c);
			}
		}
		lights[LINK_LIGHT].setBrightness(linked ? 1.f : 0.f);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "regateGapEnabled", json_boolean(regateGapEnabled));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* regateGapEnabledJ = json_object_get(rootJ, "regateGapEnabled");
		if (regateGapEnabledJ) {
			regateGapEnabled = json_boolean_value(regateGapEnabledJ);
		}
	}
};
