#include "Marf248.hpp"
#include "Marf248TriggerExpander.hpp"
#include "Marf248CvExpander.hpp"
#include "PanelLabel.hpp"
// Fuer den Stage-Number-Broadcast (Rechtsklick "Apply to all stages" auf
// Modifier-Buttons/Slidern/Reglern, siehe weiter unten): history::Action/
// history::ComplexAction/history::ParamChange fuer echtes Undo/Redo ueber
// VCV's Standard-Verlauf, da ein einzelner Broadcast potenziell alle 16
// Stages gleichzeitig ueberschreibt.
#include <history.hpp>

struct Marf248; // Forward-Deklaration fuer StageVoltageQuantity

// Rechtsklick-Zahleneingabe: Racks Basisimplementierung (ParamQuantity::
// setDisplayValueString()) parst per std::stof im "C"-Locale, das NUR den
// Punkt als Dezimaltrennzeichen akzeptiert. Bei deutscher Schreibweise mit
// Komma (z.B. "3,472") bricht das Parsen beim Komma ab und uebernimmt nur
// den Teil davor ("3") - wirkt wie sinnloses Runden auf Ganzzahlen und laesst
// bei zum Testen gern nahe am Maximum eingegebenen Werten (z.B. "10,0" ->
// "10") den Eindruck von "Vollausschlag" entstehen (Kinovice-Bugreport).
// Alle drei ParamQuantity-Subklassen mit eigener Rechtsklick-Eingabe
// (StageVoltageQuantity, StageTimeQuantity, TempoQuantity) ersetzen daher
// Kommas durch Punkte, BEVOR sie an die Basisklasse durchreichen - simpelste
// und robusteste Loesung, ohne Racks Locale-Handling selbst anfassen zu
// muessen.
static std::string normalizeDecimalComma(std::string s) {
	for (char& c : s) {
		if (c == ',') {
			c = '.';
		}
	}
	return s;
}

// Zeigt den Voltage-Regler-Wert als Notenname (z.B. "C#4") an, wenn Quantize
// fuer die jeweilige Stage aktiv ist; sonst normale Volt-Anzeige. Die
// Methode wird erst NACH der vollstaendigen Definition von Marf248
// implementiert (siehe unten), da sie auf dessen Member zugreift.
struct StageVoltageQuantity : ParamQuantity {
	int stageIndex = 0;
	std::string getDisplayValueString() override;
	// Notennamen-Eingabe (Kinovice-Bugreport "Slider nehmen keine
	// Notenbezeichnungen an") wird IMMER akzeptiert, unabhaengig vom
	// Quantize-Zustand der Stage (MOD_QUANT_OCT beeinflusst nur die
	// ANZEIGE in getDisplayValueString(), nicht was hier eingegeben werden
	// darf). Notenname hat Vorrang: erst versuchen, bei Fehlschlag auf das
	// bisherige Zahlen-Parsing (inkl. Komma-Fix) zurueckfallen.
	void setDisplayValueString(std::string s) override {
		float noteVoltage = 0.f;
		if (noteNameToVoltage(s, noteVoltage)) {
			setValue(rack::clamp(noteVoltage, getMinValue(), getMaxValue()));
			return;
		}
		ParamQuantity::setDisplayValueString(normalizeDecimalComma(s));
	}
};

// Zeigt den Drehschalter-Wert als Klartext-Modus an ("Full", "Half",
// "Lim +0" ... "Lim +8"), statt einer nackten Zahl 0-6. Braucht keinen
// Modul-Zugriff, da die Position selbst schon den vollstaendigen Zustand
// codiert (anders als bei StageVoltageQuantity/Quantize).
// Zeigt den (jetzt normalisierten 0..1) Time-Regler-Wert als tatsaechliche
// Sekundenzahl an, aufgeloest gegen den aktuell gewaehlten Time-Range (TR1-4,
// siehe MOD_TRANGE_*-Bits in Marf248.hpp) - analog zu StageVoltageQuantity,
// die den Voltage-Regler je nach Quantize-Zustand anders anzeigt. Wird erst
// nach der vollstaendigen Definition von Marf248 implementiert (siehe unten).
struct StageTimeQuantity : ParamQuantity {
	int stageIndex = 0;
	// Dial-Wert 0-10 (Buchla-Cookbook-Konvention: die Zeit-Regler sind am
	// Original mit einer 0-10-Skala bedruckt, Tabellenwerte in Patches wie
	// dem String Patch beziehen sich direkt darauf). Intern bleibt der rohe
	// Parameterwert weiterhin 0..1 (kein Breaking Change fuer bestehende
	// Presets, die MOD_TIME_EXT-Kanalwahl oder resolvedTime[] in
	// Marf248::process() - siehe dortige Kommentare) - nur Anzeige und
	// Rechtsklick-Zahleneingabe werden auf 0-10 umgerechnet.
	float getDisplayValue() override { return getValue() * 10.f; }
	void setDisplayValue(float displayValue) override { setValue(displayValue / 10.f); }
	std::string getDisplayValueString() override;
	// Komma-Fix (siehe normalizeDecimalComma oben) - hier besonders
	// relevant, da genau dieses Feld den Kinovice-Bugreport ausgeloest hat.
	void setDisplayValueString(std::string s) override {
		ParamQuantity::setDisplayValueString(normalizeDecimalComma(s));
	}
};

// Zeigt den TIME_MULT_A/B_PARAM-Rohwert (jetzt eine V/Oct-Spannung, siehe
// Marf248.hpp) als BPM-Zahl an, fuehrend, mit der Rohspannung in Klammern
// dahinter - analog zum StageTimeQuantity-Muster oben (Dial-Wert fuehrend,
// aufgeloester Wert in Klammern). Braucht keinen Modul-Zugriff, da die
// Umrechnung (tempoVoltsToBpm) rein aus dem Parameterwert selbst folgt,
// unabhaengig von CV/Attenuverter (die Anzeige des tatsaechlich inkl. CV
// wirksamen Tempos passiert separat im Live-BPM-Panel-Text, siehe
// Marf248Widget).
struct TempoQuantity : ParamQuantity {
	// BUGFIX: vorher war nur getDisplayValueString() (Tooltip-Text)
	// ueberschrieben, nicht aber getDisplayValue()/setDisplayValue() - die
	// Rechtsklick-Zahleneingabe von Rack haengt aber an LETZTEREN, nicht am
	// Tooltip-String. Ohne diesen Override griff die Eingabe direkt auf den
	// rohen Volt-Wert (-5..5) zu, obwohl der Tooltip BPM anzeigte - eine
	// eingegebene "120" landete dadurch als 120 Volt, sofort auf den
	// Anschlag (5V) geklemmt (Kinovice-Bugreport: "dreht wirr, Knopf geht
	// auf Vollausschlag"). getDisplayValue()/setDisplayValue() sind jetzt
	// die BPM-Zahl selbst (log2-Ruecktransformation in setDisplayValue()),
	// analog zum StageTimeQuantity-Dial-Muster oben.
	float getDisplayValue() override {
		return tempoVoltsToBpm(getValue());
	}
	void setDisplayValue(float displayValue) override {
		// Sicherheitsklammer gegen 0/negative Eingaben (log2 waere sonst
		// -inf/NaN) - 0.01 BPM ist ohnehin weit ausserhalb sinnvoller Werte.
		float bpm = std::max(displayValue, 0.01f);
		setValue(std::log2(bpm / TEMPO_BASE_BPM));
	}
	std::string getDisplayValueString() override {
		return string::f("%.1f BPM (%.2f V)", getDisplayValue(), getValue());
	}
	// Komma-Fix (siehe normalizeDecimalComma oben) - gleiche Rack-Eigenheit
	// betrifft auch die BPM-Eingabe.
	void setDisplayValueString(std::string s) override {
		ParamQuantity::setDisplayValueString(normalizeDecimalComma(s));
	}
};

struct StageRangeQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		static const char* labels[7] = {"Full", "Half", "Lim +0", "Lim +2", "Lim +4", "Lim +6", "Lim +8"};
		int pos = (int) std::round(getValue());
		if (pos >= 0 && pos < 7) {
			return labels[pos];
		}
		return ParamQuantity::getDisplayValueString();
	}
};

struct Marf248 : Module, Marf248Host {
	enum ParamId {
		ENUMS(STAGE_VOLTAGE_PARAM, NUM_STAGES),
		ENUMS(STAGE_TIME_PARAM, NUM_STAGES),
		ENUMS(STAGE_FIRST_PARAM, NUM_STAGES),
		ENUMS(STAGE_LAST_PARAM, NUM_STAGES),
		ENUMS(STAGE_STOP_PARAM, NUM_STAGES),
		ENUMS(STAGE_SUSTAIN_PARAM, NUM_STAGES),
		ENUMS(STAGE_ENABLE_PARAM, NUM_STAGES),
		ENUMS(STAGE_SLOPE_PARAM, NUM_STAGES),
		ENUMS(STAGE_QUANT_PARAM, NUM_STAGES),
		ENUMS(STAGE_RANGE_PARAM, NUM_STAGES),
		ENUMS(STAGE_VOLTEXT_PARAM, NUM_STAGES),
		ENUMS(STAGE_TIMEEXT_PARAM, NUM_STAGES),
		ENUMS(STAGE_SHAPE_PARAM, NUM_STAGES),
		ENUMS(STAGE_CURVE_PARAM, NUM_STAGES),
		// Normalize: reine Port-Erweiterung ohne Hardware-Vorbild (siehe
		// MOD_SLEW_NORMALIZE in Marf248.hpp), eigener Toggle-Button pro
		// Stage, unabhaengig vom Shape-Button.
		ENUMS(STAGE_NORMALIZE_PARAM, NUM_STAGES),
		// Programmed Pulse Outputs 1/2 (Manual "Output Pulses"): je Stage
		// unabhaengig togglebar, feuern beim Erreichen der Stage auf den
		// modulweiten P1OUT/P2OUT der jeweiligen Section (kontinuierliche
		// Flankenerkennung, siehe Afg::process()). Skip ist eine reine
		// Port-Erweiterung ohne
		// Manual-Entsprechung.
		ENUMS(STAGE_PULSE1_PARAM, NUM_STAGES),
		ENUMS(STAGE_PULSE2_PARAM, NUM_STAGES),
		ENUMS(STAGE_SKIP_PARAM, NUM_STAGES),
		// Time Range (TR1-4): Zyklus-Taster wie Shape, togglet die exklusiven
		// MOD_TRANGE_*-Bits durch (siehe Marf248.hpp). Zeigt seinen Zustand
		// direkt als eingeblendete Ziffer 1-4 auf dem Knopf selbst an (siehe
		// TrangeButton), daher keine eigene Light-Id noetig.
		ENUMS(STAGE_TRANGE_PARAM, NUM_STAGES),
		RUN_A_PARAM,
		RUN_B_PARAM,
		// Laufrichtung pro Section jetzt als zwei unabhaengige Schalter statt
		// eines einzelnen Zyklus-Buttons: MODE kehrt die Basisrichtung um
		// (Forward/Reverse), PENDULUM schaltet das Bouncing dazu. Erlaubt
		// Pendulum in beide Anfangsrichtungen zu starten (siehe Afg::advanceStage).
		MODE_A_PARAM,
		PENDULUM_A_PARAM,
		MODE_B_PARAM,
		PENDULUM_B_PARAM,
		// Stage Address, je Section: Adress-Knob (Internal), Int/Ext-Quelle
		// und Continuous sind latching Klick-Toggles (analog zu Mode/
		// Pendulum), Strobe und Reset sind reine Momentary-Taster. Siehe
		// Afg::applyStageAddress() fuer die Sprunglogik.
		STAGE_ADDR_KNOB_A_PARAM,
		STAGE_ADDR_MODE_A_PARAM,
		STAGE_ADDR_CONT_A_PARAM,
		STAGE_ADDR_STROBE_A_PARAM,
		STAGE_ADDR_RESET_A_PARAM,
		STAGE_ADDR_KNOB_B_PARAM,
		STAGE_ADDR_MODE_B_PARAM,
		STAGE_ADDR_CONT_B_PARAM,
		STAGE_ADDR_STROBE_B_PARAM,
		STAGE_ADDR_RESET_B_PARAM,
		// Global Clear: einzelner, modulweit geteilter Momentary-Taster (die
		// Stage-Daten selbst sind ja schon modulweit geteilt zwischen Section
		// A und B - siehe stageModifier-Kommentar), kein Paar pro Section.
		CLEAR_PARAM,
		// Reset Values: reine Software-Erweiterung ohne Hardware-Vorbild
		// (physische Slider "sind" halt wo sie sind, siehe Global-Clear-
		// Kommentar). Setzt NUR Voltage+Time aller 16 Stages auf ihren
		// configParam-Default zurueck - Modifier-Bits/Range/Curve bleiben
		// bewusst unangetastet (klare Abgrenzung zu Global Clear, siehe
		// process()).
		RESET_VALUES_PARAM,
		// Auto-Fill: reine Software-Erweiterung ohne Hardware-Vorbild (wie
		// RESET_VALUES_PARAM), fuer den Phasor-Generator-Anwendungsfall.
		// Setzt die 16 geteilten Voltage-Regler auf ein lineares 0-10V-Raster
		// (i * 10/16 = 0.625V-Schritte, Stufe 1=0V .. Stufe 16=9.375V) und
		// erzwingt Range=Full (siehe performAutoFill()). Da Voltage/Range
		// modulweit zwischen Section A und B geteilt sind, gibt es bewusst
		// nur einen einzigen, globalen Button (keine getrennten A/B-Varianten
		// moeglich - siehe Projektnotizen).
		AUTOFILL_PARAM,
		// Time Multiplier (Manual "TIME MULTIPLIER"): Hauptknob 0.5x-4.0x je
		// Section, plus Attenuverter fuer den zugehoerigen dedizierten CV-
		// Eingang (siehe INPUTS_LEN unten). Wirkt multiplikativ auf die
		// Stage-Zeiten NUR der jeweiligen Section - anders als die Stage-Daten
		// selbst ist dies also bewusst NICHT modulweit geteilt, siehe
		// Marf248::process().
		TIME_MULT_A_PARAM,
		TIME_MULT_ATTEN_A_PARAM,
		TIME_MULT_B_PARAM,
		TIME_MULT_ATTEN_B_PARAM,
		// Preset-System (Manual "PRESETS AND QUANTIZATION SCALES"): LOAD/SAVE
		// sind latching "scharf schalten"-Taster, PRESET_PARAM 1-12 sind die
		// eigentlichen Slot-Tasten. Autotune bewusst nicht Teil dieser
		// Session (haengt mit ART zusammen, siehe Projektnotizen).
		LOAD_PARAM,
		SAVE_PARAM,
		ENUMS(PRESET_PARAM, 12),
		// Scale/Key: gleiches "scharf schalten"-Prinzip wie LOAD/SAVE, reusen
		// dieselben 12 Preset-Taster als Doppelbelegung (Manual "Changing
		// Key"/"Changing Scale"). KEY_PARAM: naechster Druck auf 1-12 waehlt
		// C..B. SCALE_PARAM: nur 10/11/12 sind gueltig (Major/Minor/
		// Chromatic), siehe process()-Logik.
		SCALE_PARAM,
		KEY_PARAM,
		// BEATS_FACTOR (BPM-Feature): Trimpot pro Section, legt fest, wie
		// viele Beats ein voller Zyklus ueber alle NICHT geskippten Stages
		// darstellen soll - daraus wird der tatsaechliche Time-Multiplier
		// zurueckgerechnet (siehe process()). Ans Ende des Enums angehaengt
		// (wie ueblich bei nachtraeglich hinzugefuegten Params), damit
		// bestehende Preset-/Patch-Indizes unveraendert bleiben.
		BEATS_FACTOR_A_PARAM,
		BEATS_FACTOR_B_PARAM,
		// Stage MUTE (Session "Hauptmodul-Relayout"): bisher nur ueber
		// Marf248TriggerExpander::STAGE_MUTE_POLY_INPUT bedienbar (siehe
		// dortiger Kommentar), jetzt zusaetzlich als eigener Panel-Button pro
		// Stage - gleiches Klick-Toggle-Prinzip wie STAGE_PULSE1/2_PARAM.
		// Ans Enum-Ende angehaengt statt einsortiert, um bestehende Patch-
		// Indizes nicht zu verschieben (Standard-Prinzip in dieser Datei).
		ENUMS(STAGE_MUTE_PARAM, NUM_STAGES),
		// Quantized Glide (siehe MOD_QUANT_GLIDE in Marf248.hpp): eigener
		// Toggle-Button pro Stage, gleiches Klick-Prinzip wie STAGE_QUANT_
		// PARAM. Ans Enum-Ende angehaengt statt einsortiert (Standard-
		// Prinzip in dieser Datei, siehe STAGE_MUTE_PARAM-Kommentar oben).
		ENUMS(STAGE_QUANTGLIDE_PARAM, NUM_STAGES),
		PARAMS_LEN
	};
	enum InputId {
		START_A_INPUT,
		STOP_A_INPUT,
		START_B_INPUT,
		STOP_B_INPUT,
		// Vier externe CV-Eingaenge, modulweit geteilt zwischen Section A und B
		// (nicht pro Section dupliziert) - deckt sich mit dem Manual ("Shared
		// between both AFGs") und der 248t-Hardware.
		EXT_A_INPUT,
		EXT_B_INPUT,
		EXT_C_INPUT,
		EXT_D_INPUT,
		// Externe CV fuer die Stage-Address-Adressierung, je Section eigener
		// Eingang (im Gegensatz zu EXT_A..D ist dies NICHT geteilt, da jede
		// Section unabhaengig eine andere Stage adressieren koennen soll).
		STAGE_ADDR_EXT_A_INPUT,
		STAGE_ADDR_EXT_B_INPUT,
		// Time Multiplier CV: eigener dedizierter Eingang je Section (im
		// Gegensatz zu EXT_A..D NICHT geteilt, analog zu STAGE_ADDR_EXT_A/B -
		// jede Section soll unabhaengig extern modulierbar sein).
		TIME_MULT_CV_A_INPUT,
		TIME_MULT_CV_B_INPUT,
		// Kanalwahl-CV fuer die externe Stage-Address-CV (siehe
		// STAGE_ADDR_EXT_A/B_INPUT weiter oben, das jetzt bis zu 4 Kanaele
		// A-D via Poly-Kabel tragen kann statt nur eines einzelnen mono
		// Werts): 0-10V in 4 gleich grossen Zonen waehlt den aktiven Kanal,
		// analog zur bestehenden Zonenlogik bei Volt/Time External (siehe
		// MOD_VOLT_EXT-Kommentar). Unverbunden faellt auf Kanal A (Index 0)
		// zurueck - bestehende Mono-Patches an STAGE_ADDR_EXT_A/B_INPUT
		// bleiben dadurch unveraendert funktionsfaehig. Bewusst ans Ende des
		// Enums angehaengt statt direkt neben STAGE_ADDR_EXT_A/B_INPUT
		// einsortiert, um die Indizes der bereits existierenden
		// TIME_MULT_CV_A/B_INPUT (und damit bestehende Patch-Kabel) nicht zu
		// verschieben - gleiches Prinzip wie bei ADVANCE_A/B_INPUT im
		// Trigger-Expander.
		STAGE_ADDR_EXT_CHANNEL_A_INPUT,
		STAGE_ADDR_EXT_CHANNEL_B_INPUT,
		// BEATS_FACTOR CV-Eingang je Section (BPM-Feature), addiert sich
		// direkt (linear, keine V/Oct-Semantik) auf den BEATS_FACTOR_A/B_
		// PARAM-Trimpot-Wert - siehe Kommentar bei BEATS_FACTOR_A/B_PARAM
		// und process(). Ans Ende des Enums angehaengt, gleiches Prinzip
		// wie bei STAGE_ADDR_EXT_CHANNEL_A/B_INPUT oben.
		BEATS_FACTOR_CV_A_INPUT,
		BEATS_FACTOR_CV_B_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		VOUT_A_OUTPUT,
		TOUT_A_OUTPUT,
		APOUT_A_OUTPUT,
		P1OUT_A_OUTPUT,
		P2OUT_A_OUTPUT,
		VOUT_B_OUTPUT,
		TOUT_B_OUTPUT,
		APOUT_B_OUTPUT,
		P1OUT_B_OUTPUT,
		P2OUT_B_OUTPUT,
		// Time Output (Manual "Outputs and Indicators"): statische CV
		// proportional zur rohen Interval-Time-Sliderposition (0..1 ->
		// 0-10V), UNABHAENGIG von Time Range (TR1-4), External-Source-CV
		// oder der ablaufenden Stage-Phase. Bewusst getrennt von
		// TOUT_A/B_OUTPUT weiter oben, das trotz seines Namens tatsaechlich
		// die im Manual als "Reference Output" beschriebene abfallende
		// Rampe ueber die Stage-Zeit ausgibt (siehe Kommentar dort).
		// Neuer Eintrag ans Enum-Ende angehaengt (nicht zwischen VOUT/TOUT
		// einsortiert), damit bestehende Patch-Kabel-Indizes stabil
		// bleiben - siehe Projektkonvention "Enum entries ... always
		// appended at end".
		TIMESTATIC_A_OUTPUT,
		TIMESTATIC_B_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(STAGE_LIGHT_A, NUM_STAGES),
		ENUMS(STAGE_LIGHT_B, NUM_STAGES),
		ENUMS(STAGE_FIRST_LIGHT, NUM_STAGES),
		ENUMS(STAGE_LAST_LIGHT, NUM_STAGES),
		ENUMS(STAGE_STOP_LIGHT, NUM_STAGES),
		ENUMS(STAGE_SUSTAIN_LIGHT, NUM_STAGES),
		ENUMS(STAGE_ENABLE_LIGHT, NUM_STAGES),
		ENUMS(STAGE_SLOPE_LIGHT, NUM_STAGES),
		ENUMS(STAGE_QUANT_LIGHT, NUM_STAGES),
		ENUMS(STAGE_SHAPE_EXP_LIGHT, NUM_STAGES),
		ENUMS(STAGE_SHAPE_LOG_LIGHT, NUM_STAGES),
		ENUMS(STAGE_NORMALIZE_LIGHT, NUM_STAGES),
		ENUMS(STAGE_VOLTEXT_LIGHT, NUM_STAGES),
		ENUMS(STAGE_TIMEEXT_LIGHT, NUM_STAGES),
		ENUMS(STAGE_PULSE1_LIGHT, NUM_STAGES),
		ENUMS(STAGE_PULSE2_LIGHT, NUM_STAGES),
		ENUMS(STAGE_SKIP_LIGHT, NUM_STAGES),
		RUN_A_LIGHT,
		RUN_A_LIGHT_R = RUN_A_LIGHT,
		RUN_A_LIGHT_G,
		RUN_A_LIGHT_B,
		RUN_B_LIGHT,
		RUN_B_LIGHT_R = RUN_B_LIGHT,
		RUN_B_LIGHT_G,
		RUN_B_LIGHT_B,
		// Mode (an = Reverse, aus = Forward) und Pendulum (an = aktiv) je
		// eine LED pro Section.
		MODE_A_LIGHT,
		PENDULUM_A_LIGHT,
		MODE_B_LIGHT,
		PENDULUM_B_LIGHT,
		// Dreifarbige Stage-Address-Statusanzeige je Section (Manual
		// "FUNCTION GENERATOR CONTROL - MODE": Gruen=Run, Gelb=Hold,
		// Rot=Stop): eine gemeinsame RedGreenBlueLight statt separater
		// Einzel-LEDs, siehe RUN_A/B_LIGHT_R/G/B oben (Kinovice-Entscheidung
		// diese Session). Ersetzt einen fruehen, nie am Panel verdrahteten
		// Anlauf mit vier separaten HOLD_A/STOP_A/HOLD_B/STOP_B-LightIds
		// (waren im Enum und wurden in process() beschrieben, aber nie per
		// createLightCentered aufs Panel gebracht - siehe Git-Historie,
		// falls das nochmal nachvollzogen werden muss).
		// Int/Ext- und Continuous-Zustand der Stage Address, je Section.
		STAGE_ADDR_MODE_A_LIGHT,
		STAGE_ADDR_CONT_A_LIGHT,
		STAGE_ADDR_MODE_B_LIGHT,
		STAGE_ADDR_CONT_B_LIGHT,
		// Kurzer Blitz beim Druecken von Strobe/Reset (reine Momentary-
		// Aktionen ohne Dauerzustand, daher Blitz statt Toggle-Licht -
		// gleiches Muster wie CLEAR_LIGHT/RESET_VALUES_LIGHT weiter unten,
		// aber blau statt gelb, um optisch in derselben Stage-Address-
		// Spalte wie Mode/Continuous zu bleiben). War zuvor komplett
		// unverdrahtet (kein Light-ID, kein addChild im Widget) - siehe
		// Bugfix-Session.
		STAGE_ADDR_STROBE_A_LIGHT,
		STAGE_ADDR_RESET_A_LIGHT,
		STAGE_ADDR_STROBE_B_LIGHT,
		STAGE_ADDR_RESET_B_LIGHT,
		// Kurzer Blitz beim Druecken von Global Clear, als Rueckmeldung
		// (der Taster selbst hat sonst keinen dauerhaften Zustand).
		CLEAR_LIGHT,
		// Gleiches Blitz-Feedback-Muster fuer Reset Values (siehe
		// RESET_VALUES_PARAM).
		RESET_VALUES_LIGHT,
		// Gleiches Blitz-Feedback-Muster fuer Auto-Fill (siehe AUTOFILL_PARAM).
		AUTOFILL_LIGHT,
		// Output LEDs (siehe Referenzdokumentation, Abschnitt Ausgangs-LEDs):
        // zeigen den tatsaechlichen P1OUT/P2OUT-Trigger in
		// Echtzeit, unabhaengig von den STAGE_PULSE1/2_LIGHT oben (die
		// zeigen nur den Programmierzustand der aktuellen Stage, nicht den
		// Puls selbst).
		P1_ACTIVE_A_LIGHT,
		P2_ACTIVE_A_LIGHT,
		P1_ACTIVE_B_LIGHT,
		P2_ACTIVE_B_LIGHT,
		// Preset-System: LOAD/SAVE zeigen den "scharf"-Zustand, PRESET_LIGHT
		// je Slot zeigt gedimmt einen belegten Slot an und blitzt kurz auf
		// bei Load/Save (siehe CLEAR_LIGHT-Muster).
		LOAD_LIGHT,
		SAVE_LIGHT,
		ENUMS(PRESET_LIGHT, 12),
		// Scale/Key "scharf"-Anzeige, gleiches Muster wie LOAD_LIGHT/
		// SAVE_LIGHT. Kein eigenes Dauerlicht fuer die aktuell gewaehlte
		// Scale/Key-Einstellung (siehe Projektentscheidung: nur kurzer
		// Flash beim Uebernehmen, kein Dauerlicht).
		SCALE_LIGHT,
		KEY_LIGHT,
		// Passend zu STAGE_MUTE_PARAM oben (Session "Hauptmodul-Relayout").
		ENUMS(STAGE_MUTE_LIGHT, NUM_STAGES),
		// Quantized Glide (siehe STAGE_QUANTGLIDE_PARAM oben).
		ENUMS(STAGE_QUANTGLIDE_LIGHT, NUM_STAGES),
		LIGHTS_LEN
	};

	// Geteilter Stage-Zustand, jetzt per Klick-Buttons im Panel gesetzt
	// (statt Rechtsklick-Menue) und per dataToJson/dataFromJson persistiert.
	int stageModifier[NUM_STAGES] = {};

	// Ergebnisse der Trigger-Expander Stage-Modifier-Poly-CV-Overrides
	// (Session "CPU-Optimierung Stage-Modifier-Scans"): vorher lokale
	// process()-Variablen, die JEDEN Sample frisch auf ihren Default
	// zurueckgesetzt und bei gepatchtem Kabel neu berechnet wurden. Jetzt
	// Member, da die zugehoerige Scan-Schleife per stageCvOverrideDivider
	// (/8) dekimiert ist und ihren zuletzt berechneten Wert ueber die
	// Samples zwischen zwei Divider-Ticks hinweg BEHALTEN muss (siehe
	// process()) - sonst waere das Override 7 von 8 Samples lang
	// wirkungslos. -1 = kein Kanal-Override, 0 = keine Ratchet-Count-CV
	// gepatcht, 0.f = kein Ratchet-Offset (siehe jeweilige Kommentare an
	// der Berechnungsstelle in process()).
	int voltExtChannelOverride[NUM_STAGES];
	int timeExtChannelOverride[NUM_STAGES];
	int stageRatchetCount[NUM_STAGES];
	float stageRatchetOffset[NUM_STAGES];

	// Laufrichtung pro Section: zwei unabhaengige booleans statt eines
	// einzelnen Zustands, da MODE_A/B_PARAM und PENDULUM_A/B_PARAM jetzt
	// getrennte Toggle-Buttons sind (analog zu den Modifier-Buttons pro
	// Stage), nicht mehr ein einzelner Zyklus-Button.
	bool reverseA = false;
	bool pendulumOnA = false;
	bool reverseB = false;
	bool pendulumOnB = false;
	dsp::SchmittTrigger modeAButtonTrigger;
	dsp::SchmittTrigger pendulumAButtonTrigger;
	dsp::SchmittTrigger modeBButtonTrigger;
	dsp::SchmittTrigger pendulumBButtonTrigger;

	// Stage Address: Int/Ext-Quelle und Continuous sind latching (bool +
	// Trigger wie Mode/Pendulum), Strobe/Reset sind reine Momentary-Trigger
	// ohne gespeicherten Zustand.
	bool addrExtA = false;
	bool addrContOnA = false;
	bool addrExtB = false;
	bool addrContOnB = false;

	// BEATS_FACTOR Auto-Modus (Segmentlaengen-Sync, Session "BEATS_FACTOR
	// Auto-Modus"): pro Section unabhaengig umschaltbar zwischen manuellem
	// BEATS_FACTOR_A/B_PARAM-Trimpot-Wert (bisheriges Verhalten) und Auto
	// (BEATS_FACTOR wird aus der Anzahl nicht-geskippter Stages im aktuell
	// aktiven First/Last-Segment abgeleitet, siehe Afg::updateAutoSegment()
	// in Marf248.hpp und process() unten). Umschaltung per Kontextmenue am
	// Trimpot (BeatsFactorTrimpot::appendContextMenu()), kein eigener
	// Panel-Platz/LightId noetig. Persistiert wie reverseA/pendulumOnA etc.
	bool beatsFactorAutoA = false;
	bool beatsFactorAutoB = false;
	dsp::SchmittTrigger addrModeATrigger, addrContATrigger, addrStrobeATrigger, addrResetATrigger;
	dsp::SchmittTrigger addrModeBTrigger, addrContBTrigger, addrStrobeBTrigger, addrResetBTrigger;
	// Blitz-Feedback fuer Strobe/Reset (siehe STAGE_ADDR_STROBE_A_LIGHT
	// etc.), gleiches Muster wie resetValuesFlash weiter unten.
	dsp::PulseGenerator addrStrobeAFlash, addrResetAFlash, addrStrobeBFlash, addrResetBFlash;

	// Global Clear: Momentary-Taster + kurzer Licht-Blitz als Rueckmeldung.
	dsp::SchmittTrigger clearTrigger;
	dsp::PulseGenerator clearFlash;

	// Reset Values: eigener Momentary-Taster + Blitz, unabhaengig von Clear
	// (siehe RESET_VALUES_PARAM).
	dsp::SchmittTrigger resetValuesTrigger;
	dsp::PulseGenerator resetValuesFlash;

	// Auto-Fill: eigener Momentary-Taster + Blitz, unabhaengig von Clear/
	// Reset Values (siehe AUTOFILL_PARAM).
	dsp::SchmittTrigger autoFillTrigger;
	dsp::PulseGenerator autoFillFlash;

	// Preset-System (Manual "PRESETS AND QUANTIZATION SCALES", ohne Scale/
	// Key/Autotune - siehe Projektnotizen). Ein Slot speichert die komplette
	// Stage-Programmierung aller 16 Stages: Voltage-/Time-Slider, Range- und
	// Curve-Reglerposition sowie alle Modifier-Bits. "used" verhindert, dass
	// ein nie gespeicherter Slot beim Laden versehentlich alles auf Default
	// zuruecksetzt.
	struct PresetSlot {
		bool used = false;
		float voltage[NUM_STAGES] = {};
		float time[NUM_STAGES] = {};
		float range[NUM_STAGES] = {};
		float curve[NUM_STAGES] = {};
		int modifier[NUM_STAGES] = {};
	};
	PresetSlot presets[12];

	// Adress-Speicher (16 Slots je Section, unabhaengig vom Preset-System
	// oben): reines Sprung-Lesezeichen fuer die Stage-Address-Funktion -
	// speichert NUR den Stage-Index, keine Voltage/Time/Range/Curve/
	// Modifier-Werte wie ein Preset. Gedacht fuer feste Sprungziele (z.B.
	// die Startpositionen mehrerer First/Last-Segmente), per Trigger sofort
	// anfahrbar, unabhaengig von den 4 externen Adress-Kanaelen (siehe
	// STAGE_ADDR_EXT_A/B_INPUT) und vom Stage-Address-Knob. "used"
	// verhindert einen ungewollten Sprung auf Stage 1 bei einem nie
	// beschriebenen Slot (Index 0 waere sonst ein gueltiger, aber
	// unbeabsichtigter Default-Wert - gleiches Prinzip wie PresetSlot::used).
	struct AddrMemorySlot {
		bool used = false;
		int stage = 0;
	};
	AddrMemorySlot addrMemA[NUM_STAGES];
	AddrMemorySlot addrMemB[NUM_STAGES];

	// Trigger-Detektoren fuer die Poly-Store/Recall-Eingaenge am Trigger-
	// Expander (ADDR_STORE_A/B_POLY_INPUT, ADDR_RECALL_A/B_POLY_INPUT),
	// Kanal i = Slot i, analog zu expPresetLoadTrigger weiter unten. Store
	// und Recall sowie A und B jeweils eigene Arrays, da alle vier
	// unabhaengig und gleichzeitig gepatcht sein koennen.
	dsp::SchmittTrigger expAddrStoreATrigger[NUM_STAGES];
	dsp::SchmittTrigger expAddrStoreBTrigger[NUM_STAGES];
	dsp::SchmittTrigger expAddrRecallATrigger[NUM_STAGES];
	dsp::SchmittTrigger expAddrRecallBTrigger[NUM_STAGES];

	// Pickup/Catch je Stage fuer alle vier kontinuierlichen Regler (Voltage,
	// Time, Range, Curve) - siehe PickupState in Marf248.hpp. Wird beim
	// Preset-Load bestueckt (loadPreset()) und jeden Frame in process()
	// aufgeloest, BEVOR der jeweilige Reglerwert irgendwo sonst verwendet
	// wird (Range/Quantize/Slew etc. muessen bereits den aufgeloesten statt
	// den rohen Reglerwert sehen).
	PickupState pickupVoltage[NUM_STAGES];
	PickupState pickupTime[NUM_STAGES];
	PickupState pickupRange[NUM_STAGES];
	PickupState pickupCurve[NUM_STAGES];

	// Cache fuer die Curve-Power-Berechnung (std::pow() ist teuer und wurde
	// bisher JEDEN Sample fuer alle 16 Stages neu berechnet, unabhaengig
	// davon ob sich der aufgeloeste Curve-Knob seit dem letzten Sample
	// ueberhaupt geaendert hat - siehe process(), Stage-Scan-Schleife).
	// lastCurveKnob[i] < 0 signalisiert "noch nie berechnet" (aufgeloester
	// Knob liegt immer in [0,1]), erzwingt also im ersten Sample pro Stage
	// eine initiale Berechnung. cachedCurvePower[i] haelt das zuletzt
	// berechnete std::pow()-Ergebnis und wird nur bei tatsaechlicher
	// Knob-Aenderung neu berechnet (siehe process()).
	float lastCurveKnob[NUM_STAGES];
	float cachedCurvePower[NUM_STAGES];

	// LOAD/SAVE sind latching "scharf schalten"-Taster (wie Mode/Pendulum):
	// ein Druck aktiviert den jeweiligen Modus und deaktiviert automatisch
	// den anderen. Der naechste Druck auf einen der 12 Preset-Taster fuehrt
	// die entsprechende Aktion aus und deaktiviert den Modus wieder.
	bool loadArmed = false;
	bool saveArmed = false;
	dsp::SchmittTrigger loadTrigger, saveTrigger;
	dsp::SchmittTrigger presetTrigger[12];
	dsp::PulseGenerator presetFlash[12];

	// Scale/Key (Manual "PRESETS AND QUANTIZATION SCALES"): globaler,
	// modulweit geteilter Quantisierungs-Kontext fuer alle Stages mit
	// aktivem MOD_QUANT_OCT (beide Sections). Default Chromatic + C, damit
	// bestehende Patches ohne Scale/Key-Wissen unveraendert klingen (reines
	// Halbton-Raster wie bisher). currentKey: 0=C..11=B. currentScale:
	// 0=Major, 1=Minor, 2=Chromatic. Arm-Mechanik identisch zu LOAD/SAVE,
	// reust dieselben 12 Preset-Taster als Doppelbelegung.
	int currentKey = 0;
	int currentScale = 2;
	bool scaleArmed = false;
	bool keyArmed = false;
	dsp::SchmittTrigger scaleTrigger, keyTrigger;

	// Trigger-Expander (Marf248TriggerExpander): eigene Trigger-Detektoren
	// fuer die extern gespeisten Aktionen, getrennt von den Panel-Button-
	// Triggern oben, da sie eine andere Spannungsquelle (Expander-Eingang
	// statt Taster-Parameter) abfragen. Key laeuft ueber kontinuierliche CV
	// und braucht daher keinen eigenen Trigger. Siehe process().
	dsp::SchmittTrigger expClearTrigger;
	dsp::SchmittTrigger expResetValuesTrigger;
	dsp::SchmittTrigger expPresetLoadTrigger[12];
	dsp::SchmittTrigger expScaleMajorTrigger, expScaleMinorTrigger, expScaleChromaticTrigger;
	// SEXT (externe Strobe-Pulse, Buchla-Book-Begriff): getrennter Trigger-
	// Detektor pro Section, wird VOR dem eigentlichen Stage-Address-Block
	// ausgewertet und mit dem Panel-Strobe-Taster ODER-verknuepft (siehe
	// process()).
	dsp::SchmittTrigger expStrobeATrigger, expStrobeBTrigger;

	dsp::SchmittTrigger stageFirstTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageLastTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageStopTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageSustainTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageEnableTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageSlopeTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageQuantTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageShapeTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageNormalizeTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageVoltExtTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageTimeExtTrigger[NUM_STAGES];
	dsp::SchmittTrigger stagePulse1Trigger[NUM_STAGES];
	dsp::SchmittTrigger stagePulse2Trigger[NUM_STAGES];
	dsp::SchmittTrigger stageSkipTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageTrangeTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageMuteTrigger[NUM_STAGES];
	dsp::SchmittTrigger stageQuantGlideTrigger[NUM_STAGES];

	// CPU-Optimierung (siehe perf-Profiling-Session): die Klick-Button-
	// Erkennung + Light-Update fuer alle 16 Stages (weiter unten in
	// process()) muss nicht bei jedem einzelnen Audio-Sample laufen -
	// Taster-Zustandswechsel und LED-Helligkeit sind fuer menschliche
	// Bedienung ohnehin nicht schneller als im Millisekundenbereich
	// wahrnehmbar. Divisor 32 haelt die maximale Reaktionsverzoegerung
	// selbst bei den vom Nutzer typischerweise gefahrenen 24-32kHz
	// Sample-Rates unter ~1,3ms - unhoerbar/unspuerbar, aber deutliche
	// CPU-Ersparnis, da der Block ca. 450 Operationen/Sample kostet
	// (14 SchmittTrigger + 14 Light-Updates x 16 Stages).
	dsp::ClockDivider stageButtonDivider;

	// CPU-Audit (Session "CPU-Optimierung Stage-Modifier-Scans", Kinovice-
	// Befund: Hauptmodul-Last ca. 3% bei ca. 0,1% pro Expander): die 16
	// Trigger-Expander Stage-Modifier-Poly-CV-Scans (Skip, First, Last,
	// Stop, Sustain, Enable, Pulse1, Pulse2, Mute, Ratchet Enable, Slope,
	// Quantized Glide, VoltExt-Kanal, TimeExt-Kanal, Ratchet Count, Ratchet
	// Offset) liefen bisher UNDEKIMIERT jeden Sample - anders begruendet
	// als beim Klick-Button-Block oben ("CV soll pegelgetreu reagieren"),
	// aber diese Einzelentscheidung pro Feature-Session hat sich ueber 16
	// Bloecke hinweg zum dominanten Kostenfaktor summiert (bis zu 16x16 =
	// 256 getPolyVoltage()-Aufrufe/Sample plus redundante trigExp->
	// getInput()-Lookups je Block). Kinovice-Entscheidung (Nachtrag): /8
	// statt undekimiert - Stage-Modifier sind Kontrolldaten (Gate-
	// Schwellen/Zonen), keine Audiosignale; bei 48kHz max. ~0,17ms, bei
	// den vom Nutzer gefahrenen 24-32kHz Sample-Raten entsprechend mehr,
	// aber weiterhin weit unter jeder wahrnehmbaren Schwelle. Bewusst
	// eigener, schnellerer Divisor als stageButtonDivider (/32) - CV-
	// Override soll trotzdem spuerbar reaktionsschneller bleiben als reine
	// Panel-Klicks.
	dsp::ClockDivider stageCvOverrideDivider;

	// BPM-Feature: Live-Anzeigewerte fuers Panel (NVG-Text), analog zur
	// bestehenden Time-Sekunden-Anzeige am Time-Slider. Die eigentliche
	// BPM-Berechnung/Multiplier-Rueckrechnung laeuft jeden Sample (siehe
	// process(), Time-Multiplier-Block) - hierher wird nur der aktuelle
	// Wert dekimiert (per stageButtonDivider-Rhythmus, Kinovice-
	// Entscheidung) uebernommen, damit der Widget-Draw-Aufruf lediglich
	// einen bereits fertigen Float lesen muss statt jeden Frame selbst
	// neu zu rechnen.
	float displayBpmA = TEMPO_BASE_BPM;
	float displayBpmB = TEMPO_BASE_BPM;

	Afg afgA;
	Afg afgB;

	// Single-Stage-Slot-Anzeige fuer die Kompaktversion "Lucia" (Session
	// "Kompaktversion Marf248 - Panel & Model-Registrierung", Konzept
	// "GUI-Vereinfachung auf einzelnen Stage"). GENAU EIN Paar fuer beide
	// Sections gemeinsam (nicht je eines pro Section) - Voltage/Time/
	// Range/Curve/alle Modifier-Bits sind stage-zentrale Arrays ueber
	// NUM_STAGES, nicht separat pro Section (siehe STAGE_VOLTAGE_PARAM
	// etc. oben sowie fruehere Session-Klaerung "Modulstruktur mit zwei
	// Sektionen und gemeinsamen Stages") - ein zweiter, unabhaengiger
	// Slot fuer Section B wuerde real auf dieselben Params schreiben wie
	// der Slot fuer Section A, also keinen zweiten Bearbeitungskontext
	// ergeben.
	//
	// pendingStage: per Auswahl-Knob angewaehlter, noch nicht uebernommener
	// Index (0-15). displayedStage: per Aktivieren-Button tatsaechlich
	// uebernommener, sichtbarer Index (0-15) - bestimmt in
	// Marf248CompactWidget::step(), welches der 16 Stage-Widget-Sets
	// visible=true ist.
	//
	// Bewusst KEIN Param (Kinovice-Entscheidung im Konzeptdokument):
	// Pickup/Catch, VCV History System und Preset-Recall wuerden sonst
	// die reine GUI-Navigation beeinflussen bzw. rueckwirkend veraendern
	// - unerwuenscht, da die Anzeige ausschliesslich durch bewusste
	// Nutzerinteraktion mit Auswahl-Knob + Aktivieren-Button wechseln
	// soll. Persistenz stattdessen ueber dataToJson()/dataFromJson()
	// unten, analog zu currentKey/currentScale.
	//
	// Auch auf der 146HP-Vollversion vorhanden (gleicher Module-Typ,
	// siehe Marf248Host) - dort schlicht ungenutzt, da Marf248Widget
	// diese Felder nicht referenziert. Kein Verhaltensunterschied fuer
	// bestehende Patches.
	int pendingStage = 0;
	int displayedStage = 0;

	Marf248() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Sentinel -1.f erzwingt im allerersten process()-Sample pro Stage
		// eine einmalige Curve-Power-Berechnung (siehe cachedCurvePower/
		// lastCurveKnob oben sowie process()), da ein aufgeloester Curve-
		// Knob-Wert immer in [0,1] liegt und -1.f somit nie zufaellig als
		// "unveraendert" durchrutscht.
		for (int i = 0; i < NUM_STAGES; i++) {
			lastCurveKnob[i] = -1.f;
			cachedCurvePower[i] = SLEW_CURVE_POWER;
			// Default-Zustand der Trigger-Expander CV-Override-Arrays (siehe
			// Member-Deklaration oben) - identisch zu den fruehreren lokalen
			// Reset-Schleifen in process(), jetzt einmalig im Konstruktor,
			// da die Arrays jetzt zwischen den Samples persistieren.
			voltExtChannelOverride[i] = -1;
			timeExtChannelOverride[i] = -1;
			stageRatchetCount[i] = 0;
			stageRatchetOffset[i] = 0.f;
		}

		stageButtonDivider.setDivision(32);
		stageCvOverrideDivider.setDivision(8);

		for (int i = 0; i < NUM_STAGES; i++) {
			StageVoltageQuantity* vq = configParam<StageVoltageQuantity>(STAGE_VOLTAGE_PARAM + i, 0.f, 10.f, 0.f,
				string::f("Stage %d Voltage", i + 1), " V");
			vq->stageIndex = i;
			// Domaenen-Umstellung fuer Time Range (TR1-4): der Time-Knob
			// speichert jetzt eine normalisierte Position 0..1 statt direkt
			// Sekunden (vorher fest 0.02-0.3s) - die tatsaechliche Sekundenzahl
			// ergibt sich erst aus der Kombination mit dem gewaehlten TR-
			// Bereich (siehe resolvedTime[] in process()), analog dazu wie der
			// Voltage-Regler immer 0-10V bleibt und applyRange() erst danach
			// skaliert. ACHTUNG Kinovice: bestehende Patches interpretieren den
			// gespeicherten Rohwert dadurch anders als vorher (alte 0.1
			// bedeutete 0.1s, jetzt 10% Position im gewaehlten TR-Bereich) -
			// Time-Slider in alten Patches ggf. neu einstellen.
			StageTimeQuantity* tq = configParam<StageTimeQuantity>(STAGE_TIME_PARAM + i, 0.f, 1.f, 0.5f,
				string::f("Stage %d Time", i + 1));
			tq->stageIndex = i;
			configButton(STAGE_FIRST_PARAM + i, string::f("Stage %d First", i + 1));
			configButton(STAGE_LAST_PARAM + i, string::f("Stage %d Last", i + 1));
			configButton(STAGE_STOP_PARAM + i, string::f("Stage %d Stop", i + 1));
			configButton(STAGE_SUSTAIN_PARAM + i, string::f("Stage %d Sustain", i + 1));
			configButton(STAGE_ENABLE_PARAM + i, string::f("Stage %d Enable", i + 1));
			configButton(STAGE_SLOPE_PARAM + i, string::f("Stage %d Slope", i + 1));
			configButton(STAGE_QUANT_PARAM + i, string::f("Stage %d Quantize (1V/Oct)", i + 1));
			configButton(STAGE_QUANTGLIDE_PARAM + i, string::f("Stage %d Quantized Glide", i + 1));
			// Drehschalter mit 7 Raststufen: 0=Full, 1=Half, 2..6=Limited +0/+2/+4/+6/+8.
			// snapEnabled sorgt dafuer, dass der Regler nur auf ganzzahligen
			// Positionen einrastet statt stufenlos zu drehen.
			configParam<StageRangeQuantity>(STAGE_RANGE_PARAM + i, 0.f, 6.f, 0.f,
				string::f("Stage %d Range", i + 1))->snapEnabled = true;
			configButton(STAGE_VOLTEXT_PARAM + i, string::f("Stage %d Voltage Source (Int/Ext)", i + 1));
			configButton(STAGE_TIMEEXT_PARAM + i, string::f("Stage %d Time Source (Int/Ext)", i + 1));
			configButton(STAGE_SHAPE_PARAM + i, string::f("Stage %d Slew Shape (Lin/Exp/Log)", i + 1));
			configButton(STAGE_NORMALIZE_PARAM + i, string::f("Stage %d Slew Normalize (spread curve over full stage time)", i + 1));
			configParam(STAGE_CURVE_PARAM + i, 0.f, 1.f, 0.5f,
				string::f("Stage %d Slew Curve (links=linearer, rechts=kurviger)", i + 1));
			configButton(STAGE_PULSE1_PARAM + i, string::f("Stage %d Pulse 1", i + 1));
			configButton(STAGE_PULSE2_PARAM + i, string::f("Stage %d Pulse 2", i + 1));
			configButton(STAGE_SKIP_PARAM + i, string::f("Stage %d Skip", i + 1));
			configButton(STAGE_TRANGE_PARAM + i, string::f(
				"Stage %d Time Range (TR1 .002-.03s / TR2 .02-.3s / TR3 .2-3s Default / TR4 2-30s)", i + 1));
			configButton(STAGE_MUTE_PARAM + i, string::f("Stage %d Mute (suppresses Pulse1/2 output only)", i + 1));
		}
		configButton(RUN_A_PARAM, "Section A Start/Stop");
		configButton(RUN_B_PARAM, "Section B Start/Stop");
		configButton(MODE_A_PARAM, "Section A Mode (Forward/Reverse)");
		configButton(PENDULUM_A_PARAM, "Section A Pendulum (on/off)");
		configButton(MODE_B_PARAM, "Section B Mode (Forward/Reverse)");
		configButton(PENDULUM_B_PARAM, "Section B Pendulum (on/off)");

		// Stage Address: Adress-Knob 0-15 mit Rastung (Internal-Modus),
		// Int/Ext- und Continuous-Toggle, sowie Strobe/Reset als Momentary.
		configParam(STAGE_ADDR_KNOB_A_PARAM, 0.f, NUM_STAGES - 1, 0.f, "Section A Stage Address")->snapEnabled = true;
		configButton(STAGE_ADDR_MODE_A_PARAM, "Section A Stage Address Source (Int/Ext)");
		configButton(STAGE_ADDR_CONT_A_PARAM, "Section A Stage Address Continuous");
		configButton(STAGE_ADDR_STROBE_A_PARAM, "Section A Stage Address Strobe");
		configButton(STAGE_ADDR_RESET_A_PARAM, "Section A Stage Address Reset");
		configParam(STAGE_ADDR_KNOB_B_PARAM, 0.f, NUM_STAGES - 1, 0.f, "Section B Stage Address")->snapEnabled = true;
		configButton(STAGE_ADDR_MODE_B_PARAM, "Section B Stage Address Source (Int/Ext)");
		configButton(STAGE_ADDR_CONT_B_PARAM, "Section B Stage Address Continuous");
		configButton(STAGE_ADDR_STROBE_B_PARAM, "Section B Stage Address Strobe");
		configButton(STAGE_ADDR_RESET_B_PARAM, "Section B Stage Address Reset");

		configButton(CLEAR_PARAM, "Global Clear (all stages)");
		configButton(RESET_VALUES_PARAM, "Reset Voltage+Time to default (all stages)");
		configButton(AUTOFILL_PARAM, "Auto-Fill linear 0-10V (all stages, sets Range to Full)");

		configButton(LOAD_PARAM, "Preset Load (arm, then press 1-12)");
		configButton(SAVE_PARAM, "Preset Save (arm, then press 1-12)");
		for (int p = 0; p < 12; p++) {
			configButton(PRESET_PARAM + p, string::f("Preset %d", p + 1));
		}
		configButton(SCALE_PARAM, "Quantize Scale (arm, then press 10=Major/11=Minor/12=Chromatic)");
		configButton(KEY_PARAM, "Quantize Key (arm, then press 1-12 = C..B)");

		// Time Multiplier: Hauptknob 0.5x-4.0x (Default 1.0x, siehe Manual
		// "scaled from half-time to four times longer"), Attenuverter
		// bipolar -1..+1 fuer den zugehoerigen CV-Eingang.
		// V/Oct-Tempo-Semantik (BPM-Feature, Breaking Change - siehe
		// TEMPO_BASE_BPM-Kommentar in Marf248.hpp): 0V = 120 BPM,
		// exponentiell, +-5V Knob-Spannweite (Kinovice-Entscheidung).
		configParam<TempoQuantity>(TIME_MULT_A_PARAM, -5.f, 5.f, 0.f, "Section A Tempo");
		configParam(TIME_MULT_ATTEN_A_PARAM, -1.f, 1.f, 0.f, "Section A Tempo CV Attenuverter");
		configParam<TempoQuantity>(TIME_MULT_B_PARAM, -5.f, 5.f, 0.f, "Section B Tempo");
		configParam(TIME_MULT_ATTEN_B_PARAM, -1.f, 1.f, 0.f, "Section B Tempo CV Attenuverter");
		// BEATS_FACTOR (BPM-Feature): freier Wertebereich statt Raster-
		// Snap (Kinovice-Entscheidung), Default 4 (16-Stage-Zyklus = 1
		// Takt zu 4 Beats bei linearer Stage-Verteilung, gaengigster
		// Anwendungsfall). Siehe process() fuer die Rueckrechnung auf den
		// tatsaechlichen Time-Multiplier.
		configParam(BEATS_FACTOR_A_PARAM, 0.1f, 64.f, 4.f, "Section A Beats per Cycle");
		configParam(BEATS_FACTOR_B_PARAM, 0.1f, 64.f, 4.f, "Section B Beats per Cycle");

		configInput(START_A_INPUT, "Section A Start trigger");
		configInput(STOP_A_INPUT, "Section A Stop trigger");
		configInput(START_B_INPUT, "Section B Start trigger");
		configInput(STOP_B_INPUT, "Section B Stop trigger");
		configInput(EXT_A_INPUT, "External CV A");
		configInput(EXT_B_INPUT, "External CV B");
		configInput(EXT_C_INPUT, "External CV C");
		configInput(EXT_D_INPUT, "External CV D");
		configInput(STAGE_ADDR_EXT_A_INPUT, "Section A Stage Address CV (poly, ch. 1-4 = channel A-D, mono cable = channel A)");
		configInput(STAGE_ADDR_EXT_B_INPUT, "Section B Stage Address CV (poly, ch. 1-4 = channel A-D, mono cable = channel A)");
		configInput(STAGE_ADDR_EXT_CHANNEL_A_INPUT, "Section A Stage Address channel select CV (0-10V = A-D, unpatched = A)");
		configInput(STAGE_ADDR_EXT_CHANNEL_B_INPUT, "Section B Stage Address channel select CV (0-10V = A-D, unpatched = A)");
		configInput(TIME_MULT_CV_A_INPUT, "Section A Tempo CV (V/Oct)");
		configInput(TIME_MULT_CV_B_INPUT, "Section B Tempo CV (V/Oct)");
		configInput(BEATS_FACTOR_CV_A_INPUT, "Section A Beats per Cycle CV");
		configInput(BEATS_FACTOR_CV_B_INPUT, "Section B Beats per Cycle CV");

		configOutput(VOUT_A_OUTPUT, "Section A Voltage");
		// Name/Beschreibung praezisiert (Manual "Reference Output" - siehe
		// Kommentar bei TIMESTATIC_A/B_OUTPUT oben): abfallende Rampe ueber
		// die Stage-Zeit, kein statischer Wert.
		configOutput(TOUT_A_OUTPUT, "Section A Reference (interval ramp)");
		configOutput(APOUT_A_OUTPUT, "Section A All pulses");
		configOutput(P1OUT_A_OUTPUT, "Section A Programmed Pulse 1");
		configOutput(P2OUT_A_OUTPUT, "Section A Programmed Pulse 2");
		configOutput(VOUT_B_OUTPUT, "Section B Voltage");
		configOutput(TOUT_B_OUTPUT, "Section B Reference (interval ramp)");
		configOutput(APOUT_B_OUTPUT, "Section B All pulses");
		configOutput(P1OUT_B_OUTPUT, "Section B Programmed Pulse 1");
		configOutput(P2OUT_B_OUTPUT, "Section B Programmed Pulse 2");
		// Time Output (Manual "Outputs and Indicators"): statische CV
		// proportional zur Interval-Time-Sliderposition der aktuell
		// adressierten Stage - siehe Kommentar bei TIMESTATIC_A/B_OUTPUT.
		configOutput(TIMESTATIC_A_OUTPUT, "Section A Time (static, proportional to slider)");
		configOutput(TIMESTATIC_B_OUTPUT, "Section B Time (static, proportional to slider)");

		stageModifier[0] |= MOD_FIRST;
		stageModifier[NUM_STAGES - 1] |= MOD_LAST;
	}

	// VCV Rack Rechtsklick-Menue "Initialize" ruft dies auf. Im Gegensatz
	// zum hardwaregetreuen Global Clear (CLEAR_PARAM, folgt bewusst nur der
	// im Manual beschriebenen Teilmenge: Output Pulses, Voltage-
	// Programmierung, Operating Mode, Time Range) soll Initialize
	// tatsaechlich ALLES auf den Zustand eines frisch geladenen Moduls
	// zuruecksetzen - inklusive Mode/Pendulum, Stage Address, Presets und
	// Scale/Key, die von der Standard-Param-Reset-Logik nicht erfasst
	// werden, da sie nicht als Rack-Params sondern als eigene Member
	// gespeichert sind.
	void onReset(const ResetEvent& e) override {
		// Setzt zuerst alle regulaeren Params (Voltage/Time/Range/Curve,
		// Time Multiplier, Stage-Address-Knob, ...) auf ihre configParam-
		// Defaults zurueck - deckt alles ab, was schon ein simpler
		// Reglerwert ist.
		Module::onReset(e);

		for (int i = 0; i < NUM_STAGES; i++) {
			stageModifier[i] = 0;
			pickupVoltage[i].pending = false;
			pickupTime[i].pending = false;
			pickupRange[i].pending = false;
			pickupCurve[i].pending = false;
		}
		// Werkszustand wie im Konstruktor: Stage 1 = First, Stage 16 = Last.
		stageModifier[0] |= MOD_FIRST;
		stageModifier[NUM_STAGES - 1] |= MOD_LAST;

		// Beide AFG-Laufzeitzustaende komplett auf Default-Konstruktion
		// zuruecksetzen (Stage 1, gestoppt, kein Pendulum/Reverse, etc.) -
		// einfacher und sicherer als jedes Feld einzeln aufzuzaehlen.
		afgA = Afg();
		afgB = Afg();

		reverseA = false;
		pendulumOnA = false;
		reverseB = false;
		pendulumOnB = false;

		addrExtA = false;
		addrContOnA = false;
		addrExtB = false;
		addrContOnB = false;

		loadArmed = false;
		saveArmed = false;
		scaleArmed = false;
		keyArmed = false;
		currentKey = 0;
		currentScale = 2;

		for (int p = 0; p < 12; p++) {
			presets[p] = PresetSlot();
		}

		// Adress-Speicher: wie Presets nur bei "echtem" Initialize geleert,
		// NICHT beim performGlobalClear()-Live-Taster (siehe dort - der
		// laesst die Slots bewusst unangetastet).
		for (int i = 0; i < NUM_STAGES; i++) {
			addrMemA[i] = AddrMemorySlot();
			addrMemB[i] = AddrMemorySlot();
		}
	}

	// Speichert die aktuelle Stage-Programmierung (rohe Reglerwerte, nicht
	// die aufgeloesten Pickup-Werte) in den angegebenen Slot.
	void savePreset(int slot) {
		for (int i = 0; i < NUM_STAGES; i++) {
			presets[slot].voltage[i] = params[STAGE_VOLTAGE_PARAM + i].getValue();
			presets[slot].time[i] = params[STAGE_TIME_PARAM + i].getValue();
			presets[slot].range[i] = params[STAGE_RANGE_PARAM + i].getValue();
			presets[slot].curve[i] = params[STAGE_CURVE_PARAM + i].getValue();
			presets[slot].modifier[i] = stageModifier[i];
		}
		presets[slot].used = true;
	}

	// Laedt einen Slot: Modifier-Bits (digitale Toggles, keine physische
	// Reglerposition) werden sofort uebernommen. Die vier kontinuierlichen
	// Regler (Voltage/Time/Range/Curve) werden NICHT direkt gesetzt - das
	// wuerde den physischen Reglerwert verspringen lassen. Stattdessen wird
	// pro Regler Pickup scharf gemacht (siehe PickupState::arm()); der
	// gespeicherte Wert wirkt erst, sobald der Regler ihn "eingeholt" hat.
	void loadPreset(int slot) {
		for (int i = 0; i < NUM_STAGES; i++) {
			stageModifier[i] = presets[slot].modifier[i];
			pickupVoltage[i].arm(presets[slot].voltage[i], params[STAGE_VOLTAGE_PARAM + i].getValue());
			pickupTime[i].arm(presets[slot].time[i], params[STAGE_TIME_PARAM + i].getValue());
			pickupRange[i].arm(presets[slot].range[i], params[STAGE_RANGE_PARAM + i].getValue());
			pickupCurve[i].arm(presets[slot].curve[i], params[STAGE_CURVE_PARAM + i].getValue());
		}
	}

	// Global Clear: setzt Modifier-Bits sowie Range/Curve aller Stages
	// zurueck. Ausgelagert, damit sowohl der Panel-Button als auch (spaeter)
	// ein externer Trigger vom Marf248TriggerExpander dieselbe Aktion
	// ausloesen koennen, ohne Logik zu duplizieren.
	void performGlobalClear() {
		for (int i = 0; i < NUM_STAGES; i++) {
			stageModifier[i] = 0;
			params[STAGE_RANGE_PARAM + i].setValue(0.f);
			params[STAGE_CURVE_PARAM + i].setValue(0.5f);
			// Ein eventuell noch ausstehendes Preset-Pickup wird verworfen,
			// sonst koennte ein alter Zielwert nach dem Clear noch
			// nachwirken, sobald der Regler zufaellig darueber hinweg
			// bewegt wird.
			pickupVoltage[i].pending = false;
			pickupTime[i].pending = false;
			pickupRange[i].pending = false;
			pickupCurve[i].pending = false;
		}
		// Bugfix (Session August 2026): Werkszustand First/Last sofort wieder
		// setzen, analog zu onReset()/Konstruktor. Ohne dies blieb
		// stageModifier[] komplett leer (0 = kein First/Last-Bit irgendwo),
		// was Pendulum dauerhaft kaputt machte: Forward/Reverse haben einen
		// Array-Grenzen-Fallback (next >= NUM_STAGES) und wrappen trotzdem
		// korrekt, Pendulum aber haengt ausschliesslich an wasFirst/wasLast
		// (siehe Afg::advanceStage() in Marf248.hpp) und blieb ohne jeden
		// Turnaround-Punkt einfach an Stage 16 (rack::clamp-Obergrenze)
		// haengen - betrifft beide Sections gleichzeitig, da stageModifier
		// modulweit geteilt ist (nicht pro Section A/B). Als Nebeneffekt
		// waren davor auch die Start-/End-of-Cycle-Ausgaenge am Trigger-
		// Expander nach Global Clear stumm, da sie auf denselben Bits
		// basieren.
		stageModifier[0] |= MOD_FIRST;
		stageModifier[NUM_STAGES - 1] |= MOD_LAST;
		clearFlash.trigger(0.15f);
	}

	// Reset Values: setzt nur Voltage/Time aller Stages auf Default zurueck.
	// Ebenfalls ausgelagert (siehe performGlobalClear()).
	void performResetValues() {
		for (int i = 0; i < NUM_STAGES; i++) {
			params[STAGE_VOLTAGE_PARAM + i].setValue(0.f);
			// 0.5 = Mitte der normalisierten Time-Range-Position (siehe
			// configParam-Umstellung), nicht mehr die alte feste 0.1s.
			params[STAGE_TIME_PARAM + i].setValue(0.5f);
			pickupVoltage[i].pending = false;
			pickupTime[i].pending = false;
		}
		resetValuesFlash.trigger(0.15f);
	}

	// Auto-Fill: setzt die 16 geteilten Voltage-Regler auf ein lineares
	// 0-10V-Raster (10/16-Schritte = 0.625V) und erzwingt Range=Full je Stage.
	// Zeit/Modifier-Bits bleiben unangetastet (analog zu performResetValues()).
	void performAutoFill() {
		for (int i = 0; i < NUM_STAGES; i++) {
			params[STAGE_VOLTAGE_PARAM + i].setValue(i * 10.f / NUM_STAGES);
			// 0.f = Range Full (siehe StageRangeQuantity / rangePos-Mapping).
			params[STAGE_RANGE_PARAM + i].setValue(0.f);
			pickupVoltage[i].pending = false;
			pickupRange[i].pending = false;
		}
		autoFillFlash.trigger(0.15f);
	}

	void process(const ProcessArgs& args) override {
		// Marf248TriggerExpander-Lookup: bewusst ganz an den Anfang von
		// process() gezogen (frueher stand dieser Block erst nach der Stage-
		// Address-Verarbeitung), da der Stage-Address-Block weiter unten den
		// SEXT-Trigger (externe Strobe-Pulse) bereits kennen muss, bevor
		// applyStageAddress() aufgerufen wird. Sucht links UND rechts, damit
		// der Expander auf beiden Seiten andocken kann.
		Marf248TriggerExpander* trigExp = dynamic_cast<Marf248TriggerExpander*>(getLeftExpander().module);
		if (!trigExp) {
			trigExp = dynamic_cast<Marf248TriggerExpander*>(getRightExpander().module);
		}

		// Global Clear: momentary Taster, loescht die Programmierung aller 16
		// Stages (Manual: Output Voltage -> Continuous/Full Range/Internal
		// Source, alle Operating-Mode-Modifier geloescht). Da unsere
		// Toggle-Modifier (Stop/Sustain/Enable/First/Last/Quantize/Sloped/
		// VoltExt/TimeExt/Shape) allesamt als Bits in stageModifier liegen,
		// genuegt hierfuer ein simples Nullen des Arrays. Range ist bei uns
		// bewusst als kontinuierlicher Drehschalter statt als gespeichertes
		// Bit umgesetzt (siehe StageRangeQuantity), daher muss der Knob-Wert
        // selbst zusaetzlich auf 0 (Full) zurueckgesetzt werden, sonst
		// wuerde die naechste process()-Iteration den Range-Zustand aus der
		// unveraenderten Reglerposition sofort wieder herstellen. Der
		// Curve-Trimmer ist eine reine Port-Erweiterung ohne Manual-
		// Entsprechung; wir setzen ihn hier trotzdem mit auf die neutrale
		// Mittelstellung zurueck, im Sinne von "alles zurueck auf Default".
		// Nicht abgedeckt (im Port noch nicht implementiert): Output Pulses
		// 1/2 und Time Range - siehe Projektnotizen.
		if (clearTrigger.process(params[CLEAR_PARAM].getValue())) {
			performGlobalClear();
		}
		lights[CLEAR_LIGHT].setBrightness(clearFlash.process(args.sampleTime) ? 1.f : 0.f);

		// Reset Values (reine Software-Erweiterung, siehe RESET_VALUES_PARAM):
		// setzt NUR die Voltage/Time-Regler aller 16 Stages auf ihren
		// configParam-Default zurueck (0V bzw. 0.1s) - im Unterschied zu
		// Global Clear bleiben Modifier-Bits, Range und Curve unangetastet.
		// Praktisch zum schnellen Zuruecksetzen waehrend des Experimentierens,
		// ohne die gerade aufgebaute Stage-Programmierung (Stop/Sustain/
		// Quantize/...) zu verlieren.
		if (resetValuesTrigger.process(params[RESET_VALUES_PARAM].getValue())) {
			performResetValues();
		}
		lights[RESET_VALUES_LIGHT].setBrightness(resetValuesFlash.process(args.sampleTime) ? 1.f : 0.f);

		// Auto-Fill (siehe AUTOFILL_PARAM/performAutoFill()).
		if (autoFillTrigger.process(params[AUTOFILL_PARAM].getValue())) {
			performAutoFill();
		}
		lights[AUTOFILL_LIGHT].setBrightness(autoFillFlash.process(args.sampleTime) ? 1.f : 0.f);

		// Preset-System: LOAD/SAVE "scharf schalten", danach entscheidet der
		// naechste Druck auf einen der 12 Slot-Taster ueber Laden/Speichern.
		// Muss vor der Modifier-Buttons-Schleife unten laufen, da ein Load
		// stageModifier[] direkt ueberschreibt und die Schleife die (dann
		// schon aktuellen) Bits fuer die LEDs ausliest.
		// Vier Arm-Modi teilen sich dieselben 12 Preset-Taster (Load/Save/
		// Scale/Key) und schliessen sich gegenseitig aus - Einschalten eines
		// Modus deaktiviert automatisch alle anderen drei.
		if (loadTrigger.process(params[LOAD_PARAM].getValue())) {
			loadArmed = !loadArmed;
			if (loadArmed) {
				saveArmed = false;
				scaleArmed = false;
				keyArmed = false;
			}
		}
		if (saveTrigger.process(params[SAVE_PARAM].getValue())) {
			saveArmed = !saveArmed;
			if (saveArmed) {
				loadArmed = false;
				scaleArmed = false;
				keyArmed = false;
			}
		}
		if (scaleTrigger.process(params[SCALE_PARAM].getValue())) {
			scaleArmed = !scaleArmed;
			if (scaleArmed) {
				loadArmed = false;
				saveArmed = false;
				keyArmed = false;
			}
		}
		if (keyTrigger.process(params[KEY_PARAM].getValue())) {
			keyArmed = !keyArmed;
			if (keyArmed) {
				loadArmed = false;
				saveArmed = false;
				scaleArmed = false;
			}
		}
		lights[LOAD_LIGHT].setBrightness(loadArmed ? 1.f : 0.f);
		lights[SAVE_LIGHT].setBrightness(saveArmed ? 1.f : 0.f);
		lights[SCALE_LIGHT].setBrightness(scaleArmed ? 1.f : 0.f);
		lights[KEY_LIGHT].setBrightness(keyArmed ? 1.f : 0.f);

		for (int p = 0; p < 12; p++) {
			if (presetTrigger[p].process(params[PRESET_PARAM + p].getValue())) {
				if (saveArmed) {
					savePreset(p);
					presetFlash[p].trigger(0.2f);
					saveArmed = false;
				}
				else if (loadArmed) {
					// Geschuetzter Leer-Slot (siehe Projektentscheidung): ein nie
					// gespeicherter Slot wird beim Laden ignoriert, statt alle
					// 16 Stages versehentlich auf Default zurueckzusetzen. Kurzer
					// Doppel-Blitz-Ersatz durch etwas kuerzere Flash-Dauer als
					// Feedback "hier ist nichts gespeichert".
					if (presets[p].used) {
						loadPreset(p);
						presetFlash[p].trigger(0.2f);
					}
					else {
						presetFlash[p].trigger(0.06f);
					}
					loadArmed = false;
				}
				else if (keyArmed) {
					// Key: alle 12 Taster gueltig, direkte Abbildung 1-12 = C..B.
					currentKey = p;
					presetFlash[p].trigger(0.2f);
					keyArmed = false;
				}
				else if (scaleArmed) {
					// Scale: nur Taster 10/11/12 (Index 9/10/11) sind gueltig
					// (Major/Minor/Chromatic). Ein Druck auf 1-9 waehrend
					// Scale scharf ist wird mit einem kurzen Blitz quittiert,
					// aendert aber nichts (Feedback "ungueltige Taste").
					if (p >= 9 && p <= 11) {
						currentScale = p - 9;
						presetFlash[p].trigger(0.2f);
					}
					else {
						presetFlash[p].trigger(0.06f);
					}
					scaleArmed = false;
				}
			}
			bool flashOn = presetFlash[p].process(args.sampleTime);
			lights[PRESET_LIGHT + p].setBrightness(flashOn ? 1.f : (presets[p].used ? 0.12f : 0.f));
		}

		// Klick-Buttons pro Stage: jeder Druck togglet das jeweilige Bit,
		// gleiches Prinzip wie beim Start/Stop-Button von Section A/B.
		// Dezimiert per stageButtonDivider (siehe Deklaration oben) - Taster-
		// Erkennung und LED-Helligkeit muessen nicht mit voller Sample-Rate
		// laufen, das war laut perf-Profiling der dominante CPU-Kostenblock.
		// Ergebnis einmal einfangen statt stageButtonDivider.process() ein
		// zweites Mal weiter unten aufzurufen (wuerde den Zaehler doppelt
		// voranschreiten lassen) - wird auch fuer die dekimierte BPM-
		// Anzeige-Aktualisierung im Time-Multiplier-Block wiederverwendet.
		bool stageButtonsDecimated = stageButtonDivider.process();
		if (stageButtonsDecimated) {
		for (int i = 0; i < NUM_STAGES; i++) {
			if (stageFirstTrigger[i].process(params[STAGE_FIRST_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_FIRST;
			}
			if (stageLastTrigger[i].process(params[STAGE_LAST_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_LAST;
			}
			if (stageStopTrigger[i].process(params[STAGE_STOP_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_STOP;
			}
			if (stageSustainTrigger[i].process(params[STAGE_SUSTAIN_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_SUSTAIN;
			}
			if (stageEnableTrigger[i].process(params[STAGE_ENABLE_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_ENABLE;
			}
			if (stageSlopeTrigger[i].process(params[STAGE_SLOPE_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_SLOPED;
			}
			if (stageQuantTrigger[i].process(params[STAGE_QUANT_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_QUANT_OCT;
			}
			if (stageQuantGlideTrigger[i].process(params[STAGE_QUANTGLIDE_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_QUANT_GLIDE;
			}
			// Drehschalter statt Cycle-Button: die Position (0-6) codiert den
			// Range-Zustand direkt und wird jeden Frame in die entsprechenden
			// stageModifier-Bits uebersetzt (0=Full, 1=Half, 2..6=Limited +0..+8).
			// Anders als bei den Toggle-Buttons oben braucht es hier keinen
			// SchmittTrigger, da der Reglerwert selbst schon der Zustand ist.
			{
				stageModifier[i] &= ~(MOD_RANGE_HALF | MOD_RANGE_LIMITED | RANGE_OFFSET_MASK);
				// Pickup: solange ein per Preset geladener Range-Zielwert noch
				// nicht vom physischen Regler "eingeholt" wurde, zaehlt der
				// Zielwert statt der aktuellen Reglerposition (siehe
				// loadPreset()/PickupState).
				float rawRange = params[STAGE_RANGE_PARAM + i].getValue();
				float effRange = pickupRange[i].resolve(rawRange);
				int rangePos = (int) std::round(effRange);
				// Range-CV-Override (Trigger-Expander, STAGE_RANGE_POLY_INPUT,
				// Session "Range CV Override"): identisches isConnected()-Guard-
				// Prinzip wie alle anderen Stage-Modifier-Poly-Overrides -
				// wirkt nur, solange die Buchse gepatcht ist, und hat dann
				// Vorrang vor Reglerposition/Pickup, noch im selben Frame.
				// Ohne Kabel bleibt exakt das bisherige Verhalten (Regler +
				// Pickup) unveraendert.
				if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_RANGE_POLY_INPUT).isConnected()) {
					float rangeCv = trigExp->getInput(Marf248TriggerExpander::STAGE_RANGE_POLY_INPUT).getPolyVoltage(i);
					rangePos = resolveRangeZone(rangeCv);
				}
				if (rangePos == 1) {
					stageModifier[i] |= MOD_RANGE_HALF;
				}
				else if (rangePos >= 2) {
					stageModifier[i] |= MOD_RANGE_LIMITED;
					stageModifier[i] = setRangeOffsetIndex(stageModifier[i], rangePos - 2);
				}
			}
			if (stageVoltExtTrigger[i].process(params[STAGE_VOLTEXT_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_VOLT_EXT;
			}
			if (stageTimeExtTrigger[i].process(params[STAGE_TIMEEXT_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_TIME_EXT;
			}
			if (stagePulse1Trigger[i].process(params[STAGE_PULSE1_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_PULSE1;
			}
			if (stagePulse2Trigger[i].process(params[STAGE_PULSE2_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_PULSE2;
			}
			if (stageSkipTrigger[i].process(params[STAGE_SKIP_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_SKIP;
			}
			if (stageMuteTrigger[i].process(params[STAGE_MUTE_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_MUTE;
			}
			if (stageShapeTrigger[i].process(params[STAGE_SHAPE_PARAM + i].getValue())) {
				// Zyklus: Linear -> Exponentiell -> Logarithmisch -> Linear.
				// Beide Bits sind exklusiv, daher immer erst beide loeschen.
				if (stageModifier[i] & MOD_SLEW_EXP) {
					stageModifier[i] &= ~MOD_SLEW_EXP;
					stageModifier[i] |= MOD_SLEW_LOG;
				}
				else if (stageModifier[i] & MOD_SLEW_LOG) {
					stageModifier[i] &= ~MOD_SLEW_LOG;
				}
				else {
					stageModifier[i] |= MOD_SLEW_EXP;
				}
			}
			if (stageNormalizeTrigger[i].process(params[STAGE_NORMALIZE_PARAM + i].getValue())) {
				stageModifier[i] ^= MOD_SLEW_NORMALIZE;
			}
			if (stageTrangeTrigger[i].process(params[STAGE_TRANGE_PARAM + i].getValue())) {
				// Zyklus TR1 -> TR2 -> TR3 (Default, kein Bit) -> TR4 -> TR1.
				if (stageModifier[i] & MOD_TRANGE_1) {
					stageModifier[i] &= ~MOD_TRANGE_1;
					stageModifier[i] |= MOD_TRANGE_2;
				}
				else if (stageModifier[i] & MOD_TRANGE_2) {
					stageModifier[i] &= ~MOD_TRANGE_2;
					// -> TR3: keines der drei Bits gesetzt.
				}
				else if (stageModifier[i] & MOD_TRANGE_4) {
					stageModifier[i] &= ~MOD_TRANGE_4;
					stageModifier[i] |= MOD_TRANGE_1;
				}
				else {
					// war TR3 (kein Bit gesetzt) -> TR4.
					stageModifier[i] |= MOD_TRANGE_4;
				}
			}
			lights[STAGE_FIRST_LIGHT + i].setBrightness(stageModifier[i] & MOD_FIRST ? 1.f : 0.f);
			lights[STAGE_LAST_LIGHT + i].setBrightness(stageModifier[i] & MOD_LAST ? 1.f : 0.f);
			lights[STAGE_STOP_LIGHT + i].setBrightness(stageModifier[i] & MOD_STOP ? 1.f : 0.f);
			lights[STAGE_SUSTAIN_LIGHT + i].setBrightness(stageModifier[i] & MOD_SUSTAIN ? 1.f : 0.f);
			lights[STAGE_ENABLE_LIGHT + i].setBrightness(stageModifier[i] & MOD_ENABLE ? 1.f : 0.f);
			lights[STAGE_SLOPE_LIGHT + i].setBrightness(stageModifier[i] & MOD_SLOPED ? 1.f : 0.f);
			lights[STAGE_QUANT_LIGHT + i].setBrightness(stageModifier[i] & MOD_QUANT_OCT ? 1.f : 0.f);
			lights[STAGE_QUANTGLIDE_LIGHT + i].setBrightness(stageModifier[i] & MOD_QUANT_GLIDE ? 1.f : 0.f);
			lights[STAGE_SHAPE_EXP_LIGHT + i].setBrightness(stageModifier[i] & MOD_SLEW_EXP ? 1.f : 0.f);
			lights[STAGE_SHAPE_LOG_LIGHT + i].setBrightness(stageModifier[i] & MOD_SLEW_LOG ? 1.f : 0.f);
			lights[STAGE_NORMALIZE_LIGHT + i].setBrightness(stageModifier[i] & MOD_SLEW_NORMALIZE ? 1.f : 0.f);
			lights[STAGE_VOLTEXT_LIGHT + i].setBrightness(stageModifier[i] & MOD_VOLT_EXT ? 1.f : 0.f);
			lights[STAGE_TIMEEXT_LIGHT + i].setBrightness(stageModifier[i] & MOD_TIME_EXT ? 1.f : 0.f);
			lights[STAGE_PULSE1_LIGHT + i].setBrightness(stageModifier[i] & MOD_PULSE1 ? 1.f : 0.f);
			lights[STAGE_PULSE2_LIGHT + i].setBrightness(stageModifier[i] & MOD_PULSE2 ? 1.f : 0.f);
			lights[STAGE_SKIP_LIGHT + i].setBrightness(stageModifier[i] & MOD_SKIP ? 1.f : 0.f);
			lights[STAGE_MUTE_LIGHT + i].setBrightness(stageModifier[i] & MOD_MUTE ? 1.f : 0.f);
		}
		}

		// Trigger-Expander Stage-Modifier-Poly-CV-Overrides (Skip, First,
		// Last, Stop, Sustain, Enable, Pulse1, Pulse2, Mute, Ratchet Enable,
		// Slope, Quantized Glide, VoltExt/TimeExt-Kanalwahl, Ratchet Count,
		// Ratchet Offset) - Session "CPU-Optimierung Stage-Modifier-Scans"
		// (CPU-Audit-Ergebnis: 16 vorher einzelne, jeweils undekimierte
		// Bloecke waren der dominante Anteil der 3% Hauptmodul-Last, siehe
		// stageCvOverrideDivider-Kommentar oben). Jetzt zu EINER
		// gemeinsamen, dekimierten Schleife zusammengefasst:
		// - Alle beteiligten Input-Referenzen werden EINMAL vor der
		//   Schleife geholt (trigExp->getInput() ist zwar an sich billig,
		//   wurde aber vorher pro Block bis zu 17x/Sample erneut
		//   aufgerufen statt gecacht - gleiches Muster wie beim CV-
		//   Expander weiter unten, siehe cvExp/cvTimeInput etc.).
		// - isConnected() wird ebenfalls einmal pro Divider-Tick gecacht
		//   statt pro Stage-Iteration erneut abgefragt.
		// - Fuer die reinen Bit-Toggle-Modifier (Skip...QuantGlide) gilt
		//   weiterhin: ohne Kabel bleibt der zuletzt per Klick gesetzte
		//   Zustand erhalten (kein else-Zweig - stageModifier[i] wird nur
		//   bei gepatchtem Kabel angefasst, identisches Verhalten wie
		//   vorher).
		// - Fuer die vier reinen CV-Ableitungen ohne "letzten Panel-
		//   Klick"-Fallback (voltExtChannelOverride/timeExtChannelOverride/
		//   stageRatchetCount/stageRatchetOffset) gilt weiterhin: ohne
		//   Kabel Default (-1/-1/0/0.f) - dafuer jetzt ein expliziter
		//   else-Zweig, da diese Arrays seit dieser Session Member sind
		//   (siehe dortiger Kommentar) und sich sonst nicht mehr von
		//   selbst zuruecksetzen wuerden.
		if (stageCvOverrideDivider.process()) {
			Input* skipIn = nullptr;
			Input* firstIn = nullptr;
			Input* lastIn = nullptr;
			Input* stopIn = nullptr;
			Input* sustainIn = nullptr;
			Input* enableIn = nullptr;
			Input* pulse1In = nullptr;
			Input* pulse2In = nullptr;
			Input* muteIn = nullptr;
			Input* ratchetIn = nullptr;
			Input* slopeIn = nullptr;
			Input* quantGlideIn = nullptr;
			Input* voltExtChIn = nullptr;
			Input* timeExtChIn = nullptr;
			Input* ratchetCountIn = nullptr;
			Input* ratchetOffsetIn = nullptr;

			bool skipConn = false, firstConn = false, lastConn = false, stopConn = false;
			bool sustainConn = false, enableConn = false, pulse1Conn = false, pulse2Conn = false;
			bool muteConn = false, ratchetConn = false, slopeConn = false, quantGlideConn = false;
			bool voltExtChConn = false, timeExtChConn = false, ratchetCountConn = false, ratchetOffsetConn = false;

			if (trigExp) {
				skipIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_SKIP_POLY_INPUT);
				firstIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_FIRST_POLY_INPUT);
				lastIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_LAST_POLY_INPUT);
				stopIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_STOP_POLY_INPUT);
				sustainIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_SUSTAIN_POLY_INPUT);
				enableIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_ENABLE_POLY_INPUT);
				pulse1In = &trigExp->getInput(Marf248TriggerExpander::STAGE_PULSE1_POLY_INPUT);
				pulse2In = &trigExp->getInput(Marf248TriggerExpander::STAGE_PULSE2_POLY_INPUT);
				muteIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_MUTE_POLY_INPUT);
				ratchetIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_RATCHET_POLY_INPUT);
				slopeIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_SLOPE_POLY_INPUT);
				quantGlideIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_QUANTGLIDE_POLY_INPUT);
				voltExtChIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_VOLTEXT_CH_POLY_INPUT);
				timeExtChIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_TIMEEXT_CH_POLY_INPUT);
				ratchetCountIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_RATCHET_COUNT_POLY_INPUT);
				ratchetOffsetIn = &trigExp->getInput(Marf248TriggerExpander::STAGE_RATCHET_OFFSET_POLY_INPUT);

				skipConn = skipIn->isConnected();
				firstConn = firstIn->isConnected();
				lastConn = lastIn->isConnected();
				stopConn = stopIn->isConnected();
				sustainConn = sustainIn->isConnected();
				enableConn = enableIn->isConnected();
				pulse1Conn = pulse1In->isConnected();
				pulse2Conn = pulse2In->isConnected();
				muteConn = muteIn->isConnected();
				ratchetConn = ratchetIn->isConnected();
				slopeConn = slopeIn->isConnected();
				quantGlideConn = quantGlideIn->isConnected();
				voltExtChConn = voltExtChIn->isConnected();
				timeExtChConn = timeExtChIn->isConnected();
				ratchetCountConn = ratchetCountIn->isConnected();
				ratchetOffsetConn = ratchetOffsetIn->isConnected();
			}

			const float GATE_THRESHOLD = 2.f;

			for (int i = 0; i < NUM_STAGES; i++) {
				if (skipConn) {
					bool g = skipIn->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_SKIP; else stageModifier[i] &= ~MOD_SKIP;
					lights[STAGE_SKIP_LIGHT + i].setBrightness(g ? 1.f : 0.f);
				}
				if (firstConn) {
					bool g = firstIn->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_FIRST; else stageModifier[i] &= ~MOD_FIRST;
					lights[STAGE_FIRST_LIGHT + i].setBrightness(g ? 1.f : 0.f);
				}
				if (lastConn) {
					bool g = lastIn->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_LAST; else stageModifier[i] &= ~MOD_LAST;
					lights[STAGE_LAST_LIGHT + i].setBrightness(g ? 1.f : 0.f);
				}
				if (stopConn) {
					bool g = stopIn->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_STOP; else stageModifier[i] &= ~MOD_STOP;
					lights[STAGE_STOP_LIGHT + i].setBrightness(g ? 1.f : 0.f);
				}
				if (sustainConn) {
					bool g = sustainIn->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_SUSTAIN; else stageModifier[i] &= ~MOD_SUSTAIN;
					lights[STAGE_SUSTAIN_LIGHT + i].setBrightness(g ? 1.f : 0.f);
				}
				if (enableConn) {
					bool g = enableIn->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_ENABLE; else stageModifier[i] &= ~MOD_ENABLE;
					lights[STAGE_ENABLE_LIGHT + i].setBrightness(g ? 1.f : 0.f);
				}
				if (pulse1Conn) {
					bool g = pulse1In->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_PULSE1; else stageModifier[i] &= ~MOD_PULSE1;
					lights[STAGE_PULSE1_LIGHT + i].setBrightness(g ? 1.f : 0.f);
				}
				if (pulse2Conn) {
					bool g = pulse2In->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_PULSE2; else stageModifier[i] &= ~MOD_PULSE2;
					lights[STAGE_PULSE2_LIGHT + i].setBrightness(g ? 1.f : 0.f);
				}
				if (muteConn) {
					bool g = muteIn->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_MUTE; else stageModifier[i] &= ~MOD_MUTE;
					lights[STAGE_MUTE_LIGHT + i].setBrightness(g ? 1.f : 0.f);
				}
				if (ratchetConn) {
					// Kein Light-Update (keine STAGE_RATCHET_LIGHT vorhanden,
					// siehe urspruenglicher Kommentar).
					bool g = ratchetIn->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_RATCHET; else stageModifier[i] &= ~MOD_RATCHET;
				}
				if (slopeConn) {
					bool g = slopeIn->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_SLOPED; else stageModifier[i] &= ~MOD_SLOPED;
					lights[STAGE_SLOPE_LIGHT + i].setBrightness(g ? 1.f : 0.f);
				}
				if (quantGlideConn) {
					bool g = quantGlideIn->getPolyVoltage(i) >= GATE_THRESHOLD;
					if (g) stageModifier[i] |= MOD_QUANT_GLIDE; else stageModifier[i] &= ~MOD_QUANT_GLIDE;
					lights[STAGE_QUANTGLIDE_LIGHT + i].setBrightness(g ? 1.f : 0.f);
				}

				// VOLT_EXT/TIME_EXT-Kanalwahl (5 Zonen a 2V, siehe
				// urspruenglicher Kommentar): reine CV-Ableitung ohne
				// "letzten Klick"-Fallback, daher explizites Zuruecksetzen
				// im else-Zweig (Arrays sind jetzt Member, siehe dortiger
				// Kommentar).
				if (voltExtChConn) {
					float v = rack::clamp(voltExtChIn->getPolyVoltage(i), 0.f, 10.f);
					int zone = (int) rack::clamp(v / 2.f, 0.f, 4.f);
					if (zone == 0) {
						stageModifier[i] &= ~MOD_VOLT_EXT;
						voltExtChannelOverride[i] = -1;
					}
					else {
						stageModifier[i] |= MOD_VOLT_EXT;
						voltExtChannelOverride[i] = zone - 1;
					}
					lights[STAGE_VOLTEXT_LIGHT + i].setBrightness(stageModifier[i] & MOD_VOLT_EXT ? 1.f : 0.f);
				}
				else {
					// Kein Kabel (mehr) gepatcht: nur der Kanal-Override
					// faellt zurueck auf -1 (kein Override) - das MOD_VOLT_EXT-
					// Bit selbst bleibt unberuehrt und folgt weiterhin dem
					// zuletzt per Klick gesetzten Zustand, exakt wie vorher
					// (siehe Kommentar an urspruenglicher Stelle).
					voltExtChannelOverride[i] = -1;
				}
				if (timeExtChConn) {
					float v = rack::clamp(timeExtChIn->getPolyVoltage(i), 0.f, 10.f);
					int zone = (int) rack::clamp(v / 2.f, 0.f, 4.f);
					if (zone == 0) {
						stageModifier[i] &= ~MOD_TIME_EXT;
						timeExtChannelOverride[i] = -1;
					}
					else {
						stageModifier[i] |= MOD_TIME_EXT;
						timeExtChannelOverride[i] = zone - 1;
					}
					lights[STAGE_TIMEEXT_LIGHT + i].setBrightness(stageModifier[i] & MOD_TIME_EXT ? 1.f : 0.f);
				}
				else {
					timeExtChannelOverride[i] = -1;
				}

				// Ratchet-Count/-Offset (siehe urspruenglicher Kommentar):
				// ebenfalls reine CV-Ableitungen, Default 0 bzw. 0.f ohne
				// gepatchtes Kabel.
				if (ratchetCountConn) {
					stageRatchetCount[i] = resolveRatchetCount(ratchetCountIn->getPolyVoltage(i));
				}
				else {
					stageRatchetCount[i] = 0;
				}
				if (ratchetOffsetConn) {
					stageRatchetOffset[i] = rack::clamp(ratchetOffsetIn->getPolyVoltage(i), 0.f, 10.f) / 10.f;
				}
				else {
					stageRatchetOffset[i] = 0.f;
				}
			}
		}

		float stageVoltage[NUM_STAGES];
		float stageTime[NUM_STAGES];
		float resolvedVoltage[NUM_STAGES];
		float resolvedTime[NUM_STAGES];
		float resolvedTimeA[NUM_STAGES];
		float resolvedTimeB[NUM_STAGES];
		// Ratchet-Warp-Blend (Session "Ratcheting"): 0..1, immer gueltig
		// (unabhaengig davon, ob Ratchet fuer eine Stage ueberhaupt aktiv
		// ist) - analoges Muster zu stageCurvePower, siehe Berechnung
		// weiter unten in der Stage-Schleife.
		float stageWarpBlend[NUM_STAGES];
		float stageCurvePower[NUM_STAGES];
		// Aufgeloester Curve-Knob (Trimpot + CV + globaler Bias, siehe
		// Stage-Schleife unten) - Session "CPU-Optimierung Stage-Resolution
		// (SIMD)": war vorher eine rein lokale Scope-Variable INNERHALB der
		// Schleife, muss jetzt als Array ueberleben, da die eigentliche
		// pow()-Berechnung (stageCurvePower) in einem SEPARATEN, der
		// Schleife nachgelagerten SIMD-Batch-Durchgang passiert (siehe dort)
		// statt wie bisher direkt inline pro Stage.
		float effectiveKnob[NUM_STAGES];

		// Vier externe CV-Eingaenge, einmal pro Frame gelesen (modulweit
		// geteilt, siehe InputId-Kommentar). isConnected() wird separat
		// gebraucht, da bei External Time ein unbelegter Kanal auf den
		// schnellsten Range-Wert zurueckfallen muss statt auf 0V.
		float extVoltage[4] = {
			inputs[EXT_A_INPUT].getVoltage(),
			inputs[EXT_B_INPUT].getVoltage(),
			inputs[EXT_C_INPUT].getVoltage(),
			inputs[EXT_D_INPUT].getVoltage(),
		};
		bool extConnected[4] = {
			inputs[EXT_A_INPUT].isConnected(),
			inputs[EXT_B_INPUT].isConnected(),
			inputs[EXT_C_INPUT].isConnected(),
			inputs[EXT_D_INPUT].isConnected(),
		};
		// Marf248CvExpander: gleicher "Host liest/schreibt direkt"-Ansatz wie
		// beim Trigger-Expander (siehe dortiger Scan weiter unten), aber hier
		// VOR der Stage-Schleife noetig, da die additive Stage-Time-CV schon
		// in resolvedTime[i] einfliessen muss, lange bevor der Trigger-
		// Expander-Block erreicht wird. Derselbe cvExp-Zeiger wird weiter
		// unten erneut verwendet, um die Gate-A/B-Outputs zu schreiben.
		Marf248CvExpander* cvExp = dynamic_cast<Marf248CvExpander*>(getLeftExpander().module);
		if (!cvExp) {
			cvExp = dynamic_cast<Marf248CvExpander*>(getRightExpander().module);
		}

		// Attenuverter/Offset/Input-Referenz fuer die additive Stage-Time-CV
		// einmalig ausgelesen (skalar, gilt fuer alle Stages gleich) - die
		// tatsaechliche Sekunden-Spanne fuer die CV-Skalierung ist jetzt PRO
		// STAGE variabel (haengt vom gewaehlten Time Range TR1-4 ab, siehe
		// MOD_TRANGE_*-Bits in Marf248.hpp), daher wird stageTimeCvAdd erst
		// INNERHALB der Stage-Schleife berechnet statt wie bisher vorab in
		// einem eigenen Loop mit einer einzigen festen Spanne.
		float cvAttenTime = 0.f, cvOffsetTime = 0.f;
		int cvTimeChannels = 0;
		Input* cvTimeInput = nullptr;
		if (cvExp) {
			cvAttenTime = cvExp->getParam(Marf248CvExpander::TIME_CV_ATTEN_PARAM).getValue();
			cvOffsetTime = cvExp->getParam(Marf248CvExpander::TIME_CV_OFFSET_PARAM).getValue();
			cvTimeInput = &cvExp->getInput(Marf248CvExpander::STAGE_TIME_CV_INPUT);
			cvTimeChannels = cvTimeInput->getChannels();
		}
		const float TIME_CV_SPAN_VOLTS = 5.f;

		// Slew-Curve-CV (analoges Muster zur Stage-Time-CV oben): Attenuverter
		// skaliert die eingehende Poly-CV, der globale Bias-Knob wirkt dagegen
		// UNABHAENGIG vom Verbindungsstatus - daher curveGlobalBias auch dann
		// auslesen, wenn (theoretisch) kein Kabel am CURVE_CV_INPUT haengt.
		// Fehlt der ganze Expander, bleiben beide Werte bei 0 (Default), die
		// Stage-Curve-Berechnung unten faellt dann automatisch auf reines
		// Trimpot-Verhalten zurueck (unveraendert wie bisher).
		float cvAttenCurve = 0.f, curveGlobalBias = 0.f;
		int curveCvChannels = 0;
		Input* curveCvInput = nullptr;
		if (cvExp) {
			cvAttenCurve = cvExp->getParam(Marf248CvExpander::CURVE_CV_ATTEN_PARAM).getValue();
			curveGlobalBias = cvExp->getParam(Marf248CvExpander::CURVE_GLOBAL_PARAM).getValue();
			curveCvInput = &cvExp->getInput(Marf248CvExpander::CURVE_CV_INPUT);
			curveCvChannels = curveCvInput->getChannels();
		}
		// Gleiche Konvention wie TIME_CV_SPAN_VOLTS: +-5V CV bei vollem
		// Attenuverter-Ausschlag deckt die komplette Curve-Knob-Spanne (0..1)
		// ab (Kinovice-Entscheidung, siehe Sessionverlauf).
		const float CURVE_CV_SPAN_VOLTS = 5.f;

		// Ratchet-Warp-Blend-CV (Session "Ratcheting", Nachtrag: jetzt
		// bipolar -1..+1 statt unipolar 0..1, siehe ratchetWarpPhase() in
		// Marf248.hpp): identisches Scan-Muster wie Curve-CV oben, WARP_
		// GLOBAL_PARAM ist jetzt genauso bipolar wie CURVE_GLOBAL_PARAM.
		float cvAttenWarp = 0.f, warpGlobalBias = 0.f;
		int warpCvChannels = 0;
		Input* warpCvInput = nullptr;
		if (cvExp) {
			cvAttenWarp = cvExp->getParam(Marf248CvExpander::WARP_CV_ATTEN_PARAM).getValue();
			warpGlobalBias = cvExp->getParam(Marf248CvExpander::WARP_GLOBAL_PARAM).getValue();
			warpCvInput = &cvExp->getInput(Marf248CvExpander::WARP_CV_INPUT);
			warpCvChannels = warpCvInput->getChannels();
		}
		// Gleiche Konvention wie CURVE_CV_SPAN_VOLTS: +-5V CV bei vollem
		// Attenuverter-Ausschlag deckt die komplette Blend-Spanne (-1..+1) ab.
		const float WARP_CV_SPAN_VOLTS = 5.f;

		for (int i = 0; i < NUM_STAGES; i++) {
			// Pickup: siehe loadPreset()/PickupState. Ausserhalb eines
			// ausstehenden Pickups liefert resolve() einfach den rohen
			// Reglerwert unveraendert zurueck (kein Verhaltensunterschied zu
			// vorher).
			stageVoltage[i] = pickupVoltage[i].resolve(params[STAGE_VOLTAGE_PARAM + i].getValue());
			// stageTime[i] ist jetzt eine normalisierte Position 0..1 (nicht
			// mehr direkt Sekunden, siehe configParam-Umstellung oben) - die
			// tatsaechliche Sekundenzahl ergibt sich erst weiter unten aus der
			// Kombination mit dem gewaehlten Time Range (TR1-4).
			stageTime[i] = pickupTime[i].resolve(params[STAGE_TIME_PARAM + i].getValue());

			// Time Range (TR1-4, MOD_TRANGE_1/2/4 in Marf248.hpp): bestimmt
			// die Sekunden-Spanne, auf die die normalisierte Time-Knob-
			// Position sowie External Time/CV-Expander-Skalierung abgebildet
			// werden - analog zu applyRange() fuer die Voltage-Seite, aber
			// hier direkt in Marf248::process() aufgeloest statt in
			// Afg::process() (Afg kennt ohnehin nur die fertige Sekundenzeit,
			// keine Aenderung an Afg/Marf248.hpp::Afg noetig).
			int trIdx = getTimeRangeIndex(stageModifier[i]);
			float timeRangeMin = TIME_RANGE_MIN[trIdx];
			float timeRangeMax = TIME_RANGE_MAX[trIdx];
			float timeRangeSpan = timeRangeMax - timeRangeMin;

			// Voltage Source External/Internal: bei External bestimmt die
			// Sliderposition (0-10V in 4 gleich grossen Zonen) welcher der
			// vier externen Kanaele durchgereicht wird - der Slider dient
			// dann als Kanalwahl statt als Pegel. Die Aufloesung passiert
			// hier, VOR Range/Quantize, die weiterhin unveraendert in
			// Afg::process()/applyRange() auf das Ergebnis wirken.
			if (stageModifier[i] & MOD_VOLT_EXT) {
				// Kanalwahl-CV (STAGE_VOLTEXT_CH_POLY_INPUT) hat Vorrang vor
				// der bisherigen Slider-Kanalwahl, siehe Override-Block
				// oben. voltExtChannelOverride[i] == -1 bedeutet "kein
				// Kabel gepatcht" - dann unveraendertes Verhalten.
				int ch = (voltExtChannelOverride[i] >= 0)
					? voltExtChannelOverride[i]
					: (int) rack::clamp(stageVoltage[i] / 2.5f, 0.f, 3.f);
				resolvedVoltage[i] = extVoltage[ch];
			}
			else {
				resolvedVoltage[i] = stageVoltage[i];
			}

			// Time Source External/Internal, analog zur Voltage Source.
			// Fehlt am gewaehlten Kanal ein Patchkabel, faellt die Interval
			// Time auf den schnellsten Wert des aktuellen Zeitbereichs
			// zurueck (so beschreibt es die Referenzdokumentation sinngemaess).
			if (stageModifier[i] & MOD_TIME_EXT) {
				// Wie beim Voltage-External-Modus dient die (jetzt bereits
				// normalisierte 0..1) Slider-Position selbst als Kanalwahl
				// (A-D) in 4 gleich grossen Zonen - keine Rueckrechnung ueber
				// TIME_PARAM_MIN/MAX mehr noetig, da stageTime[i] schon 0..1 ist.
				// Kanalwahl-CV (STAGE_TIMEEXT_CH_POLY_INPUT) hat auch hier
				// Vorrang, siehe Override-Block oben und Kommentar bei
				// MOD_VOLT_EXT oben.
				int ch = (timeExtChannelOverride[i] >= 0)
					? timeExtChannelOverride[i]
					: (int) rack::clamp(stageTime[i] * 4.f, 0.f, 3.f);
				if (extConnected[ch]) {
					float v = rack::clamp(extVoltage[ch], 0.f, 10.f);
					resolvedTime[i] = timeRangeMin + (v / 10.f) * timeRangeSpan;
				}
				else {
					resolvedTime[i] = timeRangeMin;
				}
			}
			else {
				resolvedTime[i] = timeRangeMin + stageTime[i] * timeRangeSpan;
			}

			// Additive Stage-Time-CV vom CV-Expander: +-1 Attenuverter-
			// Ausschlag bei +-5V CV deckt jetzt die Spanne des JEWEILS
			// gewaehlten Time Range ab (vorher fest TIME_PARAM_MIN..MAX) -
			// haelt die CV-Modulationstiefe konsistent zum sichtbaren Knob-
			// Verhalten in jedem Bereich. Offset-Trimpot wird VOR dem
			// Attenuverter von der rohen CV abgezogen - macht eine rein
			// unipolare Quelle (z.B. 0-10V Envelope) bipolar nutzbar (siehe
			// bisherige Begruendung, unveraendert).
			float stageTimeCvAdd = 0.f;
			if (cvExp && i < cvTimeChannels) {
				float cv = cvTimeInput->getVoltage(i) - cvOffsetTime;
				stageTimeCvAdd = cvAttenTime * (cv / TIME_CV_SPAN_VOLTS) * timeRangeSpan;
			}
			// Untere Schranke verhindert eine negative/Null-Zeit. Obere
			// Schranke jetzt 30s (TR4-Maximum, der langsamste aller vier
			// Bereiche) statt vorher fest 10s, da TR4 sonst abgeschnitten
			// wuerde.
			resolvedTime[i] = rack::clamp(resolvedTime[i] + stageTimeCvAdd, 0.001f, 30.f);

			// Knob 0..1, Mitte (0.5) = SLEW_CURVE_POWER (bisheriges Verhalten).
			// Der Exponent-Faktor selbst wandert quadratisch geglaettet von
			// CURVE_FACTOR_CENTER (um die Mitte, feine Regelung) zu
			// CURVE_FACTOR_EXTREME (an den Anschlaegen, ausgepraegter):
			//   Mitte  (knob=0.5) -> Power 2.5   (wie bisher)
			//   links  (knob=0)   -> Power 1.0   (linear, wie bisher)
			//   rechts (knob=1)   -> Power 15.6  (deutlich extremer als bisher)
			float knob = pickupCurve[i].resolve(params[STAGE_CURVE_PARAM + i].getValue());

			// Slew-Curve-CV + globaler Bias: beide addieren sich zusaetzlich
			// zur per-Stage Curve-Trimpot-Position (knob), BEVOR daraus die
			// Power berechnet wird. curveCvAdd wirkt nur pro Kanal, wenn
			// tatsaechlich ein Poly-Kabel mit ausreichend Kanaelen anliegt;
			// curveGlobalBias wirkt immer, auch ohne Kabel (siehe Scan oben).
			// Das nachfolgende Clamp auf 0..1 stellt sicher, dass CV/Bias die
			// Kurve nicht ueber die durch den Knob selbst erreichbaren
			// Extreme (linear bei 0, staerkste Kruemmung bei 1) hinaus
			// treiben koennen - die zulaessige Power-Spanne bleibt also
			// unangetastet.
			float curveCvAdd = 0.f;
			if (cvExp && i < curveCvChannels) {
				curveCvAdd = cvAttenCurve * (curveCvInput->getVoltage(i) / CURVE_CV_SPAN_VOLTS);
			}
			// Nur noch Aufloesung + Ablage im Array - die eigentliche, teure
			// pow()-Berechnung (inkl. Cache-Vergleich) passiert jetzt NICHT
			// mehr hier inline pro Stage, sondern gebuendelt in einem
			// separaten SIMD-Batch-Durchgang direkt nach dieser Schleife
			// (Session "CPU-Optimierung Stage-Resolution (SIMD)", siehe
			// dort fuer die Begruendung/Batch-Level-Cache-Logik).
			effectiveKnob[i] = rack::clamp(knob + curveCvAdd + curveGlobalBias, 0.f, 1.f);

			// Ratchet-Warp-Blend (Session "Ratcheting", Nachtrag: bipolar
			// -1..+1, siehe ratchetWarpPhase() in Marf248.hpp): additive CV +
			// globaler Bias, auf -1..+1 geklemmt. Bewusst KEIN Cache wie bei
			// stageCurvePower - hier wuerde ein Cache ohnehin nicht helfen:
			// ratchetTargetPhase() (inkl. zweier std::pow()-Aufrufe) wird
			// erst unten in Afg::process() aufgerufen, dort jeden Sample neu
			// (nicht nur bei den maximal 8 tatsaechlichen Feuer-Events pro
			// Stage-Aufenthalt), da rawPhase sich pro Sub-Trigger-Index
			// aendert. Zwei std::pow()-Aufrufe/Sample/Section bei aktivem
			// Ratchet sind aber vernachlaessigbar (vgl. bereits bestehende
			// Pro-Sample-Kosten wie applyRange()/quantizeToScale()).
			float warpCvAdd = 0.f;
			if (cvExp && i < warpCvChannels) {
				warpCvAdd = cvAttenWarp * (warpCvInput->getVoltage(i) / WARP_CV_SPAN_VOLTS);
			}
			stageWarpBlend[i] = rack::clamp(warpGlobalBias + warpCvAdd, -1.f, 1.f);
		}

		// Curve-Power-Batch-Vorberechnung (Session "CPU-Optimierung Stage-
		// Resolution (SIMD)"): std::pow() ist teuer (vorherige Session-Notiz:
		// 16x pro Sample bei 48kHz = ~768000 Aufrufe/Sekunde unskaliert).
		// Statt wie bisher pro Stage einzeln skalar zu pruefen/rechnen,
		// werden jetzt 4 benachbarte Stages gemeinsam per rack::simd::
		// float_4 batchweise verarbeitet (4 Iterationen fuer NUM_STAGES=16).
		//
		// Batch-Level-Cache (Kinovice-Entscheidung, Session-Design): der
		// bisherige Pro-Stage-Cache (lastCurveKnob/cachedCurvePower, siehe
		// Member-Deklaration oben) bleibt inhaltlich erhalten, die "hat sich
		// etwas geaendert"-Entscheidung wird aber nur noch PRO 4er-BATCH
		// getroffen (ODER ueber die 4 Einzel-Stage-Aenderungen) - aendert
		// sich mindestens eine der 4 Lanes, wird der komplette Batch per
		// SIMD neu gerechnet (bis zu 3 unveraenderte Lanes "umsonst" mit-
		// gerechnet), bleiben alle 4 unveraendert, wird der Batch komplett
		// uebersprungen - identisches Ruhezustand-Verhalten wie bisher, nur
		// auf 4er- statt Einzelstage-Granularitaet. Der Sentinel -1.f in
		// lastCurveKnob (siehe Konstruktor) erzwingt dadurch weiterhin ganz
		// von selbst eine initiale Berechnung im allerersten Sample (jeder
		// Batch ist dann per Definition dirty), ohne Sonderfall-Code.
		//
		// Bewusst in Kauf genommener Trade-off: im haeufigen Fall (mehrere/
		// alle Stages gleichzeitig moduliert, z.B. LFO auf mehrkanaliger
		// Curve-CV) gewinnt SIMD klar; im seltenen Einzelfall (genau eine
		// von vier Nachbar-Stages moduliert) wird geringfuegig mehr
		// gerechnet als beim reinen Pro-Stage-Cache noetig gewesen waere -
		// siehe Design-Diskussion dieser Session.
		//
		// rack::simd::pow(float, float_4) (Basis skalar, Exponent
		// vektoriell) ist in der Rack-SDK-simd-Bibliothek als
		// exp(b * log(a)) implementiert - guenstiger als der allgemeine
		// float_4/float_4-Fall, da log(a) nur einmal skalar (nicht pro
		// Lane) berechnet wird. SLEW_CURVE_POWER ist bei uns ein fixer
		// Compile-Time-Skalar (2.5f), genau dieser Fall.
		for (int b = 0; b < NUM_STAGES; b += 4) {
			bool batchDirty =
				(effectiveKnob[b]     != lastCurveKnob[b])
				|| (effectiveKnob[b + 1] != lastCurveKnob[b + 1])
				|| (effectiveKnob[b + 2] != lastCurveKnob[b + 2])
				|| (effectiveKnob[b + 3] != lastCurveKnob[b + 3]);
			if (batchDirty) {
				rack::simd::float_4 knob4 = rack::simd::float_4::load(&effectiveKnob[b]);
				// -1 links .. 0 Mitte .. +1 rechts, analog zur vorherigen
				// skalaren Fassung (siehe Git-Historie).
				rack::simd::float_4 bipolar4 = 2.f * knob4 - 1.f;
				rack::simd::float_4 factor4 = CURVE_FACTOR_CENTER
					+ (CURVE_FACTOR_EXTREME - CURVE_FACTOR_CENTER) * (bipolar4 * bipolar4);
				rack::simd::float_4 power4 = rack::simd::pow(SLEW_CURVE_POWER, factor4 * knob4);
				power4.store(&cachedCurvePower[b]);
				knob4.store(&lastCurveKnob[b]);
			}
		}
		for (int i = 0; i < NUM_STAGES; i++) {
			stageCurvePower[i] = cachedCurvePower[i];
		}

		// Time Multiplier / BPM-Feature (Manual-Grundlage "TIME MULTIPLIER",
		// jetzt erweitert um V/Oct-Tempo-Steuerung + BEATS_FACTOR-
		// Rueckrechnung, Breaking Change - siehe TEMPO_BASE_BPM-Kommentar
		// in Marf248.hpp): TIME_MULT_A/B_PARAM ist keine direkte Zeitfaktor-
		// Eingabe mehr, sondern eine V/Oct-Tempo-Spannung (0V = 120 BPM,
		// exponentiell). BEATS_FACTOR_A/B_PARAM legt fest, wie viele Beats
		// ein voller Zyklus ueber alle NICHT geskippten Stages darstellen
		// soll; daraus wird die gewuenschte Zykluszeit berechnet und
		// gegen die tatsaechliche (unskalierte) Summe der Stage-Zeiten
		// zurueckgerechnet -> effektiver Multiplier. Pendulum-Modus geht
		// bewusst NICHT automatisch mit doppelter Zykluslaenge ein
		// (Kinovice-Entscheidung, Option B): BEATS_FACTOR bezieht sich
		// immer nur auf einen Durchlauf (Hinweg); bei aktivem Pendulum
		// dauert ein voller Hin-und-Rueck-Umlauf entsprechend automatisch
		// 2x BEATS_FACTOR Beats.

		// Basiszykluszeit: Summe der (noch unmultiplizierten) Stage-Zeiten
		// aller NICHT geskippten Stages - modulweit einmal berechnet, da
		// resolvedTime[] und stageModifier[] (inkl. MOD_SKIP) zwischen
		// Section A/B geteilt sind, nur der Multiplier selbst ist pro
		// Section unterschiedlich.
		float baseCycleTime = 0.f;
		for (int i = 0; i < NUM_STAGES; i++) {
			if (!(stageModifier[i] & MOD_SKIP)) {
				baseCycleTime += resolvedTime[i];
			}
		}

		// Sicherheitsklammern fuer den zurueckgerechneten Multiplier: rein
		// numerischer Schutz (z.B. gegen Explosion bei extrem hoher BPM +
		// hohem BEATS_FACTOR, oder gegen 0 bei baseCycleTime == 0, falls
		// alle Stages geskippt sind), keine musikalische Grenze - bei
		// Bedarf spaeter justierbar.
		const float MULT_SAFETY_MIN = 1e-4f;
		const float MULT_SAFETY_MAX = 1e4f;

		float cvA = inputs[TIME_MULT_CV_A_INPUT].getVoltage();
		float totalVoltsA = params[TIME_MULT_A_PARAM].getValue()
			+ params[TIME_MULT_ATTEN_A_PARAM].getValue() * cvA;
		totalVoltsA = rack::clamp(totalVoltsA, -TEMPO_VOLT_SAFETY_CLAMP, TEMPO_VOLT_SAFETY_CLAMP);
		float bpmA = tempoVoltsToBpm(totalVoltsA);

		// BEATS_FACTOR Auto-Modus (Segmentlaengen-Sync, Session "BEATS_FACTOR
		// Auto-Modus"): im Auto-Modus wird BEATS_FACTOR_A_PARAM (der
		// Trimpot-Wert selbst) ignoriert, BEATS_FACTOR_CV_A_INPUT bleibt
		// aber additiv wirksam (Kinovice-Entscheidung) - die aus der
		// Segmentlaenge abgeleitete Stage-Anzahl bildet die Basis, auf die
		// CV genau wie im manuellen Modus draufaddiert wird (gleiche
		// Skalierung: +-5V deckt die volle Parameterspanne von ~64 Beats
		// ab). Segmentgrenzen/-Zeitsumme kommen aus dem Cache (nur bei
		// Segmentwechsel neu berechnet, siehe Afg::updateAutoSegment() in
		// Marf248.hpp) - die Summationsbasis fuer den Multiplier ist dann
		// ebenfalls nur dieses Segment (baseCycleTimeA), nicht mehr die
		// modulweite Summe aller 16 Stages (baseCycleTime oben), siehe
		// Session-Diskussion "Option A: proportional". currentStage
		// spiegelt hier den Stand vom Ende des VORHERIGEN Frames wider
		// (applyStageAddress()/process() fuer Section A laufen erst weiter
		// unten) - ein Sample Verzoegerung bei Segmentwechseln, musikalisch
		// irrelevant.
		float beatsFactorA;
		float baseCycleTimeA;
		if (beatsFactorAutoA) {
			afgA.updateAutoSegment(stageModifier, resolvedTime);
			float beatsCvA = inputs[BEATS_FACTOR_CV_A_INPUT].getVoltage();
			beatsFactorA = (float) afgA.segAutoStageCount + (beatsCvA / 5.f) * 64.f;
			beatsFactorA = rack::clamp(beatsFactorA, 0.1f, 64.f);
			baseCycleTimeA = afgA.segAutoBaseCycleTime;
		}
		else {
			float beatsCvA = inputs[BEATS_FACTOR_CV_A_INPUT].getVoltage();
			// CV-Skalierung analog zum bisherigen Time-Multiplier-Muster
			// (offene, spaeter justierbare Annahme): +-5V CV deckt die volle
			// Parameterspanne (0.1-64 Beats, Spanne ~64) ab, additiv auf den
			// Trimpot-Wert.
			beatsFactorA = params[BEATS_FACTOR_A_PARAM].getValue() + (beatsCvA / 5.f) * 64.f;
			beatsFactorA = rack::clamp(beatsFactorA, 0.1f, 64.f);
			baseCycleTimeA = baseCycleTime;
		}

		float desiredCycleTimeA = (60.f / bpmA) * beatsFactorA;
		float multiplierA = (baseCycleTimeA > 0.f) ? (desiredCycleTimeA / baseCycleTimeA) : 1.f;
		multiplierA = rack::clamp(multiplierA, MULT_SAFETY_MIN, MULT_SAFETY_MAX);

		float cvB = inputs[TIME_MULT_CV_B_INPUT].getVoltage();
		float totalVoltsB = params[TIME_MULT_B_PARAM].getValue()
			+ params[TIME_MULT_ATTEN_B_PARAM].getValue() * cvB;
		totalVoltsB = rack::clamp(totalVoltsB, -TEMPO_VOLT_SAFETY_CLAMP, TEMPO_VOLT_SAFETY_CLAMP);
		float bpmB = tempoVoltsToBpm(totalVoltsB);

		// Auto-Modus Section B: identisches Muster zu Section A oben
		// (inkl. additiver CV), siehe dortigen Kommentar.
		float beatsFactorB;
		float baseCycleTimeB;
		if (beatsFactorAutoB) {
			afgB.updateAutoSegment(stageModifier, resolvedTime);
			float beatsCvB = inputs[BEATS_FACTOR_CV_B_INPUT].getVoltage();
			beatsFactorB = (float) afgB.segAutoStageCount + (beatsCvB / 5.f) * 64.f;
			beatsFactorB = rack::clamp(beatsFactorB, 0.1f, 64.f);
			baseCycleTimeB = afgB.segAutoBaseCycleTime;
		}
		else {
			float beatsCvB = inputs[BEATS_FACTOR_CV_B_INPUT].getVoltage();
			beatsFactorB = params[BEATS_FACTOR_B_PARAM].getValue() + (beatsCvB / 5.f) * 64.f;
			beatsFactorB = rack::clamp(beatsFactorB, 0.1f, 64.f);
			baseCycleTimeB = baseCycleTime;
		}

		float desiredCycleTimeB = (60.f / bpmB) * beatsFactorB;
		float multiplierB = (baseCycleTimeB > 0.f) ? (desiredCycleTimeB / baseCycleTimeB) : 1.f;
		multiplierB = rack::clamp(multiplierB, MULT_SAFETY_MIN, MULT_SAFETY_MAX);

		for (int i = 0; i < NUM_STAGES; i++) {
			resolvedTimeA[i] = resolvedTime[i] * multiplierA;
			resolvedTimeB[i] = resolvedTime[i] * multiplierB;
		}

		// Live-BPM-Anzeige: nur dekimiert (stageButtonDivider-Rhythmus,
		// wiederverwendet ueber die oben eingefangene stageButtonsDecimated-
		// Flag, Kinovice-Entscheidung) in die Display-Member uebernehmen -
		// die obige Berechnung selbst bleibt bewusst audio-rate, da sie
		// direkt resolvedTimeA/B und damit die tatsaechliche Stage-
		// Zeitablauf-Berechnung speist.
		if (stageButtonsDecimated) {
			displayBpmA = bpmA;
			displayBpmB = bpmB;
		}

		// Laufrichtung pro Section: zwei unabhaengige Toggle-Buttons statt
		// eines Zyklus-Buttons. Muss vor dem jeweiligen afg*.process()-Aufruf
		// gesetzt sein, da advanceStage() intern danach verzweigt.
		if (modeAButtonTrigger.process(params[MODE_A_PARAM].getValue())) {
			reverseA = !reverseA;
		}
		if (pendulumAButtonTrigger.process(params[PENDULUM_A_PARAM].getValue())) {
			pendulumOnA = !pendulumOnA;
		}
		afgA.reverseMode = reverseA;
		afgA.pendulumMode = pendulumOnA;
		lights[MODE_A_LIGHT].setBrightness(reverseA ? 1.f : 0.f);
		lights[PENDULUM_A_LIGHT].setBrightness(pendulumOnA ? 1.f : 0.f);

		if (modeBButtonTrigger.process(params[MODE_B_PARAM].getValue())) {
			reverseB = !reverseB;
		}
		if (pendulumBButtonTrigger.process(params[PENDULUM_B_PARAM].getValue())) {
			pendulumOnB = !pendulumOnB;
		}
		afgB.reverseMode = reverseB;
		afgB.pendulumMode = pendulumOnB;
		lights[MODE_B_LIGHT].setBrightness(reverseB ? 1.f : 0.f);
		lights[PENDULUM_B_LIGHT].setBrightness(pendulumOnB ? 1.f : 0.f);

		// Stage Address je Section: Int/Ext-Quelle und Continuous sind
		// latching Toggles (wie Mode/Pendulum), Strobe/Reset sind reine
		// Momentary-Aktionen. Muss VOR afg*.process() ausgewertet werden,
		// da Continuous running=false setzt und dies noch im selben Frame
		// die Zeitablauf-Logik dort pausieren soll (siehe Afg::
		// applyStageAddress()).
		// Trigger-Expander-Overrides fuer Stage Address (Session "Stage
		// Address Poly-Overrides"): STAGE_ADDR_INTEXT_POLY_INPUT/
		// STAGE_ADDR_CONTINUOUS_POLY_INPUT (Kanal 0 = Section A) uebersteuern
		// bei gepatchtem Kabel die jeweiligen Panel-Taster komplett, sind
		// aber unabhaengig voneinander (Kinovice-Bestaetigung: Continuous
		// wirkt mit Int UND Ext gleichermassen) - siehe Kommentar in
		// Marf248TriggerExpander.hpp. Ohne Kabel greift wie bisher der
		// Panel-Taster (Klick-Toggle).
		const float ADDR_GATE_THRESHOLD = 2.f;
		// Panel-Trigger IMMER verarbeiten (nicht in einem else-Zweig
		// uebersprungen lassen) - sonst verliert der SchmittTrigger eine
		// Flanke, falls waehrend gepatchtem Override-Kabel geklickt wird,
		// und das raecht sich als "Phantom-Klick" beim spaeteren Auskabeln.
		// Gleiches Prinzip wie bei allen Stage-Modifier-Poly-Overrides
		// (STAGE_SKIP_POLY_INPUT etc.): Klick-Verarbeitung + CV-Override
		// unconditional danach, CV gewinnt einfach durch Ausfuehrungs-
		// reihenfolge.
		if (addrModeATrigger.process(params[STAGE_ADDR_MODE_A_PARAM].getValue())) {
			addrExtA = !addrExtA;
		}
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_INTEXT_POLY_INPUT).isConnected()) {
			addrExtA = trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_INTEXT_POLY_INPUT).getPolyVoltage(0) >= ADDR_GATE_THRESHOLD;
		}
		if (addrContATrigger.process(params[STAGE_ADDR_CONT_A_PARAM].getValue())) {
			addrContOnA = !addrContOnA;
		}
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_CONTINUOUS_POLY_INPUT).isConnected()) {
			addrContOnA = trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_CONTINUOUS_POLY_INPUT).getPolyVoltage(0) >= ADDR_GATE_THRESHOLD;
		}
		lights[STAGE_ADDR_MODE_A_LIGHT].setBrightness(addrExtA ? 1.f : 0.f);
		lights[STAGE_ADDR_CONT_A_LIGHT].setBrightness(addrContOnA ? 1.f : 0.f);
		int addrValueA;
		// STAGE_ADDR_POLY_INPUT (Kanal 0 = Section A) ist ein volles
		// Override: umgeht bei gepatchtem Kabel sowohl Int-Knob als auch
		// Ext-Zweig komplett, unabhaengig vom aktuellen addrExtA-Zustand
		// (Kinovice-Entscheidung, siehe Header-Kommentar).
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_POLY_INPUT).isConnected()) {
			float v = rack::clamp(trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_POLY_INPUT).getPolyVoltage(0), 0.f, 10.f);
			addrValueA = (int) rack::clamp(v / 10.f * NUM_STAGES, 0.f, (float) (NUM_STAGES - 1));
		}
		else if (addrExtA) {
			// Kanalwahl A-D per eigener CV (0-10V in 4 gleich grossen Zonen,
			// analog zur Zonenlogik bei Volt/Time External) - unverbunden
			// faellt auf Kanal A (Index 0) zurueck, damit bestehende Mono-
			// Patches an STAGE_ADDR_EXT_A_INPUT unveraendert funktionieren
			// (Kanal 0 = alter Verhalten, siehe InputId-Kommentar).
			int addrChA = 0;
			if (inputs[STAGE_ADDR_EXT_CHANNEL_A_INPUT].isConnected()) {
				float chCv = rack::clamp(inputs[STAGE_ADDR_EXT_CHANNEL_A_INPUT].getVoltage(), 0.f, 10.f);
				addrChA = (int) rack::clamp(chCv / 2.5f, 0.f, 3.f);
			}
			float v = rack::clamp(inputs[STAGE_ADDR_EXT_A_INPUT].getPolyVoltage(addrChA), 0.f, 10.f);
			addrValueA = (int) rack::clamp(v / 10.f * NUM_STAGES, 0.f, (float) (NUM_STAGES - 1));
		}
		else {
			addrValueA = (int) std::round(params[STAGE_ADDR_KNOB_A_PARAM].getValue());
		}
		bool strobeA = addrStrobeATrigger.process(params[STAGE_ADDR_STROBE_A_PARAM].getValue());
		// SEXT A: externer Strobe-Puls vom Trigger-Expander, ODER-verknuepft
		// mit dem Panel-Taster. Keine zusaetzliche Continuous-Abfrage noetig -
		// applyStageAddress() ignoriert strobePressed ohnehin, wenn
		// addrContOnA aktiv ist (fruehes return im Continuous-Zweig, siehe
		// Marf248.hpp), das SEXT-Verhalten entspricht damit automatisch
		// exakt dem Panel-Taster.
		if (trigExp && expStrobeATrigger.process(trigExp->getInput(Marf248TriggerExpander::STROBE_A_INPUT).getVoltage())) {
			strobeA = true;
		}
		bool resetA = addrResetATrigger.process(params[STAGE_ADDR_RESET_A_PARAM].getValue());
		if (strobeA) addrStrobeAFlash.trigger(0.15f);
		if (resetA) addrResetAFlash.trigger(0.15f);
		lights[STAGE_ADDR_STROBE_A_LIGHT].setBrightness(addrStrobeAFlash.process(args.sampleTime) ? 1.f : 0.f);
		lights[STAGE_ADDR_RESET_A_LIGHT].setBrightness(addrResetAFlash.process(args.sampleTime) ? 1.f : 0.f);
		// Freeze A (Session "Continuous Freeze", Kanal 0 = Section A): reiner
		// Pegel-Override wie STAGE_ADDR_CONTINUOUS_POLY_INPUT oben, kein
		// eigener Panel-Taster/Zustand noetig - siehe Kommentar im Header.
		bool freezeA = trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_FREEZE_POLY_INPUT).getPolyVoltage(0) >= ADDR_GATE_THRESHOLD;
		afgA.applyStageAddress(addrContOnA, addrValueA, strobeA, resetA, stageModifier, freezeA);

		// Siehe Kommentar bei Section A oben, analog (Kanal 1 = Section B).
		if (addrModeBTrigger.process(params[STAGE_ADDR_MODE_B_PARAM].getValue())) {
			addrExtB = !addrExtB;
		}
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_INTEXT_POLY_INPUT).isConnected()) {
			addrExtB = trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_INTEXT_POLY_INPUT).getPolyVoltage(1) >= ADDR_GATE_THRESHOLD;
		}
		if (addrContBTrigger.process(params[STAGE_ADDR_CONT_B_PARAM].getValue())) {
			addrContOnB = !addrContOnB;
		}
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_CONTINUOUS_POLY_INPUT).isConnected()) {
			addrContOnB = trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_CONTINUOUS_POLY_INPUT).getPolyVoltage(1) >= ADDR_GATE_THRESHOLD;
		}
		lights[STAGE_ADDR_MODE_B_LIGHT].setBrightness(addrExtB ? 1.f : 0.f);
		lights[STAGE_ADDR_CONT_B_LIGHT].setBrightness(addrContOnB ? 1.f : 0.f);
		int addrValueB;
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_POLY_INPUT).isConnected()) {
			float v = rack::clamp(trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_POLY_INPUT).getPolyVoltage(1), 0.f, 10.f);
			addrValueB = (int) rack::clamp(v / 10.f * NUM_STAGES, 0.f, (float) (NUM_STAGES - 1));
		}
		else if (addrExtB) {
			// Siehe Kommentar bei addrChA oben, analog fuer Section B.
			int addrChB = 0;
			if (inputs[STAGE_ADDR_EXT_CHANNEL_B_INPUT].isConnected()) {
				float chCv = rack::clamp(inputs[STAGE_ADDR_EXT_CHANNEL_B_INPUT].getVoltage(), 0.f, 10.f);
				addrChB = (int) rack::clamp(chCv / 2.5f, 0.f, 3.f);
			}
			float v = rack::clamp(inputs[STAGE_ADDR_EXT_B_INPUT].getPolyVoltage(addrChB), 0.f, 10.f);
			addrValueB = (int) rack::clamp(v / 10.f * NUM_STAGES, 0.f, (float) (NUM_STAGES - 1));
		}
		else {
			addrValueB = (int) std::round(params[STAGE_ADDR_KNOB_B_PARAM].getValue());
		}
		bool strobeB = addrStrobeBTrigger.process(params[STAGE_ADDR_STROBE_B_PARAM].getValue());
		// SEXT B: siehe Kommentar bei strobeA weiter oben, analog.
		if (trigExp && expStrobeBTrigger.process(trigExp->getInput(Marf248TriggerExpander::STROBE_B_INPUT).getVoltage())) {
			strobeB = true;
		}
		bool resetB = addrResetBTrigger.process(params[STAGE_ADDR_RESET_B_PARAM].getValue());
		if (strobeB) addrStrobeBFlash.trigger(0.15f);
		if (resetB) addrResetBFlash.trigger(0.15f);
		lights[STAGE_ADDR_STROBE_B_LIGHT].setBrightness(addrStrobeBFlash.process(args.sampleTime) ? 1.f : 0.f);
		lights[STAGE_ADDR_RESET_B_LIGHT].setBrightness(addrResetBFlash.process(args.sampleTime) ? 1.f : 0.f);
		// Freeze B: siehe Kommentar bei freezeA oben, analog (Kanal 1 =
		// Section B).
		bool freezeB = trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_ADDR_FREEZE_POLY_INPUT).getPolyVoltage(1) >= ADDR_GATE_THRESHOLD;
		afgB.applyStageAddress(addrContOnB, addrValueB, strobeB, resetB, stageModifier, freezeB);

		float vA, tA, apA, p1A, p2A, scA, ecA, rgA, eosA;
		afgA.process(args.sampleTime, resolvedVoltage, resolvedTimeA, stageModifier, stageCurvePower,
			stageRatchetCount, stageWarpBlend, stageRatchetOffset,
			currentKey, currentScale,
			params[RUN_A_PARAM].getValue(),
			inputs[START_A_INPUT].isConnected(), inputs[START_A_INPUT].getVoltage(),
			inputs[STOP_A_INPUT].isConnected(), inputs[STOP_A_INPUT].getVoltage(),
			// Start/Stop-Gate vom Trigger-Expander (RUN_GATE_A_INPUT):
			// unverbunden, wenn kein Trigger-Expander andockt.
			trigExp && trigExp->getInput(Marf248TriggerExpander::RUN_GATE_A_INPUT).isConnected(),
			trigExp ? trigExp->getInput(Marf248TriggerExpander::RUN_GATE_A_INPUT).getVoltage() : 0.f,
			// Advance A (Clock-quantisiertes Advance): unverbunden, wenn kein
			// Trigger-Expander andockt - identisches Muster wie RUN_GATE_A oben.
			trigExp && trigExp->getInput(Marf248TriggerExpander::ADVANCE_A_INPUT).isConnected(),
			trigExp ? trigExp->getInput(Marf248TriggerExpander::ADVANCE_A_INPUT).getVoltage() : 0.f,
			&vA, &tA, &apA, &p1A, &p2A, &scA, &ecA, &rgA, &eosA);
		outputs[VOUT_A_OUTPUT].setVoltage(vA);
		outputs[TOUT_A_OUTPUT].setVoltage(tA);
		outputs[APOUT_A_OUTPUT].setVoltage(apA);
		outputs[P1OUT_A_OUTPUT].setVoltage(p1A);
		outputs[P2OUT_A_OUTPUT].setVoltage(p2A);
		// Time Output (siehe Kommentar bei TIMESTATIC_A/B_OUTPUT oben):
		// bewusst die ROHE Sliderposition (stageTime[], 0..1 -> 0-10V) der
		// von Section A aktuell adressierten Stage, NICHT resolvedTimeA
		// (bereits aufgeloeste Sekundenzeit inkl. Time Range/External-CV).
		// Manual: bleibt auch bei Time Source = External nutzbar, ohne die
		// tatsaechliche Ablaufzeit zu beeinflussen - genau dafuer muss der
		// Wert von resolvedTime[]/External-Override unabhaengig bleiben.
		outputs[TIMESTATIC_A_OUTPUT].setVoltage(stageTime[afgA.currentStage] * 10.f);
		lights[P1_ACTIVE_A_LIGHT].setBrightness(p1A > 0.f ? 1.f : 0.f);
		lights[P2_ACTIVE_A_LIGHT].setBrightness(p2A > 0.f ? 1.f : 0.f);

		float vB, tB, apB, p1B, p2B, scB, ecB, rgB, eosB;
		afgB.process(args.sampleTime, resolvedVoltage, resolvedTimeB, stageModifier, stageCurvePower,
			stageRatchetCount, stageWarpBlend, stageRatchetOffset,
			currentKey, currentScale,
			params[RUN_B_PARAM].getValue(),
			inputs[START_B_INPUT].isConnected(), inputs[START_B_INPUT].getVoltage(),
			inputs[STOP_B_INPUT].isConnected(), inputs[STOP_B_INPUT].getVoltage(),
			// Start/Stop-Gate vom Trigger-Expander (RUN_GATE_B_INPUT):
			// unverbunden, wenn kein Trigger-Expander andockt.
			trigExp && trigExp->getInput(Marf248TriggerExpander::RUN_GATE_B_INPUT).isConnected(),
			trigExp ? trigExp->getInput(Marf248TriggerExpander::RUN_GATE_B_INPUT).getVoltage() : 0.f,
			// Advance B (Clock-quantisiertes Advance): unverbunden, wenn kein
			// Trigger-Expander andockt - identisches Muster wie RUN_GATE_B oben.
			trigExp && trigExp->getInput(Marf248TriggerExpander::ADVANCE_B_INPUT).isConnected(),
			trigExp ? trigExp->getInput(Marf248TriggerExpander::ADVANCE_B_INPUT).getVoltage() : 0.f,
			&vB, &tB, &apB, &p1B, &p2B, &scB, &ecB, &rgB, &eosB);
		outputs[VOUT_B_OUTPUT].setVoltage(vB);
		outputs[TOUT_B_OUTPUT].setVoltage(tB);
		outputs[APOUT_B_OUTPUT].setVoltage(apB);
		outputs[P1OUT_B_OUTPUT].setVoltage(p1B);
		outputs[P2OUT_B_OUTPUT].setVoltage(p2B);
		// Time Output Section B: siehe Kommentar bei TIMESTATIC_A_OUTPUT
		// oben, analog.
		outputs[TIMESTATIC_B_OUTPUT].setVoltage(stageTime[afgB.currentStage] * 10.f);
		lights[P1_ACTIVE_B_LIGHT].setBrightness(p1B > 0.f ? 1.f : 0.f);
		lights[P2_ACTIVE_B_LIGHT].setBrightness(p2B > 0.f ? 1.f : 0.f);

		// Marf248TriggerExpander: einfacher "Host liest/schreibt direkt"-
		// Ansatz (kein Message-Passing noetig, da der Expander selbst keine
		// eigene Verarbeitung hat - siehe Marf248TriggerExpander.hpp).
		// trigExp wurde bereits am Anfang von process() ermittelt (siehe
		// dort - SEXT braucht ihn frueher als den Rest dieses Blocks).
		if (trigExp) {
			if (expClearTrigger.process(trigExp->getInput(Marf248TriggerExpander::CLEAR_INPUT).getVoltage())) {
				performGlobalClear();
			}
			if (expResetValuesTrigger.process(trigExp->getInput(Marf248TriggerExpander::RESET_VALUES_INPUT).getVoltage())) {
				performResetValues();
			}
			// Preset Load: direkter Zugriff, kein Arm-Mechanismus noetig (im
			// Unterschied zum Panel-LOAD-Taster) - derselbe Leer-Slot-Schutz
			// wie am Panel (siehe loadPreset()-Aufrufstelle weiter oben).
			// Poly-Umbau: ein einzelner Input, Kanal p (0-11) = Preset p+1.
			// getPolyVoltage(p) liefert 0V fuer nicht vorhandene/unbenutzte
			// Kanaele (auch wenn der Jack komplett unpatched ist), loest
			// also keinen Trigger aus - identisches Verhalten zu den
			// vorherigen unpatchten Einzelbuchsen.
			for (int p = 0; p < 12; p++) {
				if (expPresetLoadTrigger[p].process(trigExp->getInput(Marf248TriggerExpander::PRESET_LOAD_POLY_INPUT).getPolyVoltage(p))) {
					if (presets[p].used) {
						loadPreset(p);
						presetFlash[p].trigger(0.2f);
					}
				}
			}

			// Adress-Speicher Store/Recall (siehe AddrMemorySlot oben): reines
			// Sprung-Lesezeichen, unabhaengig vom Preset-System. Store schreibt
			// bei steigender Flanke SOFORT (kein Arm, gleiches Prinzip wie
			// Preset-Load-Poly oben), Recall springt nur, wenn der Slot zuvor
			// beschrieben wurde (used-Flag) - ein nie beschriebener Slot bleibt
			// wirkungslos, analog zum Preset-Leer-Slot-Schutz. Passiert bewusst
			// HIER, NACH afgA/afgB.process() weiter oben - wirkt also erst im
			// naechsten Sample-Frame, hat dann aber Vorrang vor Continuous/
			// Strobe/Int-Ext fuer diesen Frame (kein zusaetzlicher Guard
			// noetig: bleibt Continuous aktiv, ueberschreibt es den Sprung im
			// uebernaechsten Frame automatisch wieder - vorhersehbares
			// Verhalten, kein Bug).
			for (int i = 0; i < NUM_STAGES; i++) {
				if (expAddrStoreATrigger[i].process(trigExp->getInput(Marf248TriggerExpander::ADDR_STORE_A_POLY_INPUT).getPolyVoltage(i))) {
					addrMemA[i].stage = afgA.currentStage;
					addrMemA[i].used = true;
				}
				if (expAddrStoreBTrigger[i].process(trigExp->getInput(Marf248TriggerExpander::ADDR_STORE_B_POLY_INPUT).getPolyVoltage(i))) {
					addrMemB[i].stage = afgB.currentStage;
					addrMemB[i].used = true;
				}
				if (expAddrRecallATrigger[i].process(trigExp->getInput(Marf248TriggerExpander::ADDR_RECALL_A_POLY_INPUT).getPolyVoltage(i))) {
					if (addrMemA[i].used) {
						afgA.jumpToStage(addrMemA[i].stage, stageModifier);
					}
				}
				if (expAddrRecallBTrigger[i].process(trigExp->getInput(Marf248TriggerExpander::ADDR_RECALL_B_POLY_INPUT).getPolyVoltage(i))) {
					if (addrMemB[i].used) {
						afgB.jumpToStage(addrMemB[i].stage, stageModifier);
					}
				}
			}
			// Scale: drei diskrete Trigger, direkte Zuweisung (kein Arm noetig).
			if (expScaleMajorTrigger.process(trigExp->getInput(Marf248TriggerExpander::SCALE_MAJOR_INPUT).getVoltage())) {
				currentScale = 0;
			}
			if (expScaleMinorTrigger.process(trigExp->getInput(Marf248TriggerExpander::SCALE_MINOR_INPUT).getVoltage())) {
				currentScale = 1;
			}
			if (expScaleChromaticTrigger.process(trigExp->getInput(Marf248TriggerExpander::SCALE_CHROMATIC_INPUT).getVoltage())) {
				currentScale = 2;
			}
			// Key: kontinuierliche CV (0-10V -> 0-11), nur wenn der Jack
			// tatsaechlich gepatcht ist - sonst wuerde eine schwebende
			// (unbenutzte) Buchse staendig auf 0V/"C" zurueckfallen, nur weil
			// der Expander andockt.
			if (trigExp->getInput(Marf248TriggerExpander::KEY_CV_INPUT).isConnected()) {
				float keyCv = rack::clamp(trigExp->getInput(Marf248TriggerExpander::KEY_CV_INPUT).getVoltage(), 0.f, 10.f);
				currentKey = (int) rack::clamp(std::floor(keyCv / 10.f * 12.f), 0.f, 11.f);
			}

			// Start-/End-of-Cycle je Section direkt in die Expander-Ausgaenge
			// schreiben (siehe Afg::process() fuer die Erzeugung).
			trigExp->getOutput(Marf248TriggerExpander::START_CYCLE_A_OUTPUT).setVoltage(scA);
			trigExp->getOutput(Marf248TriggerExpander::END_CYCLE_A_OUTPUT).setVoltage(ecA);
			trigExp->getOutput(Marf248TriggerExpander::START_CYCLE_B_OUTPUT).setVoltage(scB);
			trigExp->getOutput(Marf248TriggerExpander::END_CYCLE_B_OUTPUT).setVoltage(ecB);
			// End of Stage je Section direkt in die Expander-Ausgaenge
			// schreiben (siehe Afg::process()/Afg::eosGen in Marf248.hpp).
			trigExp->getOutput(Marf248TriggerExpander::EOS_A_OUTPUT).setVoltage(eosA);
			trigExp->getOutput(Marf248TriggerExpander::EOS_B_OUTPUT).setVoltage(eosB);
		}

		// Marf248CvExpander: Gate A/B, Kanal i high sobald die jeweilige
		// Section aktuell Stage i adressiert - unabhaengig vom Run-Zustand
		// (Manual-Analogon: die Stage-Adresse selbst ist immer "aktiv",
		// unabhaengig davon ob der interne Zeitablauf laeuft). Optionale
		// Retrigger-Luecke (Kontextmenue am Expander, regateGapEnabled):
		// zieht das Gate der aktuell adressierten Stage kurz auf 0V, wenn
		// afgA/afgB.regateGen in diesem Frame aktiv ist (siehe rgA/rgB oben,
		// gesetzt bei Stage-Wiedereintritt ohne Indexwechsel).
		if (cvExp) {
			bool gapEnabled = cvExp->regateGapEnabled;
			bool gapActiveA = gapEnabled && rgA > 0.5f;
			bool gapActiveB = gapEnabled && rgB > 0.5f;

			Output& gateAOut = cvExp->getOutput(Marf248CvExpander::GATE_A_OUTPUT);
			Output& gateBOut = cvExp->getOutput(Marf248CvExpander::GATE_B_OUTPUT);
			gateAOut.setChannels(16);
			gateBOut.setChannels(16);
			for (int i = 0; i < NUM_STAGES; i++) {
				bool highA = (i == afgA.currentStage) && !gapActiveA;
				bool highB = (i == afgB.currentStage) && !gapActiveB;
				gateAOut.setVoltage(highA ? 10.f : 0.f, i);
				gateBOut.setVoltage(highB ? 10.f : 0.f, i);
			}
		}

		// Dreifarbige Stage-Address-Statusanzeige (Manual "FUNCTION GENERATOR
		// CONTROL - MODE"): Rot/Stop deckt Stop-Taster/-Puls bzw. ein nicht
		// aktives Start/Stop-Gate (effectiveRunning==false - siehe Afg::
		// process(), ODER-Verknuepfung aus Taster-Zustand und Gate), eine
		// programmierte Stop-Stage (statusStoppedByStage, running/
		// effectiveRunning bleiben intern true) und Continuous-Adressierung
		// (addrContOn) ab. Gelb/Hold ist Sustain/Enable, das auf das
		// Start-Gate wartet. Gruen/Run sonst.
		//
		// advanceModeActive (Clock-quantisiertes Advance) haengt die Section
		// bewusst vom Start/Stop-Taster ab (siehe Afg::process()) - ohne
		// Sonderfall wuerde effectiveRunning==false hier faelschlich Rot
		// zeigen, obwohl die Section aktiv auf externe Trigger wartet.
		// Daher: !effectiveRunning zaehlt nur als "Stop", wenn advanceMode
		// NICHT aktiv ist. Anders als Continuous (das ueber addrContOnA/B
		// weiterhin hart auf Rot zwingt) bekommt Advance also Gruen/Run -
		// naheliegende Wahl (siehe Session-Diskussion), da die Section im
		// Advance-Modus weiterhin eine echte Sequenz durchlaeuft (nur extern
		// statt intern getaktet), waehrend Continuous konzeptionell gar
		// keine "laufende" Sequenz mehr ist, sondern eine direkte Adressierung.
		//
		// Farbmischung auf der RGB-LED (siehe RUN_A/B_LIGHT_R/G/B oben):
		// Gruen=Run (nur G), Rot=Stop (nur R), Gelb=Hold (R+G gemeinsam,
		// Standard-additive Mischung). Blau bleibt ungenutzt (0).
		//
		// HOLD_BRIGHTNESS: Gelb (zwei volle Kanaele gleichzeitig) wirkt
		// optisch heller als Rot/Gruen allein (je nur ein Kanal) - beide
		// Kanaele werden im Hold-Zustand daher gemeinsam gedimmt
		// (Kinovice-Feedback "Gelb zu hell"). Bei Bedarf hier anpassen -
		// 1.0 waere wieder volle Helligkeit wie zuvor.
		const float HOLD_BRIGHTNESS = 0.5f;
		bool stopA = (!afgA.effectiveRunning && !afgA.advanceModeActive) || afgA.statusStoppedByStage || addrContOnA;
		bool holdA = !stopA && afgA.statusHeld;
		bool runA = !stopA && !holdA;
		lights[RUN_A_LIGHT_R].setBrightness(stopA ? 1.f : (holdA ? HOLD_BRIGHTNESS : 0.f));
		lights[RUN_A_LIGHT_G].setBrightness(runA ? 1.f : (holdA ? HOLD_BRIGHTNESS : 0.f));
		lights[RUN_A_LIGHT_B].setBrightness(0.f);

		bool stopB = (!afgB.effectiveRunning && !afgB.advanceModeActive) || afgB.statusStoppedByStage || addrContOnB;
		bool holdB = !stopB && afgB.statusHeld;
		bool runB = !stopB && !holdB;
		lights[RUN_B_LIGHT_R].setBrightness(stopB ? 1.f : (holdB ? HOLD_BRIGHTNESS : 0.f));
		lights[RUN_B_LIGHT_G].setBrightness(runB ? 1.f : (holdB ? HOLD_BRIGHTNESS : 0.f));
		lights[RUN_B_LIGHT_B].setBrightness(0.f);
		for (int i = 0; i < NUM_STAGES; i++) {
			lights[STAGE_LIGHT_A + i].setBrightness(i == afgA.currentStage ? 0.35f : 0.f);
			lights[STAGE_LIGHT_B + i].setBrightness(i == afgB.currentStage ? 0.35f : 0.f);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_t* modsJ = json_array();
		for (int i = 0; i < NUM_STAGES; i++) {
			json_array_append_new(modsJ, json_integer(stageModifier[i]));
		}
		json_object_set_new(rootJ, "stageModifier", modsJ);
		json_object_set_new(rootJ, "reverseA", json_boolean(reverseA));
		json_object_set_new(rootJ, "pendulumOnA", json_boolean(pendulumOnA));
		json_object_set_new(rootJ, "reverseB", json_boolean(reverseB));
		json_object_set_new(rootJ, "pendulumOnB", json_boolean(pendulumOnB));
		json_object_set_new(rootJ, "addrExtA", json_boolean(addrExtA));
		json_object_set_new(rootJ, "addrContOnA", json_boolean(addrContOnA));
		json_object_set_new(rootJ, "addrExtB", json_boolean(addrExtB));
		json_object_set_new(rootJ, "addrContOnB", json_boolean(addrContOnB));
		json_object_set_new(rootJ, "beatsFactorAutoA", json_boolean(beatsFactorAutoA));
		json_object_set_new(rootJ, "beatsFactorAutoB", json_boolean(beatsFactorAutoB));
		json_object_set_new(rootJ, "currentKey", json_integer(currentKey));
		json_object_set_new(rootJ, "currentScale", json_integer(currentScale));

		// Single-Stage-Slot-Anzeige (Kompaktversion "Lucia", siehe
		// Member-Deklaration oben) - reiner GUI-Zustand, kein Param.
		json_object_set_new(rootJ, "pendingStage", json_integer(pendingStage));
		json_object_set_new(rootJ, "displayedStage", json_integer(displayedStage));

		// Preset-Slots: nur belegte Slots werden mit vollem Inhalt
		// gespeichert, leere Slots nur mit used=false (keine Notwendigkeit,
		// 16 Nullen mitzuschleppen).
		json_t* presetsJ = json_array();
		for (int p = 0; p < 12; p++) {
			json_t* slotJ = json_object();
			json_object_set_new(slotJ, "used", json_boolean(presets[p].used));
			if (presets[p].used) {
				json_t* voltJ = json_array();
				json_t* timeJ = json_array();
				json_t* rangeJ = json_array();
				json_t* curveJ = json_array();
				json_t* modJ = json_array();
				for (int i = 0; i < NUM_STAGES; i++) {
					json_array_append_new(voltJ, json_real(presets[p].voltage[i]));
					json_array_append_new(timeJ, json_real(presets[p].time[i]));
					json_array_append_new(rangeJ, json_real(presets[p].range[i]));
					json_array_append_new(curveJ, json_real(presets[p].curve[i]));
					json_array_append_new(modJ, json_integer(presets[p].modifier[i]));
				}
				json_object_set_new(slotJ, "voltage", voltJ);
				json_object_set_new(slotJ, "time", timeJ);
				json_object_set_new(slotJ, "range", rangeJ);
				json_object_set_new(slotJ, "curve", curveJ);
				json_object_set_new(slotJ, "modifier", modJ);
			}
			json_array_append_new(presetsJ, slotJ);
		}
		json_object_set_new(rootJ, "presets", presetsJ);

		// Adress-Speicher: nur int+bool je Slot, damit leichtgewichtig genug
		// um immer vollstaendig geschrieben zu werden (kein Sparse-Skip wie
		// bei Presets noetig).
		json_t* addrMemAJ = json_array();
		json_t* addrMemBJ = json_array();
		for (int i = 0; i < NUM_STAGES; i++) {
			json_t* slotAJ = json_object();
			json_object_set_new(slotAJ, "used", json_boolean(addrMemA[i].used));
			json_object_set_new(slotAJ, "stage", json_integer(addrMemA[i].stage));
			json_array_append_new(addrMemAJ, slotAJ);

			json_t* slotBJ = json_object();
			json_object_set_new(slotBJ, "used", json_boolean(addrMemB[i].used));
			json_object_set_new(slotBJ, "stage", json_integer(addrMemB[i].stage));
			json_array_append_new(addrMemBJ, slotBJ);
		}
		json_object_set_new(rootJ, "addrMemA", addrMemAJ);
		json_object_set_new(rootJ, "addrMemB", addrMemBJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* modsJ = json_object_get(rootJ, "stageModifier");
		if (modsJ) {
			for (int i = 0; i < NUM_STAGES; i++) {
				json_t* modJ = json_array_get(modsJ, i);
				if (modJ) {
					stageModifier[i] = json_integer_value(modJ);
				}
			}
		}
		json_t* reverseAJ = json_object_get(rootJ, "reverseA");
		if (reverseAJ) {
			reverseA = json_boolean_value(reverseAJ);
		}
		json_t* pendulumOnAJ = json_object_get(rootJ, "pendulumOnA");
		if (pendulumOnAJ) {
			pendulumOnA = json_boolean_value(pendulumOnAJ);
		}
		json_t* reverseBJ = json_object_get(rootJ, "reverseB");
		if (reverseBJ) {
			reverseB = json_boolean_value(reverseBJ);
		}
		json_t* pendulumOnBJ = json_object_get(rootJ, "pendulumOnB");
		if (pendulumOnBJ) {
			pendulumOnB = json_boolean_value(pendulumOnBJ);
		}
		json_t* addrExtAJ = json_object_get(rootJ, "addrExtA");
		if (addrExtAJ) {
			addrExtA = json_boolean_value(addrExtAJ);
		}
		json_t* addrContOnAJ = json_object_get(rootJ, "addrContOnA");
		if (addrContOnAJ) {
			addrContOnA = json_boolean_value(addrContOnAJ);
		}
		json_t* addrExtBJ = json_object_get(rootJ, "addrExtB");
		if (addrExtBJ) {
			addrExtB = json_boolean_value(addrExtBJ);
		}
		json_t* addrContOnBJ = json_object_get(rootJ, "addrContOnB");
		if (addrContOnBJ) {
			addrContOnB = json_boolean_value(addrContOnBJ);
		}
		json_t* beatsFactorAutoAJ = json_object_get(rootJ, "beatsFactorAutoA");
		if (beatsFactorAutoAJ) {
			beatsFactorAutoA = json_boolean_value(beatsFactorAutoAJ);
		}
		json_t* beatsFactorAutoBJ = json_object_get(rootJ, "beatsFactorAutoB");
		if (beatsFactorAutoBJ) {
			beatsFactorAutoB = json_boolean_value(beatsFactorAutoBJ);
		}

		json_t* currentKeyJ = json_object_get(rootJ, "currentKey");
		if (currentKeyJ) {
			currentKey = json_integer_value(currentKeyJ);
		}
		json_t* currentScaleJ = json_object_get(rootJ, "currentScale");
		if (currentScaleJ) {
			currentScale = json_integer_value(currentScaleJ);
		}

		// Single-Stage-Slot-Anzeige (Kompaktversion "Lucia", siehe
		// Member-Deklaration oben). Clamp auf 0..NUM_STAGES-1 gegen
		// haendisch editierte oder aus einer aelteren/anderen Fassung
		// stammende Patch-Dateien mit ungueltigem Wert.
		json_t* pendingStageJ = json_object_get(rootJ, "pendingStage");
		if (pendingStageJ) {
			pendingStage = rack::clamp((int) json_integer_value(pendingStageJ), 0, NUM_STAGES - 1);
		}
		json_t* displayedStageJ = json_object_get(rootJ, "displayedStage");
		if (displayedStageJ) {
			displayedStage = rack::clamp((int) json_integer_value(displayedStageJ), 0, NUM_STAGES - 1);
		}

		json_t* presetsJ = json_object_get(rootJ, "presets");
		if (presetsJ) {
			for (int p = 0; p < 12; p++) {
				json_t* slotJ = json_array_get(presetsJ, p);
				if (!slotJ) {
					continue;
				}
				json_t* usedJ = json_object_get(slotJ, "used");
				presets[p].used = usedJ ? json_boolean_value(usedJ) : false;
				if (!presets[p].used) {
					continue;
				}
				json_t* voltJ = json_object_get(slotJ, "voltage");
				json_t* timeJ = json_object_get(slotJ, "time");
				json_t* rangeJ = json_object_get(slotJ, "range");
				json_t* curveJ = json_object_get(slotJ, "curve");
				json_t* modJ = json_object_get(slotJ, "modifier");
				for (int i = 0; i < NUM_STAGES; i++) {
					if (voltJ) {
						json_t* v = json_array_get(voltJ, i);
						if (v) presets[p].voltage[i] = json_real_value(v);
					}
					if (timeJ) {
						json_t* v = json_array_get(timeJ, i);
						if (v) presets[p].time[i] = json_real_value(v);
					}
					if (rangeJ) {
						json_t* v = json_array_get(rangeJ, i);
						if (v) presets[p].range[i] = json_real_value(v);
					}
					if (curveJ) {
						json_t* v = json_array_get(curveJ, i);
						if (v) presets[p].curve[i] = json_real_value(v);
					}
					if (modJ) {
						json_t* v = json_array_get(modJ, i);
						if (v) presets[p].modifier[i] = json_integer_value(v);
					}
				}
			}
		}

		json_t* addrMemAJ = json_object_get(rootJ, "addrMemA");
		if (addrMemAJ) {
			for (int i = 0; i < NUM_STAGES; i++) {
				json_t* slotJ = json_array_get(addrMemAJ, i);
				if (!slotJ) {
					continue;
				}
				json_t* usedJ = json_object_get(slotJ, "used");
				json_t* stageJ = json_object_get(slotJ, "stage");
				addrMemA[i].used = usedJ ? json_boolean_value(usedJ) : false;
				addrMemA[i].stage = stageJ ? (int) json_integer_value(stageJ) : 0;
			}
		}
		json_t* addrMemBJ = json_object_get(rootJ, "addrMemB");
		if (addrMemBJ) {
			for (int i = 0; i < NUM_STAGES; i++) {
				json_t* slotJ = json_array_get(addrMemBJ, i);
				if (!slotJ) {
					continue;
				}
				json_t* usedJ = json_object_get(slotJ, "used");
				json_t* stageJ = json_object_get(slotJ, "stage");
				addrMemB[i].used = usedJ ? json_boolean_value(usedJ) : false;
				addrMemB[i].stage = stageJ ? (int) json_integer_value(stageJ) : 0;
			}
		}
	}
};


// Erst jetzt, nach der vollstaendigen Definition von Marf248, kann auf
// dessen Member (stageModifier) zugegriffen werden.
std::string StageVoltageQuantity::getDisplayValueString() {
	Marf248* m = dynamic_cast<Marf248*>(module);
	if (m && (m->stageModifier[stageIndex] & MOD_VOLT_EXT)) {
		// Im External-Modus ist der Sliderwert keine Spannung mehr, sondern
		// eine Kanalwahl (A-D) - Notenname/Volt-Anzeige waere hier irrefuehrend.
		static const char* channels[4] = {"Ext A", "Ext B", "Ext C", "Ext D"};
		int ch = (int) rack::clamp(getValue() / 2.5f, 0.f, 3.f);
		return channels[ch];
	}
	if (m && (m->stageModifier[stageIndex] & MOD_QUANT_OCT)) {
		return voltageToNoteName(getValue());
	}
	return ParamQuantity::getDisplayValueString();
}

// Loest die normalisierte 0..1-Position des Time-Reglers gegen den aktuell
// gewaehlten Time Range (TR1-4) auf und zeigt das Ergebnis als Sekundenzahl
// an - ohne diesen Override wuerde der Tooltip nur die nackte 0..1-Position
// zeigen, was ohne Kontext bedeutungslos waere.
std::string StageTimeQuantity::getDisplayValueString() {
	Marf248* m = dynamic_cast<Marf248*>(module);
	// Dial-Wert (0-10, Buchla-Konvention) fuehrend, exakte Sekundenzahl in
	// Klammern dahinter - deckt beide Bedarfsfaelle in einem Tooltip ab
	// (Cookbook-Tabellenwerte direkt ablesbar UND exakte Zeit sichtbar).
	// Rechtsklick-Zahleneingabe (setDisplayValueString(), Basisklasse) liest
	// per std::stof ohnehin nur die fuehrende Zahl und ignoriert den Rest -
	// der Klammerzusatz stoert die Eingabe also nicht.
	//
	// %.1f -> %.3f (Kinovice-Feedback "Time Slider rundet auf eine
	// Nachkommastelle"): der gespeicherte Wert war nie tatsaechlich
	// gerundet (setDisplayValue() rundet nirgends), nur die Anzeige selbst
	// war mit einer Nachkommastelle zu grob - anders als Voltage, das ohne
	// eigenes getDisplayValue()/setDisplayValue() das praezisere Rack-
	// Standardformat nutzt. 3 Nachkommastellen auf der 0-10-Dial-Skala
	// entsprechen der Aufloesung von 1/1000 des Reglerwegs.
	if (!m) {
		return string::f("%.3f", getDisplayValue());
	}
	int idx = getTimeRangeIndex(m->stageModifier[stageIndex]);
	float seconds = TIME_RANGE_MIN[idx] + getValue() * (TIME_RANGE_MAX[idx] - TIME_RANGE_MIN[idx]);
	// TR1/TR2 bewegen sich im Millisekunden- bis niedrigen Zehntelbereich und
	// brauchen daher mehr Nachkommastellen als TR3/TR4, sonst wuerde z.B.
	// ".00 s" fuer den gesamten TR1-Bereich angezeigt.
	std::string secStr = (seconds < 1.f) ? string::f("%.3f s", seconds) : string::f("%.2f s", seconds);
	return string::f("%.3f (%s)", getDisplayValue(), secStr.c_str());
}


// ============================================================================
// Stage-Number-Broadcast (Session "Stage-Number-Broadcast"): Rechtsklick-
// Kontextmenue-Eintrag "Apply to all stages" auf einem Modifier-Button oder
// einem kontinuierlichen Regler (Voltage/Time/Range/Curve) uebertraegt den
// aktuellen Zustand/Wert der Quell-Stage auf alle 16 Stages. Zwei getrennte
// Mechanismen noetig, da die beiden Kategorien unterschiedlich gespeichert
// werden:
//
//   - Bit-Modifier (Pulse1/2, Skip, First/Last, Stop/Sustain/Enable, Slope)
//     leben als einzelne Bits in Marf248::stageModifier[] (Modul-internes
//     Array) - das ist KEIN von der Engine verwaltetes Param, VCV's
//     eingebautes history::ParamChange greift hier also nicht.
//   - Kontinuierliche Regler (Voltage/Time/Range/Curve) sind normale
//     ParamQuantities, dafuer genuegt eine Sammlung aus Standard-
//     history::ParamChange-Objekten in einer history::ComplexAction.
//
// Beide Pfade sind vollstaendig ueber VCV's Undo/Redo (Strg+Z) rueckgaengig
// machbar, in jeweils einem einzigen Schritt fuer den kompletten Broadcast
// (Kinovice-Entscheidung, da ein Broadcast potenziell 16 Stage-Werte
// gleichzeitig ueberschreibt).

// -- Bit-Modifier-Pfad -------------------------------------------------------
//
// Eigene history::Action-Unterklasse, die einen kompletten Snapshot des
// stageModifier[]-Arrays vor/nach dem Broadcast sichert (16 x int, billig)
// statt nur das einzelne Bit zu tracken - robuster, falls sich das Bit-Layout
// spaeter aendert, und deckt ohne Zusatzaufwand auch den (aktuell nicht
// vorkommenden) Fall ab, dass ein Broadcast mehrere Bits gleichzeitig
// betrifft. moduleId statt direktem Modul-Zeiger, da das Modul zwischen
// Undo/Redo-Aufrufen theoretisch entfernt/neu erzeugt werden koennte (Rack-
// Standardmuster, siehe history::ParamChange::moduleId in Rack SDK).
struct StageModifierBroadcastAction : history::Action {
	int64_t moduleId = -1;
	int oldValues[NUM_STAGES] = {};
	int newValues[NUM_STAGES] = {};

	void undo() override {
		Marf248* m = dynamic_cast<Marf248*>(APP->engine->getModule(moduleId));
		if (!m) {
			return;
		}
		for (int i = 0; i < NUM_STAGES; i++) {
			m->stageModifier[i] = oldValues[i];
		}
	}

	void redo() override {
		Marf248* m = dynamic_cast<Marf248*>(APP->engine->getModule(moduleId));
		if (!m) {
			return;
		}
		for (int i = 0; i < NUM_STAGES; i++) {
			m->stageModifier[i] = newValues[i];
		}
	}
};

// Fuehrt den Broadcast eines einzelnen Modifier-Bits von sourceStage auf alle
// 16 Stages aus (uebernimmt den Bit-ZUSTAND der Quell-Stage: gesetzt -> bei
// allen setzen, geloescht -> bei allen loeschen) und erzeugt dabei den
// zugehoerigen Undo-Schritt. Kein Undo-Eintrag, falls sich nichts aendert
// (z.B. wenn ohnehin schon alle Stages denselben Zustand hatten) - vermeidet
// leere/wirkungslose History-Eintraege.
static void broadcastModifierBitToAllStages(Marf248* m, uint32_t bit, int sourceStage, const std::string& actionName) {
	if (!m || sourceStage < 0 || sourceStage >= NUM_STAGES) {
		return;
	}
	bool state = (m->stageModifier[sourceStage] & bit) != 0;

	StageModifierBroadcastAction* action = new StageModifierBroadcastAction;
	action->moduleId = m->id;
	action->name = actionName;
	for (int i = 0; i < NUM_STAGES; i++) {
		action->oldValues[i] = m->stageModifier[i];
	}

	for (int i = 0; i < NUM_STAGES; i++) {
		if (state) {
			m->stageModifier[i] |= bit;
		}
		else {
			m->stageModifier[i] &= ~bit;
		}
	}

	bool changed = false;
	for (int i = 0; i < NUM_STAGES; i++) {
		action->newValues[i] = m->stageModifier[i];
		if (action->newValues[i] != action->oldValues[i]) {
			changed = true;
		}
	}
	if (changed) {
		APP->history->push(action);
	}
	else {
		delete action;
	}
}

// Lookup-Tabelle: ordnet den Basis-ParamId einer Button-Familie (STAGE_
// PULSE1_PARAM usw.) dem zugehoerigen Modifier-Bit und einem Anzeigenamen
// fuers Menue zu. Bewusst NUR die per Session-Entscheidung freigegebenen
// neun einfachen Bit-Modifier (Pulse1/2, Skip, First/Last, Stop/Sustain/
// Enable, Slope) - Quant/QuantGlide/Normalize/Mute/Volt-Ext/Time-Ext/Shape
// bleiben aussen vor (nicht Teil dieser Session).
struct BroadcastBitInfo {
	int baseParamId;
	uint32_t bit;
	const char* label;
};
static const BroadcastBitInfo BROADCAST_BIT_TABLE[] = {
	{Marf248::STAGE_PULSE1_PARAM,  MOD_PULSE1,  "Pulse 1"},
	{Marf248::STAGE_PULSE2_PARAM,  MOD_PULSE2,  "Pulse 2"},
	{Marf248::STAGE_SKIP_PARAM,    MOD_SKIP,    "Skip"},
	{Marf248::STAGE_STOP_PARAM,    MOD_STOP,    "Stop"},
	{Marf248::STAGE_SUSTAIN_PARAM, MOD_SUSTAIN, "Sustain"},
	{Marf248::STAGE_ENABLE_PARAM,  MOD_ENABLE,  "Enable"},
	{Marf248::STAGE_FIRST_PARAM,   MOD_FIRST,   "First"},
	{Marf248::STAGE_LAST_PARAM,    MOD_LAST,    "Last"},
	{Marf248::STAGE_SLOPE_PARAM,   MOD_SLOPED,  "Slope"},
};
static const int BROADCAST_BIT_TABLE_SIZE = sizeof(BROADCAST_BIT_TABLE) / sizeof(BROADCAST_BIT_TABLE[0]);

// -- Kontinuierliche-Regler-Pfad (Voltage/Time/Range/Curve) ------------------
//
// Anders als bei den Bit-Modifiern sind das normale, von der Engine
// verwaltete ParamQuantities - dafuer reicht eine Sammlung aus Standard-
// history::ParamChange-Objekten (Rack-Bordmittel) in einer
// history::ComplexAction, keine eigene Action-Klasse noetig. Wert wird ueber
// APP->engine->setParamValue() gesetzt (Engine-Thread-sicherer Weg), nicht
// per direktem ParamQuantity::setValue() wie bei RangeKnob/TrangeButton
// oben - dort unkritisch (einzelner Sofortwert), hier bewusst konsistent
// zum Engine-Setter, da 16 Werte in einer Aktion gesetzt werden.
static void broadcastParamToAllStages(Marf248* m, int baseParamId, int sourceStage, const std::string& actionName) {
	if (!m || sourceStage < 0 || sourceStage >= NUM_STAGES) {
		return;
	}
	float value = m->params[baseParamId + sourceStage].getValue();

	history::ComplexAction* complexAction = new history::ComplexAction;
	complexAction->name = actionName;
	bool any = false;
	for (int i = 0; i < NUM_STAGES; i++) {
		int paramId = baseParamId + i;
		float oldValue = m->params[paramId].getValue();
		if (oldValue == value) {
			continue;
		}
		history::ParamChange* h = new history::ParamChange;
		h->moduleId = m->id;
		h->paramId = paramId;
		h->oldValue = oldValue;
		h->newValue = value;
		complexAction->push(h);
		APP->engine->setParamValue(m, paramId, value);
		any = true;
	}
	if (any) {
		APP->history->push(complexAction);
	}
	else {
		delete complexAction;
	}
}


// Einfache helle Panel-Hintergrundflaeche im Buchla/Tiptop-Silber-Look, da
// bisher kein SVG-Panel gesetzt ist und das Modul sonst transparent bleibt.
struct PanelBackground : Widget {
	NVGcolor color = nvgRGB(228, 228, 226);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, color);
		nvgFill(args.vg);
	}
};


// Rein dekorativer Rahmen um eine Stage-Spalte. TransparentWidget statt
// OpaqueWidget, damit er keine Maus-Events abfaengt und die Knoepfe/Regler
// darunter weiterhin normal klickbar bleiben.
struct StageFrame : TransparentWidget {
	NVGcolor strokeColor = nvgRGBA(90, 90, 100, 170);
	float cornerRadius = 3.f;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, cornerRadius);
		nvgStrokeColor(args.vg, strokeColor);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}
};


// Duenne horizontale Trennlinie, um funktional zusammengehoerige Buchsen-
// Gruppen (z.B. Start/Stop, Programmed Outputs, External Inputs) optisch
// voneinander abzugrenzen, ohne dafuer extra Panel-Text/Rahmen zu brauchen.
// Gleiche Farbe/Optik wie StageFrame, damit es sich stimmig einfuegt.
struct HDivider : TransparentWidget {
	NVGcolor strokeColor = nvgRGBA(90, 90, 100, 170);

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0.f, 0.5f);
		nvgLineTo(args.vg, box.size.x, 0.5f);
		nvgStrokeColor(args.vg, strokeColor);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}
};


// Zyklus-Taster fuer Time Range (TR1-4, siehe MOD_TRANGE_*-Bits). Erbt von
// LEDButton nur die Klick-/Trigger-Mechanik (SvgSwitch-Basis unveraendert -
// Mausereignisse haengen nicht an draw()), ersetzt aber draw() komplett durch
// eine eigene, groesser dimensionierte Darstellung: die kleine Original-LED-
// Button-Grafik liess die Ziffer 1-4 kaum lesbar werden (Kinovice-Feedback),
// daher wird hier bewusst NICHT LEDButton::draw() aufgerufen (das wuerde die
// zur vergroesserten box.size nicht mehr passende Original-SVG zeichnen),
// sondern Hintergrund+Ziffer komplett per NVG selbst gezeichnet - gleiche
// Technik wie StageFrame/PanelBackground weiter oben in dieser Datei, kein
// SVG-Asset noetig. Die tatsaechliche Groesse (9x9mm statt LEDButton-Default)
// wird im Widget-Konstruktor gesetzt, siehe dortiger Kommentar.
// Live-BPM-Anzeige (BPM-Feature): reiner Text-Readout ohne eigene
// Interaktion (kein ParamWidget) - liest displayBpmA/B direkt vom Modul,
// die dort dekimiert (stageButtonDivider-Rhythmus) aktualisiert werden,
// siehe Marf248::process(). Erstes staendig sichtbares Live-NVG-Textfeld
// auf dem Hauptpanel (bisher nur Tooltips wie bei StageTimeQuantity/
// TempoQuantity) - siehe Projektnotizen zum anstehenden Panel-Labeling.
// Gleiche Zeichentechnik wie TrangeButton (Outline-Trick: Text 4x leicht
// versetzt in Dunkel, dann obenauf in Amber), aber deutlich kleinere
// Schrift, da hier eine mehrstellige Zahl statt einer einzelnen Ziffer
// dargestellt wird. module bleibt nullptr im Rack-Browser-Vorschaubild
// (kein Modul instanziiert) - draw() faengt das ab und zeigt dann einfach
// TEMPO_BASE_BPM als Platzhalter.
struct TempoBpmDisplay : TransparentWidget {
	Marf248* module = nullptr;
	int section = 0; // 0 = Section A, 1 = Section B

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 1.5f);
		nvgFillColor(args.vg, nvgRGB(40, 40, 44));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font || !font->handle) {
			return;
		}
		float bpm = module ? (section == 0 ? module->displayBpmA : module->displayBpmB) : TEMPO_BASE_BPM;
		std::string text = string::f("%.1f", bpm);

		float cx = box.size.x / 2.f;
		float cy = box.size.y / 2.f;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 15.f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		nvgFillColor(args.vg, nvgRGB(20, 20, 20));
		nvgText(args.vg, cx - 0.8f, cy, text.c_str(), NULL);
		nvgText(args.vg, cx + 0.8f, cy, text.c_str(), NULL);
		nvgText(args.vg, cx, cy - 0.8f, text.c_str(), NULL);
		nvgText(args.vg, cx, cy + 0.8f, text.c_str(), NULL);

		nvgFillColor(args.vg, nvgRGB(235, 170, 60));
		nvgText(args.vg, cx, cy, text.c_str(), NULL);
	}
};

// Ersetzt (nur fuer die neun in BROADCAST_BIT_TABLE gelisteten Modifier-
// Familien, siehe dort) den bisher verwendeten nackten LEDButton. Rein
// visuell/interaktiv identisch mit LEDButton (keine draw()-Aenderung, kein
// eigenes Rendering) - fuegt lediglich per appendContextMenu() den
// Rechtsklick-Eintrag "Apply to all stages" hinzu. Reine Widget-Klassen-
// Umstellung, keine Aenderung an ParamId/Enum-Reihenfolge, daher keine
// bestehenden Patch-Kabel betroffen.
struct StageModBroadcastButton : LEDButton {
	void appendContextMenu(ui::Menu* menu) override {
		ParamQuantity* pq = getParamQuantity();
		if (!pq) {
			return;
		}
		Marf248* m = dynamic_cast<Marf248*>(pq->module);
		if (!m) {
			return;
		}
		for (int t = 0; t < BROADCAST_BIT_TABLE_SIZE; t++) {
			const BroadcastBitInfo& info = BROADCAST_BIT_TABLE[t];
			if (pq->paramId < info.baseParamId || pq->paramId >= info.baseParamId + NUM_STAGES) {
				continue;
			}
			int stageIndex = pq->paramId - info.baseParamId;
			menu->addChild(new ui::MenuSeparator);
			menu->addChild(createMenuItem(
				string::f("Apply %s to all stages", info.label), "",
				[=]() {
					broadcastModifierBitToAllStages(m, info.bit, stageIndex,
						string::f("apply Stage %d %s to all stages", stageIndex + 1, info.label));
				}
			));
			return;
		}
	}
};


struct TrangeButton : LEDButton {
	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 2.5f);
		nvgFillColor(args.vg, nvgRGB(40, 40, 44));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		ParamQuantity* pq = getParamQuantity();
		if (!pq) {
			return;
		}
		Marf248* m = dynamic_cast<Marf248*>(pq->module);
		if (!m) {
			return;
		}
		int stageIndex = pq->paramId - Marf248::STAGE_TRANGE_PARAM;
		if (stageIndex < 0 || stageIndex >= NUM_STAGES) {
			return;
		}
		int idx = getTimeRangeIndex(m->stageModifier[stageIndex]);

		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font || !font->handle) {
			return;
		}
		static const char* labels[4] = {"1", "2", "3", "4"};
		float cx = box.size.x / 2.f;
		float cy = box.size.y / 2.f;
		nvgFontFaceId(args.vg, font->handle);
		// Gleiche Schriftgroesse/Optik wie RangeKnob (fester Wert 18px statt
		// relativ zur box.size, siehe dortiger Kommentar): Outline-Trick,
		// Text 4x leicht versetzt in Dunkel, dann obenauf in Amber - selbe
		// zwei Farben, selbe Versatzwerte, damit beide Anzeigen im Panel
		// einheitlich wirken.
		nvgFontSize(args.vg, 18.f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		nvgFillColor(args.vg, nvgRGB(20, 20, 20));
		nvgText(args.vg, cx - 0.8f, cy, labels[idx], NULL);
		nvgText(args.vg, cx + 0.8f, cy, labels[idx], NULL);
		nvgText(args.vg, cx, cy - 0.8f, labels[idx], NULL);
		nvgText(args.vg, cx, cy + 0.8f, labels[idx], NULL);

		nvgFillColor(args.vg, nvgRGB(235, 170, 60));
		nvgText(args.vg, cx, cy, labels[idx], NULL);
	}

	// Direkte Auswahl per Rechtsklick-Menue, aus demselben Grund wie bei
	// RangeKnob::appendContextMenu (siehe dortiger Kommentar) - bei einem
	// Momentary-Taster wie diesem macht eine numerische Texteingabe ohnehin
	// wenig Sinn (kein "Wert" im ueblichen Sinn, nur ein Zyklus-Trigger).
	// Manipuliert daher bewusst direkt die stageModifier-Bits statt ueber
	// den Param-Rohwert/SchmittTrigger zu gehen - klar, sofort wirksam,
	// unabhaengig vom Momentary-Verhalten des zugrundeliegenden Buttons.
	void appendContextMenu(ui::Menu* menu) override {
		ParamQuantity* pq = getParamQuantity();
		if (!pq) {
			return;
		}
		Marf248* m = dynamic_cast<Marf248*>(pq->module);
		if (!m) {
			return;
		}
		int stageIndex = pq->paramId - Marf248::STAGE_TRANGE_PARAM;
		if (stageIndex < 0 || stageIndex >= NUM_STAGES) {
			return;
		}
		menu->addChild(new ui::MenuSeparator);
		static const char* trLabels[4] = {"TR1 (.002-.03s)", "TR2 (.02-.3s)", "TR3 (.2-3s, Default)", "TR4 (2-30s)"};
		// 0 fuer TR3 (Index 2), da TR3 durch Abwesenheit aller drei Bits
		// kodiert wird (siehe MOD_TRANGE_*-Kommentar in Marf248.hpp).
		static const int trBits[4] = {MOD_TRANGE_1, MOD_TRANGE_2, 0, MOD_TRANGE_4};
		for (int t = 0; t < 4; t++) {
			menu->addChild(createCheckMenuItem(trLabels[t], "",
				[=]() { return getTimeRangeIndex(m->stageModifier[stageIndex]) == t; },
				[=]() {
					m->stageModifier[stageIndex] &= ~(MOD_TRANGE_1 | MOD_TRANGE_2 | MOD_TRANGE_4);
					m->stageModifier[stageIndex] |= trBits[t];
				}
			));
		}
	}
};


// Grosser Drehregler (gleiche Groesse/Optik wie die Voltage/Time-CV-Knoepfe)
// fuer den Range-Modifier. Zeigt zusaetzlich eine kurze Text-Abkuerzung des
// aktuellen Zustands direkt auf dem Knopf an ("F", "H", "L0".."L8"), damit
// der Zustand auch ohne Tooltip/LEDs sofort ablesbar ist. Die Abkuerzung wird
// nach dem eigentlichen Knopf gezeichnet und liegt daher immer aufrecht und
// unrotiert obenauf - nur die SVG-Grafik des Knopfes selbst rotiert intern.
struct RangeKnob : RoundBlackKnob {
	void draw(const DrawArgs& args) override {
		RoundBlackKnob::draw(args);

		ParamQuantity* pq = getParamQuantity();
		if (!pq)
			return;

		float cx = box.size.x / 2.f;
		float cy = box.size.y / 2.f;

		// Farbiger Zeiger (Projektnotiz "RangeKnob-Zeigerfarbe"): der
		// Original-Zeiger ist Teil der RoundBlackKnob-SVG-Grafik selbst und
		// laesst sich nicht direkt einfaerben. Stattdessen zeichnen wir eine
		// eigene Linie exakt mit demselben Rotationswinkel wie die
		// Basisklasse (minAngle/maxAngle sind von SvgKnob geerbte, oeffentliche
		// Member) obenauf - deckt den Original-Zeiger vollstaendig ab, statt
		// ihn nur zu korrigieren.
		float angle = rack::rescale(pq->getValue(), pq->getMinValue(), pq->getMaxValue(), minAngle, maxAngle);
		float radius = box.size.x / 2.f;
		// Zeiger beginnt erst bei halbem Radius (statt nah an der Mitte),
		// damit er die Text-Abkuerzung in der Knopfmitte nicht verdeckt.
		float innerR = radius * 0.5f;
		float outerR = radius * 0.85f;

		nvgSave(args.vg);
		nvgTranslate(args.vg, cx, cy);
		nvgRotate(args.vg, angle);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0.f, -innerR);
		nvgLineTo(args.vg, 0.f, -outerR);
		nvgLineCap(args.vg, NVG_ROUND);
		nvgStrokeWidth(args.vg, 2.4f);
		nvgStrokeColor(args.vg, nvgRGB(235, 170, 60));
		nvgStroke(args.vg);
		nvgRestore(args.vg);

		static const char* labels[7] = {"F", "H", "L0", "L2", "L4", "L6", "L8"};
		int pos = (int) std::round(pq->getValue());
		if (pos < 0 || pos >= 7)
			return;

		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font || !font->handle)
			return;

		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 18.f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		// Duennes dunkles Outline (Text 4x leicht versetzt in Schwarz, dann
		// hell obenauf), damit die Schrift auch auf hellen Knopf-Bereichen
		// lesbar bleibt und nicht nur auf dem dunklen Knopfkoerper.
		nvgFillColor(args.vg, nvgRGB(20, 20, 20));
		nvgText(args.vg, cx - 0.8f, cy, labels[pos], NULL);
		nvgText(args.vg, cx + 0.8f, cy, labels[pos], NULL);
		nvgText(args.vg, cx, cy - 0.8f, labels[pos], NULL);
		nvgText(args.vg, cx, cy + 0.8f, labels[pos], NULL);

		nvgFillColor(args.vg, nvgRGB(235, 170, 60));
		nvgText(args.vg, cx, cy, labels[pos], NULL);
	}

	// Direkte Auswahl per Rechtsklick-Menue: die normale Texteingabe im
	// Standard-Kontextmenue (Rack-Bordmittel) hat sich fuer diesen Regler als
	// unzuverlaessig herausgestellt (Kinovice-Feedback: Eingabe moeglich,
	// wird aber nicht uebernommen) - vermutlich eine Rack-Eigenheit bei
	// snapEnabled-Reglern mit eigener ParamQuantity-Anzeigeklasse. Diese
	// Checklisten-Variante ruft direkt setValue() aus einem expliziten
	// Menu-Klick auf, was zuverlaessig funktioniert und obendrein die
	// moeglichen Zustaende klar benannt zur Auswahl stellt statt eine
	// nackte Zahl 0-6 eintippen zu muessen.
	void appendContextMenu(ui::Menu* menu) override {
		ParamQuantity* pq = getParamQuantity();
		if (!pq) {
			return;
		}
		menu->addChild(new ui::MenuSeparator);
		static const char* fullLabels[7] = {"Full", "Half", "Limited +0", "Limited +2", "Limited +4", "Limited +6", "Limited +8"};
		for (int i = 0; i < 7; i++) {
			menu->addChild(createCheckMenuItem(fullLabels[i], "",
				[=]() { return (int) std::round(pq->getValue()) == i; },
				[=]() { pq->setValue((float) i); }
			));
		}

		// Stage-Number-Broadcast (Session "Stage-Number-Broadcast"): Range
		// gehoert zu den per Kinovice-Entscheidung freigegebenen
		// kontinuierlichen Reglern.
		Marf248* m = dynamic_cast<Marf248*>(pq->module);
		int stageIndex = pq->paramId - Marf248::STAGE_RANGE_PARAM;
		if (m && stageIndex >= 0 && stageIndex < NUM_STAGES) {
			menu->addChild(new ui::MenuSeparator);
			menu->addChild(createMenuItem("Apply Range to all stages", "",
				[=]() {
					broadcastParamToAllStages(m, Marf248::STAGE_RANGE_PARAM, stageIndex,
						string::f("apply Stage %d Range to all stages", stageIndex + 1));
				}
			));
		}
	}
};


// Vertikaler Schieberegler fuer Voltage/Time (Session "Hauptmodul-Relayout",
// ersetzt die bisherigen RoundBlackKnob-Drehregler). Komplett per NVG
// gezeichnet, kein SVG-Asset (siehe PanelBackground/RangeKnob/TrangeButton).
//
// ZWEITER ANLAUF beim Interaktions-Verhalten (Kinovice-Feedback: "Slider
// laesst sich nicht fassen/bedienen, Klick bewegt stattdessen das ganze
// Modul" - blieb auch nach dem ersten Fix-Versuch, box.size im Konstruktor
// zu setzen, bestehen). Erbte zuvor von app::Knob in der Annahme, dessen
// eingebautes Drag-Verhalten liesse sich 1:1 fuer einen linearen Fader
// wiederverwenden (vertikale Mausbewegung aendert den Wert, exakt wie bei
// einem Fader gewuenscht) - vermutlich haengt Knob's Interaktions-Pipeline
// aber an internen Annahmen (z.B. einem SVG-Kind-Widget aus SvgKnob), die
// bei einem nackten Knob ohne SVG nicht erfuellt sind, sodass der Klick nie
// beim Widget selbst ankam. Jetzt stattdessen direkt von ParamWidget
// abgeleitet, mit komplett selbst geschriebenem onButton/onDragStart/
// onDragMove/onDragEnd - keine Abhaengigkeit mehr von Knob-internem
// Verhalten, nur noch Rack-Grundmechanik (Event-Konsum bei PRESS, dann
// direkte Drag-Events an genau dieses Widget).
struct StageSlider : ParamWidget {
	// 48 -> 50mm Gesamthoehe (Kinovice-Feedback, zweite Runde: "Slider noch
	// etwas hoeher"), TRAVEL_MM entsprechend 42 -> 44mm bei gleichem 6mm-
	// Griffpuffer.
	static constexpr float WIDTH_MM = 8.f;
	static constexpr float TRAVEL_MM = 44.f;
	static constexpr float HANDLE_H_MM = 6.f;
	static constexpr float HEIGHT_MM = TRAVEL_MM + HANDLE_H_MM;

	StageSlider() {
		box.size = mm2px(Vec(WIDTH_MM, HEIGHT_MM));
	}

	void draw(const DrawArgs& args) override {
		float t = 0.5f;
		ParamQuantity* pq = getParamQuantity();
		if (pq) {
			t = rack::rescale(pq->getValue(), pq->getMinValue(), pq->getMaxValue(), 0.f, 1.f);
			t = math::clamp(t, 0.f, 1.f);
		}

		float w = box.size.x;
		float travelPx = mm2px(TRAVEL_MM);
		float handleHPx = mm2px(HANDLE_H_MM);
		float railTop = handleHPx / 2.f;
		float railBottom = railTop + travelPx;
		// t=1 (Maximalwert) -> Griff oben, t=0 -> Griff unten (physisches
		// Fader-Gefuehl: hoch = mehr, wie bei einem Mischpult-Fader).
		float handleY = railBottom - t * travelPx;
		float railX = w / 2.f;

		// Gehaeuse.
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.f, box.size.y - 1.f, 2.f);
		nvgFillColor(args.vg, nvgRGB(40, 40, 44));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		// Nut (dunkel).
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, railX, railTop);
		nvgLineTo(args.vg, railX, railBottom);
		nvgLineCap(args.vg, NVG_ROUND);
		nvgStrokeWidth(args.vg, 3.f);
		nvgStrokeColor(args.vg, nvgRGB(20, 20, 22));
		nvgStroke(args.vg);

		// Fuellstand von unten bis zum Griff (Amber, gleiche Akzentfarbe wie
		// RangeKnob-Zeiger/TrangeButton-Ziffer, fuer ein einheitliches
		// Erscheinungsbild aller "aktiven" Anzeigeelemente im Panel).
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, railX, railBottom);
		nvgLineTo(args.vg, railX, handleY);
		nvgLineCap(args.vg, NVG_ROUND);
		nvgStrokeWidth(args.vg, 3.f);
		nvgStrokeColor(args.vg, nvgRGB(235, 170, 60));
		nvgStroke(args.vg);

		// Griff.
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 1.5f, handleY - handleHPx / 2.f, w - 3.f, handleHPx, 1.2f);
		nvgFillColor(args.vg, nvgRGB(220, 220, 216));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(60, 60, 64));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
		// Mittellinie entfernt (Kinovice-Feedback: stoerte die Lesbarkeit der
		// neuen Wertanzeige, siehe direkt darunter).

		// Gerundeter Wert im Griff (Kinovice-Wunsch): nutzt pq->getDisplayValue()
		// statt getValue() direkt - liefert fuer STAGE_TIME_PARAM automatisch
		// die 0-10-Dial-Skala (StageTimeQuantity::getDisplayValue() rechnet
		// intern *10 um, siehe dortige Definition), fuer STAGE_VOLTAGE_PARAM
		// mangels eigenem Override den rohen 0-10V-Wert - beide landen also
		// einheitlich auf einer 0-10-Skala, exakt wie am Buchla-Original
		// aufgedruckt. Bewusst OHNE Ruecksicht auf Quant/Ext-Sondermodi (Noten-
		// name/Kanalwahl) - das wuerde die Zahl in den seltenen Sonderfaellen
		// verfaelschen, aber eine Notenname-Kuerzung waere in 8mm Griffbreite
		// ohnehin nicht lesbar; Quant/Ext-Zustand ist bereits per eigenem
		// Panel-Light sichtbar. Aufgerundet auf ganze Zahl ("%.0f"), da selbst
		// eine Nachkommastelle bei zwei Ziffern (z.B. "10.0") in den 8mm nicht
		// mehr zuverlaessig passt (Kinovice-Wunsch "gerundet, damit es passt").
		// Optik (Font/Outline-Technik) von TrangeButton uebernommen, Farben
		// aber bewusst UMGEKEHRT (Kinovice-Feedback nach Test: schwarzer
		// Rand/goldene Fuellung war auf dem hellen Griff schwer lesbar) -
		// hier also goldener Rand, schwarze Fuellung. fontSize=15 (nach
		// Kinovice-Feedback "koennten noch etwas groesser sein", erste
		// Fassung war 12) mit proportional angepasstem Outline-Versatz
		// (0.75 statt 0.6) - beides weiterhin kleiner als TrangeButton
		// (18px/0.8), da der Griff (8x6mm) deutlich kleiner ist als der
		// TrangeButton (9x9mm). Bei zwei Ziffern ("10") auf Randschaerfe
		// pruefen, falls es im 8mm-Griff eng wird.
		ParamQuantity* pqDisplay = getParamQuantity();
		if (pqDisplay) {
			std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
			if (font && font->handle >= 0) {
				std::string label = string::f("%.0f", pqDisplay->getDisplayValue());
				nvgFontFaceId(args.vg, font->handle);
				nvgFontSize(args.vg, 15.f);
				nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

				nvgFillColor(args.vg, nvgRGB(235, 170, 60));
				nvgText(args.vg, railX - 0.75f, handleY, label.c_str(), NULL);
				nvgText(args.vg, railX + 0.75f, handleY, label.c_str(), NULL);
				nvgText(args.vg, railX, handleY - 0.75f, label.c_str(), NULL);
				nvgText(args.vg, railX, handleY + 0.75f, label.c_str(), NULL);

				nvgFillColor(args.vg, nvgRGB(20, 20, 20));
				nvgText(args.vg, railX, handleY, label.c_str(), NULL);
			}
		}
	}

	// --- Interaktion, komplett selbst implementiert (siehe Kommentar oben). ---

	void onButton(const ButtonEvent& e) override {
		ParamWidget::onButton(e);
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
		}
	}

	void onDragStart(const DragStartEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		APP->window->cursorLock();
	}

	void onDragEnd(const DragEndEvent& e) override {
		APP->window->cursorUnlock();
	}

	void onDragMove(const DragMoveEvent& e) override {
		ParamQuantity* pq = getParamQuantity();
		if (!pq)
			return;
		float travelPx = mm2px(TRAVEL_MM);
		float range = pq->getMaxValue() - pq->getMinValue();
		// Maus nach oben (negatives mouseDelta.y) = Wert steigt - deckungs-
		// gleich mit der visuellen Zuordnung in draw() oben. 1:1-Kopplung an
		// den tatsaechlichen Griffweg (travelPx) statt einer festen, von
		// Knob geerbten Drehregler-Sensitivitaet - ein Mausweg exakt in
		// Hoehe des Reglerwegs faehrt den kompletten Wertebereich, wie bei
		// einem echten Fader erwartet.
		float delta = -e.mouseDelta.y / travelPx * range;
		float newVal = math::clamp(pq->getValue() + delta, pq->getMinValue(), pq->getMaxValue());
		pq->setValue(newVal);
	}

	void onDoubleClick(const DoubleClickEvent& e) override {
		ParamQuantity* pq = getParamQuantity();
		if (pq)
			pq->reset();
	}

	// Stage-Number-Broadcast (Session "Stage-Number-Broadcast"): StageSlider
	// wird sowohl fuer Voltage als auch fuer Time verwendet (siehe
	// Marf248Widget-Konstruktor) - welche der beiden Familien vorliegt, wird
	// hier rein anhand des ParamId-Bereichs unterschieden, kein eigenes
	// Member noetig.
	void appendContextMenu(ui::Menu* menu) override {
		ParamQuantity* pq = getParamQuantity();
		if (!pq) {
			return;
		}
		Marf248* m = dynamic_cast<Marf248*>(pq->module);
		if (!m) {
			return;
		}
		int baseParamId = -1;
		const char* label = nullptr;
		if (pq->paramId >= Marf248::STAGE_VOLTAGE_PARAM && pq->paramId < Marf248::STAGE_VOLTAGE_PARAM + NUM_STAGES) {
			baseParamId = Marf248::STAGE_VOLTAGE_PARAM;
			label = "Voltage";
		}
		else if (pq->paramId >= Marf248::STAGE_TIME_PARAM && pq->paramId < Marf248::STAGE_TIME_PARAM + NUM_STAGES) {
			baseParamId = Marf248::STAGE_TIME_PARAM;
			label = "Time";
		}
		if (baseParamId < 0) {
			return;
		}
		int stageIndex = pq->paramId - baseParamId;
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuItem(string::f("Apply %s to all stages", label), "",
			[=]() {
				broadcastParamToAllStages(m, baseParamId, stageIndex,
					string::f("apply Stage %d %s to all stages", stageIndex + 1, label));
			}
		));
	}
};


// Ring-Light fuer Pulse-Output-Buchsen (Manual "Output LEDs"): statt eines
// separaten Punkts neben dem Jack wird die Modul-Light-Farbe als duenner
// Ring direkt UM die Buchse herum gezeichnet. Wird exakt auf die gleiche
// Position wie der zugehoerige PJ301MPort gelegt (siehe Marf248Widget); die
// Box ist bewusst groesser als der sichtbare Jack-Durchmesser, damit der
// Ring komplett ausserhalb des Steckerkoerpers sichtbar bleibt, unabhaengig
// von der Zeichenreihenfolge. Blinkt automatisch im Rhythmus des P1/P2-
// Ausgabepulses, da dieser ohnehin nur kurz (1ms) ansteht (siehe
// Afg::process()) - keine zusaetzliche Blink-Logik noetig.
struct PulseRingLight : app::ModuleLightWidget {
	PulseRingLight() {
		box.size = mm2px(Vec(10.f, 10.f));
		addBaseColor(SCHEME_RED);
	}
	void drawBackground(const DrawArgs& args) override {}
	void drawHalo(const DrawArgs& args) override {}
	void drawLight(const DrawArgs& args) override {
		float radius = box.size.x / 2.f;
		float strokeWidth = 1.6f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, radius, radius, radius - strokeWidth / 2.f);
		nvgStrokeWidth(args.vg, strokeWidth);
		nvgStrokeColor(args.vg, color);
		nvgStroke(args.vg);
	}
};


// Anmerkung: Ein frueherer Versuch, Status-LEDs an Toggle-Buttons (Mode/
// Pendulum/Stage-Address/Load/Save/Scale/Key) als duenner Ring UM den Button
// herum darzustellen (eigener ButtonRingLight-Widget), war in der Praxis
// kaum sichtbar (zu duenner Stroke, zu nah am Button-Rand). Ersetzt durch
// das bewaehrte Muster der Stage-Modifier-Buttons: ein normales SmallLight
// exakt zentriert AUF dem Button (siehe unten), das wie ein leuchtender
// Punkt in der Taste selbst erscheint - deutlich sichtbarer.





// ---------------------------------------------------------------------
// Panel-Beschriftungs-System (Session "Panel Labeling", Fortsetzung).
// Gleiches Prinzip wie bei den beiden Expandern (siehe
// Marf248TriggerExpander.cpp fuer die ausfuehrliche Begruendung/Historie
// inkl. Font-Groessen-Korrektur), lokale Kopie hier aus demselben ODR-Grund
// (kein gemeinsamer Header fuer dieses triviale, headerlose Widget).
//
// KORREKTUR nach Testlauf (Kinovice-Feedback): eine erste, deutlich
// umfangreichere Fassung beschriftete auch Controls- und Jacks-Spalte
// (Time Multiplier, Run/Mode/Pendulum, Stage-Address, Start/Stop,
// Pulse-Outs usw.) mit kleinen seitlichen Labels. Bei dem dort sehr engen
// Rastermass (6-13mm Zeilenabstand) wirkten diese Labels zu klein und
// unruhig - wieder entfernt, dort reicht der Tooltip (Hover ueber
// Regler/Buchse). Beschriftet bleiben nur noch: Ext-Spalte (A-D),
// VOUT/TOUT-Ausgangsspalte, sowie "PRESETS"/"PRESET NR" (vertikale
// Spaltennamen) und die 12 Preset-Ziffern.
//
// Zwei Label-Varianten:
//   - addLabelBelow(): Label sitzt unter dem Element (Ext-Spalte: 22mm
//     Pitch; VOUT/TOUT-Ausgangsspalte: 12mm Pitch).
//   - addLabelSide(): Label sitzt SEITLICH auf gleicher Hoehe wie das
//     Element (Preset-Ziffern, links vom Taster).
//   - addLabelVertical(): Text 90 Grad gedreht, fuer die beiden
//     Spaltennamen "PRESETS"/"PRESET NR" rechts neben ihrem jeweiligen
//     Block.
// PanelLabel-Struct, DEG_TO_RAD und COLOR_SYSTEM/COLOR_SECTION_A/
// COLOR_SECTION_B/COLOR_STAGEMOD kommen jetzt zentral aus PanelLabel.hpp
// (Session "Fehlende Labels auf Trigger Expander": die drei Module hatten
// bisher je eine eigene lokale PanelLabel-Kopie, die hier in Marf248.cpp
// um rotationDeg erweitert war, in den beiden Expandern aber nicht -
// klassischer ODR-Verstoss mit undefiniertem Verhalten je nach Link-
// Reihenfolge, siehe PanelLabel.hpp fuer die volle Erklaerung). Die
// COLOR_MUSICAL/COLOR_TEMPO-Entfernung von vorher bleibt unveraendert -
// COLOR_MUSICAL wird nur vom Trigger Expander gebraucht und ist dort
// lokal geblieben.


// Stage-Number-Broadcast (Session "Stage-Number-Broadcast"): duenner
// Trimpot-Wrapper nur fuer den Curve-Modifier (STAGE_CURVE_PARAM), der
// bisher als nackter Trimpot ohne eigene Widget-Klasse verwendet wurde
// (siehe Marf248Widget-Konstruktor). Wie StageModBroadcastButton/RangeKnob/
// StageSlider oben: keine draw()-Aenderung, appendContextMenu() haengt sich
// lediglich an Racks Standard-Kontextmenue (Fine Adjust/Initialize etc.) an
// und ergaenzt den Broadcast-Eintrag.
struct StageCurveTrimpot : Trimpot {
	void appendContextMenu(ui::Menu* menu) override {
		ParamQuantity* pq = getParamQuantity();
		if (!pq) {
			return;
		}
		Marf248* m = dynamic_cast<Marf248*>(pq->module);
		if (!m) {
			return;
		}
		int stageIndex = pq->paramId - Marf248::STAGE_CURVE_PARAM;
		if (stageIndex < 0 || stageIndex >= NUM_STAGES) {
			return;
		}
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuItem("Apply Curve to all stages", "",
			[=]() {
				broadcastParamToAllStages(m, Marf248::STAGE_CURVE_PARAM, stageIndex,
					string::f("apply Stage %d Curve to all stages", stageIndex + 1));
			}
		));
	}
};

// BEATS_FACTOR Auto-Modus (Segmentlaengen-Sync, Session "BEATS_FACTOR
// Auto-Modus"): duenner Trimpot-Wrapper fuer BEATS_FACTOR_A/B_PARAM, wie
// StageCurveTrimpot oben. Ergaenzt Racks Standard-Kontextmenue um einen
// Checkbox-Eintrag zum Umschalten auf Auto (BEATS_FACTOR wird dann aus
// der Segmentlaenge um die aktuell adressierte Stage abgeleitet statt aus
// diesem Trimpot-Wert, siehe Marf248::process()/Afg::updateAutoSegment()
// in Marf248.hpp; BEATS_FACTOR_CV_A/B_INPUT bleibt dabei additiv wirksam,
// nur der Trimpot-Wert selbst wird ignoriert). Der Trimpot-Wert bleibt
// bei aktivem Auto unveraendert gespeichert/anzeigbar - er wird nur im
// Signalpfad ignoriert, damit beim Zurueckschalten auf manuell sofort
// wieder der zuletzt eingestellte Wert greift (kein Datenverlust).
struct BeatsFactorTrimpot : Trimpot {
	void appendContextMenu(ui::Menu* menu) override {
		ParamQuantity* pq = getParamQuantity();
		if (!pq) {
			return;
		}
		Marf248* m = dynamic_cast<Marf248*>(pq->module);
		if (!m) {
			return;
		}
		bool isA = (pq->paramId == Marf248::BEATS_FACTOR_A_PARAM);
		bool isB = (pq->paramId == Marf248::BEATS_FACTOR_B_PARAM);
		if (!isA && !isB) {
			return;
		}
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createCheckMenuItem("Auto (Segment Length)", "",
			[=]() { return isA ? m->beatsFactorAutoA : m->beatsFactorAutoB; },
			[=]() {
				bool& flag = isA ? m->beatsFactorAutoA : m->beatsFactorAutoB;
				flag = !flag;
				// Beim Einschalten den Segment-Cache invalidieren, damit
				// sofort neu ermittelt wird, statt einen evtl. veralteten
				// Stand aus einer frueheren Auto-Phase weiterzuverwenden
				// (siehe Afg::updateAutoSegment()-Kommentar in Marf248.hpp).
				if (flag) {
					Afg& afg = isA ? m->afgA : m->afgB;
					afg.segAutoLastStage = -1;
				}
			}
		));
	}
};


struct Marf248Widget : ModuleWidget {
	// Label UNTER dem Element (Expander-Muster) - nur fuer Spalten mit
	// ausreichendem Zeilenabstand, siehe Kommentar oben.
	void addLabelBelow(Vec posMM, float yOffsetMM, const std::string& text, NVGcolor color, float fontSize = 9.f) {
		addChild(new PanelLabel(posMM + Vec(0.f, yOffsetMM), text, color, fontSize, NVG_ALIGN_CENTER | NVG_ALIGN_TOP));
	}

	// Label SEITLICH auf Element-Hoehe - braucht keinen vertikalen Platz.
	// dir=+1: Text laeuft nach rechts (Anker links vom Text), dir=-1:
	// Text laeuft nach links (Anker rechts vom Text). Aktuell nur noch
	// fuer die 12 Preset-Ziffern in Gebrauch (dir=-1, links vom Taster).
	void addLabelSide(Vec posMM, float xOffsetMM, float dir, const std::string& text, NVGcolor color, float fontSize = 6.5f) {
		int align = (dir >= 0.f) ? (NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE) : (NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		addChild(new PanelLabel(posMM + Vec(dir * xOffsetMM, 0.f), text, color, fontSize, align));
	}

	// Label vertikal (90 Grad gedreht) rechts neben dem Element, mittig
	// zentriert auf dessen Hoehe - Kinovice-Wunsch fuer die Preset-Ziffern:
	// dadurch braucht das Label keinen Zeilen-Pitch mehr (wie addLabelSide),
	// sondern nur noch schmale Breite, wodurch deutlich groessere Schrift
	// moeglich wird. -90 Grad = Text laeuft von unten nach oben, gaengige
	// Konvention fuer seitliche Panel-Beschriftung.
	void addLabelVertical(Vec posMM, float xOffsetMM, const std::string& text, NVGcolor color, float fontSize) {
		addChild(new PanelLabel(posMM + Vec(xOffsetMM, 0.f), text, color, fontSize,
		                        NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, -90.f));
	}

	Marf248Widget(Marf248* module) {
		setModule(module);

		// Modul insgesamt etwas breiter (120 statt 110 HP), damit die 16
		// Stage-Spalten mehr Luft bekommen - die Knoepfe/Buttons wirkten bei
		// 22mm Rahmenbreite zu dicht gedraengt.
		// 120 -> 126: 6HP mehr fuer die neue VOUT/TOUT-Ausgangsspalte pro
		// Section (siehe gapJacksToOutputs/gapOutputsToStage weiter unten).
		// Session "Hauptmodul-Relayout": die tatsaechlich benoetigte Breite
		// ist durch die Slider/Button-Umstellung deutlich gewachsen (siehe
		// gapOutputsToStage/xStep/frameWidthMM weiter unten) - HP musste
		// entsprechend mitgezogen werden. War in den ersten beiden
		// Session-Iterationen versehentlich als fixer Wert (126) stehen
		// geblieben, obwohl die Spalten-Positionen (colBControls, colPresets
		// etc.) laengst weiter rechts lagen - dadurch fiel Section B UND die
		// gesamte Preset-Spalte ausserhalb des sichtbaren Panel-Hintergrunds
		// (Kinovice-Feedback "ganz rechts fehlt die gesamte Sektion B").
		// 150 -> 146: die 30mm Randmarge des ersten Fixes war zu grosszuegig
		// (Kinovice-Feedback "rechts etwas zu breit, ca. um die Breite der
		// Preset-Spalte schmaler") - jetzt ca. 10mm Marge hinter der Preset-
		// Rahmenkante, naeher an den uebrigen, knapp bemessenen Luecken
		// dieses Panels.
		const float HP = 146.f;
		box.size = Vec(HP * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

		// Panel-Hintergrund + klare Aussenkante (sonst bleibt das Modul
		// transparent und "verschwimmt" mit der Rack-Oberflaeche dahinter).
		PanelBackground* panelBg = new PanelBackground;
		panelBg->box.size = box.size;
		addChild(panelBg);

		PanelBorder* panelBorder = new PanelBorder;
		panelBorder->box.size = box.size;
		addChild(panelBorder);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Modulname (oben links, im freien Streifen 0-12mm ueber den Rahmen;
		// links neben den Schrauben beginnend, weit weg von den Clear/Reset/
		// Auto-Fill-Tastern in der Panelmitte).
		// Modulname zentriert wie bei den anderen Modulen (Mitte = halbe Panelbreite in mm).
		const float titleCenterMM = HP * 5.08f / 2.f;
		addChild(new PanelLabel(Vec(titleCenterMM, 3.5f), "SOFONISBA", COLOR_SYSTEM, 13.f));

		// Geometrie-Konstanten fuer die Stage-Spalte.
		// Session "Hauptmodul-Relayout", zweiter Anlauf (Kinovice-Feedback
		// nach erstem Test): Buttons jetzt in 2 statt 4 Spalten (siehe
		// buttonColOffsets weiter unten) - dadurch wird die Stage-Spalte
		// insgesamt SCHMALER als im ersten Entwurf (32mm statt 46mm), obwohl
		// die Slider gleichzeitig LAENGER werden (siehe StageSlider::HEIGHT_MM
		// unten): der Shape/Curve/TRange/Range-Block zieht in dieselbe
		// schmale Button-Unterspalte um (statt wie vorher ueber die volle
		// Stage-Breite zu gehen), was der Slider-Spalte die komplette
		// Rahmenhoehe frei macht.
		const float xStep = 36.f;
		const float frameWidthMM = 32.f;
		const float frameTopMM = 12.f;
		const float frameBottomMM = 126.f;
		const float gapControlsToJacks = 19.f;
		const float gapJacksToOutputs = 18.f;
		// 22 -> 26: beim ersten Entwurf ragte die VOUT/TOUT-Ausgangsspalte
		// in die (damals auf 46mm verbreiterte) Stage-Spalte hinein
		// (Kinovice-Feedback "alle 16 Stages muessen nach rechts
		// verschoben werden"). Selbst nach der Rueckverschmalerung auf
		// 32mm braucht es mehr Abstand als die urspruenglichen 22mm (die
		// waren auf frameWidthMM=25 austariert) - 26mm haelt denselben
		// relativen Sicherheitsabstand wie im Original (Rahmenkante zu
		// Outputs-Spalte ca. 9-10mm Luft, siehe Rechnung im Chat-Verlauf).
		const float gapOutputsToStage = 26.f;

		// Rahmenbreite der neuen VOUT/TOUT-Ausgangsspalte: nur zwei PJ301M-
		// Buchsen (Voltage/Time Ramp Out), daher schmaler als die grosse
		// Jacks-Spalte (20mm) genuegt.
		const float outputsFrameWidthMM = 12.f;

		// Rahmenbreite der neuen Controls-Spalten-Umrandung (siehe unten,
		// "Controls-Rahmen"): die Controls-Spalte enthaelt nur einspaltig
		// zentrierte Elemente (Buttons/Trimpots), daher genuegt ein deutlich
		// schmalerer Rahmen als bei der Jacks-Spalte (20mm, dort mit PJ301M-
		// Buchsen).
		const float controlsFrameWidthMM = 14.f;

		// Eigene Spalte ganz links fuer die vier modulweit geteilten
		// External-CV-Eingaenge (vorher Teil von colA - das machte Section
		// A/B unsymmetrisch). Per eigenem Rahmen (StageFrame-Widget
		// wiederverwendet) optisch abgesetzt.
		float colExt = 18.f;
		// Abstand Ext-Spalte -> Controls-Spalte, so knapp wie moeglich bei
		// weiterhin ueberlappungsfreien Rahmen (Ext-Rahmen 20mm, Controls-
		// Rahmen 14mm breit) - wird unten spiegelbildlich auch fuer den
		// Abstand Section-B-Controls -> Preset-Spalte verwendet.
		const float colToControlsGap = 19.f;
		float colAControls = colExt + colToControlsGap;
		float colAJacks = colAControls + gapControlsToJacks;
		// Neue VOUT/TOUT-Ausgangsspalte zwischen Jacks-Spalte und Stage-
		// Array, spiegelbildlich fuer Section A/B (colAOutputs liegt links
		// vom Stage-Array, colBOutputs rechts davon).
		float colAOutputs = colAJacks + gapJacksToOutputs;
		float xStart = colAOutputs + gapOutputsToStage;
		float colBOutputs = xStart + (NUM_STAGES - 1) * xStep + gapOutputsToStage;
		float colBJacks = colBOutputs + gapJacksToOutputs;
		float colBControls = colBJacks + gapControlsToJacks;

		// External-CV-Spalte ganz links (modulweit geteilte Eingaenge A-D,
		// siehe InputId-Kommentar im Modul) - eigener Rahmen, damit sie
		// optisch klar von Section A abgesetzt ist, statt wie zuvor Teil
		// einer der beiden FG-Spalten zu sein (das machte A/B unsymmetrisch).
		const float extFrameWidthMM = 20.f;
		const float extFrameTopMM = 12.f;
		const float extFrameBottomMM = 108.f;
		StageFrame* extFrame = new StageFrame;
		extFrame->box.pos = mm2px(Vec(colExt - extFrameWidthMM / 2.f, extFrameTopMM));
		extFrame->box.size = mm2px(Vec(extFrameWidthMM, extFrameBottomMM - extFrameTopMM));
		addChild(extFrame);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colExt, 22)), module, Marf248::EXT_A_INPUT));
		// fontSize explizit 10pt statt der addLabelBelow()-Default (9pt,
		// identisch zum Trigger-Expander) - im dichten 146HP-Hauptpanel
		// wirkte 9pt neben den vielen anderen Elementen optisch kleiner/
		// duenner als am deutlich luftigeren 30HP-Trigger-Expander, obwohl
		// Schriftdatei/Groesse/Versatz codeseitig identisch waren
		// (Kinovice-Feedback). 22mm Zeilenpitch der Ext-Spalte gibt genug
		// Luft fuer den zusaetzlichen Punkt.
		addLabelBelow(Vec(colExt, 22), 5.5f, "EXT A", COLOR_STAGEMOD, 10.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colExt, 44)), module, Marf248::EXT_B_INPUT));
		addLabelBelow(Vec(colExt, 44), 5.5f, "EXT B", COLOR_STAGEMOD, 10.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colExt, 66)), module, Marf248::EXT_C_INPUT));
		addLabelBelow(Vec(colExt, 66), 5.5f, "EXT C", COLOR_STAGEMOD, 10.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colExt, 88)), module, Marf248::EXT_D_INPUT));
		addLabelBelow(Vec(colExt, 88), 5.5f, "EXT D", COLOR_STAGEMOD, 10.f);

		// Eigene Jacks-Spalten pro Section (neu): buendeln alle Buchsen der
		// jeweiligen Section (Time Multiplier CV, Start/Stop, Voltage/Time-
		// Out, Pulse 1/2-Out, All-Pulses-Out, Stage-Address-CV) getrennt von
		// den reinen Bedienelementen (Controls-Spalte: Run, Mode/Pendulum,
		// Stage-Address-Knob/Buttons). Liegt jeweils zwischen der Controls-
		// Spalte und dem Stage-Array - spiegelbildlich fuer A/B.
		const float jacksFrameWidthMM = 20.f;
		const float jacksFrameTopMM = 12.f;
		// 127 -> 118: VOUT/TOUT sind in die neue Ausgangsspalte umgezogen
		// (Kinovice-Wunsch), dadurch wieder deutlich mehr Luft in der
		// Jacks-Spalte - genutzt fuer die vergroesserte BPM-Anzeige, siehe
		// deren Platzierung weiter unten.
		// 123 -> 121: 123 war laut Kinovice-Feedback unten leicht zu lang,
		// 118 davor zu kurz - 121 als Mittelweg.
		const float jacksFrameBottomMM = 121.f;
		float jacksColsFrame[2] = {colAJacks, colBJacks};
		for (int s = 0; s < 2; s++) {
			StageFrame* jacksFrame = new StageFrame;
			jacksFrame->box.pos = mm2px(Vec(jacksColsFrame[s] - jacksFrameWidthMM / 2.f, jacksFrameTopMM));
			jacksFrame->box.size = mm2px(Vec(jacksFrameWidthMM, jacksFrameBottomMM - jacksFrameTopMM));
			addChild(jacksFrame);
		}

		// Neue VOUT/TOUT/TIME-Ausgangsspalte (Kinovice-Wunsch): eigener,
		// kleiner Rahmen zwischen Jacks-Spalte und Stage-Array, spiegel-
		// bildlich fuer A/B. Untergrenze auf Kinovice-Wunsch erweitert, um
		// die neue dritte Buchse (TIME, y=49, siehe Session "Time/Reference-
		// Output-Trennung") mit einzuschliessen. BUGFIX (Kinovice-Report:
		// "Rahmen schneidet TIME-Beschriftung"): die vorherige Untergrenze
		// (56mm) ging von einer Label-Unterkante von ca. 53.5mm aus - das
		// war ein Rechenfehler, siehe Kommentar am VOUT-Label weiter unten
		// (5.5mm Versatz + ca. 4.1mm Zeilenhoehe bei 10pt = ca. 9.6mm ab
		// Buchsenmitte, also bis ca. 58.6mm, nicht 53.5mm). Auf 61mm erweitert,
		// das entspricht denselben ca. 2.4mm Luft zur Rahmenunterkante wie
		// urspruenglich fuer TIME beabsichtigt.
		const float outputsFrameTopMM = 12.f;
		const float outputsFrameBottomMM = 61.f;
		float outputsColsFrame[2] = {colAOutputs, colBOutputs};
		for (int s = 0; s < 2; s++) {
			StageFrame* outputsFrame = new StageFrame;
			outputsFrame->box.pos = mm2px(Vec(outputsColsFrame[s] - outputsFrameWidthMM / 2.f, outputsFrameTopMM));
			outputsFrame->box.size = mm2px(Vec(outputsFrameWidthMM, outputsFrameBottomMM - outputsFrameTopMM));
			addChild(outputsFrame);
		}

		// --- Section A und B: identischer Aufbau (spiegelbildlich an derselben
		// y-Position), da die External-Inputs jetzt in einer eigenen Spalte
		// sitzen und beide FG-Spalten dadurch gleich viel Inhalt haben.
		float controlsCols[2] = {colAControls, colBControls};
		float jacksCols[2] = {colAJacks, colBJacks};
		float outputsCols[2] = {colAOutputs, colBOutputs};
		int runParams[2] = {Marf248::RUN_A_PARAM, Marf248::RUN_B_PARAM};
		int runLights[2] = {Marf248::RUN_A_LIGHT, Marf248::RUN_B_LIGHT};
		int modeParams[2] = {Marf248::MODE_A_PARAM, Marf248::MODE_B_PARAM};
		int modeLights[2] = {Marf248::MODE_A_LIGHT, Marf248::MODE_B_LIGHT};
		int pendulumParams[2] = {Marf248::PENDULUM_A_PARAM, Marf248::PENDULUM_B_PARAM};
		int pendulumLights[2] = {Marf248::PENDULUM_A_LIGHT, Marf248::PENDULUM_B_LIGHT};
		int startInputs[2] = {Marf248::START_A_INPUT, Marf248::START_B_INPUT};
		int stopInputs[2] = {Marf248::STOP_A_INPUT, Marf248::STOP_B_INPUT};
		int voutOutputs[2] = {Marf248::VOUT_A_OUTPUT, Marf248::VOUT_B_OUTPUT};
		int toutOutputs[2] = {Marf248::TOUT_A_OUTPUT, Marf248::TOUT_B_OUTPUT};
		// Neuer statischer Time-Output (Manual "Time Output", siehe
		// Kommentar bei TIMESTATIC_A/B_OUTPUT weiter oben) - eigene Buchse,
		// bewusst unter TOUT platziert (Kinovice-Wunsch).
		int timeStaticOutputs[2] = {Marf248::TIMESTATIC_A_OUTPUT, Marf248::TIMESTATIC_B_OUTPUT};
		int apoutOutputs[2] = {Marf248::APOUT_A_OUTPUT, Marf248::APOUT_B_OUTPUT};
		int p1ActiveLights[2] = {Marf248::P1_ACTIVE_A_LIGHT, Marf248::P1_ACTIVE_B_LIGHT};
		int p1Outputs[2] = {Marf248::P1OUT_A_OUTPUT, Marf248::P1OUT_B_OUTPUT};
		int p2ActiveLights[2] = {Marf248::P2_ACTIVE_A_LIGHT, Marf248::P2_ACTIVE_B_LIGHT};
		int p2Outputs[2] = {Marf248::P2OUT_A_OUTPUT, Marf248::P2OUT_B_OUTPUT};
		int addrKnobParams[2] = {Marf248::STAGE_ADDR_KNOB_A_PARAM, Marf248::STAGE_ADDR_KNOB_B_PARAM};
		int addrModeParams[2] = {Marf248::STAGE_ADDR_MODE_A_PARAM, Marf248::STAGE_ADDR_MODE_B_PARAM};
		int addrModeLights[2] = {Marf248::STAGE_ADDR_MODE_A_LIGHT, Marf248::STAGE_ADDR_MODE_B_LIGHT};
		int addrContParams[2] = {Marf248::STAGE_ADDR_CONT_A_PARAM, Marf248::STAGE_ADDR_CONT_B_PARAM};
		int addrContLights[2] = {Marf248::STAGE_ADDR_CONT_A_LIGHT, Marf248::STAGE_ADDR_CONT_B_LIGHT};
		int addrStrobeParams[2] = {Marf248::STAGE_ADDR_STROBE_A_PARAM, Marf248::STAGE_ADDR_STROBE_B_PARAM};
		int addrResetParams[2] = {Marf248::STAGE_ADDR_RESET_A_PARAM, Marf248::STAGE_ADDR_RESET_B_PARAM};
		int addrStrobeLights[2] = {Marf248::STAGE_ADDR_STROBE_A_LIGHT, Marf248::STAGE_ADDR_STROBE_B_LIGHT};
		int addrResetLights[2] = {Marf248::STAGE_ADDR_RESET_A_LIGHT, Marf248::STAGE_ADDR_RESET_B_LIGHT};
		int addrExtInputs[2] = {Marf248::STAGE_ADDR_EXT_A_INPUT, Marf248::STAGE_ADDR_EXT_B_INPUT};
		int addrExtChannelInputs[2] = {Marf248::STAGE_ADDR_EXT_CHANNEL_A_INPUT, Marf248::STAGE_ADDR_EXT_CHANNEL_B_INPUT};
		int timeMultParams[2] = {Marf248::TIME_MULT_A_PARAM, Marf248::TIME_MULT_B_PARAM};
		int timeMultAttenParams[2] = {Marf248::TIME_MULT_ATTEN_A_PARAM, Marf248::TIME_MULT_ATTEN_B_PARAM};
		int timeMultCvInputs[2] = {Marf248::TIME_MULT_CV_A_INPUT, Marf248::TIME_MULT_CV_B_INPUT};
		int beatsFactorParams[2] = {Marf248::BEATS_FACTOR_A_PARAM, Marf248::BEATS_FACTOR_B_PARAM};
		int beatsFactorCvInputs[2] = {Marf248::BEATS_FACTOR_CV_A_INPUT, Marf248::BEATS_FACTOR_CV_B_INPUT};

		for (int s = 0; s < 2; s++) {
			float ctrl = controlsCols[s];
			float jack = jacksCols[s];
			float outCol = outputsCols[s];

			// Controls-Rahmen: dekorative Umrandung der Controls-Spalte
			// (Time Multiplier Knob/Attenuverter/Run/Mode/Pendulum/Stage-
			// Address), damit sie optisch klar als eigener Block erkennbar
			// ist - analog zu Jacks-Spalte, External-CV-Spalte und
			// Stage-Array, die alle schon einen Rahmen haben. Oberkante
			// bleibt auf gleicher Hoehe wie External-CV-Spalte/Stage-Array
			// (12mm) - der Inhalt beginnt aber erst etwas weiter unten
			// (Knob-Mittelpunkt bei 20mm statt 16mm), damit der Time-
			// Multiplier-Knob nicht mehr oben ueber den Rahmenrand hinausragt
			// (RoundBlackKnob ist ca. 9-10mm im Durchmesser). Die Spalte
			// waechst dafuer entsprechend weiter nach unten.
			const float controlsFrameTopMM = 12.f;
			// BPM-Feature: um 4mm auf 120mm erweitert (deckt sich jetzt mit
			// frameBottomMM des Stage-Arrays), da die neue BEATS_FACTOR-
			// Trimpot-Reihe eingefuegt wurde - siehe Kommentar unten bei
			// deren Platzierung.
			const float controlsFrameBottomMM = 120.f;
			StageFrame* controlsFrame = new StageFrame;
			controlsFrame->box.pos = mm2px(Vec(ctrl - controlsFrameWidthMM / 2.f, controlsFrameTopMM));
			controlsFrame->box.size = mm2px(Vec(controlsFrameWidthMM, controlsFrameBottomMM - controlsFrameTopMM));
			addChild(controlsFrame);

			// --- Controls-Spalte, von oben nach unten: Time Multiplier Knob
			// + Attenuverter (die CV-Buchse dazu sitzt in der Jacks-Spalte),
			// Run ("Section A/B Start/Stop"), Mode, Pendulum, dann die
			// Stage-Address-Elemente.
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ctrl, 20)), module, timeMultParams[s]));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(ctrl, 30)), module, timeMultAttenParams[s]));

			// BEATS_FACTOR-Trimpot (BPM-Feature, siehe process()): direkt
			// unter dem Tempo-Attenuverter, da beide Regler thematisch zur
			// selben Tempo-Steuerung gehoeren. Alle Elemente darunter
			// (Run/Mode/Pendulum/Stage-Address) sind dadurch um 4mm nach
			// unten gerutscht ggue. der vorherigen Session (die Original-
			// Abstaende wurden dabei absichtlich NICHT komprimiert, nur an
			// vier Stellen um je 1mm gestrafft) - siehe controlsFrameBottomMM
			// oben. ACHTUNG Kinovice: wie ueblich kurz in Rack gegenchecken,
			// bei so vielen verschobenen Elementen in einer Session besonders.
			addParam(createParamCentered<BeatsFactorTrimpot>(mm2px(Vec(ctrl, 38)), module, beatsFactorParams[s]));

			HDivider* divTimeMultRun = new HDivider;
			divTimeMultRun->box.pos = mm2px(Vec(ctrl - 4.f, 44.f));
			divTimeMultRun->box.size = mm2px(Vec(8.f, 0.f));
			addChild(divTimeMultRun);

			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 50)), module, runParams[s]));
			// Dreifarbige Statusanzeige (Gruen=Run/Gelb=Hold/Rot=Stop, siehe
			// RUN_A/B_LIGHT_R/G/B und Kommentar in process()) statt der
			// vorherigen einfarbig-gruenen LED.
			addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(ctrl, 50)), module, runLights[s]));

			HDivider* divRunMode = new HDivider;
			divRunMode->box.pos = mm2px(Vec(ctrl - 4.f, 58.f));
			divRunMode->box.size = mm2px(Vec(8.f, 0.f));
			addChild(divRunMode);

			// Laufrichtung: zwei getrennte Klick-Buttons, jetzt untereinander
			// statt nebeneinander. Mode kehrt die Basisrichtung um (LED an =
			// Reverse), Pendulum schaltet zusaetzlich das Bouncing dazu
			// (LED an = aktiv).
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 64)), module, modeParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 64)), module, modeLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 72)), module, pendulumParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 72)), module, pendulumLights[s]));

			HDivider* divModeAddr = new HDivider;
			divModeAddr->box.pos = mm2px(Vec(ctrl - 4.f, 77.f));
			divModeAddr->box.size = mm2px(Vec(8.f, 0.f));
			addChild(divModeAddr);

			addParam(createParamCentered<Trimpot>(mm2px(Vec(ctrl, 83)), module, addrKnobParams[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 91)), module, addrModeParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 91)), module, addrModeLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 99)), module, addrContParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 99)), module, addrContLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 107)), module, addrStrobeParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 107)), module, addrStrobeLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 115)), module, addrResetParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 115)), module, addrResetLights[s]));

			// --- Jacks-Spalte, von oben nach unten: Start/Stop-Trigger (eng,
			// 8mm), Time-Multiplier-CV-Eingang, Live-BPM-Anzeige,
			// BEATS_FACTOR-CV, Pulse 1/2-Out, All-Pulses-Out, Stage-
			// Address-CV-In. Voltage/Time-Out sind in die neue Ausgangsspalte
			// umgezogen (Kinovice-Wunsch) - dadurch wieder mehr Luft fuer die
			// vergroesserte BPM-Anzeige. Inhalt beginnt erst bei 20mm (statt
			// 16mm) - bei einer Oberkante von 12mm (jetzt einheitlich mit
			// den anderen Spalten) ragte die Start-Buchse bei 16mm sonst
			// leicht ueber den Rahmenrand hinaus.
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 20)), module, startInputs[s]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 28)), module, stopInputs[s]));

			HDivider* divStartTimeMult = new HDivider;
			divStartTimeMult->box.pos = mm2px(Vec(jack - 9.f, 33.f));
			divStartTimeMult->box.size = mm2px(Vec(18.f, 0.f));
			addChild(divStartTimeMult);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 41)), module, timeMultCvInputs[s]));

			// Live-BPM-Anzeige (BPM-Feature): direkt unter der Tempo-CV-
			// Buchse, da sie das tatsaechlich wirksame Tempo dieser Buchse
			// (Knob + CV) anzeigt - reiner Text-Readout, kein Bauteil, daher
			// ohne eigenen ParamWidget-Resize-Tanz einfach per box.pos/size
			// direkt platziert (siehe TempoBpmDisplay-Kommentar). Deutlich
			// vergroessert ggue. der ersten Fassung (16x5mm -> 22x9mm,
			// Schrift 8.5 -> 14px) - gleiche Amber-Farbe wie TrangeButton
			// (Time Range), damit beide Live-Anzeigen im Panel einheitlich
			// wirken (Kinovice-Wunsch). Passt jetzt bequem hierher, seit
			// VOUT/TOUT in die eigene Spalte umgezogen sind.
			TempoBpmDisplay* bpmDisplay = new TempoBpmDisplay;
			bpmDisplay->box.size = mm2px(Vec(22.f, 9.f));
			bpmDisplay->box.pos = mm2px(Vec(jack, 51)).minus(bpmDisplay->box.size.div(2));
			bpmDisplay->module = dynamic_cast<Marf248*>(module);
			bpmDisplay->section = s;
			addChild(bpmDisplay);

			// BEATS_FACTOR-CV-Eingang (BPM-Feature, siehe process()): direkt
			// unter der BPM-Anzeige, thematisch zusammengehoerig (Beats pro
			// Zyklus beeinflusst die angezeigte BPM-Zahl mit).
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 63)), module, beatsFactorCvInputs[s]));

			HDivider* divOutPulse = new HDivider;
			divOutPulse->box.pos = mm2px(Vec(jack - 9.f, 68.f));
			divOutPulse->box.size = mm2px(Vec(18.f, 0.f));
			addChild(divOutPulse);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack, 76)), module, p1Outputs[s]));
			addChild(createLightCentered<PulseRingLight>(mm2px(Vec(jack, 76)), module, p1ActiveLights[s]));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack, 84)), module, p2Outputs[s]));
			addChild(createLightCentered<PulseRingLight>(mm2px(Vec(jack, 84)), module, p2ActiveLights[s]));

			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack, 92)), module, apoutOutputs[s]));

			HDivider* divApoutAddr = new HDivider;
			divApoutAddr->box.pos = mm2px(Vec(jack - 9.f, 97.f));
			divApoutAddr->box.size = mm2px(Vec(18.f, 0.f));
			addChild(divApoutAddr);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 105)), module, addrExtInputs[s]));
			// Kanalwahl-CV direkt darunter, kein Divider noetig (gehoert
			// thematisch zur selben Buchse darueber, analog zum bisherigen
			// VOUT/TOUT-Paar ohne Trenner).
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 113)), module, addrExtChannelInputs[s]));

			// Neue Ausgangsspalte (Kinovice-Wunsch): Voltage/Time-Ramp-Out,
			// jetzt eigenstaendig zwischen Jacks-Spalte und Stage-Array statt
			// in der Jacks-Spalte selbst - schafft dort Platz fuer die
			// vergroesserte BPM-Anzeige (siehe oben).
			// VOUT auf Hoehe von Start (y=20, siehe jacksCols-Spalte
			// nebenan) hochgezogen, TOUT nur etwas hoeher (36 -> 34.5) -
			// beides auf Kinovice-Wunsch, um unter der jetzt groesseren
			// Beschriftung (siehe unten) Platz zu schaffen, ohne den
			// bestehenden Ausgangs-Rahmen (12-61mm, siehe outputsFrameBottomMM
			// oben - urspruenglich 44mm, dann 56mm, siehe dortiger Bugfix-
			// Kommentar) zu aendern. Beide Buchsen bleiben damit klar innerhalb
			// des Rahmens (VOUT-Oberkante ca. 15.8mm, TOUT-Label-Unterkante
			// ca. 44.1mm).
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(outCol, 20)), module, voutOutputs[s]));
			// Gleiches Schriftbild wie Ext A-D (10pt, 5.5mm Versatz statt
			// der vorherigen kompakten 6.5pt/4mm-Variante) - Kinovice-
			// Wunsch nach einheitlicher, besser lesbarer Beschriftung.
			addLabelBelow(Vec(outCol, 20), 5.5f, "VOUT", (s == 0 ? COLOR_SECTION_A : COLOR_SECTION_B), 10.f);
			// 14.5mm Pitch zu VOUT (statt vorher 12mm) - bei 10pt braucht
			// das VOUT-Label ca. 9.6mm Platz (5.5mm Versatz + ca. 4.1mm
			// Zeilenhoehe bei 10pt, siehe Kommentar am Dateianfang zur
			// 3.7mm-Zeilenhoehe bei 9pt, linear hochskaliert) - bei
			// 14.5mm Pitch bleiben der PJ301M-Oberkante von TOUT (Radius
			// ~4.2mm) davor noch ca. 0.7mm Luft. Knapp, aber numerisch
			// passend - bitte in Rack gegenchecken (siehe Hinweis am
			// Dateianfang zum PanelLabel-System).
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(outCol, 34.5f)), module, toutOutputs[s]));
			addLabelBelow(Vec(outCol, 34.5f), 5.5f, "TOUT", (s == 0 ? COLOR_SECTION_A : COLOR_SECTION_B), 10.f);

			// Neuer statischer Time-Output (Session "Time/Reference-Output-
			// Trennung"): direkt unter TOUT, gleicher 14.5mm-Pitch wie
			// VOUT->TOUT, bei y=49mm. Label-Unterkante ca. 58.6mm (5.5mm
			// Versatz + ca. 4.1mm Zeilenhoehe bei 10pt, siehe Kommentar am
			// VOUT-Label oben) - passt jetzt in den auf 61mm erweiterten
			// Rahmen (outputsFrameBottomMM oben, Bugfix-Session), mit ca.
			// 2.4mm Luft zur Rahmenunterkante statt vorher negativ (Label
			// ragte ueber die alte 56mm-Grenze hinaus - daher der Kinovice-
			// Report "Rahmen schneidet TIME ab").
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(outCol, 49)), module, timeStaticOutputs[s]));
			addLabelBelow(Vec(outCol, 49), 5.5f, "TIME", (s == 0 ? COLOR_SECTION_A : COLOR_SECTION_B), 10.f);
		}

		// --- Stage-Array (Mitte), Session "Hauptmodul-Relayout" (zweiter
		// Anlauf nach Kinovice-Feedback zum ersten Entwurf) ---
		//
		// Aufbau pro Stage-Spalte: links zwei gestapelte Schieberegler
		// (Voltage oben, Time darunter), rechts daneben EIN gemeinsamer
		// Block aller Modifier-Buttons - jetzt in 2 statt 4 Spalten
		// (Kinovice-Feedback "Buttons in zwei Spalten"), UND der bisherige
		// volle-Breite-Block (Shape/Curve/TRange/Range) zieht in dieselbe
		// schmale Button-Unterspalte um, statt separat quer ueber die volle
		// Stage-Breite zu liegen. Beide Aenderungen zusammen machen die
		// Stage-Spalte insgesamt SCHMALER (32mm statt 46mm im ersten
		// Entwurf) und geben gleichzeitig der Slider-Unterspalte die
		// komplette Rahmenhoehe frei - die Slider koennen dadurch von 30mm
		// auf 42mm Weg wachsen (siehe StageSlider::TRAVEL_MM), mit deutlich mehr
		// Abstand zur Stage-aktiv-LED darueber (war beim ersten Entwurf nur
		// ca. 1mm Luft, Kinovice-Feedback "zu dicht an den LEDs").
		//
		// Beschriftung (Kinovice-Entscheidung "Option A", unveraendert):
		// KEIN Label pro Button in jeder der 16 Stage-Spalten - stattdessen
		// eine einmalige Legende rechts neben Stage 16 (siehe nach dieser
		// Schleife), jetzt an die 2-Spalten/8-Zeilen-Gruppierung angepasst
		// (eine Legendenzeile pro thematischer Zweier-Zeilen-Gruppe statt
		// vorher pro Einzelzeile).
		const float sliderColOffsetX = -10.f;
		// Slider-Oberkante jetzt buendig mit der OBERKANTE der ersten
		// Buttonreihe statt mit deren Mittelpunkt (Kinovice-Feedback,
		// zweite Runde: "Oberkante des Buttons ist besser, also um den
		// Radius des Buttons hoeher"). LEDButton-Radius ~3mm (Rack-
		// Standardgroesse) -> Oberkante = buttonRowsY[0] - 3 = 20-3 = 17.
		// Hoehe kommt jetzt direkt aus StageSlider::HEIGHT_MM (50mm) - hier
		// nur noch die Mittelpunkte:
		//   voltSliderY = 17 (Oberkante) + HEIGHT_MM/2 (50/2=25) = 42
		//   timeSliderY = voltSliderY + HEIGHT_MM (50) + 6mm Luecke = 98
		const float voltSliderY = 42.f;
		const float timeSliderY = 98.f;
		// Slider-Unterkante liegt damit bei 98+25=123mm, 3mm Luft zu
		// frameBottomMM (126) - knapp, aber passend.
		const float stageLedY = 13.f;
		// 2-Spalten-Raster (Kinovice-Feedback "Buttons in zwei Spalten"),
		// bewaehrter 8mm-Pitch. 8 Zeilen fuer 14 Modifier + 2 bewusst freie
		// Plaetze (Platzreserve wie beim vorherigen 4x4-Entwurf), in
		// thematischen Zweiergruppen wie zuvor: Puls/Output, Zustand/Halten,
		// Adress-Fluss, Quelle/Override.
		const float buttonColOffsets[2] = {2.f, 10.f};
		const float buttonRowsY[8] = {20.f, 28.f, 36.f, 44.f, 52.f, 60.f, 68.f, 76.f};
		// Shape/Curve/TRange/Range: jetzt Teil der Button-Unterspalte
		// (mittig zwischen den beiden Button-Spalten, offsetX=6 = Mittelwert
		// von buttonColOffsets), nicht mehr ueber die volle Stage-Breite.
		const float shapeCurveOffsetX = 6.f;
		const float shapeCurveY = 88.f;
		const float trangeOffsetX = 6.f;
		const float trangeY = 100.f;
		const float rangeOffsetX = 6.f;
		const float rangeY = 115.f;

		for (int i = 0; i < NUM_STAGES; i++) {
			float x = xStart + i * xStep;
			float sliderX = x + sliderColOffsetX;

			// Dekorativer Rahmen um die gesamte Stage-Spalte.
			StageFrame* frame = new StageFrame;
			frame->box.pos = mm2px(Vec(x - frameWidthMM / 2.f, frameTopMM));
			frame->box.size = mm2px(Vec(frameWidthMM, frameBottomMM - frameTopMM));
			addChild(frame);

			// --- Slider-Subspalte: Voltage oben, Time darunter. Groesse
			// kommt jetzt fest aus StageSlider selbst (siehe Konstruktor
			// dort) - createParamCentered() zentriert direkt korrekt, keine
			// nachtraegliche box.size-Anpassung mehr noetig.
			StageSlider* voltSlider = createParamCentered<StageSlider>(mm2px(Vec(sliderX, voltSliderY)), module, Marf248::STAGE_VOLTAGE_PARAM + i);
			addParam(voltSlider);

			StageSlider* timeSlider = createParamCentered<StageSlider>(mm2px(Vec(sliderX, timeSliderY)), module, Marf248::STAGE_TIME_PARAM + i);
			addParam(timeSlider);

			// Stage-aktiv-Anzeige (A/B): oben ueber dem Slider-Block, mit
			// jetzt ausreichend Abstand zum Slider darunter (siehe
			// stageLedY-Kommentar oben).
			addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(sliderX - 1.5f, stageLedY)), module, Marf248::STAGE_LIGHT_A + i));
			addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(sliderX + 1.5f, stageLedY)), module, Marf248::STAGE_LIGHT_B + i));

			// --- Button-Subspalte: 2x8-Raster in thematischen Zweiergruppen.
			//   Gruppe Puls/Output:   Pulse1 | Pulse2  /  Skip  | Mute
			//   Gruppe Zustand:       Stop   | Sustain /  Enable| -
			//   Gruppe Adress-Fluss:  First  | Last    /  Slope | -
			//   Gruppe Quelle/Override: Quant| Volt-Ext/ Time-Ext| Normalize
			//
			// LICHTFARBEN-SCHEMA (nachtraeglich festgelegt und dokumentiert,
			// ausgeloest durch eine Kinovice-Nachfrage zur Mute-Farbe - siehe
			// Marf248-Dokumentation_DE.md Kapitel 11 fuer die vollstaendige
			// Begruendung, hier nur die Kurzfassung als Code-Referenz):
			//   Rot    = Ablauf-/Ausgabe-Verhalten der Stage weicht vom
			//            Normalzustand ab (Pulse1/Pulse2 feuern Trigger,
			//            Skip schliesst die Stage komplett aus, Stop/
			//            Sustain/Enable halten den Ablauf an, Mute
			//            unterdrueckt die Trigger-Ausgabe).
			//   Gruen  = reine Struktur-/Kurven-Modifikatoren ohne direkten
			//            Eingriff in Trigger-Feuerung oder Ablaufsteuerung
			//            (First/Last, Slope, Quant, QuantGlide).
			//   Blau   = Kanal-/Zustandsauswahl (Volt-Ext/Time-Ext-
			//            Kanalwahl, Shape Exp/Log-Zustandsanzeige).
			//   Gelb   = Einzelfall Normalize (bewusst abgesetzte reine
			//            Fein-Justage-Funktion ohne eigene Kategorie).
			// Skip war urspruenglich Gruen (folgte der Adress-Fluss-Logik:
			// "wirkt nur bei automatischem Ablauf"), wurde aber auf Rot
			// umgestellt, um konsistent mit Mute/Pulse1/Pulse2 zu sein -
			// alle vier beeinflussen direkt, ob/wie eine Stage nach aussen
			// wahrnehmbar ist (Skip: komplett unsichtbar, Mute: stumm,
			// Pulse1/2: erzeugen die Trigger, die Mute wieder unterdrueckt).
			addParam(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[0])), module, Marf248::STAGE_PULSE1_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[0])), module, Marf248::STAGE_PULSE1_LIGHT + i));
			addParam(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[0])), module, Marf248::STAGE_PULSE2_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[0])), module, Marf248::STAGE_PULSE2_LIGHT + i));


			addParam(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[1])), module, Marf248::STAGE_SKIP_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[1])), module, Marf248::STAGE_SKIP_LIGHT + i));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[1])), module, Marf248::STAGE_MUTE_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[1])), module, Marf248::STAGE_MUTE_LIGHT + i));

			addParam(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[2])), module, Marf248::STAGE_STOP_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[2])), module, Marf248::STAGE_STOP_LIGHT + i));
			addParam(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[2])), module, Marf248::STAGE_SUSTAIN_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[2])), module, Marf248::STAGE_SUSTAIN_LIGHT + i));

			addParam(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[3])), module, Marf248::STAGE_ENABLE_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[3])), module, Marf248::STAGE_ENABLE_LIGHT + i));
			// buttonColOffsets[1] in dieser Reihe bewusst frei.

			addParam(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[4])), module, Marf248::STAGE_FIRST_PARAM + i));
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[4])), module, Marf248::STAGE_FIRST_LIGHT + i));
			addParam(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[4])), module, Marf248::STAGE_LAST_PARAM + i));
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[4])), module, Marf248::STAGE_LAST_LIGHT + i));

			addParam(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[5])), module, Marf248::STAGE_SLOPE_PARAM + i));
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[5])), module, Marf248::STAGE_SLOPE_LIGHT + i));
			// Quantized Glide (siehe MOD_QUANT_GLIDE in Marf248.hpp): belegt
			// den bisher freien buttonColOffsets[1]-Slot in dieser Reihe,
			// thematisch direkt neben Slope (nur mit Slope zusammen wirksam).
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[5])), module, Marf248::STAGE_QUANTGLIDE_PARAM + i));
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[5])), module, Marf248::STAGE_QUANTGLIDE_LIGHT + i));

			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[6])), module, Marf248::STAGE_QUANT_PARAM + i));
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[6])), module, Marf248::STAGE_QUANT_LIGHT + i));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[6])), module, Marf248::STAGE_VOLTEXT_PARAM + i));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[6])), module, Marf248::STAGE_VOLTEXT_LIGHT + i));

			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[7])), module, Marf248::STAGE_TIMEEXT_PARAM + i));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[7])), module, Marf248::STAGE_TIMEEXT_LIGHT + i));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[7])), module, Marf248::STAGE_NORMALIZE_PARAM + i));
			addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[7])), module, Marf248::STAGE_NORMALIZE_LIGHT + i));

			// --- Shape/Curve/TRange/Range: jetzt in der Button-Unterspalte
			// (mittig zwischen den zwei Button-Spalten), NICHT mehr ueber
			// die volle Stage-Breite - das macht der Slider-Unterspalte
			// erst die komplette Rahmenhoehe frei (siehe Kommentar oben).
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + shapeCurveOffsetX + 4.f, shapeCurveY)), module, Marf248::STAGE_SHAPE_PARAM + i));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x + shapeCurveOffsetX + 1.f, shapeCurveY)), module, Marf248::STAGE_SHAPE_EXP_LIGHT + i));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x + shapeCurveOffsetX + 7.f, shapeCurveY)), module, Marf248::STAGE_SHAPE_LOG_LIGHT + i));
			addParam(createParamCentered<StageCurveTrimpot>(mm2px(Vec(x + shapeCurveOffsetX - 4.f, shapeCurveY)), module, Marf248::STAGE_CURVE_PARAM + i));

			// Time Range (TR1-4): eigene Reihe, mittig in der Button-
			// Unterspalte. TrangeButton zeichnet sich komplett selbst (kein
			// SVG-Frame), daher ist eine freie box.size-Wahl hier
			// unproblematisch - createParamCentered() zentriert zunaechst
			// mit der ererbten LEDButton-Default-Groesse, danach wird
			// box.size auf 9x9mm vergroessert und um denselben Mittelpunkt
			// neu zentriert.
			TrangeButton* trangeBtn = createParamCentered<TrangeButton>(mm2px(Vec(x + trangeOffsetX, trangeY)), module, Marf248::STAGE_TRANGE_PARAM + i);
			Vec trangeCenter = trangeBtn->box.getCenter();
			trangeBtn->box.size = mm2px(Vec(9.f, 9.f));
			trangeBtn->box.pos = trangeCenter.minus(trangeBtn->box.size.div(2));
			addParam(trangeBtn);

			// Range: eigene Reihe unten, mittig in der Button-Unterspalte,
			// mit eingeblendeter Zustands-Abkuerzung (siehe RangeKnob).
			addParam(createParamCentered<RangeKnob>(mm2px(Vec(x + rangeOffsetX, rangeY)), module, Marf248::STAGE_RANGE_PARAM + i));
		}

		// HINWEIS: die einmalige Stage-Array-Legende (Kinovice-Entscheidung
		// "Option A") aus den ersten beiden Session-Iterationen wurde
		// wieder entfernt - sie kollidierte mit der Section-B-Ausgangs-
		// spalte (VOUT/TOUT): dort stehen real nur ca. 4mm Luft zur
		// Verfuegung (colBOutputs hat selbst schon 12mm Rahmenbreite), die
		// Legendentexte brauchen aber je nach Zeile bis zu ca. 35mm - genau
		// das war der von Kinovice als "Text bei Section B, vermutlich von
		// den Expandern" gemeldete Fehler (tatsaechlich unsere eigene, zu
		// eng platzierte Legende, kein Expander-Bezug). Die vollstaendige
		// Beschriftung der Stage-Spalte braucht einen eigenen Design-
		// Durchgang mit bewusst eingeplantem Platzbedarf, statt sie in die
		// zufaellig uebrige Luecke vor Section B zu quetschen - kommt daher
		// wie ohnehin geplant als eigene Folge-Session (zusammen mit der
		// Beschriftung von Controls-/Jacks-/Ext-/Preset-Spalte).

		// Global Clear: einzelner, geteilter Taster mittig ueber dem
		// Stage-Array (kein Bezug zu einer einzelnen Section, siehe
		// Kommentar bei CLEAR_PARAM). Zentriert ueber die 16 Stages selbst
		// (xStart..letzte Stage), nicht ueber die Controls-Spalten, da diese
		// jetzt durch die neuen Jacks-Spalten weiter aussen liegen.
		// Y=6 statt vorher 8 (Kinovice-Feedback "etwas hoeher, auf der
		// Mitte zwischen Stage-Oberkante und Modulrahmen"): Modulrahmen
		// liegt bei y=0, Stage-Oberkante (frameTopMM) bei y=12, Mitte also
		// bei 6.
		// Die Taster sitzen jetzt rechts neben dem zentrierten Modulnamen (Titelbreite ca. 27mm,
		// dazu ca. 10mm Abstand): erster Taster 24mm rechts von der Panelmitte, Abstand 12mm.
		float colClear = titleCenterMM + 24.f;
		addParam(createParamCentered<LEDButton>(mm2px(Vec(colClear, 6)), module, Marf248::CLEAR_PARAM));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(colClear, 6)), module, Marf248::CLEAR_LIGHT));

		// Reset Values: direkt neben Global Clear, gleiche Reihe, andere
		// Lichtfarbe (Gelb statt Rot) zur klaren optischen Unterscheidung.
		addParam(createParamCentered<LEDButton>(mm2px(Vec(colClear + 12.f, 6)), module, Marf248::RESET_VALUES_PARAM));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(colClear + 12.f, 6)), module, Marf248::RESET_VALUES_LIGHT));

		// Auto-Fill: dritter Button in derselben Reihe, blaues Licht zur
		// Unterscheidung von Clear (rot) und Reset Values (gelb).
		addParam(createParamCentered<LEDButton>(mm2px(Vec(colClear + 24.f, 6)), module, Marf248::AUTOFILL_PARAM));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(colClear + 24.f, 6)), module, Marf248::AUTOFILL_LIGHT));

		// --- Preset-Spalte ganz rechts (Manual "PRESETS AND QUANTIZATION
		// SCALES"), spiegelbildlich zur External-CV-Spalte ganz links.
		// LOAD/SAVE "scharf schalten", darunter die 12 Slot-Taster in einer
		// durchgehenden Spalte - passt in die volle Panel-Hoehe, daher
		// separat statt in eine der bestehenden, bereits engen Spalten
		// gequetscht (siehe Projektnotizen zur Layout-Entscheidung).
		float colPresets = colBControls + colToControlsGap;
		const float presetFrameWidthMM = 20.f;
		const float presetFrameTopMM = 12.f;
		const float presetFrameBottomMM = 127.f;
		StageFrame* presetFrame = new StageFrame;
		presetFrame->box.pos = mm2px(Vec(colPresets - presetFrameWidthMM / 2.f, presetFrameTopMM));
		presetFrame->box.size = mm2px(Vec(presetFrameWidthMM, presetFrameBottomMM - presetFrameTopMM));
		addChild(presetFrame);

		// Inhalt beginnt erst bei 20mm (statt 14mm) - bei der jetzt
		// einheitlichen Oberkante von 12mm (wie alle anderen Spalten) ragte
		// der LOAD-Button bei 14mm sonst leicht ueber den Rahmenrand hinaus.
		//
		// Spalten-Header "PRESETS": vertikal rechts neben dem LOAD/SAVE/
		// SCALE/KEY-Viererblock (Kinovice-Wunsch, Korrektur nach erster
		// Fassung - die betraf faelschlich die 12 Ziffern statt des
		// Spaltennamens). Ankerpunkt vertikal mittig zwischen LOAD (20)
		// und KEY (44), also y=32 - der Viererblock spannt dabei 24mm auf
		// (17-47mm inkl. Button-Radius), reichlich Platz fuer eine deutlich
		// groessere Schrift als bei den seitlichen Button-Labels (16pt statt
		// 6.5pt): "PRESETS" (7 Zeichen) braucht vertikal ca. 20.9mm
		// (Zeichenbreite ~0.55*16/2.9528 = 2.98mm/Zeichen), passt mit ca.
		// 4.5mm Luft je Seite. Horizontal bleibt bei 6mm Versatz + halber
		// Zeilenhoehe (~3.25mm) noch ca. 0.75mm Luft zum 10mm-Rahmenrand.
		addLabelVertical(Vec(colPresets, (20.f + 44.f) / 2.f), 6.f, "PRESETS", COLOR_SYSTEM, 16.f);

		addParam(createParamCentered<LEDButton>(mm2px(Vec(colPresets, 20)), module, Marf248::LOAD_PARAM));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(colPresets, 20)), module, Marf248::LOAD_LIGHT));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(colPresets, 28)), module, Marf248::SAVE_PARAM));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(colPresets, 28)), module, Marf248::SAVE_LIGHT));

		// Scale/Key (Manual "Changing Key"/"Changing Scale"): gleiches
		// Arm-Taster-Muster wie LOAD/SAVE, direkt darunter in derselben
		// Spalte, reusen die 12 Preset-Taster weiter unten als Doppel-
		// belegung (siehe process()-Logik).
		addParam(createParamCentered<LEDButton>(mm2px(Vec(colPresets, 36)), module, Marf248::SCALE_PARAM));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(colPresets, 36)), module, Marf248::SCALE_LIGHT));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(colPresets, 44)), module, Marf248::KEY_PARAM));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(colPresets, 44)), module, Marf248::KEY_LIGHT));

		HDivider* divPresets = new HDivider;
		divPresets->box.pos = mm2px(Vec(colPresets - 9.f, 49.f));
		divPresets->box.size = mm2px(Vec(18.f, 0.f));
		addChild(divPresets);

		const float presetStartY = 55.f;
		const float presetStepY = 6.f;

		// Spalten-Header fuer den Zwoelfer-Block, gleiches Prinzip wie
		// "PRESETS" beim Viererblock oben: vertikal, rechts daneben,
		// mittig auf die Blockmitte zentriert (y=(55+121)/2=88). Deutlich
		// mehr Luft als beim Viererblock (72mm Blockhoehe statt 24mm),
		// daher bleibt "PRESET NR" bei gleicher Schriftgroesse (16pt, fuer
		// einheitliches Erscheinungsbild mit "PRESETS") komfortabel
		// mittig, ohne die Rahmenkanten (55-3=52 bis 121+3=124mm) auch nur
		// annaehernd auszureizen.
		addLabelVertical(Vec(colPresets, (presetStartY + (presetStartY + 11.f * presetStepY)) / 2.f), 6.f, "PRESET NR", COLOR_SYSTEM, 16.f);

		for (int p = 0; p < 12; p++) {
			float y = presetStartY + p * presetStepY;
			addParam(createParamCentered<LEDButton>(mm2px(Vec(colPresets, y)), module, Marf248::PRESET_PARAM + p));
			addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(colPresets, y)), module, Marf248::PRESET_LIGHT + p));
			// 6mm Pitch (Button-Radius 3mm je Seite = 0mm Luft) - Labels
			// unterhalb sind hier unmoeglich, daher reine Ziffer seitlich,
			// horizontal (nur 1-2 Zeichen, passt auch bei sehr wenig
			// Breite). Bleibt bewusst horizontal - nur der Spaltenname
			// steht vertikal, siehe oben. 13pt statt vorher 6.5pt
			// (Kinovice-Wunsch "Zahlen groesser") - Obergrenze liegt bei
			// ca. 14.7pt: die Ziffern sitzen bei 6mm Pitch alle in
			// derselben schmalen Spalte links neben der Taster-Spalte,
			// bei zu grosser Schrift wuerden Nachbar-Ziffern (z.B. "9" zu
			// "10") sich beruehren. 13pt haelt hier noch reichlich
			// Abstand (ca. 1mm Luft) und passt gleichzeitig horizontal
			// klar in den 10mm-Rahmenhalbwert (zweistellig "12" braucht
			// bei 13pt ca. 4.8mm, plus 4mm Versatz = 8.8mm).
			addLabelSide(Vec(colPresets, y), 4.f, -1.f, std::to_string(p + 1), COLOR_SYSTEM, 13.f);
		}
	}
};


// ---------------------------------------------------------------------
// Kompaktversion "Lucia": Single-Stage-Slot-Panel (Konzept "GUI-
// Vereinfachung auf einzelnen Stage", Session "Kompaktversion Marf248 -
// Panel & Model-Registrierung"). Nutzt denselben Module-Typ wie
// Sofonisba (struct Marf248) - siehe Marf248Host/dynamic_cast-Vorarbeit,
// nur das Panel/ModuleWidget unterscheidet sich: statt 16 parallelen
// Stage-Spalten wird NUR EINE angezeigt (fuer beide Sections gemeinsam,
// siehe pendingStage/displayedStage-Kommentar bei den Member-
// Deklarationen oben - Voltage/Time/Range/Curve/alle Modifier-Bits sind
// stage-zentrale Arrays, nicht getrennt pro Section).
//
// Bewusste Code-Duplikation der Pro-Stage-Widget-Erzeugung statt
// Refaktor von Marf248Widget: die bestehende, bereits getestete
// Vollversion bleibt komplett unberuehrt - gleiche Begruendung wie bei
// PanelBackground in den Expander-Dateien (siehe dortige Kommentare).
// Die Y-Offsets/Konstanten (sliderColOffsetX, voltSliderY, timeSliderY,
// stageLedY, buttonColOffsets, buttonRowsY, shapeCurveY, trangeY,
// rangeY, frameWidthMM/frameTopMM/frameBottomMM) sind 1:1 aus der
// bereits getesteten Stage-Spalte von Marf248Widget uebernommen - EIN
// Slot statt 16, aber intern unveraendertes, bewaehrtes Layout (kein
// vertikales Zusammenquetschen, keine neue Ueberlappungsgefahr). Der
// Auswahl-Knob/Aktivieren-Button sitzt stattdessen in einer separaten
// Spalte LINKS neben dem Stage-Slot, ausserhalb von dessen Y-Raster.
//
// Stand dieser Session: Panel enthaelt Auswahl/Aktivieren + den
// vollstaendigen Single-Stage-Slot (alle Voltage/Time/Range/Curve/
// Modifier-Widgets, identisch zur Vollversion). Die uebrigen, nicht
// stage-bezogenen Bedienelemente (Run/Address/Presets/Scale-Key/Time
// Mult/Outputs/Jacks) sind noch NICHT uebernommen - eigene Folge-Session
// (Panel ist bewusst erstmal schmal/unvollstaendig, um diesen Kernteil
// unabhaengig lokal testen zu koennen, siehe Projekt-Session-Prinzip
// "ein Feature pro Session").
// ---------------------------------------------------------------------

// Auswahl-Knob mit Zentrums-Ziffernanzeige (1-16). KEIN Param/ParamWidget
// (Kinovice-Entscheidung im Konzeptdokument): Pickup/Catch, VCV History
// System und Preset-Recall sollen die reine GUI-Navigation nicht
// beeinflussen. Schreibt daher direkt auf ein Modul-Member
// (module->pendingStage), eigenes Maus-Handling statt Rack-Param-Engine
// - Drag-Verhalten (Vorzeichen, Sperren des System-Cursors) analog zu
// StageSlider::onDragMove() oben, aber diskrete 16-Stufen-Rasterung
// statt kontinuierlichem Wert.
struct StageSelectKnob : OpaqueWidget {
	Marf248* module = nullptr;
	float dragAccumPx = 0.f;

	StageSelectKnob() {
		box.size = mm2px(Vec(14.f, 14.f));
	}

	void onButton(const ButtonEvent& e) override {
		OpaqueWidget::onButton(e);
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
		}
	}

	void onDragStart(const DragStartEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		dragAccumPx = 0.f;
		APP->window->cursorLock();
	}

	void onDragEnd(const DragEndEvent& e) override {
		APP->window->cursorUnlock();
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (!module)
			return;
		// 4mm Mausweg pro Stage-Schritt (grob, unkritisch bei nur 16
		// diskreten Stufen) - Maus nach oben = Wert steigt, wie bei
		// StageSlider.
		dragAccumPx -= e.mouseDelta.y;
		float pxPerStep = mm2px(4.f);
		while (dragAccumPx >= pxPerStep) {
			module->pendingStage = std::min(module->pendingStage + 1, NUM_STAGES - 1);
			dragAccumPx -= pxPerStep;
		}
		while (dragAccumPx <= -pxPerStep) {
			module->pendingStage = std::max(module->pendingStage - 1, 0);
			dragAccumPx += pxPerStep;
		}
	}

	void draw(const DrawArgs& args) override {
		float r = box.size.x / 2.f;
		Vec c = Vec(r, r);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r - 1.f);
		nvgFillColor(args.vg, nvgRGB(40, 40, 44));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		// Zentrums-Ziffer: gruen sobald pendingStage == displayedStage
		// (also der angewaehlte Wert bereits der sichtbare/aktive ist -
		// Kinovice-Entscheidung "im Knob anzeigen ob aktiv"), sonst
		// amber (gleiche Akzentfarbe wie StageSlider/TrangeButton/
		// RangeKnob, fuer ein einheitliches Erscheinungsbild).
		int pending = module ? module->pendingStage : 0;
		int displayed = module ? module->displayedStage : 0;
		bool isActive = (pending == displayed);
		NVGcolor textColor = isActive ? nvgRGB(90, 220, 110) : nvgRGB(235, 170, 60);

		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Nunito-Bold.ttf"));
		if (font && font->handle >= 0) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 15.f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, textColor);
			std::string label = string::f("%d", pending + 1);
			nvgText(args.vg, c.x, c.y, label.c_str(), NULL);
		}
	}
};

// Aktivieren-Button: uebertraegt module->pendingStage -> module->
// displayedStage. Ebenfalls bewusst kein Param, gleiche Begruendung wie
// beim Auswahl-Knob oben (reine GUI-Navigation, kein History-Eintrag).
struct StageActivateButton : OpaqueWidget {
	Marf248* module = nullptr;
	bool pressed = false;

	StageActivateButton() {
		box.size = mm2px(Vec(8.f, 8.f));
	}

	void onButton(const ButtonEvent& e) override {
		OpaqueWidget::onButton(e);
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			pressed = true;
			if (module) {
				module->displayedStage = module->pendingStage;
			}
			e.consume(this);
		}
		else if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			pressed = false;
		}
	}

	void draw(const DrawArgs& args) override {
		// Farbe zeigt den AKTIVIERUNGSZUSTAND, nicht den Maus-Klick
		// (Kinovice-Wunsch): rot solange die Auswahl (pendingStage) noch
		// nicht uebernommen ist, gruen sobald sie mit der sichtbaren Stage
		// (displayedStage) uebereinstimmt - dieselbe Bedingung wie die
		// Zentrumsziffer-Faerbung in StageSelectKnob::draw() oben, fuer ein
		// einheitliches Bild zwischen Knob und Button.
		bool isActive = module && (module->pendingStage == module->displayedStage);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 1.5f);
		nvgFillColor(args.vg, isActive ? nvgRGB(90, 220, 110) : nvgRGB(220, 70, 70));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(90, 90, 100, 200));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}
};

struct Marf248CompactWidget : ModuleWidget {
	// Label-Helfer 1:1 aus Marf248Widget uebernommen (siehe dortige
	// Kommentare - hier nicht wiederholt, da wortidentisch). Eigene Kopie
	// statt gemeinsamer Basisklasse, da ModuleWidget-Subklassen in VCV
	// Rack keine gemeinsame Zwischenklasse ueblich ist und der Aufwand
	// einer eigenen Basisklasse fuer drei Zeilen nicht lohnt.
	void addLabelBelow(Vec posMM, float yOffsetMM, const std::string& text, NVGcolor color, float fontSize = 9.f) {
		addChild(new PanelLabel(posMM + Vec(0.f, yOffsetMM), text, color, fontSize, NVG_ALIGN_CENTER | NVG_ALIGN_TOP));
	}

	void addLabelSide(Vec posMM, float xOffsetMM, float dir, const std::string& text, NVGcolor color, float fontSize = 6.5f) {
		int align = (dir >= 0.f) ? (NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE) : (NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		addChild(new PanelLabel(posMM + Vec(dir * xOffsetMM, 0.f), text, color, fontSize, align));
	}

	void addLabelVertical(Vec posMM, float xOffsetMM, const std::string& text, NVGcolor color, float fontSize) {
		addChild(new PanelLabel(posMM + Vec(xOffsetMM, 0.f), text, color, fontSize,
		                        NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, -90.f));
	}

	// Freistehendes Label ohne Element-Bezug (fuer STAGE/AKTIV-
	// Beschriftung des neuen Auswahl-Blocks) - in Marf248Widget nicht
	// noetig, da dort jedes Label an ein konkretes Element gekoppelt ist.
	void addLabel(Vec posMM, const std::string& text, NVGcolor color, float fontSize = 9.f) {
		addChild(new PanelLabel(posMM, text, color, fontSize, NVG_ALIGN_CENTER | NVG_ALIGN_TOP));
	}

	// Fuer jede der 16 Stages ALLE zugehoerigen Widgets (Params + Lights)
	// an derselben Panel-Position - Sichtbarkeit wird in step() anhand
	// von module->displayedStage umgeschaltet (siehe dort).
	std::vector<Widget*> stageWidgets[NUM_STAGES];

	Marf248CompactWidget(Marf248* module) {
		setModule(module);

		// Geometrie-Konstanten und Spaltenpositionen 1:1 aus Marf248Widget
		// uebernommen (siehe dortige ausfuehrliche Kommentare - hier nicht
		// wiederholt), MIT EINER Ausnahme: xStep/die 16-fache Wiederholung
		// entfaellt komplett, es gibt nur den einen Stage-Slot bei xStart.
		// colBOutputs/colBJacks/colBControls/colPresets ruecken dadurch
		// deutlich naeher an Section A heran als auf der 146HP-Vollversion.
		const float frameWidthMM = 32.f;
		const float frameTopMM = 12.f;
		const float frameBottomMM = 126.f;
		// gapControlsToJacks/gapJacksToOutputs (Kompaktversion-Anpassung,
		// Kinovice-Feedback "BPM-Spalte zu schmal"): jacksFrameWidthMM und
		// outputsFrameWidthMM unten wurden gegenueber der Vollversion
		// verbreitert (BPM-Anzeige ist 22mm breit, Auswahl-Knob + Aktivieren-
		// Button brauchen mehr als die urspruenglichen 12mm) - die Gaps
		// wurden entsprechend mitgezogen, um ca. 2mm Luft zu den
		// Nachbarspalten zu behalten (gleiches Prinzip wie in der
		// Vollversion, nur mit den neuen, groesseren Rahmenbreiten
		// gegengerechnet).
		const float gapControlsToJacks = 21.f;
		const float gapJacksToOutputs = 22.f;
		const float gapOutputsToStage = 26.f;
		// 12 -> 16mm (Kinovice-Feedback "Auswahl-Knob soll in die VOUT/
		// TOUT-Spalte, die muss dafuer breiter werden"): Platz fuer den
		// 14mm-Auswahl-Knob (Radius 7mm, passt jetzt mit 1mm Luft) und den
		// 8mm-Aktivieren-Button, siehe deren neue Platzierung weiter unten.
		const float outputsFrameWidthMM = 16.f;
		const float controlsFrameWidthMM = 14.f;
		// 20 -> 24mm (Kinovice-Feedback "BPM-Spalte zu schmal"):
		// TempoBpmDisplay ist 22mm breit (siehe deren Platzierung weiter
		// unten) und ragte bei 20mm Rahmenbreite links/rechts ueber den
		// Rahmen hinaus.
		const float jacksFrameWidthMM = 24.f;

		float colExt = 18.f;
		const float colToControlsGap = 19.f;
		float colAControls = colExt + colToControlsGap;
		float colAJacks = colAControls + gapControlsToJacks;
		float colAOutputs = colAJacks + gapJacksToOutputs;
		// xStart: Position des EINEN Stage-Slots (entspricht der
		// bisherigen Position von Stage 1 auf der Vollversion).
		float xStart = colAOutputs + gapOutputsToStage;
		// Kompaktversion-spezifisch: Section B folgt direkt auf den einen
		// Slot (ein gapOutputsToStage-Abstand), statt nach 15 weiteren
		// xStep-Schritten wie auf der Vollversion.
		float colBOutputs = xStart + gapOutputsToStage;
		float colBJacks = colBOutputs + gapJacksToOutputs;
		float colBControls = colBJacks + gapControlsToJacks;
		float colPresets = colBControls + colToControlsGap;

		const float presetFrameWidthMM = 20.f;
		const float presetFrameTopMM = 12.f;
		const float presetFrameBottomMM = 127.f;

		// HP aus dem tatsaechlichen Platzbedarf ableiten statt fixem Wert -
		// Kompaktversion ist deutlich schmaler als die 146HP-Vollversion,
		// aber die genaue Breite haengt von allen Spaltenabstaenden oben
		// ab. +2HP Sicherheitsmarge fuer die Preset-Rahmenkante.
		int hpInt = (int) (colPresets / 5.08f) + 2;
		const float HP = (float) hpInt;
		box.size = Vec(HP * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

		PanelBackground* panelBg = new PanelBackground;
		panelBg->box.size = box.size;
		addChild(panelBg);

		PanelBorder* panelBorder = new PanelBorder;
		panelBorder->box.size = box.size;
		addChild(panelBorder);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Modulname (wie bei Sofonisba oben links im Streifen 0-12mm; die Clear/
		// Reset/Auto-Fill-Taster liegen bei xStart, also deutlich weiter rechts).
		// Modulname zentriert wie bei den anderen Modulen; die Taster sitzen mit Abstand rechts davon.
		const float titleCenterMM = HP * 5.08f / 2.f;
		addChild(new PanelLabel(Vec(titleCenterMM, 3.5f), "LUCIA", COLOR_SYSTEM, 13.f));
		const float btnX0 = titleCenterMM + 18.f;   // Clear; Reset und Auto-Fill folgen im Abstand von 12mm

		// --- External-CV-Spalte ganz links: 1:1 aus Marf248Widget
		// (vier modulweit geteilte EXT-A/B/C/D-Eingaenge).
		const float extFrameWidthMM = 20.f;
		const float extFrameTopMM = 12.f;
		// Wie in Marf248Widget (108mm) - Auswahl-Knob/Aktivieren-Button
		// sitzen jetzt stattdessen in der VOUT/TOUT-Spalte (Kinovice-
		// Feedback, siehe dortige Platzierung weiter unten), diese Spalte
		// bleibt daher unveraendert.
		const float extFrameBottomMM = 108.f;
		StageFrame* extFrame = new StageFrame;
		extFrame->box.pos = mm2px(Vec(colExt - extFrameWidthMM / 2.f, extFrameTopMM));
		extFrame->box.size = mm2px(Vec(extFrameWidthMM, extFrameBottomMM - extFrameTopMM));
		addChild(extFrame);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colExt, 22)), module, Marf248::EXT_A_INPUT));
		addLabelBelow(Vec(colExt, 22), 5.5f, "EXT A", COLOR_STAGEMOD, 10.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colExt, 44)), module, Marf248::EXT_B_INPUT));
		addLabelBelow(Vec(colExt, 44), 5.5f, "EXT B", COLOR_STAGEMOD, 10.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colExt, 66)), module, Marf248::EXT_C_INPUT));
		addLabelBelow(Vec(colExt, 66), 5.5f, "EXT C", COLOR_STAGEMOD, 10.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colExt, 88)), module, Marf248::EXT_D_INPUT));
		addLabelBelow(Vec(colExt, 88), 5.5f, "EXT D", COLOR_STAGEMOD, 10.f);

		// --- Jacks-Rahmen Section A/B: 1:1 aus Marf248Widget.
		const float jacksFrameTopMM = 12.f;
		const float jacksFrameBottomMM = 121.f;
		float jacksColsFrame[2] = {colAJacks, colBJacks};
		for (int s = 0; s < 2; s++) {
			StageFrame* jacksFrame = new StageFrame;
			jacksFrame->box.pos = mm2px(Vec(jacksColsFrame[s] - jacksFrameWidthMM / 2.f, jacksFrameTopMM));
			jacksFrame->box.size = mm2px(Vec(jacksFrameWidthMM, jacksFrameBottomMM - jacksFrameTopMM));
			addChild(jacksFrame);
		}

		// --- Outputs-Rahmen (VOUT/TOUT/TIME) Section A/B: 1:1 aus
		// Marf248Widget, ABER Section-A-Rahmen (Kinovice-Feedback) nach
		// unten verlaengert, da darunter jetzt Auswahl-Knob + Aktivieren-
		// Button fuer den Single-Stage-Slot sitzen (siehe unten) - Section
		// B bleibt bei der urspruenglichen Hoehe, da dort nichts
		// dazukommt.
		const float outputsFrameTopMM = 12.f;
		const float outputsFrameBottomMM = 61.f;
		const float outputsFrameBottomMM_A = 126.f;
		float outputsColsFrame[2] = {colAOutputs, colBOutputs};
		float outputsColsFrameBottom[2] = {outputsFrameBottomMM_A, outputsFrameBottomMM};
		for (int s = 0; s < 2; s++) {
			StageFrame* outputsFrame = new StageFrame;
			outputsFrame->box.pos = mm2px(Vec(outputsColsFrame[s] - outputsFrameWidthMM / 2.f, outputsFrameTopMM));
			outputsFrame->box.size = mm2px(Vec(outputsFrameWidthMM, outputsColsFrameBottom[s] - outputsFrameTopMM));
			addChild(outputsFrame);
		}

		// Auswahl-Knob + Aktivieren-Button fuer den Single-Stage-Slot:
		// jetzt in der (verbreiterten, siehe outputsFrameWidthMM oben)
		// VOUT/TOUT/TIME-Spalte von Section A, direkt unter TIME (y=49) -
		// Kinovice-Feedback, naeher am eigentlichen Stage-Slot als die
		// vorherige Platzierung in der Ext-Spalte. pendingStage/
		// displayedStage gelten modulweit fuer beide Sections (siehe
		// Member-Kommentar in Marf248), daher genuegt EINE Instanz hier,
		// keine zweite in der Section-B-Spalte noetig. Erster Entwurf,
		// lokal gegenchecken.
		addLabel(Vec(colAOutputs, 70.f), "STAGE", COLOR_STAGEMOD, 7.f);
		StageSelectKnob* selectKnob = new StageSelectKnob;
		selectKnob->module = module;
		selectKnob->box.pos = mm2px(Vec(colAOutputs, 84.f)).minus(selectKnob->box.size.div(2));
		addChild(selectKnob);
		addLabel(Vec(colAOutputs, 95.f), "AKTIVIEREN", COLOR_STAGEMOD, 6.f);
		StageActivateButton* activateBtn = new StageActivateButton;
		activateBtn->module = module;
		activateBtn->box.pos = mm2px(Vec(colAOutputs, 106.f)).minus(activateBtn->box.size.div(2));
		addChild(activateBtn);

		// --- Section A und B: identischer Aufbau, 1:1 aus Marf248Widget
		// uebernommen (Param-/Input-/Output-Id-Arrays, Zuordnung siehe
		// dortige Kommentare).
		float controlsCols[2] = {colAControls, colBControls};
		float jacksCols[2] = {colAJacks, colBJacks};
		float outputsCols[2] = {colAOutputs, colBOutputs};
		int runParams[2] = {Marf248::RUN_A_PARAM, Marf248::RUN_B_PARAM};
		int runLights[2] = {Marf248::RUN_A_LIGHT, Marf248::RUN_B_LIGHT};
		int modeParams[2] = {Marf248::MODE_A_PARAM, Marf248::MODE_B_PARAM};
		int modeLights[2] = {Marf248::MODE_A_LIGHT, Marf248::MODE_B_LIGHT};
		int pendulumParams[2] = {Marf248::PENDULUM_A_PARAM, Marf248::PENDULUM_B_PARAM};
		int pendulumLights[2] = {Marf248::PENDULUM_A_LIGHT, Marf248::PENDULUM_B_LIGHT};
		int startInputs[2] = {Marf248::START_A_INPUT, Marf248::START_B_INPUT};
		int stopInputs[2] = {Marf248::STOP_A_INPUT, Marf248::STOP_B_INPUT};
		int voutOutputs[2] = {Marf248::VOUT_A_OUTPUT, Marf248::VOUT_B_OUTPUT};
		int toutOutputs[2] = {Marf248::TOUT_A_OUTPUT, Marf248::TOUT_B_OUTPUT};
		int timeStaticOutputs[2] = {Marf248::TIMESTATIC_A_OUTPUT, Marf248::TIMESTATIC_B_OUTPUT};
		int apoutOutputs[2] = {Marf248::APOUT_A_OUTPUT, Marf248::APOUT_B_OUTPUT};
		int p1ActiveLights[2] = {Marf248::P1_ACTIVE_A_LIGHT, Marf248::P1_ACTIVE_B_LIGHT};
		int p1Outputs[2] = {Marf248::P1OUT_A_OUTPUT, Marf248::P1OUT_B_OUTPUT};
		int p2ActiveLights[2] = {Marf248::P2_ACTIVE_A_LIGHT, Marf248::P2_ACTIVE_B_LIGHT};
		int p2Outputs[2] = {Marf248::P2OUT_A_OUTPUT, Marf248::P2OUT_B_OUTPUT};
		int addrKnobParams[2] = {Marf248::STAGE_ADDR_KNOB_A_PARAM, Marf248::STAGE_ADDR_KNOB_B_PARAM};
		int addrModeParams[2] = {Marf248::STAGE_ADDR_MODE_A_PARAM, Marf248::STAGE_ADDR_MODE_B_PARAM};
		int addrModeLights[2] = {Marf248::STAGE_ADDR_MODE_A_LIGHT, Marf248::STAGE_ADDR_MODE_B_LIGHT};
		int addrContParams[2] = {Marf248::STAGE_ADDR_CONT_A_PARAM, Marf248::STAGE_ADDR_CONT_B_PARAM};
		int addrContLights[2] = {Marf248::STAGE_ADDR_CONT_A_LIGHT, Marf248::STAGE_ADDR_CONT_B_LIGHT};
		int addrStrobeParams[2] = {Marf248::STAGE_ADDR_STROBE_A_PARAM, Marf248::STAGE_ADDR_STROBE_B_PARAM};
		int addrResetParams[2] = {Marf248::STAGE_ADDR_RESET_A_PARAM, Marf248::STAGE_ADDR_RESET_B_PARAM};
		int addrStrobeLights[2] = {Marf248::STAGE_ADDR_STROBE_A_LIGHT, Marf248::STAGE_ADDR_STROBE_B_LIGHT};
		int addrResetLights[2] = {Marf248::STAGE_ADDR_RESET_A_LIGHT, Marf248::STAGE_ADDR_RESET_B_LIGHT};
		int addrExtInputs[2] = {Marf248::STAGE_ADDR_EXT_A_INPUT, Marf248::STAGE_ADDR_EXT_B_INPUT};
		int addrExtChannelInputs[2] = {Marf248::STAGE_ADDR_EXT_CHANNEL_A_INPUT, Marf248::STAGE_ADDR_EXT_CHANNEL_B_INPUT};
		int timeMultParams[2] = {Marf248::TIME_MULT_A_PARAM, Marf248::TIME_MULT_B_PARAM};
		int timeMultAttenParams[2] = {Marf248::TIME_MULT_ATTEN_A_PARAM, Marf248::TIME_MULT_ATTEN_B_PARAM};
		int timeMultCvInputs[2] = {Marf248::TIME_MULT_CV_A_INPUT, Marf248::TIME_MULT_CV_B_INPUT};
		int beatsFactorParams[2] = {Marf248::BEATS_FACTOR_A_PARAM, Marf248::BEATS_FACTOR_B_PARAM};
		int beatsFactorCvInputs[2] = {Marf248::BEATS_FACTOR_CV_A_INPUT, Marf248::BEATS_FACTOR_CV_B_INPUT};

		for (int s = 0; s < 2; s++) {
			float ctrl = controlsCols[s];
			float jack = jacksCols[s];
			float outCol = outputsCols[s];

			const float controlsFrameTopMM = 12.f;
			const float controlsFrameBottomMM = 120.f;
			StageFrame* controlsFrame = new StageFrame;
			controlsFrame->box.pos = mm2px(Vec(ctrl - controlsFrameWidthMM / 2.f, controlsFrameTopMM));
			controlsFrame->box.size = mm2px(Vec(controlsFrameWidthMM, controlsFrameBottomMM - controlsFrameTopMM));
			addChild(controlsFrame);

			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ctrl, 20)), module, timeMultParams[s]));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(ctrl, 30)), module, timeMultAttenParams[s]));
			addParam(createParamCentered<BeatsFactorTrimpot>(mm2px(Vec(ctrl, 38)), module, beatsFactorParams[s]));

			HDivider* divTimeMultRun = new HDivider;
			divTimeMultRun->box.pos = mm2px(Vec(ctrl - 4.f, 44.f));
			divTimeMultRun->box.size = mm2px(Vec(8.f, 0.f));
			addChild(divTimeMultRun);

			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 50)), module, runParams[s]));
			addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(ctrl, 50)), module, runLights[s]));

			HDivider* divRunMode = new HDivider;
			divRunMode->box.pos = mm2px(Vec(ctrl - 4.f, 58.f));
			divRunMode->box.size = mm2px(Vec(8.f, 0.f));
			addChild(divRunMode);

			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 64)), module, modeParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 64)), module, modeLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 72)), module, pendulumParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 72)), module, pendulumLights[s]));

			HDivider* divModeAddr = new HDivider;
			divModeAddr->box.pos = mm2px(Vec(ctrl - 4.f, 77.f));
			divModeAddr->box.size = mm2px(Vec(8.f, 0.f));
			addChild(divModeAddr);

			addParam(createParamCentered<Trimpot>(mm2px(Vec(ctrl, 83)), module, addrKnobParams[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 91)), module, addrModeParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 91)), module, addrModeLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 99)), module, addrContParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 99)), module, addrContLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 107)), module, addrStrobeParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 107)), module, addrStrobeLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 115)), module, addrResetParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 115)), module, addrResetLights[s]));

			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 20)), module, startInputs[s]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 28)), module, stopInputs[s]));

			HDivider* divStartTimeMult = new HDivider;
			divStartTimeMult->box.pos = mm2px(Vec(jack - 9.f, 33.f));
			divStartTimeMult->box.size = mm2px(Vec(18.f, 0.f));
			addChild(divStartTimeMult);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 41)), module, timeMultCvInputs[s]));

			TempoBpmDisplay* bpmDisplay = new TempoBpmDisplay;
			bpmDisplay->box.size = mm2px(Vec(22.f, 9.f));
			bpmDisplay->box.pos = mm2px(Vec(jack, 51)).minus(bpmDisplay->box.size.div(2));
			bpmDisplay->module = dynamic_cast<Marf248*>(module);
			bpmDisplay->section = s;
			addChild(bpmDisplay);

			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 63)), module, beatsFactorCvInputs[s]));

			HDivider* divOutPulse = new HDivider;
			divOutPulse->box.pos = mm2px(Vec(jack - 9.f, 68.f));
			divOutPulse->box.size = mm2px(Vec(18.f, 0.f));
			addChild(divOutPulse);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack, 76)), module, p1Outputs[s]));
			addChild(createLightCentered<PulseRingLight>(mm2px(Vec(jack, 76)), module, p1ActiveLights[s]));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack, 84)), module, p2Outputs[s]));
			addChild(createLightCentered<PulseRingLight>(mm2px(Vec(jack, 84)), module, p2ActiveLights[s]));

			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack, 92)), module, apoutOutputs[s]));

			HDivider* divApoutAddr = new HDivider;
			divApoutAddr->box.pos = mm2px(Vec(jack - 9.f, 97.f));
			divApoutAddr->box.size = mm2px(Vec(18.f, 0.f));
			addChild(divApoutAddr);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 105)), module, addrExtInputs[s]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 113)), module, addrExtChannelInputs[s]));

			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(outCol, 20)), module, voutOutputs[s]));
			addLabelBelow(Vec(outCol, 20), 5.5f, "VOUT", (s == 0 ? COLOR_SECTION_A : COLOR_SECTION_B), 10.f);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(outCol, 34.5f)), module, toutOutputs[s]));
			addLabelBelow(Vec(outCol, 34.5f), 5.5f, "TOUT", (s == 0 ? COLOR_SECTION_A : COLOR_SECTION_B), 10.f);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(outCol, 49)), module, timeStaticOutputs[s]));
			addLabelBelow(Vec(outCol, 49), 5.5f, "TIME", (s == 0 ? COLOR_SECTION_A : COLOR_SECTION_B), 10.f);
		}

		// --- Single-Stage-Slot bei xStart: 1:1-Layout aus Marf248Widget
		// (siehe dortige Kommentare), aber nur EIN x statt 16 realer
		// Spalten - alle 16 Widget-Sets uebereinander, Sichtbarkeit steuert
		// step() unten.
		const float sliderColOffsetX = -10.f;
		const float voltSliderY = 42.f;
		const float timeSliderY = 98.f;
		const float stageLedY = 13.f;
		const float buttonColOffsets[2] = {2.f, 10.f};
		const float buttonRowsY[8] = {20.f, 28.f, 36.f, 44.f, 52.f, 60.f, 68.f, 76.f};
		const float shapeCurveOffsetX = 6.f;
		const float shapeCurveY = 88.f;
		const float trangeOffsetX = 6.f;
		const float trangeY = 100.f;
		const float rangeOffsetX = 6.f;
		const float rangeY = 115.f;

		float x = xStart;
		float sliderX = x + sliderColOffsetX;

		StageFrame* frame = new StageFrame;
		frame->box.pos = mm2px(Vec(x - frameWidthMM / 2.f, frameTopMM));
		frame->box.size = mm2px(Vec(frameWidthMM, frameBottomMM - frameTopMM));
		addChild(frame);

		for (int i = 0; i < NUM_STAGES; i++) {
			ParamWidget* voltSlider = createParamCentered<StageSlider>(mm2px(Vec(sliderX, voltSliderY)), module, Marf248::STAGE_VOLTAGE_PARAM + i);
			addParam(voltSlider);
			stageWidgets[i].push_back(voltSlider);

			ParamWidget* timeSlider = createParamCentered<StageSlider>(mm2px(Vec(sliderX, timeSliderY)), module, Marf248::STAGE_TIME_PARAM + i);
			addParam(timeSlider);
			stageWidgets[i].push_back(timeSlider);

			app::LightWidget* stageLedA = createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(sliderX - 1.5f, stageLedY)), module, Marf248::STAGE_LIGHT_A + i);
			addChild(stageLedA);
			stageWidgets[i].push_back(stageLedA);
			app::LightWidget* stageLedB = createLightCentered<MediumLight<RedLight>>(mm2px(Vec(sliderX + 1.5f, stageLedY)), module, Marf248::STAGE_LIGHT_B + i);
			addChild(stageLedB);
			stageWidgets[i].push_back(stageLedB);

			auto addModBtn = [&](ParamWidget* w, app::LightWidget* l) {
				addParam(w);
				stageWidgets[i].push_back(w);
				addChild(l);
				stageWidgets[i].push_back(l);
			};

			addModBtn(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[0])), module, Marf248::STAGE_PULSE1_PARAM + i),
			          createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[0])), module, Marf248::STAGE_PULSE1_LIGHT + i));
			addModBtn(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[0])), module, Marf248::STAGE_PULSE2_PARAM + i),
			          createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[0])), module, Marf248::STAGE_PULSE2_LIGHT + i));

			addModBtn(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[1])), module, Marf248::STAGE_SKIP_PARAM + i),
			          createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[1])), module, Marf248::STAGE_SKIP_LIGHT + i));
			addModBtn(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[1])), module, Marf248::STAGE_MUTE_PARAM + i),
			          createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[1])), module, Marf248::STAGE_MUTE_LIGHT + i));

			addModBtn(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[2])), module, Marf248::STAGE_STOP_PARAM + i),
			          createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[2])), module, Marf248::STAGE_STOP_LIGHT + i));
			addModBtn(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[2])), module, Marf248::STAGE_SUSTAIN_PARAM + i),
			          createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[2])), module, Marf248::STAGE_SUSTAIN_LIGHT + i));

			addModBtn(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[3])), module, Marf248::STAGE_ENABLE_PARAM + i),
			          createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[3])), module, Marf248::STAGE_ENABLE_LIGHT + i));

			addModBtn(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[4])), module, Marf248::STAGE_FIRST_PARAM + i),
			          createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[4])), module, Marf248::STAGE_FIRST_LIGHT + i));
			addModBtn(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[4])), module, Marf248::STAGE_LAST_PARAM + i),
			          createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[4])), module, Marf248::STAGE_LAST_LIGHT + i));

			addModBtn(createParamCentered<StageModBroadcastButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[5])), module, Marf248::STAGE_SLOPE_PARAM + i),
			          createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[5])), module, Marf248::STAGE_SLOPE_LIGHT + i));
			addModBtn(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[5])), module, Marf248::STAGE_QUANTGLIDE_PARAM + i),
			          createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[5])), module, Marf248::STAGE_QUANTGLIDE_LIGHT + i));

			addModBtn(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[6])), module, Marf248::STAGE_QUANT_PARAM + i),
			          createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[6])), module, Marf248::STAGE_QUANT_LIGHT + i));
			addModBtn(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[6])), module, Marf248::STAGE_VOLTEXT_PARAM + i),
			          createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[6])), module, Marf248::STAGE_VOLTEXT_LIGHT + i));

			addModBtn(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[7])), module, Marf248::STAGE_TIMEEXT_PARAM + i),
			          createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x + buttonColOffsets[0], buttonRowsY[7])), module, Marf248::STAGE_TIMEEXT_LIGHT + i));
			addModBtn(createParamCentered<LEDButton>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[7])), module, Marf248::STAGE_NORMALIZE_PARAM + i),
			          createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(x + buttonColOffsets[1], buttonRowsY[7])), module, Marf248::STAGE_NORMALIZE_LIGHT + i));

			ParamWidget* shapeBtn = createParamCentered<LEDButton>(mm2px(Vec(x + shapeCurveOffsetX + 4.f, shapeCurveY)), module, Marf248::STAGE_SHAPE_PARAM + i);
			addParam(shapeBtn);
			stageWidgets[i].push_back(shapeBtn);
			app::LightWidget* shapeExpLight = createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x + shapeCurveOffsetX + 1.f, shapeCurveY)), module, Marf248::STAGE_SHAPE_EXP_LIGHT + i);
			addChild(shapeExpLight);
			stageWidgets[i].push_back(shapeExpLight);
			app::LightWidget* shapeLogLight = createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x + shapeCurveOffsetX + 7.f, shapeCurveY)), module, Marf248::STAGE_SHAPE_LOG_LIGHT + i);
			addChild(shapeLogLight);
			stageWidgets[i].push_back(shapeLogLight);
			ParamWidget* curveTrim = createParamCentered<StageCurveTrimpot>(mm2px(Vec(x + shapeCurveOffsetX - 4.f, shapeCurveY)), module, Marf248::STAGE_CURVE_PARAM + i);
			addParam(curveTrim);
			stageWidgets[i].push_back(curveTrim);

			TrangeButton* trangeBtn = createParamCentered<TrangeButton>(mm2px(Vec(x + trangeOffsetX, trangeY)), module, Marf248::STAGE_TRANGE_PARAM + i);
			Vec trangeCenter = trangeBtn->box.getCenter();
			trangeBtn->box.size = mm2px(Vec(9.f, 9.f));
			trangeBtn->box.pos = trangeCenter.minus(trangeBtn->box.size.div(2));
			addParam(trangeBtn);
			stageWidgets[i].push_back(trangeBtn);

			ParamWidget* rangeKnob = createParamCentered<RangeKnob>(mm2px(Vec(x + rangeOffsetX, rangeY)), module, Marf248::STAGE_RANGE_PARAM + i);
			addParam(rangeKnob);
			stageWidgets[i].push_back(rangeKnob);
		}

		// --- Global Clear/Reset/AutoFill: zentriert ueber dem EINEN
		// Stage-Slot (statt ueber 16 Spalten gemittelt wie auf der
		// Vollversion) - 8mm-Pitch um xStart herum statt 12mm-Pitch ab
		// colClear, damit alle drei Buttons innerhalb der 32mm-Rahmenbreite
		// bleiben.
		addParam(createParamCentered<LEDButton>(mm2px(Vec(btnX0, 6)), module, Marf248::CLEAR_PARAM));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(btnX0, 6)), module, Marf248::CLEAR_LIGHT));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(btnX0 + 12.f, 6)), module, Marf248::RESET_VALUES_PARAM));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(btnX0 + 12.f, 6)), module, Marf248::RESET_VALUES_LIGHT));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(btnX0 + 24.f, 6)), module, Marf248::AUTOFILL_PARAM));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(btnX0 + 24.f, 6)), module, Marf248::AUTOFILL_LIGHT));

		// --- Preset-Spalte ganz rechts: 1:1 aus Marf248Widget.
		StageFrame* presetFrame = new StageFrame;
		presetFrame->box.pos = mm2px(Vec(colPresets - presetFrameWidthMM / 2.f, presetFrameTopMM));
		presetFrame->box.size = mm2px(Vec(presetFrameWidthMM, presetFrameBottomMM - presetFrameTopMM));
		addChild(presetFrame);

		addLabelVertical(Vec(colPresets, (20.f + 44.f) / 2.f), 6.f, "PRESETS", COLOR_SYSTEM, 16.f);

		addParam(createParamCentered<LEDButton>(mm2px(Vec(colPresets, 20)), module, Marf248::LOAD_PARAM));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(colPresets, 20)), module, Marf248::LOAD_LIGHT));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(colPresets, 28)), module, Marf248::SAVE_PARAM));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(colPresets, 28)), module, Marf248::SAVE_LIGHT));

		addParam(createParamCentered<LEDButton>(mm2px(Vec(colPresets, 36)), module, Marf248::SCALE_PARAM));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(colPresets, 36)), module, Marf248::SCALE_LIGHT));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(colPresets, 44)), module, Marf248::KEY_PARAM));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(colPresets, 44)), module, Marf248::KEY_LIGHT));

		HDivider* divPresets = new HDivider;
		divPresets->box.pos = mm2px(Vec(colPresets - 9.f, 49.f));
		divPresets->box.size = mm2px(Vec(18.f, 0.f));
		addChild(divPresets);

		const float presetStartY = 55.f;
		const float presetStepY = 6.f;

		addLabelVertical(Vec(colPresets, (presetStartY + (presetStartY + 11.f * presetStepY)) / 2.f), 6.f, "PRESET NR", COLOR_SYSTEM, 16.f);

		for (int p = 0; p < 12; p++) {
			float y = presetStartY + p * presetStepY;
			addParam(createParamCentered<LEDButton>(mm2px(Vec(colPresets, y)), module, Marf248::PRESET_PARAM + p));
			addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(colPresets, y)), module, Marf248::PRESET_LIGHT + p));
			addLabelSide(Vec(colPresets, y), 4.f, -1.f, std::to_string(p + 1), COLOR_SYSTEM, 13.f);
		}
	}

	// Sichtbarkeit: genau ein Stage-Set (i == module->displayedStage) ist
	// sichtbar, alle anderen 15 ausgeblendet - triviale, guenstige
	// Pro-Frame-Operation ohne RTKit-Relevanz (reine GUI-Schicht
	// ausserhalb von process(), siehe Konzeptdokument-Begruendung).
	void step() override {
		Marf248* module = dynamic_cast<Marf248*>(this->module);
		if (module) {
			for (int i = 0; i < NUM_STAGES; i++) {
				bool vis = (i == module->displayedStage);
				for (Widget* w : stageWidgets[i]) {
					w->visible = vis;
				}
			}
		}
		ModuleWidget::step();
	}
};


Model* modelMarf248 = createModel<Marf248, Marf248Widget>("Marf248");
Model* modelMarf248Compact = createModel<Marf248, Marf248CompactWidget>("Marf248Compact");
