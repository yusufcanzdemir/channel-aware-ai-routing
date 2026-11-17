"""Klasik (öğrenmeyen) tahmincilerin parametrelerini, modelle AYNI eğitim verisinde ayarlar.

Adil karşılaştırma için EWMA ve Holt'a da en iyi parametreleri bulma şansı verilir.
Hedef, modelinkiyle aynıdır: sonraki 3 pencerenin ortalama teslim oranı.

  EWMA : s = a * yeni + (1 - a) * s                       tahmin = s
  Holt : seviye ve eğilim birlikte izlenir                tahmin = seviye + 2 * eğilim
         (sonraki 1, 2, 3 adımın ortalaması 2 adım ileriye denk gelir)

Kullanım:
    python ml/tune_classic.py                  # data/link_degrade.csv, seed 1-7
Çıktı: ml/models/classic.txt  ("ewma_alpha 0.4" gibi satırlar; C++ okur)
"""

import itertools
import sys
from pathlib import Path

import numpy as np
import pandas as pd

REPO = Path(__file__).resolve().parent.parent
H = 3


def series(df):
    """Her (çalıştırma, link) için zaman sıralı teslim oranı dizileri."""
    df = df.sort_values(["routing", "seed", "tx", "rx", "time"])
    mean = df.groupby(["routing", "seed", "tx", "rx"])["pdr"].transform("mean")
    df = df[mean > 0.05]
    return [g["pdr"].to_numpy() for _, g in df.groupby(["routing", "seed", "tx", "rx"])]


def evaluate(seqs, predict):
    err = []
    for x in seqs:
        p = predict(x)
        for t in range(10, len(x) - H):          # model ile aynı: 10 pencere geçmiş gerekir
            err.append(p[t] - x[t + 1:t + 1 + H].mean())
    return float(np.sqrt(np.mean(np.square(err))))


def ewma(a):
    def f(x):
        s, out = x[0], np.empty_like(x)
        for t, v in enumerate(x):
            s = a * v + (1 - a) * s
            out[t] = s
        return out
    return f


def holt(a, b):
    def f(x):
        lvl, tr, out = x[0], 0.0, np.empty_like(x)
        for t, v in enumerate(x):
            prev = lvl
            lvl = a * v + (1 - a) * (lvl + tr)
            tr = b * (lvl - prev) + (1 - b) * tr
            out[t] = np.clip(lvl + 2 * tr, 0, 1)
        return out
    return f


def main():
    files = sys.argv[1:] or [str(REPO / "data/link_degrade.csv")]
    df = pd.concat([pd.read_csv(f) for f in files], ignore_index=True)
    df["routing"] = df["routing"] + "|" + df["scenario"]  # senaryolar ayrı çalıştırmalar
    train = series(df[df["seed"] <= 7])
    test = series(df[df["seed"] > 7])
    grid = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]
    best_a = min(grid, key=lambda a: evaluate(train, ewma(a)))
    best_h = min(itertools.product(grid, [0.05, 0.1, 0.2, 0.3, 0.5]),
                 key=lambda ab: evaluate(train, holt(*ab)))
    last = evaluate(test, lambda x: x)
    print(f"Test RMSE  son deger {last:.4f}")
    print(f"           EWMA(a={best_a}) {evaluate(test, ewma(best_a)):.4f}")
    print(f"           Holt(a={best_h[0]}, b={best_h[1]}) {evaluate(test, holt(*best_h)):.4f}")
    out = REPO / "ml/models/classic.txt"
    out.write_text(f"ewma_alpha {best_a}\nholt_alpha {best_h[0]}\nholt_beta {best_h[1]}\n")
    print("Parametreler:", out)


if __name__ == "__main__":
    main()
