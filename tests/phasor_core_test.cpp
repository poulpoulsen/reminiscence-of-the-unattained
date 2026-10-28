// Standalone-Test fuer PhasorCore.hpp
// Bauen: g++ -std=c++17 -O1 -I.. -o phasor_core_test phasor_core_test.cpp && ./phasor_core_test
// (EnvGen4.hpp muss im Include-Pfad liegen)
#include "PhasorCore.hpp"
#include <cstdio>
#include <vector>

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
static bool near(float a, float b, float tol = 0.02f) { return std::fabs(a - b) <= tol; }

static EnvParams defaultParams() {
	EnvParams p;
	for (int s = 0; s < 4; s++) { p.time[s] = 1.f; p.curve[s] = 0.f; }
	p.levelDelay = 0.f; p.levelAttack = 10.f; p.levelHold = 10.f; p.levelRelease = 0.f;
	return p;
}

// Phasor mit N Samples pro Eingangszyklus; dirSign +1 steigend / -1 fallend
struct Gen {
	int n; int dirSign; long i = 0;
	Gen(int n_, int d) : n(n_), dirSign(d) {}
	float volts() {
		float ph = (float) (i % n) / (float) n;
		i++;
		return (dirSign > 0 ? ph : 1.f - ph) * 10.f;
	}
};

int main() {
	const float dt = 1.f / 48000.f;
	const int N = 1000;

	// 1) 1/1 steigend: y = x, Huellkurve D/A/H/R gleich lang
	{
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
		int eoc = 0; bool prev = false;
		float e125 = -1, e375 = -1, e625 = -1, e875 = -1;
		for (int i = 0; i < 4 * N; i++) {
			c.process(dt, p, 1, 1, 0.f, g.volts(), 0.f);
			if (c.eocOut && !prev) eoc++;
			prev = c.eocOut;
			if (i == 3 * N + 125) e125 = c.envOut;
			if (i == 3 * N + 375) e375 = c.envOut;
			if (i == 3 * N + 625) e625 = c.envOut;
			if (i == 3 * N + 875) e875 = c.envOut;
		}
		CHECK(eoc == 3, "1/1 EOC-Zahl %d (erwartet 3)", eoc);
		CHECK(near(e125, 0.f, 0.1f), "Delay %.3f", e125);
		CHECK(near(e375, 5.f, 0.1f), "Attack Mitte %.3f", e375);
		CHECK(near(e625, 10.f, 0.1f), "Hold %.3f", e625);
		CHECK(near(e875, 5.f, 0.1f), "Release Mitte %.3f", e875);
	}

	// 2) 2/1: zwei y-Zyklen pro Eingangszyklus
	{
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
		int eoc = 0; bool prev = false;
		for (int i = 0; i < 4 * N; i++) {
			c.process(dt, p, 2, 1, 0.f, g.volts(), 0.f);
			if (c.eocOut && !prev) eoc++;
			prev = c.eocOut;
		}
		CHECK(eoc >= 6 && eoc <= 8, "2/1 EOC-Zahl %d", eoc);
	}

	// 3) 1/2: ein y-Zyklus ueber zwei Eingangszyklen
	{
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
		int eoc = 0; bool prev = false;
		for (int i = 0; i < 8 * N; i++) {
			c.process(dt, p, 1, 2, 0.f, g.volts(), 0.f);
			if (c.eocOut && !prev) eoc++;
			prev = c.eocOut;
		}
		CHECK(eoc >= 3 && eoc <= 4, "1/2 EOC-Zahl %d (erwartet 3-4)", eoc);
	}

	// 4) Fallende Rampe: Rise vor Fall, Phasor-Out richtungstreu
	{
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, -1);
		float eAtk = -1, lin = -1;
		for (int i = 0; i < 3 * N; i++) {
			float v = g.volts();
			c.process(dt, p, 1, 1, 0.f, v, 0.f);
			if (i == 2 * N + 125) { // x = 0.875 -> xs = 0.125 (Delay)
				CHECK(near(c.envOut, 0.f, 0.1f), "fallend Delay %.3f", c.envOut);
			}
			if (i == 2 * N + 375) { // x = 0.625 -> xs = 0.375 (Attack Mitte)
				eAtk = c.envOut;
				lin = c.phasorLinear;
			}
		}
		CHECK(near(eAtk, 5.f, 0.15f), "fallend Attack Mitte %.3f", eAtk);
		CHECK(near(lin, 0.625f, 0.01f), "Phasor-Out richtungstreu %.3f (erwartet 0.625)", lin);
	}

	// 5) Ratio-Wechsel strikt: 1/1 -> 3/2 wartet bis cycle%2==0 (Beide q: 1 und 2)
	{
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
		int ratioP = 1, ratioQ = 1;
		bool sawPending = false; bool applied = false;
		float yPrev = 0.f; int bigJumps = 0;
		for (int i = 0; i < 6 * N; i++) {
			if (i == N + 10) { ratioP = 3; ratioQ = 2; } // mitten im Zyklus 1
			c.process(dt, p, ratioP, ratioQ, 0.f, g.volts(), 0.f);
			if (i > N + 10 && c.pending) sawPending = true;
			if (sawPending && !c.pending) applied = true;
			// y darf beim Uebernehmen nicht springen (nur normale Wraps >0.5)
			float y = c.phasorLinear;
			if (i > 0 && std::fabs(y - yPrev) > 0.5f && std::fabs(y - yPrev) < 0.99f) bigJumps++;
			yPrev = y;
		}
		CHECK(sawPending, "Aenderung wurde nicht als ausstehend gemeldet");
		CHECK(applied, "Aenderung wurde nie uebernommen");
		CHECK(c.p == 3 && c.q == 2, "Ratio %d/%d (erwartet 3/2)", c.p, c.q);
	}

	// 6) Gewicht 0: Delay und Hold entfallen, Hold-Pegel 7 V -> Sprung Attack->Release
	{
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
		p.time[0] = 0.f; p.time[1] = 1.f; p.time[2] = 0.f; p.time[3] = 1.f;
		p.levelHold = 7.f;
		float sumJ = 0.f; int jc = 0;
		for (int i = 0; i < 3 * N; i++) {
			c.process(dt, p, 1, 1, 0.f, g.volts(), 0.f);
			for (int j = 0; j < c.jumpCount; j++) { sumJ += c.jumpDelta[j]; jc++; }
		}
		// Pro Zyklus genau ein Sprung (Attack-Ende 10 V -> Release-Start 7 V = -3 V)
		CHECK(jc >= 2 && jc <= 3, "Sprunganzahl %d", jc);
		CHECK(near(sumJ / jc, -3.f, 0.05f), "mittlerer Sprung %.3f", sumJ / (jc ? jc : 1));
	}

	// 7) Reset setzt Zaehler und uebernimmt Ratio sofort
	{
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
		for (int i = 0; i < 3 * N + 100; i++)
			c.process(dt, p, 1, 3, 0.f, g.volts(), 0.f);
		int before = c.cycle;
		c.process(dt, p, 2, 5, 0.f, g.volts(), 10.f); // Reset-Flanke, neue Ratio
		CHECK(before != 0 && c.cycle == 0, "Reset: cycle %d -> %d", before, c.cycle);
		CHECK(c.p == 2 && c.q == 5, "Reset: Ratio %d/%d", c.p, c.q);
	}

	// 8) Richtungswechsel erzeugt keinen falschen EOC
	{
		PhasorCore c; EnvParams p = defaultParams();
		// steigend bis 0.5, dann fallend bis 0.2: kein Wrap, kein EOC
		int eoc = 0; bool prev = false;
		for (int i = 0; i <= 500; i++) { c.process(dt, p, 1, 1, 0.f, i * 0.01f, 0.f); if (c.eocOut && !prev) eoc++; prev = c.eocOut; }
		for (int i = 0; i <= 300; i++) { c.process(dt, p, 1, 1, 0.f, (5.f - i * 0.01f), 0.f); if (c.eocOut && !prev) eoc++; prev = c.eocOut; }
		CHECK(eoc == 0, "Richtungswechsel: %d falsche EOC", eoc);
	}

	// 9) Alle Gewichte 0: Gleichverteilung statt Division durch 0
	{
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
		for (int s = 0; s < 4; s++) p.time[s] = 0.f;
		for (int i = 0; i < 2 * N; i++) {
			c.process(dt, p, 1, 1, 0.f, g.volts(), 0.f);
			CHECK(std::isfinite(c.envOut), "NaN bei Gewichten 0");
		}
	}

	// 10) Curve wirkt auf den verformten Phasor (spitz: unter linear, rund: darueber)
	{
		for (int sgn = -1; sgn <= 1; sgn += 2) {
			PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
			p.curve[1] = 0.8f * sgn;
			float w = -1.f, lin = -1.f;
			for (int i = 0; i < 2 * N; i++) {
				c.process(dt, p, 1, 1, 0.f, g.volts(), 0.f);
				if (i == N + 375) { w = c.phasorWarped; lin = c.phasorLinear; }
			}
			// Attack-Mitte linear: warped = 0.25 + 0.25 * 0.5 = 0.375
			if (sgn > 0) CHECK(w < 0.375f - 0.01f, "spitz: warped %.3f", w);
			else CHECK(w > 0.375f + 0.01f, "rund: warped %.3f", w);
			CHECK(near(lin, 0.375f, 0.01f), "linear unbeeinflusst %.3f", lin);
		}
		// Stetigkeit an den Segmentgrenzen mit Curve
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
		for (int s = 0; s < 4; s++) p.curve[s] = 0.7f;
		float prev = 0.f; int jumps = 0;
		for (int i = 0; i < 3 * N; i++) {
			c.process(dt, p, 1, 1, 0.f, g.volts(), 0.f);
			if (i > 0 && std::fabs(c.phasorWarped - prev) > 0.05f && std::fabs(c.phasorWarped - prev) < 0.9f) jumps++;
			prev = c.phasorWarped;
		}
		CHECK(jumps == 0, "warped unstetig: %d Spruenge", jumps);
	}

	// 11) Unerklaerte Spruenge werden gemeldet (minBLEP), ohne falschen EOC
	{
		// Reset mitten im Zyklus bei Ratio 1/2 (cycle = 1): y 0.6875 (Hold, 10 V) -> 0.1875 (Delay, 0 V)
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
		for (int i = 0; i < N + 375; i++)
			c.process(dt, p, 1, 2, 0.f, g.volts(), 0.f);
		float before = c.envOut;
		CHECK(near(before, 10.f, 0.1f), "Vorwert %.3f", before);
		c.process(dt, p, 1, 2, 0.f, g.volts(), 10.f);
		CHECK(c.jumpCount == 1, "Reset: %d Meldungen", c.jumpCount);
		CHECK(near(c.jumpDelta[0], -10.f, 0.3f), "Reset-Sprung %.3f (erwartet -10)", c.jumpDelta[0]);
		CHECK(!c.eocOut, "Reset loest EOC aus");
	}
	{
		// Offset-Sprung 0 -> 0.25 bei y = 0.375 (Attack Mitte 5 V -> Hold 10 V): Delta +5 V
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
		for (int i = 0; i < N + 375; i++)
			c.process(dt, p, 1, 1, 0.f, g.volts(), 0.f);
		float before = c.envOut;
		c.process(dt, p, 1, 1, 0.25f, g.volts(), 0.f);
		CHECK(c.jumpCount == 1, "Offset-Sprung: %d Meldungen", c.jumpCount);
		CHECK(near(c.jumpDelta[0], c.envOut - before, 0.5f), "Offset-Sprung delta %.3f vs %.3f", c.jumpDelta[0], c.envOut - before);
		CHECK(!c.eocOut, "Offset-Sprung loest EOC aus");
	}
	{
		// Langsamer Offset-Sweep: keine Meldungen
		PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
		int jc = 0;
		for (int i = 0; i < 4 * N; i++) {
			c.process(dt, p, 1, 1, std::fmod(i * 0.0001f, 1.f), g.volts(), 0.f);
			jc += c.jumpCount;
		}
		CHECK(jc == 0, "Offset-Sweep meldet %d Spruenge", jc);
	}
	{
		// Richtungswechsel bei x = 0.9 (y 0.9 -> 0.1): Sprung gemeldet, kein EOC
		PhasorCore c; EnvParams p = defaultParams();
		int eoc = 0, jc = 0; bool prev = false;
		for (int i = 0; i <= 900; i++)
			c.process(dt, p, 1, 1, 0.f, i * 0.01f, 0.f);   // x 0 .. 0.9 steigend
		for (int i = 1; i <= 600; i++) {
			c.process(dt, p, 1, 1, 0.f, 9.f - i * 0.01f, 0.f); // x 0.9 .. 0.3 fallend
			if (c.eocOut && !prev) eoc++;
			prev = c.eocOut; jc += c.jumpCount;
		}
		CHECK(eoc == 0, "Richtungswechsel x=0.9: %d falsche EOC", eoc);
		CHECK(jc == 1, "Richtungswechsel: %d Meldungen (erwartet 1)", jc);
	}

	// 12) Lockere Ratio-Uebernahme: sofort am naechsten Eingangs-Wrap, mit gemeldetem Sprung
	{
		for (int loose = 0; loose <= 1; loose++) {
			PhasorCore c; EnvParams p = defaultParams(); Gen g(N, +1);
			int jumpsAtApply = 0; bool eocAtApply = false; bool applied = false;
			for (int i = 0; i < 3 * N; i++) {
				int rp = (i >= 10) ? 3 : 1, rq = (i >= 10) ? 2 : 1;   // Anforderung mitten im Zyklus 0
				bool wasApplied = (c.p == 3 && c.q == 2);
				c.process(dt, p, rp, rq, 0.f, g.volts(), 0.f, loose != 0);
				if (!wasApplied && c.p == 3 && c.q == 2) {
					applied = true; jumpsAtApply = c.jumpCount; eocAtApply = c.eocOut;
					// locker: am ersten Wrap (i = N, cycle = 1); strikt: erst am zweiten
					// Wrap (i = 2N, cycle = 2 ist durch q = 2 teilbar)
					int expect = loose ? N : 2 * N;
					CHECK(i == expect, "loose=%d: Uebernahme bei Sample %d (erwartet %d)", loose, i, expect);
				}
			}
			if (loose) {
				CHECK(applied, "locker: nie uebernommen");
				CHECK(jumpsAtApply == 1, "locker: %d Sprungmeldungen", jumpsAtApply);
				CHECK(!eocAtApply, "locker: EOC beim Phasensprung");
			}
			else {
				CHECK(applied, "strikt: nie uebernommen");
			}
		}
	}

	std::printf(fails ? "\n%d Fehler\n" : "\nalle Tests bestanden\n", fails);
	return fails ? 1 : 0;
}
