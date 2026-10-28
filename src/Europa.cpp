#include "plugin.hpp"
#include "EnvGen4.hpp"
#include "PhasorCore.hpp"
#include "PanelLabel.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

// ============================================================================
// Europa - Quad Function Generator mit VCA (fuenftes Modul des Plugins,
// Namensgeberin: Anguissola-Schwester). Vier unabhaengige Huellkurven-
// generatoren (Engine: EnvGen4.hpp), die GUI zeigt immer nur einen.
//
// Umschaltung nach dem Lucia-Muster (Kinovice-Entscheidung 2026-09-28):
// ALLE Parameter aller vier Generatoren sind echte Rack-Params (4 x 24 =
// 96, davon 5 je Generator fuer den Phasor-Modus). pendingGen/displayedGen sind bewusst KEINE Params (Undo/History,
// Automation und Preset-Recall sollen die reine GUI-Navigation nicht
// beeinflussen) und werden per dataToJson()/dataFromJson() gesichert.
// Nur die Widgets des angezeigten Generators sind sichtbar, alle vier
// Generatoren laufen immer weiter. Kein Kopieren, kein Pickup/Catch.
//
// Stand (Schritt 5 im Konzept): Modul, Params, Verdrahtung, Umschaltung,
// JSON (Schritt 4) plus Panel im Stil der uebrigen Module - Rahmen je
// Stage, PanelLabel-Beschriftung, Buchsen-Block mit Eingaengen links und
// Ausgaengen auf dunkler Platte, Zeilen-Hervorhebung des angezeigten
// Generators.
// ============================================================================

static const int EUROPA_NUM_GENS = 4;

struct Europa;

// Zeit-Regler: zeigt die aufgeloeste Zeit in Sekunden (abhaengig vom Range
// derselben Stage) und nimmt Eingaben wie "5 ms" oder "1,5 s" entgegen.
// Implementierung unten, nachdem struct Europa vollstaendig bekannt ist.
struct EnvTimeQuantity : ParamQuantity {
	int gen = 0;
	int stageIdx = 0;
	int rangeIdx();
	float getDisplayValue() override;
	void setDisplayValue(float displayValue) override;
	std::string getDisplayValueString() override;
	void setDisplayValueString(std::string s) override;
};

struct Europa : Module {
	// Enum-Regel wie im Rest des Plugins: neue Eintraege nur am ENDE
	// anhaengen. ENUMS-Bloecke sind generator-major nur innerhalb des Blocks
	// (Index = gen * 4 + stage bzw. gen), Bloecke selbst bleiben stabil.
	enum ParamId {
		ENUMS(LEVEL_ATTACK_PARAM, EUROPA_NUM_GENS),
		ENUMS(LEVEL_HOLD_PARAM, EUROPA_NUM_GENS),
		ENUMS(TIME_PARAM, EUROPA_NUM_GENS * ENV_NUM_STAGES),
		ENUMS(RANGE_PARAM, EUROPA_NUM_GENS * ENV_NUM_STAGES),
		ENUMS(CURVE_PARAM, EUROPA_NUM_GENS * ENV_NUM_STAGES),
		ENUMS(MODE_PARAM, EUROPA_NUM_GENS),
		ENUMS(RETRIG0_PARAM, EUROPA_NUM_GENS),
		ENUMS(LOOP_PARAM, EUROPA_NUM_GENS),
		// Delay-Pegel (Startpegel) und Release-Pegel (Ende/Ruhepegel),
		// 0..10 V, Default 0 V = bisheriges Verhalten (am Ende angehaengt)
		ENUMS(LEVEL_DELAY_PARAM, EUROPA_NUM_GENS),
		ENUMS(LEVEL_RELEASE_PARAM, EUROPA_NUM_GENS),
		// Quadratur-Kopplung je Paar (neu, 2026-10-01, Kinovice-Entscheidung):
		// 0 = aus, 1 = Follow: Generator 2 (bzw. 4) startet am Attack-Ende von
		// Generator 1 (bzw. 3), 2 = Handshake (2026-10-05): gekoppelte
		// Decays, siehe EnvQuadMode in EnvGen4.hpp. Alte Patches (0/1)
		// bleiben unveraendert. Index 0 = Paar 1+2, Index 1 = Paar 3+4.
		ENUMS(QUAD_PARAM, 2),
		// Phasor-Modus je Generator (neu, 2026-10-06, Kinovice-Entscheidung, Engine:
		// PhasorCore.hpp). PHASOR: T1/GATE-Buchse wird zum Phasor-Eingang, die
		// Stage-Zeiten werden zu Gewichten des Zyklus. PHASOR_OUT: was ENV OUT im
		// Phasor-Modus ausgibt (0 Huellkurve, 1 Phasor linear, 2 Phasor verformt).
		// RATIO_P/Q: Verhaeltnis p/q (1..16), PHASE_OFFSET: 0..1 Zyklusanteil.
		ENUMS(PHASOR_PARAM, EUROPA_NUM_GENS),
		ENUMS(PHASOR_OUT_PARAM, EUROPA_NUM_GENS),
		ENUMS(RATIO_P_PARAM, EUROPA_NUM_GENS),
		ENUMS(RATIO_Q_PARAM, EUROPA_NUM_GENS),
		ENUMS(PHASE_OFFSET_PARAM, EUROPA_NUM_GENS),
		NUM_PARAMS
	};
	enum InputId {
		ENUMS(SIGNAL_INPUT, EUROPA_NUM_GENS),
		ENUMS(GATE_INPUT, EUROPA_NUM_GENS),
		ENUMS(TRIG_INPUT, EUROPA_NUM_GENS),
		// Poly: ch 1-4 = Zeit je Stage, ch 5 = Rate-CV (neu, 2026-10-01,
		// Kinovice-Entscheidung): bipolar, exponentiell, skaliert die
		// Geschwindigkeit des Generators formtreu (siehe EnvParams::rate).
		// Mono-/4-Kanal-Kabel lassen Kanal 5 unbelegt = Faktor 1.
		ENUMS(TIMECV_INPUT, EUROPA_NUM_GENS),
		// Poly-CV auf die vier Pegel (ch 1-4 = Delay/Attack/Hold/Release),
		// am Ende angehaengt
		ENUMS(LEVELCV_INPUT, EUROPA_NUM_GENS),
		// Hold-CV (neu, 2026-09-28, Kinovice-Entscheidung): eigene Buchse statt
		// TIME-CV-Kanal 3, ersetzt im One-Shot (inkl. Loop) den Zeit-Regler der
		// Hold-Stage komplett (nur der Range-Schalter bleibt wirksam). Mono,
		// 0-10V linear als Reglerposition ueber envResolveTime(), wie am
		// Original: 0V = kein Hold, ansteigend bis zum unendlichen Halten am
		// oberen Ende (ENV_TIME_INFINITE_VOLTS), kein Gate-Bezug. Im
		// Sustain-Modus und ohne Kabel bleibt die Hold-Zeit wie bisher (Regler
		// + TIMECV_INPUT Kanal 3).
		ENUMS(HOLDCV_INPUT, EUROPA_NUM_GENS),
		NUM_INPUTS
	};
	enum OutputId {
		ENUMS(VCA_OUTPUT, EUROPA_NUM_GENS),
		ENUMS(ENV_OUTPUT, EUROPA_NUM_GENS),
		ENUMS(EOC_OUTPUT, EUROPA_NUM_GENS),
		// OR-Ausgaenge (neu, 2026-10-01, Kinovice-Entscheidung): Maximum der
		// beiden ENV-Pegel eines Paars (Index 0 = Gen 1+2, Index 1 = Gen 3+4),
		// vor dem VCA, 0-10 V. Rein ausgangsseitig, beeinflusst die Engines nicht.
		ENUMS(OR_OUTPUT, 2),
		NUM_OUTPUTS
	};
	enum LightId {
		// RGB: je Generator drei aufeinanderfolgende Lights (R, G, B)
		ENUMS(RUN_LIGHT, EUROPA_NUM_GENS * 3),
		ENUMS(SELECT_LIGHT, EUROPA_NUM_GENS),
		// Gelb: Ratio-Aenderung (p/q) wartet auf einen gueltigen Eingangs-Wrap
		ENUMS(PENDING_LIGHT, EUROPA_NUM_GENS),
		NUM_LIGHTS
	};

	// GUI-Navigation, kein Param (siehe Kopfkommentar).
	int pendingGen = 0;
	int displayedGen = 0;

	EnvGen4 gens[EUROPA_NUM_GENS];

	// Phasor-Modus (siehe PhasorCore.hpp). Im Phasor-Modus eines Generators
	// bleibt gens[g] im Ruhezustand (Quadratur-Partner sehen dann "nicht laufend").
	PhasorCore phasor[EUROPA_NUM_GENS];
	bool prevPhasorMode[EUROPA_NUM_GENS] = {false, false, false, false};
	float phasorInVolts[EUROPA_NUM_GENS] = {0.f, 0.f, 0.f, 0.f};
	bool ratioPending[EUROPA_NUM_GENS] = {false, false, false, false};
	// Zuletzt uebernommene Ratio-Werte (Hysterese gegen Flackern bei CV auf einer
	// Rundungsgrenze, siehe process()).
	int ratioPLast[EUROPA_NUM_GENS] = {1, 1, 1, 1};
	int ratioQLast[EUROPA_NUM_GENS] = {1, 1, 1, 1};

	// Anti-Aliasing (minBLEP) fuer echte Pegelspruenge der Huellkurve (Zeit-0-
	// Stages, Retrig-Sprung). Modulweit schaltbar, kein Param (JSON), Standard
	// AUS = bisheriges Verhalten. Wirkt auf ENV, VCA und OR; EOC und die
	// Engine-Logik (Quadratur, Gates) bleiben unberuehrt.
	bool antialias = false;
	// Ratio-Wechsel im Phasor-Modus: AUS = strikt (Standard, nur wenn y vor und nach
	// dem Wechsel exakt auf dem Zyklusanfang liegt, siehe PhasorCore.hpp); AN =
	// locker (an jedem Eingangs-Wrap, y springt dabei; mit Antialiasing geglaettet).
	bool ratioLoose = false;
	dsp::MinBlepGenerator<16, 16, float> minBlep[EUROPA_NUM_GENS];
	float envAA[EUROPA_NUM_GENS] = {0.f, 0.f, 0.f, 0.f};

	// Cache der aufgeloesten Stage-Zeiten: envResolveTime() enthaelt ein
	// pow() - nur bei Aenderung von Regler/CV/Range/Modus neu rechnen.
	struct TimeCache {
		float knob = -1e9f;
		float cv = -1e9f;
		int range = -1;
		bool allowInfinite = false;
		float value = 0.f;
	};
	TimeCache timeCache[EUROPA_NUM_GENS][ENV_NUM_STAGES];
	// Eigener Cache fuer die Hold-CV (siehe HOLDCV_INPUT oben) - gleiche
	// Idee wie timeCache[], aber ohne Regler-Anteil (der Knob ist hier
	// wirkungslos), Schluessel ist nur CV-Spannung + Range.
	TimeCache holdCvCache[EUROPA_NUM_GENS];

	// Cache der Rate-CV (exp2 nur bei Spannungsaenderung)
	float rateCacheVolts[EUROPA_NUM_GENS] = {0.f, 0.f, 0.f, 0.f};
	float rateCacheValue[EUROPA_NUM_GENS] = {1.f, 1.f, 1.f, 1.f};

	dsp::ClockDivider lightDivider;
	// Lief der Generator irgendwann im aktuellen LED-Fenster? Ein Patch-Loop
	// (EOC -> T1/GATE) steht zwischen zwei Zyklen fuer genau 1 Sample still;
	// die nur alle 64 Samples abgetastete LED wuerde dabei rot aufblitzen.
	bool runSeen[EUROPA_NUM_GENS] = {false, false, false, false};

	Europa() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		static const char* stageNames[ENV_NUM_STAGES] = {"Delay", "Attack", "Hold", "Release"};

		for (int g = 0; g < EUROPA_NUM_GENS; g++) {
			std::string gn = string::f("Gen %d ", g + 1);

			configParam(LEVEL_ATTACK_PARAM + g, 0.f, 10.f, 10.f, gn + "Attack level", " V");
			configParam(LEVEL_HOLD_PARAM + g, 0.f, 10.f, 10.f, gn + "Hold level", " V");
			configParam(LEVEL_DELAY_PARAM + g, 0.f, 10.f, 0.f, gn + "Delay level (start)", " V");
			configParam(LEVEL_RELEASE_PARAM + g, 0.f, 10.f, 0.f, gn + "Release level (end / rest)", " V");

			for (int s = 0; s < ENV_NUM_STAGES; s++) {
				int i = g * ENV_NUM_STAGES + s;
				EnvTimeQuantity* tq = configParam<EnvTimeQuantity>(TIME_PARAM + i, 0.f, 1.f, 0.5f,
					gn + stageNames[s] + " time");
				tq->gen = g;
				tq->stageIdx = s;

				configSwitch(RANGE_PARAM + i, 0.f, 3.f, 2.f, gn + stageNames[s] + " time range",
					{"TR1 (0-30 ms)", "TR2 (0-300 ms)", "TR3 (0-3 s)", "TR4 (0-30 s)"});

				configParam(CURVE_PARAM + i, -1.f, 1.f, 0.f, gn + stageNames[s] + " curve (- round / + pointed)",
					"%", 0.f, 100.f);
			}

			configSwitch(MODE_PARAM + g, 0.f, 1.f, 0.f, gn + "mode", {"One-Shot", "Sustain"});
			configSwitch(RETRIG0_PARAM + g, 0.f, 1.f, 0.f, gn + "retrigger from delay level",
				{"Off (from current level)", "On (from delay level)"});
			// Dreistufig (Auto-Cycle-Session): 0/1 behalten ihre alte Bedeutung
			// (Off / Loop bei gehaltenem Gate), damit gespeicherte Patches gleich
			// bleiben; 2 = freier Zyklus ohne Gate.
			configSwitch(LOOP_PARAM + g, 0.f, 2.f, 0.f, gn + "loop (One-Shot only)",
				{"Off", "Loop while gate high", "Free-running cycle"});

			configSwitch(PHASOR_PARAM + g, 0.f, 1.f, 0.f, gn + "phasor mode (T1/GATE = phasor input)",
				{"Off (gate / trigger)", "On (phasor input)"});
			configSwitch(PHASOR_OUT_PARAM + g, 0.f, 2.f, 0.f, gn + "ENV out in phasor mode",
				{"Envelope", "Phasor (linear)", "Phasor (warped by stage weights)"});
			configParam(RATIO_P_PARAM + g, 1.f, 16.f, 1.f, gn + "ratio numerator (p)");
			paramQuantities[RATIO_P_PARAM + g]->snapEnabled = true;
			configParam(RATIO_Q_PARAM + g, 1.f, 16.f, 1.f, gn + "ratio denominator (q)");
			paramQuantities[RATIO_Q_PARAM + g]->snapEnabled = true;
			configParam(PHASE_OFFSET_PARAM + g, 0.f, 1.f, 0.f, gn + "phase offset", "%", 0.f, 100.f);

			configInput(SIGNAL_INPUT + g, gn + "VCA signal (normalled to 10 V)");
			configInput(GATE_INPUT + g, gn + "T1 / Gate (phasor mode: phasor input 0-10 V, unpatched = previous generator's input if it is in phasor mode)");
			configInput(TRIG_INPUT + g, gn + "Trigger (poly, ch 1 = phasor reset, ch 2-4 = stage 2-4)");
			configInput(TIMECV_INPUT + g, gn + "Time CV (poly, ch 1-4 = stage 1-4; ch 5 = rate: 1 oct/V, +-5 V; phasor mode: ch 5 = offset, ch 6 = ratio numerator p, ch 7 = ratio denominator q, 10 V = full range)");
			configInput(LEVELCV_INPUT + g, gn + "Level CV (poly, ch 1-4 = Delay/Attack/Hold/Release level; ch 5-8 = Delay/Attack/Hold/Release curve, +-5 V = full range)");
			configInput(HOLDCV_INPUT + g, gn + "Hold CV (0-10V, no hold .. infinite; One-Shot/Loop only, overrides Hold knob when patched)");

			configOutput(VCA_OUTPUT + g, gn + "VCA out");
			configOutput(ENV_OUTPUT + g, gn + "Envelope");
			configOutput(EOC_OUTPUT + g, gn + "End of cycle");
		}
		configSwitch(QUAD_PARAM + 0, 0.f, 2.f, 0.f, "Quadrature 1+2",
			{"Off", "Follow (gen 2 starts at end of gen 1 attack)", "Handshake (coupled decays)"});
		configSwitch(QUAD_PARAM + 1, 0.f, 2.f, 0.f, "Quadrature 3+4",
			{"Off", "Follow (gen 4 starts at end of gen 3 attack)", "Handshake (coupled decays)"});
		configOutput(OR_OUTPUT + 0, "OR of envelopes 1+2 (maximum, before VCA)");
		configOutput(OR_OUTPUT + 1, "OR of envelopes 3+4 (maximum, before VCA)");

		lightDivider.setDivision(64);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int g = 0; g < EUROPA_NUM_GENS; g++) {
			gens[g].reset();
			phasor[g].reset();
			prevPhasorMode[g] = false;
			phasorInVolts[g] = 0.f;
			ratioPending[g] = false;
			minBlep[g] = dsp::MinBlepGenerator<16, 16, float>();
			for (int s = 0; s < ENV_NUM_STAGES; s++)
				timeCache[g][s] = TimeCache();
			holdCvCache[g] = TimeCache();
		}
		pendingGen = 0;
		displayedGen = 0;
	}

	void process(const ProcessArgs& args) override {
		const float dt = args.sampleTime;

		for (int g = 0; g < EUROPA_NUM_GENS; g++) {
			EnvGen4& gen = gens[g];
			EnvParams p;

			bool sustain = params[MODE_PARAM + g].getValue() > 0.5f;
			p.sustainMode = sustain;
			// Phasor-Modus: Wechsel setzt beide Engines zurueck und verwirft die
			// Zeit-Caches (allowInfinite haengt am Modus).
			bool phasorMode = params[PHASOR_PARAM + g].getValue() > 0.5f;
			if (phasorMode != prevPhasorMode[g]) {
				prevPhasorMode[g] = phasorMode;
				gen.reset();
				phasor[g].reset();
				for (int s = 0; s < ENV_NUM_STAGES; s++)
					timeCache[g][s] = TimeCache();
				holdCvCache[g] = TimeCache();
			}
			float loopValue = params[LOOP_PARAM + g].getValue();
			p.loopEnabled = loopValue > 0.5f;
			p.freeCycle = loopValue > 1.5f;
			// Quadratur (siehe envQuadSetup() in EnvGen4.hpp): Generator 2/4 ist
			// der Follower seines Partners (g - 1, laeuft in dieser Schleife
			// bereits vor g), Generator 1/3 der Master. Im Follower-Betrieb
			// ersetzt der Gate-Zustand des Masters das eigene T1/GATE. Follow:
			// FREE verhaelt sich wie GATE; Handshake: FREE bleibt erhalten.
			// Der Handshake ist im Sustain-Modus nicht definiert (dann Follow).
			bool follower = (g % 2 == 1);
			int pairBase = g - (g % 2);
			int quadMode = (int) std::lround(params[QUAD_PARAM + g / 2].getValue());
			bool pairSustain = params[MODE_PARAM + pairBase].getValue() > 0.5f
				|| params[MODE_PARAM + pairBase + 1].getValue() > 0.5f;
			// p.loopEnabled/p.freeCycle sind oben bereits gesetzt
			bool quadStart = false;
			if (!phasorMode) // im Phasor-Modus keine Kopplung (p.coupled bleibt false)
				quadStart = envQuadSetup(p, quadMode, follower, pairSustain,
					gen, gens[follower ? g - 1 : g + 1]);
			bool coupled = p.coupled;
			p.retrigZero = params[RETRIG0_PARAM + g].getValue() > 0.5f;
			// Pegel = Regler + Level-CV (1 V = 1 V), begrenzt auf 0..10 V
			// (unipolar). Poly: ch 1 = Delay, 2 = Attack, 3 = Hold, 4 = Release.
			Input& levelCv = inputs[LEVELCV_INPUT + g];
			int lvChannels = levelCv.isConnected() ? levelCv.getChannels() : 0;
			auto level = [&](int paramId, int ch) {
				float v = params[paramId + g].getValue();
				if (ch < lvChannels)
					v += levelCv.getVoltage(ch);
				return rack::clamp(v, 0.f, 10.f);
			};
			p.levelDelay = level(LEVEL_DELAY_PARAM, 0);
			p.levelAttack = level(LEVEL_ATTACK_PARAM, 1);
			p.levelHold = level(LEVEL_HOLD_PARAM, 2);
			p.levelRelease = level(LEVEL_RELEASE_PARAM, 3);

			Input& timeCv = inputs[TIMECV_INPUT + g];
			int cvChannels = timeCv.isConnected() ? timeCv.getChannels() : 0;

			for (int s = 0; s < ENV_NUM_STAGES; s++) {
				int i = g * ENV_NUM_STAGES + s;
				float knob = params[TIME_PARAM + i].getValue();
				int range = (int) std::lround(params[RANGE_PARAM + i].getValue());
				float cv = (s < cvChannels) ? timeCv.getVoltage(s) : 0.f;
				bool allowInfinite = !sustain && !phasorMode; // "Halten" ist im Sustain-Modus ohne Wirkung

				TimeCache& c = timeCache[g][s];
				if (knob != c.knob || cv != c.cv || range != c.range || allowInfinite != c.allowInfinite) {
					c.value = envResolveTime(knob, cv, range, allowInfinite);
					c.knob = knob;
					c.cv = cv;
					c.range = range;
					c.allowInfinite = allowInfinite;
				}
				p.time[s] = c.value;
				// Curve = Regler + Curve-CV (LEVEL CV Kanal 5-8, +-5 V = voller Bereich
				// -1..+1), begrenzt auf -1..1. Gilt in beiden Modi.
				float curveVal = params[CURVE_PARAM + i].getValue();
				if (ENV_NUM_STAGES + s < lvChannels)
					curveVal += levelCv.getVoltage(ENV_NUM_STAGES + s) * 0.2f;
				p.curve[s] = rack::clamp(curveVal, -1.f, 1.f);
			}

			// Rate-CV: Kanal 5 am TIME-CV (Index 4), exponentiell, 0 V = x1.
			// exp2 nur bei Spannungsaenderung neu berechnen.
			float rateVolts = (cvChannels > ENV_NUM_STAGES) ? timeCv.getVoltage(ENV_NUM_STAGES) : 0.f;
			if (rateVolts != rateCacheVolts[g]) {
				rateCacheVolts[g] = rateVolts;
				rateCacheValue[g] = envRateFromVolts(rateVolts);
			}
			p.rate = rateCacheValue[g];

			// Hold-CV (siehe HOLDCV_INPUT oben): ueberschreibt p.time[HOLD], nur
			// im One-Shot (Loop eingeschlossen, da Loop nur eine Variante des
			// One-Shot ist) und nur wenn ein Kabel liegt - unpatched bleibt die
			// Hold-Stage unveraendert (Regler + TIMECV_INPUT Kanal 3), damit
			// bestehende Patches unveraendert klingen.
			Input& holdCv = inputs[HOLDCV_INPUT + g];
			if ((phasorMode || !sustain) && holdCv.isConnected()) {
				float hv = holdCv.getVoltage();
				int holdRange = (int) std::lround(
					params[RANGE_PARAM + g * ENV_NUM_STAGES + ENV_STAGE_HOLD].getValue());
				TimeCache& hc = holdCvCache[g];
				if (hv != hc.cv || holdRange != hc.range) {
					hc.value = envResolveTime(0.f, hv, holdRange, !phasorMode);
					hc.cv = hv;
					hc.range = holdRange;
				}
				p.time[ENV_STAGE_HOLD] = hc.value;
			}

			// Ergebnis der jeweils aktiven Engine (klassisch oder Phasor)
			float envRaw = 0.f;
			bool eocNow = false;
			bool runNow = false;
			int jumpCount = 0;
			const float* jumpPos = nullptr;
			const float* jumpDelta = nullptr;
			float phasorOutV = 0.f;
			int outMode = PHASOR_OUT_ENV;

			if (phasorMode) {
				// Phasor-Eingang = T1/GATE. Ohne Kabel: Phasor des vorherigen
				// Generators, sofern dieser ebenfalls im Phasor-Modus ist (ein Kabel
				// an Gen 1 speist alle). Reset = TRIG Kanal 1 (in Europa bisher
				// ungenutzt).
				Input& phIn = inputs[GATE_INPUT + g];
				float pv = 0.f;
				if (phIn.isConnected())
					pv = phIn.getVoltage();
				else if (g > 0 && prevPhasorMode[g - 1])
					pv = phasorInVolts[g - 1];
				phasorInVolts[g] = pv;
				Input& rst = inputs[TRIG_INPUT + g];
				float resetV = rst.isConnected() ? rst.getVoltage(0) : 0.f;

				// Ratio und Offset: Regler + TIME CV Kanal 5-7 (Phasor-Modus: Kanal 5 =
				// Offset, 6 = p, 7 = q; 10 V = voller Regelweg). p/q werden auf 1..16
				// gerundet, mit Hysterese (0.1) gegen Flackern auf der Rundungsgrenze.
				auto snapHyst = [](float fv, int& last) -> int { // expliziter Rueckgabetyp (C++11)
					fv = rack::clamp(fv, 1.f, (float) PHASOR_RATIO_MAX);
					if (std::fabs(fv - (float) last) > 0.6f)
						last = (int) std::lround(fv);
					return last;
				};
				const float ratioSpan = (float) (PHASOR_RATIO_MAX - 1);
				float pv15 = params[RATIO_P_PARAM + g].getValue();
				float qv15 = params[RATIO_Q_PARAM + g].getValue();
				float off = params[PHASE_OFFSET_PARAM + g].getValue();
				if (cvChannels > 4)
					off += timeCv.getVoltage(4) * 0.1f;
				if (cvChannels > 5)
					pv15 += timeCv.getVoltage(5) * 0.1f * ratioSpan;
				if (cvChannels > 6)
					qv15 += timeCv.getVoltage(6) * 0.1f * ratioSpan;
				off -= std::floor(off); // Offset laeuft als Phase um (Wrap)
				int rp = snapHyst(pv15, ratioPLast[g]);
				int rq = snapHyst(qv15, ratioQLast[g]);
				PhasorCore& ph = phasor[g];
				ph.process(dt, p, rp, rq, off, pv, resetV, ratioLoose);

				envRaw = ph.envOut;
				eocNow = ph.eocOut;
				runNow = ph.running;
				jumpCount = ph.jumpCount;
				jumpPos = ph.jumpPos;
				jumpDelta = ph.jumpDelta;
				ratioPending[g] = ph.pending;
				outMode = (int) std::lround(params[PHASOR_OUT_PARAM + g].getValue());
				phasorOutV = (outMode == PHASOR_OUT_LINEAR) ? ph.phasorLinear * 10.f
				           : (outMode == PHASOR_OUT_WARPED) ? ph.phasorWarped * 10.f : 0.f;
			}
			else {
				phasorInVolts[g] = 0.f;
				ratioPending[g] = false;

				// Stage-1-Eingang mono; Trigger poly: Kanal 2-4 = Stage 2-4,
				// Kanal 1 bleibt im klassischen Modus ungenutzt (Konzept Abschnitt 7).
				float gateVolts = coupled ? (gens[g - 1].gateHigh ? 10.f : 0.f)
				                          : inputs[GATE_INPUT + g].getVoltage();
				float trigVolts[ENV_NUM_STAGES] = {0.f, 0.f, 0.f, 0.f};
				Input& trig = inputs[TRIG_INPUT + g];
				int trigChannels = trig.isConnected() ? trig.getChannels() : 0;
				for (int s = 1; s < ENV_NUM_STAGES; s++) {
					if (s < trigChannels)
						trigVolts[s] = trig.getVoltage(s);
				}

				gen.process(dt, p, gateVolts, trigVolts, quadStart);
				envRaw = gen.envOut;
				eocNow = gen.eocOut;
				runNow = gen.running;
				jumpCount = gen.jumpCount;
				jumpPos = gen.jumpPos;
				jumpDelta = gen.jumpDelta;
			}
			if (runNow)
				runSeen[g] = true;

			// Anti-Aliasing: die Engine meldet echte Spruenge, minBLEP glaettet
			// sie. process() laeuft immer (Puffer leert sich), eingefuegt wird nur
			// bei aktivem Schalter. Ergebnis auf 0..10 V begrenzt (minBLEP
			// schwingt leicht ueber).
			float envOut = envRaw;
			if (antialias) {
				for (int j = 0; j < jumpCount; j++)
					minBlep[g].insertDiscontinuity(jumpPos[j], jumpDelta[j]);
			}
			float corr = minBlep[g].process();
			if (antialias)
				envOut = rack::clamp(envOut + corr, 0.f, 10.f);
			envAA[g] = envOut;

			// Ausgaenge
			// Phasor-Modus: ENV OUT kann den Phasor ausgeben; VCA und OR arbeiten
			// weiter mit der Huellkurve.
			bool phasorOnEnv = phasorMode && outMode != PHASOR_OUT_ENV;
			outputs[ENV_OUTPUT + g].setVoltage(phasorOnEnv ? phasorOutV : envOut);
			outputs[EOC_OUTPUT + g].setVoltage(eocNow ? 10.f : 0.f);

			// VCA: Eingang ohne Kabel = 10 V normalisiert, die Huellkurve
			// (0-10 V) wirkt als Gain 0..1 gleichmaessig auf alle Kanaele.
			Output& vca = outputs[VCA_OUTPUT + g];
			if (vca.isConnected()) {
				Input& in = inputs[SIGNAL_INPUT + g];
				bool inConnected = in.isConnected();
				int channels = inConnected ? std::max(in.getChannels(), 1) : 1;
				float gain = envOut * 0.1f;
				for (int c = 0; c < channels; c++) {
					float v = inConnected ? in.getVoltage(c) : 10.f;
					vca.setVoltage(v * gain, c);
				}
				vca.setChannels(channels);
			}
		}

		// OR-Ausgaenge: Maximum pro Paar (Gen 1+2, Gen 3+4), ENV-Pegel vor dem VCA
		outputs[OR_OUTPUT + 0].setVoltage(std::max(envAA[0], envAA[1]));
		outputs[OR_OUTPUT + 1].setVoltage(std::max(envAA[2], envAA[3]));

		if (lightDivider.process()) {
			float lightDt = dt * lightDivider.getDivision();
			for (int g = 0; g < EUROPA_NUM_GENS; g++) {
				const EnvGen4& gen = gens[g];
				// Rot = Stopp, Gruen = laeuft, Amber = Halten (Konzept Abschnitt 8).
				// "Laeuft" gilt fuer das ganze Fenster seit der letzten
				// Aktualisierung (runSeen), Rot nur bei vollstaendigem Leerlauf.
				// Immer nur ein Kanal aktiv (siehe EuropaStatusLight). Die
				// Helligkeiten sind bei Bedarf hier nachstellbar.
				// Helligkeit = Deckkraft der Farbe (normale Ueberblendung,
				// siehe EuropaStatusLight)
				const float STOP_BRIGHTNESS = 0.85f;
				const float RUN_BRIGHTNESS = 1.0f;
				const float HOLD_BRIGHTNESS = 1.0f;
				bool active = runSeen[g] || gen.running;
				runSeen[g] = false;
				float stopL = 0.f, runL = 0.f, holdL = 0.f;
				if (active && gen.running && gen.held)
					holdL = HOLD_BRIGHTNESS;
				else if (active)
					runL = RUN_BRIGHTNESS;
				else
					stopL = STOP_BRIGHTNESS;
				lights[RUN_LIGHT + g * 3 + 0].setBrightnessSmooth(stopL, lightDt);
				lights[RUN_LIGHT + g * 3 + 1].setBrightnessSmooth(runL, lightDt);
				lights[RUN_LIGHT + g * 3 + 2].setBrightnessSmooth(holdL, lightDt);

				// Auswahl-LED: angezeigter Generator hell, nur vorgewaehlter schwach
				const float SELECT_BRIGHTNESS = 1.0f;
				const float SELECT_PENDING_BRIGHTNESS = 0.35f;
				float sel = (g == displayedGen) ? SELECT_BRIGHTNESS : ((g == pendingGen) ? SELECT_PENDING_BRIGHTNESS : 0.f);
				lights[SELECT_LIGHT + g].setBrightnessSmooth(sel, lightDt);
				lights[PENDING_LIGHT + g].setBrightnessSmooth(ratioPending[g] ? 1.f : 0.f, lightDt);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "pendingGen", json_integer(pendingGen));
		json_object_set_new(rootJ, "displayedGen", json_integer(displayedGen));
		json_object_set_new(rootJ, "antialias", json_boolean(antialias));
		json_object_set_new(rootJ, "ratioLoose", json_boolean(ratioLoose));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* pendingJ = json_object_get(rootJ, "pendingGen");
		if (pendingJ)
			pendingGen = rack::clamp((int) json_integer_value(pendingJ), 0, EUROPA_NUM_GENS - 1);
		json_t* displayedJ = json_object_get(rootJ, "displayedGen");
		if (displayedJ)
			displayedGen = rack::clamp((int) json_integer_value(displayedJ), 0, EUROPA_NUM_GENS - 1);
		json_t* aaJ = json_object_get(rootJ, "antialias");
		antialias = aaJ ? json_is_true(aaJ) : false; // alte Patches: aus
		json_t* rlJ = json_object_get(rootJ, "ratioLoose");
		ratioLoose = rlJ ? json_is_true(rlJ) : false; // alte Patches: strikt
	}
};

// ---------------------------------------------------------------------------
// EnvTimeQuantity
// ---------------------------------------------------------------------------
static std::string formatEnvSeconds(float t) {
	if (t <= 0.f)
		return "0 ms";
	if (t < 0.001f)
		return string::f("%.0f \xC2\xB5s", t * 1e6f);
	if (t < 1.f)
		return string::f("%.2f ms", t * 1e3f);
	return string::f("%.2f s", t);
}

int EnvTimeQuantity::rangeIdx() {
	Europa* m = dynamic_cast<Europa*>(module);
	if (!m)
		return 2;
	return (int) std::lround(m->params[Europa::RANGE_PARAM + gen * ENV_NUM_STAGES + stageIdx].getValue());
}

// Anzeige-/Eingabewert ist die Zeit in SEKUNDEN, der rohe Param bleibt die
// Reglerposition 0..1. Rack's Standard-Eingabe (Rechtsklick-Feld) haengt an
// getDisplayValue()/setDisplayValue() - ohne diese Overrides landete eine
// eingegebene Zahl direkt als Reglerposition und wurde auf 0/1 geklemmt
// (gleicher Fehler wie bei TempoQuantity/StageTimeQuantity in Sofonisba).
float EnvTimeQuantity::getDisplayValue() {
	return envTimeFromPosition(getValue(), rangeIdx());
}

void EnvTimeQuantity::setDisplayValue(float displayValue) {
	setValue(envPositionFromTime(displayValue, rangeIdx()));
}

std::string EnvTimeQuantity::getDisplayValueString() {
	return formatEnvSeconds(getDisplayValue());
}

// Eingabe: "5 ms", "1,5 s", "200 us". Ohne Einheit gilt die Einheit des
// aktuell angezeigten Werts (unter 1 s = ms, sonst s) - eine nackte "5"
// im Feld, das gerade "12.50 ms" zeigt, bedeutet also 5 ms.
void EnvTimeQuantity::setDisplayValueString(std::string s) {
	std::replace(s.begin(), s.end(), ',', '.');
	const char* begin = s.c_str();
	char* end = nullptr;
	float v = std::strtof(begin, &end);
	if (end == begin || !std::isfinite(v))
		return;
	std::string unit(end);
	std::transform(unit.begin(), unit.end(), unit.begin(), [](unsigned char c) { return (char) std::tolower(c); });

	float seconds;
	if (unit.find("ms") != std::string::npos)
		seconds = v * 1e-3f;
	else if (unit.find("us") != std::string::npos || unit.find("\xC2\xB5") != std::string::npos)
		seconds = v * 1e-6f;
	else if (unit.find('s') != std::string::npos)
		seconds = v;
	else
		seconds = (getDisplayValue() < 1.f) ? v * 1e-3f : v; // keine Einheit
	setDisplayValue(seconds);
}

// ---------------------------------------------------------------------------
// GUI (Schritt 5). Bewusst eigene Kopien der Lucia-Widgets
// (StageSelectKnob/StageActivateButton) statt Export aus Marf248.cpp - dort
// sind sie file-lokal, Sofonisba/Lucia bleiben unberuehrt. Beschriftung ueber
// das zentrale PanelLabel.hpp (gleiche Schrift/Farbpalette wie die uebrigen
// Module).
// ---------------------------------------------------------------------------
struct EuropaPanelBackground : Widget {
	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, nvgRGB(228, 228, 226));
		nvgFill(args.vg);
	}
};


// Rein dekorative Flaeche/Rahmen (TransparentWidget: faengt keine Maus-
// Events ab, die Regler darunter/darueber bleiben normal bedienbar). Gleiche
// Rahmenoptik wie StageFrame in Sofonisba; optional mit Fuellung fuer die
// dunklen Flaechen (Stage-Namensband, Ausgangsplatte).
struct EuropaFrame : TransparentWidget {
	NVGcolor strokeColor = nvgRGBA(90, 90, 100, 170);
	NVGcolor fillColor = nvgRGBA(0, 0, 0, 0);
	float cornerRadius = 3.f;
	bool drawStroke = true;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, cornerRadius);
		if (fillColor.a > 0.f) {
			nvgFillColor(args.vg, fillColor);
			nvgFill(args.vg);
		}
		if (drawStroke) {
			nvgStrokeColor(args.vg, strokeColor);
			nvgStrokeWidth(args.vg, 1.f);
			nvgStroke(args.vg);
		}
	}
};

// Hebt die Buchsenzeile des ANGEZEIGTEN Generators hervor (blaues Band,
// gleiche Farbe wie die Auswahl-LEDs). Ohne Modul (Modul-Browser) Gen 1.
struct EuropaRowHighlight : TransparentWidget {
	Europa* module = nullptr;
	int gen = 0;

	void draw(const DrawArgs& args) override {
		int shown = module ? module->displayedGen : 0;
		if (shown != gen)
			return;
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 2.f);
		nvgFillColor(args.vg, nvgRGBA(40, 110, 255, 46));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(40, 110, 255, 130));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}
};

// Gruenes Feld hinter der "ON"-Beschriftung (oben) eines Generator-Schalters,
// solange der Schalter des ANGEZEIGTEN Generators oben steht. Rein
// dekorativ (TransparentWidget), liest den Param direkt vom Modul.
struct EuropaOnIndicator : TransparentWidget {
	Europa* module = nullptr;
	int paramBase = 0;
	// Leuchtet, solange lo < Param-Wert <= hi (Standard: Schalter oben bei
	// zweistufigen Schaltern; fuer die dreistufige LOOP-Stellung einstellbar).
	float lo = 0.5f;
	float hi = 1e9f;

	void draw(const DrawArgs& args) override {
		if (!module)
			return;
		int g = module->displayedGen;
		float v = module->params[paramBase + g].getValue();
		if (v <= lo || v > hi)
			return;
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 1.2f);
		nvgFillColor(args.vg, nvgRGB(90, 220, 110)); // = Activate-Button/Status-LED (aktiv)
		nvgFill(args.vg);
	}
};

// Auswahl-Knob (Generator 1-4), kein Param - schreibt auf module->pendingGen.
struct GenSelectKnob : OpaqueWidget {
	Europa* module = nullptr;
	float dragAccumPx = 0.f;

	GenSelectKnob() {
		box.size = mm2px(Vec(14.f, 14.f));
	}

	void onButton(const ButtonEvent& e) override {
		OpaqueWidget::onButton(e);
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT)
			e.consume(this);
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
			module->pendingGen = std::min(module->pendingGen + 1, EUROPA_NUM_GENS - 1);
			dragAccumPx -= pxPerStep;
		}
		while (dragAccumPx <= -pxPerStep) {
			module->pendingGen = std::max(module->pendingGen - 1, 0);
			dragAccumPx += pxPerStep;
		}
	}

	void draw(const DrawArgs& args) override {
		float r = box.size.x / 2.f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, r, r, r - 1.f);
		nvgFillColor(args.vg, nvgRGB(40, 40, 44));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		int pending = module ? module->pendingGen : 0;
		int displayed = module ? module->displayedGen : 0;
		NVGcolor textColor = (pending == displayed) ? nvgRGB(90, 220, 110) : nvgRGB(235, 170, 60);

		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Nunito-Bold.ttf"));
		if (font && font->handle >= 0) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 15.f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, textColor);
			std::string label = string::f("%d", pending + 1);
			nvgText(args.vg, r, r, label.c_str(), NULL);
		}
	}
};

// Aktivieren-Button: pendingGen -> displayedGen. Rot solange die Auswahl
// noch nicht uebernommen ist, gruen wenn beide uebereinstimmen.
struct GenActivateButton : OpaqueWidget {
	Europa* module = nullptr;

	GenActivateButton() {
		box.size = mm2px(Vec(8.f, 8.f));
	}

	void onButton(const ButtonEvent& e) override {
		OpaqueWidget::onButton(e);
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (module)
				module->displayedGen = module->pendingGen;
			e.consume(this);
		}
	}

	void draw(const DrawArgs& args) override {
		bool isActive = module && (module->pendingGen == module->displayedGen);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 1.5f);
		nvgFillColor(args.vg, isActive ? nvgRGB(90, 220, 110) : nvgRGB(220, 70, 70));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}
};

// Status-LED mit drei UNABHAENGIGEN Farbkanaelen statt RGB-Mischung: Racks
// RGB-Licht legt seine Grundfarben mit Transparenz uebereinander, ein
// "Gruenton wie beim Activate-Button" oder ein sauberes Gelb liess sich so
// nicht einstellen (Kinovice-Test 2026-09-28). Jeder Zustand bekommt daher
// seine eigene Farbe, immer nur EIN Kanal leuchtet:
//   Kanal 0 = Stopp (Rot), Kanal 1 = laeuft (Gruen wie Activate-Button),
//   Kanal 2 = Halten (Gelb).
//
// Wichtig: Racks LightWidget zeichnet auf Layer 1 im Screen-Blendmodus
// (aufhellend). Auf dem hellen Panel bleicht jede Farbe dadurch zu Pastell
// aus, egal wie satt sie gewaehlt ist (Kinovice-Test: "Activate-Button
// wirkt deutlich gruener"). drawLayer() zeichnet daher mit normaler
// Ueberblendung; Helligkeit = Deckkraft der Farbe.
struct EuropaStatusLight : app::ModuleLightWidget {
	EuropaStatusLight() {
		box.size = mm2px(Vec(3.176f, 3.176f));
		addBaseColor(SCHEME_RED);
		addBaseColor(nvgRGB(90, 220, 110)); // = Activate-Button (aktiv)
		addBaseColor(nvgRGB(255, 200, 20));
	}
	// Kein Lichthof: bei kleinen LEDs auf dunklem Grund wirkte der Halo
	// "viel zu ueberstrahlt" (Kinovice-Test). Gleiche Technik wie
	// PulseRingLight in Sofonisba.
	void drawHalo(const DrawArgs& args) override {}
	// Normale statt aufhellender Ueberblendung (siehe Kommentar oben).
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			drawLight(args);
	}
};

// Blaue Auswahl-LED (welcher Generator wird angezeigt), ebenfalls ohne Halo.
struct EuropaSelectLight : app::ModuleLightWidget {
	EuropaSelectLight() {
		box.size = mm2px(Vec(2.176f, 2.176f));
		addBaseColor(nvgRGB(40, 110, 255));
	}
	void drawHalo(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			drawLight(args);
	}
};

// Range-Schalter (TR1..TR4): Klick schaltet zum naechsten Bereich weiter,
// Shift+Klick rueckwaerts, Rechtsklick oeffnet das Standard-Kontextmenue
// (mit Auswahlliste der vier Bereiche). Bewusst KEIN Standard-Knob: Racks Knob
// skaliert die Ziehdistanz auf den Wertebereich, ein gerasteter Knob mit nur
// 4 Stellungen reagiert dadurch erst nach sehr langem Ziehen (in Europa
// getestet: "Range-Regler lassen sich nicht bewegen"). Faustregel aus
// Sofonisba: kleine diskrete Zustandszahl -> eigenes Widget. Anders als in
// Sofonisba braucht es hier keine eigene Checkliste (siehe unten).
struct EuropaRangeSwitch : ParamWidget {
	EuropaRangeSwitch() {
		box.size = mm2px(Vec(9.f, 6.f));
	}

	void setRange(int idx) {
		ParamQuantity* pq = getParamQuantity();
		if (!pq)
			return;
		float oldValue = pq->getValue();
		float newValue = (float) rack::clamp(idx, 0, 3);
		if (newValue == oldValue)
			return;
		pq->setValue(newValue);
		// Undo/Redo wie bei Racks eigenen Schaltern
		history::ParamChange* h = new history::ParamChange;
		h->name = "change time range";
		h->moduleId = pq->module->id;
		h->paramId = paramId;
		h->oldValue = oldValue;
		h->newValue = newValue;
		APP->history->push(h);
	}

	void onButton(const ButtonEvent& e) override {
		bool leftPress = (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT);
		int mods = e.mods & RACK_MOD_MASK;
		ParamQuantity* pq = getParamQuantity();
		if (pq && leftPress && (mods == 0 || mods == GLFW_MOD_SHIFT)) {
			int cur = (int) std::lround(pq->getValue());
			int next = (mods == 0) ? (cur + 1) % 4 : (cur + 3) % 4; // Shift = rueckwaerts
			setRange(next);
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}

	// Doppelklick wuerde sonst auf den Default (TR3) zuruecksetzen, obwohl
	// beide Klicks bereits weitergeschaltet haben.
	void onDoubleClick(const DoubleClickEvent& e) override {}

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 2.5f);
		nvgFillColor(args.vg, nvgRGB(40, 40, 44));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		ParamQuantity* pq = getParamQuantity();
		int idx = pq ? rack::clamp((int) std::lround(pq->getValue()), 0, 3) : 2;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font || font->handle < 0)
			return;
		static const char* labels[4] = {"TR1", "TR2", "TR3", "TR4"};
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 12.f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, nvgRGB(235, 170, 60));
		nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, labels[idx], NULL);
	}

	// Kein eigenes appendContextMenu(): der Range-Param ist ein configSwitch
	// mit Labels, Racks Standard-Kontextmenue listet die vier Bereiche schon
	// selbst (inkl. Haekchen und Undo). Eine eigene Checkliste erschien
	// doppelt (Kinovice-Test 2026-09-28).
};

// Quadratur-Taster je Paar mit drei Stellungen (Off / Follow / Handshake).
// Wie EuropaRangeSwitch ein eigenes Widget (Faustregel: kleine diskrete
// Zustandszahl): Klick schaltet weiter, Shift+Klick rueckwaerts, Rechtsklick
// oeffnet das Standard-Kontextmenue mit den drei benannten Stellungen. Die LED
// wird direkt aus dem Param gezeichnet: dunkel = Off, gruen = Follow,
// bernstein = Handshake.
struct EuropaQuadButton : ParamWidget {
	// Name des Undo-Eintrags (Standard: Quadratur; fuer den Phasor-OUT-Taster geaendert)
	std::string historyName = "change quadrature mode";

	EuropaQuadButton() {
		box.size = mm2px(Vec(6.f, 6.f));
	}

	void setMode(int idx) {
		ParamQuantity* pq = getParamQuantity();
		if (!pq)
			return;
		float oldValue = pq->getValue();
		float newValue = (float) rack::clamp(idx, 0, 2);
		if (newValue == oldValue)
			return;
		pq->setValue(newValue);
		history::ParamChange* h = new history::ParamChange;
		h->name = historyName;
		h->moduleId = pq->module->id;
		h->paramId = paramId;
		h->oldValue = oldValue;
		h->newValue = newValue;
		APP->history->push(h);
	}

	void onButton(const ButtonEvent& e) override {
		bool leftPress = (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT);
		int mods = e.mods & RACK_MOD_MASK;
		ParamQuantity* pq = getParamQuantity();
		if (pq && leftPress && (mods == 0 || mods == GLFW_MOD_SHIFT)) {
			int cur = (int) std::lround(pq->getValue());
			int next = (mods == 0) ? (cur + 1) % 3 : (cur + 2) % 3; // Shift = rueckwaerts
			setMode(next);
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}

	// Doppelklick wuerde sonst auf den Default (Off) zuruecksetzen, obwohl
	// beide Klicks bereits weitergeschaltet haben.
	void onDoubleClick(const DoubleClickEvent& e) override {}

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 2.5f);
		nvgFillColor(args.vg, nvgRGB(40, 40, 44));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		ParamQuantity* pq = getParamQuantity();
		int idx = pq ? rack::clamp((int) std::lround(pq->getValue()), 0, 2) : 0;
		NVGcolor led = (idx == 0) ? nvgRGB(62, 64, 70)
		             : (idx == 1) ? nvgRGB(60, 220, 90)
		                          : nvgRGB(255, 170, 40);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, box.size.x / 2.f, box.size.y / 2.f, std::min(box.size.x, box.size.y) * 0.27f);
		nvgFillColor(args.vg, led);
		nvgFill(args.vg);
	}
};

struct EuropaWidget : ModuleWidget {
	// Alle Param-Widgets je Generator - Sichtbarkeit in step() nach displayedGen.
	std::vector<Widget*> genWidgets[EUROPA_NUM_GENS];

	// Label-Helfer (zentrales PanelLabel, wie in Marf248Widget/Lucia).
	// Standard: mittig auf die Position ausgerichtet.
	void addLabel(Vec posMM, const std::string& text, NVGcolor color, float fontSize = 7.f,
	              int align = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE) {
		addChild(new PanelLabel(posMM, text, color, fontSize, align));
	}

	void addFrame(Vec topLeftMM, Vec sizeMM, NVGcolor fill = nvgRGBA(0, 0, 0, 0), bool stroke = true, float radius = 3.f) {
		EuropaFrame* f = new EuropaFrame;
		f->box.pos = mm2px(topLeftMM);
		f->box.size = mm2px(sizeMM);
		f->fillColor = fill;
		f->drawStroke = stroke;
		f->cornerRadius = radius;
		addChild(f);
	}

	EuropaWidget(Europa* module) {
		setModule(module);
		// 23 HP: das bisherige 20-HP-Layout (101.6 mm) bleibt unveraendert, rechts
		// kommt ein Streifen fuer die OR-Ausgaenge dazu (2026-10-01, Kinovice).
		const float HP = 23.f;
		box.size = Vec(HP * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

		EuropaPanelBackground* bg = new EuropaPanelBackground;
		bg->box.size = box.size;
		addChild(bg);

		// --- Farben ---
		// Palette: Beschriftungen kommen aus PanelLabel.hpp (COLOR_SYSTEM =
		// Generator-/Systemebene, COLOR_STAGEMOD = Stage-Parameter). Bei
		// Bedarf hier gegen die Expander-Palette abgleichen.
		const NVGcolor INK = nvgRGB(40, 40, 44);           // Text auf hellem Grund
		const NVGcolor PLATE_INK = nvgRGB(232, 232, 228);  // Text auf dunkler Flaeche
		const NVGcolor DARK = nvgRGB(40, 40, 44);

		// --- Layout in mm (Panel 116.84 x 128.5; Hauptblock 101.6 breit, rechts OR-Streifen) ---
		const float left = 2.3f, right = 99.3f;            // Rahmen-Aussenkanten
		const float stageX[ENV_NUM_STAGES] = {13.3f, 38.3f, 63.3f, 88.3f};
		const float stageFrameW = 22.f;
		const float stageTop = 31.f, stageH = 52.3f;
		const float nameY = 34.3f;
		const float levelY = 42.5f, levelLblY = 47.6f;
		const float timeY = 55.6f, timeLblY = 62.6f;
		const float rangeY = 68.8f;
		const float curveY = 76.3f, curveLblY = 81.4f;
		const float headerY = 18.5f, headerLblY = 27.4f;
		// Buchsen: Eingaenge links (T1/GATE, TRIG, TIME CV, LEVEL CV, HOLD CV,
		// VCA IN), Ausgaenge rechts auf dunkler Platte (VCA OUT, ENV, EOC).
		// 9 statt 8 Spalten (Hold-CV-Session, 2026-09-28): gleiche Herleitung
		// wie beim vorherigen Sprung 7->8 (LEVEL-CV-Session), Abstand erneut
		// verkleinert, Rand links/rechts bleibt in vergleichbarer Groesse.
		const float jackX[9] = {13.f, 23.f, 33.f, 43.f, 53.f, 63.f, 73.f, 83.f, 93.f};
		const float jackY[EUROPA_NUM_GENS] = {93.f, 102.3f, 111.6f, 120.9f};
		const float jackLblY = 87.2f;

		// --- Titel ---
		addLabel(Vec(50.8f, 5.4f), "EUROPA", INK, 13.f);

		// --- Rahmen: Header (GEN-Block | Generator-Schalter), Stages, Buchsen ---
		addFrame(Vec(left, 10.5f), Vec(38.7f, 19.f));
		addFrame(Vec(43.5f, 10.5f), Vec(right - 43.5f, 19.f));

		for (int s = 0; s < ENV_NUM_STAGES; s++) {
			addFrame(Vec(stageX[s] - stageFrameW / 2.f, stageTop), Vec(stageFrameW, stageH));
			// dunkles Namensband
			addFrame(Vec(stageX[s] - stageFrameW / 2.f + 0.7f, stageTop + 0.7f),
				Vec(stageFrameW - 1.4f, 6.2f), DARK, false, 2.f);
		}

		addFrame(Vec(left, 84.6f), Vec(right - left, 41.7f));
		addFrame(Vec(68.f, 85.4f), Vec(97.8f - 68.f, 40.1f), DARK, false, 2.5f);

		// OR-Streifen rechts neben dem Hauptblock: eine Buchsenspalte wie die
		// anderen (10 mm Raster, dunkle Platte 10 mm breit). Der helle Rahmen
		// darum hat nur den schmalen Rand der Ausgangsplatte des Hauptblocks
		// (1.5 mm seitlich, 0.8 mm oben/unten). Rechts bleibt 2.3 mm Abstand zum
		// Panelrand (wie links am Hauptblock); der Rest liegt als Abstand zum
		// Hauptblock.
		const float orPlateW = 10.f;
		const float orFramePad = 1.5f;
		const float orStripW = orPlateW + 2.f * orFramePad;
		const float orStripX = HP * 5.08f - 2.3f - orStripW;
		const float orX = orStripX + orStripW / 2.f;
		// Zwei Bloecke je Paar (Beschriftung, OR-Buchse, Quadratur-Taster mit LED):
		// Block 1 ab y = 85.4, Block 2 ab y = 105.4 (je 20 mm hoch).
		const float orBlockTop[2] = {85.4f, 105.4f};
		static const char* orNames[2] = {"OR 1+2", "OR 3+4"};
		addFrame(Vec(orStripX, 84.6f), Vec(orStripW, 41.7f));
		addFrame(Vec(orX - orPlateW / 2.f, 85.4f), Vec(orPlateW, 40.1f), DARK, false, 2.5f);
		for (int k = 0; k < 2; k++) {
			float t = orBlockTop[k];
			addLabel(Vec(orX, t + 3.0f), orNames[k], PLATE_INK, 5.5f);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(orX, t + 9.5f)), module, Europa::OR_OUTPUT + k));
			addParam(createParamCentered<EuropaQuadButton>(mm2px(Vec(orX, t + 17.f)), module, Europa::QUAD_PARAM + k));
		}

		// --- PHASOR-Streifen: gleiche Spalte wie der OR-Streifen, darueber (y 10.5 ..
		// 82.6). Die Bedienelemente gehoeren zum angezeigten Generator (siehe unten).
		addFrame(Vec(orStripX, 10.5f), Vec(orStripW, 72.1f));
		addLabel(Vec(orX, 13.6f), "PHASOR", COLOR_SYSTEM, 6.f);
		addLabel(Vec(orX, 28.6f), "OUT", COLOR_SYSTEM, 6.f);
		addLabel(Vec(orX, 42.4f), "NUM", COLOR_STAGEMOD, 6.f);
		addLabel(Vec(orX, 55.4f), "DEN", COLOR_STAGEMOD, 6.f);
		addLabel(Vec(orX, 68.4f), "OFFSET", COLOR_STAGEMOD, 5.5f);

		for (int g = 0; g < EUROPA_NUM_GENS; g++) {
			EuropaRowHighlight* hl = new EuropaRowHighlight;
			hl->module = module;
			hl->gen = g;
			hl->box.pos = mm2px(Vec(3.3f, jackY[g] - 4.6f));
			hl->box.size = mm2px(Vec(right - 3.3f - 1.f, 9.2f));
			addChild(hl);
		}

		// --- GEN-Auswahlblock ---
		GenSelectKnob* selKnob = new GenSelectKnob;
		selKnob->module = module;
		selKnob->box.pos = mm2px(Vec(14.f - 7.f, headerY - 7.f));
		addChild(selKnob);
		addLabel(Vec(14.f, headerLblY), "GEN", COLOR_SYSTEM, 7.f);

		GenActivateButton* actBtn = new GenActivateButton;
		actBtn->module = module;
		actBtn->box.pos = mm2px(Vec(31.f - 4.f, headerY - 4.f));
		addChild(actBtn);
		addLabel(Vec(31.f, headerLblY), "ACTIVATE", COLOR_SYSTEM, 7.f);

		// --- Generator-Schalter (Beschriftung) ---
		// LOOP-Schalter ist dreistufig (Auto-Cycle) und braucht rechts mehr Platz
		// fuer die Stellungsbeschriftung: swX[2] gegenueber vorher 89.5 nach links.
		const float swX[3] = {51.5f, 72.5f, 87.f};
		addLabel(Vec(swX[0], headerLblY), "MODE", COLOR_SYSTEM, 7.f);
		addLabel(Vec(swX[1], headerLblY), "RETRIG D", COLOR_SYSTEM, 7.f);
		addLabel(Vec(swX[2], headerLblY), "LOOP", COLOR_SYSTEM, 7.f);
		// Stellungsbeschriftung neben den Schaltern: oben = Sustain/On,
		// unten = One-Shot/Off (CKSS: Kinovice, 2026-09-28)
		const int sideAlign = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE;
		const float posDy = 4.f;
		static const char* topTxt[3] = {"SUSTAIN", "ON", "ON"};
		static const char* botTxt[3] = {"ONE-SHOT", "OFF", "OFF"};
		static const int swParamBase[3] = {Europa::MODE_PARAM, Europa::RETRIG0_PARAM, Europa::LOOP_PARAM};
		static const float onPillW[3] = {9.f, 4.f, 4.f};
		// MODE und RETRIG D: zweistufig (k = 0, 1); LOOP folgt unten separat.
		for (int k = 0; k < 2; k++) {
			// gruenes Feld hinter dem oberen Label (vor dem Label eingefuegt,
			// damit der Text daruber liegt)
			EuropaOnIndicator* on = new EuropaOnIndicator;
			on->module = module;
			on->paramBase = swParamBase[k];
			on->box.size = mm2px(Vec(onPillW[k], 3.2f));
			on->box.pos = mm2px(Vec(swX[k] + 3.8f, headerY - posDy - 1.6f));
			addChild(on);
			addLabel(Vec(swX[k] + 4.5f, headerY - posDy), topTxt[k], INK, 5.5f, sideAlign);
			addLabel(Vec(swX[k] + 4.5f, headerY + posDy), botTxt[k], INK, 5.5f, sideAlign);
		}

		// LOOP: dreistufig, oben FREE (freier Zyklus ohne Gate), Mitte GATE
		// (Loop bei gehaltenem Gate, bisheriges "ON"), unten OFF. Das gruene Feld
		// markiert die aktive Stellung (OFF bleibt unmarkiert, wie bei den
		// anderen Schaltern). LOOP_DY ist der Abstand der drei Beschriftungen
		// zur Schalter-Mitte - bei Bedarf an die Hoehe des CKSSThree anpassen.
		const float LOOP_DY = 4.5f;
		static const char* loopTxt[3] = {"FREE", "GATE", "OFF"};
		const float loopOffsetY[3] = {-LOOP_DY, 0.f, LOOP_DY};
		const float loopPillW = 6.f;
		for (int k = 0; k < 3; k++) {
			if (k < 2) {
				EuropaOnIndicator* on = new EuropaOnIndicator;
				on->module = module;
				on->paramBase = Europa::LOOP_PARAM;
				on->lo = (k == 0) ? 1.5f : 0.5f;   // FREE: Wert 2, GATE: Wert 1
				on->hi = (k == 0) ? 1e9f : 1.5f;
				on->box.size = mm2px(Vec(loopPillW, 3.2f));
				on->box.pos = mm2px(Vec(swX[2] + 3.8f, headerY + loopOffsetY[k] - 1.6f));
				addChild(on);
			}
			addLabel(Vec(swX[2] + 4.5f, headerY + loopOffsetY[k]), loopTxt[k], INK, 5.5f, sideAlign);
		}

		// --- Stage-Beschriftung ---
		static const char* stageNames[ENV_NUM_STAGES] = {"DELAY", "ATTACK", "HOLD", "RELEASE"};
		for (int s = 0; s < ENV_NUM_STAGES; s++) {
			addLabel(Vec(stageX[s], nameY), stageNames[s], PLATE_INK, 8.f);
			addLabel(Vec(stageX[s], levelLblY), "LEVEL", COLOR_STAGEMOD, 7.f);
			addLabel(Vec(stageX[s], timeLblY), "TIME", COLOR_STAGEMOD, 7.f);
			addLabel(Vec(stageX[s], curveLblY), "CURVE", COLOR_STAGEMOD, 7.f);
		}

		// --- Pro Generator umschaltbare Bedienelemente ---
		for (int g = 0; g < EUROPA_NUM_GENS; g++) {
			auto addGenParam = [&](ParamWidget* w) {
				addParam(w);
				genWidgets[g].push_back(w);
			};

			addGenParam(createParamCentered<CKSS>(mm2px(Vec(swX[0], headerY)), module, Europa::MODE_PARAM + g));
			addGenParam(createParamCentered<CKSS>(mm2px(Vec(swX[1], headerY)), module, Europa::RETRIG0_PARAM + g));
			addGenParam(createParamCentered<CKSSThree>(mm2px(Vec(swX[2], headerY)), module, Europa::LOOP_PARAM + g));

			// Phasor-Streifen (x = Mitte der OR-Spalte)
			addGenParam(createParamCentered<CKSS>(mm2px(Vec(orX, 21.f)), module, Europa::PHASOR_PARAM + g));
			EuropaQuadButton* outBtn = createParamCentered<EuropaQuadButton>(mm2px(Vec(orX, 35.f)),
				module, Europa::PHASOR_OUT_PARAM + g);
			outBtn->historyName = "change phasor output mode";
			addGenParam(outBtn);
			addGenParam(createParamCentered<Trimpot>(mm2px(Vec(orX, 48.f)), module, Europa::RATIO_P_PARAM + g));
			addGenParam(createParamCentered<Trimpot>(mm2px(Vec(orX, 61.f)), module, Europa::RATIO_Q_PARAM + g));
			addGenParam(createParamCentered<Trimpot>(mm2px(Vec(orX, 74.f)), module, Europa::PHASE_OFFSET_PARAM + g));
			ModuleLightWidget* pendLed = createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(orX + 4.2f, 52.f)),
				module, Europa::PENDING_LIGHT + g);
			addChild(pendLed);
			genWidgets[g].push_back(pendLed);

			addGenParam(createParamCentered<Trimpot>(mm2px(Vec(stageX[ENV_STAGE_DELAY], levelY)),
				module, Europa::LEVEL_DELAY_PARAM + g));
			addGenParam(createParamCentered<Trimpot>(mm2px(Vec(stageX[ENV_STAGE_RELEASE], levelY)),
				module, Europa::LEVEL_RELEASE_PARAM + g));
			addGenParam(createParamCentered<Trimpot>(mm2px(Vec(stageX[ENV_STAGE_ATTACK], levelY)),
				module, Europa::LEVEL_ATTACK_PARAM + g));
			addGenParam(createParamCentered<Trimpot>(mm2px(Vec(stageX[ENV_STAGE_HOLD], levelY)),
				module, Europa::LEVEL_HOLD_PARAM + g));

			for (int s = 0; s < ENV_NUM_STAGES; s++) {
				int i = g * ENV_NUM_STAGES + s;
				addGenParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(stageX[s], timeY)),
					module, Europa::TIME_PARAM + i));
				addGenParam(createParamCentered<EuropaRangeSwitch>(mm2px(Vec(stageX[s], rangeY)),
					module, Europa::RANGE_PARAM + i));
				addGenParam(createParamCentered<Trimpot>(mm2px(Vec(stageX[s], curveY)),
					module, Europa::CURVE_PARAM + i));
			}
		}

		// --- Buchsen-Beschriftung ---
		static const char* jackNames[9] = {"T1/GATE", "TRIG", "TIME CV", "LEVEL CV", "HOLD CV", "VCA IN", "VCA OUT", "ENV", "EOC"};
		for (int j = 0; j < 9; j++)
			addLabel(Vec(jackX[j], jackLblY), jackNames[j], (j < 6) ? INK : PLATE_INK, 5.5f);

		// --- Buchsen (immer sichtbar) + LEDs + Generator-Nummer ---
		for (int g = 0; g < EUROPA_NUM_GENS; g++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackX[0], jackY[g])), module, Europa::GATE_INPUT + g));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackX[1], jackY[g])), module, Europa::TRIG_INPUT + g));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackX[2], jackY[g])), module, Europa::TIMECV_INPUT + g));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackX[3], jackY[g])), module, Europa::LEVELCV_INPUT + g));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackX[4], jackY[g])), module, Europa::HOLDCV_INPUT + g));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackX[5], jackY[g])), module, Europa::SIGNAL_INPUT + g));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jackX[6], jackY[g])), module, Europa::VCA_OUTPUT + g));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jackX[7], jackY[g])), module, Europa::ENV_OUTPUT + g));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jackX[8], jackY[g])), module, Europa::EOC_OUTPUT + g));

			addChild(createLightCentered<EuropaStatusLight>(mm2px(Vec(5.0f, jackY[g] - 2.4f)),
				module, Europa::RUN_LIGHT + g * 3));
			addChild(createLightCentered<EuropaSelectLight>(mm2px(Vec(5.0f, jackY[g] + 2.2f)),
				module, Europa::SELECT_LIGHT + g));
			addLabel(Vec(7.6f, jackY[g]), std::to_string(g + 1), INK, 8.f);
		}

		// --- Schrauben in den vier Ecken (zuletzt eingefuegt, liegen oben) ---
		// Horizontal weit aussen (Rand 1 mm statt 5.08 mm), damit sie in die
		// Rundung der Umrahmung / an den Panelrand ruecken.
		const float screwMargin = mm2px(1.f);
		const float screwL = screwMargin;
		const float screwR = box.size.x - screwMargin - RACK_GRID_WIDTH;
		const float screwB = RACK_GRID_HEIGHT - RACK_GRID_WIDTH;
		addChild(createWidget<ScrewSilver>(Vec(screwL, 0)));
		addChild(createWidget<ScrewSilver>(Vec(screwR, 0)));
		addChild(createWidget<ScrewSilver>(Vec(screwL, screwB)));
		addChild(createWidget<ScrewSilver>(Vec(screwR, screwB)));
	}

	void appendContextMenu(Menu* menu) override {
		Europa* module = dynamic_cast<Europa*>(this->module);
		if (!module)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Antialiasing (minBLEP, ENV/VCA/OR)", "", &module->antialias));
		menu->addChild(createBoolPtrMenuItem("Phasor: Ratio-Wechsel locker (jeder Eingangs-Wrap)", "", &module->ratioLoose));
	}

	// Sichtbarkeit: genau der Parametersatz des angezeigten Generators.
	void step() override {
		Europa* module = dynamic_cast<Europa*>(this->module);
		// Ohne Modul (Modul-Browser) nur Generator 1 zeigen, sonst wuerden
		// sich alle vier Parametersaetze ueberlagern.
		int shown = module ? module->displayedGen : 0;
		for (int g = 0; g < EUROPA_NUM_GENS; g++) {
			bool vis = (g == shown);
			for (Widget* w : genWidgets[g])
				w->visible = vis;
		}
		ModuleWidget::step();
	}
};

Model* modelEuropa = createModel<Europa, EuropaWidget>("Europa");
