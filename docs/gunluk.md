# Haftalık Günlük

## Hafta 1
**Yapılanlar**
- NS-3.44 kuruldu, manet-routing-compare örneği çalıştırıldı (AODV, PDR ≈ %33)
- Proje deposu ve klasör yapısı oluşturuldu

**Takıldığım yerler**
- NS-3 klasörü ~/proje'den ~/Documents/_projects'e taşınınca cmake-cache eski yolu gösterdi;
  `./ns3 clean` + configure + build ile çözüldü
- Scratch alt klasöründeki program adı klasörden değil, main() içeren dosyadan gelir;
  bu yüzden ns3/main.cc → ns3/ai_routing.cc yapıldı (`./ns3 run ai_routing` çalışsın diye)

**Gelecek hafta**
- NS-3 tutorial'ı bitir
- 15–20 düğümlü sabit Wi-Fi topolojisini kur

## Hafta 2
**Yapılanlar**
- 16 düğümlü sabit Wi-Fi ad hoc ızgara, AODV/OLSR seçimi, UDP trafiği ve FlowMonitor
  metrikleri (PDR, gecikme, throughput, jitter, sekme) ai_routing.cc'ye eklendi
- Sonuçlar data/sonuclar.csv'ye satır olarak ekleniyor
- İlk deneme (seed 1-3): AODV PDR %99-99.7, OLSR PDR %94-99, ortalama 2.7-4.5 sekme

**Takıldığım yerler**
- 100 m aralıkla PDR %0 çıktı: NS-3 -82 dBm altındaki sinyali hiç algılamıyor
  (ThresholdPreambleDetectionModel::MinimumRssi). Menzil ~51 m; aralık 40 m yapıldı
- Kanal şu an deterministik: link ya tam çalışıyor ya tamamen kopuk. Tahmin modelinin
  öğrenebileceği kademeli bozulma için senaryolarda fading/girişim eklenmeli (hafta 4-5)

- scripts/run_baseline.sh (AODV/OLSR x 10 seed) ve analysis/summarize.py (ortalama ± %95 GA)
  yazıldı. Sonuç (results/baseline_ozet.csv): AODV PDR 99.28 ± 0.27, OLSR 96.76 ± 1.09;
  gecikme AODV 20.6 ± 2.8 ms, OLSR 44.5 ± 19.7 ms
- İncelenecek: OLSR bozulmasız ağda bile geride ve fark uzun yollarda büyüyor. Hipotez:
  HELLO/TC yayınları ACK'sız, çarpışınca kayboluyor → komşuluk düşüyor → rota değişimi.
  Doğrulamak için OLSR rota değişimleri / kontrol paketi kayıpları sayılabilir

**Gelecek hafta**
- Bozulma senaryoları ve link başına ölçüm (SNR, kayıp) CSV'si (hafta 4-5)

## Hafta 3
**Yapılanlar**
- `--scenario=fading|degrade`: kendi yavaş sönümleme kanal modelimiz (ns3/slow-fading-loss-model.*)
- `--linkCsv=...`: link başına yoklama tabanlı ölçüm (ns3/link-monitor.*) → saniye başına
  teslim oranı, SNR, RSSI
- scripts/run_scenario.sh: senaryo × AODV/OLSR × 10 seed, sonuç + link verisi tek seferde
- Sonuçlar (yoklamalar açık, 10 seed): bozulmasız AODV %97 / OLSR %94 PDR; fading %56 / %61;
  kademeli bozulma %53 / %58. Klasik protokoller kanal bozulunca ciddi kayıp yaşıyor
- Python ortamı (~/Documents/_projects/venv) kuruldu; ml/train_link_model.py ile ilk model
  - degrade verisi, sonraki 3 s'nin link kalitesi: Gradient Boosting RMSE 0.186, lineer 0.189,
    "son değer" 0.211 (%12 iyileşme); kötüleşecek link yakalama F1 0.855 → 0.866
- analysis/plot_demo.py: ön gösterim grafikleri + topoloji animasyonu (results/figures/)

- AI kontrolcü (`--routing=ai`, ns3/ai-controller.*): ölçüm → tahmin → ETX benzeri maliyet →
  Dijkstra → statik rotalar. C++ öznitelik/tahminleri Python ile birebir aynı (--aiDebugCsv)
- scripts/run_comparison.sh: 5 yöntem × modelin görmediği seed 101-110. PDR (%):

  | Senaryo | AODV | OLSR | AI-son | AI-ort | AI-model |
  | --- | --- | --- | --- | --- | --- |
  | Bozulmasız | 99.3 | 93.0 | 99.9 | 99.8 | 99.8 |
  | Fading | 65.1 | 65.0 | 95.6 | 94.0 | 93.7 |
  | Kademeli bozulma | 51.7 | 54.2 | 87.9 | 80.9 | 89.1 |

- Kontrol gecikmesi deneyi (scripts/run_delay.sh, analysis/plot_delay.py): rotalar 0-3 s geç
  uygulanınca PDR (%), son ölçüm → tahmin modeli:

  | Gecikme | Degrade | Fading |
  | --- | --- | --- |
  | 0 s | 87.9 → 89.1 | 95.6 → 93.7 |
  | 1 s | 76.5 → 81.9 | 89.3 → 90.4 |
  | 2 s | 71.8 → 74.8 | 84.2 → 86.9 |
  | 3 s | 69.1 → 70.0 | 80.5 → 85.0 |

  Gecikmeli koşulların hepsi birlikte (60 eş): model ortalama +2.9 puan, 60'ın 47'sinde daha iyi,
  eşli t-testi p = 0.012, Wilcoxon p = 0.0007. Tek tek gecikmelerde 10 seed anlamlılık için yetmiyor.
  Gecikme 3 s'de bile AI, AODV/OLSR'nin 15-20 puan üstünde

- Model v2 (sinyal payı + ters yön linki öznitelikleri, `--aiPredictor=linear2`).
  C++/Python uyumu doğrulandı (tahmin farkı < 1e-5). Gecikmeli koşullarda son ölçüme göre
  kazanç (60 eş): v1 +2.9 puan (p = 0.012) → v2 **+5.6 puan** (60'ın 50'sinde önde, p < 0.0001).
  Fading + 3 s gecikme: son ölçüm %80.5, v1 %85.0, v2 %90.3. Gecikmesiz degrade'de bile v2
  anlamlı şekilde önde (+3.2, p = 0.04)

- Kâhin (oracle) tahminci: rotaların geçerli olacağı dönemin gerçek kanal durumunu bilir
  (kanal modeli yörüngeyi saklayacak şekilde düzenlendi; diğer yöntemlerin sonuçları birebir aynı).
  Ölçülen teslim oranıyla korelasyonu 0.92; çarpışmaları bilmediği için ~0.08 iyimser
- Ana karşılaştırmaya v2 ve kâhin eklendi (seed 101-110, PDR %):

  | Senaryo | AODV | OLSR | AI-son | AI-v2 | AI-kâhin |
  | --- | --- | --- | --- | --- | --- |
  | Bozulmasız | 99.3 | 93.0 | 99.9 | 99.7 | 99.95 |
  | Fading | 65.1 | 65.0 | 95.6 | 96.3 | 97.0 |
  | Kademeli bozulma | 51.7 | 54.2 | 87.9 | 91.0 | 88.3 |

- Gecikme deneyi 30 seed'e çıkarıldı (son ölçüm ve v2). v2'nin son ölçüme kazancı:
  degrade 0/1/2/3 s: +2.8 (p = 0.01) / +2.0 (0.34) / +3.8 (0.05) / +4.0 (0.02);
  fading: +1.6 (0.08) / +2.4 (0.06) / +6.5 (< 0.001) / +8.0 (< 0.001).
  Gecikmeli tüm koşullar (180 eş): +4.5 puan, 147/180, p < 0.0001
- Tavan (kâhin) sonucu:
  - Gecikmesiz: kâhin v2'den anlamlı olarak iyi DEĞİL (fading +0.7, degrade -2.7). Daha iyi kanal
    tahmini burada kazandırmaz; kâhinin sadece kanalı bilip çarpışmaları bilmemesi degrade'de
    ölçüme dayalı v2'nin gerisinde kalmasına yol açıyor
  - Gecikmeli: kâhin v2'nin belirgin üstünde (degrade 1/2/3 s: +6.5 / +10.1 / +17.3;
    fading +4.8 / +5.5 / +6.7; 2-3 s'de p < 0.05). Daha iyi tahminin değeri gecikmeli koşulda

- Gerçekçi kontrol düzlemi (`--controlPlane=inband`, ns3/inband-control.*): kontrolcü düğüm
  5'te; raporlar ve rota güncellemeleri ağda gerçek UDP paketi; açılışta/rota yokken flooding.
  Sonuç (seed 101-130, PDR %, ideal → in-band):

  | | AODV | OLSR | AI-son | AI-v2 | AI-kâhin |
  | --- | --- | --- | --- | --- | --- |
  | Fading | 63.4 | 65.9 | 94.8 → 88.9 | 96.3 → 91.4 | 97.0 → 92.7 |
  | Kademeli bozulma | 56.1 | 53.1 | 89.5 → 74.2 | 92.3 → 80.5 | 92.2 → 90.4 |

  - In-band'de AI hâlâ AODV/OLSR'nin 24-28 puan önünde (29-30/30 seed)
  - In-band'de model v2 - son ölçüm: degrade +6.3 (p < 0.001), fading +2.5 (p = 0.01)
  - Kontrol: ~60 paket/s, rapor teslimi %76-98, güncelleme %90-99, gecikme ~230-265 ms
- docs/rapor/proje_rehberi.pdf: kavramlar + yöntem + bulgular + sunum notları (37 sayfa)

- NS-3 tabanlı 4 yeni bozulma senaryosu (ns3/disturbance.*): interference, jamming, nodefail,
  congestion. Kanal modeline dokunmuyorlar; model bunların verisini hiç görmedi
- Klasik (öğrenmeyen) rakipler: EWMA, Holt; parametreleri modelle aynı veride ayarlandı
- Deney döngüsü: scripts/pipeline.sh + scripts/deney_ayarlari.sh + analysis/evaluate.py
  (results/SONUCLAR.md otomatik). İlk tam koşu: 7 senaryo × 12 yöntem/düzlem × 30 seed = 2520
  simülasyon (~1 saat, 11 paralel)
- Sonuç: model v2 (sadece degrade ile eğitilmiş) gerçekçi kontrol düzleminde son ölçümü
  girişimde +7.1, tıkanıklıkta +9.9, jamming'de +2.3 puan geçiyor (p < 0.001); EWMA'yı 6 bozulmalı
  senaryonun 5'inde anlamlı geçiyor. Düğüm arızasında ve bozulmasız ağda fark yok. Kâhin
  (sadece kanalı bilen) kanal dışı senaryolarda modelin gerisinde
- Cihazda uygulanabilirlik (device/bench_controller.cc, docs/cihaz_uygulanabilirlik.md):
  16 düğümde karar döngüsü 0.12-0.56 ms, 65 KB; asıl maliyet iletişim (1 Mbps'te ölçüm +
  kontrol kanalın ~dörtte biri)

- Link gecikmesi ölçümü: yoklamalara gönderim zamanı yazılıyor; link CSV'sinde `delay_ms`.
  Bozulmasız ağda medyan ~1.1 ms, tıkanıklık/girişimde ortalama 10-14 ms, en kötü 400+ ms
- Gecikmeli maliyet (ETT, `--aiCost=ett`, ağırlık 1 ve 0.2) 30 seed × 7 senaryoda denendi:
  ağırlık 1 PDR'ı ve gecikmeyi kötüleştirdi (gerçekçi düzlemde −8/−9 puan), 0.2 nötr.
  Sonuç: gecikme izleniyor ama karar ETX ile kalıyor
- Lokal sunum sitesi (`site/`, `python site/server.py` → http://localhost:8000): animasyonlar,
  gerçek simülasyon kayıtları (AODV ve AI yan yana), canlı deney (2-4 yöntem seçilebilir)

**Takıldığım yerler / öğrendiklerim**
- Gecikmeye dayalı maliyet kendi trafiğinden kaçmaya çalışıyor: kuyruğu yaratan zaten akışların
  kendisi; trafiği zayıf linklere taşımak kayıp ve gecikmeyi artırıyor
- Kanal kötüleşince kontrol raporları da kayboluyor (degrade'de %76 zamanında ulaşıyor);
  tahmine en çok ihtiyaç duyulan anda bilgi en eksik. Model bu koşulda daha değerli
- v2 doğrulamasında Python'un "hiç duyulmayan ters link" için eğimi yanlış doldurduğu bulundu
  (-3 yerine 0 olmalıydı); --aiDebugCsv karşılaştırması bu tür hataları yakalıyor
- Asıl kazanç merkezi, link kalitesine bakan yönlendirmeden geliyor (AI-son bile AODV'yi
  30+ puan geçiyor). Tahmin modelinin "son ölçüm"e katkısı istatistiksel olarak anlamlı değil
  (degrade +1.2 puan, eşli t-testi p = 0.46; fading -1.9, p = 0.27). Model 5 s ortalamayı
  degrade'de anlamlı geçiyor (+8.2, p = 0.03)
- İdeal kontrol kanalı varsayımı AI'ya avantaj sağlıyor; gerçekte ölçüm/rota gecikmesi olunca
  "son ölçüm" bayatlar, tahminin değeri orada artması beklenir → test edilecek
- İlk denemede (sadece fading) model "son değer"i geçemedi: AR(1) kanal hafızasız, en iyi
  tahmin zaten şimdiki değere çok yakın. Geçmişin bilgi taşıdığı kademeli bozulma olayları eklendi
- Tek 1 s pencere (10 yoklama) çok gürültülü; hedefi 3 s ortalaması yapmak modeli belirgin iyileştirdi
- Model hâlâ en çok şimdiki teslim oranına bakıyor (öznitelik önemi); eğilim girdilerinin katkısı küçük
- NetAnim için Qt (qtbase5-dev) kurulu değil; yerine Python ile topoloji animasyonu yapıldı

**Gelecek hafta**
- Modeli bütün senaryoların verisiyle eğitip döngüyü yeniden koşturmak
- Ölçüm programını gerçek bir cihazda (Raspberry Pi) çalıştırmak
- Kontrolcü konumu, toplama süresi, tazeleme aralığı; bilginin yaşını modele vermek
- Gecikmeli koşul için tahmini güçlendir (tavan ile arada 5-17 puan var)
- Güncelleme aralığı (--aiInterval) deneyi
- Modeli iyileştirme: olay öncesi erken uyarı özellikleri, sınıflandırma (kötüleşecek mi?)
- Diğer senaryolar: girişim (interference), düğüm arızası, tıkanıklık
