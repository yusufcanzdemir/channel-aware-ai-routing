// Gerçekçi (in-band) kontrol düzlemi.
//
// İdeal modda AI kontrolcü ölçümleri "sihirli" şekilde anında görür ve rotaları düğümlere
// doğrudan yazar. Bu sınıf ise kontrolcüyü ağdaki bir düğüme (varsayılan 5) yerleştirir ve
// bütün bilgiyi ağın içinden gerçek UDP paketleriyle taşır:
//
//   1) Her ölçüm penceresi sonunda her düğüm kendi ölçtüğü gelen linkleri (her komşudan
//      alınan yoklama sayısı, ortalama SNR ve RSSI) bir RAPOR paketine yazıp kontrolcüye
//      gönderir. Raporlar veri trafiğiyle aynı kanalı paylaşır; çarpışabilir, kaybolabilir.
//   2) Kontrolcü collectWait süresi (0.2 s) bekler, o ana kadar gelen raporlarla tahmin ve
//      Dijkstra yapar. Raporu gelmeyen düğümün linkleri için SON BİLİNEN değer kullanılır.
//   3) Tablosu değişen düğümlere (ve her refresh pencerede bir herkese) bir GÜNCELLEME paketi
//      gönderir. Düğüm, paket ULAŞTIĞI ANDA tablosunu günceller; kaybolursa eskisiyle devam eder.
//
// Kontrol paketleri de kontrolcünün kurduğu rotaları kullanır (in-band). Gönderenin hedefe
// rotası yoksa veya açılış süresindeysek (bootstrap) paket komşudan komşuya yayınla
// (flooding) iletilir: her düğüm bir paketi en fazla bir kez yeniden yayınlar.
//
// Paket biçimi (bayt): [tür][flood][kaynak][hedef][pencere(2)] + içerik
//   RAPOR içeriği     : [k] + k × [gönderen][alınan][SNR×100 (2)][RSSI×100 (2)][çerçeve]
//   GÜNCELLEME içeriği: n × [sonraki sekme] (255 = rota yok)

#ifndef INBAND_CONTROL_H
#define INBAND_CONTROL_H

#include "ai-controller.h"
#include "link-monitor.h"

#include "ns3/ipv4-address.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"

#include <map>
#include <set>
#include <vector>

namespace ns3
{

class InbandControl
{
  public:
    InbandControl(NodeContainer nodes,
                  AiController* ai,
                  uint32_t controller,
                  uint32_t probesPerWindow,
                  double collectWait,
                  double bootstrapEnd,
                  uint32_t refreshEvery);

    // LinkMonitor her pencere sonunda bunu çağırır (düğümler kendi ölçümlerini bilir)
    void OnWindow(const std::vector<LinkMonitor::Record>& window);

    // Simülasyon sonu istatistikleri
    struct Stats
    {
        uint64_t reportsSent = 0;     // kontrolcü dışındaki düğümlerin ürettiği rapor
        uint64_t reportsFresh = 0;    // hesap anında kontrolcüde olan güncel rapor
        uint64_t updatesSent = 0;     // kontrolcünün diğer düğümlere ürettiği güncelleme
        uint64_t updatesApplied = 0;  // düğümlere ulaşıp uygulanan güncelleme
        uint64_t floodTx = 0;         // flooding ile yapılan (yeniden) yayın sayısı
        uint64_t unicastSent = 0;     // unicast olarak gönderilen kontrol paketi
        uint64_t ctrlBytes = 0;       // uygulama katmanında üretilen kontrol baytı (yayınlar dahil)
        double delaySum = 0.0;        // ölçüm penceresi sonu -> rotanın düğümde uygulanması (s)
        uint64_t delayCount = 0;
    };

    const Stats& GetStats() const;

    // Kontrol paketlerinin UDP portu (FlowMonitor'da veri trafiğinden ayırmak için)
    static const uint16_t kPort = 7000;

  private:
    enum MsgType : uint8_t
    {
        REPORT = 1,
        UPDATE = 2
    };

    struct Header
    {
        uint8_t type;
        uint8_t flood;
        uint8_t origin;
        uint8_t target;
        uint16_t window;
    };

    struct ReportEntry
    {
        uint8_t tx;
        uint8_t recv;
        double snr;
        double rssi;
        uint8_t nFrames;
    };

    struct LatestReport
    {
        uint32_t window = 0;
        std::vector<ReportEntry> entries;
        bool valid = false;
    };

    void SendReport(uint32_t node, uint32_t window, std::vector<ReportEntry> entries);
    void Compute(uint32_t window);
    // Bir kontrol mesajını üretir ve gönderir (unicast ya da flooding)
    void Send(uint32_t from, uint32_t target, uint8_t type, uint32_t window,
              const std::vector<uint8_t>& payload);
    void Broadcast(uint32_t node, const std::vector<uint8_t>& bytes);
    void HandleRead(Ptr<Socket> socket);
    void Deliver(uint32_t node, const Header& h, const uint8_t* payload, uint32_t len);
    bool HasHostRoute(uint32_t node, uint32_t target) const;

    NodeContainer m_nodes;
    AiController* m_ai;
    uint32_t m_n;
    uint32_t m_controller;
    uint32_t m_probesPerWindow;
    double m_collectWait;
    double m_bootstrapEnd;
    uint32_t m_refreshEvery;

    std::vector<Ptr<Socket>> m_sockets;
    std::vector<Ipv4Address> m_addr;
    std::vector<std::set<uint64_t>> m_seen; // düğüm başına görülmüş flood mesajları
    Ptr<UniformRandomVariable> m_jitter;

    uint32_t m_window = 0;
    std::map<uint32_t, double> m_windowEnd;          // pencere -> bitiş zamanı (s)
    std::vector<LatestReport> m_latest;              // kontrolcüde: düğüm -> son rapor
    std::vector<std::vector<int>> m_lastSent;        // kontrolcüde: düğüm -> son gönderilen satır
    std::vector<uint32_t> m_lastApplied;             // düğümde: uygulanan son pencere

    Stats m_stats;
};

} // namespace ns3

#endif
