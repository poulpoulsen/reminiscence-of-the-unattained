// Szenario-Tests fuer den Loop-Zaehler in Afg (Marf248.hpp, Session 1).
// Standalone, ohne Rack-SDK (siehe rack_stub/). Bauen/Starten: siehe run_loop_tests.sh
#define NEW_ENGINE 1
#include "engine_driver.hpp"
#include <string>
#include <sstream>

static int g_fail = 0, g_ok = 0;
#define CHECK(cond, msg) do { if (cond) g_ok++; else { g_fail++; printf("  FEHLER [%s:%d] %s\n", __FILE__, __LINE__, msg); } } while (0)
#define CHECK_EQ_STR(a, b, msg) do { std::string _a = (a), _b = (b); if (_a == _b) g_ok++; else { g_fail++; printf("  FEHLER [%d] %s\n    ist:   %s\n    soll:  %s\n", __LINE__, msg, _a.c_str(), _b.c_str()); } } while (0)

struct Trace {
	std::vector<int> stages;       // Stage bei jedem Wechsel (inkl. Wiedereintritt gleicher Stage nicht)
	std::vector<int> restAtEntry;  // loopRest beim Eintritt (1 Sample nach Wechsel gelesen)
	std::vector<int> exitAt;       // Index in stages, bei dem der Austritt auftrat (Stage VOR dem Wechsel)
	int exitSamples = 0, longestExit = 0, curExit = 0;
};

// Laeuft n Samples und protokolliert Stage-Wechsel und Austritts-Flanken.
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
		if (ex) { t.curExit++; t.exitSamples++; if (t.curExit > t.longestExit) t.longestExit = t.curExit; } else t.curExit = 0;
		prevExit = ex;
		if (r.afg.currentStage != last) {
			last = r.afg.currentStage;
			t.stages.push_back(last);
			t.restAtEntry.push_back((int) (r.loopRest + 0.5f));
		}
	}
	return t;
}
static std::string seq(const std::vector<int>& v, size_t from, size_t to, int off = 1) {
	std::ostringstream o;
	for (size_t i = from; i < v.size() && i < to; i++) o << (i > from ? " " : "") << v[i] + off; // off=1: Stage-Nummern wie am Panel (1-basiert)
	return o.str();
}
static void clearMods(Rig& r) { for (int i = 0; i < NUM_STAGES; i++) r.mod[i] = 0; }


static std::string rep(const std::string& s, int n) { std::string o; for (int i = 0; i < n; i++) o += (i ? " " : "") + s; return o; }

static void test_forward_exit() {
	printf("A Forward: Segment 1-4, 3 Durchlaeufe, danach Weiterlauf und Neuladen\n");
	Rig r; clearMods(r);
	r.mod[0] = MOD_FIRST; r.mod[3] = MOD_LAST; r.mod[15] = MOD_LAST;
	r.cfg[3] = 3;
	r.begin();
	Trace t = run(r, 400);
	std::string expect = rep("1 2 3 4", 3) + " 5 6 7 8 9 10 11 12 13 14 15 16 " + rep("1 2 3 4", 3);
	CHECK_EQ_STR(seq(t.stages, 0, 36), expect, "3 Durchlaeufe, Weiterlauf 5..16, Wrap, erneut 3 Durchlaeufe");
	CHECK(t.exitAt.size() >= 2 && t.exitAt[0] == 11 && t.exitAt[1] == 35, "Austrittsimpuls beim Verlassen von Stage 4 (je Besuch einmal)");
	CHECK_EQ_STR(seq(t.restAtEntry, 0, 14, 0), "3 3 3 3 2 2 2 2 1 1 1 1 0 0", "Restwert 3,2,1 je Durchlauf, 0 ausserhalb eines Zaehlers");
	CHECK_EQ_STR(seq(t.restAtEntry, 24, 36, 0), "3 3 3 3 2 2 2 2 1 1 1 1", "Zaehler wird im naechsten Besuch neu geladen");
	CHECK(t.longestExit <= 2, "Austrittsimpuls ~1 ms (1-2 Samples bei 1 ms/Sample)");
}

static void test_reverse_exit() {
	printf("B Reverse: Segment 5-8, 2 Durchlaeufe, Konfig an Last-Stage 8\n");
	Rig r; clearMods(r);
	r.mod[0] = MOD_FIRST; r.mod[4] = MOD_FIRST; r.mod[7] = MOD_LAST; r.mod[15] = MOD_LAST;
	r.cfg[7] = 2;
	r.afg.reverseMode = true;
	r.afg.jumpToStage(7, r.mod, true);
	r.begin();
	Trace t = run(r, 300);
	// Reverse: 8 7 6 5 | 8 7 6 5 | Austritt unter First -> 4 3 2 1 (Vorlauf, kein Zaehler) | Wrap bei Stage 1 -> 8, neu geladen
	std::string expect = rep("8 7 6 5", 2) + " 4 3 2 1 " + rep("8 7 6 5", 2);
	CHECK_EQ_STR(seq(t.stages, 0, 20), expect, "Reverse: 2 Durchlaeufe, dann Weiterlauf abwaerts");
	CHECK(!t.exitAt.empty() && t.exitAt[0] == 7, "Austritt beim Verlassen von Stage 5 (First)");
	CHECK_EQ_STR(seq(t.restAtEntry, 0, 12, 0), "2 2 2 2 1 1 1 1 0 0 0 0", "Restwert im Reverse (Kanal der Last-Stage)");
	CHECK_EQ_STR(seq(t.restAtEntry, 12, 16, 0), "2 2 2 2", "Neu geladen nach dem Wrap");
}

static void test_skip() {
	printf("C Skip: uebersprungene Stage / uebersprungene Last-Stage\n");
	{
		Rig r; clearMods(r);
		r.mod[0] = MOD_FIRST; r.mod[3] = MOD_LAST; r.mod[15] = MOD_LAST; r.mod[1] |= MOD_SKIP;
		r.cfg[3] = 2;
		r.begin();
		Trace t = run(r, 100);
		CHECK_EQ_STR(seq(t.stages, 0, 9), "1 3 4 1 3 4 5 6 7", "Skip mitten im Segment");
	}
	{
		Rig r; clearMods(r);
		r.mod[0] = MOD_FIRST; r.mod[3] = MOD_LAST | MOD_SKIP; r.mod[15] = MOD_LAST;
		r.cfg[3] = 2;
		r.begin();
		Trace t = run(r, 100);
		// Wrap findet in der Skip-Schleife statt (Last-Stage 4 ist Skip): trotzdem 2 Durchlaeufe
		CHECK_EQ_STR(seq(t.stages, 0, 9), "1 2 3 1 2 3 5 6 7", "Skip auf der Last-Stage: Wrap in Skip-Schleife zaehlt");
		CHECK(t.exitAt.size() >= 1 && t.exitAt[0] == 5, "Austritt aus Skip-Schleife");
	}
	{
		Rig r; clearMods(r);
		for (int i = 0; i < NUM_STAGES; i++) r.mod[i] = MOD_SKIP;
		r.mod[0] |= MOD_FIRST; r.mod[15] |= MOD_LAST;
		r.cfg[15] = 2;
		r.begin();
		Trace t = run(r, 300); // darf nicht haengen
		CHECK(true, "Segment nur aus Skip-Stages haengt nicht (Lauf beendet)");
		CHECK(r.loopRest >= 0.f && r.loopRest <= 9.f, "Restwert bleibt gueltig");
	}
}

static void test_array_edge() {
	printf("D Array-Kante (Option A): Austritt wrapt global, Ablauf unveraendert\n");
	{
		Rig r; clearMods(r);
		r.mod[2] = MOD_FIRST; r.mod[15] = MOD_LAST;
		r.cfg[15] = 2;
		r.afg.jumpToStage(2, r.mod, true);
		r.begin();
		Trace t = run(r, 200);
		Rig q; clearMods(q); q.mod[2] = MOD_FIRST; q.mod[15] = MOD_LAST; q.afg.jumpToStage(2, q.mod, true); q.begin();
		Trace u = run(q, 200);
		CHECK(t.stages == u.stages, "Forward: Stage-Folge identisch zu ohne Zaehler (Ein-Segment-Fall)");
		CHECK(t.exitAt.size() >= 2 && t.exitAt[0] == 27, "Forward: Austritt nach 2 Durchlaeufen (Taktteiler durch 2)");
		CHECK_EQ_STR(seq(t.stages, 0, 3), "3 4 5", "Wrap geht auf niedrigste First (Stage 3)");
	}
	{
		Rig r; clearMods(r);
		r.mod[0] = MOD_FIRST; r.mod[15] = MOD_LAST;
		r.cfg[15] = 2;
		r.afg.reverseMode = true;
		r.afg.jumpToStage(15, r.mod, true);
		r.begin();
		Trace t = run(r, 200);
		Rig q; clearMods(q); q.mod[0] = MOD_FIRST; q.mod[15] = MOD_LAST; q.afg.reverseMode = true; q.afg.jumpToStage(15, q.mod, true); q.begin();
		Trace u = run(q, 200);
		CHECK(t.stages == u.stages, "Reverse: Stage-Folge identisch zu ohne Zaehler");
		CHECK(t.exitAt.size() >= 1 && t.exitAt[0] == 31, "Reverse: Austritt nach 2 Durchlaeufen, global auf hoechste Last");
	}
}

static void test_jumps() {
	printf("E Strobe / Recall / Reset / Continuous\n");
	Rig r; clearMods(r);
	r.mod[0] = MOD_FIRST; r.mod[3] = MOD_LAST; r.mod[15] = MOD_LAST;
	r.cfg[3] = 3;
	r.begin();
	run(r, 9); // 1 2 3 4 1 2 3 4 1... -> zweiter Durchlauf
	CHECK(r.afg.loopRemaining == 2 || r.afg.loopRemaining == 3, "Ausgangslage im Zaehler");
	// laeuft bis Rest == 2
	for (int i = 0; i < 40 && (int)(r.loopRest + .5f) != 2; i++) r.step();
	CHECK((int)(r.loopRest + .5f) == 2, "Rest 2 erreicht");
	// Strobe innerhalb des Segments: Zaehler laeuft weiter
	r.step(false, 1, true);
	CHECK(r.afg.currentStage == 1 && (int)(r.loopRest + .5f) == 2, "Strobe im selben Segment: kein Neuladen");
	// Recall (jumpToStage ohne forceReload) im selben Segment
	r.afg.jumpToStage(2, r.mod); r.step();
	CHECK((int)(r.loopRest + .5f) == 2, "Recall im selben Segment: kein Neuladen");
	// Strobe in anderes Segment (5..16): Rest 0
	r.step(false, 9, true);
	CHECK(r.afg.currentStage == 9 && (int)(r.loopRest + .5f) == 0, "Strobe ausserhalb: Rest 0 (kein Zaehler)");
	// zurueck ins Segment: neu geladen
	r.step(false, 1, true);
	CHECK((int)(r.loopRest + .5f) == 3, "Zurueck ins Segment: Zaehler voll neu geladen");
	// weiter bis Rest 1, dann Reset -> neu laden auf 3, Stage = niedrigste First
	for (int i = 0; i < 80 && (int)(r.loopRest + .5f) != 1; i++) r.step();
	CHECK((int)(r.loopRest + .5f) == 1, "Rest 1 erreicht");
	r.step(false, 0, false, true);
	CHECK(r.afg.currentStage == 0 && (int)(r.loopRest + .5f) == 3, "Reset: Zaehler wird auch im selben Segment neu geladen");
	// Continuous: kein Zaehlen, Rest bleibt, Stage folgt Adresse
	for (int i = 0; i < 10; i++) r.step(true, i % 4);
	CHECK((int)(r.loopRest + .5f) == 3, "Continuous im Segment: Zaehler unveraendert");
	// resetLoopCounter (onReset / Global Clear): lädt beim naechsten Bedarf frisch
	for (int i = 0; i < 20 && (int)(r.loopRest + .5f) == 3; i++) { r.step(); }
	r.afg.resetLoopCounter(); r.step();
	CHECK(r.afg.loopLoaded && (int)(r.loopRest + .5f) == 3, "resetLoopCounter(): Zaehler startet neu mit N");
}

static void test_pendulum_and_misc() {
	printf("F Pendulum inaktiv / G Ein-Segment-Taktteiler / H Mute / I Fallback-Ende / J Clamp\n");
	{
		Rig a, b; clearMods(a); clearMods(b);
		for (Rig* x : {&a, &b}) { x->mod[0] = MOD_FIRST; x->mod[3] = MOD_LAST; x->mod[15] = MOD_LAST; x->afg.pendulumMode = true; }
		a.cfg[3] = 2;
		a.begin(); b.begin();
		Trace ta = run(a, 300), tb = run(b, 300);
		CHECK(ta.stages == tb.stages, "Pendulum: Ablauf unveraendert");
		CHECK(ta.exitAt.empty() && a.loopRest == 0.f, "Pendulum: kein Austritt, Restwert 0");
	}
	{
		Rig a, b; a.cfg[15] = 3;
		a.begin(); b.begin();
		Trace ta = run(a, 500), tb = run(b, 500);
		CHECK(ta.stages == tb.stages, "Ein Segment: Ablauf unveraendert");
		CHECK(ta.exitAt.size() >= 2 && ta.exitAt[1] - ta.exitAt[0] == 48, "Ein Segment: Austritt alle 3 Durchlaeufe (48 Stages)");
	}
	{
		Rig r; clearMods(r);
		r.mod[0] = MOD_FIRST; r.mod[3] = MOD_LAST | MOD_MUTE; r.mod[15] = MOD_LAST;
		r.cfg[3] = 1;
		r.begin();
		Trace t = run(r, 50);
		CHECK(!t.exitAt.empty() && t.exitAt[0] == 3, "Austrittsimpuls unabhaengig von MOD_MUTE; 1 Durchlauf = Austritt am ersten Wrap");
	}
	{
		Rig r; clearMods(r);   // keinerlei First/Last: Fallback-Ende Stage 16
		r.cfg[15] = 2;
		r.begin();
		Trace t = run(r, 150);
		CHECK(!t.exitAt.empty() && t.exitAt[0] == 31, "Ohne Last-Bit: Fallback-Ende Stage 16 (Kanal 16)");
	}
	{
		Rig r; r.cfg[15] = 15;  r.begin(); run(r, 10);
		CHECK(r.loopRest == 9.f, "Wert > 9 wird auf 9 geklemmt");
	}
}

int main() {
	test_forward_exit();
	test_reverse_exit();
	test_skip();
	test_array_edge();
	test_jumps();
	test_pendulum_and_misc();
	printf("\n%d Pruefungen ok, %d Fehler\n", g_ok, g_fail);
	return g_fail ? 1 : 0;
}
