#pragma once
// Gemeinsame Test-Hilfen fuer die Afg-Engine-Tests (kein Rack-SDK noetig,
// siehe rack_stub/). Wird von loop_bitident.cpp (altes UND neues Marf248.hpp,
// je als eigenes Programm gebaut) und loop_engine_test.cpp (nur neu) benutzt.
#include "plugin.hpp"
#include "Marf248.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

#ifdef NEW_ENGINE
#define LOOP_ARGS_CFG , cfg
#define LOOP_ARGS_OUT , &loopExit, &loopRest
#else
#define LOOP_ARGS_CFG
#define LOOP_ARGS_OUT
#endif

struct Rig {
	Afg afg;
	float volt[NUM_STAGES];
	float time[NUM_STAGES];
	int mod[NUM_STAGES];
	float curve[NUM_STAGES];
	int ratchet[NUM_STAGES];
	float warp[NUM_STAGES];
	float roff[NUM_STAGES];
	int cfg[NUM_STAGES];
	float dt = 1e-3f;
	// letzte Ausgaenge
	float vOut = 0, tOut = 0, ap = 0, p1 = 0, p2 = 0, sc = 0, ec = 0, rg = 0, eos = 0;
	float loopExit = 0, loopRest = 0;
	// Steuer-Eingaenge fuer den naechsten Sample-Schritt
	float runButton = 0;
	float startV = 0, advV = 0;
	bool advConnected = false;

	Rig() {
		for (int i = 0; i < NUM_STAGES; i++) {
			volt[i] = i * 0.5f; time[i] = 2e-3f; mod[i] = 0; curve[i] = 1.f;
			ratchet[i] = 0; warp[i] = 0.f; roff[i] = 0.f; cfg[i] = 0;
		}
		mod[0] |= MOD_FIRST;
		mod[NUM_STAGES - 1] |= MOD_LAST;
	}

	void step(bool continuous = false, int addr = 0, bool strobe = false, bool reset = false) {
		afg.applyStageAddress(continuous, addr, strobe, reset, mod, false);
		afg.process(dt, volt, time, mod, curve, ratchet, warp, roff LOOP_ARGS_CFG,
			0, 0, runButton, false, startV, false, 0.f, false, 0.f, advConnected, advV,
			&vOut, &tOut, &ap, &p1, &p2, &sc, &ec, &rg, &eos LOOP_ARGS_OUT);
	}
	// Fuer die Szenario-Tests: Lauf direkt freigeben, ohne Stage-Zeit zu
	// verbrauchen (Start-Stage vorher per jumpToStage() setzen).
	void begin() { afg.running = true; step(); }
	void start() { runButton = 0.f; step(); runButton = 10.f; step(); runButton = 0.f; step(); }
};
