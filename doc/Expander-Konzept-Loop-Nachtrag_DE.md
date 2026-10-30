# Loop-Expander: Entscheidungen zu den offenen Punkten (Nachtrag, Stand 2026-10-30)

Ergänzt `Expander-Konzept-Loop-CV2_DE.md`. Die dortigen Abschnitte 1-4 (Zähler gehört zum Segmentbesuch, Neuladen bei Segmentwechsel/Reset/Austritt, Forward und Reverse zählen am Wrap, Pendulum in Version 1 ausgeschlossen, Array-Kante nach Option A) bleiben unverändert. Dieser Nachtrag ersetzt die dortigen "Offenen Fragen" 1-5.

## Entscheidungen (Anwender)

1. **Eigenes Expander-Modul von Anfang an.** Kein Platz in Amilcare Spalte G. Der Loop-Expander ist wie Amilcare/Bianca ein reiner Jack-Träger ohne eigene DSP; die gesamte Logik liegt in `Afg`/`Marf248::process()`. Zum Testen dockt nur dieser eine Expander an Sofonisba (direkter Nachbar, links oder rechts). Gleichzeitiger Betrieb mit Amilcare und Bianca bleibt durch das bekannte Nachbar-Problem (nur direkte Nachbarn, siehe Abschnitt 8 der Doku) offen und wird nicht in diesem Feature gelöst.
2. **Wertebereich:** 1 V pro Zone. 0-1 V = endlos (keine Zählung), 1-2 V = 1 Durchlauf ... 9-10 V = 9 Durchläufe. Nicht gepatchte Poly-Kanäle liefern 0 V = endlos, ohne Kabel bleibt alles bitgleich zum Stand vorher. Wert wird beim Neuladen des Zählers gelesen.
3. **Konfiguration an der Last-Stage** des Segments, Poly-Kanal = Stage-Nummer (1-16), gemeinsam für Section A und B (getrennte Zählerstände). Segment ohne Last-Bit: Fallback-Ende Stage 16 (Kanal 16). Reverse zählt am First-Wrap, liest aber den Kanal der Last-Stage des Segments (`findLastAfter(first)`, nur bei Wrap-Ereignissen).
4. **Austrittsimpuls UND Restanzeige, beides in Version 1.** Die Restanzeige gibt es als CV-Ausgang (ohne Blinken) und als Panel-Ziffernanzeige mit Blink-0 beim Austritt, siehe unten.

## Ausgänge und Eingang des Loop-Expanders

| Jack | Art | Bedeutung |
|---|---|---|
| `LOOP_COUNT_POLY_INPUT` | Poly-CV, 16 Kanäle = Stage 1-16 | Zähler je Segment, am Kanal der Last-Stage |
| `LOOP_EXIT_A/B_OUTPUT` | Trigger, 1 ms | feuert beim Austritt (nach N Durchläufen), unabhängig von `MOD_MUTE` (struktureller Marker wie EOS). Feuert auch im Ein-Segment-Fall nach N Durchläufen (wirkt dort als Taktteiler durch N, der Ablauf ändert sich nicht). |
| `LOOP_REST_A/B_OUTPUT` | CV, 1 V pro Durchlauf | Vorschlag: verbleibende Durchläufe einschließlich des laufenden (erster Durchlauf = N V, letzter = 1 V). 0 V = endlos oder Segment ohne Zähler. Eindeutig, da 0 V nie "fertig" bedeutet. Maximal 9 V. |

Der CV-Ausgang `LOOP_REST_A/B` blinkt bewusst nicht: 0 V bedeutet dort "endlos oder kein Zähler", ein Blinken würde jeden stören, der den Wert als CV nutzt. "Fertig" meldet der Austrittsimpuls.

## Panel-Anzeige (Entscheidung Anwender, 2026-10-30)

Je Section eine kleine Ziffernanzeige auf dem Expander-Panel (Muster: `TempoBpmDisplay`, reines Text-Widget ohne Interaktion, das Werte aus dem Modul liest):

- Zeigt die verbleibenden Durchläufe einschließlich des laufenden, wie `LOOP_REST` (im letzten Durchlauf steht dort 1).
- Beim Austritt blinkt eine 0 für etwa 0,3 s, danach steht der Wert des neu geladenen Segments da.
- Bei endlos (kein Zähler) zeigt die Anzeige "–".
- Dauerhaft 0 wird nie angezeigt, da der Zähler beim Austritt sofort neu geladen wird.
- Technik: Das Modul hält pro Section einen Blink-Timer, der beim Austritt gesetzt wird; das Widget liest ihn nur (keine GUI-Zugriffe aus dem Echtzeit-Thread). Die Anzeige wird dezimiert aktualisiert, nicht audio-rate.

Panelgröße, Anordnung und Modulname sind in der Panel-Session festzulegen (Namensschema: Anguissola-Familie).

## Hook-Stellen (unverändert zum Konzept)

- Vier Wrap-Stellen in `Afg::advanceStage()` (Hauptzweig und Skip-Schleife, je Forward und Reverse): "Zähler > 1: dekrementieren und wrappen wie bisher, sonst Austritt".
- `onStageEntered(prevStage, currentStage, forceReload)` am Ende von `advanceStage()` und `jumpToStage()`, vor `resetPulseEdgeState()`. Reset setzt `forceReload`.
- `Afg::process()` bekommt `stageLoopCount[NUM_STAGES]` (0 = endlos) sowie die Ausgabeparameter `outLoopExit` und `outLoopRest`, analog zu den Ratchet-Parametern und `outEos`.
- Neuer `PulseGenerator loopExitGen` pro `Afg`.
- Expander-Erkennung in `Marf248::process()`: weiterer `dynamic_cast` links und rechts neben den vorhandenen für Amilcare/Bianca.

## Weitere Vorgaben

- `onReset()` und Global Clear setzen den Zähler zurück, persistiert wird nur die Konfiguration (und die kommt aus dem Kabel, nicht aus dem Patch).
- Segment nur aus Skip-Stages: Schutz gegen endloses Zählen (Skip-Schleife ist bereits auf `NUM_STAGES` gedeckelt, der Zähler zählt nur Wrap-Ereignisse).
- Pendulum: Zähler inaktiv, Verhalten wie bisher.
- Bitgleichheit ohne gepatchten Loop-Eingang per Testprogramm nachweisen (wie bei den früheren Erweiterungen).

## Session-Plan (ein Feature pro Session)

1. **Engine:** Zähler, Hooks, Austritt, Restwert in `Marf248.hpp`; Testprogramm: Bitgleichheit ohne Zähler, Forward/Reverse, Skip, Array-Kante, Strobe/Recall/Reset/Continuous.
2. **Expander-Modul:** Panel, Jacks, Ziffernanzeige mit Blink-Timer, Andock-Erkennung beidseitig, Anbindung an `Marf248::process()`, Registrierung in `plugin.cpp`/`plugin.json`. Die Engine liefert dafür den Restwert und das Austrittsereignis (Session 1).
3. **Rack-Tests** (Testliste aus dem Konzept, Test L1 ff.).
4. **Dokumentation** DE/EN (Architektur-Überblick, Abschnitt Expander, Fallstricke, Design-Entscheidungen).
5. **CV2** (zweite Spannung je Stage) bleibt ein eigenes, späteres Feature.

## Noch offen

- Modulname und Panelgröße.
- Restwert-Definition (Vorschlag oben) bestätigen.
- Ob Zähler-Konfiguration später zusätzlich per Regler im Expander möglich sein soll (nicht Teil von Version 1).
