#!/usr/bin/env bash
# Deney döngüsü: derle -> eğitim verisi -> model -> değerlendirme matrisi -> analiz -> rapor
#
# Ayarlar: scripts/deney_ayarlari.sh
#
# Kullanım:
#   ./scripts/pipeline.sh                 # bütün aşamalar
#   ./scripts/pipeline.sh deney analiz    # sadece seçilen aşamalar
#   FORCE=1 ./scripts/pipeline.sh deney   # var olan sonuçları silip yeniden koştur
#
# Aşamalar:
#   derle   : NS-3 programını derler
#   veri    : eğitim senaryolarında link ölçüm verisi üretir (yoksa)
#   egitim  : tahmin modellerini (v1, v2) ve klasik tahmincilerin parametrelerini eğitir
#   deney   : değerlendirme matrisini paralel koşturur; HER simülasyon kendi dosyasına yazar
#             (data/eval/raw/), var olanlar atlanır -> yeni senaryo/yöntem eklemek ucuzdur
#   analiz  : özet tablolar, istatistik testleri, grafikler, results/SONUCLAR.md
#   rapor   : (varsa) yerel PDF rehberini yeniden üretir
#
# Not: Ayarlar değiştiğinde (ör. model yeniden eğitildiğinde) eski sonuçlar otomatik
# geçersiz olmaz; o yöntemin sonuçlarını yenilemek için FORCE=1 ya da ilgili dosyaları sil.
set -e

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NS3_DIR="${NS3_DIR:-$HOME/Documents/_projects/ns-allinone-3.44/ns-3.44}"
VENV="${VENV:-$HOME/Documents/_projects/venv}"
source "$REPO_DIR/scripts/deney_ayarlari.sh"
PY="$VENV/bin/python"
RAW="$REPO_DIR/data/eval/raw"

STAGES="${*:-derle veri egitim deney analiz rapor}"
has() { [[ " $STAGES " == *" $1 "* ]]; }

if has derle; then
    echo "== derle"
    (cd "$NS3_DIR" && ./ns3 build ai_routing > /dev/null)
fi

if has veri; then
    echo "== veri: $TRAIN_SCENARIOS"
    for sc in $TRAIN_SCENARIOS; do
        if [[ -f "$REPO_DIR/data/link_$sc.csv" && -z "$FORCE" ]]; then
            echo "   $sc: var, atlandi"
        else
            SKIP_BUILD=1 "$REPO_DIR/scripts/run_scenario.sh" "$sc" "$TRAIN_SEEDS" > /dev/null
            echo "   $sc: uretildi"
        fi
    done
fi

if has egitim; then
    echo "== egitim"
    FILES=""
    for sc in $TRAIN_SCENARIOS; do FILES="$FILES $REPO_DIR/data/link_$sc.csv"; done
    "$PY" "$REPO_DIR/ml/train_link_model.py" $FILES > /dev/null
    "$PY" "$REPO_DIR/ml/train_link_model.py" $FILES --features 2 | sed -n '4,9p'
    "$PY" "$REPO_DIR/ml/tune_classic.py" $FILES
fi

if has deney; then
    echo "== deney"
    mkdir -p "$RAW"
    [[ -n "$FORCE" ]] && rm -f "$RAW"/*.csv
    JOBLIST="$(mktemp)"
    last_seed=$((EVAL_FIRST_SEED + EVAL_SEEDS - 1))
    for sc in $EVAL_SCENARIOS; do
        for m in $METHODS; do
            if [[ "$m" == ai:* ]]; then
                planes="$PLANES"
            else
                planes="-"
            fi
            for pl in $planes; do
                for seed in $(seq "$EVAL_FIRST_SEED" "$last_seed"); do
                    out="$RAW/${sc}__${m/:/-}__${pl}__${seed}.csv"
                    [[ -f "$out" ]] && continue
                    if [[ "$m" == ai:* ]]; then
                        args="--routing=ai --aiPredictor=${m#ai:} --controlPlane=$pl"
                    else
                        args="--routing=$m --linkCsv=/dev/null"  # yoklama yükü herkese eşit
                    fi
                    echo "ai_routing $args --scenario=$sc --seed=$seed --csv=$out" >> "$JOBLIST"
                done
            done
        done
    done
    n=$(wc -l < "$JOBLIST")
    echo "   $n simulasyon kostrulacak ($JOBS paralel)"
    if [[ "$n" -gt 0 ]]; then
        cd "$NS3_DIR"
        # Her satır bir simülasyon; başarısız olan dosya bırakmaz, bir sonraki koşuda yeniden denenir
        < "$JOBLIST" xargs -P "$JOBS" -I{} sh -c './ns3 run --no-build "{}" > /dev/null 2>&1 || echo "   HATA: {}"'
        cd "$REPO_DIR"
    fi
    rm -f "$JOBLIST"
fi

if has analiz; then
    echo "== analiz"
    "$PY" "$REPO_DIR/analysis/evaluate.py"
fi

if has rapor; then
    if [[ -f "$REPO_DIR/docs/rapor/build_report.py" ]]; then
        echo "== rapor"
        "$PY" "$REPO_DIR/docs/rapor/make_figures.py" > /dev/null
        "$PY" "$REPO_DIR/docs/rapor/build_report.py" | tail -1
    fi
fi
