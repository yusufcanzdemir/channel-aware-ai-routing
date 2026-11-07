#!/usr/bin/env bash
# AODV ve OLSR'yi birden fazla seed ile koşturur, sonuçları tek CSV'de toplar
# ve özet tabloyu (ortalama ± %95 güven aralığı) üretir.
#
# Kullanım:
#   ./scripts/run_baseline.sh                 # 10 seed, varsayılan ayarlar
#   ./scripts/run_baseline.sh 20              # 20 seed
#   ./scripts/run_baseline.sh 10 --flows=8    # ek parametreler simülasyona aynen geçer
set -e

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NS3_DIR="${NS3_DIR:-$HOME/Documents/_projects/ns-allinone-3.44/ns-3.44}"

N_SEEDS="${1:-10}"
shift || true          # ilk argüman (seed sayısı) dışındakiler simülasyona gider
EXTRA_ARGS="$*"

CSV="$REPO_DIR/data/baseline.csv"
mkdir -p "$REPO_DIR/data"
rm -f "$CSV"           # eski sonuçlar yenileriyle karışmasın

cd "$NS3_DIR"
./ns3 build ai_routing > /dev/null   # bir kere derle, sonra --no-build ile koştur

for routing in aodv olsr; do
    for seed in $(seq 1 "$N_SEEDS"); do
        echo "Calisiyor: routing=$routing seed=$seed $EXTRA_ARGS"
        ./ns3 run --no-build \
            "ai_routing --routing=$routing --seed=$seed --csv=$CSV $EXTRA_ARGS" > /dev/null
    done
done

echo "Ham sonuclar: $CSV"
python3 "$REPO_DIR/analysis/summarize.py" "$CSV" "$REPO_DIR/results/baseline_ozet.csv"
