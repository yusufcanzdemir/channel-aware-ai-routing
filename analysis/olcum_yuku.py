"""Ölçüm yükü ile başarı arasındaki ödünleşim (deney B).

Soru: Ölçüm (yoklama) ve kontrol (rapor + güncelleme) yükünü azaltınca teslim oranı ne kadar
düşüyor? Pratikte hangi ayar önerilmeli?

Ayarlar (gerçekçi kontrol düzlemi): saniyede 10 / 5 / 2 yoklama × rapor her saniye ya da
sadece linkler değişince. Her ayarda son ölçüm, EWMA ve o ayarın verisiyle eğitilmiş v3.
Yük, simülasyonda ÖLÇÜLEN kanal süresidir (ns3/airtime-meter.*): yoklama + kontrol
iletimlerinin havada kaldığı toplam süre / trafik süresi. (MAC onayları türüne göre
ayrılamadığı için dahil değil.) Referans: yoklamasız AODV/OLSR'nin kendi yönlendirme mesajları.

Kullanım: python analysis/olcum_yuku.py   (scripts/olcum_yuku.sh ve ana matris bittikten sonra)
Çıktılar: results/olcum_yuku.csv, results/OLCUM_YUKU.md, results/figures/olcum_yuku.png
"""

import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd
from scipy import stats

REPO = Path(__file__).resolve().parent.parent
RAW = REPO / "data/eval/raw"
DISTURBED = ["fading", "degrade", "interference", "jamming", "congestion"]
SCEN_NAMES = {"none": "Bozulmasız", "fading": "Fading", "degrade": "Kademeli bozulma",
              "interference": "Girişim", "jamming": "Jamming", "nodefail": "Düğüm arızası",
              "congestion": "Tıkanıklık"}
FAMILY = {"last": "Son ölçüm", "ewma": "EWMA", "linear3": "Model v3"}
# Mevcut grafiklerle aynı kategorik palet (dataviz referans paleti, sabit sıra)
COLORS = {"Son ölçüm": "#2a78d6", "EWMA": "#eb6834", "Model v3": "#1baf7a"}
TEXT, MUTED, GRID, AXIS = "#0b0b0b", "#52514e", "#e1e0d9", "#c3c2b7"


def parse(method: str):
    """ai-linear3_p5-p5-roc-ib -> (aile, yoklama/s, değişince rapor mu); ilgisizse None."""
    m = re.fullmatch(r"ai-(last|ewma|linear3)(?:_p\d+(?:roc)?|_roc)?(-p(\d+))?(-roc)?-ib", method)
    if not m:
        return None
    return FAMILY[m.group(1)], int(m.group(3) or 10), m.group(4) is not None


def load() -> pd.DataFrame:
    rows = []
    for f in RAW.glob("*.csv"):
        d = pd.read_csv(f)
        if "air_probe_pct" not in d.columns:
            continue
        rows.append(d.iloc[-1])
    df = pd.DataFrame(rows)
    df["overhead"] = df["air_probe_pct"] + df["air_control_pct"] + df["air_routing_pct"]
    return df


def main():
    df = load()
    ai = df[df["routing"].map(lambda m: parse(m) is not None)].copy()
    ai[["yontem", "yoklama", "degisince"]] = ai["routing"].map(parse).apply(pd.Series)
    ai["ayar"] = ai["yoklama"].astype(str) + "/s, " + ai["degisince"].map(
        {False: "her saniye rapor", True: "değişince rapor"})
    # Ayar başına, senaryo başına ortalama
    g = (ai.groupby(["yontem", "yoklama", "degisince", "ayar", "scenario"])
         .agg(pdr=("pdr", "mean"), yuk=("overhead", "mean"),
              yoklama_yuku=("air_probe_pct", "mean"), kontrol_yuku=("air_control_pct", "mean"),
              n=("seed", "nunique"))
         .reset_index())
    g.to_csv(REPO / "results/olcum_yuku.csv", index=False, float_format="%.3f")
    ref = df[df["routing"].isin(["aodv-np", "olsr-np"])]

    # --- Grafik: bozulmalı senaryoların ortalaması ---
    dist = g[g["scenario"].isin(DISTURBED)]
    avg = dist.groupby(["yontem", "yoklama", "degisince"])[["pdr", "yuk"]].mean().reset_index()
    fig, ax = plt.subplots(figsize=(7.2, 4.4), dpi=150)
    for fam, color in COLORS.items():
        for roc, ls, fill in ((False, "-", color), (True, "--", "white")):
            s = avg[(avg["yontem"] == fam) & (avg["degisince"] == roc)].sort_values("yoklama")
            if s.empty:
                continue
            ax.plot(s["yuk"], s["pdr"], ls, color=color, linewidth=2, zorder=2)
            ax.scatter(s["yuk"], s["pdr"], s=64, facecolor=fill, edgecolor=color,
                       linewidth=2, zorder=3)
            if fam == "Model v3":
                for _, r in s.iterrows():
                    ax.annotate(f"{r['yoklama']}/s", (r["yuk"], r["pdr"]),
                                textcoords="offset points", xytext=(6, -12 if roc else 6),
                                fontsize=8, color=MUTED)
    for name, label in (("aodv-np", "AODV"), ("olsr-np", "OLSR")):
        r = ref[(ref["routing"] == name) & (ref["scenario"].isin(DISTURBED))]
        if len(r):
            x = r.groupby("scenario")["overhead"].mean().mean()
            y = r.groupby("scenario")["pdr"].mean().mean()
            ax.scatter([x], [y], s=64, marker="s", color=MUTED, zorder=3)
            ax.annotate(f"{label} (yoklamasız)", (x, y), textcoords="offset points",
                        xytext=(6, 4), fontsize=8, color=MUTED)
    # Gösterge: renk = yöntem, dolu/boş = rapor biçimi
    from matplotlib.lines import Line2D
    handles = [Line2D([], [], color=c, linewidth=2, marker="o", markersize=7, label=f)
               for f, c in COLORS.items()]
    handles += [Line2D([], [], color=MUTED, linewidth=2, marker="o", markersize=7,
                       label="her saniye rapor"),
                Line2D([], [], color=MUTED, linewidth=2, linestyle="--", marker="o",
                       markerfacecolor="white", markersize=7, label="sadece değişince rapor")]
    ax.legend(handles=handles, frameon=False, fontsize=8, loc="lower right")
    ax.set_xlabel("Ölçüm + kontrol yükü (kanal süresinin %'si, ölçülen)", color=MUTED)
    ax.set_ylabel("Paket teslim oranı (%)", color=MUTED)
    ax.set_title("Ölçüm yükü ile başarı: bozulmalı 5 senaryonun ortalaması (in-band)",
                 loc="left", fontsize=11, color=TEXT)
    ax.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for sp in ("top", "right"):
        ax.spines[sp].set_visible(False)
    for sp in ("left", "bottom"):
        ax.spines[sp].set_color(AXIS)
    ax.tick_params(colors=MUTED)
    fig.tight_layout()
    (REPO / "results/figures").mkdir(parents=True, exist_ok=True)
    fig.savefig(REPO / "results/figures/olcum_yuku.png")
    plt.close(fig)

    # --- Markdown özet ---
    L = ["# Ölçüm yükü deneyi (otomatik: analysis/olcum_yuku.py)", "",
         "Yük = yoklama + kontrol iletimlerinin ölçülen kanal süresi (trafik süresinin %'si). "
         "Gerçekçi (in-band) kontrol düzlemi, 30 seed. Grafik: results/figures/olcum_yuku.png", ""]
    order = sorted(g["ayar"].unique(), key=lambda a: (-int(a.split("/")[0]), "değişince" in a))
    L += ["## Bozulmalı 5 senaryonun ortalaması", "",
          "| Ayar | Yoklama yükü | Kontrol yükü | Son ölçüm PDR | EWMA PDR | v3 PDR |",
          "| --- | --- | --- | --- | --- | --- |"]
    for a in order:
        s = dist[dist["ayar"] == a]
        cells = [a, f"%{s['yoklama_yuku'].mean():.1f}", f"%{s['kontrol_yuku'].mean():.1f}"]
        for fam in FAMILY.values():
            r = s[s["yontem"] == fam]
            cells.append(f"{r['pdr'].mean():.1f}" if len(r) else "-")
        L.append("| " + " | ".join(cells) + " |")
    if len(ref):
        L += ["", "Referans (yoklamasız, yönlendirme mesajlarının kanal süresi):", ""]
        for name, label in (("aodv-np", "AODV"), ("olsr-np", "OLSR")):
            r = ref[(ref["routing"] == name) & (ref["scenario"].isin(DISTURBED))]
            if len(r):
                L.append(f"- {label}: yük %{r['overhead'].mean():.1f}, PDR {r['pdr'].mean():.1f}")
    # v3: her ayar ana ayara (10/s, her saniye) göre, senaryo bazında eşli fark
    L += ["", "## v3: ana ayara (10/s, her saniye rapor) göre teslim oranı farkı (puan)", "",
          "Eşli t-testi (aynı seed'ler). Negatif = yük azaltınca kayıp.", "",
          "| Senaryo | " + " | ".join(a for a in order[1:]) + " |",
          "|" + " --- |" * len(order)]
    v3 = ai[ai["yontem"] == "Model v3"]
    for sc in [s for s in SCEN_NAMES if s in set(v3["scenario"])]:
        base = v3[(v3["scenario"] == sc) & (v3["ayar"] == order[0])].set_index("seed")["pdr"]
        cells = []
        for a in order[1:]:
            x = v3[(v3["scenario"] == sc) & (v3["ayar"] == a)].set_index("seed")["pdr"]
            d = (x - base).dropna()
            cells.append(f"{d.mean():+.1f} (p {stats.ttest_1samp(d, 0).pvalue:.3f})"
                         if len(d) > 1 and d.std() > 0 else "-")
        L.append(f"| {SCEN_NAMES[sc]} | " + " | ".join(cells) + " |")
    (REPO / "results/OLCUM_YUKU.md").write_text("\n".join(L) + "\n")
    print("Cikti: results/OLCUM_YUKU.md, results/olcum_yuku.csv, results/figures/olcum_yuku.png")


if __name__ == "__main__":
    main()
