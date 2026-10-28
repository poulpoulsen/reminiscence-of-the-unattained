# Elena – Technische Dokumentation

Siebtes Modul des Plugins **Kinovice reminiscence of the unattained**:
VCV-Rack-Modul, stark inspiriert von der wunderbaren Buchla-Modulwelt. Zwei Transformationssektionen (A/B, nach Art eines digitalen Kennlinien-Prozessors)
und sechs Slew-Limiter (C–H) mit Pulse-Ausgang. Die A/B-Sektionen sind
polyphon mit individuellen Werten je Kanal, die Slew-Sektionen verarbeiten
polyphone Signale mit gemeinsamen Reglern je Paar.

**Hinweis:** Das Plugin *Kinovice reminiscence of the unattained* ist ein eigenständiges Werk, stark inspiriert von der wunderbaren Buchla-Modulwelt.
Es steht in keiner Verbindung zu Buchla, Tiptop Audio oder einem anderen Hersteller und wird von diesen weder autorisiert noch unterstützt. Erwähnte Produkt- und Firmennamen 
sind Marken ihrer jeweiligen Inhaber und dienen nur dazu zu beschreiben, welche funktionalen Konzepte ein Modul inspiriert haben.

**Abgrenzung:** **Minerva** (analoge Spannungsrechnung mit Attenuverter, Crossfader und Offset) und Elena (digitale Kennlinien-Transformation mit Slew-Limiting) sind zwei eigenständige Module mit unterschiedlicher Funktion. Dass das eine als Weiterentwicklung des anderen gedacht ist, steht in keiner verwendeten Quelle; die Zuordnung ergibt sich nur aus den Beschreibungen selbst. Siehe `Minerva-Dokumentation_DE.md`.

Referenzen: Referenzdokumentation. Sie beschreibt die Sektionen A/B nur knapp als ähnlich zu einer älteren Sektion; die Formeln stammen deshalb aus der Beschreibung dieser älteren Sektion.

Teststand (Kinovice, live im laufenden Rack): Die Sektionen A/B
funktionieren (Kanalwahl mit ACTIVATE, Kanalwerte bleiben nach Speichern und
Laden erhalten, polyphone A1/A2 mit monophonem SEL CV, Kennlinien-Anzeige
und Breakpoint stimmen mit den nachgerechneten Werten überein). Die
Slew-Sektionen C–H funktionieren (jeder Slewer wirkt ausschließlich auf das
Signal an seinem eigenen IN) und wurden nachgemessen (Rechteck 1 Hz, POS und
NEG je 300 ms): Im Time-Modus ergibt ein ±5-V-Rechteck ein Trapez, ein
±10-V-Rechteck ein Dreieck von −6,5 bis +9,98 V (konstante Steigung
10 V/300 ms, der Hub ist durch die halbe Periode begrenzt). Im Rate-Modus
ergibt ein ±10-V-Rechteck ±9,86 V (Theorie 9,87 V) und einen Nulldurchgang
nach etwa 0,07 s (Theorie 0,069 s), bei Anstieg und Abfall gleich. Die
Zuordnung von Rate und Time bleibt eine Interpretation der Referenzdokumentation, siehe
Abschnitt 8. Die in Abschnitt 7 als „rechnerisch" gekennzeichneten
Patch-Beispiele sind nicht im Rack getestet. Die Bereichswahl je Sektion (Abschnitt 2) und der DC-Blocker sind neu, rechnerisch geprüft und noch nicht im Rack getestet.

---

<a id="toc"></a>

## Inhaltsverzeichnis

1. [Überblick](#sec1) · [[#1. Überblick|Obsidian]]
2. [Sektionen A/B: Transformation](#sec2) · [[#2. Sektionen A/B: Transformation|Obsidian]]
3. [Sektionen C–H: Slew und Auto-Pulse](#sec3) · [[#3. Sektionen C–H: Slew und Auto-Pulse|Obsidian]]
4. [Polyphonie und Kanalwahl](#sec4) · [[#4. Polyphonie und Kanalwahl|Obsidian]]
5. [Panel-Layout](#sec5) · [[#5. Panel-Layout|Obsidian]]
6. [Interne Verarbeitung (C++)](#sec6) · [[#6. Interne Verarbeitung (C++)|Obsidian]]
7. [Patch-Beispiele](#sec7) · [[#7. Patch-Beispiele|Obsidian]]
8. [Quellenlage, Annahmen und offene Punkte](#sec8) · [[#8. Quellenlage, Annahmen und offene Punkte|Obsidian]]
9. [Design-Entscheidungen](#sec9) · [[#9. Design-Entscheidungen|Obsidian]]

---

<a id="sec1"></a>

## 1. Überblick
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Modul** *(`Elena.cpp`, `CvProcElena.hpp`, `modelElena`)*<br>
42HP. Links zwei gleiche Transformationssektionen A und B, rechts drei Slew-Blöcke mit je zwei Slewern (C/D, E/F, G/H).

**Sektionen A/B**<br>
Wahl zwischen zwei Eingängen (A1/A2) per Select-Regler oder Select-CV, danach eine Kennlinie aus 0-V-Punkt, 10-V-Punkt und optionalem Breakpoint. Alle Regler sind pro Kanal getrennt gespeichert.

**Sektionen C–H**<br>
Sechs unabhängige Slew-Limiter. Jeder hat einen eigenen CV-Eingang, Slew-Ausgang und Pulse-Ausgang. Die Slew-Parameter (Positive/Negative Slew, Time/Rate, Auto-Pulse) sind paarweise geteilt und gelten für alle Kanäle gleich.

**Wichtig:** Ein Slewer verändert nur das Signal an seinem eigenen IN. Ohne Eingangssignal liegt der Ausgang auf 0 V, und die Regler bleiben wirkungslos.

---

<a id="sec2"></a>

## 2. Sektionen A/B: Transformation
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Alle Spannungen sind auf 1 normiert (10 V = 1). Pro Sektion und Kanal:

```
Vin  = InSel·A2 + (1 − InSel)·A1                    (auf 0…1 begrenzt)
ohne Breakpoint:  Vout = Vin·V10 + (1 − Vin)·V0
mit Breakpoint:   Vin < bpx:  Vout = Vin·bpy/bpx + (1 − Vin/bpx)·V0
                  Vin > bpx:  Vout = ((Vin − bpx)·V10 + (1 − Vin)·bpy) / (1 − bpx)
```

**Eingangswahl (SELECT, A1, A2, SEL CV)**<br>
Der Select-Regler überblendet zwischen A1 (0) und A2 (1). Ist SEL CV gepatcht, **ersetzt** sie den Regler (0–10 V = 0…1). Bei unbelegtem A1 (0 V) multipliziert die Eingangsstufe einfach InSel mit A2, wie in der Referenzdokumentation beschrieben; dadurch ist die Sektion auch ein Multiplizierer.

**Kennlinie (0 V, 10 V)**<br>
„0 V" legt fest, welche Ausgangsspannung bei 0 V Eingang anliegt, „10 V" die Ausgangsspannung bei 10 V Eingang; dazwischen wird linear interpoliert. Liegt „10 V" unter „0 V", invertiert die Sektion (innerhalb 0–10 V). Standardstellung: 0 V → 0 V, 10 V → 10 V, also Durchlass.

**Breakpoint (BP, BP IN, BP OUT)**<br>
Der BP-Schalter aktiviert einen Knickpunkt: BP IN legt die Eingangsspannung fest, BP OUT die Ausgangsspannung dort. Die Kennlinie besteht dann aus zwei Geraden durch (0, V0), (bpx, bpy) und (1, V10). Ohne BP sind die beiden Regler unwirksam. Der Wert von BP IN wird intern auf 0,001…0,999 begrenzt, um Divisionen durch 0 zu vermeiden.

**Kennlinien-Anzeige**<br>
Eine 34×20-mm-Grafik ersetzt die LED-Anzeige des Originals: sie zeigt die Kennlinie des angezeigten Kanals (grün, Breakpoint als Punkt) und den aktuellen Arbeitspunkt Eingang → Ausgang (amber).

**Bereich**<br>
Standardmäßig arbeiten die Sektionen unipolar im Bereich 0–10 V; bipolare Eingangssignale werden bei Vin abgeschnitten (negative Anteile werden 0). Im Modul-Kontextmenü („Input range Section A/B") lässt sich je Sektion ein anderer Bereich wählen:

**Eingangsbereich (Kontextmenü, je Sektion)**<br>
**0 to 10 V** (Standard): wie oben, Vin hart begrenzt. **+/-5 V** und **+/-10 V**: Der Bereich wird auf 0…1 abgebildet (`(v + 5)/10` bzw. `(v + 10)/20`) und gilt für A1, A2, SEL CV **und** für den Ausgang (Bereich → Bereich): bei +/-5 V gehen ±5 V rein und ±5 V raus. Die Kennlinie läuft unverändert; die Regler „0 V", „10 V", BP IN und BP OUT behalten ihre Positionen (0…1), ihre Tooltips zeigen die Spannung im gewählten Bereich (z. B. −5…+5 V). Mit den Standardwerten ist die Sektion in jedem Bereich ein Durchgang. **Unlimited (0-10 V scale)**: Skala 0–10 V, aber ohne Begrenzung von Vin; die Kennlinie wird linear über die Endpunkte hinaus fortgesetzt. Mit den Standardwerten geht ein bipolares Signal damit unverändert durch.

**Hinweise zum Bereich**<br>
SEL CV folgt dem Bereich, bleibt aber immer auf 0…1 (Überblendgewicht) begrenzt. Unbelegte Eingänge liegen auf 0 V; in den bipolaren Bereichen entspricht 0 V der Bereichsmitte (0,5 normiert). Ein unbelegtes A1 wirkt dort also nicht als 0 in der Rechnung `InSel·A2`, sondern als Mittelwert. Die Bereichswahl ändert keine Reglerwerte, nur deren Bedeutung in Volt.

**DC-Blocker (Kontextmenü, je Sektion)**<br>
Im Modul-Kontextmenü lässt sich für den Ausgang jeder A/B-Sektion einzeln ein DC-Blocker zuschalten („DC blocker Section A/B"). Er wirkt auf den fertig berechneten Ausgang (nach Kennlinie und Bereichsrückrechnung), pro Kanal mit eigenem Zustand. Er ist ein einpoliger Hochpass bei ca. 10 Hz (Details in Abschnitt 6). Typische Quelle eines Gleichanteils ist die Einweggleichrichtung bei bipolarem Audio im Bereich 0–10 V (die negative Hälfte wird 0), oder ein „0 V"-Regler bzw. Breakpoint, der das Signal verschiebt. Da der Blocker auch langsame CV-Anteile unter ca. 10 Hz entfernt, ist er standardmäßig aus.

**Regler-Bereiche und Voreinstellungen**<br>
SELECT 0…100 % (Standard 0 = A1), 0 V und 10 V je 0…10 V (Standard 0 V und 10 V), BP IN und BP OUT je 0…10 V (Standard 5 V), BP aus.

---

<a id="sec3"></a>

## 3. Sektionen C–H: Slew und Auto-Pulse
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Aufbau**<br>
Sechs Slewer C bis H, je `C IN` … `H IN`, `C OUT` … `H OUT`, `C PLS` … `H PLS`. Die Slew-Regler sind je Paar geteilt: **C/D**, **E/F**, **G/H**. Zwei LEDs je Slewer: rot (Puls) und blau (Ausgangspegel).

**POS und NEG**<br>
POS bestimmt die Geschwindigkeit steigender, NEG die fallender Flanken. Bereich 0,01…10 s, exponentiell (`t = 0,01 s · 1000^Position`), Anzeige in ms bzw. s, Eingabe per Rechtsklick in ms oder s. Die Zeit erreicht nie 0, daher entsteht wie im Original nie ein echtes Rechteck oder ein echter Sägezahn.

**Slew-CV (POS CV, NEG CV)**<br>
Addiert sich zur Reglerposition (0–10 V entsprechen dem vollen Regelweg) und wird oben begrenzt: steht der Regler ganz rechts, hat die CV keine Wirkung; am meisten Wirkung hat sie bei Regler ganz links. Die CV ist polyphon (pro Kanal).

**TIME/RATE**<br>
Die Modi verhalten sich unterschiedlich (Zuordnung siehe Abschnitt 8):

**Time**<br>
Lineare Steigung mit konstanter Geschwindigkeit `10 V / t`. Der Regler gibt die Sekunden für einen Hub von 0 auf 10 V an. Ein kleinerer Sprung braucht entsprechend weniger Zeit.

**Rate**<br>
Exponentielles Nachlaufen, die Geschwindigkeit ist proportional zum Abstand zwischen Eingang und Ausgang. Der Regler gibt die Zeit bis etwa 95 % eines Sprungs an (Zeitkonstante `t/3`).

**AUTO (Auto-Pulse)**<br>
Pro Paar ein Taster mit Textanzeige (rot, wenn aktiv), statt des Originalbedienkonzepts „2 s gedrückt halten". Ist er aktiv, löst jede **Richtungsumkehr des Ausgangs** einen 1-ms-Puls (10 V) am PLS-Ausgang aus und lässt die rote LED aufleuchten (mit kurzem Nachleuchten). Zur Erkennung wird das letzte Extremum gemerkt; erst wenn der Ausgang um mehr als 10 mV zurückweicht, gilt die Richtung als umgekehrt. Bei ausgeschaltetem AUTO bleibt der Pulse-Ausgang stumm. Sehr kleine Signalhübe (unter etwa 10 mV) lösen deshalb keinen Puls aus.

**LEDs**<br>
Blau: Betrag des Slew-Ausgangs (Maximum über alle Kanäle, 10 V = volle Helligkeit). Rot: Puls.

**DC-Blocker (Kontextmenü, je Paar)**<br>
Für die Slew-Ausgänge lässt sich je Paar ein DC-Blocker zuschalten („DC blocker Sections C/D", „E/F", „G/H"), getrennt von den A/B-Blockern. Er wirkt nur auf den Ausgang `x OUT`: der interne Slew-Zustand, die Auto-Pulse-Erkennung und die LEDs arbeiten mit dem ungefilterten Signal. Bei langsamen Slew-Signalen entfernt er diese weitgehend; er ist für Audio gedacht und standardmäßig aus.

---

<a id="sec4"></a>

## 4. Polyphonie und Kanalwahl
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**A/B: Werte pro Kanal**<br>
Jeder der 16 Kanäle hat eigene Werte für SELECT, 0 V, 10 V, BP IN, BP OUT und BP (192 Params). Ein gemeinsamer Kanalwähler (Knob mit Zentrumsziffer 1–16 und ACTIVATE) bestimmt, welchen Kanal die Regler und die Kennlinien-Anzeige zeigen. Wie bei Minerva sind `pendingCh` und `displayedCh` **keine Params** (Pickup/Catch, History und Preset-Recall sollen die GUI-Navigation nicht beeinflussen) und werden per JSON gespeichert.

**A/B: Kanalzahl**<br>
Standard „Auto": Maximum der Kanalzahlen von A1, A2 und SEL CV (mindestens 1). Im Modul-Kontextmenü („Channels Section A/B") lässt sich pro Sektion eine feste Zahl 1–16 wählen. Mono-Eingänge werden auf alle Kanäle verteilt; unbelegte Eingänge liegen auf 0 V.

**Rechtsklick „Apply to all 16 channels"**<br>
An allen A/B-Reglern und am BP-Schalter: übernimmt den Wert für alle Kanäle derselben Sektion und Art, als ein Undo-Schritt.

**C–H: polyphone Signale, globale Regler**<br>
Die Slewer verarbeiten die Kanäle des jeweiligen IN einzeln (Ausgangs- und Puls-Kanalzahl = Kanalzahl des IN, mindestens 1), jeder Kanal hat einen eigenen Slew-Zustand. POS, NEG, TIME/RATE und AUTO gelten dagegen einmal je Paar für alle Kanäle. Nur die Slew-CV wirkt pro Kanal.

---

<a id="sec5"></a>

## 5. Panel-Layout
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Kopfzeile**<br>
Über A/B: Kanalwähler (CHANNEL) und ACTIVATE, Beschriftung grau. Über den Slew-Sektionen: Titel „ELENA".

**Sektionen A (blau) und B (orange)**<br>
Rahmen je 54 mm breit, 96 mm hoch. Von oben nach unten: Kennlinien-Anzeige; Reglerreihe „0 V" und „10 V"; Reglerreihe „BP IN", BP-Schalter, „BP OUT"; Reglerreihe „SELECT" mit „SEL CV"; Buchsenreihe A1, A2, OUT.

**Sektionen C–H (türkis)**<br>
Ein Rahmen mit drei Blöcken (je 32 mm). Links im Block: POS und NEG (Regler, darunter POS CV und NEG CV), rechts davon die Schalter TIME/RATE (oben) und AUTO (unten). Rechts zwei Zeilen mit je IN, OUT, PLS (beschriftet „C IN" usw.) und zwei LEDs (rot oben, blau unten) neben der Pulse-Buchse.

---

<a id="sec6"></a>

## 6. Interne Verarbeitung (C++)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**`CvProcElena.hpp`**<br>
Freie Funktionen und ein kleines Zustands-Struct, ohne Rack-Abhängigkeit außer `simd` und `clamp`: `transfer(vin, v0, v10, bpOn, bpx, bpy)` (vier Kanäle als `float_4`, beide Segmente werden berechnet und per `ifelse` gewählt), `knobToSeconds`/`secondsToKnob`, `slewStep(in, out, tPos, tNeg, rateMode, dt)` (ein Kanal skalar) und `ReversalDetector` (Richtungsumkehr mit Hysterese).

**`Elena.cpp`**<br>
`struct Elena : Module`. `processSection(s)` für A/B (Kanalzahl, 4er-Blöcke, `getPolyVoltageSimd`, `transfer`, Speichern des Arbeitspunkts für die Anzeige), `processSlewers(dt)` für C–H (Slew, Auto-Pulse, LEDs).

**Param-Layout (204 Params)**<br>
A/B: `abParam(section, type, ch) = (section·6 + type)·16 + ch` mit den Typen SEL, V0, V10, BPX, BPY, BP (Indizes 0–191). Slew-Paare danach: `pairParam(pair, which) = 192 + pair·4 + which` mit POS, NEG, MODE, AUTO (Indizes 192–203). Neue Params werden nur am Ende angehängt.

**Eingänge/Ausgänge/Lichter**<br>
Eingänge: A1, A2, SEL je Sektion (B = A + 3), dann `SLEW_IN_C…H`, dann je Paar POS-CV und NEG-CV. Ausgänge: `OUT_A/B`, `SLEW_OUT_C…H`, `PULSE_C…H`. Lichter: `PULSE_LED_C…H` (rot), `CVOUT_LED_C…H` (blau).

**Persistenz (JSON)**<br>
`pendingCh`, `displayedCh`, `channelsA`, `channelsB`, `rangeA`, `rangeB` (0 = 0–10 V, 1 = ±5 V, 2 = ±10 V, 3 = Unlimited; fehlt der Schlüssel in älteren Patches, gilt der Standard 0), jeweils mit Clamp, und `dcBlock` (Array mit fünf Einträgen 0/1 für A, B, C/D, E/F, G/H; fehlt der Schlüssel, ist der Blocker aus). Der Filterzustand wird nicht gespeichert und bei `onReset()` gelöscht.

**DC-Blocker (Engine)**<br>
`CvProcElena::dcBlockStep`: `y[n] = x[n] − x[n−1] + R·y[n−1]` mit `R = exp(−2π·10 Hz / fs)`, wird bei Änderung der Abtastrate neu berechnet. Das ergibt −3 dB bei 10 Hz, etwa −7 dB bei 5 Hz, −20 dB bei 1 Hz und unter 1 dB Dämpfung ab 20 Hz; ein Gleichspannungssprung klingt mit τ ≈ 16 ms ab. Ausgeschaltet wird der Filterzustand mitgeführt (x[n−1] = Eingang, y[n−1] = 0), damit beim Zuschalten kein Altzustand wirkt; beim Umschalten entsteht ein Knacken in Höhe des Gleichanteils. Regler laufen über die normalen Rack-Params. Der Slew-Zustand wird nicht gespeichert und bei `onReset()` zurückgesetzt.

**Rechenaufwand**<br>
Die Slewzeit (`std::pow`) wird je Paar, Richtung und Kanal zwischengespeichert und nur bei geänderter Reglerposition (inkl. CV) neu berechnet.

**Widgets**<br>
`ElenaKnob` (mit „Apply to all 16 channels"), `ElenaToggle` (Textschalter auf Basis von `app::Switch`, für BP, TIME/RATE und AUTO), `ElenaTransferDisplay`, Kanalwähler und ACTIVATE. Alle lokalen Structs stehen im anonymen Namespace (ODR, siehe `Minerva-Dokumentation_DE.md`); nur `modelElena` ist exportiert.

---

<a id="sec7"></a>

## 7. Patch-Beispiele
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Rechteck → Sägezahn/Sinus-ähnliche Form (Slew)**<br>
Rechteck 0–10 V (unipolar, z. B. 1 Hz) in `C IN`, Scope an `C OUT`. POS und NEG gleich hoch: gerundete/trapezförmige Form; NEG ganz links und POS aufgedreht: sägezahnartig. Getestet (Slew funktioniert); die Form hängt vom Modus ab.

**Dreieck aus einer Rampe**<br>
Rampe 0–10 V in A1; 0 V = 0, 10 V = 0, BP an, BP IN = 5 V, BP OUT = 10 V: Ausgang = Dreieck 0→10→0 V.

**Multiplikation**<br>
A1 unbelegt, Signal in A2, zweites Signal (0–10 V) in SEL CV: Ausgang = `A2 · SEL_CV/10 V` (bei Standard-Kennlinie).

**Glocke `u²(1−u)²` (rechnerisch mit den Formeln aus `Elena.cpp` nachgerechnet, nicht im Rack getestet)**<br>
Braucht Minerva und beide Elena-Sektionen. 1) Minerva Sektion A: Rampe 0–10 V in Va, K = −1, Oset = 10 V → invertierte Rampe. 2) Elena Sektion A: Rampe in A2, invertierte Rampe in SEL CV, A1 unbelegt; 0 V = 0, 10 V = 10, BP an mit BP IN = 2,5 V, BP OUT = 10 V → Parabel `4u(1−u)` mit 10 V Spitze. 3) Elena Sektion B: Parabel in A2 **und** in SEL CV, A1 unbelegt, Standard-Kennlinie → Ausgang = Parabel². Der Ausgang ist symmetrisch mit flachen Ausläufern und 10 V Spitze; die Breite ergibt sich nur aus der Dauer der Rampe. Eingang muss unipolar 0–10 V sein.

**Was keine Glocke ergibt (Kinovice-Test)**<br>
Ein Dreieck (Sektion A mit Breakpoint) durch zwei kaskadierte Slews im Rate-Modus ergibt ein abgerundetes Dreieck, keine Glocke, bei periodischer Rampe sieht das Ergebnis wie ein Sinus aus. Grund: Tiefpassfilterung rundet nur die Ecken ab, erzeugt aber keine flachen Ausläufer.

---

<a id="sec8"></a>

## 8. Quellenlage, Annahmen und offene Punkte
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**A/B als Kennlinien-Sektionen**<br>
Die Referenzdokumentation beschreibt A und B nur knapp als Sektionen mit ähnlicher Funktion. Die Formeln stammen aus der Beschreibung der Transformationssektion und wurden für beide Endpunkte und den Breakpoint nachgerechnet.

**Rate/Time (abgeleitete Interpretation)**<br>
Die Referenzdokumentation beschreibt die beiden Modi nur vage: Rate gibt bei schnellerem Eingang tendenziell einen größeren Ausgangsbereich, Time basiert auf der Zeit, die eine Spannung für den Weg von 0 auf 10 V braucht. Die Umsetzung (Time = lineare Steigung, Rate = exponentielles Nachlaufen) ist eine Interpretation. Bei Time deckt sich die Umsetzung mit der Beschreibung einer linearen Laufzeit für den vollen Bereich (Kinovice). Beim Rate-Modus sind Alternativen denkbar. Das implementierte Verhalten wurde nachgemessen (siehe Teststand); ob die Zuordnung dem Original entspricht, bleibt offen.

**Widersprüchliche Suchmaschinen-Texte (abgelehnt)**<br>
Zwei im Internet gefundene Definitionen beschreiben Time als konstante Übergangszeit unabhängig vom Sprung und Rate als vorgegebene Änderungsgeschwindigkeit. Das würde die Bedeutung von Time und Rate gegenüber der Implementierung **vertauschen**. Kinovice hat entschieden, die Modi **nicht** zu vertauschen („Suchmaschinen können irren"). Zusätzlich passt die Time-Beschreibung der Referenzdokumentation (Zeit für 0–10 V) zur bestehenden Umsetzung, die Herkunft der Texte ist nicht belegt. Ein Tausch wäre eine einzeilige Änderung in `slewStep`.

**Select-CV ersetzt den Regler**<br>
Die Referenzdokumentation sagt, dass eine gepatchte CV den Regler ersetzt, und nennt zugleich den Regler auf 0; Umsetzung: gepatchte CV ersetzt den Regler vollständig.

**Begrenzung von Vin**<br>
Im Standardbereich wird Vin auf 0…1 (0–10 V) begrenzt. Die Referenzdokumentation nennt das nicht ausdrücklich; die Kennlinie des Originals ist auf diesen Bereich definiert. Die Bereichswahl (±5 V, ±10 V, Unlimited) ist eine Erweiterung gegenüber dem Original und nicht in der Referenzdokumentation beschrieben.

**Auto-Pulse**<br>
Die Referenzdokumentation erwähnt Unzuverlässigkeit bei langen Slews, Signalen über 35 Hz und kleinem Eingangsbereich. Die Umsetzung erkennt die Umkehr am Ausgang mit einer Hysterese von 10 mV; Pulsdauer 1 ms und 10 V sind angenommen. Ob der Pulse-Ausgang ohne Auto-Pulse etwas ausgibt, beschreibt die Referenzdokumentation nicht; hier bleibt er stumm.

**Slew-CV**<br>
Skalierung 10 V = voller Regelweg (angenommen; die Referenzdokumentation nennt nur, dass die CV zur Reglerposition addiert wird).

---

<a id="sec9"></a>

## 9. Design-Entscheidungen
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

- **Name Elena (Kinovice):** nach einer der Anguissola-Schwestern (zur Wahl standen Elena und Anna Maria).
- **Kompletter CV-Prozessor als eigenes Modul (Kinovice):** A/B-Sektionen mit Kennlinie plus Slew-Sektionen C–H, neben und unabhängig von Minerva. Minerva bleibt unangetastet.
- **A/B pro Kanal einstellbar wie Minerva, C–H global (Kinovice-Entscheidung):** bei Slew brächten 16 Einzelregler wenig und würden die Zahl der Params vervielfachen; die Signale bleiben polyphon.
- **Auto-Pulse per eigenem Taster statt „2 s halten" (Kinovice-Entscheidung):** der Halte-Bedienweg ist in Rack unpraktisch und der Zustand nicht sichtbar. Weicht bewusst vom Original ab.
- **Kennlinien-Grafik statt LED-Kette:** zeigt Kennlinie und Arbeitspunkt des angezeigten Kanals.
- **Rate/Time nicht vertauschen (Kinovice):** Suchmaschinen können irren; die Interpretation nach der Referenzdokumentation bleibt, siehe Abschnitt 8.
- **Kanalwähler ohne Param:** wie bei Minerva, Lucia und Europa.
- **Rechtsklick „Apply to all 16 channels"** und Kanalzahl „Auto/1–16" je A/B-Sektion (Kontextmenü) wie bei Minerva.
- **Wählbarer Eingangsbereich je Sektion (Kinovice-Entscheidung):** 0–10 V (Standard), ±5 V, ±10 V und Unlimited. Der Bereich gilt für die Eingänge **und** den Ausgang der Sektion (±5 V rein, ±5 V raus); die Wahl gilt je Sektion, nicht gemeinsam für A und B. Erweiterung für den Einsatz mit bipolaren Audio- und Modulationssignalen, nicht Teil des Originals.
- **Zuschaltbarer DC-Blocker (Kinovice-Entscheidung):** wirkt auf die A/B-Ausgänge und die Slew-Ausgänge, getrennt schaltbar je A/B-Sektion und je Slew-Paar (fünf Schalter), im Kontextmenü, mit fester Grenzfrequenz von ca. 10 Hz. Standardmäßig aus, da er langsame CV-Anteile mit entfernt. Anlass: Gleichanteil bei bipolarem Audio, der im Analyzer sichtbar wurde.
- **Layout 42HP** mit A/B links und den drei Slew-Blöcken rechts.
