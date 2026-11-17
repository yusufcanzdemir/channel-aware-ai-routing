"""Link kalitesi tahmin modeli - ilk sürüm.

Soru: Bir linkin son birkaç saniyelik ölçümlerine bakarak, önümüzdeki H saniyedeki
ortalama teslim oranını (link PDR) tahmin edebilir miyiz?

Girdi (her link, her saniye):
    - son 10 penceredeki teslim oranı (pdr_t, pdr_t-1, ..., pdr_t-9)
    - son 3 penceredeki ortalama SNR (hiç çerçeve duyulmadıysa eksik -> 0 + bayrak)
    - kısa vadeli eğilimler (pdr_t - pdr_t-2, snr_t - snr_t-2), 5 ve 10 pencerelik ortalama
Hedef:
    - (pdr_(t+1) + ... + pdr_(t+H)) / H   (varsayılan H = 3 s)
    Tek bir sonraki saniye yerine ortalama kullanılır: 10 yoklamayla ölçülen tek pencere
    çok gürültülü, yönlendirme için de önemli olan linkin "yakın gelecekteki" kalitesi.

Karşılaştırılan yöntemler:
    - "Son değer"         : gelecek = şimdiki değer (AODV/OLSR'nin zımnen yaptığı varsayım)
    - "Hareketli ort."    : son 5 pencerenin ortalaması
    - Lineer regresyon    : en hafif öğrenen model, cihazda çalıştırması çok kolay
    - Gradient Boosting   : küçük karar ağaçları topluluğu; doğrusal olmayan ilişkileri
                            yakalar, yine de hafif (HistGradientBoostingRegressor)

Not (ilk denemeden öğrenilen): "fading" senaryosunda kanal hafızasız (Markov) bir süreç
olduğu için en iyi tahmin zaten "son değer"e çok yakın; modeller onu geçemedi.
Kademeli bozulma olayları içeren "degrade" senaryosunda geçmişteki eğilim bilgi taşır.

Eğitim/test ayrımı seed'e göre yapılır (seed 1-7 eğitim, 8-10 test). Aynı
simülasyonun ardışık saniyeleri birbirine çok benzediği için rastgele satır
bölmek modelin başarısını olduğundan iyi gösterirdi.

Kullanım:
    python ml/train_link_model.py                        # data/link_degrade.csv
    python ml/train_link_model.py data/link_fading.csv --horizon 5
    python ml/train_link_model.py --offset 2   # kontrol gecikmesi 2 s: t+3..t+5 tahmin edilir
"""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # ekran olmadan PNG üret
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from sklearn.ensemble import HistGradientBoostingRegressor
from sklearn.inspection import permutation_importance
from sklearn.linear_model import LinearRegression
from sklearn.metrics import mean_absolute_error, mean_squared_error

REPO = Path(__file__).resolve().parent.parent
N_LAGS = 10  # kaç geçmiş pencere kullanılacak
BAD_LINK = 0.5  # teslim oranı bunun altındaysa "kötü link" sayılır
# NS-3'ün algılama eşiği (ThresholdPreambleDetectionModel::MinimumRssi). Sinyal "payı" buna göre.
DETECT_DBM = -82.0
# Hiç çerçeve duyulmayan pencerede pay bilinmez: eşiğin biraz altında varsayılır
MARGIN_UNKNOWN = -3.0
# v2: ters yön linkinden alınan öznitelikler (kanal iki yönde aynı sönümleniyor)
REVERSE_COLS = ["pdr_l0", "pdr_l1", "pdr_l2", "pdr_mean5", "pdr_mean10", "pdr_trend",
                "mar_l0", "mar_mean5", "mar_slope"]

# Grafik renkleri (dataviz referans paletinden: kategorik sıra + metin/çizgi tonları)
C_TRUE = "#0b0b0b"
C_SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#6250d6"]
C_MUTED = "#52514e"
C_GRID = "#e1e0d9"


def build_features(df: pd.DataFrame, horizon: int, offset: int = 0,
                   version: int = 1) -> pd.DataFrame:
    """Her (çalıştırma, link) zaman serisinden gecikmeli öznitelikler ve hedef üretir.

    version 2 ek olarak: sinyal payı (RSSI - algılama eşiği) geçmişi/eğimi ve ters yön
    linkinin öznitelikleri. C++ karşılığı: ns3/ai-controller.cc (Features).
    """
    df = df.sort_values(["routing", "seed", "tx", "rx", "time"]).copy()
    # SNR yalnızca çerçeve duyulduysa bilinir; bilinmiyorsa 0 yaz ve bayrak koy
    df["snr_known"] = df["snr_db"].notna().astype(float)
    df["snr"] = df["snr_db"].fillna(0.0)

    g = df.groupby(["routing", "seed", "tx", "rx"], sort=False)
    for k in range(N_LAGS):
        df[f"pdr_l{k}"] = g["pdr"].shift(k)
    for k in range(3):
        df[f"snr_l{k}"] = g["snr"].shift(k)
        df[f"snrk_l{k}"] = g["snr_known"].shift(k)
    df["pdr_mean5"] = df[[f"pdr_l{k}" for k in range(5)]].mean(axis=1)
    df["pdr_mean10"] = df[[f"pdr_l{k}" for k in range(N_LAGS)]].mean(axis=1)
    df["pdr_trend"] = df["pdr_l0"] - df["pdr_l2"]
    # SNR eğilimi yalnızca iki ölçüm de varsa anlamlı
    both = (df["snrk_l0"] > 0) & (df["snrk_l2"] > 0)
    df["snr_trend"] = np.where(both, df["snr_l0"] - df["snr_l2"], 0.0)

    if version >= 2:
        # Sinyal payı: alınan gücün algılama eşiğinin kaç dB üstünde olduğu.
        # Pay 0'a yaklaştıkça link kopmaya yaklaşır.
        df["margin"] = (df["rssi_dbm"] - DETECT_DBM).fillna(MARGIN_UNKNOWN)
        g = df.groupby(["routing", "seed", "tx", "rx"], sort=False)
        for k in range(5):
            df[f"mar_l{k}"] = g["margin"].shift(k)
        mar = [f"mar_l{k}" for k in range(5)]
        df["mar_mean5"] = df[mar].mean(axis=1)
        df["mar_max5"] = df[mar].max(axis=1)
        # Son 5 noktaya oturtulan doğrunun eğimi (dB/s), en küçük kareler:
        # eğim = Σ (x - 2) * y / 10, x = 4 (en eski) ... 0 (en yeni) yerine zaman ekseni
        df["mar_slope"] = (2 * df["mar_l0"] + df["mar_l1"] - df["mar_l3"] - 2 * df["mar_l4"]) / 10
        # Ters yön linki (rx -> tx) aynı andaki öznitelikleri; ölçümü yoksa "kopuk" kabul edilir
        rev = df[["routing", "seed", "tx", "rx", "time"] + REVERSE_COLS].rename(
            columns={"tx": "rx", "rx": "tx", **{c: "rev_" + c for c in REVERSE_COLS}})
        df = df.merge(rev, on=["routing", "seed", "tx", "rx", "time"], how="left")
        # Ters link hiç duyulmadıysa: teslim 0, pay sabit MARGIN_UNKNOWN, dolayısıyla eğim 0
        # (C++ tarafı da hiç duyulmayan linki böyle görür)
        for c in REVERSE_COLS:
            fill = MARGIN_UNKNOWN if c in ("mar_l0", "mar_mean5") else 0.0
            df["rev_" + c] = df["rev_" + c].fillna(fill)
        g = df.groupby(["routing", "seed", "tx", "rx"], sort=False)
    # Hedef: offset kadar pencere atlandıktan sonraki H pencerenin ortalaması.
    # offset = kontrol gecikmesi: rotalar ancak offset saniye sonra uygulanacağı için
    # asıl önemli olan o andan sonraki link kalitesi.
    ks = range(offset + 1, offset + horizon + 1)
    df["target"] = sum(g["pdr"].shift(-k) for k in ks) / horizon
    return df.dropna(subset=[f"pdr_l{N_LAGS - 1}", "target"])


def feature_columns(version: int = 1) -> list:
    cols = [f"pdr_l{k}" for k in range(N_LAGS)]
    cols += [f"snr_l{k}" for k in range(3)] + [f"snrk_l{k}" for k in range(3)]
    cols += ["pdr_mean5", "pdr_mean10", "pdr_trend", "snr_trend"]
    if version >= 2:
        cols += [f"mar_l{k}" for k in range(5)] + ["mar_mean5", "mar_max5", "mar_slope"]
        cols += ["rev_" + c for c in REVERSE_COLS]
    return cols


def scores(y_true, y_pred) -> dict:
    """Regresyon hatası + 'kötüleşecek linki önceden yakalama' başarısı."""
    bad_true = y_true < BAD_LINK
    bad_pred = y_pred < BAD_LINK
    tp = np.sum(bad_true & bad_pred)
    precision = tp / max(bad_pred.sum(), 1)
    recall = tp / max(bad_true.sum(), 1)
    return {
        "MAE": mean_absolute_error(y_true, y_pred),
        "RMSE": mean_squared_error(y_true, y_pred) ** 0.5,
        "kotu_link_precision": precision,
        "kotu_link_recall": recall,
        "kotu_link_F1": 2 * precision * recall / max(precision + recall, 1e-9),
    }


def style(ax):
    """Sade eksen görünümü: ince ızgara, üst/sağ çerçeve yok."""
    ax.grid(True, color=C_GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color("#c3c2b7")
    ax.tick_params(colors=C_MUTED)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="?", default=str(REPO / "data/link_degrade.csv"))
    ap.add_argument("--offset", type=int, default=0,
                    help="kontrol gecikmesi (s): hedef bu kadar ileriden başlar")
    ap.add_argument("--horizon", type=int, default=3,
                    help="önümüzdeki kaç saniyenin ortalaması tahmin edilsin")
    ap.add_argument("--features", type=int, default=1, choices=[1, 2],
                    help="öznitelik sürümü: 1 = ilk model, 2 = + sinyal payı + ters yön linki")
    args = ap.parse_args()

    raw = pd.read_csv(args.csv)
    # Öznitelikler TÜM linklerde hesaplanır (ters yön linki filtrelenmiş olsa da gerekir),
    # sonra sadece "anlamlı" linkler tutulur: o çalıştırmada ortalama teslim oranı > %5.
    # Hep kopuk uzak düğüm çiftleri tahmini kolaylaştırıp sonuçları şişirirdi.
    mean_pdr = raw.groupby(["routing", "seed", "tx", "rx"])["pdr"].transform("mean")
    raw["link_mean_pdr"] = mean_pdr
    data = build_features(raw, args.horizon, args.offset, args.features)
    data = data[data["link_mean_pdr"] > 0.05]
    # Dosya adları: gecikmesiz model eski adını korur (linear_h3), diğerleri linear_o2_h3;
    # öznitelik sürümü 2 için linear2_... (sonuç/grafik dosyalarında v2_...)
    name = "linear" if args.features == 1 else "linear2"
    tag = f"h{args.horizon}" if args.offset == 0 else f"o{args.offset}_h{args.horizon}"
    model_file = f"{name}_{tag}"
    if args.features == 2:
        tag = "v2_" + tag
    train = data[data["seed"] <= 7]
    test = data[data["seed"] > 7]
    X_cols = feature_columns(args.features)
    print(f"Egitim: {len(train)} ornek, test: {len(test)} ornek, "
          f"{data.groupby(['tx', 'rx']).ngroups} yonlu link, ufuk = {args.horizon} s, gecikme = {args.offset} s")

    preds = {
        "Son değer": test["pdr_l0"].to_numpy(),
        "Hareketli ort.": test["pdr_mean5"].to_numpy(),
    }

    lr = LinearRegression().fit(train[X_cols], train["target"])
    preds["Lineer regresyon"] = np.clip(lr.predict(test[X_cols]), 0, 1)

    gb = HistGradientBoostingRegressor(max_iter=200, random_state=0)
    gb.fit(train[X_cols], train["target"])
    preds["Gradient Boosting"] = np.clip(gb.predict(test[X_cols]), 0, 1)

    y = test["target"].to_numpy()
    table = pd.DataFrame({name: scores(y, p) for name, p in preds.items()}).T
    base_mae = table.loc["Son değer", "MAE"]
    table["MAE_iyilesme_%"] = 100 * (base_mae - table["MAE"]) / base_mae
    base_rmse = table.loc["Son değer", "RMSE"]
    table["RMSE_iyilesme_%"] = 100 * (base_rmse - table["RMSE"]) / base_rmse
    print()
    print(table.round(3).to_string())

    out_dir = REPO / "results"
    fig_dir = out_dir / "figures"
    fig_dir.mkdir(parents=True, exist_ok=True)
    table.to_csv(out_dir / f"model_karsilastirma_{tag}.csv", float_format="%.4f")

    # Lineer modelin katsayıları: C++ AI kontrolcüsü (ns3/ai-controller.cc) bunları kullanır
    (REPO / "ml/models").mkdir(exist_ok=True)
    with open(REPO / f"ml/models/{model_file}.json", "w") as f:
        json.dump({"features": X_cols, "coef": lr.coef_.tolist(),
                   "intercept": float(lr.intercept_), "horizon_s": args.horizon,
                   "offset_s": args.offset}, f, indent=2)
    # Aynı katsayılar C++'ın kolay okuyacağı düz metin olarak: "ad katsayı" satırları
    with open(REPO / f"ml/models/{model_file}.txt", "w") as f:
        f.write(f"intercept {lr.intercept_:.10g}\n")
        for name, c in zip(X_cols, lr.coef_):
            f.write(f"{name} {c:.10g}\n")
    # C++ ile birebir aynı sonucu verdiğimizi kontrol etmek için birkaç test örneği
    check = test[X_cols].head(5).copy()
    check["pred"] = lr.predict(check[X_cols])
    check.to_csv(REPO / f"ml/models/{model_file}_check.csv", index=False,
                 float_format="%.10g")

    # --- Grafik 1: model hataları (MAE) ---
    fig, ax = plt.subplots(figsize=(6.4, 3.2), dpi=150)
    names = list(table.index)
    colors = ["#c3c2b7", "#c3c2b7", C_SERIES[0], C_SERIES[0]]  # gri: kural, mavi: öğrenen
    bars = ax.barh(names, table["RMSE"], color=colors, height=0.55)
    for b, v, imp in zip(bars, table["RMSE"], table["RMSE_iyilesme_%"]):
        label = f"{v:.3f}" + (f"  ({imp:+.0f}%)" if b.get_y() > 0.5 else "")
        ax.text(v + 0.003, b.get_y() + b.get_height() / 2, label,
                va="center", fontsize=9, color=C_TRUE)
    ax.invert_yaxis()
    ax.set_xlim(0, table["RMSE"].max() * 1.3)
    ax.set_xlabel("Tahmin hatası (RMSE) - düşük daha iyi", color=C_MUTED)
    ax.set_title(f"Önümüzdeki {args.horizon} s'nin link teslim oranı tahmini",
                 loc="left", fontsize=11, color=C_TRUE)
    style(ax)
    ax.grid(axis="y", visible=False)
    fig.tight_layout()
    fig.savefig(fig_dir / f"model_hata_{tag}.png")
    plt.close(fig)

    # --- Grafik 2: örnek bir linkte gerçek vs tahmin ---
    # En çok dalgalanan test linkini seç (göstermek için en ilginç olanı)
    test = test.assign(rf=preds["Gradient Boosting"], last=preds["Son değer"])
    key = (test.groupby(["routing", "seed", "tx", "rx"])["target"].std().idxmax())
    s = test.set_index(["routing", "seed", "tx", "rx"]).loc[key].sort_values("time")
    t_future = s["time"]  # tahminin yapıldığı an (hedef: sonraki H saniye)

    fig, ax = plt.subplots(figsize=(8, 3.8), dpi=150)
    ax.plot(t_future, s["target"], color=C_TRUE, linewidth=2,
            label=f"Gerçek (sonraki {args.horizon} s ort.)")
    ax.plot(t_future, s["last"], color=C_SERIES[1], linewidth=1.5, linestyle="--",
            label="Son değer (referans)")
    ax.plot(t_future, s["rf"], color=C_SERIES[0], linewidth=2, label="Gradient Boosting")
    ax.axhline(BAD_LINK, color=C_MUTED, linewidth=0.8, linestyle=":")
    ax.text(t_future.iloc[-1], BAD_LINK + 0.02, "kötü link eşiği", fontsize=8,
            color=C_MUTED, ha="right")
    ax.set_ylim(-0.05, 1.05)
    ax.set_xlabel("Tahminin yapıldığı an (simülasyon zamanı, s)", color=C_MUTED)
    ax.set_ylabel("Link teslim oranı", color=C_MUTED)
    ax.set_title(f"Link {key[2]} → {key[3]} ({key[0].upper()}, seed {key[1]}): "
                 "gerçek ve tahmin", loc="left", fontsize=11, color=C_TRUE)
    ax.legend(frameon=False, loc="upper left", bbox_to_anchor=(0, -0.2), ncol=3, fontsize=9)
    style(ax)
    fig.tight_layout()
    fig.savefig(fig_dir / f"tahmin_ornek_{tag}.png")
    plt.close(fig)

    # --- Grafik 3: model hangi girdilere önem veriyor ---
    # Permütasyon önemi: bir girdiyi karıştırınca hata ne kadar artıyor
    sub = test.sample(n=min(8000, len(test)), random_state=0)
    pi = permutation_importance(gb, sub[X_cols], sub["target"], n_repeats=3,
                                random_state=0, scoring="neg_mean_squared_error")
    imp = pd.Series(pi.importances_mean, index=X_cols).sort_values().tail(8)  # en önemli 8
    fig, ax = plt.subplots(figsize=(6.4, 3.2), dpi=150)
    ax.barh(imp.index, imp.values, color=C_SERIES[0], height=0.6)
    ax.set_xlabel("Önem (girdi karıştırılınca MSE artışı)", color=C_MUTED)
    ax.set_title("Modelin kullandığı girdiler", loc="left", fontsize=11, color=C_TRUE)
    style(ax)
    ax.grid(axis="y", visible=False)
    fig.tight_layout()
    fig.savefig(fig_dir / f"oznitelik_onem_{tag}.png")
    plt.close(fig)

    print(f"\nTablo   : results/model_karsilastirma_{tag}.csv")
    print(f"Grafikler: results/figures/*_{tag}.png")
    print(f"Lineer model katsayilari: ml/models/{model_file}.txt")


if __name__ == "__main__":
    main()
