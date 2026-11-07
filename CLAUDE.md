# Proje: Kanal Durumuna Göre Yapay Zekâ Tabanlı Dinamik Trafik Yönlendirme

Bu bir dönem projesi (14 hafta, tek kişi). Öğrenci ağ simülasyonunda ve C++'ta yeni;
yaptığın her değişikliği kısa ve anlaşılır şekilde Türkçe açıkla. Kullanıcıyla Türkçe konuş.

## Amaç
Kablosuz mesh ağında linklerin kanal ölçümlerinden (SNR, gecikme, paket kaybı) yakın
gelecekteki link kalitesini tahmin eden ve trafiği buna göre yönlendiren bir algoritma
geliştirmek; NS-3'te AODV ve OLSR ile farklı bozulma senaryolarında karşılaştırmak;
modelin gerçek cihazda çalışabilirliğini değerlendirmek.

## Ortam
- Ubuntu 22.04, GCC 11.4
- NS-3.44: ~/proje/ns-allinone-3.44/ns-3.44 (aşağıda NS3_DIR)
- Bu depo: ~/proje/channel-aware-ai-routing
- `ns3/` klasörü NS3_DIR/scratch/ai_routing olarak symlink ile bağlı
- Yeni dosya eklenince: NS3_DIR içinde `./ns3 configure --enable-examples --enable-tests`
- Derleme: NS3_DIR içinde `./ns3 build`
- Çalıştırma: NS3_DIR içinde `./ns3 run "ai_routing --routing=aodv --seed=1"`
- Python sanal ortamı: ~/proje/venv (`source ~/proje/venv/bin/activate`)

## Mimari kararlar
- Topoloji: sabit düğümlü Wi-Fi ad hoc mesh, 15–20 düğüm (hareketlilik yok)
- Trafik: UDP (TCP'nin yeniden iletimi sonuçları karıştırmasın)
- Karşılaştırılan yöntemler: AODV, OLSR, AI tabanlı merkezi kontrolcü (`--routing=aodv|olsr|ai`)
- Tek simülasyon programı; topoloji, trafik ve senaryo tüm yöntemler için ortak,
  her şey komut satırı parametresiyle seçilir (adil karşılaştırma için)
- AI kontrolcü: periyodik link ölçümü → tahmin modeli → link maliyeti → Dijkstra →
  Ipv4StaticRouting ile rotaları yaz
- Model çevrimdışı (Python) eğitilir, hafif modeller tercih edilir
- Metrikler FlowMonitor ile; link başına ölçümler CSV olarak data/ altına

## Kurallar
- NS-3 API'sini tahmin etme. Emin olmadığında NS3_DIR/src ve NS3_DIR/examples içinde
  grep ile gerçek kullanımı bul. Sürüm 3.44'e uygun kod yaz.
- NS-3 kaynak koduna dokunma; yalnızca bu depoda çalış.
- Büyük işlerde önce planı anlat, onay al, sonra kodla.
- Her değişiklikten sonra derle ve çalıştırarak doğrula.
- Koddaki yorumlar Türkçe ve açıklayıcı olsun.
- Rastgelelik için her zaman `--seed` parametresi kullan (RngSeedManager::SetRun).
- Önemli bir tasarım kararı alındığında docs/kararlar.md dosyasına ekle.
