# Expander-Konzept: Loop-Zähler, Sprünge, zweite Spannung, Taktteilung und Fix

Konzeptentwurf für einen weiteren Expander zu Sofonisba (und Lucia) im Plugin **Kinovice reminiscence of the unattained**. Stand: 2026-10-28. Status: **Entwurf, noch keine Umsetzung.** Alle Festlegungen unten sind Vorschläge, die Entscheidungen stehen in Abschnitt 7.

Das Plugin ist ein eigenständiges Werk, stark inspiriert von der wunderbaren Buchla-Modulwelt. Es steht in keiner Verbindung zu Buchla, Tiptop Audio oder einem anderen Hersteller. Quelle der Idee ist die Referenzdokumentation eines 16-stufigen Funktionsgenerators samt öffentlich zugänglichen Anwendernotizen. Wörtliche Zitate und Modellbezeichnungen stehen bewusst nicht in diesem Dokument; die Umsetzung ist eigenständig.

---

## 1. Anlass und Ziel

Ein Abgleich der Funktionsliste dieses Funktionsgenerators mit Sofonisba ergab: Sofonisba deckt fast alles ab (Stages mit Spannung und Zeit, externe Quellen A–D, Sustain/Enable/Stop, zwei Pulse je Stage, Stage-Adressierung, Slope, Presets, Quantisierung, Zeitbereiche, Tempo). Es fehlen im Kern fünf Dinge:

- Schleifen mit **Zähler** und Verschachtelung
- **Sprünge** zu einer beliebigen Stage, optional mit Wahrscheinlichkeit
- eine **zweite Spannung** je Stage
- **Zeit-Quantisierung** und **Taktteilung** je Stage
- **Fix**: einen externen Wert als Reglerwert einfrieren

Ziel ist ein optionaler Expander, der diese Lücken schließt. Er soll folgende Eigenschaften haben:

- Ohne angedockten Expander verhält sich Sofonisba **bitgleich** wie bisher.
- Der Expander hat selbst keine DSP-Logik. Die Logik liegt in einer rack-freien Engine, die Sofonisba aufruft (Muster wie Europa und Anna-Maria).
- Die Entwicklung läuft in kleinen Schritten, ein Feature je Sitzung, jeweils mit Standalone-Test.

## 2. Abgrenzung

**Bereits vorhanden und nicht Teil des Konzepts**<br>
Externe Quellen je Stage (`MOD_VOLT_EXT`/`MOD_TIME_EXT`), Stop/Sustain/Enable, Advance je Sektion, Stage-Adressierung mit Strobe/Continuous/Reset/Freeze, Address Memory, zwei programmierte Pulse plus APOUT und EOS, Slope mit Kurvenformen, Presets, Scale/Key, vier Zeitbereiche, Tempo und BEATS_FACTOR. Eine "Stage wartet auf einen Puls" ist durch Stop plus Start-Trigger abgedeckt.

**Bewusst nicht enthalten**<br>
Der Offset-Regler der Referenz, der zur Adress-CV addiert wird (Sofonisba ersetzt die Adresse bei Ext). Ein Vollbau der Display- und Edit-Bedienung des Originals. Beides lässt sich später ergänzen, ist aber nicht Teil dieses Konzepts.

## 3. Funktionsumfang

### 3.1 Loop-Zähler

Bisher loopt ein First/Last-Segment endlos. Neu: Eine Schleife wiederholt sich N-mal und läuft danach weiter.

**Definition**<br>
Eine Schleife besteht aus einer **Begin-Stage** und einer **End-Stage**. An der End-Stage steht ein Zähler N (1–9 oder endlos). Verlässt eine Sektion die End-Stage und ist der Restzähler größer als 0, springt sie zur Begin-Stage und zählt herunter. Ist er 0, läuft sie normal zur nächsten Stage weiter und der Zähler wird für den nächsten Durchlauf neu geladen. "Endlos" springt immer zurück.

**Verschachtelung**<br>
Schleifen dürfen ineinander liegen. Jede End-Stage hat ihren eigenen Zähler. Weil ein Zähler beim Verlassen der Schleife neu geladen wird, läuft die innere Schleife in jedem Durchgang der äußeren wieder voll ab.

**Zuordnung Begin und End**<br>
Zu einer End-Stage gehört die nächstgelegene Begin-Stage davor (Annahme, siehe Abschnitt 6). Begin und End sind eigene Markierungen und unabhängig von `MOD_FIRST`/`MOD_LAST`, damit die Start-/End-of-Cycle-Trigger und die BEATS_FACTOR-Segmente unberührt bleiben.

**Zähler pro Sektion**<br>
Die Programmierung ist modulweit geteilt, die Zählerstände laufen je Sektion (A/B) getrennt, wie `currentStage`.

**Wechselwirkung**<br>
Skip, Stop/Sustain/Enable und Advance behalten ihre Vorrangregeln. Reset lädt alle Zähler neu. Bei Continuous-Adressierung hat die Loop-Logik keine Wirkung, denn dort bestimmt die Adresse die Stage. Pendulum und Reverse sind offen (Abschnitt 7).

### 3.2 Sprung zu einer Stage

**Definition**<br>
Je Stage ein optionales Sprungziel. Wird die Stage verlassen, springt die Sektion statt zur nächsten zu dieser Stage. Der Sprung läuft über `jumpToStage()`, damit APOUT und EOS wie bei jedem Stage-Eintritt feuern.

**Wahrscheinlichkeit**<br>
Optional je Stage eine Wahrscheinlichkeit von 1–100 %. Bei Misserfolg läuft die Sektion normal weiter. Der Zufall kommt aus dem Zufallsgenerator von Rack (`rack::random`).

**Reihenfolge mit Loops**<br>
Erst die Loop-Logik, dann der Sprung (Vorschlag): Ein Sprung an einer End-Stage greift erst, wenn die Schleife abgelaufen ist.

**Ansteuerung**<br>
Ein Poly-Gate je Stage schaltet den Sprung scharf. Das Ziel kommt als Poly-CV, 0–10 V entspricht Stage 1–16 linear (wie die Stage-Adresse). Die Wahrscheinlichkeit kommt als eigene Poly-CV, unbelegt bedeutet 100 %.

### 3.3 Zweite Spannung (CV 2)

**Definition**<br>
Je Stage eine zweite programmierte Spannung mit eigenem Ausgang je Sektion (`VOUT2_A/B`). Sie teilt Stage-Zeit und Stage-Wechsel mit CV 1. Slope, Kurvenform und Curve folgen den Einstellungen der Stage (Annahme). Range und Quantize sind eigene Optionen, Vorschlag: Range Full/Half, Quantize aus Scale/Key.

**Bedienung**<br>
Vier Varianten (Entscheidung in Abschnitt 7):

- **A: Poly-CV-Eingang.** Die 16 Werte kommen von außen. Kein Regler, geringster Aufwand, aber nicht als Preset speicherbar.
- **B: Ein Slot mit Auswahl nach dem Lucia-Muster.** Auswahl-Knob, Aktivieren-Taster, ein Satz Regler. Alle 16 Werte sind echte Rack-Params, speicherbar, Pickup/Catch wie bei den Stages.
- **C: 16 Schieberegler.** Am nächsten am Original, aber sehr breit.
- **D: B und A kombiniert.** Regler im Slot, eine gepatchte Poly-CV ersetzt den Regler (wie bei den anderen CV-Eingängen des Plugins).

### 3.4 Zeit-Quantisierung und Taktteilung je Stage

**Zeit-Quantisierung**<br>
Ein Poly-Gate je Stage rastet die aufgelöste Zeit auf binäre rhythmische Schritte ein (10 Werte von 0,01 s bis 5,12 s, jeweils verdoppelt). Die Rastung erfolgt logarithmisch zum nächsten Wert (Annahme).

**Taktteilung**<br>
Je Stage kann die Dauer an einen Takt gebunden werden. Ein Poly-CV wählt eine von sieben Teilungen (Zweiunddreißigstel bis Viertel, teils als Triole, wie in der Referenz beschrieben). Die Dauer ergibt sich aus dem Tempo der Sektion (`BPM`) und der gewählten Teilung. Ein eigener Takteingang ist dafür nicht nötig. Er wäre nur für Synchronisierung mit einer externen Uhr nötig (offen).

**Wechselwirkung mit BEATS_FACTOR**<br>
BEATS_FACTOR berechnet einen Multiplikator aus der Summe aller aufgelösten Stage-Zeiten. Taktgebundene Stages würden dabei mitskaliert und verlören ihre feste Dauer. Vorschlag: Taktgebundene Stages werden aus der Summe und aus dem Multiplikator ausgenommen (Entscheidung in Abschnitt 7).

### 3.5 Fix

**Definition**<br>
Ein globales Gate friert die externen Werte ein. Bei der steigenden Flanke wird für jede Stage mit externer Quelle der aktuelle Wert (Spannung und Zeit) übernommen und gespeichert. Solange das Gate high ist, benutzen diese Stages den gespeicherten Wert statt der Live-Spannung. Bei fallender Flanke kehren sie zur Live-Quelle zurück.

**Speicherung**<br>
Weil der Schieberegler im Ext-Modus die Kanalwahl A–D trägt, braucht Fix eigene Speicher (16 Spannungs- und 16 Zeitwerte). Ob sie mit dem Patch gespeichert werden, ist offen.

## 4. Architektur

**Neuer Expander statt Erweiterung von Amilcare**<br>
Amilcare hat in Spalte G nur noch fünf freie Plätze (Reihe 1, 2, 5, 6, 7). Der Bedarf liegt deutlich darüber: Loop Begin, Loop Count, Jump Enable, Jump Target, Jump Probability, Time Quantize, Clock Division, Fix, zwei CV-2-Ausgänge und gegebenenfalls CV-2-Eingänge. Ein eigenes Modul ist daher nötig.

**Das Dock-Problem**<br>
Sofonisba sucht heute nur den **direkten** linken und rechten Nachbarn. Amilcare und Bianca belegen damit beide Seiten. Ein dritter Expander hat keinen Platz. Vorschlag: Die Suche läuft beim Nachbarn weiter, sofern dieser selbst ein bekannter Expander ist (Sofonisba, Amilcare, Neuer Expander). Das ist eine kleine Änderung am Lookup in `Marf248::process()` und ändert nichts, solange kein dritter Expander steht. Sie ist die einzige Änderung am Kern außer den Hooks.

**Hooks im Kern**<br>
`Afg::advanceStage()` und `jumpToStage()` bekommen einen optionalen Zeiger auf eine Tabelle mit Loop- und Sprungdaten. Ist der Zeiger null, ist das Verhalten **bitgleich** zum heutigen Stand. Das wird mit einem Standalone-Test gegen die bisherige Engine geprüft (gleiches Verfahren wie bei Anna-Maria und V/OCT).

**Engine**<br>
Eine rack-freie Datei, vorläufig `LoopJump.hpp`, enthält Loop-Zähler, Sprungwahl, Zeit-Quantisierung und Fix-Logik. Standalone-Tests in `tests/` mit `assert()` und deutschen Kommentaren.

**Modul**<br>
Ein reiner Jack-Träger wie Amilcare und Bianca. Neue Eingänge hängen an das Ende der Eingangsliste, Params am Ende des Enums. `paramLayout` und bestehende Patches bleiben unberührt.

**Namensvorschlag**<br>
Der Bruder der Malerinnen hieß Asdrubale. Als Arbeitsname bietet sich das an, die Entscheidung liegt bei dir.

## 5. Reihenfolge der Umsetzung

Ein Feature je Sitzung, jeweils mit Test und Dokumentation (DE und EN gleichzeitig):

1. Engine für Loop-Zähler samt Standalone-Test
2. Engine für Sprünge samt Test
3. Hooks im Kern und Bitgleichheitstest mit Null-Zeiger
4. Expander-Modul mit Panel für Loop und Sprung, Dock-Suche
5. CV 2
6. Zeit-Quantisierung und Taktteilung
7. Fix
8. Kapitel 11 und Fallstricke-Abschnitt

## 6. Annahmen, die nicht belegt sind

- Die Referenz beschreibt die Loop-Logik nur knapp: Ob ein Zähler beim Verlassen neu geladen wird, ob beim Rückwärtslauf die Rollen von Begin und End getauscht werden und wie sich verschachtelte Zähler genau verhalten, steht dort nicht oder nur ungenau. Die Festlegungen in 3.1 sind Annahmen.
- Die Zuordnung "nächstgelegener Begin vor der End-Stage" ist eine eigene Wahl.
- Die Bereiche der Zeit- und Spannungsraster sind nur als Zahlenreihe bekannt, die genaue Rundungsregel ist offen.
- Wie die Referenz einen externen Wert beim Einfrieren speichert, ist nicht eindeutig beschrieben. Das Fix-Verhalten in 3.5 ist eine Interpretation.
- Die genaue Abbildung des Poly-CV-Bereichs auf Zähler und Teilungen (Zonen) ist offen.

## 7. Offene Entscheidungen

**1. Eigenes Modul und Namen**<br>
Neuer Expander (Empfehlung) oder Ausbau von Amilcare? Arbeitsname Asdrubale oder ein anderer?

**2. Dock-Suche über Nachbarn hinweg**<br>
Ist die Erweiterung des Lookups in `Marf248::process()` in Ordnung (bitgleich ohne dritten Expander)? Alternative: Das neue Modul belegt dieselbe Seite wie Bianca und Bianca zieht um.

**3. Loop-Zähler: Zonen**<br>
Zonen für 0 (aus), 1–9 und endlos über 0–10 V (11 Zonen zu je 0,91 V) oder andere Abbildung? Oder soll der Zähler auf dem Panel einstellbar sein (zum Beispiel im Lucia-Muster)?

**4. Loop-Verhalten bei Reverse und Pendulum**<br>
Rollen tauschen, ignorieren oder nur bei Forward erlauben?

**5. Zähler bei Strobe, Address-Memory-Recall und Reset**<br>
Reset lädt neu (Vorschlag). Sollen Strobe und Recall die Zähler ebenfalls neu laden oder behalten?

**6. Sprung mit Wahrscheinlichkeit**<br>
Mit (Vorschlag) oder nur deterministisch?

**7. CV 2: Bedienung**<br>
Variante A, B, C oder D aus Abschnitt 3.3? Meine Empfehlung ist D (Regler im Lucia-Muster, Poly-CV überschreibt).

**8. Zeit-Quantisierung und Taktteilung**<br>
Beides im ersten Wurf oder zunächst nur die Quantisierung? Wie wird mit BEATS_FACTOR umgegangen (Ausnahme der taktgebundenen Stages, Vorschlag)?

**9. Fix: Speicherung**<br>
Mit dem Patch speichern oder nur zur Laufzeit halten? Wirkt Fix auf Spannung und Zeit gemeinsam oder getrennt?

**10. Umfang des ersten Wurfs**<br>
Empfehlung: Schritte 1 bis 4 (Loop, Sprung, Hooks, Modul), die übrigen danach.

## 8. Test- und Bitgleichheitskonzept

- Engine-Tests ohne Rack für Loop (einfach, verschachtelt, endlos, Reset), Sprung (mit und ohne Wahrscheinlichkeit, feste Zufallsfolge), Zeit-Quantisierung (Randwerte), Fix (Flanken).
- Bitgleichheit der Kern-Engine mit Null-Zeiger über alle Modi, Abtastraten und Presets gegen die vorherige Fassung (Muster: 3 Mio. Samples, 0 abweichende Bytes).
- Funktionstest im Rack durch Kinovice nach jedem Schritt.
- Stabilität: Schleifen mit Zähler und Skip auf allen Stages, keine Endlosschleife innerhalb eines Samples (Begrenzung der Sprungketten pro Sample, Vorschlag: höchstens 16).
