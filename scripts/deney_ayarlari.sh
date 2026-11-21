# Deney döngüsünün (scripts/pipeline.sh) ayarları.
#
# Döngüyü genişletmek için burayı düzenle, sonra ./scripts/pipeline.sh çalıştır:
# daha önce yapılmış simülasyonlar atlanır, sadece yeni/eksik olanlar koşar.
#
# Değerlendirme ayarları komut satırından da verilebilir (sadece o koşu için geçerli), ör.
# seçilen yöntemleri gerçekçi düzlemde 100 seed'e çıkarmak:
#   METHODS="ai:ewma ai:linear2 ai:linear3" PLANES=inband EVAL_SEEDS=100 ./scripts/pipeline.sh deney

# Modelin EĞİTİM verisinin üretileceği senaryolar (seed 1-10, AODV + OLSR, yoklamalar açık).
# Model sadece bunlardan öğrenir; diğer senaryolar onun için "hiç görülmemiş" olur.
TRAIN_SCENARIOS="degrade"
TRAIN_SEEDS=10

# DEĞERLENDİRME: senaryo × yöntem × kontrol düzlemi × seed matrisi
EVAL_SCENARIOS="${EVAL_SCENARIOS:-none fading degrade interference jamming nodefail congestion}"
EVAL_FIRST_SEED=101   # eğitim seed'lerinden ayrı (model bu kanal gerçekleşmelerini görmedi)
EVAL_SEEDS="${EVAL_SEEDS:-30}"

# Yöntemler. ai:<tahminci> AI kontrolcüdür; tahminciler:
#   last   = son ölçüm (tahmin yok)         ewma, holt = klasik (öğrenmeyen) tahmin
#   linear2 = öğrenilmiş model v2           oracle = gerçek geleceği bilen kâhin (üst sınır)
#   ai:<tahminci>:ett[:ağırlık] = gecikmeli maliyet (ETT), gecikme terimi ağırlıklı (varsayılan 1)
#   v3 ablasyonu (gerçekçi veriyle eğitildi, ml/train_v3.py):
#     linear2r = v2 girdileri   linear3a = + bilginin yaşı   linear3 = + yaş + EWMA girdileri
#     gbm3 = linear3'ün girdileriyle Gradient Boosting (300 küçük karar ağacı, doğrusal olmayan)
METHODS="${METHODS:-aodv olsr ai:last ai:ewma ai:holt ai:linear2 ai:oracle ai:linear2:ett \
ai:linear2:ett:0.2 ai:linear2r ai:linear3a ai:linear3 ai:gbm3}"

# AI yöntemleri için kontrol düzlemleri: ideal (bilgi anında) ve inband (ağda gerçek paket)
PLANES="${PLANES:-ideal inband}"

# Aynı anda kaç simülasyon (varsayılan: çekirdek sayısı - 1)
JOBS="${JOBS:-$(($(nproc) - 1))}"
