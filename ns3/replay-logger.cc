#include "replay-logger.h"

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/simulator.h"

#include <fstream>
#include <iostream>

namespace ns3
{

ReplayLogger::ReplayLogger(NodeContainer nodes,
                           Ipv4InterfaceContainer interfaces,
                           std::vector<std::pair<uint32_t, uint32_t>> flows,
                           bool staticRoutes)
    : m_nodes(nodes),
      m_flows(flows),
      m_staticRoutes(staticRoutes)
{
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        m_addr.push_back(interfaces.GetAddress(i));
        m_addrToNode[interfaces.GetAddress(i)] = i;
    }
    m_sentNow.assign(flows.size(), 0);
    m_recvNow.assign(flows.size(), 0);
}

void
ReplayLogger::Connect(const std::vector<Ptr<Application>>& sources, uint16_t port)
{
    m_port = port;
    for (uint32_t f = 0; f < sources.size(); ++f)
    {
        sources[f]->TraceConnectWithoutContext("Tx", MakeCallback(&ReplayLogger::OnTx, this, f));
    }
    std::vector<bool> done(m_nodes.GetN(), false);
    for (const auto& fl : m_flows)
    {
        if (done[fl.second])
        {
            continue;
        }
        done[fl.second] = true;
        Ptr<Ipv4L3Protocol> ip = m_nodes.Get(fl.second)->GetObject<Ipv4L3Protocol>();
        ip->TraceConnectWithoutContext("LocalDeliver",
                                       MakeCallback(&ReplayLogger::OnDeliver, this, fl.second));
    }
}

void
ReplayLogger::OnTx(uint32_t flow, Ptr<const Packet> p)
{
    m_sentNow[flow]++;
}

void
ReplayLogger::OnDeliver(uint32_t dst, const Ipv4Header& h, Ptr<const Packet> p, uint32_t iface)
{
    // Sadece uygulama trafiği (UDP, ilgili port); kontrol ve yönlendirme mesajları değil
    if (h.GetProtocol() != UdpL4Protocol::PROT_NUMBER)
    {
        return;
    }
    UdpHeader udp;
    p->PeekHeader(udp);
    if (udp.GetDestinationPort() != m_port)
    {
        return;
    }
    // Gönderen adresinden ve hedef düğümden hangi akış olduğunu bul
    auto it = m_addrToNode.find(h.GetSource());
    if (it == m_addrToNode.end())
    {
        return;
    }
    for (uint32_t f = 0; f < m_flows.size(); ++f)
    {
        if (m_flows[f].first == it->second && m_flows[f].second == dst)
        {
            m_recvNow[f]++;
            return;
        }
    }
}

void
ReplayLogger::Start(double start, double stop)
{
    Simulator::Schedule(Seconds(start), &ReplayLogger::Tick, this, stop);
}

void
ReplayLogger::Tick(double stop)
{
    // Geçen saniyenin sayaçlarını ve şu anki yolları kaydet
    m_times.push_back(Simulator::Now().GetSeconds());
    m_sent.push_back(m_sentNow);
    m_recv.push_back(m_recvNow);
    std::vector<std::vector<int>> paths;
    for (uint32_t f = 0; f < m_flows.size(); ++f)
    {
        paths.push_back(m_staticRoutes ? Path(f) : std::vector<int>{});
    }
    m_paths.push_back(paths);
    std::fill(m_sentNow.begin(), m_sentNow.end(), 0);
    std::fill(m_recvNow.begin(), m_recvNow.end(), 0);
    if (Simulator::Now().GetSeconds() + 1.0 <= stop)
    {
        Simulator::Schedule(Seconds(1.0), &ReplayLogger::Tick, this, stop);
    }
}

std::vector<int>
ReplayLogger::Path(uint32_t flow) const
{
    Ipv4StaticRoutingHelper helper;
    uint32_t cur = m_flows[flow].first;
    uint32_t dst = m_flows[flow].second;
    std::vector<int> path = {int(cur)};
    for (uint32_t hop = 0; hop < m_nodes.GetN() && cur != dst; ++hop)
    {
        Ptr<Ipv4StaticRouting> rt =
            helper.GetStaticRouting(m_nodes.Get(cur)->GetObject<Ipv4>());
        int next = -1;
        for (uint32_t i = 0; i < rt->GetNRoutes(); ++i)
        {
            Ipv4RoutingTableEntry e = rt->GetRoute(i);
            if (e.IsHost() && e.GetDest() == m_addr[dst])
            {
                auto it = m_addrToNode.find(e.GetGateway());
                next = it == m_addrToNode.end() ? -1 : int(it->second);
                break;
            }
        }
        if (next < 0)
        {
            path.push_back(-1); // bu düğümde hedefe rota yok
            return path;
        }
        cur = uint32_t(next);
        path.push_back(next);
    }
    if (cur != dst)
    {
        path.push_back(-1); // döngü
    }
    return path;
}

void
ReplayLogger::Write(const std::string& path,
                    const std::string& method,
                    const std::string& scenario,
                    uint32_t seed,
                    const LinkMonitor* links) const
{
    std::ofstream o(path);
    if (!o)
    {
        std::cerr << "UYARI: kayit dosyasi acilamadi: " << path << std::endl;
        return;
    }
    o << "{\"method\":\"" << method << "\",\"scenario\":\"" << scenario << "\",\"seed\":" << seed;
    o << ",\"flows\":[";
    for (size_t f = 0; f < m_flows.size(); ++f)
    {
        o << (f ? "," : "") << "[" << m_flows[f].first << "," << m_flows[f].second << "]";
    }
    o << "],\"times\":[";
    for (size_t t = 0; t < m_times.size(); ++t)
    {
        o << (t ? "," : "") << m_times[t];
    }
    auto matrix = [&](const char* name, const std::vector<std::vector<uint32_t>>& m) {
        o << "],\"" << name << "\":[";
        for (size_t t = 0; t < m.size(); ++t)
        {
            o << (t ? "," : "") << "[";
            for (size_t f = 0; f < m[t].size(); ++f)
            {
                o << (f ? "," : "") << m[t][f];
            }
            o << "]";
        }
    };
    matrix("sent", m_sent);
    matrix("recv", m_recv);
    o << "],\"paths\":[";
    for (size_t t = 0; t < m_paths.size(); ++t)
    {
        o << (t ? "," : "") << "[";
        for (size_t f = 0; f < m_paths[t].size(); ++f)
        {
            o << (f ? "," : "") << "[";
            for (size_t k = 0; k < m_paths[t][f].size(); ++k)
            {
                o << (k ? "," : "") << m_paths[t][f][k];
            }
            o << "]";
        }
        o << "]";
    }
    // Link ölçümleri: [zaman, gönderen, alan, teslim oranı] (hiç duyulmayanlar hariç)
    o << "],\"links\":[";
    bool first = true;
    if (links)
    {
        for (const auto& r : links->GetRecords())
        {
            if (r.recv == 0)
            {
                continue;
            }
            o << (first ? "" : ",") << "[" << r.time << "," << r.tx << "," << r.rx << ","
              << double(r.recv) / std::max<uint32_t>(r.sent, 1) << "]";
            first = false;
        }
    }
    o << "]}\n";
}

} // namespace ns3
