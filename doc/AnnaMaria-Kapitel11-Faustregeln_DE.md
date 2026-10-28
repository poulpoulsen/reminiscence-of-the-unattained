# Anna-Maria – Einträge für Kapitel 11 (Faustregeln)

* **Gate nicht „weich“ bedienen**: Zwischen 10 % und 90 % der Öffnung liegen nur ca. 0,2 V CV. Die Schwelle direkt über THRESHOLD (Volt) setzen; nicht erwarten, dass eine langsame Hüllkurve linear „aufzieht“.
* **Schwelle**: THRESHOLD ist die CV in Volt (nach CV ATT), bei der das Gate halb offen ist. Standard 4,1 V. DEPTH ändert nicht die Schwelle, nur wie tief das Gate darunter schließt.
* **Pluck**: Im Menü *Vactrol response = State-based* wählen. Mit *Reference* öffnet das Gate in ~220 ms und reagiert auf Gates < 200 ms kaum.
* **Decay**: Der Ausklang hängt von der Ruhe-CV ab: Tief (≤ −4 V) = sofort zu; knapp unter der Schwelle (3…3,5 V) = 33…37 ms bis −20 dB.
* **Resonanz (Regler FEEDBACK)**: Passiert erst ab 75 %. Bis 50 % = nichts, 75 % = +2,7 dB, 90 % = +10 dB, 99 % = +29 dB. Bei 100 % Grenzstabilität (Schwingung bleibt). Gilt nur im Modus LOWPASS, Menü Resonance = Normalized und Gate knapp unter der Schwelle (gemessen bei Grenzfrequenz 407 Hz).
* **Normiert vs. absolut**: Beides gleich, solange das Gate offen oder halb offen ist; absolut verliert die Resonanz bei fast geschlossenem Gate (CV −4 V: −0,4 dB statt +6,8 dB).
* **DRIVE** wirkt nur mit FB NL = an. Ohne NL ist er klanglich neutral. Die Obertöne wachsen bei tiefen Frequenzen bis Drive ≈ 7,5, bei hohen bis 10.
* **NL hörbar** erst am Resonanzgipfel bei halb offenem Gate (bis ~13 % Oberwellen); bei offenem Gate ohne Resonanz < 0,5 %.
* **MODE**: LOWPASS = nur Filter, hat als einziger Feedback. BOTH = Filter + VCA (Pluck). VCA = nur Pegel (offen 0,63 = −4 dB).
* **Pegel**: Offen ist LOWPASS ≈ 1,0, VCA ≈ 0,63 (−4 dB). Dahinter ggf. Pegel anheben.
* **Kein CV am Eingang = 0 V**: Das Gate ist dann bei Standardwerten zu (Schwelle 4,1 V). Für Dauerton THRESHOLD unter 0 V stellen (ganz links = offen).
* **Sättigung** (Menü *Saturation = On*, Zusatz): Intensität über DRIVE. Sinus 5 V: Drive 5 = 6,7 % Oberwellen, Drive 10 = 30 %. Der Ausgangspegel sinkt dabei auf den Pegelfaktor (5 V → 3,8 V bei Drive 5, 1,25 V bei Drive 10). Am stärksten bei offenem Gate und lautem Signal. Für einen warmen Sinus im Hörtest: DRIVE ≈ 7,5.
* **Gate zu ≠ stumm**: LOWPASS/BOTH lassen bei geschlossenem Gate die Bässe durch (CV 0 V, 100 Hz: −1,6/−0,6 dB; 5 kHz: −41/−21 dB). VCA: breitbandig −35 dB bei CV 0 V. Erst bei CV −10 V praktisch stumm.
* **Negativer THRESHOLD** (bis −0,9 V bei DEPTH 50 %, −2,6 V bei DEPTH 100 %): Das Gate ist ohne CV offen, eine negative CV schließt es. Nützlich für Dauertöne mit „Ducking“, für bipolare LFOs (öffnet in der positiven Halbwelle) und mit CV ATT −100 % für invertiertes Gate. Schwellen zwischen −0,1 und +0,1 V meiden: Das Gate steht ohne CV-Kabel halb offen.
* **Warum es negative Schwellen gibt:** Das Gate öffnet bei `CV + 10 V × Offset ≈ 9,1 V`. Bei vollem Offset (10 V) fehlen an der Schwelle schon 0,9 V, die Schwelle liegt dann bei −0,9 V.
