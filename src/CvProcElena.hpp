#pragma once
#include <rack.hpp>
#include "DcBlock.hpp"

using namespace rack;

// ---------------------------------------------------------------------
// Rechen-Engine fuer "Elena" (digitaler CV-Prozessor):
// A/B-Kennlinien-Sektion und Slew-Limiter
// (Sektionen C-H). Alles freie Funktionen bzw. kleine Zustands-Structs,
// keine Rack-Abhaengigkeit ausser simd/clamp.
//
// Quelle der A/B-Formeln: Referenzdokumentation, Beschreibung der
// Transformationssektion (Werte auf 1 normiert, 10 V = 1):
//   Vin  = InSel * A2 + (1 - InSel) * A1
//   ohne Breakpoint:  Vout = Vin * V10 + (1 - Vin) * V0
//   mit Breakpoint:   Vin < bpx: Vout = Vin*bpy/bpx + (1 - Vin/bpx) * V0
//                     Vin > bpx: Vout = ((Vin-bpx)*V10 + (1-Vin)*bpy) / (1-bpx)
// ---------------------------------------------------------------------
namespace CvProcElena {

// Schutz gegen Division durch 0, wenn der Breakpoint am Rand liegt.
static constexpr float BP_EPS = 0.001f;

static constexpr float SLEW_MIN_S = 0.01f;
static constexpr float SLEW_MAX_S = 10.f;

// Regler 0..1 -> Slewzeit 0,01..10 s, exponentiell (3 Dekaden). Nie 0,
// daher nie ein "echtes" Rechteck/Saegezahn (wie im Original).
inline float knobToSeconds(float pos) {
	pos = rack::clamp(pos, 0.f, 1.f);
	return SLEW_MIN_S * std::pow(SLEW_MAX_S / SLEW_MIN_S, pos);
}

inline float secondsToKnob(float t) {
	t = rack::clamp(t, SLEW_MIN_S, SLEW_MAX_S);
	return std::log(t / SLEW_MIN_S) / std::log(SLEW_MAX_S / SLEW_MIN_S);
}

// Transfer-Kennlinie fuer vier Kanaele. vin, v0, v10, bpx, bpy sind auf
// 0..1 normiert; bpOn ist eine float_4-Maske (Breakpoint aktiv).
inline simd::float_4 transfer(simd::float_4 vin, simd::float_4 v0, simd::float_4 v10,
                              simd::float_4 bpOn, simd::float_4 bpx, simd::float_4 bpy) {
	using simd::float_4;
	float_4 one(1.f);
	float_4 lin = vin * v10 + (one - vin) * v0;

	bpx = simd::clamp(bpx, float_4(BP_EPS), float_4(1.f - BP_EPS));
	float_4 lo = vin * bpy / bpx + (one - vin / bpx) * v0;
	float_4 hi = ((vin - bpx) * v10 + (one - vin) * bpy) / (one - bpx);
	float_4 seg = simd::ifelse(vin < bpx, lo, hi);

	return simd::ifelse(bpOn, seg, lin);
}

// DC-Blocker: Implementierung in DcBlock.hpp (gemeinsam mit Minerva); hier nur
// als Namen in diesem Namespace verfuegbar, damit Elena.cpp unveraendert bleibt.
using DcBlock::DC_BLOCK_HZ;
using DcBlock::dcBlockCoeff;
using DcBlock::dcBlockStep;

// Ein Slew-Schritt fuer einen Kanal (Werte in Volt).
//  Time: lineare Steigung, tPos/tNeg = Sekunden fuer 10 V.
//  Rate: exponentielles Nachlaufen (Steigung proportional zum Abstand),
//        tPos/tNeg = Zeit bis ca. 95 % des Sprungs (tau = t/3).
// ANNAHME: Die Referenzdokumentation beschreibt Rate/Time nur vage; diese
// Interpretation ist nicht belegt.
inline float slewStep(float in, float out, float tPos, float tNeg, bool rateMode, float dt) {
	float diff = in - out;
	float t = (diff >= 0.f) ? tPos : tNeg;
	if (rateMode) {
		float x = dt * 3.f / t;
		return out + diff * (x / (1.f + x));
	}
	float step = 10.f * dt / t;
	return out + rack::clamp(diff, -step, step);
}

// Erkennt Richtungsumkehr des Ausgangssignals (Auto-Pulse). Merkt sich
// das letzte Extremum; erst wenn das Signal um mehr als hyst davon
// zurueckweicht, gilt die Richtung als umgekehrt.
struct ReversalDetector {
	int dir = 0; // 0 = noch unbekannt, +1 steigend, -1 fallend
	float extreme = 0.f;

	void reset(float x) {
		dir = 0;
		extreme = x;
	}

	// true genau in dem Sample, in dem die Umkehr erkannt wird
	bool process(float x, float hyst) {
		if (dir == 0) {
			if (x > extreme + hyst) {
				dir = 1;
				extreme = x;
			}
			else if (x < extreme - hyst) {
				dir = -1;
				extreme = x;
			}
			return false;
		}
		if (dir > 0) {
			if (x > extreme) {
				extreme = x;
			}
			else if (x < extreme - hyst) {
				dir = -1;
				extreme = x;
				return true;
			}
		}
		else {
			if (x < extreme) {
				extreme = x;
			}
			else if (x > extreme + hyst) {
				dir = 1;
				extreme = x;
				return true;
			}
		}
		return false;
	}
};

} // namespace CvProcElena
