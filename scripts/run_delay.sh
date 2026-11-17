#!/usr/bin/env bash
# Kontrol gecikmesi deneyi: AI kontrolcünün rotaları D saniye geç uygulandığında
# "son ölçüm" ile "tahmin modeli" arasındaki fark nasıl değişiyor?
#
# Her gecikme için o gecikmeye göre eğitilmiş model kullanılır (ml/models/linear_o<D>_h3.txt;
# önce: python ml/train_link_model.py --offset D). Seed'ler 101'den başlar (modelin görmediği).
#
# Kullanım:
#   ./scripts/run_delay.sh                # degrade + fading, gecikme 0-3 s, 10 seed
#   ./scripts/run_delay.sh 20             # 20 seed
#   PREDICTORS="linear2 linear2:h1" OUT=delay_v2 ./scripts/run_delay.sh
#       tahminci:hN -> --aiHorizon=N ile o ufka göre eğitilmiş model
#
# Çıktılar:
#   data/<OUT>.csv            her çalıştırmanın sonuç satırı (yöntem adı: ai-linear-d2 gibi)
#   results/<OUT>_ozet.csv    senaryo × yöntem başına ortalama ± %95 GA
set -e

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NS3_DIR="${NS3_DIR:-$HOME/Documents/_projects/ns-allinone-3.44/ns-3.44}"

N_SEEDS="${1:-10}"
FIRST_SEED="${FIRST_SEED:-101}"
SCENARIOS="${SCENARIOS:-degrade fading}"
DELAYS="0 1 2 3"
PREDICTORS="${PREDICTORS:-last linear}"
OUT="${OUT:-delay}"

CSV="$REPO_DIR/data/$OUT.csv"
mkdir -p "$REPO_DIR/data"
rm -f "$CSV"

cd "$NS3_DIR"
# Paralel çalıştırmada derleme çakışmasın diye SKIP_BUILD=1 ile atlanabilir
[[ -n "$SKIP_BUILD" ]] || ./ns3 build ai_routing > /dev/null

for sc in $SCENARIOS; do
    for d in $DELAYS; do
        for p in $PREDICTORS; do
            for seed in $(seq "$FIRST_SEED" $((FIRST_SEED + N_SEEDS - 1))); do
                echo "Calisiyor: $sc gecikme=$d $p seed=$seed"
                pred="${p%%:*}"
                hz=3
                [[ "$p" == *:h* ]] && hz="${p##*:h}"
                ./ns3 run --no-build "ai_routing --routing=ai --aiPredictor=$pred --aiHorizon=$hz \
                    --aiDelay=$d \
                    --seed=$seed --scenario=$sc --csv=$CSV" > /dev/null
            done
        done
    done
done

echo "Ham sonuclar: $CSV"
python3 "$REPO_DIR/analysis/summarize.py" "$CSV" "$REPO_DIR/results/${OUT}_ozet.csv"
