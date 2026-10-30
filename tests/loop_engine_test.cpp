// Szenario-Tests fuer die Loop-Schleifen in Afg (Marf248.hpp, Session 1b).
// Standalone, ohne Rack-SDK (siehe rack_stub/). Bauen/Starten: run_loop_tests.sh
#define NEW_ENGINE 1
#include "engine_driver.hpp"
#include <string>
#include <sstream>

static int g_fail = 0, g_ok = 0;
#define CHECK(cond, msg) do { if (cond) g_ok++; else { g_fail++; printf("  FEHLER [Zeile %d] %s\n", __LINE__, msg); } } while (0)
#define CHECK_EQ_STR(a, b, msg) do { std::string _a = (a), _b = (b); if (_a == _b) g_ok++; else { g_fail++; printf("  FEHLER [Zeile %d] %s\n    ist:   %s\n    soll:  %s\n", __LINE__, msg, _a.c_str(), _b.c_str()); } } while (0)

struct Trace {
	std::vector<int> stages;       // Stage bei jedem Wechsel (0-basiert; Wiedereintritt gleicher Stage wird nicht protokolliert)
	std::vector<int> restAtEntry;  // loopRest nach dem Wechsel
	std::vector<int> exitAt;       // Index in stages der Stage, bei deren Verlassen der Austritt feuerte
	int longestExit = 0, curExit = 0;
};

static Trace run(Rig& r, int n) {
	Trace t;
	int last = r.afg.currentStage;
	t.stages.push_back(last);
	t.restAtEntry.push_back((int) (r.loopRest + 0.5f));
	bool prevExit = false;
	for (int i = 0; i < n; i++) {
		r.step();
		bool ex = r.loopExit > 5.f;
		if (ex && !prevExit) t.exitAt.push_back((int) t.stages.size() - 1);
		if (ex) { t.curExit++; if (t.curExit > t.longestExit) t.longestExit = t.curExit; } else t.curExit = 0;
		prevExit = ex;
		if (r.afg.currentStage != last) {
			last = r.afg.currentStage;
			t.stages.push_back(last);
			t.restAtEntry.push_back((int) (r.loopRest + 0.5f));
		}
	}
	return t;
}
// off = 1: Stage-Nummern 1-basiert wie am Panel; off = 0: Rohwerte (Restwerte)
static std::string seq(const std::vector<int>& v, size_t from, size_t to, int off = 1) {
	std::ostringstream o;
	for (size_t i = from; i < v.size() && i < to; i++) o << (i > from ? " " : "") << v[i] + off;
	return o.str();
}
static std::string ex(const std::vector<int>& v, size_t n) {
	std::ostringstream o;
	for (size_t i = 0; i < v.size() && i < n; i++) o << (i ? " " : "") << v[i];
	return o.str();
}
static int rest(const Rig& r) { return (int) (r.loopRest + 0.5f); }
static void clearMods(Rig& r) { for (int i = 0; i < NUM_STAGES; i++) r.mod[i] = 0; }

static void test_simple() {
	printf("A Einfache Schleife Stage 2-4, 3 Durchlaeufe\n");
	Rig r;
	r.setLoop(2, 4, 3);
	r.begin();
	Trace t = run(r, 400);
	CHECK_EQ_STR(seq(t.stages, 0, 15), "1 2 3 4 2 3 4 2 3 4 5 6 7 8 9", "3 Durchlaeufe, dann weiter auf Stage 5");
	CHECK_EQ_STR(seq(t.restAtEntry, 0, 15, 0), "0 3 3 3 2 2 2 1 1 1 0 0 0 0 0", "Restwert 3,2,1 je Durchlauf, 0 ausserhalb");
	CHECK_EQ_STR(ex(t.exitAt, 2), "9 31", "Austrittsimpuls beim Verlassen von Stage 4 im 3. Durchlauf, je Besuch einmal");
	CHECK_EQ_STR(seq(t.restAtEntry, 22, 26, 0), "0 3 3 3", "Zaehler laedt im naechsten Besuch voll neu");
	CHECK(t.longestExit <= 2, "Austrittsimpuls ~1 ms");
}

static void test_endless() {
	printf("B Endlosschleife (Zaehler 0)\n");
	Rig r;
	r.setLoop(2, 4, 0);
	r.begin();
	Trace t = run(r, 300);
	CHECK_EQ_STR(seq(t.stages, 0, 13), "1 2 3 4 2 3 4 2 3 4 2 3 4", "springt immer zurueck");
	bool allZero = true;
	for (int v : t.restAtEntry) if (v != 0) allZero = false;
	CHECK(allZero && t.exitAt.empty(), "Restwert 0 (endlos), kein Austrittsimpuls");
}

static void test_nested() {
	printf("C Verschachtelt: aussen 2-8 (2x), innen 4-5 (3x)\n");
	Rig r;
	r.setLoop(2, 8, 2);
	r.setLoop(4, 5, 3);
	r.begin();
	Trace t = run(r, 600);
	CHECK_EQ_STR(seq(t.stages, 0, 25), "1 2 3 4 5 4 5 4 5 6 7 8 2 3 4 5 4 5 4 5 6 7 8 9 10", "innere Schleife laeuft in jedem aeusseren Durchgang voll");
	CHECK_EQ_STR(seq(t.restAtEntry, 0, 25, 0), "0 2 2 3 3 2 2 1 1 2 2 2 1 1 3 3 2 2 1 1 1 1 1 0 0", "Restwert zeigt die innerste Schleife");
	CHECK_EQ_STR(ex(t.exitAt, 3), "8 19 22", "Austritte: innen, innen, aussen");
}

static void test_reverse() {
	printf("D Reverse: Begin/End tauschen die Rollen, Schleife 5-8 (2x)\n");
	Rig r;
	r.setLoop(5, 8, 2);
	r.afg.reverseMode = true;
	r.afg.jumpToStage(7, r.mod, true);
	r.begin();
	Trace t = run(r, 400);
	CHECK_EQ_STR(seq(t.stages, 0, 12), "8 7 6 5 8 7 6 5 4 3 2 1", "Reverse: Sprung von Begin (5) zurueck auf End (8), 2 Durchlaeufe");
	CHECK_EQ_STR(seq(t.restAtEntry, 0, 8, 0), "2 2 2 2 1 1 1 1", "Restwert im Reverse (Zaehler der End-Stage)");
	CHECK_EQ_STR(seq(t.stages, 20, 28), "8 7 6 5 8 7 6 5", "naechster Besuch");
	CHECK_EQ_STR(ex(t.exitAt, 2), "7 27", "Austritt beim Verlassen der Begin-Stage im 2. Durchlauf");
	CHECK_EQ_STR(seq(t.restAtEntry, 20, 28, 0), "2 2 2 2 1 1 1 1", "Zaehler laedt neu");
}

static void test_jumps() {
	printf("E Strobe hinein / Recall heraus / Reset / Continuous\n");
	{
		Rig r; r.setLoop(3, 6, 3); r.begin();
		r.step(false, 3, true); // Strobe von aussen hinein (Stage 4)
		CHECK(r.afg.currentStage == 3 && rest(r) == 3, "Strobe hinein: voller Zaehler");
		for (int i = 0; i < 60 && rest(r) != 2; i++) r.step();
		CHECK(rest(r) == 2, "Rest 2 erreicht");
		r.step(false, 4, true); // Strobe innerhalb der Schleife
		CHECK(r.afg.currentStage == 4 && rest(r) == 2, "Strobe innerhalb: Zaehler laeuft weiter");
		r.afg.jumpToStage(9, r.mod); r.step(); // Recall heraus (Stage 10)
		CHECK(r.afg.currentStage == 9 && rest(r) == 0, "Recall heraus: ausserhalb, Rest 0");
		r.step(false, 2, true); // wieder hinein
		CHECK(rest(r) == 3, "Wieder hinein: Zaehler voll neu geladen");
		// Continuous innerhalb der Schleife aendert nichts, ausserhalb verfaellt der Zaehler
		for (int i = 0; i < 60 && rest(r) != 2; i++) r.step();
		for (int i = 0; i < 8; i++) r.step(true, 2 + (i % 4));
		CHECK(rest(r) == 2, "Continuous innerhalb: Zaehler unveraendert");
		r.step(true, 9);
		CHECK(rest(r) == 0, "Continuous ausserhalb: Rest 0");
		r.step(true, 2);
		CHECK(rest(r) == 3, "Continuous zurueck: Zaehler voll");
	}
	{
		Rig r; r.setLoop(1, 4, 3); r.begin();
		for (int i = 0; i < 80 && rest(r) != 1; i++) r.step();
		CHECK(rest(r) == 1, "Rest 1 erreicht");
		r.step(false, 0, false, true); // Reset: Stage 1 liegt IN der Schleife
		CHECK(r.afg.currentStage == 0 && rest(r) == 3, "Reset: Zaehler laedt auch innerhalb derselben Schleife neu");
	}
	{
		Rig r; r.setLoop(2, 4, 3); r.begin();
		for (int i = 0; i < 40 && rest(r) != 1; i++) r.step();
		CHECK(rest(r) == 1, "Rest 1 erreicht (resetLoopCounter-Test)");
		r.afg.resetLoopCounter(); r.step();
		CHECK(rest(r) == 3, "resetLoopCounter() (Global Clear): Zaehler startet neu");
	}
}

static void test_skip() {
	printf("F Skip auf End-/Begin-Stage, nur Skip\n");
	{
		Rig r; r.setLoop(2, 4, 2); r.mod[3] |= MOD_SKIP; r.begin();
		Trace t = run(r, 100);
		CHECK_EQ_STR(seq(t.stages, 0, 8), "1 2 3 2 3 5 6 7", "uebersprungene End-Stage zaehlt beim Verlassen");
		CHECK_EQ_STR(ex(t.exitAt, 1), "4", "Austritt in der Skip-Schleife");
	}
	{
		Rig r; r.setLoop(2, 4, 2); r.mod[1] |= MOD_SKIP; r.begin();
		Trace t = run(r, 100);
		CHECK_EQ_STR(seq(t.stages, 0, 7), "1 3 4 3 4 5 6", "uebersprungene Begin-Stage: Sprung landet und laeuft weiter");
	}
	{
		Rig r; for (int i = 0; i < NUM_STAGES; i++) r.mod[i] |= MOD_SKIP;
		r.mod[0] |= MOD_FIRST; r.mod[15] |= MOD_LAST;
		r.setLoop(2, 4, 0); r.begin();
		run(r, 300);
		CHECK(true, "Endlosschleife nur aus Skip-Stages haengt nicht");
	}
}

static void test_misc() {
	printf("G Ein-Stage-Schleife / H Pendulum / I unpaarig / J End=Last / K Clamp / L Flags zur Laufzeit\n");
	{
		Rig r; r.setLoop(3, 3, 3); r.begin();
		std::vector<int> rests; int exits = 0; bool pe = false;
		for (int i = 0; i < 14; i++) {
			r.step();
			bool e = r.loopExit > 5.f; if (e && !pe) exits++; pe = e;
			if (r.afg.currentStage == 2) rests.push_back(rest(r));
		}
		CHECK_EQ_STR(seq(rests, 0, 6, 0), "3 3 2 2 1 1", "Ein-Stage-Schleife: 3 Durchlaeufe auf Stage 3");
		CHECK(rests.size() == 6 && exits >= 1, "danach Weiterlauf, ein Austrittsimpuls");
	}
	{
		Rig a, b; a.setLoop(2, 4, 2);
		a.afg.pendulumMode = true; b.afg.pendulumMode = true;
		a.begin(); b.begin();
		Trace ta = run(a, 300), tb = run(b, 300);
		bool z = true; for (int v : ta.restAtEntry) if (v) z = false;
		CHECK(ta.stages == tb.stages && ta.exitAt.empty() && z, "Pendulum: Ablauf unveraendert, Restwert 0, kein Austritt");
	}
	{
		Rig a, b; a.flags[3] |= LOOPFLAG_END; a.cfg[3] = 2; a.flags[5] |= LOOPFLAG_BEGIN; // End ohne vorherigen Begin, Begin ohne End
		a.begin(); b.begin();
		Trace ta = run(a, 300), tb = run(b, 300);
		CHECK(ta.stages == tb.stages && ta.exitAt.empty(), "Unpaarige Begin/End werden ignoriert");
	}
	{
		Rig r; clearMods(r); r.mod[0] = MOD_FIRST; r.mod[3] = MOD_LAST;
		r.setLoop(2, 4, 2); r.begin();
		Trace t = run(r, 200);
		CHECK_EQ_STR(seq(t.stages, 0, 15), "1 2 3 4 2 3 4 1 2 3 4 2 3 4 1", "End=Last: nach dem Austritt greift der normale First/Last-Wrap");
	}
	{
		Rig r; r.setLoop(2, 4, 15); r.begin();
		for (int i = 0; i < 20 && r.afg.currentStage != 1; i++) r.step();
		CHECK(r.loopRest == 9.f, "Wert > 9 wird auf 9 geklemmt");
	}
	{
		Rig r; r.setLoop(2, 4, 0); r.begin();
		run(r, 20);
		for (int i = 0; i < NUM_STAGES; i++) r.flags[i] = 0; // Flags zur Laufzeit entfernen
		Trace t = run(r, 100);
		int mx = 0; for (int s : t.stages) if (s > mx) mx = s;
		CHECK(mx >= 9, "Flags zur Laufzeit entfernt: Schleife loest sich auf, Ablauf geht weiter");
	}
}

int main() {
	test_simple();
	test_endless();
	test_nested();
	test_reverse();
	test_jumps();
	test_skip();
	test_misc();
	printf("\n%d Pruefungen ok, %d Fehler\n", g_ok, g_fail);
	return g_fail ? 1 : 0;
}
