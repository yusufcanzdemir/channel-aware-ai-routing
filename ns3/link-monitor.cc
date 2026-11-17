#include "link-monitor.h"

#include "ns3/double.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/wifi-net-device.h"

#include <fstream>
#include <iostream>

namespace ns3
{

LinkMonitor::LinkMonitor(NodeContainer nodes,
                         NetDeviceContainer devices,
                         Time probeInterval,
                         Time window,
                         uint32_t probeSize)
    : m_nodes(nodes),
      m_devices(devices),
      m_probeInterval(probeInterval),
      m_window(window),
      m_probeSize(probeSize),
      m_n(nodes.GetN())
{
    m_sent.assign(m_n, 0);
    m_recv.assign(m_n * m_n, 0);
    m_snrSum.assign(m_n * m_n, 0.0);
    m_rssiSum.assign(m_n * m_n, 0.0);
    m_nFrames.assign(m_n * m_n, 0);

    // Yoklama zamanlarındaki rastgele kayma için ayrı, sabit numaralı stream
    // (seed'e bağlı, yönlendirme protokolünden bağımsız)
    m_jitter = CreateObject<UniformRandomVariable>();
    m_jitter->SetStream(1001);

    for (uint32_t i = 0; i < m_n; ++i)
    {
        Ptr<NetDevice> dev = m_devices.Get(i);
        m_macToNode[Mac48Address::ConvertFrom(dev->GetAddress())] = i;

        // Yoklama alıcısı: bu düğüme kendi EtherType'ımızla gelen paketler
        m_nodes.Get(i)->RegisterProtocolHandler(MakeCallback(&LinkMonitor::OnProbe, this, i),
                                                kProbeProtocol,
                                                dev);

        // SNR ölçümü: PHY'nin başarıyla aldığı her çerçeve
        Ptr<WifiPhy> phy = DynamicCast<WifiNetDevice>(dev)->GetPhy();
        phy->TraceConnectWithoutContext("MonitorSnifferRx",
                                        MakeCallback(&LinkMonitor::OnSniffRx, this, i));
    }
}

void
LinkMonitor::Start(Time start, Time stop)
{
    m_stop = stop;
    for (uint32_t i = 0; i < m_n; ++i)
    {
        // Her düğüm farklı bir anda başlasın (rastgele faz), aksi halde
        // tüm yoklamalar aynı anda gönderilip sürekli çarpışır
        Time phase = Seconds(m_jitter->GetValue(0.0, m_probeInterval.GetSeconds()));
        Simulator::Schedule(start + phase, &LinkMonitor::SendProbe, this, i);
    }
    Simulator::Schedule(start + m_window, &LinkMonitor::EndWindow, this);
}

void
LinkMonitor::SendProbe(uint32_t node)
{
    if (Simulator::Now() >= m_stop)
    {
        return;
    }
    Ptr<Packet> p = Create<Packet>(m_probeSize);
    m_devices.Get(node)->Send(p, Mac48Address::GetBroadcast(), kProbeProtocol);
    m_sent[node]++;

    // Bir sonraki yoklama: aralık ± %10 kayma
    double next = m_probeInterval.GetSeconds() * m_jitter->GetValue(0.9, 1.1);
    Simulator::Schedule(Seconds(next), &LinkMonitor::SendProbe, this, node);
}

void
LinkMonitor::OnProbe(uint32_t rx,
                     Ptr<NetDevice> dev,
                     Ptr<const Packet> pkt,
                     uint16_t protocol,
                     const Address& from,
                     const Address& to,
                     NetDevice::PacketType type)
{
    auto it = m_macToNode.find(Mac48Address::ConvertFrom(from));
    if (it != m_macToNode.end())
    {
        m_recv[it->second * m_n + rx]++;
    }
}

void
LinkMonitor::OnSniffRx(uint32_t rx,
                       Ptr<const Packet> pkt,
                       uint16_t channelFreqMhz,
                       WifiTxVector txVector,
                       MpduInfo aMpdu,
                       SignalNoiseDbm signalNoise,
                       uint16_t staId)
{
    // Çerçevenin MAC başlığından gönderen adresini (Addr2) oku.
    // ACK gibi kontrol çerçevelerinde Addr2 yok; sadece veri çerçevelerini kullan.
    WifiMacHeader hdr;
    pkt->PeekHeader(hdr);
    if (!hdr.IsData())
    {
        return;
    }
    auto it = m_macToNode.find(hdr.GetAddr2());
    if (it == m_macToNode.end() || it->second == rx)
    {
        return;
    }
    uint32_t idx = it->second * m_n + rx;
    m_snrSum[idx] += signalNoise.signal - signalNoise.noise;
    m_rssiSum[idx] += signalNoise.signal;
    m_nFrames[idx]++;
}

void
LinkMonitor::SetWindowCallback(WindowCallback cb)
{
    m_windowCb = cb;
}

void
LinkMonitor::EndWindow()
{
    double now = Simulator::Now().GetSeconds();
    size_t first = m_records.size(); // bu pencerenin kayıtları buradan başlar
    for (uint32_t tx = 0; tx < m_n; ++tx)
    {
        for (uint32_t rx = 0; rx < m_n; ++rx)
        {
            if (tx == rx)
            {
                continue;
            }
            uint32_t idx = tx * m_n + rx;
            m_records.push_back(Record{now,
                                       tx,
                                       rx,
                                       m_sent[tx],
                                       m_recv[idx],
                                       m_snrSum[idx],
                                       m_rssiSum[idx],
                                       m_nFrames[idx]});
        }
    }
    // Dinleyen varsa (AI kontrolcü) bu pencerenin ölçümlerini ilet
    if (m_windowCb)
    {
        std::vector<Record> window(m_records.begin() + first, m_records.end());
        m_windowCb(window);
    }

    // Sayaçları yeni pencere için sıfırla
    std::fill(m_sent.begin(), m_sent.end(), 0);
    std::fill(m_recv.begin(), m_recv.end(), 0);
    std::fill(m_snrSum.begin(), m_snrSum.end(), 0.0);
    std::fill(m_rssiSum.begin(), m_rssiSum.end(), 0.0);
    std::fill(m_nFrames.begin(), m_nFrames.end(), 0);

    if (Simulator::Now() + m_window <= m_stop)
    {
        Simulator::Schedule(m_window, &LinkMonitor::EndWindow, this);
    }
}

void
LinkMonitor::WriteCsv(const std::string& path,
                      const std::string& routing,
                      const std::string& scenario,
                      uint32_t seed) const
{
    // Hiç paket duyulmamış linkleri (çok uzak düğüm çiftleri) yazma; dosya şişmesin
    std::vector<bool> heard(m_n * m_n, false);
    for (const auto& r : m_records)
    {
        if (r.recv > 0 || r.nFrames > 0)
        {
            heard[r.tx * m_n + r.rx] = true;
        }
    }

    bool newFile = !std::ifstream(path).good();
    std::ofstream csv(path, std::ios::app);
    if (!csv)
    {
        std::cerr << "UYARI: link CSV dosyasi acilamadi: " << path << std::endl;
        return;
    }
    if (newFile)
    {
        csv << "routing,scenario,seed,time,tx,rx,sent,recv,pdr,snr_db,rssi_dbm,n_frames\n";
    }
    for (const auto& r : m_records)
    {
        if (!heard[r.tx * m_n + r.rx])
        {
            continue;
        }
        double pdr = r.sent > 0 ? double(r.recv) / r.sent : 0.0;
        csv << routing << "," << scenario << "," << seed << "," << r.time << "," << r.tx << ","
            << r.rx << "," << r.sent << "," << r.recv << "," << pdr << ",";
        // Pencerede hiç çerçeve duyulmadıysa SNR bilinmiyor: boş bırak
        if (r.nFrames > 0)
        {
            csv << r.snrSum / r.nFrames << "," << r.rssiSum / r.nFrames;
        }
        else
        {
            csv << ",";
        }
        csv << "," << r.nFrames << "\n";
    }
}

} // namespace ns3
