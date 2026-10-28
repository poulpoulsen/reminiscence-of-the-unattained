// doku_messungen.cpp — Messungen fuer die Anna-Maria-Dokumentation (ohne Rack)
//
// Misst mit der Engine (src/AnnaMariaLpg.hpp), wie Offset, Trim, CV, Mode, Feedback,
// NL und DRIVE auf Ausgang und Klang wirken. Alle Zahlen der Dokumentation stammen
// aus diesem Programm und lassen sich damit nachvollziehen.
//
// Bauen:   g++ -std=c++11 -O2 -Wall -Wextra -Isrc tests/doku_messungen.cpp -o doku_messungen
// Aufruf:  ./doku_messungen > doku_messungen_ausgabe.txt
//
// Konventionen: fs = 48 kHz, Engine in "Modellspannung" (+-1), Messpegel klein (0,1),
// damit nichts saettigt (ausser bei den NL-Messungen). Kanal-Parameter wie im Modul:
// "cv" ist die CV nach dem Attenuverter, "fb" normiert (a_norm), wenn nicht anders vermerkt.

#include "AnnaMariaLpg.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

static const double FS = 48000.0;
static const double PI_D = 3.14159265358979323846;

typedef AnnaMariaLpg Lpg;
typedef AnnaMariaLpg::Params P;

// Modell-Optionen (global, fliessen in makeP ein)
static bool gState = false;   // vactrolByState
static bool gCorr = false;    // nlCorrected

static P makeP(int mode, double cv, double offset, double trim, double fb, bool fbnorm, bool nl) {
	P p;
	p.mode = mode; p.cv = cv; p.offset = offset; p.trim = trim;
	p.fb = fb; p.fbnorm = fbnorm; p.nl = nl;
	p.vactrolByState = gState; p.nlCorrected = gCorr;
	return p;
}

// Amplitude der Frequenz f im Signal y[start..end) (Goertzel)
static double goertzelAmp(const std::vector<double>& y, size_t start, size_t end, double f) {
	double w = 2.0 * PI_D * f / FS, c = 2.0 * std::cos(w), s1 = 0, s2 = 0;
	for (size_t i = start; i < end; i++) { double s = y[i] + c * s1 - s2; s2 = s1; s1 = s; }
	double n = (double)(end - start);
	return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / n;
}

// Betrag der Uebertragungsfunktion bei f (statisches CV, Warmstart, Sinus 0,1)
static double mag(const P& p, double f, int settleSamples = 40000, int measSamples = 16384) {
	Lpg l; l.setSampleRate(FS); l.warmStart(p);
	const double amp = 0.1;
	std::vector<double> y(settleSamples + measSamples);
	for (int i = 0; i < settleSamples + measSamples; i++)
		y[i] = l.process(amp * std::sin(2.0 * PI_D * f * i / FS), p);
	return goertzelAmp(y, settleSamples, settleSamples + measSamples, f) / amp;
}

static double db(double x) { return 20.0 * std::log10(x > 1e-12 ? x : 1e-12); }

// Vactrol-Widerstand im eingeschwungenen Zustand
static double rfOf(const P& p) {
	Lpg l; l.setSampleRate(FS); l.warmStart(p);
	l.process(0.0, p);
	return l.currentRfv();
}

// -3-dB-Frequenz (relativ zu |H(20 Hz)|), 0 = kein Abfall bis 20 kHz
static double f3db(const P& p, double* g0out = 0) {
	double g0 = mag(p, 20.0, 30000, 16384);
	if (g0out) *g0out = g0;
	double target = g0 / std::sqrt(2.0);
	double fprev = 20.0, gprev = g0;
	for (int i = 1; i <= 80; i++) {
		double f = 20.0 * std::pow(1000.0, i / 80.0);   // 20 Hz .. 20 kHz
		double g = mag(p, f, 12000, 8192);
		if (g < target) {
			double t = (gprev - target) / (gprev - g);   // lineare Interpolation (log-f)
			return std::exp(std::log(fprev) + t * (std::log(f) - std::log(fprev)));
		}
		fprev = f; gprev = g;
	}
	return 0.0;
}

// Resonanzspitze: max |H| und Frequenz
static void peak(const P& p, double* gpk, double* fpk) {
	double best = 0, fb = 0;
	for (int i = 0; i <= 120; i++) {
		double f = 50.0 * std::pow(400.0, i / 120.0);   // 50 Hz .. 20 kHz
		double g = mag(p, f, 30000, 8192);
		if (g > best) { best = g; fb = f; }
	}
	*gpk = best; *fpk = fb;
}

// CV, bei der die Verstaerkung im VCA-Modus (1 kHz) die Haelfte des offenen Wertes erreicht
static double cvThreshold(double offset, double trim, double frac = 0.5) {
	const double open = 0.6334;   // feste offene VCA-Verstaerkung (unabhaengig von Offset/Trim)
	double lo = -10.0, hi = 10.0;
	double gHi = mag(makeP(Lpg::MODE_VCA, hi, offset, trim, 0, false, false), 1000.0, 20000, 8192);
	if (gHi < frac * open) return 99.0;   // oeffnet nie
	for (int i = 0; i < 26; i++) {
		double mid = 0.5 * (lo + hi);
		double g = mag(makeP(Lpg::MODE_VCA, mid, offset, trim, 0, false, false), 1000.0, 20000, 8192);
		if (g >= frac * open) hi = mid; else lo = mid;
	}
	return 0.5 * (lo + hi);
}

int main() {
	std::printf("=== Anna-Maria: Messungen fuer die Dokumentation (fs = 48 kHz) ===\n\n");

	// ------------------------------------------------------------------
	std::printf("--- 1. Gate-Schwelle: CV, bei der das Gate halb offen ist (VCA-Modus, 1 kHz) ---\n");
	std::printf("Formel: CV_schwelle = T(trim) - 10 V * offset, T = 390000/(R6+R7) V mit R6 = 20k*trim, R7 = 33k\n");
	std::printf("Offset \\ Trim       0.0        0.5        1.0\n");
	const double offs[5] = {0.0, 0.25, 0.5, 0.75, 1.0};
	const double trims[3] = {0.0, 0.5, 1.0};
	for (int o = 0; o < 5; o++) {
		std::printf("  %.2f        ", offs[o]);
		for (int t = 0; t < 3; t++) {
			double th = cvThreshold(offs[o], trims[t]);
			if (th > 90.0) std::printf("   (nie)   ");
			else std::printf("  %6.2f V  ", th);
		}
		std::printf("\n");
	}
	std::printf("Formelwert T(trim) (offset 0): ");
	for (int t = 0; t < 3; t++) std::printf("trim %.1f -> %.2f V   ", trims[t], 390000.0 / (20000.0 * trims[t] + 33000.0));
	std::printf("\n\n");

	std::printf("--- 2. Gate-Kennlinie bei Offset 0.5, Trim 0.5 (VCA-Modus, 1 kHz, Verstaerkung linear) ---\n");
	std::printf("   CV[V]   Rf[kOhm]   Gain(VCA)   Gain[dB]\n");
	for (double cv = -4.0; cv <= 6.001; cv += 0.5) {
		P p = makeP(Lpg::MODE_VCA, cv, 0.5, 0.5, 0, false, false);
		double r = rfOf(p) / 1000.0;
		double g = mag(p, 1000.0, 20000, 8192);
		std::printf("  %6.2f  %9.1f   %8.4f   %7.1f\n", cv, r, g, db(g));
	}
	{
		double t10 = cvThreshold(0.5, 0.5, 0.1), t90 = cvThreshold(0.5, 0.5, 0.9);
		std::printf("Uebergangsbereich (10 %% -> 90 %% der offenen Verstaerkung): %.2f V .. %.2f V (Breite %.2f V)\n\n", t10, t90, t90 - t10);
	}

	// ------------------------------------------------------------------
	std::printf("--- 3. Moduskennlinien bei Offset 0.5, Trim 0.5, Feedback 0: Gain bei 20 Hz und -3-dB-Frequenz ---\n");
	std::printf("   CV[V]  Rf[kOhm] | Lowpass: Gain(20Hz)  f-3dB   | Both: Gain(20Hz)  f-3dB   | VCA: Gain(20Hz)  f-3dB\n");
	const double cvs3[7] = {10.0, 6.0, 5.0, 4.5, 4.0, 3.5, 3.0};
	for (int i = 0; i < 7; i++) {
		double cv = cvs3[i];
		double rf = rfOf(makeP(Lpg::MODE_BOTH, cv, 0.5, 0.5, 0, true, false)) / 1000.0;
		double g1, g2, g3;
		double f1 = f3db(makeP(Lpg::MODE_LOWPASS, cv, 0.5, 0.5, 0, true, false), &g1);
		double f2 = f3db(makeP(Lpg::MODE_BOTH, cv, 0.5, 0.5, 0, true, false), &g2);
		double f3 = f3db(makeP(Lpg::MODE_VCA, cv, 0.5, 0.5, 0, true, false), &g3);
		std::printf("  %5.1f  %8.1f | %9.4f  %8.0f Hz   | %9.4f  %8.0f Hz  | %9.4f  %8.0f Hz\n",
			cv, rf, g1, f1, g2, f2, g3, f3);
	}
	std::printf("(f-3dB = 0 bedeutet: kein Abfall um 3 dB bis 20 kHz)\n\n");

	// ------------------------------------------------------------------
	std::printf("--- 4. Feedback normiert (Lowpass, CV so, dass f-3dB bei Feedback 0 etwa 1-2 kHz) ---\n");
	{
		double cvMid = 4.0;
		for (double cv = 4.4; cv >= 3.0; cv -= 0.05) {
			double f = f3db(makeP(Lpg::MODE_LOWPASS, cv, 0.5, 0.5, 0, true, false));
			cvMid = cv;
			if (f > 0 && f < 1500.0) break;
		}
		double fb0 = f3db(makeP(Lpg::MODE_LOWPASS, cvMid, 0.5, 0.5, 0, true, false));
		std::printf("Arbeitspunkt: CV = %.2f V, f-3dB(fb=0) = %.0f Hz, Rf = %.1f kOhm\n", cvMid, fb0,
			rfOf(makeP(Lpg::MODE_LOWPASS, cvMid, 0.5, 0.5, 0, true, false)) / 1000.0);
		std::printf("   fb_norm   Spitze[dB]   Spitzenfrequenz\n");
		const double fbs[7] = {0.0, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99};
		for (int i = 0; i < 7; i++) {
			double g, f;
			peak(makeP(Lpg::MODE_LOWPASS, cvMid, 0.5, 0.5, fbs[i], true, false), &g, &f);
			if (db(g) < 0.5) std::printf("   %.2f      %7.1f      (keine Spitze)\n", fbs[i], db(g));
			else std::printf("   %.2f      %7.1f      %7.0f Hz\n", fbs[i], db(g), f);
		}
		std::printf("\n");

		std::printf("--- 5. Gleichmaessigkeit der Resonanz: normiert (0.9) gegen absolut (a = 0.9 * 1.47 = 1.32) ---\n");
		std::printf("   CV[V]  Rf[kOhm]   f-3dB(fb=0)  | normiert 0.9: Spitze[dB] @Hz     | absolut 1.32: Spitze[dB] @Hz\n");
		const double cvs5[8] = {6.0, 5.0, 4.5, 4.0, 3.5, 0.0, -2.0, -4.0};
		for (int i = 0; i < 8; i++) {
			double cv = cvs5[i];
			double gN, fN, gA, fA;
			double f0 = f3db(makeP(Lpg::MODE_LOWPASS, cv, 0.5, 0.5, 0, true, false));
			peak(makeP(Lpg::MODE_LOWPASS, cv, 0.5, 0.5, 0.9, true, false), &gN, &fN);
			peak(makeP(Lpg::MODE_LOWPASS, cv, 0.5, 0.5, 0.9 * (1.0 + (2.0 * Lpg::C1 + Lpg::C2) / Lpg::C3lp), false, false), &gA, &fA);
			std::printf("  %5.1f  %8.1f   %8.0f Hz  | %7.1f dB @ %6.0f Hz           | %7.1f dB @ %6.0f Hz\n",
				cv, rfOf(makeP(Lpg::MODE_LOWPASS, cv, 0.5, 0.5, 0, true, false)) / 1000.0, f0, db(gN), fN, db(gA), fA);
		}
		std::printf("\n");
	}

	// ------------------------------------------------------------------
	std::printf("--- 6. Stabilitaet bei Feedback am Anschlag (linear, Impuls, 20 s) ---\n");
	{
		struct Case { const char* name; P p; };
		Case cases[4] = {
			{"normiert 1.00, CV 10 V (offen)",  makeP(Lpg::MODE_LOWPASS, 10.0, 0.5, 0.5, 1.0, true, false)},
			{"normiert 1.00, CV 4.0 V (mittel)", makeP(Lpg::MODE_LOWPASS, 4.0, 0.5, 0.5, 1.0, true, false)},
			{"normiert 1.00, CV 3.0 V (fast zu)", makeP(Lpg::MODE_LOWPASS, 3.0, 0.5, 0.5, 1.0, true, false)},
			{"absolut 1.47, CV 4.0 V",           makeP(Lpg::MODE_LOWPASS, 4.0, 0.5, 0.5, 1.4722, false, false)},
		};
		for (int k = 0; k < 4; k++) {
			Lpg l; l.setSampleRate(FS); l.warmStart(cases[k].p);
			double peak1 = 0, peak20 = 0, last = 0;
			const int n = (int)(20.0 * FS);
			for (int i = 0; i < n; i++) {
				double x = (i == 0) ? 1.0 : 0.0;
				double y = l.process(x, cases[k].p);
				double a = std::fabs(y);
				if (i < (int)FS && a > peak1) peak1 = a;
				if (i >= n - (int)FS && a > peak20) peak20 = a;
				last = y;
			}
			std::printf("  %-38s Spitze 1. s: %10.3e   Spitze 20. s: %10.3e   endlich: %s\n",
				cases[k].name, peak1, peak20, std::isfinite(last) ? "ja" : "NEIN");
		}
		std::printf("\n");
	}

	// ------------------------------------------------------------------
	std::printf("--- 7. NL: Selbstschwingung und Obertoene (Lowpass, Offset 0.5, Trim 0.5) ---\n");
	{
		const double drives[3] = {0.0, 5.0, 10.0};
		for (int corr = 0; corr < 2; corr++) {
		gCorr = corr == 1;
		std::printf("\n[NL-Solver: %s]\n", gCorr ? "korrigiert" : "Referenz");
		std::printf("Selbstschwingung (fb_norm 1.0, NL an, Impuls 1.0, CV 4.0 V): Amplitude nach 5 s\n");
		for (int d = 0; d < 3; d++) {
			double L = 5.0 * std::exp2(2.0 * (5.0 - drives[d]) / 5.0);
			P p = makeP(Lpg::MODE_LOWPASS, 4.0, 0.5, 0.5, 1.0, true, true);
			Lpg l; l.setSampleRate(FS); l.warmStart(p);
			double pk = 0;
			const int n = (int)(5.0 * FS);
			for (int i = 0; i < n; i++) {
				double y = l.process(i == 0 ? 1.0 / L : 0.0, p);
				if (i >= n - (int)(0.2 * FS) && std::fabs(y) > pk) pk = std::fabs(y);
			}
			std::printf("  Drive %4.1f (L = %5.2f V): Spitze im Ausgang = %.3f V\n", drives[d], L, pk * L);
		}
		std::printf("\nObertonanteil (Oberwellen 2..8 / Grundwelle) bei NL an, Eingang +-5 V (fb_norm 0.9):\n");
		std::printf("   CV[V]  f[Hz]   Drive 0     Drive 2.5   Drive 5     Drive 7.5   Drive 10    Grundwelle bei Drive 0 / 10 [V]\n");
		struct Pt { double cv, f; };
		Pt pts[4] = {{8.0, 220.0}, {3.0, 500.0}, {4.0, 500.0}, {4.0, 2000.0}};
		for (int k = 0; k < 4; k++) {
			std::printf("  %5.1f  %5.0f  ", pts[k].cv, pts[k].f);
			double fund0 = 0, fund10 = 0;
			for (int d = 0; d < 5; d++) {
				double dr = d * 2.5;
				double L = 5.0 * std::exp2(2.0 * (5.0 - dr) / 5.0);
				P p = makeP(Lpg::MODE_LOWPASS, pts[k].cv, 0.5, 0.5, 0.9, true, true);
				Lpg l; l.setSampleRate(FS); l.warmStart(p);
				const int n = 48000;
				std::vector<double> y(n);
				for (int i = 0; i < n; i++)
					y[i] = l.process(5.0 * std::sin(2.0 * PI_D * pts[k].f * i / FS) / L, p) * L;
				double a1 = goertzelAmp(y, n / 2, n, pts[k].f), h = 0;
				for (int m = 2; m <= 8; m++) { double a = goertzelAmp(y, n / 2, n, pts[k].f * m); h += a * a; }
				std::printf(" %9.4f  ", std::sqrt(h) / a1);
				if (d == 0) fund0 = a1;
				if (d == 4) fund10 = a1;
			}
			std::printf(" %.2f / %.2f\n", fund0, fund10);
		}
		}
		gCorr = false;
		std::printf("\n");
	}

	// ------------------------------------------------------------------
	std::printf("--- 8. Vactrol-Dynamik (VCA-Modus, Gleichspannung am Eingang, Offset 0.5, Trim 0.5) ---\n");
	for (int st = 0; st < 2; st++) {
		gState = st == 1;
		std::printf("\n[Vactrol-Verhalten: %s]\n", gState ? "zustandsbasiert" : "Referenz");
		const double cvLow = -10.0;
		struct Step { const char* name; double cvHigh; };
		Step steps[3] = {{"CV -10 -> +10 -> -10", 10.0}, {"CV -10 -> +5 -> -10 ", 5.0}, {"CV -10 -> +4.5 -> -10", 4.5}};
		std::printf("                         Anstieg 10->90 %%   Abfall 90->10 %%   Abfall auf 1 %% (-40 dB)   offen (Endwert)\n");
		for (int s = 0; s < 3; s++) {
			P p = makeP(Lpg::MODE_VCA, cvLow, 0.5, 0.5, 0, false, false);
			Lpg l; l.setSampleRate(FS); l.warmStart(p);
			const int nPre = (int)(0.2 * FS), nHigh = (int)(1.5 * FS), nLow = (int)(3.0 * FS);
			std::vector<double> g;
			for (int i = 0; i < nPre; i++) g.push_back(l.process(1.0, p));
			p.cv = steps[s].cvHigh;
			for (int i = 0; i < nHigh; i++) g.push_back(l.process(1.0, p));
			double gOpen = g.back();
			p.cv = cvLow;
			for (int i = 0; i < nLow; i++) g.push_back(l.process(1.0, p));
			// Anstieg
			double t10 = -1, t90 = -1, f90 = -1, f10 = -1, f1 = -1;
			for (int i = nPre; i < nPre + nHigh; i++) {
				if (t10 < 0 && g[i] >= 0.1 * gOpen) t10 = (i - nPre) / FS;
				if (t90 < 0 && g[i] >= 0.9 * gOpen) t90 = (i - nPre) / FS;
			}
			for (int i = nPre + nHigh; i < (int)g.size(); i++) {
				double t = (i - nPre - nHigh) / FS;
				if (f90 < 0 && g[i] <= 0.9 * gOpen) f90 = t;
				if (f10 < 0 && g[i] <= 0.1 * gOpen) f10 = t;
				if (f1 < 0 && g[i] <= 0.01 * gOpen) f1 = t;
			}
			std::printf("  %-22s %8.1f ms       %8.1f ms        %8.1f ms                %.3f\n",
				steps[s].name, (t90 - t10) * 1000.0, (f10 - f90) * 1000.0, f1 * 1000.0, gOpen);
		}
		std::printf("\nKurze Gate-Impulse (CV -10 -> +10 V, Laenge L): erreichte Spitzenverstaerkung, Zeit bis -20 dB (10 %% der Spitze) nach Impulsende\n");
		std::printf("   L[ms]   Spitze (rel. zu offen)   -20 dB nach Ende\n");
		const double lens[8] = {1, 2, 5, 10, 20, 50, 100, 200};
		for (int k = 0; k < 8; k++) {
			P p = makeP(Lpg::MODE_VCA, -10.0, 0.5, 0.5, 0, false, false);
			Lpg l; l.setSampleRate(FS); l.warmStart(p);
			const int nPre = (int)(0.1 * FS), nPulse = (int)(lens[k] * 0.001 * FS), nTail = (int)(3.0 * FS);
			std::vector<double> g;
			for (int i = 0; i < nPre; i++) g.push_back(l.process(1.0, p));
			p.cv = 10.0;
			for (int i = 0; i < nPulse; i++) g.push_back(l.process(1.0, p));
			p.cv = -10.0;
			for (int i = 0; i < nTail; i++) g.push_back(l.process(1.0, p));
			double pk = 0; int ipk = 0;
			for (size_t i = 0; i < g.size(); i++) if (g[i] > pk) { pk = g[i]; ipk = (int)i; }
			double t10 = -1;
			for (size_t i = ipk; i < g.size(); i++) if (g[i] <= 0.1 * pk) { t10 = (i - (nPre + nPulse)) / FS; break; }
			std::printf("  %6.0f      %6.3f              %8.1f ms\n", lens[k], pk / 0.633, t10 * 1000.0);
		}
		const double his[2] = {10.0, 5.0};
		for (int h = 0; h < 2; h++) {
		std::printf("\nAbfall nach 100-ms-Gate (+%.0f V) in Abhaengigkeit vom Ruhe-CV: Zeit bis 50 %% / 10 %% (-20 dB) der Spitze\n", his[h]);
		std::printf("   Ruhe-CV[V]   50 %%          10 %%          Endwert rel. zur Spitze\n");
		const double rests[7] = {-10.0, -4.0, -1.0, 1.0, 2.0, 3.0, 3.5};
		for (int k = 0; k < 7; k++) {
			P p = makeP(Lpg::MODE_VCA, rests[k], 0.5, 0.5, 0, false, false);
			Lpg l; l.setSampleRate(FS); l.warmStart(p);
			const int nPre = (int)(0.1 * FS), nPulse = (int)(0.1 * FS), nTail = (int)(4.0 * FS);
			std::vector<double> g;
			for (int i = 0; i < nPre; i++) g.push_back(l.process(1.0, p));
			p.cv = his[h];
			for (int i = 0; i < nPulse; i++) g.push_back(l.process(1.0, p));
			p.cv = rests[k];
			for (int i = 0; i < nTail; i++) g.push_back(l.process(1.0, p));
			double pk = 0; int ipk = 0;
			for (size_t i = 0; i < g.size(); i++) if (g[i] > pk) { pk = g[i]; ipk = (int)i; }
			double t50 = -1, t10 = -1;
			for (size_t i = ipk; i < g.size(); i++) {
				double t = (i - (nPre + nPulse)) / FS * 1000.0;
				if (t50 < 0 && g[i] <= 0.5 * pk) t50 = t;
				if (t10 < 0 && g[i] <= 0.1 * pk) t10 = t;
			}
			std::printf("  %8.1f   ", rests[k]);
			if (t50 < 0) std::printf("   (nie)     "); else std::printf("%8.1f ms  ", t50);
			if (t10 < 0) std::printf("   (nie)     "); else std::printf("%8.1f ms  ", t10);
			std::printf("   %.3f\n", g.back() / pk);
		}
		}
		std::printf("\n");
	}
	gState = false;

	// ------------------------------------------------------------------
	std::printf("--- 9. Rechenaufwand der Engine (diese Maschine, -O2, double, 1 Kanal) ---\n");
	{
		struct Cfg { const char* name; P p; };
		Cfg cfgs[3] = {
			{"linear (NL aus)",         makeP(Lpg::MODE_LOWPASS, 4.0, 0.5, 0.5, 0.5, true, false)},
			{"NL an",                   makeP(Lpg::MODE_LOWPASS, 4.0, 0.5, 0.5, 0.5, true, true)},
			{"VCA-Modus",               makeP(Lpg::MODE_VCA, 4.0, 0.5, 0.5, 0.0, false, false)},
		};
		for (int k = 0; k < 3; k++) {
			Lpg l; l.setSampleRate(FS); l.warmStart(cfgs[k].p);
			const int n = 4000000;
			double acc = 0;
			auto t0 = std::chrono::steady_clock::now();
			for (int i = 0; i < n; i++) {
				cfgs[k].p.cv = 4.0 + 0.5 * std::sin(i * 0.0005);   // bewegtes CV: Koeffizienten aendern sich
				acc += l.process(std::sin(i * 0.05), cfgs[k].p);
			}
			auto t1 = std::chrono::steady_clock::now();
			double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / n;
			std::printf("  %-18s %6.1f ns pro Sample und Kanal  (16 Kanaele bei 48 kHz: %.1f %% eines Kerns; Pruefsumme %.3f)\n",
				cfgs[k].name, ns, ns * 16.0 * FS * 1e-9 * 100.0, acc);
		}
	}
	return 0;
}
