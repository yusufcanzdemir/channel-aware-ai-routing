#include "ai-controller.h"

#include "ns3/abort.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>

namespace ns3
{

AiController::AiController(NodeContainer nodes,
                           NetDeviceContainer devices,
                           const std::string& predictor,
                           const std::string& modelPath,
                           double minPdr)
    : m_n(nodes.GetN()),
      m_predictor(predictor),
      m_minPdr(minPdr)
{
    NS_ABORT_MSG_IF(predictor != "linear" && predictor != "last" && predictor != "ma",
                    "Bilinmeyen aiPredictor: " << predictor);
    if (predictor == "linear")
    {
        LoadModel(modelPath);
    }

    Ipv4StaticRoutingHelper helper;
    for (uint32_t i = 0; i < m_n; ++i)
    {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        uint32_t ifIndex = ipv4->GetInterfaceForDevice(devices.Get(i));
        m_ifIndex.push_back(ifIndex);
        m_addr.push_back(ipv4->GetAddress(ifIndex, 0).GetLocal());
        // Liste yönlendirme içindeki statik yönlendirme nesnesini bulur
        m_routing.push_back(helper.GetStaticRouting(ipv4));
    }
    m_pdrHist.resize(m_n * m_n);
    m_snrHist.resize(m_n * m_n);
    m_nextHop.assign(m_n * m_n, -1);
}

void
AiController::LoadModel(const std::string& path)
{
    // Dosya biçimi: her satırda "ad katsayı"; ilk satır "intercept değer"
    std::ifstream in(path);
    NS_ABORT_MSG_IF(!in, "Model dosyasi acilamadi: " << path);
    std::string name;
    double value;
    while (in >> name >> value)
    {
        if (name == "intercept")
        {
            m_intercept = value;
        }
        else
        {
            m_coef[name] = value;
        }
    }
    NS_ABORT_MSG_IF(m_coef.empty(), "Model dosyasi bos: " << path);
    std::cout << "AI model yuklendi: " << path << " (" << m_coef.size() << " oznitelik)"
              << std::endl;
}

void
AiController::SetDebugCsv(const std::string& path)
{
    m_debug.open(path);
    m_debug << "time,tx,rx,pred";
    for (const auto& [name, c] : m_coef)
    {
        m_debug << "," << name;
    }
    m_debug << "\n";
}

uint64_t
AiController::GetRouteChanges() const
{
    return m_routeChanges;
}

void
AiController::OnWindow(const std::vector<LinkMonitor::Record>& window)
{
    m_now = Simulator::Now().GetSeconds();
    // 1) Ölçümleri geçmişe ekle
    for (const auto& r : window)
    {
        uint32_t idx = r.tx * m_n + r.rx;
        double pdr = r.sent > 0 ? double(r.recv) / r.sent : 0.0;
        double snr = r.nFrames > 0 ? r.snrSum / r.nFrames : std::nan("");
        m_pdrHist[idx].push_front(pdr);
        m_snrHist[idx].push_front(snr);
        if (m_pdrHist[idx].size() > kHistory)
        {
            m_pdrHist[idx].pop_back();
            m_snrHist[idx].pop_back();
        }
    }
    // 2-5) Tahmin, maliyet, Dijkstra, rota yazma
    ComputeAndInstallRoutes();
}

std::map<std::string, double>
AiController::Features(uint32_t idx) const
{
    const auto& p = m_pdrHist[idx];
    const auto& s = m_snrHist[idx];
    std::map<std::string, double> f;
    double sum5 = 0.0;
    double sum10 = 0.0;
    for (uint32_t k = 0; k < kHistory; ++k)
    {
        f["pdr_l" + std::to_string(k)] = p[k];
        sum10 += p[k];
        if (k < 5)
        {
            sum5 += p[k];
        }
    }
    for (uint32_t k = 0; k < 3; ++k)
    {
        bool known = !std::isnan(s[k]);
        f["snr_l" + std::to_string(k)] = known ? s[k] : 0.0; // bilinmiyorsa 0 + bayrak
        f["snrk_l" + std::to_string(k)] = known ? 1.0 : 0.0;
    }
    f["pdr_mean5"] = sum5 / 5.0;
    f["pdr_mean10"] = sum10 / kHistory;
    f["pdr_trend"] = p[0] - p[2];
    bool both = !std::isnan(s[0]) && !std::isnan(s[2]);
    f["snr_trend"] = both ? s[0] - s[2] : 0.0;
    return f;
}

double
AiController::Predict(uint32_t tx, uint32_t rx)
{
    uint32_t idx = tx * m_n + rx;
    const auto& p = m_pdrHist[idx];
    if (p.empty())
    {
        return 0.0; // bu link hiç ölçülmedi
    }
    if (m_predictor == "last" || p.size() < kHistory)
    {
        // Yeterli geçmiş yokken (ilk 10 s) her yöntem son ölçümü kullanır
        return p[0];
    }
    if (m_predictor == "ma")
    {
        double sum = 0.0;
        for (uint32_t k = 0; k < 5; ++k)
        {
            sum += p[k];
        }
        return sum / 5.0;
    }

    // Lineer model: kesişim + Σ katsayı * öznitelik, sonra [0, 1] aralığına kırp
    std::map<std::string, double> f = Features(idx);
    double y = m_intercept;
    for (const auto& [name, c] : m_coef)
    {
        auto it = f.find(name);
        NS_ABORT_MSG_IF(it == f.end(), "Modelde bilinmeyen oznitelik: " << name);
        y += c * it->second;
    }
    y = std::clamp(y, 0.0, 1.0);

    // Hata ayıklama: sadece bir zamanlar duyulmuş linkler (dosya şişmesin)
    if (m_debug.is_open() && f["pdr_mean10"] > 0.0)
    {
        m_debug << m_now << "," << tx << "," << rx << "," << y;
        for (const auto& [name, c] : m_coef)
        {
            m_debug << "," << f[name];
        }
        m_debug << "\n";
    }
    return y;
}

void
AiController::ComputeAndInstallRoutes()
{
    const double inf = std::numeric_limits<double>::infinity();

    // 2-3) Tahmin ve link maliyetleri: cost[u * n + v]
    std::vector<double> pred(m_n * m_n, 0.0);
    for (uint32_t u = 0; u < m_n; ++u)
    {
        for (uint32_t v = 0; v < m_n; ++v)
        {
            if (u != v)
            {
                pred[u * m_n + v] = Predict(u, v);
            }
        }
    }
    std::vector<double> cost(m_n * m_n, inf);
    for (uint32_t u = 0; u < m_n; ++u)
    {
        for (uint32_t v = 0; v < m_n; ++v)
        {
            double fwd = pred[u * m_n + v]; // veri paketi u -> v
            double rev = pred[v * m_n + u]; // ACK v -> u
            if (u != v && fwd >= m_minPdr && rev >= m_minPdr)
            {
                cost[u * m_n + v] = 1.0 / (fwd * rev); // beklenen iletim sayısı
            }
        }
    }

    // 4) Her kaynak için Dijkstra; ilk sekmeyi (first hop) da takip et
    std::vector<int> newNextHop(m_n * m_n, -1);
    for (uint32_t src = 0; src < m_n; ++src)
    {
        std::vector<double> dist(m_n, inf);
        std::vector<int> first(m_n, -1);
        std::vector<bool> done(m_n, false);
        dist[src] = 0.0;
        for (uint32_t iter = 0; iter < m_n; ++iter)
        {
            // Henüz kesinleşmemiş en yakın düğüm (16 düğümde basit dizi yeterli)
            int u = -1;
            for (uint32_t i = 0; i < m_n; ++i)
            {
                if (!done[i] && (u < 0 || dist[i] < dist[u]))
                {
                    u = i;
                }
            }
            if (u < 0 || dist[u] == inf)
            {
                break; // kalan düğümlere ulaşılamıyor
            }
            done[u] = true;
            for (uint32_t v = 0; v < m_n; ++v)
            {
                double c = cost[u * m_n + v];
                if (!done[v] && c < inf && dist[u] + c < dist[v])
                {
                    dist[v] = dist[u] + c;
                    // Kaynağın komşusuysa ilk sekme kendisi, değilse u'nun ilk sekmesi
                    first[v] = (uint32_t(u) == src) ? int(v) : first[u];
                }
            }
        }
        for (uint32_t dst = 0; dst < m_n; ++dst)
        {
            newNextHop[src * m_n + dst] = first[dst];
        }
    }

    // 5) Değişen düğümlerin rota tablolarını yeniden yaz
    for (uint32_t src = 0; src < m_n; ++src)
    {
        bool changed = false;
        for (uint32_t dst = 0; dst < m_n; ++dst)
        {
            if (newNextHop[src * m_n + dst] != m_nextHop[src * m_n + dst])
            {
                changed = true;
                m_routeChanges++;
            }
        }
        if (!changed)
        {
            continue;
        }
        Ptr<Ipv4StaticRouting> rt = m_routing[src];
        // Önce eski host rotalarını sil (sondan başa, indeksler kaymasın).
        // Alt ağ rotası (10.1.1.0/24) kalır: rotası olmayan hedefe doğrudan gönderilir.
        for (int i = int(rt->GetNRoutes()) - 1; i >= 0; --i)
        {
            if (rt->GetRoute(i).IsHost())
            {
                rt->RemoveRoute(i);
            }
        }
        for (uint32_t dst = 0; dst < m_n; ++dst)
        {
            int nh = newNextHop[src * m_n + dst];
            if (dst != src && nh >= 0)
            {
                rt->AddHostRouteTo(m_addr[dst], m_addr[nh], m_ifIndex[src]);
            }
        }
    }
    m_nextHop = newNextHop;
}

} // namespace ns3
