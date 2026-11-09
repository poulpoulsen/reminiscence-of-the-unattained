#include "SofonisbaLoopExpander.hpp"
#include "PanelLabel.hpp"

// Gleiche schlichte Panel-Optik wie Marf248/Amilcare/Bianca. WORTIDENTISCHE
// lokale Kopie (Begruendung siehe Marf248TriggerExpander.cpp: headerloser,
// trivialer Widget; nicht aendern, ohne die anderen Module mitzuziehen).
struct PanelBackground : Widget {
	NVGcolor color = nvgRGB(228, 228, 226);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, color);
		nvgFill(args.vg);
	}
};

// Ziffernanzeige je Section (Muster TempoBpmDisplay aus Marf248.cpp: reines
// Text-Widget ohne Interaktion, Outline-Trick, gleiche Amber-Farbe). Liest nur
// restShown/blinkTimer vom Modul (beschrieben von Marf248::process()):
//   blinkTimer > 0   -> "0" (Austritt gerade erfolgt, ca. 0,3 s)
//   restShown > 0    -> Ziffer 1..10 (verbleibende Durchlaeufe der innersten
//                       Schleife, wie LOOP_REST_x_OUTPUT)
//   sonst            -> "-" (keine Schleife / endlos / Pendulum)
// Der Strich wird als Rechteck gezeichnet statt als Textzeichen, damit er nicht
// von einem Glyph in der Schrift abhaengt. module bleibt im Rack-Browser-
// Vorschaubild nullptr - dann steht dort eine 3 als Platzhalter.
struct LoopRestDisplay : TransparentWidget {
	Marf248LoopExpander* module = nullptr;
	int section = 0; // 0 = Section A, 1 = Section B

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 1.5f);
		nvgFillColor(args.vg, nvgRGB(40, 40, 44));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		int digit = 3; // Platzhalter im Browser
		if (module) {
			if (module->blinkTimer[section] > 0.f) {
				// Echtes Blinken: LOOP_BLINK_PERIOD an, LOOP_BLINK_PERIOD aus, ...
				// beginnend und endend mit "an" (bei 0,5 s / 0,1 s: an aus an
				// aus an). Die "aus"-Phasen zeigen nur den Rahmen.
				float elapsed = LOOP_BLINK_SECONDS - module->blinkTimer[section];
				bool on = ((int) (elapsed / LOOP_BLINK_PERIOD) % 2) == 0;
				if (!on) {
					return;
				}
				digit = 0;
			}
			else {
				digit = module->restShown[section];
				if (digit < 0) digit = 0;
				if (digit > 10) digit = 10;
				if (digit == 0) digit = -1; // Strich
			}
		}

		float cx = box.size.x / 2.f;
		float cy = box.size.y / 2.f;

		if (digit < 0) {
			float w = 14.f;
			float h = 2.6f;
			nvgBeginPath(args.vg);
			nvgRect(args.vg, cx - w / 2.f - 0.8f, cy - h / 2.f - 0.8f, w + 1.6f, h + 1.6f);
			nvgFillColor(args.vg, nvgRGB(20, 20, 20));
			nvgFill(args.vg);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, cx - w / 2.f, cy - h / 2.f, w, h);
			nvgFillColor(args.vg, nvgRGB(235, 170, 60));
			nvgFill(args.vg);
			return;
		}

		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font || !font->handle) {
			return;
		}
		std::string text = string::f("%d", digit);
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 24.f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		nvgFillColor(args.vg, nvgRGB(20, 20, 20));
		nvgText(args.vg, cx - 0.8f, cy, text.c_str(), NULL);
		nvgText(args.vg, cx + 0.8f, cy, text.c_str(), NULL);
		nvgText(args.vg, cx, cy - 0.8f, text.c_str(), NULL);
		nvgText(args.vg, cx, cy + 0.8f, text.c_str(), NULL);

		nvgFillColor(args.vg, nvgRGB(235, 170, 60));
		nvgText(args.vg, cx, cy, text.c_str(), NULL);
	}
};


struct Marf248LoopExpanderWidget : ModuleWidget {
	// Buchse + Label in einer Zeile gebuendelt (gleicher Versatz wie bei
	// Amilcare: Label sitzt direkt unter der Buchse).
	void addLabel(Vec posMM, const std::string& text, NVGcolor color) {
		addChild(new PanelLabel(posMM + Vec(0.f, 5.5f), text, color));
	}

	Marf248LoopExpanderWidget(Marf248LoopExpander* module) {
		setModule(module);

		// 12HP (60,96mm): zwei Spalten im 27mm-Pitch wie Amilcare, symmetrisch
		// um die Panelmitte (cx +- 13,5mm).
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

		const float cx = HP * 5.08f / 2.f;
		const float colA = cx - 13.5f;
		const float colB = cx + 13.5f;

		// Modulname zentriert wie bei allen Modulen (Schriftgroesse 13, y = 3,5).
		addChild(new PanelLabel(Vec(cx, 3.5f), "RITORNELLO", COLOR_SYSTEM, 13.f));

		// Link-Licht + Label in einer eigenen Zeile unter dem Namen (links
		// daneben ist bei 12HP kein Platz, anders als bei Amilcare).
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(cx - 5.f, 12.f)), module, Marf248LoopExpander::LINK_LIGHT));
		addChild(new PanelLabel(Vec(cx - 1.f, 10.f), "LINK", COLOR_SYSTEM, 7.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE));

		// Section-Kopf + Ziffernanzeige (22 x 11mm, Zentrum y = 28mm).
		addChild(new PanelLabel(Vec(colA, 17.f), "SECTION A", COLOR_SECTION_A));
		addChild(new PanelLabel(Vec(colB, 17.f), "SECTION B", COLOR_SECTION_B));
		for (int s = 0; s < 2; s++) {
			LoopRestDisplay* disp = new LoopRestDisplay;
			disp->box.size = mm2px(Vec(22.f, 11.f));
			disp->box.pos = mm2px(Vec(s == 0 ? colA : colB, 28.f)).minus(disp->box.size.div(2));
			disp->module = module;
			disp->section = s;
			addChild(disp);
		}

		// Ergebnis-Block: Rest-CV (Reihe 1), Austrittsimpuls (Reihe 2),
		// Gesamtschleife fertig (Reihe 3), je Section eine Spalte. Drei Reihen
		// im 12-mm-Raster (y = 44/56/68), damit der Abstand zum Config-Block bleibt
		// bleibt.
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colA, 44.f)), module, Marf248LoopExpander::LOOP_REST_A_OUTPUT));
		addLabel(Vec(colA, 44.f), "REST A", COLOR_SECTION_A);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colB, 44.f)), module, Marf248LoopExpander::LOOP_REST_B_OUTPUT));
		addLabel(Vec(colB, 44.f), "REST B", COLOR_SECTION_B);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colA, 56.f)), module, Marf248LoopExpander::LOOP_EXIT_A_OUTPUT));
		addLabel(Vec(colA, 56.f), "EXIT A", COLOR_SECTION_A);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colB, 56.f)), module, Marf248LoopExpander::LOOP_EXIT_B_OUTPUT));
		addLabel(Vec(colB, 56.f), "EXIT B", COLOR_SECTION_B);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colA, 68.f)), module, Marf248LoopExpander::LOOP_DONE_A_OUTPUT));
		addLabel(Vec(colA, 68.f), "DONE A", COLOR_SECTION_A);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colB, 68.f)), module, Marf248LoopExpander::LOOP_DONE_B_OUTPUT));
		addLabel(Vec(colB, 68.f), "DONE B", COLOR_SECTION_B);

		// Konfigurations-Block (gilt fuer beide Sections): Begin/End als Paar,
		// Count mittig darunter. Stage-Struktur -> COLOR_STAGEMOD wie
		// FIRST/LAST bei Amilcare.
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, 90.f)), module, Marf248LoopExpander::LOOP_BEGIN_POLY_INPUT));
		addLabel(Vec(colA, 90.f), "LOOP BEGIN", COLOR_STAGEMOD);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, 90.f)), module, Marf248LoopExpander::LOOP_END_POLY_INPUT));
		addLabel(Vec(colB, 90.f), "LOOP END", COLOR_STAGEMOD);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cx, 106.f)), module, Marf248LoopExpander::LOOP_COUNT_POLY_INPUT));
		addLabel(Vec(cx, 106.f), "LOOP COUNT", COLOR_STAGEMOD);
	}
};


Model* modelMarf248LoopExpander = createModel<Marf248LoopExpander, Marf248LoopExpanderWidget>("Marf248LoopExpander");
