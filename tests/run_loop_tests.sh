#!/bin/sh
# Loop-Zaehler Engine-Tests (Session 1). Ohne Rack-SDK, nur g++/clang++.
# Aufruf aus dem Ordner tests/:  sh run_loop_tests.sh
# Erwartet das UNVERAENDERTE Marf248.hpp (Stand vor dem Loop-Feature) unter
# tests/reference/Marf248.hpp fuer den Bitgleichheits-Vergleich.
# Die Engine-Datei src/Marf248.hpp wird in einen Arbeitsordner kopiert, damit
# ihr #include "plugin.hpp" die Attrappe in rack_stub/ findet und nicht das
# echte src/plugin.hpp (Include mit Anfuehrungszeichen sucht zuerst im Ordner
# der einbindenden Datei).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${TMPDIR:-/tmp}/marf_loop_tests
NEWDIR="$OUT/new"
mkdir -p "$OUT" "$NEWDIR"
cp "$HERE/../src/Marf248.hpp" "$NEWDIR/Marf248.hpp"
CXX=${CXX:-g++}
FLAGS="-std=c++17 -O1 -Wall -I$HERE/rack_stub -I$HERE"
echo "== Szenario-Tests =="
$CXX $FLAGS -I"$NEWDIR" "$HERE/loop_engine_test.cpp" -o "$OUT/loop_engine_test"
"$OUT/loop_engine_test"
echo "== Bitgleichheit alt gegen neu =="
$CXX $FLAGS -I"$HERE/reference" "$HERE/loop_bitident.cpp" -o "$OUT/bi_old"
$CXX $FLAGS -DNEW_ENGINE -I"$NEWDIR" "$HERE/loop_bitident.cpp" -o "$OUT/bi_new"
"$OUT/bi_old" 500 > "$OUT/old.txt"
"$OUT/bi_new" 500 > "$OUT/new.txt"
cmp "$OUT/old.txt" "$OUT/new.txt" && echo "IDENTISCH (500 Zufalls-Skripte x 4000 Samples)"
