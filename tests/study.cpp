// Machbarkeitsstudie: Tonhoehe fuer Anna-Maria (Engine AnnaMariaLpg.hpp unveraendert)
// Ansaetze:
//   ORIG  Original ohne V/Oct
//   TS    jetzige Zeitskalierung (pitchHz)
//   RES   gestimmter Resonator VOR dem LPG (x + wet*res(x)), LPG-Rueckkopplung 0
//   KS    LPG in einer Delay-Schleife (Karplus-Strong), Laufzeit mit Phasenlaufzeit des LPG kompensiert
#include "AnnaMariaLpg.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <string>

typedef AnnaMariaLpg L;
static const double FS = 48000.0;
static const double PI = 3.14159265358979323846;

enum Mode { ORIG, TS, RES, KS };

// Phasenlaufzeit (s) des LPG (fb = 0, Lowpass-Stufe) bei Vactrol-Widerstand Rf an der Frequenz f0
static double phaseDelay(double Rf, double f0) {
	const double Ra = L::Ralp, C3 = L::C3lp;
	const double p = 1.0 / (Rf * L::C1), q = 1.0 / (Ra * L::C1), r = L::C1 / L::C2, b4 = C3 / L::C2;
	const double A = 1.0 + b4, B = 2.0 * p * r + (p + q) * A, C = p * r * (p + 2.0 * q);
	const double Om = 2.0 * PI * f0 / FS;
	const double w = 2.0 * FS * std::tan(Om / 2.0);
	const double ph = -std::atan2(B * w, C - A * w * w);
	return -ph / (2.0 * PI * f0);
}

struct Sim {
	L lpg;
	L::Params p;
	Mode mode;
	double f0;
	// Resonator
	double rc1 = 0, rc2 = 0, rb0 = 0, ry1 = 0, ry2 = 0, wet = 1.0;
	// KS
	std::vector<double> h;
	unsigned wp = 0, mask = 8191;
	double g = 0.0;
	double lastD = 0;

	Sim(Mode m, double fnote, double fb, double par, double t60) : mode(m), f0(fnote) {
		lpg.setSampleRate(FS);
		p = L::Params();
		p.cv = 10.0; p.offset = 0.0; p.trim = 1.0;
		p.mode = L::MODE_LOWPASS; p.fbnorm = true; p.fb = fb;
		if (m == TS) p.pitchHz = fnote;
		if (m == RES) {
			p.fb = 0.0;
			wet = par;
			const double w0 = 2.0 * PI * fnote / FS;
			const double r = std::pow(10.0, -3.0 / (t60 * FS));
			rc1 = 2.0 * r * std::cos(w0); rc2 = -r * r; rb0 = std::sin(w0);
		}
		if (m == KS) {
			p.fb = 0.0;
			g = par;
			h.assign(mask + 1, 0.0);
		}
		lpg.warmStart(p);
	}

	double delayed() {
		const double tau = phaseDelay(lpg.currentRfv(), f0);
		double D = FS / f0 - tau * FS;
		if (D < 2.0) D = 2.0;
		lastD = D;
		const int M = (int) std::floor(D);
		const double mu = D - M;
		const double f = 1.0 - mu;
		const double wm1 = -f * (f - 1.0) * (f - 2.0) / 6.0;
		const double w0 = (f + 1.0) * (f - 1.0) * (f - 2.0) / 2.0;
		const double w1 = -(f + 1.0) * f * (f - 2.0) / 2.0;
		const double w2 = (f + 1.0) * f * (f - 1.0) / 6.0;
		auto at = [&](int lag) { return h[(wp - (unsigned) lag) & mask]; };
		return wm1 * at(M + 2) + w0 * at(M + 1) + w1 * at(M) + w2 * at(M - 1);
	}

	double step(double x, double cv) {
		p.cv = cv;
		switch (mode) {
		case ORIG:
		case TS:
			return lpg.process(x, p);
		case RES: {
			const double y = rb0 * x + rc1 * ry1 + rc2 * ry2;
			ry2 = ry1; ry1 = y;
			return lpg.process(x + wet * y, p);
		}
		default: {
			double fbv = 0.0;
			if (g != 0.0) fbv = g * delayed();
			const double y = lpg.process(x + fbv, p);
			h[wp & mask] = y; wp++;
			return y;
		}
		}
	}
};

struct PM { double f, rms, conf; bool ok; };

static PM measure(const std::vector<double>& y, double t0ms, double f0) {
	PM r = {0, 0, 0, false};
	const double T0 = FS / f0;
	const int N = (int) std::max(0.024 * FS, 6.0 * T0);
	const int i0 = (int) (t0ms * 1e-3 * FS);
	const int lagMin = std::max(2, (int) (0.7 * T0)), lagMax = (int) (2.3 * T0);
	if (i0 + N + lagMax + 2 > (int) y.size()) return r;
	double e = 0;
	for (int i = i0; i < i0 + N; i++) e += y[i] * y[i];
	r.rms = std::sqrt(e / N);
	if (r.rms < 1e-14) return r;
	std::vector<double> ac(lagMax + 2, 0.0);
	for (int l = lagMin - 1; l <= lagMax + 1; l++) {
		double s = 0, e2 = 0;
		for (int i = i0; i < i0 + N; i++) { s += y[i] * y[i + l]; e2 += y[i + l] * y[i + l]; }
		ac[l] = (e2 > 0 && e > 0) ? s / std::sqrt(e * e2) : 0.0;
	}
	double rmax = -2;
	for (int l = lagMin; l <= lagMax; l++) rmax = std::max(rmax, ac[l]);
	int best = -1;
	for (int l = lagMin; l <= lagMax; l++)
		if (ac[l] >= 0.95 * rmax && ac[l] >= ac[l - 1] && ac[l] >= ac[l + 1]) { best = l; break; }
	if (best < 0) return r;
	const double a = ac[best - 1], b = ac[best], c = ac[best + 1];
	const double den = a - 2.0 * b + c;
	const double off = den != 0 ? 0.5 * (a - c) / den : 0.0;
	r.f = FS / (best + off);
	r.conf = b;
	r.ok = true;
	return r;
}

static std::vector<double> pluck(Mode m, double f0, double fb, double par, double t60, double relMs, double durMs) {
	Sim s(m, f0, fb, par, t60);
	const int N = (int) (durMs * 1e-3 * FS);
	std::vector<double> y(N);
	for (int n = 0; n < N; n++) {
		const double t = n * 1000.0 / FS;
		const double cv = relMs > 0 ? 10.0 * std::max(0.0, 1.0 - t / relMs) : 10.0;
		y[n] = s.step(n == 0 ? 1.0 : 0.0, cv);
	}
	return y;
}

static void report(const char* label, Mode m, double f0, double fb, double par, double t60, double rel, const double* starts, int ns) {
	std::vector<double> y = pluck(m, f0, fb, par, t60, rel, 520);
	printf("  %-12s %7.1f Hz |", label, f0);
	double ref = 0;
	for (int k = 0; k < ns; k++) {
		PM r = measure(y, starts[k], f0);
		if (k == 0) ref = r.rms;
		if (!r.ok || r.rms < ref * 1e-3 || r.conf < 0.6) printf("      --        |");
		else printf(" %+7.1fct %+5.1fdB |", 1200.0 * std::log2(r.f / f0), 20.0 * std::log10(r.rms / ref));
	}
	printf("\n");
}

static double ringLevelDb(Mode m, double f0, double fb, double par, double t60) {
	double acc = 0; int cnt = 0;
	for (int seed = 0; seed < 120; seed++) {
		std::mt19937 rng(100 + seed);
		std::normal_distribution<double> nd(0.0, 0.3);
		Sim s(m, f0, fb, par, t60);
		const int N = (int) (0.085 * FS), Nb = (int) (0.020 * FS);
		const int a = (int) (0.025 * FS), b = (int) (0.075 * FS);
		for (int n = 0; n < N; n++) {
			const double y = s.step(n < Nb ? nd(rng) : 0.0, 10.0);
			if (n >= a && n < b) { acc += y * y; cnt++; }
		}
	}
	return 10.0 * std::log10(acc / cnt);
}

static double sineGainDb(Mode m, double fnote, double fb, double fsig) {
	Sim s(m, fnote, fb, 0.0, 1.0);
	const int N = (int) (0.35 * FS);
	double pk = 0;
	for (int n = 0; n < N; n++) {
		const double y = s.step(0.05 * std::sin(2.0 * PI * fsig * n / FS), 10.0);
		if (n > N * 2 / 3) pk = std::max(pk, std::fabs(y));
	}
	return 20.0 * std::log10(pk / 0.05);
}

static double rfStatic(double cv) {
	L e; e.setSampleRate(FS);
	L::Params p; p.cv = cv; p.offset = 0.0; p.trim = 1.0; p.mode = L::MODE_LOWPASS; p.fb = 0.0;
	e.warmStart(p); e.process(0.0, p);
	return e.currentRfv();
}

#ifndef STUDY_NO_MAIN
int main() {
	const double notes[] = {65.406, 130.813, 261.626, 523.251, 1046.502, 2093.005};
	const char* nn[] = {"C2", "C3", "C4", "C5", "C6", "C7"};

	// ------------------------------------------------------------------ A
	printf("== A) Nur die Rueckkopplung (d1) veraendern ==\n");
	printf("   Klingfrequenz der Engine-Formel (ringOmega) bei fb = 0 / 50 %% / 100 %% (normiert), Abweichung in Cent gegen fb = 0\n");
	{
		const double Rfs[] = {L::rfOpen(), 10e3, 100e3, 1e6};
		for (double Rf : Rfs) {
			const double Ra = L::Ralp, C3 = L::C3lp;
			const double amax = (2.0 * L::C1 * Ra + (L::C2 + C3) * (Ra + Rf)) / (C3 * Ra);
			const double f0 = L::ringOmega(Rf, 0.0, C3, Ra) / (2 * PI);
			const double f5 = L::ringOmega(Rf, 0.5 * amax, C3, Ra) / (2 * PI);
			const double f1 = L::ringOmega(Rf, amax, C3, Ra) / (2 * PI);
			printf("   Rf = %9.0f Ohm: fb0 %9.1f Hz, fb50 %+7.1f ct, fb100 %+7.1f ct\n", Rf, f0,
				1200 * std::log2(f5 / f0), 1200 * std::log2(f1 / f0));
		}
	}

	// ------------------------------------------------------------------ Kontrolle Bitgleichheit
	printf("\n== Kontrolle: RES mit wet = 0 und KS mit g = 0 gegen ORIG (Rauschen, 1 s, fb 0,97 / 0) ==\n");
	for (int variant = 0; variant < 2; variant++) {
		std::mt19937 rng(7);
		std::normal_distribution<double> nd(0.0, 0.3);
		Sim a(ORIG, 261.6, variant ? 0.0 : 0.97, 0, 1), b(RES, 261.6, 0, 0.0, 1.0), c(KS, 261.6, 0, 0.0, 1.0);
		if (!variant) { b.p.fb = 0.97; c.p.fb = 0.97; }   // gleiche LPG-Rueckkopplung wie ORIG
		double dmaxB = 0, dmaxC = 0;
		for (int n = 0; n < (int) FS; n++) {
			const double x = nd(rng);
			const double cv = 5.0 + 5.0 * std::sin(2 * PI * 3.0 * n / FS);
			const double ya = a.step(x, cv), yb = b.step(x, cv), yc = c.step(x, cv);
			dmaxB = std::max(dmaxB, std::fabs(ya - yb)); dmaxC = std::max(dmaxC, std::fabs(ya - yc));
		}
		printf("   LPG-fb %s: max |ORIG - RES(wet 0)| = %g, max |ORIG - KS(g 0)| = %g\n", variant ? "0   " : "0,97", dmaxB, dmaxC);
	}

	// ------------------------------------------------------------------ Dumpfheit der Zeitskalierung
	printf("\n== Klangcharakter bei offenem Gate: Verstaerkung (dB) fuer Sinus 200 Hz / 1 / 4 / 10 kHz, LPG fb = 0 ==\n");
	printf("   (Spalte 'ORIG' ist auch der Direktpfad von RES und von KS mit g = 0)\n");
	{
		const double fsig[] = {200, 1000, 4000, 10000};
		printf("   %-10s", "ORIG");
		for (double f : fsig) printf(" %7.1f", sineGainDb(ORIG, 261.6, 0.0, f));
		printf("\n");
		for (int k = 0; k < 6; k += 2) {
			printf("   TS %-7s", nn[k]);
			for (double f : fsig) printf(" %7.1f", sineGainDb(TS, notes[k], 0.0, f));
			printf("\n");
		}
		printf("   mit fb = 0,97:\n");
		printf("   %-10s", "ORIG");
		for (double f : fsig) printf(" %7.1f", sineGainDb(ORIG, 261.6, 0.97, f));
		printf("\n");
		for (int k = 0; k < 6; k += 2) {
			printf("   TS %-7s", nn[k]);
			for (double f : fsig) printf(" %7.1f", sineGainDb(TS, notes[k], 0.97, f));
			printf("\n");
		}
	}

	// ------------------------------------------------------------------ Tonhoehe bei dauernd offenem Gate
	printf("\n== Dauerhaft offenes Gate: Tonhoehe-Abweichung (Cent) und Pegel nach 0 / 300 ms ==\n");
	{
		const double st[] = {0, 300};
		for (int k = 0; k < 6; k++) {
			report("TS fb97", TS, notes[k], 0.97, 0, 1, 0, st, 2);
			report("RES T60 1,5s", RES, notes[k], 0, 1.0, 1.5, 0, st, 2);
			report("KS g 0,99", KS, notes[k], 0, 0.99, 1, 0, st, 2);
		}
	}

	// ------------------------------------------------------------------ Pluck mit fallender Huellkurve
	const double st2[] = {0, 20, 60, 120};
	for (int rel = 200; rel >= 20; rel -= 180) {
		printf("\n== Pluck: Impuls bei offenem Gate, dann Huellkurve 10 V -> 0 V linear in %d ms (THRESHOLD 7,5 V, Referenz-Vactrol) ==\n", rel);
		printf("   Spalten: Fenster ab 0 / 20 / 60 / 120 ms; Cent gegen Sollnote, dB gegen Fenster 0; '--' = nicht mehr messbar\n");
		for (int k = 1; k < 5; k++) {
			report("TS fb97", TS, notes[k], 0.97, 0, 1, rel, st2, 4);
			report("TS fb99", TS, notes[k], 0.99, 0, 1, rel, st2, 4);
			report("RES T60 1,5s", RES, notes[k], 0, 1.0, 1.5, rel, st2, 4);
			report("KS g 0,99", KS, notes[k], 0, 0.99, 1, rel, st2, 4);
			printf("\n");
		}
	}

	// ------------------------------------------------------------------ Pegel bei Rauschanregung
	printf("== Rauschburst 20 ms (0,3 eff.), Ringpegel 25...75 ms danach in dB, Gate offen, relativ zu C4 desselben Ansatzes ==\n");
	{
		struct Row { const char* lab; Mode m; double fb, par, t60; };
		const Row rows[] = {{"TS fb97", TS, 0.97, 0, 1}, {"TS fb99", TS, 0.99, 0, 1},
			{"RES T60 0,3s", RES, 0, 1.0, 0.3}, {"RES T60 1,5s", RES, 0, 1.0, 1.5},
			{"KS g 0,97", KS, 0, 0.97, 1}, {"KS g 0,99", KS, 0, 0.99, 1}};
		printf("   %-14s", "");
		for (int k = 0; k < 6; k++) printf(" %7s", nn[k]);
		printf("   (absolut bei C4)\n");
		for (const Row& r : rows) {
			double v[6];
			for (int k = 0; k < 6; k++) v[k] = ringLevelDb(r.m, notes[k], r.fb, r.par, r.t60);
			printf("   %-14s", r.lab);
			for (int k = 0; k < 6; k++) printf(" %+7.1f", v[k] - v[2]);
			printf("   (%.1f dB)\n", v[2]);
		}
	}

	// ------------------------------------------------------------------ D: Rf ueber Gate-CV
	printf("\n== D) Tonhoehe ueber den Gate-Pfad (Rf muss zur Note passen, kein Code noetig) ==\n");
	printf("   Rf(cv) statisch, THRESHOLD 7,5 V, DEPTH 100 %%: ");
	for (double cv : {0.0, 3.0, 5.0, 7.0, 7.3, 7.4, 7.45, 7.5, 7.6, 8.0, 10.0}) printf("%.2f V = %.3g | ", cv, rfStatic(cv));
	printf("\n");
	for (int k = 0; k < 6; k++) {
		const double Ra = L::Ralp, C3 = L::C3lp;
		auto fRing = [&](double Rf) {
			const double a = 0.97 * (2.0 * L::C1 * Ra + (L::C2 + C3) * (Ra + Rf)) / (C3 * Ra);
			return L::ringOmega(Rf, a, C3, Ra) / (2 * PI);
		};
		double lo = 1e3, hi = 1e9;
		for (int i = 0; i < 200; i++) { const double mid = std::sqrt(lo * hi); if (fRing(mid) > notes[k]) lo = mid; else hi = mid; }
		const double RfStar = std::sqrt(lo * hi);
		double cl = 0, ch = 10;
		for (int i = 0; i < 60; i++) { const double mid = 0.5 * (cl + ch); if (rfStatic(mid) > RfStar) cl = mid; else ch = mid; }
		const double cvStar = 0.5 * (cl + ch);
		// Empfindlichkeit: Cent pro mV
		const double f1 = fRing(rfStatic(cvStar - 0.0005)), f2 = fRing(rfStatic(cvStar + 0.0005));
		printf("   %s (%7.1f Hz): Rf* = %10.0f Ohm, cv* = %.4f V, Aenderung %+.1f ct pro +1 mV\n", nn[k], notes[k], RfStar, cvStar,
			1200 * std::log2(f2 / f1) / 1.0);
	}
	// ------------------------------------------------------------------ Messfehler bei abklingendem Sinus
	printf("\n== Kontrolle des Messverfahrens: exakter Sinus mit Abklingen exp(-t/tau), Fenster ab 20 ms ==\n");
	for (double tau : {0.004, 0.008, 0.02, 0.1}) {
		printf("   tau %5.1f ms:", tau * 1000);
		for (int k = 1; k < 5; k++) {
			std::vector<double> y((int) (0.3 * FS));
			for (size_t n = 0; n < y.size(); n++) y[n] = std::exp(-(n / FS) / tau) * std::sin(2 * PI * notes[k] * n / FS);
			PM r = measure(y, 20, notes[k]);
			if (r.ok) printf("  %s %+6.1f ct", nn[k], 1200 * std::log2(r.f / notes[k])); else printf("  %s   --   ", nn[k]);
		}
		printf("\n");
	}
	return 0;
}
#endif
