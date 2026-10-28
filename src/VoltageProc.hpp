#pragma once
#include <rack.hpp>

using namespace rack;

// ---------------------------------------------------------------------
// Rechen-Engine fuer "Minerva" (polyphoner Dual-Spannungsprozessor).
// Reine lineare Analogmathematik, kein Zeitverhalten, keine
// Zustandsvariablen - daher als freie Funktion statt Klasse. Rechnet
// vier Kanaele gleichzeitig (float_4); ein Aufruf = ein Kanal-Block.
//
//   Vout = Va*K + ((Vb + 5 V*plus5)*(1-M) + Vc*M) + Voset
//   M    = Regler + M-CV / M_CV_VOLTS_FULL
//          Standard UNBEGRENZT (M > 1 bzw. < 0 invertiert Vb bzw. Vc,
//          Vier-Quadranten-Multiplikation); mLimited = true klemmt hart auf
//          0..1 (Kinovice-Entscheidung, per Modul-Kontextmenue waehlbar).
//   Voset = Regler(0..1) * VOSET_MAX_VOLTS
//   plus5 = "+.5"-Schalter (0/1): addiert 5 V auf Vb, INNERHALB der
//           Ueberblendung (bei unbelegtem Vb liegen dann 5 V an -> invertierte
//           M-Steuerung moeglich; Voset kann das nicht, da es erst die Summe
//           verschiebt).
//
// Optional danach: Ausgangsbegrenzung limitOutput() (Aus / Weich / Hart),
// Pegel fest +-12 V, Weich-Schwelle fest 0,9*L = 10,8 V.
//
// processParts() liefert die Summe in zwei Anteilen, damit ein DC-Blocker NUR
// den Signalanteil bearbeiten kann (Kinovice-Entscheidung, siehe Minerva.cpp):
//   signalPart = Va*K + Vb*(1-M) + Vc*M       Signal und Crossfader
//   staticPart = 5 V*plus5*(1-M) + Voset      bewusste Gleichanteile
// Vout = signalPart + staticPart; process() ist die Summe beider Anteile.
//
// ANNAHME (Referenzdokumentation nennt keinen Wert): M-CV wirkt mit 10 V = voller Hub
// (0-10 V ueberstreicht die komplette Ueberblendung Vb -> Vc). Aenderung
// nur hier an einer Stelle noetig.
// ---------------------------------------------------------------------
namespace VoltageProc {

static constexpr float M_CV_VOLTS_FULL = 10.f;
static constexpr float VOSET_MAX_VOLTS = 10.f;
static constexpr float PLUS5_VOLTS = 5.f;
// Ausgangsbegrenzung: fester Pegel L und Anteil des linearen Bereichs.
static constexpr float OUT_LIMIT_VOLTS = 12.f;
static constexpr float SOFT_KNEE_RATIO = 0.9f; // T = 0,9 * L = 10,8 V

// Modi fuer limitOutput() (entsprechen dem JSON-Wert "outputClip").
enum OutputClipMode { CLIP_OFF = 0, CLIP_SOFT = 1, CLIP_HARD = 2 };

// Zerlegt die Ausgangsspannung in Signalanteil und Festanteil (siehe oben).
// Beide Anteile sind in Va, Vb und Vc bzw. in M-Regler/CV/Oset/plus5 getrennt:
// der Festanteil haengt nicht von den Signaleingaengen ab.
inline void processParts(simd::float_4 va, simd::float_4 vb, simd::float_4 vc,
                         simd::float_4 mCv, simd::float_4 k, simd::float_4 mKnob,
                         simd::float_4 vosetKnob, simd::float_4 plus5, bool mLimited,
                         simd::float_4& signalPart, simd::float_4& staticPart) {
	using simd::float_4;
	// M = Regler + CV. Standard unbegrenzt; bei mLimited hart auf 0..1.
	float_4 m = mKnob + mCv * float_4(1.f / M_CV_VOLTS_FULL);
	if (mLimited)
		m = simd::clamp(m, float_4(0.f), float_4(1.f));
	float_4 oneMinusM = float_4(1.f) - m;
	signalPart = va * k + vb * oneMinusM + vc * m;
	// plus5 ist 0/1 je Kanal (Schalterstellung als Param-Wert).
	staticPart = plus5 * float_4(PLUS5_VOLTS) * oneMinusM + vosetKnob * float_4(VOSET_MAX_VOLTS);
}

// Gesamtsumme ohne DC-Blocker und ohne Ausgangsbegrenzung.
inline simd::float_4 process(simd::float_4 va, simd::float_4 vb, simd::float_4 vc,
                             simd::float_4 mCv, simd::float_4 k, simd::float_4 mKnob,
                             simd::float_4 vosetKnob, simd::float_4 plus5, bool mLimited) {
	simd::float_4 signalPart, staticPart;
	processParts(va, vb, vc, mCv, k, mKnob, vosetKnob, plus5, mLimited, signalPart, staticPart);
	return signalPart + staticPart;
}

// Ausgangsbegrenzung, vier Kanaele gleichzeitig. mode: CLIP_OFF = unveraendert,
// CLIP_HARD = clamp(x, -L, +L), CLIP_SOFT = bis T exakt linear, darueber
// sanft nach L (Soft-Knee). Rack hat kein SIMD-tanh, daher rationale
// Naeherung s(t) = t*(27+t^2)/(27+9t^2) mit t auf 3 geklemmt (s(3) = 1,
// Steigung 1 bei 0 wie tanh). Verzweigungsfrei:
//   a = |x|;  t = clamp((a - T)/(L - T), 0, 3);
//   y = sign(x) * ( min(a, T) + (L - T) * s(t) )
// Ab a = T + 3*(L - T) = 14,4 V liegt y konstant bei L.
inline simd::float_4 limitOutput(simd::float_4 x, int mode) {
	using simd::float_4;
	if (mode == CLIP_OFF)
		return x;
	const float L = OUT_LIMIT_VOLTS;
	if (mode == CLIP_HARD)
		return simd::clamp(x, float_4(-L), float_4(L));

	const float T = SOFT_KNEE_RATIO * L;
	const float W = L - T;
	float_4 a = simd::abs(x);
	float_4 t = simd::clamp((a - float_4(T)) * float_4(1.f / W), float_4(0.f), float_4(3.f));
	float_4 t2 = t * t;
	float_4 sCurve = t * (float_4(27.f) + t2) / (float_4(27.f) + float_4(9.f) * t2);
	float_4 y = simd::clamp(a, float_4(0.f), float_4(T)) + float_4(W) * sCurve;
	return simd::ifelse(x < float_4(0.f), -y, y);
}

} // namespace VoltageProc
