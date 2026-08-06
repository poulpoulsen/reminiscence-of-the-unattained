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

		// 12HP -> 18HP: die 7 neuen Stage-Modifier-Poly-Inputs (First/Last/
		// Stop/Sustain/Enable/Pulse1/Pulse2, analog zu STAGE_SKIP_POLY_INPUT)
		// brauchen eine 3. Spalte - siehe Marf248TriggerExpander.hpp.
		// 18HP -> 24HP: die 2 neuen ADVANCE_A/B_INPUT (Clock-quantisiertes
		// Advance) brauchen eine 4. Spalte. Unten ist kein Platz mehr fuer
		// eine komplett neue Reihe (nur noch 8,4mm Puffer zu den
		// Befestigungsschrauben, siehe Reihen 9-11 weiter unten) - daher
		// werden die beiden neuen Buchsen stattdessen in die bereits
		// bestehende RUN_GATE-Reihe eingefuegt (dort war Spalte C/D bisher
		// frei), statt eine neue Reihe zu erzwingen.
		const float HP = 24.f;
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
		// 3. Spalte im gleichen Abstand wie colA-colB (29mm), fuer die
		// Stage-Modifier-Poly-Inputs unten.
		float colC = 74.f;
		// 4. Spalte, gleicher 29mm-Abstand, nur fuer die RUN_GATE-Reihe
		// genutzt (ADVANCE_A/B_INPUT) - alle anderen Reihen bleiben bei
		// maximal 3 Spalten, der Rest der neuen Panel-Breite bleibt bewusst
		// leer (Reserve, analog zum bereits bestehenden ~18mm-Layout-Puffer
		// zwischen Normalize/Trange-Reihe und Range-Knob auf dem Hauptpanel).
		float colD = 103.f;

		// Link-Licht: zeigt an, ob gerade ein Marf248 andockt (siehe
		// Marf248TriggerExpander::process()).
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec((colA + colB) / 2.f, 10)), module, Marf248TriggerExpander::LINK_LIGHT));

		// Reihe 1: Clear / Reset Values.
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, 20)), module, Marf248TriggerExpander::CLEAR_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, 20)), module, Marf248TriggerExpander::RESET_VALUES_INPUT));

		// Reihen ab hier: 8.8mm-Raster (zurueck von den zwischenzeitlichen
		// 9.5mm) - fuer die neue 12. Reihe (Adress-Speicher Store/Recall,
		// siehe unten) reicht der Puffer bei 9.5mm nicht mehr aus (Reihe 11
		// laege bereits bei 115mm, eine 12. Reihe bei 124.5mm wuerde die
		// unteren Befestigungsschrauben bei ~123.4mm touchieren). Mit 8.8mm
		// liegt Reihe 11 bei 108mm und die neue Reihe 12 bei 116.8mm -
		// 6.6mm Puffer zu den Schrauben, numerisch geprueft (kein Rack-SDK
		// im Sandbox, visuell also unbestaetigt). Aendert nur Pixel-
		// Positionen, keine Enum-Reihenfolge - bestehende Patch-Kabel sind
		// davon nicht betroffen.
		const float stepY = 8.8f;

		// Reihe 2: Preset Load (Poly, Kanal 1-12 = Preset 1-12) - mittig
		// zwischen den beiden Spalten, da nur noch eine Buchse.
		float yPreset = 20.f + stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec((colA + colB) / 2.f, yPreset)), module, Marf248TriggerExpander::PRESET_LOAD_POLY_INPUT));

		// Reihe 3: Scale Major / Minor.
		float yScale1 = yPreset + stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, yScale1)), module, Marf248TriggerExpander::SCALE_MAJOR_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, yScale1)), module, Marf248TriggerExpander::SCALE_MINOR_INPUT));

		// Reihe 4: Scale Chromatic / Key CV.
		float yScale2 = yScale1 + stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, yScale2)), module, Marf248TriggerExpander::SCALE_CHROMATIC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, yScale2)), module, Marf248TriggerExpander::KEY_CV_INPUT));

		// Reihe 5-6: Start-/End-of-Cycle, Section A dann B.
		float yCycleA = yScale2 + stepY;
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colA, yCycleA)), module, Marf248TriggerExpander::START_CYCLE_A_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colB, yCycleA)), module, Marf248TriggerExpander::END_CYCLE_A_OUTPUT));

		float yCycleB = yCycleA + stepY;
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colA, yCycleB)), module, Marf248TriggerExpander::START_CYCLE_B_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colB, yCycleB)), module, Marf248TriggerExpander::END_CYCLE_B_OUTPUT));

		// Reihe 7: SEXT - externe Strobe-Pulse Section A / B (siehe
		// Marf248TriggerExpander.hpp und Marf248::process()).
		float ySext = yCycleB + stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, ySext)), module, Marf248TriggerExpander::STROBE_A_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, ySext)), module, Marf248TriggerExpander::STROBE_B_INPUT));

		// Reihe 8 (NEU): Start/Stop-Gate Section A / B (Pegel, ODER-
		// verknuepft mit dem Panel-Taster - siehe Marf248TriggerExpander.hpp
		// und Afg::process() in Marf248.hpp).
		float yRunGate = ySext + stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, yRunGate)), module, Marf248TriggerExpander::RUN_GATE_A_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, yRunGate)), module, Marf248TriggerExpander::RUN_GATE_B_INPUT));
		// Advance A/B (Clock-quantisiertes Advance) in derselben Reihe wie
		// RUN_GATE: thematisch verwandt (beide betreffen die Lauf-Steuerung
		// der jeweiligen Section) und Spalte C/D war hier bisher frei -
		// braucht daher keine eigene neue Reihe (siehe HP-Kommentar oben).
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colC, yRunGate)), module, Marf248TriggerExpander::ADVANCE_A_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colD, yRunGate)), module, Marf248TriggerExpander::ADVANCE_B_INPUT));

		// Reihe 9: Stage-Adress-Modifier Poly-Gates (Skip/First/Last) - jetzt
		// zu dritt in einer Reihe dank 3. Spalte, statt Skip allein mittig
		// wie zuvor (siehe Marf248TriggerExpander.hpp und Marf248::process()).
		float yStageSkip = yRunGate + stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, yStageSkip)), module, Marf248TriggerExpander::STAGE_SKIP_POLY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, yStageSkip)), module, Marf248TriggerExpander::STAGE_FIRST_POLY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colC, yStageSkip)), module, Marf248TriggerExpander::STAGE_LAST_POLY_INPUT));

		// Reihe 10 (NEU): Stage-Zustands-Modifier Poly-Gates (Stop/Sustain/
		// Enable), gleiches Prinzip wie Reihe 9.
		float yStageState = yStageSkip + stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, yStageState)), module, Marf248TriggerExpander::STAGE_STOP_POLY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, yStageState)), module, Marf248TriggerExpander::STAGE_SUSTAIN_POLY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colC, yStageState)), module, Marf248TriggerExpander::STAGE_ENABLE_POLY_INPUT));

		// Reihe 11: Stage-Pulse-Modifier Poly-Gates (Pulse1/Pulse2) plus
		// SLOPE in der 3. Spalte. Spalte D war hier bisher frei (nur in
		// Reihe 8 und 12 genutzt) - nimmt jetzt MUTE auf, ohne dass eine
		// neue Reihe noetig wird (siehe Marf248TriggerExpander.hpp).
		float yStagePulse = yStageState + stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, yStagePulse)), module, Marf248TriggerExpander::STAGE_PULSE1_POLY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, yStagePulse)), module, Marf248TriggerExpander::STAGE_PULSE2_POLY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colC, yStagePulse)), module, Marf248TriggerExpander::STAGE_SLOPE_POLY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colD, yStagePulse)), module, Marf248TriggerExpander::STAGE_MUTE_POLY_INPUT));

		// Reihe 12 (NEU): Adress-Speicher Store/Recall (siehe
		// Marf248TriggerExpander.hpp und Marf248::process()) - Store A/B und
		// Recall A/B nebeneinander in derselben Reihe, gleiches 4-Spalten-
		// Prinzip wie Reihe 8 (RUN_GATE + ADVANCE). Durch das auf 8.8mm
		// verdichtete Zeilenraster (siehe stepY oben) passt diese Reihe noch
		// mit 6.6mm Puffer zu den unteren Befestigungsschrauben.
		float yAddrMem = yStagePulse + stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, yAddrMem)), module, Marf248TriggerExpander::ADDR_STORE_A_POLY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, yAddrMem)), module, Marf248TriggerExpander::ADDR_STORE_B_POLY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colC, yAddrMem)), module, Marf248TriggerExpander::ADDR_RECALL_A_POLY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colD, yAddrMem)), module, Marf248TriggerExpander::ADDR_RECALL_B_POLY_INPUT));
	}
};


Model* modelMarf248TriggerExpander = createModel<Marf248TriggerExpander, Marf248TriggerExpanderWidget>("Marf248TriggerExpander");
