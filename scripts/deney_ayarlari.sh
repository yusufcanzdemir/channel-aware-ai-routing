# Deney döngüsünün (scripts/pipeline.sh) ayarları.
#
# Döngüyü genişletmek için burayı düzenle, sonra ./scripts/pipeline.sh çalıştır:
# daha önce yapılmış simülasyonlar atlanır, sadece yeni/eksik olanlar koşar.

# Modelin EĞİTİM verisinin üretileceği senaryolar (seed 1-10, AODV + OLSR, yoklamalar açık).
# Model sadece bunlardan öğrenir; diğer senaryolar onun için "hiç görülmemiş" olur.
TRAIN_SCENARIOS="degrade"
TRAIN_SEEDS=10

# DEĞERLENDİRME: senaryo × yöntem × kontrol düzlemi × seed matrisi
EVAL_SCENARIOS="none fading degrade interference jamming nodefail congestion"
EVAL_FIRST_SEED=101   # eğitim seed'lerinden ayrı (model bu kanal gerçekleşmelerini görmedi)
EVAL_SEEDS=30

# Yöntemler. ai:<tahminci> AI kontrolcüdür; tahminciler:
#   last   = son ölçüm (tahmin yok)         ewma, holt = klasik (öğrenmeyen) tahmin
#   linear2 = öğrenilmiş model v2           oracle = gerçek geleceği bilen kâhin (üst sınır)
METHODS="aodv olsr ai:last ai:ewma ai:holt ai:linear2 ai:oracle"

# AI yöntemleri için kontrol düzlemleri: ideal (bilgi anında) ve inband (ağda gerçek paket)
PLANES="ideal inband"

# Aynı anda kaç simülasyon (varsayılan: çekirdek sayısı - 1)
JOBS="${JOBS:-$(($(nproc) - 1))}"
