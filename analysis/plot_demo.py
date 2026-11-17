"""Ön gösterim (demo) grafikleri.

1) senaryo_karsilastirma.png : AODV ve OLSR'nin PDR ve gecikmesi, üç senaryoda
   (bozulmasız / fading / degrade). Hata çubukları %95 güven aralığı.
2) topoloji_anlik.png        : degrade senaryosunda 3 farklı anda ağın link kalitesi
3) topoloji.gif              : aynı şeyin saniye saniye animasyonu
4) yontem_karsilastirma.png  : AODV, OLSR ve AI kontrolcü (3 tahminci), seed 101-110

Girdi: run_scenario.sh'nin ürettiği results/<senaryo>_ozet.csv ve data/link_degrade.csv,
       run_comparison.sh'nin ürettiği results/cmp_<senaryo>_ozet.csv

Kullanım:
    python analysis/plot_demo.py
    python analysis/plot_demo.py --seed 3 --routing olsr   # animasyon için başka çalıştırma
"""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.animation import FuncAnimation, PillowWriter
from matplotlib.colors import LinearSegmentedColormap

REPO = Path(__file__).resolve().parent.parent
FIG = REPO / "results/figures"

# Renkler (dataviz referans paleti): kategorik ilk iki slot + metin/çizgi tonları
C_AODV = "#2a78d6"
C_OLSR = "#eb6834"
C_TEXT = "#0b0b0b"
C_MUTED = "#52514e"
C_GRID = "#e1e0d9"
C_AXIS = "#c3c2b7"
# Link kalitesi için ıraksak (diverging) renk skalası: kırmızı = kötü, gri = orta, mavi = iyi
LINK_CMAP = LinearSegmentedColormap.from_list("link", ["#e34948", "#c3c2b7", "#2a78d6"])

# Yöntemler ve sabit renkleri (kategorik palet, sabit sıra; renk yönteme bağlı, sıraya değil)
METHODS = [
    ("aodv", "AODV", "#2a78d6"),
    ("olsr", "OLSR", "#eb6834"),
    ("ai-last", "AI (son ölçüm)", "#1baf7a"),
    ("ai-ma", "AI (hareketli ort.)", "#eda100"),
    ("ai-linear", "AI (tahmin modeli)", "#e87ba4"),
]

SCENARIOS = [("none", "Bozulmasız"), ("fading", "Fading"), ("degrade", "Kademeli bozulma")]
GRID_W = 4
SPACING = 40.0


def style(ax):
    ax.grid(True, axis="y", color=C_GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(C_AXIS)
    ax.tick_params(colors=C_MUTED)


def plot_scenarios():
    """AODV/OLSR'nin senaryolara göre PDR ve gecikmesi (iki ayrı grafik, ortak eksen yok)."""
    rows = []
    for key, name in SCENARIOS:
        f = REPO / f"results/{key}_ozet.csv"
        if not f.exists():
            print(f"UYARI: {f} yok, once ./scripts/run_scenario.sh {key} calistir")
            continue
        d = pd.read_csv(f)
        d["name"] = name
        rows.append(d)
    df = pd.concat(rows)

    fig, axes = plt.subplots(1, 2, figsize=(10, 3.6), dpi=150)
    metrics = [("pdr", "Paket teslim oranı (%)", "yüksek daha iyi"),
               ("delay_ms", "Ortalama gecikme (ms)", "düşük daha iyi")]
    names = df["name"].unique()
    x = np.arange(len(names))
    w = 0.36
    for ax, (m, title, hint) in zip(axes, metrics):
        for i, (routing, color) in enumerate([("aodv", C_AODV), ("olsr", C_OLSR)]):
            sub = df[df["routing"] == routing].set_index("name").loc[names]
            pos = x + (i - 0.5) * (w + 0.02)  # çubuklar arasında küçük boşluk
            bars = ax.bar(pos, sub[f"{m}_mean"], w, yerr=sub[f"{m}_ci95"], color=color,
                          label=routing.upper(), error_kw=dict(ecolor=C_MUTED, lw=1, capsize=3))
            for b, v in zip(bars, sub[f"{m}_mean"]):
                ax.text(b.get_x() + b.get_width() / 2, 1.5, f"{v:.0f}", ha="center",
                        va="bottom", fontsize=8, color="white", fontweight="bold")
        ax.set_xticks(x, names)
        ax.set_title(f"{title} - {hint}", loc="left", fontsize=10, color=C_TEXT)
        style(ax)
    axes[0].set_ylim(0, 105)
    axes[0].legend(frameon=False, loc="upper right", fontsize=9)
    fig.suptitle("Kanal bozulunca klasik protokoller (16 düğüm, 10 seed, %95 GA)",
                 x=0.01, ha="left", fontsize=12, color=C_TEXT)
    fig.tight_layout()
    fig.savefig(FIG / "senaryo_karsilastirma.png")
    plt.close(fig)
    print("Grafik: results/figures/senaryo_karsilastirma.png")


def plot_methods():
    """Beş yöntemin üç senaryodaki PDR'ı ve gecikmesi (modelin görmediği seed'ler)."""
    rows = []
    for key, name in SCENARIOS:
        f = REPO / f"results/cmp_{key}_ozet.csv"
        if f.exists():
            d = pd.read_csv(f)
            d["name"] = name
            rows.append(d)
    if not rows:
        print("UYARI: results/cmp_*_ozet.csv yok, once ./scripts/run_comparison.sh calistir")
        return
    df = pd.concat(rows)
    names = [n for _, n in SCENARIOS if n in set(df["name"])]
    methods = [m for m in METHODS if m[0] in set(df["routing"])]

    fig, axes = plt.subplots(2, 1, figsize=(10, 7), dpi=150)
    metrics = [("pdr", "Paket teslim oranı (%) - yüksek daha iyi"),
               ("delay_ms", "Ortalama gecikme (ms) - düşük daha iyi")]
    x = np.arange(len(names))
    w = 0.8 / len(methods)
    for ax, (m, title) in zip(axes, metrics):
        for i, (key, label, color) in enumerate(methods):
            sub = df[df["routing"] == key].set_index("name").reindex(names)
            pos = x - 0.4 + w * (i + 0.5)
            ax.bar(pos, sub[f"{m}_mean"], w * 0.92, yerr=sub[f"{m}_ci95"], color=color,
                   label=label, error_kw=dict(ecolor=C_MUTED, lw=1, capsize=2))
            # Değer etiketi hata çubuğunun üstünde, metin renginde
            for xp, v, ci in zip(pos, sub[f"{m}_mean"], sub[f"{m}_ci95"]):
                ax.text(xp, v + ci + (1.5 if m == "pdr" else 3), f"{v:.0f}", ha="center",
                        fontsize=7.5, color=C_TEXT)
        ax.set_xticks(x, names)
        ax.set_title(title, loc="left", fontsize=10, color=C_TEXT)
        style(ax)
    axes[0].set_ylim(0, 112)
    axes[0].legend(frameon=False, loc="upper left", bbox_to_anchor=(0, 1.22), ncol=5,
                   fontsize=8.5)
    fig.suptitle("Yöntem karşılaştırması (16 düğüm, modelin görmediği 10 seed, %95 GA)",
                 x=0.01, ha="left", fontsize=12, color=C_TEXT)
    fig.tight_layout()
    fig.savefig(FIG / "yontem_karsilastirma.png")
    plt.close(fig)
    print("Grafik: results/figures/yontem_karsilastirma.png")


def node_xy(i):
    return (i % GRID_W) * SPACING, (i // GRID_W) * SPACING


def link_frame(links: pd.DataFrame, t: float) -> pd.DataFrame:
    """t anında yönsüz link kalitesi: iki yönün teslim oranlarının ortalaması."""
    d = links[links["time"] == t].copy()
    d["a"] = d[["tx", "rx"]].min(axis=1)
    d["b"] = d[["tx", "rx"]].max(axis=1)
    return d.groupby(["a", "b"], as_index=False)["pdr"].mean()


def draw_topology(ax, links_t: pd.DataFrame, n_nodes: int, t: float):
    ax.clear()
    for _, r in links_t.iterrows():
        if r["pdr"] < 0.05:
            continue  # bu anda kullanılamayan link: çizme
        (x1, y1), (x2, y2) = node_xy(int(r["a"])), node_xy(int(r["b"]))
        ax.plot([x1, x2], [y1, y2], color=LINK_CMAP(r["pdr"]),
                linewidth=1 + 4 * r["pdr"], solid_capstyle="round", zorder=1)
    for i in range(n_nodes):
        x, y = node_xy(i)
        ax.scatter([x], [y], s=260, color="white", edgecolor=C_TEXT, linewidth=1.2, zorder=2)
        ax.text(x, y, str(i), ha="center", va="center", fontsize=8, color=C_TEXT, zorder=3)
    ax.set_title(f"t = {t:.0f} s", fontsize=10, color=C_TEXT)
    ax.set_xlim(-20, (GRID_W - 1) * SPACING + 20)
    ax.set_ylim((n_nodes // GRID_W - 1) * SPACING + 20, -20)  # düğüm 0 sol üstte
    ax.set_aspect("equal")
    ax.axis("off")


def add_colorbar(fig, axes):
    sm = plt.cm.ScalarMappable(cmap=LINK_CMAP, norm=plt.Normalize(0, 1))
    cb = fig.colorbar(sm, ax=axes, shrink=0.7, pad=0.02)
    cb.set_label("Link teslim oranı (1 s pencere)", color=C_MUTED, fontsize=9)
    cb.outline.set_visible(False)
    cb.ax.tick_params(colors=C_MUTED, labelsize=8)


def plot_topology(seed: int, routing: str):
    f = REPO / "data/link_degrade.csv"
    links = pd.read_csv(f)
    links = links[(links["seed"] == seed) & (links["routing"] == routing)]
    n_nodes = int(max(links["tx"].max(), links["rx"].max())) + 1
    times = sorted(links["time"].unique())

    # Anlık görüntüler: kanalın en iyi, ortanca ve en kötü olduğu üç an
    quality = links.groupby("time")["pdr"].mean()
    picks = sorted([quality.idxmax(), quality.sort_values().index[len(quality) // 2],
                    quality.idxmin()])
    fig, axes = plt.subplots(1, 3, figsize=(11, 4), dpi=150)
    for ax, t in zip(axes, picks):
        draw_topology(ax, link_frame(links, t), n_nodes, t)
    add_colorbar(fig, axes)
    fig.suptitle(f"Kademeli bozulma senaryosunda link kalitesi ({routing.upper()}, seed {seed})",
                 x=0.01, ha="left", fontsize=12, color=C_TEXT)
    fig.savefig(FIG / "topoloji_anlik.png", bbox_inches="tight")
    plt.close(fig)
    print("Grafik: results/figures/topoloji_anlik.png")

    # Animasyon: her kare 1 saniyelik ölçüm penceresi
    fig, ax = plt.subplots(figsize=(5.2, 4.6), dpi=100)
    add_colorbar(fig, ax)

    def update(t):
        draw_topology(ax, link_frame(links, t), n_nodes, t)
        ax.set_title(f"Link kalitesi ({routing.upper()}, seed {seed})   t = {t:.0f} s",
                     fontsize=10, color=C_TEXT)

    anim = FuncAnimation(fig, update, frames=times, interval=250)
    anim.save(FIG / "topoloji.gif", writer=PillowWriter(fps=4))
    plt.close(fig)
    print("Animasyon: results/figures/topoloji.gif")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--routing", default="aodv")
    args = ap.parse_args()
    FIG.mkdir(parents=True, exist_ok=True)
    plot_scenarios()
    plot_methods()
    plot_topology(args.seed, args.routing)


if __name__ == "__main__":
    main()
