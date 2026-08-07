#include "Marf248CvExpander.hpp"

// Gleiche schlichte Panel-Optik wie die anderen Marf248-Module (siehe
// Marf248TriggerExpander.cpp fuer die identische lokale Kopie-Begruendung).
struct PanelBackground : Widget {
	NVGcolor color = nvgRGB(228, 228, 226);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, color);
		nvgFill(args.vg);
	}
};

// ---------------------------------------------------------------------
// Panel-Beschriftungs-System (Session "Panel Labeling", Variante B - siehe
// Marf248TriggerExpander.cpp fuer die vollstaendige Begruendung/Historie,
// inkl. der Font-Groessen-Korrektur von 6 auf 9). Lokale Kopie hier aus
// demselben ODR-Grund wie PanelBackground oben - keine gemeinsame Header-
// Datei fuer diese trivialen, headerlosen Widgets.
struct PanelLabel : Widget {
	std::string text;
	NVGcolor color;
	float fontSize;
	int align;
	std::shared_ptr<window::Font> font;

	PanelLabel(Vec posMM, std::string text, NVGcolor color, float fontSize = 9.f,
	           int align = NVG_ALIGN_CENTER | NVG_ALIGN_TOP)
		: text(std::move(text)), color(color), fontSize(fontSize), align(align) {
		box.pos = mm2px(posMM);
		box.size = Vec(0.f, 0.f); // reines Text-Overlay, keine eigene Hitbox
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Nunito-Bold.ttf"));
	}

	void draw(const DrawArgs& args) override {
		if (!font || font->handle < 0)
			return; // Font fehlt - still ueberspringen statt crashen
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fontSize);
		nvgTextAlign(args.vg, align);
		nvgTextLetterSpacing(args.vg, 0.f);
		nvgFillColor(args.vg, color);
		nvgText(args.vg, 0.f, 0.f, text.c_str(), NULL);
	}
};

// Gleiche Farbpalette wie Marf248TriggerExpander.cpp, fuer konsistente
// Optik ueber alle drei Module hinweg. CvExpander nutzt davon nur zwei
// Kategorien: StageMod (tuerkis) fuer die modulweiten Time/Curve-CV-
// Regler (analog zu den Stage-Modifier-Poly-Inputs auf dem Trigger-
// Expander - beide wirken pro Stage, unabhaengig von Section A/B), sowie
// Section A/B (blau/orange) fuer die beiden Gate-Ausgaenge.
static const NVGcolor COLOR_SECTION_A = nvgRGB(30, 100, 200);
static const NVGcolor COLOR_SECTION_B = nvgRGB(210, 110, 20);
static const NVGcolor COLOR_STAGEMOD  = nvgRGB(0, 140, 130);


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
