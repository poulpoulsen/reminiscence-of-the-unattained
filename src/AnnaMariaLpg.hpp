// AnnaMariaLpg.hpp — Rack-freie DSP-Engine: digitales Modell eines Lowpass Gate im West-Coast-Stil
//
// Sample-weise Portierung von "buchla_lp_gate.m" (GNU Octave) nach C++11.
//
// Quelle des Modells:
//   J. Parker and S. D'Angelo, "A Digital Model of the Buchla Lowpass-Gate",
//   Proc. 16th Intl. Conf. Digital Audio Effects (DAFx-13), pp. 278-285,
//   Maynooth, Ireland, September 2013.
//
// Die Formeln folgen bewusst der Octave-Referenz (inkl. Errata) und NICHT dem
// PDF: Gl. (20) im PDF ist fehlerhaft, die Loesung im Code ist korrekt.
//
// Original-Octave-Code:
//   Copyright (C) 2014 Stefano D'Angelo
//
//   Permission is hereby granted, free of charge, to any person obtaining a
//   copy of this software and associated documentation files (the
//   "Software"), to deal in the Software without restriction, including
//   without limitation the rights to use, copy, modify, merge, publish,
//   distribute, sublicense, and/or sell copies of the Software, and to
//   permit persons to whom the Software is furnished to do so, subject to
//   the following conditions:
//
//   The above copyright notice and this permission notice shall be included
//   in all copies or substantial portions of the Software.
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
//   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
//   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Dieser C++-Port: Teil des Plugins "reminiscence of the unattained"
// (GPL-3.0-or-later). Die MIT-Hinweise oben muessen erhalten bleiben.
//
// Einheiten: Alle Groessen sind Schaltungsgroessen in Volt/Ohm/Farad.
// Eingangs-/Ausgangspegel-Skalierung (VCV +-5 V <-> Modell) ist Sache des
// Aufrufers (Modul), nicht der Engine.
//
// Rechengenauigkeit: double (Vergleichbarkeit mit Octave).

#pragma once

#include <cmath>

struct AnnaMariaLpg {
	enum Mode { MODE_LOWPASS = 1, MODE_VCA = 2, MODE_BOTH = 3 };

	struct Params {
		double cv = 0.0;      // Steuerspannung in V, wird auf [-10, 10] begrenzt
		double offset = 0.5;  // CV-Offset (R1/R2-Teiler), 0..1
		double trim = 0.5;    // Trimmer R6, 0..1
		double fb = 0.0;      // Rueckkopplung (normiert oder absolut, siehe fbnorm)
		int mode = MODE_LOWPASS;
		bool fbnorm = true;   // fb als normierter Wert a_norm (nur Lowpass-Modus wirksam)
		bool nl = false;      // Rueckkopplungs-Nichtlinearitaet (tanh)

		// --- Modellvarianten (Standard = Verhalten der Octave-Referenz, bitgenau) ---
		// vactrolByState: Richtung der Vactrol-Zeitkonstante nicht aus dem Vorzeichen der
		//   Aenderung des Ziel-Widerstands Rf (Referenz), sondern aus dem Vergleich von Rf mit
		//   dem aktuellen Vactrol-Zustand: Rf < Zustand -> schnell (Oeffnen), sonst langsam
		//   (Schliessen). Die Referenz erkennt bei gate-artiger CV nur ein einziges Sample als
		//   "oeffnend"; danach laeuft das Oeffnen mit der langsamen Konstante (Anstieg ~220 ms).
		// nlCorrected: korrigiertes Vorzeichen des Terms a1*d1*t2m1 in yxden des NL-Zweigs.
		//   In der Referenz (.m) steht "+a1*d1*t2m1", die direkte Loesung der Gleichungen
		//   (14)-(16) mit der Linearisierung (25)/(26) ergibt "-a1*d1*t2m1". Ohne Korrektur
		//   geht der NL-Zweig bei kleinen Signalen nicht in den linearen Zweig ueber.
		bool vactrolByState = false;
		bool nlCorrected = false;

		// --- Erweiterung (nicht im Paper) ---
		// vactrolTimeScale: Faktor auf beide Vactrol-Zeitkonstanten (trise, tfall).
		//   1.0 = Werte des Papers (Standard, bitgenau gegen die Octave-Referenz).
		//   > 1 langsamer (Huellkurven-artig), < 1 schneller (folgt dem Gate).
		double vactrolTimeScale = 1.0;

		// vactrolConductance: Vactrol-Modell im Leitwert Gf = 1/Rf mit zwei parallelen Anteilen
		//   (Erweiterung, NICHT im Paper). Gedacht fuer "gepingte" kurze Impulse, bei denen die
		//   Paper-Variante (Glaettung im Widerstand) praktisch gar nicht oeffnet.
		//   Schneller Anteil: Zeitkonstanten x0,02 (Oeffnen und Schliessen, richtungsabhaengig wie
		//   bei vactrolByState) -> kurze Spitze. Langsamer Anteil: laedt schnell (x0,1 von trise),
		//   faellt langsam (x4 von tfall), Gewicht 0,016 -> lang ausklingender Schwanz bei niedrigem
		//   Pegel, kaum abhaengig von der Impulslaenge. Alle Faktoren sind fest und mit
		//   vactrolTimeScale gemeinsam skaliert (x1 = die Werte hier). Die Zahlen sind an Messungen
		//   mit einem Vergleichsmodul (Impulse 6 ms und 20 ms, Periode 620 ms) angepasst, nicht an
		//   ein Bauteil-Datenblatt. Statisch (konstantes cv) ist das Ergebnis identisch mit dem
		//   Standard-Rf (Gewichte sind normiert). vactrolByState wird in diesem Fall ignoriert.
		//   false = Paper (Standard, bitgenau gegen die Octave-Referenz).
		bool vactrolConductance = false;

		// Bauteilvarianten (Standard = Paper): Variante "Reproduction board"
		// hat R8 = 10 k statt 4,7 k und im VCA-Modus R16 = 10 k sowie eine
		// Eingangsstufe mit Verstaerkung 2 (R11 = R12 = 15 k). Ob diese Annahme stimmt, ist
		// nicht an Messdaten geprueft.
		double r8 = 4.7e3;          // Widerstand am Fuehlknoten des Steuerverstaerkers [Ohm]
		double vcaRa = 5e3;         // Querwiderstand im VCA-Modus [Ohm] (Paper: 5 k)
		double vcaPreGain = 1.0;    // Verstaerkung vor dem Gate im VCA-Modus (Paper: 1)

		// vcaLinear: linearer Gate-Anteil in den Modi VCA und BOTH statt Schaltungsnachbildung (Erweiterung,
		//   NICHT im Paper). Die Referenzdokumentation des Hardware-Moduls beschreibt "Gate" als linearen VCA
		//   und "Combo" als Kombination aus linearem Gate und Lowpass; das Paper-Modell (Vactrol-Teiler mit
		//   Knick an der Schwelle) bildet das nicht ab.
		//   Linearer Gate-Anteil: Verstaerkung = cv / 10 (cv auf 0...10 V begrenzt, 10 V = Verstaerkung 1),
		//   mit den Vactrol-Zeitkonstanten geglaettet (trise/tfall mal vactrolTimeScale, Oeffnen = Verstaerkung
		//   groesser als der Zustand, wie vactrolByState). Ob die Hardware hier eine Vactrol-Traegheit hat,
		//   ist nicht belegt (Annahme).
		//   VCA:  nur dieser Anteil (davor vcaPreGain). Kein Filter: Offset/Trim (THRESHOLD/DEPTH),
		//         Rueckkopplung und NL wirken nicht.
		//   BOTH: VCA und LPG in Reihe: erst der lineare Gate-Anteil, dann die Filterstufe. Je nach bothLowpass
		//         die Lowpass-Stufe (wie MODE_LOWPASS: C3 = 4,7 nF, Ra = 5 M, Rueckkopplung und NL wirken wie
		//         dort; das Filter schwingt nach dem Schliessen des Gates nach) oder die Paper-Stufe von BOTH
		//         (C3 = 0). Reihenfolge nach Anwender.
		//   LOWPASS: unveraendert. false = Paper (Standard, bitgenau gegen die Octave-Referenz).
		bool vcaLinear = false;

		// bothLowpass: Filterstufe von BOTH bei vcaLinear. true = Lowpass-Stufe wie MODE_LOWPASS (C3 = 4,7 nF,
		//   Rueckkopplung und NL wirken), false = Paper-Stufe von BOTH (C3 = 0, filtert kaum, keine Resonanz).
		//   Ohne Wirkung ohne vcaLinear und in den Modi VCA und LOWPASS. Eine moegliche Lesart der Schaltung (Mittel-
		//   stellung ohne C8) und die erwartete Wirkung (Lowpass mit Nachschwingen) passen nicht sicher
		//   zusammen, daher waehlbar (Standard: Lowpass-Stufe, Wahl des Anwenders).
		bool bothLowpass = true;

		// pitchHz: Tonhoehe in Hz fuer den V/Oct-Eingang (Erweiterung, NICHT im Paper). 0 = aus (Standard, bitgenau
		//   gegen die Referenz). Wirkt nur dort, wo das Filter eine Resonanz hat (C3 = 4,7 nF: MODE_LOWPASS und
		//   BOTH mit linearem Gate und Lowpass-Stufe); in VCA und Paper-BOTH ohne Wirkung.
		//   Prinzip: Alle Kapazitaeten werden mit s multipliziert. Das ist eine exakte Zeitskalierung des
		//   Filters (Frequenzachse mal k = 1/s, Form, Guete und Gleichspannungsverstaerkung bleiben, die
		//   Vactrol-Zeitkonstanten und der Steuerpfad bleiben in Echtzeit). k wird so gewaehlt, dass die
		//   Klingfrequenz des Filters bei VOLL OFFENEM Gate (Rf = Rf bei Ifmax, ca. 1,45 kOhm) auf pitchHz liegt.
		//   Beim Schliessen des Gates faellt die Tonhoehe wie im Original (Ping-Glide); die Lautstaerke bleibt
		//   von der Note unabhaengig. Klingfrequenz = gedaempfte Eigenfrequenz des Filters 2. Ordnung
		//   (bei hoher Rueckkopplung praktisch der Resonanzpeak); bei starker Daempfung gilt ein stetiger Ersatzwert.
		//   Die Bilineartransformation wird vorverzerrt, sodass die digitale Frequenz stimmt.
		double pitchHz = 0.0;

		// resHz, resWet, resT60: gestimmter Resonator vor der LPG-Stufe (Erweiterung, NICHT im Paper). resHz = Tonhoehe
		//   in Hz, 0 = aus (Standard, bitgenau gegen die Referenz). Der Resonator ist ein Zweipol mit Spitzen-
		//   normierter Impulsantwort h[n] = r^n * sin((n+1) w0): ein Impuls am Eingang gibt einen abklingenden Sinus
		//   der Amplitude 1 bei genau resHz (kein Glide, unabhaengig von Gate und Vactrol). Eingang der LPG-Stufe
		//   ist x + resWet * res(x); der Direktpfad x bleibt unveraendert (bitgleich zu resWet = 0). Das LPG
		//   bestimmt weiter, wie lange der Ton hoerbar ist. resT60 = Abklingzeit des Resonators auf -60 dB in s
		//   (unabhaengig von der Tonhoehe). Wirkt nur dort, wo das Filter eine Resonanzstufe hat (MODE_LOWPASS und
		//   BOTH mit linearem Gate und Lowpass-Stufe); in VCA und Paper-BOTH ohne Wirkung. Der Aufrufer setzt dann
		//   fb = 0 (keine eigene LPG-Resonanz). Gegenstueck zur Zeitskalierung pitchHz, die das ganze Filter stimmt
		//   und dadurch den Klang daempft (Studie 2026-10-24); pitchHz bleibt in der Engine, wird vom Modul nicht
		//   mehr gesetzt.
		double resHz = 0.0;
		double resWet = 0.0;
		double resT60 = 0.5;
		// Hochpass 1. Ordnung auf dem Direktpfad im Resonatorbetrieb: Eckfrequenz = resHpRatio * resHz (0 = aus).
		// Entfernt Gleichanteil und Tiefen eines unipolaren Nadelimpulses (Hüllkurve am IN), die sonst als tiefer
		// "Plopp" durch das offene Gate laufen. Der Resonator wird vom ungefilterten Eingang gespeist.
		double resHpRatio = 0.0;
		double resLpHz = 0.0;      // Tiefpass 1. Ordnung auf dem Direktpfad (0 = aus): rundet die senkrechte Flanke ab
		double resHpMinHz = 0.0;   // untere Grenze der Eckfrequenz in Hz (unabhaengig von der Note)
		// Pegel des Direktpfads im Resonatorbetrieb (1 = unveraendert, 0 = nur der Resonator wird gehoert).
		double resDry = 1.0;
	};

	// ---- Konstanten (Bauteile, siehe Paper Tab. 1/2 und .m) ----
	static constexpr double C1    = 1e-9;
	static constexpr double C2    = 220e-12;
	static constexpr double C3lp  = 4.7e-9;
	static constexpr double Cc    = 2e-9;
	static constexpr double Ralp  = 5e6;
	static constexpr double Ravca = 5e3;
	static constexpr double R1R2  = 10e3;
	static constexpr double R3    = 150e3;
	static constexpr double R4    = 470e3;
	static constexpr double R5    = 100e3;
	static constexpr double R6max = 20e3;
	static constexpr double R7    = 33e3;
	static constexpr double R8    = 4.7e3;
	static constexpr double R9    = 470.0;
	static constexpr double Vs    = 15.0;
	static constexpr double G     = 2e5;
	static constexpr double n     = 3.9696;
	static constexpr double VT    = 26e-3;
	static constexpr double VB    = 3.9;
	static constexpr double Ifmin = 10e-6;
	static constexpr double Ifmax = 40e-3;
	static constexpr double trise = 12e-3 / 5.0;
	static constexpr double tfall = 250e-3 / 5.0;

	static constexpr double A_    = 3.464;
	static constexpr double B_    = 1136.212;
	static constexpr double xi    = 600.0;
	static constexpr double k0    = 1.468e2;
	static constexpr double k1p   = 4.920e-1;
	static constexpr double k2p   = 4.167e-4;
	static constexpr double k3p   = 7.391e-9;
	static constexpr double kL    = 6.386;
	static constexpr double gamma = 0.001;

	// ---- Zustand ----
	double fs = 48000.0;

	// Shelving-Koeffizienten (Steuerpfad, High-Shelf nach Errata)
	double kVb1 = 1, kVb2 = 0, kVb3 = 0;
	double zVb = 0;       // DF2T-Zustand des Shelving-Filters

	double Rfz1 = 0;      // vorheriger Rf (fuer Richtungserkennung und Trapez)
	double Rfvz1 = 0;     // vorheriger Vactrol-Ausgang Rfv
	double G1z1 = 0, G2z1 = 0;  // Leitwert-Anteile (schnell, langsam) fuer vactrolConductance; 0 = noch nicht gesetzt

	double sd = 0, sx = 0, so = 0;  // Audiopfad-Zustaende
	double yoz1 = 0;                // vorheriger Ausgang (Arbeitspunkt der tanh-Linearisierung)

	// Pitch-Resonator (resHz): Zustaende und Koeffizienten-Cache
	double lpy1 = 0;
	double hpx1 = 0, hpy1 = 0;   // Hochpass des Direktpfads
	double rni1 = 0, rni2 = 0, rinv = 0;   // Verlauf von ni (Nullstelle bei 0 Hz), rinv = 1/(2 sin w0)
	double rzr = 0, rzi = 0;   // gekoppelte Form: komplexer Zustand z = rzr + j rzi
	bool rcValid = false;
	double rcHz = 0, rcT60 = 0, rcFs = 0, rrc = 0, rrs = 0, rcw = 0, rsw = 0;

	// Linearer Gate-Anteil (vcaLinear): Zielverstaerkung und geglaettete Verstaerkung des vorigen Samples.
	// gainValid = das letzte Sample hat den linearen Anteil benutzt (sonst wird der Zustand beim naechsten
	// Mal auf die Zielverstaerkung gesetzt). paperStale = Steuerpfad, Vactrol und Filter wurden im reinen
	// linearen VCA nicht nachgefuehrt; beim Wechsel in einen Filtermodus startet warmStart().
	double gLinz1 = 0, gLinv1 = 0;
	bool gainValid = false;
	bool paperStale = false;

	// Cache fuer trim-abhaengige Groessen
	double cachedTrim = -1.0;
	double cachedR8 = -1.0;
	double R6 = 0, alpha = 1, beta = 0, kIa = 0;
	// Vom trim abhaengige Konstanten, einmal in updateTrim() berechnet (gleiche Ausdruecke wie
	// vorher in controlToRf(), daher bitgleich).
	double Rsum = 0, alphaBeta = 0, kIa3 = 0, kIa4 = 0;
	unsigned trimVer = 0;   // zaehlt jede Aenderung von trim/r8 (Schluessel fuer den Rf-Cache)

	// Cache Steuerpfad: gleiche Eingaenge (Vb, offset, trim) -> gleiches Rf (reine Funktion).
	bool crfValid = false;
	double crfVb = 0, crfOff = 0, crfRf = 0;
	unsigned crfTrimVer = 0;

	// Cache Audiopfad-Koeffizienten: haengen nur von (Rfv, a, C3, Ra) und fs ab.
	bool coefValid = false;
	double cfRfv = 0, cfA = 0, cfC3 = 0, cfRa = 0, cfK = 1.0;
	// Cache Pitch-Skalierung (k haengt nur von Note, Rueckkopplung a, C3, Ra und fs ab)
	bool pkValid = false;
	double pkHz = 0, pkA = 0, pkC3 = 0, pkRa = 0, pkFs = 0, pkK = 1.0;
	double cA1 = 0, cA2 = 0, cB1 = 0, cB2 = 0, cB3 = 0, cB4 = 0, cD1 = 0;
	double cKoo = 0, cKox = 0, cKdo = 0, cKdx = 0, cKsdo = 0, cKsdx = 0;
	double cKsxi = 0, cKsxx = 0, cKsxo = 0, cKsxd = 0, cKsox = 0, cKsoo = 0;
	double cKyx = 0, cKxo = 0, cKxi = 0, cKxd = 0, cKxx = 0;   // nur linearer Zweig

	AnnaMariaLpg() { setSampleRate(48000.0); }

	void setSampleRate(double sampleRate) {
		fs = sampleRate;
		coefValid = false;
		pkValid = false;
		const double c = 2.0 * Cc * fs;
		kVb1 = (1.0 + c * (R4 + R5)) / (1.0 + c * R4);
		kVb2 = (1.0 - c * (R4 + R5)) / (1.0 + c * R4);
		kVb3 = (1.0 - c * R4) / (1.0 + c * R4);
	}

	// Kaltstart: alle Zustaende 0, exakt wie in der Octave-Referenz.
	// (Dort ist Rfz1 = 0 am ersten Sample: kuenstlicher Einschwingvorgang.)
	void reset() {
		zVb = 0; Rfz1 = 0; Rfvz1 = 0; G1z1 = 0; G2z1 = 0;
		sd = 0; sx = 0; so = 0; yoz1 = 0; rzr = 0; rzi = 0; rni1 = 0; rni2 = 0; hpx1 = 0; hpy1 = 0; lpy1 = 0;
		gLinz1 = 0; gLinv1 = 0; gainValid = false; paperStale = false;
	}

	// Warmstart: Steuerpfad im eingeschwungenen Zustand fuer konstantes cv.
	// Fuer den Betrieb im Modul verwenden (kein kuenstlicher Einschwingvorgang).
	void warmStart(const Params& p) {
		updateTrim(clampd(p.trim, 0.0, 1.0), p.r8);
		const double cv = clampd(p.cv, -10.0, 10.0);
		const double Vb = cv;                       // Shelving hat Gleichspannungsverstaerkung 1
		zVb = kVb2 * cv - kVb3 * Vb;                // stationaerer DF2T-Zustand
		const double Rf = controlToRf(Vb, clampd(p.offset, 0.0, 1.0));
		Rfz1 = Rf;
		Rfvz1 = Rf;                                 // stationaer: Rfv == Rf
		G1z1 = 1.0 / Rf; G2z1 = G1z1;
		sd = 0; sx = 0; so = 0; yoz1 = 0; rzr = 0; rzi = 0; rni1 = 0; rni2 = 0; hpx1 = 0; hpy1 = 0; lpy1 = 0;
		gainValid = false; paperStale = false;   // linearer Anteil startet beim naechsten Sample mit der Zielverstaerkung
	}

	// ---- Ein Sample verarbeiten ----
	double process(double x, const Params& pin) {
		if (pin.mode == MODE_VCA) x *= pin.vcaPreGain;
		// Linearer Gate-Anteil (Erweiterung, siehe Params::vcaLinear) in den Modi VCA und BOTH.
		const bool linGate = pin.vcaLinear && (pin.mode == MODE_VCA || pin.mode == MODE_BOTH);
		// VCA: nur der lineare Anteil, kein Filter, keine Schaltungsnachbildung.
		if (linGate && pin.mode == MODE_VCA)
			return processLinearVca(x, pin);
		// Rueckwechsel aus dem reinen linearen VCA: Steuerpfad und Vactrol im eingeschwungenen Zustand
		// starten (deren Zustaende wurden dort nicht nachgefuehrt).
		if (paperStale) warmStart(pin);
		if (!linGate) gainValid = false;
		// BOTH: erst der lineare Gate-Anteil (VCA), dann die Lowpass-Stufe (LPG), beide in Reihe.
		if (linGate) x *= linearGain(pin);
		// Grenzen (wie limit() in der .m)
		const double cv     = clampd(pin.cv, -10.0, 10.0);
		const double offset = clampd(pin.offset, 0.0, 1.0);
		const double trim   = clampd(pin.trim, 0.0, 1.0);
		double fb = pin.fb;
		// BOTH mit linearem Gate-Anteil: Filterstufe wie LOWPASS oder Paper-Stufe (siehe Params::bothLowpass).
		const int mode = (linGate && pin.bothLowpass) ? (int) MODE_LOWPASS : pin.mode;
		if (!pin.nl && mode == MODE_LOWPASS) {
			const double fbmax = pin.fbnorm ? 1.0 : 1.0 + (2.0 * C1 + C2) / C3lp;
			if (fb > fbmax) fb = fbmax;
		}

		updateTrim(trim, pin.r8);

		// Modus-abhaengige Bauteile
		const double C3 = (mode == MODE_LOWPASS) ? C3lp : 0.0;
		const double Ra = (mode == MODE_VCA) ? pin.vcaRa : Ralp;

		// Pitch-Resonator vor der LPG-Stufe (Erweiterung): nur mit Resonanzstufe (C3 > 0). Sonst Zustand loeschen,
		// damit nach einem Moduswechsel kein altes Klingen auftaucht. Ohne resHz bitgleich zu vorher.
		if (pin.resHz > 0.0 && C3 > 0.0) {
			const double xr = x;
			if (pin.resHpRatio > 0.0) {
				double fc = pin.resHpRatio * pin.resHz;
				if (fc < pin.resHpMinHz) fc = pin.resHpMinHz;
				if (fc > 0.2 * fs) fc = 0.2 * fs;
				const double a = std::exp(-2.0 * 3.14159265358979323846 * fc / fs);
				const double yh = a * (hpy1 + x - hpx1);
				hpx1 = x; hpy1 = yh; x = yh;
			}
			if (pin.resLpHz > 0.0) {
				double fl = pin.resLpHz;
				if (fl > 0.45 * fs) fl = 0.45 * fs;
				const double al = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * fl / fs);
				lpy1 += al * (x - lpy1);
				x = lpy1;
			}
			if (pin.resDry != 1.0) x *= pin.resDry;
			if (pin.resWet > 0.0)
				x += pin.resWet * pitchResonator(xr, pin.resHz, pin.resT60);
			else { rzr = 0.0; rzi = 0.0; rni1 = 0.0; rni2 = 0.0; }
		} else { rzr = 0.0; rzi = 0.0; rni1 = 0.0; rni2 = 0.0; hpx1 = 0.0; hpy1 = 0.0; lpy1 = 0.0; }

		// ---- Steuerpfad ----
		// Shelving (DF2T): y = b0*x + z; z = b1*x - a1*y
		const double Vb = kVb1 * cv + zVb;
		zVb = kVb2 * cv - kVb3 * Vb;

		double Rf;
		if (crfValid && Vb == crfVb && offset == crfOff && crfTrimVer == trimVer) {
			Rf = crfRf;
		} else {
			Rf = controlToRf(Vb, offset);
			crfValid = true; crfVb = Vb; crfOff = offset; crfRf = Rf; crfTrimVer = trimVer;
		}

		// ---- Vactrol-Modell (Trapezintegration, richtungsabhaengige Zeitkonstante) ----
		const double dRf = Rf - Rfz1;
		const double tsc = pin.vactrolTimeScale > 1e-3 ? pin.vactrolTimeScale : 1e-3;
		double Rfv;
		if (pin.vactrolConductance) {
			// Zwei parallele Leitwert-Anteile (Erweiterung, nicht im Paper), siehe Params.
			const double kFast = 0.02, kSlowFall = 4.0, kSlowRise = 0.1, wSlow = 0.016;
			const double Gf = 1.0 / Rf;
			const double Gfz = Rfz1 > 0.0 ? 1.0 / Rfz1 : Gf;
			if (G1z1 <= 0.0) { G1z1 = Gf; G2z1 = Gf; }   // nach reset() ohne warmStart
			const double t1 = ((Gf > G1z1) ? trise : tfall) * kFast * tsc;
			const double a1 = 1.0 / (1.0 + 2.0 * fs * t1), b1 = a1 * (2.0 * fs * t1 - 1.0);
			const double G1 = a1 * (Gf + Gfz) + b1 * G1z1;
			const double t2 = ((Gf > G2z1) ? trise * kSlowRise : tfall * kSlowFall) * tsc;
			const double a2 = 1.0 / (1.0 + 2.0 * fs * t2), b2 = a2 * (2.0 * fs * t2 - 1.0);
			const double G2 = a2 * (Gf + Gfz) + b2 * G2z1;
			G1z1 = G1; G2z1 = G2;
			Rfv = (1.0 + wSlow) / (G1 + wSlow * G2);
		} else {
			const double t = ((pin.vactrolByState ? (Rf < Rfvz1) : (dRf < 0.0)) ? trise : tfall) * tsc;
			const double vk1 = 1.0 / (1.0 + 2.0 * fs * t);
			const double vk2 = vk1 * (2.0 * fs * t - 1.0);
			Rfv = vk1 * (Rf + Rfz1) + vk2 * Rfvz1;
		}
		Rfvz1 = Rfv;
		Rfz1 = Rf;

		// ---- Audiopfad ----
		double a = fb;
		if (pin.fbnorm && mode == MODE_LOWPASS) {
			a *= (2.0 * C1 * Ra + (C2 + C3) * (Ra + Rfv)) / (C3 * Ra);
		}

		// V/Oct (Erweiterung): Zeitskalierung des Filters, nur mit Resonanzstufe (C3 > 0). k = 1 = aus.
		double kPitch = 1.0;
		if (pin.pitchHz > 0.0 && C3 > 0.0) {
			// Rueckkopplung fuer den Bezugszustand "Gate voll offen" (Rfv = Rf bei Ifmax), unabhaengig vom
			// laufenden Vactrol-Zustand, damit die Note bei bewegtem Gate nicht neu gerechnet wird.
			const double aRef = pin.fbnorm
				? fb * (2.0 * C1 * Ra + (C2 + C3) * (Ra + rfOpen())) / (C3 * Ra)
				: fb;
			kPitch = pitchScale(pin.pitchHz, aRef, C3, Ra);
		}

		// Koeffizienten nur neu rechnen, wenn sich ihre Eingaenge (bitweise) geaendert haben.
		// Bei konstanter CV (Vactrol eingeschwungen) trifft das fast immer zu und spart ca. 25 Divisionen.
		if (!(coefValid && Rfv == cfRfv && a == cfA && C3 == cfC3 && Ra == cfRa && kPitch == cfK)) {
			const double Rfinv = 1.0 / Rfv;
			// kPitch = 1.0 ist exakt: Multiplikation mit 1 aendert kein Bit (bitgleich ohne V/Oct).
			const double a1 = Rfinv / C1 * kPitch;
			const double a2 = -(Rfinv + 1.0 / Ra) / C1 * kPitch;
			const double b1 = Rfinv / C2 * kPitch;
			const double b2 = -2.0 * b1;
			const double b3 = b1;
			const double b4 = C3 / C2;
			const double d1 = a;
			const double d2 = -1.0;

			cA1 = a1; cA2 = a2; cB1 = b1; cB2 = b2; cB3 = b3; cB4 = b4; cD1 = d1;
			cKoo  = 1.0 / (1.0 - a2 / (2.0 * fs));
			cKox  = a1 / (2.0 * fs) * cKoo;
			cKdo  = 2.0 * fs * d1;
			cKdx  = 2.0 * fs * d2;
			cKsdo = -4.0 * fs * d1;
			cKsdx = -4.0 * fs * d2;
			cKsxi = b1 / fs;
			cKsxx = b2 / fs;
			cKsxo = b3 / fs;
			cKsxd = b4 / fs;
			cKsox = a1 / fs;
			cKsoo = a2 / fs;

			// linearer Zweig (NL aus)
			const double yxden = a2 * b2 - a1 * b3
				- 2.0 * fs * (a2 + b2 + b4 * (a1 * d1 - a2 * d2))
				- 4.0 * fs * fs * (b4 * d2 - 1.0);
			const double yxnom = 2.0 * fs - a2;
			cKyx = yxnom / yxden;
			cKxo = 2.0 * fs * (2.0 * fs * b4 * d1 + b3) / yxden;
			cKxi = cKyx * b1;
			cKxd = cKyx * b4;
			cKxx = cKyx * 2.0 * fs;

			cfRfv = Rfv; cfA = a; cfC3 = C3; cfRa = Ra; cfK = kPitch;
			coefValid = true;
		}
		const double koo = cKoo, kox = cKox, kdo = cKdo, kdx = cKdx, ksdo = cKsdo, ksdx = cKsdx;
		const double ksxi = cKsxi, ksxx = cKsxx, ksxo = cKsxo, ksxd = cKsxd, ksox = cKsox, ksoo = cKsoo;

		double yo;
		if (pin.nl) {
			const double a1 = cA1, a2 = cA2, b1 = cB1, b2 = cB2, b3 = cB3, b4 = cB4, d1 = cD1;
			const double d2 = -1.0;
			const double th   = std::tanh(yoz1);
			const double t2m1 = th * th - 1.0;

			// siehe Errata der Referenz. Referenz: "+a1*d1*t2m1"; korrekt (direkte Loesung der
			// Gleichungen) ist "-a1*d1*t2m1", schaltbar ueber nlCorrected.
			const double sgn = pin.nlCorrected ? -1.0 : 1.0;
			const double yxden = a2 * b2 - a1 * b3
				- 2.0 * fs * (a2 + b2 + b4 * (sgn * a1 * d1 * t2m1 - a2 * d2))
				- 4.0 * fs * fs * (b4 * d2 - 1.0);
			const double yxnom = 2.0 * fs - a2;
			const double kyx   = yxnom / yxden;
			const double kxo   = 2.0 * fs * (-2.0 * fs * b4 * d1 * t2m1 + b3) / yxden;
			const double kxi   = kyx * b1;
			const double kxd   = kyx * b4;
			const double kxx   = kyx * 2.0 * fs;
			const double kxt   = kxx * b4 * d1;
			const double kxyoz1 = kxt * t2m1;

			const double yx = kxo * so + kxi * x + kxd * sd + kxx * sx
				+ kxt * th + kxyoz1 * yoz1;
			yo = koo * so + kox * yx;
			const double yonl = th - (yo - yoz1) * t2m1;
			// yd wird nur fuer den sx-Update gebraucht
			const double yd = sd + kdo * yonl + kdx * yx;

			sd = -sd + ksdo * yonl + ksdx * yx;
			sx = sx + ksxi * x + ksxx * yx + ksxo * yo + ksxd * yd;
			so = so + ksox * yx + ksoo * yo;
			yoz1 = yo;
		} else {
			const double kxo = cKxo, kxi = cKxi, kxd = cKxd, kxx = cKxx;

			const double yx = kxo * so + kxi * x + kxd * sd + kxx * sx;
			yo = koo * so + kox * yx;
			const double yd = sd + kdo * yo + kdx * yx;

			sd = -sd + ksdo * yo + ksdx * yx;
			sx = sx + ksxi * x + ksxx * yx + ksxo * yo + ksxd * yd;
			so = so + ksox * yx + ksoo * yo;
			yoz1 = yo;  // bleibt gepflegt, falls zur Laufzeit auf nl umgeschaltet wird
		}
		return yo;
	}

	// Momentanwert des Vactrol-Widerstands (fuer Anzeigen/Tests)
	double currentRfv() const { return Rfvz1; }

	// Vactrol-Widerstand bei voll offenem Gate (If = Ifmax), ca. 1,45 kOhm.
	static double rfOpen() {
		static const double v = A_ / std::pow(Ifmax, 1.4) + B_;
		return v;
	}

	// Klingfrequenz (Hz, analoger Prototyp) des Filters bei Vactrol-Widerstand Rf und Rueckkopplung a.
	// Das Filter ist 2. Ordnung: Yo/X = a1*b1 / (A s^2 + B s + C) mit (p = 1/(Rf C1), q = 1/(Ra C1), r = C1/C2,
	// b4 = C3/C2, d1 = a):  A = 1 + b4,  B = 2 p r + (p + q) A - p b4 d1,  C = p r (p + 2 q).
	// Hergeleitet aus den Zustandsgleichungen des Audiopfads (Gegenprobe: Nenner entspricht yxden im Code).
	// Gedaempfte Eigenfrequenz w_d^2 = C/A - B^2/(4 A^2); bei starker Daempfung (B^2/(4A^2) > C/(2A)) gilt
	// der stetige Ersatzwert w^2 = C/(2A).
	static double ringOmega(double Rf, double a, double C3, double Ra) {
		const double p = 1.0 / (Rf * C1), q = 1.0 / (Ra * C1), r = C1 / C2, b4 = C3 / C2;
		const double Aq = 1.0 + b4;
		const double Bq = 2.0 * p * r + (p + q) * Aq - p * b4 * a;
		const double Cq = p * r * (p + 2.0 * q);
		const double w2 = Cq / Aq;
		double damp = Bq * Bq / (4.0 * Aq * Aq);
		if (damp > 0.5 * w2) damp = 0.5 * w2;
		return std::sqrt(w2 - damp);
	}

	// Frequenzfaktor k fuer die Note hz (siehe Params::pitchHz): Klingfrequenz bei voll offenem Gate
	// (Rf bei Ifmax) mal k = Ziel. Das Ziel wird fuer die Bilineartransformation vorverzerrt
	// (w = 2 fs tan(pi f / fs)), damit die digitale Frequenz stimmt. Ergebnis wird zwischengespeichert.
	double pitchScale(double hz, double a, double C3, double Ra) {
		if (pkValid && hz == pkHz && a == pkA && C3 == pkC3 && Ra == pkRa && fs == pkFs)
			return pkK;
		const double PI = 3.14159265358979323846;
		double f = hz < 0.1 ? 0.1 : hz;
		if (f > 0.49 * fs) f = 0.49 * fs;
		const double wTarget = 2.0 * fs * std::tan(PI * f / fs);
		pkK = wTarget / ringOmega(rfOpen(), a, C3, Ra);
		pkHz = hz; pkA = a; pkC3 = C3; pkRa = Ra; pkFs = fs; pkValid = true;
		return pkK;
	}

private:
	// Gestimmter Resonator in gekoppelter Form (komplexer Zustand, eine Drehung pro Sample):
	//   z[n] = r e^{j w0} z[n-1] + e^{j w0} x[n],  y[n] = Im z[n],  r = 10^(-3 / (T60 fs)).
	// Impulsantwort r^n sin((n+1) w0): Amplitude 1, Frequenz genau hz, Spitzen-normiert. Die Drehung
	// erhaelt den Betrag des Zustands, auch wenn sich w0 von Sample zu Sample aendert (Audiorate-V/Oct);
	// die direkte Zweipol-Form kann dabei aufschaukeln (Studie 2026-10-24).
	double pitchResonator(double x, double hz, double t60) {
		if (!(rcValid && hz == rcHz && t60 == rcT60 && fs == rcFs)) {
			const double PI = 3.14159265358979323846;
			double f = hz < 0.1 ? 0.1 : hz;
			if (f > 0.45 * fs) f = 0.45 * fs;
			const double T = t60 > 1e-3 ? t60 : 1e-3;
			const double w0 = 2.0 * PI * f / fs;
			const double r = std::pow(10.0, -3.0 / (T * fs));
			rcw = std::cos(w0); rsw = std::sin(w0);
			rrc = r * rcw; rrs = r * rsw; rinv = 0.5 / rsw;
			rcHz = hz; rcT60 = t60; rcFs = fs; rcValid = true;
		}
		const double nr = rrc * rzr - rrs * rzi + rcw * x;
		const double ni = rrs * rzr + rrc * rzi + rsw * x;
		rzr = nr; rzi = ni;
		// Nullstelle bei 0 Hz und Nyquist: (1 - z^-2) / (2 sin w0). Die Spitzenverstaerkung bei der Resonanz bleibt gleich
		// (Impulsantwort Amplitude 1), aber Gleichanteil und Tiefen eines Pulses/Gate-Sprungs erzeugen keinen tiefen Plopp mehr.
		const double nb = (ni - rni2) * rinv;
		rni2 = rni1; rni1 = ni;
		// Weiche Begrenzung des Resonatorausgangs: linear bis 4 (die Impulsantwort hat Amplitude 1), darueber stetig
		// gegen 8. Der Resonator summiert bei Dauerrauschen und langer T60 bis zum 70-fachen des Eingangspegels auf;
		// ohne Begrenzung loest das den Schutz des Moduls aus (Studie 2026-10-24).
		const double a = std::fabs(nb);
		if (a <= 4.0) return nb;
		const double lim = 4.0 + 4.0 * std::tanh((a - 4.0) / 4.0);
		return nb < 0.0 ? -lim : lim;
	}

	// Geglaetteter linearer Gate-Anteil: g = cv / 10, Trapezglaettung wie beim Vactrol.
	double linearGain(const Params& pin) {
		const double g = clampd(pin.cv, 0.0, 10.0) * 0.1;
		if (!gainValid) { gLinz1 = g; gLinv1 = g; gainValid = true; }
		const double tsc = pin.vactrolTimeScale > 1e-3 ? pin.vactrolTimeScale : 1e-3;
		const double t = ((g > gLinv1) ? trise : tfall) * tsc;
		const double k1 = 1.0 / (1.0 + 2.0 * fs * t);
		const double k2 = k1 * (2.0 * fs * t - 1.0);
		const double gv = k1 * (g + gLinz1) + k2 * gLinv1;
		gLinz1 = g; gLinv1 = gv;
		return gv;
	}

	// Reiner linearer VCA (Modus VCA): y = g * x, ohne Filter.
	double processLinearVca(double x, const Params& pin) {
		paperStale = true;
		// Filterzustaende auf 0, damit der Rueckwechsel in die Filtermodi sauber startet
		sd = 0; sx = 0; so = 0; yoz1 = 0;
		return linearGain(pin) * x;
	}

	static double clampd(double v, double lo, double hi) {
		return v < lo ? lo : (v > hi ? hi : v);
	}

	void updateTrim(double trim, double r8) {
		if (trim == cachedTrim && r8 == cachedR8) return;
		cachedTrim = trim;
		cachedR8 = r8;
		R6 = R6max * trim;
		alpha = 1.0 + (R6 + R7) * (1.0 / R3 + 1.0 / R5);
		beta = (1.0 / alpha - 1.0) / (R6 + R7) - 1.0 / r8;
		kIa = xi * alpha * n * VT / (G * (R6 + R7 - 1.0 / (alpha * beta)));
		Rsum = R6 + R7;
		alphaBeta = alpha * beta;
		kIa3 = VB / Rsum;
		kIa4 = (gamma * G * VB + alpha * R9 * (VB * beta + Ifmax))
			/ (gamma * G * Rsum + R9);
		trimVer++;
	}

	// Vb (Shelving-Ausgang) -> LED-Strom If -> Vactrol-Widerstand Rf.
	// Vorausgesetzt: updateTrim() wurde fuer den aktuellen trim aufgerufen.
	double controlToRf(double Vb, double offset) const {
		// Vs / (R3 * (1 + R1/R2)) mit R2 = R1R2*offset, R1 = R1R2 - R2
		// = Vs * offset / R3  (auch fuer offset = 0 ohne Division durch 0)
		const double Ia = Vb / R5 + Vs * offset / R3;

		// Statische Nichtlinearitaet (Lambert-W-Naeherung)
		double V3;
		if (Ia <= -kIa) {
			V3 = -Ia / alphaBeta;
		} else if (Ia >= kIa) {
			V3 = kL * alpha / G * n * VT - Ia * Rsum;
		} else {
			const double warg = G * Ia * (Rsum - 1.0 / alphaBeta) / (alpha * n * VT);
			const double w = k0 + warg * (k1p + warg * (k2p + warg * k3p));
			V3 = -alpha / G * n * VT * w - Ia / alphaBeta;
		}

		const double kIa2 = alpha * (Ifmin - beta * V3);
		double If;
		if (Ia <= kIa2) {
			If = Ifmin;
		} else if (Ia <= kIa3) {
			If = beta * V3 + Ia / alpha;
		} else if (Ia >= kIa4) {
			If = Ifmax;
		} else {
			If = gamma * G * (Ia * Rsum - VB) / (alpha * R9) - beta * VB + Ia / alpha;
		}

		return A_ / std::pow(If, 1.4) + B_;
	}
};

// Hinweis (C++11): Die statischen constexpr-Konstanten werden nur als Werte benutzt
// (nie per Referenz gebunden), brauchen also keine Out-of-class-Definition.
// Eine solche Definition in diesem Header wuerde bei mehreren Uebersetzungseinheiten
// zu Mehrfachdefinitionen fuehren (inline-Variablen gibt es erst ab C++17).
