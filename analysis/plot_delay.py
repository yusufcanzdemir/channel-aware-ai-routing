"""Kontrol gecikmesi deneyinin grafiği ve istatistik testi.

Soru: Rotalar gecikmeli uygulandığında tahmin modeli "son ölçüm"den daha iyi mi?

Girdi : data/delay.csv (scripts/run_delay.sh), referans için results/cmp_<senaryo>_ozet.csv
Çıktı : results/figures/gecikme_etkisi.png
        results/delay_test.csv  (her senaryo/gecikme için eşli t-testi: model - son ölçüm)

Kullanım:
    python analysis/plot_delay.py
"""

import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd
from scipy import stats

REPO = Path(__file__).resolve().parent.parent
FIG = REPO / "results/figures"

# Renkler plot_demo.py ile aynı: yöntem başına sabit renk
C_LAST = "#1baf7a"    # AI (son ölçüm)
C_LINEAR = "#e87ba4"  # AI (tahmin modeli)
C_AODV = "#2a78d6"
C_OLSR = "#eb6834"
C_TEXT = "#0b0b0b"
C_MUTED = "#52514e"
C_GRID = "#e1e0d9"
C_AXIS = "#c3c2b7"

SCENARIOS = [("degrade", "Kademeli bozulma"), ("fading", "Fading")]


def parse_method(name: str):
    """'ai-linear-d2' -> ('linear', 2); 'ai-last' -> ('last', 0)."""
    m = re.match(r"ai-(\w+?)(?:-d(\d+))?$", name)
    return m.group(1), int(m.group(2) or 0)


def ci95(x: pd.Series) -> float:
    n = len(x)
    return stats.t.ppf(0.975, n - 1) * x.std(ddof=1) / n**0.5 if n > 1 else 0.0


def main():
    df = pd.read_csv(REPO / "data/delay.csv")
    df[["predictor", "delay"]] = df["routing"].apply(lambda s: pd.Series(parse_method(s)))

    # --- Eşli t-testi: aynı seed'de model ile son ölçümün PDR farkı ---
    tests = []
    for sc, _ in SCENARIOS:
        for d in sorted(df["delay"].unique()):
            sub = df[(df["scenario"] == sc) & (df["delay"] == d)]
            a = sub[sub["predictor"] == "linear"].set_index("seed")["pdr"]
            b = sub[sub["predictor"] == "last"].set_index("seed")["pdr"]
            a, b = a.align(b, join="inner")
            diff = a - b
            tests.append({
                "scenario": sc, "delay_s": d, "n": len(diff),
                "pdr_last": b.mean(), "pdr_linear": a.mean(),
                "fark_model_eksi_son": diff.mean(), "fark_ci95": ci95(diff),
                "model_daha_iyi_seed": int((diff > 0).sum()),
                "p_degeri": stats.ttest_rel(a, b).pvalue,
            })
    tests = pd.DataFrame(tests)
    tests.to_csv(REPO / "results/delay_test.csv", index=False, float_format="%.4f")
    print(tests.round(3).to_string(index=False))

    # --- Grafik: gecikmeye göre PDR, senaryo başına bir panel ---
    fig, axes = plt.subplots(1, 2, figsize=(10, 3.8), dpi=150, sharey=True)
    for ax, (sc, title) in zip(axes, SCENARIOS):
        sub = df[df["scenario"] == sc]
        for pred, label, color in [("last", "AI (son ölçüm)", C_LAST),
                                   ("linear", "AI (tahmin modeli)", C_LINEAR)]:
            g = sub[sub["predictor"] == pred].groupby("delay")["pdr"]
            mean, ci = g.mean(), g.apply(ci95)
            ax.fill_between(mean.index, mean - ci, mean + ci, color=color, alpha=0.15, lw=0)
            ax.plot(mean.index, mean, color=color, lw=2, marker="o", ms=6, label=label,
                    markeredgecolor="white", markeredgewidth=1.5)
        # Referans: aynı senaryoda AODV ve OLSR (gecikmeden etkilenmezler)
        ref = REPO / f"results/cmp_{sc}_ozet.csv"
        if ref.exists():
            r = pd.read_csv(ref).set_index("routing")
            vals = {}
            for key, label, color in [("aodv", "AODV", C_AODV), ("olsr", "OLSR", C_OLSR)]:
                if key in r.index:
                    vals[label] = r.loc[key, "pdr_mean"]
                    ax.axhline(vals[label], color=color, lw=1.2, ls="--")
            # Değerler çok yakınsa etiketler üst üste binmesin: tek etiket yaz
            if len(vals) == 2 and abs(vals["AODV"] - vals["OLSR"]) < 3:
                v = sum(vals.values()) / 2
                ax.text(3.05, v + 1.5, f"AODV/OLSR {v:.0f}", color=C_TEXT, fontsize=8)
            else:
                for label, v in vals.items():
                    ax.text(3.05, v, f"{label} {v:.0f}", color=C_TEXT, fontsize=8, va="center")
        ax.set_title(title, loc="left", fontsize=10, color=C_TEXT)
        ax.set_xlabel("Kontrol gecikmesi (s)", color=C_MUTED)
        ax.set_xticks(sorted(df["delay"].unique()))
        ax.set_xlim(-0.2, 3.6)
        ax.grid(True, color=C_GRID, lw=0.8)
        ax.set_axisbelow(True)
        for s in ("top", "right"):
            ax.spines[s].set_visible(False)
        for s in ("left", "bottom"):
            ax.spines[s].set_color(C_AXIS)
        ax.tick_params(colors=C_MUTED)
    axes[0].set_ylabel("Paket teslim oranı (%)", color=C_MUTED)
    fig.suptitle("Rotalar gecikmeli uygulanınca: son ölçüm mü, tahmin mi? (10 seed, %95 GA)",
                 x=0.01, ha="left", fontsize=12, color=C_TEXT)
    fig.tight_layout(rect=(0, 0, 1, 0.9))
    # Açıklama başlığın altında, panellerin üstünde (çizgilerle çakışmasın)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, frameon=False, loc="upper left", bbox_to_anchor=(0.01, 0.93),
               ncol=2, fontsize=9)
    fig.savefig(FIG / "gecikme_etkisi.png")
    plt.close(fig)
    print("Grafik: results/figures/gecikme_etkisi.png")
    print("Test  : results/delay_test.csv")


if __name__ == "__main__":
    main()
