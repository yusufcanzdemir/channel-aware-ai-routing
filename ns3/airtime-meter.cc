#include "airtime-meter.h"

#include "ns3/ipv4-header.h"
#include "ns3/llc-snap-header.h"
#include "ns3/simulator.h"
#include "ns3/udp-header.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/wifi-net-device.h"

namespace ns3
{

AirtimeMeter::AirtimeMeter(NetDeviceContainer devices, uint16_t dataPort, uint16_t probeProtocol)
    : m_dataPort(dataPort),
      m_probeProtocol(probeProtocol)
{
    // Sadece verilen (ana düğümlerin) cihazları; ek girişim radyoları sayılmaz
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(devices.Get(i));
        NS_ABORT_MSG_IF(!dev, "AirtimeMeter: Wi-Fi cihazi degil");
        Ptr<WifiPhy> phy = dev->GetPhy();
        m_band = phy->GetPhyBand();
        phy->TraceConnectWithoutContext("PhyTxPsduBegin",
                                        MakeCallback(&AirtimeMeter::OnTx, this));
    }
}

void
AirtimeMeter::SetInterval(double start, double stop)
{
    m_start = start;
    m_stop = stop;
}

void
AirtimeMeter::OnTx(WifiConstPsduMap psduMap, WifiTxVector txVector, double txPowerW)
{
    double now = Simulator::Now().GetSeconds();
    if (now < m_start || now >= m_stop || psduMap.empty())
    {
        return;
    }
    // Bu iletimin havada kalma süresi (önsöz + PLCP başlığı + MAC çerçevesi)
    double dur = WifiPhy::CalculateTxDuration(psduMap, txVector, m_band).GetSeconds();
    Kind k = Classify(psduMap.begin()->second);
    m_seconds[k] += dur;
    m_count[k]++;
}

AirtimeMeter::Kind
AirtimeMeter::Classify(Ptr<const WifiPsdu> psdu) const
{
    const WifiMacHeader& mac = psdu->GetHeader(0);
    if (mac.IsAck() || mac.IsCtl())
    {
        return ACK; // onay (ve varsa diğer kontrol çerçeveleri)
    }
    if (!mac.IsData())
    {
        return OTHER;
    }
    // Veri çerçevesinin içeriği: LLC/SNAP başlığı + (yoklama | IP paketi)
    Ptr<Packet> p = psdu->GetPayload(0)->Copy();
    LlcSnapHeader llc;
    if (p->GetSize() < llc.GetSerializedSize())
    {
        return OTHER;
    }
    p->RemoveHeader(llc);
    if (llc.GetType() == m_probeProtocol)
    {
        return PROBE;
    }
    if (llc.GetType() == 0x0806)
    {
        return ARP;
    }
    if (llc.GetType() != 0x0800)
    {
        return OTHER;
    }
    Ipv4Header ip;
    p->RemoveHeader(ip);
    if (ip.GetProtocol() != UdpL4Protocol::PROT_NUMBER)
    {
        return OTHER;
    }
    UdpHeader udp;
    p->PeekHeader(udp);
    uint16_t port = udp.GetDestinationPort();
    if (port == m_dataPort)
    {
        return DATA;
    }
    if (port == 7000) // InbandControl::kPort
    {
        return CONTROL;
    }
    if (port == 654 || port == 698) // AODV, OLSR
    {
        return ROUTING;
    }
    return OTHER;
}

double
AirtimeMeter::GetSeconds(Kind k) const
{
    return m_seconds[k];
}

uint64_t
AirtimeMeter::GetCount(Kind k) const
{
    return m_count[k];
}

std::string
AirtimeMeter::Name(Kind k)
{
    static const char* names[] = {"probe", "control", "routing", "data", "ack", "arp", "other"};
    return names[k];
}

} // namespace ns3
