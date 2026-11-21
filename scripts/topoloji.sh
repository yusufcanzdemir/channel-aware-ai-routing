#!/usr/bin/env bash
# Düzensiz yerleşim ve ölçeklenme deneyi (C + D): sonuçlar 4x4 ızgaraya özel mi?
#
# Düğümler kare bir alana rastgele yerleştirilir (--topology=random; her seed farklı topoloji,
# bütün yöntemler aynı seed'de aynı topolojiyi görür). 20 ve 30 düğüm, aynı yoğunluk.
# Model YENİDEN EĞİTİLMEZ: ızgarada eğitilmiş v3 hiç görmediği topolojilerde denenir.
# Analiz: analysis/topoloji.py
#
# Kullanım: ./scripts/topoloji.sh            (var olan simülasyonlar atlanır)
set -e
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"

METHODS=""
for n in 20 30; do
    opt="+topology=random+nodes=$n"
    METHODS="$METHODS aodv+noprobe$opt olsr+noprobe$opt"
    METHODS="$METHODS ai:last$opt ai:ewma$opt ai:linear2$opt ai:linear3$opt"
done
METHODS="$METHODS" PLANES="ideal inband" "$REPO_DIR/scripts/pipeline.sh" deney
