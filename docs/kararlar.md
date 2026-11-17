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
