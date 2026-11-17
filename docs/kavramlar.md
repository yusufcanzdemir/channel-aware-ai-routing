# Projeye Giriş Rehberi: Kavramlar ve Kodun Anlatımı

Bu belge ~20-25 dakikada okunur. Amaç: şu ana kadar yapılanı ve bundan sonra
kullanacağımız kavramları anlamak. Okurken [ns3/ai_routing.cc](../ns3/ai_routing.cc)
dosyasını yan tarafta açık tutman önerilir.

---

## 1. Büyük resim: proje ne yapıyor?

Kablosuz bir mesh ağında paketler kaynaktan hedefe birkaç ara düğüm üzerinden
(çok sekmeli / multi-hop) gider. Hangi yoldan gideceğine **yönlendirme protokolü** karar verir.

Klasik protokoller (AODV, OLSR) genelde "link var mı yok mu" ya da "kaç sekme" diye bakar.
Bir link yavaş yavaş kötüleşirken (sinyal zayıflıyor, paket kaybı artıyor) bunu geç fark ederler;
link kopunca paketler kaybolur, sonra yeni rota aranır.

Bizim fikir: **linklerin ölçümlerinden (SNR, kayıp, gecikme) yakın gelecekteki kalitesini
tahmin et, kötüleşecek linkten trafiği önceden çek.**

```
link ölçümleri → tahmin modeli → link maliyeti → Dijkstra (en ucuz yol) → rota tabloları
```

Bunu gerçek cihazlarla değil **NS-3 simülatöründe** deniyoruz: her şey tekrarlanabilir
ve ölçülebilir olsun diye.

---

## 2. Projenin yol haritası

| Adım | İçerik | Durum |
| --- | --- | --- |
| 1 | Simülasyon altyapısı + AODV/OLSR referans ölçümü | ✓ bitti |
| 2 | Bozulma senaryoları + link başına ölçüm CSV'si (modelin eğitim verisi) | ✓ ilk sürüm (fading, degrade) |
| 3 | Python'da tahmin modeli eğitimi (`ml/`) | ✓ ilk model; iyileştirilecek |
| 4 | NS-3 içinde AI kontrolcü (`--routing=ai`) | ✓ (gecikme deneyi, model v2, kâhin tavanı) |
| 5 | Üç yöntemin senaryolarla karşılaştırılması | |
| 6 | Modelin gerçek cihazda çalışabilirliği + rapor | |

---

## 3. NS-3 temel kavramları

NS-3 bir **ayrık olay simülatörü**dür (discrete-event simulator). Gerçek zaman akmaz;
"0.53. saniyede düğüm 3 paket gönderir" gibi olaylar bir kuyruğa konur ve sırayla işlenir.
120 saniyelik simülasyon birkaç saniyede biter. Arayüzü yoktur; C++ programı yazıp
terminalden çalıştırırsın.

Bir NS-3 programı gerçek bir ağı katman katman kurar:

| Kavram | Gerçek dünyadaki karşılığı | Bizim kodda |
| --- | --- | --- |
| **Node** (düğüm) | Boş bir bilgisayar kasası | `NodeContainer nodes` |
| **MobilityModel** | Cihazın konumu | `GridPositionAllocator` → ızgara |
| **Channel** (kanal) | Havadaki radyo ortamı | `YansWifiChannelHelper` |
| **PropagationLossModel** | Sinyalin mesafeyle zayıflaması | `LogDistancePropagationLossModel` |
| **PHY** (fiziksel katman) | Antenin/radyonun kendisi | `YansWifiPhyHelper`, verici gücü |
| **MAC** | Kim ne zaman konuşur kuralları | `AdhocWifiMac` |
| **NetDevice** | Wi-Fi kartı | `wifi.Install(...)` sonucu `devices` |
| **Internet stack** | IP, UDP, yönlendirme | `InternetStackHelper` |
| **Application** | Trafiği üreten program | `OnOffHelper` (gönderen), `PacketSinkHelper` (alan) |

**Helper** sınıfları (`WifiHelper`, `MobilityHelper` ...) bu parçaları toplu kurmaya yarar;
NS-3 kodunun çoğu "helper oluştur → ayar yap → düğümlere kur" kalıbıdır.

**Attribute:** NS-3 nesnelerinin ayarlanabilir özellikleri. Örn.
`wifiPhy.Set("TxPowerStart", DoubleValue(16.0))` verici gücünü 16 dBm yapar.

**Trace (iz):** NS-3'te bir şey olduğunda (paket alındı, düştü...) haber veren kanca.
Kendi fonksiyonumuzu bağlarız. Adım 2'de link başına SNR'ı bu yolla ölçeceğiz.

---

## 4. Kablosuz kanal kavramları

**dBm:** Güç birimi, logaritmik. 0 dBm = 1 mW; her -10 dBm on kat daha az güç.
-78 dBm çok küçük ama Wi-Fi için alınabilir bir sinyaldir.

**Yol kaybı (path loss):** Sinyal mesafeyle zayıflar. Log-distance modelinde
kayıp ≈ sabit + 10·n·log10(mesafe). `n` (üs) büyüdükçe ortam daha "engelli" olur
(açık alan ≈ 2, bina ≈ 3-4). Biz n = 3 kullanıyoruz.

**Alınan güç = verici gücü − yol kaybı.** 40 m'de ≈ -78.7 dBm.

**Algılama eşiği:** NS-3 -82 dBm'in altındaki sinyali hiç duymaz. Bu yüzden:
- 40 m komşu → -78.7 dBm → duyulur (3.3 dB pay)
- 56.6 m çapraz komşu → -83.3 dBm → duyulmaz

Sonuç: her düğüm sadece 4 komşusunu duyar, uzak düğümlere paket mecburen
birkaç sekmede gider. (Hafta 2'de 100 m denendiğinde PDR %0 çıkmasının sebebi buydu.)

**SNR (Signal-to-Noise Ratio):** Sinyalin gürültüden kaç dB güçlü olduğu.
Yüksek SNR = temiz link. SNR düşünce paket hatası artar. Modelin en önemli girdisi bu olacak.

**Fading (sönümleme):** Gerçekte sinyal gücü zamanla dalgalanır (yansımalar, hareket eden
nesneler). `--scenario=none`'da fading yok: her link ya hep iyi ya hep kopuk. Bu yüzden
kendi kanal modelimizi yazdık (bkz. bölüm 9).

**Girişim (interference) / jamming:** Başka vericilerin aynı kanalı kirletmesi; SNR'ı düşürür.

**802.11b, 1 Mbps sabit hız:** Eski ama basit Wi-Fi standardı. Normalde Wi-Fi kanal kötüleşince
hızını düşürür (rate adaptation); bunu kapattık ki farkı yönlendirme yöntemleri yaratsın.

---

## 5. Yönlendirme ve trafik kavramları

**AODV (Ad hoc On-Demand Distance Vector) — reaktif:** Rota ancak paket gönderileceği zaman aranır.
Kaynak "hedefe giden yol bilen var mı?" diye ağa sorar (RREQ), hedef cevap verir (RREP).
Avantaj: boşta az kontrol trafiği. Dezavantaj: ilk pakette ve link kopunca gecikme.

**OLSR (Optimized Link State Routing) — proaktif:** Her düğüm sürekli HELLO (komşularım kim)
ve TC (topoloji) mesajları yayar; herkes tüm ağ haritasını bilir ve rotalar hazırdır.
Avantaj: rota hep hazır. Dezavantaj: sürekli kontrol trafiği.

**Bizim AI kontrolcü — merkezi:** Tek bir "beyin" tüm linklerin ölçümünü görür, tahmin eder,
Dijkstra ile en düşük maliyetli yolları hesaplar ve rotaları düğümlere yazar
(NS-3'te `Ipv4StaticRouting` ile). SDN'e benzer.

**Dijkstra:** Her linkin bir maliyeti olan grafikte en ucuz yolu bulan klasik algoritma.
Bizde link maliyeti = "bu link yakında ne kadar kötü olacak" tahmininden türetilecek.

**UDP:** Bağlantısız taşıma protokolü; paket kaybolursa tekrar göndermez. Bunu seçtik çünkü
TCP'nin yeniden iletimleri kaybı gizler ve ölçümü karıştırır.

**OnOff uygulaması:** Belirli hızda (bizde 32 kbps, 512 bayt paketler) sürekli paket üretir.

---

## 6. Metrikler (FlowMonitor ile ölçülüyor)

**Akış (flow):** Aynı kaynak IP/port → hedef IP/port paketleri. FlowMonitor her akışı ayrı sayar.

| Metrik | Anlamı | İyi olan |
| --- | --- | --- |
| **PDR** (Packet Delivery Ratio) | Alınan / gönderilen paket (%) | Yüksek |
| **Gecikme** | Paketin kaynaktan hedefe ortalama süresi (ms) | Düşük |
| **Throughput** | Saniyede hedefe ulaşan veri (kbps) | Yüksek |
| **Jitter** | Ardışık paketlerin gecikmesindeki dalgalanma (ms) | Düşük |
| **Ort. sekme** | Paket kaç link üzerinden geçti | Kısa yol, ama tek başına iyi/kötü değil |

Not: OLSR/AODV'nin kendi kontrol mesajları da akış sayılır; biz sadece port 9'daki
(uygulama trafiği) akışları topluyoruz.

---

## 7. Deney kavramları

**Seed / run:** Simülasyondaki rastgelelik (hangi düğümler konuşur, çarpışma zamanlamaları)
bir sayıdan türetilir. Aynı seed = birebir aynı sonuç. `--seed=3` ile seçilir
(`RngSeedManager::SetRun`).

**Neden çok seed?** Tek çalıştırma şans eseri iyi/kötü çıkabilir. 10 seed çalıştırıp ortalama alırız.

**%95 güven aralığı (GA):** "99.28 ± 0.27" demek: gerçek ortalama büyük olasılıkla
99.01-99.55 aralığında. İki yöntemin aralıkları örtüşmüyorsa fark muhtemelen gerçektir.

**Adil karşılaştırma:** Aynı seed'de AODV ve OLSR aynı topoloji, aynı akış çiftleri ile koşar;
tek fark `--routing`. (Kaynak-hedef seçimi sabit stream 1000 kullanır, bu yüzden protokolden etkilenmez.)

**Isınma (warmup):** Trafik 30. saniyede başlar; protokoller önce rota kursun diye.

---

## 8. ai_routing.cc bölüm bölüm

Dosya tek bir `main()` fonksiyonu; içinde numaralı yorumlarla 7 bölüm var:

1. **Komut satırı parametreleri (satır 54-135):** Tüm ayarlar varsayılan değerlerle tanımlanır,
   `CommandLine` ile `--routing=olsr --seed=3` gibi değiştirilebilir. `--routing=ai` şimdilik hata verir.
   Sonunda `SetRun(seed)` ile rastgelelik sabitlenir.
2. **Düğümler ve konumlar (137-153):** 16 düğüm, 4'lü satırlar halinde 40 m aralıklı ızgara, hareketsiz.
3. **Wi-Fi (155-196):** 802.11b, sabit 1 Mbps, log-distance yol kaybı (üs 3), 16 dBm güç, ad hoc mod; fading/degrade senaryosunda zincire `SlowFadingLossModel` eklenir.
4. **IP ve yönlendirme (198-219):** Seçilen protokol (AODV veya OLSR) kurulur,
   düğümlere 10.1.1.x adresleri verilir.
5. **Trafik (221-280):** Seed'e bağlı olarak 4 kaynak-hedef çifti seçilir (en az 80 m uzak).
   Hedefe alıcı, kaynağa 32 kbps gönderici kurulur; trafik 30-115. saniyeler arası.
6. **Çalıştırma (282-301):** `--linkCsv` verildiyse `LinkMonitor` (yoklamalar) başlatılır, FlowMonitor kurulur, simülasyon 120 s koşturulur.
7. **Sonuçlar (303-382):** Port 9 akışları toplanır, metrikler hesaplanır, ekrana basılır
   ve CSV'ye bir satır eklenir; link ölçümleri ayrı CSV'ye yazılır.

### Düğüm numaraları (4x4 ızgara)

```
 0 --  1 --  2 --  3        her "--" ve "|" 40 m'lik bir link
 |     |     |     |        çaprazlar (56.6 m) link değil
 4 --  5 --  6 --  7
 |     |     |     |
 8 --  9 -- 10 -- 11
 |     |     |     |
12 -- 13 -- 14 -- 15
```

---

## 9. Bozulma senaryoları, link ölçümü ve tahmin modeli (Hafta 3)

### Senaryolar (`--scenario=`)

| Senaryo | Ne olur |
| --- | --- |
| `none` | Kanal sabit. Link ya hep var ya hep yok |
| `fading` | Her linkte sinyal birkaç saniyelik ölçekte rastgele dalgalanır (±2 dB civarı) |
| `degrade` | fading + rastgele anlarda bir link 10 s içinde 4-8 dB kötüleşir, 5 s öyle kalır, 10 s'de toparlanır |

Bunu [ns3/slow-fading-loss-model.cc](../ns3/slow-fading-loss-model.cc) yapar. NS-3'te
**propagation loss model** zincir halindedir: önce mesafe kaybı (log-distance), sonra bizim
modelin eklediği kayıp uygulanır.

**AR(1) süreci:** "Yeni değer = eski değerin bir kısmı + biraz rastgelelik". Değer birden
sıçramaz, yavaş yavaş değişir. **τ (tau)** ne kadar yavaş değiştiğini, **σ (sigma)** ne kadar
büyük dalgalandığını belirler.

Komşu linklerin algılama eşiğine payı sadece 3.3 dB olduğu için birkaç dB'lik dalgalanma
linkleri zaman zaman zayıflatır veya koparır. Çapraz (56.6 m) linkler ise zaman zaman
kullanılabilir hale gelir.

### Link ölçümü (`--linkCsv=dosya.csv`)

[ns3/link-monitor.cc](../ns3/link-monitor.cc): Her düğüm saniyede 10 küçük **yoklama (probe)**
paketi yayınlar. Komşular her saniyede kaç tanesini aldıklarını sayar → **link teslim oranı**
(10 gönderildi, 7 alındı → 0.7). Aynı anda o komşudan duyulan çerçevelerin **SNR**'ı da kaydedilir.
Gerçek ağlarda kullanılan **ETX** ölçümünün mantığı da budur.

CSV'deki bir satır: "seed 3'te, 45. saniyede, düğüm 5 → düğüm 6 linkinde 10 yoklamadan 8'i
ulaştı, ortalama SNR 13.2 dB".

### Tahmin modeli ([ml/train_link_model.py](../ml/train_link_model.py))

- **Girdi (öznitelik / feature):** Linkin son 10 saniyelik teslim oranları, son 3 saniyelik
  SNR'ı, eğilimler.
- **Hedef (target):** Önümüzdeki 3 saniyenin ortalama teslim oranı.
- **Eğitim / test:** Seed 1-7 ile eğitilir, model hiç görmediği seed 8-10'da sınanır.
- **Referans (baseline):** "Son değer aynı kalır". Klasik protokoller de zımnen bunu varsayar.
  Model işe yarıyorsa bunu geçmeli.
- **Lineer regresyon:** Girdilerin ağırlıklı toplamı. En basit öğrenen model, cihaza
  birkaç sayıyla taşınır (`ml/models/linear_h3.json`).
- **Gradient Boosting:** Birbirinin hatasını düzelten çok sayıda küçük karar ağacı.

**Metrikler:**
- **MAE / RMSE:** Ortalama tahmin hatası. RMSE büyük hataları daha çok cezalandırır.
- **Precision / recall / F1 (kötü link):** "Bu link kötüleşecek (teslim < 0.5)" dediğinde
  ne kadar haklı (precision), kötüleşen linklerin ne kadarını yakaladı (recall), F1 ikisinin dengesi.

**İlk sonuç (degrade):** RMSE "son değer" 0.211 → Gradient Boosting 0.186 (%12 daha iyi).
Mütevazı ama gerçek bir kazanç. Sadece fading senaryosunda model referansı geçemedi:
o kanal hafızasız olduğu için geçmiş, şimdiki değerden fazlasını söylemiyor.

---

## 10. Diğer dosyalar

| Dosya | Ne yapar |
| --- | --- |
| [scripts/run_baseline.sh](../scripts/run_baseline.sh) | AODV ve OLSR'yi 10'ar seed ile çalıştırır, `data/baseline.csv`'ye yazar, özeti çıkarır (yoklamasız, ilk referans) |
| [ns3/ai-controller.cc](../ns3/ai-controller.cc) | AI kontrolcü: tahmin → maliyet → Dijkstra → statik rotalar |
| [scripts/run_comparison.sh](../scripts/run_comparison.sh) | 5 yöntemi (AODV, OLSR, AI × 3 tahminci) modelin görmediği seed 101-110'da karşılaştırır |
| [scripts/run_scenario.sh](../scripts/run_scenario.sh) | Bir senaryoda AODV/OLSR × 10 seed, yoklamalar açık; sonuç + link verisi + özet |
| [ml/train_link_model.py](../ml/train_link_model.py) | Link kalitesi tahmin modellerini eğitir, karşılaştırır, grafik çizer |
| [analysis/plot_demo.py](../analysis/plot_demo.py) | Senaryo karşılaştırma grafiği + topoloji animasyonu |
| [analysis/summarize.py](../analysis/summarize.py) | CSV'yi yöntem başına gruplayıp ortalama ± %95 GA hesaplar |
| [results/baseline_ozet.csv](../results/baseline_ozet.csv) | Referans sonuçlar |
| [docs/kararlar.md](kararlar.md) | Her tasarım kararı ve gerekçesi (rapor için) |
| [docs/gunluk.md](gunluk.md) | Haftalık ilerleme günlüğü |

### Akış: bir deney baştan sona

```
./scripts/run_scenario.sh degrade
   └─ her yöntem × seed için: ./ns3 run "ai_routing --scenario=degrade --linkCsv=..."
         └─ simülasyon → data/degrade.csv'ye 1 satır + data/link_degrade.csv'ye link ölçümleri
   └─ analysis/summarize.py → ekrana tablo + results/degrade_ozet.csv

source ~/Documents/_projects/venv/bin/activate
python ml/train_link_model.py         → results/model_karsilastirma_h3.csv + grafikler
python analysis/plot_demo.py          → results/figures/senaryo_karsilastirma.png, topoloji.gif
```

---

## 11. Şu ana kadarki sonuçlar ve açık sorular

Bozulmasız ağda (yoklamasız): **AODV PDR %99.3, gecikme 21 ms; OLSR PDR %96.8, gecikme 44 ms.**

Yoklamalar açık, 10 seed:

| Senaryo | AODV PDR | OLSR PDR |
| --- | --- | --- |
| Bozulmasız | %97 | %94 |
| Fading | %56 | %61 |
| Kademeli bozulma | %53 | %58 |

Kanal bozulunca klasik protokoller paketlerin yarıya yakınını kaybediyor.

### AI kontrolcü (`--routing=ai --aiPredictor=linear|last|ma`)

Her saniye: ölçümler → her link için tahmin → **maliyet = 1 / (p_gidiş × p_dönüş)** →
**Dijkstra** → her düğümün **statik rota tablosuna** "şu hedefe şu komşu üzerinden git" yazılır.
Maliyet, bir paketin linki geçmesi için beklenen deneme sayısıdır (ETX). Gidiş ve dönüş birlikte
çarpılır çünkü Wi-Fi her unicast paketi alındı onayıyla (ACK) doğrular. Tahmini teslim oranı
0.3'ün altındaki link hiç kullanılmaz.

Üç tahminci var. Böylece kazancın modelden mi yoksa merkezi yönlendirmeden mi geldiği ayrılır:
`last` = son ölçüm, `ma` = son 5 s ortalaması, `linear` = eğitilmiş model.

**Sonuç (PDR %, modelin görmediği seed 101-110):**

| Senaryo | AODV | OLSR | AI-son | AI-ort | AI-model |
| --- | --- | --- | --- | --- | --- |
| Bozulmasız | 99.3 | 93.0 | 99.9 | 99.8 | 99.8 |
| Fading | 65.1 | 65.0 | 95.6 | 94.0 | 93.7 |
| Kademeli bozulma | 51.7 | 54.2 | 87.9 | 80.9 | 89.1 |

Yorum: Asıl fark **merkezi, link kalitesine bakan yönlendirmeden** geliyor. Tahmin modelinin
"son ölçüm"e üstünlüğü henüz istatistiksel olarak anlamlı değil. Önemli kısıt: kontrol kanalı
ideal (ölçüm ve rotalar anında ulaşıyor). Gerçekte bu gecikme olunca son ölçüm bayatlar;
tahminin değeri orada ortaya çıkabilir. Sıradaki deney bu.

### Kontrol gecikmesi deneyi (`--aiDelay`, `--aiInterval`)

Gerçek bir ağda ölçümlerin kontrolcüye gitmesi ve rotaların düğümlere dönmesi zaman alır.
`--aiDelay=2` ile rotalar hesaplandıktan 2 s sonra uygulanır, yani karar 2 s bayat bilgiyle
verilmiş olur. Her gecikme için "o kadar ileriyi" tahmin eden ayrı bir model eğitilir
(`python ml/train_link_model.py --offset 2` → `ml/models/linear_o2_h3.txt`).

**Sonuç:** Gecikme yokken model ile son ölçüm aynı. Gecikme varken model ortalama
**+2.9 puan** daha iyi (60 karşılaştırmanın 47'sinde önde, p = 0.012). Yani tahmin, bilginin
bayatladığı gerçekçi koşullarda değer katıyor. Kazanç mütevazı. Asıl büyük kazanç hâlâ merkezi
yönlendirmenin kendisinden geliyor.

### Model v2 (`--aiPredictor=linear2`)

İlk modele iki fiziksel bilgi eklendi:
- **Sinyal payı (margin):** Alınan gücün algılama eşiğinin (-82 dBm) kaç dB üstünde olduğu.
  Pay 1 dB'e düştüyse link kopmanın eşiğinde demektir. Teslim oranı henüz 1.0 görünse bile.
- **Ters yön linki:** 5→6 linkinin geleceğini tahmin ederken 6→5 linkinin ölçümlerine de bakılır.
  Sönümleme iki yönde aynı olduğu için bu, aynı kanalın ikinci bir ölçümüdür ve gürültüyü azaltır.

Gecikmeli koşullarda kazanç ikiye katlandı: son ölçüme göre v1 +2.9, **v2 +5.6 puan**
(60 karşılaştırmanın 50'sinde önde, p < 0.0001). Ders: Daha karmaşık bir model yerine
**doğru bilgiyi** (öznitelik) vermek çok daha etkili oldu. Model hâlâ lineer.

### Kâhin (oracle) ve "tavan" deneyi (`--aiPredictor=oracle`)

**Kâhin**, rotaların uygulanacağı dönemde linklerin gerçek durumunu bilen hayali bir
tahmincidir. Kanal modelini biz yazdığımız için bunu hesaplayabiliyoruz. Gerçek bir sistemde
imkânsızdır. Amacı şu soruyu cevaplamak: **"Tahmin mükemmel olsaydı en fazla ne kazanırdık?"**
Bu değere **tavan (üst sınır)** denir. Kâhin çarpışmaları bilmez, sadece kanalı bilir.

Sonuç:
- **Gecikme yokken** kâhin, model v2'den anlamlı olarak iyi değil. Hatta kademeli bozulmada
  biraz geride (çünkü ölçüme dayalı model çarpışma/tıkanıklık etkisini de görüyor).
  → Bu koşulda daha iyi bir tahminci (RL dahil) kayda değer bir şey kazandıramaz.
- **Gecikme varken** kâhin ile v2 arasında 5-17 puan fark var.
  → Daha iyi tahminin değeri burada. Geliştirme yapılacaksa hedef bu koşul.

### Gerçekçi kontrol düzlemi (`--controlPlane=inband`)

İdeal modda kontrolcü ölçümleri sihirli şekilde anında görüyordu. Gerçekçi modda kontrolcü
düğüm 5'te çalışır; her düğüm her saniye bir **rapor** paketiyle ölçümlerini yollar, kontrolcü
0.2 s toplayıp hesaplar ve değişen tabloları **güncelleme** paketleriyle gönderir. Bu paketler
veri trafiğiyle aynı kanalı paylaşır, kaybolabilir. **In-band** = kontrol paketleri de veri ile
aynı ağdan, kontrolcünün kurduğu yollardan gider. **Flooding** = paketin komşudan komşuya
yayılarak herkese ulaştırılması (açılışta, yol henüz yokken kullanılır).

Sonuç: başarım 5-15 puan düşüyor ama AODV/OLSR'nin 24-28 puan önünde kalıyor. Model v2'nin son
ölçüme üstünlüğü bu koşulda artıyor (kademeli bozulmada +6.3 puan): bilgi eksik ve bayatken
tahmin daha değerli.

**Wilcoxon testi:** Eşli t-testinin, farkların normal dağıldığını varsaymayan versiyonu.

**Eşli t-testi (paired t-test):** Aynı seed'deki iki yöntemi karşılaştırır (seed başına fark).
p < 0.05 ise fark büyük olasılıkla tesadüf değildir.

OLSR'nin geride kalması için hipotez: HELLO/TC mesajları yayın (broadcast) olduğu için
alındı onayı (ACK) yok; çarpışınca kaybolur, düğüm komşusunu "kaybettiğini" sanar,
rota gereksiz yere değişir. Henüz doğrulanmadı.

---

## 12. Kısa dış kaynak listesi

- NS-3 Tutorial — "Conceptual Overview" ve "Building Topologies" (Wi-Fi kısmı):
  https://www.nsnam.org/docs/tutorial/html/
- AODV ve OLSR için birer kısa video ("AODV routing protocol explained", "OLSR explained")
- Örnek kod: `NS3_DIR/examples/routing/manet-routing-compare.cc` (bizim kodun ilham kaynağı)
