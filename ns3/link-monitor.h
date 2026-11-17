// Link başına kanal ölçümü (link monitor).
//
// Her düğüm düzenli aralıklarla küçük bir "yoklama" (probe) paketini Wi-Fi üzerinden
// yayın (broadcast) olarak gönderir. Komşular bu yoklamaları sayar. Her ölçüm
// penceresinin (ör. 1 s) sonunda her yönlü link (gönderen -> alan) için:
//   - gönderilen yoklama sayısı, alınan yoklama sayısı -> teslim oranı (link PDR)
//   - o linkten duyulan tüm veri çerçevelerinin ortalama SNR'ı ve RSSI'ı
// kaydedilir. Simülasyon sonunda en az bir kez paket duyulmuş linkler CSV'ye yazılır.
//
// Yoklamalar IP katmanını kullanmaz (doğrudan Wi-Fi cihazına, kendi EtherType'ı ile
// gönderilir). Bu yüzden yönlendirme protokolünden bağımsızdır: AODV, OLSR ve AI
// kontrolcüsü altında aynı şekilde çalışır. Bu, gerçek ağlardaki ETX ölçümüne benzer.

#ifndef LINK_MONITOR_H
#define LINK_MONITOR_H

#include "ns3/mac48-address.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include "ns3/wifi-phy.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ns3
{

class LinkMonitor
{
  public:
    // Bir pencere sonunda bir yönlü link için kaydedilen ölçüm
    struct Record
    {
        double time;      // pencerenin bitiş zamanı (s)
        uint32_t tx;      // gönderen düğüm
        uint32_t rx;      // alan düğüm
        uint32_t sent;    // pencerede tx'in gönderdiği yoklama sayısı
        uint32_t recv;    // bunlardan rx'in aldığı
        double snrSum;    // rx'in tx'ten duyduğu çerçevelerin SNR toplamı (dB)
        double rssiSum;   // aynı çerçevelerin alınan güç toplamı (dBm)
        uint32_t nFrames; // SNR örneği sayısı
    };

    LinkMonitor(NodeContainer nodes,
                NetDeviceContainer devices,
                Time probeInterval,
                Time window,
                uint32_t probeSize);

    // Yoklamaları ve ölçümü [start, stop) aralığında çalıştırır
    void Start(Time start, Time stop);

    // Her ölçüm penceresi bittiğinde o pencerenin kayıtlarıyla çağrılacak fonksiyon.
    // AI kontrolcü ölçümleri canlı olarak buradan alır.
    using WindowCallback = std::function<void(const std::vector<Record>&)>;
    void SetWindowCallback(WindowCallback cb);

    // Tüm kayıtları CSV'ye ekler (dosya yoksa başlık satırı yazar)
    void WriteCsv(const std::string& path,
                  const std::string& routing,
                  const std::string& scenario,
                  uint32_t seed) const;

    // Yoklama paketlerinin Wi-Fi EtherType'ı (yerel deneysel kullanım için ayrılmış değer)
    static const uint16_t kProbeProtocol = 0x88B5;

  private:
    void SendProbe(uint32_t node);
    void EndWindow();
    // Yoklama alındığında çağrılır (rx düğümü bağlanırken sabitlenir)
    void OnProbe(uint32_t rx,
                 Ptr<NetDevice> dev,
                 Ptr<const Packet> pkt,
                 uint16_t protocol,
                 const Address& from,
                 const Address& to,
                 NetDevice::PacketType type);
    // Wi-Fi PHY başarıyla bir çerçeve aldığında çağrılır (SNR burada okunur)
    void OnSniffRx(uint32_t rx,
                   Ptr<const Packet> pkt,
                   uint16_t channelFreqMhz,
                   WifiTxVector txVector,
                   MpduInfo aMpdu,
                   SignalNoiseDbm signalNoise,
                   uint16_t staId);

    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
    Time m_probeInterval;
    Time m_window;
    uint32_t m_probeSize;
    Time m_stop;
    uint32_t m_n;

    std::map<Mac48Address, uint32_t> m_macToNode; // MAC adresinden düğüm numarası
    Ptr<UniformRandomVariable> m_jitter;          // yoklamalar çarpışmasın diye küçük kayma

    // Mevcut pencerenin sayaçları: [tx] veya [tx][rx] (düz dizi: tx * n + rx)
    std::vector<uint32_t> m_sent;
    std::vector<uint32_t> m_recv;
    std::vector<double> m_snrSum;
    std::vector<double> m_rssiSum;
    std::vector<uint32_t> m_nFrames;

    std::vector<Record> m_records;
    WindowCallback m_windowCb;
};

} // namespace ns3

#endif
