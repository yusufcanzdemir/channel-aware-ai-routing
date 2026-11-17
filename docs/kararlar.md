# Karar Günlüğü

Her önemli kararı tarih, karar ve gerekçe olarak ekle. Rapor yazarken buradan faydalanılacak.

| Tarih | Karar | Gerekçe |
| --- | --- | --- |
| Hafta 1 | Simülatör olarak NS-3.44 | Hazır AODV/OLSR/DSDV, güçlü Wi-Fi modelleri, Python ML entegrasyon seçenekleri |
| Hafta 1 | Hibrit yaklaşım: tahmin modeli + Dijkstra | Açıklanabilir, eğitimi kararlı, cihazda çalıştırması kolay; RL opsiyonel |
| Hafta 1 | Sabit düğümlü Wi-Fi ad hoc mesh | Hareketlilik kapsamı gereksiz büyütür; kanal etkileri izole incelenir |
| Hafta 1 | Merkezi kontrolcü (SDN benzeri) | 14 haftada tek kişi için dağıtık protokol yazmak riskli |
| Hafta 1 | Model çevrimdışı eğitilecek | Simülatörle canlı entegrasyon karmaşıklığından kaçınmak |
| Hafta 2 | 16 düğüm, 4x4 ızgara, komşu arası 40 m | Her düğüm yalnızca 4 komşusunu duyar (çapraz komşu menzil dışı), trafik çok sekmeli olur |
| Hafta 2 | 802.11b, sabit hız (DsssRate1Mbps), hız uyarlama kapalı | Wi-Fi'nin kendi hız düşürmesi yöntemler arasındaki farkı gizlemesin |
| Hafta 2 | LogDistance yol kaybı (üs 3), verici gücü 16 dBm | Friis'ten gerçekçi; 40 m'de alım ≈ -78.7 dBm, NS-3 algılama eşiği (-82 dBm) üstünde 3.3 dB pay |
| Hafta 2 | 4 UDP akışı, 512 B, 32 kbps; kaynak-hedef en az 2 komşu uzaklıkta, seed'e bağlı sabit stream ile seçilir | Aynı seed tüm yöntemlerde aynı akışları verir (adil karşılaştırma) |
| Hafta 2 | Trafik 30. s'de başlar, 115. s'de biter, simülasyon 120 s | Protokoller rota kurmadan ölçüm alınmasın; son paketler hedefe ulaşabilsin |
| Hafta 2 | Metrikler sadece uygulama portundaki (9) akışlardan | FlowMonitor OLSR/AODV kontrol mesajlarını da akış sayar |
| Hafta 3 | Kendi kanal modelimiz: `SlowFadingLossModel` (AR(1) yavaş sönümleme + paket başına küçük gürültü + isteğe bağlı kademeli bozulma olayları) | NS-3'ün hazır fading modelleri (Nakagami vb.) her pakette bağımsız değişir; tahmin edilecek bir zaman yapısı yoktur. Link başına ayrı stream ve sabit zaman adımı: aynı seed'de kanal, yönlendirme yönteminden bağımsız olarak aynı şekilde değişir |
| Hafta 3 | Senaryolar: `none`, `fading` (σ = 2 dB, τ = 5 s), `degrade` (fading + link başına ~120 s'de bir, 10 s'de 4-8 dB'e inen, 5 s kalan, 10 s'de toparlanan olay) | σ = 3 dB'de PDR ~%25-50'ye düşüyordu (çok sert); 2 dB'de AODV/OLSR ~%55-60, iyileştirme payı var |
| Hafta 3 | Link ölçümü: her düğüm saniyede 10 yayın yoklaması (32 B), kendi EtherType'ıyla (0x88B5) doğrudan Wi-Fi cihazına; 1 s pencerede link teslim oranı + ortalama SNR/RSSI (`LinkMonitor`) | IP'yi kullanmadığı için yönlendirme protokolünden bağımsız; ETX benzeri, gerçek cihazda da uygulanabilir |
| Hafta 3 | Yöntem karşılaştırmalarında yoklamalar HER yöntemde açık (`run_scenario.sh`) | Yoklamalar ağa ek yük getirip PDR'ı düşürüyor (bozulmasız AODV %99.3 → %97.0). AI kontrolcü yoklamaya muhtaç; adil olması için AODV/OLSR de aynı yükle koşar |
| Hafta 3 | Model hedefi: sonraki 3 s'nin ortalama link teslim oranı; eğitim/test ayrımı seed'e göre (1-7 / 8-10) | Tek pencere (10 yoklama) çok gürültülü; ardışık saniyeler benzer olduğu için rastgele satır bölmek başarıyı şişirirdi |
| Hafta 3 | Referans tahminci "son değer"; ilk modeller lineer regresyon ve Gradient Boosting | "Son değer" klasik protokollerin zımni varsayımı; model en azından onu geçmeli. Lineer model C++'a birkaç katsayıyla taşınabilir |
| Hafta 3 | AI kontrolcü (`--routing=ai`): her 1 s'de ölçüm → tahmin → maliyet → Dijkstra → `Ipv4StaticRouting` host rotaları | Plandaki hibrit mimari; 16 düğümde tüm çiftler için Dijkstra her saniye ihmal edilebilir maliyette |
| Hafta 3 | Link maliyeti = 1 / (p_gidiş · p_dönüş), p < 0.3 olan link kullanılmaz | Tahmine dayalı ETX: unicast veri bir yöne, MAC ACK'i diğer yöne gider. Eşik, çok zayıf linkleri tamamen dışlar |
| Hafta 3 | C++'ta sadece lineer model (katsayılar `ml/models/linear_h3.txt`); öznitelikler Python ile birebir aynı, `--aiDebugCsv` ile doğrulandı (fark < 1e-4) | Gradient Boosting'e çok yakın (RMSE 0.189 / 0.186), C++'a taşıması ve cihazda çalıştırması çok kolay |
| Hafta 3 | Kontrol kanalı ideal: ölçümler kontrolcüye, rotalar düğümlere gecikmesiz/kayıpsız ulaşır | SDN çalışmalarında yaygın basitleştirme; AI'ya avantaj sağlar, raporda kısıt olarak belirtilecek. İleride gecikme eklenebilir |
| Hafta 3 | AI'nın üç tahmincisi karşılaştırılır: `last` (son ölçüm), `ma` (5 s ortalama), `linear` (model) | Kazancın tahminden mi yoksa merkezi ETX yönlendirmeden mi geldiğini ayırmak için |
| Hafta 3 | Yöntem karşılaştırması seed 101-110'da (`run_comparison.sh`); model seed 1-7 ile eğitildi | Model, test edildiği kanal gerçekleşmelerini eğitimde görmemiş olsun |
| Hafta 3 | Kontrol gecikmesi deneyi: `--aiDelay=D` rotaları D s geç uygular, `--aiInterval=K` K s'de bir günceller; kontrol mesajları simüle edilmez | İdeal kanal varsayımının etkisini ölçmek; gerçekte ölçüm toplama + rota dağıtma zaman alır |
| Hafta 3 | Her gecikme için ayrı model (`linear_o<D>_h3`): hedef t+D+1..t+D+3 ortalaması | Rotalar D s sonra uygulanacağı için önemli olan o andan sonraki link kalitesi |
| Hafta 3 | Gecikme deneyinde gecikme başına 10 seed tek tek anlamlı değil; tüm gecikmeli koşullar birlikte (60 eş) eşli t-testi + Wilcoxon ile değerlendirildi | Fark küçük (~3 puan), seed başına varyans büyük; havuzlanmış test güçlü. Daha kesin sonuç için seed sayısı artırılabilir |
| Hafta 3 | Model v2 (`--aiPredictor=linear2`, `train_link_model.py --features 2`): + sinyal payı (RSSI − (−82 dBm)) geçmişi/ortalaması/eğimi/maksimumu, + ters yön linkinin öznitelikleri | Pay, linkin kopmaya ne kadar yakın olduğunu doğrudan söyler; sönümleme iki yönde aynı olduğundan ters link ikinci bir ölçüm kaynağıdır. Hâlâ lineer (37 katsayı), C++/cihaz için hafif |
| Hafta 3 | Gradient Boosting C++'a taşınmadı | v2 özniteliklerle lineer 0.182, GB 0.179 RMSE: fark küçük, taşıma maliyeti büyük |
| Hafta 3 | `--aiHorizon=1` seçeneği (hedef: sadece sonraki 1 s) denendi | Rotalar her saniye güncellendiği için 1 s hedef kullanıma daha uygun. Sonuç h3 ile neredeyse aynı (gecikmeli havuz +6.0 / +5.6 puan); gecikmesiz fading'de h1 daha kötü → varsayılan h3 kaldı |
| Hafta 3 | Kâhin (oracle) tahminci (`--aiPredictor=oracle`): rotaların geçerli olacağı dönemin gerçek kanal durumundan beklenen teslim oranını hesaplar (Φ(pay/σ_hızlı)) | "Tahmin mükemmel olsaydı ne kazanırdık?" sorusu için üst sınır. RL gibi daha güçlü yöntemlere değip değmeyeceğine bu karar verir |
| Hafta 3 | Kanal modeli yörüngeyi (link başına kayıp dizisi) saklar; gelecek değerler erken üretilebilir | Her linkin kendi stream'i olduğundan değerler değişmez; 4 kontrol çalıştırmasında sonuçlar birebir aynı çıktı |
| Hafta 3 | Kâhin sadece KANALI bilir; çarpışma/tıkanıklığı bilmez | Bunları önceden bilmek kanal modelinden mümkün değil. Bu yüzden kâhin katı bir üst sınır değil, "mükemmel kanal tahmini" sınırıdır; ölçümle korelasyonu 0.92 |
| Hafta 3 | Gerçekçi kontrol düzlemi (`--controlPlane=inband`, `InbandControl`): kontrolcü düğüm 5'te; her düğüm her saniye UDP rapor (komşu başına 7 bayt) gönderir; kontrolcü 0.2 s toplar, hesaplar, değişen tabloları UDP güncelleme olarak yollar; düğüm paket ulaşınca uygular | "İdeal kontrol kanalı" eleştirisini kaldırmak; gecikme ve kayıp ağın kendisinden doğar |
| Hafta 3 | Kontrol paketleri in-band: kontrolcünün kurduğu rotalarla unicast; açılışta (ilk 5 s) ve rota yokken flooding (her düğüm bir kez yeniden yayınlar) | Gerçek SDN'lerdeki yaklaşım. Hep flooding kanalın ~%25'ini tüketirdi |
| Hafta 3 | Raporu gelmeyen düğümün linkleri için son bilinen rapor kullanılır; her 5 pencerede bir bütün tablolar yeniden gönderilir | Kayıplara karşı basit dayanıklılık. Bayat bilgi, gerçek sistemdeki bilgi yaşlanmasını temsil eder |
| Hafta 3 | In-band'de teslim oranı alıcı tarafında beklenen yoklama sayısına (10) göre hesaplanır | Alıcı, gönderenin gerçek gönderim sayısını bilemez (ideal modda gerçek sayı kullanılıyordu) |
