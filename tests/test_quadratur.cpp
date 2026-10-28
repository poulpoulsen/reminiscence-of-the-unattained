// Standalone-Test fuer die Quadratur-Kopplung von EnvGen4 (ohne Rack).
// Kompilieren (aus dem Projektordner): g++ -std=c++17 -I src -o /tmp/test_quadratur tests/test_quadratur.cpp
#include <cassert>
#include <cstdio>
#include "EnvGen4.hpp"

static const float DT = 1.f / 48000.f;
static const float NOTRIG[ENV_NUM_STAGES] = {0, 0, 0, 0};

// Gen 1: Delay 0, Attack 0,1 s, Hold 0,05 s, Release 0,1 s. Gen 2 gleich, aber
// Hold 0 s. Nachbildung der Schleife aus Europa::process() fuer ein Paar.
struct Paar {
	EnvGen4 g1, g2;
	EnvParams p1, p2;
	bool gekoppelt = true;
	Paar() {
		for (EnvParams* p : {&p1, &p2}) {
			p->time[ENV_STAGE_ATTACK] = 0.1f;
			p->time[ENV_STAGE_RELEASE] = 0.1f;
		}
		p1.time[ENV_STAGE_HOLD] = 0.05f;
	}
	// gate1: Gate von Gen 1, gate2: eigenes T1/GATE von Gen 2 (wird bei Kopplung ignoriert)
	void step(float gate1, float gate2) {
		g1.process(DT, p1, gate1, NOTRIG);
		p2.coupled = gekoppelt;
		float gv = gekoppelt ? (g1.gateHigh ? 10.f : 0.f) : gate2;
		g2.process(DT, p2, gv, NOTRIG, gekoppelt && g1.attackEnded);
	}
};

int main() {
	// 1) Gen 2 startet am Attack-Ende von Gen 1 (t = 0,1 s), nicht frueher
	{
		Paar p;
		int start2 = -1, ende1 = -1;
		for (int i = 0; i < 24000; i++) {
			p.step(i < 100 ? 10.f : 0.f, 0.f);
			if (p.g1.attackEnded && ende1 < 0) ende1 = i;
			if (p.g2.running && start2 < 0) start2 = i;
		}
		assert(ende1 >= 0 && start2 >= 0);
		assert(start2 == ende1); // gleiches Sample
		assert(std::fabs(start2 / 48000.0 - 0.1) < 0.001);
		std::printf("1 ok: Gen 2 startet bei %.4f s (Attack-Ende Gen 1 %.4f s)\n", start2 / 48000.0, ende1 / 48000.0);
	}

	// 2) Eigenes Gate von Gen 2 wird ignoriert
	{
		Paar p;
		for (int i = 0; i < 4800; i++) p.step(0.f, 10.f); // nur Gen-2-Gate
		assert(!p.g2.running);
		std::puts("2 ok: eigenes Gate von Gen 2 startet nichts");
	}

	// 3) Gate-Flanke von Gen 1 startet Gen 2 NICHT sofort (nur Gen 1)
	{
		Paar p;
		for (int i = 0; i < 100; i++) p.step(10.f, 0.f);
		assert(p.g1.running && !p.g2.running);
		std::puts("3 ok: Gate-Flanke startet nur Gen 1");
	}

	// 4) Ohne Kopplung: bisheriges Verhalten (Gen 2 laeuft ueber eigenes Gate)
	{
		Paar p;
		p.gekoppelt = false;
		for (int i = 0; i < 100; i++) p.step(0.f, 10.f);
		assert(p.g2.running);
		for (int i = 0; i < 100; i++) p.step(10.f, 0.f);
		std::puts("4 ok: ohne Kopplung unveraendert");
	}

	// 5) Loop Gen 1: jeder Zyklus startet Gen 2 neu (Attack-Ende alle 0,25 s)
	{
		Paar p;
		p.p1.loopEnabled = true;
		int ereignisse = 0;
		for (int i = 0; i < 48000; i++) { // Gate bleibt hoch, 1 s
			p.step(10.f, 0.f);
			if (p.g1.attackEnded) ereignisse++;
		}
		std::printf("5 ok: %d Attack-Enden von Gen 1 in 1 s (Loop, erwartet 4)\n", ereignisse);
		assert(ereignisse == 4);
	}

	// 6) Sustain Gen 2: haelt am Attack-Ende, solange Gate 1 hoch; Release bei Gate low
	{
		Paar p;
		p.p2.sustainMode = true; p.p1.sustainMode = true;
		for (int i = 0; i < 48000; i++) p.step(10.f, 0.f); // 1 s Gate hoch
		assert(p.g2.sustainHeld);
		assert(std::fabs(p.g2.envOut - 10.f) < 1e-3f);
		p.step(0.f, 0.f);
		for (int i = 0; i < 480; i++) p.step(0.f, 0.f);
		assert(p.g2.stage == ENV_STAGE_RELEASE || !p.g2.running);
		for (int i = 0; i < 12000; i++) p.step(0.f, 0.f);
		assert(!p.g2.running && std::fabs(p.g2.envOut) < 1e-3f);
		std::puts("6 ok: Sustain-Follower haelt und klingt bei Gate-Ende aus");
	}

	// 7) Attack-Ende bei Zeit 0 und Sustain-Halten feuert ebenfalls
	{
		EnvGen4 g; EnvParams q;
		q.time[ENV_STAGE_ATTACK] = 0.f;
		bool gesehen = false;
		g.process(DT, q, 10.f, NOTRIG);
		gesehen = g.attackEnded; // Zyklus startet und laeuft im selben Sample durch
		for (int i = 0; i < 10; i++) { g.process(DT, q, 10.f, NOTRIG); gesehen |= g.attackEnded; }
		assert(gesehen);
		std::puts("7 ok: Attack-Ende auch bei Attack-Zeit 0");
	}
	std::puts("ALLE TESTS BESTANDEN");
}
