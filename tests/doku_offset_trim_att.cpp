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

static double gainAt(int mode,double cv,double off,double trim,double f=1000.0){
  return mag(makeP(mode,cv,off,trim,0,true,false), f, 20000, 8192);
}
int main(){
  const double trims[5]={0,0.25,0.5,0.75,1.0};
  printf("== A. Schwelle (50%%) und Leckanstieg je Trim bei Offset 0.5 (VCA, 1 kHz) ==\n");
  for(int t=0;t<5;t++){
    double th=cvThreshold(0.5,trims[t]);
    double T=390000.0/(20000.0*trims[t]+33000.0);
    printf("trim %.2f: Schwelle %.2f V (Formel %.2f V)", trims[t], th, T-5.0);
    double d1=th-1.0, d2=th-3.0;
    double g0=gainAt(Lpg::MODE_VCA,th-0.0,0.5,trims[t]);
    double g1=gainAt(Lpg::MODE_VCA,d1,0.5,trims[t]);
    double g3=gainAt(Lpg::MODE_VCA,d2,0.5,trims[t]);
    double g6=gainAt(Lpg::MODE_VCA,th-6.0,0.5,trims[t]);
    printf("  Gain: Schwelle %.3f, -1V %.4f (%.0f dB), -3V %.4f (%.0f dB), -6V %.5f (%.0f dB)\n",g0,g1,db(g1),g3,db(g3),g6,db(g6));
  }
  printf("\n== B. Breite des Uebergangs (10%%..90%%) je Trim ==\n");
  for(int t=0;t<5;t++){ double a=cvThreshold(0.5,trims[t],0.1), b=cvThreshold(0.5,trims[t],0.9); printf("trim %.2f: %.2f V .. %.2f V (Breite %.2f V)\n",trims[t],a,b,b-a);}
  printf("\n== C. Offset = reine Verschiebung? Gain bei gleichem (cv + 10*offset) ==\n");
  double pairs[4][4]={{-5,0.5,0,0.0},{-3,0.5,-8,1.0},{-3,0.5,0.0,0.2},{0,0.5,-6,0.9}};
  // vergleiche (cv1,off1) mit (cv2,off2) bei gleichem cv+10*off
  double cvA[4]={-5,-3,0,2}, offA[4]={0.5,0.5,0.5,0.5};
  for(int i=0;i<4;i++){
    double s=cvA[i]+10*offA[i];
    double gA=gainAt(Lpg::MODE_VCA,cvA[i],offA[i],0.5);
    double cvB=s-10*0.2; double gB=gainAt(Lpg::MODE_VCA,cvB,0.2,0.5);
    double cvC=s-10*0.8; double gC=gainAt(Lpg::MODE_VCA,cvC,0.8,0.5);
    printf("cv+10*off = %.1f: (cv %.1f,off 0.5) %.5f | (cv %.1f,off 0.2) %.5f | (cv %.1f,off 0.8) %.5f\n",s,cvA[i],gA,cvB,gB,cvC,gC);
  }
  printf("\n== D. Lowpass-Grenzfrequenz (-3 dB) und Durchlass 5 kHz im Abstand D unter der Schwelle (Offset 0.5, Trim 0/0.5/1) ==\n");
  double Ds[6]={0.0,0.5,1.0,2.0,4.0,8.0};
  for(int t=0;t<5;t+=2){
    double th=cvThreshold(0.5,trims[t]);
    for(int d=0;d<6;d++){
      double cv=th-Ds[d]; if(cv<-10) continue;
      P p=makeP(Lpg::MODE_LOWPASS,cv,0.5,trims[t],0,true,false);
      double f=f3db(p); double g5=mag(makeP(Lpg::MODE_LOWPASS,cv,0.5,trims[t],0,true,false),5000.0,20000,8192);
      double gv=gainAt(Lpg::MODE_VCA,cv,0.5,trims[t]);
      printf("trim %.1f  %.1f V unter Schwelle (cv %.2f): LP f-3dB %6.0f Hz, 5kHz %6.1f dB, VCA %6.1f dB\n",trims[t],Ds[d],cv,f,db(g5),db(gv));
    }
  }
  printf("\n== E. CV-Wirkung ueber CV ATT: wirksame CV, Offset 0.5, Trim 0.5, VCA 1kHz ==\n");
  double cvin[3]={10,5,2};
  double att[6]={1,0.75,0.5,0.25,0,-0.5};
  for(int a=0;a<6;a++){ printf("ATT %+.2f:",att[a]); for(int c=0;c<3;c++){ double cv=cvin[c]*att[a]; printf("  CV %.0f -> %+.1f V -> %.4f (%.0f dB)",cvin[c],cv,gainAt(Lpg::MODE_VCA,cv,0.5,0.5),db(gainAt(Lpg::MODE_VCA,cv,0.5,0.5)));} printf("\n"); }
  printf("\n== F. CV-Klemmung: CV 10 vs 15 vs 20 V (Offset 0.5/0.9, VCA) ==\n");
  double cvs[3]={10,15,20}; double offs2[2]={0.5,0.9};
  for(int o=0;o<2;o++){ printf("offset %.1f:",offs2[o]); for(int c=0;c<3;c++) printf("  cv %.0f: %.5f",cvs[c],gainAt(Lpg::MODE_VCA,cvs[c],offs2[o],0.5)); printf("\n"); }
  printf("\n== G. Offset-Reihe Lowpass bei CV 0 V (Trim 0.5): f-3dB und VCA-Gain ==\n");
  double os[8]={0,0.2,0.4,0.6,0.7,0.8,0.9,1.0};
  for(int i=0;i<8;i++){ P p=makeP(Lpg::MODE_LOWPASS,0,os[i],0.5,0,true,false); printf("offset %.1f: LP f-3dB %6.0f Hz, VCA %.4f (%.0f dB)\n",os[i],f3db(p),gainAt(Lpg::MODE_VCA,0,os[i],0.5),db(gainAt(Lpg::MODE_VCA,0,os[i],0.5))); }
  return 0;
}
