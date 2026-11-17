"""Kontrol gecikmesi deneyinin grafikleri ve istatistik testleri.

Soru: Rotalar gecikmeli uygulandığında tahmin modeli "son ölçüm"den daha iyi mi,
ve mükemmel tahmine (kâhin) ne kadar yaklaşıyor?

Girdi : data/delay*.csv (scripts/run_delay.sh; seed 101-110 ve 111-130 çalıştırmaları),
        data/cmp_<senaryo>.csv (kâhin, AODV, OLSR; scripts/run_comparison.sh)
Çıktı : results/figures/gecikme_etkisi.png    son ölçüm vs model v2 (tüm seed'ler) + kâhin
        results/figures/gecikme_modeller.png  dört tahminci, seed 101-110
        results/delay_test.csv                senaryo/gecikme/model başına eşli test
        results/delay_test_havuz.csv          gecikmeli koşullar birlikte

Kâhin gecikmeden etkilenmez (rotaların geçerli olacağı dönemi zaten bilir), bu yüzden
gecikme 0'daki sonucu yatay çizgi olarak gösterilir.

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

# Tahminciler: (anahtar, etiket, renk). Renkler plot_demo.py ile aynı (yönteme sabit renk)
PREDICTORS = [
    ("last", "AI (son ölçüm)", "#1baf7a"),
    ("linear", "AI (model v1)", "#e87ba4"),
    ("linear2", "AI (model v2)", "#008300"),
    ("linear2-h1", "AI (model v2, 1 s hedef)", "#e34948"),
]
C_ORACLE = "#6250d6"
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


def style(ax):
    ax.grid(True, color=C_GRID, lw=0.8)
    ax.set_axisbelow(True)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(C_AXIS)
    ax.tick_params(colors=C_MUTED)


def load():
    files = sorted((REPO / "data").glob("delay*.csv"))
    df = pd.concat([pd.read_csv(f) for f in files], ignore_index=True)
    # Aynı (senaryo, yöntem, seed) birden fazla dosyada varsa bir kez say
    df = df.drop_duplicates(subset=["scenario", "routing", "seed"])
    df[["predictor", "delay"]] = df["routing"].apply(lambda s: pd.Series(parse_method(s)))
    return df


def reference(sc: str):
    """Aynı senaryoda kâhin, AODV, OLSR ortalama PDR'ı (seed 101-110)."""
    f = REPO / f"data/cmp_{sc}.csv"
    if not f.exists():
        return {}
    c = pd.read_csv(f)
    return c.groupby("routing")["pdr"].mean().to_dict()


def tests(df: pd.DataFrame):
    models = [p for p, _, _ in PREDICTORS[1:] if p in set(df["predictor"])]
    wide = df.pivot_table(index=["scenario", "delay", "seed"], columns="predictor",
                          values="pdr").reset_index()
    rows = []
    for (sc, d), sub in wide.groupby(["scenario", "delay"]):
        for mdl in models:
            diff = (sub[mdl] - sub["last"]).dropna()
            if len(diff) < 2:
                continue
            rows.append({
                "scenario": sc, "delay_s": d, "model": mdl, "n": len(diff),
                "pdr_last": sub.loc[diff.index, "last"].mean(),
                "pdr_model": sub.loc[diff.index, mdl].mean(),
                "fark": diff.mean(), "fark_ci95": ci95(diff),
                "model_daha_iyi_seed": int((diff > 0).sum()),
                "p_degeri": stats.ttest_1samp(diff, 0).pvalue,
            })
    per = pd.DataFrame(rows)
    per.to_csv(REPO / "results/delay_test.csv", index=False, float_format="%.4f")

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
    print(per.round(3).to_string(index=False))
    print("\nGecikmeli kosullar birlikte (model - son olcum):")
    print(pooled.round(4).to_string(index=False))


def draw_refs(ax, sc):
    ref = reference(sc)
    labels = []
    if "ai-oracle" in ref:
        v = ref["ai-oracle"]
        ax.axhline(v, color=C_ORACLE, lw=1.5, ls=":")
        labels.append((v, f"Kâhin {v:.0f}"))
    vals = {k: ref[k] for k in ("aodv", "olsr") if k in ref}
    for k, color in (("aodv", C_AODV), ("olsr", C_OLSR)):
        if k in vals:
            ax.axhline(vals[k], color=color, lw=1.2, ls="--")
    if len(vals) == 2 and abs(vals["aodv"] - vals["olsr"]) < 3:
        labels.append((sum(vals.values()) / 2, f"AODV/OLSR {sum(vals.values()) / 2:.0f}"))
    else:
        labels += [(v, f"{k.upper()} {v:.0f}") for k, v in vals.items()]
    for v, text in labels:
        ax.text(3.08, v, text, color=C_TEXT, fontsize=8, va="center")


def panel(ax, sub, preds, bands):
    for pred, label, color in PREDICTORS:
        if pred not in preds or pred not in set(sub["predictor"]):
            continue
        g = sub[sub["predictor"] == pred].groupby("delay")["pdr"]
        mean = g.mean()
        if pred in bands:
            ci = g.apply(ci95)
            ax.fill_between(mean.index, mean - ci, mean + ci, color=color, alpha=0.15, lw=0)
        n = g.size().min()
        ax.plot(mean.index, mean, color=color, lw=2, marker="o", ms=6,
                label=f"{label} (n={n})", markeredgecolor="white", markeredgewidth=1.5)


def figure(df, preds, bands, fname, title):
    fig, axes = plt.subplots(1, 2, figsize=(10.5, 4), dpi=150, sharey=True)
    for ax, (sc, name) in zip(axes, SCENARIOS):
        panel(ax, df[df["scenario"] == sc], preds, bands)
        draw_refs(ax, sc)
        ax.set_title(name, loc="left", fontsize=10, color=C_TEXT)
        ax.set_xlabel("Kontrol gecikmesi (s)", color=C_MUTED)
        ax.set_xticks(sorted(df["delay"].unique()))
        ax.set_xlim(-0.2, 3.9)
        style(ax)
    axes[0].set_ylabel("Paket teslim oranı (%)", color=C_MUTED)
    fig.suptitle(title, x=0.01, ha="left", fontsize=12, color=C_TEXT)
    fig.tight_layout(rect=(0, 0, 1, 0.88))
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, frameon=False, loc="upper left", bbox_to_anchor=(0.01, 0.92),
               ncol=4, fontsize=8.5)
    fig.savefig(FIG / fname)
    plt.close(fig)
    print(f"Grafik: results/figures/{fname}")


def main():
    df = load()
    tests(df)
    # Ana grafik: son ölçüm ve model v2, tüm seed'ler, güven bantlarıyla
    figure(df, {"last", "linear2"}, {"last", "linear2"}, "gecikme_etkisi.png",
           "Rotalar gecikmeli uygulanınca: son ölçüm, model v2 ve kâhin (%95 GA)")
    # Ek grafik: dört tahminci, ortak seed'ler (101-110)
    common = df[df["seed"] <= 110]
    figure(common, {p for p, _, _ in PREDICTORS}, set(), "gecikme_modeller.png",
           "Tahminci karşılaştırması (seed 101-110)")
    print("Test  : results/delay_test.csv, results/delay_test_havuz.csv")


if __name__ == "__main__":
    main()
