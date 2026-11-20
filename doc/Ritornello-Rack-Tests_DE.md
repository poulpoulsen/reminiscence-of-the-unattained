# Ritornello – Rack-Testliste (Session 3)

Stand: 2026-11-02, Branch `feature/loop-expander`. Maßgeblich: `doc/Expander-Konzept-Loop-Nachtrag_DE.md`.
Erwartungen unten sind aus der Engine-Logik abgeleitet, nicht gemessen – eine Abweichung ist ein Befund, kein Testfehler.

Hinweis: Die Erwartungswerte in Abschnitt 3, 5, 7, 8 und 9 gelten für die alte Zählung
(Spannung = Durchläufe). Seit 2026-11-02 gilt: Spannung = Wiederholungen,
Durchläufe = floor(V) + 1.

## 0. Aufbau (einmal)

- Sofonisba, Ritornello direkt daneben (rechts oder links). Amilcare nur auf die **andere** Seite, wenn gebraucht; Bianca passt gleichzeitig nicht (bekanntes Nachbar-Problem, nicht Teil dieser Tests).
- Section A läuft intern (Start/Stop-Taster), Stages 1–6 im Zyklus (First = 1, Last = 6), Slope aus, Stage-Adresse Int.
- Jede Stage soll am Ausgang unterscheidbar sein: **Auto-Fill** (lineare Spannungen 0–10 V) genügt, dann ist die Stage an VOUT A ablesbar (Scope oder Voltmeter).
- Stage-Zeiten ca. **0,6–1 s**, damit die 0,3-s-Blink-0 sichtbar ist.
- Poly-Quellen: ein Merge-Modul mit festen Spannungen je Kanal (Kanal i = Stage i). Gebraucht: Kabel für Begin, End, Count.
- EXIT A (1-ms-Impuls) am besten auf Scope im Single-Shot oder auf einen Trigger-Zähler legen. REST A auf Voltmeter/Scope.
- Notation: „B2“ = Begin-Gate (10 V) auf Kanal 2, „E3“ = End-Gate auf Kanal 3, „C3 = 2,5 V“ = Count-Spannung auf Kanal 3.

## 1. Andocken und Ruhezustand

- [x] Ritornello erscheint im Browser als „Ritornello“.
- [x] LINK-LED an, wenn neben Sofonisba (links **und** rechts getestet).
- [x] LINK-LED aus, wenn weggeschoben; EXIT/REST dabei 0 V.
- [x] Ohne Kabel: beide Anzeigen „–“, REST 0 V, EXIT still, Stage-Folge 1 2 3 4 5 6 wie ohne Ritornello.
- [x] Lucia (Kompaktversion) statt Sofonisba: LINK-LED geht an.
- [x] Patch speichern und neu laden: Ritornello bleibt, keine Fehlermeldung im Log.

## 2. Einfache Schleife

Einstellung: B2, E3, C3 = 2,5 V.

- [x] Stage-Folge: **1 2 3 2 3 4 5 6 1 …**
- [x] REST A: Stage 2/3 im ersten Durchlauf **2 V**, im zweiten **1 V**, auf Stage 4–6 **0 V**.
- [x] Anzeige A: 2 → 1 → beim Austritt Blink-**0** (ca. 0,3 s) → danach „–“.
- [x] EXIT A: genau **ein** Impuls beim Verlassen von Stage 3 im zweiten Durchlauf, sonst keiner.
- [x] Nächster Zyklus: Schleife läuft wieder voll (Zähler neu geladen).

## 3. Count-Bereich (an der Einfachschleife aus 2.)

| C3 | Erwartung |
|---|---|
| nicht gepatcht | endlos, Anzeige „–“, REST 0 V, kein EXIT |
| 0,9 V | endlos (floor = 0) |
| 1,0 V | 1 Durchlauf: **1 2 3 4 …** (keine Wiederholung), REST 1 V, EXIT feuert trotzdem |
| 1,9 V | 1 Durchlauf |
| 2,0 V | 2 Durchläufe |
| 5,5 V | 5 Durchläufe, REST zählt 5 4 3 2 1 |
| 9,0 V | 9 Durchläufe |
| 10 V | 9 Durchläufe (begrenzt) |
| −1 V | endlos |

- [x] Alle Zeilen geprüft.
- [x] Wert wird beim **Laden** des Zählers gelesen: C3 während der Schleife ändern wirkt erst im nächsten Zyklus.
- [x] Mono-Kabel (3 V) statt Poly an Count: wirkt nur auf Stage 1 → die Schleife 2–3 bleibt **endlos**.

## 4. Begin/End-Gates

- [x] Gate-Schwelle: B2 mit 1,5 V → keine Schleife; mit 2,0 V → Schleife.
- [x] Nur Begin gepatcht (End-Buchse leer): **keine** Schleife.
- [x] Nur End gepatcht (Begin-Buchse leer): **keine** Schleife.
- [x] Begin und End beide gepatcht, aber B2 ohne passendes End (E fehlt): Begin wird ignoriert, Folge linear.
- [x] End ohne vorheriges Begin (nur E3): ignoriert.
- [x] Ein-Stage-Schleife: B3 **und** E3 auf derselben Stage, C3 = 3,5 V → **1 2 3 3 3 4 5 6**, REST 3 → 2 → 1.

## 5. Verschachtelung
Einstellung: B2, B3, E4, E5; C4 = 2,5 V (innen), C5 = 2,5 V (außen).

- [x] Stage-Folge: **1 2 3 4 3 4 5 2 3 4 3 4 5 6 1 …**
- [x] Innere Schleife läuft in **jedem** Durchgang der äußeren wieder voll (zweimal 3-4).
- [x] REST/Anzeige zeigt die **innerste** Schleife: Stage 2 und 5 → äußerer Zähler (2, dann 1); Stage 3 und 4 → innerer Zähler (2, dann 1).
- [x] EXIT A: **3 Impulse** pro Zyklus (innen am Ende jedes Außendurchgangs = 2, außen = 1).
- [x] Zähler getrennt: C4 = 1,5 V, C5 = 3,5 V → innen 1 Durchlauf, außen 3 Durchläufe.
- [x] Klammerung: B2, B3, E4, E5 ist verschachtelt (3-4 innen); B2, E3, B4, E5 sind zwei getrennte Schleifen.

## 6. Section A und B getrennt

- [x] Beide Sections laufen (B z. B. mit anderer Zeit oder anderem Start), gleiche Gates/Counts.
- [x] Zählerstände und Anzeigen A und B laufen **unabhängig** voneinander.
- [x] EXIT B / REST B / Anzeige B verhalten sich wie A (jeweils eigene Impulse).
- [x] Eine Section stoppen: die andere läuft weiter, ihre Anzeige bleibt konsistent.

## 7. Strobe, Recall, Reset (Zustandsregel)

Einfachschleife aus 2. mit C3 = 3,5 V.

- [x] **Strobe hinein** (Adressregler auf Stage 2, Strobe): Zähler voll (REST 3).
- [x] Strobe **innerhalb** der Schleife (von Stage 2 auf 3 oder umgekehrt): Zähler läuft weiter (kein Nachladen).
- [x] Strobe/Recall **heraus** (auf Stage 5) und wieder hinein: Zähler wieder voll.
- [x] Recall aus der Schleife (Amilcare auf der anderen Seite): wie Strobe heraus.
- [x] Reset (Panel oder Eingang): Zähler lädt voll neu, auch innerhalb derselben Schleife.

## 8. Skip, Mute

- [x] End-Stage mit Skip (E3, Stage 3 Skip), C3 = 2,5 V: Folge laut Nachtrag **1 2 2 4 5 6**. (Übersprungene End-Stage zählt beim Verlassen.)
- [x] Begin-Stage mit Skip: Sprung landet dort und läuft sofort weiter; kein Hänger.
- [x] Schleife nur aus Skip-Stages: Sofonisba hängt nicht (Skip-Deckel).
- [x] Mute auf der End-Stage: EXIT feuert trotzdem.

## 9. Richtung

- [x] **Reverse** (B2, E4, C4 = 2,5 V, Lauf 6 → 1): Folge **6 5 4 3 2 4 3 2 1**; Verlassen der Begin-Stage 2 springt zur End-Stage 4, Zähler gehört zur End-Stage. *(offener Punkt aus dem Nachtrag – bitte notieren, ob es so klingt/läuft wie erwartet)*
- [x] **Pendulum**: Schleifen inaktiv, REST 0 V, Anzeige „–“, kein EXIT.
- [x] Zurück auf Cycle: Schleifen wieder aktiv.

## 10. Abdocken und Bitgleichheit

- [x] Ritornello im Lauf wegschieben: Stage-Folge wird linear, EXIT/REST 0 V, Anzeige „–“, nichts hängt.
- [x] Wieder andocken: Schleife läuft ohne alten Blink oder hängenden Wert wieder an.
- [x] Ritornello **mit** angedocktem Modul, aber **ohne Kabel**: Stage-Folge, Zeiten und Pulse identisch zum Patch **ohne** Ritornello (A/B-Vergleich am Scope über mehrere Zyklen).
- [x] Nur Begin **oder** nur End gepatcht: ebenfalls identisch zum Patch ohne Ritornello.

## 11. Anzeige

- [x] Blink-0 dauert etwa 0,3 s, danach Wert des neuen Zustands.
- [x] Beim Austritt einer **inneren** Schleife blinkt die 0 ebenfalls (jeder Austritt, laut Nachtrag aktuell gewollt).
- [x] Mehrfaches schnelles Austreten (kurze Stage-Zeiten): Anzeige flackert nicht kaputt, EXIT-Impulse bleiben vollständig.
- [x] Endlos und keine Schleife unterscheiden sich in der Anzeige nicht (beide „–“) – so beabsichtigt.

## Rückmeldung

Bei Abweichungen bitte: Nummer des Tests, Gate/Count-Einstellung, **beobachtete Stage-Folge** (z. B. „1 2 3 4 …“) und was die Anzeige/REST/EXIT dabei taten.

Offene Punkte aus dem Nachtrag, die dieser Test entscheiden hilft: Reverse-Verhalten (Abschnitt 9), EXIT nur für die äußerste Schleife statt für jede (Abschnitt 5).
