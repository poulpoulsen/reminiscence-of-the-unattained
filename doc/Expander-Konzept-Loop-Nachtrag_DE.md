# Loop-Expander: Nachtrag zum Konzept (Stand 2026-11-02, nach Session 3)

Ergänzt `Expander-Konzept-Loop-CV2_DE.md` (Entwurf vom 28.10.). Dieser Nachtrag hält die getroffenen Entscheidungen und den Stand der Umsetzung fest. Wo er vom Entwurf abweicht, gilt der Nachtrag. Er ersetzt die offenen Entscheidungen 3 bis 5 des Entwurfs (Zonen, Reverse/Pendulum, Strobe/Recall/Reset) sowie die Annahme zur Begin/End-Zuordnung in Abschnitt 3.1.

## Entscheidungen (Anwender)

1. **Eigenes Expander-Modul von Anfang an.** Kein Platz in Amilcare Spalte G. Der Loop-Expander ist wie Amilcare/Bianca ein reiner Jack-Träger ohne eigene DSP; die Logik liegt in `Afg`/`Marf248::process()`. Zum Testen dockt nur dieser eine Expander an Sofonisba. Der gleichzeitige Betrieb mit Amilcare und Bianca (Nachbar-Problem, Entwurf Abschnitt 4) bleibt offen und wird nicht in diesem Feature gelöst. Amilcare und Ritornello auf verschiedenen Seiten von Sofonisba funktionieren.
2. **Begin und End sind eigene Markierungen, unabhängig von First/Last.** Start-/End-of-Cycle-Trigger und BEATS_FACTOR-Segmente bleiben unberührt.
3. **Verschachtelung ist Teil von Version 1** (Original-Feature der Hardware). Jede End-Stage hat ihren eigenen Zähler.
4. **Wertebereich (geändert am 2026-11-02, ersetzt „1 V pro Zone = Durchläufe“):** 1 V pro **Wiederholung**. 0–1 V = endlos, 1–2 V = 1 Wiederholung (= 2 Durchläufe), 2–3 V = 2 Wiederholungen (= 3 Durchläufe) … 9–10 V = 9 Wiederholungen (= 10 Durchläufe). Faustregel: Durchläufe = floor(V) + 1, Wiederholungen = floor(V). Nicht gepatchte Kanäle liefern 0 V = endlos. Negative Spannungen sind ebenfalls endlos.
   - Gelesen werden nur Kanäle bis `getChannels()`. Ein Mono-Kabel wirkt deshalb nur auf Stage 1 (kein Kopieren auf alle 16 Stages).
   - **Lesezeitpunkt:** Gezählte Schleifen lesen den Wert beim Laden des Zählers; eine Änderung während des Laufs wirkt erst beim nächsten Laden. **Endlose Schleifen lesen den Wert laufend nach** (Rack-Test 2026-11-02): Wird nachträglich ein Wert ≥ 1 V angelegt, wird er sofort übernommen, der laufende Durchlauf zählt als erster.
5. **Konfiguration an der End-Stage** der Schleife, Poly-Kanal = Stage-Nummer (1–16), gemeinsam für Section A und B (getrennte Zählerstände). Wird ein End verschoben, muss der Count-Wert mitwandern, sonst läuft die Schleife endlos.
6. **Reverse:** Begin und End tauschen die Rollen. Verlassen wird die Begin-Stage, der Sprung geht zur End-Stage, der Zähler bleibt der der End-Stage. **Im Rack-Test bestätigt und so belassen** (2026-11-02).
7. **Strobe, Recall, Reset:** Strobe springt typischerweise in eine Schleife hinein, Recall aus ihr heraus. Daraus folgt die Zustandsregel unten. Zum Neuladen eines endlosen Zählers genügt jetzt das Nachlesen (Entscheidung 4); ansonsten lädt Stage-Address-Reset oder das Verlassen der Schleife neu.
8. **Restwert und Anzeige zeigen die innerste Schleife**, die die aktuelle Stage enthält, in verbleibenden **Durchläufen** einschließlich des laufenden (letzter Durchlauf = 1). Beispiel: Stage 3 liegt in der inneren Schleife und zeigt deren Zähler, Stage 2 in der äußeren zeigt deren Zähler.
9. **Austrittsimpuls UND Restanzeige** (CV-Ausgang und Panel-Ziffernanzeige mit blinkender 0) bleiben in Version 1. **EXIT feuert bei jedem regulären Austritt, auch bei inneren Schleifen** (Entscheidung 2026-10-31: so gibt es immer eine Rückmeldung; ein eigener Ausgang für „Gesamtschleife fertig“ bleibt als spätere Erweiterung möglich).
10. **Pendulum:** Schleifen inaktiv (Version 1).
11. **Modulname: Ritornello** (Musikbegriff für die wiederkehrende Passage). Das Namensschema der Anguissola-Familie ist ausgeschöpft und wird hier bewusst verlassen. Intern bleibt der Arbeitsname `Marf248LoopExpander` für Klasse, Dateien und Jack-Enums (Muster `Marf248TriggerExpander`), der Anzeigename im Panel, in `plugin.json` und in der Doku lautet Ritornello.

## Verhalten der Engine (Session 1b, im Rack-Test 2026-11-02 bestätigt)

- **Schleife:** Eine Begin- und eine End-Stage. Beim Verlassen der End-Stage (Forward) springt die Sektion zur Begin-Stage und zählt herunter. Beim letzten Durchlauf läuft sie normal weiter (Austritt). Zähler 0 springt immer zurück (endlos). Läuft die End-Stage zugleich als Last, greift nach dem Austritt der normale First/Last-Wrap.
- **Beide Buchsen nötig:** Schleifen entstehen nur, wenn **Begin und End gepatcht** sind. Ist eine der beiden Buchsen leer, bleibt der Ablauf linear. Gate-Schwelle 2 V, ohne Hysterese (wie die Stage-Modifier-Gates von Amilcare).
- **Paarung per Klammerung** (Abweichung vom Entwurf, der „nächstgelegener Begin davor“ annahm; das bricht bei Verschachtelung): Jedes End schließt das zuletzt geöffnete, noch offene Begin. Ohne Verschachtelung ist das identisch. Eine Stage mit Begin UND End ist eine Ein-Stage-Schleife. Unpaarige Begin/End werden ignoriert. Schleifen mit gleicher Begin- oder End-Stage gibt es nicht (ein Flag je Art und Stage).
  - Beispiel: B2, B3, E4, E5 ist verschachtelt (3-4 innen in 2-5); B2, E3, B4, E5 sind zwei getrennte Schleifen (2-3 und 4-5).
- **Zustandsregel:** Der Zähler einer Schleife ist nur aktiv, solange die aktuelle Stage in [Begin..End] liegt. Bei jedem Stage-Eintritt (auch Strobe, Recall, Reset, Continuous) verfallen die Zähler aller Schleifen, deren Bereich die neue Stage nicht enthält. Sie laden beim nächsten Bedarf voll neu. Strobe hinein = voller Zähler, Recall heraus = Zähler verfällt, Sprung innerhalb der Schleife = Zähler läuft weiter. Reset lädt alles neu, auch innerhalb derselben Schleife. Die innere Schleife läuft so in jedem Durchgang der äußeren wieder voll ab.
- **Skip:** Eine übersprungene End-Stage zählt beim Verlassen; die Begin-Stage wiederholt sich dann entsprechend oft (Beispiel: E3 auf Skip, C3 = 3,5 V = 4 Durchläufe → `1 · 2 2 2 2 · 4 5 6`). Ist die Begin-Stage Skip, landet der Sprung dort und läuft weiter. Das Überspringen ist auf `NUM_STAGES` Schritte gedeckelt, eine Endlosschleife nur aus Skip-Stages hängt nicht.
- **Restwert** (`outLoopRest`): verbleibende Durchläufe der innersten Schleife einschließlich des laufenden, 1 V pro Durchlauf (erster Durchlauf = N, letzter = 1), **maximal 10 V**. 0 V = keine Schleife, endlos oder Pendulum. Der CV-Ausgang blinkt bewusst nicht.
- **Austrittsimpuls** (`outLoopExit`): 1 ms, feuert bei jedem regulären Austritt einer Schleife (auch bei inneren), unabhängig von `MOD_MUTE` (struktureller Marker wie EOS). Für Scope-Tests ist ein 1-ms-Impuls knapp; eine LED oder Hüllkurve ist zuverlässiger.
- **Schnittstelle:** `Afg::process()` bekommt `stageLoopFlags[16]` (`LOOPFLAG_BEGIN`, `LOOPFLAG_END`) und `stageLoopCount[16]` (0 = endlos, 1–10 **Durchläufe**; die Umrechnung von der Spannung geschieht in `Marf248::process()`) sowie `outLoopExit` und `outLoopRest`. Hooks: Verlassen der End-Stage bzw. Begin-Stage (Reverse) in `advanceStage()` inklusive Skip-Schleife, `onStageEntered()` am Ende von `advanceStage()` und `jumpToStage()`. Ohne Flags ist das Verhalten bitgleich zum Stand davor (Test gegen die vorherige Engine, im Rack bestätigt: ohne Kabel identisch).
- **Anbindung in `Marf248.cpp`:** `Marf248LoopExpander` wird links, dann rechts per `dynamic_cast` gesucht. Ohne angedocktes Modul oder ohne beide Kabel gehen Nullen-Arrays an die Engine. `performGlobalClear()` setzt die Zähler zurück, `onReset()` erzeugt die Engine ohnehin neu.

## Panel (Ritornello, 12 HP)

- Titel zentriert oben, LINK-Licht darunter (gleiche Funktion wie bei Amilcare/Bianca).
- Je Section eine Spalte (A links, B rechts): Ziffernanzeige, `REST` (CV), `EXIT` (Trigger).
- Nach einer Trennlinie die gemeinsamen Eingänge: `LOOP BEGIN` und `LOOP END` als Paar, `LOOP COUNT` mittig darunter.
- Beschriftung: Section-Farben für Ausgänge und Anzeige, Stage-Modifier-Farbe für die drei Eingänge.

## Panel-Anzeige (Entscheidung Anwender, umgesetzt)

Je Section eine Ziffernanzeige auf dem Expander-Panel (Muster `TempoBpmDisplay`, reines Text-Widget):

- Zeigt den Restwert der innersten Schleife wie `outLoopRest` (1 bis 10, zweistellig möglich).
- **Beim Austritt blinkt eine 0:** insgesamt 0,5 s, 0,1 s an und 0,1 s aus (an, aus, an, aus, an), danach steht der Wert des neuen Zustands da. Bei einem inneren Austritt ist das der Wert der äußeren Schleife, bei einem äußeren Austritt „–“.
- Bei endlos oder ohne Schleife zeigt die Anzeige „–“ (als Rechteck gezeichnet, nicht als Zeichen).
- Technik: Das Modul hält pro Section einen Blink-Timer, der bei der steigenden Flanke des Austrittsimpulses in `Marf248::process()` gesetzt und heruntergezählt wird. Das Widget liest ihn nur (keine GUI-Zugriffe aus dem Echtzeit-Thread). Konstanten `LOOP_BLINK_SECONDS` (0,5 s) und `LOOP_BLINK_PERIOD` (0,1 s) in `Marf248LoopExpander.hpp`; die Gesamtdauer sollte ein ungerades Vielfaches der Phase sein.

## Ausgänge und Eingänge des Loop-Expanders

| Jack | Art | Bedeutung |
|---|---|---|
| `LOOP_BEGIN_POLY_INPUT` | Poly-Gate, 16 Kanäle = Stage 1–16 | markiert die Begin-Stage einer Schleife |
| `LOOP_END_POLY_INPUT` | Poly-Gate, 16 Kanäle = Stage 1–16 | markiert die End-Stage einer Schleife |
| `LOOP_COUNT_POLY_INPUT` | Poly-CV, 16 Kanäle = Stage 1–16 | Zähler, am Kanal der End-Stage (1 V pro Wiederholung, 0 V = endlos) |
| `LOOP_EXIT_A/B_OUTPUT` | Trigger, 1 ms | Austritt einer Schleife (jede, auch innere) |
| `LOOP_REST_A/B_OUTPUT` | CV, 1 V pro Durchlauf | Restwert der innersten Schleife, max. 10 V |

## Session-Plan (ein Feature pro Session)

1. **Engine (erledigt, 1b):** Schleifen, Zähler, Verschachtelung, Austritt, Restwert in `Marf248.hpp`, Standalone-Tests (Szenarien, Bitgleichheit).
2. **Expander-Modul (erledigt):** Panel, Jacks, Ziffernanzeige mit Blink-Timer, Andock-Erkennung beidseitig, Anbindung an `Marf248::process()`, Registrierung in `plugin.cpp`/`plugin.json`. Nachträge: echtes Blinken, laufendes Nachlesen endloser Zähler, Wertebereich auf Wiederholungen umgestellt.
3. **Rack-Tests (erledigt 2026-11-02)** durch den Anwender, Testliste vollständig abgehakt. Hinweis: Die Erwartungswerte in den Abschnitten 3, 5, 7, 8 und 9 der Testliste stammen aus der alten Zählung (Spannung = Durchläufe); seit 2026-11-02 gilt Spannung = Wiederholungen.
4. **Dokumentation** DE/EN (nächster Schritt): von einem durchgehenden Beispiel ausgehen (B2, B3, E4, E5, innen 1,5 V, außen 3,5 V), mit REST-Tabelle je Stage, Count-Zuordnung, Neuladeregeln, Austritt/Blink, Sonderfälle (Strobe/Recall/Reset, Skip, Mute, Reverse, Pendulum) und Einbau/Nachbar-Hinweis.
5. **Sprünge, CV 2, Zeit-Quantisierung, Taktteilung, Fix** bleiben eigene, spätere Features (Entwurf Abschnitte 3.2–3.5).

## Noch offen

- Gleichzeitiger Betrieb von Amilcare, Bianca und Ritornello an einer Sofonisba (Nachbar-Problem), eigenes Feature.
- Ob die Zähler-Konfiguration später zusätzlich per Regler im Expander möglich sein soll (nicht Teil von Version 1).
- Ob ein zusätzlicher Ausgang „Gesamtschleife fertig“ (nur äußerste Schleife) sinnvoll ist (EXIT bleibt vorerst bei jedem Austritt).
- Zweistellige Anzeige „10“ (bei 9 V und mehr) im Rack ansehen, falls noch nicht geschehen.
