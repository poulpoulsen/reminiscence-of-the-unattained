#include "Marf248.hpp"
#include "Marf248TriggerExpander.hpp"
#include "Marf248CvExpander.hpp"

struct Marf248; // Forward-Deklaration fuer StageVoltageQuantity

// Zeigt den Voltage-Regler-Wert als Notenname (z.B. "C#4") an, wenn Quantize
// fuer die jeweilige Stage aktiv ist; sonst normale Volt-Anzeige. Die
// Methode wird erst NACH der vollstaendigen Definition von Marf248
// implementiert (siehe unten), da sie auf dessen Member zugreift.
struct StageVoltageQuantity : ParamQuantity {
	int stageIndex = 0;
	std::string getDisplayValueString() override;
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

struct Marf248 : Module {
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
		RUN_B_LIGHT,
		// Mode (an = Reverse, aus = Forward) und Pendulum (an = aktiv) je
		// eine LED pro Section.
		MODE_A_LIGHT,
		PENDULUM_A_LIGHT,
		MODE_B_LIGHT,
		PENDULUM_B_LIGHT,
		// Dreifarbige Stage-Address-Statusanzeige je Section: RUN_x_LIGHT
		// (bereits vorhanden, gruen) bekommt hier zwei Partner-LEDs an
		// gleicher Panel-Position dazu - HOLD (gelb) und STOP (rot) - statt
		// eines einzelnen RGB-Lights, analog zum bestehenden Shape-Button-
		// Muster (zwei einzelne LEDs statt einer kombinierten).
		HOLD_A_LIGHT,
		STOP_A_LIGHT,
		HOLD_B_LIGHT,
		STOP_B_LIGHT,
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
		// Output LEDs (Manual "Output LEDs (Two Red LEDs): Display the gate
		// outputs"): zeigen den tatsaechlichen P1OUT/P2OUT-Trigger in
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
		LIGHTS_LEN
	};

	// Geteilter Stage-Zustand, jetzt per Klick-Buttons im Panel gesetzt
	// (statt Rechtsklick-Menue) und per dataToJson/dataFromJson persistiert.
	int stageModifier[NUM_STAGES] = {};

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

	Afg afgA;
	Afg afgB;

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
		}

		stageButtonDivider.setDivision(32);

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
		configParam(TIME_MULT_A_PARAM, 0.5f, 4.f, 1.f, "Section A Time Multiplier", "x");
		configParam(TIME_MULT_ATTEN_A_PARAM, -1.f, 1.f, 0.f, "Section A Time Multiplier CV Attenuverter");
		configParam(TIME_MULT_B_PARAM, 0.5f, 4.f, 1.f, "Section B Time Multiplier", "x");
		configParam(TIME_MULT_ATTEN_B_PARAM, -1.f, 1.f, 0.f, "Section B Time Multiplier CV Attenuverter");

		configInput(START_A_INPUT, "Section A Start trigger");
		configInput(STOP_A_INPUT, "Section A Stop trigger");
		configInput(START_B_INPUT, "Section B Start trigger");
		configInput(STOP_B_INPUT, "Section B Stop trigger");
		configInput(EXT_A_INPUT, "External CV A");
		configInput(EXT_B_INPUT, "External CV B");
		configInput(EXT_C_INPUT, "External CV C");
		configInput(EXT_D_INPUT, "External CV D");
		configInput(STAGE_ADDR_EXT_A_INPUT, "Section A Stage Address CV");
		configInput(STAGE_ADDR_EXT_B_INPUT, "Section B Stage Address CV");
		configInput(TIME_MULT_CV_A_INPUT, "Section A Time Multiplier CV");
		configInput(TIME_MULT_CV_B_INPUT, "Section B Time Multiplier CV");

		configOutput(VOUT_A_OUTPUT, "Section A Voltage");
		configOutput(TOUT_A_OUTPUT, "Section A Time (interval ramp)");
		configOutput(APOUT_A_OUTPUT, "Section A All pulses");
		configOutput(P1OUT_A_OUTPUT, "Section A Programmed Pulse 1");
		configOutput(P2OUT_A_OUTPUT, "Section A Programmed Pulse 2");
		configOutput(VOUT_B_OUTPUT, "Section B Voltage");
		configOutput(TOUT_B_OUTPUT, "Section B Time (interval ramp)");
		configOutput(APOUT_B_OUTPUT, "Section B All pulses");
		configOutput(P1OUT_B_OUTPUT, "Section B Programmed Pulse 1");
		configOutput(P2OUT_B_OUTPUT, "Section B Programmed Pulse 2");

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

	void process(const ProcessArgs& args) override {
		// Marf248TriggerExpander-Lookup: bewusst ganz an den Anfang von
		// process() gezogen (frueher stand dieser Block erst nach der Stage-
		// Address-Verarbeitung), da der Stage-Address-Block weiter unten den
		// SEXT-Trigger (externe Strobe-Pulse) bereits kennen muss, bevor
		// applyStageAddress() aufgerufen wird. Sucht links UND rechts, damit
		// der Expander auf beiden Seiten andocken kann.
		Module* leftNeighbor = getLeftExpander().module;
		Module* rightNeighbor = getRightExpander().module;
		Module* trigExp = nullptr;
		if (leftNeighbor && leftNeighbor->getModel() == modelMarf248TriggerExpander) {
			trigExp = leftNeighbor;
		}
		else if (rightNeighbor && rightNeighbor->getModel() == modelMarf248TriggerExpander) {
			trigExp = rightNeighbor;
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
		if (stageButtonDivider.process()) {
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
			lights[STAGE_SHAPE_EXP_LIGHT + i].setBrightness(stageModifier[i] & MOD_SLEW_EXP ? 1.f : 0.f);
			lights[STAGE_SHAPE_LOG_LIGHT + i].setBrightness(stageModifier[i] & MOD_SLEW_LOG ? 1.f : 0.f);
			lights[STAGE_NORMALIZE_LIGHT + i].setBrightness(stageModifier[i] & MOD_SLEW_NORMALIZE ? 1.f : 0.f);
			lights[STAGE_VOLTEXT_LIGHT + i].setBrightness(stageModifier[i] & MOD_VOLT_EXT ? 1.f : 0.f);
			lights[STAGE_TIMEEXT_LIGHT + i].setBrightness(stageModifier[i] & MOD_TIME_EXT ? 1.f : 0.f);
			lights[STAGE_PULSE1_LIGHT + i].setBrightness(stageModifier[i] & MOD_PULSE1 ? 1.f : 0.f);
			lights[STAGE_PULSE2_LIGHT + i].setBrightness(stageModifier[i] & MOD_PULSE2 ? 1.f : 0.f);
			lights[STAGE_SKIP_LIGHT + i].setBrightness(stageModifier[i] & MOD_SKIP ? 1.f : 0.f);
		}
		}

		// Stage SKIP Poly-Gate vom Trigger-Expander: bewusst AUSSERHALB des
		// stageButtonDivider-Blocks oben und jeden Frame verarbeitet, da CV
		// pegelgetreu reagieren soll, statt mit dessen Dezimierungs-Latenz
		// (anders als die reinen Klick-Buttons, wo das laut Profiling
		// unerheblich ist). Nur wirksam, solange die Buchse gepatcht ist -
		// dann hat das CV Vorrang und ueberschreibt einen im selben Frame
		// verarbeiteten manuellen Klick auf STAGE_SKIP_PARAM (siehe
		// stageSkipTrigger-Verarbeitung oben). Ohne Kabel bleibt exakt der
		// zuletzt per Klick gesetzte Zustand erhalten. getPolyVoltage(i)
		// liefert 0V fuer nicht gepatchte/unbenutzte Kanaele (siehe
		// PRESET_LOAD_POLY_INPUT-Kommentar in Marf248TriggerExpander.hpp),
		// loescht MOD_SKIP fuer die betreffende Stage also automatisch.
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_SKIP_POLY_INPUT).isConnected()) {
			const float GATE_THRESHOLD = 2.f;
			for (int i = 0; i < NUM_STAGES; i++) {
				bool skipGateHigh = trigExp->getInput(Marf248TriggerExpander::STAGE_SKIP_POLY_INPUT).getPolyVoltage(i) >= GATE_THRESHOLD;
				if (skipGateHigh) {
					stageModifier[i] |= MOD_SKIP;
				}
				else {
					stageModifier[i] &= ~MOD_SKIP;
				}
				// Eigenstaendiges Licht-Update statt auf den naechsten
				// stageButtonDivider-Zyklus zu warten - CV-Override soll
				// optisch ohne Dezimierungs-Latenz sichtbar sein.
				lights[STAGE_SKIP_LIGHT + i].setBrightness(skipGateHigh ? 1.f : 0.f);
			}
		}

		// Stage FIRST Poly-Gate vom Trigger-Expander: identisches Prinzip wie
		// STAGE_SKIP_POLY_INPUT oben (siehe dortigen Kommentar fuer Details).
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_FIRST_POLY_INPUT).isConnected()) {
			const float GATE_THRESHOLD = 2.f;
			for (int i = 0; i < NUM_STAGES; i++) {
				bool gateHigh = trigExp->getInput(Marf248TriggerExpander::STAGE_FIRST_POLY_INPUT).getPolyVoltage(i) >= GATE_THRESHOLD;
				if (gateHigh) {
					stageModifier[i] |= MOD_FIRST;
				}
				else {
					stageModifier[i] &= ~MOD_FIRST;
				}
				lights[STAGE_FIRST_LIGHT + i].setBrightness(gateHigh ? 1.f : 0.f);
			}
		}

		// Stage LAST Poly-Gate vom Trigger-Expander: identisches Prinzip wie
		// STAGE_SKIP_POLY_INPUT oben.
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_LAST_POLY_INPUT).isConnected()) {
			const float GATE_THRESHOLD = 2.f;
			for (int i = 0; i < NUM_STAGES; i++) {
				bool gateHigh = trigExp->getInput(Marf248TriggerExpander::STAGE_LAST_POLY_INPUT).getPolyVoltage(i) >= GATE_THRESHOLD;
				if (gateHigh) {
					stageModifier[i] |= MOD_LAST;
				}
				else {
					stageModifier[i] &= ~MOD_LAST;
				}
				lights[STAGE_LAST_LIGHT + i].setBrightness(gateHigh ? 1.f : 0.f);
			}
		}

		// Stage STOP Poly-Gate vom Trigger-Expander: identisches Prinzip wie
		// STAGE_SKIP_POLY_INPUT oben.
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_STOP_POLY_INPUT).isConnected()) {
			const float GATE_THRESHOLD = 2.f;
			for (int i = 0; i < NUM_STAGES; i++) {
				bool gateHigh = trigExp->getInput(Marf248TriggerExpander::STAGE_STOP_POLY_INPUT).getPolyVoltage(i) >= GATE_THRESHOLD;
				if (gateHigh) {
					stageModifier[i] |= MOD_STOP;
				}
				else {
					stageModifier[i] &= ~MOD_STOP;
				}
				lights[STAGE_STOP_LIGHT + i].setBrightness(gateHigh ? 1.f : 0.f);
			}
		}

		// Stage SUSTAIN Poly-Gate vom Trigger-Expander: identisches Prinzip
		// wie STAGE_SKIP_POLY_INPUT oben.
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_SUSTAIN_POLY_INPUT).isConnected()) {
			const float GATE_THRESHOLD = 2.f;
			for (int i = 0; i < NUM_STAGES; i++) {
				bool gateHigh = trigExp->getInput(Marf248TriggerExpander::STAGE_SUSTAIN_POLY_INPUT).getPolyVoltage(i) >= GATE_THRESHOLD;
				if (gateHigh) {
					stageModifier[i] |= MOD_SUSTAIN;
				}
				else {
					stageModifier[i] &= ~MOD_SUSTAIN;
				}
				lights[STAGE_SUSTAIN_LIGHT + i].setBrightness(gateHigh ? 1.f : 0.f);
			}
		}

		// Stage ENABLE Poly-Gate vom Trigger-Expander: identisches Prinzip
		// wie STAGE_SKIP_POLY_INPUT oben.
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_ENABLE_POLY_INPUT).isConnected()) {
			const float GATE_THRESHOLD = 2.f;
			for (int i = 0; i < NUM_STAGES; i++) {
				bool gateHigh = trigExp->getInput(Marf248TriggerExpander::STAGE_ENABLE_POLY_INPUT).getPolyVoltage(i) >= GATE_THRESHOLD;
				if (gateHigh) {
					stageModifier[i] |= MOD_ENABLE;
				}
				else {
					stageModifier[i] &= ~MOD_ENABLE;
				}
				lights[STAGE_ENABLE_LIGHT + i].setBrightness(gateHigh ? 1.f : 0.f);
			}
		}

		// Stage PULSE1 Poly-Gate vom Trigger-Expander: identisches Prinzip
		// wie STAGE_SKIP_POLY_INPUT oben.
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_PULSE1_POLY_INPUT).isConnected()) {
			const float GATE_THRESHOLD = 2.f;
			for (int i = 0; i < NUM_STAGES; i++) {
				bool gateHigh = trigExp->getInput(Marf248TriggerExpander::STAGE_PULSE1_POLY_INPUT).getPolyVoltage(i) >= GATE_THRESHOLD;
				if (gateHigh) {
					stageModifier[i] |= MOD_PULSE1;
				}
				else {
					stageModifier[i] &= ~MOD_PULSE1;
				}
				lights[STAGE_PULSE1_LIGHT + i].setBrightness(gateHigh ? 1.f : 0.f);
			}
		}

		// Stage PULSE2 Poly-Gate vom Trigger-Expander: identisches Prinzip
		// wie STAGE_SKIP_POLY_INPUT oben.
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_PULSE2_POLY_INPUT).isConnected()) {
			const float GATE_THRESHOLD = 2.f;
			for (int i = 0; i < NUM_STAGES; i++) {
				bool gateHigh = trigExp->getInput(Marf248TriggerExpander::STAGE_PULSE2_POLY_INPUT).getPolyVoltage(i) >= GATE_THRESHOLD;
				if (gateHigh) {
					stageModifier[i] |= MOD_PULSE2;
				}
				else {
					stageModifier[i] &= ~MOD_PULSE2;
				}
				lights[STAGE_PULSE2_LIGHT + i].setBrightness(gateHigh ? 1.f : 0.f);
			}
		}

		// Stage SLOPE Poly-Gate vom Trigger-Expander: identisches Prinzip
		// wie STAGE_SKIP_POLY_INPUT oben. MOD_SLOPED ist ein einfaches
		// Ein/Aus-Bit (rampt vs. springt) - nicht zu verwechseln mit Shape
		// (Lin/Exp/Log-Zyklus), das bewusst ohne CV-Override bleibt.
		if (trigExp && trigExp->getInput(Marf248TriggerExpander::STAGE_SLOPE_POLY_INPUT).isConnected()) {
			const float GATE_THRESHOLD = 2.f;
			for (int i = 0; i < NUM_STAGES; i++) {
				bool gateHigh = trigExp->getInput(Marf248TriggerExpander::STAGE_SLOPE_POLY_INPUT).getPolyVoltage(i) >= GATE_THRESHOLD;
				if (gateHigh) {
					stageModifier[i] |= MOD_SLOPED;
				}
				else {
					stageModifier[i] &= ~MOD_SLOPED;
				}
				lights[STAGE_SLOPE_LIGHT + i].setBrightness(gateHigh ? 1.f : 0.f);
			}
		}

		float stageVoltage[NUM_STAGES];
		float stageTime[NUM_STAGES];
		float resolvedVoltage[NUM_STAGES];
		float resolvedTime[NUM_STAGES];
		float resolvedTimeA[NUM_STAGES];
		float resolvedTimeB[NUM_STAGES];
		float stageCurvePower[NUM_STAGES];

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
		Module* cvLeftNeighbor = getLeftExpander().module;
		Module* cvRightNeighbor = getRightExpander().module;
		Module* cvExp = nullptr;
		if (cvLeftNeighbor && cvLeftNeighbor->getModel() == modelMarf248CvExpander) {
			cvExp = cvLeftNeighbor;
		}
		else if (cvRightNeighbor && cvRightNeighbor->getModel() == modelMarf248CvExpander) {
			cvExp = cvRightNeighbor;
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
				int ch = (int) rack::clamp(stageVoltage[i] / 2.5f, 0.f, 3.f);
				resolvedVoltage[i] = extVoltage[ch];
			}
			else {
				resolvedVoltage[i] = stageVoltage[i];
			}

			// Time Source External/Internal, analog zur Voltage Source.
			// Fehlt am gewaehlten Kanal ein Patchkabel, faellt die Interval
			// Time auf den schnellsten Wert des aktuellen Zeitbereichs
			// zurueck (Manual: "the interval time defaults to the fastest
			// value of the selected range").
			if (stageModifier[i] & MOD_TIME_EXT) {
				// Wie beim Voltage-External-Modus dient die (jetzt bereits
				// normalisierte 0..1) Slider-Position selbst als Kanalwahl
				// (A-D) in 4 gleich grossen Zonen - keine Rueckrechnung ueber
				// TIME_PARAM_MIN/MAX mehr noetig, da stageTime[i] schon 0..1 ist.
				int ch = (int) rack::clamp(stageTime[i] * 4.f, 0.f, 3.f);
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
			float effectiveKnob = rack::clamp(knob + curveCvAdd + curveGlobalBias, 0.f, 1.f);

			// std::pow() ist teuer und wurde bisher bei JEDEM Sample fuer
			// alle 16 Stages neu berechnet (16x pro Sample bei 48kHz =
			// ~768000 Aufrufe/Sekunde), unabhaengig davon, ob sich der Knob
			// seit dem letzten Sample ueberhaupt bewegt hat. Jetzt: nur bei
			// tatsaechlicher Aenderung neu rechnen, sonst gecachten Wert
			// wiederverwenden (siehe cachedCurvePower/lastCurveKnob oben).
			// Der Cache-Vergleich laeuft jetzt gegen effectiveKnob statt den
			// rohen Trimpot-Wert, da sich sonst eine anliegende CV nicht
			// mehr auswirken wuerde (Cache wuerde faelschlich "unveraendert"
			// erkennen, obwohl sich effectiveKnob durch die CV laufend
			// aendert) - bei aktiver CV wird dadurch effektiv jeden Sample
			// neu gerechnet, was beabsichtigt und noetig ist.
			if (effectiveKnob != lastCurveKnob[i]) {
				float bipolar = 2.f * effectiveKnob - 1.f; // -1 links .. 0 Mitte .. +1 rechts
				float factor = CURVE_FACTOR_CENTER
					+ (CURVE_FACTOR_EXTREME - CURVE_FACTOR_CENTER) * (bipolar * bipolar);
				cachedCurvePower[i] = std::pow(SLEW_CURVE_POWER, factor * effectiveKnob);
				lastCurveKnob[i] = effectiveKnob;
			}
			stageCurvePower[i] = cachedCurvePower[i];
		}

		// Time Multiplier (Manual "TIME MULTIPLIER"): skaliert die Stage-Zeiten
		// je Section unabhaengig, nachdem Time Source External/Internal bereits
		// aufgeloest wurde (resolvedTime[] oben) - die Skalierung wirkt also
		// gleichermassen auf interne wie externe Zeitquellen. Da der Multiplier
		// pro Section gilt, aber die Stage-Zeiten selbst modulweit geteilt
		// sind, entstehen hier zwei eigene Arrays statt einem gemeinsamen.
		//
		// CV-Skalierung (offene, spaeter justierbare Annahme analog zu
		// GATE_THRESHOLD/Curve-Mapping): ein voller Attenuverter-Ausschlag
		// (+-1) bei einer CV von +-5V deckt die komplette Multiplier-Spanne
		// (0.5x-4.0x, Spanne 3.5) ab. TIME_MULT_CV_SPAN_VOLTS ist die
		// angenommene CV-Referenzamplitude, TIME_MULT_RANGE_SPAN die
		// Multiplier-Spanne.
		const float TIME_MULT_MIN = 0.5f;
		const float TIME_MULT_MAX = 4.f;
		const float TIME_MULT_RANGE_SPAN = TIME_MULT_MAX - TIME_MULT_MIN;
		const float TIME_MULT_CV_SPAN_VOLTS = 5.f;

		float cvA = inputs[TIME_MULT_CV_A_INPUT].getVoltage();
		float multiplierA = params[TIME_MULT_A_PARAM].getValue()
			+ params[TIME_MULT_ATTEN_A_PARAM].getValue() * (cvA / TIME_MULT_CV_SPAN_VOLTS) * TIME_MULT_RANGE_SPAN;
		multiplierA = rack::clamp(multiplierA, TIME_MULT_MIN, TIME_MULT_MAX);

		float cvB = inputs[TIME_MULT_CV_B_INPUT].getVoltage();
		float multiplierB = params[TIME_MULT_B_PARAM].getValue()
			+ params[TIME_MULT_ATTEN_B_PARAM].getValue() * (cvB / TIME_MULT_CV_SPAN_VOLTS) * TIME_MULT_RANGE_SPAN;
		multiplierB = rack::clamp(multiplierB, TIME_MULT_MIN, TIME_MULT_MAX);

		for (int i = 0; i < NUM_STAGES; i++) {
			resolvedTimeA[i] = resolvedTime[i] * multiplierA;
			resolvedTimeB[i] = resolvedTime[i] * multiplierB;
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
		if (addrModeATrigger.process(params[STAGE_ADDR_MODE_A_PARAM].getValue())) {
			addrExtA = !addrExtA;
		}
		if (addrContATrigger.process(params[STAGE_ADDR_CONT_A_PARAM].getValue())) {
			addrContOnA = !addrContOnA;
		}
		lights[STAGE_ADDR_MODE_A_LIGHT].setBrightness(addrExtA ? 1.f : 0.f);
		lights[STAGE_ADDR_CONT_A_LIGHT].setBrightness(addrContOnA ? 1.f : 0.f);
		int addrValueA;
		if (addrExtA) {
			float v = rack::clamp(inputs[STAGE_ADDR_EXT_A_INPUT].getVoltage(), 0.f, 10.f);
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
		afgA.applyStageAddress(addrContOnA, addrValueA, strobeA, resetA, stageModifier);

		if (addrModeBTrigger.process(params[STAGE_ADDR_MODE_B_PARAM].getValue())) {
			addrExtB = !addrExtB;
		}
		if (addrContBTrigger.process(params[STAGE_ADDR_CONT_B_PARAM].getValue())) {
			addrContOnB = !addrContOnB;
		}
		lights[STAGE_ADDR_MODE_B_LIGHT].setBrightness(addrExtB ? 1.f : 0.f);
		lights[STAGE_ADDR_CONT_B_LIGHT].setBrightness(addrContOnB ? 1.f : 0.f);
		int addrValueB;
		if (addrExtB) {
			float v = rack::clamp(inputs[STAGE_ADDR_EXT_B_INPUT].getVoltage(), 0.f, 10.f);
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
		afgB.applyStageAddress(addrContOnB, addrValueB, strobeB, resetB, stageModifier);

		float vA, tA, apA, p1A, p2A, scA, ecA, rgA;
		afgA.process(args.sampleTime, resolvedVoltage, resolvedTimeA, stageModifier, stageCurvePower,
			currentKey, currentScale,
			params[RUN_A_PARAM].getValue(),
			inputs[START_A_INPUT].isConnected(), inputs[START_A_INPUT].getVoltage(),
			inputs[STOP_A_INPUT].isConnected(), inputs[STOP_A_INPUT].getVoltage(),
			// Start/Stop-Gate vom Trigger-Expander (RUN_GATE_A_INPUT):
			// unverbunden, wenn kein Trigger-Expander andockt.
			trigExp && trigExp->getInput(Marf248TriggerExpander::RUN_GATE_A_INPUT).isConnected(),
			trigExp ? trigExp->getInput(Marf248TriggerExpander::RUN_GATE_A_INPUT).getVoltage() : 0.f,
			&vA, &tA, &apA, &p1A, &p2A, &scA, &ecA, &rgA);
		outputs[VOUT_A_OUTPUT].setVoltage(vA);
		outputs[TOUT_A_OUTPUT].setVoltage(tA);
		outputs[APOUT_A_OUTPUT].setVoltage(apA);
		outputs[P1OUT_A_OUTPUT].setVoltage(p1A);
		outputs[P2OUT_A_OUTPUT].setVoltage(p2A);
		lights[P1_ACTIVE_A_LIGHT].setBrightness(p1A > 0.f ? 1.f : 0.f);
		lights[P2_ACTIVE_A_LIGHT].setBrightness(p2A > 0.f ? 1.f : 0.f);

		float vB, tB, apB, p1B, p2B, scB, ecB, rgB;
		afgB.process(args.sampleTime, resolvedVoltage, resolvedTimeB, stageModifier, stageCurvePower,
			currentKey, currentScale,
			params[RUN_B_PARAM].getValue(),
			inputs[START_B_INPUT].isConnected(), inputs[START_B_INPUT].getVoltage(),
			inputs[STOP_B_INPUT].isConnected(), inputs[STOP_B_INPUT].getVoltage(),
			// Start/Stop-Gate vom Trigger-Expander (RUN_GATE_B_INPUT):
			// unverbunden, wenn kein Trigger-Expander andockt.
			trigExp && trigExp->getInput(Marf248TriggerExpander::RUN_GATE_B_INPUT).isConnected(),
			trigExp ? trigExp->getInput(Marf248TriggerExpander::RUN_GATE_B_INPUT).getVoltage() : 0.f,
			&vB, &tB, &apB, &p1B, &p2B, &scB, &ecB, &rgB);
		outputs[VOUT_B_OUTPUT].setVoltage(vB);
		outputs[TOUT_B_OUTPUT].setVoltage(tB);
		outputs[APOUT_B_OUTPUT].setVoltage(apB);
		outputs[P1OUT_B_OUTPUT].setVoltage(p1B);
		outputs[P2OUT_B_OUTPUT].setVoltage(p2B);
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
			Marf248CvExpander* cvExpTyped = dynamic_cast<Marf248CvExpander*>(cvExp);
			bool gapEnabled = cvExpTyped && cvExpTyped->regateGapEnabled;
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
		bool stopA = !afgA.effectiveRunning || afgA.statusStoppedByStage || addrContOnA;
		bool holdA = !stopA && afgA.statusHeld;
		lights[STOP_A_LIGHT].setBrightness(stopA ? 1.f : 0.f);
		lights[HOLD_A_LIGHT].setBrightness(holdA ? 1.f : 0.f);
		lights[RUN_A_LIGHT].setBrightness((!stopA && !holdA) ? 1.f : 0.f);

		bool stopB = !afgB.effectiveRunning || afgB.statusStoppedByStage || addrContOnB;
		bool holdB = !stopB && afgB.statusHeld;
		lights[STOP_B_LIGHT].setBrightness(stopB ? 1.f : 0.f);
		lights[HOLD_B_LIGHT].setBrightness(holdB ? 1.f : 0.f);
		lights[RUN_B_LIGHT].setBrightness((!stopB && !holdB) ? 1.f : 0.f);
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
		json_object_set_new(rootJ, "currentKey", json_integer(currentKey));
		json_object_set_new(rootJ, "currentScale", json_integer(currentScale));

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

		json_t* currentKeyJ = json_object_get(rootJ, "currentKey");
		if (currentKeyJ) {
			currentKey = json_integer_value(currentKeyJ);
		}
		json_t* currentScaleJ = json_object_get(rootJ, "currentScale");
		if (currentScaleJ) {
			currentScale = json_integer_value(currentScaleJ);
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
	if (!m) {
		return string::f("%.1f", getDisplayValue());
	}
	int idx = getTimeRangeIndex(m->stageModifier[stageIndex]);
	float seconds = TIME_RANGE_MIN[idx] + getValue() * (TIME_RANGE_MAX[idx] - TIME_RANGE_MIN[idx]);
	// TR1/TR2 bewegen sich im Millisekunden- bis niedrigen Zehntelbereich und
	// brauchen daher mehr Nachkommastellen als TR3/TR4, sonst wuerde z.B.
	// ".00 s" fuer den gesamten TR1-Bereich angezeigt.
	std::string secStr = (seconds < 1.f) ? string::f("%.3f s", seconds) : string::f("%.2f s", seconds);
	return string::f("%.1f (%s)", getDisplayValue(), secStr.c_str());
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





struct Marf248Widget : ModuleWidget {
	Marf248Widget(Marf248* module) {
		setModule(module);

		// Modul insgesamt etwas breiter (120 statt 110 HP), damit die 16
		// Stage-Spalten mehr Luft bekommen - die Knoepfe/Buttons wirkten bei
		// 22mm Rahmenbreite zu dicht gedraengt.
		const float HP = 120.f;
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

		// Geometrie-Konstanten: xStep/frameWidth fuer das 3-spaltige
		// Modifier-Raster pro Stage (Pulse1/Pulse2/Skip usw.).
		const float xStep = 29.f;
		const float frameWidthMM = 25.f;
		const float frameTopMM = 12.f;
		const float frameBottomMM = 120.f;
		const float tripleSpreadStage = 8.f;
		// Zwei Luecken statt vorher einer einzigen (colToStageGap): Controls
		// -> Jacks-Spalte, dann Jacks-Spalte -> Stage-Array. Symmetrisch fuer
		// beide Sections (Jacks-Spalte liegt jeweils naeher am Stage-Array,
		// Controls-Spalte aussen). Beide Luecken so knapp wie moeglich
		// bemessen (nur so viel Rand, dass sich die jeweiligen Rahmen NICHT
		// beruehren), damit das Modul insgesamt schmaler bleibt.
		const float gapControlsToJacks = 19.f;
		const float gapJacksToStage = 26.f;

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
		float xStart = colAJacks + gapJacksToStage;
		float colBJacks = xStart + (NUM_STAGES - 1) * xStep + gapJacksToStage;
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
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colExt, 44)), module, Marf248::EXT_B_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colExt, 66)), module, Marf248::EXT_C_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colExt, 88)), module, Marf248::EXT_D_INPUT));

		// Eigene Jacks-Spalten pro Section (neu): buendeln alle Buchsen der
		// jeweiligen Section (Time Multiplier CV, Start/Stop, Voltage/Time-
		// Out, Pulse 1/2-Out, All-Pulses-Out, Stage-Address-CV) getrennt von
		// den reinen Bedienelementen (Controls-Spalte: Run, Mode/Pendulum,
		// Stage-Address-Knob/Buttons). Liegt jeweils zwischen der Controls-
		// Spalte und dem Stage-Array - spiegelbildlich fuer A/B.
		const float jacksFrameWidthMM = 20.f;
		const float jacksFrameTopMM = 12.f;
		const float jacksFrameBottomMM = 110.f;
		float jacksColsFrame[2] = {colAJacks, colBJacks};
		for (int s = 0; s < 2; s++) {
			StageFrame* jacksFrame = new StageFrame;
			jacksFrame->box.pos = mm2px(Vec(jacksColsFrame[s] - jacksFrameWidthMM / 2.f, jacksFrameTopMM));
			jacksFrame->box.size = mm2px(Vec(jacksFrameWidthMM, jacksFrameBottomMM - jacksFrameTopMM));
			addChild(jacksFrame);
		}

		// --- Section A und B: identischer Aufbau (spiegelbildlich an derselben
		// y-Position), da die External-Inputs jetzt in einer eigenen Spalte
		// sitzen und beide FG-Spalten dadurch gleich viel Inhalt haben.
		float controlsCols[2] = {colAControls, colBControls};
		float jacksCols[2] = {colAJacks, colBJacks};
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
		int timeMultParams[2] = {Marf248::TIME_MULT_A_PARAM, Marf248::TIME_MULT_B_PARAM};
		int timeMultAttenParams[2] = {Marf248::TIME_MULT_ATTEN_A_PARAM, Marf248::TIME_MULT_ATTEN_B_PARAM};
		int timeMultCvInputs[2] = {Marf248::TIME_MULT_CV_A_INPUT, Marf248::TIME_MULT_CV_B_INPUT};

		for (int s = 0; s < 2; s++) {
			float ctrl = controlsCols[s];
			float jack = jacksCols[s];

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
			const float controlsFrameBottomMM = 116.f;
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

			HDivider* divTimeMultRun = new HDivider;
			divTimeMultRun->box.pos = mm2px(Vec(ctrl - 4.f, 35.f));
			divTimeMultRun->box.size = mm2px(Vec(8.f, 0.f));
			addChild(divTimeMultRun);

			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 41)), module, runParams[s]));
			addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(ctrl, 41)), module, runLights[s]));

			HDivider* divRunMode = new HDivider;
			divRunMode->box.pos = mm2px(Vec(ctrl - 4.f, 49.f));
			divRunMode->box.size = mm2px(Vec(8.f, 0.f));
			addChild(divRunMode);

			// Laufrichtung: zwei getrennte Klick-Buttons, jetzt untereinander
			// statt nebeneinander. Mode kehrt die Basisrichtung um (LED an =
			// Reverse), Pendulum schaltet zusaetzlich das Bouncing dazu
			// (LED an = aktiv).
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 55)), module, modeParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 55)), module, modeLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 64)), module, pendulumParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 64)), module, pendulumLights[s]));

			HDivider* divModeAddr = new HDivider;
			divModeAddr->box.pos = mm2px(Vec(ctrl - 4.f, 69.f));
			divModeAddr->box.size = mm2px(Vec(8.f, 0.f));
			addChild(divModeAddr);

			addParam(createParamCentered<Trimpot>(mm2px(Vec(ctrl, 75)), module, addrKnobParams[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 84)), module, addrModeParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 84)), module, addrModeLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 93)), module, addrContParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 93)), module, addrContLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 102)), module, addrStrobeParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 102)), module, addrStrobeLights[s]));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(ctrl, 111)), module, addrResetParams[s]));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(ctrl, 111)), module, addrResetLights[s]));

			// --- Jacks-Spalte, von oben nach unten: Start/Stop-Trigger (eng,
			// 8mm), Time-Multiplier-CV-Eingang, Voltage/Time-Out (eng, 8mm),
			// Pulse 1/2-Out, All-Pulses-Out (jetzt ebenfalls eng an Pulse 2,
			// 8mm statt vorher 13mm), Stage-Address-CV-In. Inhalt beginnt
			// erst bei 20mm (statt 16mm) - bei einer Oberkante von 12mm
			// (jetzt einheitlich mit den anderen Spalten) ragte die Start-
			// Buchse bei 16mm sonst leicht ueber den Rahmenrand hinaus.
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 20)), module, startInputs[s]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 28)), module, stopInputs[s]));

			HDivider* divStartTimeMult = new HDivider;
			divStartTimeMult->box.pos = mm2px(Vec(jack - 9.f, 33.f));
			divStartTimeMult->box.size = mm2px(Vec(18.f, 0.f));
			addChild(divStartTimeMult);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 41)), module, timeMultCvInputs[s]));

			HDivider* divTimeMultOut = new HDivider;
			divTimeMultOut->box.pos = mm2px(Vec(jack - 9.f, 46.f));
			divTimeMultOut->box.size = mm2px(Vec(18.f, 0.f));
			addChild(divTimeMultOut);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack, 52)), module, voutOutputs[s]));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack, 60)), module, toutOutputs[s]));

			HDivider* divOutPulse = new HDivider;
			divOutPulse->box.pos = mm2px(Vec(jack - 9.f, 65.f));
			divOutPulse->box.size = mm2px(Vec(18.f, 0.f));
			addChild(divOutPulse);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack, 73)), module, p1Outputs[s]));
			addChild(createLightCentered<PulseRingLight>(mm2px(Vec(jack, 73)), module, p1ActiveLights[s]));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack, 81)), module, p2Outputs[s]));
			addChild(createLightCentered<PulseRingLight>(mm2px(Vec(jack, 81)), module, p2ActiveLights[s]));

			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack, 89)), module, apoutOutputs[s]));

			HDivider* divApoutAddr = new HDivider;
			divApoutAddr->box.pos = mm2px(Vec(jack - 9.f, 94.f));
			divApoutAddr->box.size = mm2px(Vec(18.f, 0.f));
			addChild(divApoutAddr);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack, 102)), module, addrExtInputs[s]));
		}

		// --- Stage-Array (Mitte) ---
		for (int i = 0; i < NUM_STAGES; i++) {
			float x = xStart + i * xStep;

			// Dekorativer Rahmen um die gesamte Stage-Spalte.
			StageFrame* frame = new StageFrame;
			frame->box.pos = mm2px(Vec(x - frameWidthMM / 2.f, frameTopMM));
			frame->box.size = mm2px(Vec(frameWidthMM, frameBottomMM - frameTopMM));
			addChild(frame);

			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x, 18)), module, Marf248::STAGE_VOLTAGE_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x, 30)), module, Marf248::STAGE_TIME_PARAM + i));
			addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(x - 1.5f, 40)), module, Marf248::STAGE_LIGHT_A + i));
			addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(x + 1.5f, 40)), module, Marf248::STAGE_LIGHT_B + i));

			// Modifier-Buttons dreispaltig, horizontale Aufteilung:
			//   Reihe 1: Pulse 1 | Pulse 2 | Skip
			//   Reihe 2: Stop    | Sustain | Enable
			//   Reihe 3: First   | Last    | Slope
			//   Reihe 4: Quantize| Volt-Source (Ext) | Time-Source (Ext)
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x - tripleSpreadStage, 48)), module, Marf248::STAGE_PULSE1_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x - tripleSpreadStage, 48)), module, Marf248::STAGE_PULSE1_LIGHT + i));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x, 48)), module, Marf248::STAGE_PULSE2_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x, 48)), module, Marf248::STAGE_PULSE2_LIGHT + i));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + tripleSpreadStage, 48)), module, Marf248::STAGE_SKIP_PARAM + i));
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + tripleSpreadStage, 48)), module, Marf248::STAGE_SKIP_LIGHT + i));

			addParam(createParamCentered<LEDButton>(mm2px(Vec(x - tripleSpreadStage, 56)), module, Marf248::STAGE_STOP_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x - tripleSpreadStage, 56)), module, Marf248::STAGE_STOP_LIGHT + i));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x, 56)), module, Marf248::STAGE_SUSTAIN_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x, 56)), module, Marf248::STAGE_SUSTAIN_LIGHT + i));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + tripleSpreadStage, 56)), module, Marf248::STAGE_ENABLE_PARAM + i));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(x + tripleSpreadStage, 56)), module, Marf248::STAGE_ENABLE_LIGHT + i));

			addParam(createParamCentered<LEDButton>(mm2px(Vec(x - tripleSpreadStage, 64)), module, Marf248::STAGE_FIRST_PARAM + i));
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x - tripleSpreadStage, 64)), module, Marf248::STAGE_FIRST_LIGHT + i));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x, 64)), module, Marf248::STAGE_LAST_PARAM + i));
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x, 64)), module, Marf248::STAGE_LAST_LIGHT + i));
			// Slope-Button (MOD_SLOPED): war im Code seit je vollstaendig
			// implementiert (Param, Trigger, Light), hatte aber nie einen
			// addParam()/addChild()-Aufruf im Widget - der Regler war also
			// unerreichbar und die Stage blieb immer im Default Stepped
			// haengen. Fix: nutzt die bisher bewusst freigelassene dritte
			// Position dieser Reihe (gleiches Muster wie Pulse1/2/Skip und
			// Stop/Sustain/Enable).
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + tripleSpreadStage, 64)), module, Marf248::STAGE_SLOPE_PARAM + i));
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x + tripleSpreadStage, 64)), module, Marf248::STAGE_SLOPE_LIGHT + i));

			addParam(createParamCentered<LEDButton>(mm2px(Vec(x - tripleSpreadStage, 72)), module, Marf248::STAGE_QUANT_PARAM + i));
			addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x - tripleSpreadStage, 72)), module, Marf248::STAGE_QUANT_LIGHT + i));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x, 72)), module, Marf248::STAGE_VOLTEXT_PARAM + i));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x, 72)), module, Marf248::STAGE_VOLTEXT_LIGHT + i));
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + tripleSpreadStage, 72)), module, Marf248::STAGE_TIMEEXT_PARAM + i));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x + tripleSpreadStage, 72)), module, Marf248::STAGE_TIMEEXT_LIGHT + i));

			// Shape + Curve jetzt in einer gemeinsamen Reihe statt zwei
			// getrennten (Platzgewinn fuer die vergroesserte Time-Range-
			// Anzeige weiter unten, siehe Kinovice-Feedback): Shape nach
			// rechts, Curve-Trimmer links daneben.
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + 6.f, 82)), module, Marf248::STAGE_SHAPE_PARAM + i));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x + 2.f, 82)), module, Marf248::STAGE_SHAPE_EXP_LIGHT + i));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x + 10.f, 82)), module, Marf248::STAGE_SHAPE_LOG_LIGHT + i));

			// Curve-Trimmer: Mitte = Referenzkruemmung, links linearer, rechts kurviger.
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x - 6.f, 82)), module, Marf248::STAGE_CURVE_PARAM + i));

			// Normalize nach rechts verschoben (gleiche Reihe wie Time
			// Range, die den frei gewordenen linken/mittleren Platz bekommt).
			// Y=92 statt 90: die vergroesserte Time-Range-Box (9mm) braucht
			// etwas mehr Abstand zur Shape/Curve-Reihe darueber als ein
			// normaler LEDButton, sonst wuerden sich beide Reihen beruehren.
			addParam(createParamCentered<LEDButton>(mm2px(Vec(x + 6.f, 92)), module, Marf248::STAGE_NORMALIZE_PARAM + i));
			addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(x + 6.f, 92)), module, Marf248::STAGE_NORMALIZE_LIGHT + i));

			// Time Range (TR1-4): jetzt deutlich groesser als ein normaler
			// LEDButton, damit die Ziffer 1-4 lesbar bleibt (siehe Kinovice-
			// Feedback "Ziffer viel zu klein"). TrangeButton zeichnet sich
			// komplett selbst (kein SVG-Frame), daher ist eine freie
			// box.size-Wahl hier unproblematisch - createParamCentered()
			// zentriert zunaechst mit der ererbten LEDButton-Default-Groesse,
			// danach wird box.size auf 9x9mm vergroessert und um denselben
			// Mittelpunkt neu zentriert.
			TrangeButton* trangeBtn = createParamCentered<TrangeButton>(mm2px(Vec(x - 4.f, 92)), module, Marf248::STAGE_TRANGE_PARAM + i);
			Vec trangeCenter = trangeBtn->box.getCenter();
			trangeBtn->box.size = mm2px(Vec(9.f, 9.f));
			trangeBtn->box.pos = trangeCenter.minus(trangeBtn->box.size.div(2));
			addParam(trangeBtn);

			// Range: eigene Reihe unten, gleiche Groesse/Optik wie die
			// Voltage/Time-CV-Knoepfe, mit eingeblendeter Zustands-Abkuerzung
			// (siehe RangeKnob).
			addParam(createParamCentered<RangeKnob>(mm2px(Vec(x, 110)), module, Marf248::STAGE_RANGE_PARAM + i));
		}

		// Global Clear: einzelner, geteilter Taster mittig ueber dem
		// Stage-Array (kein Bezug zu einer einzelnen Section, siehe
		// Kommentar bei CLEAR_PARAM). Zentriert ueber die 16 Stages selbst
		// (xStart..letzte Stage), nicht ueber die Controls-Spalten, da diese
		// jetzt durch die neuen Jacks-Spalten weiter aussen liegen.
		float colClear = xStart + (NUM_STAGES - 1) * xStep / 2.f;
		addParam(createParamCentered<LEDButton>(mm2px(Vec(colClear, 8)), module, Marf248::CLEAR_PARAM));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(colClear, 8)), module, Marf248::CLEAR_LIGHT));

		// Reset Values: direkt neben Global Clear, gleiche Reihe, andere
		// Lichtfarbe (Gelb statt Rot) zur klaren optischen Unterscheidung.
		addParam(createParamCentered<LEDButton>(mm2px(Vec(colClear + 12.f, 8)), module, Marf248::RESET_VALUES_PARAM));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(colClear + 12.f, 8)), module, Marf248::RESET_VALUES_LIGHT));

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
		for (int p = 0; p < 12; p++) {
			float y = presetStartY + p * presetStepY;
			addParam(createParamCentered<LEDButton>(mm2px(Vec(colPresets, y)), module, Marf248::PRESET_PARAM + p));
			addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(colPresets, y)), module, Marf248::PRESET_LIGHT + p));
		}
	}
};


Model* modelMarf248 = createModel<Marf248, Marf248Widget>("Marf248");
