#include "EnvGen4.hpp"
#include <cstdio>
#include <cstdlib>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

static const float DT = 1.f / 48000.f;

struct Run { int eoc = 0; float maxV = 0.f; bool everRunning = false; bool runningAtEnd = false; };

// Laeuft secs Sekunden mit konstantem Gate; zaehlt EOC-Pulse (steigende Flanken)
static Run run(EnvGen4& g, const EnvParams& p, float gateV, float secs) {
	Run r; bool prev = false;
	float trig[4] = {0, 0, 0, 0};
	int n = (int)(secs / DT);
	for (int i = 0; i < n; i++) {
		g.process(DT, p, gateV, trig);
		if (g.eocOut && !prev) r.eoc++;
		prev = g.eocOut;
		r.maxV = std::max(r.maxV, g.envOut);
		r.everRunning |= g.running;
	}
	r.runningAtEnd = g.running;
	return r;
}

static EnvParams base() {
	EnvParams p;
	p.time[ENV_STAGE_DELAY] = 0.f;
	p.time[ENV_STAGE_ATTACK] = 0.05f;
	p.time[ENV_STAGE_HOLD] = 0.f;
	p.time[ENV_STAGE_RELEASE] = 0.05f;   // Zyklus = 100 ms
	return p;
}

int main() {
	// 1. Referenz: One-Shot ohne Gate tut nichts
	{ EnvGen4 g; EnvParams p = base();
	  Run r = run(g, p, 0.f, 0.5f);
	  CHECK(!r.everRunning && r.eoc == 0 && r.maxV == 0.f, "One-Shot ohne Gate bleibt still"); }

	// 2. Loop bei Gate (freeCycle aus), Gate low: startet nicht
	{ EnvGen4 g; EnvParams p = base(); p.loopEnabled = true;
	  Run r = run(g, p, 0.f, 0.5f);
	  CHECK(!r.everRunning && r.eoc == 0, "Gate-Loop ohne Gate bleibt still (altes Verhalten)"); }

	// 3. Freier Zyklus ohne Gate: startet von selbst, ~5 Zyklen in 0.5 s
	{ EnvGen4 g; EnvParams p = base(); p.loopEnabled = true; p.freeCycle = true;
	  Run r = run(g, p, 0.f, 0.5f);
	  printf("      EOC-Pulse in 0.5 s: %d, Maximum %.2f V\n", r.eoc, r.maxV);
	  CHECK(r.everRunning && r.runningAtEnd, "Free Cycle startet ohne Gate und laeuft weiter");
	  CHECK(r.eoc >= 4 && r.eoc <= 5, "Free Cycle: etwa 5 EOC in 0.5 s (Zyklus 100 ms)");
	  CHECK(r.maxV > 9.9f, "Free Cycle erreicht den Attack-Pegel"); }

	// 4. Free Cycle aus -> Zyklus endet am Release-Ende, Generator steht, Ausgang = Ruhepegel
	{ EnvGen4 g; EnvParams p = base(); p.loopEnabled = true; p.freeCycle = true;
	  run(g, p, 0.f, 0.23f);
	  p.loopEnabled = false; p.freeCycle = false;
	  Run r = run(g, p, 0.f, 0.3f);
	  CHECK(!r.runningAtEnd && g.envOut == 0.f, "Free Cycle ausgeschaltet: Zyklus endet, Ausgang auf Ruhepegel"); }

	// 5. Free Cycle -> Gate-Loop ohne Gate: Zyklus endet ebenfalls
	{ EnvGen4 g; EnvParams p = base(); p.loopEnabled = true; p.freeCycle = true;
	  run(g, p, 0.f, 0.23f);
	  p.freeCycle = false;
	  Run r = run(g, p, 0.f, 0.3f);
	  CHECK(!r.runningAtEnd, "Wechsel auf Gate-Loop ohne Gate: Generator stoppt sauber"); }

	// 6. Sustain-Modus: Free Cycle wirkungslos
	{ EnvGen4 g; EnvParams p = base(); p.loopEnabled = true; p.freeCycle = true; p.sustainMode = true;
	  Run r = run(g, p, 0.f, 0.5f);
	  CHECK(!r.everRunning, "Sustain-Modus: Free Cycle ohne Wirkung"); }

	// 7. Gate-Flanke im freien Zyklus (Retrigger) bricht nichts
	{ EnvGen4 g; EnvParams p = base(); p.loopEnabled = true; p.freeCycle = true;
	  run(g, p, 0.f, 0.13f);
	  Run r = run(g, p, 5.f, 0.3f);
	  CHECK(r.runningAtEnd && r.eoc >= 2, "Gate waehrend Free Cycle: laeuft weiter"); }

	// 8. Alle Zeiten 0: kein Haenger, gueltige Spannung
	{ EnvGen4 g; EnvParams p; p.loopEnabled = true; p.freeCycle = true;
	  Run r = run(g, p, 0.f, 0.05f);
	  CHECK(std::isfinite(g.envOut) && g.envOut >= 0.f && g.envOut <= 10.f, "Alle Zeiten 0: Ausgang endlich, im Bereich"); }

	// 9. Unendliche Hold-Zeit: Free Cycle haelt (wie Gate-Loop), kein Absturz
	{ EnvGen4 g; EnvParams p = base(); p.loopEnabled = true; p.freeCycle = true;
	  p.time[ENV_STAGE_HOLD] = ENV_TIME_INFINITE;
	  Run r = run(g, p, 0.f, 0.5f);
	  CHECK(r.runningAtEnd && g.held && r.eoc == 0, "Unendliches Hold: Free Cycle haelt im Hold"); }

	// 10. Retrig-Schalter + Delay-Pegel im Free Cycle: Pegel bleibt in 0..10 V
	{ EnvGen4 g; EnvParams p = base(); p.loopEnabled = true; p.freeCycle = true;
	  p.retrigZero = true; p.levelDelay = 3.f; p.levelRelease = 2.f; p.time[ENV_STAGE_DELAY] = 0.02f;
	  float lo = 99.f, hi = -99.f; float trig[4] = {0,0,0,0};
	  for (int i = 0; i < 48000; i++) { g.process(DT, p, 0.f, trig); lo = std::min(lo, g.envOut); hi = std::max(hi, g.envOut); }
	  printf("      Bereich: %.2f .. %.2f V\n", lo, hi);
	  CHECK(lo >= 1.99f && hi <= 10.01f, "Delay-/Release-Pegel im Free Cycle: Ausgang in plausiblem Bereich"); }

	printf("\n%s (%d Fehler)\n", fails ? "FEHLGESCHLAGEN" : "ALLE TESTS BESTANDEN", fails);
	return fails ? 1 : 0;
}
