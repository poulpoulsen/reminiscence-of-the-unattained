#pragma once
#include "plugin.hpp"

// modelMarf248 und modelMarf248TriggerExpander sind jetzt final in
// plugin.hpp deklariert (siehe dort) - die frueheren lokalen extern-
// Deklarationen an dieser Stelle sind damit ueberfluessig geworden.

// Marf248TriggerExpander: reiner Jack-Traeger ohne eigene DSP-Logik. Die
// eigentliche Verarbeitung (Clear/Reset Values ausloesen, Preset laden,
// Scale/Key setzen, Start-/End-of-Cycle ausgeben) passiert komplett in
// Marf248::process() - dieses Modul liest/schreibt nur seine eigenen
// Inputs/Outputs, wenn Marf248 links oder rechts andockt (siehe VCV
// Rack "Expander"-Mechanismus, offizielle Doku: Plugin API Guide #Expanders).
//
// Bewusst OHNE Message-Passing/Doppelpuffer: da dieses Modul keine eigene
// unabhaengige Verarbeitung hat, kann Marf248 seine Ports direkt lesen/
// schreiben, ohne Race-Conditions befuerchten zu muessen (siehe Rack-Doku:
// "you can implement its entire DSP algorithm in the [primary] module").
struct Marf248TriggerExpander : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		CLEAR_INPUT,
		RESET_VALUES_INPUT,
		// Poly-Umbau (ersetzt die vorherigen 12 Einzelbuchsen ENUMS
		// (PRESET_LOAD_INPUT, 12) - Breaking Change fuer bestehende Patches,
		// die auf die alten Einzel-Inputs gepatcht hatten): ein einzelner
		// polyphoner Eingang, Kanal p (0-11) laedt Preset p+1 sofort, ohne
		// den Arm-Mechanismus (Load-Taster) des Hauptpanels - siehe
		// Marf248::process(). Unbenutzte/unpatchte Kanaele liefern 0V
		// (getPolyVoltage() faellt automatisch darauf zurueck), loesen also
		// keinen Trigger aus.
		PRESET_LOAD_POLY_INPUT,
		SCALE_MAJOR_INPUT,
		SCALE_MINOR_INPUT,
		SCALE_CHROMATIC_INPUT,
		// Key: kontinuierliche CV statt Trigger (0-10V -> C..B), analog zur
		// Stage-Address-CV im Hauptmodul - Spannungshoehe bestimmt den Wert
		// direkt, kein Arm/Commit noetig (siehe Marf248::process()).
		KEY_CV_INPUT,
		// SEXT (Buchla-Book-Begriff fuer externe Strobe-Pulse): loest exakt
		// dieselbe Aktion aus wie der jeweilige Panel-Strobe-Taster
		// (STAGE_ADDR_STROBE_A/B_PARAM in Marf248) - inklusive der
		// bestehenden Continuous-Sperre, da Afg::applyStageAddress() im
		// Continuous-Zweig vorzeitig returned und strobePressed dort
		// ignoriert (siehe Marf248.hpp). Getrennt pro Section, analog zu
		// den getrennten Panel-Tastern.
		STROBE_A_INPUT,
		STROBE_B_INPUT,
		// Start/Stop-Gate pro Section: reine Pegel-Logik (Gate high =
		// running, Gate low = kein Einfluss mehr), unabhaengig vom
		// Start/Stop-Taster (RUN_A/B_PARAM) des Hauptmoduls - ODER-
		// verknuepft (siehe Afg::process() in Marf248.hpp: effectiveRunning
		// = running || runGateHigh). Der Taster-Zustand selbst bleibt dabei
		// unveraendert erhalten: sobald das Gate wieder auf Low faellt,
		// greift wieder genau der Zustand, den der Taster zuletzt
		// eingestellt hat. Schmitt-Trigger-Hysterese (0.1V/1V) gegen
		// Gate-Jitter am Umschaltpunkt, siehe Afg::runGateTrigger.
		RUN_GATE_A_INPUT,
		RUN_GATE_B_INPUT,
		// Stage SKIP (Poly, Kanal i = Stage i+1): Pegel-Override statt Trigger,
		// analog zu RUN_GATE_A/B_INPUT - Kanal-Spannung >= GATE_THRESHOLD (2V,
		// siehe Marf248::process()) erzwingt MOD_SKIP fuer die betreffende
		// Stage, unterhalb loescht es das Bit. Wirkt nur, solange die Buchse
		// gepatcht ist (isConnected()); dann hat das CV Vorrang und
		// ueberschreibt einen manuellen Klick auf STAGE_SKIP_PARAM in
		// Marf248::process() noch im selben Frame. Ohne Kabel bleibt exakt
		// der zuletzt per Klick gesetzte Zustand erhalten - identisches
		// Rueckfall-Verhalten wie bei RUN_GATE_A/B_INPUT.
		STAGE_SKIP_POLY_INPUT,
		// Stage FIRST/LAST/STOP/SUSTAIN/ENABLE/PULSE1/PULSE2 (je Poly, Kanal i
		// = Stage i+1): identisches Pegel-Override-Prinzip wie
		// STAGE_SKIP_POLY_INPUT oben - Kanal-Spannung >= GATE_THRESHOLD (2V)
		// erzwingt das jeweilige Bit fuer die betreffende Stage, unterhalb
		// loescht es das Bit. Nur wirksam, solange die Buchse gepatcht ist;
		// dann hat das CV Vorrang und ueberschreibt einen manuellen Klick auf
		// den jeweiligen STAGE_..._PARAM in Marf248::process() noch im
		// selben Frame. Ohne Kabel bleibt exakt der zuletzt per Klick
		// gesetzte Zustand erhalten. (SLOPE folgt weiter unten separat, siehe
		// STAGE_SLOPE_POLY_INPUT - Shape/Lin-Exp-Log bleibt als einziger
		// echter Mehrfach-Zyklus bewusst ohne CV-Override.)
		STAGE_FIRST_POLY_INPUT,
		STAGE_LAST_POLY_INPUT,
		STAGE_STOP_POLY_INPUT,
		STAGE_SUSTAIN_POLY_INPUT,
		STAGE_ENABLE_POLY_INPUT,
		STAGE_PULSE1_POLY_INPUT,
		STAGE_PULSE2_POLY_INPUT,
		// Stage SLOPE (Poly, Kanal i = Stage i+1): identisches Pegel-Override-
		// Prinzip wie oben. MOD_SLOPED ist entgegen einer frueheren Annahme
		// ein einfaches Ein/Aus-Bit (steuert nur, ob die Stage rampt oder
		// springt) - NICHT zu verwechseln mit dem Shape-Button (Lin/Exp/Log-
		// Zyklus, STAGE_SHAPE_PARAM), der bewusst weiterhin ohne CV-Override
		// bleibt, da er tatsaechlich ein 3-Zustands-Zyklus ist.
		STAGE_SLOPE_POLY_INPUT,
		// Advance A/B (Clock-quantisiertes Advance, pro Section): reiner
		// Trigger-Eingang (Flanke, keine Pegel-Logik wie RUN_GATE_A/B_INPUT).
		// Sobald gepatcht, ersetzt er fuer die betreffende Section komplett
		// den automatischen Zeitablauf UND das Start/Stop-Konzept - wirkt
		// also unabhaengig von RUN_A/B_PARAM und RUN_GATE_A/B_INPUT, analog
		// zu Continuous (siehe Afg::process() in Marf248.hpp, Zweig
		// "advanceModeActive"). Die Stage-Zeit laeuft weiterhin (fuer Slope/
		// TOUT), verharrt aber am Ende bei "fertig", bis der naechste
		// Trigger eintrifft; kommt der Trigger VOR Ablauf der Stage-Zeit,
		// wird sofort gewechselt (Trigger hat immer Vorrang vor der Zeit).
		// STOP/SUSTAIN/ENABLE haben weiterhin Vorrang: ein Trigger waehrend
		// einer Haltung wird ignoriert (keine Warteschlange, kein
		// nachtraegliches Nachholen). Gegenseitig ausschliessend zu
		// Continuous (bei aktivem Continuous bleibt dieser Zweig inaktiv,
		// da Continuous die Stage ohnehin direkt adressiert).
		ADVANCE_A_INPUT,
		ADVANCE_B_INPUT,
		// Adress-Speicher Store/Recall (je Poly, Kanal i = Slot i): reines
		// Sprung-Lesezeichen-System fuer die Stage-Address-Funktion, siehe
		// Marf248::AddrMemorySlot und Marf248::process(). Store schreibt bei
		// steigender Flanke sofort die aktuell adressierte Stage der
		// jeweiligen Section in Slot i (kein Arm-Mechanismus, gleiches
		// Prinzip wie PRESET_LOAD_POLY_INPUT oben). Recall springt bei
		// steigender Flanke auf die in Slot i gespeicherte Stage, aber nur
		// wenn der Slot zuvor beschrieben wurde - ein nie beschriebener Slot
		// bleibt wirkungslos. Getrennt pro Section (A/B), da Stage Address
		// modulweit ohnehin pro Section unabhaengig ist. Ans Ende des Enums
		// angehaengt statt zwischen die bestehenden Stage-Modifier-Inputs
		// einsortiert, um deren Indizes (und damit bestehende Patch-Kabel)
		// nicht zu verschieben - gleiches Prinzip wie bei ADVANCE_A/B_INPUT
		// oben.
		ADDR_STORE_A_POLY_INPUT,
		ADDR_STORE_B_POLY_INPUT,
		ADDR_RECALL_A_POLY_INPUT,
		ADDR_RECALL_B_POLY_INPUT,
		// Stage MUTE (Poly, Kanal i = Stage i+1): identisches Pegel-Override-
		// Prinzip wie STAGE_PULSE1/2_POLY_INPUT oben, aber setzt MOD_MUTE
		// statt MOD_PULSE1/2 - unterdrueckt die P1/P2-Trigger-Erzeugung der
		// betreffenden Stage komplett (Voltage/Slew bleibt unbeeinflusst,
		// siehe Marf248.hpp), OHNE die Pulse1/2-Programmierung selbst zu
		// veraendern. Kanal-Spannung >= GATE_THRESHOLD (2V) = gemutet,
		// darunter = normal. Nur wirksam, solange die Buchse gepatcht ist
		// (isConnected()); ohne Kabel bleibt der zuletzt gesetzte Zustand
		// erhalten (gleiches Rueckfall-Verhalten wie alle anderen Stage-
		// Modifier-Poly-Inputs). Bewusst ans Enum-Ende angehaengt statt
		// zwischen die Pulse1/2/Slope-Gruppe einsortiert, um deren Indizes
		// nicht zu verschieben. Ein Panel-Button/Light auf dem Hauptmodul
		// folgt erst mit dem separat geplanten Panel-Relayout - dieser
		// Eingang ist bis dahin die einzige Bedienmoeglichkeit fuer Mute.
		STAGE_MUTE_POLY_INPUT,
		// VOLT_EXT/TIME_EXT Kanalwahl per CV (je Poly, Kanal i = Stage i+1):
		// ergaenzt das bestehende MOD_VOLT_EXT/MOD_TIME_EXT-Bit-Override-
		// Prinzip (siehe STAGE_SKIP_POLY_INPUT-Kommentar oben) um eine
		// gleichzeitige Kanalwahl (A-D), statt wie bisher nur ein einfaches
		// Ein/Aus-Gate zu sein. 5 Zonen a 2V im 0-10V-Bereich: 0-2V = Intern
		// (Bit wird geloescht, kein Kanal ausgewaehlt), 2-4V = Kanal A,
		// 4-6V = B, 6-8V = C, 8-10V = D (Bit wird gesetzt UND der jeweilige
		// Kanal erzwungen). Ueberschreibt bei gepatchtem Kabel sowohl den
		// Panel-Button (STAGE_VOLTEXT_PARAM/STAGE_TIMEEXT_PARAM) als auch die
		// bisherige Slider-Kanalwahl-Doppelbelegung (Reglerposition = Kanal,
		// siehe MOD_VOLT_EXT-Kommentar in Marf248.hpp) - ohne Kabel bleibt
		// das bisherige Verhalten (Button-Klick + Slider-Position als
		// Kanalwahl) komplett unveraendert, identisches isConnected()-Guard-
		// Prinzip wie alle anderen Stage-Modifier-Poly-Overrides. Nicht
		// angesteuerte Poly-Kanaele liefern automatisch 0V = Intern-Zone
		// (sicherer Default). Getrennte Eingaenge statt einem gemeinsamen,
		// da Voltage- und Time-Kanalwahl unabhaengig voneinander sein
		// koennen (z.B. Voltage extern von Quelle A, Time weiterhin
		// intern). Bewusst NICHT fuer Shape (Lin/Exp/Log-Zyklus) - das
		// bleibt ein echter 3-Zustands-Zyklus ohne CV-Override, siehe
		// STAGE_SLOPE_POLY_INPUT-Kommentar oben.
		STAGE_VOLTEXT_CH_POLY_INPUT,
		STAGE_TIMEEXT_CH_POLY_INPUT,
		// Stage RANGE Kanalwahl per CV (Poly, Kanal i = Stage i+1): Override
		// fuer den 7-Positionen-Range-Drehschalter (F/H/L0/L2/L4/L6/L8,
		// siehe StageRangeQuantity/STAGE_RANGE_PARAM in Marf248.hpp/.cpp) -
		// analog zum VOLT_EXT/TIME_EXT-Kanalwahl-Prinzip oben, aber mit 7
		// statt 5 Zonen (kein "Intern/Aus"-Sonderfall, jede Zone entspricht
		// direkt einer gueltigen Range-Position). 7 gleich grosse Zonen a
		// 10V/7 ~= 1.4286V im 0-10V-Bereich: Zone 0 = Full, 1 = Half,
		// 2..6 = Limited +0/+2/+4/+6/+8V (siehe RANGE_ZONE_THRESHOLDS/
		// resolveRangeZone() in Marf248.hpp). Ueberschreibt bei gepatchtem
		// Kabel die Panel-Reglerposition (inkl. Pickup) komplett - reines
		// Bit-Override wie alle anderen Stage-Modifier-Poly-Eingaenge,
		// isConnected()-Guard, ohne Kabel bleibt der Regler unveraendert
		// massgeblich. Nicht angesteuerte Poly-Kanaele liefern automatisch
		// 0V = Zone 0 = Full (sicherer Default).
		STAGE_RANGE_POLY_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		// Feuert, wenn die jeweilige Section (A/B) eine MOD_FIRST- bzw.
		// MOD_LAST-markierte Stage erreicht - unabhaengig von Cycle/
		// Pendulum-Richtung (kontinuierliche Flankenerkennung, siehe
		// Afg::process() in Marf248.hpp).
		START_CYCLE_A_OUTPUT,
		END_CYCLE_A_OUTPUT,
		START_CYCLE_B_OUTPUT,
		END_CYCLE_B_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		// Zeigt an, ob gerade ein Marf248 links oder rechts andockt. Ohne
		// Verbindung werden alle Outputs jeden Frame auf 0V gehalten (siehe
		// process()), damit beim Entfernen/Wegschieben des Hauptmoduls keine
		// haengengebliebene Spannung auf den Ausgaengen zurueckbleibt.
		LINK_LIGHT,
		LIGHTS_LEN
	};

	Marf248TriggerExpander() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configInput(CLEAR_INPUT, "Global Clear trigger");
		configInput(RESET_VALUES_INPUT, "Reset Values trigger");
		configInput(PRESET_LOAD_POLY_INPUT, "Load preset trigger (poly, ch. 1-12 = preset 1-12)");
		configInput(SCALE_MAJOR_INPUT, "Select Major scale trigger");
		configInput(SCALE_MINOR_INPUT, "Select Minor scale trigger");
		configInput(SCALE_CHROMATIC_INPUT, "Select Chromatic scale trigger");
		configInput(KEY_CV_INPUT, "Key select CV (0-10V = C..B)");
		configInput(STROBE_A_INPUT, "Section A external Strobe (SEXT)");
		configInput(STROBE_B_INPUT, "Section B external Strobe (SEXT)");
		configInput(RUN_GATE_A_INPUT, "Section A Start/Stop gate (high = run, OR'd with panel button)");
		configInput(RUN_GATE_B_INPUT, "Section B Start/Stop gate (high = run, OR'd with panel button)");
		configInput(STAGE_SKIP_POLY_INPUT, "Stage SKIP gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_FIRST_POLY_INPUT, "Stage FIRST gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_LAST_POLY_INPUT, "Stage LAST gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_STOP_POLY_INPUT, "Stage STOP gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_SUSTAIN_POLY_INPUT, "Stage SUSTAIN gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_ENABLE_POLY_INPUT, "Stage ENABLE gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_PULSE1_POLY_INPUT, "Stage PULSE1 gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_PULSE2_POLY_INPUT, "Stage PULSE2 gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(STAGE_SLOPE_POLY_INPUT, "Stage SLOPE gate (poly, ch. 1-16 = stage 1-16, high overrides panel button)");
		configInput(ADVANCE_A_INPUT, "Section A Advance trigger (clock-quantized, overrides Start/Stop while connected)");
		configInput(ADVANCE_B_INPUT, "Section B Advance trigger (clock-quantized, overrides Start/Stop while connected)");
		configInput(ADDR_STORE_A_POLY_INPUT, "Section A Address Memory Store (poly, ch. 1-16 = slot 1-16, captures current stage)");
		configInput(ADDR_STORE_B_POLY_INPUT, "Section B Address Memory Store (poly, ch. 1-16 = slot 1-16, captures current stage)");
		configInput(ADDR_RECALL_A_POLY_INPUT, "Section A Address Memory Recall (poly, ch. 1-16 = slot 1-16, jumps to stored stage)");
		configInput(ADDR_RECALL_B_POLY_INPUT, "Section B Address Memory Recall (poly, ch. 1-16 = slot 1-16, jumps to stored stage)");
		configInput(STAGE_MUTE_POLY_INPUT, "Stage MUTE gate (poly, ch. 1-16 = stage 1-16, high suppresses Pulse1/2 output without changing their programming)");
		configInput(STAGE_VOLTEXT_CH_POLY_INPUT, "Stage Voltage External channel select CV (poly, ch. 1-16 = stage 1-16, 0-2V=Internal, 2-4V=A, 4-6V=B, 6-8V=C, 8-10V=D)");
		configInput(STAGE_TIMEEXT_CH_POLY_INPUT, "Stage Time External channel select CV (poly, ch. 1-16 = stage 1-16, 0-2V=Internal, 2-4V=A, 4-6V=B, 6-8V=C, 8-10V=D)");
		configInput(STAGE_RANGE_POLY_INPUT, "Stage Range CV (poly, ch. 1-16 = stage 1-16, 7 zones of ~1.43V: Full/Half/L0/L2/L4/L6/L8, overrides panel knob)");

		configOutput(START_CYCLE_A_OUTPUT, "Section A Start of Cycle trigger");
		configOutput(END_CYCLE_A_OUTPUT, "Section A End of Cycle trigger");
		configOutput(START_CYCLE_B_OUTPUT, "Section B Start of Cycle trigger");
		configOutput(END_CYCLE_B_OUTPUT, "Section B End of Cycle trigger");
	}

	// Prueft jeden Frame selbststaendig, ob (noch) ein Marf248 unmittelbar
	// links oder rechts andockt. Ohne Verbindung werden Outputs/Light auf 0
	// gehalten - Marf248 selbst schreibt die eigentlichen Werte nur, wenn es
	// diesen Expander erkennt (siehe Marf248::process()), daher muss dieses
	// Modul sein eigenes "Aufraeumen" beim Verschieben/Entfernen des
	// Hauptmoduls uebernehmen (siehe Plugin API Guide #Expanders).
	void process(const ProcessArgs& args) override {
		Module* left = getLeftExpander().module;
		Module* right = getRightExpander().module;
		bool linked = (left && left->getModel() == modelMarf248)
			|| (right && right->getModel() == modelMarf248);

		if (!linked) {
			outputs[START_CYCLE_A_OUTPUT].setVoltage(0.f);
			outputs[END_CYCLE_A_OUTPUT].setVoltage(0.f);
			outputs[START_CYCLE_B_OUTPUT].setVoltage(0.f);
			outputs[END_CYCLE_B_OUTPUT].setVoltage(0.f);
		}
		lights[LINK_LIGHT].setBrightness(linked ? 1.f : 0.f);
	}
};
