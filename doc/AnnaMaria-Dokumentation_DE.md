# Anna-Maria – Dokumentation (Entwurf, Deutsch)

Quad-Lowpass-Gate für VCV Rack, stark inspiriert von der wunderbaren Welt der Buchla-Module. Teil des Plugins „reminiscence of the unattained“, Version 2.4.0.
Stand: 2026-10-15. Status: Entwurf. Bis zum Hörtest in Rack gelten alle Klangaussagen als **gemessen am Modell**, nicht als gehört. Die Tests für den linearen Gate-Anteil (Abschnitt 5) und das Velocity-Ziel „Audio offset“ (Abschnitt 3a) in Rack stehen noch aus; das Panel mit vier Kanälen ist in Rack bestätigt.

Anna-Maria ist ein eigenständiges Werk und steht in keiner Verbindung zu den Herstellern der Vorbilder; alle Marken und Namen gehören ihren Inhabern.

Alle Zahlenwerte stammen aus Testprogrammen gegen die Engine (Abtastrate 48 kHz, ohne Oversampling, Modus „Reference“ für das Vactrol-Verhalten), sofern nicht anders angegeben. Die Abschnitte 4 bis 8 beschreiben das Paper-Modell (LOWPASS sowie BOTH und VCA mit „Gate response = Paper circuit“); was sich mit dem linearen Gate-Anteil ändert, steht in Abschnitt 5. Die Messungen zur Nichtlinearität (Abschnitt 7) sind ausdrücklich mit „Corrected“ bzw. „Reference“ bezeichnet.

---

## 1. Überblick

Anna-Maria bildet ein Lowpass Gate nach: Ein Tiefpass und/oder ein VCA, deren Öffnung von einer Steuerspannung (CV) über ein Vactrol (LED + lichtabhängiger Widerstand) gesteuert wird. Das Modell folgt dem Paper *Parker/D'Angelo, DAFx-13* und der Octave-Simulation `buchla_lp_gate.m` (Stefano D'Angelo, MIT). Die C++-Engine stimmt mit der Octave-Referenz auf 1e-13 bis 1e-15 überein (relativ, 7 Testfälle).

Besonderheiten dieses Moduls:

* **Vier getrennte, monophone Kanäle** (LPG 1–4) plus eine Spalte ALL mit globalen Elementen (31 HP). Jeder Kanal hat eigene Regler und CV-Eingänge für ATT, Velocity, THRESHOLD und FEEDBACK sowie eigene Schalter für MODE, FB NL und DEEP. Der Ausgang ALL summiert die vier Kanäle.
* **Steuerweg nach Hardware-Vorbild:** ATT ist ein Steuerpegel von 0…10 V, Velocity ein Gleichanteil darauf; eine gesteckte CV ersetzt jeweils den Regler (Abschnitt 3).
* **VCA und BOTH mit linearem Gate-Anteil** (Standard): VCA verhält sich wie ein linearer VCA, BOTH schaltet erst den linearen VCA und dann das Lowpass-Gate in Reihe (Abschnitt 5). Das Paper-Modell bleibt im Menü wählbar.
* Ein **DRIVE**-Regler für das ganze Modul (Pegel in das Modell, wirkt nur auf die Nichtlinearität) und ein **VACTROL**-Regler (Zeitfaktor des Vactrols, Abschnitt 8), beide in der Spalte ALL.
* Feedback wahlweise **normiert** (Standard) oder **schaltungsgetreu** (absolut).
* Modellvarianten im Kontextmenü: Vactrol-Verhalten (Abschnitt 8, Standard = Referenz), NL-Solver (Abschnitt 11, Standard = Corrected), Gate-Anteil und Filterstufe von BOTH (Abschnitt 5), Velocity-Ziel (Abschnitt 3a) und eine optionale Sättigung (Abschnitt 7a).

## 2. Signalfluss

Je Kanal (Paper-Modell, MODE LOWPASS):

```
Audio IN ─ ÷Pegelfaktor ─► [ Audiopfad: 2 Integratoren + Differenzierer,
                              Rückkopplung (optional tanh) ] ─► ×Pegelfaktor ─► Audio OUT
                                         ▲
                                         │ Rf (Vactrol-Widerstand)
ATT (Regler oder CV) ─┐
                      ├─ + ─ Begrenzung 0…10 V ─ + THRESHOLD (Offset) ─ Shelving ─ Kennlinie ─ LED-Strom ─ Vactrol (Tiefpass auf Rf)
Velocity (Regler/CV) ─┘
```

* Der Vactrol-Widerstand Rf bestimmt zugleich die Gain des VCA-Teils und die Grenzfrequenz des Tiefpasses.
* Offener Zustand: Rf ≈ 1,5 kΩ. Geschlossen: Rf bis in den MΩ-Bereich.
* **Steuerpegel:** `cv = begrenze(ATT + Velocity, 0 … 10 V)`. THRESHOLD wirkt als Offset **nach** dieser Begrenzung (Abschnitt 4).
* **Linearer Gate-Anteil (VCA, BOTH):** `g = cv / 10`, mit den Vactrol-Zeitkonstanten geglättet. VCA: `Ausgang = g · Eingang`, ohne Filter. BOTH: erst `g · Eingang`, danach die Filterstufe (Abschnitt 5).
* Nur beim Velocity-Ziel „Audio offset“ gilt es anders: Der Steuerpegel ist dann `begrenze(ATT, 0 … 10 V)`, und Velocity wird hinter dem Gate als Gleichanteil zum Signal addiert (Abschnitt 3a).

## 3. Bedienelemente und ihre Wirkung

Alle Regler und Schalter außer denen in der Spalte ALL gelten **pro Kanal** (vier Kanäle). Die Spalten ALL und LPG 1–4 tragen ihre Beschriftungen über den Elementen.

**Kanalspalte (LPG 1–4), Reihen von oben:**

| Element | Bereich / Standard | Kurzwirkung |
|---|---|---|
| IN | Audio | Eingang. IN 1 ist auf IN 2–4 normalisiert, wenn dort nichts steckt. |
| OUT | Audio | Kanalausgang (mono). |
| ATT | 0…10 V, 0 V | Steuerpegel des Gate. Eine gesteckte ATT-CV ersetzt den Regler. |
| VEL | −10…+10 V, 0 V | Velocity, Gleichanteil auf dem Steuerpegel. Eine gesteckte Velocity-CV ersetzt den Regler. |
| THRESH | 0…7,5 V, 7,5 V | Schwelle des Gate (Paper-Modell). Eine gesteckte THRESHOLD-CV ersetzt den Regler. |
| FEEDBACK | 0…100 %, 0 % | Resonanz. Eine gesteckte FEEDBACK-CV ersetzt den Regler. |
| FB NL | aus/an, aus | Schaltet die tanh-Nichtlinearität im Feedback ein. |
| MODE | LOWPASS / BOTH / VCA, BOTH | Betriebsart (Abschnitt 5). |
| DEEP | aus/an, aus | Platzhalter, ohne Funktion. |

**Spalte ALL (global):**

| Element | Bereich / Standard | Kurzwirkung |
|---|---|---|
| ALL OUT | Audio | Summe der vier Kanäle, danach weicher Begrenzer auf ±10 V (linear bis ±7 V). Kein Teilen durch die Kanalzahl. |
| MODE | LOWPASS / BOTH / VCA, BOTH | Schreibt den Modus in alle vier Kanalschalter; danach sind die Kanalschalter wieder einzeln bedienbar. |
| MODE CV | 0…10 V | Wenn gesteckt, bestimmt sie den Modus **aller** Kanäle und geht den Schaltern vor: unter 3,3 V = VCA, unter 6,6 V = LOWPASS, darüber BOTH. |
| DRIVE | 0–10, 5 | Pegel in das Modell (Abschnitt 7). |
| VACTROL | ×0,1…×10, Mitte = ×1 | Zeitfaktor des Vactrols (Abschnitt 8). |
| DEEP | aus/an, aus | Platzhalter, ohne Funktion. |

**Kontextmenü (Rechtsklick):** DEPTH je Kanal (Untermenü „Depth“ mit einem Schieber für alle vier Kanäle und je einem für Kanal 1–4, Standard 100 %) sowie die Modell-Einstellungen (Abschnitt 10). Rechtsklick auf einen Kanalregler oder -schalter bietet „Apply to all 4 channels“.

**Wirkung im Einzelnen:**

* **ATT.** Der Steuerpegel des Gate in Volt. Im Standard ist der ATT-Regler (oder die gesteckte ATT-CV) die Quelle des Steuerpegels `cv`; beim Paper-Modell öffnet das Gate, wenn `cv` die Schwelle überschreitet, beim linearen Gate-Anteil ist die Verstärkung `cv / 10`. Eine gesteckte CV wird nicht skaliert (1 V pro V), sie ersetzt den Regler.
* **VEL (Velocity).** Ein Gleichanteil, der zum Steuerpegel addiert wird (Regler wie CV). Die Summe `ATT + Velocity` wird auf 0…10 V begrenzt. Eine negative Velocity verkleinert den Bereich, den ATT erreichen kann; mit ATT 5 V und Velocity −5 V ist das Gate geschlossen. Alternativ lässt sich Velocity als Gleichanteil auf dem Audiosignal einstellen (Abschnitt 3a).
* **THRESHOLD.** Der **Steuerpegel in Volt, bei dem das Gate halb offen ist**, im Paper-Modell. Kleiner = Gate öffnet früher bzw. schon bei kleinem ATT; 7,5 V = das Gate öffnet erst bei sehr hohem Steuerpegel. Ein gesteckter THRESHOLD-CV-Eingang ersetzt den Regler (1 V pro V, begrenzt auf 0…7,5 V; positive CV = höhere Schwelle = Gate schwerer zu öffnen). Intern: Offset und Schwellenformel, Abschnitt 4. Im linearen VCA hat THRESHOLD keine Wirkung, in BOTH mit Lowpass-Stufe wirkt er auf deren Filteranteil.
* **FEEDBACK.** Resonanz, siehe Abschnitt 6. FEEDBACK-CV: 0…10 V entsprechen 0…100 %, begrenzt. Beide CV-Eingänge (THRESHOLD, FEEDBACK) wirken pro Sample und sind auch mit Audiorate nutzbar; das kostet dann mehr Rechenzeit, weil sich die Filterkoeffizienten in jedem Sample ändern.
* **DEPTH (Kontextmenü).** Wie tief das Gate **weit unter der Schwelle** schließt (Leck): 3 V darunter −31 dB (0 %) bis −35 dB (100 %), 6 V darunter −36 dB bis −49 dB (Paper-VCA). Die Schwelle selbst bleibt beim Drehen stehen. Intern: Trim. Standard 100 %.
* **FB NL.** Schaltet die tanh-Nichtlinearität im Feedback ein (Abschnitt 7). Wirkt dort, wo FEEDBACK wirkt (Abschnitt 6).
* **MODE.** Siehe Abschnitt 5.
* **DRIVE.** Pegelfaktor: Eingangsspannung, die der Modellamplitude 1 entspricht = 5·2^(2·(5−Drive)/5) V, also 20 V (0), 5 V (5), 1,25 V (10). Das Ausgangssignal wird zurückskaliert; **bei ausgeschalteter NL ändert DRIVE den Klang nicht**. Mit NL: mehr Drive = mehr Obertöne und weniger Pegel (Abschnitt 7).
* **VACTROL.** Zeitfaktor auf beide Vactrol-Zeitkonstanten (Anstieg 2,4 ms, Abfall 50 ms bei ×1 = Werte des Papers). Links = schnell, das Gate folgt der CV; rechts = langsam, hüllkurvenartig (Abschnitt 8). Ein Regler für alle Kanäle. Er stellt auch die Trägheit des linearen Gate-Anteils ein (Annahme, Abschnitt 5).
* **Kanäle ohne Quelle.** Ein Kanal wird nicht gerechnet, wenn weder sein eigener IN noch IN 1 belegt ist; sein Ausgang liefert dann 0 V. Ein Kanal ohne OUT-Kabel wird übersprungen, solange ALL nicht belegt ist.

### 3a. Velocity-Ziel: „Control level“ oder „Audio offset“

Im Kontextmenü legt **Velocity target** fest, wo Velocity wirkt. Die Einstellung gilt für alle Kanäle und Modi.

* **Control level** (Standard, Hardware-Vorbild): Velocity wird zum Steuerpegel addiert, `cv = begrenze(ATT + Velocity, 0 … 10 V)`. Das Audiosignal sieht nie einen Gleichanteil. Eine negative Velocity macht das Signal also nur leiser (im linearen VCA) bzw. verschiebt die Öffnung des Gate; abgeschnitten wird nichts.
* **Audio offset:** ATT bzw. ATT-CV bleibt der Steuerpegel (ohne Velocity). Velocity wirkt als Gleichanteil auf dem Signal **nach** dem Gate (also nach der Dämpfung durch ATT), am Kanalausgang vor der Summe ALL:
  * Velocity 0 = keine Wirkung; der Übergang ist fließend.
  * Das Signal wird um die Velocity-Spannung verschoben und in Verschieberichtung bei ±10 V (Rack-Bereich) geschnitten: bei negativer Velocity unter −10 V, bei positiver über +10 V. Ein Signal, das schon über der Grenze liegt, wird nicht zusätzlich beschnitten.
  * Der Offset wirkt nur bei offenem Gate, dann mit vollem Wert: Das Gewicht ist `begrenze(cv · 2, 0, 1)` (0 bei ATT 0 V, voll ab ATT 0,5 V), mit den Vactrol-Zeiten geglättet, damit der Offset nicht knackt. Die 0,5 V sind eine Annahme.
  * Der Offset ist ein Gleichanteil am Ausgang; ALL enthält die Offsets aller vier Kanäle. In LOWPASS und im Paper-Gate dient ATT als Maß der Öffnung, nicht der echte Filterzustand.

Beispiele (Gate offen): Signal nach ATT ±2 V, Velocity −4 V → −6 / −2 V. Signal ±5 V, Velocity −4 V → +1 / −9 V; Velocity −7 V → −2 / −10 V (Schnitt); Velocity +1 V → +6 / −4 V; Velocity +7 V → +10 / +2 V (Schnitt). Bei ATT 0 V: kein Offset.

## 4. Gate-Verhalten: THRESHOLD, DEPTH und Steuerpegel

Dieser Abschnitt beschreibt das Paper-Modell: LOWPASS sowie BOTH und VCA mit „Gate response = Paper circuit“. In BOTH mit linearem Gate-Anteil und Lowpass-Stufe gelten die Schwellenwerte weiter für den Filteranteil.

**Bedienung.** Das Modell kennt intern zwei Größen, *Offset* (addiert 10 V × Offset zum Steuerpegel) und *Trim* (Vorwiderstand der LED). Zusammen legen sie die Gate-Schwelle fest. Am Panel stehen stattdessen **THRESHOLD** (die Schwelle in Volt) und **DEPTH** (Trim, wie tief das Gate schließt, im Kontextmenü). Das Modul rechnet den Offset aus der gewählten Schwelle: `Offset = (T(Trim) + 0,05 V − THRESHOLD) / 10 V`. Der Regelweg von THRESHOLD ist 0…7,5 V, Standard 7,5 V. Bei DEPTH 100 % (Standard) ist der Offset bei 7,5 V gleich 0 (die höchste erreichbare Schwelle ist dort 7,41 V), das Gate ist ohne Steuerspannung also wirklich zu. Bei kleinerem DEPTH liegt die Schwelle bei Offset 0 höher (T(Trim)); THRESHOLD 7,5 V bedeutet dann schon einen kleinen Offset. Wird der Bereich verlassen, begrenzt das Modul den Offset, und der Tooltip nennt die wirksame Schwelle. Die Umrechnung ist gegen die Engine geprüft (36 Kombinationen, größte Abweichung 0,008 V).

Alte Patches (OFFSET/TRIM, Panel mit 16 Kanälen) werden beim Laden umgerechnet; Gain und Schwelle bleiben, nur die Regler sehen anders aus. Bei Patches vor dem Hardware-Signalweg (ATT als Verstärkung ±400 %) wird ATT beim Laden auf 0 V gesetzt; Patches mit CV-Kabel klingen anders, weil die CV nicht mehr skaliert wird.

**Wie die Schwelle zustande kommt.** Das Gate öffnet, wenn `Steuerpegel + 10 V × Offset` etwa 9,12 V erreicht (bei DEPTH 50 %). Die 9,12 V sind durch die Schaltung vorgegeben (Vorwiderstand und Verhalten der LED-Stufe). Der Offset-Zweig liefert höchstens 10 V. Daraus folgt `Schwelle = 9,12 V − 10 V × Offset`:

| Offset (intern) | Schwelle | Bedeutung |
|---|---|---|
| 0 | +9,1 V (DEPTH 50 %) | Gate öffnet erst bei 9,1 V Steuerpegel |
| 0,5 | +4,1 V | Mittellage |
| 0,9 | +0,1 V | Gate gerade noch zu bei 0 V Steuerpegel |
| 1,0 | −0,9 V | Gate bei 0 V schon offen (liegt außerhalb des Regelwegs 0…7,5 V) |

„Schwelle“ ist hier die Höhe des Steuerpegels, bei der das Gate halb offen ist, ein Vergleichswert und kein Signalpegel. Da der Steuerpegel des Moduls nie unter 0 V fällt, gibt es am Panel keine negative Schwelle mehr.

**Woher T(DEPTH) kommt.** DEPTH wird als Reglerweg 0…100 % angezeigt (Wert 0…1) und ist keine Spannung. Es ist der Trimmer des Vorbilds (TP1, 20 kΩ), der den LED-Vorwiderstand verändert. Aus ihm berechnet das Modul die Bezugsschwelle T in Volt:

`T = 390 000 / (20 000 · DEPTH + 33 000)`

| Wert | Bedeutung |
|---|---|
| 390 000 | Zähler (V·Ω): nach dem Paper die Zenerspannung D1 (3,9 V) mal R5 (100 kΩ). Er legt die Gesamtskala fest. |
| 33 000 | R7 (33 kΩ), der feste Anteil des Vorwiderstands; er bleibt auch bei DEPTH 0 % bestehen. |
| 20 000 · DEPTH | R6 = der Trimmer. Bei DEPTH 0 % ist er ganz draußen, bei 100 % liegen die vollen 20 kΩ im Vorwiderstand. |
| Ergebnis T | Steuerpegel in Volt, bei dem der Knick der LED-Kennlinie erreicht wird (Ergebnis: Volt, weil Zähler V·Ω durch Ω geteilt wird). |

**Richtung:** Mehr DEPTH macht den Nenner größer und T kleiner: Das Gate öffnet bei weniger Steuerspannung, also empfindlicher. Werte: DEPTH 0 % → T = 11,82 V, 50 % → 9,07 V, 100 % → 7,36 V. Mit dem Zuschlag von 0,05 V (THR_HALF) ergeben sich 11,86 V, 9,12 V und 7,41 V; das sind die Schwellen bei Offset 0. Liegt THRESHOLD darüber, ist der Offset 0, darunter wird die Differenz als Offset auf den Steuerpegel addiert.

**Physikalischer Hintergrund.** Der Vactrol-Widerstand fällt im Original nicht gleichmäßig mit der Steuerspannung. Die LED-Kennlinie hat einen Knick dort, wo der Strom Ia = VB / (R6 + R7) erreicht wird. Darunter leuchtet die LED kaum, und der Widerstand bleibt hoch (Überschlag ca. 64 kΩ). Wenige Zehntel Volt darüber fällt er steil auf wenige kΩ (voll offen). T ist die Steuerspannung, bei der dieser Knick erreicht wird: Der Trimmer verschiebt den Knickstrom, das Modul rechnet den Abstand zwischen Knick und THRESHOLD in einen Offset auf den Steuerpegel um. Im Modell ist das Gate bei dieser Spannung halb offen und ca. 0,1 V darüber voll offen (Überschlag, nicht am Bauteil gemessen). Die Zuordnung der Zahlen zu den Bauteilen D1, R5, R6 und R7 ist aus dem Paper abgeleitet, nicht am Quelltext geprüft.

**Wofür THRESHOLD nützlich ist:**

* **Schwelle in die Hüllkurve legen.** Eine Hüllkurve von 0…10 V an der ATT-CV öffnet das Gate erst, wenn sie die Schwelle überschreitet. Kleinerer THRESHOLD = früher und länger offen; größerer = nur der obere Teil der Hüllkurve öffnet.
* **Gate ohne Hüllkurve offen halten.** Bei THRESHOLD 0 V liegt der Halböffnungspunkt bei Steuerpegel 0; mit etwas ATT oder positiver Velocity ist das Gate dann offen (Dauerton, offenes Filter).
* **Bereich verschieben.** Velocity (negativ) verkleinert den Bereich des ATT-Reglers, THRESHOLD legt fest, ab welchem Steuerpegel das Gate öffnet; beide zusammen bestimmen, wo am ATT-Regler der Übergang liegt.
* **Moduliert, auch mit Audiorate.** Der THRESHOLD-CV-Eingang (1 V pro V, 0…7,5 V) verschiebt die Schwelle pro Sample.

Vorsicht: Bei THRESHOLD knapp über 0 V (±0,1 V) liegt die Schwelle so nah am Steuerpegel 0 V, dass das Gate bei ATT 0 V halb offen bleibt. Der Tooltip nennt für den eingestellten Wert, ob das Gate bei ATT 0 V offen, halb offen oder geschlossen ist.

Die folgenden Tabellen verwenden die internen Größen Offset und Trim; Umrechnung THRESHOLD = T(Trim) + 0,05 V − 10 V · Offset.

Effektive Schwelle (Verstärkung 50 % der offenen Gain), gemessen im VCA-Modus bei 1 kHz:

`CV_Schwelle ≈ T(Trim) − 10 V · Offset`, mit `T = 390 000 / (R6 + R7)`, `R6 = 20 kΩ · Trim`, `R7 = 33 kΩ`.

| Offset \ Trim | 0,0 | 0,5 | 1,0 |
|---|---|---|---|
| 0,00 | nie (> 10 V) | 9,12 V | 7,40 V |
| 0,25 | 9,38 V | 6,62 V | 4,90 V |
| 0,50 | 6,88 V | 4,12 V | 2,40 V |
| 0,75 | 4,38 V | 1,62 V | −0,10 V |
| 1,00 | 1,88 V | −0,88 V | −2,60 V |

Das heißt: Bei Offset 0 und Trim 0 öffnet das Gate im Bereich ±10 V nie (am Panel: THRESHOLD +11,9 V, außerhalb des Regelbereichs).

**Kennlinie** (Offset 0,5, Trim 0,5, VCA, 1 kHz):

| CV [V] | Rf | Gain | dB |
|---|---|---|---|
| −4 | 1352 kΩ | 0,0013 | −57 |
| −2 | 291 kΩ | 0,0083 | −42 |
| 0 | 143 kΩ | 0,0171 | −35 |
| 2 | 90 kΩ | 0,0271 | −31 |
| 4 | 63,5 kΩ | 0,0379 | −28 |
| ≥ 4,5 | 1,5 kΩ | 0,633 | −4 |

* **Unterhalb der Schwelle** (Leckbereich) steigt die Gain nur langsam: zwischen −1 V und 4 V etwa 1,5–3 dB pro Volt, darunter steiler (−4 → −2 V: 16 dB). Das Gate ist „zu“, aber nicht stumm: bei CV 0 V noch −35 dB.
* **Die Öffnung geschieht abrupt**: zwischen 10 % und 90 % der offenen Gain liegen nur **0,19 V** (4,07…4,26 V). Ein CV-Signal, das die Schwelle überschreitet, schaltet also fast wie ein Schalter; sanfte Hüllkurven wirken nur im Leckbereich und in diesem schmalen Übergang.
* **Offene Gain**: VCA 0,633 (−4 dB), Lowpass 1,009, unabhängig von Offset und Trim.
* Die CV wird intern auf ±10 V begrenzt.

Praktische Folge: Eine Hüllkurve von 0…10 V an der ATT-CV öffnet das Gate bei THRESHOLD 4,1 V erst nach 41 % ihres Anstiegs und schließt es wieder bei 4,1 V (Offset 0,5, Trim 0,5; im Panel mit DEPTH 50 %). Mit THRESHOLD legt man fest, wo.

### 4b. THRESHOLD (Offset), DEPTH (Trim), ATT und Velocity im Detail

Messwerte: `tests/doku_offset_trim_att.cpp`, Ausgabe `tests/doku_offset_trim_att_ausgabe.txt`.

**Das Prinzip.** Das Gate hat intern genau eine Steuergröße, die „wirksame Steuerspannung“. ATT, Velocity, THRESHOLD und DEPTH bestimmen, wie diese entsteht und ab wann das Gate öffnet:

```
ATT (Regler oder CV) ─┐
                      ├─ + ─► begrenzt auf 0…10 V ─► Shelving ─┐
Velocity (Regler/CV) ─┘                                         ├─► Strom ─► LED ─► Vactrol-Widerstand Rf ─► Filter/VCA
THRESHOLD ─► 10 V × Offset (Gleichanteil) ──────────────────────┘            ▲
DEPTH ─► Vorwiderstand der LED (verschiebt die Schwelle) ───────────────────┘
```

Die Schwelle (Gate zu 50 % offen) liegt bei **Steuerpegel ≈ T(Trim) − 10 V·Offset**, mit T = 11,82 V (Trim 0), 9,07 V (0,5), 7,36 V (1).

**Das Gate ist in diesem Modell eher ein Schalter als ein Regler.** Es gibt drei Bereiche:

| Bereich | Lage relativ zur Schwelle | Filter (LOWPASS) | VCA |
|---|---|---|---|
| **offen** | über Schwelle + 0,1 V | Grenzfrequenz 12,95 kHz, Gain 1,0 | Gain 0,633 |
| **Übergang** | ± 0,1 V um die Schwelle (10…90 % in 0,17…0,22 V) | Grenzfrequenz von ≈ 400 Hz bis 13 kHz | Gain von 0,04 bis 0,63 |
| **zu (Leck)** | darunter | Grenzfrequenz 383 Hz (0,5 V unter), 296 Hz (2 V), 192 Hz (4 V), 39 Hz (8 V); Messwerte Trim 0,5 | −29 dB (0,5 V unter), −31 dB (2 V), −35 dB (4 V), −56 dB (8 V) |

Das heißt: Im Bereich „zu“ bewegt sich der Klang nur langsam (nahe der Schwelle etwa eine Oktave Grenzfrequenz pro 4 V, weiter unten schneller), und beim Überschreiten der Schwelle öffnet sich das Gate in einem Schritt. Weiche Filterfahrten entstehen daher vor allem durch das **Vactrol** (Anstieg 2,4 ms / Abfall 50 ms Zeitkonstante, Abschnitt 8), nicht durch eine langsame CV. Eine langsam laufende CV (LFO, Hüllkurve) wird erst „hörbar“, wenn sie die Schwelle kreuzt.

**THRESHOLD (intern: Offset).**
* Wirkung auf das Signal: Reine Verschiebung. Gemessen: Gain bei (CV −3 V, Offset 0,5), (CV 0 V, Offset 0,2) und (CV −6 V, Offset 0,8) ist identisch (0,00457), alle haben CV + 10·Offset = 2 V.
* Offset ohne CV-Kabel ist ein Handregler für das Gate. Bei Trim 0,5 ist das Gate ab Offset ≈ 0,91 offen (Schwelle 9,07 V). Darunter ist es „zu“, und der Regler verändert nur das Leck: LOWPASS-Grenzfrequenz 26 Hz (Offset 0), 64 Hz (0,2), 139 Hz (0,4), 236 Hz (0,6), 346 Hz (0,8), 404 Hz (0,9); VCA −110 dB (0), −47 dB (0,2), −38 dB (0,4), −33 dB (0,6), −30 dB (0,8), −28 dB (0,9). Bei 1,0: offen.
* Mit CV-Kabel verschiebt Offset die Lage der Schwelle gegenüber der Hüllkurve. Hoher Offset = Gate öffnet früher im Anstieg und schließt später im Abfall (längere Öffnungszeit); niedriger Offset = Gate öffnet erst bei hoher CV (kurze Öffnungszeit).
* Dynamik: Offset ist eine Gleichgröße und läuft **nicht** durch das Shelving-Filter; schnelle CV-Flanken bekommen dagegen einen kurzen Überschwinger (Abschnitt 8).
* Die Offset-Spannung wird nach der ±10-V-Begrenzung der CV addiert; wirksam sind also bis zu 20 V. Velocity dagegen wirkt vor der Begrenzung (sie ist Teil des Steuerpegels, Abschnitt 3).

**DEPTH (intern: Trim).**
* Wirkung 1: verschiebt die Schwelle um bis zu 4,5 V (T = 11,82 → 7,36 V), hoher Trim = Gate öffnet früher. Bei Offset 0,5 liegt die Schwelle bei 6,88 / 5,32 / 4,12 / 3,17 / 2,40 V für Trim 0 / 0,25 / 0,5 / 0,75 / 1.
* Wirkung 2: Trim ändert auch das Leck. Je höher Trim, desto stärker fällt das Gate **weit** unter der Schwelle ab: 6 V unter der Schwelle −36 dB (Trim 0), −41 dB (0,5), −49 dB (1); LOWPASS-Grenzfrequenz 8 V unter der Schwelle 101 Hz (Trim 0), 39 Hz (0,5), 26 Hz (1). In der Nähe der Schwelle ist der Unterschied klein (1 V darunter: −29 / −30 / −30 dB; 3 V darunter: −31 / −33 / −35 dB für Trim 0 / 0,5 / 1).
* Wirkung 3: Der Übergang wird mit hohem Trim minimal steiler (0,22 V bei Trim 0, 0,17 V bei Trim 1).
* Faustregel am Panel: **THRESHOLD legt die Schwelle fest** (passend zur Höhe der Hüllkurve), **DEPTH bestimmt nur, wie tief das Gate darunter schließt**. Wer eine feste Hüllkurve von 0…10 V hat, stellt THRESHOLD so, dass die Schwelle in der Hüllkurve liegt, und wählt mit DEPTH, wie tief das Gate darunter schließt.

**ATT und Velocity (Steuerpegel).**
* Wirkung auf das Signal: Der Steuerpegel ist `begrenze(ATT + Velocity, 0 … 10 V)`. Das Gate öffnet, wenn dieser Pegel die Schwelle überschreitet. Der Steuerpegel kommt vom Regler oder, wenn gesteckt, von der CV (die den Regler ersetzt).
* Für eine Hüllkurve 0…10 V an der ATT-CV und Schwelle 4,12 V (Offset 0,5, Trim 0,5): Die Hüllkurve öffnet das Gate voll, sobald sie 4,2 V erreicht; eine Hüllkurve mit nur 4,12 V Spitze liegt genau auf der Schwelle (−13 dB), mit 2,5 V Spitze öffnet das Gate nicht mehr (−31 dB). Mit negativer Velocity wird die Hüllkurve nach unten verschoben: Das Gate öffnet später im Anstieg und schließt früher im Abfall (kürzere Öffnungszeit). Bei exponentiell abfallender Hüllkurve 10 V·e^(−t/200 ms) ergibt die Geometrie (ohne Vactrol) Öffnungszeiten von ca. 178 ms (Spitze 10 V), 120 ms (7,5 V) und 38 ms (5 V).
* ATT 0 V ohne CV hält das Gate dauerhaft auf der Ruhelage (Steuerpegel 0 V plus Velocity).
* Der Steuerpegel kann nicht unter 0 V fallen; die Invertierung der alten Version (CV ATT negativ) und die Verstärkung über 100 % gibt es im Quad-Panel nicht mehr.
* Steuerpegel über 10 V werden begrenzt; gemessen im Modell: 10, 15 und 20 V ergeben dieselbe offene Gain.
* Reihenfolge im Modul: `ATT (Regler oder CV) + Velocity` → Begrenzung auf 0…10 V → Shelving; die Offset-Spannung (THRESHOLD) kommt erst **nach** der Begrenzung dazu. Folge: Eine negative Velocity hebt einen niedrigen THRESHOLD nicht auf.

**Typische Einstellungen.**

| Ziel | Einstellung |
|---|---|
| Dauer-Ton, Filter fest auf ≈ 400 Hz ohne Hüllkurve | LOWPASS, THRESHOLD knapp über 0 V (z. B. 0,1 V), DEPTH 50 % |
| Dauer-Ton offen ohne Hüllkurve | THRESHOLD 0 V, ATT einige Volt |
| Hüllkurve 0…10 V soll in der Mitte öffnen | THRESHOLD 4,1 V, DEPTH 50 % |
| Kurzer Ping aus derselben Hüllkurve | THRESHOLD erhöhen oder Velocity senken: Gate ist nur im oberen Teil offen |
| Längere Öffnung | THRESHOLD senken |
| Pluck mit tiefem Schließen (0…10-V-Hüllkurve) | MODE LOWPASS, DEPTH 100 %, THRESHOLD 7,5 V, Vactrol response = Conductance (Abschnitt 14); Modellwerte für BOTH mit State-based in Abschnitt 4c |

**Offene Frage.** Das Modell schaltet an der Schwelle in ca. 0,2 V von „zu“ auf „offen“ (Zenerverhalten der Schaltung im Modell). Ob das Originalgerät genauso steil schaltet, ist nicht geprüft. Beim Hörtest wäre ein Vergleich mit einem Video oder Gerät mit langsamer CV (LFO-Dreieck) aufschlussreich: Öffnet das Original auch abrupt?

### 4c. Pluck: tief schließendes Gate

Das Gate schließt nur so tief, wie der Steuerpegel in der Ruhe unter der Schwelle liegt. Eine 0…10-V-Hüllkurve ruht bei 0 V; bei THRESHOLD 4,1 V sind das nur 4,1 V Abstand, das Gate bleibt „angelehnt“ (Boden bei ca. −20 dB), und nach dem kurzen Burst bleiben vor allem die Bässe stehen. Das klingt wie ein tiefes Knacken, nicht wie ein Pluck. Im Quad-Panel kann der Steuerpegel nicht unter 0 V fallen (ATT 0…10 V plus Velocity, begrenzt auf 0…10 V); die Ruhelage liegt also bei 0 V, und der Abstand zur Schwelle ist die Schwelle selbst. Ein hoher THRESHOLD bei DEPTH 100 % liefert dann den größten Abstand.

Gemessen am Modell (Paper-Variante, BOTH, State-based, Rauschen, Hüllkurve 0…10 V, Abfall 100 ms; Pegel in 10-ms-Fenstern): DEPTH 100 %, THRESHOLD 7,4 V ergibt Ruhe −76 dB, Spitze −4 dB, Pegel nach 20 / 50 / 100 / 200 / 400 ms: 0 / −9 / −13 / −21 / −36 dB. Der Standard 4,1 V (Offset „mittel“) lässt das Gate „angelehnt“ (Ruhe −21 dB).

Ein langsames Vactrol (VACTROL rechts) macht den Schwanz länger, senkt aber die Spitze, weil das Gate nicht voll aufgeht, solange die Hüllkurve kurz ist. Eine schnelle Hüllkurve zusammen mit einem schnellen Vactrol (VACTROL links) klingt härter. Hinweis: Die Zahlen sind Modellwerte, keine Hörergebnisse; sie gelten für den Paper-Gate-Anteil. Im linearen Gate-Anteil von BOTH schließt das Gate bei ATT 0 V vollständig. Die Messwerte oben gelten für BOTH mit State-based; der im Hörtest gefundene Pluck (Abschnitt 14, Beispiel 1) läuft dagegen in LOWPASS mit Vactrol response = Conductance bei THRESHOLD 7,5 V und DEPTH 100 %, wo die Ruhelage ebenfalls 0 V und der Abstand zur Schwelle ca. 7,4 V beträgt.

### 4a. Gate geschlossen ist nicht stumm

Wie beim Original erscheint das Eingangssignal auch bei geschlossenem Gate am Ausgang, gefiltert beziehungsweise abgeschwächt (Offset 0,5, Trim 0,5, Feedback 0, Gain in dB):

| Modus | CV 0 V | CV −4 V | CV −10 V |
|---|---|---|---|
| LOWPASS | 100 Hz −1,6 / 1 kHz −17 / 5 kHz −41 | −17 / −51 / −80 | −68 / −107 / −136 |
| BOTH | −0,6 / −7 / −21 | −7,5 / −28 / −53 | −42 / −80 / −109 |
| VCA | alle Frequenzen ≈ −35 | −55 … −69 | −91 … −125 |

Nur in LOWPASS und BOTH bleibt ein tiefpassgefiltertes Signal hörbar, VCA schwächt breitbandig ab. Ob die Werte dem Originalgerät entsprechen, ist nicht geprüft (keine Messdaten vom Original).

Diese Tabelle gilt für das Paper-Modell. Mit dem linearen Gate-Anteil (Standard in VCA und BOTH) schließt das Gate in VCA bei ATT 0 V vollständig (Gain 0); in BOTH ist die Gain `cv / 10` mal die Gain der Filterstufe.

## 5. Modi

Der Modus pro Kanal (MODE-Schalter oder globaler MODE bzw. MODE CV) bestimmt die Schaltung. LOWPASS ist immer das Paper-Modell. Bei VCA und BOTH wählt das Kontextmenü **Gate response (VCA, BOTH)**:

* **Linear** (Standard): Das Gate ist ein linearer VCA. Die Referenzdokumentation des Hardware-Moduls beschreibt den Gate-Modus als linearen VCA und den kombinierten Modus als Zusammenschaltung aus linearem Gate und Lowpass; deshalb ist der lineare Anteil Standard.
* **Paper circuit**: Das Paper-Modell (Vactrol-Teiler mit Knick bei THRESHOLD) wie in den Abschnitten 4 bis 8; steil schaltend, in VCA nur 0,63 offene Gain.

**Linearer Gate-Anteil.** `g = begrenze(cv, 0 … 10 V) / 10`, also 10 V = Verstärkung 1 (Annahme: Die Referenzdokumentation nennt keine Zahlen). Die Verstärkung wird mit den Vactrol-Zeitkonstanten geglättet (Anstieg 2,4 ms, Abfall 50 ms, mal VACTROL-Faktor); die Richtung folgt dem Zustand (steigt g, gilt die schnelle Konstante). Ob das Hardware-Modul im Gate-Modus Vactrol-Trägheit hat, ist nicht belegt (Annahme).

| Modus | Gate response = Linear (Standard) | Gate response = Paper circuit |
|---|---|---|
| LOWPASS | Tiefpass mit C3 = 4,7 nF, Ra = 5 MΩ (Paper-Gate), unverändert | derselbe Modus |
| BOTH | Erst linearer VCA, dann Filterstufe (Menü „BOTH filter stage“) | Tiefpass und VCA gleichzeitig (C3 = 0, Ra = 5 MΩ), wie vor der Änderung |
| VCA | Reiner linearer VCA: `Ausgang = g · Eingang`, kein Filter | Reiner VCA (C3 = 0, Ra = 5 kΩ), offene Gain 0,633 |

Klangliche Einordnung:

* **LOWPASS**: Der Pegel bleibt im Durchlassbereich nahe 1, nur die Höhen schließen. Das Gate wirkt wie ein Filter mit CV-gesteuerter Grenzfrequenz (404 Hz bei Steuerpegel 4 V im Beispiel von Abschnitt 4). Hier ist das Feedback immer wirksam.
* **BOTH** (linear): VCA und Lowpass-Gate in Reihe. Die Lautstärke folgt linear dem Steuerpegel, zusätzlich schließt das Filter die Höhen. Menü **BOTH filter stage**: *Lowpass* (Standard) verwendet die Filterstufe von LOWPASS; THRESHOLD, DEPTH, FEEDBACK und FB NL wirken dort wie in LOWPASS, und das Filter schwingt nach dem Schließen des Gate leise nach. *Paper BOTH* verwendet C3 = 0: das Filter filtert kaum und hat keine Resonanz (FEEDBACK und FB NL ohne Wirkung); BOTH ist dann das Paper-BOTH mit vorgeschaltetem linearem Gate. Die Lowpass-Stufe ist Standard, weil der Anwender sie für sinnvoller hält und flexibel bleiben will.
* **VCA** (linear): kein Filter, nur Pegel; THRESHOLD, DEPTH, FEEDBACK und FB NL wirken nicht. Die Lautstärke ist exakt `cv / 10`.

Messwerte am Modell (48 kHz):

* VCA linear, Sinus 100 Hz: Steuerpegel 0 / 0,5 / 1 / 2,5 / 5 / 7,5 / 10 V = Gain 0 / 0,05 / 0,1 / 0,25 / 0,5 / 0,75 / 1,0, unabhängig von THRESHOLD, DEPTH und FEEDBACK. Zeit (Sprung 0 ↔ 10 V, VACTROL ×1): auf 63 % nach 2,40 ms, 95 % nach 7,19 ms; ab auf 37 % nach 50,0 ms, 5 % nach 149,8 ms.
* BOTH linear, stationär (THRESHOLD 7,5 V, DEPTH 100 %, FEEDBACK 0, Sinus 100 Hz), Gain für Steuerpegel 1 / 3 / 5 / 7 / 10 V: Lowpass-Stufe 0,019 / 0,213 / 0,443 / 0,659 / 0,999; Paper-Stufe 0,051 / 0,266 / 0,474 / 0,679 / 0,999; zum Vergleich Paper-BOTH ohne linearen Anteil 0,505 / 0,886 / 0,949 / 0,969 / 0,999. BOTH linear mit Lowpass-Stufe = LOWPASS × `cv / 10`.
* Nachschwingen von BOTH nach dem Gate-Schluss (Ping: 20 ms Burst 600 Hz Amplitude 1, dann Eingang 0 und Gate zu, THRESHOLD 7,5 V, FEEDBACK 90 %): Lowpass-Stufe 0,0081 (20–50 ms) und 0,0025 (100–200 ms); Paper-Stufe 0,0000. Das Nachschwingen ist leise (ca. 0,8 % der Burst-Amplitude).
* Stabilität BOTH linear (Lowpass-Stufe, FEEDBACK 100 %, normiert, NL aus): Spitze 33,7 gegen 43,7 in LOWPASS bei gleicher Eingabe; der Schutz des Moduls greift bei 50. Kein NaN.

Moduswechsel im Betrieb: LOWPASS ↔ BOTH ohne Sprung (Abweichung 6e-4, Gleichspannungsgewinn der Lowpass-Stufe). Aus dem reinen linearen VCA zurück in LOWPASS oder BOTH startet das Filter mit leeren Zuständen (kurzer Einschwingvorgang möglich). Das Umschalten der Filterstufe im Betrieb erzeugt kein NaN und keinen Sprung über dem normalen Signal.

Paper-Werte (zur Orientierung, Gate response = Paper circuit):

| Modus | Schaltung | Offene Gain (20 Hz) | Grenzfrequenz offen (−3 dB) | bei Steuerpegel 4,0 V (Rf 63,5 kΩ) | bei 3,0 V (Rf 74,6 kΩ) |
|---|---|---|---|---|---|
| LOWPASS | Tiefpass mit C3 = 4,7 nF, Ra = 5 MΩ | 1,009 | 12,95 kHz | Gain 0,98, f = 404 Hz | 346 Hz |
| BOTH | Tiefpass und VCA gleichzeitig (C3 = 0, Ra = 5 MΩ) | 1,009 | 19,4 kHz | Gain 0,98, f = 1188 Hz | 1014 Hz |
| VCA | reiner VCA (C3 = 0, Ra = 5 kΩ) | 0,639 | über 20 kHz | Gain 0,038 | 0,033 |

Offene Frage: Ob die Mittelstellung des Hardware-Moduls ein Lowpass mit leiser Resonanz oder (nach einer möglichen Lesart der Schaltung) einen Zweig ohne Filterkondensator verwendet, ist nicht sicher entscheidbar; deshalb ist die Filterstufe von BOTH wählbar.

## 6. Feedback (Resonanz)

### 6.0 Was FEEDBACK bewirkt

Im Modus LOWPASS führt FEEDBACK einen Teil des Ausgangs über einen Differenzierer zurück an den Filtereingang. Daraus entsteht eine **Resonanz** (Betonung) um die Grenzfrequenz: Ohne Feedback ist das Filter ein weicher Tiefpass, mit Feedback „singt“ es an einer Stelle des Spektrums.

* **Wo FEEDBACK wirkt:** In LOWPASS immer. In BOTH nur mit „Gate response = Linear“ und „BOTH filter stage = Lowpass“ (beides Standard). **In VCA hat FEEDBACK keine Wirkung** (und FB NL ebenso), ebenso in BOTH mit der Paper-Stufe oder dem Paper-Gate (dort ist der Kondensator C3 des Rückkopplungspfads im Modell abgeschaltet, C3 = 0). Die Reglerstellung bleibt im Patch gespeichert und ist beim Umschalten wieder wirksam. Gemessen: BOTH Paper und VCA mit FEEDBACK 90 % gegen 0 % ergeben dieselbe Spitze. Der Tooltip weist darauf hin.
* **Die Spitze liegt deutlich über der Grenzfrequenz**, etwa beim 2,3- bis 2,9-fachen der −3-dB-Frequenz ohne Feedback (Beispiel unten: Grenzfrequenz 407 Hz, Spitze bei 951…1162 Hz). Das ist das Verhalten des Modells aus dem Paper, nicht das eines klassischen Resonanztiefpasses mit der Spitze an der Grenze.
* **Tiefe Frequenzen bleiben fast unberührt** (−0,2 dB bei 50 Hz). Das Feedback hebt nur den Bereich um die Spitze an.
* **Die Spitze wandert mit dem Gate.** Öffnet sich das Gate, steigt die Grenzfrequenz und die Resonanz fährt mit nach oben: der typische „Wah“- oder „Ping“-Klang. Bei weit offenem Gate (Grenzfrequenz 13 kHz) liegt die Spitze bei etwa 19 kHz und ist kaum noch hörbar.
* **Bei Anregung klingelt das Filter nach.** Ein Impuls, eine Gate-Flanke oder ein Rauschburst lässt das Filter mit der Resonanzfrequenz ausschwingen, je näher an 100 %, desto länger. Bei 100 % klingt es nicht mehr ab.
* **FB NL aus** (Standard): Das Signal bleibt ein Sinus, nur Pegel und Phase ändern sich. **FB NL an**: Ein tanh begrenzt die Rückkopplung, es entsteht Verzerrung, vor allem bei hohem DRIVE und Signalen nahe der Resonanzfrequenz (Abschnitt 7).
* **Praxis:** Für „Ping“ oder „Wah“ 75–90 %, ein kurzes Gate und eine Eingangsfrequenz nahe der Spitze oder Rauschen. Ab etwa 95 % wird die Resonanz zu einem fast tonalen Pfeifen, das sich mit der Gate-Öffnung bewegt.

### 6.1 Messwerte

Nur im Modus LOWPASS. Der Resonanzgipfel liegt über der −3-dB-Frequenz ohne Feedback.

Arbeitspunkt CV 4,05 V, f(−3 dB, ohne Feedback) = 407 Hz, **normiert**:

| FEEDBACK | Spitze | Spitzenfrequenz |
|---|---|---|
| 0 … 50 % | keine (−0,2…−0,1 dB) | – |
| 75 % | +2,7 dB | 951 Hz |
| 90 % | +10,0 dB | 1105 Hz |
| 95 % | +15,8 dB | 1162 Hz |
| 99 % | +28,6 dB | 1162 Hz |

* Bis 50 % geschieht klanglich wenig; die Resonanz entsteht erst ab 75 %. Die Strecke zwischen 75 % und 99 % überstreicht 26 dB.
* Bei 100 % liegt das Modell genau auf der Stabilitätsgrenze: Ein Impuls klingt nicht ab, es entsteht eine gleichbleibende Schwingung kleiner Amplitude (Impuls 1,0 → Spitze 0,145). Das Modul läuft nicht davon (Schutz gegen Überlauf ist trotzdem eingebaut).
* Die Resonanzfrequenz folgt der CV mit dem Gate.

**Normiert vs. schaltungsgetreu (Kontextmenü „Resonance“):**

* *Normalized*: Der Regler 0…100 % ist auf die jeweilige Stabilitätsgrenze skaliert; gleiche Reglerstellung = gleiche Resonanzstärke, unabhängig vom Gate-Zustand.
* *Circuit-accurate*: Der Regler 0…100 % entspricht der absoluten Rückkopplung 0…1,472. Bei offenem Gate (Rf klein) praktisch gleich zum normierten Modus; bei fast geschlossenem Gate wird die Resonanz schwächer. Beispiel FEEDBACK 90 %:

| CV | Rf | normiert | absolut |
|---|---|---|---|
| 6,0 / 5,0 / 4,5 | 1,5 kΩ | 9,7 dB | 9,6 dB |
| 4,0 | 63,5 kΩ | 10,0 dB | 9,4 dB |
| 3,5 | 68,7 kΩ | 10,0 dB | 9,2 dB |
| 0,0 | 143 kΩ | 9,8 dB | 8,4 dB |
| −2,0 | 291 kΩ | 9,3 dB | 6,8 dB |
| −4,0 | 1352 kΩ | 6,8 dB | −0,4 dB (keine Resonanz) |

  Praktisch: Solange das Gate offen oder halb offen ist, sind beide Modi nahezu gleich. Der Unterschied tritt erst im fast geschlossenen Bereich auf; dort „schluckt“ der absolute Modus die Resonanz.

## 7. Nichtlinearität (FB NL) und DRIVE

Mit FB NL = an geht die Rückkopplung durch einen tanh-Begrenzer. Wirkung nur bei ausreichend großem Signal; DRIVE bestimmt, wie groß das Signal im Modell ist (Abschnitt 3).

Obertonanteil (Wurzel aus der Summe der Oberwellen 2…8, bezogen auf die Grundwelle), Eingang ±5 V, FEEDBACK 90 %, **NL-Solver „Corrected“**:

| CV | f | Drive 0 | 2,5 | 5 | 7,5 | 10 | Grundwelle Drive 0 / 10 |
|---|---|---|---|---|---|---|---|
| 8,0 V (offen) | 220 Hz | 0,0002 | 0,0007 | 0,0021 | 0,0038 | 0,0041 | 5,00 / 5,00 V |
| 3,0 V | 500 Hz | 0,0228 | 0,0643 | 0,1079 | 0,1199 | 0,1040 | 6,37 / 3,89 V |
| 4,0 V | 500 Hz | 0,0282 | 0,0754 | 0,1196 | 0,1282 | 0,1066 | 5,91 / 4,08 V |
| 4,0 V | 2000 Hz | 0,0007 | 0,0025 | 0,0090 | 0,0250 | 0,0460 | 2,26 / 1,70 V |

(Zum Vergleich der Solver „Reference“: 0,0031 / 0,0122 / 0,0391 / 0,0724 / 0,0811 für CV 3,0 V, 500 Hz.)

Was man hört:

* **Bei offenem Gate und ohne Resonanz-Spitze am Signal** bleibt die NL praktisch unhörbar (Oberwellenanteil < 0,5 %).
* **Am Resonanzgipfel** (Gate halb offen, Signal nahe der Resonanzfrequenz) entsteht deutliche Verzerrung (bis ~13 % Oberwellen) und der Pegel sinkt (Grundwelle 5,9 → 4,1 V).
* Bei 500 Hz steigt der Oberwellenanteil mit DRIVE bis ca. 7,5 und fällt dann wieder; bei 2 kHz steigt er bis Drive 10 weiter.
* DRIVE ohne NL: **keine** Wirkung.
* Mit NL und FEEDBACK 100 % (Solver „Corrected“, CV 4 V) bleibt nach einem Impuls eine kleine Schwingung bestehen (Spitze im Ausgang nach 5 s: 0,094 V bei Drive 0, 0,030 V bei Drive 5, 0,008 V bei Drive 10). Mit Solver „Reference“ klingt sie ab (0 V).

## 7a. Sättigung (Zusatz, Menü „Saturation“)

Nicht Teil des Paper-Modells. Hintergrund: Auf Scope-Aufnahmen des Originals hat ein Sinus eine leicht abgeflachte Spitze, das Modell hat im Audiopfad aber keine Sättigung. Mit „Saturation = On“ läuft das Modellausgangssignal (in Modelleinheiten) durch `tanh`, danach wird auf Volt zurückskaliert. Standard ist Off.

* Die Intensität bestimmt **DRIVE** (Pegel im Modell, Abschnitt 3). Kleinsignalverstärkung bleibt 1, es ändert sich nur der Verlauf großer Amplituden. Wirkt in allen Modi und auch bei Feedback 0.
* Gemessen für einen Sinus mit 5 V Spitze, wenn das Modell Verstärkung 1 hat (LOWPASS offen 1,009):

| DRIVE | Modellamplitude | Oberwellen 3…9 / Grundwelle | Ausgangsspitze |
|---|---|---|---|
| 0 | 0,25 | 0,5 % | 4,90 V |
| 2,5 | 0,50 | 2,0 % | 4,62 V |
| 5 | 1,00 | 6,7 % | 3,81 V |
| 7,5 | 2,00 | 17,3 % | 2,41 V |
| 10 | 4,00 | 30,0 % | 1,25 V |

* Es entstehen nur ungeradzahlige Obertöne (symmetrische Kennlinie).
* **Der Ausgangspegel sinkt mit steigendem DRIVE**: Die Ausgangsspitze ist auf den Pegelfaktor begrenzt (20 / 5 / 1,25 V bei Drive 0 / 5 / 10). Bei hohem Drive deshalb dahinter ggf. Pegel anheben.
* Im VCA-Modus (offene Gain 0,63) und bei teilweise geschlossenem Gate ist die Amplitude im Modell kleiner, die Sättigung entsprechend schwächer: Sie wirkt am stärksten bei offenem Gate und lautem Signal.
* **Hörtest (Kinovice, Rack):** Der warme Klang eines Sinus stellt sich ein, wenn DRIVE bei etwa 75 % (≈ 7,5) steht, d. h. bei rund 17 % Oberwellen (Tabelle). Bei Drive 5 ist die Wirkung zu schwach.
* Die Stufe ist ein Zusatz, keine Messung am Original. Der gemessene Effekt entspricht dem Rechenmodell, nicht notwendig dem Klang des Geräts.

## 8. Zeitverhalten: Vactrol

Zeitkonstanten des Modells: Öffnen 2,4 ms, Schließen 50 ms (Tiefpass auf dem Widerstand Rf). Der Regler **VACTROL** (ein Regler für alle Kanäle) skaliert beide Werte um den Faktor ×0,1…×10 (Mitte ×1 = Werte des Papers, bitgenau gegen die Referenz geprüft). Er stellt auch die Trägheit des linearen Gate-Anteils (Abschnitt 5) ein. Gemessen (State-based, Paper-VCA, Sprung −10 → +10 V): Anstieg 10 → 90 % bei ×0,3 = 3,2 ms, ×1 = 10,5 ms, ×3 = 31,6 ms. Die Tabellen in diesem Abschnitt gelten für ×1.

Kontextmenü **Vactrol response**:

* *Reference (as published)*: Wie die Octave-Referenz. Die Referenz wählt die schnelle Konstante nur, wenn der Zielwert Rf in diesem Sample sinkt. Bei einer gate-artigen CV passiert das nur im ersten Sample; danach öffnet das Vactrol mit der langsamen Konstante.
* *State-based*: Richtung aus dem Vergleich des Ziels mit dem aktuellen Vactrol-Zustand. Verhält sich wie ein echtes LPG.

Gate-Sprung −10 V → +10 V (VCA, Gleichspannung am Eingang):

| | Reference | State-based |
|---|---|---|
| Anstieg 10 → 90 % | **219,7 ms** | **10,5 ms** |
| Schließen 90 → 10 % | 0,1 ms | 0,1 ms |

Das sehr schnelle Schließen beim Sprung von +10 V auf −10 V kommt vom Shelving-Filter im Steuerpfad: Die steile CV-Flanke überschwingt, das Vactrol wird kurz auf extrem hohen Widerstand getrieben. Bei sanfterem Abfall der CV schließt es langsamer (Tabelle unten).

Kurze Gate-Impulse (CV −10 → +10 V), erreichte Spitzengain relativ zu offen:

| Länge | Reference | State-based |
|---|---|---|
| 5 ms | 0,000 | 0,001 |
| 10 ms | 0,000 | 0,007 |
| 20 ms | 0,000 | 0,32 |
| 50 ms | 0,000 | 1,00 |
| 100 ms | 0,001 | 1,00 |
| 200 ms | 0,006 | – (1,00) |

Mit der Referenz-Einstellung reagiert das Modul daher auf Trigger/kurze Gates praktisch nicht (Gate muss weit über 200 ms lang sein). Für Pluck-Sounds bitte „State-based“ oder „Conductance“ wählen; „Conductance“ liefert den Nachlauf, mit dem der Pluck in Abschnitt 14 gefunden wurde.

Schließzeit nach einem 100-ms-Gate (+10 V), State-based, abhängig von der Ruhe-CV (Zeit bis 50 % / 10 % der Spitzengain):

| Ruhe-CV | 50 % | 10 % | Endwert (rel.) |
|---|---|---|---|
| −10 V | 0,0 ms | 0,1 ms | 0,000 |
| −4 V | 0,0 ms | 0,1 ms | 0,003 |
| −1 V | 0,4 ms | 8,8 ms | 0,020 |
| 1 V | 1,4 ms | 19,1 ms | 0,035 |
| 2 V | 2,0 ms | 25,4 ms | 0,043 |
| 3 V | 2,5 ms | 33,0 ms | 0,051 |
| 3,5 V | 2,8 ms | 37,4 ms | 0,056 |

Der „Decay“ ist also kein freier Parameter, sondern hängt davon ab, **wohin** die CV zurückfällt. Tiefe Ruhe-CV (≤ −4 V) schaltet praktisch sofort ab; eine Ruhe-CV knapp unter der Schwelle (3…3,5 V) erzeugt einen längeren, weicheren Ausklang (33…37 ms bis −20 dB). Mit THRESHOLD lässt sich die Lage der Ruhe-CV zur Schwelle ändern.

## 9. Kanäle und Eingänge

* Vier getrennte, monophone Kanäle; jeder hat eigenen Zustand, eigene Regler und eigene Schalter. IN 1 ist auf IN 2–4 normalisiert, wenn dort nichts steckt.
* Ein Kanal ohne Audioquelle (weder eigener IN noch IN 1) wird nicht gerechnet, sein Ausgang liegt auf 0 V. Ein Kanal ohne OUT-Kabel wird übersprungen, solange ALL nicht belegt ist. Das spart bis zu drei Viertel der Rechenzeit. Beim Reaktivieren startet der Kanal im eingeschwungenen Zustand (kein Einschwingen des Vactrols).
* Alte Patches der polyphonen Version (16 Kanäle) landen auf den Kanälen 1–4; die Param-Blöcke der alten Version bleiben bestehen, damit die Indizes stimmen. Ihre Kabel an AUDIO und CV liegen auf Kanal 1, nur Kanal 1 der Polyphonie wird gelesen (Poly-Eingänge werden nicht aufgefächert).

## 10. Kontextmenü

Die Regler liegen auf dem Panel; das Kontextmenü enthält DEPTH und die Modell-Einstellungen.

| Eintrag | Optionen (Standard fett) | Wirkung |
|---|---|---|
| Depth | Schieber „All channels“ und je Kanal 1–4 (Standard 100 %) | Wie tief das Gate unter der Schwelle schließt (Abschnitt 3) |
| Resonance | **Normalized** / Circuit-accurate | Abschnitt 6 |
| Vactrol response | **Reference** / State-based / Conductance | Abschnitt 8 |
| NL solver | Reference / **Corrected** | Abschnitt 11 (Modellgrenzen) |
| Circuit variant | **Paper** / Reproduction board | Bauteilwerte nach Paper oder nach einer Variante mit geänderten Bauteilwerten (Abschnitt 11a) |
| Gate response (VCA, BOTH) | **Linear** / Paper circuit | Linearer Gate-Anteil oder Paper-Modell (Abschnitt 5) |
| Velocity target | **Control level** / Audio offset | Wo Velocity wirkt (Abschnitt 3a) |
| BOTH filter stage | **Lowpass** / Paper BOTH | Filterstufe von BOTH, nur mit linearem Gate-Anteil (Abschnitt 5) |
| Saturation | **Off** / On | Zusatz, nicht im Paper-Modell (Abschnitt 7a) |
| Oversampling | **Off** / 2x | 2× Oversampling des Audiopfads; verdoppelt die Rechenzeit, kann Aliasing durch die Nichtlinearität verringern (im Hörtest zu prüfen) |

Einstellungen werden im Patch gespeichert. Ältere Patches ohne die Schlüssel für Gate response, BOTH filter stage und Velocity target laden mit den Standardwerten (Linear, Lowpass, Control level).

## 11. Modellgrenzen und Auffälligkeiten der Referenz

Beim Nachrechnen der Referenz wurden drei Punkte gefunden:

1. **Paper-Gleichung (20)** ist im PDF fehlerhaft; der Code der Referenz ist korrekt. Umgesetzt ist der Code.
2. **Vactrol-Richtung**: siehe Abschnitt 8. Bei Gate-CV öffnet die Referenz mit rund 220 ms. Als Option „State-based“ verfügbar.
3. **NL-Solver**: In `yxden` steht `+a1·d1·t2m1`; die Herleitung ergibt `−a1·d1·t2m1`. Folge: der NL-Zweig geht bei kleinem Signal **nicht** in den linearen Zweig über. Gemessen: Abweichung NL gegen linear bei 1 µV: **52 %** (Reference) gegenüber 1,4·10⁻¹² (Corrected). Als Option „Corrected“ verfügbar und seit dem Hörtest Standard im Modul.

Standard: NL solver = **Corrected** (Entscheidung nach Hörtest), Vactrol response = Reference (noch offen). Die Engine selbst (`AnnaMariaLpg`) hat weiterhin die Referenz als Voreinstellung und ist so bitgenau gegen Octave geprüft; das Modul setzt Corrected explizit. Weitere Grenzen:

* Die Parameter des Vactrol-Modells sind aus dem Paper übernommen, nicht gemessen.
* Streuung von Bauteilen und Temperatur sind nicht modelliert.
* Bei sehr hohen Frequenzen nahe Nyquist wirkt die Trapez-Integration; Oversampling 2× hilft.

## 11a. Abgleich mit einer Bauteilvariante

Quelle: Referenzdokumentation einer Schaltungsvariante des Lowpass Gates. Die Werte wurden daraus gelesen und nicht gegen Messdaten geprüft.

* **Abweichungen gegenüber dem Paper:** R8 = 10 k statt 4,7 k; im VCA-Modus Querwiderstand R16 = 10 k statt 5 k und eine Eingangsstufe mit Verstärkung 2 (R11 = R12 = 15 k). Die Vorlage hat außerdem einen DEEP-Schalter, der das Gate bei gleicher Reglerstellung tiefer schließen lässt (etwa wie ein um einige Volt höherer THRESHOLD). DEEP ist nicht modelliert; eine genaue Nachbildung bräuchte die LED-Durchlassspannung.

* **Menü „Circuit variant = Reproduction board“** setzt R8 = 10 k, VCA-Querwiderstand 10 k und im VCA-Modus Eingangsverstärkung 2. Standard bleibt Paper.

Messwerte (Sinus 1 kHz, THRESHOLD 4,1 V, DEPTH 50 %, State-based, Verstärkung in dB):

| Modus | Variante | CV −4 V | 0 V | 4 V | 8 V |
|---|---|---|---|---|---|
| LOWPASS | Paper | −27,3 | −6,8 | −2,5 | 0,0 |
| LOWPASS | Reproduction | −40,2 | −13,9 | −7,4 | 0,0 |
| BOTH | Paper | −50,7 | −16,8 | −8,9 | 0,0 |
| BOTH | Reproduction | −66,4 | −28,9 | −17,9 | 0,0 |
| VCA | Paper | −57,1 | −35,3 | −28,4 | −4,0 |
| VCA | Reproduction | −58,2 | −31,3 | −24,2 | +3,8 |

Die Schwelle ändert sich nicht, das geschlossene Gate wird in LOWPASS/BOTH 6…12 dB tiefer. Im VCA-Modus ist offen +3,8 dB statt −4 dB (Pegel im Modus beachten).

**Pluck ohne neue Regler** (Paper-Variante, BOTH, State-based, Rauschen, Hüllkurve 0…10 V, Abfall 100 ms): DEPTH 100 %, THRESHOLD 7,4 V (entspricht Offset am unteren Anschlag) ergibt Ruhe −76 dB, Spitze −4 dB, Pegel nach 20 / 50 / 100 / 200 / 400 ms: 0 / −9 / −13 / −21 / −36 dB. Der Standard 4,1 V (Offset „mittel“) lässt das Gate „angelehnt“ (Ruhe −21 dB).

## 12. Rechenaufwand

Engine (Messmaschine, double, -O2, ein Kanal): 70–90 ns pro Sample bei bewegter CV, bei konstanter CV 20–26 ns (Cache für Steuerpfad und Audio-Koeffizienten, bitgleich zum Original). Messung im Rack: ein aktiver Kanal 0,2–0,4 %, ALL belegt (vier Kanäle) 1,5 % (ca. 0,35 % je Kanal bei bewegter CV). Mit 2× Oversampling etwa doppelt. Die CV-Eingänge für THRESHOLD und FEEDBACK mit Audiorate kosten mehr, weil sich die Koeffizienten in jedem Sample ändern. Der lineare Gate-Anteil in VCA ist billiger als das Paper-Modell.

## 13. Lizenz und Quellen

Plugin: GPL-3.0-or-later. Die Formeln folgen `buchla_lp_gate.m` (Copyright 2014 Stefano D'Angelo, MIT; Hinweis in `AnnaMariaLpg.hpp`) und dem Paper *Parker, D'Angelo: A Digital Model of the Buchla Lowpass-Gate*, DAFx-13.

## 14. Patch-Beispiele (zu prüfen im Hörtest)

1. **Pluck / Marimba** (im Hörtest gefunden): MODE LOWPASS, THRESHOLD 7,5 V, DEPTH 100 % (Standard), Vactrol response = Conductance, VACTROL ×1. Hüllkurve an die ATT-CV, VEL 0: Delay 700 ms, Attack 5 ms, Hold 0 ms, Release 200 ms, Spitze 10 V (erzeugt mit dem Hüllkurvenmodul Europa). Am IN ein Sägezahn (100 Hz) oder ein Dreieck (2 kHz, klingt glocken- bzw. marimbaartig). DRIVE 1,9, FEEDBACK 63–80 %, FB NL an, Resonance = Normalized, NL solver = Corrected, Circuit variant = Paper, Saturation = On, Oversampling aus. „Gate response“ und „BOTH filter stage“ wirken nur in BOTH und VCA und sind hier ohne Bedeutung. Weil THRESHOLD 7,5 V ist, öffnet nur der obere Teil der Hüllkurve das Gate (ab ca. 7,4 V Steuerpegel); der Ausklang kommt vom Nachlauf des Vactrol-Modells (Abschnitt 8). Für weicheren Ausklang VACTROL nach rechts, für härteren nach links. Variante mit BOTH: siehe Abschnitt 4c (Modellwerte).
2. **Linearer VCA**: MODE VCA, Hüllkurve oder Regler an ATT: Die Lautstärke folgt exakt `cv / 10`; Velocity verschiebt den Bereich (Abschnitt 3).
3. **Filter-Drone**: MODE LOWPASS, THRESHOLD knapp über 0 V (z. B. 0,1 V), sodass das Gate ohne Hüllkurve bei der gewünschten Grenzfrequenz steht; LFO an ATT-CV mit kleiner Amplitude; FEEDBACK 75–90 %.
4. **Resonanz-Verzerrung**: MODE LOWPASS, FB NL an, FEEDBACK 90 %, DRIVE 6–8, Gate halb offen (Steuerpegel an der Schwelle).
5. **Gleichanteil-Verschiebung**: Velocity target = Audio offset, ATT 10 V, Signal ±5 V am IN, Velocity von 0 auf −4 V: Das Signal rutscht nach unten (+1 / −9 V), bei weiterer Absenkung wird unten bei −10 V geschnitten (Abschnitt 3a).

## 15. Entscheidungen (Protokoll)

Entscheidungen des Anwenders (Kinovice); die Reihenfolge ist chronologisch.

* **Quad-Layout (2026-10-14).** Vier monophone Kanäle statt 16 polyphoner, 31 HP, Spalte ALL (ALL OUT, MODE global, MODE CV, DRIVE, VACTROL, DEEP). Reihenfolge je Kanal: IN | OUT, ATT | VEL, CV | CV, THRESH | FEEDBACK, CV | CV, FB NL, MODE | DEEP. ALL: erst summieren, dann auf ±10 V weich begrenzen (keine Division durch 4, weil sonst zu leise). IN 1 wird auf IN 2–4 durchgereicht.
* **Signalweg nach Hardware-Vorbild (2026-10-14).** ATT = Steuerpegel 0…10 V, Velocity = Gleichanteil darauf, Summe auf 0…10 V begrenzt; eine gesteckte CV ersetzt jeweils den Regler (ATT und Velocity). THRESHOLD-CV 1 V/V über 0…7,5 V, FEEDBACK-CV 0…10 V = 0…100 %.
* **Linearer VCA (2026-10-15).** Nach der Referenzdokumentation soll VCA ein linearer VCA sein (Paper-Schaltung dort unwirksam). BOTH bekommt ebenfalls den linearen Anteil.
* **Reihenfolge in BOTH (2026-10-15).** Erst VCA, dann LPG, beide in Reihe (entspricht der Aussage in einem Video zur Hardware).
* **Filterstufe von BOTH (2026-10-15).** Per Menü wählbar („flexibel bleiben“), Standard Lowpass („für mich sinnvoller“).
* **Velocity-Ziel (2026-10-15).** Das Menü „Velocity target“ mit „Control level“ (Standard) und „Audio offset“. Regel für „Audio offset“: Velocity als Gleichanteil nach dem Gate/ATT; Velocity 0 = keine Wirkung; Schnitt in Verschieberichtung bei ±10 V; der Offset wirkt nur bei offenem Gate, dann mit vollem Wert (verworfen: Offset vor dem Gate, feste Untergrenze 0 V oder −5 V, Kappung bei +5 V oder +10 V am Eingang).
* **Dokumentation (2026-09-30).** Die Dokumentation liegt öffentlich im Repo; Originalnamen, Modellnummern, Links und wörtliche Zitate aus Hersteller-Handbüchern werden vermieden, Quellen nur als „Referenzdokumentation“.
