# Sonuç özeti (otomatik üretildi: analysis/evaluate.py)

Bu dosya `./scripts/pipeline.sh` her çalıştığında yeniden yazılır. Ayarlar: `scripts/deney_ayarlari.sh`.

Seed sayısı (yöntem başına): 30-30. Değerler seed ortalamasıdır.

## Paket teslim oranı (%)

| Senaryo | AODV | OLSR | Son ölçüm (ideal) | EWMA (ideal) | Holt (ideal) | Model v2 (ideal) | Kâhin (ideal) | v2 + gecikme (1) (ideal) | v2 + gecikme (0.2) (ideal) | v2 gerçekçi veri (ideal) | v3 (+ yaş) (ideal) | v3 (+ yaş + EWMA) (ideal) | v3 Gradient Boosting (ideal) | Son ölçüm (inband) | EWMA (inband) | Holt (inband) | Model v2 (inband) | Kâhin (inband) | v2 + gecikme (1) (inband) | v2 + gecikme (0.2) (inband) | v2 gerçekçi veri (inband) | v3 (+ yaş) (inband) | v3 (+ yaş + EWMA) (inband) | v3 Gradient Boosting (inband) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Bozulmasız | 99.2 | 94.0 | 99.8 | 99.8 | 99.8 | 99.8 | 99.9 | 99.8 | 99.8 | 99.8 | 99.8 | 99.8 | 99.8 | 99.5 | 99.6 | 99.6 | 99.5 | 98.4 | 99.1 | 100.0 | 99.8 | 99.7 | 99.7 | 99.5 |
| Fading | 63.3 | 65.9 | 94.8 | 96.1 | 95.5 | 96.3 | 97.0 | 95.8 | 96.5 | 95.5 | 95.1 | 96.1 | 96.1 | 88.9 | 88.1 | 89.1 | 91.4 | 92.7 | 83.1 | 89.7 | 91.6 | 93.0 | 92.9 | 93.5 |
| Kademeli bozulma | 56.1 | 53.1 | 89.5 | 90.3 | 88.5 | 92.3 | 92.2 | 89.6 | 90.5 | 91.2 | 91.0 | 89.7 | 91.9 | 74.2 | 76.4 | 73.8 | 80.5 | 90.4 | 71.1 | 78.4 | 81.0 | 84.6 | 85.2 | 85.0 |
| Girişim | 76.4 | 63.0 | 84.0 | 84.8 | 82.8 | 88.4 | 83.3 | 88.4 | 87.9 | 88.3 | 88.7 | 89.6 | 89.0 | 79.0 | 80.7 | 77.2 | 86.2 | 80.7 | 82.0 | 84.5 | 85.7 | 85.7 | 85.9 | 81.8 |
| Jamming | 67.5 | 67.2 | 71.3 | 70.5 | 70.5 | 73.3 | 75.7 | 72.7 | 72.5 | 72.7 | 72.0 | 72.8 | 73.1 | 71.5 | 71.4 | 71.1 | 73.8 | 75.0 | 72.1 | 73.0 | 72.8 | 74.2 | 74.4 | 73.4 |
| Düğüm arızası | 95.4 | 89.9 | 99.4 | 99.3 | 99.2 | 99.3 | 88.2 | 99.3 | 99.3 | 99.3 | 99.3 | 99.3 | 99.4 | 96.9 | 97.0 | 97.3 | 97.2 | 87.1 | 96.3 | 96.6 | 97.2 | 97.7 | 97.3 | 97.6 |
| Tıkanıklık | 79.3 | 79.9 | 87.1 | 90.3 | 89.2 | 93.1 | 90.2 | 89.6 | 92.2 | 93.4 | 93.6 | 92.8 | 94.1 | 79.0 | 81.9 | 80.6 | 88.9 | 87.1 | 81.3 | 86.7 | 89.4 | 90.6 | 90.5 | 90.6 |

## Öğrenilmiş model (v2) ne kazandırıyor?

Fark = model v2 PDR − rakip PDR (puan), aynı seed'lerde eşli. p: eşli t-testi.

| Senaryo | Düzlem | v2 − son ölçüm | v2 − EWMA | v2 − Holt | Kâhin − v2 |
| --- | --- | --- | --- | --- | --- |
| Bozulmasız | ideal | -0.1 (p 0.143) | -0.1 (p 0.101) | -0.0 (p 0.476) | +0.2 (p 0.053) |
| Bozulmasız | inband | +0.0 (p 0.094) | -0.0 (p 0.307) | -0.1 (p 0.327) | -1.1 (p 0.349) |
| Fading | ideal | +1.6 (p 0.078) | +0.3 (p 0.602) | +0.8 (p 0.147) | +0.7 (p 0.270) |
| Fading | inband | +2.5 (p 0.010) | +3.3 (p 0.007) | +2.3 (p 0.010) | +1.3 (p 0.582) |
| Kademeli bozulma | ideal | +2.8 (p 0.011) | +2.0 (p 0.073) | +3.8 (p <0.001) | -0.1 (p 0.941) |
| Kademeli bozulma | inband | +6.3 (p <0.001) | +4.1 (p 0.048) | +6.7 (p 0.002) | +9.9 (p <0.001) |
| Girişim | ideal | +4.4 (p <0.001) | +3.5 (p <0.001) | +5.6 (p <0.001) | -5.1 (p <0.001) |
| Girişim | inband | +7.1 (p <0.001) | +5.5 (p <0.001) | +9.0 (p <0.001) | -5.4 (p <0.001) |
| Jamming | ideal | +2.0 (p 0.011) | +2.8 (p <0.001) | +2.8 (p <0.001) | +2.4 (p <0.001) |
| Jamming | inband | +2.3 (p <0.001) | +2.3 (p <0.001) | +2.6 (p <0.001) | +1.2 (p 0.007) |
| Düğüm arızası | ideal | -0.1 (p 0.252) | -0.0 (p 0.706) | +0.1 (p 0.365) | -11.1 (p <0.001) |
| Düğüm arızası | inband | +0.3 (p 0.232) | +0.2 (p 0.383) | -0.0 (p 0.886) | -10.1 (p <0.001) |
| Tıkanıklık | ideal | +6.1 (p <0.001) | +2.9 (p <0.001) | +4.0 (p <0.001) | -2.9 (p 0.006) |
| Tıkanıklık | inband | +9.9 (p <0.001) | +7.0 (p <0.001) | +8.3 (p <0.001) | -1.8 (p 0.252) |

## Klasik protokollere karşı (model v2)

| Senaryo | Düzlem | v2 − AODV | v2 − OLSR |
| --- | --- | --- | --- |
| Bozulmasız | ideal | +0.5 (24/30 seed önde) | +5.8 (29/30 seed önde) |
| Bozulmasız | inband | +0.3 (28/30 seed önde) | +5.6 (29/30 seed önde) |
| Fading | ideal | +33.0 (30/30 seed önde) | +30.4 (30/30 seed önde) |
| Fading | inband | +28.1 (29/30 seed önde) | +25.5 (30/30 seed önde) |
| Kademeli bozulma | ideal | +36.2 (30/30 seed önde) | +39.2 (30/30 seed önde) |
| Kademeli bozulma | inband | +24.4 (29/30 seed önde) | +27.4 (30/30 seed önde) |
| Girişim | ideal | +12.0 (27/30 seed önde) | +25.3 (30/30 seed önde) |
| Girişim | inband | +9.8 (26/30 seed önde) | +23.1 (30/30 seed önde) |
| Jamming | ideal | +5.8 (26/30 seed önde) | +6.1 (22/30 seed önde) |
| Jamming | inband | +6.2 (28/30 seed önde) | +6.5 (23/30 seed önde) |
| Düğüm arızası | ideal | +3.9 (22/30 seed önde) | +9.4 (30/30 seed önde) |
| Düğüm arızası | inband | +1.9 (17/30 seed önde) | +7.3 (27/30 seed önde) |
| Tıkanıklık | ideal | +13.8 (30/30 seed önde) | +13.2 (30/30 seed önde) |
| Tıkanıklık | inband | +9.6 (24/30 seed önde) | +9.0 (24/30 seed önde) |

## Gecikmeli maliyet (ETT) ne kazandırıyor?

Model v2 aynı; sadece link maliyeti değişiyor: ETX yerine beklenen iletim süresi + ağırlık × ölçülen gecikme. Fark = ETT − ETX (aynı seed'ler). PDR'da + iyi; gecikmede − iyi.

| Senaryo | Düzlem | ağırlık 1: PDR | ağırlık 1: gecikme (ms) | ağırlık 0.2: PDR | ağırlık 0.2: gecikme (ms) |
| --- | --- | --- | --- | --- | --- |
| Bozulmasız | ideal | +0.0 (p 0.654) | -0.1 (p 0.631) | +0.0 (p 0.591) | -0.1 (p 0.667) |
| Bozulmasız | inband | -0.4 (p 0.615) | -0.2 (p 0.006) | +0.4 (p 0.331) | +0.0 (p 0.581) |
| Fading | ideal | -0.5 (p 0.397) | +1.4 (p 0.168) | +0.2 (p 0.689) | -0.7 (p 0.379) |
| Fading | inband | -8.4 (p 0.003) | +10.6 (p 0.037) | -1.7 (p 0.494) | +0.3 (p 0.873) |
| Kademeli bozulma | ideal | -2.7 (p 0.013) | +2.0 (p 0.258) | -1.7 (p 0.105) | -2.6 (p 0.049) |
| Kademeli bozulma | inband | -9.4 (p <0.001) | +32.7 (p 0.003) | -2.0 (p 0.317) | +4.0 (p 0.592) |
| Girişim | ideal | +0.0 (p 0.937) | -1.7 (p 0.488) | -0.4 (p 0.693) | -4.8 (p 0.016) |
| Girişim | inband | -4.2 (p 0.004) | +55.3 (p <0.001) | -1.7 (p 0.077) | +16.4 (p 0.007) |
| Jamming | ideal | -0.6 (p 0.258) | -1.9 (p 0.268) | -0.8 (p 0.019) | -1.1 (p 0.472) |
| Jamming | inband | -1.7 (p 0.053) | +13.2 (p 0.019) | -0.8 (p 0.271) | +3.2 (p 0.216) |
| Düğüm arızası | ideal | +0.0 (p 0.649) | -0.1 (p 0.722) | +0.0 (p 0.094) | +0.0 (p 0.814) |
| Düğüm arızası | inband | -1.0 (p 0.468) | -0.3 (p 0.824) | -0.7 (p 0.479) | -0.6 (p 0.533) |
| Tıkanıklık | ideal | -3.6 (p <0.001) | +24.8 (p 0.013) | -1.0 (p 0.071) | +6.2 (p 0.258) |
| Tıkanıklık | inband | -7.6 (p <0.001) | +40.7 (p 0.003) | -2.2 (p 0.067) | +15.2 (p 0.143) |

## Model v3: kazanç nereden geliyor? (ablasyon)

Her sütun bir adımın katkısı: bir önceki modele göre eşli fark (puan). Son iki sütun klasik rakip EWMA'ya göre fark. p: eşli t-testi. Ayrıntı: ml/train_v3.py.

| Senaryo | Düzlem | gerçekçi veri | + yaş | + EWMA | doğrusal olmayan | v3 lineer − EWMA | v3 GB − EWMA |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Bozulmasız | ideal | +0.1 (p 0.110) | -0.0 (p 0.645) | -0.0 (p 0.916) | -0.0 (p 0.520) | -0.0 (p 0.778) | -0.0 (p 0.458) |
| Bozulmasız | inband | +0.2 (p 0.314) | -0.1 (p 0.334) | -0.0 (p 0.345) | -0.1 (p 0.279) | +0.1 (p 0.299) | -0.0 (p 0.269) |
| Fading | ideal | -0.8 (p 0.223) | -0.4 (p 0.299) | +1.0 (p 0.057) | +0.1 (p 0.895) | +0.0 (p 0.986) | +0.1 (p 0.904) |
| Fading | inband | +0.2 (p 0.741) | +1.4 (p 0.196) | -0.1 (p 0.837) | +0.6 (p 0.544) | +4.8 (p 0.004) | +5.4 (p 0.002) |
| Kademeli bozulma | ideal | -1.1 (p 0.459) | -0.2 (p 0.776) | -1.3 (p 0.369) | +2.1 (p 0.145) | -0.5 (p 0.656) | +1.6 (p 0.188) |
| Kademeli bozulma | inband | +0.5 (p 0.661) | +3.6 (p 0.003) | +0.6 (p 0.353) | -0.2 (p 0.815) | +8.8 (p <0.001) | +8.7 (p <0.001) |
| Girişim | ideal | -0.1 (p 0.908) | +0.4 (p 0.490) | +0.9 (p 0.016) | -0.6 (p 0.328) | +4.8 (p <0.001) | +4.1 (p <0.001) |
| Girişim | inband | -0.4 (p 0.381) | -0.1 (p 0.763) | +0.2 (p 0.661) | -4.1 (p 0.007) | +5.2 (p <0.001) | +1.1 (p 0.454) |
| Jamming | ideal | -0.6 (p 0.365) | -0.7 (p 0.180) | +0.9 (p 0.021) | +0.3 (p 0.798) | +2.4 (p 0.006) | +2.6 (p 0.005) |
| Jamming | inband | -1.0 (p 0.003) | +1.4 (p <0.001) | +0.1 (p 0.471) | -1.0 (p 0.007) | +2.9 (p <0.001) | +1.9 (p <0.001) |
| Düğüm arızası | ideal | -0.0 (p 0.491) | +0.0 (p 0.784) | -0.0 (p 0.523) | +0.1 (p 0.041) | -0.1 (p 0.245) | +0.0 (p 0.430) |
| Düğüm arızası | inband | -0.0 (p 0.924) | +0.5 (p 0.097) | -0.4 (p 0.144) | +0.3 (p 0.341) | +0.3 (p 0.146) | +0.5 (p 0.082) |
| Tıkanıklık | ideal | +0.3 (p 0.681) | +0.2 (p 0.531) | -0.8 (p 0.046) | +1.3 (p 0.023) | +2.5 (p <0.001) | +3.8 (p <0.001) |
| Tıkanıklık | inband | +0.5 (p 0.475) | +1.2 (p 0.049) | -0.1 (p 0.793) | +0.1 (p 0.883) | +8.7 (p <0.001) | +8.7 (p <0.001) |

## Gerçekçi kontrol düzlemi (model v2, in-band)

| Senaryo | Kontrol paketi/s | Rapor teslimi | Güncelleme teslimi | Kontrol gecikmesi |
| --- | --- | --- | --- | --- |
| Bozulmasız | 65 | %97 | %99 | 226 ms |
| Fading | 61 | %87 | %95 | 250 ms |
| Kademeli bozulma | 61 | %76 | %90 | 263 ms |
| Girişim | 64 | %72 | %84 | 274 ms |
| Jamming | 62 | %81 | %85 | 248 ms |
| Düğüm arızası | 64 | %91 | %96 | 227 ms |
| Tıkanıklık | 63 | %81 | %89 | 276 ms |

Grafikler: `results/figures/eval_pdr.png`, `results/figures/eval_model_kazanci.png`
