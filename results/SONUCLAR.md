# Sonuç özeti (otomatik üretildi: analysis/evaluate.py)

Bu dosya `./scripts/pipeline.sh` her çalıştığında yeniden yazılır. Ayarlar: `scripts/deney_ayarlari.sh`.

Seed sayısı (yöntem başına): 29-100. Değerler seed ortalamasıdır.

## Paket teslim oranı (%)

| Senaryo | AODV | OLSR | Son ölçüm (ideal) | EWMA (ideal) | Holt (ideal) | Model v2 (ideal) | Kâhin (ideal) | v2 gerçekçi veri (ideal) | v3 (+ yaş) (ideal) | v3 (+ yaş + EWMA) (ideal) | v3 Gradient Boosting (ideal) | Son ölçüm (inband) | EWMA (inband) | Holt (inband) | Model v2 (inband) | Kâhin (inband) | v2 gerçekçi veri (inband) | v3 (+ yaş) (inband) | v3 (+ yaş + EWMA) (inband) | v3 Gradient Boosting (inband) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Bozulmasız | 99.3 | 94.1 | 99.8 | 99.8 | 99.8 | 99.8 | 99.9 | 99.8 | 99.8 | 99.8 | 99.8 | 99.5 | 99.2 | 99.6 | 98.7 | 98.4 | 99.7 | 99.6 | 99.6 | 99.7 |
| Fading | 62.1 | 64.6 | 94.8 | 96.0 | 94.9 | 95.6 | 96.9 | 96.0 | 95.3 | 96.4 | 96.7 | 89.5 | 88.9 | 88.8 | 91.0 | 92.7 | 91.1 | 93.3 | 93.1 | 94.2 |
| Kademeli bozulma | 57.1 | 52.8 | 89.3 | 89.8 | 88.3 | 91.7 | 91.9 | 92.3 | 91.4 | 90.0 | 92.2 | 73.6 | 79.1 | 74.2 | 82.8 | 90.6 | 81.1 | 84.3 | 85.0 | 84.3 |
| Girişim | 78.4 | 63.5 | 84.0 | 84.7 | 83.0 | 89.1 | 83.7 | 88.8 | 89.0 | 89.2 | 88.3 | 79.4 | 79.7 | 77.5 | 85.4 | 81.0 | 85.2 | 85.9 | 85.5 | 85.0 |
| Jamming | 66.7 | 68.6 | 71.1 | 70.6 | 70.5 | 73.6 | 75.6 | 71.9 | 71.7 | 72.7 | 73.4 | 71.2 | 72.7 | 71.4 | 75.0 | 74.9 | 73.5 | 74.4 | 75.1 | 73.8 |
| Düğüm arızası | 95.5 | 90.9 | 99.4 | 99.3 | 99.2 | 99.3 | 88.2 | 99.3 | 99.3 | 99.3 | 98.8 | 97.1 | 95.6 | 97.2 | 95.7 | 87.1 | 97.5 | 97.4 | 96.3 | 95.8 |
| Tıkanıklık | 79.3 | 80.2 | 87.5 | 90.6 | 89.5 | 93.3 | 90.4 | 93.3 | 93.5 | 93.2 | 93.4 | 78.8 | 83.7 | 80.7 | 89.3 | 87.5 | 90.4 | 90.4 | 90.3 | 88.7 |

## Öğrenilmiş model (v2) ne kazandırıyor?

Fark = model v2 PDR − rakip PDR (puan), aynı seed'lerde eşli. p: eşli t-testi.

| Senaryo | Düzlem | v2 − son ölçüm | v2 − EWMA | v2 − Holt | Kâhin − v2 |
| --- | --- | --- | --- | --- | --- |
| Bozulmasız | ideal | -0.1 (p 0.196) | -0.1 (p 0.105) | -0.0 (p 0.561) | +0.1 (p 0.028) |
| Bozulmasız | inband | +0.0 (p 0.163) | -0.5 (p 0.221) | -0.1 (p 0.308) | -1.1 (p 0.344) |
| Fading | ideal | +0.8 (p 0.402) | -0.4 (p 0.639) | +0.7 (p 0.378) | +1.3 (p 0.113) |
| Fading | inband | +2.2 (p 0.019) | +2.0 (p <0.001) | +2.9 (p 0.002) | +0.9 (p 0.696) |
| Kademeli bozulma | ideal | +2.4 (p 0.026) | +1.9 (p 0.115) | +3.3 (p 0.002) | +0.3 (p 0.877) |
| Kademeli bozulma | inband | +8.1 (p <0.001) | +3.7 (p <0.001) | +7.4 (p <0.001) | +8.9 (p <0.001) |
| Girişim | ideal | +5.1 (p <0.001) | +4.4 (p <0.001) | +6.1 (p <0.001) | -5.4 (p <0.001) |
| Girişim | inband | +6.9 (p <0.001) | +5.6 (p <0.001) | +8.9 (p <0.001) | -5.4 (p <0.001) |
| Jamming | ideal | +2.5 (p <0.001) | +3.1 (p <0.001) | +3.2 (p <0.001) | +2.0 (p <0.001) |
| Jamming | inband | +2.7 (p <0.001) | +2.2 (p <0.001) | +2.6 (p <0.001) | +1.0 (p 0.009) |
| Düğüm arızası | ideal | -0.1 (p 0.378) | +0.0 (p 0.856) | +0.1 (p 0.441) | -11.1 (p <0.001) |
| Düğüm arızası | inband | +0.3 (p 0.201) | +0.2 (p 0.520) | +0.2 (p 0.301) | -10.3 (p <0.001) |
| Tıkanıklık | ideal | +5.8 (p <0.001) | +2.7 (p <0.001) | +3.8 (p <0.001) | -2.9 (p 0.008) |
| Tıkanıklık | inband | +10.9 (p <0.001) | +5.6 (p <0.001) | +9.0 (p <0.001) | -2.1 (p 0.173) |

## Klasik protokollere karşı (model v2)

| Senaryo | Düzlem | v2 − AODV | v2 − OLSR |
| --- | --- | --- | --- |
| Bozulmasız | ideal | +0.5 (22/30 seed önde) | +5.6 (28/30 seed önde) |
| Bozulmasız | inband | +0.3 (27/30 seed önde) | +5.4 (29/30 seed önde) |
| Fading | ideal | +33.5 (30/30 seed önde) | +31.0 (30/30 seed önde) |
| Fading | inband | +29.7 (29/30 seed önde) | +27.1 (30/30 seed önde) |
| Kademeli bozulma | ideal | +34.5 (30/30 seed önde) | +38.9 (30/30 seed önde) |
| Kademeli bozulma | inband | +24.5 (28/30 seed önde) | +28.9 (30/30 seed önde) |
| Girişim | ideal | +10.7 (29/30 seed önde) | +25.6 (30/30 seed önde) |
| Girişim | inband | +8.0 (27/30 seed önde) | +22.9 (30/30 seed önde) |
| Jamming | ideal | +6.9 (26/30 seed önde) | +5.0 (21/30 seed önde) |
| Jamming | inband | +7.3 (28/30 seed önde) | +5.3 (22/30 seed önde) |
| Düğüm arızası | ideal | +3.8 (21/30 seed önde) | +8.4 (30/30 seed önde) |
| Düğüm arızası | inband | +1.9 (17/30 seed önde) | +6.5 (27/30 seed önde) |
| Tıkanıklık | ideal | +13.9 (30/30 seed önde) | +13.1 (30/30 seed önde) |
| Tıkanıklık | inband | +10.3 (24/30 seed önde) | +9.5 (26/30 seed önde) |

## Model v3: kazanç nereden geliyor? (ablasyon)

Her sütun bir adımın katkısı: bir önceki modele göre eşli fark (puan). Son iki sütun klasik rakip EWMA'ya göre fark. p: eşli t-testi. Ayrıntı: ml/train_v3.py.

| Senaryo | Düzlem | gerçekçi veri | + yaş | + EWMA | doğrusal olmayan | v3 lineer − EWMA | v3 GB − EWMA |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Bozulmasız | ideal | +0.1 (p 0.127) | -0.0 (p 0.143) | +0.0 (p 0.613) | -0.0 (p 0.524) | -0.0 (p 0.717) | -0.0 (p 0.373) |
| Bozulmasız | inband | +0.1 (p 0.302) | -0.1 (p 0.269) | -0.0 (p 0.463) | +0.1 (p 0.312) | +0.4 (p 0.186) | +0.1 (p 0.302) |
| Fading | ideal | +0.4 (p 0.699) | -0.6 (p 0.197) | +1.0 (p 0.052) | +0.3 (p 0.484) | +0.4 (p 0.538) | +0.7 (p 0.172) |
| Fading | inband | -0.6 (p 0.436) | +2.2 (p 0.042) | -0.1 (p 0.923) | +1.0 (p 0.121) | +4.2 (p <0.001) | +5.5 (p <0.001) |
| Kademeli bozulma | ideal | +0.6 (p 0.640) | -0.9 (p 0.290) | -1.4 (p 0.261) | +2.2 (p 0.138) | +0.2 (p 0.813) | +2.4 (p 0.095) |
| Kademeli bozulma | inband | -0.5 (p 0.683) | +3.2 (p 0.019) | +1.3 (p 0.107) | -1.3 (p 0.387) | +5.9 (p <0.001) | +8.3 (p <0.001) |
| Girişim | ideal | -0.3 (p 0.681) | +0.2 (p 0.765) | +0.3 (p 0.650) | -0.9 (p 0.249) | +4.6 (p <0.001) | +3.7 (p <0.001) |
| Girişim | inband | -1.2 (p 0.051) | +0.7 (p 0.335) | +1.1 (p 0.086) | -2.0 (p 0.007) | +5.7 (p <0.001) | +4.3 (p <0.001) |
| Jamming | ideal | -1.7 (p 0.011) | -0.3 (p 0.519) | +1.0 (p 0.084) | +0.7 (p 0.366) | +2.1 (p 0.011) | +2.9 (p <0.001) |
| Jamming | inband | -0.5 (p 0.031) | +0.8 (p 0.006) | -0.4 (p 0.289) | -0.1 (p 0.706) | +2.3 (p <0.001) | +2.3 (p <0.001) |
| Düğüm arızası | ideal | +0.0 (p 0.883) | -0.0 (p 0.585) | +0.0 (p 0.826) | -0.5 (p 0.342) | +0.0 (p 0.945) | -0.5 (p 0.344) |
| Düğüm arızası | inband | +0.1 (p 0.623) | -0.1 (p 0.875) | -0.1 (p 0.828) | -1.5 (p 0.346) | +0.8 (p 0.022) | -1.1 (p 0.490) |
| Tıkanıklık | ideal | +0.0 (p 0.938) | +0.1 (p 0.517) | -0.3 (p 0.434) | +0.2 (p 0.749) | +2.6 (p <0.001) | +2.8 (p <0.001) |
| Tıkanıklık | inband | +0.7 (p 0.330) | +0.0 (p 0.939) | -0.4 (p 0.613) | -1.3 (p 0.590) | +6.5 (p <0.001) | +6.8 (p 0.012) |

## Gerçekçi kontrol düzlemi (model v2, in-band)

| Senaryo | Kontrol paketi/s | Rapor teslimi | Güncelleme teslimi | Kontrol gecikmesi |
| --- | --- | --- | --- | --- |
| Bozulmasız | 65 | %97 | %99 | 226 ms |
| Fading | 61 | %86 | %96 | 250 ms |
| Kademeli bozulma | 61 | %79 | %92 | 260 ms |
| Girişim | 64 | %71 | %84 | 276 ms |
| Jamming | 62 | %80 | %85 | 249 ms |
| Düğüm arızası | 63 | %89 | %96 | 227 ms |
| Tıkanıklık | 63 | %81 | %90 | 273 ms |

Grafikler: `results/figures/eval_pdr.png`, `results/figures/eval_model_kazanci.png`
