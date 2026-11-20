#pragma once
// Minimaler Ersatz fuer die Rack-SDK-Header, nur fuer die Standalone-Tests
// der Afg-Engine (Marf248.hpp). Bildet exakt das Verhalten von rack::clamp,
// dsp::SchmittTrigger und dsp::PulseGenerator nach (Rack 2).
#include <algorithm>
namespace rack {
template <typename T>
T clamp(T x, T a, T b) { return std::min(std::max(x, a), b); }
namespace dsp {
struct SchmittTrigger {
	bool state = true;
	bool process(float in, float lowThreshold = 0.f, float highThreshold = 1.f) {
		if (state) {
			if (in <= lowThreshold) state = false;
		}
		else {
			if (in >= highThreshold) { state = true; return true; }
		}
		return false;
	}
	bool isHigh() const { return state; }
	void reset() { state = true; }
};
struct PulseGenerator {
	float remaining = 0.f;
	void reset() { remaining = 0.f; }
	bool process(float deltaTime) {
		if (remaining > 0.f) { remaining -= deltaTime; return true; }
		return false;
	}
	void trigger(float duration = 1e-3f) {
		if (duration > remaining) remaining = duration;
	}
};
} // namespace dsp
} // namespace rack
using namespace rack;
