#pragma once
// ============================================================================
// EnvGen4.hpp - Engine fuer "Europa" (Quad Function Generator mit VCA)
//
// EIN Hüllkurvengenerator mit den vier Stages Delay / Attack / Hold / Release.
// Das Modul haelt vier Instanzen (vier Generatoren laufen unabhaengig).
//
// Bewusst RACK-FREI (nur <cmath>/<algorithm>): die Engine bekommt bereits
// aufgeloeste Werte (Sekunden, Volt) und liefert Volt zurueck. Dadurch laesst
// sie sich als standalone C++ testen (Schritt 3 im Konzept) und beruehrt
// Afg/Sofonisba nicht (Entscheidung Weg B, Session 2026-09-28).
//
// Entscheidungen dieser Session (Kinovice):
//   - Alle vier Zeitbereiche beginnen bei 0, Obergrenzen wie in Sofonisba
//     (30 ms / 300 ms / 3 s / 30 s), exponentielle Reglerabbildung, exakt 0
//     im Linksanschlag.
//   - Delay-Pegel (Startpegel) und Release-Pegel (Endpegel/Ruhepegel) sind
//     einstellbar, unipolar 0..10 V (Default 0 V = bisheriges Verhalten).
//     Im Stillstand liegt der Ausgang auf dem Release-Pegel; Retrig-Schalter
//     springt auf den Delay-Pegel.
//   - Eigene bipolare Curve-Abbildung (negativ = rund, 0 = linear, positiv =
//     spitz), statt der Sofonisba-Abbildung mit getrennten Exp/Log-Bits.
//     Die Kurve ist RELATIV ZUR BEWEGUNGSRICHTUNG definiert (Kinovice,
//     2026-09-28): bei einer fallenden Stage (Ziel < Startpegel) wird sie
//     gespiegelt. "Rund" heisst dann immer: am oberen Pegelende flach
//     (Attack: schnell hoch, oben abflachend; Release: oben flach startend,
//     zum Ende schneller fallend) - beide links ergeben eine Glockenform.
// ============================================================================

#include <cmath>
#include <algorithm>

static const int ENV_NUM_STAGES = 4;

enum EnvStageIndex {
	ENV_STAGE_DELAY = 0,
	ENV_STAGE_ATTACK,
	ENV_STAGE_HOLD,
	ENV_STAGE_RELEASE
};

// Obergrenze fuer Stage-Uebergaenge pro Sample (Zeit-0-Durchlauf, interner
// Loop). Bei Zeit 0 in allen Stages ergibt das einen Zyklus pro Sample.
static const int ENV_MAX_TRANSITIONS_PER_SAMPLE = 4;

// Sentinel fuer "unendlich halten" (Zeit-CV >= ENV_TIME_INFINITE_VOLTS).
// Bewusst grosser endlicher Wert statt INFINITY: Rack wird mit
// -funsafe-math-optimizations gebaut, Vergleiche mit INFINITY waeren dort
// nicht garantiert.
static const float ENV_TIME_INFINITE = 1e30f;
static const float ENV_TIME_INFINITE_VOLTS = 9.5f;

// Obergrenzen der vier Zeitbereiche TR1..TR4 in Sekunden. Alle Bereiche
// beginnen bei 0 (Kinovice, 2026-09-28). Die Untergrenzen TIME_RANGE_MIN aus
// Sofonisba werden hier NICHT verwendet.
static const float ENV_TIME_RANGE_MAX[4] = {0.03f, 0.3f, 3.f, 30.f};

// Basis der exponentiellen Reglerabbildung (Testkonstante, beim Ausprobieren
// nachstellbar): t = max * (B^x - 1) / (B - 1), x = 0..1. Bei B = 1000 liegt
// die Reglermitte von TR1 bei ca. 1 ms.
static const float ENV_TIME_EXP_BASE = 1000.f;

// Zeit-CV wirkt additiv auf die Reglerposition: 10 V = voller Regelweg.
static const float ENV_TIME_CV_SPAN_VOLTS = 10.f;

// Rate-CV (Kinovice, 2026-10-01): bipolarer Geschwindigkeits-CV, exponentiell.
// Faktor = 2^(Volt * ENV_RATE_OCTAVES_PER_VOLT), 0 V = Faktor 1, positiv
// schneller, negativ langsamer. Die Spannung wird auf +-ENV_RATE_CV_LIMIT_V
// begrenzt (bei 1 Okt/V also 1/32 .. x32). Beide Konstanten sind
// Testwerte und beim Ausprobieren nachstellbar.
static const float ENV_RATE_OCTAVES_PER_VOLT = 1.f;
static const float ENV_RATE_CV_LIMIT_V = 5.f;

// Schwellen: Stage-1-Eingang als Schmitt-Trigger (wie Run-Gate auf Amilcare),
// Stage-2..4-Trigger mit fester 2-V-Schwelle ohne Hysterese (wie die
// Poly-Overrides auf Amilcare).
static const float ENV_GATE_LOW_V  = 0.1f;
static const float ENV_GATE_HIGH_V = 1.f;
static const float ENV_TRIG_THRESHOLD_V = 2.f;

// Anti-Aliasing-Hilfe (2026-10-02): die Engine MELDET echte Pegelspruenge
// (Zeit-0-Stages, Retrig-Sprung), das Glaetten (minBLEP) passiert ausserhalb
// im Modul - die Engine bleibt rack-frei und ihr Ausgang unveraendert.
// Pro Sample hoechstens: 1 Retrig-Sprung + ENV_MAX_TRANSITIONS_PER_SAMPLE.
// Ausnahme: Degenerationsfall alle Zeiten 0 + Loop - die Meldungen sind dort
// unvollstaendig (harmlos: gleiche Position, Spruenge heben sich auf).
static const int ENV_MAX_JUMPS_PER_SAMPLE = 8;

// EOC-Pulslaenge: min(ENV_EOC_MAX_SECONDS, halbe Laenge des letzten Zyklus),
// mindestens 1 Sample.
static const float ENV_EOC_MAX_SECONDS = 1e-3f;

// Curve-Abbildung (Betrag a = |bipolar| in 0..1):
//   power = ENV_CURVE_BASE ^ (a * (CENTER + (EXTREME - CENTER) * a * a))
// gleiche Groessenordnung wie Sofonisba (max. 2.5^3 ~ 15.6), aber 0 = linear.
static const float ENV_CURVE_BASE = 2.5f;
static const float ENV_CURVE_FACTOR_CENTER  = 2.f;
static const float ENV_CURVE_FACTOR_EXTREME = 3.f;

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------

// Reglerposition 0..1 -> Sekunden im Bereich rangeIdx (0..3). Exakt 0 bei x=0.
inline float envTimeFromPosition(float position, int rangeIdx) {
	float x = std::min(std::max(position, 0.f), 1.f);
	if (x <= 0.f)
		return 0.f;
	int r = std::min(std::max(rangeIdx, 0), 3);
	return ENV_TIME_RANGE_MAX[r] * (std::pow(ENV_TIME_EXP_BASE, x) - 1.f) / (ENV_TIME_EXP_BASE - 1.f);
}

// Umkehrfunktion zu envTimeFromPosition(): Sekunden -> Reglerposition 0..1
// (fuer die Texteingabe im Rechtsklick-Feld des Zeit-Reglers).
inline float envPositionFromTime(float seconds, int rangeIdx) {
	int r = std::min(std::max(rangeIdx, 0), 3);
	float t = std::min(std::max(seconds, 0.f), ENV_TIME_RANGE_MAX[r]);
	if (t <= 0.f)
		return 0.f;
	return std::log(1.f + t / ENV_TIME_RANGE_MAX[r] * (ENV_TIME_EXP_BASE - 1.f)) / std::log(ENV_TIME_EXP_BASE);
}

// Regler + additive Zeit-CV -> Sekunden (oder ENV_TIME_INFINITE). Die Summe
// der Position wird auf 0..1 begrenzt (Konzept: Summe >= 0). allowInfinite
// ist im Sustain-Modus false ("Dauerhaftes Halten" dort ignoriert).
inline float envResolveTime(float knob, float cvVolts, int rangeIdx, bool allowInfinite) {
	if (allowInfinite && cvVolts >= ENV_TIME_INFINITE_VOLTS)
		return ENV_TIME_INFINITE;
	return envTimeFromPosition(knob + cvVolts / ENV_TIME_CV_SPAN_VOLTS, rangeIdx);
}

// Rate-CV in Volt -> Geschwindigkeitsfaktor (> 0). 0 V = 1.
inline float envRateFromVolts(float volts) {
	float v = std::min(std::max(volts, -ENV_RATE_CV_LIMIT_V), ENV_RATE_CV_LIMIT_V);
	return std::exp2(v * ENV_RATE_OCTAVES_PER_VOLT);
}

// Bipolarer Curve-Wert (-1..1) -> Kruemmungs-Exponent >= 1.
inline float envCurvePower(float bipolar) {
	float a = std::min(std::fabs(bipolar), 1.f);
	float factor = ENV_CURVE_FACTOR_CENTER
		+ (ENV_CURVE_FACTOR_EXTREME - ENV_CURVE_FACTOR_CENTER) * a * a;
	return std::pow(ENV_CURVE_BASE, a * factor);
}

// Wendet die Kurvenform auf den linearen Phasenwert t (0..1) an. Der
// Aufrufer uebergibt bei fallenden Stages das gespiegelte Vorzeichen (siehe
// EnvGen4::process()), hier gilt daher die Sicht "steigend":
//   bipolar < 0: rund  (schneller Start, sanftes Ziel)  f = 1 - (1-t)^p
//   bipolar > 0: spitz (sanfter Start, schnelles Ziel)  f = t^p
inline float envShapePhase(float t, float bipolar, float power) {
	if (bipolar < 0.f)
		return 1.f - std::pow(1.f - t, power);
	if (bipolar > 0.f)
		return std::pow(t, power);
	return t;
}

// ---------------------------------------------------------------------------
// Parameter (pro Sample uebergeben, bereits aufgeloest)
// ---------------------------------------------------------------------------
struct EnvParams {
	// Zeit je Stage in Sekunden (>= 0; >= ENV_TIME_INFINITE = unendlich).
	float time[ENV_NUM_STAGES] = {0.f, 0.f, 0.f, 0.f};
	// Bipolare Kurvenform je Stage (-1 rund .. 0 linear .. +1 spitz, relativ zur Bewegungsrichtung).
	float curve[ENV_NUM_STAGES] = {0.f, 0.f, 0.f, 0.f};
	// Zielpegel in Volt (0..10 V). Delay-Pegel = Startpegel des Zyklus
	// (Sprungziel bei Retrig-Schalter), Release-Pegel = Endpegel und
	// Ruhepegel im Stillstand.
	float levelDelay = 0.f;
	float levelAttack = 10.f;
	float levelHold = 10.f;
	float levelRelease = 0.f;
	bool sustainMode = false;
	bool loopEnabled = false;
	// Freier Zyklus (Auto-Cycle): der Generator startet im One-Shot-Modus von
	// selbst und laeuft ohne Gate dauerhaft im Loop. Setzt loopEnabled voraus
	// (der Aufrufer setzt beide Flags gemeinsam).
	bool freeCycle = false;
	bool retrigZero = false;
	// Geschwindigkeitsfaktor (Rate-CV, siehe envRateFromVolts()). Er skaliert
	// nur den Zeitfortschritt innerhalb der Stages; weil die Phase als
	// elapsed / Stage-Zeit berechnet wird, bleibt die Kurvenform bei jeder
	// Rate erhalten (auch bei Aenderung mitten in einer Stage). EOC-Laenge und
	// Zyklusmessung laufen weiter in echter Zeit. Auf unendliches Halten und
	// Sustain-Halten hat er keine Wirkung.
	float rate = 1.f;
	// Quadratur-Kopplung (Follower-Modus, Kinovice 2026-10-01): wird NUR fuer
	// Generator 2 bzw. 4 gesetzt. Die Gate-Flanke startet dann keinen Zyklus
	// mehr (der Start kommt ueber den startRequest-Parameter von process(),
	// ausgeloest vom Attack-Ende des Partnergenerators). Der Gate-Pegel selbst
	// bleibt wirksam (Sustain-Halten, Release bei Gate low, Loop bei Gate).
	bool coupled = false;

	// Handshake (Quadratur-Modus 2, Kinovice 2026-10-05, Vorbild: klassischer Quad-Funktionsgenerator):
	// beide Generatoren eines Paars warten am Attack-Ende aufeinander. Alle hs*-
	// Felder setzt envQuadSetup() (unten), nicht der Aufrufer von Hand.
	//   handshake           - dieser Generator nimmt am Handshake teil
	//   hsMaster            - Generator 1/3 (Gegenstueck: Follower 2/4, coupled)
	//   hsPartnerWaiting    - Follower: der Master haelt gerade (wartet auf ihn)
	//   hsPartnerRunning    - Master: der Follower laeuft noch
	//   hsReleaseEvent      - Master: Attack-Ende des Followers (Vorsample),
	//                         Follower: Zyklusende des Masters (selbes Sample)
	//   hsPartnerCycleEnded - Master: Zyklusende des Followers (Vorsample)
	bool handshake = false;
	bool hsMaster = false;
	bool hsPartnerWaiting = false;
	bool hsPartnerRunning = false;
	bool hsReleaseEvent = false;
	bool hsPartnerCycleEnded = false;
};

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------
struct EnvGen4 {
	// --- Ausgaenge (nach process() gueltig) ---
	float envOut = 0.f;        // Huellkurve in Volt
	bool eocOut = false;       // EOC-Puls (Gate-Zustand dieses Samples)
	bool running = false;      // Zyklus aktiv (auch waehrend Halten)
	bool held = false;         // Sustain-Halten oder unendliche Stage-Zeit
	// Attack ist in diesem Sample abgelaufen (nur ein Sample lang gueltig).
	// Das ist das Startereignis fuer die Quadratur-Kopplung des Partners.
	bool attackEnded = false;
	// Zyklusende (Release abgelaufen) in diesem Sample, ebenfalls nur ein
	// Sample lang gueltig (Handshake: Minimum erreicht).
	bool cycleEnded = false;
	// Handshake-Halten: Generator steht am Attack-Pegel und wartet auf das
	// Partner-Ereignis (nutzt intern dasselbe Halten wie der Sustain-Modus).
	bool hsHeld = false;
	int stage = ENV_STAGE_DELAY;

	// Gemeldete Pegelspruenge dieses Samples (nur gueltig bis zum naechsten
	// process()). jumpDelta = Pegel nach dem Sprung minus Pegel davor (Volt),
	// jumpPos = Zeitpunkt in (-1, 0] relativ zum aktuellen Sample (0 = genau
	// auf dem Sample, -0.5 = Gate-/Trigger-Ereignis, Position unbekannt).
	// Stetige Verlaeufe (auch sehr schnelle Rampen) werden NICHT gemeldet.
	int jumpCount = 0;
	float jumpPos[ENV_MAX_JUMPS_PER_SAMPLE] = {};
	float jumpDelta[ENV_MAX_JUMPS_PER_SAMPLE] = {};

	// --- Zustand ---
	bool sustainHeld = false;
	double elapsed = 0.0;
	float voltageFrom = 0.f;
	float voltageOut = 0.f;
	bool gateHigh = false;
	bool trigHigh[ENV_NUM_STAGES] = {false, false, false, false};

	// EOC. Die Pulslaenge haengt an der Laenge des zuletzt abgelaufenen
	// ZYKLUS (Start bis Ende), nicht an der Zeit seit dem letzten gefeuerten
	// EOC - sonst wuerde ein verschluckter Puls die gemessene Periode
	// verdoppeln und den Puls dauerhaft zu lang halten.
	bool eocPending = false;
	int eocSamplesLeft = 0;
	double cycleTime = 0.0;
	double lastCycleSeconds = 1.0;

	// Curve-Cache: power nur bei Aenderung neu berechnen (pow ist teuer)
	float lastCurve[ENV_NUM_STAGES] = {1e9f, 1e9f, 1e9f, 1e9f};
	float cachedPower[ENV_NUM_STAGES] = {1.f, 1.f, 1.f, 1.f};

	// Flag: in diesem Sample wurde ein Zyklus/Sprung gestartet -> dt nicht
	// noch einmal aufaddieren (Start beginnt bei elapsed = 0).
	bool startedThisSample = false;

	// Zeitpunkt (in (-1, 0]) des gerade abgearbeiteten Stage-Endes, wird in
	// process() vor jedem finishStage() gesetzt (nur fuer die Sprungmeldung).
	float eventPos = 0.f;

	void addJump(float pos, float delta) {
		if (std::fabs(delta) <= 1e-6f || jumpCount >= ENV_MAX_JUMPS_PER_SAMPLE)
			return;
		jumpPos[jumpCount] = pos;
		jumpDelta[jumpCount] = delta;
		jumpCount++;
	}

	void reset() {
		*this = EnvGen4();
	}

	static float stageTarget(int s, const EnvParams& p) {
		if (s == ENV_STAGE_DELAY) return p.levelDelay;
		if (s == ENV_STAGE_ATTACK) return p.levelAttack;
		if (s == ENV_STAGE_HOLD) return p.levelHold;
		return p.levelRelease; // Release
	}

	// Startet einen neuen Zyklus bei Stage 1 (Delay), Retrigger inklusive.
	void startCycle(const EnvParams& p) {
		if (p.retrigZero) {
			addJump(-0.5f, p.levelDelay - voltageOut); // Sprung melden (Anti-Aliasing)
			voltageOut = p.levelDelay; // Retrig-Schalter: Ausgang springt auf den Delay-Pegel
		}
		voltageFrom = voltageOut;
		stage = ENV_STAGE_DELAY;
		elapsed = 0.0;
		cycleTime = 0.0;
		running = true;
		sustainHeld = false;
		hsHeld = false;
		startedThisSample = true;
	}

	// Sprung in Stage s (Trigger auf Stage 2..4): immer vom aktuellen Pegel.
	void jumpTo(int s) {
		if (!running)
			cycleTime = 0.0;
		voltageFrom = voltageOut;
		stage = s;
		elapsed = 0.0;
		running = true;
		sustainHeld = false;
		hsHeld = false;
		startedThisSample = true;
	}

	void startRelease() {
		jumpTo(ENV_STAGE_RELEASE);
	}

	// Aktuelle Stage ist abgelaufen (elapsed wurde bereits um die Stage-Zeit
	// vermindert, der Rest wird in die naechste Stage uebernommen).
	// Rueckgabe true: Schleife darf weiterlaufen; false: gestoppt/haltend.
	bool finishStage(const EnvParams& p) {
		float t = p.time[stage];
		if (stage == ENV_STAGE_ATTACK)
			attackEnded = true; // Quadratur-Startereignis (auch bei Sustain-Halten)
		// Delay mit Zeit 0 aendert den Pegel nicht (klickfreier Retrigger,
		// der Retrig-Schalter erzwingt den Delay-Pegel bereits in
		// startCycle()). Alle anderen Stages haben ihr Ziel erreicht.
		if (!(stage == ENV_STAGE_DELAY && t <= 0.f)) {
			// Eine Stage mit Zeit 0 erreicht ihr Ziel schlagartig: echter Sprung
			// (eine Stage mit Zeit > 0 endet stetig und meldet nichts).
			if (t <= 0.f)
				addJump(eventPos, stageTarget(stage, p) - voltageFrom);
			voltageFrom = stageTarget(stage, p);
		}

		// Sustain: am Ende von Attack (bzw. Hold nach Sprung) bei Gate high halten.
		if (p.sustainMode && gateHigh && (stage == ENV_STAGE_ATTACK || stage == ENV_STAGE_HOLD)) {
			sustainHeld = true;
			voltageOut = stageTarget(stage, p);
			voltageFrom = voltageOut;
			elapsed = 0.0;
			return false;
		}

		// Handshake: am Attack-Ende bleibt der Generator oben stehen, bis
		// das Partner-Ereignis kommt (Master: Attack-Ende des Followers,
		// Follower: Minimum des Masters). Die Hold-Stage entfaellt dabei, wie
		// beim Original. Ein Follower, dessen Master nicht wartet (freilaufender
		// Follower), fuehrt den Zyklus ohne Halten direkt in den Release.
		if (p.handshake && stage == ENV_STAGE_ATTACK) {
			if (p.hsMaster || p.hsPartnerWaiting) {
				sustainHeld = true;
				hsHeld = true;
				voltageOut = stageTarget(stage, p);
				voltageFrom = voltageOut;
				elapsed = 0.0;
				return false;
			}
			stage = ENV_STAGE_RELEASE;
			return true;
		}

		if (stage < ENV_STAGE_RELEASE) {
			stage++;
			return true;
		}

		// Ende von Release = Zyklusende
		eocPending = true;
		cycleEnded = true;
		lastCycleSeconds = cycleTime;
		cycleTime = 0.0;
		// Loop: bei gehaltenem Gate, im freien Zyklus (Auto-Cycle) auch ohne Gate
		// (Handshake-Master: kein interner Loop, der Neustart kommt vom Ende
		// des Followers.)
		if (!p.sustainMode && p.loopEnabled && (gateHigh || p.freeCycle) && !p.hsMaster) {
			stage = ENV_STAGE_DELAY; // interner Loop, Zeitrest bleibt
			return true;
		}
		running = false;
		elapsed = 0.0;
		voltageOut = p.levelRelease; // Ruhepegel
		voltageFrom = p.levelRelease;
		return false;
	}

	// gateVolts: Stage-1-Eingang, trigVolts[1..3]: Stage-2..4-Trigger
	// (trigVolts[0] wird ignoriert).
	// startRequest: nur im Follower-Modus (p.coupled) wirksam - startet einen
	// Zyklus wie eine Gate-Flanke (Quadratur: Attack-Ende des Partners).
	void process(float dt, const EnvParams& p, float gateVolts, const float trigVolts[ENV_NUM_STAGES],
	             bool startRequest = false) {
		startedThisSample = false;
		attackEnded = false;
		cycleEnded = false;
		jumpCount = 0;
		if (running)
			cycleTime += dt;

		// --- Eingaenge ---
		bool prevGate = gateHigh;
		if (gateVolts >= ENV_GATE_HIGH_V)
			gateHigh = true;
		else if (gateVolts <= ENV_GATE_LOW_V)
			gateHigh = false;
		bool gateRise = gateHigh && !prevGate;
		bool gateFall = !gateHigh && prevGate;

		bool trigRise[ENV_NUM_STAGES] = {false, false, false, false};
		for (int i = 1; i < ENV_NUM_STAGES; i++) {
			bool h = trigVolts[i] >= ENV_TRIG_THRESHOLD_V;
			trigRise[i] = h && !trigHigh[i];
			trigHigh[i] = h;
		}

		// Moduswechsel waehrend des Haltens: Halten aufheben und die Stage
		// sofort als abgelaufen behandeln (elapsed = Stage-Zeit), damit die
		// Schleife unten regulaer zur Folge-Stage weitergeht.
		// (Handshake-Halten endet, wenn der Handshake ausgeschaltet wird -
		// sonst koennte ein verwaister Partner den Generator ewig festhalten.)
		if (sustainHeld && ((hsHeld && !p.handshake) || (!hsHeld && !p.sustainMode))) {
			sustainHeld = false;
			hsHeld = false;
			elapsed = p.time[stage];
		}

		// --- Gate / Trigger ---
		if (gateRise) {
			if (!p.coupled)
				startCycle(p); // im Follower-Modus startet nur startRequest
		}
		else if (gateFall && p.sustainMode && running) {
			if (stage == ENV_STAGE_DELAY && std::fabs(voltageOut - p.levelRelease) <= 1e-6f) {
				// Gate low waehrend Delay noch auf dem Ruhepegel: Abbruch
				// ohne Release/EOC
				running = false;
				sustainHeld = false;
				elapsed = 0.0;
			}
			else if (stage != ENV_STAGE_RELEASE) {
				startRelease(); // Release vom aktuellen Pegel
			}
		}
		for (int i = 1; i < ENV_NUM_STAGES; i++) {
			if (trigRise[i] && (!p.sustainMode || gateHigh))
				jumpTo(i);
		}
		if (startRequest && p.coupled)
			startCycle(p);

		// Handshake: Partner-Ereignisse. Release beendet das Halten; der
		// Master startet neu, wenn der Follower seinen Zyklus beendet hat
		// (entspricht dem Kabel \"Pulse B -> Trigger A\" am Original).
		if (p.handshake) {
			if (hsHeld && p.hsReleaseEvent)
				startRelease();
			if (p.hsMaster && p.hsPartnerCycleEnded && !running && p.loopEnabled
				&& !p.sustainMode && (gateHigh || p.freeCycle))
				startCycle(p);
		}

		// Freier Zyklus (Auto-Cycle): steht der Generator im One-Shot-Modus
		// still, startet er von selbst. startCycle() setzt startedThisSample,
		// der Zeitfortschritt unten addiert in diesem Sample also kein dt.
		// Im Sustain-Modus bleibt das wirkungslos (Gate-gesteuert).
		if (p.freeCycle && p.loopEnabled && !p.sustainMode && !running
			&& !(p.hsMaster && p.hsPartnerRunning))
			startCycle(p);

		// --- Zeitfortschritt mit Zeitrest-Uebernahme ---
		int guard = 0;
		if (running && !sustainHeld) {
			if (!startedThisSample)
				elapsed += (double) dt * p.rate;
			while (running && !sustainHeld && guard < ENV_MAX_TRANSITIONS_PER_SAMPLE) {
				float t = p.time[stage];
				if (t >= ENV_TIME_INFINITE)
					break; // unendlich halten, Ende nur per Trigger
				if (elapsed < t)
					break;
				elapsed -= t;
				guard++;
				// Zeitpunkt des Stage-Endes: der Zeitrest (in Stage-Sekunden) liegt
				// so viele Samples zurueck. Direkt nach einem Start (Gate/Trigger)
				// ist der Zeitpunkt unbekannt -> -0.5.
				if (startedThisSample)
					eventPos = -0.5f;
				else
					eventPos = -std::min((float) (elapsed / ((double) dt * p.rate)), 0.999f);
				if (!finishStage(p))
					break;
			}
			// Nur im Degenerationsfall (alle Zeiten 0): Rest verwerfen, sonst
			// wuchse elapsed unbegrenzt.
			if (guard >= ENV_MAX_TRANSITIONS_PER_SAMPLE && running && !sustainHeld
				&& p.time[stage] < ENV_TIME_INFINITE && elapsed >= p.time[stage])
				elapsed = 0.0;
		}

		// --- Ausgangspegel ---
		held = false;
		if (running) {
			float t = p.time[stage];
			float target = stageTarget(stage, p);
			if (sustainHeld) {
				voltageOut = target;
				held = true;
			}
			else if (t >= ENV_TIME_INFINITE) {
				voltageOut = voltageFrom;
				held = true;
			}
			else if (t <= 0.f) {
				voltageOut = target;
			}
			else {
				float phase = (float) std::min(std::max(elapsed / t, 0.0), 1.0);
				float c = p.curve[stage];
				if (c != lastCurve[stage]) {
					cachedPower[stage] = envCurvePower(c); // haengt nur von |c| ab
					lastCurve[stage] = c;
				}
				// Fallende Stage: Vorzeichen spiegeln, damit "rund" immer
				// "oben flach" bedeutet (siehe Kopfkommentar).
				float cEff = (target < voltageFrom) ? -c : c;
				float shaped = envShapePhase(phase, cEff, cachedPower[stage]);
				voltageOut = voltageFrom + (target - voltageFrom) * shaped;
			}
		}
		else {
			// Stillstand: Ausgang folgt dem Release-Pegel (Regler wirkt live)
			voltageOut = p.levelRelease;
			voltageFrom = p.levelRelease;
		}
		envOut = voltageOut;

		// --- EOC: adaptive Pulslaenge, mind. 1 Sample Pause zwischen Pulsen ---
		bool prevHigh = eocOut;
		if (eocPending) {
			if (!prevHigh) {
				int maxS = std::max(1, (int) std::lround(ENV_EOC_MAX_SECONDS / dt));
				int halfS = (int) std::floor(lastCycleSeconds / dt * 0.5 + 1e-6);
				eocSamplesLeft = std::min(std::max(halfS, 1), maxS);
			}
			eocPending = false; // Event waehrend eines laufenden Pulses verschmilzt
		}
		eocOut = eocSamplesLeft > 0;
		if (eocOut)
			eocSamplesLeft--;
	}
};

// ---------------------------------------------------------------------------
// Quadratur-Kopplung eines Generatorpaars (Gen 1+2 bzw. 3+4)
// ---------------------------------------------------------------------------
enum EnvQuadMode {
	ENV_QUAD_OFF = 0,       // Generatoren unabhaengig
	ENV_QUAD_FOLLOW = 1,    // Follower startet am Attack-Ende des Masters (2026-10-01)
	ENV_QUAD_HANDSHAKE = 2  // Handshake: gekoppelte Decays (2026-10-05)
};

// Setzt die Kopplungsfelder von p fuer einen Generator des Paars und liefert
// den startRequest fuer EnvGen4::process() (nur Follower). Aufruf in der
// Reihenfolge Master, dann Follower (wie in Europa::process()).
//   isFollower  - Generator 2/4 (true) oder Generator 1/3 (false)
//   pairSustain - einer der beiden Generatoren steht im Sustain-Modus: der
//                 Handshake ist dort nicht definiert, es gilt FOLLOW
//   self/partner - Engines des Generators und seines Partners
// Muss NACH dem Setzen von p.loopEnabled/p.freeCycle aufgerufen werden.
inline bool envQuadSetup(EnvParams& p, int quadMode, bool isFollower, bool pairSustain,
                         const EnvGen4& self, const EnvGen4& partner) {
	p.coupled = false;
	p.handshake = false;
	p.hsMaster = false;
	p.hsPartnerWaiting = false;
	p.hsPartnerRunning = false;
	p.hsReleaseEvent = false;
	p.hsPartnerCycleEnded = false;
	if (quadMode <= ENV_QUAD_OFF)
		return false;

	if (quadMode == ENV_QUAD_FOLLOW || pairSustain) {
		if (!isFollower)
			return false;
		p.coupled = true;
		p.freeCycle = false; // FREE verhaelt sich im Follow-Modus wie GATE
		return partner.attackEnded;
	}

	p.handshake = true;
	if (isFollower) {
		p.coupled = true;
		p.hsPartnerWaiting = partner.hsHeld;
		p.hsReleaseEvent = partner.cycleEnded;
		// Ein loopender Follower laeuft frei und wird vom Attack-Ende des
		// Masters nicht neu gestartet - es sei denn, er wartet selbst noch
		// (Master wurde extern neu getriggert), sonst gaebe es eine
		// gegenseitige Blockade.
		return partner.attackEnded && !(p.loopEnabled && self.running && !self.hsHeld);
	}
	p.hsMaster = true;
	p.hsReleaseEvent = partner.attackEnded;      // Vorsample: Partner laeuft nach dem Master
	p.hsPartnerCycleEnded = partner.cycleEnded;  // Vorsample, s.o.
	p.hsPartnerRunning = partner.running;
	return false;
}
