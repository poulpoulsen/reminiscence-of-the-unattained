// bitcmp: schreibt die Ausgabe der Engine fuer viele Einstellungen (3 Abtastraten, alle Modi, NL, Vactrol-Varianten,
// linear/Paper, BOTH-Stufe, normiert/absolut) in eine Binaerdatei. Zweimal bauen (alte und neue Engine), beide
// Dateien mit cmp vergleichen: gleich = bitgleich.
// Bauen: g++ -std=c++11 -O2 -Isrc tests/bitcmp.cpp -o bitcmp     Aufruf: ./bitcmp ausgabe.bin^
#include "AnnaMariaLpg.hpp"
#include <cstdio>
#include <random>
#include <cmath>
int main(int, char** argv) {
	if (!argv[1]) { std::fprintf(stderr, "Aufruf: bitcmp ausgabe.bin\n"); return 1; }
	FILE* f = fopen(argv[1], "wb");
	const double rates[] = {44100.0, 48000.0, 96000.0};
	long count = 0;
	for (double fs : rates)
	for (int mode = 1; mode <= 3; mode++)
	for (int nl = 0; nl < 2; nl++)
	for (int vm = 0; vm < 3; vm++)
	for (int lin = 0; lin < 2; lin++)
	for (int bl = 0; bl < 2; bl++)
	for (int fbn = 0; fbn < 2; fbn++) {
		AnnaMariaLpg e; e.setSampleRate(fs);
		AnnaMariaLpg::Params p;
		p.mode = mode; p.nl = nl; p.nlCorrected = true;
		p.vactrolByState = (vm == 1); p.vactrolConductance = (vm == 2);
		p.vcaLinear = lin; p.bothLowpass = bl; p.fbnorm = fbn; p.fb = fbn ? 0.9 : 1.2;
		p.offset = 0.0; p.trim = 1.0; p.vactrolTimeScale = 1.0;
		p.cv = 5.0; e.warmStart(p);
		std::mt19937 rng(11 + mode); std::normal_distribution<double> nd(0.0, 0.3);
		for (int n = 0; n < (int) fs / 8; n++) {
			p.cv = 5.0 + 5.0 * std::sin(2 * 3.14159265358979 * 7.0 * n / fs);
			if (n % 5000 == 0) p.trim = 0.5 + 0.5 * ((n / 5000) % 2);
			double y = e.process(nd(rng), p);
			fwrite(&y, sizeof y, 1, f); count++;
		}
	}
	fclose(f);
	printf("%ld Samples\n", count);
	return 0;
}
