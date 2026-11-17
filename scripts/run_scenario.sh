#!/usr/bin/env bash
# Bir bozulma senaryosunda AODV ve OLSR'yi birden fazla seed ile koşturur.
# Link ölçümü (yoklama paketleri) açıktır: hem yöntem sonuçları hem de modelin
# eğitim verisi (link başına SNR / teslim oranı) aynı çalıştırmalardan çıkar.
#
# Kullanım:
#   ./scripts/run_scenario.sh fading            # 10 seed
#   ./scripts/run_scenario.sh fading 20         # 20 seed
#   ./scripts/run_scenario.sh fading 10 --fadeSigma=3   # ek parametreler simülasyona geçer
#
# Çıktılar:
#   data/<senaryo>.csv           her çalıştırmanın sonuç satırı (PDR, gecikme, ...)
#   data/link_<senaryo>.csv      link başına, saniye başına ölçümler (modelin verisi)
#   results/<senaryo>_ozet.csv   yöntem başına ortalama ± %95 GA
set -e

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NS3_DIR="${NS3_DIR:-$HOME/Documents/_projects/ns-allinone-3.44/ns-3.44}"

SCENARIO="${1:?senaryo adi gerekli (ornek: fading)}"
N_SEEDS="${2:-10}"
shift 2 || shift || true
EXTRA_ARGS="$*"

CSV="$REPO_DIR/data/$SCENARIO.csv"
LINK_CSV="$REPO_DIR/data/link_$SCENARIO.csv"
mkdir -p "$REPO_DIR/data"
rm -f "$CSV" "$LINK_CSV"   # eski sonuçlar yenileriyle karışmasın

cd "$NS3_DIR"
./ns3 build ai_routing > /dev/null

for routing in aodv olsr; do
    for seed in $(seq 1 "$N_SEEDS"); do
        echo "Calisiyor: scenario=$SCENARIO routing=$routing seed=$seed $EXTRA_ARGS"
        ./ns3 run --no-build \
            "ai_routing --routing=$routing --seed=$seed --scenario=$SCENARIO \
             --csv=$CSV --linkCsv=$LINK_CSV $EXTRA_ARGS" > /dev/null
    done
done

echo "Ham sonuclar: $CSV"
echo "Link olcumleri: $LINK_CSV"
python3 "$REPO_DIR/analysis/summarize.py" "$CSV" "$REPO_DIR/results/${SCENARIO}_ozet.csv"
