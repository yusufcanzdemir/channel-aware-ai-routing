# Cihazda Uygulanabilirlik Değerlendirmesi

Proje tanımındaki üçüncü çıktı: algoritmanın gerçek bir cihazda çalışıp çalışamayacağının
değerlendirilmesi. Üç kaynak incelenir: **hesaplama**, **bellek** ve **iletişim**.

## 1. Cihazda ne çalışacak?

| Bileşen | Nerede | İş |
| --- | --- | --- |
| Yoklama gönderme/sayma | Her düğüm | Saniyede 10 küçük yayın, gelenleri saymak, SNR/RSSI ortalaması |
| Ölçüm raporu | Her düğüm | Saniyede 1 UDP paketi (komşu başına 7 bayt) |
| Tahmin + Dijkstra | Kontrolcü düğüm | Saniyede 1 karar döngüsü |
| Rota yazma | Her düğüm | Gelen güncellemeyle yönlendirme tablosunu değiştirmek |

Model eğitimi (Python) çevrimdışıdır; cihaza yalnızca **37 katsayılık bir metin dosyası**
(`ml/models/linear2_h3.txt`, 304 bayt) gider. Cihazda Python veya makine öğrenmesi kütüphanesi
gerekmez.

## 2. Hesaplama ve bellek (ölçüm)

`device/bench_controller.cc`, kontrolcünün işini NS-3'ten bağımsız olarak birebir yapar
(37 öznitelik, lineer tahmin, maliyet, her kaynaktan Dijkstra). Herhangi bir cihazda:

```
g++ -O2 -std=c++17 -o bench_controller device/bench_controller.cc
./bench_controller ml/models/linear2_h3.txt
```

Geliştirme bilgisayarında (AMD Ryzen 5 220, tek çekirdek) ölçülen değerler:

| Düğüm sayısı | Karar döngüsü: tüm çiftler | Karar döngüsü: sadece komşu linkler | Kontrolcü durumu |
| --- | --- | --- | --- |
| 16 | 0.56 ms | 0.12 ms | 65 KB |
| 25 | 1.42 ms | 0.22 ms | 159 KB |
| 50 | 6.19 ms | 0.72 ms | 635 KB |
| 100 | 27.2 ms | 3.08 ms | 2.5 MB |

Tek bir link tahmini 2.3 µs sürmektedir (öznitelikler okunabilirlik için metin anahtarlı bir
sözlükte tutulduğundan; sabit dizilerle bu süre bir mertebe kısalır). Sürecin toplam bellek
kullanımı 5.9 MB'tır.

**Yorum:** Karar döngüsü saniyede bir kez çalışır. 16 düğümde döngü 1 ms'nin altında, yani
işlemci zamanının binde birinden azdır. Raspberry Pi sınıfı bir cihaz masaüstü işlemciden
birkaç kat ile bir mertebe arası daha yavaş olsa bile döngü birkaç milisaniyede kalır.
(Bu bir tahmindir; kesin değer için program hedef cihazda çalıştırılmalıdır.) Gerçek bir
uygulama yalnızca ölçümü olan komşu linkleri tahmin eder; bu durumda 100 düğümde bile
döngü ~3 ms'dir. Dijkstra'nın maliyeti düğüm sayısının karesiyle-küpüyle büyür; yüzlerce
düğümlük ağlarda öncelik kuyruğu ile Dijkstra veya yalnızca değişen kısmın yeniden
hesaplanması gerekir.

## 3. İletişim yükü (asıl maliyet)

Hesaplama ihmal edilebilir düzeydeyken, **ölçüm ve kontrol için kanalda harcanan süre**
gerçek maliyettir. 802.11b, 1 Mbps için çerçeve süreleri (uzun önsöz + PLCP başlığı 192 µs,
her bayt 8 µs; MAC başlığı + FCS 28 bayt, LLC/SNAP 8 bayt):

| Çerçeve | Boyut (bayt) | Havada kalma süresi |
| --- | --- | --- |
| Yoklama (32 bayt yük, yayın, ACK yok) | 68 | 192 + 544 = **~0.74 ms** |
| Ölçüm raporu (4 komşu: 35 bayt + UDP/IP 28) | 99 | 192 + 792 + ACK ~0.3 = **~1.3 ms / sekme** |
| Veri paketi (512 bayt + UDP/IP 28) | 576 | 192 + 4608 + ACK ~0.3 = **~5.1 ms / sekme** |

Ağ genelinde saniye başına kanal kullanımı (simülasyonda ölçülen paket sayılarıyla):

| Trafik | Saniyedeki iletim | Kanal süresi (ms/s) |
| --- | --- | --- |
| Yoklamalar (16 düğüm × 10) | 160 | ~118 |
| Kontrol düzlemi (rapor + güncelleme, ölçülen) | ~60-65 | ~80 |
| Veri (4 akış × 7.8 paket × ~3.5 sekme) | ~110 | ~560 |

Yani 1 Mbps'lik bu ağda ölçüm ve kontrol, kanal kullanımının yaklaşık **dörtte birini**
oluşturmaktadır. Bu yük, yoklamaların bozulmasız ağda AODV'nin teslim oranını %99.3'ten %97'ye
düşürmesinde de görülmüştür. (Kablosuz ağda aynı anda birden fazla bölgede iletim
yapılabildiği için yerel yük bu toplamdan düşüktür; tablo kabaca bir üst sınırdır.)

Güncel Wi-Fi hızlarında (ör. 802.11g/n, 6-65 Mbps) veri süresi 6-60 kat, önsöz ~10 kat kısalır;
küçük kontrol paketlerinin göreli maliyeti aynı oranda düşer.

## 4. Yükü azaltma seçenekleri

- Yoklama hızını düşürmek (10/s yerine 2-5/s) veya yoklamaları veri paketlerine eklemek.
- Raporu yalnızca linkler değiştiğinde göndermek; değişmeyen komşuları göndermemek.
- **Dağıtık tahmin:** Her düğüm kendi linklerini kendisi tahmin edip kontrolcüye tahmin
  değerini (1 bayt) gönderir; bir düğümün işi komşu başına 37 çarpma-toplamadır.

## 5. Sonuç

- **Hesaplama ve bellek açısından uygulanabilir:** Lineer model 304 baytlık bir katsayı
  dosyasıdır; 16 düğümlü ağın karar döngüsü masaüstünde 1 ms'nin altında, kontrolcü durumu
  65 KB'tır. Gömülü sınıf bir cihaz için bu yük önemsizdir.
- **Asıl kısıt iletişimdir:** 1 Mbps'lik kanalda ölçüm + kontrol, kanal süresinin yaklaşık
  dörtte birini kullanır. Gerçek bir uygulamada yoklama ve rapor sıklığı ayarlanmalıdır.
- **Açık iş:** Ölçüm programının gerçek bir gömülü cihazda (ör. Raspberry Pi) çalıştırılıp
  süre ve bellek değerlerinin bu belgeye eklenmesi.
