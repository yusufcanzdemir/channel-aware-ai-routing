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
                           double minPdr,
                           double delay,
                           uint32_t interval)
    : m_n(nodes.GetN()),
      m_predictor(predictor),
      m_minPdr(minPdr),
      m_delay(delay),
      m_interval(interval)
{
    NS_ABORT_MSG_IF(interval == 0, "aiInterval en az 1 olmali");
    NS_ABORT_MSG_IF(predictor != "linear" && predictor != "linear2" && predictor != "last" &&
                        predictor != "ma" && predictor != "oracle" && predictor != "ewma" &&
                        predictor != "holt",
                    "Bilinmeyen aiPredictor: " << predictor);
    // Lineer modellerin katsayıları ya da klasik tahmincilerin parametreleri dosyadan
    if (predictor == "linear" || predictor == "linear2" || predictor == "ewma" ||
        predictor == "holt")
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
    m_rssiHist.resize(m_n * m_n);
    m_ewma.assign(m_n * m_n, 0.0);
    m_level.assign(m_n * m_n, 0.0);
    m_trend.assign(m_n * m_n, 0.0);
    m_started.assign(m_n * m_n, false);
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

void
AiController::SetOracle(Ptr<SlowFadingLossModel> fading, std::vector<double> nominalRxDbm)
{
    m_fading = fading;
    m_nominalRx = nominalRxDbm;
}

double
AiController::OraclePdr(uint32_t tx, uint32_t rx) const
{
    // Rotalar [şimdi + gecikme, şimdi + gecikme + aralık) döneminde geçerli olacak
    Time start = Simulator::Now() + Seconds(m_delay);
    Time end = start + Seconds(double(m_interval));
    double nominal = m_nominalRx[tx * m_n + rx];
    if (!m_fading)
    {
        // Sönümleme yok: link ya hep var ya hep yok
        return nominal >= kDetectDbm ? 1.0 : 0.0;
    }
    double sigma = m_fading->GetFastSigma();
    Time step = m_fading->GetStep();
    double sum = 0.0;
    uint32_t count = 0;
    for (Time t = start; t < end; t += step)
    {
        // Paket alınır <=> nominal - yavaş kayıp - hızlı kayıp >= eşik.
        // Hızlı kayıp ~ N(0, sigma) olduğundan olasılık = Φ(pay / sigma)
        double margin = nominal - m_fading->GetSlowLossDbAt(tx, rx, t) - kDetectDbm;
        double p = sigma > 0 ? 0.5 * std::erfc(-margin / (sigma * std::sqrt(2.0)))
                             : (margin >= 0 ? 1.0 : 0.0);
        sum += p;
        count++;
    }
    return count > 0 ? sum / count : 0.0;
}

uint64_t
AiController::GetRouteChanges() const
{
    return m_routeChanges;
}

void
AiController::OnWindow(const std::vector<LinkMonitor::Record>& window)
{
    // 1) Ölçümleri geçmişe ekle
    PushHistory(window);
    // Rotalar sadece her m_interval pencerede bir yeniden hesaplanır
    m_windowCount++;
    if (m_windowCount % m_interval != 0)
    {
        return;
    }
    // 2-4) Tahmin, maliyet, Dijkstra
    std::vector<int> routes = ComputeRoutes();
    // 5) Rotaları yaz: hemen ya da kontrol gecikmesi kadar sonra
    if (m_delay > 0.0)
    {
        Simulator::Schedule(Seconds(m_delay), &AiController::InstallRoutes, this, routes);
    }
    else
    {
        InstallRoutes(routes);
    }
}

std::vector<int>
AiController::ProcessWindow(const std::vector<LinkMonitor::Record>& window)
{
    PushHistory(window);
    return ComputeRoutes();
}

void
AiController::PushHistory(const std::vector<LinkMonitor::Record>& window)
{
    m_now = Simulator::Now().GetSeconds();
    for (const auto& r : window)
    {
        uint32_t idx = r.tx * m_n + r.rx;
        double pdr = r.sent > 0 ? double(r.recv) / r.sent : 0.0;
        double snr = r.nFrames > 0 ? r.snrSum / r.nFrames : std::nan("");
        double rssi = r.nFrames > 0 ? r.rssiSum / r.nFrames : std::nan("");
        // Klasik tahmincilerin durumu (ilk ölçümde başlatılır)
        if (!m_started[idx])
        {
            m_ewma[idx] = pdr;
            m_level[idx] = pdr;
            m_trend[idx] = 0.0;
            m_started[idx] = true;
        }
        else if (m_predictor == "ewma")
        {
            double a = m_coef.at("ewma_alpha");
            m_ewma[idx] = a * pdr + (1 - a) * m_ewma[idx];
        }
        else if (m_predictor == "holt")
        {
            double a = m_coef.at("holt_alpha");
            double b = m_coef.at("holt_beta");
            double prev = m_level[idx];
            m_level[idx] = a * pdr + (1 - a) * (m_level[idx] + m_trend[idx]);
            m_trend[idx] = b * (m_level[idx] - prev) + (1 - b) * m_trend[idx];
        }
        m_pdrHist[idx].push_front(pdr);
        m_snrHist[idx].push_front(snr);
        m_rssiHist[idx].push_front(rssi);
        if (m_pdrHist[idx].size() > kHistory)
        {
            m_pdrHist[idx].pop_back();
            m_snrHist[idx].pop_back();
            m_rssiHist[idx].pop_back();
        }
    }
}

std::map<std::string, double>
AiController::Features(uint32_t tx, uint32_t rx) const
{
    std::map<std::string, double> f = OwnFeatures(tx * m_n + rx);
    // Ters yön (rx -> tx): sönümleme iki yönde aynı olduğu için ek bir ölçüm kaynağı
    std::map<std::string, double> r = OwnFeatures(rx * m_n + tx);
    for (const char* name : {"pdr_l0",
                             "pdr_l1",
                             "pdr_l2",
                             "pdr_mean5",
                             "pdr_mean10",
                             "pdr_trend",
                             "mar_l0",
                             "mar_mean5",
                             "mar_slope"})
    {
        f[std::string("rev_") + name] = r[name];
    }
    return f;
}

std::map<std::string, double>
AiController::OwnFeatures(uint32_t idx) const
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

    // Sinyal payı: alınan gücün algılama eşiğinin kaç dB üstünde olduğu
    const auto& rs = m_rssiHist[idx];
    double mar[5];
    double sumMar = 0.0;
    double maxMar = -std::numeric_limits<double>::infinity();
    for (uint32_t k = 0; k < 5; ++k)
    {
        mar[k] = std::isnan(rs[k]) ? kMarginUnknown : rs[k] - kDetectDbm;
        f["mar_l" + std::to_string(k)] = mar[k];
        sumMar += mar[k];
        maxMar = std::max(maxMar, mar[k]);
    }
    f["mar_mean5"] = sumMar / 5.0;
    f["mar_max5"] = maxMar;
    // Son 5 noktanın en küçük kareler eğimi (dB/s); mar[0] en yeni
    f["mar_slope"] = (2 * mar[0] + mar[1] - mar[3] - 2 * mar[4]) / 10.0;
    return f;
}

double
AiController::Predict(uint32_t tx, uint32_t rx)
{
    if (m_predictor == "oracle")
    {
        NS_ABORT_MSG_IF(m_nominalRx.empty(), "oracle icin SetOracle cagrilmali");
        double y = OraclePdr(tx, rx);
        // Doğrulama: kâhin tahmini ile sonradan ölçülen teslim oranı karşılaştırılabilsin
        if (m_debug.is_open() && y > 0.01)
        {
            m_debug << m_now << "," << tx << "," << rx << "," << y << "\n";
        }
        return y;
    }
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
    if (m_predictor == "ewma")
    {
        return m_ewma[idx];
    }
    if (m_predictor == "holt")
    {
        // Sonraki 1, 2, 3 adımın ortalaması = 2 adım ileri
        return std::clamp(m_level[idx] + 2.0 * m_trend[idx], 0.0, 1.0);
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
    std::map<std::string, double> f = Features(tx, rx);
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

std::vector<int>
AiController::ComputeRoutes()
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

    return newNextHop;
}

void
AiController::InstallRoutes(std::vector<int> newNextHop)
{
    // Her düğümün satırını (hedef -> sonraki sekme) ayrı ayrı yaz
    for (uint32_t src = 0; src < m_n; ++src)
    {
        std::vector<int> row(newNextHop.begin() + src * m_n,
                             newNextHop.begin() + (src + 1) * m_n);
        InstallNodeRoutes(src, row);
    }
}

void
AiController::InstallNodeRoutes(uint32_t src, const std::vector<int>& row)
{
    bool changed = false;
    for (uint32_t dst = 0; dst < m_n; ++dst)
    {
        if (row[dst] != m_nextHop[src * m_n + dst])
        {
            changed = true;
            m_routeChanges++;
            m_nextHop[src * m_n + dst] = row[dst];
        }
    }
    if (!changed)
    {
        return;
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
        int nh = row[dst];
        if (dst != src && nh >= 0)
        {
            rt->AddHostRouteTo(m_addr[dst], m_addr[nh], m_ifIndex[src]);
        }
    }
}

uint32_t
AiController::GetN() const
{
    return m_n;
}

} // namespace ns3
