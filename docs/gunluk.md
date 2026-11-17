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

**Takıldığım yerler / öğrendiklerim**
- İlk denemede (sadece fading) model "son değer"i geçemedi: AR(1) kanal hafızasız, en iyi
  tahmin zaten şimdiki değere çok yakın. Geçmişin bilgi taşıdığı kademeli bozulma olayları eklendi
- Tek 1 s pencere (10 yoklama) çok gürültülü; hedefi 3 s ortalaması yapmak modeli belirgin iyileştirdi
- Model hâlâ en çok şimdiki teslim oranına bakıyor (öznitelik önemi); eğilim girdilerinin katkısı küçük
- NetAnim için Qt (qtbase5-dev) kurulu değil; yerine Python ile topoloji animasyonu yapıldı

**Gelecek hafta**
- Modeli iyileştirme: daha sık yoklama, olay öncesi erken uyarı özellikleri, sınıflandırma (kötüleşecek mi?)
- AI kontrolcü tasarımı: tahmin → link maliyeti → Dijkstra → Ipv4StaticRouting
