#include "disturbance.h"

#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace ns3
{

// Ek vericilerin çerçeveleri için yerel deneysel EtherType (yoklamalardan farklı);
// ana düğümlerde bu protokolü dinleyen kimse yok, çerçeveler sadece kanalı meşgul eder
static const uint16_t kNoiseProtocol = 0x88B6;
static const uint16_t kCrossPort = 10; // tıkanıklık akışları (metriklere girmez)

bool
Disturbance::IsDisturbance(const std::string& scenario)
{
    return scenario == "interference" || scenario == "jamming" || scenario == "nodefail" ||
           scenario == "congestion";
}

Disturbance::Disturbance(const std::string& scenario,
                         NodeContainer nodes,
                         NetDeviceContainer devices,
                         double spacing,
                         uint32_t gridWidth,
                         double start,
                         double stop)
    : m_scenario(scenario),
      m_nodes(nodes),
      m_devices(devices),
      m_spacing(spacing),
      m_gridWidth(gridWidth),
      m_start(start),
      m_stop(stop)
{
    m_rng = CreateObject<UniformRandomVariable>();
    m_rng->SetStream(1100);
}

void
Disturbance::SetCenter(Vector center)
{
    m_center = center;
    m_hasCenter = true;
}

void
Disturbance::InstallRadios(WifiHelper& wifi, YansWifiPhyHelper& phy, WifiMacHelper& mac)
{
    std::vector<Vector> pos;
    uint32_t rows = (m_nodes.GetN() + m_gridWidth - 1) / m_gridWidth;
    // Izgaranın orta noktası (4x4'te düğüm 5, 6, 9, 10'un ortası)
    double cx = m_hasCenter ? m_center.x : (m_gridWidth - 1) * m_spacing / 2.0;
    double cy = m_hasCenter ? m_center.y : (rows - 1) * m_spacing / 2.0;
    if (m_scenario == "interference")
    {
        // Orta bölgenin etrafına üç verici: bölgedeki linkleri meşgul eder
        pos = {Vector(cx - 0.4 * m_spacing, cy, 0), Vector(cx + 0.4 * m_spacing, cy, 0),
               Vector(cx, cy + 0.4 * m_spacing, 0)};
    }
    else if (m_scenario == "jamming")
    {
        pos = {Vector(cx, cy, 0)};
    }
    if (pos.empty())
    {
        return;
    }
    m_extra.Create(pos.size());
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> alloc = CreateObject<ListPositionAllocator>();
    for (const auto& p : pos)
    {
        alloc->Add(p);
    }
    mobility.SetPositionAllocator(alloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(m_extra);
    m_extraDevs = wifi.Install(phy, mac, m_extra);
    if (m_scenario == "jamming")
    {
        // Jammer daha güçlü: 20 dBm ile ~69 m'ye kadar algılanır (normal düğüm ~51 m)
        Ptr<WifiPhy> jphy = DynamicCast<WifiNetDevice>(m_extraDevs.Get(0))->GetPhy();
        jphy->SetTxPowerStart(20.0);
        jphy->SetTxPowerEnd(20.0);
    }
}

void
Disturbance::Emit(uint32_t k,
                  double onStart,
                  double onEnd,
                  double rateStart,
                  double rateEnd,
                  uint32_t size)
{
    double now = Simulator::Now().GetSeconds();
    if (now >= onEnd || now >= m_stop)
    {
        return;
    }
    m_extraDevs.Get(k)->Send(Create<Packet>(size), Mac48Address::GetBroadcast(), kNoiseProtocol);
    // Hız açık kalma süresi boyunca rateStart'tan rateEnd'e doğrusal artar
    double frac = (now - onStart) / std::max(onEnd - onStart, 1e-9);
    double rate = rateStart + (rateEnd - rateStart) * frac;
    Simulator::Schedule(Seconds(1.0 / rate), &Disturbance::Emit, this, k, onStart, onEnd,
                        rateStart, rateEnd, size);
}

void
Disturbance::Schedule(Ipv4InterfaceContainer interfaces,
                      std::vector<std::pair<uint32_t, uint32_t>> flows)
{
    m_flows = flows;
    std::ostringstream os;
    os.precision(1);
    os << std::fixed;

    if (m_scenario == "interference")
    {
        // Her verici için ardışık açık/kapalı dönemler: kapalı 10-25 s, açık 8-20 s.
        // Açıkken hız 20'den 150 çerçeve/s'ye çıkar (500 bayt; 1 Mbps'te kanalın ~%65'i)
        for (uint32_t k = 0; k < m_extra.GetN(); ++k)
        {
            double t = m_start + m_rng->GetValue(0.0, 10.0);
            while (t < m_stop)
            {
                double on = m_rng->GetValue(8.0, 20.0);
                double end = std::min(t + on, m_stop);
                Simulator::Schedule(Seconds(t), &Disturbance::Emit, this, k, t, end, 20.0, 150.0,
                                    500u);
                os.str("");
                os << "girisim kaynagi " << k << ": " << t << "-" << end << " s";
                m_plan.push_back(os.str());
                t = end + m_rng->GetValue(10.0, 25.0);
            }
        }
    }
    else if (m_scenario == "jamming")
    {
        // İki saldırı: ilki trafiğin ilk yarısında, ikincisi ikinci yarısında; 8-15 s.
        // 1000 baytlık çerçeve her 9 ms'de bir: kanal saldırı boyunca neredeyse hep dolu
        double mid = (m_start + m_stop) / 2.0;
        for (int a = 0; a < 2; ++a)
        {
            double lo = a == 0 ? m_start + 5.0 : mid;
            double hi = a == 0 ? mid - 15.0 : m_stop - 15.0;
            double t = m_rng->GetValue(lo, std::max(lo, hi));
            double end = t + m_rng->GetValue(8.0, 15.0);
            Simulator::Schedule(Seconds(t), &Disturbance::Emit, this, 0u, t, end, 111.0, 111.0,
                                1000u);
            os.str("");
            os << "jammer saldirisi: " << t << "-" << end << " s";
            m_plan.push_back(os.str());
        }
    }
    else if (m_scenario == "nodefail")
    {
        // Kaynak ve hedef olmayan düğümler arasından 3 tanesi rastgele seçilir
        std::vector<uint32_t> candidates;
        for (uint32_t i = 0; i < m_nodes.GetN(); ++i)
        {
            bool endpoint = false;
            for (const auto& f : m_flows)
            {
                endpoint = endpoint || f.first == i || f.second == i;
            }
            if (!endpoint)
            {
                candidates.push_back(i);
            }
        }
        for (int k = 0; k < 3 && !candidates.empty(); ++k)
        {
            uint32_t idx = m_rng->GetInteger(0, candidates.size() - 1);
            uint32_t node = candidates[idx];
            candidates.erase(candidates.begin() + idx);
            double t = m_rng->GetValue(m_start + 5.0, m_stop - 25.0);
            double end = t + m_rng->GetValue(10.0, 25.0);
            Ptr<WifiPhy> phy = DynamicCast<WifiNetDevice>(m_devices.Get(node))->GetPhy();
            Simulator::Schedule(Seconds(t), &WifiPhy::SetOffMode, phy);
            Simulator::Schedule(Seconds(end), &WifiPhy::ResumeFromOff, phy);
            os.str("");
            os << "dugum " << node << " arizali: " << t << "-" << end << " s";
            m_plan.push_back(os.str());
        }
    }
    else if (m_scenario == "congestion")
    {
        // 3 arka plan akışı: rastgele kaynak-hedef, 96 kbps, rastgele 20-40 s'lik dönem
        PacketSinkHelper sink("ns3::UdpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), kCrossPort));
        std::vector<bool> hasSink(m_nodes.GetN(), false);
        for (int k = 0; k < 3; ++k)
        {
            uint32_t src = m_rng->GetInteger(0, m_nodes.GetN() - 1);
            uint32_t dst = m_rng->GetInteger(0, m_nodes.GetN() - 2);
            if (dst >= src)
            {
                dst++; // kaynak ile hedef farklı olsun
            }
            if (!hasSink[dst])
            {
                sink.Install(m_nodes.Get(dst)).Start(Seconds(0.0));
                hasSink[dst] = true;
            }
            double t = m_rng->GetValue(m_start + 5.0, m_stop - 40.0);
            double end = std::min(t + m_rng->GetValue(20.0, 40.0), m_stop);
            OnOffHelper onoff("ns3::UdpSocketFactory",
                              InetSocketAddress(interfaces.GetAddress(dst), kCrossPort));
            onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            onoff.SetAttribute("DataRate", StringValue("96kbps"));
            onoff.SetAttribute("PacketSize", UintegerValue(512));
            ApplicationContainer app = onoff.Install(m_nodes.Get(src));
            app.Start(Seconds(t));
            app.Stop(Seconds(end));
            os.str("");
            os << "arka plan akisi " << src << " -> " << dst << ": " << t << "-" << end << " s";
            m_plan.push_back(os.str());
        }
    }
}

void
Disturbance::PrintPlan() const
{
    for (const auto& line : m_plan)
    {
        std::cout << "Bozulma: " << line << std::endl;
    }
}

} // namespace ns3
