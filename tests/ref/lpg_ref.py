#!/usr/bin/env python3
"""Woertliche Python-Transkription von buchla_lp_gate.m (Stefano D'Angelo, MIT).

Zweck: unabhaengige Gegenprobe fuer die C++-Engine, solange kein GNU Octave zur
Verfuegung steht. Die Struktur folgt bewusst der VEKTORISIERTEN Octave-Fassung
(Masken c1/c2/c3, Steuerpfad komplett vor dem Audiopfad), nicht der
sample-weisen C++-Fassung. Die verbindliche Referenz bleibt die .m-Datei:
tests/ref/vergleich_octave.m vergleicht gegen echtes Octave.

Aufruf:  python3 -I tests/ref/lpg_ref.py <datenverzeichnis>
"""
# Original-Octave-Code (buchla_lp_gate.m):
#   Copyright (C) 2014 Stefano D'Angelo
#
#   Permission is hereby granted, free of charge, to any person obtaining a
#   copy of this software and associated documentation files (the
#   "Software"), to deal in the Software without restriction, including
#   without limitation the rights to use, copy, modify, merge, publish,
#   distribute, sublicense, and/or sell copies of the Software, and to
#   permit persons to whom the Software is furnished to do so, subject to
#   the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
#   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
#   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
#   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
#   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
#   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import sys
import os
import numpy as np

# ---- Testfaelle (muessen mit tests/test_anna_maria_lpg.cpp und vergleich_octave.m uebereinstimmen) ----
N = 24000
CASES = [
    # name,            fs,    mode, nl, fbnorm, fb,  offset, trim
    ("lp_lin_fb0",     48000, 1, 0, 1, 0.0, 0.5, 0.5),
    ("lp_lin_fb09",    48000, 1, 0, 1, 0.9, 0.3, 0.2),
    ("lp_nl_fb09",     48000, 1, 1, 1, 0.9, 0.3, 0.8),
    ("vca_lin",        48000, 2, 0, 0, 0.0, 0.5, 0.5),
    ("both_nl",        48000, 3, 1, 0, 0.1, 0.7, 0.5),
    ("lp_nl_abs12",    48000, 1, 1, 0, 1.2, 0.5, 1.0),
    ("lp_lin_fb09_44", 44100, 1, 0, 1, 0.9, 0.3, 0.2),
]


def make_inputs(fs, n=N):
    t = np.arange(n) / fs
    x = 1.0 * np.sin(2 * np.pi * 220.0 * t) + 0.5 * np.sin(2 * np.pi * 3100.0 * t)
    cv = np.zeros(n)
    cv[(t >= 0.05) & (t < 0.15)] = 6.0
    cv[(t >= 0.15) & (t < 0.30)] = -2.0
    m = t >= 0.30
    cv[m] = -2.0 + 10.0 * (t[m] - 0.30) / 0.2
    return x, cv


def limit(x, lo, up):
    return np.minimum(np.maximum(x, lo), up)


def buchla_lp_gate(fs, x, cv, offset, trim, fb, mode, fbnorm, nl):
    L = len(x)
    C1 = 1e-9; C2 = 220e-12; C3lp = 4.7e-9; Cc = 2e-9
    Ralp = 5e6; Ravca = 5e3
    R1R2 = 10e3; R3 = 150e3; R4 = 470e3; R5 = 100e3; R6max = 20e3
    R7 = 33e3; R8 = 4.7e3; R9 = 470.0
    Vs = 15.0; G = 2e5; n = 3.9696; VT = 26e-3; VB = 3.9
    Ifmin = 10e-6; Ifmax = 40e-3
    trise = 12e-3 / 5; tfall = 250e-3 / 5
    A = 3.464; B = 1136.212; xi = 600.0
    k0 = 1.468e2; k1c = 4.920e-1; k2c = 4.167e-4; k3c = 7.391e-9
    kL = 6.386; gamma = 0.001

    cv = np.full(L, cv, dtype=float) if np.isscalar(cv) else np.asarray(cv, float)
    offset = np.full(L, offset, dtype=float) if np.isscalar(offset) else np.asarray(offset, float)
    trim = np.full(L, trim, dtype=float) if np.isscalar(trim) else np.asarray(trim, float)
    fb = np.full(L, fb, dtype=float) if np.isscalar(fb) else np.asarray(fb, float)
    mode = np.full(L, mode, dtype=int) if np.isscalar(mode) else np.asarray(mode, int)

    cv = limit(cv, -10, 10)
    offset = limit(offset, 0, 1)
    trim = limit(trim, 0, 1)
    if not nl:
        mm = mode == 1
        if fbnorm:
            fb[mm] = np.minimum(fb[mm], 1.0)
        else:
            fb[mm] = np.minimum(fb[mm], 1 + (2 * C1 + C2) / C3lp)

    R2 = R1R2 * offset
    R1 = R1R2 - R2
    R6 = R6max * trim

    C3 = np.zeros(L)
    C3[mode == 1] = C3lp
    Ra = np.full(L, Ralp)
    Ra[mode == 2] = Ravca

    alpha = 1 + (R6 + R7) * (1 / R3 + 1 / R5)
    beta = (1 / alpha - 1) / (R6 + R7) - 1 / R8

    # ---- Steuerpfad: High-Shelving, filter([kVb1 kVb2],[1 kVb3], cv) ----
    kVb1 = (1 + 2 * Cc * fs * (R4 + R5)) / (1 + 2 * Cc * fs * R4)
    kVb2 = (1 - 2 * Cc * fs * (R4 + R5)) / (1 + 2 * Cc * fs * R4)
    kVb3 = (1 - 2 * Cc * fs * R4) / (1 + 2 * Cc * fs * R4)
    Vb = np.zeros(L)
    z = 0.0
    for i in range(L):
        Vb[i] = kVb1 * cv[i] + z
        z = kVb2 * cv[i] - kVb3 * Vb[i]

    with np.errstate(divide="ignore", invalid="ignore"):
        Ia = Vb / R5 + Vs / (R3 * (1 + R1 / R2))

    # ---- statische Nichtlinearitaet ----
    kIa = xi * alpha * n * VT / (G * (R6 + R7 - 1 / (alpha * beta)))
    c1 = Ia <= -kIa
    c3 = Ia >= kIa
    c2 = ~(c1 | c3)
    V3 = np.zeros(L)
    V3[c1] = -Ia[c1] / (alpha[c1] * beta[c1])
    V3[c3] = kL * alpha[c3] / G * n * VT - Ia[c3] * (R6[c3] + R7)
    warg = np.zeros(L)
    w = np.zeros(L)
    warg[c2] = G * Ia[c2] * (R6[c2] + R7 - 1 / (alpha[c2] * beta[c2])) / (alpha[c2] * n * VT)
    w[c2] = k0 + warg[c2] * (k1c + warg[c2] * (k2c + warg[c2] * k3c))
    V3[c2] = -alpha[c2] / G * n * VT * w[c2] - Ia[c2] / (alpha[c2] * beta[c2])

    kIa2 = alpha * (Ifmin - beta * V3)
    kIa3 = VB / (R6 + R7)
    kIa4 = (gamma * G * VB + alpha * R9 * (VB * beta + Ifmax)) / (gamma * G * (R6 + R7) + R9)
    c1 = Ia <= kIa2
    c2 = (Ia <= kIa3) & ~c1
    c4 = Ia >= kIa4
    c3 = ~(c4 | c1 | c2)
    If = np.zeros(L)
    If[c1] = Ifmin
    If[c2] = beta[c2] * V3[c2] + Ia[c2] / alpha[c2]
    If[c3] = gamma * G * (Ia[c3] * (R6[c3] + R7) - VB) / (alpha[c3] * R9) - beta[c3] * VB + Ia[c3] / alpha[c3]
    If[c4] = Ifmax

    Rf = A / If ** 1.4 + B

    # ---- Vactrol ----
    Rfz1 = np.concatenate(([0.0], Rf[:-1]))
    dRf = Rf - Rfz1
    t = np.full(L, tfall)
    t[dRf < 0] = trise
    k1 = 1 / (1 + 2 * fs * t)
    k2 = k1 * (2 * fs * t - 1)
    Rfv = np.zeros(L)
    Rfvz1 = 0.0
    for i in range(L):
        Rfv[i] = k1[i] * (Rf[i] + Rfz1[i]) + k2[i] * Rfvz1
        Rfvz1 = Rfv[i]

    # ---- Audiopfad ----
    a = fb.copy()
    if fbnorm:
        m1 = mode == 1
        a[m1] = a[m1] * (2 * C1 * Ra[m1] + (C2 + C3[m1]) * (Ra[m1] + Rfv[m1])) / (C3[m1] * Ra[m1])

    Rfinv = 1 / Rfv
    a1 = Rfinv / C1
    a2 = -(Rfinv + 1 / Ra) / C1
    b1 = Rfinv / C2
    b2 = -2 * b1
    b3 = b1
    b4 = C3 / C2
    d1 = a
    d2 = -1.0

    koo = 1 / (1 - a2 / (2 * fs))
    kox = a1 / (2 * fs) * koo
    kdo = 2 * fs * d1
    kdx = 2 * fs * d2
    ksdo = -4 * fs * d1
    ksdx = -4 * fs * d2
    ksxi = b1 / fs
    ksxx = b2 / fs
    ksxo = b3 / fs
    ksxd = b4 / fs
    ksox = a1 / fs
    ksoo = a2 / fs

    sd = 0.0; sx = 0.0; so = 0.0
    y = np.zeros(L)
    if nl:
        yoz1 = 0.0
        for i in range(L):
            tt = np.tanh(yoz1)
            t2m1 = tt * tt - 1
            yxden = (a2[i] * b2[i] - a1[i] * b3[i]
                     - 2 * fs * (a2[i] + b2[i] + b4[i] * (a1[i] * d1[i] * t2m1 - a2[i] * d2))
                     - 4 * fs * fs * (b4[i] * d2 - 1))
            yxnom = 2 * fs - a2[i]
            kyx = yxnom / yxden
            kxo = 2 * fs * (-2 * fs * b4[i] * d1[i] * t2m1 + b3[i]) / yxden
            kxi = kyx * b1[i]
            kxd = kyx * b4[i]
            kxx = kyx * 2 * fs
            kxt = kxx * b4[i] * d1[i]
            kxyoz1 = kxt * t2m1

            yx = kxo * so + kxi * x[i] + kxd * sd + kxx * sx + kxt * tt + kxyoz1 * yoz1
            yo = koo[i] * so + kox[i] * yx
            yonl = tt - (yo - yoz1) * t2m1
            yd = sd + kdo[i] * yonl + kdx * yx

            sd = -sd + ksdo[i] * yonl + ksdx * yx
            sx = sx + ksxi[i] * x[i] + ksxx[i] * yx + ksxo[i] * yo + ksxd[i] * yd
            so = so + ksox[i] * yx + ksoo[i] * yo
            yoz1 = yo
            y[i] = yo
    else:
        yxden = (a2 * b2 - a1 * b3
                 - 2 * fs * (a2 + b2 + b4 * (a1 * d1 - a2 * d2))
                 - 4 * fs * fs * (b4 * d2 - 1))
        yxnom = 2 * fs - a2
        kyx = yxnom / yxden
        kxo = 2 * fs * (2 * fs * b4 * d1 + b3) / yxden
        kxi = kyx * b1
        kxd = kyx * b4
        kxx = kyx * 2 * fs
        for i in range(L):
            yx = kxo[i] * so + kxi[i] * x[i] + kxd[i] * sd + kxx[i] * sx
            yo = koo[i] * so + kox[i] * yx
            yd = sd + kdo[i] * yo + kdx * yx
            sd = -sd + ksdo[i] * yo + ksdx * yx
            sx = sx + ksxi[i] * x[i] + ksxx[i] * yx + ksxo[i] * yo + ksxd[i] * yd
            so = so + ksox[i] * yx + ksoo[i] * yo
            y[i] = yo
    return y


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "data"
    os.makedirs(outdir, exist_ok=True)
    for (name, fs, mode, nl, fbnorm, fb, offset, trim) in CASES:
        x, cv = make_inputs(fs)
        y = buchla_lp_gate(fs, x, cv, offset, trim, fb, mode, bool(fbnorm), bool(nl))
        path = os.path.join(outdir, "ref_py_%s.csv" % name)
        with open(path, "w") as f:
            f.write("n,x,cv,y\n")
            for i in range(len(x)):
                f.write("%d,%.17g,%.17g,%.17g\n" % (i, x[i], cv[i], y[i]))
        print("%-16s peak|y|=%.6g  finite=%s" % (name, np.max(np.abs(y)), bool(np.all(np.isfinite(y)))))


if __name__ == "__main__":
    main()
