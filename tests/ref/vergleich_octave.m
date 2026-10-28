## vergleich_octave.m — Vergleich der C++-Engine (AnnaMariaLpg.hpp) mit buchla_lp_gate.m
##
## Voraussetzungen:
##   * buchla_lp_gate.m (Original von Stefano D'Angelo) liegt im Octave-Pfad,
##     z.B. im selben Ordner wie dieses Skript oder per addpath().
##   * data/cpp_<fall>.csv wurde vorher vom C++-Test erzeugt:
##       ./test_anna_maria_lpg data
##
## Aufruf in Octave (im Projektordner):
##   addpath('tests/ref'); addpath('<Ordner mit buchla_lp_gate.m>');
##   vergleich_octave('data')
##
## Das Skript liest x und cv aus den CSV-Dateien (damit die Eingaenge garantiert
## identisch sind), ruft buchla_lp_gate auf, schreibt data/ref_octave_<fall>.csv
## und gibt den Fehler gegenueber der C++-Ausgabe aus.
##
## Die Faelle muessen mit CASES in tests/test_anna_maria_lpg.cpp uebereinstimmen.

function vergleich_octave(datadir)
  if (nargin < 1)
    datadir = "data";
  endif

  #        name              fs     mode nl fbnorm fb   offset trim
  cases = {"lp_lin_fb0",     48000, 1,   0, 1,     0.0, 0.5,   0.5;
           "lp_lin_fb09",    48000, 1,   0, 1,     0.9, 0.3,   0.2;
           "lp_nl_fb09",     48000, 1,   1, 1,     0.9, 0.3,   0.8;
           "vca_lin",        48000, 2,   0, 0,     0.0, 0.5,   0.5;
           "both_nl",        48000, 3,   1, 0,     0.1, 0.7,   0.5;
           "lp_nl_abs12",    48000, 1,   1, 0,     1.2, 0.5,   1.0;
           "lp_lin_fb09_44", 44100, 1,   0, 1,     0.9, 0.3,   0.2};

  printf("%-16s %12s %12s\n", "Fall", "max|dy|", "relativ");
  for k = 1:rows(cases)
    name   = cases{k, 1};
    fs     = cases{k, 2};
    mode   = cases{k, 3};
    nl     = cases{k, 4};
    fbnorm = cases{k, 5};
    fb     = cases{k, 6};
    offset = cases{k, 7};
    trim   = cases{k, 8};

    M = dlmread([datadir "/cpp_" name ".csv"], ",", 1, 0);   # Kopfzeile ueberspringen
    x    = M(:, 2).';
    cv   = M(:, 3).';
    ycpp = M(:, 4).';

    y = buchla_lp_gate(fs, x, cv, offset, trim, fb, mode, logical(fbnorm), logical(nl));

    fid = fopen([datadir "/ref_octave_" name ".csv"], "w");
    fprintf(fid, "n,x,cv,y\n");
    for i = 1:length(x)
      fprintf(fid, "%d,%.17g,%.17g,%.17g\n", i - 1, x(i), cv(i), y(i));
    endfor
    fclose(fid);

    err  = max(abs(y - ycpp));
    peak = max(abs(y));
    printf("%-16s %12.3e %12.3e\n", name, err, err / peak);
  endfor
  printf("\nref_octave_*.csv geschrieben. Der C++-Test vergleicht sie beim naechsten Lauf ebenfalls.\n");
endfunction
