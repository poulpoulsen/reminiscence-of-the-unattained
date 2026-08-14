#pragma once
#include "plugin.hpp"
#include <cmath>
#include <string>

static const int NUM_STAGES = 16;

// Dauer der Retrigger-Luecke fuer den CV-Expander (Marf248CvExpander,
// Gate A/B): wenn eine Section eine Stage OHNE Indexwechsel neu betritt
// (Pendulum-Wendepunkt-Wiederholung, Strobe auf dieselbe Adresse, Sustain/
// Enable-Freigabe), zieht das Gate kurz auf 0V, damit nachgeschaltete
// Huellkurven zuverlaessig neu retriggern - siehe Afg::regateGen.
static const float REGATE_GAP_SECONDS = 0.005f;

enum StageModifierBits {
	MOD_QUANT_OCT   = 1 << 0,
	MOD_SLOPED      = 1 << 1,
	MOD_STOP        = 1 << 2,
	MOD_SUSTAIN     = 1 << 3,
	MOD_ENABLE      = 1 << 4,
	MOD_FIRST       = 1 << 5,
	MOD_LAST        = 1 << 6,
	// Slew-Kurvenform, nur wirksam wenn MOD_SLOPED gesetzt ist. Kein Bit
	// gesetzt = linear (Default). Beide Bits sind exklusiv zueinander.
	MOD_SLEW_EXP    = 1 << 7,
	MOD_SLEW_LOG    = 1 << 8,
	// Range: Full/Half/Limited, exklusiv zueinander. Kein Bit gesetzt = Full
	// (Default, 0-10V unveraendert). MOD_RANGE_HALF = Half (0-5V). Bei
	// MOD_RANGE_LIMITED greift zusaetzlich der 3-Bit-Oktav-Offset weiter unten.
	MOD_RANGE_HALF    = 1 << 9,
	MOD_RANGE_LIMITED = 1 << 10,
	// Voltage/Time Source: External/Internal, je ein eigenes Bit (kein Bit =
	// Internal, Default). Bei External wird nicht der Slider-Rohwert selbst
	// verwendet, sondern seine Position dient als Kanalwahl (A-D) fuer die
	// vier externen CV-Eingaenge. Die Aufloesung von Slider-Position -> Kanal
	// -> tatsaechlichem Wert passiert VOR Range/Quantize in Marf248::process()
	// (nicht hier in der Afg-Kernlogik, da die externen Inputs modulweit
	// geteilt sind und Afg selbst keine Kenntnis von Inputs hat).
	MOD_VOLT_EXT = 1 << 14,
	MOD_TIME_EXT = 1 << 15,
	// Programmed Pulse Outputs (Manual "Output Pulses 1/2"): je Stage zwei
	// unabhaengige Trigger-Flags, unabhaengig von den Voltage/Time-
	// Modifiern. Loesen beim Erreichen der Stage einen kurzen Trigger auf
	// P1OUT/P2OUT aus (analog zu APOUT, aber nur wenn das jeweilige Bit
	// gesetzt ist) - kontinuierliche Flankenerkennung, siehe Afg::process().
	MOD_PULSE1 = 1 << 16,
	MOD_PULSE2 = 1 << 17,
	// Skip: reine Port-Erweiterung ohne Hardware-Vorbild. Eine als Skip
	// markierte Stage wird beim automatischen Zeitablauf komplett
	// uebersprungen (keine Zeit, keine Trigger, keine Voltage-Ausgabe davon)
	// - siehe Afg::advanceStage(). Manuelle Adressierung (Reset/Strobe/
	// Continuous) ignoriert Skip bewusst, da dort die Stage explizit
	// gewaehlt wird.
	MOD_SKIP = 1 << 18,
	// Normalize: reine Port-Erweiterung (wie Skip), kein Hardware-Vorbild.
	// Nur wirksam in Kombination mit MOD_SLEW_EXP/MOD_SLEW_LOG bei
	// extremer Curve-Trimpot-Stellung (siehe applySlewCurve()): streckt
	// den "sichtbar bewegten" Teil der Kurve auf die volle Stage-Zeit,
	// statt ihn bei hoher Power in einen winzigen Zeitbruchteil zu
	// quetschen. Bewusst als eigener, abschaltbarer Modifier statt einer
	// stillen Korrektur, da sich dadurch die Kurvenform selbst spuerbar
	// aendert (flacher an den Raendern, steiler in der Mitte).
	MOD_SLEW_NORMALIZE = 1 << 19,
	// Time Range (Manual-Aequivalent TR1-4, im Python-Original MOD_TRANGE1-4):
	// 4 exklusive Zeitbereiche fuer die Stage-Zeit, analog zum Voltage-Range-
	// Modifier oben, aber wirkt NICHT in Afg::process() (Afg kennt nur die
	// bereits fertig aufgeloeste Sekundenzeit) - stattdessen ausschliesslich
	// in Marf248::process() (interner Time-Knob, Time External, additive CV
	// vom CV-Expander, siehe dortige Kommentare). Kein Bit gesetzt = TR3
	// (.2-3s, Default), analog zum "kein Bit = Default"-Muster bei Slope/
	// Range/Shape oben. Drei statt vier Bits genuegen, da TR3 implizit durch
	// Abwesenheit aller drei kodiert wird (spart ein Bit, gleiches Prinzip
	// wie MOD_RANGE_HALF/LIMITED).
	MOD_TRANGE_1 = 1 << 20,  // .002 - .03s (schnellster Bereich)
	MOD_TRANGE_2 = 1 << 21,  // .02  - .3s
	MOD_TRANGE_4 = 1 << 22,  // 2    - 30s (langsamster Bereich)
	// Mute: unterdrueckt die Trigger-Erzeugung von P1OUT/P2OUT UND APOUT
	// (All Pulses) fuer diese Stage komplett (siehe Afg::process(),
	// pulse1Active/pulse2Active, sowie allPulseGen.trigger() in
	// jumpToStage()/advanceStage()), OHNE die MOD_PULSE1/MOD_PULSE2-Bits
	// selbst zu veraendern - die Programmierung bleibt also erhalten und
	// greift wieder normal, sobald Mute aufgehoben wird. APOUT ist
	// bewusst mit einbezogen (Kinovice-Entscheidung), da es sonst
	// umstaendlich waere, P1 UND P2 einzeln deaktivieren zu muessen, nur
	// um eine Stage wirklich komplett stumm zu bekommen - APOUT feuert
	// sonst unabhaengig von MOD_PULSE1/2 bei jeder Stage
	// (allPulseGen = reiner Stage-Wechsel-Marker). CV/Slew/Voltage-
	// Ausgabe der Stage bleibt davon vollstaendig unbeeinflusst (reine
	// Pulse-Unterdrueckung, kein Eingriff in applyRange()/
	// applySlewCurve()). Vorerst nur per Pegel-Override vom Trigger-
	// Expander gesetzt (STAGE_MUTE_POLY_INPUT, siehe
	// Marf248TriggerExpander.hpp) - ein Panel-Button/Light auf dem
	// Hauptmodul folgt erst mit dem separat geplanten Panel-Relayout
	// (Kinovice-Entscheidung, eigene Session).
	MOD_MUTE = 1 << 23,
	// Quantized Glide (Kinovice-Feature, keine Buchla/Tiptop-Original-
	// Entsprechung): im Original quantisiert MOD_QUANT_OCT nur den
	// Zielwert (target) der Stage, der Slew-Verlauf dorthin bleibt eine
	// glatte Kurve (siehe MOD_QUANT_OCT-Kommentar/quantizeToScale()
	// weiter unten). Mit MOD_QUANT_GLIDE wird stattdessen der komplette
	// interpolierte Verlauf waehrend des Slews Sample fuer Sample auf
	// das globale Scale/Key-Raster gesnapt - dadurch entsteht die
	// hoerbare/sichtbare Treppe *innerhalb* eines einzelnen Stage-
	// Uebergangs (vgl. externer Quantizer im Signalweg nach einem
	// klassischen MARF/248t). Nur wirksam in Kombination mit
	// MOD_SLOPED (ohne Slope springt der Output ohnehin sofort auf den
	// Zielwert, siehe process()); wirkt unabhaengig von MOD_QUANT_OCT -
	// auch wenn der Zielwert selbst nicht separat quantisiert ist, snapt
	// MOD_QUANT_GLIDE trotzdem kontinuierlich, wodurch der Endwert de
	// facto ebenfalls quantisiert erreicht wird.
	MOD_QUANT_GLIDE = 1 << 24,
};

// Sekunden-Spannen der 4 Time-Range-Bereiche, indiziert 0..3 (TR1..TR4).
// getTimeRangeIndex() loest die exklusiven MOD_TRANGE_*-Bits zu diesem Index
// auf; TR3 (Index 2) ist der Default, wenn keines der drei Bits gesetzt ist.
static const float TIME_RANGE_MIN[4] = {0.002f, 0.02f, 0.2f, 2.f};
static const float TIME_RANGE_MAX[4] = {0.03f,  0.3f,  3.f,  30.f};

inline int getTimeRangeIndex(int mod) {
	if (mod & MOD_TRANGE_1) return 0;
	if (mod & MOD_TRANGE_2) return 1;
	if (mod & MOD_TRANGE_4) return 3;
	return 2; // TR3, Default
}

// Oktav-Offset im Limited-Range-Modus: 3 Bits kodieren einen Index 0..4, der
// auf +0/+2/+4/+6/+8 Volt Offset abgebildet wird (siehe RANGE_OFFSET_VOLTS).
// Liegt ausserhalb von StageModifierBits, da es sich um ein Werte-Feld statt
// eines einzelnen Flags handelt.
static const int RANGE_OFFSET_SHIFT = 11;
static const int RANGE_OFFSET_MASK  = 0x7 << RANGE_OFFSET_SHIFT;
static const float RANGE_OFFSET_VOLTS[5] = {0.f, 2.f, 4.f, 6.f, 8.f};

inline int getRangeOffsetIndex(int mod) {
	return (mod & RANGE_OFFSET_MASK) >> RANGE_OFFSET_SHIFT;
}

inline int setRangeOffsetIndex(int mod, int idx) {
	return (mod & ~RANGE_OFFSET_MASK) | ((idx & 0x7) << RANGE_OFFSET_SHIFT);
}

// Zonen-Schwellen fuer den Range-CV-Override (Trigger-Expander,
// STAGE_RANGE_POLY_INPUT, Session "Range CV Override"): 7 gleich grosse
// Zonen im 0-10V-Bereich, eine je Range-Position (F/H/L0/L2/L4/L6/L8) -
// identisches Positions-Encoding wie der Panel-Drehschalter oben
// (rangePos 0=Full, 1=Half, 2..6=Limited +0..+8, siehe
// Marf248::process()). Zonenbreite = 10V/7 ~= 1.4286V; 6 Schwellwerte
// trennen die 7 Zonen (Zone k liegt bei Spannungen >= Schwelle[k-1] und
// < Schwelle[k], Zone 0 darunter). Explizites Array statt Division im
// Hot-Path, damit die Zonengrenzen an einer Stelle nachvollziehbar
// stehen (analog zu TIME_RANGE_MIN/MAX oben).
static const float RANGE_ZONE_THRESHOLDS[6] = {
	10.f / 7.f * 1.f,
	10.f / 7.f * 2.f,
	10.f / 7.f * 3.f,
	10.f / 7.f * 4.f,
	10.f / 7.f * 5.f,
	10.f / 7.f * 6.f,
};

// Loest eine 0-10V-Spannung zur Range-Position (0..6) auf, siehe
// RANGE_ZONE_THRESHOLDS oben. Spannung wird vorab auf 0-10V geklemmt,
// daher immer ein gueltiger Rueckgabewert.
inline int resolveRangeZone(float voltage) {
	float v = rack::clamp(voltage, 0.f, 10.f);
	int zone = 0;
	for (int i = 0; i < 6; i++) {
		if (v >= RANGE_ZONE_THRESHOLDS[i])
			zone = i + 1;
	}
	return zone;
}

// Referenz-Kruemmung bei Knob-Mittelstellung (0.5) - entspricht dem bisherigen
// festen Wert. Ueber den Curve-Knob je Stage wird die tatsaechliche Power
// stufenlos dagegen verschoben (siehe Marf248::process()).
static const float SLEW_CURVE_POWER = 2.5f;

// Der Curve-Knob bildet nicht auf einen fixen Exponent-Faktor ab, sondern auf
// einen Faktor, der selbst fliessend von CURVE_FACTOR_CENTER (nahe der
// Knob-Mitte, feine Regelung) zu CURVE_FACTOR_EXTREME (an den Anschlaegen,
// ausgepraegtere Kurvenform) wandert. Siehe Marf248::process() fuer die
// quadratische Uebergangs-Kurve.
static const float CURVE_FACTOR_CENTER  = 2.f;
static const float CURVE_FACTOR_EXTREME = 3.f;

// Wendet die per Stage gewaehlte Slew-Kurvenform auf den linearen Phasenwert
// t (0..1) an, mit variabler Kruemmungs-Staerke "power":
//   EXPONENTIELL: f(t) = 1 - (1-t)^power  (schneller Start, sanftes Ziel)
//   LOGARITHMISCH: f(t) = t^power          (sanfter Start, schnelles Ziel)
// power == 1.0 ergibt in beiden Faellen eine Gerade (linear).
//
// Bei hoher Power (Curve-Trimpot nahe den Anschlaegen) erreicht die
// unskalierte EXP-Kurve ihr Ziel schon weit vor Ablauf der Stage-Zeit
// (bzw. bleibt LOG lange nahe am Startwert und springt erst spaet) - der
// "sichtbar bewegte" Teil der Kurve wird dadurch in einen winzigen
// Zeitbruchteil gequetscht. MOD_SLEW_NORMALIZE (optionaler Modifier, siehe
// Marf248.hpp) korrigiert das, indem die Zeitachse so gestreckt wird, dass
// der Punkt, an dem die Kurve SLEW_NORMALIZE_THRESHOLD (bzw. bei LOG das
// Komplement davon) erreicht, exakt auf t=1 faellt statt vorher.
static const float SLEW_NORMALIZE_THRESHOLD = 0.999f;

// Normalisierte EXP-Kurve: s ist der Zeitpunkt, an dem die theoretische
// (unskalierte) Kurve die Schwelle erreichen wuerde. t wird auf [0, s]
// gestaucht statt auf [0, 1], das Ergebnis anschliessend durch die
// Schwelle geteilt, damit bei t=1 trotzdem exakt 1.0 erreicht wird (kein
// "Haengenbleiben" bei 99.9%).
inline float slewExpNormalized(float t, float power) {
	float s = 1.f - std::pow(1.f - SLEW_NORMALIZE_THRESHOLD, 1.f / power);
	float scaledT = t * s;
	return (1.f - std::pow(1.f - scaledT, power)) / SLEW_NORMALIZE_THRESHOLD;
}

inline float applySlewCurve(float t, int mod, float power) {
	bool normalize = mod & MOD_SLEW_NORMALIZE;
	if (mod & MOD_SLEW_EXP) {
		if (normalize) {
			return slewExpNormalized(t, power);
		}
		return 1.f - std::pow(1.f - t, power);
	}
	else if (mod & MOD_SLEW_LOG) {
		if (normalize) {
			// LOG ist zeitlich gespiegeltes EXP (f_log(t) = 1 - f_exp(1-t));
			// dieselbe Beziehung gilt auch fuer die normalisierte Variante,
			// daher genuegt es, slewExpNormalized gespiegelt aufzurufen statt
			// die Herleitung zu wiederholen.
			return 1.f - slewExpNormalized(1.f - t, power);
		}
		return std::pow(t, power);
	}
	return t;
}

// Quantisierung: snappt eine Spannung auf das naechste 1V/Oct-Halbton-Raster
// (Eurorack-Konvention, 12 Halbtoene pro Volt). Wird angewendet, wenn
// MOD_QUANT_OCT fuer die aktuelle Stage gesetzt ist.
static const float QUANT_STEP = 1.f / 12.f;

inline float quantizeToSemitone(float v) {
	return std::round(v / QUANT_STEP) * QUANT_STEP;
}

// Scale/Key-Quantisierung (Manual "PRESETS AND QUANTIZATION SCALES"): statt
// stur auf jeden Halbton zu snappen, wird nur auf Toene innerhalb der
// gewaehlten Tonleiter+Grundton gerastert. scaleMode: 0=Major, 1=Minor
// (natuerlich Moll), 2=Chromatic (= reines Halbton-Raster, Key wirkungslos).
// key: 0=C .. 11=B, modulweit global (siehe Marf248::currentKey/currentScale).
static const int SCALE_MAJOR[7] = {0, 2, 4, 5, 7, 9, 11};
static const int SCALE_MINOR[7] = {0, 2, 3, 5, 7, 8, 10};

inline float quantizeToScale(float v, int key, int scaleMode) {
	if (scaleMode == 2) {
		// Chromatic: identisch zum bisherigen Verhalten, Key spielt keine
		// Rolle, da ohnehin jeder Halbton erlaubt ist.
		return quantizeToSemitone(v);
	}
	const int* scale = (scaleMode == 1) ? SCALE_MINOR : SCALE_MAJOR;
	float semitoneFloat = v * 12.f;
	int center = (int) std::round(semitoneFloat);
	float bestVal = 0.f;
	float bestDiff = 1e9f;
	// Grosszuegiges Suchfenster (+-12 Halbtoene um den naiv gerundeten
	// Wert): Tonleiter-Toene liegen nie weiter als ein paar Halbtoene
	// auseinander, daher garantiert dieses Fenster das tatsaechlich
	// naechstgelegene Scale-Mitglied zu finden, unabhaengig von Oktavlage.
	for (int s = center - 12; s <= center + 12; s++) {
		int pitchClass = ((s - key) % 12 + 12) % 12;
		bool inScale = false;
		for (int i = 0; i < 7; i++) {
			if (scale[i] == pitchClass) {
				inScale = true;
				break;
			}
		}
		if (inScale) {
			float diff = std::fabs((float) s - semitoneFloat);
			if (diff < bestDiff) {
				bestDiff = diff;
				bestVal = (float) s;
			}
		}
	}
	return bestVal / 12.f;
}

// Wendet den Range-Modifier (Full/Half/Limited) auf die rohe Slider-Spannung
// an. Reihenfolge im Signalpfad (siehe Manual "Voltage Programming
// Modifiers"): Range skaliert zuerst, danach greift ggf. Quantize auf das
// bereits skalierte Ergebnis.
//   Full    (Default): Slider unveraendert, 0-10V.
//   Half:    Slider auf 0-5V gestaucht (entspricht ~5 Oktaven Original-Buchla).
//   Limited: Slider auf eine 2V-Spanne gestaucht (2 Oktaven in Eurorack
//            1V/Okt-Konvention) und um den gewaehlten Oktav-Offset
//            (+0/+2/+4/+6/+8V) verschoben.
inline float applyRange(float raw, int mod) {
	if (mod & MOD_RANGE_LIMITED) {
		int idx = getRangeOffsetIndex(mod);
		float offset = RANGE_OFFSET_VOLTS[idx];
		return (raw / 10.f) * 2.f + offset;
	}
	else if (mod & MOD_RANGE_HALF) {
		return raw * 0.5f;
	}
	return raw;
}

// Wandelt eine 1V/Oct-Spannung in einen Notennamen um (Eurorack-Konvention:
// 0V = C4). Wird fuer die Anzeige des Voltage-Reglers benutzt, wenn Quantize
// fuer die jeweilige Stage aktiv ist.
inline std::string voltageToNoteName(float v) {
	static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	int semitoneOffset = (int) std::round(v * 12.f);
	int octave = 4 + (int) std::floor(semitoneOffset / 12.0);
	int noteIndex = ((semitoneOffset % 12) + 12) % 12;
	return std::string(names[noteIndex]) + std::to_string(octave);
}

// BPM-Feature (Time Multiplier V/Oct-Tempo-Semantik, Breaking Change ggue.
// der vorherigen linearen 0.5x-4.0x-Faktor-Semantik): TIME_MULT_A/B_PARAM
// speichert jetzt eine V/Oct-Spannung statt eines direkten Multiplikators.
// TEMPO_BASE_BPM ist das Tempo bei 0V (Buchla/Eurorack-Konvention: 1V = eine
// Oktave = Tempo-Verdopplung, analog zum 1V/Oct-Pitch-Standard). ACHTUNG
// Kinovice: bestehende Patches interpretieren den gespeicherten Rohwert
// dadurch komplett anders (alte 2.0 bedeutete 2.0x Zeitfaktor, jetzt 2 Volt
// -> 480 BPM statt vorher 240% Zeit) - Time-Multiplier-Knob in alten
// Patches neu einstellen.
static const float TEMPO_BASE_BPM = 120.f;

// Reine Sicherheitsklammer fuer die effektive Gesamtspannung (Knob +
// Attenuverter*CV), NICHT musikalisch gemeint, sondern Schutz vor absurden
// BPM-Werten/Rechenueberlauf bei Kabel-Fehlanschluss oder hoher CV-
// Amplitude (siehe Marf248::process()). Bei +-8V ergibt das ca.
// 0.47-30720 BPM - weit ausserhalb sinnvoller Nutzung, aber endlich.
static const float TEMPO_VOLT_SAFETY_CLAMP = 8.f;

inline float tempoVoltsToBpm(float volts) {
	return TEMPO_BASE_BPM * std::pow(2.f, volts);
}

// Pickup/Catch-Zustand fuer einen einzelnen kontinuierlichen Stage-Regler
// (Preset-Recall, siehe Manual "the sliders are only active once they have
// moved past the stored value"). Haelt den beim Preset-Load gespeicherten
// Zielwert getrennt vom physischen Reglerwert, bis der Regler ueber den
// Zielwert hinweg bewegt wurde (oder ihn exakt trifft). Wird pro Stage fuer
// Voltage, Time, Range und Curve je einmal instanziiert (siehe Marf248).
struct PickupState {
	bool pending = false;
	float target = 0.f;
	float lastRaw = 0.f;

	// Aktiviert Pickup fuer einen neuen Zielwert (Preset-Load); lastRaw wird
	// auf den aktuellen physischen Reglerwert gesetzt - das ist der
	// Referenzpunkt, gegen den der erste resolve()-Aufruf die Ueberkreuzung
	// prueft.
	void arm(float targetValue, float currentRaw) {
		target = targetValue;
		lastRaw = currentRaw;
		pending = true;
	}

	// Liefert jeden Frame den tatsaechlich zu verwendenden Wert: solange
	// Pickup aussteht, den gespeicherten Zielwert; danach den physischen
	// Reglerwert direkt. Erkennt die Ueberkreuzung durch Vorzeichenwechsel
	// (oder exaktes Treffen) der Differenz Reglerwert-Zielwert zwischen zwei
	// aufeinanderfolgenden Frames - deckt damit auch den Fall ab, dass der
	// Regler beim Load bereits zufaellig auf dem Zielwert steht (sofortiges
	// Pickup, kein "Steckenbleiben").
	float resolve(float currentRaw) {
		if (pending) {
			float diffPrev = lastRaw - target;
			float diffCur = currentRaw - target;
			if (diffPrev == 0.f || diffCur == 0.f || (diffPrev > 0.f) != (diffCur > 0.f)) {
				pending = false;
			}
		}
		lastRaw = currentRaw;
		return pending ? target : currentRaw;
	}
};

struct Afg {
	// Laufrichtung der Section wird jetzt durch zwei unabhaengige Schalter
	// bestimmt statt eines einzelnen 3-Wege-Zustands: reverseMode kehrt die
	// Basisrichtung um (Forward/Reverse), pendulumMode schaltet zusaetzlich
	// das Bouncing zwischen First und Last dazu. Der Vorteil: Pendulum kann
	// so in beide Anfangsrichtungen gestartet werden (reverseMode bestimmt,
	// mit welcher Schwingrichtung der Bounce beginnt), statt nur vorwaerts.
	bool reverseMode = false;
	bool pendulumMode = false;

	// Pendulum-Laufzustand: pendulumDir ist die aktuelle Bewegungsrichtung
	// innerhalb des Pendulum-Bounces (+1/-1), pendulumHeld merkt, ob der
	// aktuelle Wendepunkt (First oder Last) bereits einmal wiederholt wurde.
	// pendulumWasOn merkt den Pendulum-Zustand vom letzten Frame, um beim
	// Einschalten (aus->an) pendulumDir frisch aus reverseMode zu setzen -
	// das ist der Punkt, an dem sich die Start-Schwingrichtung entscheidet.
	// Siehe advanceStage() fuer die Herleitung, warum beide Enden je einmal
	// wiederholt werden muessen (Periode 2xM statt 2x(M-1), damit Pendulum
	// mit einer normal vorwaerts laufenden Section synchron bleibt).
	//
	// Session-Historie (August 2026): der Halt wurde zwischenzeitlich
	// versuchsweise entfernt (echte Spiegelung ohne Wiederholung), um einen
	// vermeintlichen "Versatz von einem Step" beim Treffen mit einer
	// Forward-Section zu beheben. Ergebnis war schlechter: ohne Halt betraegt
	// die Pendulum-Periode 2x(M-1) - bei M=16 also 30 statt 32 Ticks - und
	// das ist KEIN sauberes Vielfaches der Forward-Periode M (16) mehr.
	// Forward und Pendulum liefen dadurch komplett auseinander (kleinstes
	// gemeinsames Vielfaches von 16 und 30 ist 240 Ticks - praktisch nie).
	// Mit Halt (2xM = 32, sauberes Vielfaches von 16) treffen sich beide
	// Enden dagegen zuverlaessig alle 2 Forward-Umlaeufe. Daher zurueckgebaut
	// auf den urspruenglichen Stand mit Halt.
	int pendulumDir = 1;
	bool pendulumHeld = false;
	bool pendulumWasOn = false;

	int currentStage = 0;
	bool running = false;
	// Effektiver Lauf-Zustand dieses Frames: running (Taster/Start-Stop-
	// Puls) ODER-verknuepft mit einem high anliegenden Start/Stop-Gate vom
	// Trigger-Expander (RUN_GATE_A/B_INPUT). Bewusst getrennt von running
	// gehalten (statt running direkt zu ueberschreiben), da running den
	// Taster-Zustand dauerhaft speichert - faellt das Gate wieder auf Low,
	// soll wieder genau der zuletzt vom Taster eingestellte Zustand gelten,
	// ohne dass der High-Moment des Gates das persistente running
	// veraendert hat. Wird von Marf248::process() fuer RUN-Light/Status
	// anstelle von running ausgelesen (siehe dort).
	bool effectiveRunning = false;
	// Merkt sich, ob applyStageAddress() diesen Frame im Continuous-Modus
	// lief (siehe dortigen Kommentar). Wird in process() verwendet, um a)
	// den Timer-Auto-Advance waehrend Continuous zu unterdruecken (sonst
	// koennte ein extern gehaltenes Trigger-Expander RUN_GATE die Stage
	// zusaetzlich zur Adressierung weiterschalten - Doppelsteuerung) und
	// b) trotzdem eine Slope-Rampe zum adressierten Zielwert laufen zu
	// lassen (siehe MARF-Handbuch: "slope time is equal to the programmed
	// interval time" - gilt laut Dokumentation auch bei externer/Continuous-
	// Adressierung, nicht nur im Timer-Betrieb).
	bool continuousActive = false;
	// Clock-quantisiertes Advance: true, solange ADVANCE_A/B_INPUT gepatcht
	// ist (siehe process()). Als Member statt lokaler Variable gehalten,
	// damit Marf248::process() ihn nach dem process()-Aufruf fuer die
	// dreifarbige RUN/STOP-Status-LED auslesen kann - sonst wuerde die
	// Section faelschlich als "gestoppt" (rot) angezeigt, obwohl sie aktiv
	// auf Trigger reagiert, da effectiveRunning selbst nichts von diesem
	// Modus weiss (siehe Marf248.cpp, stopA/stopB-Berechnung).
	bool advanceModeActive = false;
	float elapsed = 0.f;

	// Slew-Zustand: voltageOut ist der zuletzt ausgegebene Wert; voltageFrom
	// ist der Startwert der aktuellen Stage (Ausgangspunkt der Rampe, falls
	// MOD_SLOPED gesetzt ist). Ohne MOD_SLOPED springt voltageOut sofort auf
	// den Zielwert (Stepped-Verhalten, wie bisher).
	float voltageFrom = 0.f;
	float voltageOut = 0.f;

	dsp::SchmittTrigger startTrigger;
	dsp::SchmittTrigger stopTrigger;
	dsp::SchmittTrigger runButtonTrigger;
	// Pegel-Erkennung (kein Edge-Trigger) fuer das Start/Stop-Gate vom
	// Trigger-Expander: .process() wird jeden Frame aufgerufen, aber nur
	// .isHigh() ausgewertet - das liefert den aktuellen Gate-Zustand
	// inklusive 0.1V/1V-Schmitt-Hysterese gegen Jitter am Umschaltpunkt
	// (siehe Afg::process()).
	dsp::SchmittTrigger runGateTrigger;
	// Clock-quantisiertes Advance (Trigger-Expander ADVANCE_A/B_INPUT):
	// echte Flanken-Erkennung (kein Pegel wie runGateTrigger), analog zu
	// startTrigger/stopTrigger - .process() wird nur aufgerufen, wenn die
	// Buchse gepatcht ist (siehe Afg::process()), damit ein unverbundener
	// Eingang den internen Zustand nicht beeinflusst.
	dsp::SchmittTrigger advanceTrigger;
	dsp::PulseGenerator allPulseGen;
	// Programmed Pulse Outputs P1/P2: eigene Pulsgeneratoren, unabhaengig von
	// allPulseGen (das bei JEDER neuen Stage feuert). Diese hier feuern nur,
	// wenn die neu erreichte Stage das jeweilige MOD_PULSE1/2-Bit gesetzt hat.
	dsp::PulseGenerator p1Gen;
	dsp::PulseGenerator p2Gen;
	// Start-/End-of-Cycle: feuert, wenn die neu erreichte Stage MOD_FIRST
	// bzw. MOD_LAST gesetzt hat - unabhaengig von Laufrichtung (Forward/
	// Reverse/Pendulum) und ohne Entprellung bei Pendulum-Wendepunkten
	// (dort wird die Endstage bewusst zweimal angefahren, siehe
	// advanceStage() - beide Landungen loesen hier je einen Puls aus,
	// analog zum bestehenden APOUT-Verhalten). Nur relevant fuer den
	// optionalen Trigger-Expander (Marf248TriggerExpander), kein eigener
	// Jack auf dem Hauptpanel.
	dsp::PulseGenerator startCycleGen;
	dsp::PulseGenerator endCycleGen;

	// Retrigger-Luecke fuer den CV-Expander (Marf248CvExpander, Gate A/B):
	// feuert bei JEDEM Stage-Eintritt (auch ohne Indexwechsel, siehe
	// advanceStage()/jumpToStage()) sowie zusaetzlich, wenn eine Sustain/
	// Enable-Haltung endet (sustainEnableWasHeld-Uebergang true->false in
	// process()) - dort findet kein advanceStage()/jumpToStage()-Aufruf
	// statt, da currentStage unveraendert bleibt, die Stage aus Sicht eines
	// nachgeschalteten Envelope-Generators aber trotzdem neu "startet".
	// Rein optionale Zusatzinformation fuer den CV-Expander, kein eigener
	// Jack auf dem Hauptpanel.
	dsp::PulseGenerator regateGen;
	bool sustainEnableWasHeld = false;

	// Vorwerte fuer die kontinuierliche Flankenerkennung von P1/P2/First/Last
	// unten in process() - siehe dortigen Kommentar fuer die vollstaendige
	// Herleitung (Design-Diskussion: externes CV-Gate fuer PULSE1/2 via
	// Trigger-Expander koennte den exakten Eintritts-Sample verpassen, wenn
	// es "nur" beim Eintritt geprueft wird).
	bool pulse1PrevActive = false;
	bool pulse2PrevActive = false;
	bool firstPrevActive = false;
	bool lastPrevActive = false;

	// Setzt die Flanken-Vorwerte fuer P1/P2/First/Last zurueck auf false -
	// wird von advanceStage() und jumpToStage() nach dem Setzen von
	// currentStage aufgerufen (gleiche Stelle wie allPulseGen), IMMER bei
	// jedem (Wieder-)Eintritt, auch ohne Indexwechsel (Strobe-Wiederholung,
	// Pendulum-Wendepunkt) - analog zu regateGen.trigger() oben, damit ein
	// wiederholter manueller Sprung auf dieselbe Stage weiterhin jedesmal
	// feuert (bisheriges Verhalten). Das eigentliche Feuern der
	// Pulsgeneratoren passiert NICHT mehr hier, sondern kontinuierlich in
	// process() - siehe dortigen Kommentar: ein bereits beim Eintritt
	// gesetztes Bit zaehlt dank dieses Resets weiterhin als steigende
	// Flanke, ein erst waehrend des Stage-Aufenthalts eintreffendes externes
	// CV-Gate (Trigger-Expander STAGE_..._POLY_INPUT) wird zusaetzlich
	// erfasst, statt verpasst zu werden.
	void resetPulseEdgeState() {
		pulse1PrevActive = false;
		pulse2PrevActive = false;
		firstPrevActive = false;
		lastPrevActive = false;
	}

	// Status-Flags fuer die dreifarbige Stage-Address-LED (gruen/gelb/rot),
	// werden am Ende von process() gesetzt und von Marf248::process() zur
	// Anzeige ausgelesen. statusHeld entspricht "Gelb - Hold" (Sustain/Enable
	// wartet auf Start-Gate), statusStoppedByStage "Rot - Stop durch
	// programmierte Stop-Stage" (running bleibt intern true, die Stage
	// wartet aber auf einen Start-Puls - siehe Manual "Programmed Stop
	// stage"). Der dritte Rot-Grund (Stop-Taster/-Puls oder Continuous-
	// Adressierung) ergibt sich bereits aus running == false und braucht
	// kein eigenes Flag.
	bool statusHeld = false;
	bool statusStoppedByStage = false;

	// Sprung auf eine konkrete Stage, ausserhalb des normalen Zeitablaufs
	// (Reset/Strobe/Continuous-Adressierung). Startpunkt der Rampe ist wie
	// bei advanceStage() der zuletzt ausgegebene Wert, damit ein Slew nicht
	// abrupt abreisst.
	void jumpToStage(int stage, const int stageModifier[NUM_STAGES]) {
		int prevStage = currentStage;
		voltageFrom = voltageOut;
		currentStage = rack::clamp(stage, 0, NUM_STAGES - 1);
		elapsed = 0.f;
		// MOD_MUTE unterdrueckt jetzt auch APOUT (Kinovice-Entscheidung:
		// sonst muesste man P1 UND P2 einzeln deaktivieren, um eine Stage
		// wirklich komplett stumm zu bekommen) - allPulseGen feuert sonst
		// bei JEDER Stage, unabhaengig von MOD_PULSE1/2.
		if (!(stageModifier[currentStage] & MOD_MUTE)) {
			allPulseGen.trigger(1e-3f);
		}
		// Strobe erneut auf dieselbe Adresse) - bei einem echten Sprung auf
		// eine andere Stage ist die Gate-Adressierung ohnehin schon high auf
		// dem neuen Index, keine Luecke noetig (siehe Spec-Praezisierung).
		if (currentStage == prevStage) {
			regateGen.trigger(REGATE_GAP_SECONDS);
		}
		resetPulseEdgeState();
	}

	// Stage-Address-Feature: wird von Marf248::process() VOR dem eigentlichen
	// process()-Aufruf aufgerufen, da Continuous running=false setzen muss,
	// damit die normale Zeitablauf-Logik in process() in diesem Frame
	// pausiert (siehe Kommentar dort).
	//   Reset: springt auf die erste MOD_FIRST-Stage (Fallback Stage 1),
	//          unabhaengig von Continuous/Strobe - reine Sofort-Aktion.
	//   Continuous: haelt currentStage jeden Frame synchron zur Adresse
	//          (Knob oder externe CV) und erzwingt sofortige volle Auslenkung
	//          (kein Slew), da dies als adressierbarer Speicher gedacht ist
	//          (Manual/MARF-Book: "sweeps through stages ... stops the
	//          internal clock").
	//   Strobe: einmaliger Sprung auf die aktuelle Adresse, danach laeuft
	//          der normale Zeitablauf/Slew wieder wie gewohnt weiter.
	void applyStageAddress(bool continuous, int addr, bool strobePressed, bool resetPressed,
	                        const int stageModifier[NUM_STAGES]) {
		// Fuer den Auto-Advance-Guard und die Glide-Berechnung in process()
		// IMMER aktuell halten, unabhaengig davon, welcher Zweig unten
		// tatsaechlich greift (auch wenn z.B. resetPressed vorrangig behandelt
		// wird) - siehe Kommentar bei der Member-Deklaration oben.
		continuousActive = continuous;

		if (resetPressed) {
			int first = findLowestFirst(stageModifier);
			jumpToStage(first, stageModifier);
			return;
		}
		if (continuous) {
			if (addr != currentStage) {
				// jumpToStage() setzt voltageFrom = voltageOut und elapsed = 0 -
				// exakt der Ausgangspunkt fuer eine normale Rampe. Frueher wurde
				// elapsed hier stattdessen sofort auf "fertig" gezwungen
				// (elapsed = stageTimeHint/1e6f), wodurch Slope in Continuous
				// wirkungslos war (Phase startete bereits bei 1.0) - siehe
				// Session-Diskussion: das widerspricht sowohl dem MARF-Handbuch
				// ("slope time is equal to the programmed interval time") als
				// auch dem String-Patch aus dem Cookbook, der Continuous+Slope
				// bewusst fuer Glide-Verhalten kombiniert. Der eigentliche
				// Rampen-Fortschritt (elapsed += sampleTime) passiert jetzt
				// kontinuierlich in process(), siehe dortigen Kommentar.
				jumpToStage(addr, stageModifier);
			}
			// Start/Stop-Konzept ist im Continuous-Modus nicht anwendbar (siehe
			// Session-Diskussion: die Stage wird direkt adressiert, nicht per
			// Timer durchlaufen) - running bleibt daher bewusst unwirksam.
			running = false;
			return;
		}
		if (strobePressed) {
			jumpToStage(addr, stageModifier);
		}
	}

	// Fuer Reset gedacht (siehe applyStageAddress()): findet die MOD_FIRST-
	// Stage mit dem NIEDRIGSTEN Index, unabhaengig von currentStage - anders
	// als findFirstBefore() (das die naechstgelegene First-Stage in einer
	// bestimmten Suchrichtung liefert, gedacht fuer den Auto-Advance-Wrap).
	// Bei nur einer First-Stage liefern beide Funktionen dasselbe Ergebnis;
	// bei mehreren First-Bits (z.B. mehrere First/Last-Segmente, siehe
	// Projektnotizen) soll Reset aber IMMER zum allerersten Segment
	// springen, nicht zum naechstgelegenen von Stage 16 aus gesehen -
	// findFirstBefore(NUM_STAGES-1) wuerde stattdessen die hoechstindizierte
	// First-Stage finden (Rueckwaertssuche ab Index 15). Fallback: Stage 0,
	// falls gar keine Stage MOD_FIRST traegt.
	int findLowestFirst(const int stageModifier[NUM_STAGES]) const {
		for (int i = 0; i < NUM_STAGES; i++) {
			if (stageModifier[i] & MOD_FIRST)
				return i;
		}
		return 0;
	}

	int findFirstBefore(const int stageModifier[NUM_STAGES], int idx) const {
		for (int i = idx; i >= 0; i--) {
			if (stageModifier[i] & MOD_FIRST)
				return i;
		}
		for (int i = NUM_STAGES - 1; i > idx; i--) {
			if (stageModifier[i] & MOD_FIRST)
				return i;
		}
		return 0;
	}

	// Spiegelbild von findFirstBefore, fuer den Backward-Wrap: sucht die
	// naechste MOD_LAST-markierte Stage vorwaerts ab idx (mit Wraparound),
	// analog zur Ring-Suchlogik oben.
	int findLastAfter(const int stageModifier[NUM_STAGES], int idx) const {
		for (int i = idx; i < NUM_STAGES; i++) {
			if (stageModifier[i] & MOD_LAST)
				return i;
		}
		for (int i = 0; i < idx; i++) {
			if (stageModifier[i] & MOD_LAST)
				return i;
		}
		return NUM_STAGES - 1;
	}

	void advanceStage(const int stageModifier[NUM_STAGES]) {
		int next;
		bool wasFirst = stageModifier[currentStage] & MOD_FIRST;
		bool wasLast  = stageModifier[currentStage] & MOD_LAST;

		// Beim Einschalten von Pendulum (aus->an) die Schwingrichtung frisch
		// aus reverseMode setzen: Forward -> Bounce startet Richtung Last,
		// Reverse -> Bounce startet Richtung First. Laeuft Pendulum schon,
		// aendert ein Umschalten von reverseMode NICHTS an der aktuellen
		// Schwingrichtung (erst der naechste Aus/An-Zyklus greift das neu
		// auf) - vermeidet einen abrupten Sprung mitten im Bounce.
		if (pendulumMode && !pendulumWasOn) {
			pendulumDir = reverseMode ? -1 : 1;
			pendulumHeld = false;
		}
		pendulumWasOn = pendulumMode;

		if (pendulumMode) {
			// Nimmt an, dass First..Last ein zusammenhaengender, nicht
			// wrapender Bereich ist (einfache +-1-Schrittweite, kein
			// Ring-Wraparound wie bei Forward/Reverse ohne Pendulum). Fuer
			// den ueblichen Anwendungsfall (First < Last, ein Block)
			// ausreichend; siehe Projektnotizen.
			bool atTurnPoint = (pendulumDir > 0 && wasLast) || (pendulumDir < 0 && wasFirst);
			if (atTurnPoint) {
				if (!pendulumHeld) {
					// Wendepunkt zum ersten Mal erreicht: einmal auf der
					// gleichen Stage halten (Endpunkt-Wiederholung), damit
					// die Gesamtperiode 2xM betraegt statt 2x(M-1).
					next = currentStage;
					pendulumHeld = true;
				}
				else {
					// Wendepunkt wurde bereits einmal wiederholt: jetzt
					// tatsaechlich umkehren.
					pendulumDir = -pendulumDir;
					pendulumHeld = false;
					next = currentStage + pendulumDir;
				}
			}
			else {
				next = currentStage + pendulumDir;
			}
			next = rack::clamp(next, 0, NUM_STAGES - 1);
		}
		else if (reverseMode) {
			// Reverse ohne Pendulum: Spiegelung von Forward, dekrementiert
			// statt inkrementiert, wrapt bei First (statt Last) zurueck auf
			// die naechste Last-markierte Stage (statt First).
			next = currentStage - 1;
			if (next < 0 || wasFirst) {
				next = findLastAfter(stageModifier, currentStage);
			}
		}
		else {
			// Forward ohne Pendulum (Default, bisheriges Verhalten
			// unveraendert).
			next = currentStage + 1;
			if (next >= NUM_STAGES || wasLast) {
				next = findFirstBefore(stageModifier, currentStage);
			}
		}

		// Skip: ueberspringt Stages mit MOD_SKIP, indem dieselbe Vorwaerts-/
		// Rueckwaerts-/Pendulum-Schrittlogik wiederholt angewendet wird, bis
		// eine nicht uebersprungene Stage gefunden ist. Auf NUM_STAGES
		// Versuche begrenzt als Sicherheitsnetz, falls (versehentlich) alle
		// Stages als Skip markiert sind - dann bleibt next einfach auf der
		// zuletzt berechneten Stage stehen, statt sich aufzuhaengen. Wirkt
		// nur beim automatischen Zeitablauf, nicht bei manueller
		// Adressierung (Reset/Strobe/Continuous - siehe applyStageAddress()).
		for (int guard = 0; guard < NUM_STAGES && (stageModifier[next] & MOD_SKIP); guard++) {
			bool nextIsFirst = stageModifier[next] & MOD_FIRST;
			bool nextIsLast  = stageModifier[next] & MOD_LAST;
			if (pendulumMode) {
				bool atTurn = (pendulumDir > 0 && nextIsLast) || (pendulumDir < 0 && nextIsFirst);
				if (atTurn) {
					pendulumDir = -pendulumDir;
				}
				next = rack::clamp(next + pendulumDir, 0, NUM_STAGES - 1);
			}
			else if (reverseMode) {
				int cand = next - 1;
				next = (cand < 0 || nextIsFirst) ? findLastAfter(stageModifier, next) : cand;
			}
			else {
				int cand = next + 1;
				next = (cand >= NUM_STAGES || nextIsLast) ? findFirstBefore(stageModifier, next) : cand;
			}
		}

		// Rampen-Startpunkt der neuen Stage ist der zuletzt ausgegebene Wert,
		// damit der Slew nahtlos an die vorherige Stage anschliesst.
		voltageFrom = voltageOut;
		bool sameStageReentry = (next == currentStage);
		currentStage = next;
		elapsed = 0.f;
		// MOD_MUTE unterdrueckt jetzt auch APOUT, siehe identischer Kommentar
		// in jumpToStage() oben.
		if (!(stageModifier[currentStage] & MOD_MUTE)) {
			allPulseGen.trigger(1e-3f);
		}
		// Wendepunkt-Wiederholung, next==currentStage von oben) - bei einem
		// normalen Schritt auf eine andere Stage braucht es keine Luecke,
		// die Gate-Adressierung wechselt dort ohnehin den Kanal (siehe
		// Spec-Praezisierung, war vorher faelschlich unconditional).
		if (sameStageReentry) {
			regateGen.trigger(REGATE_GAP_SECONDS);
		}
		resetPulseEdgeState();
	}

	void process(float sampleTime,
	             const float stageVoltage[NUM_STAGES],
	             const float stageTime[NUM_STAGES],
	             const int stageModifier[NUM_STAGES],
	             const float stageCurvePower[NUM_STAGES],
	             int quantKey, int quantScaleMode,
	             float runButtonValue, bool startConnected, float startVoltage,
	             bool stopConnected, float stopVoltage,
	             bool runGateConnected, float runGateVoltage,
	             bool advanceConnected, float advanceVoltage,
	             float* outVoltage, float* outTime, float* outAllPulse,
	             float* outP1, float* outP2,
	             float* outStartCycle, float* outEndCycle, float* outRegateGap) {
		// Button togglet Start/Stop (daher der Name "Start/Stop"-Button).
		// CV-Inputs bleiben reine Start- bzw. Stop-Pulse, wie im Original.
		if (runButtonTrigger.process(runButtonValue)) {
			running = !running;
		}
		if (startConnected && startTrigger.process(startVoltage)) {
			running = true;
		}
		if (stopConnected && stopTrigger.process(stopVoltage)) {
			running = false;
		}

		// Start/Stop-Gate (Trigger-Expander, RUN_GATE_A/B_INPUT): reine
		// Pegel-Logik statt Trigger-Flanke, mit 0.1V/1V-Schmitt-Hysterese
		// gegen Gate-Jitter am Umschaltpunkt. ODER-verknuepft mit running
		// (Taster/Start-Stop-Puls) statt es zu ueberschreiben, siehe
		// effectiveRunning-Kommentar oben. .process() wird auch bei nicht
		// verbundenem Eingang mit 0V aufgerufen, damit der interne
		// Hysterese-Zustand sauber auf Low faellt (statt "haengen" zu
		// bleiben), falls das Kabel waehrend eines High-Gates gezogen wird.
		runGateTrigger.process(runGateConnected ? runGateVoltage : 0.f, 0.1f, 1.f);
		bool runGateHigh = runGateConnected && runGateTrigger.isHigh();
		effectiveRunning = running || runGateHigh;

		// Clock-quantisiertes Advance (Trigger-Expander ADVANCE_A/B_INPUT):
		// sobald gepatcht, uebernimmt advanceModeActive komplett die Kontrolle
		// ueber den Stage-Wechsel dieser Section, UNABHAENGIG von running/
		// effectiveRunning (Start/Stop-Taster und RUN_GATE werden bewusst
		// ignoriert, solange das Kabel steckt - Kinovice-Entscheidung,
		// analog zu continuousActive weiter unten). .process() wird nur bei
		// gepatchter Buchse aufgerufen (wie startTrigger/stopTrigger), damit
		// ein Kabelziehen den Flanken-Zustand nicht verfaelscht.
		advanceModeActive = advanceConnected;
		bool advanceTriggered = advanceConnected && advanceTrigger.process(advanceVoltage);

		bool stageIsStop = stageModifier[currentStage] & MOD_STOP;
		bool stageIsSustain = stageModifier[currentStage] & MOD_SUSTAIN;
		bool stageIsEnable = stageModifier[currentStage] & MOD_ENABLE;
		bool stageIsSkip = stageModifier[currentStage] & MOD_SKIP;

		// Gate-Schwelle: deckt sowohl den kurzen HIGH-Trigger (10V) als auch
		// den HOLD-Zustand (5V) der Buchla-Pulskonvention ab.
		const float GATE_THRESHOLD = 2.f;
		// WICHTIG - haeufige Stolperfalle beim Patchen/Testen: startVoltage
		// ist die rohe Spannung am START_A/B_INPUT, OHNE Ruecksicht auf
		// startConnected. Ein unverbundener Eingang liefert in Rack 0V
		// zurueck (getVoltage() auf einem leeren Port), was hier exakt wie
		// ein dauerhaft LOW-Gate behandelt wird - NICHT wie "Gate-Konzept
		// ausgeschaltet" oder "neutral". Das hat fuer Sustain und Enable
		// GEGENSAETZLICHE, beide auf den ersten Blick ueberraschende
		// Konsequenzen (von Kinovice beim Live-Test bestaetigt/nachgefragt):
		//   - Sustain OHNE Kabel: haelt NIE (startGateHigh ist immer false)
		//     -> die Stage laeuft scheinbar "wie eine normale Stage" durch,
		//     das Sustain-Bit hat also ohne gepatchtes Gate gar keine
		//     sichtbare Wirkung.
		//   - Enable OHNE Kabel: haelt IMMER (startGateHigh ist immer
		//     false, !startGateHigh also immer true) -> die Stage bleibt
		//     STEHEN, fuer immer, auch mit beliebig vielen Advance-Triggern
		//     oder abgelaufener Stage-Zeit - sieht wie ein haengengebliebe-
		//     nes/kaputtes Modul aus, ist aber exakt die spezifizierte
		//     "wartet auf Freigabe per Gate"-Semantik (Manual: Enable gibt
		//     die Stage erst frei, wenn ein Gate high geht - ohne Gate gibt
		//     es also nie eine Freigabe).
		// Sind BEIDE Bits gleichzeitig auf derselben Stage gesetzt (Sustain
		// UND Enable), gewinnt ohne Gate effektiv Enable (haelt), da beide
		// Bedingungen unabhaengig ausgewertet und per OR in statusHeld
		// zusammengefuehrt werden (siehe unten) - Sustain traegt in diesem
		// Fall nichts zum Halten bei, verhindert das Halten aber auch nicht.
		// Zum Testen/Verifizieren: ein Kabel (z.B. von einem manuellen
		// Trigger-/Gate-Taster-Modul) an START_A/B patchen und dessen Gate
		// hoch/runter schalten - erst dann zeigt sich das jeweils andere,
		// "freigebende" Verhalten (Sustain haelt dann WAEHREND High, Enable
		// haelt dann WAEHREND Low und gibt bei High frei).
		bool startGateHigh = startVoltage >= GATE_THRESHOLD;

		// Sustain: haelt die Stage, solange am Start-Eingang ein High-Pegel
		// (Gate) anliegt; sobald das Gate auf Low geht, laeuft die Stage
		// normal weiter (Release-Verhalten wie bei einer ADSR). Siehe
		// Kommentar bei GATE_THRESHOLD oben fuer das Verhalten ohne Kabel.
		bool heldBySustain = stageIsSustain && startGateHigh;
		// Enable: haelt die Stage, solange KEIN High-Pegel am Start-Eingang
		// anliegt; erst ein Gate oberhalb der Schwelle gibt die Stage frei.
		// Siehe Kommentar bei GATE_THRESHOLD oben - OHNE gepatchtes Kabel
		// haelt dies die Stage auf UNBESTIMMTE ZEIT (kein Bug).
		bool heldByEnable = stageIsEnable && !startGateHigh;

		statusHeld = heldBySustain || heldByEnable;
		statusStoppedByStage = stageIsStop;

		// Sustain/Enable-Freigabe: kein advanceStage()/jumpToStage()-Aufruf
		// (currentStage bleibt gleich), aber aus Sicht eines nachgeschalteten
		// Envelope-Generators "startet" die Stage hier trotzdem neu - siehe
		// regateGen-Kommentar oben.
		if (!statusHeld && sustainEnableWasHeld) {
			regateGen.trigger(REGATE_GAP_SECONDS);
		}
		sustainEnableWasHeld = statusHeld;

		if (effectiveRunning && !continuousActive && !advanceModeActive && !stageIsStop && !heldBySustain && !heldByEnable) {
			if (stageIsSkip) {
				// Skip wirkt sofort, nicht erst nach Ablauf der Stage-Zeit -
				// falls z.B. Reset/Strobe auf eine Skip-Stage gelandet ist,
				// oder mehrere Skip-Stages am Stueck vorliegen (advanceStage()
				// loest bereits mehrfach hintereinander liegende Skips aus,
				// dieser Aufruf faengt nur den Fall ab, dass die AKTUELLE
				// Stage selbst noch als Skip markiert ist).
				advanceStage(stageModifier);
			}
			else {
				elapsed += sampleTime;
				if (elapsed >= stageTime[currentStage]) {
					advanceStage(stageModifier);
				}
			}
		}

		// Clock-quantisiertes Advance: eigener Zweig statt Erweiterung des
		// obigen Timer-Zweigs, da advanceModeActive bewusst UNABHAENGIG von
		// effectiveRunning wirkt (siehe Kommentar bei advanceModeActive
		// oben) - der Timer-Zweig oben bleibt dagegen an effectiveRunning
		// gebunden. Mutual exclusion zu continuousActive wie beim Timer-
		// Zweig: Continuous adressiert die Stage ohnehin direkt, ein
		// Advance-Trigger waere dort bedeutungslos. STOP/SUSTAIN/ENABLE
		// haben weiterhin Vorrang (siehe Trigger-Expander-Header-Kommentar):
		// ein Trigger, der waehrend einer Haltung eintrifft, wird schlicht
		// nicht ausgewertet (advanceTriggered bleibt fuer diesen Sample
		// ungenutzt, keine Warteschlange/kein Nachholen nach Haltungsende).
		if (advanceModeActive && !continuousActive && !stageIsStop && !heldBySustain && !heldByEnable) {
			if (stageIsSkip) {
				// Skip bleibt sofort wirksam, unabhaengig vom Advance-Modus -
				// gleiche Begruendung wie im Timer-Zweig oben.
				advanceStage(stageModifier);
			}
			else {
				// elapsed laeuft weiter (gecappt auf stageTime, analog zum
				// continuousActive-Zweig unten), damit Slope/TOUT so
				// aussehen, als waere die Stage-Zeit reguere abgelaufen -
				// die Stage verharrt am Zielwert/bei TOUT=0, bis der
				// naechste Trigger kommt. Der eigentliche Stage-Wechsel
				// haengt NICHT an diesem Wert, sondern ausschliesslich am
				// Trigger: kommt er VOR Ablauf der Stage-Zeit, wird sofort
				// gewechselt (Trigger hat immer Vorrang vor der Zeit).
				elapsed = rack::clamp(elapsed + sampleTime, 0.f, stageTime[currentStage]);
				if (advanceTriggered) {
					advanceStage(stageModifier);
				}
			}
		}

		// Continuous-Modus: kein Auto-Advance (siehe !continuousActive-Guard
		// oben - sonst koennte ein extern gehaltenes Trigger-Expander RUN_GATE
		// die Stage zusaetzlich zur Adressierung weiterschalten), aber elapsed
		// laeuft trotzdem hoch, damit eine Slope-Rampe zum adressierten
		// Zielwert fortschreiten kann (siehe applyStageAddress()-Kommentar und
		// MARF-Handbuch: "slope time is equal to the programmed interval
		// time" - gilt auch bei externer/Continuous-Adressierung). Bewusst
		// OHNE Ruecksicht auf stageIsStop/heldBySustain/heldByEnable: diese
		// Timer-Pausenkonzepte sind im Continuous-Betrieb nicht anwendbar,
		// analog dazu, dass Continuous auch Skip bewusst ignoriert (siehe
		// MOD_SKIP-Kommentar oben). Bei elapsed >= stageTime bleibt die Rampe
		// einfach am Zielwert stehen (rack::clamp in der Phase-Berechnung
		// unten), kein Ueberschwingen.
		if (continuousActive) {
			float tContinuous = stageTime[currentStage];
			if (tContinuous > 0.f) {
				elapsed = rack::clamp(elapsed + sampleTime, 0.f, tContinuous);
			}
		}

		// P1/P2 sowie Start-/End-of-Cycle-Trigger (First/Last): kontinuierliche
		// Flankenerkennung statt Momentaufnahme beim Stage-Eintritt. Vorher
		// wertete triggerStagePulses() die Bits nur EINMAL exakt im
		// Eintritts-Sample aus - ein extern per CV gesetztes Bit (Trigger-
		// Expander STAGE_..._POLY_INPUT), das erst danach high wird, wurde
		// dadurch verpasst und feuerte gar nicht mehr, bis zum naechsten
		// Stage-Wechsel. resetPulseEdgeState() (aufgerufen von advanceStage()/
		// jumpToStage(), IMMER bei jedem (Wieder-)Eintritt, auch ohne
		// Indexwechsel) setzt die Vorwerte unten auf false zurueck, sodass:
		// - ein beim Eintritt bereits gesetztes Bit weiterhin als steigende
		//   Flanke zaehlt (bisheriges Verhalten unveraendert),
		// - ein erst waehrend des Stage-Aufenthalts eintreffendes CV-Gate
		//   zusaetzlich erkannt wird, sobald es high wird,
		// - bei mehrfachem Gate-Wechsel waehrend EINES Stage-Aufenthalts
		//   (z.B. externer Clock) JEDE steigende Flanke erneut feuert (freier
		//   Clock-Betrieb statt "nur einmal pro Besuch" - Kinovice-Entscheidung).
		// MOD_MUTE unterdrueckt nur die Flankenerkennung von P1/P2 (das
		// Bit selbst bleibt in stageModifier[] unangetastet) - First/Last
		// bleiben von Mute bewusst unberuehrt, da sie Zyklus-Marker sind,
		// keine "Output Pulses" im Sinne des Manuals.
		bool muteActive = stageModifier[currentStage] & MOD_MUTE;
		bool pulse1Active = !muteActive && (stageModifier[currentStage] & MOD_PULSE1);
		bool pulse2Active = !muteActive && (stageModifier[currentStage] & MOD_PULSE2);
		bool firstActive  = stageModifier[currentStage] & MOD_FIRST;
		bool lastActive   = stageModifier[currentStage] & MOD_LAST;
		if (pulse1Active && !pulse1PrevActive) {
			p1Gen.trigger(1e-3f);
		}
		if (pulse2Active && !pulse2PrevActive) {
			p2Gen.trigger(1e-3f);
		}
		if (firstActive && !firstPrevActive) {
			startCycleGen.trigger(1e-3f);
		}
		if (lastActive && !lastPrevActive) {
			endCycleGen.trigger(1e-3f);
		}
		pulse1PrevActive = pulse1Active;
		pulse2PrevActive = pulse2Active;
		firstPrevActive = firstActive;
		lastPrevActive = lastActive;

		float t = stageTime[currentStage];
		float phase = t > 0.f ? rack::clamp(elapsed / t, 0.f, 1.f) : 0.f;

		// Range skaliert/verschiebt zuerst die rohe Slider-Spannung
		// (Full/Half/Limited), erst danach greift ggf. Quantize auf das
		// bereits skalierte Ergebnis.
		float target = applyRange(stageVoltage[currentStage], stageModifier[currentStage]);
		if (stageModifier[currentStage] & MOD_QUANT_OCT) {
			// Quantize sampelt/snappt den Zielwert auf das globale Scale/Key-
			// Raster (Default Chromatic + C entspricht dem bisherigen reinen
			// Halbton-Raster); bei aktivem Slope laeuft die Rampe weiterhin
			// sanft, aber zum quantisierten statt zum rohen Slider-Wert.
			target = quantizeToScale(target, quantKey, quantScaleMode);
		}
		if (stageModifier[currentStage] & MOD_SLOPED) {
			float shaped = applySlewCurve(phase, stageModifier[currentStage], stageCurvePower[currentStage]);
			voltageOut = voltageFrom + (target - voltageFrom) * shaped;
			if (stageModifier[currentStage] & MOD_QUANT_GLIDE) {
				// Quantized Glide: snappt jeden Sample-Wert waehrend des
				// Slews (nicht nur den bereits oben ggf. quantisierten
				// target) auf das Scale/Key-Raster - siehe MOD_QUANT_GLIDE-
				// Kommentar im Enum weiter oben.
				voltageOut = quantizeToScale(voltageOut, quantKey, quantScaleMode);
			}
		}
		else {
			voltageOut = target;
		}
		*outVoltage = voltageOut;

		*outTime = (1.f - phase) * 10.f;

		*outAllPulse = allPulseGen.process(sampleTime) ? 10.f : 0.f;
		*outP1 = p1Gen.process(sampleTime) ? 10.f : 0.f;
		*outP2 = p2Gen.process(sampleTime) ? 10.f : 0.f;
		*outStartCycle = startCycleGen.process(sampleTime) ? 10.f : 0.f;
		*outEndCycle = endCycleGen.process(sampleTime) ? 10.f : 0.f;
		*outRegateGap = regateGen.process(sampleTime) ? 1.f : 0.f;
	}
};
