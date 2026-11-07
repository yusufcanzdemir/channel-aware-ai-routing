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
