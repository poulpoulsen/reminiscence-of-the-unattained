#pragma once
#include <rack.hpp>

using namespace rack;

// ---------------------------------------------------------------------
// Gemeinsamer DC-Blocker fuer Minerva und Elena: einpoliger Hochpass
//   y[n] = x[n] - x[n-1] + R * y[n-1],  R = exp(-2*pi*fc/fs)
// mit fester Grenzfrequenz DC_BLOCK_HZ (Kinovice-Entscheidung: ca. 10 Hz).
// Entfernt Gleichanteile, daher nur bei Audio sinnvoll - langsame CV-Anteile
// unter ca. 10 Hz werden ebenfalls entfernt (-3 dB bei 10 Hz, ca. -7 dB bei
// 5 Hz, -20 dB bei 1 Hz). Der Filterzustand (x[n-1], y[n-1]) liegt beim
// Aufrufer, je Kanal.
// ---------------------------------------------------------------------
namespace DcBlock {

static constexpr float DC_BLOCK_HZ = 10.f;

inline float dcBlockCoeff(float sampleRate) {
	// 2*pi als Literal (M_PI ist nicht ueberall verfuegbar, siehe PanelLabel.hpp)
	return std::exp(-6.2831853f * DC_BLOCK_HZ / sampleRate);
}

inline simd::float_4 dcBlockStep(simd::float_4 x, simd::float_4& xPrev, simd::float_4& yPrev, float r) {
	simd::float_4 y = x - xPrev + yPrev * simd::float_4(r);
	xPrev = x;
	yPrev = y;
	return y;
}

inline float dcBlockStep(float x, float& xPrev, float& yPrev, float r) {
	float y = x - xPrev + r * yPrev;
	xPrev = x;
	yPrev = y;
	return y;
}

} // namespace DcBlock
