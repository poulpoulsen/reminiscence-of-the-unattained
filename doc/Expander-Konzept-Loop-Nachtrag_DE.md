# Loop-Expander: Nachtrag zum Konzept (Stand 2026-10-30, nach Session 1b)

Ergänzt `Expander-Konzept-Loop-CV2_DE.md` (Entwurf vom 28.10.). Dieser Nachtrag hält die getroffenen Entscheidungen und den Stand der Engine fest. Wo er vom Entwurf abweicht, gilt der Nachtrag. Er ersetzt die offenen Entscheidungen 3 bis 5 des Entwurfs (Zonen, Reverse/Pendulum, Strobe/Recall/Reset) sowie die Annahme zur Begin/End-Zuordnung in Abschnitt 3.1.

## Entscheidungen (Anwender)

1. **Eigenes Expander-Modul von Anfang an.** Kein Platz in Amilcare Spalte G. Der Loop-Expander ist wie Amilcare/Bianca ein reiner Jack-Träger ohne eigene DSP; die Logik liegt in `Afg`/`Marf248::process()`. Zum Testen dockt nur dieser eine Expander an Sofonisba. Der gleichzeitige Betrieb mit Amilcare und Bianca (Nachbar-Problem, Entwurf Abschnitt 4) bleibt offen und wird nicht in diesem Feature gelöst.
2. **Begin und End sind eigene Markierungen, unabhängig von First/Last.** Start-/End-of-Cycle-Trigger und BEATS_FACTOR-Segmente bleiben unberührt.
3. **Verschachtelung ist Teil von Version 1** (Original-Feature der Hardware). Jede End-Stage hat ihren eigenen Zähler.
4. **Wertebereich:** 1 V pro Zone. 0–1 V = endlos, 1–2 V = 1 Durchlauf … 9–10 V = 9 Durchläufe. Nicht gepatchte Kanäle liefern 0 V = endlos. Der Wert wird beim Laden des Zählers gelesen.
5. **Konfiguration an der End-Stage** der Schleife, Poly-Kanal = Stage-Nummer (1–16), gemeinsam für Section A und B (getrennte Zählerstände).
6. **Reverse:** Begin und End tauschen die Rollen. Verlassen wird die Begin-Stage, der Sprung geht zur End-Stage, der Zähler bleibt der der End-Stage.
7. **Strobe, Recall, Reset:** Strobe springt typischerweise in eine Schleife hinein, Recall aus ihr heraus. Daraus folgt die Zustandsregel unten.
8. **Restwert und Anzeige zeigen die innerste Schleife**, die die aktuelle Stage enthält.
9. **Austrittsimpuls UND Restanzeige** (CV-Ausgang und Panel-Ziffernanzeige mit Blink-0) bleiben in Version 1.
10. **Pendulum:** Schleifen inaktiv (Version 1).
11. **Modulname: Ritornello** (Musikbegriff für die wiederkehrende Passage). Das Namensschema der Anguissola-Familie ist ausgeschöpft und wird hier bewusst verlassen. Intern bleibt der Arbeitsname `Marf248LoopExpander` für Klasse, Dateien und Jack-Enums (Muster `Marf248TriggerExpander`), der Anzeigename im Panel, in `plugin.json` und in der Doku lautet Ritornello.

## Verhalten der Engine (Session 1b, umgesetzt und getestet)

- **Schleife:** Eine Begin- und eine End-Stage. Beim Verlassen der End-Stage (Forward) springt die Sektion zur Begin-Stage und zählt herunter. Beim letzten Durchlauf läuft sie normal weiter (Austritt). Zähler 0 springt immer zurück (endlos). Läuft die End-Stage zugleich als Last, greift nach dem Austritt der normale First/Last-Wrap.
- **Paarung per Klammerung** (Abweichung vom Entwurf, der "nächstgelegener Begin davor" annahm; das bricht bei Verschachtelung): Jedes End schließt das zuletzt geöffnete, noch offene Begin. Ohne Verschachtelung ist das identisch. Eine Stage mit Begin UND End ist eine Ein-Stage-Schleife. Unpaarige Begin/End werden ignoriert. Schleifen mit gleicher Begin- oder End-Stage gibt es nicht (ein Flag je Art und Stage).
- **Zustandsregel:** Der Zähler einer Schleife ist nur aktiv, solange die aktuelle Stage in [Begin..End] liegt. Bei jedem Stage-Eintritt (auch Strobe, Recall, Reset, Continuous) verfallen die Zähler aller Schleifen, deren Bereich die neue Stage nicht enthält. Sie laden beim nächsten Bedarf voll neu. Strobe hinein = voller Zähler, Recall heraus = Zähler verfällt, Sprung innerhalb der Schleife = Zähler läuft weiter. Reset lädt alles neu, auch innerhalb derselben Schleife. Die innere Schleife läuft so in jedem Durchgang der äußeren wieder voll ab.
- **Skip:** Eine übersprungene End-Stage zählt beim Verlassen. Ist die Begin-Stage Skip, landet der Sprung dort und läuft weiter. Das Überspringen ist auf `NUM_STAGES` Schritte gedeckelt, eine Endlosschleife nur aus Skip-Stages hängt nicht.
- **Restwert** (`outLoopRest`): verbleibende Durchläufe der innersten Schleife einschließlich des laufenden, 1 V pro Durchlauf (erster Durchlauf = N, letzter = 1), maximal 9 V. 0 V = keine Schleife, endlos oder Pendulum. Der CV-Ausgang blinkt bewusst nicht.
- **Austrittsimpuls** (`outLoopExit`): 1 ms, feuert bei jedem regulären Austritt einer Schleife (auch bei inneren), unabhängig von `MOD_MUTE` (struktureller Marker wie EOS).
- **Schnittstelle:** `Afg::process()` bekommt `stageLoopFlags[16]` (`LOOPFLAG_BEGIN`, `LOOPFLAG_END`) und `stageLoopCount[16]` (0 = endlos, 1–9) sowie `outLoopExit` und `outLoopRest`. Hooks: Verlassen der End-Stage bzw. Begin-Stage (Reverse) in `advanceStage()` inklusive Skip-Schleife, `onStageEntered()` am Ende von `advanceStage()` und `jumpToStage()`. Ohne Flags ist das Verhalten bitgleich zum Stand davor (Test gegen die vorherige Engine).
- **Stand in `Marf248.cpp`:** Bis der Expander angebunden ist, werden Nullen-Arrays übergeben und die Ausgänge nicht verdrahtet. `performGlobalClear()` setzt die Zähler zurück, `onReset()` erzeugt die Engine ohnehin neu.

## Panel-Anzeige (Entscheidung Anwender)

Je Section eine kleine Ziffernanzeige auf dem Expander-Panel (Muster `TempoBpmDisplay`, reines Text-Widget):

- Zeigt den Restwert der innersten Schleife wie `outLoopRest`.
- Beim Austritt blinkt eine 0 für etwa 0,3 s, danach steht der Wert des neuen Zustands da.
- Bei endlos oder ohne Schleife zeigt die Anzeige "–".
- Technik: Das Modul hält pro Section einen Blink-Timer, der beim Austritt gesetzt wird. Das Widget liest ihn nur (keine GUI-Zugriffe aus dem Echtzeit-Thread). Die Anzeige wird dezimiert aktualisiert.

## Ausgänge und Eingänge des Loop-Expanders (Entwurf für Session 2)

| Jack | Art | Bedeutung |
|---|---|---|
| `LOOP_BEGIN_POLY_INPUT` | Poly-Gate, 16 Kanäle = Stage 1–16 | markiert die Begin-Stage einer Schleife |
| `LOOP_END_POLY_INPUT` | Poly-Gate, 16 Kanäle = Stage 1–16 | markiert die End-Stage einer Schleife |
| `LOOP_COUNT_POLY_INPUT` | Poly-CV, 16 Kanäle = Stage 1–16 | Zähler, am Kanal der End-Stage (1 V pro Zone) |
| `LOOP_EXIT_A/B_OUTPUT` | Trigger, 1 ms | Austritt einer Schleife |
| `LOOP_REST_A/B_OUTPUT` | CV, 1 V pro Durchlauf | Restwert der innersten Schleife |

Die Jack-Namen sind Vorschläge. Panelgröße und Anordnung sind in der Panel-Session festzulegen.

## Session-Plan (ein Feature pro Session)

1. **Engine (erledigt, 1b):** Schleifen, Zähler, Verschachtelung, Austritt, Restwert in `Marf248.hpp`, Standalone-Tests (Szenarien, Bitgleichheit).
2. **Expander-Modul:** Panel, Jacks, Ziffernanzeige mit Blink-Timer, Andock-Erkennung beidseitig, Anbindung an `Marf248::process()`, Registrierung in `plugin.cpp`/`plugin.json`.
3. **Rack-Tests** durch Anwender.
4. **Dokumentation** DE/EN.
5. **Sprünge, CV 2, Zeit-Quantisierung, Taktteilung, Fix** bleiben eigene, spätere Features (Entwurf Abschnitte 3.2–3.5).

## Noch offen

- Panelgröße.
- Wie die Begin/End-Gates und der Zähler-Poly im Panel angeordnet werden und ob sie auf einer Seite zusammenliegen.
- Ob die Zähler-Konfiguration später zusätzlich per Regler im Expander möglich sein soll (nicht Teil von Version 1).
- Ob der Austrittsimpuls bei verschachtelten Schleifen nur für die äußerste Schleife feuern soll (aktuell: jede).
- Ob Reverse-Schleifen mit Begin/End-Rollentausch in Rack wie erwartet klingen (Rack-Test).
