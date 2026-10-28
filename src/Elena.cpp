#include "plugin.hpp"
#include "PanelLabel.hpp"
#include "CvProcElena.hpp"
#include <history.hpp>

// ---------------------------------------------------------------------
// "Elena": digitaler CV-Prozessor mit Kennlinien-Sektionen und Slew-Limitern.
//
//  - Sektionen A/B (Kennlinie mit Breakpoint): Eingangswahl A1/A2 per Select (Regler
//    oder CV), danach Transfer-Kennlinie mit 0-V-/10-V-Punkt und optional
//    einem Breakpoint. Polyphon, JEDER der 16 Kanaele hat eigene Werte
//    (gemeinsamer Kanalwaehler, Muster wie Minerva: pendingCh/displayedCh,
//    kein Param, per JSON gespeichert; 16 Widget-Sets uebereinander).
//    Je Sektion waehlbarer Ein-/Ausgangsbereich (0-10 V, +/-5 V, +/-10 V,
//    Unlimited), siehe InputRange. Zuschaltbarer DC-Blocker (ca. 10 Hz) je
//    A/B-Ausgang und je Slew-Paar (Kontextmenue).
//  - Sektionen C-H: sechs unabhaengige Slew-Limiter mit Pulse-Ausgang.
//    Regler paarweise geteilt (C/D, E/F, G/H), GLOBAL (nicht pro Kanal),
//    die Signale sind polyphon.
//
// Alles im anonymen Namespace (interne Bindung, siehe PanelLabel-ODR-
// Vorfall); nur das Model ist exportiert.
// ---------------------------------------------------------------------

namespace {

using simd::float_4;

static constexpr int NUM_CH = 16;
static constexpr int NUM_SLEWERS = 6; // C..H
static constexpr int NUM_PAIRS = 3;   // C/D, E/F, G/H

// Reglerarten der A/B-Sektionen. Je Sektion und Art ein zusammenhaengender
// 16er-Block (siehe abParam()).
enum AbType { AB_SEL, AB_V0, AB_V10, AB_BPX, AB_BPY, AB_BP, NUM_AB };
// Geteilte Slew-Regler je Paar (global, nicht pro Kanal).
enum PairParam { PAIR_POS, PAIR_NEG, PAIR_MODE, PAIR_AUTO, NUM_PAIR_PARAMS };

static constexpr int AB_PARAM_COUNT = 2 * NUM_AB * NUM_CH; // 192

static inline int abParam(int section, int type, int ch) {
	return (section * NUM_AB + type) * NUM_CH + ch;
}
static inline int pairParam(int pair, int which) {
	return AB_PARAM_COUNT + pair * NUM_PAIR_PARAMS + which;
}

// Slew-Regler in Sekunden anzeigen/eingeben (0,01..10 s exponentiell).
struct SlewTimeQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float t = CvProcElena::knobToSeconds(getValue());
		if (t < 1.f)
			return string::f("%.0f ms", t * 1000.f);
		return string::f("%.2f s", t);
	}

	void setDisplayValueString(std::string s) override {
		// Komma -> Punkt (siehe normalizeDecimalComma in Marf248.cpp)
		for (char& c : s) {
			if (c == ',')
				c = '.';
		}
		float v = (float) std::atof(s.c_str());
		if (s.find("ms") != std::string::npos)
			v *= 0.001f;
		setValue(CvProcElena::secondsToKnob(v));
	}
};

// Cache fuer die Slewzeit: std::pow nur neu berechnen, wenn sich die
// Reglerposition (inkl. CV) tatsaechlich geaendert hat.
struct SlewTimeCache {
	float pos = -1.f;
	float t = CvProcElena::SLEW_MIN_S;

	float get(float p) {
		if (p != pos) {
			pos = p;
			t = CvProcElena::knobToSeconds(p);
		}
		return t;
	}
};

// Eingangsbereich je A/B-Sektion (Kontextmenue). Der Bereich gilt fuer A1, A2
// und SEL CV UND fuer den Ausgang (Bereich -> Bereich); Kennlinie und
// Reglerpositionen (0..1) bleiben unveraendert, die Anzeige in Volt richtet
// sich nach dem Bereich.
//  RANGE_0_10      0..10 V, Eingang hart auf den Bereich begrenzt (Standard)
//  RANGE_PM5       -5..+5 V, begrenzt
//  RANGE_PM10      -10..+10 V, begrenzt
//  RANGE_UNLIMITED Skala 0..10 V, aber KEINE Begrenzung von Vin: die Kennlinie
//                  setzt sich linear ueber die Endpunkte hinaus fort.
// Neue Eintraege nur am Ende anhaengen (Werte stehen in gespeicherten Patches).
enum InputRange { RANGE_0_10, RANGE_PM5, RANGE_PM10, RANGE_UNLIMITED, NUM_RANGES };

static inline void rangeBounds(int mode, float& lo, float& hi) {
	switch (mode) {
		case RANGE_PM5: lo = -5.f; hi = 5.f; break;
		case RANGE_PM10: lo = -10.f; hi = 10.f; break;
		default: lo = 0.f; hi = 10.f; break; // RANGE_0_10 und RANGE_UNLIMITED
	}
}

// Regler "0 V", "10 V", "BP IN", "BP OUT" in Volt anzeigen/eingeben: der
// Reglerwert 0..1 laeuft ueber den Bereich der jeweiligen Sektion.
struct RangeVoltageQuantity : ParamQuantity {
	const int* rangeMode = nullptr; // zeigt auf Elena::inputRange[Sektion]

	float getDisplayValue() override {
		float lo, hi;
		rangeBounds(rangeMode ? *rangeMode : RANGE_0_10, lo, hi);
		return lo + getValue() * (hi - lo);
	}

	void setDisplayValue(float v) override {
		float lo, hi;
		rangeBounds(rangeMode ? *rangeMode : RANGE_0_10, lo, hi);
		setValue((v - lo) / (hi - lo));
	}
};

struct Elena : Module {
	// Enum-Disziplin: nur am Ende erweitern. Die Params sind kein enum,
	// sondern per abParam()/pairParam() adressiert (192 + 12).
	enum ParamId {
		NUM_PARAMS = AB_PARAM_COUNT + NUM_PAIRS * NUM_PAIR_PARAMS
	};
	// Je Sektion A1, A2, Select-CV (Sektion B = Sektion A + 3).
	enum InputId {
		A1_A_INPUT,
		A2_A_INPUT,
		SEL_A_INPUT,
		A1_B_INPUT,
		A2_B_INPUT,
		SEL_B_INPUT,
		SLEW_IN_C_INPUT,
		SLEW_IN_D_INPUT,
		SLEW_IN_E_INPUT,
		SLEW_IN_F_INPUT,
		SLEW_IN_G_INPUT,
		SLEW_IN_H_INPUT,
		// je Paar: Positive-Slew-CV, Negative-Slew-CV
		POSCV_CD_INPUT,
		NEGCV_CD_INPUT,
		POSCV_EF_INPUT,
		NEGCV_EF_INPUT,
		POSCV_GH_INPUT,
		NEGCV_GH_INPUT,
		NUM_INPUTS
	};
	enum OutputId {
		OUT_A_OUTPUT,
		OUT_B_OUTPUT,
		SLEW_OUT_C_OUTPUT,
		SLEW_OUT_D_OUTPUT,
		SLEW_OUT_E_OUTPUT,
		SLEW_OUT_F_OUTPUT,
		SLEW_OUT_G_OUTPUT,
		SLEW_OUT_H_OUTPUT,
		PULSE_C_OUTPUT,
		PULSE_D_OUTPUT,
		PULSE_E_OUTPUT,
		PULSE_F_OUTPUT,
		PULSE_G_OUTPUT,
		PULSE_H_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightId {
		PULSE_LED_C_LIGHT, // rot: Puls
		PULSE_LED_D_LIGHT,
		PULSE_LED_E_LIGHT,
		PULSE_LED_F_LIGHT,
		PULSE_LED_G_LIGHT,
		PULSE_LED_H_LIGHT,
		CVOUT_LED_C_LIGHT, // blau: Slew-Ausgang
		CVOUT_LED_D_LIGHT,
		CVOUT_LED_E_LIGHT,
		CVOUT_LED_F_LIGHT,
		CVOUT_LED_G_LIGHT,
		CVOUT_LED_H_LIGHT,
		NUM_LIGHTS
	};

	// Reiner GUI-Zustand (kein Param, kein History-Eintrag, siehe Lucia).
	int pendingCh = 0;
	int displayedCh = 0;
	// Kanalzahl je A/B-Sektion: 0 = Auto (Maximum der angeschlossenen
	// Eingaenge, mindestens 1), 1..16 = fest.
	int channelsOverride[2] = {0, 0};
	// Eingangs-/Ausgangsbereich je A/B-Sektion (InputRange, siehe oben).
	int inputRange[2] = {RANGE_0_10, RANGE_0_10};

	// Schaltbarer DC-Blocker (Kontextmenue): Index 0/1 = Ausgang Sektion A/B,
	// 2/3/4 = Slew-Ausgaenge der Paare C/D, E/F, G/H. Aus = Standard.
	bool dcBlock[5] = {false, false, false, false, false};
	float dcR = 0.f;          // Filterkoeffizient, siehe CvProcElena::dcBlockCoeff
	float dcSampleRate = 0.f; // Abtastrate, fuer die dcR berechnet wurde
	// Filterzustand je Kanal (x[n-1], y[n-1]); A/B und die sechs Slewer
	float abDcX[2][NUM_CH] = {};
	float abDcY[2][NUM_CH] = {};
	float slewDcX[NUM_SLEWERS][NUM_CH] = {};
	float slewDcY[NUM_SLEWERS][NUM_CH] = {};

	// Fuer die Kennlinien-Anzeige: aktueller Arbeitspunkt (normiert 0..1)
	// je Sektion und Kanal. Wird nur zur Anzeige gelesen.
	float liveVin[2][NUM_CH] = {};
	float liveVout[2][NUM_CH] = {};

	// Slew-Zustand je Slewer und Kanal.
	float slewOut[NUM_SLEWERS][NUM_CH] = {};
	CvProcElena::ReversalDetector reversal[NUM_SLEWERS][NUM_CH];
	dsp::PulseGenerator pulseGen[NUM_SLEWERS][NUM_CH];
	SlewTimeCache timeCache[NUM_PAIRS][2][NUM_CH]; // [Paar][pos/neg][Kanal]
	float pulseLedValue[NUM_SLEWERS] = {};

	Elena() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const char* secName[2] = {"A", "B"};
		for (int s = 0; s < 2; s++) {
			for (int c = 0; c < NUM_CH; c++) {
				// Werte sind normiert 0..1, Anzeige als Volt bzw. Prozent.
				configParam(abParam(s, AB_SEL, c), 0.f, 1.f, 0.f,
				            string::f("Section %s select A1-A2 (channel %d)", secName[s], c + 1),
				            " %", 0.f, 100.f);
				// Spannungsregler: Anzeige in Volt ueber den Bereich der Sektion
				// (RangeVoltageQuantity liest inputRange[s]).
				RangeVoltageQuantity* q;
				q = configParam<RangeVoltageQuantity>(abParam(s, AB_V0, c), 0.f, 1.f, 0.f,
				            string::f("Section %s output at lower input point (channel %d)", secName[s], c + 1), " V");
				q->rangeMode = &inputRange[s];
				q = configParam<RangeVoltageQuantity>(abParam(s, AB_V10, c), 0.f, 1.f, 1.f,
				            string::f("Section %s output at upper input point (channel %d)", secName[s], c + 1), " V");
				q->rangeMode = &inputRange[s];
				q = configParam<RangeVoltageQuantity>(abParam(s, AB_BPX, c), 0.f, 1.f, 0.5f,
				            string::f("Section %s breakpoint input (channel %d)", secName[s], c + 1), " V");
				q->rangeMode = &inputRange[s];
				q = configParam<RangeVoltageQuantity>(abParam(s, AB_BPY, c), 0.f, 1.f, 0.5f,
				            string::f("Section %s breakpoint output (channel %d)", secName[s], c + 1), " V");
				q->rangeMode = &inputRange[s];
				configSwitch(abParam(s, AB_BP, c), 0.f, 1.f, 0.f,
				             string::f("Section %s breakpoint (channel %d)", secName[s], c + 1),
				             {"Off", "On"});
			}
			configInput(A1_A_INPUT + s * 3, string::f("Section %s A1", secName[s]));
			configInput(A2_A_INPUT + s * 3, string::f("Section %s A2", secName[s]));
			configInput(SEL_A_INPUT + s * 3, string::f("Section %s select CV (replaces knob)", secName[s]));
			configOutput(OUT_A_OUTPUT + s, string::f("Section %s output", secName[s]));
		}

		const char* pairName[NUM_PAIRS] = {"C/D", "E/F", "G/H"};
		for (int p = 0; p < NUM_PAIRS; p++) {
			configParam<SlewTimeQuantity>(pairParam(p, PAIR_POS), 0.f, 1.f, 0.f,
			                              string::f("Sections %s positive slew (rising)", pairName[p]));
			configParam<SlewTimeQuantity>(pairParam(p, PAIR_NEG), 0.f, 1.f, 0.f,
			                              string::f("Sections %s negative slew (falling)", pairName[p]));
			configSwitch(pairParam(p, PAIR_MODE), 0.f, 1.f, 0.f,
			             string::f("Sections %s slew mode", pairName[p]), {"Time", "Rate"});
			configSwitch(pairParam(p, PAIR_AUTO), 0.f, 1.f, 0.f,
			             string::f("Sections %s auto-pulse", pairName[p]), {"Off", "On"});
			configInput(POSCV_CD_INPUT + p * 2, string::f("Sections %s positive slew CV", pairName[p]));
			configInput(NEGCV_CD_INPUT + p * 2, string::f("Sections %s negative slew CV", pairName[p]));
		}
		for (int k = 0; k < NUM_SLEWERS; k++) {
			char letter = (char) ('C' + k);
			configInput(SLEW_IN_C_INPUT + k, string::f("Section %c CV in", letter));
			configOutput(SLEW_OUT_C_OUTPUT + k, string::f("Section %c slewed CV out", letter));
			configOutput(PULSE_C_OUTPUT + k, string::f("Section %c pulse out", letter));
			configLight(PULSE_LED_C_LIGHT + k, string::f("Section %c pulse", letter));
			configLight(CVOUT_LED_C_LIGHT + k, string::f("Section %c CV out", letter));
		}
	}

	// Unbelegter Eingang = 0 V. Mono-Eingang wird auf alle Kanaele verteilt.
	static inline float_4 readIn(Input& in, int c) {
		if (!in.isConnected())
			return float_4(0.f);
		return in.getPolyVoltageSimd<float_4>(c);
	}

	// ----- Sektionen A/B ------------------------------------------------
	void processSection(int s) {
		Input& inA1 = inputs[A1_A_INPUT + s * 3];
		Input& inA2 = inputs[A2_A_INPUT + s * 3];
		Input& inSel = inputs[SEL_A_INPUT + s * 3];
		Output& out = outputs[OUT_A_OUTPUT + s];

		int n = channelsOverride[s];
		if (n <= 0) {
			n = std::max(std::max(inA1.getChannels(), inA2.getChannels()),
			             std::max(inSel.getChannels(), 1));
		}
		n = rack::clamp(n, 1, NUM_CH);
		out.setChannels(n);

		// Bereich der Sektion: Eingaenge (A1, A2, SEL CV) werden von lo..hi auf
		// 0..1 normiert, der Ausgang von 0..1 wieder auf lo..hi zurueckgerechnet.
		float lo, hi;
		rangeBounds(inputRange[s], lo, hi);
		float_4 loV(lo), spanV(hi - lo), invSpanV(1.f / (hi - lo));
		bool limitVin = (inputRange[s] != RANGE_UNLIMITED);

		for (int c = 0; c < n; c += 4) {
			float sel[4], v0[4], v10[4], bx[4], by[4], bp[4];
			for (int i = 0; i < 4; i++) {
				int ch = c + i; // immer < 16, da c Vielfaches von 4 und n <= 16
				sel[i] = params[abParam(s, AB_SEL, ch)].getValue();
				v0[i] = params[abParam(s, AB_V0, ch)].getValue();
				v10[i] = params[abParam(s, AB_V10, ch)].getValue();
				bx[i] = params[abParam(s, AB_BPX, ch)].getValue();
				by[i] = params[abParam(s, AB_BPY, ch)].getValue();
				bp[i] = params[abParam(s, AB_BP, ch)].getValue();
			}

			// Select: CV ersetzt den Regler, sobald gepatcht (Referenzdokumentation).
			float_4 selV;
			if (inSel.isConnected()) {
				// SEL CV folgt dem Bereich der Sektion, bleibt aber immer auf 0..1
				// begrenzt (Gewicht der Ueberblendung).
				selV = simd::clamp((readIn(inSel, c) - loV) * invSpanV, float_4(0.f), float_4(1.f));
			}
			else {
				selV = float_4::load(sel);
			}

			// Eingaenge auf 1 normiert (10 V = 1); unbelegtes A1 = 0 (Referenzdokumentation:
			// dann multipliziert die Eingangsstufe nur InSel * A2).
			float_4 a1 = (readIn(inA1, c) - loV) * invSpanV;
			float_4 a2 = (readIn(inA2, c) - loV) * invSpanV;
			float_4 vin = selV * a2 + (float_4(1.f) - selV) * a1;
			// Standard (ANNAHME): Vin wird auf den Bereich begrenzt (die Kennlinie des
			// Originals ist auf 0..10 V definiert). Nur "Unlimited" laesst Vin
			// ueber 0..1 hinauslaufen (lineare Fortsetzung der Kennlinie).
			if (limitVin)
				vin = simd::clamp(vin, float_4(0.f), float_4(1.f));

			float_4 bpOn = float_4::load(bp) > float_4(0.5f);
			float_4 vout = CvProcElena::transfer(vin, float_4::load(v0), float_4::load(v10), bpOn,
			                                    float_4::load(bx), float_4::load(by));
			float_4 res = vout * spanV + loV;
			if (dcBlock[s]) {
				float_4 xp = float_4::load(&abDcX[s][c]);
				float_4 yp = float_4::load(&abDcY[s][c]);
				res = CvProcElena::dcBlockStep(res, xp, yp, dcR);
				xp.store(&abDcX[s][c]);
				yp.store(&abDcY[s][c]);
			}
			else {
				// Zustand mitfuehren, damit beim Zuschalten kein Altzustand wirkt.
				res.store(&abDcX[s][c]);
				float_4(0.f).store(&abDcY[s][c]);
			}
			out.setVoltageSimd(res, c);

			float vinA[4], voutA[4];
			vin.store(vinA);
			vout.store(voutA);
			for (int i = 0; i < 4; i++) {
				liveVin[s][c + i] = vinA[i];
				liveVout[s][c + i] = voutA[i];
			}
		}
	}

	// ----- Sektionen C-H: Slew + Auto-Pulse -----------------------------
	void processSlewers(float dt) {
		// Hysterese fuer die Richtungsumkehr (Volt). Klein genug fuer
		// langsame Slews, gross genug gegen Rauschen.
		const float REVERSAL_HYST = 0.01f;

		for (int p = 0; p < NUM_PAIRS; p++) {
			bool rateMode = params[pairParam(p, PAIR_MODE)].getValue() > 0.5f;
			bool autoPulse = params[pairParam(p, PAIR_AUTO)].getValue() > 0.5f;
			float posKnob = params[pairParam(p, PAIR_POS)].getValue();
			float negKnob = params[pairParam(p, PAIR_NEG)].getValue();
			Input& posCv = inputs[POSCV_CD_INPUT + p * 2];
			Input& negCv = inputs[NEGCV_CD_INPUT + p * 2];

			for (int j = 0; j < 2; j++) {
				int k = p * 2 + j;
				Input& in = inputs[SLEW_IN_C_INPUT + k];
				Output& outSlew = outputs[SLEW_OUT_C_OUTPUT + k];
				Output& outPulse = outputs[PULSE_C_OUTPUT + k];

				int n = std::max(in.getChannels(), 1);
				outSlew.setChannels(n);
				outPulse.setChannels(n);

				bool pulseAny = false;
				float outMaxAbs = 0.f;

				for (int c = 0; c < n; c++) {
					float x = in.isConnected() ? in.getPolyVoltage(c) : 0.f;

					// Slew-CV addiert sich zur Reglerposition (0..10 V = voller
					// Regelweg), oben begrenzt: steht der Regler ganz rechts,
					// bleibt die CV wirkungslos (wie in der Referenzdokumentation beschrieben).
					float pos = posKnob;
					if (posCv.isConnected())
						pos += posCv.getPolyVoltage(c) * 0.1f;
					float neg = negKnob;
					if (negCv.isConnected())
						neg += negCv.getPolyVoltage(c) * 0.1f;
					pos = rack::clamp(pos, 0.f, 1.f);
					neg = rack::clamp(neg, 0.f, 1.f);

					float tPos = timeCache[p][0][c].get(pos);
					float tNeg = timeCache[p][1][c].get(neg);

					float y = CvProcElena::slewStep(x, slewOut[k][c], tPos, tNeg, rateMode, dt);
					slewOut[k][c] = y;

					if (autoPulse) {
						if (reversal[k][c].process(y, REVERSAL_HYST))
							pulseGen[k][c].trigger(1e-3f);
					}
					else {
						reversal[k][c].reset(y);
					}
					bool pulseHigh = pulseGen[k][c].process(dt);
					if (pulseHigh)
						pulseAny = true;

					// DC-Blocker nur auf den Ausgang; interner Slew-Zustand, Auto-Pulse
					// und LED arbeiten mit dem ungefilterten Wert y.
					float yOut = y;
					if (dcBlock[2 + p]) {
						yOut = CvProcElena::dcBlockStep(y, slewDcX[k][c], slewDcY[k][c], dcR);
					}
					else {
						slewDcX[k][c] = y;
						slewDcY[k][c] = 0.f;
					}
					outSlew.setVoltage(yOut, c);
					outPulse.setVoltage(pulseHigh ? 10.f : 0.f, c);
					outMaxAbs = std::max(outMaxAbs, std::fabs(y));
				}

				// Rote LED: kurzer Puls waere bei 1 ms kaum sichtbar, daher
				// mit kleinem Nachleuchten. Blaue LED: Slew-Ausgang.
				if (pulseAny)
					pulseLedValue[k] = 1.f;
				else
					pulseLedValue[k] = std::max(0.f, pulseLedValue[k] - 15.f * dt);
				lights[PULSE_LED_C_LIGHT + k].setBrightness(pulseLedValue[k]);
				lights[CVOUT_LED_C_LIGHT + k].setBrightness(rack::clamp(outMaxAbs * 0.1f, 0.f, 1.f));
			}
		}
	}

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != dcSampleRate) {
			dcSampleRate = args.sampleRate;
			dcR = CvProcElena::dcBlockCoeff(dcSampleRate);
		}
		processSection(0);
		processSection(1);
		processSlewers(args.sampleTime);
	}

	void onReset() override {
		pendingCh = 0;
		displayedCh = 0;
		channelsOverride[0] = 0;
		channelsOverride[1] = 0;
		inputRange[0] = RANGE_0_10;
		inputRange[1] = RANGE_0_10;
		for (int i = 0; i < 5; i++)
			dcBlock[i] = false;
		for (int s = 0; s < 2; s++) {
			for (int c = 0; c < NUM_CH; c++) {
				abDcX[s][c] = 0.f;
				abDcY[s][c] = 0.f;
			}
		}
		for (int k = 0; k < NUM_SLEWERS; k++) {
			for (int c = 0; c < NUM_CH; c++) {
				slewDcX[k][c] = 0.f;
				slewDcY[k][c] = 0.f;
			}
		}
		for (int k = 0; k < NUM_SLEWERS; k++) {
			for (int c = 0; c < NUM_CH; c++) {
				slewOut[k][c] = 0.f;
				reversal[k][c].reset(0.f);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "pendingCh", json_integer(pendingCh));
		json_object_set_new(rootJ, "displayedCh", json_integer(displayedCh));
		json_object_set_new(rootJ, "channelsA", json_integer(channelsOverride[0]));
		json_object_set_new(rootJ, "channelsB", json_integer(channelsOverride[1]));
		json_object_set_new(rootJ, "rangeA", json_integer(inputRange[0]));
		json_object_set_new(rootJ, "rangeB", json_integer(inputRange[1]));
		// DC-Blocker je Ziel (A, B, C/D, E/F, G/H) als 0/1
		json_t* dcJ = json_array();
		for (int i = 0; i < 5; i++)
			json_array_append_new(dcJ, json_integer(dcBlock[i] ? 1 : 0));
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
		// Fehlt der Schluessel (aelterer Patch), bleibt der Standard 0..10 V.
		j = json_object_get(rootJ, "rangeA");
		if (j)
			inputRange[0] = rack::clamp((int) json_integer_value(j), 0, NUM_RANGES - 1);
		j = json_object_get(rootJ, "rangeB");
		if (j)
			inputRange[1] = rack::clamp((int) json_integer_value(j), 0, NUM_RANGES - 1);
		// Fehlt der Schluessel (aelterer Patch), bleibt der DC-Blocker aus.
		j = json_object_get(rootJ, "dcBlock");
		if (j && json_is_array(j)) {
			for (int i = 0; i < 5; i++) {
				json_t* e = json_array_get(j, i);
				dcBlock[i] = (e && json_integer_value(e) != 0);
			}
		}
	}
};


// ---------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------

struct ElenaPanelBackground : Widget {
	NVGcolor color = nvgRGB(228, 228, 226);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, color);
		nvgFill(args.vg);
	}
};

// Rein dekorativer Rahmen, optional mit waagerechten Trennlinien (mm,
// relativ zur Oberkante). TransparentWidget: faengt keine Maus-Events ab.
struct ElenaFrame : TransparentWidget {
	NVGcolor color = nvgRGB(120, 120, 120);
	std::vector<float> dividersMM;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 3.f);
		nvgStrokeColor(args.vg, color);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
		for (float d : dividersMM) {
			float y = mm2px(d);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 4.f, y);
			nvgLineTo(args.vg, box.size.x - 4.f, y);
			nvgStroke(args.vg);
		}
	}
};

// Kanal-Auswahlknob (1-16), wie StageSelectKnob in Marf248.cpp: KEIN
// Param, schreibt direkt auf module->pendingCh.
struct ElenaSelectKnob : OpaqueWidget {
	Elena* module = nullptr;
	float dragAccumPx = 0.f;

	ElenaSelectKnob() {
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

struct ElenaActivateButton : OpaqueWidget {
	Elena* module = nullptr;

	ElenaActivateButton() {
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
// derselben Sektion und Art, als EIN Undo-Schritt (ComplexAction).
static void applyToAllChannels(ParamWidget* w, int baseParamId) {
	ParamQuantity* pq = w->getParamQuantity();
	if (!w->module || !pq)
		return;
	float value = pq->getValue();

	history::ComplexAction* complex = new history::ComplexAction;
	complex->name = "apply Elena value to all channels";
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

// Regler; baseParamId >= 0: Rechtsklick-Eintrag "Apply to all 16 channels".
struct ElenaKnob : RoundSmallBlackKnob {
	int baseParamId = -1;

	void appendContextMenu(Menu* menu) override {
		if (baseParamId < 0)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Apply to all 16 channels", "", [this]() { applyToAllChannels(this, baseParamId); }));
	}
};

// Einfacher Textschalter (2 Zustaende) mit eigener Optik: Breakpoint,
// Time/Rate und Auto-Pulse. Klick schaltet um (Basisklasse app::Switch).
struct ElenaToggle : app::Switch {
	int baseParamId = -1;
	std::string textOff = "OFF";
	std::string textOn = "ON";
	NVGcolor colorOn = nvgRGB(90, 220, 110);

	ElenaToggle() {
		box.size = mm2px(Vec(11.f, 5.5f));
	}

	void appendContextMenu(Menu* menu) override {
		if (baseParamId < 0)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Apply to all 16 channels", "", [this]() { applyToAllChannels(this, baseParamId); }));
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

// Kennlinien-Anzeige (Ersatz fuer die LED-Kette des Originals): zeigt die
// Transfer-Funktion des angezeigten Kanals und den aktuellen Arbeitspunkt.
struct ElenaTransferDisplay : TransparentWidget {
	Elena* module = nullptr;
	int section = 0;

	ElenaTransferDisplay() {
		box.size = mm2px(Vec(34.f, 20.f));
	}

	void draw(const DrawArgs& args) override {
		float w = box.size.x;
		float h = box.size.y;

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, w, h, 2.f);
		nvgFillColor(args.vg, nvgRGB(30, 32, 36));
		nvgFill(args.vg);

		// Mittellinien
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, w / 2.f, 1.f);
		nvgLineTo(args.vg, w / 2.f, h - 1.f);
		nvgMoveTo(args.vg, 1.f, h / 2.f);
		nvgLineTo(args.vg, w - 1.f, h / 2.f);
		nvgStrokeColor(args.vg, nvgRGBA(120, 120, 130, 90));
		nvgStrokeWidth(args.vg, 0.5f);
		nvgStroke(args.vg);

		// Ohne Modul (Modul-Browser): Standard = Gerade 0 -> 1
		float v0 = 0.f, v10 = 1.f, bx = 0.5f, by = 0.5f;
		bool bp = false;
		float lvin = 0.f, lvout = 0.f;
		if (module) {
			int ch = module->displayedCh;
			v0 = module->params[abParam(section, AB_V0, ch)].getValue();
			v10 = module->params[abParam(section, AB_V10, ch)].getValue();
			bx = module->params[abParam(section, AB_BPX, ch)].getValue();
			by = module->params[abParam(section, AB_BPY, ch)].getValue();
			bp = module->params[abParam(section, AB_BP, ch)].getValue() > 0.5f;
			lvin = module->liveVin[section][ch];
			lvout = module->liveVout[section][ch];
		}

		auto X = [&](float u) { return 1.f + u * (w - 2.f); };
		auto Y = [&](float v) { return (h - 1.f) - v * (h - 2.f); };

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, X(0.f), Y(v0));
		if (bp)
			nvgLineTo(args.vg, X(bx), Y(by));
		nvgLineTo(args.vg, X(1.f), Y(v10));
		nvgStrokeColor(args.vg, nvgRGB(90, 220, 110));
		nvgStrokeWidth(args.vg, 1.4f);
		nvgStroke(args.vg);

		if (bp) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, X(bx), Y(by), 1.8f);
			nvgFillColor(args.vg, nvgRGB(90, 220, 110));
			nvgFill(args.vg);
		}

		// Aktueller Arbeitspunkt (Eingang -> Ausgang) des angezeigten Kanals
		float px = rack::clamp(lvin, 0.f, 1.f);
		float py = rack::clamp(lvout, 0.f, 1.f);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, X(px), Y(py), 2.f);
		nvgFillColor(args.vg, nvgRGB(235, 170, 60));
		nvgFill(args.vg);
	}
};


struct ElenaWidget : ModuleWidget {
	Elena* mod = nullptr;
	// [Sektion][Art][Kanal]: alle Widgets liegen uebereinander, step()
	// blendet alle bis auf den angezeigten Kanal aus.
	ParamWidget* abWidgets[2][NUM_AB][NUM_CH] = {};

	void addLabel(Vec posMM, const std::string& text, NVGcolor color, float fontSize = 9.f) {
		addChild(new PanelLabel(posMM, text, color, fontSize));
	}

	ElenaWidget(Elena* module) {
		setModule(module);
		mod = module;

		const float HP = 42.f;
		box.size = Vec(HP * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		const float W_MM = 213.36f; // 42 HP

		ElenaPanelBackground* panelBg = new ElenaPanelBackground;
		panelBg->box.size = box.size;
		addChild(panelBg);

		PanelBorder* panelBorder = new PanelBorder;
		panelBorder->box.size = box.size;
		addChild(panelBorder);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Kopfzeile: Kanalwahl ueber A/B, Titel ueber den Slew-Sektionen.
		const float selX = 44.f, actX = 72.f, selY = 11.5f;
		ElenaSelectKnob* sel = new ElenaSelectKnob;
		sel->module = module;
		sel->box.pos = mm2px(Vec(selX, selY)) - sel->box.size / 2.f;
		addChild(sel);
		ElenaActivateButton* act = new ElenaActivateButton;
		act->module = module;
		act->box.pos = mm2px(Vec(actX, selY)) - act->box.size / 2.f;
		addChild(act);
		addLabel(Vec(selX, 19.5f), "CHANNEL", COLOR_SYSTEM);
		addLabel(Vec(actX, 19.5f), "ACTIVATE", COLOR_SYSTEM);

		const float titleX = 163.f;
		addChild(new PanelLabel(Vec(titleX, 3.5f), "ELENA", COLOR_SYSTEM, 14.f));
		addChild(new PanelLabel(Vec(titleX, 11.f), "CONTROL VOLTAGE PROCESSOR", COLOR_SYSTEM, 6.f));

		const float frameTop = 24.f;
		const float frameH = 96.f;

		// ---- Sektionen A und B ----
		const float frameX[2] = {3.f, 59.f};
		const float frameW = 54.f;
		const NVGcolor secColor[2] = {COLOR_SECTION_A, COLOR_SECTION_B};
		const char* secTitle[2] = {"SECTION A", "SECTION B"};

		struct AbKnobPos {
			int type;
			float dx, dy;
			const char* label;
		};
		const AbKnobPos abKnobs[5] = {
			{AB_V0, -13.f, 35.f, "0 V"},
			{AB_V10, 13.f, 35.f, "10 V"},
			{AB_BPX, -13.f, 51.f, "BP IN"},
			{AB_BPY, 13.f, 51.f, "BP OUT"},
			{AB_SEL, -13.f, 67.f, "SELECT"},
		};

		for (int s = 0; s < 2; s++) {
			float fx = frameX[s];
			float cx = fx + frameW / 2.f;
			NVGcolor col = secColor[s];

			ElenaFrame* frame = new ElenaFrame;
			NVGcolor frameCol = col;
			frameCol.a = 0.65f;
			frame->color = frameCol;
			frame->box.pos = mm2px(Vec(fx, frameTop));
			frame->box.size = mm2px(Vec(frameW, frameH));
			addChild(frame);

			addChild(new PanelLabel(Vec(fx + 3.5f, frameTop + 2.f), secTitle[s], col, 8.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP));

			ElenaTransferDisplay* disp = new ElenaTransferDisplay;
			disp->module = module;
			disp->section = s;
			disp->box.pos = mm2px(Vec(cx - 17.f, frameTop + 6.5f));
			addChild(disp);

			for (const AbKnobPos& kp : abKnobs) {
				Vec pos = Vec(cx + kp.dx, frameTop + kp.dy);
				for (int c = 0; c < NUM_CH; c++) {
					ElenaKnob* k = createParamCentered<ElenaKnob>(mm2px(pos), module, abParam(s, kp.type, c));
					k->baseParamId = abParam(s, kp.type, 0);
					abWidgets[s][kp.type][c] = k;
					addParam(k);
				}
				addLabel(Vec(pos.x, pos.y + 6.2f), kp.label, col);
			}

			// Breakpoint-Schalter zwischen BP IN / BP OUT
			for (int c = 0; c < NUM_CH; c++) {
				ElenaToggle* t = createParamCentered<ElenaToggle>(mm2px(Vec(cx, frameTop + 51.f)), module, abParam(s, AB_BP, c));
				t->baseParamId = abParam(s, AB_BP, 0);
				t->textOff = "BP";
				t->textOn = "BP";
				t->colorOn = nvgRGB(60, 170, 90);
				abWidgets[s][AB_BP][c] = t;
				addParam(t);
			}

			// Select-CV neben dem Select-Regler
			Vec selCv = Vec(cx + 13.f, frameTop + 67.f);
			addInput(createInputCentered<PJ301MPort>(mm2px(selCv), module, Elena::SEL_A_INPUT + s * 3));
			addLabel(Vec(selCv.x, selCv.y + 5.5f), "SEL CV", col);

			// Buchsenreihe: A1, A2, Ausgang
			float jackY = frameTop + 83.f;
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cx - 16.f, jackY)), module, Elena::A1_A_INPUT + s * 3));
			addLabel(Vec(cx - 16.f, jackY + 5.5f), "A1", col);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cx, jackY)), module, Elena::A2_A_INPUT + s * 3));
			addLabel(Vec(cx, jackY + 5.5f), "A2", col);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(cx + 16.f, jackY)), module, Elena::OUT_A_OUTPUT + s));
			addLabel(Vec(cx + 16.f, jackY + 5.5f), "OUT", col);
		}

		// ---- Sektionen C-H: drei Paare untereinander ----
		const float slewFrameX = 116.f;
		const float slewFrameW = W_MM - slewFrameX - 3.f;
		const NVGcolor slewColor = COLOR_STAGEMOD;

		ElenaFrame* slewFrame = new ElenaFrame;
		NVGcolor slewFrameCol = slewColor;
		slewFrameCol.a = 0.65f;
		slewFrame->color = slewFrameCol;
		slewFrame->dividersMM = {32.f, 64.f};
		slewFrame->box.pos = mm2px(Vec(slewFrameX, frameTop));
		slewFrame->box.size = mm2px(Vec(slewFrameW, frameH));
		addChild(slewFrame);

		const float bx = slewFrameX + 2.f;
		for (int p = 0; p < NUM_PAIRS; p++) {
			float by = frameTop + p * 32.f;

			// Geteilte Regler und CVs links im Block
			ElenaKnob* posKnob = createParamCentered<ElenaKnob>(mm2px(Vec(bx + 8.f, by + 6.5f)), module, pairParam(p, PAIR_POS));
			addParam(posKnob);
			addLabel(Vec(bx + 8.f, by + 6.5f + 6.2f), "POS", slewColor);
			ElenaKnob* negKnob = createParamCentered<ElenaKnob>(mm2px(Vec(bx + 23.f, by + 6.5f)), module, pairParam(p, PAIR_NEG));
			addParam(negKnob);
			addLabel(Vec(bx + 23.f, by + 6.5f + 6.2f), "NEG", slewColor);

			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(bx + 8.f, by + 21.5f)), module, Elena::POSCV_CD_INPUT + p * 2));
			addLabel(Vec(bx + 8.f, by + 21.5f + 5.5f), "POS CV", slewColor);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(bx + 23.f, by + 21.5f)), module, Elena::NEGCV_CD_INPUT + p * 2));
			addLabel(Vec(bx + 23.f, by + 21.5f + 5.5f), "NEG CV", slewColor);

			ElenaToggle* mode = createParamCentered<ElenaToggle>(mm2px(Vec(bx + 38.f, by + 8.f)), module, pairParam(p, PAIR_MODE));
			mode->textOff = "TIME";
			mode->textOn = "RATE";
			mode->colorOn = nvgRGB(220, 150, 40);
			addParam(mode);
			ElenaToggle* autoP = createParamCentered<ElenaToggle>(mm2px(Vec(bx + 38.f, by + 20.f)), module, pairParam(p, PAIR_AUTO));
			autoP->textOff = "AUTO";
			autoP->textOn = "AUTO";
			autoP->colorOn = nvgRGB(220, 70, 70);
			addParam(autoP);

			// Zwei Slewer je Paar: IN, OUT, PULSE + zwei LEDs
			for (int j = 0; j < 2; j++) {
				int k = p * 2 + j;
				char letter = (char) ('C' + k);
				float y = by + (j == 0 ? 6.5f : 21.5f);
				float xIn = bx + 52.f, xOut = bx + 65.f, xPulse = bx + 78.f, xLed = bx + 86.6f;

				addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xIn, y)), module, Elena::SLEW_IN_C_INPUT + k));
				addLabel(Vec(xIn, y + 5.5f), string::f("%c IN", letter), slewColor, 8.f);
				addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(xOut, y)), module, Elena::SLEW_OUT_C_OUTPUT + k));
				addLabel(Vec(xOut, y + 5.5f), string::f("%c OUT", letter), slewColor, 8.f);
				addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(xPulse, y)), module, Elena::PULSE_C_OUTPUT + k));
				addLabel(Vec(xPulse, y + 5.5f), string::f("%c PLS", letter), slewColor, 8.f);

				addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(xLed, y - 2.4f)), module, Elena::PULSE_LED_C_LIGHT + k));
				addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(xLed, y + 2.4f)), module, Elena::CVOUT_LED_C_LIGHT + k));
			}
		}
	}

	void step() override {
		int shown = mod ? mod->displayedCh : 0;
		for (int s = 0; s < 2; s++) {
			for (int t = 0; t < NUM_AB; t++) {
				for (int c = 0; c < NUM_CH; c++) {
					if (abWidgets[s][t][c])
						abWidgets[s][t][c]->visible = (c == shown);
				}
			}
		}
		ModuleWidget::step();
	}

	// Kanalzahl je A/B-Sektion: Auto oder fest 1-16.
	void appendContextMenu(Menu* menu) override {
		Elena* m = mod;
		if (!m)
			return;
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

		// Eingangs-/Ausgangsbereich je Sektion.
		menu->addChild(new MenuSeparator);
		const char* rangeMenuNames[2] = {"Input range Section A", "Input range Section B"};
		const char* rangeLabels[NUM_RANGES] = {"0 to 10 V", "+/-5 V", "+/-10 V", "Unlimited (0-10 V scale)"};
		for (int s = 0; s < 2; s++) {
			menu->addChild(createSubmenuItem(rangeMenuNames[s], rangeLabels[m->inputRange[s]], [m, s, rangeLabels](Menu* sub) {
				for (int r = 0; r < NUM_RANGES; r++) {
					sub->addChild(createCheckMenuItem(rangeLabels[r], "",
						[m, s, r]() { return m->inputRange[s] == r; },
						[m, s, r]() { m->inputRange[s] = r; }));
				}
			}));
		}

		// DC-Blocker (ca. 10 Hz) getrennt je Ausgang: A, B und je Slew-Paar.
		menu->addChild(new MenuSeparator);
		const char* dcNames[5] = {"DC blocker Section A", "DC blocker Section B", "DC blocker Sections C/D",
		                          "DC blocker Sections E/F", "DC blocker Sections G/H"};
		for (int i = 0; i < 5; i++) {
			menu->addChild(createCheckMenuItem(dcNames[i], "",
				[m, i]() { return m->dcBlock[i]; },
				[m, i]() { m->dcBlock[i] = !m->dcBlock[i]; }));
		}
	}
};

} // namespace

Model* modelElena = createModel<Elena, ElenaWidget>("Elena");
