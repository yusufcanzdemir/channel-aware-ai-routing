#include "inband-control.h"

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"

#include <algorithm>
#include <cmath>

namespace ns3
{

static const uint32_t kHeaderSize = 6;

InbandControl::InbandControl(NodeContainer nodes,
                             AiController* ai,
                             uint32_t controller,
                             uint32_t probesPerWindow,
                             double collectWait,
                             double bootstrapEnd,
                             uint32_t refreshEvery)
    : m_nodes(nodes),
      m_ai(ai),
      m_n(nodes.GetN()),
      m_controller(controller),
      m_probesPerWindow(probesPerWindow),
      m_collectWait(collectWait),
      m_bootstrapEnd(bootstrapEnd),
      m_refreshEvery(std::max<uint32_t>(refreshEvery, 1))
{
    NS_ABORT_MSG_IF(controller >= m_n, "Kontrolcu dugumu gecersiz: " << controller);
    NS_ABORT_MSG_IF(m_n > 250, "Paket bicimi en fazla 250 dugumu destekler");

    // Gönderim zamanlarındaki küçük rastgele kaymalar (raporlar aynı anda çarpışmasın).
    // Sabit stream: seed'e bağlı, yöntemden bağımsız
    m_jitter = CreateObject<UniformRandomVariable>();
    m_jitter->SetStream(1002);

    m_seen.resize(m_n);
    m_latest.resize(m_n);
    m_lastSent.assign(m_n, std::vector<int>(m_n, -2)); // -2: hiç gönderilmedi
    m_lastApplied.assign(m_n, 0);

    for (uint32_t i = 0; i < m_n; ++i)
    {
        Ptr<Node> node = m_nodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        m_addr.push_back(ipv4->GetAddress(1, 0).GetLocal()); // arayüz 1: Wi-Fi

        Ptr<Socket> s = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
        s->Bind(InetSocketAddress(Ipv4Address::GetAny(), kPort));
        s->SetAllowBroadcast(true);
        s->SetRecvCallback(MakeCallback(&InbandControl::HandleRead, this));
        m_sockets.push_back(s);
    }
}

void
InbandControl::SetReportOnChange(bool on, double threshold)
{
    m_reportOnChange = on;
    m_changeThreshold = threshold;
    m_lastReported.assign(m_n, {});
    m_lastReportWindow.assign(m_n, 0);
}

bool
InbandControl::ShouldReport(uint32_t node, const std::vector<ReportEntry>& entries)
{
    if (!m_reportOnChange || node == m_controller)
    {
        return true; // kontrolcünün kendi ölçümü ağ kullanmaz
    }
    std::map<uint8_t, uint8_t> now;
    for (const auto& e : entries)
    {
        now[e.tx] = e.recv;
    }
    const auto& last = m_lastReported[node];
    bool send = m_lastReportWindow[node] == 0 ||                    // hiç göndermedi
                m_window - m_lastReportWindow[node] >= m_refreshEvery || // periyodik tazeleme
                now.size() != last.size();                          // komşu eklendi/kayboldu
    // Belirgin değişim: teslim oranı farkı >= eşik (yoklama sayısı cinsinden)
    double minDiff = m_changeThreshold * m_probesPerWindow - 1e-9;
    for (auto it = now.begin(); !send && it != now.end(); ++it)
    {
        auto old = last.find(it->first);
        send = old == last.end() || std::abs(int(it->second) - int(old->second)) >= minDiff;
    }
    if (send)
    {
        m_lastReported[node] = now;
        m_lastReportWindow[node] = m_window;
    }
    else
    {
        m_stats.reportsSuppressed++;
    }
    return send;
}

void
InbandControl::SetReportDelay(bool on)
{
    m_reportDelay = on;
}

const InbandControl::Stats&
InbandControl::GetStats() const
{
    return m_stats;
}

void
InbandControl::OnWindow(const std::vector<LinkMonitor::Record>& window)
{
    m_window++;
    m_windowEnd[m_window] = Simulator::Now().GetSeconds();

    // Her düğüm sadece KENDİ ölçtüğü (kendisine gelen) linkleri bilir
    std::vector<std::vector<ReportEntry>> perNode(m_n);
    for (const auto& r : window)
    {
        if (r.recv == 0 && r.nFrames == 0)
        {
            continue; // hiç duyulmayan komşu raporlanmaz (paket küçük kalsın)
        }
        ReportEntry e;
        e.tx = r.tx;
        e.recv = std::min<uint32_t>(r.recv, 255);
        e.snr = r.nFrames > 0 ? r.snrSum / r.nFrames : 0.0;
        e.rssi = r.nFrames > 0 ? r.rssiSum / r.nFrames : 0.0;
        e.nFrames = std::min<uint32_t>(r.nFrames, 255);
        e.delay = r.recv > 0 ? r.delaySum / r.recv : 0.0;
        perNode[r.rx].push_back(e);
    }
    for (uint32_t i = 0; i < m_n; ++i)
    {
        // Raporlar pencere sonundan sonraki 0-50 ms içinde rastgele bir anda gönderilir.
        // (Rastgele bekleme her durumda çekilir: "sadece değişince rapor" açık/kapalı
        // karşılaştırmasında diğer düğümlerin zamanlaması aynı kalsın.)
        double wait = m_jitter->GetValue(0.0, 0.05);
        if (!ShouldReport(i, perNode[i]))
        {
            continue;
        }
        Simulator::Schedule(Seconds(wait),
                            &InbandControl::SendReport, this, i, m_window, perNode[i]);
    }
    Simulator::Schedule(Seconds(m_collectWait), &InbandControl::Compute, this, m_window);
}

void
InbandControl::SendReport(uint32_t node, uint32_t window, std::vector<ReportEntry> entries)
{
    if (node == m_controller)
    {
        // Kontrolcünün kendi ölçümü ağ kullanmadan doğrudan elinde
        m_latest[node] = LatestReport{window, entries, true};
        return;
    }
    std::vector<uint8_t> payload;
    payload.push_back(uint8_t(entries.size()));
    for (const auto& e : entries)
    {
        int16_t snr = int16_t(std::lround(e.snr * 100));
        int16_t rssi = int16_t(std::lround(e.rssi * 100));
        payload.push_back(e.tx);
        payload.push_back(e.recv);
        payload.push_back(uint8_t(uint16_t(snr) >> 8));
        payload.push_back(uint8_t(uint16_t(snr) & 0xff));
        payload.push_back(uint8_t(uint16_t(rssi) >> 8));
        payload.push_back(uint8_t(uint16_t(rssi) & 0xff));
        payload.push_back(e.nFrames);
        if (m_reportDelay)
        {
            // 0.1 ms biriminde, en fazla 6.5 s
            uint16_t d = uint16_t(std::min(65535.0, std::round(e.delay * 10000.0)));
            payload.push_back(uint8_t(d >> 8));
            payload.push_back(uint8_t(d & 0xff));
        }
    }
    m_stats.reportsSent++;
    Send(node, m_controller, REPORT, window, payload);
}

void
InbandControl::Compute(uint32_t window)
{
    // Kontrolcünün o anki bilgisiyle her yönlü link için bir ölçüm kaydı oluştur.
    // Raporu gelmeyen düğüm için son bilinen rapor (bayat bilgi) kullanılır.
    std::vector<LinkMonitor::Record> records;
    records.reserve(m_n * (m_n - 1));
    std::vector<LinkMonitor::Record> base(m_n * m_n);
    for (uint32_t tx = 0; tx < m_n; ++tx)
    {
        for (uint32_t rx = 0; rx < m_n; ++rx)
        {
            base[tx * m_n + rx] = LinkMonitor::Record{Simulator::Now().GetSeconds(),
                                                      tx, rx, m_probesPerWindow, 0, 0.0, 0.0, 0};
        }
    }
    for (uint32_t rx = 0; rx < m_n; ++rx)
    {
        const LatestReport& rep = m_latest[rx];
        if (!rep.valid)
        {
            continue;
        }
        if (rep.window == window && rx != m_controller)
        {
            m_stats.reportsFresh++;
        }
        for (const auto& e : rep.entries)
        {
            auto& r = base[e.tx * m_n + rx];
            // Teslim oranı alıcı tarafında "beklenen yoklama sayısına" göre hesaplanır
            r.recv = std::min<uint32_t>(e.recv, m_probesPerWindow);
            r.nFrames = e.nFrames;
            r.snrSum = e.snr * e.nFrames;
            r.rssiSum = e.rssi * e.nFrames;
            // Kontrolcü ortalamayı sum / recv ile geri alır. Raporlar gecikme taşımıyorsa
            // kontrolcü gecikmeyi bilemez: NaN (öznitelikte "bilinmiyor" olarak görünür)
            r.delaySum = m_reportDelay ? e.delay * r.recv : std::nan("");
        }
    }
    for (uint32_t tx = 0; tx < m_n; ++tx)
    {
        for (uint32_t rx = 0; rx < m_n; ++rx)
        {
            if (tx != rx)
            {
                records.push_back(base[tx * m_n + rx]);
            }
        }
    }

    // Her düğümün kontrolcüdeki son raporunun yaşı (pencere): 0 = bu pencerenin raporu geldi.
    // Kontrolcü bunu gerçekten bilebilir: raporun başlığında pencere numarası var.
    std::vector<uint32_t> ages(m_n);
    for (uint32_t i = 0; i < m_n; ++i)
    {
        ages[i] = m_latest[i].valid ? window - m_latest[i].window : 1000; // 1000: hiç gelmedi
    }
    m_ai->SetReportAges(ages, m_windowEnd[window]);
    std::vector<int> routes = m_ai->ProcessWindow(records);

    for (uint32_t x = 0; x < m_n; ++x)
    {
        std::vector<int> row(routes.begin() + x * m_n, routes.begin() + (x + 1) * m_n);
        if (x == m_controller)
        {
            // Kontrolcü kendi tablosunu hemen uygular
            m_ai->InstallNodeRoutes(x, row);
            m_lastApplied[x] = window;
            m_stats.delaySum += Simulator::Now().GetSeconds() - m_windowEnd[window];
            m_stats.delayCount++;
            continue;
        }
        // Sadece değişen satırlar gönderilir; kayıpları telafi etmek için periyodik tazeleme
        bool refresh = window % m_refreshEvery == 0;
        if (row == m_lastSent[x] && !refresh)
        {
            continue;
        }
        m_lastSent[x] = row;
        std::vector<uint8_t> payload;
        for (uint32_t d = 0; d < m_n; ++d)
        {
            payload.push_back(row[d] < 0 ? 255 : uint8_t(row[d]));
        }
        m_stats.updatesSent++;
        Send(m_controller, x, UPDATE, window, payload);
    }
}

bool
InbandControl::HasHostRoute(uint32_t node, uint32_t target) const
{
    Ipv4StaticRoutingHelper helper;
    Ptr<Ipv4StaticRouting> rt = helper.GetStaticRouting(m_nodes.Get(node)->GetObject<Ipv4>());
    for (uint32_t i = 0; i < rt->GetNRoutes(); ++i)
    {
        Ipv4RoutingTableEntry e = rt->GetRoute(i);
        if (e.IsHost() && e.GetDest() == m_addr[target])
        {
            return true;
        }
    }
    return false;
}

void
InbandControl::Send(uint32_t from,
                    uint32_t target,
                    uint8_t type,
                    uint32_t window,
                    const std::vector<uint8_t>& payload)
{
    // Açılışta ya da hedefe rota yokken flooding; aksi halde kontrolcünün rotalarıyla unicast
    bool flood = Simulator::Now().GetSeconds() < m_bootstrapEnd || !HasHostRoute(from, target);
    std::vector<uint8_t> bytes = {type,
                                  uint8_t(flood ? 1 : 0),
                                  uint8_t(from),
                                  uint8_t(target),
                                  uint8_t((window >> 8) & 0xff),
                                  uint8_t(window & 0xff)};
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    m_stats.ctrlBytes += bytes.size();

    if (flood)
    {
        // Kendi mesajını tekrar yayınlamasın diye "görüldü" olarak işaretle
        uint64_t key = (uint64_t(type) << 40) | (uint64_t(from) << 32) |
                       (uint64_t(target) << 24) | (window & 0xffff);
        m_seen[from].insert(key);
        Broadcast(from, bytes);
    }
    else
    {
        Ptr<Packet> p = Create<Packet>(bytes.data(), bytes.size());
        m_sockets[from]->SendTo(p, 0, InetSocketAddress(m_addr[target], kPort));
        m_stats.unicastSent++;
    }
}

void
InbandControl::Broadcast(uint32_t node, const std::vector<uint8_t>& bytes)
{
    Ptr<Packet> p = Create<Packet>(bytes.data(), bytes.size());
    // 255.255.255.255: yönlendirme tablosuna bakılmadan tüm komşulara tek sekme yayın
    m_sockets[node]->SendTo(p, 0, InetSocketAddress(Ipv4Address::GetBroadcast(), kPort));
    m_stats.floodTx++;
}

void
InbandControl::HandleRead(Ptr<Socket> socket)
{
    uint32_t node = socket->GetNode()->GetId();
    Ptr<Packet> p;
    Address from;
    while ((p = socket->RecvFrom(from)))
    {
        uint32_t size = p->GetSize();
        if (size < kHeaderSize)
        {
            continue;
        }
        std::vector<uint8_t> buf(size);
        p->CopyData(buf.data(), size);
        Header h{buf[0], buf[1], buf[2], buf[3], uint16_t((buf[4] << 8) | buf[5])};

        if (h.flood)
        {
            uint64_t key = (uint64_t(h.type) << 40) | (uint64_t(h.origin) << 32) |
                           (uint64_t(h.target) << 24) | h.window;
            if (!m_seen[node].insert(key).second)
            {
                continue; // bu mesajı zaten gördük
            }
            if (h.target != node)
            {
                // Hedef ben değilim: bir kez yeniden yayınla (küçük rastgele beklemeyle)
                m_stats.ctrlBytes += size;
                Simulator::Schedule(Seconds(m_jitter->GetValue(0.0, 0.01)),
                                    &InbandControl::Broadcast, this, node, buf);
                continue;
            }
        }
        if (h.target == node)
        {
            Deliver(node, h, buf.data() + kHeaderSize, size - kHeaderSize);
        }
    }
}

void
InbandControl::Deliver(uint32_t node, const Header& h, const uint8_t* payload, uint32_t len)
{
    if (h.type == REPORT && node == m_controller)
    {
        if (len < 1)
        {
            return;
        }
        uint32_t k = payload[0];
        uint32_t es = m_reportDelay ? 9 : 7; // rapor girdisi boyutu (bayt)
        if (len < 1 + es * k)
        {
            return;
        }
        // Daha eski bir pencerenin geciken raporu yenisinin üstüne yazılmasın
        if (m_latest[h.origin].valid && h.window < m_latest[h.origin].window)
        {
            return;
        }
        LatestReport rep;
        rep.window = h.window;
        rep.valid = true;
        for (uint32_t i = 0; i < k; ++i)
        {
            const uint8_t* e = payload + 1 + es * i;
            ReportEntry r;
            r.tx = e[0];
            r.recv = e[1];
            r.snr = int16_t(uint16_t(e[2] << 8 | e[3])) / 100.0;
            r.rssi = int16_t(uint16_t(e[4] << 8 | e[5])) / 100.0;
            r.nFrames = e[6];
            r.delay = m_reportDelay ? uint16_t(e[7] << 8 | e[8]) / 10000.0 : 0.0;
            rep.entries.push_back(r);
        }
        m_latest[h.origin] = rep;
    }
    else if (h.type == UPDATE)
    {
        if (len < m_n || h.window <= m_lastApplied[node])
        {
            return; // eksik ya da daha eski bir güncelleme
        }
        std::vector<int> row(m_n);
        for (uint32_t d = 0; d < m_n; ++d)
        {
            row[d] = payload[d] == 255 ? -1 : int(payload[d]);
        }
        m_ai->InstallNodeRoutes(node, row);
        m_lastApplied[node] = h.window;
        m_stats.updatesApplied++;
        m_stats.delaySum += Simulator::Now().GetSeconds() - m_windowEnd[h.window];
        m_stats.delayCount++;
    }
}

} // namespace ns3
