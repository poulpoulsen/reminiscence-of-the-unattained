# Anna-Maria – Dokumentation (Entwurf, Deutsch)

Anna-Maria ist ein Quad-Lowpass-Gate für VCV Rack, stark inspiriert von der Welt der Buchla-Module. Es gehört zum Plugin „reminiscence of the unattained“, Version 2.4.0.
Stand: 2026-10-24. Status: Entwurf.

Bis zum Hörtest in Rack sind alle Klangaussagen am Modell gemessen, nicht gehört. In Rack noch nicht getestet sind der lineare Gate-Anteil (Abschnitt 5), das Velocity-Ziel „Audio offset“ (Abschnitt 3a) und der gestimmte Resonator mit V/OCT und WET (Abschnitt 6a). Den Resonator hat Kinovice in Rack gebaut und mit Puls und Rauschen ausprobiert, die Testreihe steht aus. Das Panel mit vier Kanälen ist in Rack bestätigt.

Anna-Maria ist ein eigenständiges Werk und steht in keiner Verbindung zu den Herstellern der Vorbilder. Alle Marken und Namen gehören ihren Inhabern.

Alle Zahlenwerte stammen, wenn nichts anderes angegeben ist, aus Testprogrammen gegen die Engine. Dabei gelten 48 kHz, kein Oversampling und für das Vactrol der Modus „Reference“. Die Abschnitte 4 bis 8 beschreiben das Paper-Modell, also LOWPASS sowie BOTH und VCA mit „Gate response = Paper circuit“. Was sich mit dem linearen Gate-Anteil ändert, steht in Abschnitt 5. Die Messungen zur Nichtlinearität (Abschnitt 7) nennen jeweils den Solver „Corrected“ oder „Reference“.

---

## 1. Überblick

Anna-Maria bildet ein Lowpass Gate nach. Es besteht aus einem Tiefpass und/oder einem VCA. Eine Steuerspannung (CV) regelt über ein Vactrol, also eine LED mit lichtabhängigem Widerstand, wie weit beide öffnen. Das Modell folgt dem Paper *Parker/D'Angelo, DAFx-13* und der Octave-Simulation `buchla_lp_gate.m` (Stefano D'Angelo, MIT). Die C++-Engine stimmt in sieben Testfällen mit der Octave-Referenz auf 1e-13 bis 1e-15 (relativ) überein.

Besonderheiten dieses Moduls:

* **Vier getrennte, monophone Kanäle** (LPG 1–4) und eine Spalte ALL mit globalen Elementen, zusammen 31 HP. Jeder Kanal hat eigene Regler und CV-Eingänge für ATT, Velocity, THRESHOLD und FEEDBACK sowie eigene Schalter für MODE, FB NL und DEEP. Der Ausgang ALL summiert die vier Kanäle.
* **Steuerweg nach Hardware-Vorbild:** ATT ist ein Steuerpegel von 0…10 V, Velocity ein Gleichanteil darauf. Eine gesteckte CV ersetzt jeweils den Regler (Abschnitt 3).
* **VCA und BOTH mit linearem Gate-Anteil** (Standard): VCA verhält sich wie ein linearer VCA. BOTH schaltet erst den linearen VCA und dann das Lowpass-Gate in Reihe (Abschnitt 5). Das Paper-Modell bleibt im Menü wählbar.
* **DRIVE und VACTROL** für das ganze Modul, beide in der Spalte ALL. DRIVE stellt den Pegel in das Modell ein und wirkt nur auf die Nichtlinearität. VACTROL ist der Zeitfaktor des Vactrols (Abschnitt 8).
* **Feedback** wahlweise normiert (Standard) oder schaltungsgetreu (absolut).
* **V/OCT-Eingang und WET** (Abschnitt 6a): Jeder Kanal hat einen V/OCT-Eingang. Er stimmt einen Resonator vor dem Gate auf eine Note. So lässt sich ein Ping oder Pluck auf einer Tonhöhe spielen. Ein globaler WET-Regler mit polyphonem CV-Eingang stellt den Pegel des Resonators ein.
* **Modellvarianten im Kontextmenü:**
  * Vactrol-Verhalten (Abschnitt 8, Standard = Referenz)
  * NL-Solver (Abschnitt 11, Standard = Corrected)
  * Gate-Anteil und Filterstufe von BOTH (Abschnitt 5)
  * Velocity-Ziel (Abschnitt 3a)
  * optionale Sättigung (Abschnitt 7a)

## 2. Signalfluss

Je Kanal, Paper-Modell, MODE LOWPASS:

```
Audio IN ─ ÷Pegelfaktor ─► [ Audiopfad: 2 Integratoren + Differenzierer,
                              Rückkopplung (optional tanh) ] ─► ×Pegelfaktor ─► Audio OUT
                                         ▲
                                         │ Rf (Vactrol-Widerstand)
ATT (Regler oder CV) ─┐
                      ├─ + ─ Begrenzung 0…10 V ─ + THRESHOLD (Offset) ─ Shelving ─ Kennlinie ─ LED-Strom ─ Vactrol (Tiefpass auf Rf)
Velocity (Regler/CV) ─┘
```

* Der Vactrol-Widerstand Rf bestimmt zugleich die Verstärkung des VCA-Teils und die Grenzfrequenz des Tiefpasses.
* Im offenen Zustand beträgt Rf etwa 1,5 kΩ. Geschlossen reicht er bis in den MΩ-Bereich.
* **Steuerpegel:** `cv = begrenze(ATT + Velocity, 0 … 10 V)`. THRESHOLD wirkt als Offset nach dieser Begrenzung (Abschnitt 4).
* **Linearer Gate-Anteil (VCA, BOTH):** `g = cv / 10`, geglättet mit den Vactrol-Zeitkonstanten.
  * VCA: `Ausgang = g · Eingang`, ohne Filter.
  * BOTH: erst `g · Eingang`, danach die Filterstufe (Abschnitt 5).
* **Ausnahme Velocity-Ziel „Audio offset“:** Der Steuerpegel ist dann `begrenze(ATT, 0 … 10 V)`. Velocity wird hinter dem Gate als Gleichanteil zum Signal addiert (Abschnitt 3a).

## 3. Bedienelemente und ihre Wirkung

Alle Regler und Schalter außer denen in der Spalte ALL gelten pro Kanal. Die Spalten ALL und LPG 1–4 tragen ihre Beschriftungen über den Elementen.

**Kanalspalte (LPG 1–4), Reihen von oben:**

| Element | Bereich / Standard | Kurzwirkung |
|---|---|---|
| IN | Audio | Eingang. IN 1 ist auf IN 2–4 normalisiert, solange dort nichts steckt. |
| OUT | Audio | Kanalausgang (mono). |
| ATT | 0…10 V, 0 V | Steuerpegel des Gate. Eine gesteckte ATT-CV ersetzt den Regler. |
| VEL | −10…+10 V, 0 V | Velocity, Gleichanteil auf dem Steuerpegel. Eine gesteckte Velocity-CV ersetzt den Regler. |
| THRESH | 0…7,5 V, 7,5 V | Schwelle des Gate (Paper-Modell). Eine gesteckte THRESHOLD-CV ersetzt den Regler. |
| FEEDBACK | 0…100 %, 0 % | Resonanz. Eine gesteckte FEEDBACK-CV ersetzt den Regler. |
| FB NL | aus/an, aus | Schaltet die tanh-Nichtlinearität im Feedback ein. |
| V/OCT | −6…+6 V, 0 V = C4 | Tonhöhe des Resonators (Abschnitt 6a). Die Buchse liegt in der Reihe von FB NL, rechts davon. |
| MODE | LOWPASS / BOTH / VCA, BOTH | Betriebsart (Abschnitt 5). |
| DEEP | aus/an, aus | Platzhalter, ohne Funktion. |

**Spalte ALL (global):**

| Element | Bereich / Standard | Kurzwirkung |
|---|---|---|
| ALL OUT | Audio | Summe der vier Kanäle, danach weicher Begrenzer auf ±10 V (linear bis ±7 V). Es wird nicht durch die Kanalzahl geteilt. |
| MODE | LOWPASS / BOTH / VCA, BOTH | Schreibt den Modus in alle vier Kanalschalter. Danach sind die Kanalschalter wieder einzeln bedienbar. |
| MODE CV | 0…10 V | Wenn gesteckt, bestimmt sie den Modus aller Kanäle und geht den Schaltern vor: unter 3,3 V = VCA, unter 6,6 V = LOWPASS, darüber BOTH. |
| DRIVE | 0–10, 5 | Pegel in das Modell (Abschnitt 7). |
| VACTROL | ×0,1…×10, Mitte = ×1 | Zeitfaktor des Vactrols (Abschnitt 8). |
| WET | 0…100 %, 35 % | Pegel des gestimmten Resonators (Abschnitt 6a). |
| WET CV | 0…10 V, polyphon | Wenn gesteckt, ersetzt sie den WET-Regler. Kanal 1–4 des Kabels steuern LPG 1–4. |

Die Spalte ALL hat keinen globalen DEEP-Schalter mehr. Die DEEP-Schalter der Kanäle sind geblieben, sie bleiben Platzhalter.

**Kontextmenü (Rechtsklick):** Es enthält DEPTH je Kanal und die Modell-Einstellungen (Abschnitt 10). Das Untermenü „Depth“ hat einen Schieber für alle vier Kanäle und je einen für Kanal 1–4, Standard 100 %. Ein Rechtsklick auf einen Kanalregler oder -schalter bietet „Apply to all 4 channels“.

**Wirkung im Einzelnen:**

* **ATT.** ATT ist der Steuerpegel des Gate in Volt. Im Standard liefert der ATT-Regler oder die gesteckte ATT-CV den Steuerpegel `cv`. Im Paper-Modell öffnet das Gate, sobald `cv` die Schwelle überschreitet. Beim linearen Gate-Anteil ist die Verstärkung `cv / 10`. Eine gesteckte CV wird nicht skaliert (1 V pro V), sie ersetzt den Regler.
* **VEL (Velocity).** Velocity ist ein Gleichanteil, der zum Steuerpegel addiert wird, als Regler oder als CV. Die Summe `ATT + Velocity` wird auf 0…10 V begrenzt. Eine negative Velocity verkleinert den Bereich, den ATT erreichen kann. Mit ATT 5 V und Velocity −5 V ist das Gate geschlossen. Alternativ lässt sich Velocity als Gleichanteil auf das Audiosignal legen (Abschnitt 3a).
* **THRESHOLD.** Im Paper-Modell ist THRESHOLD der Steuerpegel in Volt, bei dem das Gate halb offen ist. Ein kleinerer Wert lässt das Gate früher öffnen, also schon bei kleinem ATT. Bei 7,5 V öffnet es erst bei sehr hohem Steuerpegel. Ein gesteckter THRESHOLD-CV-Eingang ersetzt den Regler. Er arbeitet mit 1 V pro V und ist auf 0…7,5 V begrenzt. Eine positive CV bedeutet eine höhere Schwelle, das Gate öffnet also schwerer. Offset und Schwellenformel stehen in Abschnitt 4. Im linearen VCA hat THRESHOLD keine Wirkung. In BOTH mit Lowpass-Stufe wirkt er auf deren Filteranteil.
* **FEEDBACK.** FEEDBACK stellt die Resonanz ein (Abschnitt 6). Bei FEEDBACK-CV entsprechen 0…10 V den Werten 0…100 %, begrenzt. Beide CV-Eingänge (THRESHOLD, FEEDBACK) wirken pro Sample und sind auch mit Audiorate nutzbar. Das kostet mehr Rechenzeit, weil sich die Filterkoeffizienten dann in jedem Sample ändern. Ist ein V/OCT-Kabel gesteckt, stellt FEEDBACK stattdessen die Klingdauer des Resonators ein (Abschnitt 6a).
* **V/OCT.** Der Eingang stimmt den Resonator vor dem Gate auf die Note (0 V = C4, ±6 Oktaven). Er erzeugt selbst keinen Ton, sondern lässt das Gate bei einem Impuls oder Rauschen auf dieser Tonhöhe klingeln. Ohne Kabel ist das Verhalten wie ohne Resonator (Abschnitt 6a).
* **WET.** WET stellt den Pegel des Resonators ein, gilt für alle Kanäle und hat eine polyphone CV (Abschnitt 6a).
* **DEPTH (Kontextmenü).** DEPTH bestimmt, wie tief das Gate weit unter der Schwelle schließt (Leck). Bei 3 V unter der Schwelle sind es −31 dB (0 %) bis −35 dB (100 %), bei 6 V darunter −36 dB bis −49 dB (Paper-VCA). Die Schwelle selbst bleibt beim Drehen stehen. Intern entspricht DEPTH dem Trim. Standard ist 100 %.
* **FB NL.** FB NL schaltet die tanh-Nichtlinearität im Feedback ein (Abschnitt 7). Der Schalter wirkt dort, wo auch FEEDBACK wirkt (Abschnitt 6).
* **MODE.** Siehe Abschnitt 5.
* **DRIVE.** DRIVE legt den Pegelfaktor fest. Das ist die Eingangsspannung, die der Modellamplitude 1 entspricht: 5·2^(2·(5−Drive)/5) V, also 20 V (0), 5 V (5) und 1,25 V (10). Das Ausgangssignal wird zurückskaliert. Bei ausgeschalteter NL ändert DRIVE den Klang nicht. Mit NL bedeutet mehr Drive mehr Obertöne und weniger Pegel (Abschnitt 7).
* **VACTROL.** VACTROL ist ein Zeitfaktor auf beide Vactrol-Zeitkonstanten (Anstieg 2,4 ms, Abfall 50 ms bei ×1, das sind die Werte des Papers). Links ist es schnell, das Gate folgt der CV. Rechts ist es langsam und hüllkurvenartig (Abschnitt 8). Ein Regler gilt für alle Kanäle. Er stellt auch die Trägheit des linearen Gate-Anteils ein (Annahme, Abschnitt 5).
* **Kanäle ohne Quelle.** Ein Kanal wird nicht gerechnet, wenn weder sein eigener IN noch IN 1 belegt ist. Sein Ausgang liefert dann 0 V. Ein Kanal ohne OUT-Kabel wird übersprungen, solange ALL nicht belegt ist.

### 3a. Velocity-Ziel: „Control level“ oder „Audio offset“

Im Kontextmenü legt **Velocity target** fest, wo Velocity wirkt. Die Einstellung gilt für alle Kanäle und Modi.

* **Control level** (Standard, Hardware-Vorbild): Velocity wird zum Steuerpegel addiert, `cv = begrenze(ATT + Velocity, 0 … 10 V)`. Das Audiosignal erhält nie einen Gleichanteil. Eine negative Velocity macht das Signal im linearen VCA nur leiser. Beim Paper-Gate verschiebt sie die Öffnung. Abgeschnitten wird nichts.
* **Audio offset:** ATT bzw. ATT-CV bleibt der Steuerpegel, ohne Velocity. Velocity wirkt als Gleichanteil auf dem Signal nach dem Gate, also nach der Dämpfung durch ATT. Der Gleichanteil liegt am Kanalausgang, vor der Summe ALL.
  * Velocity 0 hat keine Wirkung, der Übergang ist fließend.
  * Das Signal wird um die Velocity-Spannung verschoben und in Verschieberichtung bei ±10 V (Rack-Bereich) geschnitten: bei negativer Velocity unter −10 V, bei positiver über +10 V. Ein Signal, das schon über der Grenze liegt, wird nicht zusätzlich beschnitten.
  * Der Offset wirkt nur bei offenem Gate, dann mit vollem Wert. Das Gewicht ist `begrenze(cv · 2, 0, 1)`: 0 bei ATT 0 V, voll ab ATT 0,5 V. Es wird mit den Vactrol-Zeiten geglättet, damit der Offset nicht knackt. Die 0,5 V sind eine Annahme.
  * Der Offset ist ein Gleichanteil am Ausgang. ALL enthält die Offsets aller vier Kanäle.
  * In LOWPASS und im Paper-Gate dient ATT als Maß der Öffnung, nicht der echte Filterzustand.

Beispiele (Gate offen):

* Signal nach ATT ±2 V, Velocity −4 V → −6 / −2 V.
* Signal ±5 V, Velocity −4 V → +1 / −9 V.
* Signal ±5 V, Velocity −7 V → −2 / −10 V (Schnitt).
* Signal ±5 V, Velocity +1 V → +6 / −4 V.
* Signal ±5 V, Velocity +7 V → +10 / +2 V (Schnitt).
* Bei ATT 0 V gibt es keinen Offset.

## 4. Gate-Verhalten: THRESHOLD, DEPTH und Steuerpegel

Dieser Abschnitt beschreibt das Paper-Modell, also LOWPASS sowie BOTH und VCA mit „Gate response = Paper circuit“. In BOTH mit linearem Gate-Anteil und Lowpass-Stufe gelten die Schwellenwerte weiter für den Filteranteil.

**Bedienung.** Das Modell kennt intern zwei Größen. *Offset* addiert 10 V × Offset zum Steuerpegel. *Trim* ist der Vorwiderstand der LED. Zusammen legen sie die Gate-Schwelle fest. Am Panel stehen dafür **THRESHOLD** (die Schwelle in Volt) und **DEPTH** (der Trim, im Kontextmenü; er bestimmt, wie tief das Gate schließt).

Das Modul rechnet den Offset aus der gewählten Schwelle:

`Offset = (T(Trim) + 0,05 V − THRESHOLD) / 10 V`

* Der Regelweg von THRESHOLD ist 0…7,5 V, Standard 7,5 V.
* Bei DEPTH 100 % (Standard) ist der Offset bei 7,5 V gleich 0. Die höchste erreichbare Schwelle ist dort 7,41 V. Das Gate ist ohne Steuerspannung also wirklich zu.
* Bei kleinerem DEPTH liegt die Schwelle bei Offset 0 höher (T(Trim)). THRESHOLD 7,5 V bedeutet dann schon einen kleinen Offset.
* Außerhalb des Bereichs begrenzt das Modul den Offset. Der Tooltip nennt die wirksame Schwelle.
* Die Umrechnung ist gegen die Engine geprüft (36 Kombinationen, größte Abweichung 0,008 V).

Alte Patches (OFFSET/TRIM, Panel mit 16 Kanälen) werden beim Laden umgerechnet. Gain und Schwelle bleiben erhalten, nur die Regler sehen anders aus. Bei Patches vor dem Hardware-Signalweg (ATT als Verstärkung ±400 %) setzt das Modul ATT beim Laden auf 0 V. Patches mit CV-Kabel klingen anders, weil die CV nicht mehr skaliert wird.

**Wie die Schwelle zustande kommt.** Das Gate öffnet, wenn `Steuerpegel + 10 V × Offset` etwa 9,12 V erreicht (bei DEPTH 50 %). Die 9,12 V sind durch die Schaltung vorgegeben, nämlich durch den Vorwiderstand und das Verhalten der LED-Stufe. Der Offset-Zweig liefert höchstens 10 V. Daraus folgt `Schwelle = 9,12 V − 10 V × Offset`:

| Offset (intern) | Schwelle | Bedeutung |
|---|---|---|
| 0 | +9,1 V (DEPTH 50 %) | Gate öffnet erst bei 9,1 V Steuerpegel |
| 0,5 | +4,1 V | Mittellage |
| 0,9 | +0,1 V | Gate gerade noch zu bei 0 V Steuerpegel |
| 1,0 | −0,9 V | Gate bei 0 V schon offen (außerhalb des Regelwegs 0…7,5 V) |

„Schwelle“ ist die Höhe des Steuerpegels, bei der das Gate halb offen ist. Sie ist ein Vergleichswert, kein Signalpegel. Der Steuerpegel fällt nie unter 0 V, deshalb gibt es am Panel keine negative Schwelle mehr.

**Woher T(DEPTH) kommt.** DEPTH wird als Reglerweg 0…100 % angezeigt (Wert 0…1) und ist keine Spannung. Es entspricht dem Trimmer des Vorbilds (TP1, 20 kΩ), der den LED-Vorwiderstand verändert. Daraus berechnet das Modul die Bezugsschwelle T in Volt:

`T = 390 000 / (20 000 · DEPTH + 33 000)`

| Wert | Bedeutung |
|---|---|
| 390 000 | Zähler (V·Ω): nach dem Paper die Zenerspannung D1 (3,9 V) mal R5 (100 kΩ). Er legt die Gesamtskala fest. |
| 33 000 | R7 (33 kΩ), der feste Anteil des Vorwiderstands. Er bleibt auch bei DEPTH 0 %. |
| 20 000 · DEPTH | R6, der Trimmer. Bei DEPTH 0 % ist er ganz draußen, bei 100 % liegen die vollen 20 kΩ im Vorwiderstand. |
| Ergebnis T | Steuerpegel in Volt, bei dem der Knick der LED-Kennlinie erreicht wird. Die Einheit ist Volt, weil der Zähler (V·Ω) durch Ω geteilt wird. |

**Richtung:** Mehr DEPTH vergrößert den Nenner und verkleinert T. Das Gate öffnet dann bei weniger Steuerspannung, ist also empfindlicher. Die Werte sind T = 11,82 V (DEPTH 0 %), 9,07 V (50 %) und 7,36 V (100 %). Mit dem Zuschlag von 0,05 V (THR_HALF) ergeben sich 11,86 V, 9,12 V und 7,41 V. Das sind die Schwellen bei Offset 0. Liegt THRESHOLD darüber, ist der Offset 0. Liegt er darunter, wird die Differenz als Offset auf den Steuerpegel addiert.

**Physikalischer Hintergrund.** Im Original fällt der Vactrol-Widerstand nicht gleichmäßig mit der Steuerspannung. Die LED-Kennlinie hat einen Knick dort, wo der Strom Ia = VB / (R6 + R7) erreicht wird. Darunter leuchtet die LED kaum, und der Widerstand bleibt hoch (Überschlag: ca. 64 kΩ). Wenige Zehntel Volt darüber fällt er steil auf wenige kΩ, das Gate ist dann voll offen.

T ist die Steuerspannung, bei der dieser Knick erreicht wird. Der Trimmer verschiebt den Knickstrom. Das Modul rechnet den Abstand zwischen Knick und THRESHOLD in einen Offset auf den Steuerpegel um. Im Modell ist das Gate bei dieser Spannung halb offen und ca. 0,1 V darüber voll offen. Das ist ein Überschlag und nicht am Bauteil gemessen. Die Zuordnung der Zahlen zu den Bauteilen D1, R5, R6 und R7 ist aus dem Paper abgeleitet, nicht am Quelltext geprüft.

**Wofür THRESHOLD nützlich ist:**

* **Schwelle in die Hüllkurve legen.** Eine Hüllkurve von 0…10 V an der ATT-CV öffnet das Gate erst, wenn sie die Schwelle überschreitet. Ein kleinerer THRESHOLD öffnet früher und länger, ein größerer nur im oberen Teil der Hüllkurve.
* **Gate ohne Hüllkurve offen halten.** Bei THRESHOLD 0 V liegt der Halböffnungspunkt bei Steuerpegel 0. Mit etwas ATT oder positiver Velocity ist das Gate dann offen (Dauerton, offenes Filter).
* **Bereich verschieben.** Eine negative Velocity verkleinert den Bereich des ATT-Reglers. THRESHOLD legt fest, ab welchem Steuerpegel das Gate öffnet. Beide zusammen bestimmen, wo am ATT-Regler der Übergang liegt.
* **Moduliert, auch mit Audiorate.** Der THRESHOLD-CV-Eingang (1 V pro V, 0…7,5 V) verschiebt die Schwelle pro Sample.

Vorsicht: Bei THRESHOLD knapp über 0 V (±0,1 V) liegt die Schwelle so nah am Steuerpegel 0 V, dass das Gate bei ATT 0 V halb offen bleibt. Der Tooltip zeigt für den eingestellten Wert, ob das Gate bei ATT 0 V offen, halb offen oder geschlossen ist.

Die folgenden Tabellen verwenden die internen Größen Offset und Trim. Die Umrechnung lautet THRESHOLD = T(Trim) + 0,05 V − 10 V · Offset.

Effektive Schwelle (Verstärkung 50 % der offenen Gain), gemessen im VCA-Modus bei 1 kHz:

`CV_Schwelle ≈ T(Trim) − 10 V · Offset`, mit `T = 390 000 / (R6 + R7)`, `R6 = 20 kΩ · Trim`, `R7 = 33 kΩ`.

| Offset \ Trim | 0,0 | 0,5 | 1,0 |
|---|---|---|---|
| 0,00 | nie (> 10 V) | 9,12 V | 7,40 V |
| 0,25 | 9,38 V | 6,62 V | 4,90 V |
| 0,50 | 6,88 V | 4,12 V | 2,40 V |
| 0,75 | 4,38 V | 1,62 V | −0,10 V |
| 1,00 | 1,88 V | −0,88 V | −2,60 V |

Bei Offset 0 und Trim 0 öffnet das Gate im Bereich ±10 V also nie. Am Panel entspräche das THRESHOLD +11,9 V, außerhalb des Regelbereichs.

**Kennlinie** (Offset 0,5, Trim 0,5, VCA, 1 kHz):

| CV [V] | Rf | Gain | dB |
|---|---|---|---|
| −4 | 1352 kΩ | 0,0013 | −57 |
| −2 | 291 kΩ | 0,0083 | −42 |
| 0 | 143 kΩ | 0,0171 | −35 |
| 2 | 90 kΩ | 0,0271 | −31 |
| 4 | 63,5 kΩ | 0,0379 | −28 |
| ≥ 4,5 | 1,5 kΩ | 0,633 | −4 |

* **Unterhalb der Schwelle** (Leckbereich) steigt die Gain nur langsam. Zwischen −1 V und 4 V sind es etwa 1,5–3 dB pro Volt, darunter steiler (−4 → −2 V: 16 dB). Das Gate ist „zu“, aber nicht stumm: Bei CV 0 V liegt die Gain noch bei −35 dB.
* **Die Öffnung geschieht abrupt.** Zwischen 10 % und 90 % der offenen Gain liegen nur 0,19 V (4,07…4,26 V). Ein CV-Signal, das die Schwelle überschreitet, schaltet das Gate also fast wie einen Schalter. Sanfte Hüllkurven wirken nur im Leckbereich und in diesem schmalen Übergang.
* **Offene Gain:** VCA 0,633 (−4 dB), Lowpass 1,009, unabhängig von Offset und Trim.
* Die CV wird intern auf ±10 V begrenzt.

Praktische Folge: Eine Hüllkurve von 0…10 V an der ATT-CV öffnet das Gate bei THRESHOLD 4,1 V erst nach 41 % ihres Anstiegs und schließt es bei 4,1 V wieder. Das gilt für Offset 0,5 und Trim 0,5, im Panel für DEPTH 50 %. Mit THRESHOLD legt man fest, wo das geschieht.

### 4b. THRESHOLD (Offset), DEPTH (Trim), ATT und Velocity im Detail

Messwerte: `tests/doku_offset_trim_att.cpp`, Ausgabe `tests/doku_offset_trim_att_ausgabe.txt`.

**Das Prinzip.** Das Gate hat intern genau eine Steuergröße, die „wirksame Steuerspannung“. ATT, Velocity, THRESHOLD und DEPTH bestimmen, wie sie entsteht und ab wann das Gate öffnet:

```
ATT (Regler oder CV) ─┐
                      ├─ + ─► begrenzt auf 0…10 V ─► Shelving ─┐
Velocity (Regler/CV) ─┘                                         ├─► Strom ─► LED ─► Vactrol-Widerstand Rf ─► Filter/VCA
THRESHOLD ─► 10 V × Offset (Gleichanteil) ──────────────────────┘            ▲
DEPTH ─► Vorwiderstand der LED (verschiebt die Schwelle) ───────────────────┘
```

Die Schwelle, bei der das Gate zu 50 % offen ist, liegt bei **Steuerpegel ≈ T(Trim) − 10 V·Offset**. T ist 11,82 V (Trim 0), 9,07 V (0,5) und 7,36 V (1).

**Das Gate ist in diesem Modell eher ein Schalter als ein Regler.** Es gibt drei Bereiche:

| Bereich | Lage relativ zur Schwelle | Filter (LOWPASS) | VCA |
|---|---|---|---|
| **offen** | über Schwelle + 0,1 V | Grenzfrequenz 12,95 kHz, Gain 1,0 | Gain 0,633 |
| **Übergang** | ± 0,1 V um die Schwelle (10…90 % in 0,17…0,22 V) | Grenzfrequenz von ≈ 400 Hz bis 13 kHz | Gain von 0,04 bis 0,63 |
| **zu (Leck)** | darunter | Grenzfrequenz 383 Hz (0,5 V unter), 296 Hz (2 V), 192 Hz (4 V), 39 Hz (8 V); Messwerte Trim 0,5 | −29 dB (0,5 V unter), −31 dB (2 V), −35 dB (4 V), −56 dB (8 V) |

Im Bereich „zu“ bewegt sich der Klang nur langsam. Nahe der Schwelle ändert sich die Grenzfrequenz um etwa eine Oktave pro 4 V, weiter unten schneller. Beim Überschreiten der Schwelle öffnet sich das Gate dann in einem Schritt. Weiche Filterfahrten entstehen daher vor allem durch das **Vactrol** (Anstieg 2,4 ms, Abfall 50 ms, Abschnitt 8) und nicht durch eine langsame CV. Eine langsame CV, etwa von einem LFO oder einer Hüllkurve, wird erst hörbar, wenn sie die Schwelle kreuzt.

**THRESHOLD (intern: Offset).**

* **Wirkung auf das Signal:** Reine Verschiebung. Die Gain ist bei (CV −3 V, Offset 0,5), (CV 0 V, Offset 0,2) und (CV −6 V, Offset 0,8) identisch (0,00457). Alle drei haben CV + 10·Offset = 2 V.
* **Ohne CV-Kabel** ist der Offset ein Handregler für das Gate. Bei Trim 0,5 ist das Gate ab Offset ≈ 0,91 offen (Schwelle 9,07 V). Darunter ist es „zu“, und der Regler verändert nur das Leck:
  * LOWPASS-Grenzfrequenz: 26 Hz (Offset 0), 64 Hz (0,2), 139 Hz (0,4), 236 Hz (0,6), 346 Hz (0,8), 404 Hz (0,9).
  * VCA: −110 dB (0), −47 dB (0,2), −38 dB (0,4), −33 dB (0,6), −30 dB (0,8), −28 dB (0,9).
  * Bei Offset 1,0 ist das Gate offen.
* **Mit CV-Kabel** verschiebt der Offset die Lage der Schwelle gegenüber der Hüllkurve. Ein hoher Offset öffnet das Gate früher im Anstieg und schließt es später im Abfall, die Öffnungszeit wird länger. Ein niedriger Offset öffnet erst bei hoher CV, die Öffnungszeit ist kurz.
* **Dynamik:** Der Offset ist eine Gleichgröße und läuft nicht durch das Shelving-Filter. Schnelle CV-Flanken bekommen dagegen einen kurzen Überschwinger (Abschnitt 8).
* **Reihenfolge:** Die Offset-Spannung wird nach der ±10-V-Begrenzung der CV addiert, wirksam sind also bis zu 20 V. Velocity wirkt dagegen vor der Begrenzung, weil sie Teil des Steuerpegels ist (Abschnitt 3).

**DEPTH (intern: Trim).**

* **Wirkung 1:** DEPTH verschiebt die Schwelle um bis zu 4,5 V (T = 11,82 → 7,36 V). Ein hoher Trim lässt das Gate früher öffnen. Bei Offset 0,5 liegt die Schwelle bei 6,88 / 5,32 / 4,12 / 3,17 / 2,40 V für Trim 0 / 0,25 / 0,5 / 0,75 / 1.
* **Wirkung 2:** Trim ändert auch das Leck. Je höher der Trim, desto stärker fällt das Gate weit unter der Schwelle ab:
  * 6 V unter der Schwelle: −36 dB (Trim 0), −41 dB (0,5), −49 dB (1).
  * LOWPASS-Grenzfrequenz 8 V unter der Schwelle: 101 Hz (Trim 0), 39 Hz (0,5), 26 Hz (1).
  * In der Nähe der Schwelle ist der Unterschied klein. 1 V darunter sind es −29 / −30 / −30 dB, 3 V darunter −31 / −33 / −35 dB für Trim 0 / 0,5 / 1.
* **Wirkung 3:** Der Übergang wird mit hohem Trim minimal steiler (0,22 V bei Trim 0, 0,17 V bei Trim 1).
* **Faustregel am Panel:** THRESHOLD legt die Schwelle fest, passend zur Höhe der Hüllkurve. DEPTH bestimmt nur, wie tief das Gate darunter schließt. Wer eine feste Hüllkurve von 0…10 V hat, stellt zuerst THRESHOLD so ein, dass die Schwelle in der Hüllkurve liegt. Danach wählt er mit DEPTH die Tiefe des Schließens.

**ATT und Velocity (Steuerpegel).**

* **Wirkung auf das Signal:** Der Steuerpegel ist `begrenze(ATT + Velocity, 0 … 10 V)`. Das Gate öffnet, wenn dieser Pegel die Schwelle überschreitet. Der Steuerpegel kommt vom Regler oder, wenn gesteckt, von der CV, die den Regler ersetzt.
* **Hüllkurve 0…10 V an der ATT-CV, Schwelle 4,12 V** (Offset 0,5, Trim 0,5):
  * Die Hüllkurve öffnet das Gate voll, sobald sie 4,2 V erreicht.
  * Eine Hüllkurve mit nur 4,12 V Spitze liegt genau auf der Schwelle (−13 dB).
  * Mit 2,5 V Spitze öffnet das Gate nicht mehr (−31 dB).
  * Eine negative Velocity verschiebt die Hüllkurve nach unten. Das Gate öffnet dann später im Anstieg und schließt früher im Abfall, die Öffnungszeit wird kürzer.
  * Bei einer exponentiell abfallenden Hüllkurve 10 V·e^(−t/200 ms) ergibt die Geometrie ohne Vactrol Öffnungszeiten von ca. 178 ms (Spitze 10 V), 120 ms (7,5 V) und 38 ms (5 V).
* ATT 0 V ohne CV hält das Gate dauerhaft auf der Ruhelage (Steuerpegel 0 V plus Velocity).
* Der Steuerpegel kann nicht unter 0 V fallen. Die Invertierung der alten Version (CV ATT negativ) und die Verstärkung über 100 % gibt es im Quad-Panel nicht mehr.
* Steuerpegel über 10 V werden begrenzt. Im Modell ergeben 10, 15 und 20 V dieselbe offene Gain.
* **Reihenfolge im Modul:** `ATT (Regler oder CV) + Velocity` → Begrenzung auf 0…10 V → Shelving. Die Offset-Spannung (THRESHOLD) kommt erst nach der Begrenzung dazu. Eine negative Velocity hebt deshalb einen niedrigen THRESHOLD nicht auf.

**Typische Einstellungen.**

| Ziel | Einstellung |
|---|---|
| Dauer-Ton, Filter fest auf ≈ 400 Hz ohne Hüllkurve | LOWPASS, THRESHOLD knapp über 0 V (z. B. 0,1 V), DEPTH 50 % |
| Dauer-Ton offen ohne Hüllkurve | THRESHOLD 0 V, ATT einige Volt |
| Hüllkurve 0…10 V soll in der Mitte öffnen | THRESHOLD 4,1 V, DEPTH 50 % |
| Kurzer Ping aus derselben Hüllkurve | THRESHOLD erhöhen oder Velocity senken: Das Gate ist nur im oberen Teil offen |
| Längere Öffnung | THRESHOLD senken |
| Pluck mit tiefem Schließen (0…10-V-Hüllkurve) | MODE LOWPASS, DEPTH 100 %, THRESHOLD 7,5 V, Vactrol response = Conductance (Abschnitt 14); Modellwerte für BOTH mit State-based in Abschnitt 4c |

**Offene Frage.** Das Modell schaltet an der Schwelle in ca. 0,2 V von „zu“ auf „offen“ (Zenerverhalten der Schaltung im Modell). Ob das Originalgerät genauso steil schaltet, ist nicht geprüft. Aufschlussreich wäre im Hörtest ein Vergleich mit einem Video oder Gerät mit langsamer CV, etwa einem LFO-Dreieck: Öffnet das Original ebenfalls abrupt?

### 4c. Pluck: tief schließendes Gate

Das Gate schließt nur so tief, wie der Steuerpegel in der Ruhe unter der Schwelle liegt. Eine 0…10-V-Hüllkurve ruht bei 0 V. Bei THRESHOLD 4,1 V sind das nur 4,1 V Abstand. Das Gate bleibt „angelehnt“ (Boden bei ca. −20 dB), und nach dem kurzen Burst bleiben vor allem die Bässe stehen. Das klingt wie ein tiefes Knacken, nicht wie ein Pluck.

Im Quad-Panel kann der Steuerpegel nicht unter 0 V fallen, weil ATT 0…10 V plus Velocity auf 0…10 V begrenzt wird. Die Ruhelage liegt daher bei 0 V, und der Abstand zur Schwelle ist die Schwelle selbst. Ein hoher THRESHOLD bei DEPTH 100 % liefert dann den größten Abstand.

Am Modell gemessen (Paper-Variante, BOTH, State-based, Rauschen, Hüllkurve 0…10 V, Abfall 100 ms, Pegel in 10-ms-Fenstern):

* DEPTH 100 %, THRESHOLD 7,4 V: Ruhe −76 dB, Spitze −4 dB. Der Pegel nach 20 / 50 / 100 / 200 / 400 ms beträgt 0 / −9 / −13 / −21 / −36 dB.
* Der Standard 4,1 V (Offset „mittel“) lässt das Gate „angelehnt“ (Ruhe −21 dB).

Ein langsames Vactrol (VACTROL rechts) macht den Schwanz länger, senkt aber die Spitze. Das Gate geht nicht voll auf, solange die Hüllkurve kurz ist. Eine schnelle Hüllkurve zusammen mit einem schnellen Vactrol (VACTROL links) klingt härter.

Die Zahlen sind Modellwerte, keine Hörergebnisse, und gelten für den Paper-Gate-Anteil. Im linearen Gate-Anteil von BOTH schließt das Gate bei ATT 0 V vollständig. Die Messwerte oben gelten für BOTH mit State-based. Der im Hörtest gefundene Pluck (Abschnitt 14, Beispiel 1) läuft dagegen in LOWPASS mit Vactrol response = Conductance, THRESHOLD 7,5 V und DEPTH 100 %. Dort liegt die Ruhelage ebenfalls bei 0 V, und der Abstand zur Schwelle beträgt ca. 7,4 V.

### 4a. Gate geschlossen ist nicht stumm

Wie beim Original erscheint das Eingangssignal auch bei geschlossenem Gate am Ausgang, gefiltert beziehungsweise abgeschwächt. Die Tabelle zeigt die Gain in dB (Offset 0,5, Trim 0,5, Feedback 0):

| Modus | CV 0 V | CV −4 V | CV −10 V |
|---|---|---|---|
| LOWPASS | 100 Hz −1,6 / 1 kHz −17 / 5 kHz −41 | −17 / −51 / −80 | −68 / −107 / −136 |
| BOTH | −0,6 / −7 / −21 | −7,5 / −28 / −53 | −42 / −80 / −109 |
| VCA | alle Frequenzen ≈ −35 | −55 … −69 | −91 … −125 |

Nur in LOWPASS und BOTH bleibt ein tiefpassgefiltertes Signal hörbar. VCA schwächt breitbandig ab. Ob die Werte dem Originalgerät entsprechen, ist nicht geprüft, da keine Messdaten vom Original vorliegen.

Die Tabelle gilt für das Paper-Modell. Mit dem linearen Gate-Anteil (Standard in VCA und BOTH) schließt das Gate in VCA bei ATT 0 V vollständig (Gain 0). In BOTH ist die Gain `cv / 10` mal die Gain der Filterstufe.

## 5. Modi

Der Modus pro Kanal bestimmt die Schaltung. Er wird per MODE-Schalter, globalem MODE oder MODE CV gewählt. LOWPASS ist immer das Paper-Modell. Bei VCA und BOTH wählt das Kontextmenü **Gate response (VCA, BOTH)**:

* **Linear** (Standard): Das Gate ist ein linearer VCA. Die Referenzdokumentation des Hardware-Moduls beschreibt den Gate-Modus als linearen VCA und den kombinierten Modus als Zusammenschaltung aus linearem Gate und Lowpass. Deshalb ist der lineare Anteil Standard.
* **Paper circuit**: Das Paper-Modell (Vactrol-Teiler mit Knick bei THRESHOLD), wie in den Abschnitten 4 bis 8. Es schaltet steil, und in VCA beträgt die offene Gain nur 0,63.

**Linearer Gate-Anteil.** Die Verstärkung ist `g = begrenze(cv, 0 … 10 V) / 10`, also 10 V = Verstärkung 1. Das ist eine Annahme, denn die Referenzdokumentation nennt keine Zahlen. Die Verstärkung wird mit den Vactrol-Zeitkonstanten geglättet (Anstieg 2,4 ms, Abfall 50 ms, mal VACTROL-Faktor). Die Richtung folgt dem Zustand: Steigt g, gilt die schnelle Konstante. Ob das Hardware-Modul im Gate-Modus Vactrol-Trägheit hat, ist nicht belegt (Annahme).

| Modus | Gate response = Linear (Standard) | Gate response = Paper circuit |
|---|---|---|
| LOWPASS | Tiefpass mit C3 = 4,7 nF, Ra = 5 MΩ (Paper-Gate), unverändert | derselbe Modus |
| BOTH | Erst linearer VCA, dann Filterstufe (Menü „BOTH filter stage“) | Tiefpass und VCA gleichzeitig (C3 = 0, Ra = 5 MΩ), wie vor der Änderung |
| VCA | Reiner linearer VCA: `Ausgang = g · Eingang`, kein Filter | Reiner VCA (C3 = 0, Ra = 5 kΩ), offene Gain 0,633 |

Klangliche Einordnung:

* **LOWPASS:** Der Pegel bleibt im Durchlassbereich nahe 1, nur die Höhen schließen. Das Gate wirkt wie ein Filter mit CV-gesteuerter Grenzfrequenz (404 Hz bei Steuerpegel 4 V im Beispiel von Abschnitt 4). Das Feedback ist hier immer wirksam.
* **BOTH (linear):** VCA und Lowpass-Gate liegen in Reihe. Die Lautstärke folgt linear dem Steuerpegel, und das Filter schließt zusätzlich die Höhen. Das Menü **BOTH filter stage** bietet zwei Filterstufen:
  * *Lowpass* (Standard) verwendet die Filterstufe von LOWPASS. THRESHOLD, DEPTH, FEEDBACK und FB NL wirken dort wie in LOWPASS, und das Filter schwingt nach dem Schließen des Gate leise nach. Sie ist Standard, weil Kinovice sie für sinnvoller hält und flexibel bleiben will.
  * *Paper BOTH* verwendet C3 = 0. Das Filter filtert kaum und hat keine Resonanz, FEEDBACK und FB NL bleiben ohne Wirkung. BOTH ist dann das Paper-BOTH mit vorgeschaltetem linearem Gate.
* **VCA (linear):** Es gibt kein Filter, nur Pegel. THRESHOLD, DEPTH, FEEDBACK und FB NL wirken nicht. Die Lautstärke ist exakt `cv / 10`.

Messwerte am Modell (48 kHz):

* **VCA linear, Sinus 100 Hz:** Steuerpegel 0 / 0,5 / 1 / 2,5 / 5 / 7,5 / 10 V ergeben die Gain 0 / 0,05 / 0,1 / 0,25 / 0,5 / 0,75 / 1,0, unabhängig von THRESHOLD, DEPTH und FEEDBACK. Bei einem Sprung 0 ↔ 10 V (VACTROL ×1) erreicht der Anstieg 63 % nach 2,40 ms und 95 % nach 7,19 ms. Beim Abfall sind es 37 % nach 50,0 ms und 5 % nach 149,8 ms.
* **BOTH linear, stationär** (THRESHOLD 7,5 V, DEPTH 100 %, FEEDBACK 0, Sinus 100 Hz). Gain für Steuerpegel 1 / 3 / 5 / 7 / 10 V:
  * Lowpass-Stufe: 0,019 / 0,213 / 0,443 / 0,659 / 0,999.
  * Paper-Stufe: 0,051 / 0,266 / 0,474 / 0,679 / 0,999.
  * Zum Vergleich Paper-BOTH ohne linearen Anteil: 0,505 / 0,886 / 0,949 / 0,969 / 0,999.
  * BOTH linear mit Lowpass-Stufe entspricht LOWPASS × `cv / 10`.
* **Nachschwingen von BOTH nach dem Gate-Schluss.** Der Ping besteht aus einem 20-ms-Burst mit 600 Hz und Amplitude 1. Danach ist der Eingang 0 und das Gate zu (THRESHOLD 7,5 V, FEEDBACK 90 %). Die Lowpass-Stufe schwingt mit 0,0081 (20–50 ms) und 0,0025 (100–200 ms) nach, die Paper-Stufe mit 0,0000. Das Nachschwingen ist also leise, etwa 0,8 % der Burst-Amplitude.
* **Stabilität BOTH linear** (Lowpass-Stufe, FEEDBACK 100 %, normiert, NL aus): Die Spitze beträgt 33,7, in LOWPASS bei gleicher Eingabe 43,7. Der Schutz des Moduls greift bei 50. Es entsteht kein NaN.

Moduswechsel im Betrieb:

* LOWPASS ↔ BOTH geschieht ohne Sprung (Abweichung 6e-4, der Gleichspannungsgewinn der Lowpass-Stufe).
* Aus dem reinen linearen VCA zurück in LOWPASS oder BOTH startet das Filter mit leeren Zuständen. Dabei ist ein kurzer Einschwingvorgang möglich.
* Das Umschalten der Filterstufe im Betrieb erzeugt weder NaN noch einen Sprung über dem normalen Signal.

Paper-Werte zur Orientierung (Gate response = Paper circuit):

| Modus | Schaltung | Offene Gain (20 Hz) | Grenzfrequenz offen (−3 dB) | bei Steuerpegel 4,0 V (Rf 63,5 kΩ) | bei 3,0 V (Rf 74,6 kΩ) |
|---|---|---|---|---|---|
| LOWPASS | Tiefpass mit C3 = 4,7 nF, Ra = 5 MΩ | 1,009 | 12,95 kHz | Gain 0,98, f = 404 Hz | 346 Hz |
| BOTH | Tiefpass und VCA gleichzeitig (C3 = 0, Ra = 5 MΩ) | 1,009 | 19,4 kHz | Gain 0,98, f = 1188 Hz | 1014 Hz |
| VCA | reiner VCA (C3 = 0, Ra = 5 kΩ) | 0,639 | über 20 kHz | Gain 0,038 | 0,033 |

Offene Frage: Nach einer möglichen Lesart der Schaltung verwendet die Mittelstellung des Hardware-Moduls einen Zweig ohne Filterkondensator. Nach der Beschreibung wäre sie dagegen ein Lowpass mit leiser Resonanz. Beides lässt sich nicht sicher entscheiden. Deshalb ist die Filterstufe von BOTH wählbar.

## 6. Feedback (Resonanz)

### 6.0 Was FEEDBACK bewirkt

Im Modus LOWPASS führt FEEDBACK einen Teil des Ausgangs über einen Differenzierer zurück an den Filtereingang. Daraus entsteht eine Resonanz, also eine Betonung um die Grenzfrequenz. Ohne Feedback ist das Filter ein weicher Tiefpass, mit Feedback „singt“ es an einer Stelle des Spektrums.

* **Wo FEEDBACK wirkt:**
  * In LOWPASS immer.
  * In BOTH nur mit „Gate response = Linear“ und „BOTH filter stage = Lowpass“ (beides Standard).
  * In VCA wirken weder FEEDBACK noch FB NL. Dasselbe gilt für BOTH mit der Paper-Stufe oder dem Paper-Gate, weil der Kondensator C3 des Rückkopplungspfads dort im Modell abgeschaltet ist (C3 = 0).
  * Die Reglerstellung bleibt im Patch gespeichert und ist beim Umschalten wieder wirksam.
  * Gemessen: BOTH Paper und VCA ergeben mit FEEDBACK 90 % dieselbe Spitze wie mit 0 %. Der Tooltip weist darauf hin.
  * Mit gestecktem V/OCT-Kabel wirkt FEEDBACK nicht mehr auf das Filter, sondern stellt die Klingdauer des Resonators ein (Abschnitt 6a). Die Rückkopplung des Filters ist dann 0.
* **Die Spitze liegt deutlich über der Grenzfrequenz,** etwa beim 2,3- bis 2,9-fachen der −3-dB-Frequenz ohne Feedback. Im Beispiel unten liegt die Grenzfrequenz bei 407 Hz und die Spitze bei 951…1162 Hz. Das ist das Verhalten des Modells aus dem Paper, nicht das eines klassischen Resonanztiefpasses mit der Spitze an der Grenze.
* **Tiefe Frequenzen bleiben fast unberührt** (−0,2 dB bei 50 Hz). Das Feedback hebt nur den Bereich um die Spitze an.
* **Die Spitze wandert mit dem Gate.** Öffnet sich das Gate, steigt die Grenzfrequenz, und die Resonanz fährt mit nach oben. Das ist der typische „Wah“- oder „Ping“-Klang. Bei weit offenem Gate (Grenzfrequenz 13 kHz) liegt die Spitze bei etwa 19 kHz und ist kaum noch hörbar.
* **Bei Anregung klingelt das Filter nach.** Ein Impuls, eine Gate-Flanke oder ein Rauschburst lässt das Filter mit der Resonanzfrequenz ausschwingen, und zwar umso länger, je näher FEEDBACK an 100 % liegt. Bei 100 % klingt es nicht mehr ab.
* **FB NL aus** (Standard): Ein Sinus bleibt ein Sinus, nur Pegel und Phase ändern sich. **FB NL an:** Ein tanh begrenzt die Rückkopplung, und es entsteht Verzerrung. Sie tritt vor allem bei hohem DRIVE und bei Signalen nahe der Resonanzfrequenz auf (Abschnitt 7).
* **Praxis:** Für „Ping“ oder „Wah“ eignen sich 75–90 %, ein kurzes Gate und eine Eingangsfrequenz nahe der Spitze oder Rauschen. Ab etwa 95 % wird die Resonanz zu einem fast tonalen Pfeifen, das sich mit der Gate-Öffnung bewegt.

### 6.1 Messwerte

Die Messungen gelten nur für den Modus LOWPASS. Der Resonanzgipfel liegt über der −3-dB-Frequenz ohne Feedback.

Arbeitspunkt CV 4,05 V, f(−3 dB, ohne Feedback) = 407 Hz, **normiert**:

| FEEDBACK | Spitze | Spitzenfrequenz |
|---|---|---|
| 0 … 50 % | keine (−0,2…−0,1 dB) | – |
| 75 % | +2,7 dB | 951 Hz |
| 90 % | +10,0 dB | 1105 Hz |
| 95 % | +15,8 dB | 1162 Hz |
| 99 % | +28,6 dB | 1162 Hz |

* Bis 50 % geschieht klanglich wenig, die Resonanz entsteht erst ab 75 %. Die Strecke von 75 % bis 99 % überstreicht 26 dB.
* Bei 100 % liegt das Modell genau auf der Stabilitätsgrenze. Ein Impuls klingt nicht ab, es entsteht eine gleichbleibende Schwingung kleiner Amplitude (Impuls 1,0 → Spitze 0,145). Das Modul läuft nicht davon. Ein Schutz gegen Überlauf ist trotzdem eingebaut.
* Die Resonanzfrequenz folgt der CV mit dem Gate.

**Normiert und schaltungsgetreu (Kontextmenü „Resonance“):**

* *Normalized:* Der Regler 0…100 % ist auf die jeweilige Stabilitätsgrenze skaliert. Gleiche Reglerstellung bedeutet gleiche Resonanzstärke, unabhängig vom Gate-Zustand.
* *Circuit-accurate:* Der Regler 0…100 % entspricht der absoluten Rückkopplung 0…1,472. Bei offenem Gate (Rf klein) ist das praktisch gleich dem normierten Modus. Bei fast geschlossenem Gate wird die Resonanz schwächer.

Beispiel FEEDBACK 90 %:

| CV | Rf | normiert | absolut |
|---|---|---|---|
| 6,0 / 5,0 / 4,5 | 1,5 kΩ | 9,7 dB | 9,6 dB |
| 4,0 | 63,5 kΩ | 10,0 dB | 9,4 dB |
| 3,5 | 68,7 kΩ | 10,0 dB | 9,2 dB |
| 0,0 | 143 kΩ | 9,8 dB | 8,4 dB |
| −2,0 | 291 kΩ | 9,3 dB | 6,8 dB |
| −4,0 | 1352 kΩ | 6,8 dB | −0,4 dB (keine Resonanz) |

Praktisch gilt: Solange das Gate offen oder halb offen ist, sind beide Modi nahezu gleich. Ein Unterschied tritt erst im fast geschlossenen Bereich auf. Dort „schluckt“ der absolute Modus die Resonanz.

## 6a. V/OCT, Resonator und WET

Der V/OCT-Eingang macht aus dem Gate ein Instrument für Ping und Pluck auf einer festen Tonhöhe. Dazu liegt vor dem Gate ein Resonator, der auf die Note gestimmt ist. Das Gate selbst bleibt unverändert und bestimmt weiterhin, wie lange der Ton hörbar ist.

**Aufbau.**

* Der Eingang des Gate ist `x + WET · res(x)`. `x` ist das Eingangssignal, `res(x)` der gestimmte Resonator. Der Direktpfad bleibt exakt wie ohne V/OCT.
* Der Resonator hat dieselbe Tonhöhe bei jeder Note. Seine Impulsantwort ist ein abklingender Sinus mit Amplitude 1.
* Eine weiche Begrenzung am Resonatorausgang verhindert, dass Dauerrauschen mit langer Klingdauer den Pegel aufsummiert. Bis 4 Modelleinheiten ist sie linear, darüber geht sie stetig gegen 8.
* Solange V/OCT gesteckt ist, ist die Rückkopplung des Filters selbst 0. Die Resonanz kommt nur vom Resonator.
* Der Resonator wirkt nur mit Resonanzstufe: in LOWPASS und in BOTH mit „Gate response = Linear“ und „BOTH filter stage = Lowpass“. In VCA und in Paper-BOTH hat V/OCT keine Wirkung.
* Ohne gestecktes V/OCT-Kabel ist alles bitgleich zum Stand ohne Resonator.

**Bedienung.**

* **V/OCT:** 0 V = C4 (261,6256 Hz), Bereich ±6 Oktaven. Ein Audiorate-Signal ist möglich (FM).
* **FEEDBACK** (Regler oder CV) ist bei gestecktem V/OCT die Klingdauer T60 des Resonators: `T60 = 0,05 s · 60^FEEDBACK`. Das sind 0,05 s bei 0 %, 0,39 s bei 50 % und 3 s bei 100 %. Der Tooltip zeigt die Zeit.
* **WET** (Regler 0…100 %, global, Standard 35 %): Die Verstärkung des Resonators ist `0,05 · k²` mit k = WET. Das ergibt:

| WET | 25 % | 35 % (Standard) | 50 % | 75 % | 100 % |
|---|---|---|---|---|---|
| Verstärkung | 0,0031 | 0,0061 | 0,0125 | 0,028 | 0,05 |
| dB | −50 | −44 | −38 | −31 | −26 |

* **WET CV** (polyphon): 0…10 V entsprechen 0…100 % des Reglers, mit derselben Kurve. Eine gesteckte WET CV ersetzt den Regler. Kanal 1–4 des Kabels steuern LPG 1–4, ein monophones Kabel gilt für alle Kanäle. Mit Kabel ab wirkt der Regler wieder.

**Kalibrierung des Pegels.** Der Wertebereich ist auf einen Puls als Anregung zugeschnitten. Ein Puls summiert sich im Resonator zur Fläche auf, deshalb ist die nötige Verstärkung klein. Für Kinovice klang es bei einem Ausgangspegel von etwa 10 V und −45 dB gut, das entspricht WET um 33 %. Nach oben bleiben rund 19 dB Reserve. Bei anderem Material, etwa Rauschburst oder Dauerrauschen, kann der passende Wert abweichen.

**Genauigkeit (Modell, 48 kHz).**

* Die Tonhöhe stimmt von C2 bis C8 bei T60 0,1 / 0,5 / 3 s auf 0,0 Cent (C8: +0,2 bis +0,3 Cent). Das Abklingen zwischen 50 und 300 ms entspricht dem Soll (−150, −30 und −5 dB).
* Der Ringpegel bei Rauschanregung ist über die Tonlage flach (C2 bis C7 innerhalb −0,8 bis +0,1 dB). Ein Pegelausgleich für tiefe Noten ist deshalb nicht nötig.
* Beim Schließen des Gate weicht die Tonhöhe bei einer 200-ms-Hüllkurve um −4 bis −12 Cent ab (C3 bis C6, in späteren Fenstern). Bei einer 20-ms-Hüllkurve sind es bis etwa ±40 Cent, aber nur in leisen Ausklängen (−20 bis −50 dB). Das Signal gleitet nicht nach unten.
* Stabilität: Mit Audiorate-V/OCT (±2 Oktaven bei 0,7 Hz plus ±1,5 Oktaven FM bei 220 Hz), T60 3 s und 20 s Rauschen entsteht kein NaN. Die Spitze liegt bei 16,7, die Modulgrenze bei 50. Dieser Test lief mit einer größeren Verstärkung als der jetzigen.

**Hinweise zur Anwendung.**

* V/OCT erzeugt keinen Ton. Es stimmt nur die Resonanz. Ein Dauerton oder LFO am IN bleibt in seiner eigenen Tonhöhe hörbar, V/OCT ändert dann nur die Klangfarbe.
* Zum Hören der Tonhöhe braucht der Resonator einen breitbandigen Anstoß: Rauschen oder einen kurzen Impuls oder Trigger. Das Gate sollte offen sein (ATT 10 V), FEEDBACK hoch (Klingdauer lang).
* Mit Rauschen am IN steigt die Tonhöhe mit steigender Spannung (bestätigt).

**Gegenüber der früheren Fassung.** Eine erste Fassung hat das ganze Filter durch Zeitskalierung auf die Note gestimmt. Sie hat sich dumpf angehört: Bei offenem Gate und V/OCT auf C4 sank die Verstärkung für 1 kHz von −0,0 auf −20,5 dB, für 4 kHz von −0,5 auf −42,3 dB. Außerdem klang der Ton bei hohen Noten viel kürzer. Deshalb gibt es jetzt den Resonator vor dem Gate. Der Direktpfad bleibt unberührt, und das Gate klingt wie ohne V/OCT. Der frühere Menüpunkt „Pitch level compensation“ ist entfallen, weil der Pegel jetzt über die Tonlage von selbst flach ist. Ein alter Patch mit gestecktem V/OCT klingt anders (Resonator statt Zeitskalierung). Ein gespeicherter Schlüssel `pitchComp` wird ignoriert.

**Offen.** Die Rack-Testreihe (Tonhöhe C1 bis C7, WET-Reihe, WET CV, Klingdauer, V/OCT ab, Audiorate-V/OCT, Schließen des Gate, alter Patch) und der Hörvergleich mit einem Vergleichsmodul stehen aus. Mögliche Erweiterungen: ein Hold-Modus (konstante Tonhöhe über den ganzen Ton), eine einstellbare Klingzeit-Spanne (jetzt 0,05 bis 3 s) und eine Pegelkurve für WET per Menü.

## 7. Nichtlinearität (FB NL) und DRIVE

Mit FB NL = an geht die Rückkopplung durch einen tanh-Begrenzer. Das wirkt nur bei ausreichend großem Signal. DRIVE bestimmt, wie groß das Signal im Modell ist (Abschnitt 3).

Obertonanteil (Wurzel aus der Summe der Oberwellen 2…8, bezogen auf die Grundwelle), Eingang ±5 V, FEEDBACK 90 %, **NL-Solver „Corrected“**:

| CV | f | Drive 0 | 2,5 | 5 | 7,5 | 10 | Grundwelle Drive 0 / 10 |
|---|---|---|---|---|---|---|---|
| 8,0 V (offen) | 220 Hz | 0,0002 | 0,0007 | 0,0021 | 0,0038 | 0,0041 | 5,00 / 5,00 V |
| 3,0 V | 500 Hz | 0,0228 | 0,0643 | 0,1079 | 0,1199 | 0,1040 | 6,37 / 3,89 V |
| 4,0 V | 500 Hz | 0,0282 | 0,0754 | 0,1196 | 0,1282 | 0,1066 | 5,91 / 4,08 V |
| 4,0 V | 2000 Hz | 0,0007 | 0,0025 | 0,0090 | 0,0250 | 0,0460 | 2,26 / 1,70 V |

Zum Vergleich der Solver „Reference“: 0,0031 / 0,0122 / 0,0391 / 0,0724 / 0,0811 für CV 3,0 V, 500 Hz.

Was man hört:

* **Bei offenem Gate und ohne Resonanzspitze am Signal** bleibt die NL praktisch unhörbar (Oberwellenanteil unter 0,5 %).
* **Am Resonanzgipfel** entsteht deutliche Verzerrung (bis ca. 13 % Oberwellen) und der Pegel sinkt (Grundwelle 5,9 → 4,1 V). Das gilt bei halb offenem Gate und einem Signal nahe der Resonanzfrequenz.
* **Bei 500 Hz** steigt der Oberwellenanteil mit DRIVE bis ca. 7,5 und fällt dann wieder. Bei 2 kHz steigt er bis Drive 10 weiter.
* **DRIVE ohne NL** hat keine Wirkung.
* **Mit NL und FEEDBACK 100 %** (Solver „Corrected“, CV 4 V) bleibt nach einem Impuls eine kleine Schwingung bestehen. Die Spitze im Ausgang nach 5 s beträgt 0,094 V bei Drive 0, 0,030 V bei Drive 5 und 0,008 V bei Drive 10. Mit dem Solver „Reference“ klingt sie ab (0 V).

## 7a. Sättigung (Zusatz, Menü „Saturation“)

Die Sättigung ist nicht Teil des Paper-Modells. Auf Scope-Aufnahmen des Originals hat ein Sinus eine leicht abgeflachte Spitze, das Modell hat im Audiopfad aber keine Sättigung. Mit „Saturation = On“ läuft das Modellausgangssignal (in Modelleinheiten) durch `tanh` und wird danach auf Volt zurückskaliert. Standard ist Off.

* **DRIVE** bestimmt die Intensität (Pegel im Modell, Abschnitt 3). Die Kleinsignalverstärkung bleibt 1, es ändert sich nur der Verlauf großer Amplituden. Die Sättigung wirkt in allen Modi und auch bei Feedback 0.
* Gemessen für einen Sinus mit 5 V Spitze, wenn das Modell Verstärkung 1 hat (LOWPASS offen 1,009):

| DRIVE | Modellamplitude | Oberwellen 3…9 / Grundwelle | Ausgangsspitze |
|---|---|---|---|
| 0 | 0,25 | 0,5 % | 4,90 V |
| 2,5 | 0,50 | 2,0 % | 4,62 V |
| 5 | 1,00 | 6,7 % | 3,81 V |
| 7,5 | 2,00 | 17,3 % | 2,41 V |
| 10 | 4,00 | 30,0 % | 1,25 V |

* Es entstehen nur ungeradzahlige Obertöne, weil die Kennlinie symmetrisch ist.
* **Der Ausgangspegel sinkt mit steigendem DRIVE.** Die Ausgangsspitze ist auf den Pegelfaktor begrenzt (20 / 5 / 1,25 V bei Drive 0 / 5 / 10). Bei hohem Drive sollte man den Pegel deshalb dahinter anheben.
* Im VCA-Modus (offene Gain 0,63) und bei teilweise geschlossenem Gate ist die Amplitude im Modell kleiner, und die Sättigung wirkt entsprechend schwächer. Am stärksten wirkt sie bei offenem Gate und lautem Signal.
* **Hörtest (Kinovice, Rack):** Der warme Klang eines Sinus stellt sich ein, wenn DRIVE bei etwa 75 % (≈ 7,5) steht. Das entspricht rund 17 % Oberwellen (Tabelle). Bei Drive 5 ist die Wirkung zu schwach.
* Die Stufe ist ein Zusatz und keine Messung am Original. Der gemessene Effekt entspricht dem Rechenmodell, nicht notwendig dem Klang des Geräts.

## 8. Zeitverhalten: Vactrol

Das Modell hat zwei Zeitkonstanten: Öffnen 2,4 ms, Schließen 50 ms (Tiefpass auf dem Widerstand Rf). Der Regler **VACTROL** skaliert beide Werte um den Faktor ×0,1…×10 und gilt für alle Kanäle. Die Mitte (×1) entspricht den Werten des Papers und ist bitgenau gegen die Referenz geprüft. VACTROL stellt auch die Trägheit des linearen Gate-Anteils ein (Abschnitt 5).

Gemessen (State-based, Paper-VCA, Sprung −10 → +10 V): Der Anstieg von 10 auf 90 % dauert bei ×0,3 3,2 ms, bei ×1 10,5 ms und bei ×3 31,6 ms. Die Tabellen in diesem Abschnitt gelten für ×1.

Das Kontextmenü **Vactrol response** bietet:

* *Reference (as published):* wie die Octave-Referenz. Die Referenz wählt die schnelle Konstante nur, wenn der Zielwert Rf in diesem Sample sinkt. Bei einer gate-artigen CV passiert das nur im ersten Sample. Danach öffnet das Vactrol mit der langsamen Konstante.
* *State-based:* Die Richtung ergibt sich aus dem Vergleich des Ziels mit dem aktuellen Vactrol-Zustand. Das verhält sich wie ein echtes LPG.
* *Conductance:* ein Zusatz, der einen längeren Nachlauf liefert (siehe unten und Abschnitt 14).

Gate-Sprung −10 V → +10 V (VCA, Gleichspannung am Eingang):

| | Reference | State-based |
|---|---|---|
| Anstieg 10 → 90 % | **219,7 ms** | **10,5 ms** |
| Schließen 90 → 10 % | 0,1 ms | 0,1 ms |

Das sehr schnelle Schließen beim Sprung von +10 V auf −10 V kommt vom Shelving-Filter im Steuerpfad. Die steile CV-Flanke überschwingt, und das Vactrol wird kurz auf extrem hohen Widerstand getrieben. Bei sanfterem Abfall der CV schließt es langsamer (Tabelle unten).

Kurze Gate-Impulse (CV −10 → +10 V), erreichte Spitzengain relativ zu offen:

| Länge | Reference | State-based |
|---|---|---|
| 5 ms | 0,000 | 0,001 |
| 10 ms | 0,000 | 0,007 |
| 20 ms | 0,000 | 0,32 |
| 50 ms | 0,000 | 1,00 |
| 100 ms | 0,001 | 1,00 |
| 200 ms | 0,006 | – (1,00) |

Mit der Einstellung „Reference“ reagiert das Modul auf Trigger und kurze Gates praktisch nicht. Das Gate müsste weit über 200 ms lang sein. Für Pluck-Sounds sollte man „State-based“ oder „Conductance“ wählen. „Conductance“ liefert den Nachlauf, mit dem der Pluck in Abschnitt 14 gefunden wurde.

Schließzeit nach einem 100-ms-Gate (+10 V), State-based, abhängig von der Ruhe-CV (Zeit bis 50 % und 10 % der Spitzengain):

| Ruhe-CV | 50 % | 10 % | Endwert (rel.) |
|---|---|---|---|
| −10 V | 0,0 ms | 0,1 ms | 0,000 |
| −4 V | 0,0 ms | 0,1 ms | 0,003 |
| −1 V | 0,4 ms | 8,8 ms | 0,020 |
| 1 V | 1,4 ms | 19,1 ms | 0,035 |
| 2 V | 2,0 ms | 25,4 ms | 0,043 |
| 3 V | 2,5 ms | 33,0 ms | 0,051 |
| 3,5 V | 2,8 ms | 37,4 ms | 0,056 |

Der „Decay“ ist also kein freier Parameter. Er hängt davon ab, wohin die CV zurückfällt. Eine tiefe Ruhe-CV (≤ −4 V) schaltet praktisch sofort ab. Eine Ruhe-CV knapp unter der Schwelle (3…3,5 V) erzeugt einen längeren, weicheren Ausklang von 33…37 ms bis −20 dB. Mit THRESHOLD lässt sich die Lage der Ruhe-CV zur Schwelle ändern.

## 9. Kanäle, Eingänge und alte Patches

**Kanäle und Eingänge.**

* Es gibt vier getrennte, monophone Kanäle. Jeder hat einen eigenen Zustand, eigene Regler und eigene Schalter. IN 1 ist auf IN 2–4 normalisiert, solange dort nichts steckt.
* Jeder Kanal hat einen eigenen V/OCT-Eingang. Der Eingang WET CV in der Spalte ALL ist polyphon: Kanal 1–4 des Kabels steuern LPG 1–4.
* Ein Kanal ohne Audioquelle (weder eigener IN noch IN 1) wird nicht gerechnet, sein Ausgang liegt auf 0 V. Ein Kanal ohne OUT-Kabel wird übersprungen, solange ALL nicht belegt ist. Das spart bis zu drei Viertel der Rechenzeit. Beim Reaktivieren startet der Kanal im eingeschwungenen Zustand, das Vactrol schwingt also nicht ein.

**Alte Patches.**
* Alte Patches der polyphonen Version (16 Kanäle) landen auf den Kanälen 1–4. Die Param-Blöcke der alten Version bleiben bestehen, damit die Indizes stimmen. Ihre Kabel an AUDIO und CV liegen auf Kanal 1. Gelesen wird nur Kanal 1 der Polyphonie, Poly-Eingänge werden nicht aufgefächert (Ausnahme: WET CV).
* Patches vor dem Hardware-Signalweg bekommen ATT 0 V, und ihre CV-Kabel werden nicht mehr skaliert (Abschnitt 4).
* Neue Eingänge (THRESHOLD-CV, FEEDBACK-CV, V/OCT, WET CV) hängen am Ende der Eingangsliste. Alte Patches behalten dadurch ihre Kabel.
* Ein Patch mit gestecktem V/OCT aus der Zeit der Zeitskalierung klingt jetzt anders (Abschnitt 6a).

## 10. Kontextmenü

Die Regler liegen auf dem Panel. Das Kontextmenü enthält DEPTH und die Modell-Einstellungen.

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
| Oversampling | **Off** / 2x | 2× Oversampling des Audiopfads. Es verdoppelt die Rechenzeit und kann Aliasing durch die Nichtlinearität verringern (im Hörtest zu prüfen). |

Die Einstellungen werden im Patch gespeichert. Ältere Patches ohne die Schlüssel für Gate response, BOTH filter stage und Velocity target laden mit den Standardwerten (Linear, Lowpass, Control level).

## 11. Modellgrenzen und Auffälligkeiten der Referenz

Beim Nachrechnen der Referenz fielen drei Punkte auf:

1. **Paper-Gleichung (20):** Sie ist im PDF fehlerhaft, der Code der Referenz ist korrekt. Umgesetzt ist der Code.
2. **Vactrol-Richtung:** Siehe Abschnitt 8. Bei Gate-CV öffnet die Referenz mit rund 220 ms. Als Option steht „State-based“ zur Verfügung.
3. **NL-Solver:** In `yxden` steht `+a1·d1·t2m1`, die Herleitung ergibt aber `−a1·d1·t2m1`. Der NL-Zweig geht deshalb bei kleinem Signal nicht in den linearen Zweig über. Gemessen weicht die NL bei 1 µV um 52 % vom linearen Zweig ab („Reference“), mit „Corrected“ sind es 1,4·10⁻¹². „Corrected“ ist als Option verfügbar und seit dem Hörtest Standard im Modul.

Standard ist NL solver = **Corrected** (Entscheidung nach dem Hörtest). Für Vactrol response gilt Reference, diese Entscheidung ist noch offen. Die Engine selbst (`AnnaMariaLpg`) hat weiterhin die Referenz als Voreinstellung und ist so bitgenau gegen Octave geprüft. Das Modul setzt Corrected ausdrücklich.

Weitere Grenzen:

* Die Parameter des Vactrol-Modells sind aus dem Paper übernommen, nicht gemessen.
* Streuung von Bauteilen und Temperatur sind nicht modelliert.
* Bei sehr hohen Frequenzen nahe Nyquist wirkt die Trapez-Integration. Oversampling 2× hilft.

## 11a. Abgleich mit einer Bauteilvariante

Quelle ist die Referenzdokumentation einer Schaltungsvariante des Lowpass Gates. Die Werte wurden daraus gelesen und nicht gegen Messdaten geprüft.

**Abweichungen gegenüber dem Paper:**

* R8 = 10 k statt 4,7 k.
* Im VCA-Modus ist der Querwiderstand R16 = 10 k statt 5 k, und es gibt eine Eingangsstufe mit Verstärkung 2 (R11 = R12 = 15 k).
* Die Vorlage hat außerdem einen DEEP-Schalter. Er lässt das Gate bei gleicher Reglerstellung tiefer schließen, etwa wie ein um einige Volt höherer THRESHOLD. DEEP ist nicht modelliert. Eine genaue Nachbildung bräuchte die LED-Durchlassspannung.

**Menü „Circuit variant = Reproduction board“:** Es setzt R8 = 10 k, den VCA-Querwiderstand auf 10 k und im VCA-Modus die Eingangsverstärkung auf 2. Standard bleibt Paper.

Messwerte (Sinus 1 kHz, THRESHOLD 4,1 V, DEPTH 50 %, State-based, Verstärkung in dB):

| Modus | Variante | CV −4 V | 0 V | 4 V | 8 V |
|---|---|---|---|---|---|
| LOWPASS | Paper | −27,3 | −6,8 | −2,5 | 0,0 |
| LOWPASS | Reproduction | −40,2 | −13,9 | −7,4 | 0,0 |
| BOTH | Paper | −50,7 | −16,8 | −8,9 | 0,0 |
| BOTH | Reproduction | −66,4 | −28,9 | −17,9 | 0,0 |
| VCA | Paper | −57,1 | −35,3 | −28,4 | −4,0 |
| VCA | Reproduction | −58,2 | −31,3 | −24,2 | +3,8 |

Die Schwelle ändert sich nicht. Das geschlossene Gate liegt in LOWPASS und BOTH 6…12 dB tiefer. Im VCA-Modus beträgt die offene Gain +3,8 dB statt −4 dB, hier muss man auf den Pegel achten.

**Pluck ohne neue Regler** (Paper-Variante, BOTH, State-based, Rauschen, Hüllkurve 0…10 V, Abfall 100 ms): DEPTH 100 % und THRESHOLD 7,4 V (Offset am unteren Anschlag) ergeben Ruhe −76 dB und Spitze −4 dB. Der Pegel nach 20 / 50 / 100 / 200 / 400 ms beträgt 0 / −9 / −13 / −21 / −36 dB. Der Standard 4,1 V (Offset „mittel“) lässt das Gate „angelehnt“ (Ruhe −21 dB).

## 12. Rechenaufwand

* **Engine** (Messmaschine, double, -O2, ein Kanal): 70–90 ns pro Sample bei bewegter CV. Bei konstanter CV sind es 20–26 ns, weil Steuerpfad und Audio-Koeffizienten zwischengespeichert werden (bitgleich zum Original).
* **Rack:** Ein aktiver Kanal braucht 0,2–0,4 %. Bei belegtem ALL (vier Kanäle) sind es 1,5 %, das sind ca. 0,35 % je Kanal bei bewegter CV.
* Mit 2× Oversampling verdoppelt sich der Aufwand etwa.
* Die CV-Eingänge für THRESHOLD und FEEDBACK mit Audiorate kosten mehr, weil sich die Koeffizienten in jedem Sample ändern.
* Der Resonator mit V/OCT kostet zusätzlich Rechenzeit. Bei Audiorate-V/OCT ändern sich die Koeffizienten in jedem Sample, und je Sample und Kanal laufen sin, cos und pow. Das ist im Rack noch nicht gemessen.
* Der lineare Gate-Anteil in VCA ist billiger als das Paper-Modell.

## 13. Lizenz und Quellen

Das Plugin steht unter GPL-3.0-or-later. Die Formeln folgen `buchla_lp_gate.m` (Copyright 2014 Stefano D'Angelo, MIT; Hinweis in `AnnaMariaLpg.hpp`) und dem Paper *Parker, D'Angelo: A Digital Model of the Buchla Lowpass-Gate*, DAFx-13.

## 14. Patch-Beispiele (zu prüfen im Hörtest)

1. **Pluck / Marimba** (im Hörtest gefunden):
   * Modul: MODE LOWPASS, THRESHOLD 7,5 V, DEPTH 100 % (Standard), Vactrol response = Conductance, VACTROL ×1.
   * Hüllkurve an die ATT-CV, VEL 0: Delay 700 ms, Attack 5 ms, Hold 0 ms, Release 200 ms, Spitze 10 V (erzeugt mit dem Hüllkurvenmodul Europa).
   * Am IN liegt ein Sägezahn (100 Hz) oder ein Dreieck (2 kHz). Das Dreieck klingt glocken- bzw. marimbaartig.
   * Weitere Einstellungen: DRIVE 1,9, FEEDBACK 63–80 %, FB NL an, Resonance = Normalized, NL solver = Corrected, Circuit variant = Paper, Saturation = On, Oversampling aus.
   * „Gate response“ und „BOTH filter stage“ wirken nur in BOTH und VCA und sind hier ohne Bedeutung.
   * Weil THRESHOLD 7,5 V ist, öffnet nur der obere Teil der Hüllkurve das Gate (ab ca. 7,4 V Steuerpegel). Der Ausklang kommt vom Nachlauf des Vactrol-Modells (Abschnitt 8).
   * Für weicheren Ausklang VACTROL nach rechts drehen, für härteren nach links.
   * Eine Variante mit BOTH steht in Abschnitt 4c (Modellwerte).
   * Auf einer Note: Einen V/OCT-Eingang stecken, am IN einen kurzen Impuls oder Rauschburst geben und FEEDBACK hoch stellen (Abschnitt 6a). Die Hüllkurve an der ATT-CV bleibt.
2. **Linearer VCA:** MODE VCA, Hüllkurve oder Regler an ATT. Die Lautstärke folgt exakt `cv / 10`. Velocity verschiebt den Bereich (Abschnitt 3).
3. **Filter-Drone:** MODE LOWPASS, THRESHOLD knapp über 0 V (z. B. 0,1 V), sodass das Gate ohne Hüllkurve bei der gewünschten Grenzfrequenz steht. Ein LFO mit kleiner Amplitude geht an die ATT-CV, FEEDBACK 75–90 %.
4. **Resonanz-Verzerrung:** MODE LOWPASS, FB NL an, FEEDBACK 90 %, DRIVE 6–8, Gate halb offen (Steuerpegel an der Schwelle).
5. **Gleichanteil-Verschiebung:** Velocity target = Audio offset, ATT 10 V, Signal ±5 V am IN, Velocity von 0 auf −4 V. Das Signal rutscht nach unten (+1 / −9 V). Bei weiterer Absenkung wird unten bei −10 V geschnitten (Abschnitt 3a).

## 15. Entscheidungen (Protokoll)

Entscheidungen Kinovice, in chronologischer Reihenfolge:

* **Dokumentation (2026-09-30).** Die Dokumentation liegt öffentlich im Repo. Originalnamen, Modellnummern, Links und wörtliche Zitate aus Hersteller-Handbüchern werden vermieden. Quellen heißen nur „Referenzdokumentation“.
* **Quad-Layout (2026-10-14).** Vier monophone Kanäle statt 16 polyphoner, 31 HP, Spalte ALL (ALL OUT, MODE global, MODE CV, DRIVE, VACTROL, DEEP; Stand dieses Eintrags, die Spalte hat sich am 2026-10-24 geändert). Reihenfolge je Kanal: IN | OUT, ATT | VEL, CV | CV, THRESH | FEEDBACK, CV | CV, FB NL, MODE | DEEP. ALL summiert zuerst und begrenzt dann weich auf ±10 V. Es wird nicht durch 4 geteilt, weil das Signal sonst zu leise wäre. IN 1 wird auf IN 2–4 durchgereicht.
* **Signalweg nach Hardware-Vorbild (2026-10-14).** ATT ist der Steuerpegel 0…10 V, Velocity ein Gleichanteil darauf. Die Summe wird auf 0…10 V begrenzt, und eine gesteckte CV ersetzt jeweils den Regler (ATT und Velocity). THRESHOLD-CV: 1 V/V über 0…7,5 V. FEEDBACK-CV: 0…10 V = 0…100 %.
* **Linearer VCA (2026-10-15).** Nach der Referenzdokumentation soll VCA ein linearer VCA sein, die Paper-Schaltung ist dort unwirksam. BOTH bekommt ebenfalls den linearen Anteil.
* **Reihenfolge in BOTH (2026-10-15).** Erst VCA, dann LPG, beide in Reihe. Das entspricht der Aussage in einem Video zur Hardware.
* **Filterstufe von BOTH (2026-10-15).** Die Stufe ist per Menü wählbar („flexibel bleiben“). Standard ist Lowpass („für mich sinnvoller“).
* **Velocity-Ziel (2026-10-15).** Das Menü „Velocity target“ bietet „Control level“ (Standard) und „Audio offset“. Für „Audio offset“ gilt:
  * Velocity ist ein Gleichanteil nach dem Gate/ATT.
  * Velocity 0 hat keine Wirkung.
  * Geschnitten wird in Verschieberichtung bei ±10 V.
  * Der Offset wirkt nur bei offenem Gate, dann mit vollem Wert.
  * Verworfen wurden: Offset vor dem Gate, feste Untergrenze 0 V oder −5 V, Kappung bei +5 V oder +10 V am Eingang.
* **V/OCT-Eingang (2026-10-20).** Jeder Kanal bekommt einen eigenen Eingang zum Patchen der Tonhöhe, ohne Menüpunkt. Die erste Fassung stimmte das ganze Filter per Zeitskalierung. Ein Pegelausgleich bei tiefen Noten wurde als Option eingebaut, blieb aber standardmäßig aus.
* **Resonator statt Zeitskalierung (2026-10-24).** Kinovice hörte mit der Zeitskalierung einen deutlich schlechteren Klang als ohne V/OCT. Die Hörprobe „Resonator vor dem Gate“ war der Favorit („klingt anders, aber rund“). Die Zeitskalierung und der Pegelausgleich sind entfallen.
* **WET (2026-10-24).** Der Pegel des Resonators ist ein globaler Regler. Ein polyphoner CV-Eingang ersetzt im gesteckten Zustand den Regler. Der Wertebereich ist auf die Rack-Messung von Kinovice zugeschnitten (guter Klang bei 10 V und −45 dB).
* **Panel (2026-10-24).** Der globale DEEP-Schalter ist vom Panel entfallen, um Platz für WET und WET CV zu schaffen. Die DEEP-Schalter der Kanäle bleiben als Platzhalter.
