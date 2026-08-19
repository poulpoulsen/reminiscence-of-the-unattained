#include "Marf248CvExpander.hpp"
#include "PanelLabel.hpp"

// Gleiche schlichte Panel-Optik wie die anderen Marf248-Module (siehe
// Marf248TriggerExpander.cpp fuer die identische lokale Kopie-Begruendung -
// PanelBackground bleibt bewusst dupliziert, da wortidentisch und daher
// ODR-unkritisch, im Unterschied zu PanelLabel).
struct PanelBackground : Widget {
	NVGcolor color = nvgRGB(228, 228, 226);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, color);
		nvgFill(args.vg);
	}
};

// PanelLabel-Struct, DEG_TO_RAD und COLOR_SECTION_A/COLOR_SECTION_B/
// COLOR_STAGEMOD kommen jetzt zentral aus PanelLabel.hpp (Session
// "Fehlende Labels auf Trigger Expander" - die vorherige lokale Kopie hier
// war eine von drei abweichenden PanelLabel-Definitionen und Teil eines
// ODR-Verstosses, siehe dortiger Kommentar).


struct Marf248CvExpanderWidget : ModuleWidget {
	// Siehe Marf248TriggerExpander.cpp fuer die Begruendung dieses Helfers.
	void addLabel(Vec posMM, const std::string& text, NVGcolor color) {
		addChild(new PanelLabel(posMM + Vec(0.f, 5.5f), text, color));
	}

	Marf248CvExpanderWidget(Marf248CvExpander* module) {
		setModule(module);

		// 12HP bleibt unveraendert: nur 8 Bedienelemente, das bestehende
		// Zeilenraster (14mm-Schritte, identisch zur stepY-Rechnung im
		// Trigger-Expander) hat hier reichlich Puffer nach unten - siehe
		// Reihen unten.
		const float HP = 12.f;
		box.size = Vec(HP * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

		PanelBackground* panelBg = new PanelBackground;
		panelBg->box.size = box.size;
		addChild(panelBg);

		PanelBorder* panelBorder = new PanelBorder;
		panelBorder->box.size = box.size;
		addChild(panelBorder);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Zweispaltig wie bisher: linke Spalte Stage Time CV, rechte Spalte
		// Slew Curve CV. Gate-Ausgaenge bleiben modulweit und sitzen mittig.
		float colTime = 15.f;
		float colCurve = 45.f;
		float colCenter = 30.f;

		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(colCenter, 10)), module, Marf248CvExpander::LINK_LIGHT));
		addChild(new PanelLabel(Vec(colCenter + 6.f, 8.f), "LINK", COLOR_STAGEMOD, 7.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE));

		// Zeilenraster: 14mm-Schritte (identische Rechnung wie im Trigger-
		// Expander bei fontSize=9 - Label-Versatz 5.5mm + Zeilenhoehe
		// ~3.7mm + naechstes Element ~4.2mm Freiraum = ca. 13.4mm minimal,
		// stepY=14mm mit Marge). Bei nur 3 Reihen + Gate-Reihen bleibt hier
		// deutlich mehr Puffer zu den unteren Schrauben als beim Trigger-
		// Expander (siehe dortiger Kommentar).
		const float stepY = 14.f;
		float y = 26.f;

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colTime, y)), module, Marf248CvExpander::STAGE_TIME_CV_INPUT));
		addLabel(Vec(colTime, y), "TIME CV", COLOR_STAGEMOD);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colCurve, y)), module, Marf248CvExpander::CURVE_CV_INPUT));
		addLabel(Vec(colCurve, y), "CURVE CV", COLOR_STAGEMOD);

		y += stepY;
		addParam(createParamCentered<Trimpot>(mm2px(Vec(colTime, y)), module, Marf248CvExpander::TIME_CV_ATTEN_PARAM));
		addLabel(Vec(colTime, y), "TIME ATTEN", COLOR_STAGEMOD);
		addParam(createParamCentered<Trimpot>(mm2px(Vec(colCurve, y)), module, Marf248CvExpander::CURVE_CV_ATTEN_PARAM));
		addLabel(Vec(colCurve, y), "CURVE ATTEN", COLOR_STAGEMOD);

		y += stepY;
		addParam(createParamCentered<Trimpot>(mm2px(Vec(colTime, y)), module, Marf248CvExpander::TIME_CV_OFFSET_PARAM));
		addLabel(Vec(colTime, y), "TIME OFFSET", COLOR_STAGEMOD);
		// Globaler Bias-Knob ist bewusst groesser (RoundBlackKnob statt
		// Trimpot, siehe .hpp) - Label-Versatz bleibt trotzdem einheitlich
		// bei 5.5mm (Knob-Radius ist kleiner als die 5.5mm, also weiterhin
		// ausreichend Abstand).
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colCurve, y)), module, Marf248CvExpander::CURVE_GLOBAL_PARAM));
		addLabel(Vec(colCurve, y), "CURVE BIAS", COLOR_STAGEMOD);

		// Ratchet-Warp-Blend (Session "Ratcheting", siehe MOD_RATCHET/
		// ratchetWarpPhase() in Marf248.hpp): analoge 2-Zeilen-Anordnung wie
		// Time/Curve oben, aber auf EINE Spalte verdichtet (colTime fuer den
		// CV-Eingang, colCurve fuer Attenuverter+Bias darunter), da bei nur
		// 12HP kein Platz fuer eine dritte Spalte bleibt - horizontal bleibt
		// dieses Modul unveraendert 12HP, die vom urspruenglichen Autor
		// erwaehnte "reichlich Puffer nach unten" wird stattdessen fuer die
		// beiden zusaetzlichen Zeilen genutzt (siehe Screw-Abstand unten).
		// Gate A/B ruecken dadurch zwei Zeilen tiefer.
		y += stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colTime, y)), module, Marf248CvExpander::WARP_CV_INPUT));
		addLabel(Vec(colTime, y), "WARP CV", COLOR_STAGEMOD);
		addParam(createParamCentered<Trimpot>(mm2px(Vec(colCurve, y)), module, Marf248CvExpander::WARP_CV_ATTEN_PARAM));
		addLabel(Vec(colCurve, y), "WARP ATTEN", COLOR_STAGEMOD);

		// colTime bleibt in dieser Zeile bewusst leer (kein zweiter Regler
		// noetig, siehe Kommentar oben) - der globale Bias sitzt weiterhin
		// in Spalte colCurve, direkt unter dem Attenuverter.
		y += stepY;
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colCurve, y)), module, Marf248CvExpander::WARP_GLOBAL_PARAM));
		addLabel(Vec(colCurve, y), "WARP BLEND", COLOR_STAGEMOD);

		// Zeilen-y ab hier: WARP CV=68, WARP BLEND=82, GATE A=96, GATE B=110
		// (stepY=14mm ab y=26 Startzeile). Label der letzten Zeile (GATE B)
		// endet bei ca. 115.5mm - numerisch
		// analog zur Trigger-Expander-Rechnung (siehe dortiger Kommentar:
		// stepY=14mm, unterste Buchsen-Schrauben bei RACK_GRID_HEIGHT-
		// RACK_GRID_WIDTH, identisch fuer jede HP-Breite) knapper Puffer zu
		// den Befestigungsschrauben als vorher, aber im selben Rahmen wie
		// beim Trigger-Expander bei 30HP - wie immer im Sandbox ohne
		// Rack-SDK nicht visuell bestaetigt, bitte lokal gegenchecken.
		y += stepY;
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colCenter, y)), module, Marf248CvExpander::GATE_A_OUTPUT));
		addLabel(Vec(colCenter, y), "GATE A", COLOR_SECTION_A);

		y += stepY;
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colCenter, y)), module, Marf248CvExpander::GATE_B_OUTPUT));
		addLabel(Vec(colCenter, y), "GATE B", COLOR_SECTION_B);
	}

	void appendContextMenu(Menu* menu) override {
		Marf248CvExpander* module = dynamic_cast<Marf248CvExpander*>(this->module);
		if (!module)
			return;

		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Retrigger-Luecke bei Stage-Wiedereintritt (~5ms)", "", &module->regateGapEnabled));
	}
};


Model* modelMarf248CvExpander = createModel<Marf248CvExpander, Marf248CvExpanderWidget>("Marf248CvExpander");
