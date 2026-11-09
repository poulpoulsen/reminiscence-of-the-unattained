# Ritornello – Dokumentation

*Stand: 2026-11-09 · Loop-Expander für Sofonisba und Lucia · 12 HP · Plugin „Kinovice reminiscence of the unattained“*

> **Hinweis:** Das Plugin ist ein eigenständiges Werk, stark inspiriert von der wunderbaren Buchla-Welt der Module. Es steht in keiner Verbindung zu Buchla, Tiptop Audio oder einem anderen Hersteller. Alle Marken gehören ihren jeweiligen Inhabern. Die Umsetzung ist eigenständig; Quelle der Idee ist eine öffentlich zugängliche Referenzdokumentation.

<a id="ritornello-ueberblick"></a>

## 1. Überblick

Ritornello erweitert Sofonisba und Lucia um **Schleifen mit Zähler**. Bisher läuft ein First/Last-Segment endlos im Kreis. Mit Ritornello markierst du über Gate-Eingänge eine **Begin-Stage** und eine **End-Stage**. Verlässt die Sektion die End-Stage, springt sie zurück zur Begin-Stage und zählt dabei herunter. Nach dem letzten Durchlauf läuft sie normal weiter. Schleifen dürfen ineinander liegen.

Das Modul enthält keine eigene Klangerzeugung und keine eigene Logik. Wie Amilcare und Bianca ist es ein Träger für Buchsen und Anzeigen; die Schleifenlogik läuft in Sofonisba bzw. Lucia.

**Name**<br>
Ritornello ist der musikalische Begriff für die wiederkehrende Passage. Das Namensschema der Anguissola-Familie ist ausgeschöpft und wird hier bewusst verlassen.

**Was du bekommst**<br>
Drei Eingänge (Begin, End, Count), je Sektion eine Ziffernanzeige, ein Restwert-Ausgang (REST) und ein Austrittsimpuls (EXIT).

**Wenn nichts gepatcht ist**<br>
Ohne angedocktes Ritornello, oder solange nicht sowohl Begin als auch End gepatcht sind, verhält sich Sofonisba bitgleich wie ohne die Loop-Funktion.

<a id="ritornello-einbau"></a>

## 2. Einbau und Andocken

Ritornello dockt links oder rechts an Sofonisba oder Lucia an. Die LINK-Leuchte unter dem Modulnamen zeigt, ob ein Host erkannt wurde.

**Kette**<br>
Der Host sucht ab dem direkten Nachbarn weiter über andere Expander (Amilcare, Bianca, Ritornello), insgesamt bis zu drei Module je Seite. Ein Fremdmodul oder eine Lücke beendet die Kette.

**Mehrere Ritornello an einem Host**<br>
Genutzt wird nur das nächste, links vor rechts. Die weiter entfernten bleiben ohne Wirkung, ihre LINK-Leuchte bleibt aus.

**Ohne Verbindung**<br>
Trennst du Ritornello vom Host, setzt es Ausgänge, Anzeigen und LINK-Leuchte zurück.

<a id="ritornello-panel"></a>

## 3. Panel

Oben stehen Titel und LINK-Leuchte. Darunter folgt je Sektion eine Spalte (A links, B rechts), nach einer Trennlinie folgen die gemeinsamen Eingänge.

**Ziffernanzeige A/B**<br>
Zeigt den Restwert der innersten Schleife, die die aktuelle Stage der Sektion enthält, siehe [Abschnitt 7](#ritornello-anzeige) ([[#7. Restwert, Anzeige und Austritt|Obsidian]]).

**REST A/B**<br>
CV-Ausgang, 1 V pro verbleibendem Durchlauf, wie die Anzeige.

**EXIT A/B**<br>
Trigger-Ausgang (1 ms), feuert bei jedem regulären Austritt aus einer Schleife.

**LOOP BEGIN**<br>
Poly-Gate-Eingang, Kanal 1–16 = Stage 1–16. Ein High (ab 2 V) markiert die erste Stage einer Schleife.

**LOOP END**<br>
Poly-Gate-Eingang, Kanal 1–16 = Stage 1–16. Ein High (ab 2 V) markiert die letzte Stage einer Schleife.

**LOOP COUNT**<br>
Poly-CV-Eingang, Kanal 1–16 = Stage 1–16, wirksam am Kanal der End-Stage. 1 V pro Wiederholung, 0 V = endlos.

<a id="ritornello-schleifen"></a>

## 4. Schleifen: Begin und End

**Beide Buchsen nötig**<br>
Schleifen entstehen nur, wenn **LOOP BEGIN und LOOP END gepatcht** sind. Ist eine der beiden Buchsen leer, bleibt der Ablauf linear.

**Gate-Pegel**<br>
Schwelle 2 V, ohne Hysterese (wie bei den Stage-Modifier-Gates von Amilcare). Gelesen werden nur die Kanäle, die das Kabel tatsächlich trägt. Ein Mono-Kabel markiert deshalb nur Stage 1.

**Gemeinsam für A und B**<br>
Die Markierungen gelten für beide Sektionen, genau wie die Stage-Programmierung selbst. Die Zählerstände laufen je Sektion getrennt.

**Unabhängig von First und Last**<br>
Begin und End sind eigene Markierungen. Start-/End-of-Cycle-Trigger und BEATS_FACTOR-Segmente bleiben unberührt. Liegt die End-Stage zugleich auf Last, greift nach dem Austritt der normale First/Last-Wrap.

**Paarung per Klammerung**<br>
Jedes End schließt das zuletzt geöffnete, noch offene Begin. Daraus folgt:

- Begin 2, Begin 3, End 4, End 5: **verschachtelt**. Die innere Schleife umfasst Stage 3–4, die äußere Stage 2–5.
- Begin 2, End 3, Begin 4, End 5: **zwei getrennte** Schleifen (2–3 und 4–5).
- Begin und End auf derselben Stage: eine **Ein-Stage-Schleife**. Die Stage wiederholt sich.
- Unpaarige Begin- oder End-Markierungen werden ignoriert.
- Je Stage gibt es höchstens ein Begin und ein End. Zwei Schleifen mit derselben Begin- oder End-Stage sind daher nicht möglich.

<a id="ritornello-zaehler"></a>

## 5. Der Zähler

Der Zähler liegt an der **End-Stage** der Schleife: Der Count-Wert am Kanal der End-Stage gilt für die Schleife, die dort endet.

**Spannung = Wiederholungen**<br>
1 V entspricht einer Wiederholung. Eine Wiederholung bedeutet einen zusätzlichen Durchlauf.

| Spannung am Kanal der End-Stage | Wiederholungen | Durchläufe |
|---|---|---|
| unter 1 V (auch negativ, nicht gepatcht) | – | endlos |
| 1 V bis unter 2 V | 1 | 2 |
| 2 V bis unter 3 V | 2 | 3 |
| … | … | … |
| 9 V und mehr | 9 | 10 |

Faustregel: Durchläufe = floor(V) + 1, Wiederholungen = floor(V). Eine Schleife mit genau einem Durchlauf gibt es nicht; die kleinste gezählte Schleife läuft zweimal.

**Nicht gepatchter Count**<br>
Ist LOOP COUNT nicht gepatcht oder trägt das Kabel diesen Kanal nicht, ist die Schleife endlos. Begin und End allein ergeben also eine Endlosschleife.

**Mono-Kabel**<br>
Ein Mono-Kabel an LOOP COUNT wirkt nur auf Stage 1. Für mehrere Schleifen brauchst du ein Poly-Kabel mit mindestens so vielen Kanälen, wie die letzte End-Stage hoch liegt.

**Lesezeitpunkt**<br>
Gezählte Schleifen lesen den Wert, wenn der Zähler geladen wird. Eine Änderung während des Laufs wirkt erst beim nächsten Laden. **Endlose Schleifen lesen den Wert laufend nach.** Wird nachträglich ein Wert ab 1 V angelegt, gilt er sofort, und der gerade laufende Durchlauf zählt als erster. So kannst du eine endlose Schleife im Live-Betrieb in eine gezählte verwandeln.

**End-Stage verschoben**<br>
Verschiebst du ein End auf eine andere Stage, muss der Count-Wert mitwandern. Sonst liegt am neuen End-Kanal 0 V, und die Schleife läuft endlos.

<a id="ritornello-beispiel"></a>

## 6. Durchgehendes Beispiel

Aufbau: **Begin** auf Stage 2 und 3, **End** auf Stage 4 und 5. Count am Kanal 4 = **1,5 V** (innere Schleife, 2 Durchläufe) und am Kanal 5 = **3,5 V** (äußere Schleife, 4 Durchläufe). Die innere Schleife umfasst Stage 3–4, die äußere Stage 2–5.

**Ablauf der Stages**<br>
1, dann viermal die Folge 2 3 4 3 4 5, danach 6.

Die Tabelle zeigt, was Anzeige und REST-Ausgang dabei liefern. Das Zeichen „–“ steht für 0 V (keine Schleife).

| Äußerer Durchlauf | Stage 2 | Stage 3 | Stage 4 | Stage 3 | Stage 4 | Stage 5 |
|---|---|---|---|---|---|---|
| 1 | 4 | 2 | 2 | 1 | 1 | 4 |
| 2 | 3 | 2 | 2 | 1 | 1 | 3 |
| 3 | 2 | 2 | 2 | 1 | 1 | 2 |
| 4 | 1 | 2 | 2 | 1 | 1 | 1 |

Stage 1 und Stage 6 liegen in keiner Schleife und zeigen „–“.

**EXIT-Impulse**<br>
Beim Verlassen von Stage 4 im zweiten Durchlauf der inneren Schleife feuert EXIT, und zwar in **jedem** der vier äußeren Durchläufe. Beim Verlassen von Stage 5 im vierten Durchlauf feuert EXIT noch einmal für die äußere Schleife. Insgesamt sind das fünf Impulse.

**Innere Schleife lädt jedes Mal neu**<br>
Sobald die Sektion Stage 5 oder Stage 2 betritt, liegt sie außerhalb der inneren Schleife; deren Zähler verfällt und lädt beim nächsten Eintritt in Stage 3 wieder voll. Deshalb läuft die innere Schleife in jedem äußeren Durchlauf zweimal ab.

<a id="ritornello-anzeige"></a>

## 7. Restwert, Anzeige und Austritt

**Restwert**<br>
REST und Ziffernanzeige zeigen die **verbleibenden Durchläufe der innersten Schleife**, die die aktuelle Stage der Sektion enthält, **einschließlich des laufenden**. Beim ersten Durchlauf steht dort die Gesamtzahl, beim letzten eine 1. Der REST-Ausgang liefert 1 V pro Durchlauf, höchstens 10 V. 0 V bedeutet: keine Schleife, endlos oder Pendulum. Der CV-Ausgang blinkt bewusst nicht.

**Ziffernanzeige**<br>
Zeigt 1 bis 10. Bei endlos oder ohne Schleife steht dort ein „–“ (als Balken gezeichnet).

**Blinkende 0 beim Austritt**<br>
Verlässt die Sektion eine Schleife regulär, blinkt die Anzeige eine 0: 0,5 s lang, 0,1 s an und 0,1 s aus (an, aus, an, aus, an). Danach steht der Wert des neuen Zustands da. Nach dem Austritt aus einer inneren Schleife ist das der Wert der äußeren, nach dem Austritt aus der äußersten Schleife ein „–“.

**EXIT**<br>
Ein 1-ms-Trigger (10 V) bei **jedem** regulären Austritt, auch bei inneren Schleifen. So gibt es immer eine Rückmeldung. MOD_MUTE einer Stage unterdrückt EXIT nicht (struktureller Marker wie End of Stage). Ein eigener Ausgang für „Gesamtschleife fertig“ bleibt als spätere Erweiterung möglich.

**Scope-Hinweis**<br>
Ein 1-ms-Impuls ist am Scope knapp. Für Tests ist eine LED, eine Hüllkurve oder ein Gate-Verlängerer zuverlässiger.

<a id="ritornello-sonderfaelle"></a>

## 8. Zustandsregel und Sonderfälle

**Zustandsregel**<br>
Der Zähler einer Schleife ist nur aktiv, solange die aktuelle Stage zwischen Begin und End liegt. Bei jedem Stage-Eintritt, auch bei Strobe, Recall, Reset und Continuous, verfallen die Zähler aller Schleifen, deren Bereich die neue Stage nicht enthält. Sie laden beim nächsten Bedarf wieder voll.

**Strobe, Recall, Reset**<br>
Strobe in eine Schleife hinein startet mit vollem Zähler. Recall aus einer Schleife heraus lässt ihren Zähler verfallen. Ein Sprung innerhalb der Schleife lässt den Zähler weiterlaufen. Reset lädt alles neu, auch innerhalb derselben Schleife. Eine endlose Schleife nimmt einen neu angelegten Count dagegen schon durch das laufende Nachlesen auf, siehe [Abschnitt 5](#ritornello-zaehler) ([[#5. Der Zähler|Obsidian]]).

**Continuous**<br>
Bei Continuous-Adressierung bestimmt die Adresse die Stage. Die Sprünge der Schleifen greifen dort nicht, die Zustandsregel oben gilt aber weiter.

**Skip**<br>
Eine übersprungene End-Stage zählt beim Verlassen, die Begin-Stage wiederholt sich entsprechend oft. Beispiel: Stage 3 ist End und auf Skip, Begin liegt auf Stage 2, Count 3,5 V (4 Durchläufe): Die Folge ist 1, 2, 2, 2, 2, 4, 5, 6. Ist die Begin-Stage auf Skip, landet der Sprung dort und läuft sofort weiter. Eine Endlosschleife nur aus Skip-Stages hängt nicht, das Überspringen ist auf 16 Schritte begrenzt.

**Mute**<br>
Mute unterdrückt nur die Pulse P1/P2/APOUT der Stage. Schleifen und EXIT bleiben davon unberührt.

**Stop, Sustain, Enable, Advance**<br>
Ihre Vorrangregeln bleiben unverändert. Eine Schleife springt, wenn die End-Stage verlassen wird, egal ob durch Zeitablauf oder durch einen Advance-Trigger.

**Reverse**<br>
Begin und End tauschen die Rollen. Verlassen wird die Begin-Stage, der Sprung geht zur End-Stage, der Zähler bleibt der der End-Stage. Das Verhalten ist im Rack bestätigt und bleibt so.

**Pendulum**<br>
Schleifen sind bei Pendulum inaktiv. REST liefert 0 V, die Anzeige „–“.

**Global Clear**<br>
Global Clear setzt die Schleifenzähler zurück. Die Markierungen kommen von den Kabeln und bleiben davon unberührt.

**Presets**<br>
Begin, End und Count liegen nur an den Kabeln an. Sie werden weder in den Presets noch als Stage-Daten gespeichert; im Patch bleibt die Verkabelung.

<a id="ritornello-stolperfallen"></a>

## 9. Stolperfallen

**Schleife läuft endlos**<br>
Die häufigste Ursache: Count nicht gepatcht, ein Mono-Kabel an LOOP COUNT (wirkt nur auf Stage 1), Count am falschen Kanal (er gehört an die **End**-Stage) oder weniger als 1 V am Kanal. Siehe [Abschnitt 5](#ritornello-zaehler) ([[#5. Der Zähler|Obsidian]]).

**Es springt gar nichts**<br>
Begin und End müssen beide gepatcht sein. Eine fehlende Buchse lässt den Ablauf linear.

**Schleife springt zur falschen Stage**<br>
Die Paarung geschieht per Klammerung. Bei verschachtelten Schleifen schließt jedes End das zuletzt geöffnete Begin, siehe [Abschnitt 4](#ritornello-schleifen) ([[#4. Schleifen: Begin und End|Obsidian]]).

**Die Zahl bleibt trotz Count-Änderung stehen**<br>
Gezählte Schleifen lesen den Count beim Laden. Verlasse die Schleife oder löse einen Reset aus, damit der neue Wert gelesen wird.

**EXIT am Scope nicht zu sehen**<br>
Der Impuls dauert nur 1 ms.

**„Eine Wiederholung“ ist nicht „ein Durchlauf“**<br>
1 V bedeutet zwei Durchläufe. Wer die Anzeige mit dem Count vergleicht: Die Anzeige zählt Durchläufe, der Count Wiederholungen.

<a id="ritornello-technik"></a>

## 10. Technische Daten

| Eigenschaft | Wert |
|---|---|
| Breite | 12 HP |
| Gate-Schwelle (Begin, End) | 2 V, keine Hysterese |
| Count-Abbildung | floor(V), begrenzt auf 0…9 Wiederholungen |
| Zähler je Sektion | getrennt, Markierungen gemeinsam |
| REST-Ausgang | 1 V pro Durchlauf, höchstens 10 V |
| EXIT-Impuls | 1 ms, 10 V |
| Blinken | 0,5 s gesamt, 0,1 s Phasen |
| Kette | bis zu 3 Expander je Seite |

**Verhalten ohne Loop-Funktion**<br>
Ohne angedocktes Ritornello oder ohne beide Kabel erhält die Engine Null-Daten und verhält sich bitgleich zum Stand ohne Loop-Funktion (gegen die vorherige Engine geprüft).

<a id="ritornello-entscheidungen"></a>

## 11. Entscheidungen und offene Punkte

Entscheidungen von Kinovice, in der Reihenfolge der Umsetzung:

1. **Eigenes Expander-Modul von Anfang an**, kein Platz in Amilcare. Reiner Jack-Träger ohne eigene DSP; die Logik liegt in der Engine von Sofonisba.
2. **Begin und End sind eigene Markierungen**, unabhängig von First und Last.
3. **Verschachtelung** ist Teil von Version 1. Jede End-Stage hat ihren eigenen Zähler.
4. **Wertebereich** (2026-11-02 geändert): 1 V pro Wiederholung statt 1 V pro Durchlauf. 0 V = endlos.
5. **Konfiguration an der End-Stage**, Poly-Kanal = Stage-Nummer, gemeinsam für A und B.
6. **Reverse:** Begin und End tauschen die Rollen. Im Rack bestätigt und so belassen.
7. **Strobe hinein, Recall heraus** (so verstanden): Die Zustandsregel folgt daraus.
8. **Restwert und Anzeige zeigen die innerste Schleife.**
9. **Austrittsimpuls und Restanzeige** bleiben beide in Version 1. **EXIT feuert auch bei inneren Schleifen**, damit es immer eine Rückmeldung gibt. Die Anzeige blinkt beim Austritt eine echte, abwechselnd an- und ausgeschaltete 0.
10. **Endlose Schleifen lesen den Count laufend nach**, damit ein im Live-Betrieb gestecktes Kabel sofort wirkt.
11. **Pendulum:** Schleifen inaktiv in Version 1.
12. **Name Ritornello:** Das Namensschema der Anguissola-Familie wird bewusst verlassen.
13. **Kettensuche** (bis zu drei Expander je Seite): Amilcare, Bianca und Ritornello arbeiten gleichzeitig an einer Sofonisba. Im Rack geprüft, ohne Fehler.
14. **Zweistellige Anzeige „10“** (bei 9 V und mehr): passt auf das Display, im Rack geprüft.

**Offen**
- Ob die Zähler-Konfiguration später zusätzlich per Regler im Expander möglich sein soll.
- Ob ein zusätzlicher Ausgang „Gesamtschleife fertig“ (nur äußerste Schleife) sinnvoll ist.
- Sprünge, zweite Spannung (CV 2), Zeit-Quantisierung, Taktteilung und Fix bleiben eigene, spätere Features.
