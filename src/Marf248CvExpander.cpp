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


struct Marf248CvExpanderWidget : ModuleWidget {
	Marf248CvExpanderWidget(Marf248CvExpander* module) {
		setModule(module);

		// 12HP (statt bisher 6HP): zweite Spalte fuer die neue Slew-Curve-CV
		// (Poly-Input + Attenuverter + globaler Bias-Knob) neben der
		// bestehenden Stage-Time-CV-Spalte. Kinovice-Entscheidung, den
		// Expander bei dieser Gelegenheit gleich breiter anzulegen, da
		// absehbar weitere CV-Ein-/Ausgaenge dazukommen.
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

		// Jetzt zweispaltig: linke Spalte wie bisher (Stage Time CV), rechte
		// Spalte neu (Slew Curve CV). Gate-Ausgaenge bleiben modulweit (nicht
		// pro Spalte) und sitzen daher mittig zwischen beiden Spalten.
		float colTime = 15.f;
		float colCurve = 45.f;
		float colCenter = 30.f;

		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(colCenter, 10)), module, Marf248CvExpander::LINK_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colTime, 26)), module, Marf248CvExpander::STAGE_TIME_CV_INPUT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(colTime, 40)), module, Marf248CvExpander::TIME_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(colTime, 52)), module, Marf248CvExpander::TIME_CV_OFFSET_PARAM));

		// Slew Curve CV: Poly-Input + Attenuverter (klein, wie beim Time-CV-
		// Pendant) + globaler Bias-Knob (bewusst groesser/praesenter als die
		// beiden Trimpots, da er - anders als Attenuverter/Offset - auch ganz
		// ohne Kabel als eigenstaendiger Regler benutzt wird).
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colCurve, 26)), module, Marf248CvExpander::CURVE_CV_INPUT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(colCurve, 40)), module, Marf248CvExpander::CURVE_CV_ATTEN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colCurve, 56)), module, Marf248CvExpander::CURVE_GLOBAL_PARAM));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colCenter, 80)), module, Marf248CvExpander::GATE_A_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colCenter, 100)), module, Marf248CvExpander::GATE_B_OUTPUT));
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
