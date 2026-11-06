#include "AnnaMariaLpg.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
typedef AnnaMariaLpg L;
static const double FS = 48000.0, PI = 3.14159265358979323846;

static double pitchOf(const std::vector<double>& y, int i0, int N, double f0) {
	const double T0 = FS / f0; const int lmin = std::max(2, (int)(0.7*T0)), lmax = (int)(1.3*T0);
	double e = 0; for (int i = i0; i < i0+N; i++) e += y[i]*y[i];
	std::vector<double> ac(lmax+2, 0.0);
	for (int l = lmin-1; l <= lmax+1; l++) { double s=0,e2=0; for (int i=i0;i<i0+N;i++){s+=y[i]*y[i+l];e2+=y[i+l]*y[i+l];} ac[l]=s/std::sqrt(e*e2); }
	int b = lmin; for (int l = lmin; l <= lmax; l++) if (ac[l] > ac[b]) b = l;
	double a=ac[b-1],c=ac[b],d=ac[b+1]; double off=0.5*(a-d)/(a-2*c+d);
	return FS/(b+off);
}
static double rms(const std::vector<double>& y, int i0, int N){ double e=0; for(int i=i0;i<i0+N;i++) e+=y[i]*y[i]; return std::sqrt(e/N); }

static L::Params base(int mode) {
	L::Params p; p.mode = mode; p.offset = 0.0; p.trim = 1.0; p.fb = 0.0; p.fbnorm = true; p.cv = 10.0;
	p.vcaLinear = true; p.bothLowpass = true; return p;
}

int main() {
	const double notes[] = {65.406, 130.813, 261.626, 523.251, 1046.502, 2093.005, 4186.009};
	const char* nn[] = {"C2","C3","C4","C5","C6","C7","C8"};
	printf("== Tonhoehe und Abklingzeit (LOWPASS, Gate offen, wet 1, Impuls) ==\n");
	for (double t60 : {0.1, 0.5, 3.0}) {
		printf("  T60 %.2f s:", t60);
		for (int k = 0; k < 7; k++) {
			L e; e.setSampleRate(FS); L::Params p = base(L::MODE_LOWPASS); p.resHz = notes[k]; p.resWet = 1.0; p.resT60 = t60;
			e.warmStart(p);
			const int N = (int)(1.2 * FS); std::vector<double> y(N+4000);
			for (int n = 0; n < (int)y.size(); n++) y[n] = e.process(n==0?1.0:0.0, p);
			const int w = (int)(std::max(0.03, 6.0/notes[k]) * FS);
			const double f = pitchOf(y, (int)(0.02*FS), w, notes[k]);
			// Abklingzeit: Amplitude in 2 Fenstern, Soll in dB
			const int a0 = (int)(0.05*FS), a1 = (int)(0.30*FS), wl = (int)(std::max(0.02, 4.0/notes[k]) * FS);
			const double dB = 20*std::log10(rms(y,a1,wl)/rms(y,a0,wl));
			const double want = -60.0 * (0.25 / t60);
			printf("  %s %+5.1fct %+6.1f/%+6.1fdB |", nn[k], 1200*std::log2(f/notes[k]), dB, want);
		}
		printf("\n");
	}
	printf("   (Abklingen: gemessen / Soll in dB zwischen 50 und 300 ms; C2 hat nur sehr wenige Perioden im Fenster)\n");

	printf("\n== wet = 0 und resHz = 0 sind gleich; VCA und Paper-BOTH ohne Wirkung ==\n");
	for (int variant = 0; variant < 4; variant++) {
		L a, b; a.setSampleRate(FS); b.setSampleRate(FS);
		L::Params pa = base(variant==2 ? L::MODE_VCA : L::MODE_BOTH), pb = pa;
		if (variant == 0) { pa.mode = L::MODE_LOWPASS; pb.mode = L::MODE_LOWPASS; pb.resHz = 261.6; pb.resWet = 0.0; }   // wet 0
		if (variant == 1) { pa.vcaLinear = false; pb = pa; pb.resHz = 261.6; pb.resWet = 1.0; }                         // Paper-BOTH
		if (variant == 2) { pb = pa; pb.resHz = 261.6; pb.resWet = 1.0; }                                               // VCA
		if (variant == 3) { pa.mode = L::MODE_LOWPASS; pb = pa; pb.resHz = 0.0; pb.resWet = 1.0; }                      // resHz 0
		pa.cv = pb.cv = 6.0; a.warmStart(pa); b.warmStart(pb);
		std::mt19937 rng(3); std::normal_distribution<double> nd(0.0, 0.3);
		double dm = 0;
		for (int n = 0; n < (int)FS; n++) { double x = nd(rng); pa.cv = pb.cv = 5+4*std::sin(2*PI*2*n/FS); dm = std::max(dm, std::fabs(a.process(x,pa)-b.process(x,pb))); }
		const char* lab[] = {"LOWPASS wet 0", "BOTH Paper (vcaLinear aus), wet 1", "VCA, wet 1", "LOWPASS resHz 0, wet 1"};
		printf("  %-36s max Differenz %g\n", lab[variant], dm);
	}
	{
		L a, b; a.setSampleRate(FS); b.setSampleRate(FS);
		L::Params pa = base(L::MODE_BOTH), pb = pa; pb.resHz = 261.6; pb.resWet = 1.0;
		a.warmStart(pa); b.warmStart(pb);
		double dm = 0;
		for (int n = 0; n < 4800; n++) dm = std::max(dm, std::fabs(a.process(n==0?1.0:0.0,pa)-b.process(n==0?1.0:0.0,pb)));
		printf("  BOTH linear + Lowpass-Stufe, wet 1: max Differenz %g (soll > 0: Resonator wirkt)\n", dm);
	}

	printf("\n== Stabilitaet: Audiorate-Modulation der Tonhoehe, wet 2, T60 3 s, Rauschen, 20 s ==\n");
	for (int mode : {L::MODE_LOWPASS, L::MODE_BOTH}) {
		L e; e.setSampleRate(FS); L::Params p = base(mode); p.resWet = 2.0; p.resT60 = 3.0; e.warmStart(p);
		std::mt19937 rng(9); std::normal_distribution<double> nd(0.0, 0.3);
		double pk = 0; bool bad = false;
		for (int n = 0; n < (int)(20*FS); n++) {
			const double t = n / FS;
			p.resHz = 261.626 * std::exp2(2.0 * std::sin(2*PI*0.7*t) + 1.5 * std::sin(2*PI*220.0*t));
			p.cv = 5.0 + 5.0 * std::sin(2*PI*3.0*t);
			const double y = e.process(nd(rng), p);
			if (!std::isfinite(y)) bad = true; 
			pk = std::max(pk, std::fabs(y));
		}
		printf("  Modus %d: %s, Spitze %.2f (Modellgrenze im Modul: 50)\n", mode, bad ? "NaN!" : "kein NaN", pk);
	}

	printf("\n== Moduswechsel: Klingen nach Wechsel LOWPASS -> VCA -> LOWPASS ==\n");
	{
		L e; e.setSampleRate(FS); L::Params p = base(L::MODE_LOWPASS); p.resHz = 261.6; p.resWet = 1.0; p.resT60 = 3.0; e.warmStart(p);
		for (int n = 0; n < 2400; n++) e.process(n==0?1.0:0.0, p);
		const double ringBefore = std::fabs(e.process(0.0, p)) + std::fabs(e.process(0.0, p));
		p.mode = L::MODE_VCA;  for (int n = 0; n < 480; n++) e.process(0.0, p);
		p.mode = L::MODE_LOWPASS; double mx = 0; for (int n = 0; n < 4800; n++) mx = std::max(mx, std::fabs(e.process(0.0, p)));
		printf("  Ring vor dem Wechsel %.4f, danach (Eingang 0) maximal %.2e\n", ringBefore, mx);
	}
	return 0;
}
