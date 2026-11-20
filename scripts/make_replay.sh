#!/usr/bin/env bash
# Sunum sitesi için gerçek simülasyon kayıtları üretir: aynı senaryo ve seed'de AODV ile
# AI kontrolcü (model v2, gerçekçi kontrol düzlemi) yan yana oynatılır.
#
# Kullanım:
#   ./scripts/make_replay.sh degrade 101 [interference 107 ...]   (senaryo seed çiftleri)
# Çıktı: site/replay/<senaryo>_<seed>_<yontem>.json, ardından site/data.js güncellenir
set -e
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NS3_DIR="${NS3_DIR:-$HOME/Documents/_projects/ns-allinone-3.44/ns-3.44}"
OUT="$REPO_DIR/site/replay"
mkdir -p "$OUT"
cd "$NS3_DIR"
while [[ $# -ge 2 ]]; do
    sc="$1"; seed="$2"; shift 2
    echo "Kayit: $sc seed $seed"
    ./ns3 run --no-build "ai_routing --routing=aodv --linkCsv=/dev/null --scenario=$sc --seed=$seed \
        --csv=/dev/null --replayLog=$OUT/${sc}_${seed}_aodv.json" > /dev/null
    ./ns3 run --no-build "ai_routing --routing=ai --aiPredictor=linear2 --controlPlane=inband \
        --scenario=$sc --seed=$seed --csv=/dev/null --replayLog=$OUT/${sc}_${seed}_ai.json" > /dev/null
done
"$HOME/Documents/_projects/venv/bin/python" "$REPO_DIR/site/build_data.py"
