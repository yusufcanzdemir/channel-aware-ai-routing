// Merkezi AI yönlendirme kontrolcüsü (SDN benzeri).
//
// Her ölçüm penceresi (1 s) sonunda:
//   1) LinkMonitor'dan her yönlü linkin teslim oranı ve SNR ölçümünü alır, geçmişe ekler
//   2) Her link için önümüzdeki birkaç saniyenin teslim oranını tahmin eder
//        "linear": Python'da eğitilmiş lineer model (ml/models/linear_h3.txt)
//        "linear2": aynı, ek öznitelikler: sinyal payı (RSSI - algılama eşiği) ve ters yön
//                   linkinin ölçümleri (ml/models/linear2_h3.txt)
//        "last"  : tahmin yok, son ölçüm kullanılır (karşılaştırma için)
//        "ma"    : son 5 ölçümün ortalaması (karşılaştırma için)
//        "ewma"  : üstel ağırlıklı hareketli ortalama (klasik, öğrenmeyen rakip)
//        "holt"  : seviye + eğilim izleyen Holt yöntemi (klasik, öğrenmeyen rakip)
//                  Parametreleri modelle aynı veride ayarlandı (ml/tune_classic.py,
//                  ml/models/classic.txt)
//        "oracle": kâhin; rotaların uygulanacağı dönemin GERÇEK kanal durumunu kanal
//                  modelinden okur. Gerçekte imkânsızdır; "tahmin mükemmel olsaydı ne
//                  kazanırdık?" sorusunun cevabı, yani teorik üst sınır. Çarpışmaları bilmez.
//   3) Link maliyeti = 1 / (p_gidiş * p_dönüş)  (tahmine dayalı ETX: unicast veri bir yöne,
//      ACK diğer yöne gider; ikisi de başarılı olmalı). p < minPdr olan link kullanılmaz.
//   4) Her düğümden her düğüme Dijkstra ile en ucuz yolu bulur
//   5) Her düğümün Ipv4StaticRouting tablosuna "hedef -> sonraki sekme" host rotalarını yazar
//
// Kontrol döngüsü ayarları:
//   delay   : hesaplanan rotaların düğümlere uygulanma gecikmesi (s). Ölçümün kontrolcüye
//             gitmesi + hesap + rotanın geri gelmesi toplamını temsil eder. 0 = ideal kanal.
//   interval: rotaların kaç pencerede (saniyede) bir yeniden hesaplanacağı. Ölçümler her
//             pencerede geçmişe eklenmeye devam eder.
// Kontrol mesajlarının kendisi simüle edilmez (kayıpsız, ağa yük getirmez).

#ifndef AI_CONTROLLER_H
#define AI_CONTROLLER_H

#include "link-monitor.h"
#include "slow-fading-loss-model.h"

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
                 double minPdr,
                 double delay = 0.0,
                 uint32_t interval = 1);

    // İdeal kontrol düzlemi: LinkMonitor her pencere sonunda bunu çağırır
    // (ölçümler anında gelir, rotalar doğrudan ya da --aiDelay kadar sonra yazılır)
    void OnWindow(const std::vector<LinkMonitor::Record>& window);

    // Gerçekçi kontrol düzlemi (InbandControl) için: ölçümleri geçmişe ekler ve yeni
    // rota tablosunu [src * n + dst] -> sonraki sekme olarak döndürür, YAZMAZ
    std::vector<int> ProcessWindow(const std::vector<LinkMonitor::Record>& window);
    // Tek bir düğümün rota satırını (hedef -> sonraki sekme, -1: yok) tablosuna yazar.
    // Gerçekçi modda rota güncellemesi paketi düğüme ulaştığında çağrılır.
    void InstallNodeRoutes(uint32_t src, const std::vector<int>& row);
    uint32_t GetN() const;

    // Kâhin için: kanal modeli (yoksa sönümleme yok) ve sönümlemesiz alınan güç
    // nominalRxDbm[tx * n + rx] (mesafe kaybından)
    void SetOracle(Ptr<SlowFadingLossModel> fading, std::vector<double> nominalRxDbm);

    // Doğrulama için: her pencerede her linkin öznitelikleri ve tahmini bu dosyaya yazılır
    void SetDebugCsv(const std::string& path);

    // Simülasyon boyunca kaç kez bir (kaynak, hedef) için sonraki sekme değişti
    uint64_t GetRouteChanges() const;

  private:
    static const uint32_t kHistory = 10; // modelin baktığı geçmiş pencere sayısı
    static constexpr double kDetectDbm = -82.0;    // NS-3 algılama eşiği (dBm)
    static constexpr double kMarginUnknown = -3.0; // çerçeve duyulmadıysa varsayılan pay (dB)

    void LoadModel(const std::string& path);
    void PushHistory(const std::vector<LinkMonitor::Record>& window);
    // tx -> rx linkinin tahmini teslim oranı (0..1)
    double Predict(uint32_t tx, uint32_t rx);
    // Lineer modelin girdileri (Python'daki build_features ile birebir aynı):
    // linkin kendi öznitelikleri + ters yön linkinden "rev_" önekli olanlar
    std::map<std::string, double> Features(uint32_t tx, uint32_t rx) const;
    // Tek bir yönlü linkin kendi öznitelikleri
    std::map<std::string, double> OwnFeatures(uint32_t idx) const;
    // Tahmin + maliyet + Dijkstra: [src * n + dst] -> sonraki sekme (-1: rota yok)
    std::vector<int> ComputeRoutes();
    // Rota tablosunu düğümlere yazar (gecikmeli olarak da çağrılabilir)
    void InstallRoutes(std::vector<int> newNextHop);

    uint32_t m_n;
    std::string m_predictor;
    double m_minPdr;
    double m_delay;
    uint32_t m_interval;
    uint32_t m_windowCount = 0;
    std::vector<Ipv4Address> m_addr;                // düğüm -> IP adresi
    std::vector<Ptr<Ipv4StaticRouting>> m_routing; // düğüm -> statik rota tablosu
    std::vector<uint32_t> m_ifIndex;               // düğüm -> Wi-Fi arayüz numarası

    // Link geçmişi [tx * n + rx]; ön taraf en yeni ölçüm
    std::vector<std::deque<double>> m_pdrHist;
    std::vector<std::deque<double>> m_snrHist;  // SNR (dB), bilinmiyorsa NaN
    std::vector<std::deque<double>> m_rssiHist; // alınan güç (dBm), bilinmiyorsa NaN
    // Klasik tahmincilerin link başına durumu
    std::vector<double> m_ewma;
    std::vector<double> m_level;
    std::vector<double> m_trend;
    std::vector<bool> m_started;

    double m_intercept = 0.0;
    std::map<std::string, double> m_coef;

    std::vector<int> m_nextHop; // [src * n + dst], -1: rota yok
    uint64_t m_routeChanges = 0;
    double m_now = 0.0;

    std::ofstream m_debug;

    // Kâhin: rotaların geçerli olacağı dönemde gerçek beklenen teslim oranı
    double OraclePdr(uint32_t tx, uint32_t rx) const;
    Ptr<SlowFadingLossModel> m_fading;
    std::vector<double> m_nominalRx;
};

} // namespace ns3

#endif
