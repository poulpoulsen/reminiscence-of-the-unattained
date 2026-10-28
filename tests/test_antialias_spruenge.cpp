// ============================================================================
// test_antialias_spruenge.cpp - Standalone-Test fuer die Sprungmeldung von
// EnvGen4 (Grundlage des optionalen minBLEP-Antialiasings in Europa).
//
// Kein Rack noetig. Liegt in tests/ (NICHT in src/, sonst linkt das Makefile
// zwei main()). Bauen und ausfuehren, z. B. aus dem Plugin-Ordner:
//   g++ -std=c++17 -O1 -Isrc tests/test_antialias_spruenge.cpp -o /tmp/t_aa && /tmp/t_aa
// ============================================================================
#include "EnvGen4.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cmath>

static const float DT = 1.f / 48000.f;

static uint32_t rngState = 12345u;
static float rnd() {
	rngState = rngState * 1664525u + 1013904223u;
	return (float) (rngState >> 8) / 16777216.f;
}

static void noTrig(float t[ENV_NUM_STAGES]) {
	for (int i = 0; i < ENV_NUM_STAGES; i++) t[i] = 0.f;
}

// Fall 1: Alle Zeiten > 0 -> nie ein gemeldeter Sprung.
static void testKeineSpruengeBeiStetigerHuellkurve() {
	EnvGen4 g;
	EnvParams p;
	for (int s = 0; s < ENV_NUM_STAGES; s++) p.time[s] = 0.005f + 0.002f * s;
	p.loopEnabled = true;
	p.retrigZero = true;
	float trig[ENV_NUM_STAGES];
	noTrig(trig);
	for (int n = 0; n < 48000; n++) {
		float gate = (n >= 100 && n < 30000) ? 10.f : 0.f;
		g.process(DT, p, gate, trig);
		assert(g.jumpCount == 0);
	}
	std::printf("ok: stetige Huellkurve meldet keine Spruenge\n");
}

// Fall 2: Attack-Zeit 0 (Delay 0) -> genau ein Sprung 0 -> 10 V.
static void testAttackZeitNull() {
	EnvGen4 g;
	EnvParams p;
	p.time[ENV_STAGE_DELAY] = 0.f;
	p.time[ENV_STAGE_ATTACK] = 0.f;
	p.time[ENV_STAGE_HOLD] = 0.01f;
	p.time[ENV_STAGE_RELEASE] = 0.01f;
	float trig[ENV_NUM_STAGES];
	noTrig(trig);
	for (int n = 0; n < 10; n++) g.process(DT, p, 0.f, trig);
	float prev = g.envOut;
	g.process(DT, p, 10.f, trig); // Gate-Flanke
	assert(g.jumpCount == 1);
	assert(std::fabs(g.jumpDelta[0] - 10.f) < 1e-4f);
	assert(g.jumpPos[0] > -1.f && g.jumpPos[0] <= 0.f);
	assert(std::fabs((g.envOut - prev) - g.jumpDelta[0]) < 1e-3f);
	std::printf("ok: Attack-Zeit 0 -> ein Sprung von 10 V\n");
}

// Fall 3: Retrig-Schalter springt auf den Delay-Pegel (3 V), Delay-Zeit > 0.
static void testRetrigSprung() {
	EnvGen4 g;
	EnvParams p;
	p.levelDelay = 3.f;
	p.levelRelease = 0.f;
	p.retrigZero = true;
	p.time[ENV_STAGE_DELAY] = 0.01f;
	p.time[ENV_STAGE_ATTACK] = 0.01f;
	p.time[ENV_STAGE_HOLD] = 0.01f;
	p.time[ENV_STAGE_RELEASE] = 0.01f;
	float trig[ENV_NUM_STAGES];
	noTrig(trig);
	for (int n = 0; n < 10; n++) g.process(DT, p, 0.f, trig);
	assert(std::fabs(g.envOut) < 1e-6f);
	g.process(DT, p, 10.f, trig);
	assert(g.jumpCount == 1);
	assert(std::fabs(g.jumpDelta[0] - 3.f) < 1e-4f);
	std::printf("ok: Retrig-Sprung auf den Delay-Pegel\n");
}

// Fall 4: Release-Zeit 0 im Sustain-Modus: Gate low -> Sprung auf Release-Pegel.
static void testReleaseZeitNull() {
	EnvGen4 g;
	EnvParams p;
	p.sustainMode = true;
	p.time[ENV_STAGE_DELAY] = 0.f;
	p.time[ENV_STAGE_ATTACK] = 0.001f;
	p.time[ENV_STAGE_HOLD] = 0.001f;
	p.time[ENV_STAGE_RELEASE] = 0.f;
	float trig[ENV_NUM_STAGES];
	noTrig(trig);
	for (int n = 0; n < 2000; n++) g.process(DT, p, 10.f, trig); // sustain bei 10 V
	assert(std::fabs(g.envOut - 10.f) < 1e-4f && g.held);
	g.process(DT, p, 0.f, trig);
	assert(g.jumpCount == 1);
	assert(std::fabs(g.jumpDelta[0] + 10.f) < 1e-4f);
	std::printf("ok: Release-Zeit 0 -> Sprung 10 V -> 0 V\n");
}

// Fall 5: Zufallslaeufe. Zeiten 0 oder >= 5 ms. Pruefung:
//  - Positionen in (-1, 0]
//  - Aenderung pro Sample abzueglich der Summe der gemeldeten Spruenge bleibt
//    klein (nur stetiger Anteil, bei >= 5 ms und Kurvenexponent <= ~16 unter 1,5 V)
//  - Jumpzaehler nie ueber dem Limit
static void testZufallslaeufe() {
	int jumpsTotal = 0;
	for (int run = 0; run < 300; run++) {
		EnvGen4 g;
		EnvParams p;
		for (int s = 0; s < ENV_NUM_STAGES; s++) {
			p.time[s] = (rnd() < 0.4f) ? 0.f : 0.005f + 0.02f * rnd();
			p.curve[s] = rnd() * 2.f - 1.f;
		}
		p.levelDelay = 10.f * rnd();
		p.levelAttack = 10.f * rnd();
		p.levelHold = 10.f * rnd();
		p.levelRelease = 10.f * rnd();
		p.sustainMode = rnd() < 0.3f;
		p.loopEnabled = rnd() < 0.5f;
		p.freeCycle = p.loopEnabled && !p.sustainMode && rnd() < 0.3f;
		p.retrigZero = rnd() < 0.5f;
		p.rate = (rnd() < 0.3f) ? 4.f : 1.f;
		// Degenerationsfall ausnehmen: alle Zeiten 0 + Loop = Endlosschleife mit
		// Abbruch nach ENV_MAX_TRANSITIONS_PER_SAMPLE. Die Spruenge heben sich
		// dort bei gleicher Position gegenseitig auf (kein Effekt auf minBLEP).
		if ((p.loopEnabled || p.freeCycle)
			&& p.time[0] <= 0.f && p.time[1] <= 0.f && p.time[2] <= 0.f && p.time[3] <= 0.f)
			p.time[3] = 0.01f;
		float trig[ENV_NUM_STAGES];
		noTrig(trig);
		// Anlaufsample: im Stillstand folgt der Ausgang dem Release-Pegel, der
		// Start auf diesen Pegel ist kein Huellkurvensprung.
		g.process(DT, p, 0.f, trig);
		float prev = g.envOut;
		bool gate = false;
		for (int n = 0; n < 24000; n++) {
			if (rnd() < 0.0008f) gate = !gate;
			noTrig(trig);
			for (int i = 1; i < ENV_NUM_STAGES; i++)
				if (rnd() < 0.0004f) trig[i] = 10.f;
			g.process(DT, p, gate ? 10.f : 0.f, trig);
			assert(g.jumpCount >= 0 && g.jumpCount <= ENV_MAX_JUMPS_PER_SAMPLE);
			float sum = 0.f;
			for (int j = 0; j < g.jumpCount; j++) {
				assert(g.jumpPos[j] > -1.f && g.jumpPos[j] <= 0.f);
				sum += g.jumpDelta[j];
			}
			jumpsTotal += g.jumpCount;
			float stetig = (g.envOut - prev) - sum;
			if (std::fabs(stetig) > 1.5f) {
				std::printf("FEHLER run=%d n=%d envOut=%f prev=%f sumJump=%f jumps=%d\n",
				            run, n, g.envOut, prev, sum, g.jumpCount);
				assert(false);
			}
			prev = g.envOut;
		}
	}
	assert(jumpsTotal > 0); // der Test muss Spruenge wirklich erzeugen
	std::printf("ok: 300 Zufallslaeufe konsistent (%d Spruenge gemeldet)\n", jumpsTotal);
}

int main() {
	testKeineSpruengeBeiStetigerHuellkurve();
	testAttackZeitNull();
	testRetrigSprung();
	testReleaseZeitNull();
	testZufallslaeufe();
	std::printf("alle Tests bestanden\n");
	return 0;
}
