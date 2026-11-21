// Kanal süresi ölçümü: her düğümün radyosunun yaptığı her iletimin havada kaldığı süre,
// iletimin türüne göre ayrı ayrı toplanır.
//
// Neden: Ölçüm (yoklama) ve kontrol (rapor + rota güncellemesi) paketlerinin kanalda ne kadar
// yer kapladığını tahmin etmek yerine doğrudan ölçmek. 1 Mbps'lik 802.11b'de küçük bir paketin
// bile önsözü (192 µs) vardır; yani maliyet paket SAYISIYLA da artar, sadece bayt ile değil.
//
// Türler:
//   probe   : yoklama yayınları (LinkMonitor, kendi EtherType'ı)
//   control : AI kontrol düzlemi (rapor + güncelleme, UDP 7000; flooding yeniden yayınları dahil)
//   routing : AODV (UDP 654) ve OLSR (UDP 698) mesajları
//   data    : uygulama trafiği (UDP veri portu)
//   ack     : MAC onay çerçeveleri (unicast çerçevelerin ardından; hangi türe ait olduğu ayrılmaz)
//   arp     : ARP (komşunun IP adresinden MAC adresini öğrenme; yayın istek + unicast cevap)
//   other   : geri kalan
// Süre, NS-3'ün iletim süresi hesabıyla (WifiPhy::CalculateTxDuration: önsöz + başlık + veri)
// bulunur. Sadece İZLER: simülasyonun davranışını değiştirmez.
//
// Not: Aynı anda farklı bölgelerde iletim yapılabildiği için (uzamsal yeniden kullanım) toplam
// süre 1 s'yi geçebilir; burada raporlanan, düğümlerin toplam gönderim süresidir.

#ifndef AIRTIME_METER_H
#define AIRTIME_METER_H

#include "ns3/net-device-container.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-psdu.h"

#include <array>
#include <string>

namespace ns3
{

class AirtimeMeter
{
  public:
    enum Kind
    {
        PROBE = 0,
        CONTROL,
        ROUTING,
        DATA,
        ACK,
        ARP,
        OTHER,
        N_KINDS
    };

    // dataPort: uygulama trafiğinin UDP portu; probeProtocol: yoklamaların EtherType'ı
    AirtimeMeter(NetDeviceContainer devices, uint16_t dataPort, uint16_t probeProtocol);

    // [start, stop) aralığında yapılan iletimler sayılır (ısınma süresi dışarıda bırakılabilir)
    void SetInterval(double start, double stop);

    // Türün toplam iletim süresi (s) ve iletim sayısı
    double GetSeconds(Kind k) const;
    uint64_t GetCount(Kind k) const;
    static std::string Name(Kind k);

  private:
    void OnTx(WifiConstPsduMap psduMap, WifiTxVector txVector, double txPowerW);
    Kind Classify(Ptr<const WifiPsdu> psdu) const;

    uint16_t m_dataPort;
    uint16_t m_probeProtocol;
    WifiPhyBand m_band = WIFI_PHY_BAND_2_4GHZ;
    double m_start = 0.0;
    double m_stop = 1e18;
    std::array<double, N_KINDS> m_seconds{};
    std::array<uint64_t, N_KINDS> m_count{};
};

} // namespace ns3

#endif
