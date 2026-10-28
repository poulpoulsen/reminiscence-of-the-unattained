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

// ---- Test: THRESHOLD (V) -> Offset -> gemessene Gate-Schwelle ----
// Die Formeln stehen in src/AnnaMaria.cpp (anonymer Namespace); hier dupliziert.
// Bei Aenderungen dort bitte beide Stellen anpassen.
static double trimT(double trim) { return 390000.0 / (20000.0 * trim + 33000.0); }
static const double THR_HALF = 0.05;
int main() {
	const double depths[5] = {0, 0.25, 0.5, 0.75, 1.0};
	const double thrs[9] = {-1, 0, 1, 2, 3, 4.1, 6, 8, 9};
	int fails = 0, tested = 0; double worst = 0;
	for (int d = 0; d < 5; d++) for (int k = 0; k < 9; k++) {
		double o = (trimT(depths[d]) + THR_HALF - thrs[k]) / 10.0;
		if (o < 0.0 || o > 1.0) continue;            // nicht erreichbar -> Modul begrenzt
		double th = cvThreshold(o, depths[d]);        // gemessene 50-%-Schwelle der Engine
		double err = th - thrs[k]; tested++;
		if (std::fabs(err) > worst) worst = std::fabs(err);
		if (std::fabs(err) > 0.03) { printf("ABWEICHUNG depth %.2f thr %.2f: gemessen %.3f\n", depths[d], thrs[k], th); fails++; }
	}
	printf("%d Kombinationen geprueft, groesste Abweichung %.4f V\n", tested, worst);
	printf(fails ? "FEHLER\n" : "OK\n");
	return fails ? 1 : 0;
}
