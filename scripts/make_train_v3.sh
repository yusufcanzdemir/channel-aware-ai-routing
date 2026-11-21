#!/usr/bin/env bash
# Model v3 için GERÇEKÇİ eğitim verisi üretir.
#
# Eski modeller (v1, v2) AODV/OLSR koşularındaki gerçek link ölçümleriyle eğitildi: her
# saniye her linkin taze ölçümü. Oysa gerçekçi (in-band) kontrol düzleminde kontrolcü
# raporları geç ya da hiç almaz, eski raporu yeniden kullanır. Model sınava çalıştığından
# farklı sorularla giriyordu. Burada AI kontrolcü (son ölçüm, yani tahminsiz) koşturulur ve
# her rota hesabında kontrolcünün GÖRDÜĞÜ öznitelikler (--aiTrainCsv) ile linklerin GERÇEK
# ölçümleri (--linkCsv) aynı koşudan yazılır. Python (ml/train_v3.py) ikisini eşler:
# girdi = kontrolcünün gördüğü, hedef = linkin gerçek gelecekteki teslim oranı.
#
# Kullanım:
#   ./scripts/make_train_v3.sh            # 7 senaryo × 2 düzlem × 20 seed (var olanlar atlanır)
#   SEEDS=5 ./scripts/make_train_v3.sh    # daha az seed
# Çıktı: data/train_v3/<senaryo>__<düzlem>__<seed>.{feat,link}.csv
set -e
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NS3_DIR="${NS3_DIR:-$HOME/Documents/_projects/ns-allinone-3.44/ns-3.44}"
# TAG + EXTRA: başka bir ölçüm ayarı için veri (ör. TAG=_p5 EXTRA="--probeInterval=0.2")
TAG="${TAG:-}"
EXTRA="${EXTRA:-}"
OUT="$REPO_DIR/data/train_v3$TAG"
SCENARIOS="${SCENARIOS:-none fading degrade interference jamming nodefail congestion}"
PLANES="${PLANES:-ideal inband}"
SEEDS="${SEEDS:-20}"   # seed 1..SEEDS; değerlendirme seed'leri (101+) ile çakışmaz
JOBS="${JOBS:-$(($(nproc) - 1))}"
mkdir -p "$OUT"

cd "$NS3_DIR"

# Tek bir koşu (betik paralel işler için kendini bu modda çağırır)
if [[ "$1" == "--one" ]]; then
    sc="$2"; pl="$3"; seed="$4"
    base="$OUT/${sc}__${pl}__${seed}"
    rm -f "$base.feat.csv" "$base.link.csv"
    if ./ns3 run --no-build "ai_routing --routing=ai --aiPredictor=last --controlPlane=$pl \
        --scenario=$sc --seed=$seed --csv=/dev/null $EXTRA \
        --aiTrainCsv=$base.feat.csv --linkCsv=$base.link.csv" > /dev/null 2>&1; then
        touch "$base.done" # yarım kalan koşular bir sonraki çalıştırmada yenilenir
    else
        echo "   HATA: $sc $pl $seed"
    fi
    exit 0
fi

./ns3 build ai_routing > /dev/null
JOBLIST="$(mktemp)"
for sc in $SCENARIOS; do
    for pl in $PLANES; do
        for seed in $(seq 1 "$SEEDS"); do
            [[ -f "$OUT/${sc}__${pl}__${seed}.done" ]] || echo "$sc $pl $seed" >> "$JOBLIST"
        done
    done
done
n=$(wc -l < "$JOBLIST")
echo "$n simulasyon kosturulacak ($JOBS paralel)"
< "$JOBLIST" xargs -P "$JOBS" -L 1 "$REPO_DIR/scripts/make_train_v3.sh" --one
rm -f "$JOBLIST"
echo "Bitti: $OUT"
