#!/usr/bin/env bash
# Yöntem karşılaştırması: AODV, OLSR ve AI kontrolcünün üç tahmincisi, aynı senaryo
# ve aynı seed'lerle. Yoklamalar tüm yöntemlerde açık (adil yük).
#
# ÖNEMLİ: Seed'ler 101'den başlar. Tahmin modeli seed 1-10 verisiyle (run_scenario.sh)
# eğitildiği için karşılaştırma modelin hiç görmediği kanal gerçekleşmelerinde yapılır.
#
# Kullanım:
#   ./scripts/run_comparison.sh degrade          # 10 seed (101-110)
#   ./scripts/run_comparison.sh fading 20        # 20 seed (101-120)
#
# Çıktılar:
#   data/cmp_<senaryo>.csv          her çalıştırmanın sonuç satırı
#   results/cmp_<senaryo>_ozet.csv  yöntem başına ortalama ± %95 GA
set -e

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NS3_DIR="${NS3_DIR:-$HOME/Documents/_projects/ns-allinone-3.44/ns-3.44}"

SCENARIO="${1:?senaryo adi gerekli (ornek: degrade)}"
N_SEEDS="${2:-10}"
FIRST_SEED=101
METHODS="${METHODS:-aodv olsr ai:last ai:ma ai:linear ai:linear2 ai:oracle}"   # ai:<tahminci>

CSV="$REPO_DIR/data/cmp_$SCENARIO.csv"
mkdir -p "$REPO_DIR/data"
rm -f "$CSV"

cd "$NS3_DIR"
# Paralel çalıştırmada derleme çakışmasın diye SKIP_BUILD=1 ile atlanabilir
[[ -n "$SKIP_BUILD" ]] || ./ns3 build ai_routing > /dev/null

for m in $METHODS; do
    if [[ "$m" == ai:* ]]; then
        ARGS="--routing=ai --aiPredictor=${m#ai:}"
    else
        ARGS="--routing=$m --linkCsv=/dev/null"   # yoklamalar açık, link verisi kaydedilmez
    fi
    for seed in $(seq "$FIRST_SEED" $((FIRST_SEED + N_SEEDS - 1))); do
        echo "Calisiyor: scenario=$SCENARIO $m seed=$seed"
        ./ns3 run --no-build \
            "ai_routing $ARGS --seed=$seed --scenario=$SCENARIO --csv=$CSV" > /dev/null
    done
done

echo "Ham sonuclar: $CSV"
python3 "$REPO_DIR/analysis/summarize.py" "$CSV" "$REPO_DIR/results/cmp_${SCENARIO}_ozet.csv"
