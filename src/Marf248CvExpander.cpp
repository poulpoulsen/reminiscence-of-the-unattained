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

		// 6HP dank Polyphonie (4 Jacks statt 32 bei Einzel-Kabeln).
		const float HP = 6.f;
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

		// Schmale 6HP-Front: eine einzelne zentrierte Spalte statt der
		// Zweispalten-Optik des Trigger-Expanders.
		float col = 15.f;

		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(col, 10)), module, Marf248CvExpander::LINK_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col, 26)), module, Marf248CvExpander::STAGE_TIME_CV_INPUT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col, 40)), module, Marf248CvExpander::TIME_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col, 52)), module, Marf248CvExpander::TIME_CV_OFFSET_PARAM));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col, 76)), module, Marf248CvExpander::GATE_A_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col, 98)), module, Marf248CvExpander::GATE_B_OUTPUT));
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
