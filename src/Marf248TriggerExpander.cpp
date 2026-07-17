#include "Marf248TriggerExpander.hpp"

// Gleiche schlichte Panel-Optik wie Marf248 (kein SVG-Asset, nur eine
// eingefaerbte Flaeche + Rack-Standard-PanelBorder). Lokale Kopie statt
// gemeinsamem Header, da es sich um einen trivialen, headerlosen Widget
// handelt (kein ODR-Risiko: reine Klassendefinition ohne exportierte
// Symbole, siehe Marf248.cpp fuer das Original).
struct PanelBackground : Widget {
	NVGcolor color = nvgRGB(228, 228, 226);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, color);
		nvgFill(args.vg);
	}
};


struct Marf248TriggerExpanderWidget : ModuleWidget {
	Marf248TriggerExpanderWidget(Marf248TriggerExpander* module) {
		setModule(module);

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

		float colA = 16.f;
		float colB = 45.f;

		// Link-Licht: zeigt an, ob gerade ein Marf248 andockt (siehe
		// Marf248TriggerExpander::process()).
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec((colA + colB) / 2.f, 10)), module, Marf248TriggerExpander::LINK_LIGHT));

		// Reihe 1: Clear / Reset Values.
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, 20)), module, Marf248TriggerExpander::CLEAR_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, 20)), module, Marf248TriggerExpander::RESET_VALUES_INPUT));

		// Reihen 2-7: Preset Load 1-12, zwei Spalten (1,3,5,7,9,11 links /
		// 2,4,6,8,10,12 rechts), direkter Zugriff ohne Load-Arm-Taster.
		const float presetStartY = 30.f;
		const float presetStepY = 9.5f;
		for (int row = 0; row < 6; row++) {
			float y = presetStartY + row * presetStepY;
			int leftPreset = row * 2;
			int rightPreset = row * 2 + 1;
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, y)), module, Marf248TriggerExpander::PRESET_LOAD_INPUT + leftPreset));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, y)), module, Marf248TriggerExpander::PRESET_LOAD_INPUT + rightPreset));
		}

		// Reihe 8: Scale Major / Minor.
		float yScale1 = presetStartY + 6 * presetStepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, yScale1)), module, Marf248TriggerExpander::SCALE_MAJOR_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, yScale1)), module, Marf248TriggerExpander::SCALE_MINOR_INPUT));

		// Reihe 9: Scale Chromatic / Key CV.
		float yScale2 = yScale1 + presetStepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, yScale2)), module, Marf248TriggerExpander::SCALE_CHROMATIC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, yScale2)), module, Marf248TriggerExpander::KEY_CV_INPUT));

		// Reihe 10-11: Start-/End-of-Cycle, Section A dann B.
		float yCycleA = yScale2 + presetStepY;
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colA, yCycleA)), module, Marf248TriggerExpander::START_CYCLE_A_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colB, yCycleA)), module, Marf248TriggerExpander::END_CYCLE_A_OUTPUT));

		float yCycleB = yCycleA + presetStepY;
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colA, yCycleB)), module, Marf248TriggerExpander::START_CYCLE_B_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colB, yCycleB)), module, Marf248TriggerExpander::END_CYCLE_B_OUTPUT));
	}
};


Model* modelMarf248TriggerExpander = createModel<Marf248TriggerExpander, Marf248TriggerExpanderWidget>("Marf248TriggerExpander");
