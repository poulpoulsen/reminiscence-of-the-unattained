# Kinovice Marf248 – Technische Dokumentation

VCV-Rack-C++-Portierung des Buchla/Tiptop 248t MARF (Multiple Arbitrary
Function Generator). Diese Dokumentation beschreibt das Hauptmodul
**Marf248** sowie die beiden Erweiterungsmodule **Marf248TriggerExpander**
(24HP) und **Marf248CvExpander** (12HP): jedes Bedienelement, jeden
Ein-/Ausgang, die interne Signalverarbeitung, die wichtigsten C++-Funktionen
und die bekannten Fallstricke im Zusammenspiel der Funktionen.

Referenzen: Tiptop Audio 248t Manual, "MARF Book" (Buchla 248-1602
Dokumentation, maxl0rd), Suzanne Ciani Cookbook, V2.66 Firmware-Changelog.

Stand: alle in dieser Dokumentation beschriebenen Features wurden von
Kinovice live im laufenden Rack getestet und bestätigt (u.a. Adress-Speicher
Store/Recall, 4-Kanal-A–D-Kanalwahl inkl. Zonengrenzen und Fallback-
Verhalten, Backward-Kompatibilität bestehender Mono-Patches, V/Oct-
Tempo-Steuerung inkl. Rechtsklick-Zahleneingabe, Stage Mute inkl.
APOUT-Unterdrückung).

---

<a id="toc"></a>
## Inhaltsverzeichnis

*Jeder Eintrag hat zwei Links: den ersten (Standard-Markdown-Anker) nutzt du in GitHub, Codeberg, VSCodium-Preview & den meisten anderen Viewern; "Obsidian" ist der Wikilink speziell für Obsidian. In Obsidian selbst kannst du auch den ersten Link nutzen - er wird dort nur nicht immer zuverlässig aufgelöst, daher die zweite Variante als Ergänzung.*

1. [Architektur-Überblick](#sec1) · [[#1. Architektur-Überblick|Obsidian]]
2. [Grundbegriffe: Stage, Section, Modifier-Bits](#sec2) · [[#2. Grundbegriffe: Stage, Section, Modifier-Bits|Obsidian]]
3. [Marf248 – Hauptmodul](#sec3) · [[#3. Marf248 – Hauptmodul|Obsidian]]
   - [Panel-Layout](#sec3-1) · [[#3.1 Panel-Layout|Obsidian]]
   - [Bedienelemente pro Stage](#sec3-2) · [[#3.2 Bedienelemente pro Stage|Obsidian]]
   - [Bedienelemente pro Section (A/B)](#sec3-3) · [[#3.3 Bedienelemente pro Section (A/B)|Obsidian]]
   - [Modulweit geteilte Elemente](#sec3-4) · [[#3.4 Modulweit geteilte Elemente|Obsidian]]
   - [Ein-/Ausgänge im Überblick](#sec3-5) · [[#3.5 Ein-/Ausgänge im Überblick|Obsidian]]
4. [Interne Signalverarbeitung](#sec4) · [[#4. Interne Signalverarbeitung|Obsidian]]
   - [Der Afg-Kern (Ablaufsteuerung einer Section)](#sec4-1) · [[#4.1 Der Afg-Kern (Ablaufsteuerung einer Section)|Obsidian]]
   - [Stage-Advance-Logik: Forward / Reverse / Pendulum / Skip](#sec4-2) · [[#4.2 Stage-Advance-Logik: Forward / Reverse / Pendulum / Skip|Obsidian]]
   - [Stage Address: Reset / Continuous / Strobe / Kanalwahl](#sec4-3) · [[#4.3 Stage Address: Reset / Continuous / Strobe / Kanalwahl|Obsidian]]
   - [Voltage-Signalpfad: Range, Quantize, Slew](#sec4-4) · [[#4.4 Voltage-Signalpfad: Range, Quantize, Slew|Obsidian]]
   - [Time-Signalpfad: Time Range, External Time, Time-CV, Time Multiplier](#sec4-5) · [[#4.5 Time-Signalpfad: Time Range, External Time, Time-CV, Time Multiplier|Obsidian]]
   - [Sustain / Enable / Stop](#sec4-6) · [[#4.6 Sustain / Enable / Stop|Obsidian]]
   - [Pulse-Ausgänge: APOUT, P1/P2OUT, Start-/End-of-Cycle](#sec4-7) · [[#4.7 Pulse-Ausgänge: APOUT, P1/P2OUT, Start-/End-of-Cycle|Obsidian]]
   - [Preset-System (Pickup/Catch)](#sec4-8) · [[#4.8 Preset-System (Pickup/Catch)|Obsidian]]
   - [Adress-Speicher (Address Memory)](#sec4-9) · [[#4.9 Adress-Speicher (Address Memory)|Obsidian]]
   - [Clock-quantisiertes Advance](#sec4-10) · [[#4.10 Clock-quantisiertes Advance|Obsidian]]
   - [Scale/Key-Quantisierung](#sec4-11) · [[#4.11 Scale/Key-Quantisierung|Obsidian]]
   - [Tempo/BPM-Steuerung (V/Oct, BEATS_FACTOR)](#sec4-12) · [[#4.12 Tempo/BPM-Steuerung (V/Oct, BEATS_FACTOR)|Obsidian]]
5. [Marf248 – Funktionsreferenz (C++)](#sec5) · [[#5. Marf248 – Funktionsreferenz (C++)|Obsidian]]
6. [Marf248TriggerExpander](#sec6) · [[#6. Marf248TriggerExpander|Obsidian]]
7. [Marf248CvExpander](#sec7) · [[#7. Marf248CvExpander|Obsidian]]
8. [Expander-Mechanismus (Kommunikation zwischen den Modulen)](#sec8) · [[#8. Expander-Mechanismus (Kommunikation zwischen den Modulen)|Obsidian]]
9. [Persistenz (Patch-Speicherung)](#sec9) · [[#9. Persistenz (Patch-Speicherung)|Obsidian]]
10. [Bekannte Fallstricke im Zusammenspiel](#sec10) · [[#10. Bekannte Fallstricke im Zusammenspiel|Obsidian]]
11. [Design-Entscheidungen & Abgrenzungen](#sec11) · [[#11. Design-Entscheidungen & Abgrenzungen|Obsidian]]

---

<a id="sec1"></a>
## 1. Architektur-Überblick
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Das Projekt besteht aus drei VCV-Rack-Modulen, die zusammen ein Plugin bilden:

**Marf248** *(`Marf248.hpp`, `Marf248.cpp`)*<br>
Hauptmodul: zwei unabhängige AFG-Sections (A/B), die sich 16 gemeinsame Stages teilen. Enthält die gesamte DSP-Kernlogik.

**Marf248TriggerExpander** *(`Marf248TriggerExpander.hpp/.cpp`)*<br>
Optionaler 24HP-Expander: zusätzliche Trigger-/Gate-Ein- und Ausgänge (Clear, Presets, Scale/Key, Start/End-of-Cycle, Run-Gates, Advance, Adress-Speicher, Stage-Modifier-CV, SEXT).

**Marf248CvExpander** *(`Marf248CvExpander.hpp/.cpp`)*<br>
Optionaler 12HP-Expander: polyphone CV-Modulation der Stage-Zeit und der Slew-Kurve, sowie polyphone Gate-Ausgänge pro Stage.

**Grundprinzip des Expander-Mechanismus:** Beide Expander besitzen selbst
**keine eigene DSP-Logik**. Sie sind reine "Jack-Träger". Das Hauptmodul
`Marf248::process()` prüft bei jedem Sample, ob links oder rechts ein
passendes Modul andockt (`getLeftExpander()`/`getRightExpander()`), und
liest/schreibt dessen Parameter, Inputs und Outputs direkt. Das vermeidet
Message-Passing/Doppelpufferung, ist aber nur zulässig, weil die Expander
keine eigene unabhängige Verarbeitung haben (siehe VCV-Rack-Plugin-API-Guide,
Abschnitt „Expanders": „you can implement its entire DSP algorithm in the
[primary] module"). Die Expander selbst prüfen ebenfalls jeden Frame
eigenständig, ob ein Marf248 andockt, um ihre Outputs/Lichter beim
Entfernen/Verschieben sauber auf 0V zurückzusetzen.

Der Trigger-Expander-Lookup steht bewusst ganz am Anfang von
`Marf248::process()`, noch vor der Stage-Address-Verarbeitung, da SEXT
(externe Strobe-Pulse) den Trigger-Expander bereits kennen muss, bevor
`applyStageAddress()` aufgerufen wird.

Alle drei Module werden in `plugin.cpp`/`plugin.hpp` registriert
(`modelMarf248`, `modelMarf248TriggerExpander`, `modelMarf248CvExpander`).

---

<a id="sec2"></a>
## 2. Grundbegriffe: Stage, Section, Modifier-Bits
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

- **Stage**: Eine von 16 Programmierstufen (`NUM_STAGES = 16`). Jede Stage
  besitzt einen Spannungswert, einen Zeitwert, einen Range- und einen
  Curve-Regler sowie ein Bündel digitaler Modifier-Bits. Die 16 Stages sind
  **modulweit geteilt** – Section A und Section B lesen dieselbe
  Programmierung, laufen aber unabhängig als zwei eigene "Zeiger"
  (`currentStage`) darüber.
- **Section (A/B)**: Zwei unabhängige AFG-Instanzen (`Afg afgA`, `Afg afgB`
  in `Marf248`), die jeweils eine eigene Laufrichtung, eigenen
  Run/Stop-Zustand, eigenen Time-Multiplier, eigene Stage-Address-Quelle
  usw. besitzen, aber dieselben 16 Stages abfahren.
- **Modifier-Bits** (`enum StageModifierBits`, in `Marf248.hpp`): Ein
  einzelnes `int stageModifier[NUM_STAGES]`-Array kodiert pro Stage alle
  digitalen Zustände als Bitmaske. Übersicht:

| Bit | Bedeutung | Hardware-Vorbild? |
|---|---|---|
| `MOD_QUANT_OCT` | Quantisierung der Stage-Spannung auf Scale/Key | ja |
| `MOD_SLOPED` | Stage rampt (Slew) statt zu springen | ja |
| `MOD_STOP` | Programmierte Stop-Stage | ja |
| `MOD_SUSTAIN` | Hält die Stage, solange Start-Gate high ist | ja |
| `MOD_ENABLE` | Hält die Stage, solange Start-Gate low ist | ja |
| `MOD_FIRST` | Markiert die Stage als Zyklus-Anfang | ja |
| `MOD_LAST` | Markiert die Stage als Zyklus-Ende | ja |
| `MOD_SLEW_EXP` / `MOD_SLEW_LOG` | Kurvenform der Rampe (kein Bit = linear) | ja |
| `MOD_RANGE_HALF` / `MOD_RANGE_LIMITED` (+ 3-Bit-Oktav-Offset) | Voltage-Range Full/Half/Limited | ja |
| `MOD_VOLT_EXT` / `MOD_TIME_EXT` | Externe Spannungs-/Zeitquelle (Kanalwahl A–D) statt Regler | ja |
| `MOD_PULSE1` / `MOD_PULSE2` | Programmierte Impuls-Ausgänge P1OUT/P2OUT | ja |
| `MOD_SKIP` | Stage wird beim automatischen Ablauf übersprungen | **nein** (reine Software-Erweiterung) |
| `MOD_SLEW_NORMALIZE` | Streckt die sichtbar bewegte Kurve auf die volle Stage-Zeit | **nein** |
| `MOD_TRANGE_1/2/4` | Time Range TR1/TR2/TR4 (kein Bit = TR3, Default) | ja (TR1–4) |
| `MOD_MUTE` | Unterdrückt P1OUT/P2OUT **und** APOUT für diese Stage, ohne `MOD_PULSE1`/`MOD_PULSE2` zu verändern; Voltage/Slew unbeeinflusst | **nein** |

Zusätzlich kodiert ein 3-Bit-Feld (`RANGE_OFFSET_MASK`) den Oktav-Offset im
Limited-Range-Modus (5 Stufen: +0/+2/+4/+6/+8 V).

Zwei weitere, vom Modifier-Bit-Array **unabhängige** Speicherstrukturen
ergänzen die Stage-Programmierung: das **Preset-System** (komplette
Programmierung aller 16 Stages, 12 Slots, siehe [4.8](#sec4-8)) und der neue
**Adress-Speicher** (reines Sprung-Lesezeichen auf einen Stage-Index, 16
Slots pro Section, siehe [4.9](#sec4-9)) – beide sind konzeptionell klar
voneinander getrennt und dürfen nicht verwechselt werden.

---

<a id="sec3"></a>
## 3. Marf248 – Hauptmodul
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

<a id="sec3-1"></a>
### 3.1 Panel-Layout
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Das Panel ist 146HP breit (vorher 120HP, dann 126HP für die neue
Ausgangsspalte, dann durch das Slider-/Button-Relayout nochmal breiter
geworden, siehe unten) und in mehrere senkrechte Spalten gegliedert
(von links nach rechts):

```
[External CV A–D] [Section-A-Controls] [Section-A-Jacks] [Section-A-Ausgänge] [Stage-Array (16×)] [Section-B-Ausgänge] [Section-B-Jacks] [Section-B-Controls] [Preset-Spalte]
```

- **External-CV-Spalte** (ganz links): die vier modulweit geteilten
  externen CV-Eingänge (Buchsen A–D), die von `MOD_VOLT_EXT`/`MOD_TIME_EXT`
  je Stage referenziert werden können.
- **Section-A/B-Controls-Spalte**: Tempo-Knob/Attenuverter,
  BEATS_FACTOR-Trimmer, Run-Taster, Mode/Pendulum-Taster,
  Stage-Address-Bedienelemente.
- **Section-A/B-Jacks-Spalte** (12–121mm): Start/Stop, Tempo-CV, Live-BPM-
  Anzeige, BEATS_FACTOR-CV, P1OUT/P2OUT, APOUT, Stage-Address-CV,
  Stage-Address-Kanalwahl-CV.
- **Section-A/B-Ausgangsspalte** (neu, 12mm breit, 12–44mm): nur VOUT
  (Voltage-Ausgang) und TOUT (Zeit-/Interval-Ausgang) – eigene, schmale
  Spalte zwischen Jacks-Spalte und Stage-Array, spiegelbildlich für A/B.
  Wurde aus der Jacks-Spalte ausgelagert, um dort Platz für die
  vergrößerte BPM-Anzeige zu schaffen.
- **Stage-Array** (Mitte, 16 gleich aufgebaute Spalten): die eigentliche
  Programmierung jeder der 16 Stages.
- **Preset-Spalte** (ganz rechts): Load/Save, Scale/Key, 12 Preset-Slots.
- Über dem Stage-Array mittig: **Global Clear**, **Reset Values** und
  **Auto-Fill**.

Jede Spalte ist zur optischen Gliederung von einem dünnen Rahmen
(`StageFrame`-Widget) umgeben; funktional zusammengehörige Buchsengruppen
werden zusätzlich durch dünne horizontale Trennlinien (`HDivider`) abgesetzt.

<a id="sec3-2"></a>
### 3.2 Bedienelemente pro Stage
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Jede der 16 Stage-Spalten enthält von oben nach unten:

**Voltage** (`STAGE_VOLTAGE_PARAM`) *(Vertikaler Schieberegler, 0–10V)*<br>
Programmierte Ausgangsspannung der Stage. Im External-Modus (`MOD_VOLT_EXT`) dient die Reglerposition stattdessen als Kanalwahl A–D (4 Zonen à 2,5V). Tooltip zeigt je nach Zustand Volt, Notenname (bei Quantize) oder Kanalname (bei External) – siehe `StageVoltageQuantity`.

**Time** (`STAGE_TIME_PARAM`) *(Vertikaler Schieberegler, normalisiert 0–1)*<br>
Programmierte Stage-Zeit als Position **innerhalb** des gewählten Time-Range-Bereichs (TR1–4). Der Tooltip zeigt den Buchla-Cookbook-Dial-Wert (0–10, führend) mit der exakt aufgelösten Sekundenzahl dahinter in Klammern, z.B. `4.0 (0.132 s)` – siehe `StageTimeQuantity`. Rechtsklick-Zahleneingabe akzeptiert weiterhin den 0–10-Dial-Wert. Im External-Modus (`MOD_TIME_EXT`) dient die Position als Kanalwahl A–D.

**Griff-Wertanzeige** *(beide Slider, direkt im Griff)*<br>
Zusätzlich zum Tooltip zeigt der Schieberegler-Griff den aktuellen Wert gerundet auf eine Ganzzahl (0–10) direkt auf dem Panel an – bei Voltage die Spannungshöhe in Volt, bei Time denselben Buchla-Cookbook-Dial-Wert wie im Tooltip (die führende Zahl vor der Klammer), da `pq->getDisplayValue()` für beide Parameter dieselbe 0–10-Skala liefert. Schriftart ShareTechMono, goldener Rand (RGB 235,170,60) mit schwarzer Füllung, mittig im Griff. In den Quantize-/External-Sondermodi zeigt die Ziffer weiterhin den rohen Reglerwert (0–10), nicht die aufgelöste Bedeutung (Notenname bzw. Kanalwahl A–D) – dafür bleibt der Tooltip zuständig, da eine Kürzung im 8×6mm-Griff nicht zuverlässig lesbar wäre.

**Zwei Stage-LEDs (`STAGE_LIGHT_A/B`)** *(Licht)*<br>
Zeigen an, ob Section A bzw. B gerade diese Stage adressiert (grün/rot, nebeneinander).

**Pulse 1 / Pulse 2 / Skip** *(LED-Taster (Toggle))*<br>
Reihe 1: `MOD_PULSE1`/`MOD_PULSE2` lösen beim Erreichen der Stage einen kurzen Trigger auf P1OUT/P2OUT der jeweiligen Section aus. `MOD_SKIP` (reine Software-Erweiterung) überspringt die Stage beim automatischen Zeitablauf vollständig – keine Zeit, keine Trigger, keine Voltage-Ausgabe. Wirkt nur beim automatischen Ablauf, nicht bei manueller Adressierung (Reset/Strobe/Continuous).

**Stop / Sustain / Enable** *(LED-Taster (Toggle))*<br>
Reihe 2: `MOD_STOP` = programmierte Stop-Stage (pausiert den Ablauf, bis die Section neu gestartet wird). `MOD_SUSTAIN` = hält die Stage, solange am Start-Eingang ein High-Pegel (Gate) anliegt (Release-Verhalten wie ADSR). `MOD_ENABLE` = hält die Stage, solange **kein** High-Pegel anliegt (Freigabe erst durch Gate). Beide Halte-Zustände pausieren den Verweilzeit-Zähler `elapsed`. **Achtung Fallstrick ohne gepatchtes Kabel an START_A/B – siehe [Abschnitt 10](#sec10).**

**First / Last / Slope** *(LED-Taster (Toggle))*<br>
Reihe 3: `MOD_FIRST`/`MOD_LAST` markieren Zyklus-Anfang/-Ende (Wrap-Punkte für Forward/Reverse/Pendulum, lösen Start-/End-of-Cycle-Trigger am Trigger-Expander aus; mehrere First/Last-Paare sind möglich, siehe [4.2](#sec4-2)). `MOD_SLOPED` schaltet zwischen gesprungener (Stepped, Default) und rampender (Slew) Spannungsänderung um.

**Quantize / Volt-Ext / Time-Ext** *(LED-Taster (Toggle))*<br>
Reihe 4: `MOD_QUANT_OCT` quantisiert die Zielspannung auf das globale Scale/Key-Raster. `MOD_VOLT_EXT`/`MOD_TIME_EXT` schalten Spannungs- bzw. Zeitquelle der Stage auf extern (Kanalwahl über den jeweiligen Regler).

**Shape** (`STAGE_SHAPE_PARAM`) + zwei Lichter *(LED-Taster (3-Zustands-Zyklus))*<br>
Zyklus Linear → Exponentiell → Logarithmisch (nur wirksam bei aktivem Slope). Zwei separate LEDs (`STAGE_SHAPE_EXP_LIGHT`/`STAGE_SHAPE_LOG_LIGHT`) statt einer kombinierten Anzeige; kein Bit gesetzt = linear.

**Curve** (`STAGE_CURVE_PARAM`) *(Trimmer, 0–1)*<br>
Kurvenstärke der Exp/Log-Rampe. Mitte (0,5) = Referenzkrümmung (`SLEW_CURVE_POWER = 2,5`), links linearer, rechts stärker gekrümmt (bis Exponent ≈15,6). Kann zusätzlich per Slew-Curve-CV vom CV-Expander moduliert werden.

**Normalize** (`STAGE_NORMALIZE_PARAM`) *(LED-Taster (Toggle))*<br>
`MOD_SLEW_NORMALIZE` (reine Software-Erweiterung): streckt bei extremer Curve-Einstellung den sichtbar bewegten Teil der Kurve auf die volle Stage-Zeit, statt ihn in einen winzigen Zeitbruchteil zu quetschen.

**Time Range** (`STAGE_TRANGE_PARAM`, `TrangeButton`) *(Großer, selbstzeichnender Taster (4-Zustands-Zyklus))*<br>
Wählt TR1 (0,002–0,03s) bis TR4 (2–30s); zeigt die aktuelle Ziffer 1–4 direkt auf dem Taster an. TR3 (0,2–3s) ist Default (kein Bit gesetzt). Rechtsklick öffnet ein Kontextmenü mit direkter Auswahl.

**Range** (`STAGE_RANGE_PARAM`, `RangeKnob`) *(Großer Drehregler mit Textanzeige (7 Raststellungen))*<br>
Full (0–10V unverändert) → Half (0–5V gestaucht) → Limited +0/+2/+4/+6/+8V (2V-Spanne, entspricht 2 Oktaven 1V/Okt., um den gewählten Offset verschoben). Zeigt Kürzel ("F"/"H"/"L0"…"L8") direkt auf dem Regler sowie einen farbigen Zeiger. Rechtsklick öffnet Checklisten-Kontextmenü (zuverlässiger als Texteingabe bei diesem snap-Regler).

<a id="sec3-3"></a>
### 3.3 Bedienelemente pro Section (A/B)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Identischer, spiegelbildlicher Aufbau für Section A und B:

**Controls-Spalte** (von oben nach unten):
- **Tempo** (`TIME_MULT_A/B_PARAM`, −5…+5V) + **Attenuverter**
  (`TIME_MULT_ATTEN_A/B_PARAM`, −1..+1) für den zugehörigen CV-Eingang.
  **Breaking Change ggü. der ursprünglichen 0,5×–4,0×-Faktor-Semantik**:
  der Regler speichert jetzt eine V/Oct-Tempo-Spannung (0V = 120 BPM,
  exponentiell) statt eines direkten Zeitfaktors. Tooltip und
  Rechtsklick-Zahleneingabe arbeiten in BPM (`TempoQuantity`). Siehe
  [4.12](#sec4-12) für die vollständige Herleitung.
- **BEATS_FACTOR** (`BEATS_FACTOR_A/B_PARAM`, Trimmer, 0,1–64, Default 4)
  + zugehöriger CV-Eingang: legt fest, wie viele Beats ein voller Zyklus
  über alle nicht geskippten Stages darstellen soll; daraus wird der
  tatsächlich wirksame Zeitfaktor zurückgerechnet. Siehe [4.12](#sec4-12).
- **Run** (`RUN_A/B_PARAM`, LED-Taster): togglet Start/Stop der Section
  (grüne LED = läuft). ODER-verknüpft mit einem externen Start/Stop-Gate
  vom Trigger-Expander. **Wird ignoriert, solange Advance gepatcht ist –
  siehe [4.10](#sec4-10) und [Fallstricke](#sec10).**
- **Mode** (`MODE_A/B_PARAM`, LED-Taster): kehrt die Basis-Laufrichtung um
  (aus = Forward, an = Reverse).
- **Pendulum** (`PENDULUM_A/B_PARAM`, LED-Taster): schaltet zusätzlich das
  Hin-und-Her-Pendeln zwischen First und Last dazu (siehe [4.2](#sec4-2)).
- **Stage-Address-Knob** (`STAGE_ADDR_KNOB_A/B_PARAM`, Trimmer 0–15):
  interne Stage-Adresse (nur wirksam im Int-Modus). **Bewegt `currentStage`
  aus sich selbst heraus NIE – erst Strobe/Continuous/Reset wenden die
  Adresse tatsächlich an, siehe [Fallstricke](#sec10).**
- **Stage-Address Int/Ext** (`STAGE_ADDR_MODE_A/B_PARAM`, LED-Taster):
  wählt zwischen internem Knob und externer CV
  (`STAGE_ADDR_EXT_A/B_INPUT`) als Adressquelle.
- **Continuous** (`STAGE_ADDR_CONT_A/B_PARAM`, LED-Taster): hält
  `currentStage` jeden Frame synchron zur Adresse und erzwingt sofortige
  volle Auslenkung – die Section läuft dabei intern nicht (`running` wird
  in `applyStageAddress()` erzwungen unwirksam). Eine aktive Slope-Rampe
  zum adressierten Zielwert läuft trotzdem weiter (siehe [4.3](#sec4-3)).
- **Strobe** (`STAGE_ADDR_STROBE_A/B_PARAM`, Momentary-Taster): einmaliger
  Sprung auf die aktuelle Adresse; danach läuft der normale Zeitablauf/Slew
  weiter. Blitzt kurz auf (Blau) als Feedback.
- **Reset** (`STAGE_ADDR_RESET_A/B_PARAM`, Momentary-Taster): springt sofort
  auf die MOD_FIRST-Stage mit dem **niedrigsten Index** (`findLowestFirst()`,
  Fallback: Stage 1) – wichtig bei mehreren First/Last-Segmenten, siehe
  [4.2](#sec4-2).

**Jacks-Spalte** (von oben nach unten): Start-Trigger, Stop-Trigger,
Tempo-CV-Eingang, **Live-BPM-Anzeige** (reiner Text-Readout, kein Bauteil,
zeigt das tatsächlich wirksame Tempo inkl. CV – siehe [4.12](#sec4-12)),
BEATS_FACTOR-CV-Eingang, P1OUT/P2OUT (programmierte Impulse, mit rotem
Ring-Licht `PulseRingLight` um die Buchse, das mit dem Puls mitblinkt),
APOUT (All-Pulses, feuert bei **jedem** Stage-Wechsel), Stage-Address-CV-
Eingang, Stage-Address-Kanalwahl-CV-Eingang.

**Ausgangsspalte** (neu, eigene schmale Spalte zwischen Jacks-Spalte und
Stage-Array): VOUT (Voltage-Ausgang), TOUT (Zeit-/Interval-Ausgang, 0–10V
absteigende Rampe). Vorher Teil der Jacks-Spalte, jetzt ausgelagert, um
dort Platz für die größere BPM-Anzeige zu schaffen.

**Status-LED "Run/Hold/Stop"** (dreifarbig, an derselben Panel-Position):
- **Grün** (`RUN_x_LIGHT`): Section läuft normal (oder Advance-Modus
  aktiv – wird bewusst wie "läuft" angezeigt, siehe [4.10](#sec4-10)).
- **Gelb** (`HOLD_x_LIGHT`): Section wartet in einer Sustain-/Enable-Stage
  auf ein Start-Gate.
- **Rot** (`STOP_x_LIGHT`): Section gestoppt (Taster/Puls, programmierte
  Stop-Stage, oder Continuous-Adressierung aktiv).

<a id="sec3-4"></a>
### 3.4 Modulweit geteilte Elemente
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

- **Global Clear** (`CLEAR_PARAM`, LED-Taster, rotes Blitzlicht): setzt bei
  allen 16 Stages sämtliche Modifier-Bits, Range (→ Full) und Curve
  (→ Mitte) zurück. Verwirft zusätzlich ausstehende Preset-Pickups. Folgt
  bewusst nur der im Manual beschriebenen Teilmenge (Output Pulses,
  Voltage-Programmierung, Operating Mode, Time Range) – Voltage/Time-
  Regler selbst bleiben unangetastet. **Der Adress-Speicher bleibt von
  Global Clear unberührt** (siehe [4.9](#sec4-9)).
- **Reset Values** (`RESET_VALUES_PARAM`, LED-Taster, gelbes Blitzlicht):
  reine Software-Erweiterung ohne Hardware-Vorbild. Setzt **nur**
  Voltage- und Time-Regler aller 16 Stages auf Default zurück (0V bzw.
  Zeit-Mitte) – Modifier-Bits, Range und Curve bleiben unverändert. Klare
  Abgrenzung zu Global Clear.
- **Auto-Fill** (`AUTOFILL_PARAM`, LED-Taster, blaues Blitzlicht): reine
  Software-Erweiterung ohne Hardware-Vorbild, für den
  Phasor-Generator-Anwendungsfall entwickelt. Setzt die 16 geteilten
  Voltage-Regler auf ein lineares 0–10V-Raster (Schrittweite 10/16 = 0,625V,
  Stage 1 = 0V … Stage 16 = 9,375V) und erzwingt gleichzeitig `Range = Full`
  je Stage. Zeit- und Modifier-Bits bleiben unangetastet (analog zu Reset
  Values). Da Voltage/Range modulweit zwischen Section A und B geteilt
  sind, gibt es bewusst nur einen einzigen globalen Button – getrennte
  A/B-Varianten sind architektonisch nicht möglich, ohne die geteilte
  Stage-Datenstruktur grundlegend umzubauen.
- **Preset-System** (`LOAD_PARAM`, `SAVE_PARAM`, `PRESET_PARAM` ×12): Load
  bzw. Save "scharf schalten" (latching), danach wählt der nächste Druck
  auf einen der 12 Preset-Taster den Slot. Ein Preset speichert die
  komplette Programmierung aller 16 Stages (Voltage/Time/Range/Curve-
  Reglerwerte + alle Modifier-Bits). Siehe [4.8](#sec4-8) (Pickup/Catch).
  Leere Slots werden beim Laden ignoriert (kein versehentliches
  Zurücksetzen), kurzer Blitz als "hier ist nichts gespeichert"-Feedback.
- **Scale/Key** (`SCALE_PARAM`, `KEY_PARAM`): gleiches Arm-Prinzip wie
  Load/Save, reused dieselben 12 Preset-Taster als Doppelbelegung. Scale:
  nur Taster 10/11/12 gültig (Major/Minor/Chromatic). Key: alle 12 Taster
  gültig (C…B). Siehe [4.11](#sec4-11).

<a id="sec3-5"></a>
### 3.5 Ein-/Ausgänge im Überblick
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

| Kategorie | Ein-/Ausgänge | Geteilt? |
|---|---|---|
| Section-Start/Stop | `START_A/B_INPUT`, `STOP_A/B_INPUT` | pro Section |
| Externe Spannungsquellen | `EXT_A/B/C/D_INPUT` | modulweit geteilt |
| Stage-Address-CV | `STAGE_ADDR_EXT_A/B_INPUT` (poly, bis zu 4 Kanäle A–D) | pro Section |
| Stage-Address-Kanalwahl-CV | `STAGE_ADDR_EXT_CHANNEL_A/B_INPUT` (neu) | pro Section |
| Time-Multiplier-CV | `TIME_MULT_CV_A/B_INPUT` (Tempo, V/Oct) | pro Section |
| BEATS_FACTOR-CV | `BEATS_FACTOR_CV_A/B_INPUT` (neu) | pro Section |
| Section-Ausgänge | `VOUT_A/B`, `TOUT_A/B`, `APOUT_A/B`, `P1OUT_A/B`, `P2OUT_A/B` | pro Section |

---

<a id="sec4"></a>
## 4. Interne Signalverarbeitung
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

<a id="sec4-1"></a>
### 4.1 Der Afg-Kern (Ablaufsteuerung einer Section)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

`struct Afg` (in `Marf248.hpp`) kapselt den **kompletten Laufzeitzustand
und die Ablaufsteuerung einer einzelnen Section** (A oder B). `Marf248`
hält zwei Instanzen (`afgA`, `afgB`), die dieselben Stage-Daten (über
Funktionsparameter) lesen, aber unabhängig laufen.

Wichtige Zustandsfelder:
- `currentStage`: Index (0–15) der aktuell adressierten Stage.
- `running` / `effectiveRunning`: `running` speichert den persistenten
  Taster-/Puls-Zustand; `effectiveRunning = running || runGateHigh` ist der
  tatsächlich für diesen Frame wirksame Lauf-Zustand (siehe
  Trigger-Expander-Run-Gate). Vom Advance-Modus bewusst **unabhängig**
  gehalten (siehe [4.10](#sec4-10)).
- `elapsed`: Verweilzeit-Zähler seit Eintritt in die aktuelle Stage.
- `voltageFrom` / `voltageOut`: Start- bzw. aktueller Wert der laufenden
  Rampe (für nahtloses Slew über Stage-Grenzen hinweg).
- `reverseMode`, `pendulumMode`, `pendulumDir`, `pendulumHeld`,
  `pendulumWasOn`: Laufrichtungssteuerung (siehe [4.2](#sec4-2)).
- `continuousActive`: merkt sich, ob `applyStageAddress()` diesen Frame im
  Continuous-Zweig lief (siehe [4.3](#sec4-3)).
- `advanceModeActive`: true, solange `ADVANCE_A/B_INPUT` gepatcht ist
  (siehe [4.10](#sec4-10)).
- `statusHeld`, `statusStoppedByStage`: Für die dreifarbige Status-LED.
- Mehrere `dsp::PulseGenerator`/`dsp::SchmittTrigger`-Instanzen für
  Trigger-Erkennung und Impuls-Ausgabe, darunter `advanceTrigger` (echte
  Flanken-Erkennung, nur verarbeitet wenn die Buchse gepatcht ist) und
  `runGateTrigger` (reine Pegel-Erkennung mit Schmitt-Hysterese).

<a id="sec4-2"></a>
### 4.2 Stage-Advance-Logik: Forward / Reverse / Pendulum / Skip
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

`Afg::advanceStage()` wird aufgerufen, wenn `elapsed >= stageTime[currentStage]`
(automatischer Ablauf) oder sofort, wenn die aktuelle Stage als Skip
markiert ist. Ablauf:

1. **Nächste Stage ermitteln**, abhängig vom Laufmodus:
   - **Forward** (Default): `next = currentStage + 1`; bei Erreichen des
     Endes oder einer `MOD_LAST`-Stage Wrap zurück auf die nächste
     `MOD_FIRST`-Stage (`findFirstBefore()`).
   - **Reverse** (`reverseMode`): Spiegelbild von Forward, dekrementiert
     und wrapt bei `MOD_FIRST` auf die nächste `MOD_LAST`-Stage
     (`findLastAfter()`).
   - **Pendulum** (`pendulumMode`): pendelt zwischen First und Last hin
     und her (nimmt an, dass First..Last ein zusammenhängender,
     nicht-wrapender Bereich ist). An jedem Wendepunkt wird die Stage
     **einmal wiederholt** (`pendulumHeld`), bevor die Richtung umkehrt –
     dadurch beträgt die Gesamtperiode exakt 2×M (M = Anzahl Stages im
     First–Last-Bereich) statt 2×(M−1); das hält Pendulum synchron zu
     einer parallel vorwärts laufenden Section. Die Startrichtung beim
     Einschalten von Pendulum wird aus `reverseMode` abgeleitet
     (Forward→Bounce Richtung Last, Reverse→Bounce Richtung First); ein
     Umschalten von `reverseMode` während Pendulum bereits läuft wirkt
     sich erst beim nächsten Aus/An-Zyklus aus (kein abrupter Sprung
     mitten im Bounce).

   **Sync-Verhalten mit einer parallel laufenden Forward-Section**
   (Live-verifiziert, Session August 2026): Bei identischer Stage-Zeit auf
   beiden Sections ergibt sich ein exakt periodisches Zwei-Zustands-Muster
   über je 2 Forward-Umläufe (= 1 vollständige Pendulum-Hin-und-Rückfahrt,
   2×M Ticks): im 1. Umlauf laufen beide **deckungsgleich** (Pendulum
   steigt wie Forward), im 2. Umlauf laufen beide **spiegelbildlich**
   (Pendulum fällt, während Forward erneut steigt) – beide Zustände zählen
   als „synchron" im Sinne von Spannungsverlauf. Ein testweise entfernter
   Wendepunkt-Halt (Periode 2×(M−1) statt 2×M) wurde wieder verworfen: ohne
   den Halt ist die Pendulum-Periode **kein** sauberes Vielfaches der
   Forward-Periode M mehr (kleinstes gemeinsames Vielfaches bei M=16 und
   2×(M−1)=30 wäre 240 Ticks) – beide Sections liefen dadurch komplett
   auseinander statt synchron zu bleiben. Ein anfänglich beobachteter
   Einzelausreißer (3 von 4 Durchgängen synchron, einer nicht) ließ sich
   nicht auf einen Code-Fehler zurückführen, sondern verschwand nach einem
   gemeinsamen Neustart/Reset beider Sections – deutet auf einen
   Startzeitpunkt-Versatz zwischen A und B hin, nicht auf ein
   Pendulum-Logikproblem.
2. **Skip überspringen**: Eine Schleife (max. `NUM_STAGES` Versuche als
   Sicherheitsnetz) wiederholt dieselbe Schrittlogik, solange die
   gefundene Stage `MOD_SKIP` gesetzt hat. Skip wirkt nur beim
   automatischen Ablauf, nicht bei manueller Adressierung.
3. **Rampen-Startpunkt** setzen (`voltageFrom = voltageOut`), `elapsed`
   auf 0, `allPulseGen` triggern (APOUT).
4. Bei Wiedereintritt **ohne** Indexwechsel (Pendulum-Wendepunkt-
   Wiederholung): zusätzlicher kurzer Retrigger-Impuls (`regateGen`,
   `REGATE_GAP_SECONDS = 5ms`) für den CV-Expander, damit nachgeschaltete
   Hüllkurvengeneratoren zuverlässig neu triggern.
5. `resetPulseEdgeState()` setzt die Flanken-Vorwerte für P1/P2/First/Last
   zurück (siehe [4.7](#sec4-7)).

**Mehrere First/Last-Segmente:** Da `findFirstBefore()`/`findLastAfter()`
lokal (relativ zur aktuellen Position) wrappen statt global, lässt sich
das 16-Stage-Feld in mehrere unabhängig loopende Segmente unterteilen
(z.B. First=1/Last=3, First=5/Last=8, First=12/Last=16) – jedes Segment
loopt eigenständig innerhalb seiner Grenzen. Der Wechsel zwischen Segmenten
erfolgt aktiv über externe Stage-Address-CV + Strobe/Continuous, nicht
automatisch. Reset springt in diesem Fall gezielt zum **allerersten**
Segment (`findLowestFirst()`, niedrigster Index) – nicht zum aus Sicht der
aktuellen Position nächstgelegenen First (das wäre bei `findFirstBefore()`
der Fall gewesen und hätte je nach `currentStage` ein anderes Segment
getroffen).

<a id="sec4-3"></a>
### 4.3 Stage Address: Reset / Continuous / Strobe / Kanalwahl
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

`Afg::applyStageAddress()` wird **vor** dem eigentlichen `Afg::process()`
aufgerufen (Reihenfolge wichtig, da Continuous `running` unwirksam machen
muss, damit die normale Zeitablauflogik im selben Frame pausiert):

- **Reset**: springt sofort auf die MOD_FIRST-Stage mit dem niedrigsten
  Index (`findLowestFirst()`, Fallback Stage 1) – unabhängig vom aktuellen
  Modus, unabhängig von Continuous/Strobe.
- **Continuous**: hält `currentStage` jeden Frame synchron zur Adresse
  (Knob oder externe CV) und erzwingt sofortige volle Auslenkung. Der
  eigentliche Rampen-Fortschritt (`elapsed += sampleTime`) läuft dabei
  weiterhin kontinuierlich in `process()`, sodass eine Slope-Rampe zum
  neu adressierten Zielwert trotzdem sanft fortschreitet – das entspricht
  dem MARF-Handbuch ("slope time is equal to the programmed interval
  time", gilt laut Firmware-Changelog auch bei externer/Continuous-
  Adressierung) und wird z.B. im Cookbook-"String Patch" bewusst für
  Glide-Verhalten genutzt. Start/Stop ist im Continuous-Modus nicht
  anwendbar (`running` bleibt unwirksam).
- **Strobe**: einmaliger Sprung auf die aktuelle Adresse (`jumpToStage()`),
  danach läuft der normale Ablauf/Slew unverändert weiter. Wird im
  Continuous-Zweig durch das frühe `return` bewusst ignoriert.

Die Adresse selbst kommt entweder vom internen Knob (`STAGE_ADDR_KNOB_A/B`)
oder – bei aktivem Ext-Modus – aus der externen CV
(`STAGE_ADDR_EXT_A/B_INPUT`).

**4-Kanal-A–D-Kanalwahl (neu):** `STAGE_ADDR_EXT_A/B_INPUT` ist inzwischen
ein **polyphoner** Eingang und kann bis zu 4 unabhängige Adress-CVs (Kanal
A–D) gleichzeitig führen. Welcher Kanal tatsächlich gelesen wird, bestimmt
die neue, dedizierte Kanalwahl-CV `STAGE_ADDR_EXT_CHANNEL_A/B_INPUT`
(0–10V, in 4 gleich große 2,5V-Zonen aufgeteilt, identisches Zonenprinzip
wie bei `MOD_VOLT_EXT`/`MOD_TIME_EXT`). Ist die Kanalwahl-Buchse **nicht**
gepatcht, fällt die Kanalwahl automatisch auf Kanal A (Index 0) zurück –
dadurch bleiben bestehende Mono-Patches an `STAGE_ADDR_EXT_A/B_INPUT`
(die automatisch auf Kanal 1 landen) unverändert funktionsfähig
(Backward-Kompatibilität, von Kinovice live verifiziert). Für den
Kanalwahl-Test wird zusätzlich zum Ext-Modus auch **Continuous** benötigt,
damit die Section der gewählten Quelle tatsächlich live folgt – der
Ext-Modus allein liefert nur den Adresswert, wendet ihn aber genau wie der
interne Knob nicht automatisch an (siehe [Fallstricke](#sec10)).

Zusätzlich kann ein externer Strobe-Puls ("SEXT", vom Trigger-Expander) den
Panel-Strobe-Taster ODER-verknüpft auslösen.

<a id="sec4-4"></a>
### 4.4 Voltage-Signalpfad: Range, Quantize, Slew
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Reihenfolge im Signalpfad (in `Afg::process()`), pro Sample:

1. **Range** (`applyRange()`): skaliert/verschiebt die rohe Slider-Spannung
   zuerst (Full unverändert, Half auf 0–5V gestaucht, Limited auf eine
   2V-Spanne gestaucht plus Oktav-Offset).
2. **Quantize** (`quantizeToScale()`), falls `MOD_QUANT_OCT` gesetzt: snappt
   das bereits skalierte Ergebnis auf den nächsten Ton innerhalb der
   gewählten Scale+Key (Major/Minor: 7-Ton-Raster; Chromatic: reines
   Halbton-Raster, Key wirkungslos). Sucht im Fenster ±12 Halbtöne um den
   naiv gerundeten Wert nach dem nächstgelegenen Scale-Mitglied.
3. **Slew** (`applySlewCurve()`), falls `MOD_SLOPED` gesetzt: interpoliert
   zwischen `voltageFrom` und dem Zielwert `target` anhand der
   normalisierten Phase `t = elapsed / stageTime` und der gewählten
   Kurvenform:
   - Linear (Default): `f(t) = t`
   - Exponentiell: `f(t) = 1 - (1-t)^power` (schneller Start, sanftes Ziel)
   - Logarithmisch: `f(t) = t^power` (sanfter Start, schnelles Ziel)

   `power` wird pro Stage aus der Curve-Reglerposition (plus optionaler
   Slew-Curve-CV vom CV-Expander) berechnet: Mitte → `SLEW_CURVE_POWER =
   2,5`, Anschläge → linear (power=1) bzw. stark gekrümmt (power≈15,6),
   quadratisch geglättete Übergangskurve zwischen `CURVE_FACTOR_CENTER`
   und `CURVE_FACTOR_EXTREME`.

   Ohne `MOD_SLOPED` springt `voltageOut` sofort auf den Zielwert
   (Stepped-Verhalten).
4. **Normalize** (`MOD_SLEW_NORMALIZE`, optional): korrigiert bei extremer
   Curve-Einstellung, dass die theoretisch fast schon erreichte Kurve
   (>99,9%) den Zielwert erst exakt bei `t=1` erreicht, statt vorher
   „hängenzubleiben" – streckt dazu die Zeitachse (`slewExpNormalized()`).

<a id="sec4-5"></a>
### 4.5 Time-Signalpfad: Time Range, External Time, Time-CV, Time Multiplier
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Der Time-Regler jeder Stage speichert **keine** direkte Sekundenzahl,
sondern eine normalisierte Position 0–1 **innerhalb** des per
`STAGE_TRANGE_PARAM` gewählten Bereichs (TR1–TR4). Auflösung in
`Marf248::process()` (nicht in `Afg`, da `Afg` nur die fertige
Sekundenzeit kennt):

1. **Time Range** (`getTimeRangeIndex()`) bestimmt `[timeRangeMin,
   timeRangeMax]` aus `TIME_RANGE_MIN[]`/`TIME_RANGE_MAX[]` (TR1:
   0,002–0,03s … TR4: 2–30s; TR3 = 0,2–3s ist Default ohne gesetztes Bit).
2. **External Time** (`MOD_TIME_EXT`): die (normalisierte) Regler-Position
   dient als Kanalwahl A–D (4 gleich große Zonen). Fehlt am gewählten
   Kanal ein Patchkabel, fällt die Zeit auf den schnellsten Wert des
   Bereichs zurück (Manual-Vorgabe).
3. **Internal Time**: `resolvedTime[i] = timeRangeMin + stageTime[i] *
   timeRangeSpan`.
4. **Additive Stage-Time-CV** (CV-Expander, optional): wird zur
   aufgelösten Zeit addiert, skaliert relativ zur Spanne des jeweils
   gewählten Time Range (siehe [Abschnitt 7](#sec7)).
5. Clamp auf `[0,001s, 30s]`.
6. **Tempo/Time Multiplier** (pro Section, V/Oct-BPM-Steuerung +
   BEATS_FACTOR-Rückrechnung, moduliert per CV + Attenuverter): skaliert die
   bereits aufgelöste Zeit **zusätzlich**, getrennt für Section A
   (`resolvedTimeA`) und B (`resolvedTimeB`), da diese Werte nicht
   modulweit geteilt sind. **Breaking Change** ggü. der ursprünglichen
   0,5×–4,0×-Faktor-Semantik – volle Herleitung in [4.12](#sec4-12).

<a id="sec4-6"></a>
### 4.6 Sustain / Enable / Stop
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Die drei Halte-Modifier pausieren `elapsed` (der Vergleich `elapsed +=
sampleTime` und `elapsed >= stageTime` liegen beide innerhalb derselben
Bedingung, die auch `!continuousActive && !advanceModeActive` verlangt):

- **Stop**: pausiert bedingungslos.
- **Sustain**: pausiert, solange am Start-Eingang ein High-Pegel (Gate,
  Schwelle `GATE_THRESHOLD = 2V`, deckt sowohl 10V-Trigger als auch
  5V-Hold-Pegel ab) anliegt. Release-Verhalten wie eine ADSR.
- **Enable**: pausiert, solange **kein** High-Pegel anliegt – Freigabe
  erst durch ein Gate.

Nach Ende der Haltung läuft `elapsed` exakt ab dem eingefrorenen Wert
weiter – die Haltezeit selbst zählt nicht zur Stage Time, die real
verstrichene Zeit bis zum nächsten Stage-Wechsel ist entsprechend länger.
Beim Ende einer Sustain/Enable-Haltung (Übergang `statusHeld` true→false,
ohne dass `advanceStage()`/`jumpToStage()` aufgerufen wird, da
`currentStage` unverändert bleibt) feuert zusätzlich `regateGen` – aus
Sicht eines nachgeschalteten Envelope-Generators „startet" die Stage
dadurch trotzdem neu.

Sind Sustain **und** Enable gleichzeitig auf derselben Stage gesetzt, gilt
ohne gepatchtes Gate effektiv Enable (hält), da beide Bedingungen
unabhängig ausgewertet und per OR in `statusHeld` zusammengeführt werden –
Sustain trägt in diesem Fall nichts zum Halten bei, verhindert es aber
auch nicht. **Siehe unbedingt [Abschnitt 10](#sec10) für das
kontraintuitive Verhalten von Sustain/Enable an einem unverbundenen
Start-Eingang.**

STOP/SUSTAIN/ENABLE haben zudem Vorrang vor Clock-quantisiertem Advance
(siehe [4.10](#sec4-10)): ein Trigger, der während einer Haltung eintrifft,
wird schlicht nicht ausgewertet – keine Warteschlange, kein nachträgliches
Nachholen nach Ende der Haltung.

<a id="sec4-7"></a>
### 4.7 Pulse-Ausgänge: APOUT, P1/P2OUT, Start-/End-of-Cycle
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

- **APOUT** (`allPulseGen`): feuert bei **jedem** Stage-Eintritt (auch bei
  Wiedereintritt ohne Indexwechsel), 1ms-Trigger.
- **P1OUT/P2OUT**: feuern, wenn die aktuelle Stage `MOD_PULSE1`/`MOD_PULSE2`
  gesetzt hat. Erkennung per **kontinuierlicher Flankenerkennung** (nicht
  nur einmalig beim Eintritt): `resetPulseEdgeState()` setzt beim
  (Wieder-)Eintritt die Vorwerte (`pulse1PrevActive` usw.) auf `false`
  zurück; das eigentliche Feuern passiert danach **jeden Sample** in
  `Afg::process()` per Flankenvergleich (`bit-aktiv && !prevActive`). Damit
  wird auch ein extern per CV (Trigger-Expander) gesetztes Gate erkannt,
  das erst mitten im Stage-Aufenthalt high wird, statt bis zum nächsten
  Stage-Wechsel verpasst zu werden; mehrfaches Gate-Toggeln während einer
  Stage (z.B. externer Clock) löst jede steigende Flanke erneut aus
  (bewusste Design-Entscheidung: freier Clock-Betrieb statt „nur einmal
  pro Besuch").
- **Start-/End-of-Cycle** (nur am Trigger-Expander verfügbar): analoges
  Prinzip für `MOD_FIRST`/`MOD_LAST`, unabhängig von Laufrichtung. Auch
  bei Pendulum-Wendepunkt-Wiederholung (Stage wird bewusst zweimal
  angefahren) löst jede Landung einen eigenen Puls aus.
- **`MOD_MUTE`**: unterdrückt `p1Gen`/`p2Gen` **und** `allPulseGen`
  (also P1OUT, P2OUT und APOUT) für die betreffende Stage vollständig,
  ohne `MOD_PULSE1`/`MOD_PULSE2` selbst zu verändern – die Programmierung
  bleibt erhalten und greift sofort wieder, sobald Mute aufgehoben wird.
  Umgesetzt als Guard direkt vor den `allPulseGen.trigger(1e-3f)`-Aufrufen
  in `jumpToStage()`/`advanceStage()` sowie in der `pulse1Active`/
  `pulse2Active`-Berechnung in `Afg::process()`. Bewusst auch APOUT
  einbezogen (statt nur P1/P2), da sonst beide Pulse-Bits einzeln
  deaktiviert werden müssten, um eine Stage wirklich komplett stumm zu
  bekommen. Voltage-/Slew-Ausgabe der Stage bleibt davon **vollständig
  unbeeinflusst** – reiner Eingriff in die Trigger-Erzeugung, kein
  Eingriff in `applyRange()`/`applySlewCurve()`. `MOD_MUTE` ist wie alle
  anderen Modifier-Bits **modulweit pro Stage**, nicht pro Section A/B.
  Aktuell nur per `STAGE_MUTE_POLY_INPUT` am Trigger-Expander setzbar
  (siehe [6](#sec6)) – kein Panel-Button/Light am Hauptmodul, das folgt
  erst mit der separat geplanten Panel-Relayout-Session.

<a id="sec4-8"></a>
### 4.8 Preset-System (Pickup/Catch)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Ein Preset-Slot (`Marf248::PresetSlot`) speichert die vollständige
Programmierung aller 16 Stages: Voltage-, Time-, Range- und
Curve-Reglerwerte (roh, nicht aufgelöst) sowie alle Modifier-Bits.

- `savePreset(slot)`: kopiert die aktuellen Regler-Rohwerte + Modifier in
  den Slot, markiert ihn als `used`.
- `loadPreset(slot)`: Modifier-Bits werden **sofort** übernommen
  (rein digitale Zustände). Die vier kontinuierlichen Regler werden
  **nicht** direkt gesetzt – das würde den physischen Reglerwert
  „verspringen" lassen. Stattdessen wird pro Regler **Pickup/Catch**
  scharf gemacht (`PickupState::arm()`).

**`PickupState`** (in `Marf248.hpp`): hält einen ausstehenden Zielwert
getrennt vom aktuellen Reglerwert. `resolve(currentRaw)` liefert jeden
Frame entweder den gespeicherten Zielwert (solange Pickup aussteht) oder
den physischen Reglerwert direkt. Die Überkreuzung wird durch
Vorzeichenwechsel der Differenz „Reglerwert − Zielwert" zwischen zwei
aufeinanderfolgenden Frames erkannt (deckt auch den Fall ab, dass der
Regler beim Laden bereits zufällig auf dem Zielwert steht → sofortiges
Pickup).

**Wichtig zur Abgrenzung vom Adress-Speicher (siehe [4.9](#sec4-9)):** Ein
Preset speichert die **komplette** Stage-Programmierung; der
Adress-Speicher merkt sich **nur** einen Stage-Index als Sprungziel. Beide
Systeme sind unabhängig voneinander nutzbar.

<a id="sec4-9"></a>
### 4.9 Adress-Speicher (Address Memory)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Neues Feature (Trigger-Expander, Reihe 12): ein reines Sprung-Lesezeichen-
System für die Stage-Address-Funktion, unabhängig vom Preset-System. 16
Slots **pro Section** (`AddrMemorySlot addrMemA[NUM_STAGES]`,
`addrMemB[NUM_STAGES]`, analog zu `NUM_STAGES`), jeder Slot speichert
lediglich `{bool used; int stage;}` – **keine** Voltage/Time/Range/Curve/
Modifier-Werte wie ein Preset.

- **Store** (`ADDR_STORE_A/B_POLY_INPUT`, poly 16 Kanäle, Kanal i = Slot
  i): steigende Flanke schreibt sofort die aktuell adressierte Stage
  (`afgA.currentStage`/`afgB.currentStage`) in Slot i und markiert ihn als
  `used` – **kein Arm-Mechanismus**, identisches Prinzip wie
  `PRESET_LOAD_POLY_INPUT`.
- **Recall** (`ADDR_RECALL_A/B_POLY_INPUT`, poly 16 Kanäle): steigende
  Flanke ruft `afgA/B.jumpToStage(addrMemA/B[i].stage, stageModifier)`
  auf, **aber nur, wenn `used` gesetzt ist** – ein nie beschriebener Slot
  bleibt wirkungslos (analog zum Leer-Slot-Schutz beim Preset-System).

Store/Recall werden **nach** `afgA.process()`/`afgB.process()` im selben
Frame verarbeitet, wirken also erst im nächsten Sample-Frame, haben dann
aber Vorrang vor Continuous/Strobe/Int-Ext für diesen Frame – bleibt
Continuous aktiv, überschreibt es den Sprung im übernächsten Frame
automatisch wieder (vorhersehbares Verhalten, kein Bug).

`performGlobalClear()` lässt den Adress-Speicher unangetastet (wie
Presets); nur `onReset()` (VCV-Rack „Initialize") leert beide Arrays.
JSON-Persistenz: pro Section ein Array mit 16 `{used, stage}`-Objekten,
immer vollständig geschrieben.

**Store/Recall besitzen keine Panel-Taster zum Anklicken** – der
Trigger-Expander ist bewusst ein reiner Jack-Träger ohne eigene
Bedienelemente; es wird zwingend ein echtes Kabel von einer Trigger-/
Gate-Quelle benötigt (siehe [Fallstricke](#sec10)).

<a id="sec4-10"></a>
### 4.10 Clock-quantisiertes Advance
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Neues Feature (Trigger-Expander, `ADVANCE_A/B_INPUT`, reiner
Flanken-Trigger-Eingang, kein Pegel wie Run-Gate): sobald gepatcht,
übernimmt `advanceModeActive` für die betreffende Section vollständig die
Kontrolle über den Stage-Wechsel, **unabhängig von** `running`/
`effectiveRunning` – Start/Stop-Taster und Run-Gate werden bewusst
ignoriert, solange das Kabel steckt (analog zu `continuousActive`).

Verhalten im Detail:
- `elapsed` läuft weiter (gecappt auf `stageTime[currentStage]`), sodass
  Slope/TOUT so aussehen, als wäre die Stage-Zeit regulär abgelaufen – die
  Stage verharrt am Zielwert/bei TOUT=0, bis der nächste Trigger kommt.
- Der eigentliche Stage-Wechsel hängt **nicht** an diesem Zeitwert, sondern
  ausschließlich am Trigger: Kommt der Trigger **vor** Ablauf der
  Stage-Zeit, wird sofort gewechselt – der Trigger hat immer Vorrang vor
  der Zeit.
- Skip bleibt sofort wirksam, unabhängig vom Advance-Modus.
- STOP/SUSTAIN/ENABLE haben weiterhin Vorrang (siehe [4.6](#sec4-6)): ein
  Trigger während einer Haltung wird ignoriert, keine Warteschlange.
- **Gegenseitig ausschließend zu Continuous**: bei aktivem Continuous
  bleibt der Advance-Zweig inaktiv, da Continuous die Stage ohnehin direkt
  adressiert.
- Die dreifarbige RUN/STOP-Status-LED zeigt bei aktivem Advance-Modus
  bewusst **Grün** (läuft) statt Rot, im Unterschied zu Continuous, das
  weiterhin als Rot (Stop) angezeigt wird – da `advanceModeActive` selbst
  nichts an `effectiveRunning` ändert, wird dies separat ausgewertet.

Panel-technisch sitzen `ADVANCE_A_INPUT`/`ADVANCE_B_INPUT` auf dem
Trigger-Expander in der bestehenden Run-Gate-Reihe (Spalte C/D), da dort
noch Platz frei war (siehe [Abschnitt 6](#sec6)).

<a id="sec4-11"></a>
### 4.11 Scale/Key-Quantisierung
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Global (modulweit) gültig für alle Stages mit aktivem `MOD_QUANT_OCT`,
beide Sections: `currentKey` (0=C…11=B) und `currentScale` (0=Major,
1=Minor, 2=Chromatic, Default). `quantizeToScale()`:

- **Chromatic**: identisch zum reinen Halbton-Raster
  (`quantizeToSemitone()`), Key wirkungslos.
- **Major/Minor**: sucht im Fenster ±12 Halbtöne um den naiv gerundeten
  Zielwert das nächstgelegene Mitglied der 7-Ton-Skala
  (`SCALE_MAJOR`/`SCALE_MINOR`) relativ zum gewählten Key.

Bedienung: Scale/Key-Taster "scharf schalten" (Arm), danach wählt der
nächste Druck auf einen der 12 Preset-Taster den Wert (siehe [3.4](#sec3-4)).

<a id="sec4-12"></a>
### 4.12 Tempo/BPM-Steuerung (V/Oct, BEATS_FACTOR)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Breaking Change**: `TIME_MULT_A/B_PARAM` speicherte ursprünglich einen
direkten Zeitfaktor (0,5×–4,0×). Jetzt speichert derselbe Parameter-Slot
eine **V/Oct-Tempo-Spannung** (−5…+5V) – bestehende Patches/Presets
interpretieren den gespeicherten Rohwert dadurch komplett anders (alte 2,0
bedeutete 2,0× Zeitfaktor, jetzt 2 Volt → 480 BPM). Tempo-Knob in alten
Patches muss neu eingestellt werden.

**Grundformel** (`tempoVoltsToBpm()`, Marf248.hpp):

```
BPM = 120 × 2^Volt
```

0V = 120 BPM (`TEMPO_BASE_BPM`), exponentiell nach 1V/Oct-Konvention (1V =
eine Oktave = Tempo-Verdopplung). CV wird **direkt additiv** in Volt zum
Knob-Wert addiert (kein "volle Spanne bei X Volt"-Konzept wie beim alten
Time Multiplier), der Attenuverter skaliert nur die CV-Amplitude:

```
totalVolts = Knob + Attenuverter × CV
```

`totalVolts` wird auf ±8V geklemmt (`TEMPO_VOLT_SAFETY_CLAMP`) – reine
numerische Absicherung gegen absurde BPM-Werte bei Kabel-Fehlanschluss,
keine musikalische Grenze.

**BEATS_FACTOR-Rückrechnung**: `BEATS_FACTOR_A/B_PARAM` (+ CV, additiv,
±5V deckt die volle Parameterspanne 0,1–64 ab) legt fest, wie viele Beats
ein voller Zyklus über alle **nicht geskippten** Stages darstellen soll.
Daraus wird der tatsächlich auf `resolvedTime[]` wirkende Multiplier
zurückgerechnet:

```
baseCycleTime = Σ resolvedTime[i]  (nur Stages ohne MOD_SKIP)
desiredCycleTime = (60 / BPM) × BEATS_FACTOR
multiplier = desiredCycleTime / baseCycleTime
```

`multiplier` wird zusätzlich auf `[1e-4, 1e4]` geklemmt (reiner
Rechenschutz, z.B. gegen Division durch 0 falls alle Stages geskippt sind).

**Pendulum-Verhalten (Option B, bewusste Entscheidung)**: BEATS_FACTOR
bezieht sich **immer nur auf einen Durchlauf** (Hinweg über alle nicht
geskippten Stages), unabhängig davon, ob Pendulum aktiv ist. Bei aktivem
Pendulum dauert ein voller Hin-und-Rück-Umlauf dadurch automatisch
2×BEATS_FACTOR Beats – es gibt **keine** automatische Verdopplung/
Halbierung im Code. Wer einen festen Umlaufwert will, stellt BEATS_FACTOR
entsprechend selbst halbiert ein.

**Live-BPM-Anzeige** (`TempoBpmDisplay`, Jacks-Spalte): reiner NVG-Text-
Readout ohne eigene Interaktion, zeigt das tatsächlich wirksame Tempo
(inkl. CV) pro Section. Aktualisiert dekimiert im `stageButtonDivider`-
Rhythmus (nicht audio-rate) – die eigentliche Multiplier-Berechnung selbst
läuft weiterhin jeden Sample, da sie direkt `resolvedTimeA/B` speist. Same
Amber-Farbe/Outline-Technik wie `TrangeButton` (Time Range), für optische
Konsistenz.

**Rechtsklick-Zahleneingabe** (`TempoQuantity`): `getDisplayValue()`/
`setDisplayValue()` sind auf BPM (nicht Volt) gemappt, mit log2-
Rücktransformation (`Volt = log2(BPM / 120)`) – Eingabe und Tooltip-Anzeige
sind dasselbe Feld. **Wichtiger, während der Entwicklung gefundener Bug**:
eine frühere Fassung überschrieb nur `getDisplayValueString()` (Tooltip-
Text), nicht aber `getDisplayValue()`/`setDisplayValue()` – dadurch griff
die Rechtsklick-Eingabe auf den rohen Volt-Wert zu, während der Tooltip BPM
zeigte. Eine eingegebene "120" landete dadurch als 120 Volt, sofort auf den
Anschlag (5V) geklemmt. Gefixt durch expliziten Override beider Methoden.

**Praxis-Richtwert**: BEATS_FACTOR=4 (Default) bedeutet 1 Stage = 1
Sechzehntelnote – bei echten 120 BPM ergibt das 2s Zykluszeit über alle 16
Stages, für den eher gemächlichen MARF-Funktionsgenerator-Charakter oft zu
hektisch. BEATS_FACTOR=8 (1 Stage = 1 Achtelnote, 4s/Zyklus bei 120 BPM)
oder 16 (1 Stage = 1 Viertelnote/Beat, 8s/Zyklus) passen erfahrungsgemäß
besser zu klassischen Cookbook-Patches.

---

<a id="sec5"></a>
## 5. Marf248 – Funktionsreferenz (C++)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

### `Marf248.hpp`

**`applySlewCurve(t, mod, power)`**<br>
Wendet die gewählte Slew-Kurvenform (Exp/Log/Linear) auf die normalisierte Phase `t` an, inkl. optionaler Normalisierung.

**`slewExpNormalized(t, power)`**<br>
Hilfsfunktion für `MOD_SLEW_NORMALIZE`: staucht/streckt die Zeitachse so, dass die Kurve exakt bei `t=1` die Schwelle erreicht.

**`quantizeToSemitone(v)`**<br>
Snappt eine Spannung auf das nächste 1V/Okt-Halbton-Raster.

**`quantizeToScale(v, key, scaleMode)`**<br>
Quantisiert auf Major/Minor/Chromatic-Skala + Key.

**`applyRange(raw, mod)`**<br>
Wendet Full/Half/Limited-Range-Skalierung inkl. Oktav-Offset an.

**`voltageToNoteName(v)`**<br>
Wandelt 1V/Okt-Spannung in Notennamen (Eurorack-Konvention, 0V=C4) um.

**`getTimeRangeIndex(mod)`**<br>
Löst die exklusiven `MOD_TRANGE_*`-Bits zu einem Index 0–3 auf (TR3 = Default).

**`getRangeOffsetIndex(mod)` / `setRangeOffsetIndex(mod, idx)`**<br>
Lesen/Schreiben des 3-Bit-Oktav-Offset-Felds im Limited-Range-Modus.

**`tempoVoltsToBpm(volts)`**<br>
**Neu**: rechnet eine V/Oct-Tempo-Spannung nach `BPM = 120 × 2^Volt` in eine BPM-Zahl um, siehe [4.12](#sec4-12).

**`PickupState::arm(target, currentRaw)`**<br>
Aktiviert Pickup für einen neuen Zielwert (Preset-Load).

**`PickupState::resolve(currentRaw)`**<br>
Liefert pro Frame den effektiv zu nutzenden Wert (Ziel oder physischer Reglerwert), erkennt Überkreuzung.

**`Afg::jumpToStage(stage, mod)`**<br>
Sofortiger Sprung auf eine konkrete Stage (Reset/Strobe/Continuous/Adress-Speicher-Recall), Rampen-Start bleibt der zuletzt ausgegebene Wert.

**`Afg::applyStageAddress(continuous, addr, strobe, reset, mod)`**<br>
Verarbeitet Reset/Continuous/Strobe-Adressierung, siehe [4.3](#sec4-3).

**`Afg::findLowestFirst(mod)`**<br>
**Neu**: findet die `MOD_FIRST`-Stage mit dem niedrigsten Index (unabhängig von `currentStage`) – für Reset, damit bei mehreren First/Last-Segmenten immer das allererste Segment angefahren wird.

**`Afg::findFirstBefore(mod, idx)` / `Afg::findLastAfter(mod, idx)`**<br>
Ring-Suche nach der nächsten `MOD_FIRST`- bzw. `MOD_LAST`-Stage für den automatischen Wrap-Around beim Forward-/Reverse-Ablauf.

**`Afg::advanceStage(mod)`**<br>
Kernlogik für den nächsten Stage-Schritt (Forward/Reverse/Pendulum, Skip-Behandlung), siehe [4.2](#sec4-2).

**`Afg::resetPulseEdgeState()`**<br>
Setzt die Flanken-Vorwerte für P1/P2/First/Last zurück (bei jedem Stage-(Wieder-)Eintritt).

**`Afg::process(...)`**<br>
Hauptverarbeitung einer Section pro Sample: Start/Stop, Run-Gate, Advance, Sustain/Enable/Stop-Logik, automatischer Zeitablauf, Voltage-/Zeit-Ausgabe, alle Puls-Ausgänge. Nimmt die bereits aufgelösten Stage-Arrays (Spannung, Zeit, Modifier, Curve-Power) sowie Key/Scale und Start/Stop/Run-Gate/Advance-Eingaben entgegen.

### `Marf248.cpp`

**`StageVoltageQuantity::getDisplayValueString()`**<br>
Tooltip-Text des Voltage-Reglers: Kanalname bei External, Notenname bei Quantize, sonst Volt-Zahl.

**`StageTimeQuantity::getDisplayValue()` / `setDisplayValue()`**<br>
**Neu**: rechnet den internen 0–1-Rohwert für Anzeige/Rechtsklick-Eingabe auf die Buchla-Cookbook-Dial-Konvention (0–10) um – der interne Parameterwert bleibt unverändert 0–1 (kein Breaking Change für bestehende Presets).

**`StageTimeQuantity::getDisplayValueString()`**<br>
Tooltip-Text des Time-Reglers: zeigt den 0–10-Dial-Wert führend, dahinter die exakt gegen den aktuellen Time Range aufgelöste Sekundenzahl in Klammern (z.B. `4.0 (0.132 s)`).

**`StageRangeQuantity::getDisplayValueString()`**<br>
Tooltip-Text des Range-Reglers als Klartext ("Full", "Half", "Lim +0"…"+8").

**`TempoQuantity::getDisplayValue()` / `setDisplayValue()`**<br>
**Neu**: mappt den rohen Volt-Parameterwert auf BPM (`getDisplayValue`) bzw. rechnet eine eingegebene BPM-Zahl per log2 zurück in Volt (`setDisplayValue`) – Tooltip und Rechtsklick-Eingabe nutzen dasselbe Feld, siehe [4.12](#sec4-12).

**`TempoBpmDisplay::draw()`**<br>
**Neu**: selbstzeichnendes Live-Text-Widget (kein ParamWidget), zeigt `displayBpmA/B` in Amber, gleiche Outline-Technik wie `TrangeButton`.

**`Marf248::onReset(e)`**<br>
Reagiert auf VCV-Rack „Initialize": setzt zusätzlich zu den Standard-Params auch Modifier-Bits, beide Afg-Instanzen, Mode/Pendulum, Stage Address, Presets, **Adress-Speicher** und Scale/Key vollständig zurück (Felder, die nicht als Rack-Params gespeichert sind).

**`Marf248::savePreset(slot)` / `loadPreset(slot)`**<br>
Siehe [4.8](#sec4-8).

**`Marf248::performGlobalClear()`**<br>
Setzt Modifier-Bits, Range und Curve aller Stages zurück, verwirft ausstehende Pickups, löst Blitzlicht aus. Wiederverwendet vom Panel-Taster und vom Trigger-Expander-Eingang. Lässt den Adress-Speicher unangetastet.

**`Marf248::performResetValues()`**<br>
Setzt nur Voltage/Time aller Stages auf Default zurück. Wiederverwendet vom Panel-Taster und vom Trigger-Expander-Eingang.

**`Marf248::performAutoFill()`**<br>
**Neu**: setzt die 16 geteilten Voltage-Regler auf ein lineares 0–10V-Raster und erzwingt `Range = Full` je Stage, siehe [3.4](#sec3-4).

**`Marf248::process(args)`**<br>
Zentrale Verarbeitungsfunktion, pro Sample aufgerufen. Ablauf (siehe auch Abschnitte 3–4): Trigger-Expander-Lookup (ganz am Anfang, für SEXT) → Global Clear/Reset Values/Auto-Fill → Preset-/Scale-/Key-Arm-Logik → Stage-Modifier-Klick-Buttons (dezimiert per `stageButtonDivider`) → Stage-Modifier-Poly-CV-Overrides vom Trigger-Expander (jeden Sample, ungedrosselt) → Auflösung von Voltage/Time/Curve pro Stage (Range, External-Quellen, Time Range, additive CV, Curve-Power-Cache) → **Tempo-/BEATS_FACTOR-Auflösung und Multiplier-Rückrechnung je Section (siehe [4.12](#sec4-12))** → Stage-Address-Verarbeitung je Section (inkl. Kanalwahl-CV) → `afgA.process()`/`afgB.process()`-Aufrufe (inkl. Run-Gate und Advance) → Ausgabe schreiben → dekimierte Übernahme der Live-BPM-Anzeigewerte → Trigger-Expander-Aktionen (Clear/Reset/Preset/Adress-Speicher Store/Recall/Scale/Key/Start-End-of-Cycle) → CV-Expander-Gate-Ausgabe → dreifarbige Status-LEDs.

**`Marf248::dataToJson()` / `dataFromJson(rootJ)`**<br>
Persistiert Modifier-Bits, Mode/Pendulum/Stage-Address-Zustände, Scale/Key, alle 12 Preset-Slots sowie den Adress-Speicher (siehe [Abschnitt 9](#sec9)).

**`TrangeButton::draw()`**<br>
Selbstzeichnender Time-Range-Taster (kein SVG-Asset): dunkler abgerundeter Hintergrund + Ziffer 1–4 in Amber mit dunklem Outline.

**`TrangeButton::appendContextMenu()`**<br>
Rechtsklick-Checkliste zur direkten TR1–4-Auswahl (manipuliert `stageModifier` direkt statt über den Momentary-Button-Rohwert).

**`RangeKnob::draw()`**<br>
Zeichnet zusätzlich zum Standard-`RoundBlackKnob` einen farbigen Zeiger (deckt den Original-SVG-Zeiger ab) sowie ein Textkürzel des aktuellen Zustands in der Knopfmitte.

**`RangeKnob::appendContextMenu()`**<br>
Rechtsklick-Checkliste zur direkten Range-Auswahl (Standard-Texteingabe war laut Praxistest bei diesem snap-Regler unzuverlässig).

**`PulseRingLight`**<br>
Eigenes Licht-Widget: zeichnet einen dünnen Ring um die P1/P2-Ausgangsbuchse statt eines separaten Punkts.

**`Marf248Widget::Marf248Widget(module)`**<br>
Baut das komplette 146HP-Panel auf: Rahmen, Spalten-Geometrie (inkl. VOUT/TOUT-Ausgangsspalte pro Section), alle Regler/Taster/Buchsen/Lichter für External-CV, beide Sections und die 16 Stage-Spalten sowie die Preset-Spalte (siehe [3.1](#sec3-1)).

---

<a id="sec6"></a>
## 6. Marf248TriggerExpander
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

24HP-Expander (Entwicklungsverlauf: 12HP → 18HP für die Stage-Modifier-
Poly-CVs → 24HP für Advance). Reiner Jack-Träger ohne eigene DSP-Logik –
die gesamte Verarbeitung findet in `Marf248::process()` statt, sobald das
Modul als linker oder rechter Nachbar erkannt wird (`LINK_LIGHT` zeigt den
Andock-Status).

### Eingänge

**`CLEAR_INPUT`** *(Trigger)*<br>
Löst `performGlobalClear()` aus (wie Panel-Taster).

**`RESET_VALUES_INPUT`** *(Trigger)*<br>
Löst `performResetValues()` aus.

**`PRESET_LOAD_POLY_INPUT`** *(Poly-Trigger (12 Kanäle))*<br>
Kanal p (0–11) lädt Preset p+1 **sofort**, ohne Arm-Mechanismus. Leere Slots werden ignoriert. Unbenutzte Kanäle liefern 0V (kein Trigger). **Kanäle 13–16 werden nicht ausgewertet** – die Verarbeitungsschleife läuft nur von `p=0` bis `p=11` (siehe [Fallstricke](#sec10)).

**`SCALE_MAJOR_INPUT` / `SCALE_MINOR_INPUT` / `SCALE_CHROMATIC_INPUT`** *(Trigger)*<br>
Setzt `currentScale` direkt (kein Arm nötig).

**`KEY_CV_INPUT`** *(Kontinuierliche CV, 0–10V)*<br>
Setzt `currentKey` direkt (0–11 = C–B), nur wirksam wenn tatsächlich gepatcht (`isConnected()`-Prüfung verhindert ungewollten Rückfall auf C).

**`STROBE_A_INPUT` / `STROBE_B_INPUT` ("SEXT")** *(Trigger)*<br>
Löst denselben Effekt wie der jeweilige Panel-Strobe-Taster aus (ODER-verknüpft), inklusive Continuous-Sperre.

**`RUN_GATE_A_INPUT` / `RUN_GATE_B_INPUT`** *(Pegel (Gate), Schmitt-Hysterese 0,1V/1V)*<br>
ODER-verknüpft mit dem Run-Taster-Zustand (`effectiveRunning = running || runGateHigh`); der Taster-Zustand selbst bleibt beim Zurückfallen des Gates erhalten. **Wird ignoriert, solange Advance auf derselben Section gepatcht ist.**

**`ADVANCE_A_INPUT` / `ADVANCE_B_INPUT`** *(**Neu**, reiner Flanken-Trigger)*<br>
Clock-quantisiertes Advance, siehe [4.10](#sec4-10). Sitzt in der Run-Gate-Reihe (Spalte C/D).

**`STAGE_SKIP_POLY_INPUT`, `STAGE_FIRST_POLY_INPUT`, `STAGE_LAST_POLY_INPUT`, `STAGE_STOP_POLY_INPUT`, `STAGE_SUSTAIN_POLY_INPUT`, `STAGE_ENABLE_POLY_INPUT`, `STAGE_PULSE1_POLY_INPUT`, `STAGE_PULSE2_POLY_INPUT`, `STAGE_SLOPE_POLY_INPUT`** *(Poly-Gate (16 Kanäle = Stage 1–16))*<br>
Pegel-Override (Schwelle `GATE_THRESHOLD = 2V`) für das jeweilige Modifier-Bit. Nur wirksam, solange die Buchse gepatcht ist (`isConnected()`); dann hat CV Vorrang und überschreibt einen im selben Frame verarbeiteten manuellen Klick. Ohne Kabel bleibt der zuletzt per Klick gesetzte Zustand erhalten. **Shape/Lin-Exp-Log bleibt bewusst ohne CV-Override**, da es ein echter 3-Zustands-Zyklus statt eines einfachen Bits ist.

**`STAGE_MUTE_POLY_INPUT`** *(**Neu**, Poly-Gate (16 Kanäle = Stage 1–16))*<br>
Identisches Pegel-Override-Prinzip, setzt `MOD_MUTE` statt eines der obigen Bits (siehe [4.7](#sec4-7)). Sitzt in Reihe 11, Spalte D (dort bisher frei). **Kein Panel-Button/Light am Hauptmodul** – bis zur geplanten Panel-Relayout-Session ist dies die einzige Bedienmöglichkeit für Mute.

**`ADDR_STORE_A_POLY_INPUT` / `ADDR_STORE_B_POLY_INPUT`** *(**Neu**, Poly-Trigger (16 Kanäle = Slot 1–16))*<br>
Adress-Speicher Store, siehe [4.9](#sec4-9). Sitzt in Reihe 12.

**`ADDR_RECALL_A_POLY_INPUT` / `ADDR_RECALL_B_POLY_INPUT`** *(**Neu**, Poly-Trigger (16 Kanäle = Slot 1–16))*<br>
Adress-Speicher Recall, siehe [4.9](#sec4-9). Sitzt in Reihe 12.

### Ausgänge

**`START_CYCLE_A/B_OUTPUT`**<br>
Feuert (1ms-Trigger), wenn Section A/B eine `MOD_FIRST`-markierte Stage erreicht (kontinuierliche Flankenerkennung, unabhängig von Laufrichtung).

**`END_CYCLE_A/B_OUTPUT`**<br>
Analog für `MOD_LAST`.

### Panel-Layout (24HP, 4 Spalten à 29mm Abstand, 8,8mm-Zeilenraster)

Reihe 1: Clear / Reset Values. Reihe 2: Preset Load (Poly, mittig). Reihe
3–4: Scale Major/Minor/Chromatic + Key CV. Reihe 5–6: Start-/End-of-Cycle
A und B. Reihe 7: SEXT A/B. **Reihe 8: Run-Gate A/B (Spalte A/B) + Advance
A/B (Spalte C/D, neu)** – thematisch verwandt (beide betreffen die
Lauf-Steuerung der jeweiligen Section), Spalte C/D war hier bisher frei,
daher keine eigene neue Reihe nötig. Reihe 9: Stage Skip / First / Last (3
Spalten). Reihe 10: Stage Stop / Sustain / Enable. **Reihe 11: Stage Pulse1
/ Pulse2 / Slope / Mute (Spalte D, neu)** – Spalte D war hier bisher frei,
daher auch für Mute keine eigene neue Reihe nötig. Reihe 12 (neu):
Adress-Speicher Store A / Store B /
Recall A / Recall B** (4 Spalten, gleiches Prinzip wie Reihe 8) – durch das
auf 8,8mm verdichtete Zeilenraster passt diese Reihe noch mit 6,6mm Puffer
zu den unteren Befestigungsschrauben.

### Wichtiger Implementierungshinweis: kontinuierliche statt einmalige Flankenerkennung

Ein früherer Bug: `triggerStagePulses()` wertete P1/P2/First/Last-Bits nur
**einmal** exakt im Eintritts-Sample aus. Ein extern per CV gesetztes Gate,
das erst mitten im Stage-Aufenthalt high wurde, wurde dadurch komplett
verpasst. Fix: `resetPulseEdgeState()` setzt nur noch die vier
Flanken-Vorwerte zurück; das eigentliche Feuern passiert seither
kontinuierlich **jeden Sample** in `Afg::process()` per Flankenerkennung.
Bewusst **nicht** auf `stageButtonDivider` dezimiert, da ein schnelles
externes CV-Gate zwischen zwei Divider-Ticks sonst verschluckt werden
könnte (Aliasing) – die CPU-Mehrkosten (~10 Operationen/Sample/Section)
sind vernachlässigbar.

---

<a id="sec7"></a>
## 7. Marf248CvExpander
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

12HP-Expander (ursprünglich 6HP, für die Slew-Curve-CV-Spalte erweitert).
Ebenfalls reiner Jack-Träger ohne eigene DSP-Logik. Unverändert gegenüber
dem letzten Dokumentationsstand.

### Eingänge

**`STAGE_TIME_CV_INPUT` (Poly, 16 Kanäle)**<br>
Additive CV pro Stage auf die aufgelöste Stage-Zeit, **nach** External/Internal-Zeitauflösung, **vor** dem Time-Multiplier (wird also von diesem mitskaliert). Ein geteilter Attenuverter (`TIME_CV_ATTEN_PARAM`) skaliert alle 16 Kanäle gemeinsam; ±5V CV bei vollem Attenuverter-Ausschlag deckt die komplette Spanne des jeweils aktiven Time Range (TR1–4) ab.

**`CURVE_CV_INPUT` (Poly, 16 Kanäle)**<br>
Additive CV pro Stage auf die Curve-Knob-Position (Kruemmung der Slew-Kurve), vor der Power-Berechnung aufaddiert. Eigener Attenuverter (`CURVE_CV_ATTEN_PARAM`), ±5V-Referenzspanne.

### Parameter

**`TIME_CV_ATTEN_PARAM`**<br>
Attenuverter für Stage-Time-CV (−1..+1).

**`TIME_CV_OFFSET_PARAM`**<br>
Offset-Trimmer (−10..+10V), wird **vor** dem Attenuverter von der rohen CV abgezogen – macht eine unipolare Quelle (z.B. 0–10V Envelope) bipolar nutzbar.

**`CURVE_CV_ATTEN_PARAM`**<br>
Attenuverter für Slew-Curve-CV (−1..+1).

**`CURVE_GLOBAL_PARAM`**<br>
Globaler Bias-Knob (−1..+1): wirkt **immer** auf alle 16 Stages, auch ohne gepatchtes Kabel am `CURVE_CV_INPUT` – im Unterschied zu `TIME_CV_OFFSET_PARAM`, der ohne Kabel wirkungslos bleibt.

### Ausgänge

**`GATE_A_OUTPUT` / `GATE_B_OUTPUT` (Poly, 16 Kanäle)**<br>
Kanal i ist high (10V), sobald Section A/B aktuell Stage i adressiert – unabhängig vom Run-Zustand. Optionale Retrigger-Lücke (Kontextmenü, `regateGapEnabled`, Default an): zieht das Gate der aktuell adressierten Stage kurz auf 0V (~5ms), wenn ein Wiedereintritt ohne Indexwechsel erkannt wird (Pendulum-Wendepunkt, Strobe auf dieselbe Adresse, Sustain/Enable-Freigabe), damit nachgeschaltete Hüllkurvengeneratoren zuverlässig neu triggern. **Fallstrick:** Poly-zu-Mono-Konvertierung dieser Gates erzeugt aufgrund nahtloser Kanalübergänge einen durchgehend hohen Pegel (verifiziert per Bogaudio MONO auf dem Scope) – für Trigger-Zwecke daher besser APOUT verwenden, siehe [Abschnitt 10](#sec10).

### Kontextmenü

Rechtsklick auf das Panel: „Retrigger-Lücke bei Stage-Wiedereintritt
(~5ms)" (Checkbox, `regateGapEnabled`, persistiert per JSON).

---

<a id="sec8"></a>
## 8. Expander-Mechanismus (Kommunikation zwischen den Modulen)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Beide Expander erkennen `Marf248` links **oder** rechts von sich
(`getLeftExpander()`/`getRightExpander()`, Vergleich über `getModel() ==
modelMarf248`) und zeigen dies über ein grünes `LINK_LIGHT` an. Solange
kein Marf248 andockt, setzen die Expander ihre eigenen Ausgänge selbst
jeden Frame auf 0V zurück (verhindert hängenbleibende Spannung beim
Entfernen/Verschieben).

`Marf248::process()` sucht umgekehrt jeden Frame nach einem
`Marf248TriggerExpander` bzw. `Marf248CvExpander` als direkten Nachbarn
und liest/schreibt dessen Parameter/Inputs/Outputs unmittelbar über
`getParam()`/`getInput()`/`getOutput()` – ohne Message-Passing, da die
Expander selbst keine eigene Verarbeitung besitzen. Beide Expander-Typen
können unabhängig voneinander (keiner, einer, oder beide gleichzeitig)
angedockt sein; fehlt ein Expander, fallen die zugehörigen Modulationen
einfach weg (z.B. Curve-CV/Time-CV = 0, keine Poly-Gate-Overrides, kein
Advance, kein Adress-Speicher).

Die externen CV-Eingaben werden generell mit ansonsten unbelegten Kanälen
so behandelt, dass ohne Patchkabel exakt das Verhalten ohne Expander
erhalten bleibt (0V auf nicht gepatchten Poly-Kanälen, `isConnected()`-
Prüfungen dort, wo 0V mit einem gültigen Zustand kollidieren würde, z.B.
Key CV oder die neue Stage-Address-Kanalwahl-CV).

---

<a id="sec9"></a>
## 9. Persistenz (Patch-Speicherung)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

`Marf248::dataToJson()`/`dataFromJson()` persistieren zusätzlich zu den
automatisch von VCV Rack gespeicherten Params:

- `stageModifier[]` (alle 16 Stages) als JSON-Array.
- `reverseA/B`, `pendulumOnA/B`, `addrExtA/B`, `addrContOnA/B`.
- `currentKey`, `currentScale`.
- Alle 12 Preset-Slots (`used`-Flag + Voltage/Time/Range/Curve-Arrays +
  Modifier-Array je Slot).
- **Neu:** der Adress-Speicher – pro Section ein JSON-Array mit 16
  `{used, stage}`-Objekten (`addrMemA`/`addrMemB`), immer vollständig
  geschrieben (kein Sparse-Skip wie bei Presets, Datenmenge trivial).

`Marf248CvExpander::dataToJson()`/`dataFromJson()` persistiert zusätzlich
`regateGapEnabled`.

**Breaking Change (Time-Range-Umstellung):** Der Time-Regler speichert
seit Einführung von TR1–4 eine normalisierte Position 0–1 statt direkt
Sekunden. Alte Patches interpretieren den gespeicherten Rohwert dadurch
anders (z.B. bedeutete 0,1 früher 0,1s, jetzt 10% Position im gewählten
TR-Bereich) – Time-Slider in alten Patches ggf. neu einstellen. Die
Anzeige selbst (Dial-Wert + Sekunden, siehe [5](#sec5)) hat darauf keinen
Einfluss, da nur Darstellung, nicht Speicherung betroffen ist.

---

<a id="sec10"></a>
## 10. Bekannte Fallstricke im Zusammenspiel
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Dieser Abschnitt sammelt alle in den bisherigen Entwicklungssessions
aufgetretenen Missverständnisse und kontraintuitiven, aber **korrekten**
(nicht-Bug-)Verhaltensweisen – jeweils mit Fundstelle im Code.

### 10.1 Sustain/Enable ohne gepatchtes Start-Kabel wirken gegensätzlich

`startVoltage` ist die rohe Spannung an `START_A/B_INPUT`, **ohne**
Rücksicht auf `startConnected`. Ein unverbundener Eingang liefert in Rack
0V zurück, was exakt wie ein dauerhaft LOW-Gate behandelt wird – nicht wie
„Gate-Konzept ausgeschaltet" oder „neutral". Das hat für Sustain und
Enable **gegensätzliche** Konsequenzen:

- **Sustain ohne Kabel**: hält **nie** (`startGateHigh` ist immer `false`)
  → die Stage läuft scheinbar wie eine normale Stage durch, das
  Sustain-Bit hat ohne gepatchtes Gate also gar keine sichtbare Wirkung.
- **Enable ohne Kabel**: hält **immer** (`!startGateHigh` ist immer
  `true`) → die Stage bleibt für immer stehen, auch mit beliebig vielen
  Advance-Triggern oder abgelaufener Stage-Zeit – sieht wie ein
  hängengebliebenes/kaputtes Modul aus, ist aber exakt die spezifizierte
  „wartet auf Freigabe per Gate"-Semantik.

Zum Testen/Verifizieren: ein Kabel (z.B. von einem manuellen Trigger-/
Gate-Taster-Modul) an START_A/B patchen und dessen Gate hoch/runter
schalten – erst dann zeigt sich das jeweils andere, „freigebende"
Verhalten (Sustain hält dann WÄHREND High, Enable hält dann WÄHREND Low
und gibt bei High frei). *(Code: `GATE_THRESHOLD`-Kommentarblock in
`Afg::process()`, `Marf248.hpp`.)*

### 10.2 Der Stage-Address-Knob bewegt currentStage nie von selbst

Der interne Stage-Address-Knob (`STAGE_ADDR_KNOB_A/B_PARAM`) liefert nur
einen **Wert**, keine Aktion. Erst **Strobe** (einmaliger Sprung),
**Continuous** (laufende Nachführung) oder **Reset** wenden die Adresse
tatsächlich an – ein reines Drehen am Knob (oder Ändern der externen
Adress-CV) allein bewegt `currentStage` **nicht**. Das gilt unverändert
auch für die neue Kanalwahl-CV: Der Ext-Modus liefert nur den Adresswert
über den gewählten Kanal, wendet ihn aber genau wie der interne Knob nicht
automatisch an. Für einen Live-Test der 4-Kanal-Kanalwahl braucht es daher
zusätzlich **Continuous**, damit die Section der gewählten Quelle
tatsächlich kontinuierlich folgt.

### 10.3 Adress-Speicher Store/Recall haben keine Panel-Taster

`ADDR_STORE/RECALL_A/B_POLY_INPUT` sind reine CV-/Trigger-Buchsen auf dem
Trigger-Expander **ohne** zugehörigen Panel-Taster zum Anklicken. Store
und Recall funktionieren ausschließlich über ein tatsächlich gepatchtes
Kabel von einer Trigger-/Gate-Quelle – ein Mausklick ist hier bewusst
nicht vorgesehen, da der Trigger-Expander konzeptionell ein reiner
Jack-Träger ohne eigene Bedienelemente ist.

### 10.4 Kanal-Nummer bei Store/Recall = Slot, nicht Stage

Die Kanal-Nummer (1–16) am `ADDR_STORE`/`ADDR_RECALL`-Poly-Input bestimmt
**ausschließlich** die Slot-Nummer (das "Ablagefach"), hat aber **nichts**
mit der gerade adressierten Stage-Nummer zu tun. Beispiel: Stage 12 kann
bewusst in Slot 1 abgelegt werden – Kanal und Stage sind komplett
unabhängige Zahlen. Ein Mono-Kabel in einen dieser Poly-Jacks landet
automatisch auf Kanal 1 (= Slot 1).

### 10.5 Preset-Load-Poly: Kanäle 13–16 werden ignoriert

Die Verarbeitungsschleife für `PRESET_LOAD_POLY_INPUT` läuft nur von
`p = 0` bis `p = 11` (12 Presets). Ein Trigger auf Kanal 13–16 wird
**nie** ausgewertet (`getPolyVoltage()` für diese Kanäle wird gar nicht
erst abgefragt) – kein Fehlverhalten, sondern schlicht außerhalb des
gültigen Bereichs. Das `configInput`-Tooltip-Text ("poly, ch. 1-12 =
preset 1-12") beschreibt das korrekt.

### 10.6 Continuous + Advance + Timer-Auto-Advance schließen sich gegenseitig aus

Pro Section gibt es drei Mechanismen, die `currentStage` verändern können:
den normalen Timer-Auto-Advance, Continuous-Adressierung und
Clock-quantisiertes Advance. Alle drei sind im Code über gegenseitige
`!continuousActive`/`!advanceModeActive`-Guards explizit voneinander
entkoppelt: Continuous hat immer Vorrang (Timer und Advance bleiben
inaktiv, solange Continuous läuft), Advance ignoriert dafür Start/Stop und
Run-Gate komplett, solange die Advance-Buchse gepatcht ist. Wer versucht,
z.B. Run-Gate und Advance gleichzeitig zur Steuerung derselben Section zu
nutzen, wird feststellen, dass Advance immer gewinnt.

### 10.7 CV-Expander-Gates: Poly-zu-Mono-Konvertierung liefert Dauerpegel

Die polyphonen Gate-Ausgänge (`GATE_A/B_OUTPUT`) des CV-Expanders sind
so konzipiert, dass genau ein Kanal (die aktuell adressierte Stage) high
ist. Bei der Konvertierung auf ein Mono-Signal (z.B. via Bogaudio MONO)
entsteht durch die nahtlosen Kanalübergänge zwischen den 16 Kanälen aber
ein **durchgehend hoher Pegel** statt einzelner Trigger-Impulse (per Scope
verifiziert) – für reine Trigger-Zwecke ist stattdessen **APOUT** die
richtige Wahl, das bei jedem Stage-Wechsel einen echten 1ms-Impuls liefert.

### 10.8 Start/Stop wirkt nicht während Continuous-Adressierung

`applyStageAddress()` erzwingt `running = false` im Continuous-Zweig,
unabhängig vom Zustand des Start/Stop-Tasters oder -Pulses. Ein während
aktiver Continuous-Adressierung gedrückter Start-Taster hat daher
scheinbar keine Wirkung – das ist beabsichtigt (Continuous adressiert die
Stage direkt, ein Timer-Konzept ist dort nicht anwendbar), nicht defekt.
Der Zustand `addrContOnA/B` wird zudem per JSON persistiert: ein Preset,
das mit aktivem Continuous gespeichert wurde, reaktiviert Continuous beim
Laden automatisch.

### 10.9 Mehrere First/Last-Segmente: Reset springt immer zum ersten Segment

Bei mehreren First/Last-Paaren im 16-Stage-Feld springt Reset **immer**
zur `MOD_FIRST`-Stage mit dem niedrigsten Index (`findLowestFirst()`),
unabhängig davon, in welchem Segment sich `currentStage` gerade befindet.
Das unterscheidet sich bewusst von der Wrap-Logik des automatischen
Ablaufs (`findFirstBefore()`), die stattdessen lokal zum nächstgelegenen
First-Punkt wrappt. Wer zwischen Segmenten wechseln will, muss das aktiv
über externe Stage-Address-CV kombiniert mit Strobe oder Continuous tun –
ein einfacher Reset-Druck bringt immer zum allerersten Segment zurück.

### 10.10 Global Clear löschte First/Last-Bits dauerhaft (behoben, August 2026)

**Bug, nicht Design** – im Gegensatz zu den übrigen Punkten in diesem
Abschnitt. `performGlobalClear()` setzte `stageModifier[i] = 0` für alle
16 Stages, ohne anschließend wie `onReset()`/der Konstruktor die
Werkszustand-Bits (Stage 1 = `MOD_FIRST`, Stage 16 = `MOD_LAST`) wieder zu
setzen. Da `stageModifier[]` modulweit geteilt ist (ein Array, nicht pro
Section A/B getrennt), waren nach einem Druck auf Global Clear **beide**
Sections betroffen.

**Symptome, die das ausgelöst hat:**
- Pendulum blieb ohne jeden Wendepunkt an Stage 16 hängen
  (`rack::clamp`-Obergrenze in `advanceStage()`) – der Turnaround hängt
  ausschließlich an `wasFirst`/`wasLast`, anders als Forward/Reverse, die
  zusätzlich einen Array-Grenzen-Fallback (`next >= NUM_STAGES`) haben und
  dadurch trotz fehlender Bits weiter korrekt wrappten. Das hat den Bug
  lange maskiert: Forward/Reverse fühlten sich nach Global Clear normal
  an, nur Pendulum zeigte sichtbar kaputtes Verhalten.
- Start-/End-of-Cycle-Ausgänge am Trigger-Expander blieben nach Global
  Clear ebenfalls stumm (gleiche Bit-Abhängigkeit, siehe [4.7](#sec4-7)).

**Fix:** `performGlobalClear()` setzt `stageModifier[0] |= MOD_FIRST` und
`stageModifier[NUM_STAGES-1] |= MOD_LAST` nach dem Löschen wieder – analog
zu `onReset()`. Ein VCV-„Initialize" (Rechtsklick → Initialize) hat den
Zustand schon vor dem Fix repariert, da `onReset()` die Bits korrekt neu
gesetzt hat – nur der Panel-/CV-Taster für Global Clear selbst war
betroffen.

### 10.11 Mute wirkt modulweit pro Stage, hat kein Panel-Feedback und keinen eigenen Reset

Drei Eigenheiten von `MOD_MUTE`, die beim ersten Test leicht zu Verwirrung
führen:

- **Modulweit pro Stage, nicht pro Section A/B** – wie alle anderen
  Modifier-Bits teilen sich Section A und B dasselbe `stageModifier[]`.
  Ein High-Gate an `STAGE_MUTE_POLY_INPUT` Kanal i mutet Stage i für
  **beide** Sections gleichzeitig, sobald sie diese Stage erreichen.
- **Kein Panel-Feedback** – bis zur geplanten Panel-Relayout-Session gibt
  es weder Button noch Light am Hauptmodul. Der Mute-Zustand einer Stage
  ist ausschließlich am CV-Pegel des Trigger-Expanders ablesbar.
- **Kein eigener Reset-Mechanismus** – wird das Kabel abgezogen, während
  der Kanal high war, bleibt die Stage dauerhaft gemutet (gleiches
  Rückfallverhalten wie bei allen anderen Poly-Overrides, siehe
  [6](#sec6)). Einzige Möglichkeit, ein so hängendes Mute aufzuheben, ist
  **Global Clear** (`performGlobalClear()` setzt `stageModifier[i] = 0`
  für alle Stages).

---

<a id="sec11"></a>
## 11. Design-Entscheidungen & Abgrenzungen
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

- **Skip, Normalize, Reset Values, Auto-Fill, Global-Clear-Blitzlicht**:
  reine Software-Erweiterungen ohne direktes Hardware-Vorbild.
- **Shape (Lin/Exp/Log-Zyklus) bleibt ohne CV-Override**, da echter
  3-Zustands-Zyklus (im Unterschied zu allen anderen, per Poly-CV
  überschreibbaren Ein/Aus-Bits inkl. Slope).
- **`Afg` kennt keine Inputs/Ports direkt** – External-Voltage/Time-Quellen
  und die Stage-Address-Kanalwahl werden zentral in `Marf248::process()`
  aufgelöst, `Afg` bekommt nur die bereits fertig aufgelösten Werte
  übergeben. Das hält die Kernlogik unabhängig vom modulweiten I/O-Layout.
- **Stage-Address-CV und per-Stage-External-Voltage sind unabhängige
  Mechanismen** – Adress-Eingänge liegen bewusst am Hauptmodul und nicht
  am Expander, damit die Kernfunktion nicht von einem optionalen Modul
  abhängt.
- **Adress-Speicher ist kein Preset-Ersatz** – er speichert bewusst nur
  einen Stage-Index als Sprungziel, keine vollständige Programmierung
  (klare Trennung der Konzepte, siehe [4.8](#sec4-8)/[4.9](#sec4-9)).
- **Auto-Fill nur global, nicht pro Section** – da `STAGE_VOLTAGE_PARAM`
  und `STAGE_RANGE_PARAM` modulweit zwischen Section A und B geteilt sind,
  wäre eine getrennte A/B-Variante nur mit größerem strukturellem Umbau
  möglich.
- **APOUT ist reiner Trigger, kein Gate** – für Hüllkurven eignet sich ein
  AD-Envelope besser als ein ADSR.
- **ART (CV+Gate-Kombiausgang) und Autotune** (aus dem Python-Emulator)
  sind explizit **nicht** Teil des C++-Ports. **Davon zu unterscheiden:
  APOUT** (`APOUT_A/B_OUTPUT`, "All Pulses") **existiert sehr wohl** im
  C++-Port – ein einfacher Trigger-Ausgang, der bei jedem Stage-Eintritt
  feuert (siehe [4.7](#sec4-7)). Eine ältere, missverständliche Notiz zu
  diesem Thema wurde korrigiert.
- **Mute unterdrückt bewusst auch APOUT, nicht nur P1/P2** – sonst müsste
  man beide Pulse-Bits einzeln deaktivieren, um eine Stage wirklich
  komplett stumm zu bekommen (siehe [4.7](#sec4-7)).
- **Mute ist modulweit pro Stage, nicht pro Section A/B** – wie alle
  anderen Modifier-Bits auch, da `stageModifier[]` zwischen Section A und
  B geteilt ist.
- **Mute hat aktuell keinen Panel-Button** – nur per CV am Trigger-Expander
  steuerbar; ein Panel-Button/Light folgt erst mit der separat geplanten
  Panel-Relayout-Session (breitere 4-Spalten-Button-Reihen). Wichtig dabei:
  VCV-Modulhöhe ist fix (`RACK_GRID_HEIGHT`), ein Relayout kann das Modul
  also nur breiter, nie niedriger machen.
- **TR3 ist der implizite Default** (kein Bit gesetzt), nach demselben
  Muster wie Range/Shape/Slope-Defaults.
- **Gate-Schwelle einheitlich 2V** (nicht die manualspezifizierten 5V für
  Enable) – bewusste Projektentscheidung, ggf. später überprüfbar.
- **Kein SIMD**: durch fehlende Polyphonie und stark verzweigten
  Pro-Stage-Code aktuell nicht sinnvoll anwendbar; natürlicher Auslöser
  wäre eine künftige Polyphonie-Erweiterung.
- **Advance und Continuous sind bewusst gegenseitig exklusiv** – beide
  Mechanismen adressieren die Stage auf konzeptionell unterschiedliche Art
  (Trigger-basiert vs. direkt-adressierend), eine Kombination wäre
  semantisch widersprüchlich.
- **Tempo-Knob ist V/Oct statt linearer Faktor** (Breaking Change, siehe
  [4.12](#sec4-12)) – folgt der Eurorack-Standardkonvention statt einer
  eigenen Skala, macht das Modul kompatibel zu externen V/Oct-Pitch-/
  Tempo-Quellen.
- **BEATS_FACTOR bezieht sich bei Pendulum bewusst nicht automatisch auf
  den vollen Hin-und-Rück-Umlauf** (Option B, siehe [4.12](#sec4-12)) – ein
  Parameter behält eine feste, vorhersehbare Bedeutung statt eines
  versteckten Pendulum-Sonderfalls im Code.
- **Multiplier-Sicherheitsklammern (`[1e-4, 1e4]`) sind reiner
  Rechenschutz, keine musikalische Grenze** – analog zur bestehenden
  CV-Expander-Zeit-Klammer (`[0,001s, 30s]`).
