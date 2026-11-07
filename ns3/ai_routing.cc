// Kanal durumuna göre yapay zekâ tabanlı yönlendirme - ana simülasyon programı.
//
// Tüm yöntemler (AODV, OLSR, AI) AYNI topoloji, AYNI trafik ve AYNI seed ile
// koşulur; tek fark --routing parametresidir. Böylece karşılaştırma adil olur.
//
// Topoloji : sabit düğümlü Wi-Fi ad hoc ızgara (varsayılan 4x4 = 16 düğüm)
// Trafik   : UDP OnOff akışları, kaynak-hedef çiftleri seed'e göre seçilir
// Metrikler: FlowMonitor ile PDR, gecikme, throughput, jitter, ortalama sekme
//
// Örnek:  ./ns3 run "ai_routing --routing=olsr --seed=3"

#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/olsr-module.h"
#include "ns3/wifi-module.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AiRouting");

// Uygulama trafiğinin kullandığı UDP portu. FlowMonitor yönlendirme protokollerinin
// kendi mesajlarını da (OLSR, AODV) akış olarak sayar; sadece bu porttakileri ölçeceğiz.
static const uint16_t kAppPort = 9;

// Bu dosyanın bulunduğu klasörden depo kökündeki data/sonuclar.csv yolunu üretir.
// __FILE__ NS-3 içindeki scratch/ai_routing/ai_routing.cc'yi gösterir; scratch/ai_routing
// bizim ns3/ klasörümüze symlink olduğu için "/../data" depo kökündeki data/ olur.
static std::string
DefaultCsvPath()
{
    std::string file = __FILE__;
    std::string dir = file.substr(0, file.find_last_of('/'));
    return dir + "/../data/sonuclar.csv";
}

int
main(int argc, char* argv[])
{
    // ---------------------------------------------------------------------
    // 1. Komut satırı parametreleri
    // ---------------------------------------------------------------------
    std::string routing = "aodv";  // aodv | olsr | ai
    std::string scenario = "none"; // ileride: interference, pathloss, nodefail, congestion, jamming
    uint32_t seed = 1;
    uint32_t nNodes = 16;     // toplam düğüm sayısı
    uint32_t gridWidth = 4;   // ızgarada bir satırdaki düğüm sayısı
    // Komşu düğümler arası mesafe (metre). 16 dBm ve log-distance (üs 3) ile:
    //   40 m   -> alınan güç ≈ -78.7 dBm (algılama eşiği -82 dBm'in 3.3 dB üstü, link var)
    //   56.6 m -> (çapraz komşu) ≈ -83.3 dBm, eşiğin altı, link yok
    // Böylece her düğüm sadece 4 yakın komşusunu duyar ve trafik çok sekmeli olur.
    double distance = 40.0;
    double txPower = 16.0;    // verici gücü (dBm)
    std::string phyMode = "DsssRate1Mbps"; // sabit Wi-Fi hızı (802.11b)
    uint32_t nFlows = 4;      // UDP akış sayısı
    uint32_t packetSize = 512;     // bayt
    std::string dataRate = "32kbps"; // akış başına gönderim hızı
    double warmup = 30.0;     // trafik başlamadan önce protokollerin rota kurma süresi (s)
    double simTime = 120.0;   // toplam simülasyon süresi (s)
    std::string csvPath = DefaultCsvPath();

    CommandLine cmd(__FILE__);
    cmd.AddValue("routing", "Yonlendirme yontemi: aodv | olsr | ai", routing);
    cmd.AddValue("scenario", "Bozulma senaryosu", scenario);
    cmd.AddValue("seed", "Rastgelelik icin calistirma numarasi", seed);
    cmd.AddValue("nodes", "Dugum sayisi", nNodes);
    cmd.AddValue("gridWidth", "Izgarada bir satirdaki dugum sayisi", gridWidth);
    cmd.AddValue("distance", "Komsu dugumler arasi mesafe (m)", distance);
    cmd.AddValue("txPower", "Verici gucu (dBm)", txPower);
    cmd.AddValue("phyMode", "Sabit Wi-Fi hizi", phyMode);
    cmd.AddValue("flows", "UDP akis sayisi", nFlows);
    cmd.AddValue("packetSize", "Paket boyutu (bayt)", packetSize);
    cmd.AddValue("dataRate", "Akis basina gonderim hizi", dataRate);
    cmd.AddValue("warmup", "Trafik baslamadan onceki isinma suresi (s)", warmup);
    cmd.AddValue("simTime", "Toplam simulasyon suresi (s)", simTime);
    cmd.AddValue("csv", "Sonuclarin eklenecegi CSV dosyasi", csvPath);
    cmd.Parse(argc, argv);

    if (routing == "ai")
    {
        // Merkezi AI kontrolcü 8-9. haftalarda eklenecek.
        std::cerr << "HATA: --routing=ai henuz uygulanmadi (hafta 8-9)." << std::endl;
        return 1;
    }
    if (routing != "aodv" && routing != "olsr")
    {
        std::cerr << "HATA: bilinmeyen yonlendirme yontemi: " << routing << std::endl;
        return 1;
    }

    // Aynı seed ile her zaman aynı sonucu almak için
    RngSeedManager::SetRun(seed);

    // ---------------------------------------------------------------------
    // 2. Düğümler ve konumlar: sabit ızgara
    // ---------------------------------------------------------------------
    NodeContainer nodes;
    nodes.Create(nNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(distance),
                                  "GridWidth", UintegerValue(gridWidth),
                                  "LayoutType", StringValue("RowFirst"));
    // Hareketlilik yok: düğümler simülasyon boyunca yerinde kalır
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // ---------------------------------------------------------------------
    // 3. Wi-Fi: 802.11b, ad hoc mod, sabit hız, log-distance yol kaybı
    // ---------------------------------------------------------------------
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    // Hız uyarlama kapalı: kanal kötüleşince Wi-Fi'nin kendi hızını düşürmesi
    // yöntemlerin farkını gizlemesin. Yayın paketleri (OLSR HELLO vb.) da aynı hızda.
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    // Log-distance: kayıp = ReferenceLoss + 10 * Exponent * log10(d). Exponent=3
    // açık alandan (2) daha kötü, binalı ortama yakın. Yol kaybı senaryosu bu modeli
    // değiştirerek kurulacak.
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                   "Exponent", DoubleValue(3.0));

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // erişim noktası yok, düğümler eşit
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // ---------------------------------------------------------------------
    // 4. IP yığını ve yönlendirme protokolü
    // ---------------------------------------------------------------------
    AodvHelper aodv;
    OlsrHelper olsr;
    Ipv4ListRoutingHelper list;
    if (routing == "aodv")
    {
        list.Add(aodv, 100);
    }
    else
    {
        list.Add(olsr, 100);
    }

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // ---------------------------------------------------------------------
    // 5. Trafik: UDP akışları
    // ---------------------------------------------------------------------
    // Kaynak-hedef seçimi için ayrı bir rastgele sayı akışı (stream) kullanıyoruz.
    // Stream numarası sabit olduğu için seçim sadece seed'e bağlıdır; yönlendirme
    // protokolü değişse de aynı seed aynı çiftleri verir (adil karşılaştırma).
    Ptr<UniformRandomVariable> pick = CreateObject<UniformRandomVariable>();
    pick->SetStream(1000);

    // Çok sekmeli trafik olsun diye kaynak ile hedef en az 2 komşu mesafesi uzakta olmalı
    auto farEnough = [&](uint32_t a, uint32_t b) {
        Vector pa = nodes.Get(a)->GetObject<MobilityModel>()->GetPosition();
        Vector pb = nodes.Get(b)->GetObject<MobilityModel>()->GetPosition();
        return CalculateDistance(pa, pb) >= 2.0 * distance - 1e-6;
    };

    std::vector<std::pair<uint32_t, uint32_t>> flows;
    while (flows.size() < nFlows)
    {
        uint32_t src = pick->GetInteger(0, nNodes - 1);
        uint32_t dst = pick->GetInteger(0, nNodes - 1);
        if (src != dst && farEnough(src, dst))
        {
            flows.emplace_back(src, dst);
        }
    }

    // Trafik ısınmadan sonra başlar, simülasyon bitmeden 5 s önce durur ki
    // yoldaki son paketler de hedefe ulaşabilsin.
    double trafficStop = simTime - 5.0;
    double trafficDuration = trafficStop - warmup;

    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), kAppPort));
    for (uint32_t i = 0; i < flows.size(); ++i)
    {
        uint32_t src = flows[i].first;
        uint32_t dst = flows[i].second;

        // Hedefte gelen paketleri karşılayan alıcı (aynı düğüme iki kez kurulmasın)
        if (nodes.Get(dst)->GetNApplications() == 0)
        {
            sinkHelper.Install(nodes.Get(dst)).Start(Seconds(0.0));
        }

        OnOffHelper onoff("ns3::UdpSocketFactory",
                          InetSocketAddress(interfaces.GetAddress(dst), kAppPort));
        // Sürekli açık: sabit hızda kesintisiz gönderim
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", StringValue(dataRate));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer app = onoff.Install(nodes.Get(src));
        // Akışlar aynı anda başlamasın diye 0.1 s aralıklarla başlatılır
        app.Start(Seconds(warmup + 0.1 * i));
        app.Stop(Seconds(trafficStop));

        std::cout << "Akis " << i << ": dugum " << src << " -> dugum " << dst << std::endl;
    }

    // ---------------------------------------------------------------------
    // 6. Ölçüm ve çalıştırma
    // ---------------------------------------------------------------------
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // ---------------------------------------------------------------------
    // 7. Sonuçlar: tüm uygulama akışlarının toplamı
    // ---------------------------------------------------------------------
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());

    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t rxBytes = 0;
    uint64_t timesForwarded = 0;
    double delaySum = 0.0;  // saniye
    double jitterSum = 0.0; // saniye
    uint64_t jitterCount = 0;

    for (const auto& [flowId, st] : monitor->GetFlowStats())
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        if (t.destinationPort != kAppPort)
        {
            continue; // yönlendirme protokolünün kendi mesajları, atla
        }
        txPackets += st.txPackets;
        rxPackets += st.rxPackets;
        rxBytes += st.rxBytes;
        timesForwarded += st.timesForwarded;
        delaySum += st.delaySum.GetSeconds();
        jitterSum += st.jitterSum.GetSeconds();
        if (st.rxPackets > 1)
        {
            jitterCount += st.rxPackets - 1;
        }
    }

    // PDR: alınan / gönderilen paket
    double pdr = txPackets > 0 ? 100.0 * rxPackets / txPackets : 0.0;
    // Ortalama uçtan uca gecikme ve jitter (milisaniye)
    double delayMs = rxPackets > 0 ? 1000.0 * delaySum / rxPackets : 0.0;
    double jitterMs = jitterCount > 0 ? 1000.0 * jitterSum / jitterCount : 0.0;
    // Toplam throughput: trafik süresi boyunca alınan bit / süre (kbps)
    double throughputKbps = rxBytes * 8.0 / trafficDuration / 1000.0;
    // Ortalama sekme sayısı ≈ 1 + paket başına ara düğümde iletilme sayısı
    double avgHops = rxPackets > 0 ? 1.0 + double(timesForwarded) / rxPackets : 0.0;

    std::cout << "--- Sonuclar (" << routing << ", seed=" << seed << ") ---" << std::endl;
    std::cout << "  Gonderilen paket : " << txPackets << std::endl;
    std::cout << "  Alinan paket     : " << rxPackets << std::endl;
    std::cout << "  PDR              : " << pdr << " %" << std::endl;
    std::cout << "  Ort. gecikme     : " << delayMs << " ms" << std::endl;
    std::cout << "  Throughput       : " << throughputKbps << " kbps" << std::endl;
    std::cout << "  Jitter           : " << jitterMs << " ms" << std::endl;
    std::cout << "  Ort. sekme       : " << avgHops << std::endl;

    // CSV'ye bir satır ekle; dosya yoksa önce başlık satırını yaz
    bool newFile = !std::ifstream(csvPath).good();
    std::ofstream csv(csvPath, std::ios::app);
    if (!csv)
    {
        std::cerr << "UYARI: CSV dosyasi acilamadi: " << csvPath << std::endl;
    }
    else
    {
        if (newFile)
        {
            csv << "routing,scenario,seed,nodes,distance,flows,tx_packets,rx_packets,"
                   "pdr,delay_ms,throughput_kbps,jitter_ms,avg_hops\n";
        }
        csv << routing << "," << scenario << "," << seed << "," << nNodes << "," << distance
            << "," << nFlows << "," << txPackets << "," << rxPackets << "," << pdr << ","
            << delayMs << "," << throughputKbps << "," << jitterMs << "," << avgHops << "\n";
    }

    Simulator::Destroy();
    return 0;
}
