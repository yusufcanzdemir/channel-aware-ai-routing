"""Düzensiz yerleşim ve ölçeklenme (deney C + D): sonuçlar 4x4 ızgaraya özel mi?

Rastgele yerleşimli 20 ve 30 düğümlü ağlarda (scripts/topoloji.sh) yöntemlerin teslim oranı,
ızgaradaki (16 düğüm) sonuçlarla yan yana. Model ızgarada eğitildi, burada hiç görmediği
topolojilerde çalışıyor.

Kullanım:
    python analysis/topoloji.py            # tablolar
    python analysis/topoloji.py --cizim    # + örnek topoloji çizimi (birkaç kısa simülasyon)
Çıktılar: results/TOPOLOJI.md, results/topoloji.csv, results/figures/topoloji_ornek.png
"""

import argparse
import re
import subprocess
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from scipy import stats

REPO = Path(__file__).resolve().parent.parent
RAW = REPO / "data/eval/raw"
NS3 = Path.home() / "Documents/_projects/ns-allinone-3.44/ns-3.44"
SCEN = ["none", "fading", "degrade", "interference", "jamming", "nodefail", "congestion"]
SCEN_NAMES = {"none": "Bozulmasız", "fading": "Fading", "degrade": "Kademeli bozulma",
              "interference": "Girişim", "jamming": "Jamming", "nodefail": "Düğüm arızası",
              "congestion": "Tıkanıklık"}
METHODS = [("aodv-np", "-", "AODV"), ("olsr-np", "-", "OLSR"),
           ("ai-last", "inband", "Son ölçüm"), ("ai-ewma", "inband", "EWMA"),
           ("ai-linear2", "inband", "v2"), ("ai-linear3", "inband", "v3")]
TEXT, MUTED, GRID, AXIS = "#0b0b0b", "#52514e", "#e1e0d9", "#c3c2b7"


def load() -> pd.DataFrame:
    rows = []
    for f in RAW.glob("*.csv"):
        d = pd.read_csv(f)
        rows.append(d.iloc[-1])
    df = pd.DataFrame(rows)
    df["plane"] = np.where(df["routing"].str.endswith("-ib"), "inband",
                           np.where(df["routing"].str.startswith("ai-"), "ideal", "-"))
    m = df["routing"].str.extract(r"^(.*?)(?:-rnd(\d+))?(?:-ib)?$")
    df["method"], df["rnd"] = m[0], m[1]
    # Izgara: ana matrisin yoklamasız AODV/OLSR'si ve ana ayardaki AI yöntemleri
    df["topoloji"] = np.where(df["rnd"].notna(), "rastgele " + df["rnd"].fillna("") + " düğüm",
                              "ızgara 16 düğüm")
    return df


def paired(df, topo, sc, a, b):
    sub = df[(df["topoloji"] == topo) & (df["scenario"] == sc)]
    x = sub[(sub["method"] == a[0]) & (sub["plane"] == a[1])].set_index("seed")["pdr"]
    y = sub[(sub["method"] == b[0]) & (sub["plane"] == b[1])].set_index("seed")["pdr"]
    d = (x - y).dropna()
    if len(d) < 2 or d.std() == 0:
        return None
    return d.mean(), stats.ttest_1samp(d, 0).pvalue


def tables(df):
    topos = ["ızgara 16 düğüm"] + sorted(t for t in df["topoloji"].unique() if "rastgele" in t)
    rows = []
    for topo in topos:
        for sc in SCEN:
            for m, pl, name in METHODS:
                s = df[(df["topoloji"] == topo) & (df["scenario"] == sc) & (df["method"] == m)
                       & (df["plane"] == pl)]
                if len(s):
                    rows.append({"topoloji": topo, "senaryo": sc, "yontem": name,
                                 "pdr": s["pdr"].mean(), "n": len(s)})
    t = pd.DataFrame(rows)
    t.to_csv(REPO / "results/topoloji.csv", index=False, float_format="%.2f")
    L = ["# Düzensiz yerleşim ve ölçeklenme (otomatik: analysis/topoloji.py)", "",
         "Rastgele yerleşim: kare alan, ortalama ~4 komşu, her seed farklı topoloji. Model "
         "ızgarada eğitildi (yeniden eğitilmedi). AI yöntemleri gerçekçi (in-band) düzlemde; "
         "AODV/OLSR yoklamasız.", ""]
    for topo in topos:
        sub = t[t["topoloji"] == topo]
        if sub.empty:
            continue
        names = [n for _, _, n in METHODS if n in set(sub["yontem"])]
        L += [f"## {topo}: teslim oranı (%)", "",
              "| Senaryo | " + " | ".join(names) + " | v3 − EWMA | v3 − AODV |",
              "|" + " --- |" * (len(names) + 3)]
        for sc in SCEN:
            r = sub[sub["senaryo"] == sc].set_index("yontem")["pdr"]
            if r.empty:
                continue
            cells = [f"{r[n]:.1f}" if n in r else "-" for n in names]
            for b in (("ai-ewma", "inband"), ("aodv-np", "-")):
                p = paired(df, topo, sc, ("ai-linear3", "inband"), b)
                cells.append(f"{p[0]:+.1f} (p {p[1]:.3f})" if p else "-")
            L.append(f"| {SCEN_NAMES[sc]} | " + " | ".join(cells) + " |")
        L.append("")
    (REPO / "results/TOPOLOJI.md").write_text("\n".join(L) + "\n")
    print("Cikti: results/TOPOLOJI.md, results/topoloji.csv")


def draw():
    """Birkaç örnek rastgele topoloji: düğümler, sönümlemesiz kararlı linkler, kontrolcü."""
    cases = [(20, 101), (20, 102), (30, 101)]
    fig, axes = plt.subplots(1, len(cases), figsize=(10.5, 3.8), dpi=150)
    for ax, (n, seed) in zip(axes, cases):
        out = subprocess.run(
            ["./ns3", "run", "--no-build",
             f"ai_routing --routing=aodv --topology=random --nodes={n} --seed={seed} "
             f"--scenario=none --simTime=31 --csv=/dev/null"],
            cwd=NS3, capture_output=True, text=True).stdout
        pos = np.array([[float(v) for v in l.split()[2:4]]
                        for l in out.splitlines() if l.startswith("KONUM")])
        side = float(re.search(r"alan ([\d.]+) x", out).group(1))
        # Sönümlemesiz alınan güç: 16 dBm - (46.6777 + 30 log10 d) >= -81 dBm (eşik + 1 dB)
        for a in range(n):
            for b in range(a + 1, n):
                d = np.linalg.norm(pos[a] - pos[b])
                if 16 - (46.6777 + 30 * np.log10(d)) >= -81:
                    ax.plot(*zip(pos[a], pos[b]), color=AXIS, linewidth=1, zorder=1)
        # Kontrolcü: ağ merkezi (AI koşusundaki seçimle aynı kural); AODV çıktısında yazmaz,
        # burada aynı ölçütle hesaplanır
        adj = np.array([[a != b and 16 - (46.6777 + 30 * np.log10(np.linalg.norm(pos[a] - pos[b])
                                                                     + 1e-9)) >= -81
                         for b in range(n)] for a in range(n)])
        sums = []
        for c in range(n):
            hop = np.full(n, -1)
            hop[c] = 0
            frontier = [c]
            while frontier:
                nxt = []
                for u in frontier:
                    for v in np.flatnonzero(adj[u]):
                        if hop[v] < 0:
                            hop[v] = hop[u] + 1
                            nxt.append(v)
                frontier = nxt
            sums.append(hop.sum())
        ctrl = int(np.argmin(sums))
        ax.scatter(pos[:, 0], pos[:, 1], s=36, color="#2a78d6", zorder=2)
        ax.scatter(*pos[ctrl], s=110, facecolor="white", edgecolor="#eb6834", linewidth=2.5,
                   zorder=3)
        ax.set_title(f"{n} düğüm, seed {seed}", loc="left", fontsize=10, color=TEXT)
        ax.set_xlim(0, side)
        ax.set_ylim(0, side)
        ax.set_aspect("equal")
        ax.tick_params(colors=MUTED, labelsize=7)
        for sp in ax.spines.values():
            sp.set_color(AXIS)
    axes[0].set_ylabel("m", color=MUTED)
    fig.suptitle("Örnek rastgele yerleşimler (turuncu halka: kontrolcü, gri: kararlı linkler)",
                 x=0.01, ha="left", fontsize=11, color=TEXT)
    fig.tight_layout()
    fig.savefig(REPO / "results/figures/topoloji_ornek.png")
    plt.close(fig)
    print("Cizim: results/figures/topoloji_ornek.png")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cizim", action="store_true", help="örnek topoloji çizimi de üret")
    args = ap.parse_args()
    tables(load())
    if args.cizim:
        draw()


if __name__ == "__main__":
    main()
