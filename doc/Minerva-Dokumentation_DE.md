# Minerva – Technische Dokumentation

Sechstes Modul des Plugins **Kinovice reminiscence of the unattained**:
ein VCV-Rack-Modul, stark inspiriert von der wunderbaren Buchla-Modulwelt –
ein Dual Voltage Processor (14HP). Zwei identische Sektionen verrechnen je
vier Spannungen zu einer Ausgangsspannung. Minerva ist polyphon; jeder der
16 Kanäle hat eigene Reglerwerte. Diese Datei beschreibt jedes Bedienelement,
die Rechnung, die Polyphonie-Mechanik, die C++-Struktur und die Annahmen, die
die Referenzdokumentation offenlässt.

**Abgrenzung:** Minerva deckt analoge Spannungsrechnung ab (Attenuverter,
Crossfader, Offset). Kennlinien-Transformation und Slew-Limiting, wie man sie
von einem digitalen Control-Voltage-Processor kennt, sind eine andere Funktion
und im Plugin als eigenes Modul **Elena** umgesetzt, siehe
`Elena-Dokumentation_DE.md`.

Quellen: öffentlich verfügbare Benutzerdokumentation der Hardware, die dieses
Modul inspiriert hat.

**Hinweis:** Das Plugin *Kinovice reminiscence of the unattained* ist ein eigenständiges Werk, stark inspiriert von der wunderbaren Buchla-Modulwelt. Es steht in keiner Verbindung zu Buchla, Tiptop Audio oder einem anderen Hersteller und wird von diesen weder autorisiert noch unterstützt. Erwähnte Produkt- und Firmennamen sind Marken ihrer jeweiligen Inhaber und dienen nur dazu zu beschreiben, welche funktionalen Konzepte ein Modul inspiriert haben.

Teststand (Kinovice, live im laufenden Rack): Grundfunktion beider
Sektionen, polyphoner Betrieb, individuelle Reglerwerte je Kanal,
Kanalwähler und der +5-Vb-Schalter (einzeln je Kanal schaltbar) funktionieren.
Ebenfalls im Rack geprüft: der M-Regler im Bereich −1…+2 an Vb und Vc, die
Ringmodulation im Standardmodus (unbegrenzter M-Bereich), „Begrenzt 0…1",
Ausgangsbegrenzung Weich und Hart sowie das Laden eines älteren Patches ohne
die neuen JSON-Schlüssel. Der DC-Blocker (Abschnitt 2) ist neu, rechnerisch geprüft und noch nicht
im Rack getestet. Nicht im Rack geprüft ist außerdem das in Abschnitt 6 als
„rechnerisch" gekennzeichnete Parabel-Beispiel.

---

<a id="toc"></a>

## Inhaltsverzeichnis

1. [Überblick](#sec1) · [[#1. Überblick|Obsidian]]
2. [Rechnung und Signalfluss](#sec2) · [[#2. Rechnung und Signalfluss|Obsidian]]
3. [Bedienelemente und Panel](#sec3) · [[#3. Bedienelemente und Panel|Obsidian]]
4. [Polyphonie und Kanalwahl](#sec4) · [[#4. Polyphonie und Kanalwahl|Obsidian]]
5. [Interne Verarbeitung (C++)](#sec5) · [[#5. Interne Verarbeitung (C++)|Obsidian]]
6. [Patch-Beispiele](#sec6) · [[#6. Patch-Beispiele|Obsidian]]
7. [Annahmen und offene Punkte](#sec7) · [[#7. Annahmen und offene Punkte|Obsidian]]
8. [Design-Entscheidungen](#sec8) · [[#8. Design-Entscheidungen|Obsidian]]

---

<a id="sec1"></a>

## 1. Überblick
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Modul** *(`Minerva.cpp`, `VoltageProc.hpp`, `modelMinerva`)*<br>
14HP, zwei identische Sektionen A und B. Pro Sektion: ein Attenuverter (Va, Regler K), ein Crossfader zwischen Vb und Vc (Regler M plus M-CV-Eingang, dazu ein +5-Vb-Schalter auf Vb) und ein Offset (Regler Oset, 0–10 V). Alle drei Blöcke werden zum Ausgang Vout summiert.

**Polyphonie**<br>
Bis zu 16 Kanäle je Sektion. Alle Regler und der +5-Vb-Schalter sind pro Kanal getrennt gespeichert (128 Params). Ein gemeinsamer Kanalwähler bestimmt, welchen Kanal die Regler auf dem Panel gerade zeigen und ändern.

**Keine Zustandsvariablen (Ausnahme: DC-Blocker)**<br>
Reine lineare Analogmathematik ohne Zeitverhalten; einzige Ausnahme ist der optional zuschaltbare DC-Blocker (Abschnitt 2). Ein Sample-Aufruf berechnet die Ausgangsspannung ausschließlich aus den aktuellen Eingängen und Reglerwerten.

**Zwei Optionen im Modul-Kontextmenü**<br>
Standardmäßig rechnet Minerva frei: M ist nicht begrenzt (Vier-Quadranten-Multiplikation, Invertierung, Verstärkung über 1), und der Ausgang wird nicht beschnitten. Beides lässt sich umschalten: **M-Bereich** (unbegrenzt / begrenzt auf 0…1) und **Ausgangsbegrenzung** (aus / weich / hart, fest ±12 V). Beide Einstellungen gelten für das ganze Modul und liegen nur im JSON, nicht in den Params. Zusätzlich lässt sich je Sektion ein DC-Blocker zuschalten (siehe Abschnitt 2).

---

<a id="sec2"></a>

## 2. Rechnung und Signalfluss
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Pro Sektion und Kanal gilt:

```
Vout  = Va·K + ( (Vb + 5 V·S)·(1 − M) + Vc·M ) + Voset
M     = M_Regler + M_CV / 10 V          (Standard: unbegrenzt)
M     = clamp( M_Regler + M_CV / 10 V , 0 , 1 )   (Option: begrenzt)
Voset = Oset_Regler(0…1) · 10 V
```

Danach folgt optional die Ausgangsbegrenzung (siehe unten).

**Attenuverter (Va, K)**<br>
K läuft von −1 bis +1, die Mittelstellung ist 0 (Va stumm). Nach links wird die Polarität von Va umgekehrt (K = −1: Va invertiert), nach rechts bleibt sie erhalten (K = +1: unverändert). Zwischenwerte skalieren nur die Stärke. K wirkt ausschließlich auf Va.

**Crossfader (Vb, Vc, M)**<br>
M überblendet zwischen Vb (M = 0) und Vc (M = 1). Regler und M-CV-Eingang addieren sich. Da M mit Vc und (1 − M) mit Vb multipliziert wird, ist der Crossfader zugleich ein Multiplizierer: bei unbelegtem Vb ergibt sich `Vc·M`.

**M-Bereich: unbegrenzt (Standard) oder begrenzt**<br>
*Unbegrenzt:* M darf beliebig weit über 1 oder unter 0 laufen. Die Formel rechnet dann einfach weiter:

- M > 1 (z. B. 1,5): `(1 − M) = −0,5`, Vb wird invertiert und halbiert, Vc wird mit 1,5 verstärkt.
- M < 0 (z. B. −0,5): Vc wird invertiert und halbiert, Vb wird mit 1,5 verstärkt.

**Wichtig: Vb und Vc verhalten sich an den Reglerenden gegensätzlich.** Vc wird mit M gewichtet, Vb mit (1 − M). Deshalb gilt am Regler von −1 bis +2:

| M | Vout (nur Überblendung) | Vc | Vb |
|---|---|---|---|
| −1 | `2·Vb − Vc` | invertiert | verdoppelt |
| 0 | `Vb` | aus | unverändert |
| 0,5 | `0,5·Vb + 0,5·Vc` | halb | halb |
| 1 | `Vc` | unverändert | aus |
| +2 | `−Vb + 2·Vc` | verdoppelt | invertiert |

Ein Signal an **Vc** wird bei M = +2 verstärkt und bei M = −1 invertiert. Ein Signal an **Vb** verhält sich genau umgekehrt: bei M = −1 verstärkt, bei M = +2 invertiert. Die Gewichte ergeben immer zusammen 1, die Differenz zwischen Vb und Vc wird also über die Enden hinaus extrapoliert. Beim +5-Vb-Schalter wirkt der Faktor auf `Vb + 5 V`, der Offset wird also mit verstärkt bzw. invertiert. Ein Signal an M CV verschiebt M zusätzlich (10 V = Hub 1).

Damit ist die M-Steuerung bipolar nutzbar: Ein bipolares Signal an M CV bei M-Regler 0 und unbelegtem Vb ergibt eine echte Vier-Quadranten-Multiplikation `Vc·M` (Ringmodulation).

*Begrenzt:* M wird hart auf 0…1 geklemmt. Dann findet nur eine reine Überblendung statt, beide Faktoren bleiben positiv. Nützlich für Patches, die auf dem Abschneiden beruhen, und für größere Nähe zum Original.

**+5-Vb-Schalter (S)**<br>
Im Original als „.5" bzw. „+.5" beschriftet, im Plugin als **„+5 Vb"**. Addiert 5 V auf Vb, und zwar **innerhalb** der Überblendung (Faktor (1 − M)). Bei unbelegtem Vb liegen dadurch 5 V an, der Beitrag ist `5 V·(1 − M)`. So lässt sich die M-Steuerung invertieren und skalieren. Der Oset-Regler kann das nicht ersetzen, da er erst die fertige Summe verschiebt.

**Offset (Oset)**<br>
Addiert 0–10 V zur Summe.

**DC-Blocker (Kontextmenü, je Sektion)**<br>
Im Modul-Kontextmenü lässt sich je Sektion ein DC-Blocker zuschalten („DC blocker Section A/B"), standardmäßig aus. Er ist ein einpoliger Hochpass bei ca. 10 Hz (−3 dB bei 10 Hz, etwa −7 dB bei 5 Hz, −20 dB bei 1 Hz), pro Kanal mit eigenem Zustand. Reihenfolge am Ausgang: Signalanteil → DC-Blocker → + Festanteil → Ausgangsbegrenzung. Er sitzt **vor Oset**: Er bearbeitet nur den Signalanteil `Va·K + Vb·(1 − M) + Vc·M`. Oset und der +5-Vb-Anteil `5 V·(1 − M)` werden danach addiert und bleiben als bewusste Gleichanteile erhalten (Kinovice-Entscheidung). Gleichanteile aus dem Eingangssignal werden entfernt. Da er auch langsame CV-Anteile unter ca. 10 Hz entfernt, ist er für Audio gedacht. Beim Zu- oder Abschalten entsteht ein Knacken in Höhe des Gleichanteils.

**Unbelegte Eingänge**<br>
Liegen auf 0 V (das gilt auch für Vb, bevor S addiert wird).

**Ausgangsbegrenzung (Aus / Weich / Hart)**<br>
Sie sitzt am Ausgang, nach der Summe (und nach einem zugeschalteten DC-Blocker), und entspricht einem Op-Amp an seiner Versorgungsspannung. Der Pegel ist fest **L = ±12 V**. Zwischenwerte wie M bleiben unberührt.

- **Aus (Standard):** keine Begrenzung. Negative Spannungen und Werte über 12 V sind möglich.
- **Hart:** `Vout = clamp(Vout, −L, +L)`. Unterhalb von L exakt linear, darüber hart abgeschnitten.
- **Weich:** Bis zur Schwelle **T = 0,9·L = 10,8 V** bleibt das Signal exakt linear, darüber geht es sanft in L über:

```
|x| ≤ T:  y = x
|x| > T:  y = sign(x) · ( T + (L − T) · tanh( (|x| − T) / (L − T) ) )
```

Die Kurve ist an der Schwelle stetig und hat dort dieselbe Steigung (kein Knick). Das Knie ist (L − T) = 1,2 V breit. Der übliche 0…10-V-Bereich bleibt damit vollständig unverformt, erst echte Übersteuerungen werden beschnitten. Als Sicherheitsnetz ist die Begrenzung besonders sinnvoll, wenn M unbegrenzt ist und ein Fehlpatch große Pegel erzeugt.

---

<a id="sec3"></a>

## 3. Bedienelemente und Panel
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Kopfbereich**<br>
Titel „MINERVA". Darunter der Kanalwähler (Knob mit Zentrumsziffer 1–16) und der Button ACTIVATE. Beschriftung grau, da Blau in der Farblogik des Plugins bereits Section A ist.

**Sektionen A (blau) und B (orange)**<br>
Zwei übereinanderliegende Rahmen (je 42 mm hoch). Reglerreihe: **K** (links), **M** (Mitte), **OSET** (rechts). Rechts neben dem Sektionstitel sitzt der Schalter **+5 Vb** (leuchtet orange, wenn aktiv). Buchsenreihe in der Reihenfolge des Originals: **Va, Vb, M CV, Vc, OUT**.

**Regler-Bereiche und Voreinstellungen**<br>
K: −1…+1, Standard 0. M: −1…+2, Standard 0,5 (Vb und Vc je zur Hälfte; 0 = nur Vb, 1 = nur Vc). Oset: 0…10 V (Anzeige in Volt), Standard 0. +5 Vb: aus. **Hinweis:** Mit K = 0 ist Va standardmäßig stumm – K muss aufgedreht werden, damit Va zu hören ist. Der Regler M läuft von −1 bis +2. Werte außerhalb 0…1 (Invertierung, Verstärkung) wirken nur bei unbegrenztem M-Bereich; bei „Begrenzt 0…1" wird die Summe aus Regler und M CV weiterhin auf 0…1 geklemmt. Über M CV lässt sich der Bereich zusätzlich überschreiten (10 V = Hub 1).

**Kontextmenü Regler und Schalter**<br>
„Apply to all 16 channels" übernimmt den Wert dieses Reglers (bzw. Schalters) für alle 16 Kanäle derselben Sektion und Art, als **ein** Undo-Schritt.

**Kontextmenü Modul**<br>
- „Channels Section A" und „Channels Section B": Auto oder fest 1–16 (siehe Abschnitt 4).
- „M-Bereich": **Unbegrenzt** (Standard) / Begrenzt 0…1.
- „Ausgangsbegrenzung": **Aus** (Standard) / Weich / Hart. Pegel fest ±12 V, Weich-Schwelle fest 10,8 V.
- „DC blocker Section A" / „DC blocker Section B": DC-Blocker je Sektion zuschalten (Standard aus, siehe Abschnitt 2).

M-Bereich und Ausgangsbegrenzung gelten gemeinsam für beide Sektionen und alle Kanäle. Der DC-Blocker wird je Sektion gesetzt.

---

<a id="sec4"></a>

## 4. Polyphonie und Kanalwahl
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Kanalzahl des Ausgangs**<br>
Standard „Auto": Maximum der Kanalzahlen von Va, Vb, M CV und Vc, mindestens 1. Über das Modul-Kontextmenü kann pro Sektion eine feste Kanalzahl (1–16) gewählt werden. Damit lässt sich ein Mono-Signal auf bis zu 16 Kanäle aufteilen, die jeweils eigene Reglerwerte haben.

**Mono-Eingänge**<br>
Ein Mono-Eingang wird auf alle Ausgangskanäle verteilt.

**Kanalwähler (kein Param)**<br>
`pendingCh` (Auswahl) und `displayedCh` (angezeigter Kanal) sind bewusst **keine Params**: Pickup/Catch, History und Preset-Recall sollen die GUI-Navigation nicht beeinflussen. Der Knob ändert `pendingCh`; ACTIVATE übernimmt ihn in `displayedCh`. Die Ziffer im Knob ist grün, wenn Auswahl und angezeigter Kanal übereinstimmen, sonst amber; ACTIVATE ist dann grün bzw. rot. Beide Werte werden per JSON gespeichert und beim Laden geklemmt.

**Widget-Sets**<br>
Alle Regler und Schalter existieren 16-fach übereinander (ein Set je Kanal, insgesamt 128 Param-Widgets); `step()` blendet alle bis auf den angezeigten Kanal aus.

**Modulweite Optionen gelten für alle Kanäle**<br>
M-Bereich und Ausgangsbegrenzung sind nicht kanalweise einstellbar. Sie wirken einheitlich auf alle Kanäle beider Sektionen und sind wie der Kanalwähler keine Params.

---

<a id="sec5"></a>

## 5. Interne Verarbeitung (C++)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**`VoltageProc.hpp`**<br>
Drei freie Funktionen, alle für vier Kanäle gleichzeitig als `float_4`:

- `VoltageProc::processParts(va, vb, vc, mCv, k, mKnob, vosetKnob, plus5, mLimited, signalPart, staticPart)` berechnet die Summe in zwei Anteilen: `signalPart = Va·K + Vb·(1 − M) + Vc·M` (Signal und Crossfader) und `staticPart = 5 V·plus5·(1 − M) + Voset` (bewusste Gleichanteile). Bei `mLimited = false` (Standard) wird M nicht geklemmt, sonst per `simd::clamp(m, 0, 1)`.
- `VoltageProc::process(…)` mit denselben Eingaben liefert die Gesamtsumme `signalPart + staticPart`.
- `VoltageProc::limitOutput(x, mode)` wendet die Ausgangsbegrenzung an (`mode`: 0 = Aus, 1 = Weich, 2 = Hart).

Konstanten: `M_CV_VOLTS_FULL = 10`, `VOSET_MAX_VOLTS = 10`, `PLUS5_VOLTS = 5`, `OUT_LIMIT_VOLTS = 12`, `SOFT_KNEE_RATIO = 0.9` (Schwelle T = 10,8 V).

**Weiches Limit in SIMD**<br>
Rack bietet kein SIMD-`tanh`. Deshalb wird `tanh` durch die rationale Näherung `s(t) = t·(27 + t²) / (27 + 9t²)` ersetzt, mit `t` vorher auf 3 geklemmt (dort ist `s = 1`, der Übergang ist also stetig; Steigung bei 0 ist 1, wie beim echten `tanh`). Verzweigungsfrei gerechnet:

```
a    = abs(x)
t    = min( max(a − T, 0) / (L − T) , 3 )
s    = t·(27 + t²) / (27 + 9t²)
y    = sign(x) · ( min(a, T) + (L − T)·s )
```

Unterhalb von T ist `t = 0` und damit `y = x`. Ab `a = T + 3·(L − T) = 14,4 V` liegt `y` konstant bei L.

**`Minerva.cpp`**<br>
`struct Minerva : Module` mit `processSection(s)` je Sektion: Kanalzahl bestimmen, in 4er-Blöcken Params lesen, Eingänge per `getPolyVoltageSimd` holen (unbelegt = 0 V), `VoltageProc::process` aufrufen, bei `outputClip != 0` das Ergebnis durch `VoltageProc::limitOutput` schicken und mit `setVoltageSimd` schreiben. Ist die Begrenzung aus, entfällt der Aufruf komplett.

**Param-Layout (128 Params)**<br>
Je Sektion drei zusammenhängende 16er-Blöcke: K, M, Oset (Indizes 0–95, per `paramIndex(section, type, ch)`). Die +5-Vb-Schalter liegen als zwei weitere 16er-Blöcke **am Enum-Ende** (Indizes 96–127, per `plus5Param(section, ch)`), damit die Indizes der ursprünglichen 96 Params und damit gespeicherte Patches unverändert blieben. Ein `static_assert` sichert die Übereinstimmung von Enum und Hilfsfunktion. M-Bereich und Ausgangsbegrenzung belegen keine Params.

**Eingänge/Ausgänge**<br>
Eingänge je Sektion in der Reihenfolge Va, Vb, M CV, Vc (Sektion B = Sektion A + 4). Ausgänge `OUT_A_OUTPUT`, `OUT_B_OUTPUT`.

**Persistenz (JSON)**<br>
`pendingCh`, `displayedCh`, `channelsA`, `channelsB` (0 = Auto, 1–16 fest), `mLimited` (bool, fehlt der Schlüssel: `false`, also unbegrenzt) und `outputClip` (0 = Aus, 1 = Weich, 2 = Hart; fehlt der Schlüssel: 0), jeweils mit Clamp beim Laden. Alle Reglerwerte laufen über die normalen Rack-Params. Dazu `dcBlock` (Array mit zwei Einträgen 0/1 für A und B; fehlt der Schlüssel in älteren Patches, ist der Blocker aus). Der Filterzustand wird nicht gespeichert und bei `onReset()` gelöscht.

**DC-Blocker (Engine)**<br>
`DcBlock.hpp` (gemeinsam mit Elena): `y[n] = x[n] − x[n−1] + R·y[n−1]` mit `R = exp(−2π·10 Hz / fs)`, bei Änderung der Abtastrate neu berechnet. `Minerva.cpp` holt Signal- und Festanteil getrennt aus `VoltageProc::processParts` (die jeweilige M-Behandlung, unbegrenzt oder begrenzt, gilt dabei für beide Anteile). Der Blocker filtert nur den Signalanteil, danach wird der Festanteil (Oset und `5 V·(1 − M)`) wieder addiert und anschließend die Ausgangsbegrenzung angewendet. Ausgeschaltet wird der Filterzustand mitgeführt (x[n−1] = Signalanteil, y[n−1] = 0), damit beim Zuschalten kein Altzustand wirkt.

**Namensraum**<br>
Alle lokalen Structs stehen in einem anonymen Namespace (interne Bindung), damit sie nie mit gleichnamigen Structs anderer `.cpp`-Dateien der Plugin-`.so` kollidieren (One-Definition-Rule-Vorfall bei `PanelLabel`, siehe Sofonisba-Dokumentation). Nur `modelMinerva` ist exportiert. `PanelLabel` und die Farben kommen aus `PanelLabel.hpp`.

---

<a id="sec6"></a>

## 6. Patch-Beispiele
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Invertieren (Sinus, 180°)**<br>
Bipolarer Sinus in Va, K = −1: Ausgang = −Va. Beim Sinus entspricht das einer Phasendrehung um 180°. Genau genommen ist es eine Spiegelung der Spannung, keine Zeitverschiebung: eine Rampe wird zur fallenden Rampe. Beliebige Phasenwinkel sind mit Minerva nicht möglich.

**Invertierte Rampe (10 V → 0 V)**<br>
Rampe 0–10 V in Va, K = −1, Oset = 10 V.

**Multiplikation (unipolar)**<br>
Vb unbelegt, Signal in Vc, zweites Signal (0–10 V) in M CV, M-Regler auf 0: Ausgang = `Vc · (M_CV / 10)`.

**Ringmodulation (Vier-Quadranten-Multiplikation, im Rack geprüft)**<br>
M-Bereich unbegrenzt (Standard). Vb unbelegt, +5 Vb aus, Audiosignal in Vc, bipolarer Modulator (z. B. ±5 V) in M CV, M-Regler auf 0: Ausgang = `Vc · (M_CV / 10)`. Die negative Halbwelle des Modulators kehrt Vc um. Im Modus „begrenzt" würde sie stattdessen auf M = 0 abgeschnitten.

**M-Steuerung invertieren**<br>
Vb unbelegt, +5 Vb an, Vc unbelegt: Ausgang = `5 V·(1 − M)`. Die M-CV wirkt dann invertiert.

**Parabel aus einer Rampe (rechnerisch, nicht im Rack geprüft)**<br>
Sektion A: Rampe in Va, K = −1, Oset = 10 V → invertierte Rampe. Sektion B: Rampe in Vc, invertierte Rampe in M CV, M-Regler 0 → `10 V · u · (1 − u)` (u = Rampe/10 V). Die Spitze liegt nur bei 2,5 V, Minerva hat pro Sektion höchstens den Verstärkungsfaktor 2. Für die vollständige Glocke `u²(1−u)²` mit 10 V Spitze siehe `Elena-Dokumentation_DE.md`. Da u zwischen 0 und 1 bleibt, ist das Ergebnis in beiden M-Modi gleich.

---

<a id="sec7"></a>

## 7. Annahmen und offene Punkte
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Die Referenzdokumentation enthält die Gleichung, lässt aber Details offen. Folgende Punkte sind **Annahmen** und nicht belegt:

**M-CV-Skalierung**<br>
Die Referenzdokumentation nennt nur, dass Regler und CV addiert werden. Angenommen: 10 V = voller Hub (M 0→1). Die Konstante `M_CV_VOLTS_FULL` steht an einer Stelle in `VoltageProc.hpp`.

**Verhalten von M außerhalb 0…1**<br>
Die Referenzdokumentation sagt dazu nichts. Beide Varianten (unbegrenzt und begrenzt) sind Annahmen. Die echte Schaltung begrenzt vermutlich eher über die Betriebsspannung als hart bei M = 1, das ist aber nicht belegt. Kinovice-Entscheidung: Standard ist **unbegrenzt**, weil das mehr Patch-Möglichkeiten eröffnet (Ringmodulation, Verstärkung über 1, Invertierung von Vb bzw. Vc als Rechenoperation). Der begrenzte Modus bleibt per Kontextmenü wählbar. **Hinweis zu älteren Patches:** Patches, die mit der früheren festen Begrenzung gebaut wurden und darauf angewiesen sind (z. B. eine Rampe, die über 10 V hinausläuft), verhalten sich im neuen Standard anders. Dort ist „M-Bereich: Begrenzt" zu wählen.

**Ausgangsbegrenzung, Pegel und Impedanz**<br>
Über die Hardware-Grenzen der Schaltung sagt die Referenzdokumentation nichts; ein Schaltplan liegt nicht vor. Der feste Pegel ±12 V orientiert sich an der üblichen Eurorack-Versorgung, die Weich-Schwelle von 0,9·L (10,8 V) an dem Wunsch, den gesamten 0…10-V-Bereich unverformt zu lassen. Beides sind Annahmen. Standard ist „Aus".

**+5-Vb-Schalter auf unbelegtem Vb**<br>
Es liegen 5 V an (die Beschreibung spricht davon, dass der Schalter intern ein +5-V-Signal zuschaltet).

---

<a id="sec8"></a>

## 8. Design-Entscheidungen
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

- **Name Minerva (Kinovice):** nach einer der Anguissola-Schwestern, wie die übrigen Module des Plugins.
- **Analoge Spannungsrechnung und digitale Transformation als getrennte Module (Kinovice):** zwei verschiedene Funktionen, daher zwei Module: Minerva (diese Datei) und `Elena`.
- **Polyphon mit individuellen Werten je Kanal (Kinovice-Anforderung):** alle 16 Kanäle einzeln einstellbar, mit gemeinsamem Kanalwähler für beide Sektionen (Kinovice-Entscheidung).
- **Kanalwähler ohne Param:** wie bei Lucia und Europa, damit Pickup/Catch, History und Preset-Recall die Navigation nicht beeinflussen.
- **Nachträglicher +5-Vb-Schalter (Kinovice-Entscheidung):** zunächst fälschlich weggelassen mit der Begründung, Oset ersetze ihn. Das stimmt nicht, da Oset die Summe verschiebt, der Schalter aber innerhalb der Überblendung wirkt. Nachgerüstet **je Sektion und Kanal**, als Ende-Anhängsel am Enum, damit gespeicherte Patches unverändert laden.
- **Rechtsklick „Apply to all 16 channels"** als ein Undo-Schritt, damit nicht alle 16 Kanäle einzeln gesetzt werden müssen.
- **Kanalzahl „Auto" oder fest 1–16 pro Sektion (Kontextmenü):** Auto folgt der Eingangs-Polyphonie, fest erlaubt die Aufteilung eines Mono-Signals.
- **M-Bereich standardmäßig unbegrenzt, umschaltbar (Kinovice-Entscheidung):** ersetzt die frühere feste Begrenzung auf 0…1. Unbegrenzt ermöglicht Vier-Quadranten-Multiplikation, Verstärkung über 1 und Invertierung. Der M-Regler reicht dafür von −1 bis +2 (Standard 0,5), damit Invertierung und Verstärkung ohne zweite Spannungsquelle direkt am Regler möglich sind. Die Umschaltung liegt nur im JSON (kein Param), gespeicherte Patches laden weiter, verhalten sich aber im Standard anders, wenn sie auf der alten Begrenzung beruhten.
- **Ausgangsbegrenzung optional: Aus / Weich / Hart (Kinovice-Entscheidung):** ersetzt die frühere Entscheidung „kein Ausgangs-Clipping". Standard bleibt „Aus", damit Minerva ein exakt linearer Rechenbaustein bleibt. Die Begrenzung dient als Sicherheitsnetz, vor allem bei unbegrenztem M.
- **Fester Pegel ±12 V, Weich-Schwelle fest 0,9·L = 10,8 V (Kinovice-Entscheidung):** hält das Kontextmenü klein. Mit 0,9 statt 0,8 bleibt der komplette 0…10-V-Bereich exakt linear (mit 0,8 wäre ab 9,6 V bereits ein reguläres 10-V-Signal leicht verformt worden).
- **Weich-Modus als Soft-Knee statt reinem `tanh`:** ein reiner `L·tanh(x/L)` verformt schon kleine Pegel (bei 0,5·L nur 0,46·L) und würde Multiplikation oder Parabel leicht verzerren. Der Soft-Knee bleibt bis T exakt linear.
- **Modulweit statt je Sektion (Kinovice-Entscheidung):** M-Bereich und Ausgangsbegrenzung gelten für das ganze Modul, das hält das Menü übersichtlich.
- **Zuschaltbarer DC-Blocker vor Oset (Kinovice-Entscheidung):** je Sektion im Kontextmenü, ca. 10 Hz fest, standardmäßig aus. Er filtert nur Signal und Crossfader; Oset und der +5-Vb-Anteil bleiben erhalten, weil sie bewusste Gleichanteile sind. Er sitzt vor der Ausgangsbegrenzung. Anlass: Gleichanteil bei eingespeistem Audio, der im Analyzer sichtbar wurde.
