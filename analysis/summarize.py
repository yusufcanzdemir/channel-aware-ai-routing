"""Simülasyon sonuçlarını yöntem (ve senaryo) başına özetler.

Her metrik için ortalama, standart sapma ve %95 güven aralığı (t-dağılımı) hesaplar.
Seed sayısı az olduğu için normal dağılım yerine t-dağılımı kullanılır.

Kullanım:
    python3 analysis/summarize.py data/baseline.csv results/baseline_ozet.csv
"""

import sys

import pandas as pd
from scipy import stats

# Özetlenecek metrikler ve ekranda görünecek adları
METRICS = {
    "pdr": "PDR (%)",
    "delay_ms": "Gecikme (ms)",
    "throughput_kbps": "Throughput (kbps)",
    "jitter_ms": "Jitter (ms)",
    "avg_hops": "Ort. sekme",
}


def ci95(values: pd.Series) -> float:
    """%95 güven aralığının yarı genişliği: t * s / sqrt(n)."""
    n = len(values)
    if n < 2:
        return float("nan")
    t = stats.t.ppf(0.975, df=n - 1)
    return t * values.std(ddof=1) / n**0.5


def summarize(df: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for (scenario, routing), g in df.groupby(["scenario", "routing"]):
        row = {"scenario": scenario, "routing": routing, "n": len(g)}
        for col in METRICS:
            row[f"{col}_mean"] = g[col].mean()
            row[f"{col}_std"] = g[col].std(ddof=1)
            row[f"{col}_ci95"] = ci95(g[col])
        rows.append(row)
    return pd.DataFrame(rows)


def print_table(summary: pd.DataFrame) -> None:
    """Ekrana 'ortalama ± güven aralığı' biçiminde okunaklı tablo yazar."""
    shown = summary[["scenario", "routing", "n"]].copy()
    for col, name in METRICS.items():
        shown[name] = [
            f"{m:.2f} ± {c:.2f}"
            for m, c in zip(summary[f"{col}_mean"], summary[f"{col}_ci95"])
        ]
    print(shown.to_string(index=False))


def main() -> None:
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    df = pd.read_csv(sys.argv[1])
    summary = summarize(df)
    print_table(summary)

    if len(sys.argv) >= 3:
        summary.to_csv(sys.argv[2], index=False, float_format="%.4f")
        print(f"Ozet tablo: {sys.argv[2]}")


if __name__ == "__main__":
    main()
