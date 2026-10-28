// Standalone-Test fuer den Handshake (Quadratur-Modus 2) von EnvGen4 (ohne Rack).
// Kompilieren (aus dem Projektordner): g++ -std=c++17 -I src -o /tmp/test_handshake tests/test_handshake.cpp
#include <cassert>
#include <cstdio>
#include <vector>
#include "EnvGen4.hpp"

static const float DT = 1.f / 48000.f;
static const float NOTRIG[ENV_NUM_STAGES] = {0, 0, 0, 0};

// Nachbildung der Paar-Schleife aus Europa::process(): erst Master (i = 0), dann
// Follower (i = 1), Kopplungsfelder ueber envQuadSetup().
struct Paar {
	EnvGen4 g[2];
	float att[2] = {0.1f, 0.1f}, rel[2] = {0.1f, 0.1f}, hold[2] = {0.05f, 0.05f}, del[2] = {0.f, 0.f};
	int loop[2] = {0, 0};          // 0 = Off, 1 = Gate, 2 = Free
	bool sus[2] = {false, false};
	int mode = ENV_QUAD_HANDSHAKE;
	long n = 0;                    // Sample-Zaehler
	bool lastP0Handshake = false, lastP1Coupled = false;

	void step(float gate1) {
		bool pairSustain = sus[0] || sus[1];
		for (int i = 0; i < 2; i++) {
			EnvParams p;
			p.time[ENV_STAGE_DELAY] = del[i];
			p.time[ENV_STAGE_ATTACK] = att[i];
			p.time[ENV_STAGE_HOLD] = hold[i];
			p.time[ENV_STAGE_RELEASE] = rel[i];
			p.sustainMode = sus[i];
			p.loopEnabled = loop[i] > 0;
			p.freeCycle = loop[i] > 1;
			bool req = envQuadSetup(p, mode, i == 1, pairSustain, g[i], g[1 - i]);
			float gv = p.coupled ? (g[0].gateHigh ? 10.f : 0.f) : (i == 0 ? gate1 : 0.f);
			g[i].process(DT, p, gv, NOTRIG, req);
			if (i == 0) lastP0Handshake = p.handshake;
			else lastP1Coupled = p.coupled;
		}
		n++;
	}
	double t() const { return n * (double) DT; }
};

static bool nahe(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

int main() {
	// 1) Grundablauf bei gleichen Zeiten (0,1 s), Loop aus: ein Viererzyklus.
	//    A: hoch 0-0,1, oben bis 0,2, runter 0,2-0,3. B: hoch 0,1-0,2, oben bis 0,3, runter 0,3-0,4.
	//    Die Hold-Zeit (0,05 s) darf keine Rolle spielen.
	{
		Paar p;
		double aMax = -1, bMax = -1, aMin = -1, bEnd = -1, aRelStart = -1;
		float aAt15 = 0, bAt25 = 0, aAt25 = 0, bAt35 = 0;
		for (int i = 0; i < 30000; i++) {
			p.step(i < 48 ? 10.f : 0.f); // kurzer Gate-Puls (1 ms)
			double t = p.t();
			if (p.g[0].attackEnded && aMax < 0) aMax = t;
			if (p.g[1].attackEnded && bMax < 0) bMax = t;
			if (p.g[0].cycleEnded && aMin < 0) aMin = t;
			if (p.g[1].cycleEnded && bEnd < 0) bEnd = t;
			if (aRelStart < 0 && p.g[0].running && p.g[0].stage == ENV_STAGE_RELEASE) aRelStart = t;
			if (i == 7200) aAt15 = p.g[0].envOut;   // t = 0,15 s
			if (i == 12000) { aAt25 = p.g[0].envOut; bAt25 = p.g[1].envOut; } // 0,25 s
			if (i == 16800) bAt35 = p.g[1].envOut;  // 0,35 s
		}
		std::printf("1: A max %.4f, B max %.4f, A Release ab %.4f, A min %.4f, B Ende %.4f\n", aMax, bMax, aRelStart, aMin, bEnd);
		assert(nahe(aMax, 0.1, 0.001));
		assert(nahe(bMax, 0.2, 0.001));
		assert(nahe(aRelStart, 0.2, 0.001)); // Hold-Zeit ignoriert, A faellt direkt nach B-Maximum
		assert(nahe(aMin, 0.3, 0.001));
		assert(nahe(bEnd, 0.4, 0.001));
		assert(nahe(aAt15, 10.0, 1e-3));  // A haelt oben waehrend B steigt
		assert(nahe(aAt25, 5.0, 0.1));    // A faellt (Mitte)
		assert(nahe(bAt25, 10.0, 1e-3));  // B haelt oben waehrend A faellt
		assert(nahe(bAt35, 5.0, 0.1));    // B faellt (Mitte)
		assert(!p.g[0].running && !p.g[1].running); // Loop aus: nur ein Zyklus
		std::puts("1 ok: Viererzyklus (A haelt, B haelt, A min -> B faellt), Hold-Zeit ohne Wirkung");
	}

	// 2) Loop Gate an Gen 1: Periode = Summe der vier Zeiten (0,4 s), B = A um 0,1 s verschoben
	{
		Paar p;
		p.loop[0] = 1;
		std::vector<float> a, b;
		std::vector<double> aEnden;
		for (int i = 0; i < 96000; i++) { // 2 s, Gate bleibt hoch
			p.step(10.f);
			a.push_back(p.g[0].envOut);
			b.push_back(p.g[1].envOut);
			if (p.g[0].attackEnded) aEnden.push_back(p.t());
		}
		assert(aEnden.size() >= 5);
		for (size_t k = 1; k < aEnden.size(); k++)
			assert(nahe(aEnden[k] - aEnden[k - 1], 0.4, 0.001));
		// B(t) = A(t - 0,1 s) im eingeschwungenen Zustand
		int shift = 4800;
		double maxAbw = 0;
		for (int i = 48000; i < 96000; i++)
			maxAbw = std::max(maxAbw, (double) std::fabs(b[i] - a[i - shift]));
		std::printf("2: %zu Zyklen, Periode 0,400 s, max. Abweichung B zu A(t-0,1 s) = %.3f V\n", aEnden.size(), maxAbw);
		assert(maxAbw < 0.1);
		std::puts("2 ok: Loop-Periode 0,4 s, echte Viertelperiode");
	}

	// 3) Ungleiche Zeiten + Delay: Periode = DelayA + AttA + DelayB + AttB + RelA + RelB = 0,7 s (Loop Free)
	{
		Paar p;
		p.loop[0] = 2;
		p.del[0] = 0.02f; p.att[0] = 0.1f; p.rel[0] = 0.3f;
		p.del[1] = 0.03f; p.att[1] = 0.2f; p.rel[1] = 0.05f;
		std::vector<double> aEnden;
		for (int i = 0; i < 96000; i++) {
			p.step(0.f); // kein Gate noetig
			if (p.g[0].attackEnded) aEnden.push_back(p.t());
		}
		assert(aEnden.size() >= 3);
		for (size_t k = 1; k < aEnden.size(); k++)
			assert(nahe(aEnden[k] - aEnden[k - 1], 0.7, 0.001));
		std::printf("3 ok: Periode %.4f s (erwartet 0,7000 s)\n", aEnden[1] - aEnden[0]);
	}

	// 4) Waehrend des Halten-Zustands bleiben die Pegel exakt am Attack-Pegel
	{
		Paar p;
		p.loop[0] = 2;
		p.att[0] = 0.05f; p.rel[0] = 0.08f; p.att[1] = 0.07f; p.rel[1] = 0.06f;
		for (int i = 0; i < 96000; i++) {
			p.step(0.f);
			if (p.g[0].hsHeld) assert(nahe(p.g[0].envOut, 10.0, 1e-4));
			if (p.g[1].hsHeld) assert(nahe(p.g[1].envOut, 10.0, 1e-4));
			assert(p.g[0].envOut >= -1e-3f && p.g[0].envOut <= 10.001f);
			assert(p.g[1].envOut >= -1e-3f && p.g[1].envOut <= 10.001f);
		}
		std::puts("4 ok: Haltepegel exakt 10 V, Pegel immer im Bereich");
	}

	// 5) Follower im Cycle (Loop Free): laeuft frei, Master wartet auf dessen naechstes Maximum,
	//    Follower haelt waehrend des Master-Decays und zyklt danach weiter.
	{
		Paar p;
		p.loop[1] = 2;                       // B frei laufend
		p.att[1] = 0.02f; p.rel[1] = 0.02f;  // kurzer B-Zyklus 0,04 s
		p.att[0] = 0.1f; p.rel[0] = 0.1f;
		int bZyklenVorher = 0;
		double aMax = -1, aRelStart = -1, aMin = -1;
		bool bHieltWaehrendADecay = true;
		int bZyklenNachher = 0;
		for (int i = 0; i < 48000; i++) {
			bool gate = (i >= 24000 && i < 24048); // A wird bei 0,5 s getriggert
			p.step(gate ? 10.f : 0.f);
			double t = p.t();
			if (t < 0.5 && p.g[1].cycleEnded) bZyklenVorher++;
			if (p.g[0].attackEnded && aMax < 0) aMax = t;
			if (aRelStart < 0 && p.g[0].running && p.g[0].stage == ENV_STAGE_RELEASE) aRelStart = t;
			if (aRelStart > 0 && aMin < 0) {
				if (p.g[0].cycleEnded) aMin = t;
				else if (t > aRelStart + 0.0005 && !nahe(p.g[1].envOut, 10.0, 1e-3)) bHieltWaehrendADecay = false;
			}
			if (aMin > 0 && t > aMin + 0.002 && p.g[1].cycleEnded) bZyklenNachher++;
		}
		std::printf("5: B-Zyklen vor A: %d, A max %.4f, A Release ab %.4f (B-Max danach), A min %.4f, B-Zyklen nach A: %d\n",
		            bZyklenVorher, aMax, aRelStart, aMin, bZyklenNachher);
		assert(bZyklenVorher >= 10);                 // B lief schon vorher frei
		assert(aMax > 0.5 && nahe(aMax, 0.6, 0.002));
		assert(aRelStart > aMax && aRelStart - aMax <= 0.042); // hoechstens ein B-Zyklus warten
		assert(nahe(aMin - aRelStart, 0.1, 0.001));
		assert(bHieltWaehrendADecay);                // B oben waehrend A faellt
		assert(bZyklenNachher >= 5);                 // B zyklt danach weiter
		assert(!p.g[0].running);                     // A Loop aus: Einzelzyklus
		std::puts("5 ok: freilaufender Follower - Master wartet auf B-Maximum, B haelt waehrend A-Decay");
	}

	// 6) Follower im Cycle + Master-Loop (Gate): Master startet nach dem Follower-Zyklusende neu
	{
		Paar p;
		p.loop[0] = 1; p.loop[1] = 2;
		p.att[1] = 0.02f; p.rel[1] = 0.02f;
		int aEnden = 0;
		for (int i = 0; i < 96000; i++) {
			p.step(10.f);
			if (p.g[0].attackEnded) aEnden++;
		}
		std::printf("6 ok: %d Master-Zyklen in 2 s mit freilaufendem Follower\n", aEnden);
		assert(aEnden >= 5);
	}

	// 7) Sustain im Paar: Handshake faellt auf Follow zurueck
	{
		Paar p;
		p.sus[0] = true;
		p.step(10.f);
		assert(!p.lastP0Handshake && p.lastP1Coupled);
		Paar q;
		q.sus[1] = true;
		q.step(10.f);
		assert(!q.lastP0Handshake && q.lastP1Coupled);
		std::puts("7 ok: Sustain in einem der beiden Generatoren -> Follow");
	}

	// 8) Handshake im Halten ausschalten: kein Festhaengen
	{
		Paar p;
		for (int i = 0; i < 6000; i++) p.step(i < 48 ? 10.f : 0.f); // 0,125 s: A haelt, B laeuft Attack
		assert(p.g[0].hsHeld);
		p.mode = ENV_QUAD_FOLLOW;
		for (int i = 0; i < 48000; i++) p.step(0.f);
		assert(!p.g[0].running && !p.g[0].hsHeld);
		std::puts("8 ok: Wechsel auf Follow loest das Halten");
	}

	// 9) Externer Neutrigger des Masters, waehrend Follower (Loop Gate) haelt: keine Blockade
	{
		Paar p;
		p.loop[1] = 1;
		bool bEndeGesehen = false;
		for (int i = 0; i < 96000; i++) {
			double t = i * (double) DT;
			bool gateHigh = true;
			if (t >= 0.25 && t < 0.2502) gateHigh = false; // kurzer Gate-Einbruch -> neue Anstiegsflanke
			p.step(gateHigh ? 10.f : 0.f);
			if (t > 0.3 && p.g[1].cycleEnded) bEndeGesehen = true;
		}
		assert(bEndeGesehen);
		std::puts("9 ok: Neutrigger waehrend Halten - Paar laeuft weiter (keine gegenseitige Blockade)");
	}

	// 10) Alle Zeiten 0, Loop Free: kein Haenger, Pegel im Bereich, Ereignisse laufen
	{
		Paar p;
		p.loop[0] = 2;
		for (int k = 0; k < 2; k++) { p.att[k] = 0.f; p.rel[k] = 0.f; p.hold[k] = 0.f; }
		int aEnden = 0;
		for (int i = 0; i < 4800; i++) {
			p.step(0.f);
			if (p.g[0].attackEnded) aEnden++;
			assert(p.g[0].envOut >= -1e-3f && p.g[0].envOut <= 10.001f);
			assert(p.g[1].envOut >= -1e-3f && p.g[1].envOut <= 10.001f);
		}
		std::printf("10 ok: Zeit 0 - %d Master-Zyklen in 4800 Samples\n", aEnden);
		assert(aEnden > 100);
	}

	// 11) Follow-Modus unveraendert (Dekays unabhaengig): A faellt ohne B-Maximum abzuwarten
	{
		Paar p;
		p.mode = ENV_QUAD_FOLLOW;
		p.att[1] = 0.3f; // B steigt lange
		double aRelStart = -1;
		for (int i = 0; i < 24000; i++) {
			p.step(i < 48 ? 10.f : 0.f);
			if (aRelStart < 0 && p.g[0].running && p.g[0].stage == ENV_STAGE_RELEASE) aRelStart = p.t();
		}
		// Hold 0,05 s nach Attack-Ende (0,1 s) -> Release ab ca. 0,15 s, unabhaengig von B
		assert(nahe(aRelStart, 0.15, 0.002));
		std::puts("11 ok: Follow-Modus unveraendert");
	}

	std::puts("ALLE TESTS BESTANDEN");
}
