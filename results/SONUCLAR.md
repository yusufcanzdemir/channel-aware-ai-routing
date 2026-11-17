# Sonuç özeti (otomatik üretildi: analysis/evaluate.py)

Bu dosya `./scripts/pipeline.sh` her çalıştığında yeniden yazılır. Ayarlar: `scripts/deney_ayarlari.sh`.

Seed sayısı (yöntem başına): 30-30. Değerler seed ortalamasıdır.

## Paket teslim oranı (%)

| Senaryo | AODV | OLSR | Son ölçüm (ideal) | EWMA (ideal) | Holt (ideal) | Model v2 (ideal) | Kâhin (ideal) | Son ölçüm (inband) | EWMA (inband) | Holt (inband) | Model v2 (inband) | Kâhin (inband) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Bozulmasız | 99.2 | 94.0 | 99.8 | 99.8 | 99.8 | 99.8 | 99.9 | 99.5 | 99.6 | 99.6 | 99.5 | 98.4 |
| Fading | 63.3 | 65.9 | 94.8 | 96.1 | 95.5 | 96.3 | 97.0 | 88.9 | 88.1 | 89.1 | 91.4 | 92.7 |
| Kademeli bozulma | 56.1 | 53.1 | 89.5 | 90.3 | 88.5 | 92.3 | 92.2 | 74.2 | 76.4 | 73.8 | 80.5 | 90.4 |
| Girişim | 76.4 | 63.0 | 84.0 | 84.8 | 82.8 | 88.4 | 83.3 | 79.0 | 80.7 | 77.2 | 86.2 | 80.7 |
| Jamming | 67.5 | 67.2 | 71.3 | 70.5 | 70.5 | 73.3 | 75.7 | 71.5 | 71.4 | 71.1 | 73.8 | 75.0 |
| Düğüm arızası | 95.4 | 89.9 | 99.4 | 99.3 | 99.2 | 99.3 | 88.2 | 96.9 | 97.0 | 97.3 | 97.2 | 87.1 |
| Tıkanıklık | 79.3 | 79.9 | 87.1 | 90.3 | 89.2 | 93.1 | 90.2 | 79.0 | 81.9 | 80.6 | 88.9 | 87.1 |

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
