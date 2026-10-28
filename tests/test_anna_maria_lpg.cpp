// test_anna_maria_lpg.cpp — Standalone-Test fuer src/AnnaMariaLpg.hpp (ohne Rack/JACK)
//
// Bauen (C++11, wie das Plugin):
//   g++ -std=c++11 -O2 -Wall -Wextra -Isrc tests/test_anna_maria_lpg.cpp -o test_anna_maria_lpg
// Aufruf:
//   ./test_anna_maria_lpg data
//
// Was passiert:
//   1. Fuer jeden Testfall werden Eingangssignal und CV per Formel erzeugt (identisch
//      zu tests/ref/lpg_ref.py und tests/ref/vergleich_octave.m), die Engine im
//      Kaltstart (= Octave-Verhalten) darueber laufen gelassen und das Ergebnis
//      nach data/cpp_<fall>.csv geschrieben (Spalten n,x,cv,y).
//   2. Liegt data/ref_py_<fall>.csv (Python-Gegenprobe) oder data/ref_octave_<fall>.csv
//      (echtes Octave) vor, wird y verglichen. Toleranz: relativ zum Spitzenwert.
//   3. Ein paar Eigenschaftstests (Warmstart, Gate auf/zu, Endlichkeit) ohne Referenzdatei.

#include "AnnaMariaLpg.hpp"

#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static const int N = 24000;
static const double PI_D = 3.14159265358979323846;

struct Case {
	const char* name;
	double fs;
	int mode;
	int nl;
	int fbnorm;
	double fb;
	double offset;
	double trim;
};

// Muss mit CASES in lpg_ref.py und mit vergleich_octave.m uebereinstimmen.
static const Case CASES[] = {
	{"lp_lin_fb0",     48000, 1, 0, 1, 0.0, 0.5, 0.5},
	{"lp_lin_fb09",    48000, 1, 0, 1, 0.9, 0.3, 0.2},
	{"lp_nl_fb09",     48000, 1, 1, 1, 0.9, 0.3, 0.8},
	{"vca_lin",        48000, 2, 0, 0, 0.0, 0.5, 0.5},
	{"both_nl",        48000, 3, 1, 0, 0.1, 0.7, 0.5},
	{"lp_nl_abs12",    48000, 1, 1, 0, 1.2, 0.5, 1.0},
	{"lp_lin_fb09_44", 44100, 1, 0, 1, 0.9, 0.3, 0.2},
};
static const int NUM_CASES = sizeof(CASES) / sizeof(CASES[0]);

static void makeInputs(double fs, std::vector<double>& x, std::vector<double>& cv) {
	x.assign(N, 0.0);
	cv.assign(N, 0.0);
	for (int i = 0; i < N; i++) {
		double t = i / fs;
		x[i] = 1.0 * std::sin(2.0 * PI_D * 220.0 * t) + 0.5 * std::sin(2.0 * PI_D * 3100.0 * t);
		double c = 0.0;
		if (t >= 0.05 && t < 0.15) c = 6.0;
		else if (t >= 0.15 && t < 0.30) c = -2.0;
		else if (t >= 0.30) c = -2.0 + 10.0 * (t - 0.30) / 0.2;
		cv[i] = c;
	}
}

static bool readRef(const std::string& path, std::vector<double>& y) {
	FILE* f = std::fopen(path.c_str(), "r");
	if (!f) return false;
	char line[512];
	if (!std::fgets(line, sizeof(line), f)) { std::fclose(f); return false; }  // Kopfzeile
	y.clear();
	int n;
	double xx, cc, yy;
	while (std::fscanf(f, "%d,%lf,%lf,%lf", &n, &xx, &cc, &yy) == 4) y.push_back(yy);
	std::fclose(f);
	return !y.empty();
}

static double runCase(const Case& c, const std::string& dir, std::vector<double>& y) {
	std::vector<double> x, cv;
	makeInputs(c.fs, x, cv);
	AnnaMariaLpg lpg;
	lpg.setSampleRate(c.fs);
	lpg.reset();  // Kaltstart = Octave-Verhalten
	AnnaMariaLpg::Params p;
	p.offset = c.offset;
	p.trim = c.trim;
	p.fb = c.fb;
	p.mode = c.mode;
	p.fbnorm = c.fbnorm != 0;
	p.nl = c.nl != 0;
	y.assign(N, 0.0);
	double peak = 0.0;
	for (int i = 0; i < N; i++) {
		p.cv = cv[i];
		y[i] = lpg.process(x[i], p);
		assert(std::isfinite(y[i]));
		if (std::fabs(y[i]) > peak) peak = std::fabs(y[i]);
	}
	std::string path = dir + "/cpp_" + c.name + ".csv";
	FILE* f = std::fopen(path.c_str(), "w");
	assert(f);
	std::fprintf(f, "n,x,cv,y\n");
	for (int i = 0; i < N; i++)
		std::fprintf(f, "%d,%.17g,%.17g,%.17g\n", i, x[i], cv[i], y[i]);
	std::fclose(f);
	return peak;
}

static double maxAbsDiff(const std::vector<double>& a, const std::vector<double>& b) {
	assert(a.size() == b.size());
	double m = 0.0;
	for (size_t i = 0; i < a.size(); i++) {
		double d = std::fabs(a[i] - b[i]);
		if (d > m) m = d;
	}
	return m;
}

static double rms(const std::vector<double>& v, int from, int to) {
	double s = 0.0;
	for (int i = from; i < to; i++) s += v[i] * v[i];
	return std::sqrt(s / (to - from));
}

int main(int argc, char** argv) {
	std::string dir = (argc > 1) ? argv[1] : "data";
	int failures = 0;

	// ---- 1./2. Referenzfaelle ----
	for (int k = 0; k < NUM_CASES; k++) {
		std::vector<double> y;
		double peak = runCase(CASES[k], dir, y);
		std::printf("%-16s peak|y| = %.6g\n", CASES[k].name, peak);

		const char* refKinds[2] = {"ref_py_", "ref_octave_"};
		for (int r = 0; r < 2; r++) {
			std::vector<double> ref;
			std::string path = dir + "/" + refKinds[r] + CASES[k].name + ".csv";
			if (!readRef(path, ref)) continue;
			if (ref.size() != y.size()) {
				std::printf("   %s: Laenge %zu != %zu  FEHLER\n", refKinds[r], ref.size(), y.size());
				failures++;
				continue;
			}
			double err = maxAbsDiff(y, ref);
			double rel = err / (peak > 0 ? peak : 1.0);
			bool ok = rel < 1e-9;
			std::printf("   vs %-12s max|dy| = %.3e  (relativ %.3e)  %s\n",
				refKinds[r], err, rel, ok ? "OK" : "ABWEICHUNG");
			if (!ok) failures++;
		}
	}

	// ---- 3. Eigenschaftstests ----
	{
		// Warmstart: Vactrol-Zustand ist sofort stationaer, kein kuenstlicher Einschwingvorgang
		AnnaMariaLpg::Params p;
		p.cv = 3.0; p.offset = 0.5; p.trim = 0.5; p.fb = 0.3;
		AnnaMariaLpg warm, cold;
		warm.setSampleRate(48000.0); cold.setSampleRate(48000.0);
		warm.warmStart(p); cold.reset();
		warm.process(0.0, p); cold.process(0.0, p);
		double warmFirst = warm.currentRfv(), coldFirst = cold.currentRfv();
		for (int i = 0; i < 20000; i++) { warm.process(0.0, p); cold.process(0.0, p); }
		double warmLater = warm.currentRfv(), coldLater = cold.currentRfv();
		double warmDev = std::fabs(warmFirst - warmLater) / warmLater;
		double coldDev = std::fabs(coldFirst - coldLater) / coldLater;
		std::printf("Warmstart: Abweichung Rfv erstes Sample vs. eingeschwungen = %.3e (kalt: %.3e)\n",
			warmDev, coldDev);
		if (!(warmDev < 1e-9)) { std::printf("   FEHLER: Warmstart nicht stationaer\n"); failures++; }
		if (!(coldDev > 0.1))  { std::printf("   FEHLER: Kaltstart zeigt keinen Einschwingvorgang\n"); failures++; }
	}
	{
		// Gate zu / Gate auf im VCA-Modus: 1 kHz, Amplitude 1
		const double fs = 48000.0;
		const int len = 48000;
		std::vector<double> yClosed(len), yOpen(len);
		for (int pass = 0; pass < 2; pass++) {
			AnnaMariaLpg lpg;
			lpg.setSampleRate(fs);
			AnnaMariaLpg::Params p;
			p.cv = pass == 0 ? -10.0 : 10.0;
			p.offset = 0.5; p.trim = 0.5; p.fb = 0.0; p.mode = AnnaMariaLpg::MODE_VCA;
			p.fbnorm = false; p.nl = false;
			lpg.warmStart(p);
			for (int i = 0; i < len; i++) {
				double v = lpg.process(std::sin(2.0 * PI_D * 1000.0 * i / fs), p);
				(pass == 0 ? yClosed : yOpen)[i] = v;
			}
		}
		double rc = rms(yClosed, 24000, len), ro = rms(yOpen, 24000, len);
		std::printf("VCA-Modus: RMS Gate zu = %.3e, Gate auf = %.3e (Eingang RMS 0.707)\n", rc, ro);
		if (!(ro > 100.0 * rc)) { std::printf("   FEHLER: Gate schliesst nicht deutlich\n"); failures++; }
		if (!(ro > 0.3 && ro < 0.8)) { std::printf("   FEHLER: offene VCA-Verstaerkung unplausibel\n"); failures++; }
	}
	{
		// Lowpass-Modus: bei geschlossenem Gate werden Hoehen staerker bedaempft als Tiefen
		const double fs = 48000.0;
		const int len = 48000;
		double r[2];
		const double freqs[2] = {100.0, 5000.0};
		for (int k = 0; k < 2; k++) {
			AnnaMariaLpg lpg;
			lpg.setSampleRate(fs);
			AnnaMariaLpg::Params p;
			p.cv = 1.0; p.offset = 0.5; p.trim = 0.5; p.fb = 0.0;
			p.mode = AnnaMariaLpg::MODE_LOWPASS; p.fbnorm = true; p.nl = false;
			lpg.warmStart(p);
			std::vector<double> y(len);
			for (int i = 0; i < len; i++) y[i] = lpg.process(std::sin(2.0 * PI_D * freqs[k] * i / fs), p);
			r[k] = rms(y, 24000, len);
		}
		std::printf("Lowpass: RMS bei 100 Hz = %.4g, bei 5 kHz = %.4g\n", r[0], r[1]);
		if (!(r[0] > 3.0 * r[1])) { std::printf("   FEHLER: Lowpass daempft Hoehen nicht\n"); failures++; }
	}

	{
		// Option nlCorrected: bei winzigem Signal muss der NL-Zweig dem linearen entsprechen
		const double fs = 48000.0;
		const int len = 4800;
		double dev[2];
		for (int corr = 0; corr < 2; corr++) {
			AnnaMariaLpg a, b;
			a.setSampleRate(fs); b.setSampleRate(fs);
			AnnaMariaLpg::Params p;
			p.cv = 4.0; p.offset = 0.5; p.trim = 0.5; p.fb = 0.8; p.fbnorm = true;
			p.mode = AnnaMariaLpg::MODE_LOWPASS; p.nlCorrected = corr == 1;
			AnnaMariaLpg::Params pn = p; pn.nl = true; p.nl = false;
			a.warmStart(p); b.warmStart(pn);
			double d = 0.0, m = 0.0;
			for (int i = 0; i < len; i++) {
				double x = 1e-6 * std::sin(2.0 * PI_D * 800.0 * i / fs);
				double ya = a.process(x, p), yb = b.process(x, pn);
				d = std::max(d, std::fabs(ya - yb)); m = std::max(m, std::fabs(ya));
			}
			dev[corr] = d / m;
		}
		std::printf("NL-Zweig vs. linear bei 1 uV: Referenz %.3e, korrigiert %.3e\n", dev[0], dev[1]);
		if (!(dev[1] < 1e-4)) { std::printf("   FEHLER: korrigierter NL-Solver weicht im Kleinsignal ab\n"); failures++; }
	}
	{
		// Option vactrolByState: Anstiegszeit 10->90 % bei Gate-Sprung -10 -> +10 V (VCA, DC)
		const double fs = 48000.0;
		double rise[2];
		for (int st = 0; st < 2; st++) {
			AnnaMariaLpg lpg;
			lpg.setSampleRate(fs);
			AnnaMariaLpg::Params p;
			p.offset = 0.5; p.trim = 0.5; p.fb = 0.0; p.fbnorm = false; p.nl = false;
			p.mode = AnnaMariaLpg::MODE_VCA; p.vactrolByState = st == 1;
			p.cv = -10.0; lpg.warmStart(p);
			for (int i = 0; i < 4800; i++) lpg.process(1.0, p);
			p.cv = 10.0;
			const int len = 96000;
			std::vector<double> y(len);
			for (int i = 0; i < len; i++) y[i] = lpg.process(1.0, p);
			double fin = y[len - 1];
			int i10 = -1, i90 = -1;
			for (int i = 0; i < len; i++) {
				if (i10 < 0 && y[i] >= 0.1 * fin) i10 = i;
				if (i90 < 0 && y[i] >= 0.9 * fin) i90 = i;
			}
			rise[st] = 1000.0 * (i90 - i10) / fs;
		}
		std::printf("Vactrol-Anstieg 10->90 %%: Referenz %.1f ms, zustandsbasiert %.1f ms\n", rise[0], rise[1]);
		if (!(rise[0] > 100.0)) { std::printf("   FEHLER: Referenz-Anstieg nicht langsam\n"); failures++; }
		if (!(rise[1] < 30.0))  { std::printf("   FEHLER: zustandsbasierter Anstieg nicht schnell\n"); failures++; }
	}

	{
		// Option vactrolTimeScale: Faktor 1 = unveraendert, Anstiegszeit skaliert mit dem Faktor
		const double fs = 48000.0;
		double rise[3], scales[3] = {1.0, 0.3, 3.0};
		for (int k = 0; k < 3; k++) {
			AnnaMariaLpg lpg;
			lpg.setSampleRate(fs);
			AnnaMariaLpg::Params p;
			p.offset = 0.5; p.trim = 0.5; p.fb = 0.0; p.fbnorm = false; p.nl = false;
			p.mode = AnnaMariaLpg::MODE_VCA; p.vactrolByState = true; p.vactrolTimeScale = scales[k];
			p.cv = -10.0; lpg.warmStart(p);
			for (int i = 0; i < 4800; i++) lpg.process(1.0, p);
			p.cv = 10.0;
			const int len = 96000;
			std::vector<double> y(len);
			for (int i = 0; i < len; i++) y[i] = lpg.process(1.0, p);
			double fin = y[len - 1];
			int i10 = -1, i90 = -1;
			for (int i = 0; i < len; i++) {
				if (i10 < 0 && y[i] >= 0.1 * fin) i10 = i;
				if (i90 < 0 && y[i] >= 0.9 * fin) i90 = i;
			}
			rise[k] = 1000.0 * (i90 - i10) / fs;
		}
		std::printf("Vactrol-Zeitfaktor (zustandsbasiert) Anstieg 10->90 %%: x1 %.1f ms, x0.3 %.1f ms, x3 %.1f ms\n", rise[0], rise[1], rise[2]);
		if (!(rise[1] < rise[0] && rise[0] < rise[2])) { std::printf("   FEHLER: Zeitfaktor wirkt nicht monoton\n"); failures++; }
		// Faktor 1 muss bitgleich zur Standardeinstellung sein
		AnnaMariaLpg a, b;
		a.setSampleRate(fs); b.setSampleRate(fs);
		AnnaMariaLpg::Params pa, pb;
		pa.cv = pb.cv = 3.0; pb.vactrolTimeScale = 1.0;
		a.warmStart(pa); b.warmStart(pb);
		double dmax = 0;
		for (int i = 0; i < 4800; i++) {
			double x = std::sin(0.05 * i);
			pa.cv = pb.cv = (i % 2400 < 1200) ? 8.0 : -2.0;
			dmax = std::max(dmax, std::fabs(a.process(x, pa) - b.process(x, pb)));
		}
		if (!(dmax == 0.0)) { std::printf("   FEHLER: Zeitfaktor 1 weicht ab (%.3e)\n", dmax); failures++; }
	}

	if (failures == 0) std::printf("\nALLE TESTS BESTANDEN\n");
	else std::printf("\n%d TEST(S) FEHLGESCHLAGEN\n", failures);
	return failures == 0 ? 0 : 1;
}
