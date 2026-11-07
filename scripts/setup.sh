#!/usr/bin/env bash
# Depoyu NS-3'e bağlar ve Git'i başlatır. Bir kere çalıştırman yeterli.
set -e

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
# NS-3 yolu; farklıysa NS3_DIR ortam değişkeniyle değiştirilebilir
NS3_DIR="${NS3_DIR:-$HOME/Documents/_projects/ns-allinone-3.44/ns-3.44}"
LINK="$NS3_DIR/scratch/ai_routing"

if [ ! -d "$NS3_DIR" ]; then
    echo "HATA: NS-3 bulunamadi: $NS3_DIR"
    exit 1
fi

if [ -L "$LINK" ] || [ -e "$LINK" ]; then
    echo "Baglanti zaten var: $LINK"
else
    ln -s "$REPO_DIR/ns3" "$LINK"
    echo "Baglanti olusturuldu: $LINK -> $REPO_DIR/ns3"
fi

mkdir -p "$REPO_DIR/data"

cd "$REPO_DIR"
if [ ! -d .git ]; then
    git init -b main
    git add .
    git commit -m "Initial project structure"
    echo "Git deposu baslatildi."
fi

echo "Tamam. Simdi NS-3 klasorunde configure, build ve test calistir:"
echo "  cd $NS3_DIR"
echo "  ./ns3 configure --enable-examples --enable-tests && ./ns3 build"
echo "  ./ns3 run \"ai_routing --routing=olsr --seed=5\""
