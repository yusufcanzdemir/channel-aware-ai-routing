// Merkezi AI yönlendirme kontrolcüsü (SDN benzeri).
//
// Her ölçüm penceresi (1 s) sonunda:
//   1) LinkMonitor'dan her yönlü linkin teslim oranı ve SNR ölçümünü alır, geçmişe ekler
//   2) Her link için önümüzdeki birkaç saniyenin teslim oranını tahmin eder
//        "linear": Python'da eğitilmiş lineer model (ml/models/linear_h3.txt)
//        "last"  : tahmin yok, son ölçüm kullanılır (karşılaştırma için)
//        "ma"    : son 5 ölçümün ortalaması (karşılaştırma için)
//   3) Link maliyeti = 1 / (p_gidiş * p_dönüş)  (tahmine dayalı ETX: unicast veri bir yöne,
//      ACK diğer yöne gider; ikisi de başarılı olmalı). p < minPdr olan link kullanılmaz.
//   4) Her düğümden her düğüme Dijkstra ile en ucuz yolu bulur
//   5) Her düğümün Ipv4StaticRouting tablosuna "hedef -> sonraki sekme" host rotalarını yazar
//
// Basitleştirme (docs/kararlar.md): kontrol kanalı ideal kabul edilir; ölçümler
// kontrolcüye ve rotalar düğümlere gecikmesiz ve kayıpsız ulaşır.

#ifndef AI_CONTROLLER_H
#define AI_CONTROLLER_H

#include "link-monitor.h"

#include "ns3/ipv4-address.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"

#include <deque>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace ns3
{

class AiController
{
  public:
    AiController(NodeContainer nodes,
                 NetDeviceContainer devices,
                 const std::string& predictor,
                 const std::string& modelPath,
                 double minPdr);

    // LinkMonitor her pencere sonunda bunu çağırır
    void OnWindow(const std::vector<LinkMonitor::Record>& window);

    // Doğrulama için: her pencerede her linkin öznitelikleri ve tahmini bu dosyaya yazılır
    void SetDebugCsv(const std::string& path);

    // Simülasyon boyunca kaç kez bir (kaynak, hedef) için sonraki sekme değişti
    uint64_t GetRouteChanges() const;

  private:
    static const uint32_t kHistory = 10; // modelin baktığı geçmiş pencere sayısı

    void LoadModel(const std::string& path);
    // tx -> rx linkinin tahmini teslim oranı (0..1)
    double Predict(uint32_t tx, uint32_t rx);
    // Lineer modelin girdileri (Python'daki build_features ile birebir aynı)
    std::map<std::string, double> Features(uint32_t idx) const;
    void ComputeAndInstallRoutes();

    uint32_t m_n;
    std::string m_predictor;
    double m_minPdr;
    std::vector<Ipv4Address> m_addr;                // düğüm -> IP adresi
    std::vector<Ptr<Ipv4StaticRouting>> m_routing; // düğüm -> statik rota tablosu
    std::vector<uint32_t> m_ifIndex;               // düğüm -> Wi-Fi arayüz numarası

    // Link geçmişi [tx * n + rx]; ön taraf en yeni ölçüm
    std::vector<std::deque<double>> m_pdrHist;
    std::vector<std::deque<double>> m_snrHist; // SNR (dB), bilinmiyorsa NaN

    double m_intercept = 0.0;
    std::map<std::string, double> m_coef;

    std::vector<int> m_nextHop; // [src * n + dst], -1: rota yok
    uint64_t m_routeChanges = 0;
    double m_now = 0.0;

    std::ofstream m_debug;
};

} // namespace ns3

#endif
