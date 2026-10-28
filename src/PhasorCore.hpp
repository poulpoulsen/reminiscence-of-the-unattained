#pragma once
// ============================================================================
// PhasorCore.hpp - Phasor-Modus fuer "Europa" (ein Generator)
//
// Rack-frei wie EnvGen4.hpp (nur <cmath>/<algorithm>), standalone testbar.
// Nutzt die Hilfsfunktionen und EnvParams aus EnvGen4.hpp (Zeiten, Kurven,
// Pegel), beruehrt EnvGen4 selbst aber nicht.
//
// Konzept (Kinovice, 2026-10-06):
//   - Eingang ist ein Phasor 0..10 V (T1/GATE-Buchse des Generators).
//   - Fallende Rampe: Eingang wird gespiegelt (xs = 1 - x), fuer die Kette ist
//     es immer eine steigende Rampe -> Rise/Fall (hier: D/A/H/R) laufen immer in
//     Reihenfolge, EOC liegt bei fallender Rampe automatisch bei x = 0.
//   - Entrollter Phasor: Zykluszaehler (mod 720720 = kgV(1..16)) + Ratio p/q:
//         y = frac( ((cycle mod q) + xs) * p / q + offset )
//   - Ratio-Wechsel: nur am Eingangs-Wrap; Standard strikt, optional locker (an
//     jedem Eingangs-Wrap, mit Phasensprung), strikt (cycle mod q_alt == 0 UND
//     cycle mod q_neu == 0), dann ist y vor und nach dem Wechsel exakt 0 plus
//     Offset. Reset (Rising Edge >= 2 V) setzt den Zaehler auf 0 und uebernimmt
//     die ausstehende Ratio sofort.
//   - Die vier Zeiten (Delay/Attack/Hold/Release) sind GEWICHTE: normiert auf
//     100 % teilen sie den Zyklus y in vier Segmente. Pegel und Kurvenform wie
//     in EnvGen4 (Stage s laeuft von Pegel(s-1) nach Pegel(s); Delay startet vom
//     Release-Pegel). Gewicht 0 = Segment entfaellt (Pegelsprung, wird wie in
//     EnvGen4 gemeldet). Sind alle Gewichte 0, gilt Gleichverteilung.
//   - Unerklaerte Spruenge von y (Reset, Offset-Sprung, Richtungswechsel) werden
//     als Huellkurven-Sprung an den minBLEP gemeldet, ohne EOC.
//   - Ausgaenge: envOut (Huellkurve, Volt), phasorLinear / phasorWarped
//     (0..1, RICHTUNGSTREU: bei fallendem Eingang fallend), eocOut.
//       phasorLinear: y (Ratio und Offset angewendet)
//       phasorWarped: (k + curve(u)) / 4 - jedes Segment bekommt ein Viertel,
//                     die Curve des Segments formt den Verlauf darin
// ============================================================================

#include "EnvGen4.hpp"
#include <cmath>
#include <algorithm>

// Eigener ggT: Rack baut mit C++11, std::gcd gibt es erst ab C++17.
inline int phasorGcd(int a, int b) {
	while (b != 0) {
		int t = a % b;
		a = b;
		b = t;
	}
	return a;
}

static const int PHASOR_CYCLE_MOD = 720720;      // kgV(1..16)
static const int PHASOR_RATIO_MAX = 16;
// Hysterese der Richtungserkennung (Phasorschritt pro Sample, 0..1). Kleiner
// als jede sinnvolle Rampe, groesser als Rauschen auf einem stehenden Phasor.
static const float PHASOR_DIR_EPS = 1e-5f;
static const int PHASOR_MAX_JUMPS = 2;

enum PhasorOutMode {
	PHASOR_OUT_ENV = 0,     // ENV OUT liefert die Huellkurve (Standard)
	PHASOR_OUT_LINEAR = 1,  // ENV OUT liefert y (Ratio/Offset)
	PHASOR_OUT_WARPED = 2   // ENV OUT liefert (k + u) / 4
};

struct PhasorCore {
	// --- Ausgaenge (nach process() gueltig) ---
	float envOut = 0.f;         // Huellkurve in Volt
	float phasorLinear = 0.f;   // 0..1, richtungstreu
	float phasorWarped = 0.f;   // 0..1, richtungstreu
	bool eocOut = false;
	bool running = false;       // Eingangsphasor bewegt sich
	bool pending = false;       // Ratio-Aenderung wartet auf gueltigen Wrap
	int jumpCount = 0;
	float jumpPos[PHASOR_MAX_JUMPS] = {};
	float jumpDelta[PHASOR_MAX_JUMPS] = {};

	// --- Zustand ---
	bool init = false;
	float xPrev = 0.f;
	float xsPrev = 0.f;
	float yPrev = 0.f;
	int dir = 1;
	int dirPrev = 1;
	int cycle = 0;
	int p = 1;
	int q = 1;
	int segPrev = 0;
	bool resetHigh = false;
	bool eocPending = false;
	int eocSamplesLeft = 0;
	double samplesSinceWrap = 0.0;
	double lastCycleSamples = 1.0;
	float lastCurve[ENV_NUM_STAGES] = {1e9f, 1e9f, 1e9f, 1e9f};
	float cachedPower[ENV_NUM_STAGES] = {1.f, 1.f, 1.f, 1.f};

	void reset() {
		*this = PhasorCore();
	}

	void addJump(float pos, float delta) {
		if (std::fabs(delta) <= 1e-6f || jumpCount >= PHASOR_MAX_JUMPS)
			return;
		jumpPos[jumpCount] = std::min(std::max(pos, -0.999f), 0.f);
		jumpDelta[jumpCount] = delta;
		jumpCount++;
	}

	// Huellkurve an der Zyklusphase y (Segmente aus den oberen Grenzen b[]).
	// Liefert optional Segment k und lokale Phase u.
	float envAt(float y, const EnvParams& pr, const float* b, int* kOut, float* uOut) {
		int k = 0;
		while (k < ENV_NUM_STAGES - 1 && y >= b[k])
			k++;
		float lo = (k == 0) ? 0.f : b[k - 1];
		float segW = b[k] - lo;
		float u = (segW > 0.f) ? (y - lo) / segW : 0.f;
		u = std::min(std::max(u, 0.f), 1.f);
		const float target[ENV_NUM_STAGES] = {pr.levelDelay, pr.levelAttack, pr.levelHold, pr.levelRelease};
		float from = (k == 0) ? pr.levelRelease : target[k - 1];
		float to = target[k];
		float c = pr.curve[k];
		if (c != lastCurve[k]) {
			cachedPower[k] = envCurvePower(c);
			lastCurve[k] = c;
		}
		float cEff = (to < from) ? -c : c;
		if (kOut) *kOut = k;
		if (uOut) *uOut = u;
		return from + (to - from) * envShapePhase(u, cEff, cachedPower[k]);
	}

	// pr: aufgeloeste Zeiten (= Gewichte, >= 0, NICHT unendlich), Kurven, Pegel
	// ratioP/ratioQ: 1..16 (werden geklemmt und gekuerzt)
	// offset: 0..1 Zyklusanteil
	// phasorVolts: Eingangsphasor 0..10 V; resetVolts: Reset-Eingang (>= 2 V)
	// looseRatio: false = strikte Ratio-Uebernahme (Standard), true = locker
	void process(float dt, const EnvParams& pr, int ratioP, int ratioQ, float offset,
	             float phasorVolts, float resetVolts, bool looseRatio = false) {
		jumpCount = 0;

		ratioP = std::min(std::max(ratioP, 1), PHASOR_RATIO_MAX);
		ratioQ = std::min(std::max(ratioQ, 1), PHASOR_RATIO_MAX);
		int g = phasorGcd(ratioP, ratioQ);
		int pNew = ratioP / g;
		int qNew = ratioQ / g;

		float x = phasorVolts * 0.1f;
		x -= std::floor(x);

		bool first = !init;
		if (first) {
			init = true;
			xPrev = x;
			xsPrev = x;
			p = pNew;
			q = qNew;
		}

		bool resetNow = resetVolts >= ENV_TRIG_THRESHOLD_V;
		bool resetRise = resetNow && !resetHigh;
		resetHigh = resetNow;

		// --- Richtung mit Hysterese ---
		float d = x - xPrev;
		if (d < -0.5f)
			dir = +1;                       // Wrap einer steigenden Rampe
		else if (d > 0.5f)
			dir = -1;                       // Wrap einer fallenden Rampe
		else if (std::fabs(d) > PHASOR_DIR_EPS)
			dir = (d > 0.f) ? +1 : -1;
		// sonst: dir bleibt (Stillstand, Rauschen)
		running = std::fabs(d) > 0.f;

		float xs = (dir > 0) ? x : 1.f - x;

		// --- Eingangs-Wrap (nur Vorwaerts auf dem gespiegelten Signal) ---
		bool inWrap = (dir == dirPrev) && (xs - xsPrev < -0.5f);
		if (inWrap)
			cycle = (cycle + 1) % PHASOR_CYCLE_MOD;

		int pUsed = p;   // Ratio, mit der sich y seit dem letzten Sample bewegt hat
		int qUsed = q;

		// --- Ratio-Uebernahme ---
		if (resetRise) {
			cycle = 0;
			p = pNew;
			q = qNew;
		}
		else if (inWrap && (pNew != p || qNew != q)) {
			// strikt: y liegt vor und nach dem Wechsel exakt auf dem Zyklusanfang.
			// locker: sofort am Eingangs-Wrap; y springt (bleibt aber am globalen
			// Zykluszaehler ausgerichtet) und wird als unerklaerter Sprung gemeldet.
			if (looseRatio || (cycle % q == 0 && cycle % qNew == 0)) {
				p = pNew;
				q = qNew;
			}
		}
		pending = (pNew != p || qNew != q);

		// --- Zyklusphase y ---
		double v = ((double) (cycle % q) + (double) xs) * (double) p / (double) q
			+ (double) std::min(std::max(offset, 0.f), 1.f);
		float y = (float) (v - std::floor(v));
		if (y >= 1.f)
			y = 0.f;

		// --- Segmente aus den Gewichten ---
		float w[ENV_NUM_STAGES];
		float sum = 0.f;
		for (int s = 0; s < ENV_NUM_STAGES; s++) {
			float t = pr.time[s];
			w[s] = (t > 0.f) ? std::min(t, 1e6f) : 0.f;
			sum += w[s];
		}
		if (sum <= 1e-12f) {
			for (int s = 0; s < ENV_NUM_STAGES; s++)
				w[s] = 0.25f;
			sum = 1.f;
		}
		float b[ENV_NUM_STAGES]; // obere Segmentgrenzen, b[3] = 1
		float acc = 0.f;
		for (int s = 0; s < ENV_NUM_STAGES; s++) {
			acc += w[s] / sum;
			b[s] = acc;
		}
		b[ENV_NUM_STAGES - 1] = 1.f;

		int k = 0;
		float u = 0.f;
		envOut = envAt(y, pr, b, &k, &u);
		const float target[ENV_NUM_STAGES] = {pr.levelDelay, pr.levelAttack, pr.levelHold, pr.levelRelease};
		float c = pr.curve[k];

		// --- Phasor-Ausgaenge, richtungstreu zurueckgespiegelt ---
		// Verformter Phasor: die Segment-Curve gilt auch hier (immer steigend,
		// daher ohne Spiegelung wie bei der Huellkurve). Stetig an den Grenzen.
		float warped = (k + envShapePhase(u, c, cachedPower[k])) / (float) ENV_NUM_STAGES;
		phasorLinear = (dir > 0) ? y : 1.f - y;
		phasorWarped = (dir > 0) ? warped : 1.f - warped;

		// --- Unerklaerte Spruenge von y (Reset, Offset-Sprung, Richtungswechsel,
		// Phasor-Sprung am Eingang): Erwartung = y bewegt sich mit der Eingangs-
		// bewegung und der bisherigen Ratio weiter. Weicht y deutlich ab, springt
		// auch die Huellkurve -> an den minBLEP melden (Lage im Sample unbekannt,
		// daher Mitte). Kleine/stetige Aenderungen (Regler, langsame CV) bleiben
		// ausserhalb der Schwelle und werden nicht gemeldet.
		bool mismatch = false;
		if (!first) {
			double dxs = 0.0;   // Richtungswechsel/Reset: keine erwartete Bewegung
			if (dir == dirPrev && !resetRise) {
				dxs = (double) xs - (double) xsPrev;
				if (inWrap)
					dxs += 1.0;
			}
			double ve = (double) yPrev + dxs * (double) pUsed / (double) qUsed;
			double m = (double) y - (ve - std::floor(ve));
			m -= std::floor(m + 0.5);   // kuerzeste Differenz auf dem Kreis
			if (std::fabs(m) > 0.02) {
				mismatch = true;
				float yExp = (float) (ve - std::floor(ve));
				float delta = envOut - envAt(yExp, pr, b, nullptr, nullptr);
				if (std::fabs(delta) > 0.05f)
					addJump(-0.5f, delta);
			}
		}

		// --- y-Wrap (EOC) und Pegelspruenge durch uebersprungene Segmente ---
		// Bei unerklaertem Sprung kein EOC und keine Segment-Logik (siehe oben).
		bool wrapY = !first && !mismatch && (yPrev - y > 0.5f);
		if (!first && !mismatch && (wrapY || k > segPrev)) {
			// Ein Sprung entsteht nur, wenn Segmente uebersprungen wurden
			// (Gewicht 0): Startpegel des neuen Segments minus Endpegel des alten.
			float fromNew = (k == 0) ? pr.levelRelease : target[k - 1];
			float delta = fromNew - target[segPrev];
			float pos = -0.5f;
			if (wrapY) {
				float travel = (1.f - yPrev) + y;
				if (travel > 0.f)
					pos = -(y / travel);
			}
			else {
				float dy = y - yPrev;
				if (dy > 0.f)
					pos = -((y - b[segPrev]) / dy);
			}
			addJump(pos, delta);
		}
		segPrev = k;
		yPrev = y;

		samplesSinceWrap += 1.0;
		if (wrapY) {
			eocPending = true;
			lastCycleSamples = samplesSinceWrap;
			samplesSinceWrap = 0.0;
		}
		bool prevHigh = eocOut;
		if (eocPending) {
			if (!prevHigh) {
				int maxS = std::max(1, (int) std::lround(ENV_EOC_MAX_SECONDS / dt));
				int halfS = (int) std::floor(lastCycleSamples * 0.5 + 1e-6);
				eocSamplesLeft = std::min(std::max(halfS, 1), maxS);
			}
			eocPending = false;
		}
		eocOut = eocSamplesLeft > 0;
		if (eocOut)
			eocSamplesLeft--;

		xPrev = x;
		xsPrev = xs;
		dirPrev = dir;
	}
};
