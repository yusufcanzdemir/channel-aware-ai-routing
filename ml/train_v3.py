"""Model v3: bayatlığa duyarlı link kalitesi tahmini.

Sorun (v2'de): model, her saniye her linkin TAZE ölçümüyle eğitildi. Gerçekçi (in-band)
kontrol düzleminde ise kontrolcü bazı düğümlerin raporunu geç alır ya da hiç alamaz ve eski
raporu yeniden kullanır (girişimde link bilgilerinin ~1/3'ü bayat). Model bunu bilmiyordu:
sınava çalıştığından farklı sorularla giriyordu (eğitim-kullanım uyuşmazlığı).

Çözüm:
  1) Eğitim verisi kontrolcünün GÖRDÜĞÜ veriden gelir (scripts/make_train_v3.sh,
     --aiTrainCsv). Öznitelikleri C++ hesaplayıp yazar; Python yeniden hesaplamaz, böylece
     eğitimdeki ve simülasyondaki girdiler birebir aynıdır. Hedef ise linkin GERÇEK
     gelecekteki teslim oranıdır (aynı koşunun --linkCsv dosyası).
  2) Yeni girdiler: bilginin yaşı (rapor kaç pencere eski), yaşla çarpılmış girdiler
     (lineer model "bayatsa son ölçüme az güven" ilişkisini ancak böyle öğrenebilir) ve
     klasik EWMA tahmincisinin kendisi.
  3) Bütün senaryolardan ve iki kontrol düzleminden veri.

Ablasyon (kazanç nereden geliyor?):
    v2 (mevcut)       : eski model, eski veriyle eğitilmiş (ml/models/linear2_h3.txt)
    linear2r          : v2'nin girdileri, gerçekçi veriyle yeniden eğitilmiş   -> veri etkisi
    linear3a          : + bilginin yaşı                                          -> yaş etkisi
    linear3           : + EWMA girdileri                                         -> birleştirme
    gbm3              : linear3'ün girdileriyle doğrusal olmayan model (Gradient Boosting,
                        300 küçük ağaç): "daha karmaşık model gerekir mi?" C++ kontrolcü
                        ağaçları ml/models/gbm3_h3.txt'den okur (--aiPredictor=gbm3)

Değerlendirme (çevrimdışı, tahmin hatası):
    - seed 1-14 eğitim, 15-20 test (aynı koşunun ardışık saniyeleri birbirine çok benzer;
      rastgele satır bölmek başarıyı olduğundan iyi gösterirdi)
    - taze ve bayat bilgi ayrı ayrı
    - bir senaryoyu hiç göstermeden eğitip o senaryoda test (genelleme)
Asıl ölçüt yine simülasyondaki teslim oranıdır (scripts/pipeline.sh); bu betik modelleri
üretir ve tahmin hatasını raporlar.

Kullanım:
    python ml/train_v3.py
Çıktılar:
    ml/models/{linear2r,linear3a,linear3}_h3.{txt,json}   (C++ kontrolcü .txt'yi okur)
    ml/models/gbm3_h3.txt                                  (ağaçlar, C++ için düz metin)
    results/model_v3_karsilastirma.csv, results/model_v3_genelleme.csv
"""

import json
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.ensemble import HistGradientBoostingRegressor
from sklearn.linear_model import LinearRegression

REPO = Path(__file__).resolve().parent.parent
DATA = REPO / "data" / "train_v3"
HORIZON = 3          # hedef: sonraki 3 pencerenin ortalama teslim oranı (v2 ile aynı)
TRAIN_MAX_SEED = 14  # 1-14 eğitim, 15-20 test

# Öznitelik kümeleri (adlar C++'taki AiController::Features ile aynı)
V2 = json.load(open(REPO / "ml/models/linear2_h3.json"))["features"]
AGE = ["age_l0", "age_mean5", "stale_l0", "age_x_pdr0", "age_x_ewmas",
       "rev_age_l0", "rev_age_mean5"]
# Hızlı EWMA (α = 0.6, ewma_f) girdi olarak KULLANILMAZ: 0.4^10 ≈ 0.0001 olduğundan son 10
# ölçümün neredeyse tam doğrusal birleşimi; zaten var olan pdr_l0..l9 ile aynı bilgiyi taşır ve
# katsayıları patlatır (ilk denemede -793). Bu aynı zamanda önemli bir gözlem: son 10 ölçüme
# bakan lineer model EWMA'yı zaten "içerir"; EWMA'yı ancak başka bilgiyle geçebilir.
# Yavaş EWMA (α = 0.2) 10 saniyeden eskiyi de hatırlar, yeni bilgi taşır.
EWMA = ["ewma_s", "rev_ewma_f", "rev_ewma_s"]
VARIANTS = {
    "linear2r": V2,
    "linear3a": V2 + AGE,
    "linear3": V2 + AGE + EWMA,
}


def load_run(feat_path: Path) -> pd.DataFrame:
    """Bir koşunun öznitelikleri + gerçek gelecekteki teslim oranı (hedef)."""
    sc, plane, seed = feat_path.name.replace(".feat.csv", "").split("__")
    feat = pd.read_csv(feat_path)
    link = pd.read_csv(str(feat_path).replace(".feat.csv", ".link.csv"),
                       usecols=["time", "tx", "rx", "pdr"])
    # Pencere bitiş zamanları tam saniye; kayan nokta farkı olmasın diye yuvarla
    feat["time"] = feat["time"].round().astype(int)
    link["time"] = link["time"].round().astype(int)
    # Hedef: t+1 .. t+H pencerelerinin gerçek teslim oranlarının ortalaması
    link = link.sort_values(["tx", "rx", "time"])
    g = link.groupby(["tx", "rx"])["pdr"]
    link["target"] = sum(g.shift(-k) for k in range(1, HORIZON + 1)) / HORIZON
    # Linkin bu koşudaki ortalama gerçek teslim oranı (hep kopuk linkleri ayıklamak için)
    link["link_mean_pdr"] = g.transform("mean")
    df = feat.merge(link[["time", "tx", "rx", "target", "link_mean_pdr"]],
                    on=["time", "tx", "rx"], how="inner")
    df = df.dropna(subset=["target"])
    # v2 ile aynı kural: ortalama teslim oranı %5'in altındaki (hep kopuk) linkler dışarıda
    df = df[df["link_mean_pdr"] > 0.05]
    df["scenario"], df["plane"], df["seed"] = sc, plane, int(seed)
    return df


def load_all() -> pd.DataFrame:
    files = sorted(DATA.glob("*.feat.csv"))
    files = [f for f in files if Path(str(f).replace(".feat.csv", ".done")).exists()]
    if not files:
        raise SystemExit(f"Veri yok: önce ./scripts/make_train_v3.sh çalıştır ({DATA})")
    return pd.concat([load_run(f) for f in files], ignore_index=True)


def linear_from_file(name: str, df: pd.DataFrame) -> np.ndarray:
    """Var olan bir model dosyasının (ör. v2) tahmini, C++ ile aynı şekilde [0, 1]'e kırpılır."""
    m = json.load(open(REPO / f"ml/models/{name}.json"))
    y = m["intercept"] + df[m["features"]].to_numpy() @ np.array(m["coef"])
    return np.clip(y, 0, 1)


def errors(y, p) -> dict:
    return {"MAE": float(np.mean(np.abs(y - p))), "RMSE": float(np.sqrt(np.mean((y - p) ** 2)))}


def compare(train: pd.DataFrame, test: pd.DataFrame) -> tuple[dict, dict]:
    """Bütün yöntemlerin test tahminleri ve eğitilen lineer modeller."""
    preds = {
        "Son ölçüm": test["pdr_l0"].to_numpy(),
        "EWMA (α=0.6)": test["ewma_f"].to_numpy(),
        "v2 (mevcut)": linear_from_file("linear2_h3", test),
    }
    models = {}
    for name, cols in VARIANTS.items():
        lr = LinearRegression().fit(train[cols], train["target"])
        preds[name] = np.clip(lr.predict(test[cols]), 0, 1)
        models[name] = (lr, cols)
    cols = VARIANTS["linear3"]
    gb = gb_model()
    gb.fit(train[cols].to_numpy(), train["target"])
    preds["Gradient Boosting"] = np.clip(gb.predict(test[cols].to_numpy()), 0, 1)
    return preds, models


def save_model(name: str, lr: LinearRegression, cols: list, check: pd.DataFrame):
    out = REPO / "ml/models"
    with open(out / f"{name}_h{HORIZON}.json", "w") as f:
        json.dump({"features": cols, "coef": lr.coef_.tolist(),
                   "intercept": float(lr.intercept_), "horizon_s": HORIZON, "offset_s": 0,
                   "egitim": "gercekci veri (data/train_v3), 7 senaryo, ideal + inband"},
                  f, indent=2)
    with open(out / f"{name}_h{HORIZON}.txt", "w") as f:
        f.write(f"intercept {lr.intercept_:.10g}\n")
        for c, v in zip(cols, lr.coef_):
            f.write(f"{c} {v:.10g}\n")
    chk = check[cols].head(5).copy()
    chk["pred"] = lr.predict(chk[cols])
    chk.to_csv(out / f"{name}_h{HORIZON}_check.csv", index=False, float_format="%.10g")


def gb_model(seed: int = 0) -> HistGradientBoostingRegressor:
    """Doğrusal olmayan model: küçük karar ağaçlarının toplamı.

    Erken durdurma kapalı (her eğitimde aynı sayıda ağaç olsun). Ağaç başına en fazla 31
    yaprak (varsayılan); 300 ağaç. Cihazda tahmin = her ağaçta kökten yaprağa ~5 karşılaştırma.
    """
    return HistGradientBoostingRegressor(max_iter=300, early_stopping=False, random_state=seed)


def save_gbm(name: str, gb: HistGradientBoostingRegressor, cols: list, check: pd.DataFrame):
    """Ağaçları C++ kontrolcünün okuyacağı düz metne yazar.

    Biçim:
        gbm
        features <k> <ad1> ... <adk>
        baseline <değer>
        trees <T>
        tree <düğüm sayısı>
        <öznitelik no> <eşik> <sol> <sağ> <yaprak mı (0/1)> <yaprak değeri>    (düğüm başına)
    Tahmin = baseline + Σ (her ağaçta ulaşılan yaprağın değeri). Bir düğümde
    x[öznitelik] <= eşik ise sola, değilse sağa gidilir. (Öğrenme oranı yaprak değerlerine
    zaten katılmıştır; girdilerde eksik değer yoktur.)
    """
    trees = [t[0].nodes for t in gb._predictors]
    path = REPO / f"ml/models/{name}_h{HORIZON}.txt"
    with open(path, "w") as f:
        f.write("gbm\n")
        f.write(f"features {len(cols)} " + " ".join(cols) + "\n")
        f.write(f"baseline {float(np.ravel(gb._baseline_prediction)[0]):.17g}\n")
        f.write(f"trees {len(trees)}\n")
        for nodes in trees:
            f.write(f"tree {len(nodes)}\n")
            for nd in nodes:
                f.write(f"{int(nd['feature_idx'])} {float(nd['num_threshold']):.17g} "
                        f"{int(nd['left'])} {int(nd['right'])} {int(nd['is_leaf'])} "
                        f"{float(nd['value']):.17g}\n")
    # Kontrol: dosyadaki ağaçlarla elle hesaplanan tahmin scikit-learn ile aynı mı?
    x = check[cols].head(200).to_numpy()
    manual = np.full(len(x), float(np.ravel(gb._baseline_prediction)[0]))
    for nodes in trees:
        for i, row in enumerate(x):
            k = 0
            while not nodes[k]["is_leaf"]:
                nd = nodes[k]
                k = nd["left"] if row[nd["feature_idx"]] <= nd["num_threshold"] else nd["right"]
            manual[i] += nodes[k]["value"]
    err = np.max(np.abs(manual - gb.predict(x)))
    assert err < 1e-9, f"Agac disa aktarimi hatali: {err}"
    chk = check[cols].head(5).copy()
    chk["pred"] = gb.predict(chk[cols].to_numpy())
    chk.to_csv(REPO / f"ml/models/{name}_h{HORIZON}_check.csv", index=False,
               float_format="%.10g")
    n_nodes = sum(len(t) for t in trees)
    print(f"{name}: {len(trees)} agac, {n_nodes} dugum, dosya {path.stat().st_size // 1024} KB "
          f"(elle hesap = scikit-learn, en buyuk fark {err:.1e})")


def main():
    df = load_all()
    train = df[df["seed"] <= TRAIN_MAX_SEED]
    test = df[df["seed"] > TRAIN_MAX_SEED]
    print(f"Veri: {df[['scenario', 'plane', 'seed']].drop_duplicates().shape[0]} koşu, "
          f"eğitim {len(train)} / test {len(test)} örnek; "
          f"in-band'de bayat bilgi oranı: {(df[df.plane == 'inband'].age_l0 > 0).mean():.1%}")

    # --- 1) Genel karşılaştırma: düzlem ve tazelik kırılımında ---
    preds, _ = compare(train, test)
    y = test["target"].to_numpy()
    rows = []
    groups = {
        "hepsi": np.ones(len(test), bool),
        "ideal": (test["plane"] == "ideal").to_numpy(),
        "inband": (test["plane"] == "inband").to_numpy(),
        "inband-taze": ((test["plane"] == "inband") & (test["age_l0"] == 0)).to_numpy(),
        "inband-bayat": ((test["plane"] == "inband") & (test["age_l0"] > 0)).to_numpy(),
    }
    for gname, mask in groups.items():
        for m, p in preds.items():
            rows.append({"grup": gname, "yontem": m, "n": int(mask.sum()),
                         **errors(y[mask], p[mask])})
    t = pd.DataFrame(rows)
    t.to_csv(REPO / "results/model_v3_karsilastirma.csv", index=False, float_format="%.4f")
    print("\nTahmin hatası (MAE, düşük daha iyi):")
    print(t.pivot(index="yontem", columns="grup", values="MAE")
          .reindex(list(preds))[list(groups)].round(4).to_string())

    # Senaryo bazında in-band MAE
    rows = []
    for sc in sorted(test["scenario"].unique()):
        mask = ((test["scenario"] == sc) & (test["plane"] == "inband")).to_numpy()
        for m, p in preds.items():
            rows.append({"senaryo": sc, "yontem": m, **errors(y[mask], p[mask])})
    s = pd.DataFrame(rows).pivot(index="yontem", columns="senaryo", values="MAE")
    print("\nIn-band, senaryo bazında MAE:")
    print(s.reindex(list(preds)).round(4).to_string())

    # --- 2) Genelleme: senaryoyu hiç göstermeden eğit, o senaryoda test et ---
    rows = []
    for sc in sorted(df["scenario"].unique()):
        tr = train[train["scenario"] != sc]
        te = test[test["scenario"] == sc]
        cols = VARIANTS["linear3"]
        seen = LinearRegression().fit(train[cols], train["target"])
        unseen = LinearRegression().fit(tr[cols], tr["target"])
        yy = te["target"].to_numpy()
        rows.append({
            "senaryo": sc,
            "Son ölçüm": errors(yy, te["pdr_l0"].to_numpy())["MAE"],
            "EWMA": errors(yy, te["ewma_f"].to_numpy())["MAE"],
            "linear3 (senaryoyu gördü)": errors(yy, np.clip(seen.predict(te[cols]), 0, 1))["MAE"],
            "linear3 (hiç görmedi)": errors(yy, np.clip(unseen.predict(te[cols]), 0, 1))["MAE"],
        })
    gtab = pd.DataFrame(rows).set_index("senaryo")
    gtab.to_csv(REPO / "results/model_v3_genelleme.csv", float_format="%.4f")
    print("\nGenelleme (MAE, test seed'leri, iki düzlem):")
    print(gtab.round(4).to_string())

    # --- 3) Simülasyonda kullanılacak modeller: bütün seed'lerle eğit ve kaydet ---
    for name, cols in VARIANTS.items():
        lr = LinearRegression().fit(df[cols], df["target"])
        save_model(name, lr, cols, df)
    # Doğrusal olmayan model (C++'ta ai:gbm3): linear3 ile aynı girdiler
    cols = VARIANTS["linear3"]
    gb = gb_model().fit(df[cols].to_numpy(), df["target"])
    save_gbm("gbm3", gb, cols, df)
    print("\nModeller: ml/models/{linear2r,linear3a,linear3,gbm3}_h3.txt")
    # En büyük katsayılar (öznitelikler farklı ölçeklerde; sadece fikir vermesi için)
    lr = LinearRegression().fit(df[VARIANTS["linear3"]], df["target"])
    coef = pd.Series(lr.coef_, index=VARIANTS["linear3"])
    print("\nlinear3: yaş ve EWMA katsayıları")
    print(coef[AGE + EWMA].round(4).to_string())


if __name__ == "__main__":
    main()
