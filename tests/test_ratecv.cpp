// Standalone-Test fuer die Rate-CV von EnvGen4 (ohne Rack).
// Kompilieren: g++ -std=c++17 -o test_ratecv test_ratecv.cpp && ./test_ratecv
#include <cassert>
#include <cstdio>
#include <vector>
#include "EnvGen4.hpp"

static const float DT = 1.f / 48000.f;

// Parameter: Delay 0, Attack 0,1 s, Hold 0, Release 0,1 s, Kurve einstellbar
static EnvParams basis(float rate, float curve = 0.f) {
	EnvParams p;
	p.time[ENV_STAGE_DELAY] = 0.f;
	p.time[ENV_STAGE_ATTACK] = 0.1f;
	p.time[ENV_STAGE_HOLD] = 0.f;
	p.time[ENV_STAGE_RELEASE] = 0.1f;
	p.curve[ENV_STAGE_ATTACK] = curve;
	p.curve[ENV_STAGE_RELEASE] = curve;
	p.rate = rate;
	return p;
}

// Einen Zyklus per Gate-Flanke starten und die Huellkurve aufzeichnen,
// bis der Generator wieder steht. Gate bleibt kurz hoch (One-Shot).
static std::vector<float> zyklus(float rate, float curve, double* dauer) {
	EnvGen4 g;
	EnvParams p = basis(rate, curve);
	float trig[ENV_NUM_STAGES] = {0, 0, 0, 0};
	std::vector<float> out;
	int n = 0;
	g.process(DT, p, 10.f, trig);
	out.push_back(g.envOut);
	while (g.running && n < 48000 * 20) {
		g.process(DT, p, n < 10 ? 10.f : 0.f, trig);
		out.push_back(g.envOut);
		n++;
	}
	*dauer = out.size() * (double) DT;
	return out;
}

int main() {
	// 1) Faktor-Abbildung
	assert(std::fabs(envRateFromVolts(0.f) - 1.f) < 1e-6f);
	assert(std::fabs(envRateFromVolts(1.f) - 2.f) < 1e-5f);
	assert(std::fabs(envRateFromVolts(-1.f) - 0.5f) < 1e-6f);
	assert(std::fabs(envRateFromVolts(99.f) - 32.f) < 1e-3f);    // Begrenzung +5 V
	assert(std::fabs(envRateFromVolts(-99.f) - 1.f / 32.f) < 1e-5f);
	std::puts("1 ok: Faktor-Abbildung und Begrenzung");

	// 2) Dauer skaliert mit 1/rate (0,2 s bei x1)
	double d1, d2, d05;
	auto a1 = zyklus(1.f, 0.f, &d1);
	auto a2 = zyklus(2.f, 0.f, &d2);
	auto a05 = zyklus(0.5f, 0.f, &d05);
	assert(std::fabs(d1 - 0.2) < 0.002);
	assert(std::fabs(d2 - 0.1) < 0.002);
	assert(std::fabs(d05 - 0.4) < 0.004);
	std::printf("2 ok: Dauer x1=%.4f s, x2=%.4f s, x0,5=%.4f s\n", d1, d2, d05);

	// 3) Kurvenform bleibt gleich: Huellkurve bei x2 an Stelle i == x1 an Stelle 2i
	for (float c : {-0.8f, 0.f, 0.8f}) {
		double da, db;
		auto x1 = zyklus(1.f, c, &da);
		auto x2 = zyklus(2.f, c, &db);
		float maxAbw = 0.f;
		for (size_t i = 0; i < x2.size(); i++) {
			size_t j = std::min(2 * i, x1.size() - 1);
			maxAbw = std::max(maxAbw, std::fabs(x2[i] - x1[j]));
		}
		assert(maxAbw < 0.15f); // Abtast-Versatz um max. 1 Sample bei steiler Flanke
		std::printf("3 ok: Kurve %+.1f, max. Abweichung %.4f V\n", c, maxAbw);
	}

	// 4) Rate mitten in der Stage aendern: Pegel bleibt stetig (kein Sprung)
	{
		EnvGen4 g;
		EnvParams p = basis(1.f);
		float trig[ENV_NUM_STAGES] = {0, 0, 0, 0};
		g.process(DT, p, 10.f, trig);
		for (int i = 0; i < 2400; i++) g.process(DT, p, 10.f, trig); // 50 ms
		float vorher = g.envOut;
		p.rate = 8.f;
		g.process(DT, p, 10.f, trig);
		assert(std::fabs(g.envOut - vorher) < 0.2f);
		std::puts("4 ok: Rate-Wechsel mitten in der Stage ist stetig");
	}

	// 5) Auto-Cycle: EOC-Zahl in 0,5 s steigt mit der Rate, EOC-Laenge in echter Zeit
	for (float rate : {1.f, 4.f}) {
		EnvGen4 g;
		EnvParams p = basis(rate);
		p.loopEnabled = true;
		p.freeCycle = true;
		float trig[ENV_NUM_STAGES] = {0, 0, 0, 0};
		int eoc = 0;
		bool prev = false;
		for (int i = 0; i < 24000; i++) {
			g.process(DT, p, 0.f, trig);
			if (g.eocOut && !prev) eoc++;
			prev = g.eocOut;
		}
		std::printf("5 ok: Auto-Cycle x%.0f: %d EOC in 0,5 s\n", rate, eoc);
		// Erwartung 2,5 Zyklen je x1; ein EOC genau an der Fenstergrenze darf fehlen/dazukommen
		assert(eoc >= (int) (2.5f * rate) - 1 && eoc <= (int) (2.5f * rate) + 1);
	}

	// 6) Unendliches Halten bleibt unbeeinflusst (auch bei hoher Rate)
	{
		EnvGen4 g;
		EnvParams p = basis(32.f);
		p.time[ENV_STAGE_HOLD] = ENV_TIME_INFINITE;
		float trig[ENV_NUM_STAGES] = {0, 0, 0, 0};
		g.process(DT, p, 10.f, trig);
		for (int i = 0; i < 48000; i++) g.process(DT, p, 0.f, trig);
		assert(g.running && g.held && g.stage == ENV_STAGE_HOLD);
		std::puts("6 ok: unendliches Halten bleibt bei x32 stehen");
	}

	// 7) Extreme: alle Zeiten 0 bei x32, kein Haenger
	{
		EnvGen4 g;
		EnvParams p;
		p.rate = 32.f;
		p.loopEnabled = true; p.freeCycle = true;
		float trig[ENV_NUM_STAGES] = {0, 0, 0, 0};
		for (int i = 0; i < 1000; i++) g.process(DT, p, 0.f, trig);
		std::puts("7 ok: alle Zeiten 0 bei x32 ohne Haenger");
	}
	std::puts("ALLE TESTS BESTANDEN");
}
