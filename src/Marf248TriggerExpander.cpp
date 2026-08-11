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

// ---------------------------------------------------------------------
// Panel-Beschriftungs-System (Session "Panel Labeling", Variante B: reine
// Code/NanoVG-Labels statt SVG-Artwork).
//
// Bewusst als eigenstaendiges Widget statt Text direkt in draw() der
// ModuleWidget-Klasse: dadurch bleibt die Position jedes Labels exakt an
// derselben Stelle im Code wie die zugehoerige addInput()/addOutput()-
// Zeile - keine getrennte SVG-Datei, die bei Layout-Aenderungen von Hand
// synchron gehalten werden muesste.
//
// SCHRIFTGROESSE (Korrektur nach erstem Test, war zu klein): NVG-
// Fontgroessen sind in Rack-px angegeben, 1mm entspricht ca. 2.9528px
// (SVG_DPI 75 / MM_PER_IN 25.4). fontSize=6 (erster Entwurf) ergab nur
// ca. 1.4mm Kappenhoehe - kaum lesbar, wie von Kinovice bemaengelt.
// fontSize=9 (jetzt) ergibt ca. 2.1mm Kappenhoehe / ca. 3.7mm Zeilenhoehe
// - spuerbar besser lesbar, ohne das Layout komplett zu sprengen.
//
// WICHTIG: Nunito-Bold.ttf ist NICHT im Rack-SDK enthalten und liegt auch
// nicht bei den hochgeladenen Dateien - diese Datei muss von Kinovice noch
// besorgt und unter res/fonts/Nunito-Bold.ttf im Plugin-Verzeichnis
// abgelegt werden (Google Fonts, SIL Open Font License, kostenlos:
// https://fonts.google.com/specimen/Nunito -> Bold-Schnitt). Ohne diese
// Datei liefert loadFont() einen leeren Font (font->handle == -1) und
// draw() ueberspringt den Text still (kein Crash, aber keine sichtbare
// Beschriftung) - siehe Guard in PanelLabel::draw().
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
			return; // Font fehlt (siehe Kommentar oben) - still ueberspringen statt crashen
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fontSize);
		nvgTextAlign(args.vg, align);
		nvgTextLetterSpacing(args.vg, 0.f);
		nvgFillColor(args.vg, color);
		nvgText(args.vg, 0.f, 0.f, text.c_str(), NULL);
	}
};

// Farbkategorien (Kinovice-Entscheidung: "nach Sektion/Funktion
// eingefaerbt"). Fuenf Kategorien, ueber alle Reihen hinweg konsistent:
// - System: modulweite Housekeeping-Trigger (Clear/Reset/Preset)
// - Musical: Scale/Key - musikalische Funktionen, ebenfalls modulweit
// - Section A / Section B: alles was eine Section-Kennung traegt (auch
//   wenn poly, wie Address Memory Store/Recall)
// - StageMod: die 10 Stage-Modifier-Poly-Inputs (Skip...Mute) - wirken
//   modulweit auf beide Sections gleichzeitig, daher eigene Kategorie statt
//   A/B-Zuordnung.
static const NVGcolor COLOR_SYSTEM    = nvgRGB(70, 70, 68);
static const NVGcolor COLOR_MUSICAL   = nvgRGB(126, 87, 194);
static const NVGcolor COLOR_SECTION_A = nvgRGB(30, 100, 200);
static const NVGcolor COLOR_SECTION_B = nvgRGB(210, 110, 20);
static const NVGcolor COLOR_STAGEMOD  = nvgRGB(0, 140, 130);


struct Marf248TriggerExpanderWidget : ModuleWidget {
	// Kleiner Helfer, um addInput()/addOutput() und das zugehoerige Label
	// in einer Zeile zu buendeln - Position und Text bleiben dadurch
	// zusammen im Code, statt an zwei getrennten Stellen gepflegt zu werden.
	void addLabel(Vec posMM, const std::string& text, NVGcolor color) {
		// Label sitzt "direkt darunter": Buchsen-Radius (PJ301MPort)
		// ~4.2mm + 1.3mm Abstand = 5.5mm Versatz vom Buchsen-Mittelpunkt
		// zur Textoberkante.
		addChild(new PanelLabel(posMM + Vec(0.f, 5.5f), text, color));
	}

	Marf248TriggerExpanderWidget(Marf248TriggerExpander* module) {
		setModule(module);

		// 12HP -> 18HP -> 24HP -> 30HP (diese Session, "Panel Labeling",
		// zweiter Anlauf nach zu kleiner Schrift): die groessere,
		// lesbarere Schrift (siehe PanelLabel-Kommentar oben) braucht mehr
		// Platz - sowohl in der Hoehe (weniger, dafuer hoehere Reihen) als
		// auch in der Breite (5. Spalte, da bei nur 4 Spalten die
		// Zeilenzahl trotz hoeherer Reihen nicht ausgereicht haette, um
		// alle 31 Ein-/Ausgaenge unterzubringen - siehe Rechnung unten).
		// Hoehe ist bei Rack-Modulen fix (RACK_GRID_HEIGHT), also bleibt
		// nur die Breite als Stellschraube. FALLS diese Verbreiterung im
		// Case zu eng wird: Alternative waere zweizeilige Labels bei 24HP
		// belassen - dann bitte Rueckmeldung, das ist ein separater Umbau.
		const float HP = 30.f;
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

		// 5 Spalten, 27mm Pitch (vorher 4 Spalten a 29mm) - bei fontSize=9
		// ist die laengste Beschriftung ("ADDR RECALL B", 13 Zeichen) ca.
		// 22mm breit (Schaetzung: Zeichenbreite ~0.55*fontSize/2.9528mm),
		// passt mit Luft in die 27mm-Spalten.
		float colA = 16.f;
		float colB = 43.f;
		float colC = 70.f;
		float colD = 97.f;
		float colE = 124.f;

		// Link-Licht mittig ueber allen 5 Spalten, Label daneben statt
		// darunter (spart die Zeile, die sonst mit Reihe 1 kollidieren
		// wuerde).
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(colC, 10)), module, Marf248TriggerExpander::LINK_LIGHT));
		addChild(new PanelLabel(Vec(colC + 6.f, 8.f), "LINK", COLOR_SYSTEM, 7.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE));

		// ---------------------------------------------------------------
		// Zeilenraster: 7 Reihen a bis zu 5 Spalten (vorher 8 Reihen a 4
		// Spalten - reichte bei groesserer Schrift weder in der Hoehe noch
		// in der Spaltenzahl fuer alle 31 Elemente).
		//
		// stepY-Rechnung: Label-Versatz 5.5mm (addLabel()) + Zeilenhoehe bei
		// fontSize=9 ca. 3.7mm (9px * 1.2 Zeilenhoehen-Faktor / 2.9528) =
		// Label endet ca. 9.2mm unterhalb der Buchsenmitte. Naechste Buchse
		// braucht oberhalb ihrer Mitte wiederum ~4.2mm frei (PJ301M-Radius)
		// -> ca. 13.4mm Minimalabstand, stepY = 14mm gewaehlt (Marge).
		//
		// Reihe 7 (letzte) liegt bei y = 20 + 6*14 = 104mm, Label endet bei
		// ca. 113.2mm. Untere Befestigungsschrauben liegen (wie in
		// fruehreren Sessions ermittelt, RACK_GRID_HEIGHT-RACK_GRID_WIDTH,
		// unveraendert durch die HP-Erweiterung, da nur die X-Position von
		// der Breite abhaengt) bei ca. 123.4mm -> ca. 10mm Puffer.
		// Numerisch geprueft, aber wie immer im Sandbox ohne Rack-SDK nicht
		// visuell bestaetigt - bitte lokal gegenchecken.
		const float stepY = 14.f;
		float y = 20.f;

		// Reihe 1: Clear / Reset Values / Preset Load (System, grau).
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, y)), module, Marf248TriggerExpander::CLEAR_INPUT));
		addLabel(Vec(colA, y), "CLEAR", COLOR_SYSTEM);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, y)), module, Marf248TriggerExpander::RESET_VALUES_INPUT));
		addLabel(Vec(colB, y), "RESET VAL", COLOR_SYSTEM);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colC, y)), module, Marf248TriggerExpander::PRESET_LOAD_POLY_INPUT));
		addLabel(Vec(colC, y), "PRESET", COLOR_SYSTEM);
		// Run Gate A/B (Kinovice-Wunsch: in Reihe 1 statt Reihe 4, naeher an
		// den uebrigen globalen Housekeeping-Controls). Farblich bleiben sie
		// trotzdem klar A/B-zugeordnet, auch wenn sie jetzt neben den
		// System-Controls stehen.
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colD, y)), module, Marf248TriggerExpander::RUN_GATE_A_INPUT));
		addLabel(Vec(colD, y), "RUN GATE A", COLOR_SECTION_A);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colE, y)), module, Marf248TriggerExpander::RUN_GATE_B_INPUT));
		addLabel(Vec(colE, y), "RUN GATE B", COLOR_SECTION_B);

		// Reihe 2: Scale Major/Minor/Chromatic + Key CV (Musical, violett).
		y += stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, y)), module, Marf248TriggerExpander::SCALE_MAJOR_INPUT));
		addLabel(Vec(colA, y), "SCALE MAJ", COLOR_MUSICAL);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, y)), module, Marf248TriggerExpander::SCALE_MINOR_INPUT));
		addLabel(Vec(colB, y), "SCALE MIN", COLOR_MUSICAL);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colC, y)), module, Marf248TriggerExpander::SCALE_CHROMATIC_INPUT));
		addLabel(Vec(colC, y), "SCALE CHR", COLOR_MUSICAL);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colD, y)), module, Marf248TriggerExpander::KEY_CV_INPUT));
		addLabel(Vec(colD, y), "KEY CV", COLOR_MUSICAL);

		// Reihe 3: Start-/End-of-Cycle, Section A dann B (Outputs).
		y += stepY;
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colA, y)), module, Marf248TriggerExpander::START_CYCLE_A_OUTPUT));
		addLabel(Vec(colA, y), "SOC A", COLOR_SECTION_A);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colB, y)), module, Marf248TriggerExpander::END_CYCLE_A_OUTPUT));
		addLabel(Vec(colB, y), "EOC A", COLOR_SECTION_A);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colC, y)), module, Marf248TriggerExpander::START_CYCLE_B_OUTPUT));
		addLabel(Vec(colC, y), "SOC B", COLOR_SECTION_B);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colD, y)), module, Marf248TriggerExpander::END_CYCLE_B_OUTPUT));
		addLabel(Vec(colD, y), "EOC B", COLOR_SECTION_B);

		// Reihe 4: SEXT (externe Strobe A/B) + Address Memory Recall A/B
		// direkt daneben (Kinovice-Wunsch: Recall A neben Strobe B, Recall B
		// direkt danach). Spalte E war bisher leer (siehe aeltere Fassung
		// dieses Kommentars) - jetzt belegt mit der Voltage-External-
		// Kanalwahl-CV (Session "VOLT_EXT/TIME_EXT-Kanalwahl per CV"), da
		// hier thematisch am naechsten zur bisherigen Recall/Strobe-Reihe.
		// Run Gate A/B sitzt seit vorheriger Aenderung in Reihe 1.
		y += stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, y)), module, Marf248TriggerExpander::STROBE_A_INPUT));
		addLabel(Vec(colA, y), "STROBE A", COLOR_SECTION_A);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, y)), module, Marf248TriggerExpander::STROBE_B_INPUT));
		addLabel(Vec(colB, y), "STROBE B", COLOR_SECTION_B);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colC, y)), module, Marf248TriggerExpander::ADDR_RECALL_A_POLY_INPUT));
		addLabel(Vec(colC, y), "ADDR RECALL A", COLOR_SECTION_A);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colD, y)), module, Marf248TriggerExpander::ADDR_RECALL_B_POLY_INPUT));
		addLabel(Vec(colD, y), "ADDR RECALL B", COLOR_SECTION_B);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colE, y)), module, Marf248TriggerExpander::STAGE_VOLTEXT_CH_POLY_INPUT));
		addLabel(Vec(colE, y), "VOLT EXT CH", COLOR_STAGEMOD);

		// Reihe 5: Advance A/B + Skip/First/Last (Stage-Adress-Modifier).
		y += stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, y)), module, Marf248TriggerExpander::ADVANCE_A_INPUT));
		addLabel(Vec(colA, y), "ADVANCE A", COLOR_SECTION_A);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, y)), module, Marf248TriggerExpander::ADVANCE_B_INPUT));
		addLabel(Vec(colB, y), "ADVANCE B", COLOR_SECTION_B);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colC, y)), module, Marf248TriggerExpander::STAGE_SKIP_POLY_INPUT));
		addLabel(Vec(colC, y), "SKIP", COLOR_STAGEMOD);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colD, y)), module, Marf248TriggerExpander::STAGE_FIRST_POLY_INPUT));
		addLabel(Vec(colD, y), "FIRST", COLOR_STAGEMOD);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colE, y)), module, Marf248TriggerExpander::STAGE_LAST_POLY_INPUT));
		addLabel(Vec(colE, y), "LAST", COLOR_STAGEMOD);

		// Reihe 6: Stop/Sustain/Enable/Pulse1/Pulse2 (Stage-Zustands- und
		// Puls-Modifier).
		y += stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, y)), module, Marf248TriggerExpander::STAGE_STOP_POLY_INPUT));
		addLabel(Vec(colA, y), "STOP", COLOR_STAGEMOD);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, y)), module, Marf248TriggerExpander::STAGE_SUSTAIN_POLY_INPUT));
		addLabel(Vec(colB, y), "SUSTAIN", COLOR_STAGEMOD);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colC, y)), module, Marf248TriggerExpander::STAGE_ENABLE_POLY_INPUT));
		addLabel(Vec(colC, y), "ENABLE", COLOR_STAGEMOD);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colD, y)), module, Marf248TriggerExpander::STAGE_PULSE1_POLY_INPUT));
		addLabel(Vec(colD, y), "PULSE1", COLOR_STAGEMOD);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colE, y)), module, Marf248TriggerExpander::STAGE_PULSE2_POLY_INPUT));
		addLabel(Vec(colE, y), "PULSE2", COLOR_STAGEMOD);

		// Reihe 7: Slope/Mute (Stage-Modifier) + Adress-Speicher Store A/B.
		// Recall A/B sitzt seit Kinovice-Wunsch jetzt in Reihe 4 neben den
		// Strobe-Buchsen. Spalte E war dadurch frei geworden - jetzt belegt
		// mit der Time-External-Kanalwahl-CV (Session "VOLT_EXT/TIME_EXT-
		// Kanalwahl per CV"), als Gegenstueck zu VOLT EXT CH in Reihe 4.
		y += stepY;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, y)), module, Marf248TriggerExpander::STAGE_SLOPE_POLY_INPUT));
		addLabel(Vec(colA, y), "SLOPE", COLOR_STAGEMOD);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, y)), module, Marf248TriggerExpander::STAGE_MUTE_POLY_INPUT));
		addLabel(Vec(colB, y), "MUTE", COLOR_STAGEMOD);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colC, y)), module, Marf248TriggerExpander::ADDR_STORE_A_POLY_INPUT));
		addLabel(Vec(colC, y), "ADDR STORE A", COLOR_SECTION_A);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colD, y)), module, Marf248TriggerExpander::ADDR_STORE_B_POLY_INPUT));
		addLabel(Vec(colD, y), "ADDR STORE B", COLOR_SECTION_B);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colE, y)), module, Marf248TriggerExpander::STAGE_TIMEEXT_CH_POLY_INPUT));
		addLabel(Vec(colE, y), "TIME EXT CH", COLOR_STAGEMOD);
	}
};


Model* modelMarf248TriggerExpander = createModel<Marf248TriggerExpander, Marf248TriggerExpanderWidget>("Marf248TriggerExpander");
