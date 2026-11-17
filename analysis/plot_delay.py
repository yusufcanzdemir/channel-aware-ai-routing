"""Kontrol gecikmesi deneyinin grafiği ve istatistik testi.

Soru: Rotalar gecikmeli uygulandığında tahmin modeli "son ölçüm"den daha iyi mi?

Girdi : data/delay.csv ve (varsa) data/delay_v2.csv (scripts/run_delay.sh),
        referans için results/cmp_<senaryo>_ozet.csv
Çıktı : results/figures/gecikme_etkisi.png
        results/delay_test.csv  (her senaryo/gecikme/model için eşli t-testi: model - son ölçüm)
        results/delay_test_havuz.csv (gecikmeli tüm koşullar birlikte)

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
# Tahminciler: (anahtar, etiket, renk) - kategorik paletin sabit sırası (plot_demo ile uyumlu)
PREDICTORS = [
    ("last", "AI (son ölçüm)", "#1baf7a"),
    ("linear", "AI (model v1)", "#e87ba4"),
    ("linear2", "AI (model v2)", "#008300"),
    ("linear2-h1", "AI (model v2, 1 s hedef)", "#6250d6"),
]
C_AODV = "#2a78d6"
C_OLSR = "#eb6834"
C_TEXT = "#0b0b0b"
C_MUTED = "#52514e"
C_GRID = "#e1e0d9"
C_AXIS = "#c3c2b7"

SCENARIOS = [("degrade", "Kademeli bozulma"), ("fading", "Fading")]


def parse_method(name: str):
    """'ai-linear-d2' -> ('linear', 2); 'ai-linear2-d1-h1' -> ('linear2-h1', 1)."""
    m = re.match(r"ai-([a-z0-9]+)(?:-d(\d+))?(?:-h(\d+))?$", name)
    pred = m.group(1) + (f"-h{m.group(3)}" if m.group(3) else "")
    return pred, int(m.group(2) or 0)


def ci95(x: pd.Series) -> float:
    n = len(x)
    return stats.t.ppf(0.975, n - 1) * x.std(ddof=1) / n**0.5 if n > 1 else 0.0


def main():
    files = [REPO / "data/delay.csv", REPO / "data/delay_v2.csv"]
    df = pd.concat([pd.read_csv(f) for f in files if f.exists()], ignore_index=True)
    df[["predictor", "delay"]] = df["routing"].apply(lambda s: pd.Series(parse_method(s)))
    models = [p for p, _, _ in PREDICTORS[1:] if p in set(df["predictor"])]

    # Eşleştirme tablosu: (senaryo, gecikme, seed) satırı, tahminci sütunu
    wide = df.pivot_table(index=["scenario", "delay", "seed"], columns="predictor",
                          values="pdr").reset_index()

    # --- Eşli t-testi: aynı seed'de her model ile son ölçümün PDR farkı ---
    tests = []
    for (sc, d), sub in wide.groupby(["scenario", "delay"]):
        for mdl in models:
            diff = (sub[mdl] - sub["last"]).dropna()
            tests.append({
                "scenario": sc, "delay_s": d, "model": mdl, "n": len(diff),
                "pdr_last": sub["last"].mean(), "pdr_model": sub[mdl].mean(),
                "fark": diff.mean(), "fark_ci95": ci95(diff),
                "model_daha_iyi_seed": int((diff > 0).sum()),
                "p_degeri": stats.ttest_1samp(diff, 0).pvalue,
            })
    tests = pd.DataFrame(tests)
    tests.to_csv(REPO / "results/delay_test.csv", index=False, float_format="%.4f")
    print(tests.round(3).to_string(index=False))

    # --- Havuz: gecikmeli (> 0 s) tüm koşullar birlikte ---
    pooled = []
    delayed = wide[wide["delay"] > 0]
    for mdl in models:
        for sc in ["hepsi"] + [s for s, _ in SCENARIOS]:
            sub = delayed if sc == "hepsi" else delayed[delayed["scenario"] == sc]
            diff = (sub[mdl] - sub["last"]).dropna()
            pooled.append({
                "model": mdl, "scenario": sc, "n": len(diff), "fark": diff.mean(),
                "fark_ci95": ci95(diff), "model_daha_iyi": int((diff > 0).sum()),
                "p_ttest": stats.ttest_1samp(diff, 0).pvalue,
                "p_wilcoxon": stats.wilcoxon(diff).pvalue,
            })
    pooled = pd.DataFrame(pooled)
    pooled.to_csv(REPO / "results/delay_test_havuz.csv", index=False, float_format="%.4f")
    print("\nGecikmeli kosullar birlikte (model - son olcum):")
    print(pooled.round(4).to_string(index=False))

    # --- Grafik: gecikmeye göre PDR, senaryo başına bir panel ---
    fig, axes = plt.subplots(1, 2, figsize=(10, 3.8), dpi=150, sharey=True)
    for ax, (sc, title) in zip(axes, SCENARIOS):
        sub = df[df["scenario"] == sc]
        for pred, label, color in PREDICTORS:
            if pred not in set(sub["predictor"]):
                continue
            g = sub[sub["predictor"] == pred].groupby("delay")["pdr"]
            mean = g.mean()
            # Dört çizgide güven bantları üst üste binip okunmaz hale gelir: yalnızca
            # referans (son ölçüm) için bant çizilir; testler results/delay_test*.csv'de
            if pred == "last":
                ci = g.apply(ci95)
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
    fig.suptitle("Rotalar gecikmeli uygulanınca: son ölçüm mü, tahmin mi? (10 seed)",
                 x=0.01, ha="left", fontsize=12, color=C_TEXT)
    fig.tight_layout(rect=(0, 0, 1, 0.9))
    # Açıklama başlığın altında, panellerin üstünde (çizgilerle çakışmasın)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, frameon=False, loc="upper left", bbox_to_anchor=(0.01, 0.93),
               ncol=4, fontsize=9)
    fig.savefig(FIG / "gecikme_etkisi.png")
    plt.close(fig)
    print("Grafik: results/figures/gecikme_etkisi.png")
    print("Test  : results/delay_test.csv")


if __name__ == "__main__":
    main()
