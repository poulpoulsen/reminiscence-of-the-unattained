# Anna-Maria – Testszenario und Prüfliste (Deutsch)

Stand: 2026-10-09. Zu Modulversion 2.4.0 mit NL solver = Corrected als Standard. Alle Soll-Werte wurden mit demselben Signalweg wie im Modul berechnet (Pegelfaktor aus DRIVE, Modell, optionale Sättigung, Rückskalierung), Abtastrate 48 kHz, ohne Oversampling. Erzeugt von `tests/testplan/make_plan.py`; die Zahlen lassen sich mit `tests/testplan/expect.cpp` einzeln nachrechnen.

## 0. Aufbau und Konventionen

**Patch-Grundaufbau**

* VCV *Fundamental VCO* (Ausgang **SIN**) → *Anna-Maria* **AUDIO IN**. Der VCO-Sinus hat 5 V Spitze (10 Vpp). Zur Kontrolle denselben Ausgang zusätzlich auf Kanal 1 eines *Scope*.
* Anna-Maria **AUDIO OUT** → Kanal 2 desselben Scopes und → *Audio*-Modul.
* **CV IN bleibt unbelegt (= 0 V)**, außer es steht ausdrücklich etwas anderes da. Wirksam ist dann nur die Stellung von THRESHOLD: Das Gate ist offen, wenn THRESHOLD unter 0 V steht, und zu, wenn darüber (CV = 0 V).
* Für Gleichspannungen: eine einstellbare Quelle, z. B. *Fundamental SEQ3* (Stufe auf Zielspannung) oder ein MIDI-CC-Modul. Für Gates: *Fundamental LFO*, Rechteck, unipolar (0…10 V).
* Scope: Y-Skala so wählen, dass die Soll-Spitze etwa 3 Kästchen hoch ist; Trigger auf Kanal 1; Ablesegenauigkeit ±5 %. Frequenz am VCO-Regler einstellen (Tooltip zeigt Hz).

**Grundzustand („Basis“)** vor jedem Block herstellen (Modul neu einfügen oder Rechtsklick auf die Regler → Initialize):

| Element | Basis |
|---|---|
| THRESHOLD / DEPTH / FEEDBACK | 4,1 V / 50 % / 0 % |
| CV ATT | +100 % |
| MODE | BOTH (mittlere Stellung) |
| FB NL | aus |
| DRIVE | 5 |
| Menü Resonance | Normalized |
| Menü Vactrol response | Reference |
| Menü NL solver | Corrected |
| Menü Saturation | Off |
| Menü Oversampling | Off |
| Menü Channels | Auto |

Einstellungen gelten für **den angezeigten Kanal**. Im Kanal-Wähler zuerst den Kanal 1 wählen, ACTIVATE betätigen. Wo es heißt „alle Kanäle“, Rechtsklick auf den Regler → *Apply to all 16 channels*.

**Lesehilfe:** Mit *Spitze* ist der Maximalwert gegen 0 V gemeint (Eingang 5 V Spitze = 10 Vpp). *Gain* ist Ausgangsgrundwelle ÷ Eingangsgrundwelle. *Oberwellen* = Wurzel aus der Summe der Oberwellen 2…9 bezogen auf die Grundwelle.

## 1. Eingangsprüfung und Grundzustand

- [ ] **1.1 Eingang**: VCO-Sinus auf dem Scope = Sinus, **5 V Spitze** (±5 V, 10 Vpp). Wenn nicht, gelten alle Pegel unten proportional (lineare Tests) oder gar nicht (Tests mit NL/Sättigung).
- [ ] **1.2 Panel**: Alle Beschriftungen lesbar; 6 Elemente pro Kanal (THRESHOLD, DEPTH, FEEDBACK, CV ATT, MODE, FB NL), DRIVE, drei Buchsen IN/CV/OUT; Titel ANNA-MARIA.
- [ ] **1.3 Tooltips**: Mouseover zeigt je Regler Name inkl. Kanalnummer; DRIVE zeigt den Pegelfaktor in Volt (5 V bei 5).
- [ ] **1.4 Basis-Ausgang** (Eingang 220 Hz, 5 V Spitze, alles Basis, CV unbelegt): Ausgang = Sinus, unverzerrt, Frequenz 220 Hz, **Spitze 4,40 V** (-1,1 dB). Das Gate ist bei 0 V CV „zu“, im Modus BOTH geht der Bass fast ungedämpft durch.

## 2. Modi bei offenem Gate

Einstellungen für alle drei Tests: Basis, aber **THRESHOLD ganz links (−1,0 V)**; Eingang 220 Hz, 5 V Spitze; CV unbelegt (die Schwelle liegt unter 0 V, das Gate ist offen).

| ☐ | MODE | Erwartete Form | Erwartete Spitze | Gain | Wirkung des Elements |
|---|---|---|---|---|---|
| ☐ 2.1 | LOWPASS | Sinus, unverzerrt | 5,00 V | 0,0 dB | Nur Tiefpass (C3 = 4,7 nF), Gate offen: Höhen bei 12,95 kHz −3 dB, Pegel ≈ 1 |
| ☐ 2.2 | BOTH | Sinus, unverzerrt | 5,00 V | 0,0 dB | Tiefpass und VCA-Pfad, offen: Grenzfrequenz 19,4 kHz, Pegel ≈ 1 |
| ☐ 2.3 | VCA | Sinus, unverzerrt | 3,16 V | -4,0 dB | Reiner VCA, offen: Pegel 0,63 für alle Frequenzen |

Zusatz: Bei 5 kHz (VCO umstellen) ändert sich der Pegel offen nur leicht (LOWPASS ca. −4 %). Prüfwerte:

| ☐ | MODE | Spitze bei 5 kHz |
|---|---|---|
| ☐ 2.4 | LOWPASS | 4,79 V |
| ☐ 2.5 | BOTH | 4,97 V |
| ☐ 2.6 | VCA | 3,15 V |

## 3. Gate geschlossen ist nicht stumm (Durchlass)

Einstellungen: Basis (THRESHOLD 4,1 V, DEPTH 50 %, CV unbelegt → Gate zu, Rest-CV 0 V). Eingang 5 V Spitze, MODE wie angegeben, jeweils drei Frequenzen. Sollwerte (Spitze am Ausgang):

| ☐ | MODE | 100 Hz | 1 kHz | 5 kHz | Charakter |
|---|---|---|---|---|---|
| ☐ 3.1 | LOWPASS | 4,176 V (-2 dB) | 0,719 V (-17 dB) | 0,045 V (-41 dB) | nur Bass kommt durch, dumpf |
| ☐ 3.2 | BOTH | 4,657 V (-1 dB) | 2,294 V (-7 dB) | 0,456 V (-21 dB) | Bass und Mitten, Höhen gedämpft |
| ☐ 3.3 | VCA | 0,086 V (-35 dB) | 0,086 V (-35 dB) | 0,076 V (-36 dB) | leise, hell (alle Frequenzen gleich leise) |

Alle Ausgänge sind Sinus (unverzerrt). Wirkung: *MODE* wählt, ob bei zugehendem Gate die Höhen (LOWPASS), Höhen und Pegel (BOTH) oder nur der Pegel (VCA) verschwinden.

- [ ] **3.4 Tiefe CV**: CV −10 V an CV IN (Gleichspannung), MODE VCA, Rest Basis, 1 kHz, 5 V Spitze: Ausgang praktisch null. Soll: Spitze < 0,001 V (nicht sichtbar am Scope, etwa −110 dB).

## 4. THRESHOLD, DEPTH und CV ATT (Gate-Schwelle)

Einstellungen: MODE **VCA**, FEEDBACK 0, NL aus, Eingang 1 kHz, 5 V Spitze, CV unbelegt. Offene Gain 0,633 → **3,17 V Spitze**, Leck bei „zu“. Der Übergang von zu nach offen ist nur ca. 0,2 V CV breit, daher springt der Pegel in der Nähe der Schwelle.

**4a THRESHOLD-Reihe** (DEPTH 50 %, CV unbelegt = 0 V): Das Gate ist offen, sobald THRESHOLD unter 0 V steht. Darüber bestimmt der Abstand zwischen Schwelle und 0 V, wie viel Leck durchkommt.

| ☐ | THRESHOLD | Schwelle minus CV (0 V) | Erwartete Spitze | Zustand |
|---|---|---|---|---|
| ☐ 4a.1 | +9,0 V | 9,0 V | 0,000 V (-106 dB) | zu (Leck) |
| ☐ 4a.2 | +7,0 V | 7,0 V | 0,025 V (-46 dB) | zu (Leck) |
| ☐ 4a.3 | +5,0 V | 5,0 V | 0,065 V (-38 dB) | zu (Leck) |
| ☐ 4a.4 | +4,1 V | 4,1 V | 0,086 V (-35 dB) | zu (Leck) |
| ☐ 4a.5 | +3,0 V | 3,0 V | 0,113 V (-33 dB) | zu (Leck) |
| ☐ 4a.6 | +2,0 V | 2,0 V | 0,138 V (-31 dB) | zu (Leck) |
| ☐ 4a.7 | +1,0 V | 1,0 V | 0,165 V (-30 dB) | zu (Leck) |
| ☐ 4a.8 | +0,5 V | 0,5 V | 0,178 V (-29 dB) | zu (Leck) |
| ☐ 4a.9 | +0,0 V | 0,0 V | 1,590 V (-10 dB) | Übergang |
| ☐ 4a.10 | -0,5 V | -0,5 V | 3,164 V (-4 dB) | offen |
| ☐ 4a.11 | -1,0 V | -1,0 V | 3,164 V (-4 dB) | offen |

Hinweis: Bei −1,0 V (Anschlag links) liegt die Schwelle rechnerisch bei −0,88 V, weil der Regler dort an seine Grenze stößt. Das ändert nichts: das Gate ist offen.

**4b DEPTH-Reihe** (THRESHOLD **4,1 V** fest, CV unbelegt = 0 V, 4,1 V unter der Schwelle; die Schwelle bleibt beim Drehen von DEPTH stehen):

| ☐ | DEPTH | Erwartete Spitze | Zustand |
|---|---|---|---|
| ☐ 4b.1 | 0 % | 0,113 V (-33 dB) | zu (Leck) |
| ☐ 4b.2 | 25 % | 0,099 V (-34 dB) | zu (Leck) |
| ☐ 4b.3 | 50 % | 0,086 V (-35 dB) | zu (Leck) |
| ☐ 4b.4 | 75 % | 0,074 V (-37 dB) | zu (Leck) |
| ☐ 4b.5 | 100 % | 0,063 V (-38 dB) | zu (Leck) |

Wirkung: THRESHOLD setzt die CV, bei der das Gate öffnet (kleiner = früher). DEPTH ändert nicht die Schwelle, sondern wie tief das Gate weit unter der Schwelle schließt (höher = tiefer). In der Nähe der Schwelle ist der Unterschied klein. Das Gate öffnet beim Überschreiten der Schwelle innerhalb von etwa 0,2 V.

**4c CV ATT** (THRESHOLD 4,1 V, DEPTH 50 %, Gleichspannung **+10 V** an CV IN):

| ☐ | CV ATT | wirksame CV | Erwartete Spitze | Zustand |
|---|---|---|---|---|
| ☐ 4c.1 | +100 % | +10,0 V | 3,164 V (-4 dB) | offen |
| ☐ 4c.2 | +50 % | +5,0 V | 3,164 V (-4 dB) | offen |
| ☐ 4c.3 | +45 % | +4,5 V | 3,164 V (-4 dB) | offen |
| ☐ 4c.4 | +40 % | +4,0 V | 0,190 V (-28 dB) | zu (Leck) |
| ☐ 4c.5 | +20 % | +2,0 V | 0,136 V (-31 dB) | zu (Leck) |
| ☐ 4c.6 | +0 % | +0,0 V | 0,086 V (-35 dB) | zu (Leck) |
| ☐ 4c.7 | -100 % | -10,0 V | 0,000 V (-110 dB) | zu (Leck) |

Wirkung: CV ATT skaliert und invertiert die CV; −100 % dreht das Gate um (hohe CV = zu). Das Gate öffnet, wenn CV × ATT die Schwelle überschreitet (hier 4,1 V).

## 5. FEEDBACK (Resonanz)

Einstellungen: MODE **LOWPASS**, THRESHOLD **0,1 V**, DEPTH 50 %, CV unbelegt (Gate knapp unter der Schwelle: Grenzfrequenz −3 dB ≈ 400 Hz, Resonanzgipfel bei ≈ 1,1 kHz), NL aus. Eingang **1 V** Spitze (ändert wegen Linearität nur den Pegel), Frequenz 1,1 kHz. Menü Resonance = Normalized.

| ☐ | FEEDBACK | Erwartete Spitze (Eingang 1 V) | Gain | Wirkung auf den Klang |
|---|---|---|---|---|
| ☐ 5.1 | 0,00 | 0,327 V | -9,7 dB | keine Resonanz, weicher Tiefpass |
| ☐ 5.2 | 0,50 | 0,653 V | -3,7 dB | leichte Betonung um die Grenzfrequenz |
| ☐ 5.3 | 0,75 | 1,300 V | +2,3 dB | hörbare Resonanz |
| ☐ 5.4 | 0,90 | 3,160 V | +10,0 dB | deutliche Resonanz („Wah“-Charakter) |
| ☐ 5.5 | 0,95 | 5,774 V | +15,2 dB | stark, singend |
| ☐ 5.6 | 0,99 | 11,515 V | +21,2 dB | sehr stark, Pfeifen |

Form immer Sinus (NL aus). Die Resonanz erscheint **nur im Modus LOWPASS**: Gegenprobe:

- [ ] **5.7** MODE BOTH, FEEDBACK 0,9 gegenüber 0: Spitze unverändert (0,723 V gegen 0,723 V).
- [ ] **5.8** MODE VCA, FEEDBACK 0,9 gegenüber 0: Spitze unverändert (0,038 V gegen 0,038 V).

**Normiert gegen absolut** (Menü *Resonance*), LOWPASS, 1 kHz, 1 V Spitze, FEEDBACK 0,9:

| ☐ | THRESHOLD (Gate-Lage) | Normalized | Circuit-accurate |
|---|---|---|---|
| ☐ 5.9 | 0,1 V – knapp unter Schwelle, f = 1100 Hz | +10,0 dB | +9,4 dB |
| ☐ 5.10 | 4,1 V – geschlossen (CV 0 V, Schwelle 4,1 V), f = 500 Hz | +9,8 dB | +8,4 dB |
| ☐ 5.11 | 6,1 V – tief geschlossen, f = 250 Hz | +9,3 dB | +6,8 dB |

Wirkung: Beide Modi sind bei offenem/halb offenem Gate nahezu gleich; bei tief geschlossenem Gate fällt Circuit-accurate stärker ab (Soll-Differenz siehe Tabelle; bei noch tieferer CV, z. B. −4 V, verschwindet die Resonanz dort fast ganz). Die Frequenz des Gipfels bleibt hier nicht fest: Er wandert mit dem Gate. Die Prüffrequenz ist so gewählt, dass sie nahe am jeweiligen Gipfel liegt; bei Abweichungen > 3 dB Frequenz am VCO um ±20 % nachführen.

- [ ] **5.13 Stabilität**: FEEDBACK 1,0 (Normalized), kurzer Impuls oder Gate auf CV: kein Davonlaufen, kein Knacken, Ausgang bleibt endlich. Im Grenzfall bleibt eine kleine Schwingung stehen.

## 6. FB NL und DRIVE

Einstellungen: MODE **LOWPASS**, THRESHOLD 0,1 V, DEPTH 50 %, FEEDBACK 0,9, **FB NL an**, Saturation Off, Menü NL solver = Corrected. Eingang **1,1 kHz, 5 V Spitze** (Resonanzgipfel).

| ☐ | DRIVE | Pegelfaktor | Erwartete Spitze (+/−) | Oberwellen | Form |
|---|---|---|---|---|---|
| ☐ 6.1 | 0 | 20,00 V | +10,38 / -10,38 V | 2,2 % | Sinus, Spitzen minimal gerundet |
| ☐ 6.2 | 2,5 | 10,00 V | +7,67 / -7,67 V | 4,1 % | Sinus mit sichtbar abgeflachten Spitzen |
| ☐ 6.3 | 5 | 5,00 V | +5,46 / -5,46 V | 6,3 % | Sinus mit sichtbar abgeflachten Spitzen |
| ☐ 6.4 | 7,5 | 2,50 V | +3,88 / -3,88 V | 8,1 % | Sinus mit sichtbar abgeflachten Spitzen |
| ☐ 6.5 | 10 | 1,25 V | +2,86 / -2,86 V | 8,6 % | Sinus mit sichtbar abgeflachten Spitzen |
| ☐ 6.6 | 5, **FB NL aus** (Gegenprobe) | 5,00 V | +15,80 / -15,80 V | 0,0 % | Sinus, unverzerrt |

Wirkung: FB NL schaltet den tanh in der Rückkopplung zu. Die Verzerrung entsteht nur dort, wo die Resonanz das Signal anhebt. DRIVE bestimmt, wie weit der Pegel in das nichtlineare Gebiet reicht. Bei FB NL aus hat DRIVE **keine** Wirkung (6.6 ist bei jedem DRIVE gleich).

Zusatz offenes Gate ohne Resonanz (THRESHOLD −1,0 V, FEEDBACK 0, NL an, Eingang 220 Hz, 5 V Spitze), DRIVE 5 und 10:

- [ ] **6.7** DRIVE 5: Spitze 5,00 V, Oberwellen 0,00 % – Sinus, unverzerrt (praktisch unverändert, keine Resonanz).
- [ ] **6.8** DRIVE 10: Spitze 5,00 V, Oberwellen 0,00 % – Sinus, unverzerrt (praktisch unverändert, keine Resonanz).

Vergleich der NL-Solver (Menü *NL solver*), Einstellung wie 6.3 (DRIVE 5):

- [ ] **6.9** Corrected: Oberwellen 6,3 %, Spitze 5,46 V. Reference: Oberwellen 1,0 %, Spitze 4,12 V. Corrected soll deutlich stärker verzerren.

## 7. Sättigung (Menü Saturation) und DRIVE

Einstellungen: MODE **LOWPASS**, THRESHOLD −1,0 V (Gate offen), FEEDBACK 0, FB NL aus, **Saturation On**. Eingang **220 Hz, 5 V Spitze**.

| ☐ | DRIVE | Pegelfaktor | Erwartete Spitze | Oberwellen | Form |
|---|---|---|---|---|---|
| ☐ 7.1 | 0 | 20,00 V | 4,90 V | 0,5 % | Sinus, Spitzen minimal gerundet |
| ☐ 7.2 | 2,5 | 10,00 V | 4,62 V | 2,0 % | Sinus, Spitzen minimal gerundet |
| ☐ 7.3 | 5 | 5,00 V | 3,81 V | 6,7 % | Sinus mit sichtbar abgeflachten Spitzen |
| ☐ 7.4 | 7,5 | 2,50 V | 2,41 V | 17,3 % | deutlich abgeflachte Spitzen |
| ☐ 7.5 | 10 | 1,25 V | 1,25 V | 30,0 % | stark abgeflacht, annähernd trapezförmig |
| ☐ 7.6 | 7,5, **Saturation Off** | 2,50 V | 5,00 V | 0,0 % | Sinus, unverzerrt |

Wirkung: Saturation formt die Spitzen mit tanh; es entstehen nur ungeradzahlige Obertöne (3., 5., 7. …). Mit steigendem DRIVE sinkt der Ausgangspegel (Begrenzung auf den Pegelfaktor). 7.6 zeigt, dass ohne Saturation die Kurve rein bleibt. Der „warme“ Klang aus dem Hörtest liegt bei DRIVE ≈ 7,5.

Zusatz VCA-Modus (offene Gain 0,63 → kleinere Amplitude im Modell → schwächere Sättigung), DRIVE 7,5, Saturation On, THRESHOLD −1,0 V:

- [ ] **7.7** MODE VCA: Spitze 2,13 V, Oberwellen 9,7 % – Sinus mit sichtbar abgeflachten Spitzen.

**Frequenzabhängigkeit** (Saturation On, DRIVE 7,5, LOWPASS offen, 220 Hz gegen 2 kHz):

- [ ] **7.8** 220 Hz: Oberwellen 17,3 %, Spitze 2,41 V; 2 kHz: Oberwellen 17,2 %, Spitze 2,40 V. Die Sättigung ist frequenzunabhängig; kleine Abweichungen kommen vom Rückfalten (Aliasing) der hohen Oberwellen.

## 8. Vactrol-Zeitverhalten (Menü Vactrol response)

Einstellungen: MODE **VCA**, THRESHOLD 4,1 V, DEPTH 50 %, FEEDBACK 0. CV IN von *Fundamental LFO*, **Rechteck unipolar 0…10 V** (Ruhe 0 V, Gate 10 V). Eingang 1 kHz, 5 V Spitze; Scope auf die **Hüllkurve** (Zeitbasis 10…50 ms/Teilung, Ausgang gleichgerichtet gedacht). Offene Spitze = 3,17 V. Gate-Länge über LFO-Frequenz und Pulsweite einstellen.

| ☐ | Gate-Länge | Reference: erreichte Spitze (rel. zu offen) | State-based: erreichte Spitze (rel. zu offen) |
|---|---|---|---|
| ☐ 8.1 | 5 ms | 0,030 (≈ 0,10 V) | 0,183 (≈ 0,58 V) |
| ☐ 8.2 | 10 ms | 0,033 (≈ 0,11 V) | 0,642 (≈ 2,04 V) |
| ☐ 8.3 | 20 ms | 0,040 (≈ 0,13 V) | 0,991 (≈ 3,14 V) |
| ☐ 8.4 | 50 ms | 0,071 (≈ 0,23 V) | 0,999 (≈ 3,17 V) |
| ☐ 8.5 | 100 ms | 0,172 (≈ 0,55 V) | 0,999 (≈ 3,17 V) |
| ☐ 8.6 | 200 ms | 0,605 (≈ 1,92 V) | 0,999 (≈ 3,17 V) |
| ☐ 8.7 | 500 ms | 0,998 (≈ 3,16 V) | 0,999 (≈ 3,17 V) |

- [ ] **8.8 Anstieg** bei langem Gate (500 ms), Zeit von 10 % bis 90 % der offenen Hüllkurve: Reference 220 ms, State-based 11 ms. Das heißt: Reference zeigt eine träge Anstiegsflanke, State-based eine schnelle.
- [ ] **8.9 Abfall** nach einem 500-ms-Gate: Zeit bis 50 % / 10 % der Spitze nach Gate-Ende: Reference 0,0 ms / 0,4 ms, State-based 0,9 ms / 13,8 ms. Der Endwert ist die Ruhe-Gain (−35 dB bei Rest-CV 0 V).
- [ ] **8.9b** Rest-CV **3 V** (LFO-Offset, Rechteck 3…10 V), Gate 100 ms, State-based: Zeit bis 50 % / 10 %: 2,5 ms / 33,1 ms. Je näher die Rest-CV an der Schwelle liegt, desto länger der Ausklang.
- [ ] **8.10 Persistenz**: Menü auf State-based, Patch speichern, neu laden: Einstellung bleibt.

## 9. Polyphonie und Kanalwahl

Aufbau: *Fundamental VCO* auf 3 Kanäle (Polyphonie 3, z. B. Merge/Poly-Cable), 220 Hz, 5 V Spitze, alle Kanäle gleich; Anna-Maria **Channels = Auto**. Ausgang zu einem *Split*, drei Scope-Kanäle.

Einstellungen je Kanal (Kanal-Wähler + ACTIVATE): 

| ☐ | Kanal | MODE | THRESHOLD | Erwartete Spitze | Form |
|---|---|---|---|---|---|
| ☐ 9.1 | 1 | VCA | −1,0 V | 3,16 V | Sinus, unverzerrt |
| ☐ 9.2 | 2 | LOWPASS | −1,0 V | 5,00 V | Sinus, unverzerrt |
| ☐ 9.3 | 3 | BOTH | 4,1 V | 4,40 V | Sinus, unverzerrt |

- [ ] **9.4** Kanalzahl am Ausgang = 3 (Auto). Menü Channels = 1: nur Kanal 1; = 16: 16 Kanäle (Kanäle 4…16 mit Basiseinstellung, Eingang 0 V → Ausgang 0 V).
- [ ] **9.5** Kanal-Wähler zeigt die Einstellungen des gewählten Kanals; Umschalten ändert die anderen Kanäle nicht.
- [ ] **9.6** *Apply to all 16 channels* an THRESHOLD: alle Kanäle haben danach dieselbe Schwelle (ACTIVATE → kontrollieren). Kanäle 1…3 ergeben dann dieselben Spitzen.
- [ ] **9.7** Mono-Eingang auf Poly-CV: Ein Mono-Eingang wird auf alle Kanäle verteilt (z. B. Mono-Audio, 3 Kanäle CV).

## 10. Menü und Patch-Speicherung

- [ ] **10.1** Alle Menüeinträge vorhanden: Resonance, Vactrol response, NL solver, Saturation, Oversampling, Channels; jeder mit Häkchen beim aktiven Eintrag.
- [ ] **10.2** Neu eingefügtes Modul: Resonance Normalized, Vactrol Reference, **NL solver Corrected**, Saturation Off, Oversampling Off, Channels Auto.
- [ ] **10.3** Alle Menüwerte auf nicht-Standard stellen, Patch speichern, Rack neu laden: alles bleibt erhalten, ebenso die Reglerwerte aller 16 Kanäle (Kanal 7 vorher anders einstellen).
- [ ] **10.4** Rechtsklick auf Regler: *Initialize* setzt auf Standard; *Apply to all 16 channels* in der Liste vorhanden.

## 11. Oversampling, Rechenlast, Robustheit

- [ ] **11.1 Aliasing**: Saturation On, DRIVE 7,5, LOWPASS offen (THRESHOLD −1,0 V), Eingang **9 kHz**, 5 V Spitze. Die 3. Oberwelle (27 kHz) liegt über Nyquist (24 kHz) und klappt als **21 kHz**-Linie zurück. Soll: ohne Oversampling ist die 21-kHz-Linie sichtbar (Spektrum-Modul), mit Oversampling 2x deutlich kleiner (nicht berechnet, vergleiche den Abstand). Grundwelle 9 kHz: Amplitude 2,67 V, 21-kHz-Linie ohne Oversampling ca. 0,386 V (-17 dB gegen die Grundwelle).
- [ ] **11.2 CPU**: 16 Kanäle gleichzeitig, MODE LOWPASS, FB NL an. Engine allein ca. 100 ns pro Sample und Kanal gemessen (≈ 8 % eines Kerns bei 48 kHz), mit Oversampling 2x etwa doppelt. In Rack die Anzeige des Moduls vergleichen; bitte Wert notieren: ________.
- [ ] **11.3 Extremwerte**: Eingang ±10 V Spitze, DRIVE 10, FEEDBACK 1,0, FB NL an, Saturation On, CV zwischen −10 und +10 V springend: kein Knall, kein Dauerton aus NaN, Ausgang bleibt endlich (< 50 V).
- [ ] **11.4 Abtastrate**: Rack auf 44,1 kHz und 96 kHz stellen: Klang/Pegel bei Test 2.2 bleibt gleich (Pegel ±0,1 dB, Grenzfrequenz gleich).
- [ ] **11.5 Leerlauf**: AUDIO IN unbelegt: Ausgang 0 V, kein Rauschen.

## 12. Wirkung der GUI-Elemente auf das Signal (Übersicht)

| Element | Wirkt auf | Hörbare Wirkung | Prüfpunkte |
|---|---|---|---|
| THRESHOLD | CV-Pegel in Volt, bei dem das Gate öffnet | Niedrig = Gate öffnet früher bzw. ohne CV; hoch = nur große CV öffnet | 4a, 5, 3 |
| DEPTH | Tiefe des geschlossenen Gates | Hoch = Gate schließt weit unter der Schwelle tiefer; Schwelle bleibt gleich | 4b |
| FEEDBACK | Resonanz im Tiefpass (nur LOWPASS) | 0…0,5 kaum, 0,75…0,99 deutlich bis pfeifend | 5 |
| CV ATT | CV vor der Schwelle | skaliert und invertiert die Steuerung (−100…+100 %) | 4c |
| MODE | Signalweg | LOWPASS = Filter; BOTH = Filter + VCA; VCA = nur Pegel | 2, 3 |
| FB NL | tanh in der Rückkopplung | Verzerrung am Resonanzgipfel; ohne Wirkung in BOTH/VCA | 6 |
| DRIVE | Pegel im Modell | Mit FB NL/Saturation: mehr Verzerrung und weniger Ausgangspegel; sonst neutral | 6, 7 |
| Menü Resonance | Feedback-Skalierung | Circuit-accurate verliert Resonanz bei geschlossenem Gate | 5 |
| Menü Vactrol response | Zeitverhalten | State-based: schnelles Öffnen (~10 ms), Reference: ~220 ms | 8 |
| Menü NL solver | NL-Rechnung | Corrected: stärkere, richtige Verzerrung; Reference: schwächer | 6.9 |
| Menü Saturation | tanh nach dem Modell | warme, ungeradzahlige Obertöne; Intensität über DRIVE | 7 |
| Menü Oversampling | Rechenrate | weniger Aliasing, doppelte Last | 11.1, 11.2 |
| Menü Channels | Kanalzahl | Auto oder fest 1…16 | 9 |
| CHANNEL / ACTIVATE | Anzeige eines Kanals | kein Einfluss auf den Klang | 9 |

## 13. Abschluss

- [ ] Alle Abweichungen > 5 % (Pegel) oder > 3 dB (Resonanz) mit Block-Nr., Einstellung und gemessenem Wert notieren.
- [ ] Auffälligkeiten beim Hören notieren (Knacken, Zirpen, Pegelsprünge).
- [ ] Entscheidung zum Standard von *Vactrol response*: ☐ Reference  ☐ State-based.

**Hinweis zur Genauigkeit:** Die Sollwerte beschreiben das Rechenmodell (Paper + Octave-Referenz, mit den dokumentierten Änderungen), nicht das Originalgerät. Das Rechenmodell selbst ist gegen Octave auf 1e-13 geprüft; das Modul in Rack (float, Pegelfaktor, Polyphonie, Oberfläche) ist in diesem Plan noch ungeprüft.
