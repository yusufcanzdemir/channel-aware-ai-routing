# Ölçüm yükü deneyi (otomatik: analysis/olcum_yuku.py)

Yük = yoklama + kontrol iletimlerinin ölçülen kanal süresi (trafik süresinin %'si). Gerçekçi (in-band) kontrol düzlemi, 30 seed. Grafik: results/figures/olcum_yuku.png

## Bozulmalı 5 senaryonun ortalaması

| Ayar | Yoklama yükü | Kontrol yükü | Son ölçüm PDR | EWMA PDR | v3 PDR |
| --- | --- | --- | --- | --- | --- |
| 10/s, her saniye rapor | %11.6 | %10.8 | 78.5 | 80.8 | 85.8 |
| 10/s, değişince rapor | %11.6 | %7.6 | 79.0 | 80.8 | 85.3 |
| 5/s, her saniye rapor | %5.8 | %10.9 | 74.5 | 78.8 | 86.1 |
| 5/s, değişince rapor | %5.8 | %8.4 | 75.6 | 79.7 | 87.4 |
| 2/s, her saniye rapor | %2.3 | %11.7 | 68.3 | 74.1 | 86.0 |
| 2/s, değişince rapor | %2.3 | %10.4 | 68.7 | 74.0 | 86.5 |

Referans (yoklamasız, yönlendirme mesajlarının kanal süresi):

- AODV: yük %3.9, PDR 69.8
- OLSR: yük %1.7, PDR 68.0

## v3: ana ayara (10/s, her saniye rapor) göre teslim oranı farkı (puan)

Eşli t-testi (aynı seed'ler). Negatif = yük azaltınca kayıp.

| Senaryo | 10/s, değişince rapor | 5/s, her saniye rapor | 5/s, değişince rapor | 2/s, her saniye rapor | 2/s, değişince rapor |
| --- | --- | --- | --- | --- | --- |
| Bozulmasız | -0.5 (p 0.493) | +0.4 (p 0.326) | +0.4 (p 0.339) | +0.2 (p 0.518) | +0.4 (p 0.324) |
| Fading | -2.2 (p 0.126) | -3.4 (p 0.071) | +1.1 (p 0.248) | -0.8 (p 0.641) | +0.3 (p 0.781) |
| Kademeli bozulma | -2.5 (p 0.267) | -0.2 (p 0.877) | -0.2 (p 0.901) | -3.0 (p 0.154) | -4.1 (p 0.083) |
| Girişim | -0.9 (p 0.251) | +0.7 (p 0.098) | +1.6 (p 0.010) | -0.9 (p 0.514) | +0.1 (p 0.916) |
| Jamming | +0.9 (p 0.035) | +2.7 (p 0.000) | +2.5 (p 0.000) | +3.8 (p 0.000) | +3.9 (p 0.000) |
| Düğüm arızası | +0.3 (p 0.714) | -0.7 (p 0.633) | +0.2 (p 0.804) | -0.8 (p 0.581) | +0.5 (p 0.534) |
| Tıkanıklık | +1.2 (p 0.151) | +1.1 (p 0.398) | +2.3 (p 0.009) | +0.9 (p 0.528) | +2.3 (p 0.010) |
