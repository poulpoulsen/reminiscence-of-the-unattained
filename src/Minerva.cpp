#include "plugin.hpp"
#include "PanelLabel.hpp"
#include "VoltageProc.hpp"
#include "DcBlock.hpp"
#include <history.hpp>

// ---------------------------------------------------------------------
// "Minerva": polyphoner Dual-Spannungsprozessor
// (zwei identische Sektionen, je Va*K + Vb*(1-M) + Vc*M + Voset).
//
// Polyphon: jede Sektion rechnet bis zu 16 Kanaele, JEDER Kanal hat
// eigene K/M/Voset-Werte (96 echte Rack-Params). Die GUI zeigt nur den
// gewaehlten Kanal (gemeinsamer Wahlschalter fuer beide Sektionen,
// Muster wie Lucia: pendingCh/displayedCh, kein Param, per JSON
// gespeichert). 16 Widget-Sets pro Regler liegen uebereinander, nur eines
// ist sichtbar.
//
// Alles steht im anonymen Namespace (interne Bindung): Lehre aus dem
// PanelLabel-ODR-Vorfall - lokale Structs koennen so nie mit gleichnamigen
// Structs aus anderen .cpp-Dateien der .so kollidieren. Nur das Model ist
// exportiert.
// ---------------------------------------------------------------------

namespace {

using simd::float_4;

static constexpr int NUM_CH = 16;
enum ParamType { PT_K, PT_M, PT_VOSET, NUM_PT };

// Index eines Params: je Sektion drei zusammenhaengende 16er-Bloecke.
static inline int paramIndex(int section, int type, int ch) {
	return (section * NUM_PT + type) * NUM_CH + ch;
}

// Index des "+.5"-Schalters (siehe Enum-Kommentar: separater Block hinter
// den 96 Regler-Params, daher nicht Teil von paramIndex()).
static inline int plus5Param(int section, int ch) {
	return 6 * NUM_CH + section * NUM_CH + ch;
}

struct Minerva : Module {
	// Enum-Disziplin: nur am Ende erweitern. K/M/Voset je Sektion sind ein
	// zusammenhaengender 16er-Block, siehe paramIndex().
	enum ParamId {
		K_A_PARAM,
		M_A_PARAM = K_A_PARAM + NUM_CH,
		VOSET_A_PARAM = M_A_PARAM + NUM_CH,
		K_B_PARAM = VOSET_A_PARAM + NUM_CH,
		M_B_PARAM = K_B_PARAM + NUM_CH,
		VOSET_B_PARAM = M_B_PARAM + NUM_CH,
		// Spaeter ergaenzt ("+.5"-Schalter auf Vb, je Sektion und Kanal):
		// bewusst ANS ENDE angehaengt, damit die Indizes der bisherigen 96
		// Params (und damit gespeicherte Patches) unveraendert bleiben.
		PLUS5_A_PARAM = VOSET_B_PARAM + NUM_CH,
		PLUS5_B_PARAM = PLUS5_A_PARAM + NUM_CH,
		NUM_PARAMS = PLUS5_B_PARAM + NUM_CH
	};
	// Reihenfolge je Sektion: Va, Vb, M-CV, Vc (Sektion B = Sektion A + 4).
	enum InputId {
		VA_A_INPUT,
		VB_A_INPUT,
		MCV_A_INPUT,
		VC_A_INPUT,
		VA_B_INPUT,
		VB_B_INPUT,
		MCV_B_INPUT,
		VC_B_INPUT,
		NUM_INPUTS
	};
	enum OutputId {
		OUT_A_OUTPUT,
		OUT_B_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightId {
		NUM_LIGHTS
	};

	// Reiner GUI-Zustand (kein Param, kein History-Eintrag, siehe Lucia).
	int pendingCh = 0;
	int displayedCh = 0;
	// Kanalzahl je Sektion: 0 = Auto (Maximum der angeschlossenen Eingaenge,
	// mindestens 1), 1..16 = fest.
	int channelsOverride[2] = {0, 0};
	// Modulweite Einstellungen, nur im JSON (keine Params, gespeicherte Patches
	// bleiben kompatibel). Gelten fuer beide Sektionen und alle Kanaele.
	// mLimited: false = M unbegrenzt (Standard), true = M hart auf 0..1.
	bool mLimited = false;
	// outputClip: VoltageProc::CLIP_OFF (Standard) / CLIP_SOFT / CLIP_HARD.
	int outputClip = VoltageProc::CLIP_OFF;

	// Schaltbarer DC-Blocker je Sektion (Kontextmenue, Standard aus, ca. 10 Hz).
	// Reihenfolge am Ausgang: Signalanteil -> DC-Blocker -> + Festanteil (Oset und
	// +5-Vb-Anteil) -> Ausgangsbegrenzung. Gespeichert im JSON (keine Params).
	bool dcBlock[2] = {false, false};
	float dcR = 0.f;           // Filterkoeffizient, siehe DcBlock::dcBlockCoeff
	float dcSampleRate = 0.f;  // Abtastrate, fuer die dcR berechnet wurde
	float dcX[2][NUM_CH] = {}; // Filterzustand je Sektion und Kanal
	float dcY[2][NUM_CH] = {};

	Minerva() {
		// Absicherung: plus5Param() und Enum muessen uebereinstimmen.
		static_assert(PLUS5_A_PARAM == 6 * NUM_CH, "plus5Param() passt nicht zum Enum");
		static_assert(PLUS5_B_PARAM == 7 * NUM_CH, "plus5Param() passt nicht zum Enum");
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const char* secName[2] = {"A", "B"};
		for (int s = 0; s < 2; s++) {
			for (int c = 0; c < NUM_CH; c++) {
				configParam(paramIndex(s, PT_K, c), -1.f, 1.f, 0.f,
				            string::f("Section %s K (channel %d)", secName[s], c + 1));
				configParam(paramIndex(s, PT_M, c), -1.f, 2.f, 0.5f,
				            string::f("Section %s M (channel %d)", secName[s], c + 1));
				configParam(paramIndex(s, PT_VOSET, c), 0.f, 1.f, 0.f,
				            string::f("Section %s Oset (channel %d)", secName[s], c + 1),
				            " V", 0.f, VoltageProc::VOSET_MAX_VOLTS);
				configSwitch(plus5Param(s, c), 0.f, 1.f, 0.f,
				             string::f("Section %s +5 V on Vb (channel %d)", secName[s], c + 1),
				             {"Off", "On"});
			}
			configInput(VA_A_INPUT + s * 4, string::f("Section %s Va", secName[s]));
			configInput(VB_A_INPUT + s * 4, string::f("Section %s Vb", secName[s]));
			configInput(MCV_A_INPUT + s * 4, string::f("Section %s M CV", secName[s]));
			configInput(VC_A_INPUT + s * 4, string::f("Section %s Vc", secName[s]));
			configOutput(OUT_A_OUTPUT + s, string::f("Section %s Vout", secName[s]));
		}
	}

	// Unbelegter Eingang = 0 V (Kinovice-Entscheidung). Mono-Eingang wird
	// auf alle Kanaele verteilt (getPolyVoltageSimd).
	static inline float_4 readIn(Input& in, int c) {
		if (!in.isConnected())
			return float_4(0.f);
		return in.getPolyVoltageSimd<float_4>(c);
	}

	void processSection(int s) {
		Input& inVa = inputs[VA_A_INPUT + s * 4];
		Input& inVb = inputs[VB_A_INPUT + s * 4];
		Input& inM = inputs[MCV_A_INPUT + s * 4];
		Input& inVc = inputs[VC_A_INPUT + s * 4];
		Output& out = outputs[OUT_A_OUTPUT + s];

		int n = channelsOverride[s];
		if (n <= 0) {
			n = std::max(std::max(inVa.getChannels(), inVb.getChannels()),
			             std::max(std::max(inM.getChannels(), inVc.getChannels()), 1));
		}
		n = rack::clamp(n, 1, NUM_CH);
		out.setChannels(n);

		for (int c = 0; c < n; c += 4) {
			float k[4], m[4], o[4], p5[4];
			for (int i = 0; i < 4; i++) {
				int ch = c + i; // immer < 16, da c Vielfaches von 4 und n <= 16
				k[i] = params[paramIndex(s, PT_K, ch)].getValue();
				m[i] = params[paramIndex(s, PT_M, ch)].getValue();
				o[i] = params[paramIndex(s, PT_VOSET, ch)].getValue();
				p5[i] = params[plus5Param(s, ch)].getValue();
			}
			// Signalanteil (Va*K + Crossfader) und Festanteil (Oset und der +5-Vb-Anteil
			// 5 V*(1-M), bewusste Gleichanteile) getrennt: der DC-Blocker soll nur den
			// Signalanteil bearbeiten.
			float_4 signalPart, staticPart;
			VoltageProc::processParts(readIn(inVa, c), readIn(inVb, c), readIn(inVc, c),
			                          readIn(inM, c), float_4::load(k), float_4::load(m),
			                          float_4::load(o), float_4::load(p5), mLimited,
			                          signalPart, staticPart);
			if (dcBlock[s]) {
				float_4 xp = float_4::load(&dcX[s][c]);
				float_4 yp = float_4::load(&dcY[s][c]);
				signalPart = DcBlock::dcBlockStep(signalPart, xp, yp, dcR);
				xp.store(&dcX[s][c]);
				yp.store(&dcY[s][c]);
			}
			else {
				// Zustand mitfuehren, damit beim Zuschalten kein Altzustand wirkt.
				signalPart.store(&dcX[s][c]);
				float_4(0.f).store(&dcY[s][c]);
			}
			float_4 v = signalPart + staticPart;
			// Ausgangsbegrenzung nur bei Bedarf (Aus = kein Aufruf), nach dem Blocker.
			if (outputClip != VoltageProc::CLIP_OFF)
				v = VoltageProc::limitOutput(v, outputClip);
			out.setVoltageSimd(v, c);
		}
	}

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != dcSampleRate) {
			dcSampleRate = args.sampleRate;
			dcR = DcBlock::dcBlockCoeff(dcSampleRate);
		}
		processSection(0);
		processSection(1);
	}

	void onReset() override {
		pendingCh = 0;
		displayedCh = 0;
		channelsOverride[0] = 0;
		channelsOverride[1] = 0;
		mLimited = false;
		outputClip = VoltageProc::CLIP_OFF;
		for (int s = 0; s < 2; s++) {
			dcBlock[s] = false;
			for (int c = 0; c < NUM_CH; c++) {
				dcX[s][c] = 0.f;
				dcY[s][c] = 0.f;
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "pendingCh", json_integer(pendingCh));
		json_object_set_new(rootJ, "displayedCh", json_integer(displayedCh));
		json_object_set_new(rootJ, "channelsA", json_integer(channelsOverride[0]));
		json_object_set_new(rootJ, "channelsB", json_integer(channelsOverride[1]));
		json_object_set_new(rootJ, "mLimited", json_boolean(mLimited));
		json_object_set_new(rootJ, "outputClip", json_integer(outputClip));
		// DC-Blocker je Sektion (A, B) als 0/1
		json_t* dcJ = json_array();
		for (int s = 0; s < 2; s++)
			json_array_append_new(dcJ, json_integer(dcBlock[s] ? 1 : 0));
		json_object_set_new(rootJ, "dcBlock", dcJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* j = json_object_get(rootJ, "pendingCh");
		if (j)
			pendingCh = rack::clamp((int) json_integer_value(j), 0, NUM_CH - 1);
		j = json_object_get(rootJ, "displayedCh");
		if (j)
			displayedCh = rack::clamp((int) json_integer_value(j), 0, NUM_CH - 1);
		j = json_object_get(rootJ, "channelsA");
		if (j)
			channelsOverride[0] = rack::clamp((int) json_integer_value(j), 0, NUM_CH);
		j = json_object_get(rootJ, "channelsB");
		if (j)
			channelsOverride[1] = rack::clamp((int) json_integer_value(j), 0, NUM_CH);
		// Fehlende Schluessel (aeltere Patches): Standard = unbegrenzt / Aus.
		j = json_object_get(rootJ, "mLimited");
		mLimited = j ? json_is_true(j) : false;
		j = json_object_get(rootJ, "outputClip");
		outputClip = j ? rack::clamp((int) json_integer_value(j), (int) VoltageProc::CLIP_OFF, (int) VoltageProc::CLIP_HARD)
		               : (int) VoltageProc::CLIP_OFF;
		// Fehlt der Schluessel (aelterer Patch), bleibt der DC-Blocker aus.
		j = json_object_get(rootJ, "dcBlock");
		if (j && json_is_array(j)) {
			for (int s = 0; s < 2; s++) {
				json_t* e = json_array_get(j, s);
				dcBlock[s] = (e && json_integer_value(e) != 0);
			}
		}
	}
};


// ---------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------

// Schlichte Panel-Flaeche wie in den uebrigen Modulen (lokale Kopie mit
// eigenem Namen, siehe Kommentar oben zum anonymen Namespace).
struct MinervaPanelBackground : Widget {
	NVGcolor color = nvgRGB(228, 228, 226);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, color);
		nvgFill(args.vg);
	}
};

// Rein dekorativer Rahmen je Sektion. TransparentWidget, damit er keine
// Maus-Events der Regler darunter abfaengt.
struct MinervaSectionFrame : TransparentWidget {
	NVGcolor color = nvgRGB(120, 120, 120);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 3.f);
		nvgStrokeColor(args.vg, color);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}
};

// Kanal-Auswahlknob mit Zentrumsziffer (1-16). Wie StageSelectKnob in
// Marf248.cpp: KEIN Param, schreibt direkt auf module->pendingCh. Ziffer
// gruen, sobald angewaehlter == angezeigter Kanal, sonst amber.
struct MinervaSelectKnob : OpaqueWidget {
	Minerva* module = nullptr;
	float dragAccumPx = 0.f;

	MinervaSelectKnob() {
		box.size = mm2px(Vec(14.f, 14.f));
	}

	void onButton(const ButtonEvent& e) override {
		OpaqueWidget::onButton(e);
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
		}
	}

	void onDragStart(const DragStartEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		dragAccumPx = 0.f;
		APP->window->cursorLock();
	}

	void onDragEnd(const DragEndEvent& e) override {
		APP->window->cursorUnlock();
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (!module)
			return;
		// 4 mm Mausweg pro Kanal-Schritt, Maus nach oben = Wert steigt.
		dragAccumPx -= e.mouseDelta.y;
		float pxPerStep = mm2px(4.f);
		while (dragAccumPx >= pxPerStep) {
			module->pendingCh = std::min(module->pendingCh + 1, NUM_CH - 1);
			dragAccumPx -= pxPerStep;
		}
		while (dragAccumPx <= -pxPerStep) {
			module->pendingCh = std::max(module->pendingCh - 1, 0);
			dragAccumPx += pxPerStep;
		}
	}

	void draw(const DrawArgs& args) override {
		float r = box.size.x / 2.f;
		Vec c = Vec(r, r);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r - 1.f);
		nvgFillColor(args.vg, nvgRGB(40, 40, 44));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		int pending = module ? module->pendingCh : 0;
		int displayed = module ? module->displayedCh : 0;
		bool isActive = (pending == displayed);
		NVGcolor textColor = isActive ? nvgRGB(90, 220, 110) : nvgRGB(235, 170, 60);

		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Nunito-Bold.ttf"));
		if (font && font->handle >= 0) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 15.f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, textColor);
			std::string label = string::f("%d", pending + 1);
			nvgText(args.vg, c.x, c.y, label.c_str(), NULL);
		}
	}
};

// Aktivieren-Button: uebertraegt pendingCh -> displayedCh. Rot solange die
// Auswahl noch nicht uebernommen ist, gruen wenn sie mit dem sichtbaren
// Kanal uebereinstimmt (wie StageActivateButton).
struct MinervaActivateButton : OpaqueWidget {
	Minerva* module = nullptr;

	MinervaActivateButton() {
		box.size = mm2px(Vec(8.f, 8.f));
	}

	void onButton(const ButtonEvent& e) override {
		OpaqueWidget::onButton(e);
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (module) {
				module->displayedCh = module->pendingCh;
			}
			e.consume(this);
		}
	}

	void draw(const DrawArgs& args) override {
		bool isActive = module && (module->pendingCh == module->displayedCh);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 1.5f);
		nvgFillColor(args.vg, isActive ? nvgRGB(90, 220, 110) : nvgRGB(220, 70, 70));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}
};

// Uebernimmt den Wert eines Reglers/Schalters fuer alle 16 Kanaele
// derselben Sektion und Art, als EIN Undo-Schritt (ComplexAction), analog
// zum Stage-Broadcast der Marf-Module. Kein Undo-Eintrag, falls sich nichts
// aendert.
static void minervaApplyToAllChannels(ParamWidget* w, int baseParamId) {
	ParamQuantity* pq = w->getParamQuantity();
	if (!w->module || !pq)
		return;
	float value = pq->getValue();

	history::ComplexAction* complex = new history::ComplexAction;
	complex->name = "apply Minerva value to all channels";
	bool changed = false;
	for (int i = 0; i < NUM_CH; i++) {
		int id = baseParamId + i;
		float oldValue = w->module->params[id].getValue();
		if (oldValue == value)
			continue;
		history::ParamChange* h = new history::ParamChange;
		h->name = "change parameter";
		h->moduleId = w->module->id;
		h->paramId = id;
		h->oldValue = oldValue;
		h->newValue = value;
		complex->push(h);
		APP->engine->setParamValue(w->module, id, value);
		changed = true;
	}
	if (changed) {
		APP->history->push(complex);
	}
	else {
		delete complex;
	}
}

// Regler mit Rechtsklick-Eintrag "Apply to all 16 channels".
struct MinervaParamKnob : RoundSmallBlackKnob {
	int baseParamId = 0; // erster Param des 16er-Blocks (Kanal 1)

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Apply to all 16 channels", "", [this]() { minervaApplyToAllChannels(this, baseParamId); }));
	}
};

// Einfacher Textschalter (2 Zustaende), hier fuer "+.5" auf Vb. Klick
// schaltet um (Basisklasse app::Switch), gleiche Optik wie ElenaToggle.
struct MinervaToggle : app::Switch {
	int baseParamId = -1;
	std::string textOff = "OFF";
	std::string textOn = "ON";
	NVGcolor colorOn = nvgRGB(220, 150, 40);

	MinervaToggle() {
		box.size = mm2px(Vec(13.f, 5.f));
	}

	void appendContextMenu(Menu* menu) override {
		if (baseParamId < 0)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Apply to all 16 channels", "", [this]() { minervaApplyToAllChannels(this, baseParamId); }));
	}

	void draw(const DrawArgs& args) override {
		ParamQuantity* pq = getParamQuantity();
		bool on = pq && pq->getValue() > 0.5f;

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 1.5f);
		nvgFillColor(args.vg, on ? colorOn : nvgRGB(205, 205, 200));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 220));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Nunito-Bold.ttf"));
		if (font && font->handle >= 0) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 8.f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, on ? nvgRGB(255, 255, 255) : nvgRGB(60, 60, 60));
			const std::string& t = on ? textOn : textOff;
			nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f + 0.5f, t.c_str(), NULL);
		}
	}
};


struct MinervaWidget : ModuleWidget {
	Minerva* mod = nullptr;
	// [Sektion][Reglerart][Kanal] - alle 96 Widgets liegen uebereinander,
	// step() blendet alle bis auf den angezeigten Kanal aus.
	MinervaParamKnob* knobs[2][NUM_PT][NUM_CH] = {};
	// "+.5"-Schalter je Sektion und Kanal, ebenfalls uebereinander
	MinervaToggle* plus5Widgets[2][NUM_CH] = {};

	void addLabel(Vec posMM, const std::string& text, NVGcolor color) {
		addChild(new PanelLabel(posMM, text, color));
	}

	MinervaWidget(Minerva* module) {
		setModule(module);
		mod = module;

		const float HP = 14.f;
		box.size = Vec(HP * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		const float W_MM = 71.12f; // 14 HP
		const float CX = W_MM / 2.f;

		MinervaPanelBackground* panelBg = new MinervaPanelBackground;
		panelBg->box.size = box.size;
		addChild(panelBg);

		PanelBorder* panelBorder = new PanelBorder;
		panelBorder->box.size = box.size;
		addChild(panelBorder);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Titel. Selector-Beschriftung bewusst grau (COLOR_SYSTEM): Blau ist
		// in PanelLabel.hpp bereits Section A.
		addChild(new PanelLabel(Vec(CX, 3.5f), "MINERVA", COLOR_SYSTEM, 13.f));
		addChild(new PanelLabel(Vec(CX, 9.2f), "DUAL VOLTAGE PROCESSOR", COLOR_SYSTEM, 6.f));

		// Kanalwahl (gemeinsam fuer beide Sektionen), Mitte y = 19 mm.
		const float selX = 22.f, actX = 49.f, selY = 19.f;
		MinervaSelectKnob* sel = new MinervaSelectKnob;
		sel->module = module;
		sel->box.pos = mm2px(Vec(selX, selY)) - sel->box.size / 2.f;
		addChild(sel);
		MinervaActivateButton* act = new MinervaActivateButton;
		act->module = module;
		act->box.pos = mm2px(Vec(actX, selY)) - act->box.size / 2.f;
		addChild(act);
		addLabel(Vec(selX, 27.f), "CHANNEL", COLOR_SYSTEM);
		addLabel(Vec(actX, 27.f), "ACTIVATE", COLOR_SYSTEM);

		// Zwei Sektionen, je 42 mm hoch: A ab y=33, B ab y=79 (Ende 121 mm,
		// unterhalb davon liegen die Befestigungsschrauben bei ca. 123 mm).
		const float frameTop[2] = {33.f, 79.f};
		const float frameH = 42.f;
		const NVGcolor secColor[2] = {COLOR_SECTION_A, COLOR_SECTION_B};
		const char* secTitle[2] = {"SECTION A", "SECTION B"};

		const float knobX[3] = {15.f, CX, W_MM - 15.f};
		const char* knobLabel[3] = {"K", "M", "OSET"};
		const float jackX[5] = {9.5f, 22.5f, CX, W_MM - 22.5f, W_MM - 9.5f};
		const char* jackLabel[5] = {"Va", "Vb", "M CV", "Vc", "OUT"};

		for (int s = 0; s < 2; s++) {
			float f = frameTop[s];
			NVGcolor col = secColor[s];

			MinervaSectionFrame* frame = new MinervaSectionFrame;
			NVGcolor frameCol = col;
			frameCol.a = 0.65f;
			frame->color = frameCol;
			frame->box.pos = mm2px(Vec(3.f, f));
			frame->box.size = mm2px(Vec(W_MM - 6.f, frameH));
			addChild(frame);

			addChild(new PanelLabel(Vec(6.5f, f + 2.f), secTitle[s], col, 8.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP));

			// Reglerreihe: K, M, Oset - je 16 uebereinanderliegende Knobs.
			// (1,5 mm tiefer als zuvor, damit oben Platz fuer den +.5-Schalter bleibt)
			float knobY = f + 13.5f;
			for (int t = 0; t < NUM_PT; t++) {
				for (int c = 0; c < NUM_CH; c++) {
					MinervaParamKnob* k = createParamCentered<MinervaParamKnob>(
						mm2px(Vec(knobX[t], knobY)), module, paramIndex(s, t, c));
					k->baseParamId = paramIndex(s, t, 0);
					knobs[s][t][c] = k;
					addParam(k);
				}
				addLabel(Vec(knobX[t], knobY + 6.2f), knobLabel[t], col);
			}

			// "+.5"-Schalter (5 V auf Vb) in der Kopfzeile rechts neben dem
			// Sektionstitel - je Kanal ein eigener Param, sichtbar ist nur der
			// des angezeigten Kanals.
			for (int c = 0; c < NUM_CH; c++) {
				MinervaToggle* t = createParamCentered<MinervaToggle>(mm2px(Vec(46.f, f + 4.6f)), module, plus5Param(s, c));
				t->baseParamId = plus5Param(s, 0);
				t->textOff = "+5 Vb";
				t->textOn = "+5 Vb";
				plus5Widgets[s][c] = t;
				addParam(t);
			}

			// Buchsenreihe in Reihenfolge des Originals: Va, Vb, M CV, Vc, Vout.
			float jackY = f + 29.5f;
			for (int j = 0; j < 4; j++) {
				addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackX[j], jackY)), module,
				                                         Minerva::VA_A_INPUT + s * 4 + j));
				addLabel(Vec(jackX[j], jackY + 5.5f), jackLabel[j], col);
			}
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jackX[4], jackY)), module,
			                                           Minerva::OUT_A_OUTPUT + s));
			addLabel(Vec(jackX[4], jackY + 5.5f), jackLabel[4], col);
		}
	}

	void step() override {
		int shown = mod ? mod->displayedCh : 0;
		for (int s = 0; s < 2; s++) {
			for (int t = 0; t < NUM_PT; t++) {
				for (int c = 0; c < NUM_CH; c++) {
					if (knobs[s][t][c])
						knobs[s][t][c]->visible = (c == shown);
				}
			}
			for (int c = 0; c < NUM_CH; c++) {
				if (plus5Widgets[s][c])
					plus5Widgets[s][c]->visible = (c == shown);
			}
		}
		ModuleWidget::step();
	}

	// Kanalzahl je Sektion (Auto oder fest 1-16), M-Bereich und
	// Ausgangsbegrenzung (Checklisten, Faustregel).
	void appendContextMenu(Menu* menu) override {
		Minerva* m = mod;
		if (!m)
			return;
		menu->addChild(new MenuSeparator);

		// Modulweit fuer beide Sektionen und alle Kanaele.
		menu->addChild(createSubmenuItem("M range", m->mLimited ? "Limited 0..1" : "Unlimited", [m](Menu* sub) {
			sub->addChild(createCheckMenuItem("Unlimited", "",
				[m]() { return !m->mLimited; },
				[m]() { m->mLimited = false; }));
			sub->addChild(createCheckMenuItem("Limited 0..1", "",
				[m]() { return m->mLimited; },
				[m]() { m->mLimited = true; }));
		}));
		{
			const char* clipNames[3] = {"Off", "Soft", "Hard"};
			int cur = rack::clamp(m->outputClip, 0, 2);
			menu->addChild(createSubmenuItem("Output limiting (\xC2\xB1" "12 V)", clipNames[cur], [m, clipNames](Menu* sub) {
				for (int i = 0; i < 3; i++) {
					sub->addChild(createCheckMenuItem(clipNames[i], "",
						[m, i]() { return m->outputClip == i; },
						[m, i]() { m->outputClip = i; }));
				}
			}));
		}
		menu->addChild(new MenuSeparator);
		const char* names[2] = {"Channels Section A", "Channels Section B"};
		for (int s = 0; s < 2; s++) {
			std::string current = (m->channelsOverride[s] == 0) ? "Auto" : string::f("%d", m->channelsOverride[s]);
			menu->addChild(createSubmenuItem(names[s], current, [m, s](Menu* sub) {
				sub->addChild(createCheckMenuItem("Auto", "",
					[m, s]() { return m->channelsOverride[s] == 0; },
					[m, s]() { m->channelsOverride[s] = 0; }));
				for (int n = 1; n <= NUM_CH; n++) {
					sub->addChild(createCheckMenuItem(string::f("%d", n), "",
						[m, s, n]() { return m->channelsOverride[s] == n; },
						[m, s, n]() { m->channelsOverride[s] = n; }));
				}
			}));
		}

		// DC-Blocker (ca. 10 Hz) je Sektion: wirkt auf Signal und Crossfader, nicht
		// auf Oset und den +5-Vb-Anteil.
		menu->addChild(new MenuSeparator);
		const char* dcNames[2] = {"DC blocker Section A", "DC blocker Section B"};
		for (int s = 0; s < 2; s++) {
			menu->addChild(createCheckMenuItem(dcNames[s], "",
				[m, s]() { return m->dcBlock[s]; },
				[m, s]() { m->dcBlock[s] = !m->dcBlock[s]; }));
		}
	}
};

} // namespace

Model* modelMinerva = createModel<Minerva, MinervaWidget>("Minerva");
