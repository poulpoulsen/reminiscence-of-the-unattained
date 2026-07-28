# Marf248 – Designskizze: Stage-Erweiterung per Expander-Kette (+16 pro Modul)

Status: Konzeptpapier, noch nicht implementiert. Vorbereitet für eine eigene, spätere Session.

---

## 1. Zielbild

Ein neuer Expandertyp `Marf248StageExpander`, der sich rechts an Marf248 (oder an einen weiteren
`Marf248StageExpander`) andockt und den Stage-Pool um jeweils 16 Stages verlängert (32, 48, 64 …).
Jeder Expander bringt seine eigene Mini-DSP-Pipeline mit (Slew, Quant, Range, Modifier-Bits) –
konsistent mit dem bestehenden Prinzip "kein zentrales DSP-Monopol", aber ein bewusster Bruch mit
der bisherigen Regel "Expander = nur Port-Durchreichung", da hier erstmals eigene Stage-Logik im
Expander laufen muss.

---

## 2. Architektur-Entscheidung: Message-Chain statt zentralem Array

Zwei Optionen wurden verworfen zugunsten einer dritten:

- **Verworfen: `std::vector<Stage>` im Hauptmodul, das per Expander-Erkennung wächst.**
  Würde bedeuten, dass Marf248 aktiv in den Speicher/Zustand des Expanders schreibt – bricht mit
  Rack-Konventionen (Module besitzen ihren eigenen State) und mit eurer Architekturregel, dass
  Expander unabhängige, eigenständige Module bleiben.

- **Verworfen: Komplett unabhängige Parallel-Sektion ohne Kopplung.**
  Würde bedeuten, Adressierung/Run/Reset laufen pro Block getrennt – kein gemeinsamer "ein Lauflicht
  über 32 Stages"-Charakter mehr, widerspricht dem Konzept einer einzigen erweiterten AFG-Sektion.

- **Gewählt: Message-Chain über `leftExpander`/`rightExpander`, jeder Block behält eigenes DSP,
  aber Adressierung/Run-Zustand werden vom Hauptmodul zentral vorgegeben und pro Sample an den
  nächsten Block weitergereicht (Kette).**
  Damit bleibt jeder Expander für sich lauffähig und testbar, während die Kette sich wie ein
  durchgehender 32/48/64-Stage-AFG verhält.

---

## 3. Message-Struct (Kern der Kette)

Pro Sample-Frame reicht Marf248 (bzw. der vorherige Expander) einen Struct nach rechts weiter:

```cpp
struct StageChainMessage {
    // Steuerung, die vom Hauptmodul kommt und durchgereicht wird
    bool running;
    bool continuousActive;
    int  globalStageAddress;     // 0-basiert über die GESAMTE Kette (0..N-1)
    bool strobeEdge;             // Flankenimpuls für Continuous/Strobe-Reentry
    bool resetActive;
    bool pendulumActive;
    bool pendulumReverse;

    // Ergebnis, das der jeweils AKTIVE Block zurück nach links meldet
    bool  stageIsInThisBlock;    // true, wenn globalStageAddress in den 16 dieses Blocks liegt
    float voltageOut;
    bool  gateOut;
    bool  endOfCycle;            // für Start/End-of-Cycle-Outputs am Trigger-Expander

    // Kettenlänge/Selbstauskunft (für Adress-Bit-Berechnung, siehe Abschnitt 5)
    int blockIndex;              // 0 = Hauptmodul, 1 = erster Stage-Expander, ...
    int totalStagesInChain;      // wird beim Durchlaufen aufsummiert
};
```

Wichtig: `globalStageAddress` wird vom Hauptmodul berechnet (dort sitzt weiterhin der
Stage-Address-Mechanismus inkl. Int/Ext/Continuous/Strobe), aber jeder Block prüft nur, ob die
Adresse in seinen eigenen 16 fällt (`blockIndex*16 <= addr < blockIndex*16+16`). Nur der
zuständige Block schreibt `voltageOut`/`gateOut` in die Message; alle anderen lassen sie
unverändert durch. Das vermeidet einen zentralen "wer ist gerade dran"-Schalter im Hauptmodul.

Jeder Block macht dabei **sein eigenes** Slew/Quant/Range/Modifier-Processing für seine 16 Stages,
exakt wie bisher `Afg::process()`, nur mit `stageTimeHint`, `voltageFrom` etc. lokal gespeichert.

---

## 4. Poly-Aufteilung (harte Grenze: 16 Kanäle pro Kabel)

Der bestehende CV-Expander nutzt 16-Kanal-Poly für Stage-Time-CV, Curve-CV, Gate A/B. Das ist
gleichzeitig das VCV-Maximum – **kein Poly-Kabel kann über 16 Stages hinaus erweitert werden.**

Konsequenz: Jeder `Marf248StageExpander` bekommt **eigene** Poly-Ports (16 Kanäle, Kanal i = Stage
i+1 innerhalb des jeweiligen Blocks), keine Fortsetzung eines gemeinsamen Kabels:

- Stage-Time-CV: eigener Poly-In am Stage-Expander (nicht am Hauptmodul-CV-Expander)
- Curve-CV: eigener Poly-In, eigener Attenuverter/Bias-Trimpot
- Gate A/B: eigener Poly-Out
- Stage-Modifier-Overrides (Skip/First/Last/Stop/Sustain/Enable/Pulse1/Pulse2/Slope): eigener
  Poly-In pro Block, exakt analog zum bestehenden Trigger-Expander-Muster

Das bedeutet: Ein voll ausgebautes 32-Stage-System hat zwei komplett getrennte Sätze an
CV-/Trigger-Expandern (einen für Stages 1–16, einen für 17–32) statt eines gemeinsamen mit
"virtuellen" 32 Kanälen. Konsistent, vorhersagbar, aber mehr HP-Bedarf.

---

## 5. Adressierung über die Kette hinweg

Aktuell: 4 Bit (0–15), Int-Knob 0–15, Ext-Kanal A–D.

Für die Kette:

- Der **globale** Adressraum wächst mit jedem Expander um 16 (32, 48, 64 Stages → 5, 6 Bit nötig).
- Der Int-Knob am Hauptmodul müsste auf den vollen Bereich skaliert werden – ODER (bevorzugte
  Variante, weniger invasiv): Int-Knob bleibt 0–15 **pro Block**, plus ein neuer "Block Select"
  Parameter/CV am Hauptmodul, der bestimmt, welcher der angedockten Blöcke gerade adressiert wird.
  Das vermeidet einen radikalen Umbau des bestehenden Address-Widgets und bleibt näher an der
  Handbuch-Logik (die Original-Hardware kennt ohnehin keine 32-Stage-Betriebsart).
- Ext-Kanal A–D (External Stage Address CV) bleibt unverändert 0–10V auf 0–15 gemappt; bei
  aktivem Block-Select würde dieselbe Spannung einfach auf den gewählten Block angewendet.
- `totalStagesInChain` (aus der Message-Chain) wird zur Laufzeit ermittelt, damit das Hauptmodul
  weiß, ob Block-Select 1, 2, 3 oder 4 Positionen anbieten muss (kein Andocken eines Expanders =
  weiterhin reines 16-Stage-Verhalten, voll abwärtskompatibel).

---

## 6. Pendulum-Modus über die Kette

Bereits mathematisch abgesichertes Prinzip (2×M Periode, beide Endpunkte wiederholt) muss auf den
**globalen** Stage-Count M angewendet werden, nicht pro Block. Das Hauptmodul berechnet weiterhin
zentral die Pendulum-Sequenz und schickt nur `globalStageAddress` weiter – aus Sicht der Blöcke
ändert sich nichts, sie kennen nur ihre eigene Zuständigkeitsprüfung.

---

## 7. Panel/HP-Überlegungen

- Hauptmodul bleibt 120HP wie bisher (Lauflicht 1–16 unverändert).
- `Marf248StageExpander` bekommt ein eigenes Lauflicht (16 Stages) plus die Slew/Quant/Range-Reihe
  wie im Hauptmodul – de facto ein reduziertes Abbild des Hauptpanels ohne Sektions-A/B-Globalteil
  (Scale/Key, Presets, Time-Multiplier bleiben zentral am Hauptmodul, gelten für die ganze Kette).
- Geschätzt vergleichbare Breite wie der bisherige 16-Stage-Bereich im Hauptmodul (~90HP dort) –
  grobe Schätzung, keine belastbare Zahl ohne echtes Layout.
- Passt zeitlich gut zum ohnehin geplanten HP-Layout-Audit (siehe "Auf dem Horizont" in den
  bisherigen Notizen) – beide Themen sollten nicht parallel, aber nacheinander sauber angegangen
  werden, da ein Stage-Expander das Layout-Bild grundlegend verändert.

---

## 8. Offene Designfragen (bewusst nicht vorentschieden)

1. Block-Select als Knob, CV-Eingang, oder beides (Pickup-Mechanik wie bei Presets)?
2. Was passiert bei Trennen eines Expanders während `running`? (Vorschlag: sofortiger Rücksprung
   auf Block 0 / Stage 0, analog zu eurem bestehenden "Held/Enable"-Sicherheitsprinzip)
3. Scale/Key-Quantisierung ist aktuell global (Hauptmodul) – bleibt das so für die ganze Kette,
   oder soll jeder Block eigene Scale/Key-Overrides bekommen? (Tendenz: global lassen, Overrides
   wären ein separates, noch größeres Feature)
4. Soll `Marf248StageExpander` rechts UND links andockbar sein (Kette in beide Richtungen), oder
   nur rechts (einfacher, reicht für 4+ Blöcke)?
5. Regressionstests (geplant, siehe bestehende Notizen zu DSP-Core-Tests): Wie testet man eine
   Kette ohne Rack/JACK? Vorschlag: `StageChainMessage`-Struct pur in C++ durchspielen, mehrere
   `Afg`-Instanzen manuell verketten, plain `assert()` wie beim bereits geplanten Testfile.

---

## 9. Empfohlene Reihenfolge für die Umsetzungs-Session

1. `StageChainMessage`-Struct final festlegen (inkl. Antworten auf Abschnitt 8).
2. Minimal-Skelett: ein Stage-Expander, der sich korrekt an- und abdockt (Message-Passing-Grundgerüst,
   noch ohne eigenes DSP) – Sichtbarkeit/Erkennung zuerst, dann Inhalt.
3. DSP-Logik im Expander (Kopie/Refactor aus `Afg`, lokal auf 16 Stages beschränkt).
4. Block-Select-Mechanismus im Hauptmodul.
5. Eigene CV-/Trigger-Poly-Ports pro Stage-Expander (separates Sub-Feature, ggf. eigene Session).
6. Pendulum/Continuous-Anpassung auf globale Stage-Anzahl.
7. Panel/HP – erst nach obigem Audit, siehe Abschnitt 7.

Ein Feature pro Session bleibt wie bisher das Prinzip; dieses Vorhaben ist groß genug, um in
mehrere Sessions entlang obiger Reihenfolge zerlegt zu werden.
