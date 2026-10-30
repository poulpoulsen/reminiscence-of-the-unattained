// Bitgleichheit: gleiches Zufalls-Skript gegen altes (orig/Marf248.hpp) und
// neues Marf248.hpp (ohne gepatchten Loop-Eingang: cfg = 0). Beide Varianten
// werden als EIGENE Programme gebaut (ODR: zwei verschiedene struct Afg) und
// geben pro Sample einen FNV-Hash ueber alle Ausgaenge + Engine-Zustand aus.
#include "engine_driver.hpp"

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 11); }
static uint64_t H = 1469598103934665603ull;
static void mix(const void* p, size_t n) {
	const unsigned char* c = (const unsigned char*)p;
	for (size_t i = 0; i < n; i++) { H ^= c[i]; H *= 1099511628211ull; }
}
template <typename T> static void mixv(T v) { mix(&v, sizeof v); }

int main(int argc, char** argv) {
	int scripts = argc > 1 ? atoi(argv[1]) : 300;
	for (int sc = 0; sc < scripts; sc++) {
		Rig r;
		rng ^= (uint64_t)(sc + 1) * 0x2545F4914F6CDD1Dull;
		for (int i = 0; i < NUM_STAGES; i++) r.time[i] = (1 + rnd() % 6) * 1e-3f;
		bool cont = false;
		int addr = 0;
		r.start(); // Sonst stuende der Lauf-Zustand still (running = false)
		for (int t = 0; t < 4000; t++) {
			uint32_t e = rnd() % 400;
			// Konfig-Aenderungen mitten im Lauf
			if (e == 0) { // First/Last/Skip/Mute neu wuerfeln
				for (int i = 0; i < NUM_STAGES; i++) {
					r.mod[i] = 0;
					uint32_t k = rnd() % 12;
					if (k == 0) r.mod[i] |= MOD_FIRST;
					if (k == 1) r.mod[i] |= MOD_LAST;
					if (k == 2) r.mod[i] |= MOD_SKIP;
					if (k == 3) r.mod[i] |= MOD_MUTE;
					if (k == 4) r.mod[i] |= MOD_FIRST | MOD_LAST;
					if (k == 5) r.mod[i] |= MOD_STOP;
				}
				if (rnd() % 2) r.mod[0] |= MOD_FIRST;
				if (rnd() % 2) r.mod[NUM_STAGES - 1] |= MOD_LAST;
			}
			if (e == 1) r.afg.reverseMode = !r.afg.reverseMode;
			if (e == 2) r.afg.pendulumMode = !r.afg.pendulumMode;
			if (e == 3) cont = !cont;
			if (e == 4 && (sc % 5 == 0)) r.runButton = 10.f; else r.runButton = 0.f;
			bool strobe = (e >= 5 && e < 8);
			bool reset = (e == 8);
			if (e >= 9 && e < 14) addr = rnd() % NUM_STAGES;
			r.advConnected = (sc % 3 == 0);
			r.advV = (e >= 14 && e < 18) ? 10.f : 0.f;
			if (sc % 4 == 1 && e == 19) r.afg.jumpToStage(rnd() % NUM_STAGES, r.mod); // Addr-Recall
			r.step(cont, addr, strobe, reset);
			mixv(r.vOut); mixv(r.tOut); mixv(r.ap); mixv(r.p1); mixv(r.p2); mixv(r.sc);
			mixv(r.ec); mixv(r.rg); mixv(r.eos);
			mixv(r.afg.currentStage); mixv(r.afg.elapsed); mixv(r.afg.pendulumDir);
			mixv(r.afg.running);
		}
		printf("%016llx\n", (unsigned long long)H);
	}
	return 0;
}
