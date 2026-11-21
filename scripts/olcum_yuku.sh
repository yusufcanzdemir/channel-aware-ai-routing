#!/usr/bin/env bash
# Ölçüm yükü deneyi (B): yükü azaltınca başarı ne kadar düşüyor?
#
# Ayarlar (gerçekçi kontrol düzleminde):
#   yoklama hızı      : saniyede 10 (ana deney), 5, 2
#   rapor             : her saniye (ana deney) ya da sadece linkler değişince (--reportOnChange)
# Her ayar için:
#   1) o ayarda eğitim verisi üretilir (scripts/make_train_v3.sh, TAG/EXTRA ile)
#   2) v3 o verinin üzerinde yeniden eğitilir (ml/train_v3.py --tag) -> linear3<etiket>
#   3) son ölçüm, EWMA ve linear3<etiket> 7 senaryo × 30 seed koşturulur (scripts/pipeline.sh)
# Ana ayarın (10/s, her saniye rapor) sonuçları ana deney matrisinden gelir.
# Analiz: analysis/olcum_yuku.py
#
# Kullanım: ./scripts/olcum_yuku.sh            (var olan simülasyonlar atlanır)
set -e
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PY="${VENV:-$HOME/Documents/_projects/venv}/bin/python"

# etiket | simülasyon parametreleri (pipeline biçiminde: +anahtar=değer)
CONFIGS=(
    "_p5|+probeInterval=0.2"
    "_p2|+probeInterval=0.5"
    "_roc|+reportOnChange=1"
    "_p5roc|+probeInterval=0.2+reportOnChange=1"
    "_p2roc|+probeInterval=0.5+reportOnChange=1"
)

METHODS=""
for c in "${CONFIGS[@]}"; do
    tag="${c%%|*}"; opts="${c#*|}"
    # "+a=b+c=d" -> "--a=b --c=d"
    extra="$(echo "$opts" | sed 's/+/ --/g')"
    echo "== $tag: egitim verisi ($extra)"
    TAG="$tag" EXTRA="$extra" PLANES=inband "$REPO_DIR/scripts/make_train_v3.sh" | tail -1
    echo "== $tag: model"
    "$PY" "$REPO_DIR/ml/train_v3.py" --tag "$tag"
    METHODS="$METHODS ai:last$opts ai:ewma$opts ai:linear3$tag$opts"
done

echo "== degerlendirme"
METHODS="$METHODS" PLANES=inband "$REPO_DIR/scripts/pipeline.sh" deney
