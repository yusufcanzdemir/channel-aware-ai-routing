// Sunum sitesi için simülasyon kaydı (--replayLog=dosya.json).
//
// Simülasyonu sadece İZLER, davranışını değiştirmez. Her saniye şunları kaydeder:
//   - her ana akışın o saniyede gönderdiği ve hedefe ulaşan paket sayısı (AODV, OLSR, AI)
//   - statik yönlendirmede (AI kontrolcü) her akışın o anki yolu: düğümlerin GERÇEK rota
//     tabloları kaynaktan hedefe adım adım izlenerek çıkarılır (in-band'de bir düğümün
//     tablosu henüz güncellenmemişse yol bunu aynen gösterir)
//   - simülasyon sonunda LinkMonitor'un link ölçümleri
// Çıktı JSON'dur; site/build_data.py bunu siteye gömer.

#ifndef REPLAY_LOGGER_H
#define REPLAY_LOGGER_H

#include "link-monitor.h"

#include "ns3/application.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{

class ReplayLogger
{
  public:
    ReplayLogger(NodeContainer nodes,
                 Ipv4InterfaceContainer interfaces,
                 std::vector<std::pair<uint32_t, uint32_t>> flows,
                 bool staticRoutes);

    // Akış uygulamalarına (gönderici) ve hedef düğümlerin IP katmanına bağlanır.
    // Ulaşan paketler IP'de hedefe teslim anında sayılır (FlowMonitor ile aynı nokta);
    // böylece hedefte alıcı uygulama olup olmamasından etkilenmez.
    void Connect(const std::vector<Ptr<Application>>& sources, uint16_t port);

    // [start, stop) aralığında her saniye bir kayıt
    void Start(double start, double stop);

    void Write(const std::string& path,
               const std::string& method,
               const std::string& scenario,
               uint32_t seed,
               const LinkMonitor* links) const;

  private:
    void OnTx(uint32_t flow, Ptr<const Packet> p);
    void OnDeliver(uint32_t dst, const Ipv4Header& h, Ptr<const Packet> p, uint32_t iface);
    void Tick(double stop);
    // Kaynaktan hedefe rota tablolarını izleyerek o anki yol (bulunamazsa son eleman -1)
    std::vector<int> Path(uint32_t flow) const;

    NodeContainer m_nodes;
    std::vector<Ipv4Address> m_addr;
    std::map<Ipv4Address, uint32_t> m_addrToNode;
    std::vector<std::pair<uint32_t, uint32_t>> m_flows;
    bool m_staticRoutes;
    uint16_t m_port = 0;

    std::vector<uint32_t> m_sentNow, m_recvNow; // içinde bulunulan saniye
    std::vector<double> m_times;
    std::vector<std::vector<uint32_t>> m_sent, m_recv;   // [zaman][akış]
    std::vector<std::vector<std::vector<int>>> m_paths;  // [zaman][akış] -> düğüm listesi
};

} // namespace ns3

#endif
