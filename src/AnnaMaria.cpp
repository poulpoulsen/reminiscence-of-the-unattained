#include "plugin.hpp"
#include "PanelLabel.hpp"
#include "AnnaMariaLpg.hpp"
#include <history.hpp>

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------
// "Anna-Maria": Lowpass-Gate-Modell im West-Coast-Stil nach Parker/D'Angelo,
// "A Digital Model of the Buchla Lowpass-Gate", DAFx-13.
//
// Die DSP-Engine (AnnaMariaLpg.hpp) ist rack-frei und gegen die Octave-
// Referenz buchla_lp_gate.m geprueft (Abweichung <= 1e-13). Dieses Modul
// ist die Rack-Huelle.
//
// QUAD-LPG (paramLayout 6): vier getrennte, monophone Kanaele (NUM_LPG).
// Panel (31 HP) mit fuenf Spalten, Beschriftungen ueber den Elementen:
//   Spalte 1 (ALL):  ALL-Ausgang, MODE (global), MODE-CV (global), DRIVE, VACTROL, WET (global), WET-CV (polyphon, 4 Kanaele)
//   Spalte 2-5:      je Kanal zwei Unterspalten in sechs Reihen:
//                    IN | OUT, ATT | VEL, CV | CV, THRESH | FEEDBACK, CV | CV, FB NL | V/OCT, MODE | DEEP
// V/OCT (je Kanal, neuer Eingang hinter den anderen, Params unveraendert, paramLayout bleibt 6): stimmt einen Resonator
// vor der LPG-Stufe (AnnaMariaLpg::Params::resHz) auf die Note; der Klang des LPG bleibt unveraendert, der gestimmte Ton
// kommt dazu (kein Glide). WET (Knopf, global; ein gestecktes polyphones WET-CV ersetzt ihn, Kanal 1-4 = LPG 1-4,
// monophon = alle) bestimmt seinen Anteil, FEEDBACK (Knopf oder CV) die Klingdauer (0,05 ... 3 s), solange V/OCT steckt.
// Nur in LOWPASS und in BOTH mit linearem Gate und Lowpass-Stufe. Der globale DEEP-Schalter (Platzhalter) ist vom
// Panel entfallen (Platz fuer WET/WET-CV); der Param DEEP_ALL_PARAM bleibt, die DEEP-Schalter der Kanaele auch.
// Hardware-Vorbild (Layout 5): ATT = Steuerpegel 0...10 V, eine gesteckte CV ersetzt den Knopf;
// Velocity (-10...+10 V) wird als Gleichanteil addiert (Knopf oder gesteckte Velocity-CV),
// Summe auf 0...10 V begrenzt.
// Layout 6: THRESHOLD und FEEDBACK je Kanal auf dem Panel, mit je einem CV-Eingang, der den Knopf
// ersetzt, wenn er gesteckt ist. THRESHOLD-CV: 1 V/V (0...7,5 V, wie der Knopf); FEEDBACK-CV:
// 0...10 V = 0...100 %. FB NL (je Kanal) liegt als Schalter auf dem Panel, DRIVE und VACTROL
// (global) als Knoepfe in der ALL-Spalte. Im Kontextmenue bleiben DEPTH (je Kanal) und die
// Modell-Einstellungen.
//
// Entwurfsentscheidungen:
//  * Param-Layout: Die 16er-Bloecke der alten polyphonen Version bleiben bestehen (nur
//    Kanal 1-4 werden benutzt), damit alte Patches ihre Param-Indizes behalten. Ein alter
//    Patch landet damit auf Kanal 1-4; seine Kabel an AUDIO/CV auf Kanal 1 (nur Kanal 1
//    der Polyphonie wird gelesen, Poly-Eingaenge werden nicht mehr aufgefaechert).
//  * Eingaenge: IN 1 ist auf IN 2-4 normalisiert (nur wenn dort nichts steckt).
//  * Globaler MODE- und DEEP-Schalter: Eine Aenderung wird auf alle vier Kanalschalter
//    geschrieben ("global ueberschreibt"); danach sind die Kanalschalter wieder einzeln
//    bedienbar. Beim Laden eines Patches wird nichts ueberschrieben.
//  * MODE-CV (global): Ist die Buchse belegt, bestimmt sie den Modus ALLER Kanaele und geht
//    vor den Schaltern: < 3,3 V = VCA, < 6,6 V = LOWPASS, darueber BOTH.
//  * DEEP: nur Platzhalter (Param vorhanden, keine Funktion).
//  * Gate-Anteil in VCA und BOTH (Standard: linear): Die Referenzdokumentation des Hardware-Moduls
//    beschreibt "Gate" als linearen VCA und "Combo" als Kombination aus linearem Gate und Lowpass.
//    Der lineare Anteil ist cv/10 mit cv = clamp(ATT oder CV + Velocity, 0, 10 V). VCA: nur dieser
//    Anteil (THRESHOLD, DEPTH, FEEDBACK und NL wirken nicht). BOTH: VCA und LPG in Reihe, erst der lineare
//    Anteil, dann die Filterstufe. Standard ist die Lowpass-Stufe wie in LOWPASS (THRESHOLD, DEPTH, FEEDBACK,
//    NL wirken dort; das Filter schwingt nach dem Schliessen nach); im Menue "BOTH filter stage" laesst sich
//    stattdessen die Paper-Stufe von BOTH waehlen (C3 = 0, ohne Resonanz), siehe AnnaMariaLpg::Params.
//    VACTROL stellt auch die Traegheit des linearen Anteils ein (Annahme, nicht belegt). Im Kontextmenue
//    "Gate response" laesst sich auf das Paper-Modell (Vactrol-Teiler mit Knick) zurueckschalten.
//  * ALL-Ausgang: Summe der vier Kanalausgaenge, danach Softclip auf +-10 V (linear bis
//    +-7 V, darueber weich gegen +-10 V). Kein Teilen durch die Kanalzahl.
//  * Pegel: Der DRIVE-Regler (0..10, stufenlos, ein Regler fuer das ganze Modul)
//    setzt, welche Spannung im Modell "1" entspricht (driveToModelLevel()). Mitte 5 =
//    +-5 V -> +-1. Er beeinflusst nur, wie stark das tanh der Rueckkopplung saettigt.
//  * Kontextmenue (nur JSON bzw. Params): Regler, Resonanz normiert/absolut, Vactrol-
//    Verhalten, NL-Solver, Schaltungsvariante, Saettigung, Oversampling 2x.
//  * Beim Start, bei Samplerate- oder Oversampling-Wechsel starten die Kanaele per
//    warmStart(), also ohne kuenstlichen Vactrol-Einschwingvorgang.
//
// Alles steht im anonymen Namespace (interne Bindung, siehe Minerva):
// lokale Structs koennen so nie mit gleichnamigen Structs aus anderen
// .cpp-Dateien der .so kollidieren. Nur das Model ist exportiert.
//
// C++11 (das Plugin wird mit C++11 gebaut).
// ---------------------------------------------------------------------

namespace {

static constexpr int NUM_CH = 16;   // Groesse der Param-Bloecke (Kompatibilitaet mit alten Patches)
static constexpr int NUM_LPG = 4;   // tatsaechlich benutzte Kanaele
enum ParamType { PT_THRESHOLD, PT_DEPTH, PT_FEEDBACK, PT_CVATT, PT_MODE, PT_NL, NUM_PT,
                 // Spaeter ergaenzt (liegt hinter DRIVE, siehe ParamId): CV BIAS (= VELOCITY) je Kanal
                 PT_CVBIAS = NUM_PT, NUM_PT_ALL };

// Index eines Params: je Reglerart ein zusammenhaengender 16er-Block.
static inline int paramIndex(int type, int ch) {
	return type * NUM_CH + ch;
}

// DRIVE-Regler (0..10, stufenlos, ein Regler fuer das ganze Modul, kein Kanal-Param).
// Umrechnung Rack-Volt <-> Modellspannung (Modell arbeitet mit Signalen um +-1):
//   x = Eingang / L,  Ausgang = y * L,  L = Spannung, die im Modell "1" entspricht.
// L haengt exponentiell vom Regler ab: 5 = Mitte = 5 V (+-5 V -> +-1, bisheriger
// Festwert), 0 = 20 V (+-5 V -> +-0,25, nahezu linear), 10 = 1,25 V (+-5 V -> +-4,
// starke Saettigung). Pro Reglerschritt (1 Einheit) aendert sich L um den Faktor 2^0,4.
// Der Pegelfaktor betrifft nur das tanh der Rueckkopplung (NL an); das lineare
// Filter ist davon unabhaengig, die Lautstaerke bei offenem Gate bleibt gleich.
static const float DRIVE_DEFAULT = 5.f;

// CV ATT: -400 ... +400 % (Verstaerkung), CV BIAS (Velocity): -10 ... +10 V (Gleichanteil vor der
// +-10-V-Begrenzung). So wird aus einer 0...10-V-Huellkurve mit ATT 200 % und BIAS -10 V
// ein Hub von -10 ... +10 V, bei dem das Gate in der Ruhe tief schliesst.
// Ab paramLayout 5 (Hardware-Vorbild): ATT ist ein Regler 0...10 V (Steuerpegel), keine +-400-%-Verstaerkung mehr;
// eine gesteckte CV-Buchse ERSETZT den Regler. Velocity (-10...+10 V) wird als Offset addiert (fuer Regler wie
// CV), Summe auf 0...10 V begrenzt. Die Velocity-CV ersetzt ebenso den Velocity-Regler (Annahme).
static const float ATT_MAX = 10.f;
static const float CVBIAS_MAX = 10.f;

// MODE-CV: Schwellen fuer die drei Modi (0 = Lowpass, 1 = Both, 2 = VCA)
static const float MODECV_LOW = 3.3f;
static const float MODECV_HIGH = 6.6f;
static inline int modeFromCv(float v) {
	return v < MODECV_LOW ? 2 : (v < MODECV_HIGH ? 0 : 1);
}

// Softclip fuer den ALL-Ausgang: linear bis +-7 V, darueber weich gegen +-10 V (stetig, Steigung 1 am Knick).
static inline float softClip10(float x) {
	const float knee = 7.f, range = 3.f;
	float a = std::fabs(x);
	if (a <= knee)
		return x;
	float y = knee + range * std::tanh((a - knee) / range);
	return x < 0.f ? -y : y;
}

// Basis aller Regler-Anzeigen. Rack wertet getippte Werte (Doppelklick auf den Regler) mit tinyexpr
// aus, und dort ist das Komma ein Listen-Trenner: "7,5" ergibt 5 (nur der Teil nach dem Komma),
// "0,5" ergibt 5 und damit Vollausschlag. Deshalb wird das Komma vor der Auswertung durch einen
// Punkt ersetzt, damit Dezimalkomma-Eingaben wie 7,5 oder 0,25 funktionieren. Tausendertrennzeichen
// gibt es in Rack nicht, ein Komma ist hier immer ein Dezimalzeichen.
struct AmQuantity : ParamQuantity {
	void setDisplayValueString(std::string s) override {
		std::replace(s.begin(), s.end(), ',', '.');
		ParamQuantity::setDisplayValueString(s);
	}
};

// VACTROL: Zeitfaktor auf die Zeitkonstanten des Vactrol-Modells (Paper: Faktor 1 in der Mitte).
// 0 = x0,1 (schnell, folgt dem Gate), 0,5 = x1, 1 = x10 (langsam, huellkurvenartig).
static inline double vactrolKnobToScale(float v) {
	return std::pow(10.0, 2.0 * ((double) v - 0.5));
}
struct VactrolQuantity : AmQuantity {
	std::string getDisplayValueString() override {
		double sc = vactrolKnobToScale(getValue());
		return string::f("x%.2f (rise %.1f ms, fall %.0f ms time constant; x1 = paper values; Conductance mode: x1 = tuned to a ~300 ms ping tail)",
			sc, 2.4 * sc, 50.0 * sc);
	}
};
// WET: Knopf 0..1, Verstaerkung des gestimmten Resonators 0,045 k^2 (100 % = -27 dB, 90 % = -29 dB = Standard, 50 % = -39 dB).
// Der Anwender hat mit 0,0225 (90 %: 2,5 V) einen guten Klang gefunden und wollte den Pegel auf 5 V anheben (+6 dB).
// Die Impulsantwort des Resonators hat Amplitude 1 (Modelleinheit, = DRIVE-Pegel). Ein Puls summiert sich im Resonator zur Flaeche
// auf (10 V, einige ms: ueber 100 Modelleinheiten roh, begrenzt auf 8). Rack-Messung des Anwenders: 30 V bei -6 dB; guter Klang bei
// 10 V und -45 dB. Der Bereich ist deshalb auf diese Region zugeschnitten. Eine Zeile, falls der Pegel nachgestellt wird.
static const double RES_DRY_HP_RATIO = 0.5;
// Untere Grenze der Hochpass-Eckfrequenz in Hz (gilt unabhaengig von der Note). Der Direktpfad eines Nadelimpulses ist
// stark tieflastig (Studie: ca. 18 dB je Oktave abfallend ab etwa 40 Hz); 150 Hz nimmt den Bass-Plopp um etwa 25 dB.
// Kleiner = mehr Bass bleibt im Direktklang, groesser = duenner.
static const double RES_DRY_HP_MIN_HZ = 150.0;
// Tiefpass 1. Ordnung auf dem Direktpfad (Hz): rundet die senkrechte Flanke eines Nadelimpulses ab (hartes Knacken).
// 0 = aus. Richtwerte: 8000 = leicht, 4000 = deutlich, 2000 = stark. Wirkt nur mit gestecktem V/OCT.
static const double RES_DRY_LP_HZ = 4000.0;
static inline float dryToGain(float k) {
	return k * k;
}
struct DryQuantity : AmQuantity {
	std::string getDisplayValueString() override {
		const float g = dryToGain(getValue());
		if (g < 0.0003f)
			return "off";
		return string::f("%.0f %% (gain %.2f = %+.1f dB)", getValue() * 100.f, g, 20.f * std::log10(g));
	}
};
static inline float wetToGain(float k) {
	return 0.045f * k * k;
}
struct WetQuantity : AmQuantity {
	std::string getDisplayValueString() override {
		const float g = wetToGain(getValue());
		if (g < 0.0002f)
			return "off";
		return string::f("%.0f %% (gain %.2f = %+.1f dB)", getValue() * 100.f, g, 20.f * std::log10(g));
	}
};
static inline float driveToModelLevel(float drive) {
	return 5.f * std::exp2(2.f * (5.f - drive) / 5.f);
}

// Anzeige des DRIVE-Reglers: Reglerwert und die daraus folgende Vollaussteuerung.
struct DriveQuantity : AmQuantity {
	std::string getDisplayValueString() override {
		float v = getValue();
		return string::f("%.1f (+-%.1f V = model full scale)", v, driveToModelLevel(v));
	}
};

// Obere Grenze der absoluten Rueckkopplung, bei der das lineare Filter fuer jedes Rf
// stabil bleibt: 1 + (2*C1 + C2)/C3 (siehe buchla_lp_gate.m, ca. 1,47)
static const double ABS_FB_MAX =
	1.0 + (2.0 * AnnaMariaLpg::C1 + AnnaMariaLpg::C2) / AnnaMariaLpg::C3lp;

// ---------------------------------------------------------------------
// THRESHOLD (V) und DEPTH (0..1) statt OFFSET und TRIM
// ---------------------------------------------------------------------
// Das Modell kennt Offset (0..1, addiert 10 V * Offset zur CV) und Trim (0..1, Vorwiderstand
// der LED). Die Gate-Schwelle (CV, bei der das Gate halb offen ist) ist
//     thr = T(trim) + THR_HALF - 10 V * offset,     T = 390000 / (20000 * trim + 33000)
// (gemessen, siehe tests/doku_offset_trim_att.cpp; THR_HALF = 0,05 V ist der Abstand des
// 50-%-Punktes von der Formel). Der Regler zeigt diese Schwelle in Volt; das Modul rechnet
// daraus den Offset aus. DEPTH ist der Trim-Wert und steuert, wie tief das Gate schliesst.
// Regelweg 0 ... 7,5 V, Standard 7,5 V (Gate im Ruhezustand zu). Bei DEPTH 100 % liegt der Knick
// bei 7,408 V; 7,5 V ist also der Anschlag, bei dem der Offset 0 ist (kleinere DEPTH-Werte
// haben eine hoehere Schwelle, die der Regler nicht mehr erreicht). Vor paramLayout 3: -1 ... 9 V,
// Standard 4,1 V.
static const float THR_MIN = 0.f;
static const float THR_MAX = 7.5f;
static const float THR_DEFAULT = 7.5f;
// DEPTH-Standard 100 % (vorher 50 %): nur bei DEPTH 100 % ist der Offset bei THRESHOLD 7,5 V gleich 0,
// das Gate also ohne CV wirklich zu. Bei kleinerem DEPTH liegt die Schwelle hoeher (T(depth)).
static const double THR_HALF = 0.05;

static inline double trimT(double trim) {
	return 390000.0 / (20000.0 * trim + 33000.0);
}
// Offset (0..1) zu gewuenschter Schwelle; clamped = true, wenn der Offset begrenzt wurde.
static inline double thresholdToOffset(double thr, double depth, bool* clamped = nullptr) {
	double o = (trimT(depth) + THR_HALF - thr) / 10.0;
	double oc = o < 0.0 ? 0.0 : (o > 1.0 ? 1.0 : o);
	if (clamped) *clamped = (oc != o);
	return oc;
}
// Erreichbarer Schwellenbereich bei gegebenem DEPTH
static inline void thresholdRange(double depth, double* lo, double* hi) {
	*hi = trimT(depth) + THR_HALF;          // Offset 0
	*lo = trimT(depth) + THR_HALF - 10.0;   // Offset 1
}
// Alte Patches (OFFSET/TRIM) -> neue Schwelle
static inline float offsetTrimToThreshold(double offset, double trim) {
	double t = trimT(trim) + THR_HALF - 10.0 * offset;
	return (float) (t < THR_MIN ? THR_MIN : (t > THR_MAX ? THR_MAX : t));
}

// lineare Interpolation in einer Tabelle mit gleichmaessigen Stuetzstellen
static inline float lerpTable(const float* tab, int n, float x01) {
	float pos = rack::clamp(x01, 0.f, 1.f) * (n - 1);
	int i = (int) pos;
	if (i >= n - 1) return tab[n - 1];
	float f = pos - i;
	return tab[i] * (1.f - f) + tab[i + 1] * f;
}

// Steht der Gate-Anteil in VCA/BOTH auf "linear"? Fuer die Tooltips; AnnaMaria ist an dieser Stelle noch
// nicht definiert, die Definition steht hinter struct AnnaMaria.
static bool amLinearGate(Module* m);
// Wirkt die Rueckkopplung in BOTH? Nur mit linearem Gate-Anteil und Lowpass-Stufe.
static bool amBothResonance(Module* m);
// Steckt am V/OCT-Eingang dieses Kanals ein Kabel? (FEEDBACK bestimmt dann die Klingdauer des Resonators.)
static bool amPitchPatched(Module* m, int ch);

struct ThresholdQuantity : AmQuantity {
	std::string getDisplayValueString() override {
		int ch = paramId % NUM_CH;
		float depth = module->params[paramIndex(PT_DEPTH, ch)].getValue();
		float thr = getValue();
		bool clamped = false;
		double off = thresholdToOffset(thr, depth, &clamped);
		// Schwelle in Volt; zusaetzlich, was ohne CV-Kabel (0 V) passiert
		std::string s = string::f("%.2f V\n"
				"Gate half open at this control level\n"
				"(ATT or CV, plus velocity), fully open\n"
				"about 0.1 V above.\n"
				"At ATT 0 V (plus velocity): gate %s", thr,
			(thr < -0.1f) ? "open" : (thr > 0.1f ? "closed" : "half open"));
		if (clamped) {
			double lo, hi;
			thresholdRange(depth, &lo, &hi);
			double eff = trimT(depth) + THR_HALF - 10.0 * off;
			s = string::f("%.2f V set, %.2f V effective\n"
				"(DEPTH limits the range to %.1f..%.1f V)\n"
				"Gate half open at the effective level\n"
				"(ATT or CV, plus velocity).\n"
				"At ATT 0 V (plus velocity): gate %s",
				thr, eff, lo, hi, (eff < -0.1) ? "open" : (eff > 0.1 ? "closed" : "half open"));
		}
		// Linearer VCA: THRESHOLD wirkt im Modus VCA nicht
		int mode = (int) std::round(module->params[paramIndex(PT_MODE, ch)].getValue());
		if (mode == 2 && amLinearGate(module))
			s += "\nNo effect in VCA mode (linear gate)";
		return s;
	}
};

struct DepthQuantity : AmQuantity {
	std::string getDisplayValueString() override {
		// VCA-Daempfung bei geschlossenem Gate, 3 V und 6 V unter der Schwelle (Messwerte)
		static const float db3[5] = {-31.f, -32.f, -33.f, -34.f, -35.f};
		static const float db6[5] = {-36.f, -38.f, -41.f, -44.f, -49.f};
		float v = getValue();
		std::string s = string::f("%.0f %% (closed gate, 3 V below threshold: %.0f dB, 6 V below: %.0f dB)",
			v * 100.f, lerpTable(db3, 5, v), lerpTable(db6, 5, v));
		int ch = paramId % NUM_CH;
		int mode = (int) std::round(module->params[paramIndex(PT_MODE, ch)].getValue());
		if (mode == 2 && amLinearGate(module))
			s += " - no effect in VCA mode with the linear gate response";
		return s;
	}
};

// FEEDBACK in Prozent, dazu die ungefaehre Resonanzspitze (Lowpass, normiert, Gate knapp zu)
struct FeedbackQuantity : AmQuantity {
	std::string getDisplayValueString() override {
		static const float xs[7] = {0.f, 0.5f, 0.75f, 0.9f, 0.95f, 0.99f, 1.f};
		static const float ys[7] = {0.f, 0.f, 2.7f, 10.f, 15.8f, 28.6f, 40.f};
		float v = getValue();
		// Kanal dieses Reglers und dessen MODE: Rueckkopplung wirkt in LOWPASS (0), in BOTH (1) nur mit linearem Gate-Anteil
		int ch = (paramId - PT_FEEDBACK * NUM_CH) % NUM_CH;
		int mode = (int) std::round(module->params[paramIndex(PT_MODE, ch)].getValue());
		// Rueckkopplung wirkt in LOWPASS; in BOTH nur mit linearem Gate-Anteil und Lowpass-Stufe
		const bool bothRes = amBothResonance(module);
		std::string note;
		if (mode == 2)
			note = bothRes ? "\nNo effect in VCA mode;\nworks in LOWPASS and BOTH" : "\nNo effect in BOTH/VCA mode;\nworks in LOWPASS only";
		else if (mode == 1 && !bothRes)
			note = "\nNo effect in BOTH/VCA mode;\nworks in LOWPASS only";
		if (amPitchPatched(module, ch) && (mode == 0 || (mode == 1 && bothRes)))
			return string::f("%.0f %%\nResonator decay about %.2f s\n(V/OCT patched; the gate's own\nresonance is off)", v * 100.f, 0.05 * std::pow(60.0, (double) v));
		float db = ys[0];
		for (int i = 0; i < 6; i++) {
			if (v <= xs[i + 1]) {
				float f = (v - xs[i]) / (xs[i + 1] - xs[i]);
				db = ys[i] + f * (ys[i + 1] - ys[i]);
				break;
			}
			db = ys[6];
		}
		if (v < 0.5f) return string::f("%.0f %%\nNo audible resonance", v * 100.f) + note;
		if (v >= 0.995f) return string::f("%.0f %%\nAt the stability limit, rings on", v * 100.f) + note;
		return string::f("%.0f %%\nResonance peak about +%.0f dB\n(normalized mode)", v * 100.f, db) + note;
	}
};

// Sicherheitsgrenze: Ausgang des Modells ueber diesem Betrag gilt als "ausser Kontrolle"
static const float RUNAWAY_LIMIT = 50.f;

static const int OVERSAMPLE = 2;
static const int OS_QUALITY = 16;

struct AnnaMaria : Module {
	// Enum-Disziplin: nur am Ende erweitern. Je Reglerart ein 16er-Block,
	// siehe paramIndex().
	enum ParamId {
		THRESHOLD_PARAM,
		DEPTH_PARAM = THRESHOLD_PARAM + NUM_CH,
		FEEDBACK_PARAM = DEPTH_PARAM + NUM_CH,
		CVATT_PARAM = FEEDBACK_PARAM + NUM_CH,
		MODE_PARAM = CVATT_PARAM + NUM_CH,   // 0 = Lowpass, 1 = Both, 2 = VCA
		NL_PARAM = MODE_PARAM + NUM_CH,      // 0 = aus, 1 = Rueckkopplungs-Nichtlinearitaet (tanh)
		// Spaeter ergaenzt: ein gemeinsamer DRIVE-Regler (Pegelfaktor), bewusst ANS ENDE,
		// damit die Indizes der 96 Kanal-Params unveraendert bleiben.
		DRIVE_PARAM = NL_PARAM + NUM_CH,
		// Danach ergaenzt (alte Patches behalten ihre Indizes, neue Regler stehen auf Standard):
		CVBIAS_PARAM,                           // 16 Kanal-Params (im Panel: VELOCITY)
		VACTROL_PARAM = CVBIAS_PARAM + NUM_CH,  // global
		// Quad-Umbau (paramLayout 4):
		MODE_ALL_PARAM = VACTROL_PARAM + 1,     // globaler MODE-Schalter (schreibt auf alle Kanaele)
		DEEP_ALL_PARAM,                         // globaler DEEP-Schalter (Platzhalter)
		DEEP_PARAM,                             // NUM_LPG Kanal-Schalter (Platzhalter)
		WET_PARAM = DEEP_PARAM + NUM_LPG,       // global: Anteil des gestimmten V/OCT-Resonators (0..1, Verstaerkung 0,0225 k^2)
		DRY_PARAM,                              // global: Pegel des Direktpfads im V/OCT-Betrieb (0..1, Verstaerkung k^2)
		NUM_PARAMS
	};
	enum InputId {
		AUDIO_INPUT,    // IN 1 (wie bisher)
		CV_INPUT,       // CV 1 (wie bisher)
		AUDIO2_INPUT,
		AUDIO3_INPUT,
		AUDIO4_INPUT,
		CV2_INPUT,
		CV3_INPUT,
		CV4_INPUT,
		VEL_CV_INPUT,   // Velocity-CV Kanal 1, danach 2-4 (aufeinanderfolgend)
		VEL_CV2_INPUT,
		VEL_CV3_INPUT,
		VEL_CV4_INPUT,
		MODE_CV_INPUT,  // global
		THR_CV_INPUT,   // THRESHOLD-CV Kanal 1, danach 2-4 (aufeinanderfolgend), ersetzt den Knopf
		THR_CV2_INPUT,
		THR_CV3_INPUT,
		THR_CV4_INPUT,
		FB_CV_INPUT,    // FEEDBACK-CV Kanal 1, danach 2-4 (aufeinanderfolgend), ersetzt den Knopf
		FB_CV2_INPUT,
		FB_CV3_INPUT,
		FB_CV4_INPUT,
		PITCH_INPUT,    // V/Oct Kanal 1, danach 2-4 (aufeinanderfolgend), 0 V = C4; stimmt den Resonator vor dem LPG
		PITCH2_INPUT,
		PITCH3_INPUT,
		PITCH4_INPUT,
		WET_CV_INPUT,   // polyphon: Kanal 1-4 = LPG 1-4 (monophon: alle), ersetzt den WET-Knopf
		NUM_INPUTS
	};
	enum OutputId {
		AUDIO_OUTPUT,   // OUT 1 (wie bisher)
		ALL_OUTPUT,     // Summe der vier Kanaele, Softclip +-10 V
		OUT2_OUTPUT,
		OUT3_OUTPUT,
		OUT4_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightId {
		NUM_LIGHTS
	};

	static int audioIn(int c) { return c == 0 ? (int) AUDIO_INPUT : (int) AUDIO2_INPUT + c - 1; }
	static int cvIn(int c) { return c == 0 ? (int) CV_INPUT : (int) CV2_INPUT + c - 1; }
	static int velCvIn(int c) { return (int) VEL_CV_INPUT + c; }
	static int thrCvIn(int c) { return (int) THR_CV_INPUT + c; }
	static int fbCvIn(int c) { return (int) FB_CV_INPUT + c; }
	static int pitchIn(int c) { return (int) PITCH_INPUT + c; }
	static int outId(int c) { return c == 0 ? (int) AUDIO_OUTPUT : (int) OUT2_OUTPUT + c - 1; }

	AnnaMariaLpg lpg[NUM_LPG];
	dsp::Upsampler<OVERSAMPLE, OS_QUALITY> up[NUM_LPG];
	dsp::Decimator<OVERSAMPLE, OS_QUALITY, float> down[NUM_LPG];

	// Modulweite Einstellungen, nur im JSON (keine Params).
	bool fbAbsolute = false;   // false = normiert (a_norm), true = schaltungsgetreu (absolut)
	bool oversample = false;   // 2x Oversampling
	// Vactrol-Verhalten: 0 = Referenz (wie veroeffentlicht), 1 = zustandsbasiert (schneller Anstieg),
	// 2 = Leitwert (Zusatz, nicht im Paper: Glaettung in 1/Rf, langer Nachklang wie "gepingt")
	int vactrolMode = 0;
	bool nlCorrected = true;     // true (Standard) = Vorzeichenkorrektur im NL-Solver, false = Referenz
	bool boardRepro = false;     // Bauteilvariante: Plan eines Nachbaus (R8 10 k, VCA 10 k, Eingang x2) statt Paper
	bool saturation = false;     // Zusatz (nicht im Paper): tanh-Saettigung hinter dem Modell, Intensitaet ueber DRIVE
	bool vcaLinear = true;       // Modi VCA und BOTH: true = linearer Gate-Anteil (Referenzdokumentation der Hardware), false = Paper-Schaltung
	bool velAudio = false;       // Velocity-Ziel: false = Steuerpegel (Standard, Hardware-Vorbild), true = Gleichanteil auf dem Audiosignal
	bool bothLowpass = true;     // BOTH mit linearem Gate-Anteil: true = Lowpass-Stufe (Standard), false = Paper-Stufe von BOTH (C3 = 0, ohne Resonanz)
	// V/OCT-Resonator: Klingdauer aus dem FEEDBACK-Wert (exponentiell 0,05 ... 3 s), nur bei Aenderung neu rechnen
	float resFbCached[NUM_LPG] = {-1.f, -1.f, -1.f, -1.f};
	double resT60Cached[NUM_LPG] = {0.5, 0.5, 0.5, 0.5};

	float cachedSampleRate = -1.f;  // -1 erzwingt Neuinitialisierung im naechsten process()
	// Kanal ist gerade aktiv (wird gerechnet). Kanaele ohne OUT-Kabel werden uebersprungen (solange
	// auch ALL nicht belegt ist); beim erneuten Aktivieren startet der Kanal per warmStart() neu.
	bool chActive[NUM_LPG] = {};
	float velOpen[NUM_LPG] = {};   // geglaettete Gate-Oeffnung 0..1 (0 bei ATT 0 V, 1 ab 0,5 V) fuer den Velocity-Offset (Ziel Audio)
	// pow() des VACTROL-Faktors nur bei Aenderung des Reglers neu rechnen
	float vactrolKnobCached = -1.f;
	double vactrolScaleCached = 1.0;
	// Letzte Stellung der globalen Schalter (-1 = noch unbekannt: beim ersten Sample nur merken,
	// nichts auf die Kanaele schreiben, damit geladene Patches unveraendert bleiben).
	float lastModeAll = -1.f;
	float lastDeepAll = -1.f;
	// Geglaetteter Pegelfaktor L (Volt je Modelleinheit), folgt dem DRIVE-Regler.
	// Die Glaettung verhindert Knackser beim Drehen (x = Eingang/L, Ausgang = y*L).
	float modelLevel = driveToModelLevel(DRIVE_DEFAULT);

	AnnaMaria() {
		static_assert(DRIVE_PARAM == NUM_PT * NUM_CH, "ParamId passt nicht zu paramIndex()");
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int c = 0; c < NUM_CH; c++) {
			// Namen kurz, Details in "description": Rack zeigt sie als eigene Zeilen unter dem Wert.
			configParam<ThresholdQuantity>(paramIndex(PT_THRESHOLD, c), THR_MIN, THR_MAX, THR_DEFAULT,
			            string::f("Threshold (channel %d)", c + 1))->description =
			            "A patched threshold CV replaces this knob.";
			// Eingabe in Prozent (Doppelklick, "50" = 50 %), intern 0..1
			configParam<DepthQuantity>(paramIndex(PT_DEPTH, c), 0.f, 1.f, 1.f,
			            string::f("Depth, how far the closed gate attenuates (channel %d)", c + 1))->displayMultiplier = 100.f;
			{
				ParamQuantity* fbq = configParam<FeedbackQuantity>(paramIndex(PT_FEEDBACK, c), 0.f, 1.f, 0.f,
				            string::f("Feedback (channel %d)", c + 1));
				fbq->displayMultiplier = 100.f;
				fbq->description = "Resonance. Works in LOWPASS, and in BOTH with\n"
				                   "linear gate + lowpass stage.\n"
				                   "With V/OCT patched: resonator decay.\n"
				                   "A patched feedback CV replaces this knob.";
			}
			configParam<AmQuantity>(paramIndex(PT_CVATT, c), 0.f, ATT_MAX, 0.f,
			            string::f("ATT (channel %d; control level 0...10 V; a patched CV input replaces this knob)", c + 1), " V");
			configParam<AmQuantity>(CVBIAS_PARAM + c, -CVBIAS_MAX, CVBIAS_MAX, 0.f,
			            string::f("Velocity (channel %d; DC offset added to ATT knob or CV, result clamped to 0...10 V; a patched velocity CV replaces this knob)", c + 1), " V");
			configSwitch(paramIndex(PT_MODE, c), 0.f, 2.f, 1.f,
			             string::f("Mode (channel %d)", c + 1),
			             {"Lowpass", "Both", "VCA"});
			configSwitch(paramIndex(PT_NL, c), 0.f, 1.f, 0.f,
			             string::f("Feedback nonlinearity (channel %d)", c + 1),
			             {"Off", "On"})->description =
			             "Works with the feedback: in LOWPASS, and in\n"
			             "BOTH with linear gate + lowpass stage.";
		}
		configParam<DriveQuantity>(DRIVE_PARAM, 0.f, 10.f, DRIVE_DEFAULT, "Drive (input level into the model, affects feedback nonlinearity)");
		configParam<VactrolQuantity>(VACTROL_PARAM, 0.f, 1.f, 0.5f, "Vactrol time scale (left = fast, follows the gate; right = slow, envelope-like)");
		configParam<DryQuantity>(DRY_PARAM, 0.f, 1.f, 0.3f, "Dry, level of the direct signal while V/OCT is patched (all channels; 100 % = unchanged, 0 = only the tuned resonator is heard; default 30 %)");
		configParam<WetQuantity>(WET_PARAM, 0.f, 1.f, 0.9f, "Wet, level of the tuned V/OCT resonator (all channels; a patched wet CV replaces this knob)");
		configSwitch(MODE_ALL_PARAM, 0.f, 2.f, 1.f, "Mode, all channels (a change is written to all four channel switches)",
		             {"Lowpass", "Both", "VCA"});
		configSwitch(DEEP_ALL_PARAM, 0.f, 1.f, 0.f, "Deep, all channels (placeholder, no function yet)", {"Off", "On"});
		for (int c = 0; c < NUM_LPG; c++) {
			configSwitch(DEEP_PARAM + c, 0.f, 1.f, 0.f,
			             string::f("Deep (channel %d; placeholder, no function yet)", c + 1), {"Off", "On"});
			configInput(audioIn(c), string::f("Audio %d%s", c + 1, c > 0 ? " (normalled to 1)" : ""));
			configInput(cvIn(c), string::f("ATT CV %d (replaces the ATT knob when patched, 0...10 V)", c + 1));
			configInput(velCvIn(c), string::f("Velocity CV %d (replaces the velocity knob when patched, 1 V per V)", c + 1));
			configInput(thrCvIn(c), string::f("Threshold CV %d (replaces the threshold knob when patched; 1 V per V, 0...7.5 V)", c + 1));
			configInput(fbCvIn(c), string::f("Feedback CV %d (replaces the feedback knob when patched; 0...10 V = 0...100 %%)", c + 1));
			configInput(pitchIn(c), string::f("V/Oct %d (0 V = C4)", c + 1))->description =
				"Tunes a resonator in front of the gate; the\n"
				"gate sound itself is unchanged. Works in\n"
				"LOWPASS, and in BOTH with linear gate +\n"
				"lowpass stage; not in VCA mode.\n"
				"FEEDBACK sets the decay (0.05 ... 3 s), the\n"
				"gate's own resonance is off, WET the level.\n"
				"Excite it with a pulse, click or noise burst.";
			configOutput(outId(c), string::f("Audio %d", c + 1));
			configBypass(audioIn(c), outId(c));
		}
		configInput(WET_CV_INPUT, "Wet CV, polyphonic (channel 1-4 = LPG 1-4, a mono cable is used for all four; replaces the wet knob when patched; 0...10 V = 0...100 % of the knob range)");
		configInput(MODE_CV_INPUT, "Mode CV, all channels (< 3.3 V VCA, < 6.6 V Lowpass, above Both; overrides the switches)");
		configOutput(ALL_OUTPUT, "All (sum of the four channels, soft clip at +-10 V)");
	}

	void onReset() override {
		fbAbsolute = false;
		oversample = false;
		vactrolMode = 0;
		nlCorrected = true;
		saturation = false;
		boardRepro = false;
		vcaLinear = true;
		bothLowpass = true;
		velAudio = false;
		cachedSampleRate = -1.f;
		lastModeAll = -1.f;
		lastDeepAll = -1.f;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "fbAbsolute", json_boolean(fbAbsolute));
		json_object_set_new(rootJ, "oversample", json_boolean(oversample));
		// vactrolByState bleibt fuer aeltere Versionen mitgeschrieben (nur Modus 1 = true)
		json_object_set_new(rootJ, "vactrolByState", json_boolean(vactrolMode == 1));
		json_object_set_new(rootJ, "vactrolMode", json_integer(vactrolMode));
		json_object_set_new(rootJ, "nlCorrected", json_boolean(nlCorrected));
		json_object_set_new(rootJ, "saturation", json_boolean(saturation));
		json_object_set_new(rootJ, "boardRepro", json_boolean(boardRepro));
		// Fehlt der Schluessel (aeltere Patches), gilt der lineare Gate-Anteil.
		json_object_set_new(rootJ, "vcaLinear", json_boolean(vcaLinear));
		json_object_set_new(rootJ, "bothLowpass", json_boolean(bothLowpass));
		json_object_set_new(rootJ, "velAudio", json_boolean(velAudio));
		// 4 = Quad-LPG (4 Kanaele, VELOCITY, MODE/DEEP global); 3 = THRESHOLD 0...7,5 V, Standard 7,5 V
		// (Layout 2: -1...9 V, Standard 4,1 V; Layout 1: OFFSET/TRIM statt THRESHOLD/DEPTH)
		// 5 = ATT 0...10 V als Steuerpegel (CV ersetzt den Regler), Velocity als Offset (vorher 4: ATT = +-400 %)
		// 6 = THRESHOLD-/FEEDBACK-CV-Eingaenge (nur neue Eingaenge, Params unveraendert)
		json_object_set_new(rootJ, "paramLayout", json_integer(6));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// Alte Schluessel (pendingCh, displayedCh, channels) werden nicht mehr gebraucht.
		// Fehlende Schluessel: Standard = normiert / kein Oversampling.
		json_t* j = json_object_get(rootJ, "fbAbsolute");
		fbAbsolute = j ? json_is_true(j) : false;
		j = json_object_get(rootJ, "oversample");
		oversample = j ? json_is_true(j) : false;
		// vactrolMode (neu); fehlt er, gilt der alte Schluessel vactrolByState (false = Referenz).
		j = json_object_get(rootJ, "vactrolMode");
		if (j) {
			vactrolMode = rack::clamp((int) json_integer_value(j), 0, 2);
		}
		else {
			j = json_object_get(rootJ, "vactrolByState");
			vactrolMode = (j && json_is_true(j)) ? 1 : 0;
		}
		j = json_object_get(rootJ, "nlCorrected");
		nlCorrected = j ? json_is_true(j) : true;
		j = json_object_get(rootJ, "saturation");
		saturation = j ? json_is_true(j) : false;
		j = json_object_get(rootJ, "boardRepro");
		if (!j) j = json_object_get(rootJ, "boardNR");   // Patches vor der Umbenennung
		boardRepro = j ? json_is_true(j) : false;
		j = json_object_get(rootJ, "vcaLinear");
		vcaLinear = j ? json_is_true(j) : true;
		j = json_object_get(rootJ, "bothLowpass");
		bothLowpass = j ? json_is_true(j) : true;
		j = json_object_get(rootJ, "velAudio");
		velAudio = j ? json_is_true(j) : false;
		// Alte Patches (ohne paramLayout) hatten OFFSET und TRIM in den ersten beiden
		// Reglerbloecken: in Schwelle (V) und Tiefe umrechnen. Rack hat die Params zu diesem
		// Zeitpunkt schon geladen.
		j = json_object_get(rootJ, "paramLayout");
		if (!j || json_integer_value(j) < 2) {
			for (int c = 0; c < NUM_CH; c++) {
				float offOld = params[paramIndex(PT_THRESHOLD, c)].getValue();
				float trimOld = params[paramIndex(PT_DEPTH, c)].getValue();
				params[paramIndex(PT_THRESHOLD, c)].setValue(offsetTrimToThreshold(offOld, trimOld));
			}
		}
		// Patches vor Layout 5: ATT war eine Verstaerkung (+-400 %) fuer die CV-Buchse und ist jetzt ein
		// Steuerpegel in Volt; der alte Wert passt nicht mehr, daher auf 0 V (Standard) setzen.
		if (!j || json_integer_value(j) < 5) {
			for (int c = 0; c < NUM_CH; c++)
				params[paramIndex(PT_CVATT, c)].setValue(0.f);
		}
		cachedSampleRate = -1.f;
		lastModeAll = -1.f;
		lastDeepAll = -1.f;
	}

	// Samplerate (inkl. Oversampling-Faktor) in alle Engines schreiben und alle Kanaele
	// beim naechsten process() per warmStart() neu starten.
	void applySampleRate(float sampleRate) {
		cachedSampleRate = sampleRate;
		double engineRate = (double) sampleRate * (oversample ? OVERSAMPLE : 1);
		for (int c = 0; c < NUM_LPG; c++)
			lpg[c].setSampleRate(engineRate);
		for (int c = 0; c < NUM_LPG; c++)
			chActive[c] = false;
		vactrolKnobCached = -1.f;
		// Pegelfaktor ohne Glaettung auf den Reglerwert setzen (Start, Patch laden)
		modelLevel = driveToModelLevel(params[DRIVE_PARAM].getValue());
	}

	// Engine-Parameter eines Kanals aus den Rack-Params zusammenstellen (ohne cv).
	// modeOverride >= 0 (0 Lowpass, 1 Both, 2 VCA): Modus kommt von der MODE-CV statt vom Schalter.
	AnnaMariaLpg::Params channelParams(int c, int modeOverride) {
		AnnaMariaLpg::Params p;
		p.trim = params[paramIndex(PT_DEPTH, c)].getValue();
		// Gesteckte CV ersetzt den Knopf: THRESHOLD 1 V/V (0...7,5 V), FEEDBACK 0...10 V = 0...100 %.
		Input& thrIn = inputs[thrCvIn(c)];
		float thr = thrIn.isConnected() ? rack::clamp(thrIn.getVoltage(), THR_MIN, THR_MAX)
		                                : params[paramIndex(PT_THRESHOLD, c)].getValue();
		p.offset = thresholdToOffset(thr, p.trim);
		Input& fbIn = inputs[fbCvIn(c)];
		float fbKnob = fbIn.isConnected() ? rack::clamp(fbIn.getVoltage() * 0.1f, 0.f, 1.f)
		                                  : params[paramIndex(PT_FEEDBACK, c)].getValue();
		if (fbAbsolute) {
			p.fbnorm = false;
			p.fb = fbKnob * ABS_FB_MAX;
		}
		else {
			p.fbnorm = true;
			p.fb = fbKnob;
		}
		int sw = (modeOverride >= 0) ? modeOverride : (int) std::round(params[paramIndex(PT_MODE, c)].getValue());
		p.mode = (sw <= 0) ? AnnaMariaLpg::MODE_LOWPASS
		       : (sw >= 2) ? AnnaMariaLpg::MODE_VCA
		       : AnnaMariaLpg::MODE_BOTH;
		p.nl = params[paramIndex(PT_NL, c)].getValue() > 0.5f;
		p.vactrolByState = (vactrolMode == 1);
		p.vactrolConductance = (vactrolMode == 2);
		p.nlCorrected = nlCorrected;
		p.vcaLinear = vcaLinear;
		p.bothLowpass = bothLowpass;
		float vk = params[VACTROL_PARAM].getValue();
		if (vk != vactrolKnobCached) {
			vactrolKnobCached = vk;
			vactrolScaleCached = vactrolKnobToScale(vk);
		}
		p.vactrolTimeScale = vactrolScaleCached;
		// V/Oct (gesteckt): gestimmter Resonator vor der LPG-Stufe (Engine resHz), 0 V = C4 (261,6256 Hz, wie in Rack
		// ueblich). Bereich +-6 Oktaven um C4, die Engine begrenzt zusaetzlich nach oben auf 0,45 fs. Nur in den Modi mit
		// Resonanzstufe (LOWPASS; BOTH mit linearem Gate und Lowpass-Stufe). Dann bestimmt FEEDBACK (Knopf oder CV)
		// die Klingdauer des Resonators, die LPG-Rueckkopplung ist 0. WET: Knopf oder polyphones CV (ersetzt den Knopf).
		Input& pitchInp = inputs[pitchIn(c)];
		p.pitchHz = 0.0;
		const bool resStage = (p.mode == AnnaMariaLpg::MODE_LOWPASS)
			|| (p.mode == AnnaMariaLpg::MODE_BOTH && vcaLinear && bothLowpass);
		if (pitchInp.isConnected() && resStage) {
			// Hochpass auf dem Direktpfad (Eckfrequenz = Faktor * Notenfrequenz): nimmt Gleichanteil und Tiefen eines
			// unipolaren Nadelimpulses (Huellkurve am IN) weg, die sonst als tiefer Plopp durch das Gate laufen.
			// Faktor 0 = aus; 0.5 = Standard; 1 = staerker.
			p.resHpRatio = RES_DRY_HP_RATIO;
			p.resHpMinHz = RES_DRY_HP_MIN_HZ;
			p.resLpHz = RES_DRY_LP_HZ;
			p.resHz = 261.6255653005986 * std::exp2((double) rack::clamp(pitchInp.getVoltage(), -6.f, 6.f));
			Input& wetIn = inputs[WET_CV_INPUT];
			const float wetK = wetIn.isConnected() ? rack::clamp(wetIn.getPolyVoltage(c) * 0.1f, 0.f, 1.f)
			                                       : params[WET_PARAM].getValue();
			p.resWet = wetToGain(wetK);
			p.resDry = dryToGain(params[DRY_PARAM].getValue());
			if (fbKnob != resFbCached[c]) {
				resFbCached[c] = fbKnob;
				resT60Cached[c] = 0.05 * std::pow(60.0, (double) fbKnob);
			}
			p.resT60 = resT60Cached[c];
			p.fb = 0.0;
		}
		if (boardRepro) {
			p.r8 = 10e3;          // Plan: R8 = 10 k (Paper: 4,7 k)
			p.vcaRa = 10e3;       // Plan: R16 = 10 k im VCA-Modus (Paper: 5 k)
			p.vcaPreGain = 2.0;   // Plan: R11 = R12 = 15 k, im VCA-Modus Verstaerkung 2
		}
		return p;
	}

	void restartChannel(int c, const AnnaMariaLpg::Params& p) {
		lpg[c].warmStart(p);
		velOpen[c] = rack::clamp(p.cv * 2.f, 0.f, 1.f);
		up[c].reset();
		down[c].reset();
	}

	// Globale Schalter (MODE, DEEP): eine Aenderung wird auf alle vier Kanalschalter geschrieben.
	void applyGlobalSwitches() {
		float m = params[MODE_ALL_PARAM].getValue();
		if (lastModeAll < 0.f) {
			lastModeAll = m;   // erstes Sample / Patch geladen: nur merken
		}
		else if (m != lastModeAll) {
			for (int c = 0; c < NUM_LPG; c++)
				params[paramIndex(PT_MODE, c)].setValue(m);
			lastModeAll = m;
		}
		float d = params[DEEP_ALL_PARAM].getValue();
		if (lastDeepAll < 0.f) {
			lastDeepAll = d;
		}
		else if (d != lastDeepAll) {
			for (int c = 0; c < NUM_LPG; c++)
				params[DEEP_PARAM + c].setValue(d);
			lastDeepAll = d;
		}
	}

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != cachedSampleRate)
			applySampleRate(args.sampleRate);

		applyGlobalSwitches();

		// MODE-CV (global): belegt = bestimmt den Modus aller Kanaele
		int modeOverride = -1;
		if (inputs[MODE_CV_INPUT].isConnected())
			modeOverride = modeFromCv(inputs[MODE_CV_INPUT].getVoltage());

		// Pegelfaktor folgt dem DRIVE-Regler mit ca. 2,5 ms Zeitkonstante (alle Kanaele gleich)
		float targetLevel = driveToModelLevel(params[DRIVE_PARAM].getValue());
		modelLevel += (targetLevel - modelLevel) * std::min(1.f, args.sampleTime * 400.f);

		float sumAll = 0.f;   // Summe der Kanalausgaenge in Rack-Volt fuer den ALL-Ausgang

		// Kanaele ohne OUT-Kabel nicht rechnen (spart bis zu drei Viertel der Rechenzeit); ALL braucht alle vier.
		const bool allUsed = outputs[ALL_OUTPUT].isConnected();

		for (int c = 0; c < NUM_LPG; c++) {
			// Ohne Audioquelle (eigener IN und IN 1 leer) gibt es nichts zu hoeren: Kanal nicht rechnen,
			// Ausgang auf 0 V. Ohne OUT-Kabel (und ohne ALL) braucht niemand das Ergebnis.
			const bool hasSource = inputs[audioIn(c)].isConnected() || inputs[AUDIO_INPUT].isConnected();
			if (!hasSource || (!allUsed && !outputs[outId(c)].isConnected())) {
				chActive[c] = false;
				if (outputs[outId(c)].isConnected()) {
					outputs[outId(c)].setChannels(1);
					outputs[outId(c)].setVoltage(0.f);
				}
				continue;
			}
			AnnaMariaLpg::Params p = channelParams(c, modeOverride);

			// Eingang: IN 1 ist auf IN 2-4 normalisiert, wenn dort nichts steckt. Unbelegt = 0 V.
			Input& inA = inputs[audioIn(c)].isConnected() ? inputs[audioIn(c)] : inputs[AUDIO_INPUT];
			float in = inA.isConnected() ? inA.getVoltage() : 0.f;

			Input& inC = inputs[cvIn(c)];
			// Hardware-Vorbild: gesteckte CV ersetzt den ATT-Regler; Velocity (Regler oder gesteckte
			// Velocity-CV) wird als Gleichanteil addiert, Summe auf 0...10 V begrenzt.
			float src = inC.isConnected() ? inC.getVoltage() : params[paramIndex(PT_CVATT, c)].getValue();
			float vel = inputs[velCvIn(c)].isConnected() ? inputs[velCvIn(c)].getVoltage()
			                                             : params[CVBIAS_PARAM + c].getValue();
			if (velAudio) {
				// Velocity-Ziel Audio: Steuerpegel ohne Velocity, Velocity als Gleichanteil auf dem
				// Audiosignal (Offset nach ATT, Schnitt bei +-10 V).
				p.cv = rack::clamp(src, 0.f, 10.f);
				// Velocity wirkt hier nicht auf den Steuerpegel, sondern als Gleichanteil auf dem Signal
				// nach dem Gate/ATT (siehe unten bei yOut).
			}
			else {
				p.cv = rack::clamp(src + vel, 0.f, 10.f);
			}

			// Neuer Kanal: Steuerpfad im eingeschwungenen Zustand starten
			if (!chActive[c]) {
				restartChannel(c, p);
				chActive[c] = true;
			}

			float x = in / modelLevel;
			float y;
			float rawPeak = 0.f;   // groesster Modellausgang vor der Saettigung (fuer den Schutz)
			if (!oversample) {
				y = (float) lpg[c].process(x, p);
				rawPeak = std::fabs(y);
				if (saturation) y = std::tanh(y);
			}
			else {
				float xu[OVERSAMPLE];
				float yu[OVERSAMPLE];
				up[c].process(x, xu);
				for (int k = 0; k < OVERSAMPLE; k++) {
					yu[k] = (float) lpg[c].process(xu[k], p);
					rawPeak = std::max(rawPeak, std::fabs(yu[k]));
					if (saturation) yu[k] = std::tanh(yu[k]);
				}
				y = down[c].process(yu);
			}

			// Schutz: bei Ueberlauf/NaN den Kanal neu starten (Ausgang 0 fuer dieses Sample)
			if (!std::isfinite(y) || !(rawPeak <= RUNAWAY_LIMIT)) {
				restartChannel(c, p);
				y = 0.f;
			}

			float yOut = y * modelLevel;
			if (velAudio) {
				// Gleichanteil nach ATT/Gate und nur bei offenem Gate: Velocity wird mit der Gate-Oeffnung
				// (0 bei ATT 0 V, voller Offset ab ATT 0,5 V, mit den Vactrol-Zeiten geglaettet, damit der Offset nicht knackt) skaliert.
				// Danach wird in Verschieberichtung bei +-10 V (Rack-Bereich) geschnitten.
				// Beispiel +-2 V nach ATT, Gate offen: Vel -4 V -> -6/-2 V; Gate zu (ATT 0): kein Offset.
				// Vel 0 = keine Wirkung (fliessender Uebergang). Ein Signal, das schon ueber der
				// Grenze liegt, wird nicht zusaetzlich beschnitten.
				const float tgt = rack::clamp(p.cv * 2.f, 0.f, 1.f);   // 0 bei ATT 0 V, voll ab 0,5 V
				const float tau = (tgt > velOpen[c] ? 0.0024f : 0.050f) * std::max((float) p.vactrolTimeScale, 0.001f);
				velOpen[c] += (tgt - velOpen[c]) * (1.f - std::exp(-args.sampleTime / tau));
				const float velEff = vel * velOpen[c];
				if (velEff < 0.f)
					yOut = std::max(yOut + velEff, std::min(yOut, -10.f));
				else
					yOut = std::min(yOut + velEff, std::max(yOut, 10.f));
			}
			outputs[outId(c)].setChannels(1);
			outputs[outId(c)].setVoltage(yOut);
			sumAll += yOut;
		}

		// ALL: Summe der vier Kanaele, danach Softclip auf +-10 V (mono).
		outputs[ALL_OUTPUT].setChannels(1);
		outputs[ALL_OUTPUT].setVoltage(softClip10(sumAll));
	}
};


static bool amLinearGate(Module* m) {
	AnnaMaria* a = dynamic_cast<AnnaMaria*>(m);
	return a && a->vcaLinear;
}

static bool amBothResonance(Module* m) {
	AnnaMaria* a = dynamic_cast<AnnaMaria*>(m);
	return a && a->vcaLinear && a->bothLowpass;
}

static bool amPitchPatched(Module* m, int ch) {
	AnnaMaria* a = dynamic_cast<AnnaMaria*>(m);
	return a && ch >= 0 && ch < NUM_LPG && a->inputs[AnnaMaria::pitchIn(ch)].isConnected();
}


// ---------------------------------------------------------------------
// Menue-Regler
// ---------------------------------------------------------------------

// Quantity fuer die Regler im Kontextmenue: schreibt den Wert in `count` aufeinanderfolgende Params
// ab `firstId` (count = 1: ein Param, count = NUM_LPG: derselbe Wert in alle vier Kanaele) und liest
// den ersten.
struct AmMenuQuantity : Quantity {
	enum Kind { K_THRESHOLD, K_PERCENT, K_DRIVE, K_VACTROL };
	AnnaMaria* module;
	int firstId;
	int count;
	int kind;
	std::string label;
	// Nur bei count > 1 ("alle Kanaele"): eigener Anzeigewert des Schiebers. Er startet beim Oeffnen
	// des Menues mit dem Wert von Kanal 1 und folgt danach nur dem eigenen Schieber, nicht den
	// Einzelschiebern (sonst wuerde ein Kanalschieber den "alle"-Schieber mitbewegen).
	float local = 0.f;

	AmMenuQuantity(AnnaMaria* m, int first, int cnt, int k, const std::string& l)
		: module(m), firstId(first), count(cnt), kind(k), label(l) {
		local = m->params[first].getValue();
	}

	ParamQuantity* pq() { return module->paramQuantities[firstId]; }

	void setValue(float v) override {
		v = rack::clamp(v, getMinValue(), getMaxValue());
		local = v;
		for (int i = 0; i < count; i++)
			APP->engine->setParamValue(module, firstId + i, v);
	}
	float getValue() override { return count > 1 ? local : module->params[firstId].getValue(); }
	float getMinValue() override { return pq()->getMinValue(); }
	float getMaxValue() override { return pq()->getMaxValue(); }
	float getDefaultValue() override { return pq()->getDefaultValue(); }
	std::string getLabel() override { return label; }
	std::string getUnit() override { return ""; }
	std::string getDisplayValueString() override {
		float v = getValue();
		switch (kind) {
			case K_THRESHOLD: return string::f("%.2f V", v);
			case K_PERCENT: return string::f("%.0f %%", v * 100.f);
			case K_DRIVE: return string::f("%.1f", v);
			default: return string::f("x%.2f", vactrolKnobToScale(v));
		}
	}
};

struct AmMenuSlider : ui::Slider {
	AmMenuSlider(Quantity* q) {
		quantity = q;
		box.size.x = 230.f;
	}
	~AmMenuSlider() {
		delete quantity;
	}

	// Mausrad ueber dem Schieber aendert den Wert (wie bei den Knoepfen im Panel).
	// Eine Rastung des Rads (scrollDelta.y ca. 50) = 2 % des Regelwegs;
	// Strg = fein (1/10), Shift = grob (x5). Rad nach oben = Wert hoch
	// (falls bei dir umgekehrt: das Vorzeichen bei e.scrollDelta.y tauschen).
	// Achtung: Rack-Menues reichen onHoverScroll NICHT an ihre Kinder weiter (Menu::onHoverScroll
	// verschiebt nur das Menue selbst). Dieser Handler wird daher nur aufgerufen, wenn das Event
	// direkt ankommt; im Kontextmenue sorgt AmMenuScrollCatcher (unten) dafuer.
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (applyScroll(e.scrollDelta.y))
			e.consume(this);
	}

	bool applyScroll(float scrollY) {
		if (!quantity)
			return false;
		float range = quantity->getMaxValue() - quantity->getMinValue();
		float d = scrollY * 0.0004f * range;
		int mods = APP->window->getMods();
		if (mods & GLFW_MOD_CONTROL)
			d *= 0.1f;
		if (mods & GLFW_MOD_SHIFT)
			d *= 5.f;
		quantity->setValue(quantity->getValue() + d);
		return true;
	}
};

// Unsichtbarer Faenger im MenuOverlay (Geschwister des Menues, als letztes Kind eingefuegt und
// damit als erstes befragt): Liegt der Mauszeiger auf einem AmMenuSlider (hoveredWidget), dreht
// das Mausrad dessen Wert und das Event gilt als verbraucht; sonst bleibt alles wie in Rack
// (Menue scrollen). Ein einziger Fänger genuegt auch fuer die Untermenues, denn alle Menues
// einer Kette haengen im selben Overlay.
struct AmMenuScrollCatcher : Widget {
	void step() override {
		if (parent)
			box = Rect(Vec(0.f, 0.f), parent->box.size);
		Widget::step();
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		AmMenuSlider* s = dynamic_cast<AmMenuSlider*>(APP->event->hoveredWidget);
		if (s && s->applyScroll(e.scrollDelta.y))
			e.consume(this);
	}
};

// DEPTH fuer `count` Kanaele ab Kanal `c` (count = NUM_LPG: alle vier). THRESHOLD, FEEDBACK, FB NL, DRIVE
// und VACTROL liegen auf dem Panel.
static void addDepthSlider(Menu* menu, AnnaMaria* m, int c, int count, const std::string& label) {
	menu->addChild(new AmMenuSlider(new AmMenuQuantity(m, paramIndex(PT_DEPTH, c), count, AmMenuQuantity::K_PERCENT, label)));
}


// ---------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------

// Schlichte Panel-Flaeche wie in den uebrigen Modulen (lokale Kopie mit
// eigenem Namen, siehe Kommentar oben zum anonymen Namespace).
struct AnnaMariaPanelBackground : Widget {
	NVGcolor color = nvgRGB(228, 228, 226);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, color);
		nvgFill(args.vg);
	}
};

// Rein dekorativer Rahmen. TransparentWidget, damit er keine Maus-Events
// der Regler darunter abfaengt.
struct AnnaMariaFrame : TransparentWidget {
	NVGcolor color = nvgRGB(120, 120, 120);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 3.f);
		nvgStrokeColor(args.vg, color);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}
};

// Uebernimmt den Wert eines Reglers/Schalters fuer alle vier Kanaele derselben
// Art, als EIN Undo-Schritt (ComplexAction), analog zu Minerva. Kein
// Undo-Eintrag, falls sich nichts aendert.
static void annaApplyToAllChannels(ParamWidget* w, int baseParamId) {
	ParamQuantity* pq = w->getParamQuantity();
	if (!w->module || !pq)
		return;
	float value = pq->getValue();

	history::ComplexAction* complex = new history::ComplexAction;
	complex->name = "apply Anna-Maria value to all channels";
	bool changed = false;
	for (int i = 0; i < NUM_LPG; i++) {
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

// Regler mit Rechtsklick-Eintrag "Apply to all 4 channels".
struct AnnaMariaParamKnob : RoundSmallBlackKnob {
	int baseParamId = 0; // erster Param des Blocks (Kanal 1)

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Apply to all 4 channels", "", [this]() { annaApplyToAllChannels(this, baseParamId); }));
	}
};

// Textschalter mit 2 oder 3 Zustaenden (Klick schaltet weiter, Basisklasse
// app::Switch), gleiche Optik wie MinervaToggle. Der Zustandstext kommt aus
// `texts`; ab `highlightFrom` wird der Schalter farbig hinterlegt (99 = nie).
// baseParamId < 0: kein "Apply to all"-Eintrag (globale Schalter).
struct AnnaMariaToggle : app::Switch {
	int baseParamId = -1;
	std::vector<std::string> texts;
	int highlightFrom = 99;
	float fontSize = 7.f;
	NVGcolor colorOn = nvgRGB(220, 150, 40);

	AnnaMariaToggle() {
		box.size = mm2px(Vec(14.f, 5.f));
	}

	void appendContextMenu(Menu* menu) override {
		if (baseParamId < 0)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Apply to all 4 channels", "", [this]() { annaApplyToAllChannels(this, baseParamId); }));
	}

	void draw(const DrawArgs& args) override {
		ParamQuantity* pq = getParamQuantity();
		int state = pq ? (int) std::round(pq->getValue()) : 0;
		bool on = state >= highlightFrom;

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 1.5f);
		nvgFillColor(args.vg, on ? colorOn : nvgRGB(205, 205, 200));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 220));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Nunito-Bold.ttf"));
		if (font && font->handle >= 0 && state >= 0 && state < (int) texts.size()) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, fontSize);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, on ? nvgRGB(255, 255, 255) : nvgRGB(60, 60, 60));
			nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f + 0.5f, texts[state].c_str(), NULL);
		}
	}
};


// Spaltenueberschrift: dunkles Namensband wie die Stage-Baender in Europa
// (Text kommt als PanelLabel darueber).
struct AnnaMariaHeader : TransparentWidget {
	NVGcolor fillColor = nvgRGB(40, 40, 44);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 2.f);
		nvgFillColor(args.vg, fillColor);
		nvgFill(args.vg);
	}
};


struct AnnaMariaWidget : ModuleWidget {
	AnnaMaria* mod = nullptr;

	// Beschriftung: Oberkante bei posMM.y, mittig, ueber dem zugehoerigen Element.
	void addLabel(Vec posMM, const std::string& text, NVGcolor color) {
		addChild(new PanelLabel(posMM, text, color, 7.f, NVG_ALIGN_CENTER | NVG_ALIGN_TOP));
	}

	AnnaMariaWidget(AnnaMaria* module) {
		setModule(module);
		mod = module;

		const float HP = 31.f;
		box.size = Vec(HP * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		const float W_MM = HP * 5.08f;       // 157,48 mm
		const float CX = W_MM / 2.f;
		const float ALLW = 21.92f;           // Spalte ALL
		const float CHW = 33.89f;            // je Kanalspalte (ALLW + 4 x CHW = 157,48 mm)
		// Seitenabstand wie in der ALL-Spalte: dort sind es 2,96 mm zwischen Rahmen und Schalter
		// (Rahmen 1 mm vom Spaltenrand, Schalter 14 mm in 21,92 mm). In den Kanalspalten haben die
		// Schalter 11,4 mm; die Unterspalten-Mitten liegen so, dass derselbe Abstand entsteht.
		const float SUBX = 1.f + 2.96f + 11.4f / 2.f;   // Abstand der Unterspalten-Mitte vom Spaltenrand

		AnnaMariaPanelBackground* panelBg = new AnnaMariaPanelBackground;
		panelBg->box.size = box.size;
		addChild(panelBg);

		PanelBorder* panelBorder = new PanelBorder;
		panelBorder->box.size = box.size;
		addChild(panelBorder);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		// Titel
		addChild(new PanelLabel(Vec(CX, 5.4f), "ANNA-MARIA", nvgRGB(40, 40, 44), 13.f,
			NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE));

		// Reihen in mm: Beschriftung (Oberkante) steht ueber dem Element, Element (Mitte) darunter.
		// Sieben Reihen je Kanal; Knopf/CV-Paare sind eng gruppiert, zwischen den Gruppen mehr Luft.
		// Untergrenze: Buchsenunterkante ca. 122,3 mm, darunter liegen die Schrauben (ab 123,4 mm).
		const float lR1 = 20.5f,  yR1 = 28.f;     // IN | OUT (Buchsen); ALL: ALL OUT
		const float lR2 = 37.f,   yR2 = 45.f;     // ATT | VEL (Knoepfe); ALL: MODE (Schalter)
		const float lR3 = 51.f,   yR3 = 58.3f;    // CV | CV (Buchsen);   ALL: MODE CV
		const float lR4 = 68.f,   yR4 = 76.f;     // THRESH | FEEDBACK (Knoepfe); ALL: DRIVE
		const float lR5 = 82.f,   yR5 = 89.3f;    // CV | CV (Buchsen);   ALL: VACTROL (Knopf, y = lR5 + 8)
		const float lR6 = 95.5f;                  // FB NL (Schalter, Mitte der Kanalspalte)
		const float lR7 = 108.f;                 // MODE | DEEP (Schalter); ALL: DEEP
		const float togDy = 6.8f - 0.f;           // Schalter steht 6,8 mm unter seiner Beschriftung

		// Schalter mit frei waehlbarer Breite, mittig auf (centerMM) gesetzt.
		auto addToggle = [&](Vec centerMM, int paramId, float wMM, int baseParamId,
		                     const std::vector<std::string>& texts, int highlightFrom) {
			AnnaMariaToggle* t = createParam<AnnaMariaToggle>(Vec(0.f, 0.f), module, paramId);
			t->box.size = mm2px(Vec(wMM, 5.f));
			t->box.pos = mm2px(centerMM).minus(t->box.size.div(2.f));
			t->baseParamId = baseParamId;
			t->texts = texts;
			t->highlightFrom = highlightFrom;
			addParam(t);
		};

		// Spaltenrahmen und Kopfband (wie Stage-Rahmen/Namensband in Europa):
		// Spalte 0 = ALL, Spalten 1-4 = Kanal 1-4
		for (int i = 0; i < 5; i++) {
			float left = (i == 0) ? 0.f : ALLW + (i - 1) * CHW;
			float w = (i == 0) ? ALLW : CHW;
			float xc = left + w / 2.f;
			AnnaMariaFrame* frame = new AnnaMariaFrame;
			frame->color = nvgRGBA(90, 90, 100, 170);
			frame->box.pos = mm2px(Vec(left + 1.f, 10.5f));
			frame->box.size = mm2px(Vec(w - 2.f, 112.5f));
			addChild(frame);

			AnnaMariaHeader* head = new AnnaMariaHeader;
			head->box.pos = mm2px(Vec(left + 1.7f, 11.2f));
			head->box.size = mm2px(Vec(w - 3.4f, 6.2f));
			addChild(head);
			addChild(new PanelLabel(Vec(xc, 14.3f), (i == 0) ? std::string("ALL") : string::f("LPG %d", i),
				nvgRGB(232, 232, 228), 8.f, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE));
		}

		// ---- Spalte 1: ALL-Ausgang, MODE (global), MODE-CV, DRIVE, VACTROL, WET, WET-CV ----
		{
			const float x = ALLW * 0.5f;
			addLabel(Vec(x, lR1), "ALL OUT", COLOR_STAGEMOD);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, yR1)), module, AnnaMaria::ALL_OUTPUT));

			addLabel(Vec(x, lR2), "MODE", COLOR_STAGEMOD);
			addToggle(Vec(x, lR2 + togDy), AnnaMaria::MODE_ALL_PARAM, 14.f, -1, {"LOWPASS", "BOTH", "VCA"}, 99);

			addLabel(Vec(x, lR3), "MODE CV", COLOR_STAGEMOD);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, yR3)), module, AnnaMaria::MODE_CV_INPUT));

			addLabel(Vec(x, lR4), "DRIVE", COLOR_STAGEMOD);
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x, yR4)), module, AnnaMaria::DRIVE_PARAM));
			addLabel(Vec(x, lR5), "VACTROL", COLOR_STAGEMOD);
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x, lR5 + 8.f)), module, AnnaMaria::VACTROL_PARAM));

			addLabel(Vec(x - 4.8f, lR6), "DRY", COLOR_STAGEMOD);
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x - 4.8f, lR6 + 7.5f)), module, AnnaMaria::DRY_PARAM));
			addLabel(Vec(x + 4.8f, lR6), "WET", COLOR_STAGEMOD);
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x + 4.8f, lR6 + 7.5f)), module, AnnaMaria::WET_PARAM));
			addLabel(Vec(x, lR7), "WET CV", COLOR_STAGEMOD);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, lR7 + togDy)), module, AnnaMaria::WET_CV_INPUT));
		}

		// ---- Spalten 2-5: je ein Kanal, zwei Unterspalten (links / rechts) ----
		for (int c = 0; c < NUM_LPG; c++) {
			const float left = ALLW + c * CHW;
			const float xl = left + SUBX;
			const float xr = left + CHW - SUBX;

			// Reihe 1: IN | OUT
			addLabel(Vec(xl, lR1), "IN", COLOR_STAGEMOD);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xl, yR1)), module, AnnaMaria::audioIn(c)));
			addLabel(Vec(xr, lR1), "OUT", COLOR_STAGEMOD);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(xr, yR1)), module, AnnaMaria::outId(c)));

			// Reihe 2: ATT | VEL
			addLabel(Vec(xl, lR2), "ATT", COLOR_STAGEMOD);
			AnnaMariaParamKnob* att = createParamCentered<AnnaMariaParamKnob>(
				mm2px(Vec(xl, yR2)), module, paramIndex(PT_CVATT, c));
			att->baseParamId = paramIndex(PT_CVATT, 0);
			addParam(att);
			addLabel(Vec(xr, lR2), "VEL", COLOR_STAGEMOD);
			AnnaMariaParamKnob* vel = createParamCentered<AnnaMariaParamKnob>(
				mm2px(Vec(xr, yR2)), module, AnnaMaria::CVBIAS_PARAM + c);
			vel->baseParamId = AnnaMaria::CVBIAS_PARAM;
			addParam(vel);

			// Reihe 3: zugehoerige CV-Eingaenge
			addLabel(Vec(xl, lR3), "CV", COLOR_STAGEMOD);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xl, yR3)), module, AnnaMaria::cvIn(c)));
			addLabel(Vec(xr, lR3), "CV", COLOR_STAGEMOD);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xr, yR3)), module, AnnaMaria::velCvIn(c)));

			// Reihe 4: THRESH | FEEDBACK
			addLabel(Vec(xl, lR4), "THRESH", COLOR_STAGEMOD);
			AnnaMariaParamKnob* thr = createParamCentered<AnnaMariaParamKnob>(
				mm2px(Vec(xl, yR4)), module, paramIndex(PT_THRESHOLD, c));
			thr->baseParamId = paramIndex(PT_THRESHOLD, 0);
			addParam(thr);
			addLabel(Vec(xr, lR4), "FEEDBACK", COLOR_STAGEMOD);
			AnnaMariaParamKnob* fb = createParamCentered<AnnaMariaParamKnob>(
				mm2px(Vec(xr, yR4)), module, paramIndex(PT_FEEDBACK, c));
			fb->baseParamId = paramIndex(PT_FEEDBACK, 0);
			addParam(fb);

			// Reihe 5: CV-Eingaenge fuer THRESH und FEEDBACK
			addLabel(Vec(xl, lR5), "CV", COLOR_STAGEMOD);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xl, yR5)), module, AnnaMaria::thrCvIn(c)));
			addLabel(Vec(xr, lR5), "CV", COLOR_STAGEMOD);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xr, yR5)), module, AnnaMaria::fbCvIn(c)));

			// Reihe 6: FB NL (links) | V/OCT (rechts, Buchse in Hoehe des Schalters)
			addLabel(Vec(xl, lR6), "FB NL", COLOR_STAGEMOD);
			addToggle(Vec(xl, lR6 + togDy), paramIndex(PT_NL, c), 11.4f, paramIndex(PT_NL, 0),
			          {"OFF", "ON"}, 1);
			addLabel(Vec(xr, lR6), "V/OCT", COLOR_STAGEMOD);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xr, lR6 + togDy)), module, AnnaMaria::pitchIn(c)));

			// Reihe 7: MODE | DEEP
			addLabel(Vec(xl, lR7), "MODE", COLOR_STAGEMOD);
			addToggle(Vec(xl, lR7 + togDy), paramIndex(PT_MODE, c), 11.4f, paramIndex(PT_MODE, 0),
			          {"LOWPASS", "BOTH", "VCA"}, 99);
			addLabel(Vec(xr, lR7), "DEEP", COLOR_STAGEMOD);
			addToggle(Vec(xr, lR7 + togDy), AnnaMaria::DEEP_PARAM + c, 11.4f, AnnaMaria::DEEP_PARAM,
			          {"OFF", "ON"}, 1);
		}
	}

	// DEPTH (je Kanal) und Modell-Einstellungen
	// (Checklisten, Faustregel).
	void appendContextMenu(Menu* menu) override {
		AnnaMaria* m = mod;
		if (!m)
			return;
		// Mausrad ueber den Menue-Schiebern (siehe AmMenuScrollCatcher): Fänger ins MenuOverlay.
		if (menu->parent)
			menu->parent->addChild(new AmMenuScrollCatcher);
		menu->addChild(new MenuSeparator);

		// DEPTH (wie weit das geschlossene Gate daempft): alle Schieber in einem Untermenue
		menu->addChild(createSubmenuItem("Depth", "", [m](Menu* sub) {
			addDepthSlider(sub, m, 0, NUM_LPG, "All channels (sets all four)");
			for (int c = 0; c < NUM_LPG; c++)
				addDepthSlider(sub, m, c, 1, string::f("Channel %d", c + 1));
		}));

		menu->addChild(new MenuSeparator);

		// Modulweit fuer alle Kanaele.
		menu->addChild(createSubmenuItem("Resonance", m->fbAbsolute ? "Circuit-accurate" : "Normalized", [m](Menu* sub) {
			sub->addChild(createCheckMenuItem("Normalized (even, always stable)", "",
				[m]() { return !m->fbAbsolute; },
				[m]() { m->fbAbsolute = false; }));
			sub->addChild(createCheckMenuItem("Circuit-accurate (absolute)", "",
				[m]() { return m->fbAbsolute; },
				[m]() { m->fbAbsolute = true; }));
		}));
		menu->addChild(createSubmenuItem("Vactrol response",
			m->vactrolMode == 2 ? "Conductance" : (m->vactrolMode == 1 ? "State-based" : "Reference"), [m](Menu* sub) {
			sub->addChild(createCheckMenuItem("Reference (as published, slow attack)", "",
				[m]() { return m->vactrolMode == 0; },
				[m]() { m->vactrolMode = 0; }));
			sub->addChild(createCheckMenuItem("State-based (fast attack, like a real LPG)", "",
				[m]() { return m->vactrolMode == 1; },
				[m]() { m->vactrolMode = 1; }));
			sub->addChild(createCheckMenuItem("Conductance (ping-like, not in the paper)", "",
				[m]() { return m->vactrolMode == 2; },
				[m]() { m->vactrolMode = 2; }));
		}));
		menu->addChild(createSubmenuItem("NL solver", m->nlCorrected ? "Corrected" : "Reference", [m](Menu* sub) {
			sub->addChild(createCheckMenuItem("Reference (as published, sign error)", "",
				[m]() { return !m->nlCorrected; },
				[m]() { m->nlCorrected = false; }));
			sub->addChild(createCheckMenuItem("Corrected (sign fix, default)", "",
				[m]() { return m->nlCorrected; },
				[m]() { m->nlCorrected = true; }));
		}));
		menu->addChild(createSubmenuItem("Circuit variant", m->boardRepro ? "Reproduction board" : "Paper", [m](Menu* sub) {
			sub->addChild(createCheckMenuItem("Paper (Parker/D'Angelo values, default)", "",
				[m]() { return !m->boardRepro; },
				[m]() { m->boardRepro = false; }));
			sub->addChild(createCheckMenuItem("Reproduction board (R8 10k, VCA 10k + gain 2; from the schematic of a reproduction)", "",
				[m]() { return m->boardRepro; },
				[m]() { m->boardRepro = true; }));
		}));
		menu->addChild(createSubmenuItem("Gate response (VCA, BOTH)", m->vcaLinear ? "Linear" : "Paper circuit", [m](Menu* sub) {
			sub->addChild(createCheckMenuItem("Linear gate (VCA: gain = ATT + velocity, no filter; BOTH: linear gate, then the filter stage chosen below; default)", "",
				[m]() { return m->vcaLinear; },
				[m]() { m->vcaLinear = true; }));
			sub->addChild(createCheckMenuItem("Paper circuit (vactrol divider model, steep knee at THRESHOLD)", "",
				[m]() { return !m->vcaLinear; },
				[m]() { m->vcaLinear = false; }));
		}));
		menu->addChild(createSubmenuItem("Velocity target", m->velAudio ? "Audio offset" : "Control level", [m](Menu* sub) {
			sub->addChild(createCheckMenuItem("Control level (default)", "",
				[m]() { return !m->velAudio; },
				[m]() { m->velAudio = false; }));
			sub->addChild(createCheckMenuItem("Audio offset", "",
				[m]() { return m->velAudio; },
				[m]() { m->velAudio = true; }));
			sub->addChild(new MenuSeparator);
			sub->addChild(createMenuLabel("Control level: velocity is added to ATT/CV,"));
			sub->addChild(createMenuLabel("result clamped to 0...10 V."));
			sub->addChild(createMenuLabel("Audio offset: velocity is added to the signal"));
			sub->addChild(createMenuLabel("after gate/ATT, only while the gate is open"));
			sub->addChild(createMenuLabel("(full from ATT 0.5 V, none at ATT 0)."));
			sub->addChild(createMenuLabel("Cut at +-10 V, e.g. +-5 V with -4 V -> +1/-9 V,"));
			sub->addChild(createMenuLabel("+-5 V with +7 V -> +10/+2 V."));
		}));
		menu->addChild(createSubmenuItem("BOTH filter stage", m->bothLowpass ? "Lowpass" : "Paper BOTH", [m](Menu* sub) {
			sub->addChild(createCheckMenuItem("Lowpass stage (as in LOWPASS mode, resonance works; default; only with Linear gate response)", "",
				[m]() { return m->bothLowpass; },
				[m]() { m->bothLowpass = true; }));
			sub->addChild(createCheckMenuItem("Paper BOTH stage (hardly filters, no resonance; only with Linear gate response)", "",
				[m]() { return !m->bothLowpass; },
				[m]() { m->bothLowpass = false; }));
		}));
		menu->addChild(createSubmenuItem("Saturation", m->saturation ? "On" : "Off", [m](Menu* sub) {
			sub->addChild(createCheckMenuItem("Off (model as published)", "",
				[m]() { return !m->saturation; },
				[m]() { m->saturation = false; }));
			sub->addChild(createCheckMenuItem("On (soft clip after the gate, intensity via DRIVE)", "",
				[m]() { return m->saturation; },
				[m]() { m->saturation = true; }));
		}));
		menu->addChild(createSubmenuItem("Oversampling", m->oversample ? "2x" : "Off", [m](Menu* sub) {
			sub->addChild(createCheckMenuItem("Off", "",
				[m]() { return !m->oversample; },
				[m]() { m->oversample = false; m->cachedSampleRate = -1.f; }));
			sub->addChild(createCheckMenuItem("2x", "",
				[m]() { return m->oversample; },
				[m]() { m->oversample = true; m->cachedSampleRate = -1.f; }));
		}));
	}
};

} // namespace

Model* modelAnnaMaria = createModel<AnnaMaria, AnnaMariaWidget>("AnnaMaria");
