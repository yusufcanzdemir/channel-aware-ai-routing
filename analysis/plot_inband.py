"""Gerçekçi (in-band) kontrol düzlemi deneyinin analizi.

Soru: Ölçüm raporları ve rota güncellemeleri ağda gerçek paket olarak taşındığında
(gecikme, kayıp, ek yük) AI kontrolcü ne kadar başarım kaybeder, hâlâ AODV/OLSR'nin
önünde mi, ve tahmin modeli bu koşulda son ölçüme göre değer katıyor mu?

Girdi : data/cmp_<senaryo>.csv, data/ideal_ext_<senaryo>.csv, data/classic_ext_<senaryo>.csv
        (ideal kontrol düzlemi, seed 101-130) ve data/ib_*.csv (in-band, scripts/run_comparison.sh)
Çıktı : results/figures/kontrol_duzlemi_sonuc.png
        results/inband_ozet.csv   (senaryo × yöntem: PDR, gecikme, kontrol istatistikleri)
        results/inband_test.csv   (eşli testler)

Kullanım:
    python analysis/plot_inband.py
"""

from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.patches import Patch
from scipy import stats

REPO = Path(__file__).resolve().parent.parent
DATA = REPO / "data"
FIG = REPO / "results/figures"

SCENARIOS = [("none", "Bozulmasız"), ("fading", "Fading"), ("degrade", "Kademeli bozulma")]
# (anahtar, etiket, renk) - renkler diğer grafiklerle aynı (yönteme sabit renk)
METHODS = [
    ("aodv", "AODV", "#2a78d6"),
    ("olsr", "OLSR", "#eb6834"),
    ("ai-last", "AI (son ölçüm)", "#1baf7a"),
    ("ai-linear2", "AI (model v2)", "#008300"),
    ("ai-oracle", "AI (kâhin)", "#6250d6"),
]
TEXT, MUTED, GRID, AXIS = "#0b0b0b", "#52514e", "#e1e0d9", "#c3c2b7"


def ci95(x):
    x = pd.Series(x).dropna()
    n = len(x)
    return stats.t.ppf(0.975, n - 1) * x.std(ddof=1) / n**0.5 if n > 1 else 0.0


def load():
    frames = []
    for sc, _ in SCENARIOS:
        for f in [f"cmp_{sc}", f"ideal_ext_{sc}", f"classic_ext_{sc}", f"ib_{sc}"] + \
                 [f"ib_{sc}_{m}" for m in ("last", "linear2", "oracle")]:
            p = DATA / f"{f}.csv"
            if p.exists():
                frames.append(pd.read_csv(p))
    df = pd.concat(frames, ignore_index=True)
    df = df.drop_duplicates(subset=["scenario", "routing", "seed"])
    df["plane"] = np.where(df["routing"].str.endswith("-ib"), "inband", "ideal")
    df["method"] = df["routing"].str.replace("-ib", "", regex=False)
    # Sadece karşılaştırılan yöntemler (hareketli ort., v1 bu analizin dışında)
    df = df[df["method"].isin([m for m, _, _ in METHODS])]
    return df


def summary(df):
    rows = []
    for (sc, m, pl), g in df.groupby(["scenario", "method", "plane"]):
        row = {"scenario": sc, "method": m, "plane": pl, "n": len(g),
               "pdr": g["pdr"].mean(), "pdr_ci95": ci95(g["pdr"]),
               "delay_ms": g["delay_ms"].mean()}
        if pl == "inband":
            for c in ("ctrl_tx_per_s", "report_dr", "update_dr", "ctrl_delay_ms"):
                row[c] = g[c].mean()
        rows.append(row)
    s = pd.DataFrame(rows)
    s.to_csv(REPO / "results/inband_ozet.csv", index=False, float_format="%.4f")
    print(s.round(3).to_string(index=False))
    return s


def tests(df):
    """Aynı seed'lerde eşli karşılaştırmalar."""
    w = df.pivot_table(index=["scenario", "seed"], columns=["method", "plane"], values="pdr")
    pairs = [
        (("ai-linear2", "inband"), ("ai-last", "inband"), "in-band: model v2 - son ölçüm"),
        (("ai-oracle", "inband"), ("ai-linear2", "inband"), "in-band: kâhin - model v2"),
        (("ai-last", "inband"), ("ai-last", "ideal"), "son ölçüm: in-band - ideal"),
        (("ai-linear2", "inband"), ("ai-linear2", "ideal"), "model v2: in-band - ideal"),
        (("ai-linear2", "inband"), ("aodv", "ideal"), "in-band model v2 - AODV"),
        (("ai-linear2", "inband"), ("olsr", "ideal"), "in-band model v2 - OLSR"),
    ]
    rows = []
    for sc, _ in SCENARIOS:
        sub = w.loc[sc] if sc in w.index.get_level_values(0) else None
        if sub is None:
            continue
        for a, b, name in pairs:
            if a not in sub.columns or b not in sub.columns:
                continue
            d = (sub[a] - sub[b]).dropna()
            if len(d) < 2:
                continue
            rows.append({"scenario": sc, "karsilastirma": name, "n": len(d), "fark": d.mean(),
                         "fark_ci95": ci95(d), "ilk_daha_iyi": int((d > 0).sum()),
                         "p_ttest": stats.ttest_1samp(d, 0).pvalue,
                         "p_wilcoxon": stats.wilcoxon(d).pvalue if (d != 0).any() else 1.0})
    t = pd.DataFrame(rows)
    t.to_csv(REPO / "results/inband_test.csv", index=False, float_format="%.4f")
    print()
    print(t.round(4).to_string(index=False))


def figure(s):
    fig, ax = plt.subplots(figsize=(11, 4.6), dpi=150)
    names = [n for k, n in SCENARIOS if k in set(s["scenario"])]
    keys = [k for k, _ in SCENARIOS if k in set(s["scenario"])]
    # Çubuk sırası: AODV, OLSR, sonra her AI tahmincisi için ideal + in-band yan yana
    bars = [("aodv", "ideal"), ("olsr", "ideal")]
    for m in ("ai-last", "ai-linear2", "ai-oracle"):
        bars += [(m, "ideal"), (m, "inband")]
    color = {m: c for m, _, c in METHODS}
    x = np.arange(len(keys))
    w = 0.85 / len(bars)
    for i, (m, pl) in enumerate(bars):
        vals, cis = [], []
        for sc in keys:
            r = s[(s["scenario"] == sc) & (s["method"] == m) & (s["plane"] == pl)]
            vals.append(r["pdr"].iloc[0] if len(r) else np.nan)
            cis.append(r["pdr_ci95"].iloc[0] if len(r) else 0)
        pos = x - 0.425 + w * (i + 0.5)
        inband = pl == "inband"
        ax.bar(pos, vals, w * 0.9, yerr=cis, color="white" if inband else color[m],
               edgecolor=color[m], hatch="////" if inband else None, lw=1.2,
               error_kw=dict(ecolor=MUTED, lw=0.9, capsize=2))
        # Değer etiketi hata çubuğunun üstünde, metin renginde (koyu çubukta da okunur)
        for xp, v, ci in zip(pos, vals, cis):
            if not np.isnan(v):
                ax.text(xp, v + ci + 1.2, f"{v:.0f}", ha="center", va="bottom", fontsize=7,
                        color=TEXT)
    ax.set_xticks(x, names)
    ax.set_ylim(0, 112)
    ax.set_ylabel("Paket teslim oranı (%)", color=MUTED)
    ax.grid(True, axis="y", color=GRID)
    ax.set_axisbelow(True)
    for sp in ("top", "right"):
        ax.spines[sp].set_visible(False)
    for sp in ("left", "bottom"):
        ax.spines[sp].set_color(AXIS)
    ax.tick_params(colors=MUTED)
    handles = [Patch(fc=c, ec=c, label=l) for _, l, c in METHODS]
    handles += [Patch(fc=MUTED, ec=MUTED, label="dolu: ideal kontrol düzlemi"),
                Patch(fc="white", ec=MUTED, hatch="////", label="taralı: gerçekçi (in-band)")]
    ax.legend(handles=handles, frameon=False, ncol=4, fontsize=8.5, loc="upper left",
              bbox_to_anchor=(0, 1.2))
    fig.suptitle("Kontrol mesajları ağda gerçek paket olarak taşınınca (%95 GA)",
                 x=0.01, ha="left", fontsize=12, color=TEXT)
    fig.tight_layout()
    fig.savefig(FIG / "kontrol_duzlemi_sonuc.png")
    plt.close(fig)
    print("Grafik: results/figures/kontrol_duzlemi_sonuc.png")


def main():
    df = load()
    s = summary(df)
    tests(df)
    figure(s)


if __name__ == "__main__":
    main()
