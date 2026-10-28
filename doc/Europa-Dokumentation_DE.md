# Kinovice reminiscence of the unattained – Europa (Technische Dokumentation)

**Europa** ist das fünfte Modul des Plugins: ein Quad Function Generator mit VCA.
Vier unabhängige Hüllkurvengeneratoren mit je vier Stages (Delay, Attack, Hold,
Release), jeder zugleich VCA und Hüllkurvenausgang. Die Oberfläche zeigt immer
nur einen Generator, alle vier laufen im Hintergrund weiter. Diese Dokumentation
beschreibt jedes Bedienelement, jeden Ein-/Ausgang, die interne Signalverarbeitung,
die C++-Struktur und die Design-Entscheidungen.

Die Namensgebung folgt der Anguissola-Familie (wie Sofonisba, Amilcare, Bianca und
Lucia): Europa ist eine der Schwestern.

**Hinweis:** Das Plugin *Kinovice reminiscence of the unattained* ist ein eigenständiges Werk, stark inspiriert von der wunderbaren Buchla-Modulwelt. Es steht in keiner Verbindung zu Buchla, Tiptop Audio oder einem anderen Hersteller und wird von diesen weder autorisiert noch unterstützt. Erwähnte Produkt- und Firmennamen sind Marken ihrer jeweiligen Inhaber und dienen nur dazu zu beschreiben, welche funktionalen Konzepte ein Modul inspiriert haben.

Stand: 2026-10-06. Alle beschriebenen Funktionen wurden von Kinovice im laufenden
Rack getestet, außer ausdrücklich als offen markiert (siehe [Abschnitt 6](#sec6)).

---

<a id="toc"></a>

## Inhaltsverzeichnis

*Jeder Eintrag hat zwei Links: den ersten (Standard-Markdown-Anker) nutzt du in GitHub, Codeberg, VSCodium-Preview & den meisten anderen Viewern; "Obsidian" ist der Wikilink speziell für Obsidian.*

1. [Überblick](#sec1) · [[#1. Überblick|Obsidian]]
2. [Bedienelemente](#sec2) · [[#2. Bedienelemente|Obsidian]]
   - [Panel-Layout](#sec2-1) · [[#2.1 Panel-Layout|Obsidian]]
   - [Generator-Auswahl (GEN und ACTIVATE)](#sec2-2) · [[#2.2 Generator-Auswahl (GEN und ACTIVATE)|Obsidian]]
   - [Generator-Schalter (MODE, RETRIG D, LOOP)](#sec2-3) · [[#2.3 Generator-Schalter (MODE, RETRIG D, LOOP)|Obsidian]]
   - [Stage-Regler (LEVEL, TIME, RANGE, CURVE)](#sec2-4) · [[#2.4 Stage-Regler (LEVEL, TIME, RANGE, CURVE)|Obsidian]]
   - [Ein- und Ausgänge](#sec2-5) · [[#2.5 Ein- und Ausgänge|Obsidian]]
   - [OR-Ausgänge und Quadratur-Taster](#sec2-6) · [[#2.6 OR-Ausgänge und Quadratur-Taster|Obsidian]]
   - [Kontextmenü (Antialiasing, Ratio-Wechsel)](#sec2-7) · [[#2.7 Kontextmenü (Antialiasing, Ratio-Wechsel)|Obsidian]]
   - [Phasor-Modus (Bedienung)](#sec2-8) · [[#2.8 Phasor-Modus (Bedienung)|Obsidian]]
3. [Interne Signalverarbeitung](#sec3) · [[#3. Interne Signalverarbeitung|Obsidian]]
   - [Zustandsautomat pro Generator](#sec3-1) · [[#3.1 Zustandsautomat pro Generator|Obsidian]]
   - [Zeit-Signalpfad](#sec3-2) · [[#3.2 Zeit-Signalpfad|Obsidian]]
   - [Pegel-Signalpfad](#sec3-3) · [[#3.3 Pegel-Signalpfad|Obsidian]]
   - [Kurvenform](#sec3-4) · [[#3.4 Kurvenform|Obsidian]]
   - [VCA](#sec3-5) · [[#3.5 VCA|Obsidian]]
   - [EOC-Ausgang](#sec3-6) · [[#3.6 EOC-Ausgang|Obsidian]]
   - [Status-LEDs](#sec3-7) · [[#3.7 Status-LEDs|Obsidian]]
   - [Quadratur-Kopplung und OR-Logik](#sec3-8) · [[#3.8 Quadratur-Kopplung und OR-Logik|Obsidian]]
   - [Antialiasing (minBLEP)](#sec3-9) · [[#3.9 Antialiasing (minBLEP)|Obsidian]]
   - [Phasor-Modus (Signalverarbeitung)](#sec3-10) · [[#3.10 Phasor-Modus (Signalverarbeitung)|Obsidian]]
4. [Funktionsreferenz (C++)](#sec4) · [[#4. Funktionsreferenz (C++)|Obsidian]]
5. [Persistenz](#sec5) · [[#5. Persistenz|Obsidian]]
6. [Bekannte Fallstricke und offene Punkte](#sec6) · [[#6. Bekannte Fallstricke und offene Punkte|Obsidian]]
7. [Patch-Ideen](#sec7) · [[#7. Patch-Ideen|Obsidian]]
8. [Design-Entscheidungen & Abgrenzungen](#sec8) · [[#8. Design-Entscheidungen & Abgrenzungen|Obsidian]]

---

<a id="sec1"></a>

## 1. Überblick
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Aufbau**<br>
Vier Generatoren mit je vier Stages: **Delay, Attack, Hold, Release**. Jeder Generator hat einen VCA (Signal rein, Signal raus) und gibt die Hüllkurve zusätzlich separat aus (0–10 V). Das Modul ist 23HP breit (116,84 × 128,5 mm), rechts neben dem Hauptblock sitzt die OR-Spalte.

**Ein Generator sichtbar, alle aktiv**<br>
Die Regler zeigen immer nur den gewählten Generator. Ein Auswahl-Knob (GEN) mit Aktivieren-Button (ACTIVATE) schaltet um. Alle vier Generatoren laufen unabhängig weiter, ihre Buchsen sind immer sichtbar.

**Zwei Modi und Loop**<br>
Pro Generator: One-Shot oder Sustain, dazu ein dreistufiger Loop-Schalter (Off / Gate / Free, nur im One-Shot wirksam) und ein Retrig-Schalter, der festlegt, ob ein Retrigger vom aktuellen Pegel oder vom Delay-Pegel startet.

**Hauptnutzung**<br>
Hüllkurven, Trapez-/AR-/Gate-Verläufe und vor allem LFO-Betrieb (Loop, LFO bis in den Audiobereich als Zusatz). Delay- und Release-Pegel sowie alle Pegel per CV steuerbar machen auch offset-behaftete LFOs und Wavefolder-artige Effekte möglich (siehe [Abschnitt 7](#sec7)).

**Erweiterungen (2026-10-01)**<br>
Freier Zyklus (LOOP = FREE, Loop ohne Gate), Rate-CV (Kanal 5 am TIME-CV-Eingang, bipolar, exponentiell, formtreu), zwei OR-Ausgänge (Maximum je Paar 1+2 und 3+4) und eine schaltbare Quadratur-Kopplung je Paar (Generator 2 bzw. 4 startet am Ende von Attack von Generator 1 bzw. 3). Siehe [2.3](#sec2-3), [2.5](#sec2-5), [2.6](#sec2-6), [3.2](#sec3-2) und [3.8](#sec3-8).

**Erweiterung (2026-10-02)**<br>
Optionales Antialiasing (minBLEP) für die harten Pegelsprünge der Hüllkurve, schaltbar im Rechtsklick-Menü, Standard aus. Siehe [2.7](#sec2-7) und [3.9](#sec3-9).

**Erweiterung (2026-10-05)**<br>
Der Quadratur-Taster hat eine dritte Stellung: den **Handshake**. Beide Generatoren eines Paars warten am Attack-Ende aufeinander und koppeln ihre Decays, so entsteht eine echte Viertelperiode zwischen den beiden Hüllkurven. Siehe [2.6](#sec2-6) und [3.8](#sec3-8). Der Funktionstest im Rack steht noch aus (siehe [Abschnitt 6](#sec6)).

**Erweiterung (2026-10-06)**<br>
**Phasor-Modus** je Generator: Die Buchse T1/GATE nimmt statt eines Gates einen **Phasor** (Rampe 0–10 V) auf, und die vier Stage-Zeiten werden zu Gewichten, die einen Zyklus in Delay, Attack, Hold und Release teilen. Hüllkurven, Phasor-Verformungen und Sequenzer bleiben damit taktgenau an einen externen Phasor gebunden, ohne eigene Zeitbasis. Dazu kommen eine freie Ratio p/q, ein Offset, eine Richtungserkennung (fallende Rampen laufen ebenfalls als Rise vor Fall), ein Reset, ein Phasor-Ausgang an ENV und CV für Curve, Offset und Ratio. Siehe [2.8](#sec2-8) und [3.10](#sec3-10).

**Engine**<br>
Eigene rack-freie Klasse `EnvGen4` (`EnvGen4.hpp`, nur `<cmath>`/`<algorithm>`), vier Instanzen im Modul `Europa` (`Europa.cpp`). Sofonisba (`Afg`) bleibt unberührt.

---

<a id="sec2"></a>

## 2. Bedienelemente
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

<a id="sec2-1"></a>

### 2.1 Panel-Layout
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Das Panel besteht von oben nach unten aus vier Bereichen (Maße in mm, Panel 116,84 × 128,5, also 23 HP; der Hauptblock misst 101,6 mm, rechts schließt die OR-Spalte an):

```
EUROPA
[ GEN-Knob ] [ ACTIVATE ]  |  MODE        RETRIG D      LOOP
                              SUSTAIN      ON            FREE
                                                         GATE
                              ONE-SHOT     OFF           OFF

 DELAY       ATTACK      HOLD        RELEASE      (dunkles Namensband)
 [Level]     [Level]     [Level]     [Level]
 [Time]      [Time]      [Time]      [Time]
 [Range]     [Range]     [Range]     [Range]
 [Curve]     [Curve]     [Curve]     [Curve]

 T1/GATE TRIG TIME CV LEVEL CV HOLD CV VCA IN | VCA OUT ENV EOC | OR   (Ausgänge auf dunkler Platte, OR-Spalte rechts)
 1 ...                                                     (Zeile Generator 1)
 2 ...
 3 ...
 4 ...
```

**Kopfbereich (y 10,5–29,5)**<br>
Links ein Rahmen mit GEN-Knob (14 mm Durchmesser, Mitte x 14) und ACTIVATE-Button (Mitte x 31). Rechts ein Rahmen mit den drei Generator-Schaltern (Mitte x 51,5 / 72,5 / 89,5).

**Stage-Spalten (y 31–83,3)**<br>
Vier Rahmen à 22 mm Breite bei x 13,3 / 38,3 / 63,3 / 88,3, je mit dunklem Namensband. Reihen von oben: LEVEL (Trimpot), TIME (Knob), RANGE (Schalter), CURVE (Trimpot).

**Buchsenblock (y 84,6–126,3)**<br>
Vier Zeilen (eine je Generator) bei y 93 / 102,3 / 111,6 / 120,9, neun Spalten im Abstand 10 mm ab x 13. Die Eingänge (T1/GATE, TRIG, TIME CV, LEVEL CV, HOLD CV, VCA IN) liegen links auf hellem Grund, die Ausgänge (VCA OUT, ENV, EOC) rechts auf einer dunklen Platte. Links in jeder Zeile stehen Status-LED, Auswahl-LED und die Generator-Nummer 1–4.

**OR-Spalte (x 101,5–114,5, y 84,6–126,3)**<br>
Eine zusätzliche Spalte rechts neben dem Hauptblock, im Stil der Buchsenspalten gestaltet: eine 10 mm breite dunkle Platte in einem hellen Rahmen mit schmalem Rand (1,5 mm seitlich, 0,8 mm oben und unten), rechts 2,3 mm Abstand zum Panelrand. Zwei Blöcke übereinander, je Paar eine Beschriftung (OR 1+2, OR 3+4), eine Buchse und ein Klickschalter mit drei Stellungen und LED für die Quadratur-Kopplung. Siehe [2.6](#sec2-6).

**PHASOR-Streifen (x 101,5–114,5, y 10,5–82,6)**<br>
Über der OR-Spalte, in deren Breite: ein heller Rahmen mit den Bedienelementen des Phasor-Modus für den angezeigten Generator. Von oben: PHASOR-Schalter, OUT-Taster, NUM (Zähler), DEN (Nenner), OFFSET, daneben die gelbe Pending-LED. Der Streifen nutzt den bisher leeren Bereich, die Panelbreite bleibt 23 HP. Siehe [2.8](#sec2-8).

**Zeilenhervorhebung**<br>
Die Zeile des gerade angezeigten Generators hat ein blaues Band.

<a id="sec2-2"></a>

### 2.2 Generator-Auswahl (GEN und ACTIVATE)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**GEN-Knob**<br>
Vertikales Ziehen wählt die Nummer 1–4 vor (`pendingGen`), 4 mm Ziehweg pro Schritt. Die Zahl im Knob ist grün, solange die Vorwahl dem angezeigten Generator entspricht, sonst amber.

**ACTIVATE-Button**<br>
Übernimmt die Vorwahl (`displayedGen = pendingGen`). Grün, wenn Vorwahl und Anzeige übereinstimmen, sonst rot. Erst dann wechseln alle Regler auf den neuen Generator.

**Auswahl-LEDs**<br>
Eine blaue LED je Zeile: hell beim angezeigten Generator, schwach beim nur vorgewählten.

`pendingGen`/`displayedGen` sind bewusst **keine Rack-Params**: Undo/History, Automation und Preset-Recall sollen die reine Navigation nicht beeinflussen. Sie werden per JSON gesichert ([Abschnitt 5](#sec5)).

<a id="sec2-3"></a>

### 2.3 Generator-Schalter (MODE, RETRIG D, LOOP)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

MODE und RETRIG D sind Kippschalter (CKSS) mit zwei Stellungen: **oben = an**, unten = aus. Steht ein Schalter oben, liegt ein grünes Feld hinter dem oberen Text. LOOP ist ein Dreistellungsschalter (`CKSSThree`): oben FREE, Mitte GATE, unten OFF, die aktive Stellung ist grün markiert (OFF bleibt unmarkiert). Alle drei Schalter gehören zum angezeigten Generator.

**MODE** (`MODE_PARAM`)<br>
Oben: SUSTAIN, unten: ONE-SHOT (Default). Siehe [3.1](#sec3-1).

**RETRIG D** (`RETRIG0_PARAM`)<br>
Oben: ON. Ein Retrigger (steigende Flanke an T1/GATE während des Laufs) startet dann vom **Delay-Pegel** statt vom aktuellen Ausgangspegel. Der Ausgang springt beim Retrigger auf den Delay-Pegel (ggf. hörbarer Sprung). Aus (Default): Start vom aktuellen Pegel, kein Klick. Wirkt nur auf T1/GATE, Sprungtrigger an TRIG starten immer vom aktuellen Pegel. Der interne Name `RETRIG0_PARAM` bleibt aus historischen Gründen unverändert (früher "Retrig 0": Sprung auf 0 V).

**LOOP** (`LOOP_PARAM`, 0..2, Default 0)<br>
Drei Stellungen, nur im One-Shot wirksam, im Sustain-Modus ohne Funktion. **OFF** (unten, 0): kein Loop, ein Durchlauf pro Gate. **GATE** (Mitte, 1): Loop bei gehaltenem Gate, solange das Gate hoch bleibt, beginnt nach Release automatisch ein neuer Zyklus (das Verhalten des früheren Schalters in Stellung ON). **FREE** (oben, 2): freier Zyklus (Auto-Cycle), der Generator startet von selbst ohne Gate und läuft dauerhaft im Loop. Ein Gate ist nicht erforderlich, wirkt aber weiterhin als Retrigger. Ein früher gespeicherter Schalter in Stellung ON lädt in der Mitte (GATE). Siehe [3.1](#sec3-1).

<a id="sec2-4"></a>

### 2.4 Stage-Regler (LEVEL, TIME, RANGE, CURVE)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**LEVEL** (Trimpot, 0–10 V)<br>
Zielpegel der Stage. Alle vier Stages haben einen Pegel: `LEVEL_DELAY_PARAM` (Default 0 V), `LEVEL_ATTACK_PARAM` (Default 10 V), `LEVEL_HOLD_PARAM` (Default 10 V), `LEVEL_RELEASE_PARAM` (Default 0 V). Unipolar, siehe [3.3](#sec3-3). Zusätzlich per LEVEL CV steuerbar.

**TIME** (Knob, `TIME_PARAM`, 0..1, Default 0,5)<br>
Reglerposition, die zusammen mit dem RANGE-Schalter die Stage-Zeit ergibt (exponentielle Abbildung, im Linksanschlag exakt 0). Tooltip und Rechtsklick-Feld zeigen die Zeit in Sekunden. Eingabe: `5 ms`, `1,5 s`, `200 us`. Eine Zahl ohne Einheit nimmt die Einheit des aktuell angezeigten Werts (unter 1 s: ms, sonst s). Werte über der Range-Obergrenze werden auf den Anschlag begrenzt. Siehe [3.2](#sec3-2).

**RANGE** (Schalter, `RANGE_PARAM`, 0..3, Default TR3)<br>
Klick schaltet zum nächsten Bereich weiter (TR1 → TR2 → TR3 → TR4 → TR1), Shift+Klick rückwärts. Rechtsklick öffnet das Standard-Menü von Rack, das die vier Bereiche als Auswahlliste enthält (`RANGE_PARAM` ist ein `configSwitch` mit Labels). Undo/Redo werden unterstützt. Bereiche (alle ab 0): TR1 0–30 ms, TR2 0–300 ms, TR3 0–3 s, TR4 0–30 s.

**CURVE** (Trimpot, `CURVE_PARAM`, −1..+1, Default 0)<br>
Kurvenform der Stage. Links = rund, Mitte = linear, rechts = spitz, jeweils relativ zur Bewegungsrichtung. Siehe [3.4](#sec3-4).

<a id="sec2-5"></a>

### 2.5 Ein- und Ausgänge
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Pro Generator neun Buchsen, insgesamt 36, dazu die zwei OR-Ausgänge (siehe [2.6](#sec2-6)). Alle Buchsen sind immer sichtbar und unabhängig vom angezeigten Generator.

**T1/GATE** (`GATE_INPUT`, mono)<br>
Startet die Hüllkurve bei Stage 1 (Delay). One-Shot: Trigger (Flanke), Sustain: Gate (Pegel). Schmitt-Trigger mit 0,1 V / 1 V Hysterese.

**TRIG** (`TRIG_INPUT`, poly)<br>
Sprungtrigger: Kanal 2 = Attack, Kanal 3 = Hold, Kanal 4 = Release, Kanal 1 ungenutzt. Steigende Flanke über 2 V, keine Hysterese. Ein Mono-Kabel liegt auf Kanal 1 und bewirkt nichts. Siehe [3.1](#sec3-1).

**TIME CV** (`TIMECV_INPUT`, poly)<br>
Kanal 1–4 = Zeit-CV für Delay, Attack, Hold, Release, additiv zur Reglerposition. **Kanal 5 = Rate-CV** (bipolar, exponentiell, 1 Oktave pro Volt, begrenzt auf ±5 V): ändert die Geschwindigkeit des ganzen Generators, ohne die Kurvenform zu verändern. Ein Mono-Kabel wirkt nur auf Delay, Kanal 5 liegt nur bei einem Kabel mit mindestens fünf Kanälen an. Siehe [3.2](#sec3-2).

**LEVEL CV** (`LEVELCV_INPUT`, poly)<br>
Kanal 1–4 = Pegel-CV für Delay, Attack, Hold, Release, additiv zum Regler (1 V = 1 V), Summe auf 0–10 V begrenzt. Ein Mono-Kabel wirkt nur auf den Delay-Pegel. Siehe [3.3](#sec3-3).

**HOLD CV** (`HOLDCV_INPUT`, mono)<br>
Eigene Buchse für die Hold-Zeit, unabhängig vom TIME-CV-Kanal 3. Nur wirksam bei gepatchtem Kabel **und** im One-Shot (Loop eingeschlossen), im Sustain-Modus ohne Wirkung. Ist ein Kabel gepatcht, ersetzt die Spannung den TIME-Regler der Hold-Stage vollständig, nur der RANGE-Schalter (TR1–TR4) bleibt wirksam: 0 V = kein Hold, steigend bis knapp unter 9,5 V im gewählten Bereich, ab 9,5 V unendliches Halten (Ende nur durch einen Trigger an TRIG Kanal 4). Kein Bezug zum Gate. Ohne Kabel bleibt die Hold-Stage unverändert (Regler + TIME-CV Kanal 3). Siehe [3.1](#sec3-1) und [3.2](#sec3-2).

**VCA IN** (`SIGNAL_INPUT`, poly)<br>
Signaleingang des VCA, ohne Kabel auf 10 V normalisiert. Siehe [3.5](#sec3-5).

**VCA OUT** (`VCA_OUTPUT`, poly)<br>
Signal mal Hüllkurve, Kanalzahl wie VCA IN (ohne Kabel mono).

**ENV** (`ENV_OUTPUT`, mono)<br>
Hüllkurve, 0–10 V.

**EOC** (`EOC_OUTPUT`, mono)<br>
Trigger (10 V) am Zyklusende. Siehe [3.6](#sec3-6).

**Phasor-Modus: geänderte Belegung**<br>
Steht ein Generator im Phasor-Modus ([2.8](#sec2-8)), gilt für seine Buchsen:
- **T1/GATE:** Phasor-Eingang, 0–10 V, statt Trigger/Gate. Ohne Kabel nimmt der Generator den Phasor des vorherigen Generators, sofern dieser ebenfalls im Phasor-Modus ist (ein Kabel an Generator 1 speist so alle vier). Generator 1 hat keinen Vorgänger, ohne Kabel liegt dort 0 V an.
- **TRIG:** Kanal 1 ist der **Reset** des Zykluszählers (steigende Flanke ab 2 V). Die Stage-Trigger (Kanal 2–4) haben im Phasor-Modus keine Wirkung.
- **TIME CV:** Kanal 1–4 wirken als Gewichte der Stages (wie Zeiten). **Kanal 5 = Offset**, **Kanal 6 = Zähler p**, **Kanal 7 = Nenner q** (statt der Rate-CV), jeweils additiv zum Regler, 10 V entsprechen dem vollen Regelweg. Ein Kabel muss dafür mindestens 5, 6 bzw. 7 Kanäle haben.
- **LEVEL CV:** Kanal 1–4 wie bisher. **Kanal 5–8 = Curve** von Delay, Attack, Hold, Release (additiv, ±5 V entsprechen dem vollen Bereich −1…+1, Summe auf −1…+1 begrenzt). Die Curve-CV gilt in **beiden** Modi, auch im klassischen Betrieb.
- **HOLD CV:** ersetzt das Gewicht der Hold-Stage, auch im Sustain-Modus, nie unendlich.
- **ENV:** Hüllkurve oder Phasor, je nach OUT-Taster. **VCA OUT** und die **OR-Ausgänge** arbeiten immer mit der Hüllkurve.
- **EOC:** Ende des Zyklus von y (siehe [3.10](#sec3-10)).

<a id="sec2-6"></a>

### 2.6 OR-Ausgänge und Quadratur-Taster
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**OR 1+2, OR 3+4** (`OR_OUTPUT`, zwei Ausgänge, mono)<br>
Liefern den **höheren der beiden ENV-Pegel** eines Paars (Generator 1+2 bzw. 3+4), 0–10 V, abgegriffen vor dem VCA. Sie arbeiten unabhängig davon, welcher Generator gerade angezeigt wird. Siehe [3.8](#sec3-8).

**Quadratur-Taster** (`QUAD_PARAM`, zwei Params mit je drei Stellungen 0..2)<br>
Klickschalter mit LED unter der jeweiligen OR-Buchse, Default Off. Klick schaltet weiter (Off → Follow → Handshake → Off), Shift+Klick rückwärts. Rechtsklick öffnet das Standard-Menü von Rack mit den drei benannten Stellungen. Undo/Redo werden unterstützt. Die LED zeigt die Stellung: dunkel = **Off**, grün = **Follow**, bernstein = **Handshake**. Die Taster gehören zum Paar, nicht zum angezeigten Generator.

**Off**<br>
Die Generatoren des Paars sind unabhängig.

**Follow**<br>
**Generator 2 (bzw. 4) folgt dem Generator 1 (bzw. 3)**: Er startet am Ende von Attack des Partners und ignoriert sein eigenes T1/GATE. Die Decays beider Generatoren laufen unabhängig voneinander.

**Handshake**<br>
Beide Generatoren warten am Attack-Ende aufeinander und koppeln ihre Decays (Vorbild: der Quadratur-Modus des inspirierenden Hüllkurvengenerators). Gate an T1/GATE von Generator 1, Generator 2 startet am Attack-Ende von Generator 1. Der genaue Ablauf steht in [3.8](#sec3-8).

Siehe auch [3.8](#sec3-8).

<a id="sec2-7"></a>

### 2.7 Kontextmenü (Antialiasing, Ratio-Wechsel)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Antialiasing (minBLEP, ENV/VCA/OR)** (`Europa::antialias`, kein Rack-Param)<br>
Haken im Rechtsklick-Menü des Moduls, **modulweit für alle vier Generatoren**, Standard **aus**. Eingeschaltet glättet das Modul die Pegelsprünge der Hüllkurve (Stage mit Zeit 0, Retrig-Sprung) an ENV, am VCA und an den OR-Ausgängen. EOC, Gate-Logik und Quadratur-Start bleiben unberührt. Der Schalter ist bewusst kein Param (Undo, Automation und Preset-Recall sollen ihn nicht beeinflussen) und wird per JSON gesichert ([Abschnitt 5](#sec5)). Funktionsweise und Grenzen siehe [3.9](#sec3-9).

**Phasor: Ratio-Wechsel locker (jeder Eingangs-Wrap)** (`Europa::ratioLoose`, kein Rack-Param)<br>
Haken im Rechtsklick-Menü, **modulweit für alle Generatoren im Phasor-Modus**, Standard **aus** (strikte Übernahme). Eingeschaltet übernimmt jeder Generator eine neue Ratio p/q an jedem Eingangs-Wrap, nicht erst an einem Wrap, an dem y exakt auf dem Zyklusanfang liegt. y springt dabei. Der Sprung wird als Hüllkurven-Sprung gemeldet und mit eingeschaltetem Antialiasing geglättet. Der Schalter wird per JSON gesichert ([Abschnitt 5](#sec5)), alte Patches laden strikt. Unterschiede im Detail siehe [3.10](#sec3-10).

<a id="sec2-8"></a>

### 2.8 Phasor-Modus (Bedienung)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Der Phasor-Modus macht aus einem Generator eine **Hüllkurve, die zeitlich vollständig von einem externen Phasor gesteuert wird**. Der Generator hat dann keine eigene Zeit: Alles ist eine reine Abbildung der Eingangsphase, es kann nichts driften, und Hüllkurven, Sequenzen und Modulationen bleiben taktgenau. Die Bedienelemente liegen im PHASOR-Streifen über der OR-Spalte ([2.1](#sec2-1)) und gehören zum angezeigten Generator. Alle Einstellungen sind echte Rack-Params (je Generator fünf). Die Funktionsweise steht in [3.10](#sec3-10).

**PHASOR** (`PHASOR_PARAM`, Kippschalter, Default aus)<br>
Oben: Phasor-Modus an. Der Generator verhält sich dann wie in [3.10](#sec3-10) beschrieben. Unten (Default): der Generator arbeitet exakt wie bisher, alle alten Patches bleiben unverändert. Ein Moduswechsel setzt die Engine dieses Generators zurück. Der Modus gilt pro Generator, zwei Generatoren können also als klassische Hüllkurven laufen und zwei als Phasor-Hüllkurven.

**OUT** (`PHASOR_OUT_PARAM`, Klick-Taster mit drei Stellungen 0..2, Default 0)<br>
Legt fest, was die ENV-Buchse im Phasor-Modus ausgibt. Klick schaltet weiter (Hüllkurve → linear → verformt), Shift+Klick rückwärts, Rechtsklick öffnet das Standard-Menü von Rack mit den drei benannten Stellungen. Undo/Redo werden unterstützt. Die LED zeigt die Stellung: dunkel = **Hüllkurve**, grün = **Phasor linear**, bernstein = **Phasor verformt**. Das Widget ist dasselbe wie beim Quadratur-Taster (`EuropaQuadButton`), mit eigenem Undo-Namen.
- **Hüllkurve:** ENV gibt die Hüllkurve aus (0–10 V).
- **Phasor linear:** ENV gibt y aus, 0–10 V, eine gleichmäßige Rampe mit Ratio und Offset.
- **Phasor verformt:** ENV gibt `(k + curve(u)) / 4` aus. Jedes der vier Segmente belegt ein Viertel, die Gewichte bestimmen also, wie schnell sich die Eingangsphase durch die vier Viertel bewegt, und die Curve-Regler formen den Verlauf in jedem Segment. Das ist der Phasor für Folgemodule und Folgegeneratoren.

Beide Phasor-Ausgaben sind **richtungstreu**: Eine fallende Eingangsrampe ergibt eine fallende Ausgangsrampe. VCA OUT und die OR-Ausgänge behalten in allen Stellungen die Hüllkurve.

**NUM** und **DEN** (`RATIO_P_PARAM`, `RATIO_Q_PARAM`, Trimpots mit Raster, 1..16, Default 1)<br>
Zähler p und Nenner q der Ratio p/q. 2/1 läuft doppelt so schnell wie der Eingang, 1/2 halb so schnell, 3/2 anderthalbmal so schnell. Die Werte werden gekürzt (8/8 ist 1/1). Der Tooltip zeigt den Wert. Per CV steuerbar (TIME CV Kanal 6 und 7). Wann eine neue Ratio übernommen wird, steht in [3.10](#sec3-10).

**OFFSET** (`PHASE_OFFSET_PARAM`, 0–100 %, Default 0)<br>
Phasenverschiebung des Zyklus von y in Zyklusanteilen, in Laufrichtung nach vorn. Per CV steuerbar (TIME CV Kanal 5), der Wert läuft als Phase um.

**Pending-LED** (`PENDING_LIGHT`, gelb)<br>
Leuchtet, solange eine Änderung von p oder q auf einen gültigen Eingangs-Wrap wartet. Im strikten Modus kann das mehrere Eingangszyklen dauern, im lockeren nur bis zum nächsten Wrap.

**Im Phasor-Modus ohne Wirkung**<br>
MODE (Sustain), RETRIG D, LOOP, die Rate-CV (Kanal 5 am TIME-CV-Eingang ist dort der Offset) und die Stage-Trigger an TRIG (Kanal 2–4). Die Quadratur-Taster sind für einen Generator im Phasor-Modus nicht vorgesehen ([Abschnitt 6](#sec6)).

**Kurzanleitung**<br>
1. Phasor-Quelle (Rampe 0–10 V, steigend oder fallend) an T1/GATE des Generators. Mit mehreren Generatoren im Phasor-Modus genügt ein Kabel an Generator 1.
2. PHASOR einschalten.
3. Zeiten (TIME und RANGE) als **Anteile** des Zyklus einstellen: Gleiche Zeiten ergeben vier gleich lange Segmente. Ungleiche Zeiten verschieben die Segmentgrenzen.
4. Pegel und Curve wie bei einer normalen Hüllkurve.
5. NUM/DEN und OFFSET einstellen. Ein Reset an TRIG Kanal 1 legt die Zählerposition fest.
6. OUT wählen: ENV als Hüllkurve oder als Phasor.

---

<a id="sec3"></a>

## 3. Interne Signalverarbeitung
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

<a id="sec3-1"></a>

### 3.1 Zustandsautomat pro Generator
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**One-Shot**<br>
Steigende Flanke an T1/GATE startet Delay → Attack → Hold → Release, danach Ruhe (Ausgang auf dem Release-Pegel, EOC-Puls). Ein Trigger an TRIG springt in die gewählte Stage und läuft bis zum Zyklusende weiter, unabhängig vom Gate.

**Sustain**<br>
Gate high startet Delay, dann Attack, dann **Halten** am Attack-Pegel. Hold wird übersprungen, das Gate ersetzt sie. (Wurde per TRIG-Sprung in Hold gesprungen, hält die Hüllkurve am Ende von Hold auf dem Hold-Pegel.) Gate low startet Release vom aktuellen Pegel. Gate low während Delay, solange der Ausgang noch auf dem Ruhepegel steht, bricht ohne Release und ohne EOC ab. Trigger an TRIG wirken nur bei Gate high. Ein unendliches Halten per Zeit-CV wird im Sustain-Modus ignoriert.

**Interner Loop (One-Shot)**<br>
Am Ende von Release wird der Gate-Pegel geprüft. Gate high: Delay folgt im selben Sample, der Zeitrest wird übernommen. Gate low: Ende. Ein kurzer Trigger allein ergibt einen einzelnen Durchlauf, das Gate muss für den Loop gehalten werden. Der Loop hat keine Kabel-Latenz, die Periode ist exakt die Summe der Stage-Zeiten. EOC feuert bei jedem Zyklusende, auch im Loop.

**Freier Zyklus (LOOP = FREE)**<br>
Nur im One-Shot. Ein ruhender Generator startet von selbst über `startCycle()`, also mit demselben Verhalten wie bei einem Gate (Retrig-Schalter, Delay-Pegel). Nach Release beginnt sofort der nächste Zyklus, auch ohne Gate: `loopEnabled` und `freeCycle` sind gesetzt, die Schleife läuft auf `gateHigh || freeCycle`. Ein Gate wirkt weiterhin als Retrigger. Wird FREE ausgeschaltet (Stellung GATE oder OFF), endet der laufende Zyklus regulär am Ende von Release. Der Ausgang springt dabei nicht und ruht danach auf dem Release-Pegel. Bei unendlichem Hold bleibt der Generator stehen, bis ein Trigger an TRIG kommt. EOC feuert bei jedem Zyklusende. Sind alle Zeiten 0, läuft ein Durchlauf pro Sample (Entartungsfall, stabil).

**Patch-Loop**<br>
EOC → T1/GATE desselben Generators. Der Puls startet den nächsten Durchlauf, ein gezogenes Kabel stoppt nach dem laufenden Durchlauf. Das Kabel hat 1 Sample Latenz. Ohne Delay geht EOC → TRIG Kanal 2. Auch für Ketten und Ringe über mehrere Generatoren geeignet. Im Sustain-Modus nicht sinnvoll (Gate ist ein Pegel).

**Retrigger**<br>
Steigende Flanke an T1/GATE während des Laufs startet Stage 1 neu. RETRIG D aus: vom aktuellen Pegel, an: vom Delay-Pegel.

**Hold-Dauer per CV (nur One-Shot/Loop)**<br>
Ist HOLD CV gepatcht, bestimmt allein die Spannung die Länge der Hold-Stage, unabhängig vom Gate: 0 V bedeutet kein Hold (die Stage läuft mit Zeit 0 durch), ansteigend bis zum unendlichen Halten ab 9,5 V. Der TIME-Regler der Hold-Stage ist dann wirkungslos, der RANGE-Schalter bestimmt weiter den Maßstab. Im Sustain-Modus wird HOLD CV ignoriert, dort bestimmt wie bisher das Gate die Haltedauer.

**Zeit 0**<br>
Jede Stage kann Zeit 0 haben (Regler links oder CV). Eine solche Stage wird im selben Sample durchlaufen, Zeitreste gehen in die Folge-Stage. Delay 0: Attack startet sofort. Attack 0: Stufe auf den Attack-Pegel. Release 0: sofortiger Sprung auf den Release-Pegel. Bei Zeit 0 in allen Stages sind maximal vier Stage-Übergänge pro Sample erlaubt (Mindestzyklus 1 Sample), damit die Schleife nicht hängen bleibt.

**Unendliches Halten**<br>
Zeit-CV ≥ 9,5 V an einer Stage hält sie unendlich, bis ein Trigger an TRIG die nächste Stage startet (nur One-Shot). Der Ausgang bleibt dabei auf dem Startpegel der Stage stehen.

**Moduswechsel im Lauf**<br>
Wechselt der Schalter von Sustain auf One-Shot während der Haltephase, endet das Halten und die Hüllkurve läuft regulär weiter.

<a id="sec3-2"></a>

### 3.2 Zeit-Signalpfad
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Reglerabbildung**<br>
`t = max · (B^x − 1) / (B − 1)` mit `x` = Reglerposition (0..1) plus Zeit-CV/10 V (begrenzt auf 0..1), `B = ENV_TIME_EXP_BASE = 1000` und `max` = Obergrenze des Bereichs. Bei `x = 0` ist die Zeit exakt 0. Die Reglermitte von TR1 liegt bei etwa 0,9 ms. Die Umkehrfunktion `envPositionFromTime()` dient der Texteingabe.

**Zeit-CV**<br>
Additiv zur Reglerposition, 10 V entsprechen dem vollen Regelweg. Die Summe wird auf 0..1 begrenzt. ≥ 9,5 V (nur die CV selbst) bedeutet unendliches Halten (nicht im Sustain-Modus).

**Hold-CV (Sonderfall)**<br>
Bei gepatchtem HOLD CV im One-Shot/Loop wird die Hold-Zeit nicht additiv, sondern direkt aus der Spannung berechnet: dieselbe Funktion wie oben, aber mit Reglerposition fest 0 und der Spannung als alleiniger Eingabe (`envResolveTime(0, cv, range, true)`), sodass die Spannung selbst die volle Reglerposition 0..1 abdeckt. Eigener Cache, unabhängig vom Zeit-Regler-Cache.

**Rate-CV (Kanal 5 von TIME CV)**<br>
Kanal 5 des polyphonen TIME-CV-Eingangs wirkt nicht auf eine Stage-Zeit, sondern auf die Geschwindigkeit des ganzen Generators. Faktor = `2^(U · ENV_RATE_OCTAVES_PER_VOLT)`, mit `U` begrenzt auf ±`ENV_RATE_CV_LIMIT_V` (Werte 1 Oktave pro Volt und 5 V, damit 1/32 bis ×32; `envRateFromVolts()`). 0 V ergibt den Faktor 1, positiv ist schneller, negativ langsamer. Die Rate skaliert nur den Zeitfortschritt in `EnvGen4::process()` (`elapsed += dt · rate`). Weil die Phase einer Stage aus `elapsed` geteilt durch die Stage-Zeit folgt, bleibt die Kurvenform bei jeder Rate erhalten, auch bei einer Änderung mitten in einer Stage (der Ausgang springt nicht). Pulslänge am EOC und Zyklusmessung laufen in echter Zeit. Unendliches Hold und Sustain-Halten bleiben unbeeinflusst, Gate und Trigger reagieren unverändert. Ohne Kabel sowie bei Mono- oder Vierkanalkabel bleibt der Faktor 1, alte Patches klingen unverändert. Die Zyklusrate ist durch die Abtastrate begrenzt (höchstens ein Zyklus pro Sample). Der Faktor wird pro Generator nur bei einer Spannungsänderung neu berechnet.

**Cache**<br>
Die aufgelöste Zeit enthält ein `pow()` und wird pro Stage nur bei Änderung von Regler, CV, Range oder Modus neu berechnet.

<a id="sec3-3"></a>

### 3.3 Pegel-Signalpfad
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Pegel**<br>
Jede Stage hat einen Zielpegel, unipolar 0–10 V: Regler plus LEVEL CV, Summe auf 0–10 V begrenzt. Delay-Pegel = Startpegel des Zyklus (nur mit RETRIG D als Sprungziel, sonst Ziel der Delay-Stage), Release-Pegel = Endpegel und Ruhepegel.

**Ruhepegel**<br>
Im Stillstand liegt der Ausgang auf dem Release-Pegel, auch direkt nach dem Einschalten. Der Regler wirkt live, ohne dass ein Zyklus läuft. Nach Release bleibt der Ausgang dort stehen. Beim internen Loop startet der nächste Zyklus von diesem Pegel.

**Interpolation**<br>
Jede Stage interpoliert vom Pegel bei Stage-Start (`voltageFrom`) zum Zielpegel. Läuft ein Pegelregler oder die Level-CV während einer Stage, interpoliert diese ab dem nächsten Sample zum neuen Ziel weiter. Das erklärt die lebendigen Effekte bei LFO-modulierten Pegeln.

<a id="sec3-4"></a>

### 3.4 Kurvenform
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Bipolare Abbildung**<br>
CURVE −1..+1. Aus dem Betrag `a = |c|` folgt der Krümmungsexponent `power = 2,5^(a · (2 + a²))` (maximal 2,5³ ≈ 15,6, bei `c = 0` linear).

**Relativ zur Bewegungsrichtung**<br>
Für eine steigende Stage gilt: `c < 0` (rund) `f = 1 − (1 − t)^power` (schneller Start, sanftes Ziel), `c > 0` (spitz) `f = t^power` (sanfter Start, schnelles Ziel). Läuft die Stage abwärts (Ziel unter Startpegel), wird das Vorzeichen gespiegelt. „Rund“ bedeutet damit immer: **am oberen Pegelende flach**. Attack schießt hoch und flacht oben ab, Release startet oben flach und fällt zum Ende schneller. Beide Regler links ergeben eine symmetrische, glockenartige Hüllkurve, beide rechts eine spitze. Ein Test mit Kurve −0,5 ergab Attack 5,54 V bei 25 % und 8,57 V bei 50 %, Release 9,79 V bei 25 % und 8,57 V bei 50 %.

**Cache**<br>
`power` hängt nur vom Betrag ab und wird bei Änderung der Kurve neu berechnet.

<a id="sec3-5"></a>

### 3.5 VCA
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Der Gain ist die Hüllkurve mal 0,1 (0 bei 0 V, 1 bei 10 V) und wirkt gleichmäßig auf alle Kanäle. Das Signal darf bipolar sein, das Vorzeichen bleibt erhalten (linearer VCA, kein Gleichrichter). Der Gain wird nie negativ, weil alle Pegel auf 0–10 V begrenzt sind. Ohne Kabel an VCA IN liegen 10 V an, VCA OUT entspricht dann ENV als eigenes Signal. Die Kanalzahl von VCA OUT folgt VCA IN. Der VCA wird nur berechnet, wenn VCA OUT gepatcht ist. Ist das Antialiasing eingeschaltet, nutzt der Gain die geglättete Hüllkurve ([3.9](#sec3-9)), die ebenfalls auf 0–10 V begrenzt bleibt.

Bei einem Release-Pegel über 0 V schließt der VCA im Ruhezustand nicht ganz.

<a id="sec3-6"></a>

### 3.6 EOC-Ausgang
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Der EOC-Puls (10 V) feuert am Ende von Release. Die Pulslänge ist adaptiv: das Minimum aus 1 ms und der halben Länge des letzten Zyklus, mindestens 1 Sample. Die Länge hängt an der Länge des zuletzt abgelaufenen Zyklus, nicht an der Zeit seit dem letzten gefeuerten EOC, damit ein verschluckter Puls die Periode nicht verdoppelt. Zwischen zwei Pulsen liegt mindestens 1 Sample Pause (steigende Flanke garantiert, Mindestzyklus 2 Samples). Ein Ereignis während eines laufenden Pulses verschmilzt mit diesem. Ein Abbruch in Delay (Sustain) erzeugt keinen EOC. Bei einer Rate ungleich 1 bleibt die Pulslänge in echter Zeit, maßgeblich ist die gemessene echte Zykluslänge.

<a id="sec3-7"></a>

### 3.7 Status-LEDs
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Pro Zeile eine Status-LED mit drei unabhängigen Farbkanälen (immer nur einer leuchtet): **Rot** = Stopp, **Grün** = läuft (gleiches Grün wie ACTIVATE), **Gelb** = Halten (Sustain oder unendliche Stage-Zeit). Die LED wird nur alle 64 Samples aktualisiert. „Läuft“ gilt für das ganze Fenster seit der letzten Aktualisierung, Rot nur bei vollständigem Leerlauf, damit ein Patch-Loop (EOC → T1) nicht rot aufblitzt. Die LED zeichnet mit normaler Überblendung ohne Lichthof, weil Racks Standard-Screen-Blendmodus auf dem hellen Panel zu Pastelltönen ausbleicht.

<a id="sec3-8"></a>

### 3.8 Quadratur-Kopplung und OR-Logik
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**OR**<br>
Rein ausgangsseitig in `Europa::process()` nach der Generatorschleife: `max(gens[0].envOut, gens[1].envOut)` und `max(gens[2].envOut, gens[3].envOut)`. Die Engines bleiben unberührt, der Pegel wird vor dem VCA abgegriffen. Bei eingeschaltetem Antialiasing ([3.9](#sec3-9)) vergleicht das Maximum die geglätteten Pegel (`envAA[]`), sonst sind sie identisch zu `envOut`.

**Startereignis**<br>
`EnvGen4::attackEnded` ist genau ein Sample lang wahr. Es wird in `finishStage()` gesetzt, wenn Attack abläuft, auch bei Attack-Zeit 0 und beim Sustain-Halten. Ein Sprungtrigger direkt in Hold löst kein Ereignis aus.

**Follower (Modus Follow)**<br>
Bei aktivem Taster (Stellung Follow) ist `EnvParams::coupled` für Generator 2 (bzw. 4) gesetzt. Seine Gate-Flanke startet dann keinen Zyklus, der Start kommt über den optionalen Parameter `startRequest` von `process()`. `Europa::process()` leitet ihn aus `attackEnded` des Partners ab. Generator 1 läuft in der Schleife vor Generator 2, das Ereignis liegt also im selben Sample vor. Das eigene T1/GATE des Followers wird ignoriert, stattdessen gilt der Gate-Zustand des Partners (`gateHigh`). Die Trigger an TRIG für Stage 2–4 bleiben aktiv. LOOP = FREE verhält sich bei aktiver Kopplung wie GATE (Loop bei gehaltenem Gate des Partners), damit sich der Follower nicht selbst startet.

**Sustain-Modus**<br>
Im Sustain-Modus übernimmt der Follower den Gate-Zustand des Partners: Er startet am Attack-Ende, hält am Ende von Attack, solange das Gate des Partners hoch ist, und beginnt Release, sobald es fällt.

**Delay des Followers**<br>
Der Start des Followers beginnt mit dessen Delay-Stage. Eine Delay-Zeit an Generator 2 (Regler oder Delay-CV an Kanal 1 von TIME CV) verschiebt den Beginn seiner Hüllkurve zusätzlich zum Attack-Ende des Partners und stellt so den Versatz zwischen beiden Kurven ein.

**Loop**<br>
Für einen einzelnen Durchlauf muss der LOOP-Schalter **beider** Generatoren auf OFF stehen. Läuft Generator 1 im Loop (GATE oder FREE), wiederholt sich der Zweistufen-Zyklus: Jedes Attack-Ende von Generator 1 startet Generator 2 neu.

**Zusammen mit OR**<br>
Gekoppelt und über den OR-Ausgang zusammengeführt ergeben zwei Generatoren eine zweiteilige Hüllkurve, deren zweiter Teil am Höhepunkt des ersten einsetzt.

**Handshake (Stellung 2, 2026-10-05)**<br>
Vorbild ist der Quadratur-Modus des inspirierenden Hüllkurvengenerators nach einer öffentlichen Beschreibung. Die Grafiken der Quelle konnten nicht ausgewertet werden, die Umsetzung ist eine eigenständige Interpretation des beschriebenen Ablaufs. Master ist Generator 1 (bzw. 3), Follower Generator 2 (bzw. 4). Der Viererzyklus:

1. Der Master startet (Gate an T1/GATE), durchläuft Delay und Attack und **hält** auf dem Attack-Pegel.
2. Mit dem Attack-Ende des Masters startet der Follower (mit seiner Delay-Stage) und steigt.
3. Erreicht der Follower sein Attack-Ende, **hält** auch er auf seinem Attack-Pegel, und der Master beginnt Release.
4. Erreicht der Master sein Ende (Minimum), beginnt der Follower Release.
5. Am Ende von Release des Followers ist das Zyklusende. Mit LOOP am Master startet dann der Master neu.

**Periode und Phase**<br>
Die Periode ist die Summe aus Delay und Attack des Masters, Delay und Attack des Followers sowie Release des Masters und Release des Followers. Bei gleichen Zeiten ist der Follower gegenüber dem Master genau um eine Viertelperiode verschoben (im Standalone-Test beträgt die Abweichung 0,002 V). Der OR-Ausgang liefert dann ein Trapez mit doppelt so breitem Plateau, weil immer einer der beiden oben steht. Die Zeitregler wirken gekoppelt: Ein längerer Attack des Followers verlängert das Plateau des Masters, ein längerer Release des Masters verschiebt den Release des Followers.

**Hold-Stage entfällt**<br>
Das Warten ersetzt Hold, wie am Original. Hold-Zeit, Hold-Pegel und HOLD CV haben im Handshake keine Wirkung. Gehalten wird auf dem Attack-Pegel.

**Loop**<br>
Der LOOP-Schalter des Masters wiederholt den Viererzyklus, entsprechend dem Kabel „Pulse B → Trigger A“ am Original: Der Master startet nach dem Zyklusende des Followers neu. GATE: nur bei gehaltenem Gate. FREE: ohne Gate. Der Master hat im Handshake keinen internen Loop, er wartet immer, bis der Follower fertig ist. Mit LOOP = OFF läuft genau ein Zyklus.

**Follower im Cycle**<br>
Steht der LOOP-Schalter des Followers auf GATE (Gate des Masters) oder FREE, läuft der Follower frei und wird vom Attack-Ende des Masters nicht neu gestartet. Der Master wartet am Attack-Ende auf das nächste Attack-Ende des Followers. Der Follower hält nur dann am Attack-Ende, wenn der Master gerade wartet, sonst läuft er ohne Halten durch Release weiter. Während des Release des Masters bleibt der Follower oben und zykliert danach weiter, das entspricht der Beschreibung des Cycle-Falls am Original. FREE am Follower verhält sich hier nicht wie GATE (anders als im Modus Follow).

**Sustain**<br>
Der Handshake gilt nur im One-Shot. Steht einer der beiden Generatoren im Sustain-Modus, gilt automatisch das Verhalten von Follow. Wird der Handshake ausgeschaltet oder ein Generator auf Sustain gestellt, während ein Generator wartet, endet das Halten sofort. Ein verwaister Partner kann den Generator also nicht festhalten.

**Externer Neutrigger**<br>
Wird der Master von außen neu getriggert, während der Follower noch hält, startet der Follower mit dem nächsten Attack-Ende des Masters neu, auch mit LOOP am Follower. Ohne diese Regel würden sich beide gegenseitig blockieren.

**Ereignisse und Implementierung**<br>
Die Engine meldet neben `attackEnded` das neue Ereignis `cycleEnded` (ein Sample lang, Release abgelaufen). Das Warten nutzt intern dasselbe Halten wie der Sustain-Modus (`sustainHeld`), zusätzlich mit `hsHeld` markiert, deshalb leuchtet die Status-LED währenddessen gelb. `envQuadSetup()` in `EnvGen4.hpp` setzt aus den Ereignissen des Partners die Handshake-Felder in `EnvParams` und liefert den `startRequest` des Followers. Der Master läuft in `Europa::process()` vor dem Follower: Er sieht Ereignisse des Followers ein Sample später (rund 21 µs bei 48 kHz), der Follower sieht die Ereignisse des Masters im selben Sample.

**Nicht übernommen**<br>
Hardware-Eigenheiten der Quelle bilden wir nicht nach: das mit langem Decay abfallende Pulse-Level, die durch CMOS-Belastung zu früh kippende Schwelle, die Pulsform (15 V, ca. 150 µs) und die Triggerschwellen der Transient-Schaltung. Die Schwellen sind hier exakt, Ereignisse sind interne Flags. Die Beschreibung des Vierfach-Panning-Patches in der Quelle ist in sich widersprüchlich und wurde nicht umgesetzt.

<a id="sec3-9"></a>

### 3.9 Antialiasing (minBLEP)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Problem**<br>
Harte Flanken der Hüllkurve (Stage mit Zeit 0, Retrig-Sprung) enthalten Obertöne weit über der Nyquist-Frequenz. Läuft die Hüllkurve im Audiobereich (z. B. als Rechteck im Free-Zyklus) und moduliert über den VCA ein Audiosignal, falten diese Obertöne als Aliasing zurück ins Hörbare. Antialiasing ist dafür optional ([2.7](#sec2-7)), Standard aus.

**Was die Engine meldet**<br>
`EnvGen4` meldet pro Sample die **echten Pegelsprünge** (`jumpCount`, `jumpPos[]`, `jumpDelta[]`): (a) eine Stage mit Zeit 0 (außer Delay) erreicht ihr Ziel schlagartig, der Sprung ist Zielpegel minus Startpegel; (b) der Retrig-Schalter springt beim Start auf den Delay-Pegel. Stetige Verläufe, auch sehr schnelle Rampen, werden nicht gemeldet. Eine Delay-Stage mit Zeit 0 ändert den Pegel nicht und meldet nichts. Die Engine bleibt rack-frei, ihr Ausgang ist gegenüber der Version ohne Meldung bitgleich (Vergleich über 8 Millionen Samples mit Zufallsparametern).

**Zeitpunkt der Sprünge**<br>
`jumpPos` liegt in (−1, 0] relativ zum aktuellen Sample. Bei einem abgelaufenen Zeit-0-Stage-Ende folgt die Position aus dem Zeitrest (Subsample-genau, auch bei Rate ≠ 1). Bei Gate- und Trigger-Ereignissen ist der Zeitpunkt innerhalb des Samples unbekannt, dort gilt −0,5.

**Glättung**<br>
Je Generator ein `dsp::MinBlepGenerator<16, 16>` in `Europa`. `Europa::process()` fügt die gemeldeten Sprünge ein und addiert die Korrektur zum Engine-Ausgang (`envOut + Korrektur`). Die Impulsantwort ist rund 32 Samples lang und kausal, es entsteht keine Latenz. Weil minBLEP leicht überschwingt, wird das Ergebnis auf 0–10 V begrenzt. Die Engine-Zustände (Quadratur, Gates, EOC) rechnen weiter mit dem ungeglätteten Wert.

**Wo es wirkt**<br>
ENV, VCA (Gain) und OR-Ausgänge. Nicht betroffen: EOC. Ausgeschaltet entspricht der Ausgang dem bisherigen Verhalten.

**Phasor-Modus**<br>
Im Phasor-Modus meldet `PhasorCore` die Sprünge der Hüllkurve auf dieselbe Weise ([3.10](#sec3-10)): Segmente mit Gewicht 0, Reset, Offset-Sprünge, Richtungswechsel des Eingangs und Phasensprünge bei lockerer Ratio-Übernahme. Der Ausgang ENV ist nur dann geglättet, wenn er die Hüllkurve ausgibt. Ein Phasor an ENV bleibt ungeglättet.

**Grenzen**<br>
- Nur **Sprünge** werden geglättet, nicht die **Knicke** an Stage-Übergängen (die fallen mit 1/f² ab und sind deutlich leiser, polyBLAMP wurde vorerst zurückgestellt).
- Sprünge durch **Modulation** (Zeit- oder Pegel-CV, Reglerbewegung mitten in einer Stage) werden nicht gemeldet.
- Bei **sehr hoher Zyklusfrequenz** überlappen sich die Korrekturen benachbarter Flanken, die Wirkung lässt nach.
- Aliasing, das schon **im VCA-Eingangssignal** steckt, kann Europa nicht entfernen.
- Entartungsfall alle Zeiten 0 plus Loop: Die Meldungen sind dort unvollständig, harmlos, weil sich die Sprünge an derselben Stelle aufheben.

**Zum Messen**<br>
Ein Sinus (nicht ein Rechteck) am VCA IN, Analyzer an VCA OUT, Schalter aus gegen ein. Die Seitenbänder bei f_Sinus ± n · f_Hüllkurve bleiben (Amplitudenmodulation, gewollt), die zurückfaltenden Linien nehmen deutlich ab. Siehe auch [Abschnitt 6](#sec6).

**Test**<br>
`tests/test_antialias_spruenge.cpp` (standalone, ohne Rack): keine Sprünge bei stetiger Hüllkurve, Attack-Zeit 0, Retrig-Sprung, Release-Zeit 0 im Sustain und 300 Zufallsläufe, in denen Größe und Vorzeichen jedes gemeldeten Sprungs zur Pegeländerung passen. Funktionstest im Rack am 2026-10-02: funktioniert.

<a id="sec3-10"></a>

### 3.10 Phasor-Modus (Signalverarbeitung)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Die Engine des Phasor-Modus ist die rack-freie Klasse `PhasorCore` (`PhasorCore.hpp`, [Abschnitt 4](#sec4)). Pro Generator läuft eine Instanz. Sie benutzt `EnvParams` und die Hilfsfunktionen aus `EnvGen4.hpp` (Zeiten, Kurvenformen, Pegel), die Zustände von `EnvGen4` bleiben im Phasor-Modus im Ruhezustand.

**Idee: eine Abbildung, kein Zustand der Zeit**<br>
Aus der Eingangsphase `x` (0–10 V, geteilt durch 10) entsteht über eine feste Kette die Phase `y` des Generators, aus `y` die Hüllkurve und die Phasor-Ausgänge. Es gibt keine Integratoren und keine eigene Zeit. Der Ausgang folgt dem Eingang also exakt, auch bei Tempowechseln, Stillstand und Rückwärtslauf. Der einzige echte Zustand ist ein Zykluszähler, der nur von den Wraps des Eingangs abhängt. Ein Clock-Eingang mit Periodenmessung wurde bewusst verworfen (Abschnitt 8).

**Eingang und Richtung**<br>
`x = U / 10`, auf 0..1 gebracht (`x − floor(x)`, damit auch leicht überschwingende Phasoren funktionieren). Die Richtung folgt aus dem Schritt `d = x − xPrev` mit einer Hysterese: `d < −0,5` ist ein Wrap einer steigenden Rampe, `d > 0,5` ein Wrap einer fallenden Rampe, `|d| > 10⁻⁵` setzt die Richtung nach dem Vorzeichen, kleinere Schritte (Stillstand, Rauschen) ändern sie nicht. Bis zum ersten Schritt gilt „steigend“.

**Fallende Rampe: Spiegelung am Eingang**<br>
Bei fallender Rampe arbeitet die ganze Kette mit `xs = 1 − x`. Für den Generator ist das eine normale steigende Rampe: Er läuft immer in der Reihenfolge Delay → Attack → Hold → Release (Rise vor Fall), und das EOC liegt bei fallender Rampe automatisch bei x = 0. Offset und Ratio gelten aus Sicht des Generators in Laufrichtung. Die Phasor-Ausgänge werden am Ende wieder zurückgespiegelt (siehe „Phasor-Ausgänge“ unten), bleiben also richtungstreu. Ein Richtungswechsel lässt `xs` springen (`x ↔ 1 − x`), die Hüllkurve springt dabei (gemeldet, siehe unten), der Phasor-Ausgang nicht.

**Entrollter Phasor und Ratio p/q**<br>
Ein Zykluszähler `cycle` (0..719719, also modulo 720720 = kgV(1..16)) zählt die Eingangs-Wraps, nur vorwärts auf dem gespiegelten Signal. Daraus folgt
`y = frac(((cycle mod q) + xs) · p / q + offset)`.
p/q wird vorher gekürzt. Die Rechnung läuft in `double`, damit `y` an den Zyklusgrenzen exakt bleibt. Beispiele: 1/2 ist ein Zyklus von y über zwei Eingangszyklen, 3/2 drei y-Zyklen über zwei Eingangszyklen, 2/1 zwei y-Zyklen pro Eingangszyklus. Weil `cycle` nur von den Eingangs-Wraps abhängt und über Reset und Ratio-Übernahme definiert ist, bleiben alle Generatoren phasenstarr zueinander.

**Ratio-Übernahme (strikt und locker)**<br>
Der Regler samt CV liefert den Zielwert (p, q), er wird nur an einem Eingangs-Wrap übernommen.
- **Strikt (Standard):** nur wenn `cycle mod q_alt == 0` **und** `cycle mod q_neu == 0` gilt. Dann liegt y vor und nach dem Wechsel exakt auf dem Zyklusanfang (plus Offset), es gibt keinen Sprung. Der Preis ist die Wartezeit: Bei 4 → 6 alle 12 Eingangszyklen, bei 7 → 11 alle 77. Springt die CV vor dem passenden Wrap zurück, geht die Änderung verloren.
- **Locker (Kontextmenü, [2.7](#sec2-7)):** an jedem Eingangs-Wrap sofort. y springt auf den Wert, den der globale Zähler für die neue Ratio vorgibt. Der Sprung wird als Hüllkurven-Sprung gemeldet, es gibt keinen EOC.
- **Reset:** Eine steigende Flanke an TRIG Kanal 1 setzt den Zähler auf 0 und übernimmt eine ausstehende Ratio sofort (y springt dabei, gemeldet). Ohne Reset ist die Phasenlage bei Division durch den Einschaltzeitpunkt bestimmt.
- Die gelbe Pending-LED leuchtet, solange der Zielwert von der aktuellen Ratio abweicht.

**Abtastung der Ratio-CV (Faustregel)**<br>
Die Ratio wird nur an Eingangs-Wraps gelesen (im strikten Modus nur an gültigen), die CV wird also mit der Frequenz des Eingangsphasors abgetastet. Für eine erkennbare Form braucht ein LFO mindestens etwa vier Messpunkte pro Periode, also höchstens ein Viertel der Eingangsfrequenz. Bei einem 1-Hz-Phasor sind LFOs über etwa 0,25 Hz zu schnell, sie wirken, als funktionierten sie nur manchmal. Bei 8 Hz Eingang liefen 0,5–2 Hz sauber (Kinovice, Test im Rack). Abhilfe: schnelleren Eingang verwenden und über q herunterteilen, oder als Modulationsquelle den Phasor-Ausgang eines anderen Generators oder einen Sequencer im Takt nehmen.

**CV-Abbildung (alle additiv zum Regler)**<br>

| Ziel | Kanal | Abbildung |
|---|---|---|
| Gewichte (Zeit) | TIME CV 1–4 | wie Zeit-CV ([3.2](#sec3-2)) |
| Offset | TIME CV 5 | 10 V = ein voller Zyklus, läuft als Phase um |
| Zähler p | TIME CV 6 | 10 V = voller Weg 1…16, gerundet |
| Nenner q | TIME CV 7 | 10 V = voller Weg 1…16, gerundet |
| Curve | LEVEL CV 5–8 | ±5 V = −1…+1, begrenzt |

p und q werden mit einer Hysterese gerundet: Der Wert wechselt erst, wenn er mehr als 0,6 Stufen vom zuletzt übernommenen Wert abweicht (je 0,1 Stufen Spielraum an der Rundungsgrenze), damit eine CV auf der Grenze die Pending-LED nicht flackern lässt. Kleine Amplituden unter etwa ±0,4 Stufen bewegen die Ratio daher nicht.

**Segmente aus den Gewichten**<br>
Die aufgelösten Stage-Zeiten (Regler, TIME CV und RANGE, wie in [3.2](#sec3-2); HOLD CV ersetzt das Hold-Gewicht) sind Gewichte `w₁…w₄`. Normiert (`pᵢ = wᵢ / Σw`) teilen sie y in vier Segmente. Es gibt kein unendliches Halten. Ein Gewicht von 0 lässt das Segment entfallen (die Hüllkurve springt dort, gemeldet), sind alle Gewichte 0, gilt eine Gleichverteilung (je ein Viertel). Wichtig: **Der RANGE-Schalter skaliert die Gewichte** (TR1 bis TR4 haben unterschiedliche Obergrenzen). Für gleiche Anteile müssen die Stages denselben Bereich haben, sonst wiegt eine Stage in TR4 tausendfach schwerer als eine in TR1.

**Hüllkurve im Segment**<br>
Stage `k` läuft von Pegel(k−1) nach Pegel(k), das Delay-Segment startet vom Release-Pegel. Die Phase im Segment `u = (y − Anfang) / Länge` wird mit der Curve der Stage geformt (wie [3.4](#sec3-4), relativ zur Bewegungsrichtung, also abwärts laufende Stages mit gespiegeltem Vorzeichen), inklusive der Curve-CV. Der Ausgang ist `Start + (Ziel − Start) · f(u)`.

**Phasor-Ausgänge**<br>
`phasorLinear = y`. `phasorWarped = (k + f(u)) / 4` mit derselben Curve `f` (immer steigend, ohne Spiegelung). Er ist stetig und monoton, bei ungleichen Gewichten entspricht er einer Verformung der Zeit, in der jedes Segment ein Viertel belegt. Beide werden bei fallendem Eingang zurückgespiegelt (`1 − Wert`), damit eine fallende Eingangsrampe eine fallende Ausgangsrampe ergibt. Die Rückspiegelung hebt die Eingangsspiegelung genau auf, ein Richtungswechsel lässt den Phasor-Ausgang deshalb nicht springen.

**EOC**<br>
Der EOC-Puls feuert, wenn `y` über die Zyklusgrenze läuft (`y` fällt um mehr als 0,5 gegenüber dem Vorsample). Pulslänge wie in [3.6](#sec3-6): Minimum aus 1 ms und der halben Länge des letzten Zyklus von y, mindestens 1 Sample. Bei p > q gibt es mehrere EOC pro Eingangszyklus. Kein EOC entsteht bei unerklärten Sprüngen von y (Reset, Offset-Sprung, Richtungswechsel, Phasensprung bei lockerer Übernahme).

**Unerklärte Sprünge und minBLEP**<br>
Pro Sample vergleicht die Engine `y` mit dem erwarteten Wert (`y` des Vorsamples plus Eingangsbewegung mal bisherige Ratio). Weicht `y` um mehr als 2 % eines Zyklus ab, hat sich die Hüllkurve unerwartet bewegt. Beträgt der Unterschied zwischen tatsächlichem und erwartetem Hüllkurvenwert mehr als 0,05 V, wird er als Sprung gemeldet (`jumpCount`/`jumpPos`/`jumpDelta`, Lage im Sample unbekannt, daher −0,5), und `Europa::process()` glättet ihn bei eingeschaltetem Antialiasing wie die Sprünge der klassischen Engine ([3.9](#sec3-9)). Nicht gemeldet werden stetige Änderungen (Regler, langsame CV), sie bleiben unter der Schwelle. Segmentsprünge durch Gewicht 0 meldet die Engine gesondert (Pegeldifferenz, Lage subsamplegenau aus dem y-Verlauf). Beim strikten Ratio-Wechsel bleibt `y` stetig, dort gibt es nichts zu melden.

**Moduswechsel**<br>
Beim Umschalten des PHASOR-Schalters werden die `EnvGen4`-Instanz und die `PhasorCore`-Instanz des Generators sowie die Zeit-Caches zurückgesetzt. Im Phasor-Modus rechnet `EnvGen4` nicht, der Quadratur-Start wird nicht aufgerufen.

**Grenzen**<br>
- Pro Sample darf sich `y` um höchstens 0,5 bewegen: Bei sehr hohem Verhältnis `p/q` und schnellem Eingang entstehen sonst falsche Wraps.
- Ein Offset, der sich mit Audiorate und großer Amplitude ändert, wird als Folge von Sprüngen behandelt und nicht als Modulation geglättet.
- Eine Ratio-CV zwischen den Wraps wirkt nicht stufenlos: Eine stetige Ratio würde die feste Phasenlage zum Eingang aufgeben.
- Ein Phasor an ENV (OUT auf linear oder verformt) ist nicht minBLEP-geglättet.

**Test**<br>
`tests/phasor_core_test.cpp` (standalone, ohne Rack, mit `-std=c++11` gebaut): Ratio 1/1, 2/1 und 1/2 (Hüllkurve und EOC-Zahl), fallende Rampe (Reihenfolge und richtungstreuer Phasor-Ausgang), strikter Ratio-Wechsel, Gewicht 0 mit Sprungmeldung, Reset, Richtungswechsel ohne falschen EOC, alle Gewichte 0, Curve auf dem verformten Phasor (spitz und rund, stetig an den Segmentgrenzen), unerklärte Sprünge (Reset, Offset-Sprung, langsamer Offset-Sweep ohne Meldung, Richtungswechsel) und die lockere Ratio-Übernahme gegenüber der strikten.

---

<a id="sec4"></a>

## 4. Funktionsreferenz (C++)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

### `EnvGen4.hpp` (rack-frei)

**Konstanten**<br>
`ENV_NUM_STAGES = 4`, `ENV_MAX_TRANSITIONS_PER_SAMPLE = 4`, `ENV_TIME_INFINITE = 1e30f` (endlicher Sentinel statt INFINITY, weil Rack mit `-funsafe-math-optimizations` gebaut wird), `ENV_TIME_INFINITE_VOLTS = 9,5`, `ENV_TIME_RANGE_MAX = {0,03; 0,3; 3; 30}`, `ENV_TIME_EXP_BASE = 1000`, `ENV_TIME_CV_SPAN_VOLTS = 10`, Gate 0,1/1 V, Trigger 2 V, `ENV_EOC_MAX_SECONDS = 1e-3`, `ENV_RATE_OCTAVES_PER_VOLT = 1`, `ENV_RATE_CV_LIMIT_V = 5`, `ENV_MAX_JUMPS_PER_SAMPLE = 8`.

**`envTimeFromPosition()` / `envPositionFromTime()`**<br>
Regler ↔ Sekunden je Range.

**`envResolveTime()`**<br>
Regler + Zeit-CV → Sekunden oder `ENV_TIME_INFINITE`.

**`envCurvePower()` / `envShapePhase()`**<br>
Krümmungsexponent und Kurvenanwendung, siehe [3.4](#sec3-4).

**`envRateFromVolts()`**<br>
Rate-CV in Volt → Geschwindigkeitsfaktor (> 0), siehe [3.2](#sec3-2).

**`struct EnvParams`**<br>
Pro Sample übergebene, bereits aufgelöste Werte: `time[4]`, `curve[4]`, `levelDelay`, `levelAttack`, `levelHold`, `levelRelease`, `sustainMode`, `loopEnabled`, `freeCycle`, `retrigZero`, `rate`, `coupled`, dazu die Handshake-Felder `handshake`, `hsMaster`, `hsPartnerWaiting`, `hsPartnerRunning`, `hsReleaseEvent`, `hsPartnerCycleEnded` (gesetzt von `envQuadSetup()`).

**`struct EnvGen4`**<br>
Ausgänge `envOut`, `eocOut`, `running`, `held`, `stage`, `attackEnded`, `cycleEnded`, `hsHeld`. Sprungmeldung für das Antialiasing: `jumpCount`, `jumpPos[]`, `jumpDelta[]` (siehe [3.9](#sec3-9)). Methoden `process(dt, params, gateVolts, trigVolts[4], startRequest = false)`, `startCycle()`, `jumpTo()`, `startRelease()`, `finishStage()`, `addJump()`, `reset()`.

**`enum EnvQuadMode` / `envQuadSetup()`**<br>
`ENV_QUAD_OFF = 0`, `ENV_QUAD_FOLLOW = 1`, `ENV_QUAD_HANDSHAKE = 2`. `envQuadSetup(p, quadMode, isFollower, pairSustain, self, partner)` setzt die Kopplungsfelder von `EnvParams` für einen Generator des Paars und gibt den `startRequest` für `process()` zurück (nur Follower). Muss nach dem Setzen von `loopEnabled`/`freeCycle` aufgerufen werden. Bei `pairSustain` gilt auch in Stellung 2 das Verhalten von Follow. Testbar ohne Rack, `tests/test_handshake.cpp` benutzt dieselbe Funktion wie `Europa::process()`.

### `Europa.cpp`

**`struct Europa : Module`**<br>
Enums `ParamId`, `InputId`, `OutputId`, `LightId`. `EnvGen4 gens[4]`, `TimeCache`, Rate-Cache (`rateCacheVolts`/`rateCacheValue`), `pendingGen`, `displayedGen`, `antialias`, `minBlep[4]` (`dsp::MinBlepGenerator<16, 16>`), `envAA[4]` (geglättete Hüllkurve), `process()`, `dataToJson()`/`dataFromJson()`, `onReset()`.

**`EnvTimeQuantity`**<br>
Anzeige/Eingabe der Zeit in Sekunden (`getDisplayValue`, `setDisplayValue`, `getDisplayValueString`, `setDisplayValueString`), abhängig vom Range derselben Stage.

**Widgets**<br>
`EuropaWidget`, `EuropaRangeSwitch`, `EuropaQuadButton`, `GenSelectKnob`, `GenActivateButton`, `EuropaStatusLight`, `EuropaSelectLight`, `EuropaFrame`, `EuropaRowHighlight`, `EuropaOnIndicator`, `EuropaPanelBackground`. Beschriftung über das zentrale `PanelLabel.hpp`. `EuropaWidget::appendContextMenu()` enthält den Antialiasing-Schalter.

### `PhasorCore.hpp` (rack-frei)

**Konstanten und Hilfen**<br>
`PHASOR_CYCLE_MOD = 720720` (kgV(1..16)), `PHASOR_RATIO_MAX = 16`, `PHASOR_DIR_EPS = 1e-5` (Hysterese der Richtungserkennung), `PHASOR_MAX_JUMPS = 2`. `enum PhasorOutMode` (`PHASOR_OUT_ENV = 0`, `PHASOR_OUT_LINEAR = 1`, `PHASOR_OUT_WARPED = 2`). `phasorGcd()` ist ein eigener ggT, weil Rack mit C++11 gebaut wird und `std::gcd` erst ab C++17 existiert.

**`struct PhasorCore`**<br>
Ausgänge `envOut`, `phasorLinear`, `phasorWarped` (0..1, richtungstreu), `eocOut`, `running`, `pending`, dazu die Sprungmeldung `jumpCount`, `jumpPos[]`, `jumpDelta[]` (wie bei `EnvGen4`, [3.9](#sec3-9)). Methoden `process(dt, EnvParams, ratioP, ratioQ, offset, phasorVolts, resetVolts, looseRatio = false)`, `envAt()` (Hüllkurve an einer Zyklusphase, auch für die Sprungprüfung), `addJump()`, `reset()`. Zustand: Zykluszähler, aktuelle Ratio, Richtung, Vorwerte, EOC-Puls.

### `Europa.cpp`, Ergänzungen für den Phasor-Modus

`PHASOR_PARAM`, `PHASOR_OUT_PARAM`, `RATIO_P_PARAM`, `RATIO_Q_PARAM`, `PHASE_OFFSET_PARAM` (je vier, am Enum-Ende), `PENDING_LIGHT` (vier), Members `phasor[4]`, `prevPhasorMode`, `phasorInVolts` (für die Durchschleifung), `ratioPending`, `ratioPLast`/`ratioQLast` (Hysterese der p/q-Rundung) und `ratioLoose`. `EuropaQuadButton` hat ein einstellbares `historyName` (Undo-Name), der Phasor-OUT-Taster nutzt es.

---

<a id="sec5"></a>

## 5. Persistenz
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Params**<br>
Alle Einstellungen aller vier Generatoren sind echte Rack-Params: 98 insgesamt (4 × 24: je 4 Pegel, 4 × Zeit, 4 × Range, 4 × Curve, Modus, Retrig, Loop und die fünf Phasor-Params Phasor, Phasor-Out, p, q, Offset; dazu zwei dreistufige Quadratur-Taster (0..2), je einer pro Paar). `LOOP_PARAM` hat drei Stellungen (0..2). Rack speichert sie im Patch.

**JSON**<br>
`pendingGen` und `displayedGen` (mit Clamp auf 0–3), dazu `antialias` und `ratioLoose` (beide Bool). Fehlt ein Eintrag in einem alten Patch, bleibt das Antialiasing aus und die Ratio-Übernahme strikt.

**Enum-Regel**<br>
Neue Einträge nur am Ende anhängen, damit Kabelindizes bestehender Patches stabil bleiben. Die Delay-/Release-Pegel-Params und `LEVELCV_INPUT` wurden so nachträglich angehängt. Alte Patches laden sie mit Default 0 V und klingen unverändert. Ebenso wurden `QUAD_PARAM` und `OR_OUTPUT` angehängt. Alte Patches laden die Kopplung ausgeschaltet. `QUAD_PARAM` bekam am 2026-10-05 die dritte Stellung (Wertebereich 0..2 statt 0..1), 0 (Off) und 1 (Follow) behalten ihre Bedeutung. Die frühere Lichtgruppe `QUAD_LIGHT` entfiel: Der Taster zeichnet seine LED selbst aus dem Param (Lichter werden nicht im Patch gespeichert, es gibt keine Kompatibilitätsfolgen). Die Werte 0 (Off) und 1 (Gate) von `LOOP_PARAM` behalten ihre Bedeutung, der neue Wert 2 steht für den freien Zyklus.

Der Phasor-Modus (2026-10-06) hängt `PHASOR_PARAM`, `PHASOR_OUT_PARAM`, `RATIO_P_PARAM`, `RATIO_Q_PARAM`, `PHASE_OFFSET_PARAM` (je vier) und `PENDING_LIGHT` am Ende an. Alte Patches laden alle Generatoren im klassischen Modus und klingen unverändert. Einzige Ausnahme: ein Poly-Kabel mit mehr als vier Kanälen an LEVEL CV, dessen Kanäle 5–8 jetzt als Curve-CV wirken.

**Reset**<br>
`onReset()` setzt alle Engines (auch die `PhasorCore`-Instanzen) und die Generator-Anzeige zurück.

---

<a id="sec6"></a>

## 6. Bekannte Fallstricke und offene Punkte
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Delay-Pegel wirkt bei Delay-Zeit 0 nur mit RETRIG D**<br>
Eine Delay-Stage mit Zeit 0 ändert den Pegel nicht (klickfreier Retrigger). Der Delay-Pegel ist dann wirkungslos, außer der Retrig-Schalter ist an, der beim Start auf den Delay-Pegel springt. Sonst gleitet Attack vom aktuellen Pegel los.

**Ruhepegel über 0 V öffnet den VCA**<br>
Ein Release-Pegel über 0 V lässt das VCA-Signal im Ruhezustand durchscheinen. Für reinen VCA-Betrieb Release-Pegel auf 0 V lassen (Default).

**Mono-Kabel an TRIG, TIME CV und LEVEL CV**<br>
TRIG: Kanal 1 ist ungenutzt, ein Mono-Kabel bewirkt nichts. TIME CV und LEVEL CV: ein Mono-Kabel wirkt nur auf Delay. Für alle anderen Stages ein Poly-Kabel verwenden.

**HOLD CV wirkt nur im One-Shot/Loop**<br>
Im Sustain-Modus bleibt HOLD CV wirkungslos, dort bestimmt das Gate die Hold-Dauer wie bisher. Ein gepatchtes Kabel macht den TIME-Regler der Hold-Stage im One-Shot vollständig wirkungslos, das ist Absicht und keine Fehlfunktion.

**Trigger im Sustain-Modus**<br>
Sprungtrigger wirken nur bei Gate high, sonst werden sie ignoriert.

**Loop im Sustain-Modus**<br>
Der LOOP-Schalter (auch die Stellung FREE) ist im Sustain ohne Wirkung, ebenso EOC → T1/GATE.

**Rate-CV braucht ein Kabel mit mindestens fünf Kanälen**<br>
Ein Mono-Kabel landet auf Kanal 1 und wirkt dort als Delay-Zeit-CV, nicht als Rate. Kanal 5 erreichst du über ein Merge-Modul oder einen Poly-Splitter, die Kanäle 1–4 dürfen leer sein. Der Bereich ist mit ±5 V und einer Oktave pro Volt großzügig: Ein Sinus mit ±5 V pendelt zwischen 1/32 und ×32, ein unipolarer LFO (0–10 V) liegt ab 5 V dauerhaft an der Begrenzung (×32).

**Quadratur (Follow): Loop beider Generatoren auf OFF für Einzeldurchläufe**<br>
Steht der LOOP-Schalter von Generator 1 auf GATE oder FREE, läuft er nach Release von selbst neu, und jedes Attack-Ende startet Generator 2 neu. Das sieht im Scope aus, als würde Generator 1 auf Generator 2 warten. Für einen einzelnen Durchlauf Loop beider Generatoren auf OFF stellen. Eine Delay-Zeit an Generator 2 verschiebt dessen Start zusätzlich zum Attack-Ende von Generator 1.

**Handshake: Hold-Regler ohne Wirkung**<br>
Im Handshake ersetzt das Warten auf den Partner die Hold-Stage. Hold-Zeit, Hold-Pegel und HOLD CV bewirken dort nichts. Das ist Absicht und keine Fehlfunktion.

**Handshake: Warten kann lange dauern**<br>
Jeder Generator wartet am Attack-Ende auf den Partner. Ist der Attack des Followers unendlich (Zeit-CV ≥ 9,5 V), wartet der Master entsprechend lange. Das entspricht dem Original. Ausschalten des Handshakes beendet das Halten sofort.

**Handshake: Loop nur am Master sinnvoll für den Viererzyklus**<br>
Der Master hat im Handshake keinen internen Loop. Er startet nur nach dem Zyklusende des Followers neu, und nur bei LOOP = GATE mit gehaltenem Gate oder bei LOOP = FREE. LOOP des Followers auf GATE oder FREE macht ihn dagegen zum freilaufenden Oszillator (siehe [3.8](#sec3-8)).

**Handshake nur im One-Shot, ein Sample Latenz**<br>
Steht einer der beiden Generatoren im Sustain, gilt Follow. Der Master sieht Ereignisse des Followers ein Sample später (rund 21 µs bei 48 kHz). Das ist bei Hüllkurven unhörbar, aber nicht exakt null.

**Gespeicherte Patches und die Panelbreite**<br>
Das Panel ist von 20 auf 23 HP gewachsen. Module, die in einem gespeicherten Patch direkt rechts neben Europa sitzen, können nach dem Laden überlappen und müssen dann verschoben werden.

**Antialiasing glättet nur Sprünge**<br>
Der Schalter ([2.7](#sec2-7)) wirkt nur auf die gemeldeten Pegelsprünge der Hüllkurve, nicht auf die Knicke an Stage-Übergängen, nicht auf Modulationssprünge und nicht auf Aliasing im VCA-Eingangssignal. Bei sehr hoher Zyklusfrequenz (die Impulsantwort ist rund 32 Samples lang) lässt die Wirkung nach. Für die Beurteilung am besten einen Sinus in den VCA speisen: Ein Rechteck am VCA IN erzeugt zusammen mit dem Rechteck der Hüllkurve ein Produkt aus zwei breitbandigen Signalen, in dem sich der Anteil der Hüllkurve schlecht erkennen lässt.

**Attack, Release und Delay alle 0 ergibt kein Rechteck**<br>
Bei Zeit 0 in Delay, Attack und Release steht die Hüllkurve im freien Zyklus dauerhaft auf dem Hold-Pegel, die Täler haben die Breite null. Der VCA hat dann konstant Gain 1 und lässt das Signal unverändert durch, es gibt nichts zu glätten. Ein Rechteck entsteht erst mit einer **Delay-Zeit größer 0** (bei Delay-Pegel 0 V), die Periode ist dann Delay plus Hold.

**Audiorate**<br>
Ein Patch-Loop hat durch das Kabel 1 Sample Latenz. Bei 48 kHz sind erkennbare Trapezformen bis ca. 5–6 kHz möglich (rund 8 Samples pro Zyklus), theoretisch 24 kHz (nur Impulsfolge). Die Latenz verstimmt bei hohen Frequenzen hörbar (bei 1 kHz ca. 2 %, bei 5 kHz ca. 10 %). Es gibt kein Oversampling. Harte Hüllkurvensprünge (Zeit-0-Stages, Retrig-Sprung) lassen sich im Kontextmenü mit minBLEP glätten ([2.7](#sec2-7), [3.9](#sec3-9)), ohne den Schalter aliasen sie. Der interne Loop hat keine Latenz.

**Automation und MIDI-Map**<br>
Da alle 98 Params echt sind, funktionieren MIDI-Map und Automation auf jedem Generator. Sichtbar ist aber immer nur der angezeigte Generator.

**Phasor-Modus: die Ratio-CV wird nur einmal pro Eingangszyklus gelesen**<br>
Die CV an TIME CV Kanal 6 und 7 wird an den Eingangs-Wraps abgetastet (siehe Faustregel in [3.10](#sec3-10)). Ein LFO über etwa einem Viertel der Eingangsfrequenz wirkt unregelmäßig oder bleibt stehen. Bei einem 1-Hz-Phasor sind schon LFOs ab 0,25 Hz zu schnell.

**Phasor-Modus: die strikte Übernahme kann CV-Änderungen verlieren**<br>
Die strikte Übernahme gilt nur an Wraps, an denen der Zähler zu altem und neuem q passt. Springt die CV vor einem passenden Wrap zurück, wird die Änderung verworfen. Für CV-gesteuerte Ratios (Sequencer, LFO) den lockeren Modus im Kontextmenü wählen ([2.7](#sec2-7)).

**Phasor-Modus: gleiche CV auf Zähler und Nenner hebt sich auf**<br>
Liegt dasselbe Rechteck auf TIME CV Kanal 6 und 7, wechseln p und q gleichzeitig (16/16 und 1/1), das Verhältnis bleibt 1/1, und es passiert nichts. Negative CV macht beide Zahlen kleiner: Beim Zähler verlangsamt das den Generator (8 → 4: 1/2), beim Nenner beschleunigt es ihn (8 → 4: 2/1). Das ist die Mathematik der Ratio, kein Fehler.

**Phasor-Modus: Kanal 5–7 am TIME-CV-Eingang brauchen entsprechend viele Kanäle**<br>
Offset, p und q sind nur erreichbar, wenn das Kabel mindestens 5, 6 bzw. 7 Kanäle hat. Ein Mono-Kabel landet auf Kanal 1 und wirkt dort als Gewicht der Delay-Stage.

**Phasor-Modus: RANGE skaliert die Gewichte**<br>
Die Stage-Zeiten sind im Phasor-Modus Gewichte, und TR1…TR4 haben unterschiedliche Obergrenzen. Für gleich lange Segmente alle vier Stages auf denselben RANGE stellen.

**Phasor-Modus: Reset, Offset-Sprung und Richtungswechsel lassen die Hüllkurve springen**<br>
Diese Ereignisse bewegen `y` unerwartet. Die Hüllkurve springt (hörbar, mit Antialiasing geglättet), und es gibt keinen EOC. Der Phasor-Ausgang ist nicht geglättet. Bei lockerer Ratio-Übernahme springt `y` an jedem Wechsel.

**Phasor-Modus: Quadratur nicht vorgesehen**<br>
Ein Generator im Phasor-Modus ignoriert die Quadratur-Kopplung. Den Partner eines Phasor-Generators nicht im Follow- oder Handshake-Modus betreiben (nicht vorgesehen und ungetestet).

**LEVEL CV Kanal 5–8 wirken als Curve-CV, in beiden Modi**<br>
Ein Poly-Kabel mit mehr als vier Kanälen an LEVEL CV verändert jetzt die Curve der Stages. Alte Patches mit einem solchen Kabel können anders klingen.

**Offene Punkte**<br>
Eine umfassende DSP-Testdatei für die Grundfunktionen von `EnvGen4` (standalone, siehe [Abschnitt 8](#sec8)). Für den freien Zyklus, die Rate-CV, die Quadratur-Kopplung und die Sprungmeldung des Antialiasing liegen eigene Standalone-Tests in `tests/` (`test_autocycle.cpp`, `test_ratecv.cpp`, `test_quadratur.cpp`, `test_handshake.cpp`, `test_antialias_spruenge.cpp`). **Der Handshake ist im Standalone-Test (`tests/test_handshake.cpp`, elf Fälle) geprüft, der Funktionstest im Rack steht noch aus.** Ebenso ungetestet im Rack: das neue Tasterwidget `EuropaQuadButton` (Klick, Shift+Klick, Undo, LED). polyBLAMP für die Knicke an Stage-Übergängen ist vorerst zurückgestellt. Panel-Abbildung als SVG. **Phasor-Modus:** `tests/phasor_core_test.cpp` (standalone) deckt die Engine ab. Im Rack bestätigt (Kinovice, 2026-10-06): klassischer Modus unverändert, die Phasor-Modi und -Ausgänge, Panel-Layout, die CV auf p und q mit konstanter Spannung, strikte und lockere Ratio-Übernahme (LFO-Test siehe [3.10](#sec3-10)). **Offen im Rack:** die Curve-CV (LEVEL CV Kanal 5–8), die Offset-CV (TIME CV Kanal 5), die Durchschleifung des Phasors zwischen Generatoren, der Reset an TRIG Kanal 1 und die minBLEP-Meldung der Phasor-Sprünge.

---

<a id="sec7"></a>

## 7. Patch-Ideen
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**LFO mit Offset**<br>
Loop an, Delay-Pegel und Release-Pegel auf gleiche Werte (z. B. 3 V), Attack-Pegel z. B. 7 V: der Loop läuft zwischen zwei frei wählbaren Werten. Amplitude und Offset sind getrennt einstellbar.

**Ducking**<br>
Delay-Pegel 10 V, Attack-Pegel 0 V: die Hüllkurve senkt ab statt anzuheben.

**Kaskade**<br>
EOC eines Generators auf TRIG Kanal 2 eines anderen: Der zweite startet direkt in Attack, sobald der erste fertig ist. EOC → T1/GATE desselben Generators ergibt den Patch-Loop.

**Stage überspringen**<br>
Trigger auf TRIG Kanal 3 springt direkt zum Hold-Pegel ohne Anstieg.

**Externer Release**<br>
Trigger auf TRIG Kanal 4 beendet die Hüllkurve zu einem beliebigen Zeitpunkt, z. B. per Clock-Impuls.

**LFO mit exponentieller Frequenzsteuerung**<br>
LOOP auf FREE, ein Poly-Kabel mit fünf Kanälen am TIME-CV-Eingang, die Rate-CV auf Kanal 5: Der Generator läuft ohne Gate als LFO, dessen Frequenz mit einer Oktave pro Volt folgt. Ein LFO oder Sequencer auf Kanal 5 moduliert die Geschwindigkeit, die Kurvenform bleibt dabei unverändert.

**Zweistufige Hüllkurve (Quadratur + OR)**<br>
Taster von Paar 1+2 an, Gate an T1/GATE von Generator 1, Loop beider Generatoren auf OFF. Generator 2 startet, sobald Generator 1 seinen Attack-Pegel erreicht hat. Der OR-Ausgang 1+2 liefert die zusammengesetzte Kurve. Mit einer Delay-Zeit an Generator 2 lässt sich der Versatz einstellen.

**Viertelperioden-LFO (Handshake)**<br>
Taster von Paar 1+2 auf Handshake (LED bernstein), LOOP von Generator 1 auf FREE, LOOP von Generator 2 auf OFF, Attack und Release beider Generatoren gleich lang. ENV 1 und ENV 2 liefern zwei trapezförmige Steuerspannungen mit 90° Phasenversatz, z. B. für Panning oder gegenläufige Modulation. Die Periode ist die Summe aller vier Zeiten, ungleiche Zeiten verformen die Trapeze. Der OR-Ausgang 1+2 liefert bei gleichen Zeiten ein Trapez mit doppelt so breitem Plateau.

**ADSR-Form aus zwei Generatoren (OR)**<br>
Generator 1 im One-Shot mit Attack-Pegel 10 V und kurzem Release, Generator 2 im Sustain mit Attack-Pegel 5 V, beide mit demselben Gate. Das OR steigt auf 10 V, fällt auf 5 V zurück, hält 5 V und klingt beim Gate-Ende aus.

**Wavefolder-artige Effekte**<br>
Ein LFO auf LEVEL CV (Kanal 2–3), der über die Grenzen 0–10 V hinausschwingt, wird begrenzt und faltet sich am Rand zurück. Mit LFO-Frequenzen und Time-Ranges, deren Stage-Zeiten im Bereich der LFO-Periode liegen, und runden oder spitzen Kurven entstehen Wavefolder-ähnliche Verläufe. Konkrete Einstellungen (LFO-Frequenz, Time-Range, Kurvenstellung) hier ergänzen.

**Phasor-Sequencer**<br>
Phasor-Quelle an T1/GATE von Generator 1, PHASOR an, Ratio 1/4: Ein Hüllkurven-Zyklus über vier Eingangszyklen. Der EOC liefert einen Takt für den Folge-Sequencer. Mit den Gewichten bestimmst du, wie lang Delay, Attack, Hold und Release innerhalb des Zyklus sind, ohne dass sich etwas verschiebt.

**Polyrhythmische Hüllkurven**<br>
Ein Phasor-Kabel an Generator 1, alle vier Generatoren im Phasor-Modus (die Durchschleifung speist 2–4), Ratios 1/1, 3/2, 4/3, 5/4: vier synchrone Hüllkurven mit verschiedenen Zyklusdauern, die am Reset wieder zusammenfallen.

**Verformter Phasor als Taktquelle**<br>
OUT von Generator 1 auf „Phasor verformt“, ENV von Generator 1 an T1/GATE von Generator 2: Der zweite Generator bekommt einen Phasor mit ungleichmäßig laufender Zeit (Swing, Ritardando, Accelerando), eingestellt über die Gewichte und Curves von Generator 1.

**Ratio per Sequencer**<br>
Stufen-CV an TIME CV Kanal 6 und 7, lockere Ratio-Übernahme, Eingangsphasor schnell genug (zum Beispiel 8 Hz, mit q herunterteilt): Die Ratio folgt dem Sequencer in jedem Eingangszyklus. Ein LFO sollte höchstens ein Viertel der Eingangsfrequenz haben.

**Fallende Rampe als Rückwärtslauf**<br>
Ein fallender Phasor an T1/GATE: Die Hüllkurve beginnt weiter mit Attack, der Phasor-Ausgang fällt mit. So lässt sich ein rückwärts laufender Takt in einen normalen Hüllkurvenverlauf übersetzen.

**Curve per CV**<br>
Ein LFO oder Sequencer an LEVEL CV Kanal 5–8 verformt die Hüllkurve oder den verformten Phasor während des Laufs: Rund und spitz wechseln im Takt, in beiden Modi.

---

<a id="sec8"></a>

## 8. Design-Entscheidungen & Abgrenzungen
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Alle Entscheidungen am 2026-09-28 mit Kinovice abgestimmt und getestet, sofern nicht anders vermerkt. Die Entscheidungen vom 2026-10-01, 2026-10-02, 2026-10-05 und 2026-10-06 sind einzeln datiert.

- **Engine-Weg B (Kinovice):** eigene schlanke Klasse `EnvGen4`, `Afg`/Sofonisba bleibt unberührt. `Afg` ist auf 16 Stages und das gemeinsame `stageModifier[]`-Array ausgelegt, eine Generalisierung hätte Regressionsrisiko im featurekompletten Sofonisba bedeutet.
- **Rack-freie Engine:** `EnvGen4.hpp` bekommt bereits aufgelöste Werte (Sekunden, Volt) und liefert Volt. Dadurch standalone testbar (DSP-Testdatei geplant).
- **Zeitbereiche (Kinovice):** alle vier Bereiche beginnen bei 0 (exakt 0 im Linksanschlag), Obergrenzen wie Sofonisba (30 ms / 300 ms / 3 s / 30 s), exponentielle Reglerabbildung. Die Untergrenzen aus Sofonisba entfallen.
- **Generator-Umschaltung nach dem Lucia-Muster (Kinovice):** alle Params (inzwischen 98) sind echte Rack-Params, die GUI blendet nur den gewählten Generator ein. Verworfen: das im Konzept vorgesehene Edit-Param-Modell mit Pickup/Catch. Folge: kein Kopieren und kein Pickup/Catch nötig, MIDI-Map und Automation wirken auf jeden Generator.
- **`pendingGen`/`displayedGen` kein Param (Kinovice):** Undo, Automation und Preset-Recall sollen die reine GUI-Navigation nicht beeinflussen (gleiche Entscheidung wie bei Lucia).
- **Range als eigener Klick-Schalter (Kinovice-Test):** gerasterte Standard-Knobs mit nur 4 Stellungen ließen sich in Rack nicht bewegen (Racks Knob skaliert die Ziehdistanz auf den Wertebereich). Faustregel aus Sofonisba: kleine diskrete Zustandszahl → eigenes Widget. Eine eigene Kontextmenü-Checkliste braucht es hier nicht, Racks Standard-Menü listet die Bereiche bereits (eine zusätzliche Liste erschien doppelt, Kinovice-Test).
- **Zeit-Werteingabe in Sekunden (Kinovice-Test):** ohne Overrides von `getDisplayValue()`/`setDisplayValue()` landete eine eingegebene Zahl als Reglerposition und wurde auf 0/Anschlag geklemmt (gleiche Fehlerklasse wie bei Sofonisba/Tempo).
- **Eigene bipolare Curve-Abbildung, relativ zur Bewegungsrichtung (Kinovice):** statt der Sofonisba-Exp/Log-Bits. Grund: Bei fallenden Stages ergab die richtungsunabhängige Kurve einen spitzen Exponential-Abfall statt der erwarteten Glockenform. Links = rund (oben flach), rechts = spitz. Tooltip „curve (- round / + pointed)“.
- **Delay- und Release-Pegel einstellbar, unipolar 0–10 V (Kinovice):** vorher fest 0 V. Ermöglicht LFO mit Offset, invertierte Hüllkurven und Ruhepegel. Verworfen: bipolar, weil die VCA-Funktion negative Gains bekäme.
- **Retrig-Schalter springt auf den Delay-Pegel, Umbenennung „RETRIG D“ (Kinovice):** vorher Sprung auf 0 V („Retrig 0“). Interner Enum-Name bleibt.
- **LEVEL CV als eigene Buchse, Weg A (Kinovice):** poly, Kanal 1–4 = Delay/Attack/Hold/Release, additiv, Summe auf 0–10 V begrenzt. Verworfen: Kanäle 5–8 am TIME-CV-Eingang (kein Platzbedarf, aber unübersichtlicher). Folge: Buchsenblock zunächst mit 8 Spalten statt 7.
- **HOLD CV als eigene, ersetzende Buchse statt TIME-CV-Kanal 3 (Kinovice, 2026-09-28):** entdeckt beim Vergleich mit älteren Patches, im Original per CV 0–10 V ohne Gate-Bezug. Eigene Buchse statt Erweiterung des bestehenden Kanals, damit der Regler beim Patchen bewusst wirkungslos wird (klare Trennung „regler- vs. CV-gesteuert“) statt einer additiven Mischung. Abbildung weiter über TR1–TR4 (exponentiell), nicht linear, für Konsistenz mit allen anderen Zeit-Eingaben im Modul. Geltungsbereich auf One-Shot/Loop beschränkt, weil die Hold-Dauer im Sustain bereits eindeutig vom Gate bestimmt wird und eine CV-Übersteuerung dort das Konzept von Sustain verwässert hätte. Folge: Buchsenblock mit 9 Spalten.
- **TRIG als Strobe mit festen Zielen:** vergleichbar mit dem Strobe von Sofonisba (sofortiger Sprung), aber ohne Adresse: jeder Poly-Kanal hat seine feste Stage (2–4).
- **Status-LED mit unabhängigen Farbkanälen und normaler Überblendung (Kinovice-Test):** die RGB-Mischung von Rack ließ keine sauberen Farben zu, außerdem bleicht der Standard-Blendmodus auf dem hellen Panel aus.
- **Kein Oversampling:** Hauptnutzung ist LFO-Betrieb, der Audiobereich ein Zusatz. Oversampling würde die CPU-Last mit dem Faktor multiplizieren (vier Generatoren, polyphoner VCA) und wurde verworfen.
- **Optionales Antialiasing nur für Europa, minBLEP nur für Sprünge (Kinovice, 2026-10-02):** Nach einem Überblick über alle Module des Plugins ist Europa das einzige mit einem sinnvollen Audiorate-Fall: Die Sprünge der Hüllkurve (Zeit-0-Stages, Retrig) wirken über den VCA direkt auf Audio. Die übrigen Module sind CV- und Logikwerkzeuge (Gates und Trigger dürfen nicht gefiltert werden) oder erzeugen höchstens milde Knicke. Umsetzung: Die Engine meldet echte Sprünge und bleibt dabei rack-frei und im Ausgang bitgleich, minBLEP sitzt im Modul und korrigiert nur den Ausgang, nie die Engine-Zustände (sonst würde die Hüllkurve verfälscht). Das Ergebnis wird auf 0–10 V begrenzt (Überschwingen). Standard **aus**, weil minBLEP aus einem harten Sprung eine kurze, leicht überschwingende Flanke macht und das den Charakter scharfer Hüllkurven verändert. Alte Patches klingen unverändert. Verworfen: polyBLAMP für die Knicke (Gewinn klein, 1/f²-Abfall, vorerst zurückgestellt) und eine Rack-Param statt des Kontextmenüs. Standalone-Test `tests/test_antialias_spruenge.cpp`, Funktionstest im Rack am 2026-10-02: funktioniert (bei Delay 0 sauber, bei Delay 1 ms mit Schalter ein deutlich weniger Oberwellen im Spektrum).
- **Freier Zyklus als dritte Stellung des LOOP-Schalters (Kinovice, 2026-10-01):** statt eines eigenen Schalters oder zusätzlichen Params. Gründe: kein Platz für einen weiteren Schalter im Kopfbereich, keine neuen Enum-Einträge, die Werte 0 und 1 behalten ihre Bedeutung. Wirkt nur im One-Shot, im Sustain ohne Wirkung. Das Retrigger-Verhalten bleibt bewusst unverändert, ein Gate im freien Zyklus triggert wie gewohnt neu. Angeregt durch die Analyse eines öffentlichen Community-Threads zur Emulation von Hüllkurvengeneratoren aus der Buchla-Welt, in dem ein Zyklusmodus ohne Gate beschrieben ist. Die Referenzdokumentation ist nur Ideenquelle, die Umsetzung ist eigenständig. Standalone-Test `tests/test_autocycle.cpp`, Funktionstest im Rack durch Kinovice am 2026-10-01: funktioniert.
- **Rate-CV auf Kanal 5 des TIME-CV-Eingangs (Kinovice, 2026-10-01):** bipolar, exponentiell und formtreu. Statt einer eigenen Buchse liegt sie auf Kanal 5 des bestehenden polyphonen Eingangs (Vorschlag übernommen): Das Panel hatte schon neun Buchsenspalten, Kanalmuster gibt es bereits bei LEVEL CV und TRIG, gespeicherte Patches bleiben gültig. Reichweite eine Oktave pro Volt, begrenzt auf ±5 V (Vorschlag übernommen, beide Werte als Konstanten nachstellbar). Ein globaler Schalter für unipolare CVs wurde nicht umgesetzt. Standalone-Test `tests/test_ratecv.cpp` (Faktor-Abbildung und Begrenzung, Dauer ∝ 1/Rate, identische Kurvenform, stetiger Ausgang bei Rate-Wechsel, EOC-Zahl im freien Zyklus, unendliches Hold, alle Zeiten 0 bei ×32), Funktionstest im Rack am 2026-10-01: funktioniert („höchst interessante Modulation“).
- **OR-Ausgänge (Kinovice, 2026-10-01):** zwei Ausgänge, je Paar (1+2, 3+4) das Maximum der ENV-Pegel vor dem VCA. Ein Pegelregler entfällt, weil jede Stage bereits einen eigenen Pegel hat. Eigene Spalte rechts, das Panel wächst von 20 auf 23 HP (22 HP waren zu schmal, 24 HP zu breit), gestaltet wie die anderen Buchsenspalten (dunkle Platte 10 mm breit, schmaler heller Rahmen; `orPlateW`/`orFramePad` in `Europa.cpp`). Verworfen: eine zehnte Spalte im bestehenden Raster (drittes Verengen, Beschriftung zu knapp) und ein Kontextmenü statt Buchsen (würde die einzelnen Hüllkurvenausgänge von Generator 2 und 4 kosten). Funktionstest im Rack am 2026-10-01: funktioniert, Panel-Layout von Kinovice abgenommen.
- **Quadratur-Kopplung (Kinovice, 2026-10-01):** pro Paar schaltbar, Generator 2 (bzw. 4) startet am **Ende von Attack** von Generator 1 (bzw. 3). Verworfen als Startereignis: Beginn von Release und Zyklusende (EOC). Das genaue Verhalten der Hardware ist aus der Referenzdokumentation nicht eindeutig zu entnehmen, die Wahl ist eine eigene Festlegung. Der Schalter ist ein rastender LED-Taster in der OR-Spalte. Das eigene T1/GATE des Followers wird ignoriert, die Trigger für Stage 2–4 bleiben aktiv; im Sustain-Modus folgt der Follower dem Gate des Partners; bei aktiver Kopplung verhält sich FREE wie GATE (Vorschlag übernommen). Standalone-Test `tests/test_quadratur.cpp`, Funktionstest im Rack am 2026-10-01: funktioniert wie gebaut. Ein zunächst scheinbar vertauschter Start beruhte auf einem eingeschalteten Loop-Schalter, daher der Hinweis in Abschnitt 6. Der Schalter ist seit 2026-10-05 dreistufig (nächster Eintrag), die hier beschriebene Festlegung gilt unverändert als Stellung „Follow“.
- **Handshake als dritte Stellung des Quadratur-Tasters (Kinovice, 2026-10-05):** Anlass war eine öffentliche Beschreibung des Quadratur-Modus. Daraus ergab sich, dass die Quadratur ein Handshake aus vier Ereignissen ist (Attack-Ende A → B startet, Maximum B → A fällt, Minimum A → B fällt, Ende B → A startet neu), während Follow nur den ersten Schritt abbildete. Kinovice entschied sich für eine **dritte Stellung** statt Ersetzen, damit Follow und gespeicherte Patches (0/1) erhalten bleiben. Weitere Festlegungen: die Hold-Stage entfällt im Handshake (das Warten ersetzt sie), der Master hat keinen internen Loop und startet über das Zyklusende des Followers neu (entspricht dem Kabel „Pulse B → Trigger A“), ein loopender Follower läuft frei (Cycle-Fall der Quelle) und hält nur, wenn der Master wartet, im Sustain gilt automatisch Follow, ein externer Neutrigger des Masters startet einen haltenden Follower neu (sonst gegenseitige Blockade, im Test gefunden), das Halten endet beim Ausschalten des Handshakes. Der Taster ist ein eigenes Klick-Widget (Faustregel: kleine diskrete Zustandszahl), die LED wird aus dem Param gezeichnet, `QUAD_LIGHT` entfällt. Die Pulse- und Schwellen-Eigenheiten der Hardware werden nicht nachgebildet, die widersprüchliche Vierfach-Panning-Beschreibung der Quelle wurde nicht umgesetzt. Standalone-Test `tests/test_handshake.cpp` (elf Fälle: Viererzyklus, Periode und Viertelphase, ungleiche Zeiten mit Delay, Haltepegel, freilaufender Follower mit und ohne Master-Loop, Sustain-Rückfall, Ausschalten im Halten, Neutrigger, alle Zeiten 0, Follow unverändert) bestanden; `tests/test_quadratur.cpp` läuft unverändert weiter. Funktionstest im Rack steht noch aus.
- **Phasor-Modus (Kinovice, 2026-10-06):** Anlass war die Idee, am ENV-Ausgang einen Phasor zu erzeugen, der immer synchron zu einer externen Clock oder einem Phasor läuft (taktgenaue Modulation, phasorbasierter Sequenzer). Entscheidungen:
  - **Phasor-Eingang statt Clock-zu-Phasor:** Eine Periodenmessung zwischen Clock-Flanken ist wegen der Messzeit fehleranfällig (Kinovice-Erfahrung), ein Phasor-Eingang macht das Modul zustandslos bezüglich der Zeit. Verworfen: Clock → Phasor mit PLL.
  - **Vier unabhängige, zum Eingang synchrone Phasoren (Variante B):** jeder Generator läuft über den ganzen Eingangszyklus mit eigener Ratio, statt die vier Generatoren als Segmente eines Zyklus zu verketten (Variante A, Sequenzer).
  - **Moduswahl pro Generator per Taster mit Param:** T1/GATE bleibt im Normalfall der Trigger, der Phasor-Eingang nutzt dieselbe Buchse. Eine automatische Erkennung von Trigger, Gate und Phasor wurde verworfen, weil die Signale nicht zuverlässig zu unterscheiden sind (ein stehender Phasor sieht aus wie ein Gate, eine langsame Rampe wie ein Gate mit Flanke, ein schneller Phasor wie eine Triggerfolge, ein Fehlgriff würde mitten im Zyklus den Modus wechseln). `isConnected()` verrät nur, dass ein Kabel steckt.
  - **Stage-Zeiten als Gewichte:** Delay, Attack, Hold und Release teilen den Zyklus, statt eines Rise/Fall-Knickpunkts. Das passt zu den vier Stages von Europa, Pegel und Curve bleiben unverändert. Gewicht 0 lässt ein Segment entfallen, alle 0 ergeben Gleichverteilung.
  - **Ratio p/q mit freien Reglern (1..16)**, nicht als feste Liste. Entrollter Phasor mit Zykluszähler modulo 720720 (kgV 1..16), p/q gekürzt, ein Reset-Eingang (TRIG Kanal 1, in Europa bisher ungenutzt) legt die Phasenlage fest.
  - **Ratio-Übernahme am Eingangs-Wrap, strikt als Standard:** y bleibt stetig, der Preis ist die Wartezeit (zum Beispiel alle 12 Zyklen bei 4 → 6). Auf Wunsch (CV-Modulation) zusätzlich **locker** als modulweiter Kontextmenüpunkt (`ratioLoose`): sofortige Übernahme mit Phasensprung, mit minBLEP geglättet. Anlass waren Rack-Tests mit Rechteck-CV, bei denen die strikte Übernahme Änderungen verlor.
  - **Richtung:** Hysterese bei der Wrap-Erkennung. Bei fallender Rampe wird der Eingang gespiegelt, damit immer Rise vor Fall läuft und das EOC bei x = 0 liegt. Der Phasor-Ausgang wird zurückgespiegelt und bleibt richtungstreu (Empfehlung von Kinovice übernommen). Ein Schalter „Spiegeln bei fallender Rampe“ entfällt.
  - **ENV als Phasor-Ausgang statt neuer Buchsen:** drei Stellungen (Hüllkurve, Phasor linear, Phasor verformt), der Hüllkurvenpegel bleibt über VCA OUT und OR erreichbar, weil VCA IN auf 10 V normalisiert ist. Verworfen: neue Buchsen (das Panel hatte keine freie Spalte) und ein Menüpunkt statt eines Params.
  - **Phasor verformt = (k + curve(u)) / 4**, die Curve-Regler wirken also auch auf den Phasor-Ausgang (Kinovice-Wunsch), der lineare Phasor bleibt die saubere Referenz.
  - **CV-Belegung ohne neue Buchsen:** Curve auf LEVEL CV Kanal 5–8 (gilt in beiden Modi), Offset, p und q auf TIME CV Kanal 5–7 im Phasor-Modus (Kanal 5 ist dort nicht mehr die Rate-CV). Skalierung wie die Zeit-CV: additiv, 10 V = voller Regelweg (Curve ±5 V). p und q mit 0,1 Stufen Hysterese beim Runden.
  - **Durchschleifung des Phasors:** Ohne Kabel an T1/GATE nimmt ein Generator im Phasor-Modus den Phasor des vorherigen Generators, weil T1/GATE mono und pro Generator vorhanden ist (ein Kabel an Generator 1 speist alle vier).
  - **Unerklärte Sprünge werden an den minBLEP gemeldet** (Kinovice-Wunsch nach Hörtest): Reset, Offset-Sprung, Richtungswechsel und Phasensprung bei lockerer Übernahme. Kein EOC bei diesen Ereignissen.
  - **Panel:** PHASOR-Streifen über der OR-Spalte im bisher leeren Bereich, 23 HP bleiben, fünf Params je Generator am Enum-Ende. Verworfen: Panelverbreiterung. Das Tasterwidget des Quadratur-Tasters wird wiederverwendet (einstellbarer Undo-Name).
  - **Erkenntnis aus den Rack-Tests:** Die Ratio-CV wird nur mit der Frequenz des Eingangsphasors abgetastet (Faustregel: LFO höchstens ein Viertel). Zusätzliche Lesezeitpunkte (Ratio an jedem y-Wrap, 1–2 ms Einschwingfenster nach dem Wrap) wurden erwogen und bewusst nicht umgesetzt, weil ein schnellerer Eingangsphasor mit Teilung über q das Problem löst und das Verhalten sonst schwerer vorhersagbar würde.
  - **Build:** Rack baut mit C++11, daher ein eigener ggT (`phasorGcd()`) statt `std::gcd` (Build-Fehler im ersten Rack-Test). Standalone-Test `tests/phasor_core_test.cpp`. Funktionstest im Rack durch Kinovice am 2026-10-06 (Umfang siehe [Abschnitt 6](#sec6)).
- **Dokumentation abschließend (Kinovice):** erst alle Features, dann die Doku, inklusive Konzeptdokument-Update. Eigene Dokumentationsdatei für Europa, weil das Plugin fünf Module hat und Europa fachlich nichts mit der MARF-Kerndokumentation gemeinsam hat.
