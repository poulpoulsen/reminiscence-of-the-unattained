# Minerva – Technische Dokumentation

Sechstes Modul des Plugins **reminiscence of the unattained**:
ein VCV-Rack-Modul, stark inspiriert von der wunderbaren Buchla-Modulwelt –
ein Dual Voltage Processor (14HP). Zwei identische Sektionen verrechnen je
vier Spannungen zu einer Ausgangsspannung. Minerva ist polyphon; jeder der
16 Kanäle hat eigene Reglerwerte. Diese Datei beschreibt jedes Bedienelement,
die Rechnung, die Polyphonie-Mechanik, die C++-Struktur und die Annahmen, die
die Referenzdokumentation offenlässt.

**Abgrenzung:** Minerva deckt analoge Spannungsrechnung ab (Attenuverter,
Crossfader, Offset). Kennlinien-Transformation und Slew-Limiting, wie sie ein
digitaler Control-Voltage-Processor bietet, sind eine andere Funktion. Das
Plugin setzt sie im eigenen Modul **Elena** um, siehe
`Elena-Dokumentation_DE.md`.

Quellen: öffentlich verfügbare Benutzerdokumentation der Hardware, die dieses
Modul inspiriert hat.

**Hinweis:** Das Plugin *reminiscence of the unattained* ist ein eigenständiges Werk, stark inspiriert von der wunderbaren Buchla-Modulwelt. Es steht in keiner Verbindung zu Buchla, Tiptop Audio oder einem anderen Hersteller und wird von diesen weder autorisiert noch unterstützt. Erwähnte Produkt- und Firmennamen sind Marken ihrer jeweiligen Inhaber. Sie dienen nur dazu, die funktionalen Konzepte zu beschreiben, die ein Modul inspiriert haben.

Teststand (live im laufenden Rack): Grundfunktion beider
Sektionen, polyphoner Betrieb, individuelle Reglerwerte je Kanal,
Kanalwähler und der +5-Vb-Schalter (einzeln je Kanal schaltbar) funktionieren.
Ebenfalls im Rack geprüft: der M-Regler im Bereich −1…+2 an Vb und Vc, die
Ringmodulation im Standardmodus (unbegrenzter M-Bereich), „Begrenzt 0…1",
Ausgangsbegrenzung Weich und Hart sowie das Laden eines älteren Patches ohne
die neuen JSON-Schlüssel. Der DC-Blocker (Abschnitt 2) ist neu, rechnerisch
geprüft und noch nicht im Rack getestet. Das Parabel-Beispiel in Abschnitt 6
ist ebenfalls nur rechnerisch geprüft.

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
14HP, zwei identische Sektionen A und B. Jede Sektion hat einen Attenuverter (Va, Regler K), einen Crossfader zwischen Vb und Vc (Regler M, M-CV-Eingang und ein +5-Vb-Schalter auf Vb) sowie einen Offset (Regler Oset, 0–10 V). Die drei Blöcke werden zum Ausgang Vout summiert.

**Polyphonie**<br>
Jede Sektion verarbeitet bis zu 16 Kanäle. Alle Regler und der +5-Vb-Schalter sind pro Kanal gespeichert (128 Params). Ein gemeinsamer Kanalwähler bestimmt, welchen Kanal die Regler auf dem Panel zeigen und ändern.

**Keine Zustandsvariablen (Ausnahme: DC-Blocker)**<br>
Minerva rechnet rein linear und ohne Zeitverhalten. Einzige Ausnahme ist der optionale DC-Blocker (Abschnitt 2). Jeder Sample-Aufruf berechnet die Ausgangsspannung nur aus den aktuellen Eingängen und Reglerwerten.

**Zwei Optionen im Modul-Kontextmenü**<br>
Standardmäßig rechnet Minerva frei: M ist nicht begrenzt (Vier-Quadranten-Multiplikation, Invertierung, Verstärkung über 1), und der Ausgang wird nicht beschnitten. Beides ist umschaltbar: **M-Bereich** (unbegrenzt / begrenzt auf 0…1) und **Ausgangsbegrenzung** (aus / weich / hart, fest ±12 V). Beide Einstellungen gelten für das ganze Modul und liegen nur im JSON, nicht in den Params. Zusätzlich lässt sich je Sektion ein DC-Blocker zuschalten (Abschnitt 2).

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
K läuft von −1 bis +1, die Mittelstellung 0 schaltet Va stumm. Nach links kehrt sich die Polarität von Va um (K = −1: Va invertiert), nach rechts bleibt sie erhalten (K = +1: unverändert). Zwischenwerte skalieren nur die Stärke. K wirkt ausschließlich auf Va.

**Crossfader (Vb, Vc, M)**<br>
M überblendet zwischen Vb (M = 0) und Vc (M = 1). Regler und M-CV-Eingang addieren sich. Weil M mit Vc und (1 − M) mit Vb multipliziert wird, ist der Crossfader zugleich ein Multiplizierer: Bei unbelegtem Vb ergibt sich `Vc·M`.

**M-Bereich: unbegrenzt (Standard) oder begrenzt**<br>
*Unbegrenzt:* M darf beliebig weit über 1 oder unter 0 laufen. Die Formel rechnet einfach weiter:

- M > 1 (z. B. 1,5): `(1 − M) = −0,5`. Vb wird invertiert und halbiert, Vc mit 1,5 verstärkt.
- M < 0 (z. B. −0,5): Vc wird invertiert und halbiert, Vb mit 1,5 verstärkt.

**Wichtig: Vb und Vc verhalten sich an den Reglerenden gegensätzlich.** Vc wird mit M gewichtet, Vb mit (1 − M). Am Regler von −1 bis +2 ergibt sich daher:

| M | Vout (nur Überblendung) | Vc | Vb |
|---|---|---|---|
| −1 | `2·Vb − Vc` | invertiert | verdoppelt |
| 0 | `Vb` | aus | unverändert |
| 0,5 | `0,5·Vb + 0,5·Vc` | halb | halb |
| 1 | `Vc` | unverändert | aus |
| +2 | `−Vb + 2·Vc` | verdoppelt | invertiert |

Ein Signal an **Vc** wird bei M = +2 verstärkt und bei M = −1 invertiert. Ein Signal an **Vb** verhält sich genau umgekehrt: bei M = −1 verstärkt, bei M = +2 invertiert. Die Gewichte ergeben immer zusammen 1. Die Differenz zwischen Vb und Vc wird also über die Enden hinaus extrapoliert. Beim +5-Vb-Schalter wirkt der Faktor auf `Vb + 5 V`; der Offset wird dabei mit verstärkt bzw. invertiert. Ein Signal an M CV verschiebt M zusätzlich (10 V = Hub 1).

So lässt sich M bipolar steuern: Ein bipolares Signal an M CV ergibt bei M-Regler 0 und unbelegtem Vb eine echte Vier-Quadranten-Multiplikation `Vc·M` (Ringmodulation).

*Begrenzt:* M wird hart auf 0…1 geklemmt. Es findet nur eine reine Überblendung statt, beide Faktoren bleiben positiv. Das eignet sich für Patches, die auf dem Abschneiden beruhen, und liegt näher am Original.

**+5-Vb-Schalter (S)**<br>
Das Original beschriftet ihn mit „.5" bzw. „+.5", das Plugin mit **„+5 Vb"**. Der Schalter addiert 5 V auf Vb, und zwar **innerhalb** der Überblendung (Faktor (1 − M)). Bei unbelegtem Vb liegen dadurch 5 V an, der Beitrag ist `5 V·(1 − M)`. Damit lässt sich die M-Steuerung invertieren und skalieren. Der Oset-Regler kann das nicht ersetzen, da er erst die fertige Summe verschiebt.

**Offset (Oset)**<br>
Addiert 0–10 V zur Summe.

**DC-Blocker (Kontextmenü, je Sektion)**<br>
Im Modul-Kontextmenü lässt sich je Sektion ein DC-Blocker zuschalten („DC blocker Section A/B"), standardmäßig aus. Er ist ein einpoliger Hochpass bei ca. 10 Hz (−3 dB bei 10 Hz, etwa −7 dB bei 5 Hz, −20 dB bei 1 Hz) mit eigenem Zustand pro Kanal. Reihenfolge am Ausgang: Signalanteil → DC-Blocker → + Festanteil → Ausgangsbegrenzung. Der Blocker sitzt **vor Oset** und bearbeitet nur den Signalanteil `Va·K + Vb·(1 − M) + Vc·M`. Oset und der +5-Vb-Anteil `5 V·(1 − M)` werden danach addiert und bleiben als bewusste Gleichanteile erhalten (Meine Entscheidung). Gleichanteile aus dem Eingangssignal entfernt der Blocker. Weil er auch langsame CV-Anteile unter ca. 10 Hz entfernt, ist er für Audio gedacht. Beim Zu- oder Abschalten knackt es in Höhe des Gleichanteils.

**Unbelegte Eingänge**<br>
Sie liegen auf 0 V. Das gilt auch für Vb, bevor S addiert wird.

**Ausgangsbegrenzung (Aus / Weich / Hart)**<br>
Sie sitzt am Ausgang nach der Summe (und nach einem zugeschalteten DC-Blocker) und entspricht einem Op-Amp an seiner Versorgungsspannung. Der Pegel ist fest **L = ±12 V**. Zwischenwerte wie M bleiben unberührt.

- **Aus (Standard):** keine Begrenzung. Negative Spannungen und Werte über 12 V sind möglich.
- **Hart:** `Vout = clamp(Vout, −L, +L)`. Unterhalb von L exakt linear, darüber hart abgeschnitten.
- **Weich:** Bis zur Schwelle **T = 0,9·L = 10,8 V** bleibt das Signal exakt linear, darüber geht es sanft in L über:

```
|x| ≤ T:  y = x
|x| > T:  y = sign(x) · ( T + (L − T) · tanh( (|x| − T) / (L − T) ) )
```

Die Kurve ist an der Schwelle stetig und hat dort dieselbe Steigung (kein Knick). Das Knie ist (L − T) = 1,2 V breit. Der übliche 0…10-V-Bereich bleibt damit vollständig unverformt; erst echte Übersteuerungen werden beschnitten. Als Sicherheitsnetz ist die Begrenzung besonders sinnvoll, wenn M unbegrenzt ist und ein Fehlpatch große Pegel erzeugt.

---

<a id="sec3"></a>

## 3. Bedienelemente und Panel
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Kopfbereich**<br>
Titel „MINERVA". Darunter liegen der Kanalwähler (Knob mit Zentrumsziffer 1–16) und der Button ACTIVATE. Die Beschriftung ist grau, weil Blau in der Farblogik des Plugins bereits Section A kennzeichnet.

**Sektionen A (blau) und B (orange)**<br>
Zwei übereinanderliegende Rahmen (je 42 mm hoch). Reglerreihe: **K** (links), **M** (Mitte), **OSET** (rechts). Rechts neben dem Sektionstitel sitzt der Schalter **+5 Vb** (leuchtet orange, wenn aktiv). Die Buchsenreihe folgt der Reihenfolge des Originals: **Va, Vb, M CV, Vc, OUT**.

**Regler-Bereiche und Voreinstellungen**<br>
K: −1…+1, Standard 0. M: −1…+2, Standard 0,5 (Vb und Vc je zur Hälfte; 0 = nur Vb, 1 = nur Vc). Oset: 0…10 V (Anzeige in Volt), Standard 0. +5 Vb: aus. **Hinweis:** Mit K = 0 ist Va standardmäßig stumm. K muss aufgedreht werden, damit Va zu hören ist. Werte von M außerhalb 0…1 (Invertierung, Verstärkung) wirken nur bei unbegrenztem M-Bereich. Bei „Begrenzt 0…1" wird die Summe aus Regler und M CV weiterhin auf 0…1 geklemmt. Über M CV lässt sich der Bereich zusätzlich überschreiten (10 V = Hub 1).

**Kontextmenü Regler und Schalter**<br>
„Apply to all 16 channels" überträgt den Wert dieses Reglers (bzw. Schalters) auf alle 16 Kanäle derselben Sektion und Art, als **ein** Undo-Schritt.

**Kontextmenü Modul**<br>
- „Channels Section A" und „Channels Section B": Auto oder fest 1–16 (siehe Abschnitt 4).
- „M-Bereich": **Unbegrenzt** (Standard) / Begrenzt 0…1.
- „Ausgangsbegrenzung": **Aus** (Standard) / Weich / Hart. Pegel fest ±12 V, Weich-Schwelle fest 10,8 V.
- „DC blocker Section A" / „DC blocker Section B": DC-Blocker je Sektion zuschalten (Standard aus, siehe Abschnitt 2).

M-Bereich und Ausgangsbegrenzung gelten gemeinsam für beide Sektionen und alle Kanäle. Den DC-Blocker setzt man je Sektion.

---

<a id="sec4"></a>

## 4. Polyphonie und Kanalwahl
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Kanalzahl des Ausgangs**<br>
Standard ist „Auto": das Maximum der Kanalzahlen von Va, Vb, M CV und Vc, mindestens 1. Im Modul-Kontextmenü lässt sich pro Sektion eine feste Kanalzahl (1–16) wählen. So kann man ein Mono-Signal auf bis zu 16 Kanäle aufteilen, die jeweils eigene Reglerwerte haben.

**Mono-Eingänge**<br>
Ein Mono-Eingang wird auf alle Ausgangskanäle verteilt.

**Kanalwähler (kein Param)**<br>
`pendingCh` (Auswahl) und `displayedCh` (angezeigter Kanal) sind bewusst **keine Params**. Pickup/Catch, History und Preset-Recall sollen die GUI-Navigation nicht beeinflussen. Der Knob ändert `pendingCh`; ACTIVATE übernimmt ihn in `displayedCh`. Die Ziffer im Knob ist grün, wenn Auswahl und angezeigter Kanal übereinstimmen, sonst amber. ACTIVATE ist dann grün bzw. rot. Beide Werte werden per JSON gespeichert und beim Laden geklemmt.

**Widget-Sets**<br>
Alle Regler und Schalter existieren 16-fach übereinander (ein Set je Kanal, insgesamt 128 Param-Widgets). `step()` blendet alle bis auf den angezeigten Kanal aus.

**Modulweite Optionen gelten für alle Kanäle**<br>
M-Bereich und Ausgangsbegrenzung sind nicht kanalweise einstellbar. Sie wirken einheitlich auf alle Kanäle beider Sektionen und sind wie der Kanalwähler keine Params.

---

<a id="sec5"></a>

## 5. Interne Verarbeitung (C++)
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**`VoltageProc.hpp`**<br>
Drei freie Funktionen, alle für vier Kanäle gleichzeitig als `float_4`:

- `VoltageProc::processParts(va, vb, vc, mCv, k, mKnob, vosetKnob, plus5, mLimited, signalPart, staticPart)` berechnet die Summe in zwei Anteilen: `signalPart = Va·K + Vb·(1 − M) + Vc·M` (Signal und Crossfader) und `staticPart = 5 V·plus5·(1 − M) + Voset` (bewusste Gleichanteile). Bei `mLimited = false` (Standard) wird M nicht geklemmt, sonst per `simd::clamp(m, 0, 1)`.
- `VoltageProc::process(…)` nimmt dieselben Eingaben und liefert die Gesamtsumme `signalPart + staticPart`.
- `VoltageProc::limitOutput(x, mode)` wendet die Ausgangsbegrenzung an (`mode`: 0 = Aus, 1 = Weich, 2 = Hart).

Konstanten: `M_CV_VOLTS_FULL = 10`, `VOSET_MAX_VOLTS = 10`, `PLUS5_VOLTS = 5`, `OUT_LIMIT_VOLTS = 12`, `SOFT_KNEE_RATIO = 0.9` (Schwelle T = 10,8 V).

**Weiches Limit in SIMD**<br>
Die weiche Begrenzung braucht eine `tanh`-Kurve. Für SIMD-Vektoren (`float_4`) stellt Rack jedoch keine `tanh`-Funktion bereit. Minerva nutzt stattdessen die rationale Näherung `s(t) = t·(27 + t²) / (27 + 9t²)`. Sie verläuft ähnlich wie `tanh`: Bei 0 hat sie die Steigung 1 und nähert sich dann dem Wert 1. Bei t = 3 erreicht sie 1 exakt (`tanh(3)` liegt bei 0,995). Ohne Begrenzung würde sie darüber weiter ansteigen. Deshalb wird `t` vorher auf 3 geklemmt, ab dort bleibt `s = 1`, und die Kurve geht ohne Sprung in den konstanten Wert über. Die Rechnung kommt ohne Verzweigungen (`if`/`else`) aus, sodass alle vier Kanäle eines `float_4` gleichzeitig berechnet werden:

```
a    = abs(x)
t    = min( max(a − T, 0) / (L − T) , 3 )
s    = t·(27 + t²) / (27 + 9t²)
y    = sign(x) · ( min(a, T) + (L − T)·s )
```

Unterhalb von T ist `t = 0` und damit `y = x`. Ab `a = T + 3·(L − T) = 14,4 V` liegt `y` konstant bei L.

**`Minerva.cpp`**<br>
`struct Minerva : Module` mit `processSection(s)` je Sektion. Die Funktion bestimmt die Kanalzahl, liest die Params in 4er-Blöcken und holt die Eingänge per `getPolyVoltageSimd` (unbelegt = 0 V). Danach ruft sie `VoltageProc::process` auf, schickt das Ergebnis bei `outputClip != 0` durch `VoltageProc::limitOutput` und schreibt es mit `setVoltageSimd`. Ist die Begrenzung aus, entfällt dieser Aufruf komplett.

**Param-Layout (128 Params)**<br>
Je Sektion belegen K, M und Oset drei zusammenhängende 16er-Blöcke (Indizes 0–95, per `paramIndex(section, type, ch)`). Die +5-Vb-Schalter liegen als zwei weitere 16er-Blöcke **am Enum-Ende** (Indizes 96–127, per `plus5Param(section, ch)`). So bleiben die Indizes der ursprünglichen 96 Params und damit gespeicherte Patches unverändert. Ein `static_assert` sichert die Übereinstimmung von Enum und Hilfsfunktion. M-Bereich und Ausgangsbegrenzung belegen keine Params.

**Eingänge/Ausgänge**<br>
Die Eingänge jeder Sektion folgen der Reihenfolge Va, Vb, M CV, Vc (Sektion B = Sektion A + 4). Die Ausgänge heißen `OUT_A_OUTPUT` und `OUT_B_OUTPUT`.

**Persistenz (JSON)**<br>
Gespeichert werden `pendingCh`, `displayedCh`, `channelsA`, `channelsB` (0 = Auto, 1–16 fest), `mLimited` (bool; fehlt der Schlüssel: `false`, also unbegrenzt) und `outputClip` (0 = Aus, 1 = Weich, 2 = Hart; fehlt der Schlüssel: 0). Alle Werte werden beim Laden geklemmt. Die Reglerwerte laufen über die normalen Rack-Params. Dazu kommt `dcBlock`, ein Array mit zwei Einträgen 0/1 für A und B. Fehlt der Schlüssel in älteren Patches, ist der Blocker aus. Der Filterzustand wird nicht gespeichert und bei `onReset()` gelöscht.

**DC-Blocker (Engine)**<br>
`DcBlock.hpp` (gemeinsam mit Elena): `y[n] = x[n] − x[n−1] + R·y[n−1]` mit `R = exp(−2π·10 Hz / fs)`, bei Änderung der Abtastrate neu berechnet. `Minerva.cpp` holt Signal- und Festanteil getrennt aus `VoltageProc::processParts`; die jeweilige M-Behandlung (unbegrenzt oder begrenzt) gilt dabei für beide Anteile. Der Blocker filtert nur den Signalanteil. Danach wird der Festanteil (Oset und `5 V·(1 − M)`) wieder addiert und anschließend die Ausgangsbegrenzung angewendet. Bei ausgeschaltetem Blocker wird der Filterzustand mitgeführt (x[n−1] = Signalanteil, y[n−1] = 0), damit beim Zuschalten kein Altzustand wirkt.

**Namensraum**<br>
Alle lokalen Structs stehen in einem anonymen Namespace (interne Bindung). So kollidieren sie nie mit gleichnamigen Structs anderer `.cpp`-Dateien der Plugin-`.so` (One-Definition-Rule-Vorfall bei `PanelLabel`, siehe Sofonisba-Dokumentation). Exportiert wird nur `modelMinerva`. `PanelLabel` und die Farben kommen aus `PanelLabel.hpp`.

---

<a id="sec6"></a>

## 6. Patch-Beispiele
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

**Invertieren (Sinus, 180°)**<br>
Bipolarer Sinus in Va, K = −1: Ausgang = −Va. Beim Sinus entspricht das einer Phasendrehung um 180°. Genau genommen ist es eine Spiegelung der Spannung, keine Zeitverschiebung: Eine Rampe wird zur fallenden Rampe. Beliebige Phasenwinkel sind mit Minerva nicht möglich.

**Invertierte Rampe (10 V → 0 V)**<br>
Rampe 0–10 V in Va, K = −1, Oset = 10 V.

**Multiplikation (unipolar)**<br>
Vb unbelegt, Signal in Vc, zweites Signal (0–10 V) in M CV, M-Regler auf 0: Ausgang = `Vc · (M_CV / 10)`.

**Ringmodulation (Vier-Quadranten-Multiplikation, im Rack geprüft)**<br>
M-Bereich unbegrenzt (Standard). Vb unbelegt, +5 Vb aus, Audiosignal in Vc, bipolarer Modulator (z. B. ±5 V) in M CV, M-Regler auf 0: Ausgang = `Vc · (M_CV / 10)`. Die negative Halbwelle des Modulators kehrt Vc um. Im Modus „begrenzt" würde sie stattdessen auf M = 0 abgeschnitten.

**M-Steuerung invertieren**<br>
Vb unbelegt, +5 Vb an, Vc unbelegt: Ausgang = `5 V·(1 − M)`. Die M-CV wirkt dann invertiert.

**Parabel aus einer Rampe (rechnerisch, nicht im Rack geprüft)**<br>
Sektion A: Rampe in Va, K = −1, Oset = 10 V → invertierte Rampe. Sektion B: Rampe in Vc, invertierte Rampe in M CV, M-Regler 0 → `10 V · u · (1 − u)` (u = Rampe/10 V). Die Spitze liegt nur bei 2,5 V, da Minerva pro Sektion höchstens den Verstärkungsfaktor 2 hat. Die vollständige Glocke `u²(1−u)²` mit 10 V Spitze beschreibt `Elena-Dokumentation_DE.md`. Da u zwischen 0 und 1 bleibt, ist das Ergebnis in beiden M-Modi gleich.

---

<a id="sec7"></a>

## 7. Annahmen und offene Punkte
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

Die Referenzdokumentation nennt die Gleichung, lässt aber Details offen. Die folgenden Punkte sind **Annahmen** und nicht belegt:

**M-CV-Skalierung**<br>
Die Referenzdokumentation sagt nur, dass sich Regler und CV addieren. Angenommen ist 10 V = voller Hub (M 0→1). Die Konstante `M_CV_VOLTS_FULL` steht an einer Stelle in `VoltageProc.hpp`.

**Verhalten von M außerhalb 0…1**<br>
Dazu sagt die Referenzdokumentation nichts. Beide Varianten (unbegrenzt und begrenzt) sind Annahmen. Die echte Schaltung begrenzt vermutlich eher über die Betriebsspannung als hart bei M = 1, belegt ist das nicht. Meine Entscheidung: Standard ist **unbegrenzt**, weil das mehr Patch-Möglichkeiten eröffnet (Ringmodulation, Verstärkung über 1, Invertierung von Vb bzw. Vc als Rechenoperation). Der begrenzte Modus bleibt per Kontextmenü wählbar. **Hinweis zu älteren Patches:** Patches, die mit der früheren festen Begrenzung gebaut wurden und darauf angewiesen sind (z. B. eine Rampe, die über 10 V hinausläuft), verhalten sich im neuen Standard anders. Für sie ist „M-Bereich: Begrenzt" zu wählen.

**Ausgangsbegrenzung, Pegel und Impedanz**<br>
Über die Hardware-Grenzen der Schaltung sagt die Referenzdokumentation nichts, ein Schaltplan liegt nicht vor. Der feste Pegel von ±12 V orientiert sich an der üblichen Eurorack-Versorgung. Die Weich-Schwelle liegt bei 0,9·L = 10,8 V. Bis zu dieser Spannung gibt der Weich-Modus das Signal unverändert weiter; darüber drückt er es sanft gegen 12 V. Ein reguläres 0…10-V-Signal bleibt damit vollständig unverformt. Beide Werte sind Annahmen. Standard ist „Aus".

**+5-Vb-Schalter auf unbelegtem Vb**<br>
Es liegen 5 V an. Die Beschreibung besagt, dass der Schalter intern ein +5-V-Signal zuschaltet.

---

<a id="sec8"></a>

## 8. Design-Entscheidungen
*[↑ Zurück zum Inhaltsverzeichnis](#toc) · [[#Inhaltsverzeichnis|Obsidian]]*

- **Name Minerva:** nach einer der Anguissola-Schwestern, wie die übrigen Module des Plugins.
- **Analoge Spannungsrechnung und digitale Transformation als getrennte Module:** Es sind zwei verschiedene Funktionen, daher zwei Module: Minerva (diese Datei) und `Elena`.
- **Polyphon mit individuellen Werten je Kanal (Meine Anforderung):** Alle 16 Kanäle sind einzeln einstellbar, mit gemeinsamem Kanalwähler für beide Sektionen (Meine Entscheidung).
- **Kanalwähler ohne Param:** wie bei Lucia und Europa, damit Pickup/Catch, History und Preset-Recall die Navigation nicht beeinflussen.
- **Nachträglicher +5-Vb-Schalter (Meine Entscheidung):** Er fehlte zunächst mit der Begründung, Oset ersetze ihn. Das ist falsch, denn Oset verschiebt die Summe, der Schalter wirkt aber innerhalb der Überblendung. Nachgerüstet wurde er **je Sektion und Kanal**, als Anhängsel am Enum-Ende, damit gespeicherte Patches unverändert laden.
- **Rechtsklick „Apply to all 16 channels"** als ein Undo-Schritt, damit nicht jeder der 16 Kanäle einzeln gesetzt werden muss.
- **Kanalzahl „Auto" oder fest 1–16 pro Sektion (Kontextmenü):** Auto folgt der Eingangs-Polyphonie, fest erlaubt die Aufteilung eines Mono-Signals.
- **M-Bereich standardmäßig unbegrenzt, umschaltbar (Meine Entscheidung):** Das ersetzt die frühere feste Begrenzung auf 0…1. Unbegrenzt ermöglicht Vier-Quadranten-Multiplikation, Verstärkung über 1 und Invertierung. Der M-Regler reicht dafür von −1 bis +2 (Standard 0,5), sodass Invertierung und Verstärkung ohne zweite Spannungsquelle direkt am Regler möglich sind. Die Umschaltung liegt nur im JSON (kein Param). Gespeicherte Patches laden weiter, verhalten sich im Standard aber anders, wenn sie auf der alten Begrenzung beruhten.
- **Ausgangsbegrenzung optional: Aus / Weich / Hart (Meine Entscheidung):** Sie ersetzt die frühere Entscheidung „kein Ausgangs-Clipping". Standard bleibt „Aus", damit Minerva ein exakt linearer Rechenbaustein bleibt. Die Begrenzung dient als Sicherheitsnetz, vor allem bei unbegrenztem M.
- **Fester Pegel ±12 V, Weich-Schwelle fest 0,9·L = 10,8 V (Meine Entscheidung):** Das hält das Kontextmenü klein. Mit 0,9 statt 0,8 bleibt der komplette 0…10-V-Bereich exakt linear; mit 0,8 wäre ein reguläres 10-V-Signal ab 9,6 V leicht verformt worden.
- **Weich-Modus als Soft-Knee statt reinem `tanh`:** Ein reiner `L·tanh(x/L)` verformt schon kleine Pegel (bei 0,5·L nur 0,46·L) und würde Multiplikation oder Parabel leicht verzerren. Der Soft-Knee bleibt bis T exakt linear.
- **Modulweit statt je Sektion (Meine Entscheidung):** M-Bereich und Ausgangsbegrenzung gelten für das ganze Modul, das hält das Menü übersichtlich.
- **Zuschaltbarer DC-Blocker vor Oset (Meine Entscheidung):** Er ist je Sektion im Kontextmenü schaltbar, fest auf ca. 10 Hz eingestellt und standardmäßig aus. Er filtert nur Signal und Crossfader. Oset und der +5-Vb-Anteil bleiben erhalten, weil sie bewusste Gleichanteile sind. Er sitzt vor der Ausgangsbegrenzung. Anlass war ein Gleichanteil bei eingespeistem Audio, der im Analyzer sichtbar wurde.
