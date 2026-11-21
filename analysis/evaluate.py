"""Deney döngüsünün analiz aşaması (scripts/pipeline.sh analiz).

data/eval/raw/ altındaki her simülasyon sonucunu toplar ve şunları üretir:
  results/eval_ozet.csv               senaryo × yöntem × kontrol düzlemi: PDR, gecikme, kontrol yükü
  results/eval_test.csv               aynı seed'lerde eşli karşılaştırmalar (t-testi, Wilcoxon)
  results/figures/eval_pdr.png        bütün senaryo ve yöntemlerin PDR ısı haritası
  results/figures/eval_model_kazanci.png  model v2'nin son ölçüme ve EWMA'ya göre kazancı
  results/SONUCLAR.md                 okunabilir özet (her döngüde yeniden yazılır)

Kullanım:
    python analysis/evaluate.py
"""

from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from scipy import stats

REPO = Path(__file__).resolve().parent.parent
RAW = REPO / "data/eval/raw"
FIG = REPO / "results/figures"

SCEN_NAMES = {"none": "Bozulmasız", "fading": "Fading", "degrade": "Kademeli bozulma",
              "interference": "Girişim", "jamming": "Jamming", "nodefail": "Düğüm arızası",
              "congestion": "Tıkanıklık"}
METH_NAMES = {"aodv": "AODV", "olsr": "OLSR", "aodv-np": "AODV (yoklamasız)",
              "olsr-np": "OLSR (yoklamasız)", "ai-last": "Son ölçüm", "ai-ma": "Hareketli ort.",
              "ai-ewma": "EWMA", "ai-holt": "Holt", "ai-linear": "Model v1",
              "ai-linear2": "Model v2", "ai-oracle": "Kâhin",
              "ai-linear2-ett": "v2 + gecikme (1)", "ai-linear2-ettw20": "v2 + gecikme (0.2)",
              "ai-linear2r": "v2 gerçekçi veri", "ai-linear3a": "v3 (+ yaş)",
              "ai-linear3": "v3 (+ yaş + EWMA)", "ai-gbm3": "v3 Gradient Boosting"}
TEXT, MUTED, GRID, AXIS = "#0b0b0b", "#52514e", "#e1e0d9", "#c3c2b7"


# Model v3 ablasyon zinciri: (yeni, önceki). Her adım tek bir şeyi değiştirir.
V3_STEPS = [("ai-linear2r", "ai-linear2"),   # aynı girdiler, gerçekçi eğitim verisi
            ("ai-linear3a", "ai-linear2r"),  # + bilginin yaşı
            ("ai-linear3", "ai-linear3a"),   # + EWMA girdileri
            ("ai-gbm3", "ai-linear3")]       # aynı girdiler, doğrusal olmayan model
V3_MODELS = ["ai-linear3", "ai-gbm3"]


def ci95(x):
    x = pd.Series(x).dropna()
    n = len(x)
    return stats.t.ppf(0.975, n - 1) * x.std(ddof=1) / n**0.5 if n > 1 else float("nan")


def load():
    files = sorted(RAW.glob("*.csv"))
    if not files:
        raise SystemExit("data/eval/raw bos: once ./scripts/pipeline.sh deney")
    df = pd.concat([pd.read_csv(f) for f in files], ignore_index=True)
    df["plane"] = np.where(df["routing"].str.endswith("-ib"), "inband",
                           np.where(df["routing"].str.startswith("ai-"), "ideal", "-"))
    df["method"] = df["routing"].str.replace("-ib", "", regex=False)
    return df


def order(values, ref):
    return [v for v in ref if v in set(values)]


def summary(df):
    rows = []
    for (sc, m, pl), g in df.groupby(["scenario", "method", "plane"]):
        r = {"scenario": sc, "method": m, "plane": pl, "n": len(g),
             "pdr": g["pdr"].mean(), "pdr_ci95": ci95(g["pdr"]),
             "delay_ms": g["delay_ms"].mean(), "throughput_kbps": g["throughput_kbps"].mean()}
        if pl == "inband":
            for c in ("ctrl_tx_per_s", "report_dr", "update_dr", "ctrl_delay_ms"):
                r[c] = g[c].mean()
        rows.append(r)
    s = pd.DataFrame(rows)
    s.to_csv(REPO / "results/eval_ozet.csv", index=False, float_format="%.4f")
    return s


def paired(df, sc, a, b, metric="pdr"):
    """a - b farkı, aynı seed'lerde. a, b: (method, plane). metric: pdr ya da delay_ms."""
    sub = df[df["scenario"] == sc]
    x = sub[(sub["method"] == a[0]) & (sub["plane"] == a[1])].set_index("seed")[metric]
    y = sub[(sub["method"] == b[0]) & (sub["plane"] == b[1])].set_index("seed")[metric]
    d = (x - y).dropna()
    if len(d) < 2:
        return None
    pw = stats.wilcoxon(d).pvalue if (d != 0).any() else 1.0
    return {"n": len(d), "fark": d.mean(), "fark_ci95": ci95(d), "ilk_daha_iyi": int((d > 0).sum()),
            "p_ttest": stats.ttest_1samp(d, 0).pvalue if d.std() > 0 else 1.0, "p_wilcoxon": pw}


def tests(df):
    rows = []
    for sc in order(df["scenario"], SCEN_NAMES):
        for pl in ("ideal", "inband"):
            comps = [("ai-linear2", "ai-last"), ("ai-linear2", "ai-ewma"), ("ai-linear2", "ai-holt"),
                     ("ai-ewma", "ai-last"), ("ai-oracle", "ai-linear2")]
            # Model v3 ablasyonu: her adım bir öncekine göre (kazanç nereden geliyor?) ve
            # klasik rakip EWMA'ya göre
            comps += V3_STEPS + [(m, "ai-ewma") for m in V3_MODELS]
            for a, b in comps:
                r = paired(df, sc, (a, pl), (b, pl))
                if r:
                    rows.append({"scenario": sc, "plane": pl, "karsilastirma": f"{a} - {b}", **r})
            # Gecikmeli maliyet (ETT) varyantları: PDR ve uçtan uca gecikme farkı
            for ett in ("ai-linear2-ett", "ai-linear2-ettw20"):
                for metric, tag in (("pdr", ""), ("delay_ms", " [gecikme ms]")):
                    r = paired(df, sc, (ett, pl), ("ai-linear2", pl), metric)
                    if r:
                        rows.append({"scenario": sc, "plane": pl,
                                     "karsilastirma": f"{ett} - ai-linear2{tag}", **r})
            for ref in ("aodv", "olsr"):
                r = paired(df, sc, ("ai-linear2", pl), (ref, "-"))
                if r:
                    rows.append({"scenario": sc, "plane": pl, "karsilastirma": f"ai-linear2 - {ref}",
                                 **r})
        r = paired(df, sc, ("ai-linear2", "inband"), ("ai-linear2", "ideal"))
        if r:
            rows.append({"scenario": sc, "plane": "-", "karsilastirma": "linear2 inband - ideal",
                         **r})
    cols = ["scenario", "plane", "karsilastirma", "n", "fark", "fark_ci95", "ilk_daha_iyi",
            "p_ttest", "p_wilcoxon"]
    t = pd.DataFrame(rows, columns=cols)
    t.to_csv(REPO / "results/eval_test.csv", index=False, float_format="%.4f")
    return t


def columns(s):
    """Isı haritası sütunları: AODV, OLSR, sonra AI tahmincileri (ideal, in-band)."""
    cols = [(m, "-") for m in order(s["method"], ["aodv", "olsr"])]
    for pl in ("ideal", "inband"):
        for m in order(s[s["plane"] == pl]["method"], METH_NAMES):
            cols.append((m, pl))
    return cols


def fig_heatmap(s):
    scen = order(s["scenario"], SCEN_NAMES)
    cols = columns(s)
    M = np.full((len(scen), len(cols)), np.nan)
    for i, sc in enumerate(scen):
        for j, (m, pl) in enumerate(cols):
            r = s[(s["scenario"] == sc) & (s["method"] == m) & (s["plane"] == pl)]
            if len(r):
                M[i, j] = r["pdr"].iloc[0]
    fig, ax = plt.subplots(figsize=(1.0 + 0.82 * len(cols), 0.9 + 0.48 * len(scen)), dpi=150)
    # Tek tonlu (sıralı) renk skalası: açık = düşük PDR, koyu = yüksek
    cmap = matplotlib.colors.LinearSegmentedColormap.from_list(
        "pdr", ["#f0efec", "#cde2fb", "#6da7ec", "#2a78d6", "#184f95"])
    ax.imshow(M, cmap=cmap, vmin=40, vmax=100, aspect="auto")
    for i in range(len(scen)):
        for j in range(len(cols)):
            if not np.isnan(M[i, j]):
                ax.text(j, i, f"{M[i, j]:.0f}", ha="center", va="center", fontsize=8.5,
                        color="white" if M[i, j] > 82 else TEXT)
    labels = [METH_NAMES.get(m, m) + ("" if pl == "-" else ("\n(ideal)" if pl == "ideal"
                                                             else "\n(in-band)"))
              for m, pl in cols]
    ax.set_xticks(range(len(cols)), labels, fontsize=7.5, rotation=0)
    ax.set_yticks(range(len(scen)), [SCEN_NAMES[x] for x in scen], fontsize=9)
    ax.tick_params(length=0, colors=TEXT)
    for sp in ax.spines.values():
        sp.set_visible(False)
    # İdeal ve in-band bloklarını ayıran ince çizgiler
    n_classic = sum(1 for _, pl in cols if pl == "-")
    n_ideal = sum(1 for _, pl in cols if pl == "ideal")
    for x in (n_classic - 0.5, n_classic + n_ideal - 0.5):
        ax.axvline(x, color="white", lw=3)
    ax.set_title("Paket teslim oranı (%) - senaryo × yöntem (seed ortalaması)", loc="left",
                 fontsize=11, color=TEXT)
    fig.tight_layout()
    fig.savefig(FIG / "eval_pdr.png")
    plt.close(fig)


def fig_gain(t):
    scen = [x for x in SCEN_NAMES if x in set(t["scenario"])]
    series = [("ai-linear2 - ai-last", "ideal", "Model v2 − son ölçüm (ideal)", "#1baf7a", True),
              ("ai-linear2 - ai-last", "inband", "Model v2 − son ölçüm (in-band)", "#1baf7a", False),
              ("ai-linear2 - ai-ewma", "ideal", "Model v2 − EWMA (ideal)", "#6250d6", True),
              ("ai-linear2 - ai-ewma", "inband", "Model v2 − EWMA (in-band)", "#6250d6", False)]
    fig, ax = plt.subplots(figsize=(8.5, 0.8 + 0.62 * len(scen)), dpi=150)
    for k, (comp, pl, label, color, filled) in enumerate(series):
        for i, sc in enumerate(scen):
            r = t[(t["scenario"] == sc) & (t["plane"] == pl) & (t["karsilastirma"] == comp)]
            if not len(r):
                continue
            y = i + (k - 1.5) * 0.16
            v, c = r["fark"].iloc[0], r["fark_ci95"].iloc[0]
            ax.plot([v - c, v + c], [y, y], color=color, lw=1.4)
            ax.plot([v], [y], "o", ms=7, color=color, mfc=color if filled else "white", mew=1.6,
                    label=label if i == 0 else None)
    ax.axvline(0, color=MUTED, lw=1)
    ax.set_yticks(range(len(scen)), [SCEN_NAMES[x] for x in scen])
    ax.invert_yaxis()
    ax.set_xlabel("PDR farkı (puan, %95 GA) - sağ taraf: model daha iyi", color=MUTED)
    ax.grid(True, axis="x", color=GRID)
    ax.set_axisbelow(True)
    for sp in ("top", "right", "left"):
        ax.spines[sp].set_visible(False)
    ax.spines["bottom"].set_color(AXIS)
    ax.tick_params(colors=MUTED)
    # Açıklama grafiğin altında (başlıkla çakışmasın)
    ax.legend(frameon=False, fontsize=8, ncol=2, loc="upper left", bbox_to_anchor=(0, -0.13))
    ax.set_title("Öğrenilmiş model, öğrenmeyen tahmincilere göre ne kazandırıyor?", loc="left",
                 fontsize=11, color=TEXT)
    fig.tight_layout()
    fig.savefig(FIG / "eval_model_kazanci.png")
    plt.close(fig)


def fmt_p(p):
    return "<0.001" if p < 0.001 else f"{p:.3f}"


def markdown(s, t):
    scen = order(s["scenario"], SCEN_NAMES)
    cols = columns(s)
    L = ["# Sonuç özeti (otomatik üretildi: analysis/evaluate.py)", "",
         "Bu dosya `./scripts/pipeline.sh` her çalıştığında yeniden yazılır. Ayarlar: "
         "`scripts/deney_ayarlari.sh`.", ""]
    n = int(s["n"].min()), int(s["n"].max())
    L += [f"Seed sayısı (yöntem başına): {n[0]}-{n[1]}. Değerler seed ortalamasıdır.", ""]
    L += ["## Paket teslim oranı (%)", ""]
    head = ["Senaryo"] + [METH_NAMES.get(m, m) + ("" if pl == "-" else f" ({pl})") for m, pl in cols]
    L += ["| " + " | ".join(head) + " |", "|" + " --- |" * len(head)]
    for sc in scen:
        row = [SCEN_NAMES[sc]]
        for m, pl in cols:
            r = s[(s["scenario"] == sc) & (s["method"] == m) & (s["plane"] == pl)]
            row.append(f"{r['pdr'].iloc[0]:.1f}" if len(r) else "-")
        L.append("| " + " | ".join(row) + " |")
    L += ["", "## Öğrenilmiş model (v2) ne kazandırıyor?", "",
          "Fark = model v2 PDR − rakip PDR (puan), aynı seed'lerde eşli. p: eşli t-testi.", "",
          "| Senaryo | Düzlem | v2 − son ölçüm | v2 − EWMA | v2 − Holt | Kâhin − v2 |",
          "| --- | --- | --- | --- | --- | --- |"]
    for sc in scen:
        for pl in ("ideal", "inband"):
            cells = []
            for comp in ("ai-linear2 - ai-last", "ai-linear2 - ai-ewma", "ai-linear2 - ai-holt",
                         "ai-oracle - ai-linear2"):
                r = t[(t["scenario"] == sc) & (t["plane"] == pl) & (t["karsilastirma"] == comp)]
                cells.append(f"{r['fark'].iloc[0]:+.1f} (p {fmt_p(r['p_ttest'].iloc[0])})"
                             if len(r) else "-")
            L.append(f"| {SCEN_NAMES[sc]} | {pl} | " + " | ".join(cells) + " |")
    L += ["", "## Klasik protokollere karşı (model v2)", "",
          "| Senaryo | Düzlem | v2 − AODV | v2 − OLSR |", "| --- | --- | --- | --- |"]
    for sc in scen:
        for pl in ("ideal", "inband"):
            cells = []
            for comp in ("ai-linear2 - aodv", "ai-linear2 - olsr"):
                r = t[(t["scenario"] == sc) & (t["plane"] == pl) & (t["karsilastirma"] == comp)]
                cells.append(f"{r['fark'].iloc[0]:+.1f} ({r['ilk_daha_iyi'].iloc[0]}/"
                             f"{r['n'].iloc[0]} seed önde)" if len(r) else "-")
            L.append(f"| {SCEN_NAMES[sc]} | {pl} | " + " | ".join(cells) + " |")
    if t["karsilastirma"].str.startswith("ai-linear2-ett").any():
        L += ["", "## Gecikmeli maliyet (ETT) ne kazandırıyor?", "",
              "Model v2 aynı; sadece link maliyeti değişiyor: ETX yerine beklenen iletim süresi "
              "+ ağırlık × ölçülen gecikme. Fark = ETT − ETX (aynı seed'ler). PDR'da + iyi; "
              "gecikmede − iyi.", "",
              "| Senaryo | Düzlem | ağırlık 1: PDR | ağırlık 1: gecikme (ms) "
              "| ağırlık 0.2: PDR | ağırlık 0.2: gecikme (ms) |",
              "| --- | --- | --- | --- | --- | --- |"]
        for sc in scen:
            for pl in ("ideal", "inband"):
                cells = []
                for ett in ("ai-linear2-ett", "ai-linear2-ettw20"):
                    for tag in ("", " [gecikme ms]"):
                        comp = f"{ett} - ai-linear2{tag}"
                        r = t[(t["scenario"] == sc) & (t["plane"] == pl) &
                              (t["karsilastirma"] == comp)]
                        cells.append(f"{r['fark'].iloc[0]:+.1f} (p {fmt_p(r['p_ttest'].iloc[0])})"
                                     if len(r) else "-")
                L.append(f"| {SCEN_NAMES[sc]} | {pl} | " + " | ".join(cells) + " |")
    if t["karsilastirma"].isin([f"{a} - {b}" for a, b in V3_STEPS]).any():
        names = {"ai-linear2r - ai-linear2": "gerçekçi veri", "ai-linear3a - ai-linear2r": "+ yaş",
                 "ai-linear3 - ai-linear3a": "+ EWMA", "ai-gbm3 - ai-linear3": "doğrusal olmayan"}
        comps = list(names) + [f"{m} - ai-ewma" for m in V3_MODELS]
        L += ["", "## Model v3: kazanç nereden geliyor? (ablasyon)", "",
              "Her sütun bir adımın katkısı: bir önceki modele göre eşli fark (puan). Son iki "
              "sütun klasik rakip EWMA'ya göre fark. p: eşli t-testi. Ayrıntı: ml/train_v3.py.", "",
              "| Senaryo | Düzlem | " + " | ".join(names.values()) +
              " | v3 lineer − EWMA | v3 GB − EWMA |",
              "|" + " --- |" * (2 + len(comps))]
        for sc in scen:
            for pl in ("ideal", "inband"):
                cells = []
                for comp in comps:
                    r = t[(t["scenario"] == sc) & (t["plane"] == pl) & (t["karsilastirma"] == comp)]
                    cells.append(f"{r['fark'].iloc[0]:+.1f} (p {fmt_p(r['p_ttest'].iloc[0])})"
                                 if len(r) else "-")
                L.append(f"| {SCEN_NAMES[sc]} | {pl} | " + " | ".join(cells) + " |")
    ib = s[(s["plane"] == "inband") & (s["method"] == "ai-linear2")]
    if len(ib):
        L += ["", "## Gerçekçi kontrol düzlemi (model v2, in-band)", "",
              "| Senaryo | Kontrol paketi/s | Rapor teslimi | Güncelleme teslimi | Kontrol gecikmesi |",
              "| --- | --- | --- | --- | --- |"]
        for sc in scen:
            r = ib[ib["scenario"] == sc]
            if len(r):
                r = r.iloc[0]
                L.append(f"| {SCEN_NAMES[sc]} | {r['ctrl_tx_per_s']:.0f} | %{100 * r['report_dr']:.0f}"
                         f" | %{100 * r['update_dr']:.0f} | {r['ctrl_delay_ms']:.0f} ms |")
    L += ["", "Grafikler: `results/figures/eval_pdr.png`, `results/figures/eval_model_kazanci.png`",
          ""]
    (REPO / "results/SONUCLAR.md").write_text("\n".join(L))


def main():
    df = load()
    s = summary(df)
    t = tests(df)
    FIG.mkdir(parents=True, exist_ok=True)
    fig_heatmap(s)
    fig_gain(t)
    markdown(s, t)
    print(f"{len(df)} simulasyon sonucu okundu")
    print("Ozet   : results/eval_ozet.csv, results/eval_test.csv, results/SONUCLAR.md")
    print("Grafik : results/figures/eval_pdr.png, results/figures/eval_model_kazanci.png")


if __name__ == "__main__":
    main()
