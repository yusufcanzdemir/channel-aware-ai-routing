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

#include "ai-controller.h"
#include "airtime-meter.h"
#include "disturbance.h"
#include "inband-control.h"
#include "replay-logger.h"
#include "link-monitor.h"
#include "slow-fading-loss-model.h"

#include <cmath>
#include <deque>
#include <fstream>
#include <memory>
#include <set>
#include <iostream>
#include <limits>
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
RepoPath(const std::string& relative)
{
    std::string file = __FILE__;
    std::string dir = file.substr(0, file.find_last_of('/'));
    return dir + "/../" + relative;
}

static std::string
DefaultCsvPath()
{
    return RepoPath("data/sonuclar.csv");
}

int
main(int argc, char* argv[])
{
    // ---------------------------------------------------------------------
    // 1. Komut satırı parametreleri
    // ---------------------------------------------------------------------
    std::string routing = "aodv";  // aodv | olsr | ai
    // none   : bozulmasız, deterministik kanal
    // fading : her linkte zamanla yavaş değişen sönümleme (slow-fading-loss-model.h)
    // degrade: fading + linklerde rastgele anlarda kademeli bozulma olayları
    // interference | jamming | nodefail | congestion: NS-3'ün kendi mekanizmalarıyla
    //          gerçek olaylar; kanal modeline dokunmaz (disturbance.h)
    // ileride: interference, pathloss, nodefail, congestion, jamming
    std::string scenario = "none";
    uint32_t seed = 1;
    uint32_t nNodes = 16;     // toplam düğüm sayısı
    uint32_t gridWidth = 4;   // ızgarada bir satırdaki düğüm sayısı
    // Komşu düğümler arası mesafe (metre). 16 dBm ve log-distance (üs 3) ile:
    //   40 m   -> alınan güç ≈ -78.7 dBm (algılama eşiği -82 dBm'in 3.3 dB üstü, link var)
    //   56.6 m -> (çapraz komşu) ≈ -83.3 dBm, eşiğin altı, link yok
    // Böylece her düğüm sadece 4 yakın komşusunu duyar ve trafik çok sekmeli olur.
    double distance = 40.0;
    // Yerleşim: grid (ızgara, ana deney) | random (alana rastgele, her seed farklı topoloji)
    std::string topology = "grid";
    double area = 0.0; // random: kare alanın kenarı (m); 0 = ortalama ~4 komşu olacak şekilde
    double txPower = 16.0;    // verici gücü (dBm)
    std::string phyMode = "DsssRate1Mbps"; // sabit Wi-Fi hızı (802.11b)
    uint32_t nFlows = 4;      // UDP akış sayısı
    uint32_t packetSize = 512;     // bayt
    std::string dataRate = "32kbps"; // akış başına gönderim hızı
    double warmup = 30.0;     // trafik başlamadan önce protokollerin rota kurma süresi (s)
    double simTime = 120.0;   // toplam simülasyon süresi (s)
    std::string csvPath = DefaultCsvPath();
    // fading senaryosu ayarları
    double fadeSigma = 2.0; // yavaş sönümlemenin büyüklüğü (dB); 2 dB: PDR ~%70-80
    double fadeTau = 5.0;   // yavaş sönümlemenin ne kadar yavaş değiştiği (s)
    double fastSigma = 1.0; // paket başına hızlı dalgalanma (dB)
    // degrade senaryosu: link başına ortalama 120 s'de bir, ~8 dB'e kadar, 25 s süren olay
    double eventRate = 1.0 / 120.0;
    double eventDepth = 8.0;
    // Link ölçümü: dosya yolu verilirse yoklama paketleri açılır ve link CSV'si yazılır
    std::string linkCsv = "";
    std::string replayLog = "";   // sunum sitesi için saniye saniye kayıt (JSON)
    double probeInterval = 0.1; // yoklama aralığı (s): saniyede 10 yoklama
    double window = 1.0;        // ölçüm penceresi (s)
    // AI kontrolcü ayarları (--routing=ai)
    std::string aiPredictor = "linear"; // linear | linear2 | last | ma | ewma | holt | oracle
    uint32_t aiHorizon = 3;             // model kaç saniyelik ortalamayı tahmin ediyor
    std::string aiModel = "";           // boş: gecikmeye uygun model otomatik seçilir
    double aiDelay = 0.0;               // rotaların düğümlere ulaşma gecikmesi (s)
    uint32_t aiInterval = 1;            // rotalar kaç saniyede bir güncellenir
    double aiMinPdr = 0.3;              // tahmini teslim oranı bunun altındaki link kullanılmaz
    // Link maliyeti: etx (beklenen iletim sayısı) | ett (beklenen iletim süresi + gecikme)
    std::string aiCost = "etx";
    double aiTxTime = 0.0;              // ett: bir veri paketinin iletim süresi (s); 0 = hesapla
    double aiDelayWeight = 1.0;         // ett: gecikme teriminin ağırlığı
    std::string aiDebugCsv = "";        // doğrulama: öznitelik + tahmin dökümü
    std::string aiTrainCsv = "";        // eğitim verisi: kontrolcünün gördüğü öznitelikler
    // inband: rapor yalnızca linkler belirgin değişince (ya da her `refresh` pencerede bir)
    bool reportOnChange = false;
    // Teslim oranında bu kadar değişim "belirgin" sayılır. 0.3: tek seed denemesinde sakin ağda
    // raporların ~%67'sini, kademeli bozulmada ~%25'ini bastırdı (0.2: sadece ~%10)
    double changeThreshold = 0.3;
    // Kontrol düzlemi: ideal (bilgi anında, ağa yük yok) | inband (ağda gerçek paketler)
    std::string controlPlane = "ideal";
    // inband: kontrolcünün çalıştığı düğüm. -1 = otomatik: ızgarada 5 (merkeze yakın), rastgele
    // yerleşimde diğer düğümlere toplam sekme sayısı en az olan (ağın merkezi)
    int32_t controllerArg = -1;
    double collectWait = 0.2;     // inband: raporları toplama süresi (s)
    double bootstrap = 5.0;       // inband: bu ana kadar kontrol paketleri flooding ile
    uint32_t refresh = 5;         // inband: her N pencerede bir bütün tablolar yeniden gönderilir

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
    cmd.AddValue("fadeSigma", "fading: yavas sonumleme std sapmasi (dB)", fadeSigma);
    cmd.AddValue("fadeTau", "fading: yavas sonumleme korelasyon suresi (s)", fadeTau);
    cmd.AddValue("fastSigma", "fading: paket basina hizli dalgalanma (dB)", fastSigma);
    cmd.AddValue("eventRate", "degrade: link basina saniyede olay olasiligi", eventRate);
    cmd.AddValue("eventDepth", "degrade: olay derinligi ust siniri (dB)", eventDepth);
    cmd.AddValue("replayLog", "Site icin simulasyon kaydi (JSON dosyasi)", replayLog);
    cmd.AddValue("linkCsv", "Link olcum CSV dosyasi (bos = olcum kapali)", linkCsv);
    cmd.AddValue("probeInterval", "Yoklama paketi araligi (s)", probeInterval);
    cmd.AddValue("window", "Link olcum penceresi (s)", window);
    cmd.AddValue("aiPredictor",
                 "AI tahminci: linear* | gbm* | last | ma | ewma | holt | oracle",
                 aiPredictor);
    cmd.AddValue("aiHorizon", "AI: modelin tahmin ufku (s), model dosyasini secer", aiHorizon);
    cmd.AddValue("aiModel", "Lineer model katsayi dosyasi", aiModel);
    cmd.AddValue("aiDelay", "AI: rota uygulama gecikmesi (s), 0 = ideal", aiDelay);
    cmd.AddValue("aiInterval", "AI: rota guncelleme araligi (pencere)", aiInterval);
    cmd.AddValue("aiMinPdr", "AI: bu tahminin altindaki link kullanilmaz", aiMinPdr);
    cmd.AddValue("aiCost", "AI link maliyeti: etx | ett (gecikme dahil)", aiCost);
    cmd.AddValue("aiTxTime", "ett: veri paketi iletim suresi (s), 0 = otomatik", aiTxTime);
    cmd.AddValue("aiDelayWeight", "ett: gecikme teriminin agirligi", aiDelayWeight);
    cmd.AddValue("aiDebugCsv", "AI: oznitelik/tahmin dokumu (dogrulama icin)", aiDebugCsv);
    cmd.AddValue("reportOnChange",
                 "inband: rapor sadece linkler degisince (ve her refresh pencerede bir)",
                 reportOnChange);
    cmd.AddValue("changeThreshold", "reportOnChange: belirgin degisim esigi", changeThreshold);
    cmd.AddValue("aiTrainCsv",
                 "AI: kontrolcunun gordugu tum oznitelikler (model egitimi icin)",
                 aiTrainCsv);
    cmd.AddValue("controlPlane", "AI kontrol duzlemi: ideal | inband", controlPlane);
    cmd.AddValue("controller", "inband: kontrolcu dugumu (-1 = otomatik, merkez)",
                 controllerArg);
    cmd.AddValue("topology", "Yerlesim: grid (izgara) | random (rastgele)", topology);
    cmd.AddValue("area", "random: kare alanin kenari (m), 0 = yogunluga gore otomatik", area);
    cmd.AddValue("collectWait", "inband: rapor toplama suresi (s)", collectWait);
    cmd.AddValue("bootstrap", "inband: acilista flooding suresi (s)", bootstrap);
    cmd.AddValue("refresh", "inband: tam tablo tazeleme araligi (pencere)", refresh);
    cmd.Parse(argc, argv);

    if (routing != "aodv" && routing != "olsr" && routing != "ai")
    {
        std::cerr << "HATA: bilinmeyen yonlendirme yontemi: " << routing << std::endl;
        return 1;
    }
    if (scenario != "none" && scenario != "fading" && scenario != "degrade" &&
        !Disturbance::IsDisturbance(scenario))
    {
        std::cerr << "HATA: bilinmeyen senaryo: " << scenario << std::endl;
        return 1;
    }

    // Sonuç dosyalarında görünecek yöntem adı: AI'da tahminci de belirtilir (ai-linear gibi)
    std::string method = routing;
    if (routing == "ai")
    {
        method = "ai-" + aiPredictor;
        // Gecikme / aralık varsayılan değilse yöntem adına eklenir (ai-linear-d2-i3 gibi)
        if (aiDelay > 0)
        {
            method += "-d" + std::to_string(int(std::lround(aiDelay)));
        }
        if (aiInterval > 1)
        {
            method += "-i" + std::to_string(aiInterval);
        }
        if (aiHorizon != 3 && aiPredictor.rfind("linear", 0) == 0)
        {
            method += "-h" + std::to_string(aiHorizon);
        }
        if (aiCost == "ett")
        {
            method += "-ett"; // gecikmeli maliyet
            if (std::abs(aiDelayWeight - 1.0) > 1e-9)
            {
                // ağırlık yüzde olarak: 0.2 -> ai-linear2-ettw20
                method += "w" + std::to_string(int(std::lround(aiDelayWeight * 100)));
            }
        }
        // Ölçüm yükü ayarları varsayılan değilse: saniyedeki yoklama sayısı ve
        // "sadece değişince rapor" (ai-linear3-p5-roc-ib gibi)
        if (std::abs(probeInterval - 0.1) > 1e-9)
        {
            method += "-p" + std::to_string(int(std::lround(1.0 / probeInterval)));
        }
        if (reportOnChange)
        {
            method += "-roc";
        }
        if (controlPlane == "inband")
        {
            method += "-ib"; // gerçekçi kontrol düzlemi
        }
        else if (controlPlane != "ideal")
        {
            std::cerr << "HATA: bilinmeyen kontrol duzlemi: " << controlPlane << std::endl;
            return 1;
        }
        // Model seçilmediyse gecikmeye göre eğitilmiş modeli kullan
        // (gecikme D s ise model t+D+1..t+D+3 aralığını tahmin edecek şekilde eğitildi)
        if (aiModel.empty() && (aiPredictor == "ewma" || aiPredictor == "holt"))
        {
            aiModel = RepoPath("ml/models/classic.txt");
        }
        if (aiModel.empty() && aiPredictor.rfind("gbm", 0) == 0)
        {
            // Gradient Boosting: ml/models/gbm3_h3.txt gibi (sadece gecikmesiz model var)
            aiModel = RepoPath("ml/models/" + aiPredictor + "_h" + std::to_string(aiHorizon) +
                               ".txt");
        }
        if (aiModel.empty() && aiPredictor.rfind("linear", 0) == 0)
        {
            // Örnek: linear_h3.txt, linear_o2_h3.txt, linear2_o1_h1.txt
            long d = std::lround(aiDelay);
            std::string h = "_h" + std::to_string(aiHorizon) + ".txt";
            aiModel = RepoPath("ml/models/" + aiPredictor +
                               (d == 0 ? "" : "_o" + std::to_string(d)) + h);
        }
    }
    else if (linkCsv.empty())
    {
        // AODV/OLSR yoklamasız (doğal hali): ölçüm yükü taşımaz. Yoklamalı koşular eski adını
        // (aodv, olsr) korur.
        method += "-np";
    }
    if (topology == "random")
    {
        // Rastgele yerleşim sonuçları ızgarayla karışmasın: ai-linear3-rnd20-ib, aodv-np-rnd20
        std::string tag = "-rnd" + std::to_string(nNodes);
        bool ib = method.size() > 3 && method.compare(method.size() - 3, 3, "-ib") == 0;
        method = ib ? method.substr(0, method.size() - 3) + tag + "-ib" : method + tag;
    }
    else if (topology != "grid")
    {
        std::cerr << "HATA: bilinmeyen topoloji: " << topology << std::endl;
        return 1;
    }

    // Aynı seed ile her zaman aynı sonucu almak için
    RngSeedManager::SetRun(seed);

    // ---------------------------------------------------------------------
    // 2. Düğümler ve konumlar: sabit ızgara ya da rastgele yerleşim
    // ---------------------------------------------------------------------
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Sönümlemesiz alınan güç (dBm): kanaldakiyle aynı mesafe kaybı modeli (ayrı bir nesne)
    Ptr<LogDistancePropagationLossModel> nominalLoss =
        CreateObject<LogDistancePropagationLossModel>();
    nominalLoss->SetAttribute("Exponent", DoubleValue(3.0));
    auto nominalRx = [&](const Vector& a, const Vector& b) {
        Ptr<ConstantPositionMobilityModel> ma = CreateObject<ConstantPositionMobilityModel>();
        Ptr<ConstantPositionMobilityModel> mb = CreateObject<ConstantPositionMobilityModel>();
        ma->SetPosition(a);
        mb->SetPosition(b);
        return nominalLoss->CalcRxPower(txPower, ma, mb);
    };
    const double kDetect = -82.0; // NS-3 algılama eşiği (dBm)

    MobilityHelper mobility;
    double areaSide = 0.0;               // random: alanın kenarı (m)
    std::vector<Vector> randomPos;       // random: düğüm konumları
    std::vector<bool> nominalLink;       // random: [a * n + b] sönümlemesiz link var mı
    if (topology == "random")
    {
        // Menzil: sönümlemesiz alınan gücün algılama eşiğine eşit olduğu mesafe
        double lo = 1.0, hi = 1000.0;
        for (int it = 0; it < 60; ++it)
        {
            double mid = 0.5 * (lo + hi);
            (nominalRx(Vector(0, 0, 0), Vector(mid, 0, 0)) >= kDetect ? lo : hi) = mid;
        }
        double range = lo;
        // Alan: ortalama komşu sayısı ≈ n · π r² / A ≈ 4 olacak şekilde (ızgarada 3; kenardaki
        // düğümler daha az komşu gördüğü için biraz yüksek tutuldu)
        areaSide = area > 0 ? area : std::sqrt(nNodes * M_PI * range * range / 4.0);
        // Ayrı ve sabit numaralı stream: konumlar sadece seed'e bağlı, yöntemden bağımsız
        Ptr<UniformRandomVariable> place = CreateObject<UniformRandomVariable>();
        place->SetStream(1300);
        bool connected = false;
        for (int attempt = 0; attempt < 1000 && !connected; ++attempt)
        {
            // Düğümler arası en az 10 m (aynı noktaya yığılmasınlar)
            randomPos.clear();
            while (randomPos.size() < nNodes)
            {
                Vector p(place->GetValue(0, areaSide), place->GetValue(0, areaSide), 0);
                bool ok = true;
                for (const auto& q : randomPos)
                {
                    ok = ok && CalculateDistance(p, q) >= 10.0;
                }
                if (ok)
                {
                    randomPos.push_back(p);
                }
            }
            // Bağlılık: eşiğin 1 dB üstündeki (sönümlemesiz kararlı) linklerle her düğüme
            // ulaşılabilmeli; değilse yerleşim yeniden çekilir
            nominalLink.assign(nNodes * nNodes, false);
            for (uint32_t a = 0; a < nNodes; ++a)
            {
                for (uint32_t b = 0; b < nNodes; ++b)
                {
                    nominalLink[a * nNodes + b] =
                        a != b && nominalRx(randomPos[a], randomPos[b]) >= kDetect + 1.0;
                }
            }
            std::vector<bool> seen(nNodes, false);
            std::deque<uint32_t> queue = {0};
            seen[0] = true;
            uint32_t count = 1;
            while (!queue.empty())
            {
                uint32_t u = queue.front();
                queue.pop_front();
                for (uint32_t v = 0; v < nNodes; ++v)
                {
                    if (nominalLink[u * nNodes + v] && !seen[v])
                    {
                        seen[v] = true;
                        count++;
                        queue.push_back(v);
                    }
                }
            }
            connected = count == nNodes;
        }
        NS_ABORT_MSG_IF(!connected, "Bagli bir rastgele yerlesim bulunamadi; --area kucultun");
        Ptr<ListPositionAllocator> alloc = CreateObject<ListPositionAllocator>();
        for (const auto& p : randomPos)
        {
            alloc->Add(p);
        }
        mobility.SetPositionAllocator(alloc);
        uint32_t links = 0;
        for (bool l : nominalLink)
        {
            links += l;
        }
        std::cout << "Rastgele yerlesim: " << nNodes << " dugum, alan " << areaSide << " x "
                  << areaSide << " m, menzil " << range << " m, ort. komsu "
                  << double(links) / nNodes << std::endl;
        // Çizim için (analysis/topoloji.py): "KONUM düğüm x y"
        for (uint32_t i = 0; i < nNodes; ++i)
        {
            std::cout << "KONUM " << i << " " << randomPos[i].x << " " << randomPos[i].y << "\n";
        }
    }
    else
    {
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(distance),
                                      "DeltaY", DoubleValue(distance),
                                      "GridWidth", UintegerValue(gridWidth),
                                      "LayoutType", StringValue("RowFirst"));
    }

    // Kontrolcü düğümü: ızgarada 5; rastgele yerleşimde ağın merkezi (diğer düğümlere
    // sönümlemesiz linklerle toplam sekme sayısı en az olan; eşitlikte küçük numara)
    uint32_t controller = 5;
    if (controllerArg >= 0)
    {
        controller = uint32_t(controllerArg);
    }
    else if (topology == "random")
    {
        uint32_t best = std::numeric_limits<uint32_t>::max();
        for (uint32_t c = 0; c < nNodes; ++c)
        {
            std::vector<uint32_t> hop(nNodes, std::numeric_limits<uint32_t>::max());
            std::deque<uint32_t> queue = {c};
            hop[c] = 0;
            uint32_t sum = 0;
            while (!queue.empty())
            {
                uint32_t u = queue.front();
                queue.pop_front();
                sum += hop[u];
                for (uint32_t v = 0; v < nNodes; ++v)
                {
                    if (nominalLink[u * nNodes + v] && hop[v] == std::numeric_limits<uint32_t>::max())
                    {
                        hop[v] = hop[u] + 1;
                        queue.push_back(v);
                    }
                }
            }
            if (sum < best)
            {
                best = sum;
                controller = c;
            }
        }
        std::cout << "Kontrolcu (ag merkezi): dugum " << controller << std::endl;
    }
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
    if (scenario == "fading" || scenario == "degrade")
    {
        // İkinci kayıp modeli zincire eklenir: önce mesafe kaybı, sonra sönümleme.
        // Komşu linklerin algılama eşiğine payı sadece 3.3 dB olduğu için birkaç dB'lik
        // dalgalanma linkleri zaman zaman zayıflatır/koparır; çapraz (56.6 m) linkler
        // ise zaman zaman kullanılabilir hale gelir.
        wifiChannel.AddPropagationLoss("ns3::SlowFadingLossModel",
                                       "Sigma", DoubleValue(fadeSigma),
                                       "Tau", TimeValue(Seconds(fadeTau)),
                                       "FastSigma", DoubleValue(fastSigma),
                                       "EventRate",
                                       DoubleValue(scenario == "degrade" ? eventRate : 0.0),
                                       "EventDepth", DoubleValue(eventDepth));
    }

    YansWifiPhyHelper wifiPhy;
    Ptr<YansWifiChannel> channel = wifiChannel.Create();
    wifiPhy.SetChannel(channel);
    // Kâhin (oracle) için sönümleme modeline erişim: zincirin ikinci halkası
    Ptr<SlowFadingLossModel> fadingModel;
    {
        PointerValue pv;
        channel->GetAttribute("PropagationLossModel", pv);
        fadingModel = DynamicCast<SlowFadingLossModel>(pv.Get<PropagationLossModel>()->GetNext());
    }
    wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // erişim noktası yok, düğümler eşit
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // NS-3 tabanlı bozulma senaryoları: ek radyolar (girişim kaynağı, jammer) aynı kanala
    std::unique_ptr<Disturbance> disturbance;
    if (Disturbance::IsDisturbance(scenario))
    {
        disturbance = std::make_unique<Disturbance>(scenario,
                                                    nodes,
                                                    devices,
                                                    distance,
                                                    gridWidth,
                                                    warmup,
                                                    simTime - 5.0);
        if (topology == "random")
        {
            disturbance->SetCenter(Vector(areaSide / 2.0, areaSide / 2.0, 0)); // alanın ortası
        }
        disturbance->InstallRadios(wifi, wifiPhy, wifiMac);
    }

    // ---------------------------------------------------------------------
    // 4. IP yığını ve yönlendirme protokolü
    // ---------------------------------------------------------------------
    AodvHelper aodv;
    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper list;
    if (routing == "aodv")
    {
        list.Add(aodv, 100);
    }
    else if (routing == "olsr")
    {
        list.Add(olsr, 100);
    }
    else
    {
        // AI: düğümlerde protokol çalışmaz; rotaları merkezi kontrolcü statik tabloya yazar
        list.Add(staticRouting, 0);
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

    // Çok sekmeli trafik olsun diye kaynak ile hedef en az 2 komşu mesafesi uzakta olmalı.
    // Rastgele yerleşimde: birbirini doğrudan duyamamalı (sönümlemesiz güç eşiğin altında)
    auto farEnough = [&](uint32_t a, uint32_t b) {
        Vector pa = nodes.Get(a)->GetObject<MobilityModel>()->GetPosition();
        Vector pb = nodes.Get(b)->GetObject<MobilityModel>()->GetPosition();
        if (topology == "random")
        {
            return nominalRx(pa, pb) < kDetect;
        }
        return CalculateDistance(pa, pb) >= 2.0 * distance - 1e-6;
    };

    std::vector<std::pair<uint32_t, uint32_t>> flows;
    std::vector<Ptr<Application>> flowApps; // kayıt için: her akışın gönderici uygulaması
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
    std::set<uint32_t> sinkNodes; // alıcı kurulmuş düğümler
    for (uint32_t i = 0; i < flows.size(); ++i)
    {
        uint32_t src = flows[i].first;
        uint32_t dst = flows[i].second;

        // Hedefte gelen paketleri karşılayan alıcı (aynı düğüme iki kez kurulmasın).
        // Eski sürüm "düğümde hiç uygulama yoksa" diye bakıyordu: hedef aynı zamanda önceki
        // bir akışın KAYNAĞIYSA alıcı kurulmuyor, her gelen paket kaynağa bir ICMP "port
        // ulaşılamıyor" mesajı döndürüyordu (kanal süresinin %2-5'i). Kanal süresi ölçümüyle
        // bulundu; düzeltmeden sonra bütün deneyler yeniden koşturuldu.
        if (sinkNodes.insert(dst).second)
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
        flowApps.push_back(app.Get(0));
        // Akışlar aynı anda başlamasın diye 0.1 s aralıklarla başlatılır
        app.Start(Seconds(warmup + 0.1 * i));
        app.Stop(Seconds(trafficStop));

        std::cout << "Akis " << i << ": dugum " << src << " -> dugum " << dst << std::endl;
    }

    // ---------------------------------------------------------------------
    // 6. Ölçüm ve çalıştırma
    // ---------------------------------------------------------------------
    // Kanal süresi: trafik süresince her iletim türünün havada kaldığı toplam süre (sadece izler)
    AirtimeMeter airtime(devices, kAppPort, LinkMonitor::kProbeProtocol);
    airtime.SetInterval(warmup, trafficStop);

    std::unique_ptr<ReplayLogger> replay;
    if (!replayLog.empty())
    {
        // Sadece izler: gönderilen/ulaşan paketler, AI'da rota tablolarından yollar
        replay = std::make_unique<ReplayLogger>(nodes, interfaces, flows, routing == "ai");
        replay->Connect(flowApps, kAppPort);
        replay->Start(1.0, simTime);
    }

    if (disturbance)
    {
        disturbance->Schedule(interfaces, flows);
        disturbance->PrintPlan();
    }

    // Link ölçümü: CSV istendiyse veya AI kontrolcü kullanılıyorsa (ölçümlere muhtaç).
    // Yoklamalar simülasyonun başından sonuna kadar.
    std::unique_ptr<LinkMonitor> linkMonitor;
    std::unique_ptr<AiController> ai;
    std::unique_ptr<InbandControl> inband;
    if (!linkCsv.empty() || routing == "ai")
    {
        linkMonitor = std::make_unique<LinkMonitor>(nodes,
                                                    devices,
                                                    Seconds(probeInterval),
                                                    Seconds(window),
                                                    32 /* yoklama boyutu, bayt */);
        linkMonitor->Start(Seconds(1.0), Seconds(simTime));
    }
    if (routing == "ai")
    {
        ai = std::make_unique<AiController>(nodes,
                                            devices,
                                            aiPredictor,
                                            aiModel,
                                            aiMinPdr,
                                            aiDelay,
                                            aiInterval);
        if (!aiDebugCsv.empty())
        {
            ai->SetDebugCsv(aiDebugCsv);
        }
        if (!aiTrainCsv.empty())
        {
            ai->SetTrainCsv(aiTrainCsv);
        }
        if (aiTxTime <= 0.0)
        {
            // 802.11b, 1 Mbps: önsöz 192 µs + (yük + UDP/IP 28 + MAC/LLC/FCS 36 bayt) × 8 µs
            //                  + SIFS 10 µs + ACK 304 µs + DIFS 50 µs  (512 bayt için ~5.2 ms)
            aiTxTime = (192.0 + (packetSize + 28 + 36) * 8.0 + 10.0 + 304.0 + 50.0) * 1e-6;
        }
        ai->SetCost(aiCost, aiTxTime, aiDelayWeight);
        if (aiPredictor == "oracle")
        {
            // Sönümlemesiz alınan güç: kanaldakiyle aynı ayarlı, AYRI bir mesafe kaybı modeli
            // (asıl kanaldaki zinciri çağırmak hızlı sönümleme üretecinden sayı tüketirdi)
            Ptr<LogDistancePropagationLossModel> pathLoss =
                CreateObject<LogDistancePropagationLossModel>();
            pathLoss->SetAttribute("Exponent", DoubleValue(3.0));
            std::vector<double> nominal(nNodes * nNodes, -1000.0);
            for (uint32_t a = 0; a < nNodes; ++a)
            {
                for (uint32_t b = 0; b < nNodes; ++b)
                {
                    if (a != b)
                    {
                        nominal[a * nNodes + b] = pathLoss->CalcRxPower(
                            txPower,
                            nodes.Get(a)->GetObject<MobilityModel>(),
                            nodes.Get(b)->GetObject<MobilityModel>());
                    }
                }
            }
            ai->SetOracle(fadingModel, nominal);
        }
        // Her ölçüm penceresi bitince kontrolcü tahmin yapıp rotaları günceller
        AiController* aiPtr = ai.get();
        if (controlPlane == "inband")
        {
            // Ölçümler ve rotalar ağın içinden gerçek paketlerle taşınır
            inband = std::make_unique<InbandControl>(nodes,
                                                     aiPtr,
                                                     controller,
                                                     uint32_t(std::lround(window / probeInterval)),
                                                     collectWait,
                                                     bootstrap,
                                                     refresh);
            inband->SetReportDelay(aiCost == "ett");
            inband->SetReportOnChange(reportOnChange, changeThreshold);
            InbandControl* ibPtr = inband.get();
            linkMonitor->SetWindowCallback(
                [ibPtr](const std::vector<LinkMonitor::Record>& w) { ibPtr->OnWindow(w); });
        }
        else
        {
            linkMonitor->SetWindowCallback(
                [aiPtr](const std::vector<LinkMonitor::Record>& w) { aiPtr->OnWindow(w); });
        }
    }

    FlowMonitorHelper flowmonHelper;
    // Sadece ana düğümler (ek girişim radyolarında IP yığını yok)
    Ptr<FlowMonitor> monitor = flowmonHelper.Install(nodes);

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
    // Kontrol trafiği (inband): unicast kontrol paketlerinin link üzerindeki iletim sayısı
    uint64_t ctrlLinkTx = 0;

    for (const auto& [flowId, st] : monitor->GetFlowStats())
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        if (t.destinationPort == InbandControl::kPort &&
            t.destinationAddress != Ipv4Address::GetBroadcast())
        {
            // Her iletim (kaynakta 1 + her ara düğümde 1) kanalda bir kez yer kaplar
            ctrlLinkTx += st.txPackets + st.timesForwarded;
            continue;
        }
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

    std::cout << "--- Sonuclar (" << method << ", seed=" << seed << ") ---" << std::endl;
    std::cout << "  Gonderilen paket : " << txPackets << std::endl;
    std::cout << "  Alinan paket     : " << rxPackets << std::endl;
    std::cout << "  PDR              : " << pdr << " %" << std::endl;
    std::cout << "  Ort. gecikme     : " << delayMs << " ms" << std::endl;
    std::cout << "  Throughput       : " << throughputKbps << " kbps" << std::endl;
    std::cout << "  Jitter           : " << jitterMs << " ms" << std::endl;
    std::cout << "  Ort. sekme       : " << avgHops << std::endl;
    if (ai)
    {
        std::cout << "  Rota degisimi    : " << ai->GetRouteChanges() << std::endl;
    }
    // Kontrol düzlemi istatistikleri (ideal modda hepsi 0)
    double ctrlTxPerS = 0.0;  // saniyede kanala çıkan kontrol paketi
    double reportDr = 0.0;    // hesap anında kontrolcüde olan güncel rapor oranı
    double updateDr = 0.0;    // düğümlere ulaşan güncelleme oranı
    double ctrlDelayMs = 0.0; // ölçüm penceresi sonu -> rotanın düğümde uygulanması
    if (inband)
    {
        const InbandControl::Stats& cs = inband->GetStats();
        ctrlTxPerS = double(cs.floodTx + ctrlLinkTx) / (simTime - 1.0);
        reportDr = cs.reportsSent > 0 ? double(cs.reportsFresh) / cs.reportsSent : 0.0;
        updateDr = cs.updatesSent > 0 ? double(cs.updatesApplied) / cs.updatesSent : 0.0;
        ctrlDelayMs = cs.delayCount > 0 ? 1000.0 * cs.delaySum / cs.delayCount : 0.0;
        std::cout << "  Kontrol paketi/s : " << ctrlTxPerS << " (flood " << cs.floodTx
                  << ", unicast iletim " << ctrlLinkTx << ")" << std::endl;
        std::cout << "  Rapor teslim     : " << 100.0 * reportDr << " %" << std::endl;
        if (reportOnChange)
        {
            std::cout << "  Bastirilan rapor : " << cs.reportsSuppressed << " (gonderilen "
                      << cs.reportsSent << ")" << std::endl;
        }
        std::cout << "  Guncelleme teslim: " << 100.0 * updateDr << " %" << std::endl;
        std::cout << "  Kontrol gecikmesi: " << ctrlDelayMs << " ms" << std::endl;
    }

    // Kanal süresi: türe göre, trafik süresinin yüzdesi olarak (düğümlerin toplam gönderim
    // süresi / geçen süre). Uzamsal yeniden kullanım yüzünden toplam %100'ü geçebilir.
    std::vector<double> airPct(AirtimeMeter::N_KINDS);
    std::cout << "  Kanal suresi (%) :";
    for (int k = 0; k < AirtimeMeter::N_KINDS; ++k)
    {
        airPct[k] = 100.0 * airtime.GetSeconds(AirtimeMeter::Kind(k)) / trafficDuration;
        std::cout << " " << AirtimeMeter::Name(AirtimeMeter::Kind(k)) << " " << airPct[k];
    }
    std::cout << std::endl;

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
                   "pdr,delay_ms,throughput_kbps,jitter_ms,avg_hops,"
                   "ctrl_tx_per_s,report_dr,update_dr,ctrl_delay_ms";
            for (int k = 0; k < AirtimeMeter::N_KINDS; ++k)
            {
                csv << ",air_" << AirtimeMeter::Name(AirtimeMeter::Kind(k)) << "_pct";
            }
            csv << "\n";
        }
        csv << method << "," << scenario << "," << seed << "," << nNodes << "," << distance
            << "," << nFlows << "," << txPackets << "," << rxPackets << "," << pdr << ","
            << delayMs << "," << throughputKbps << "," << jitterMs << "," << avgHops << ","
            << ctrlTxPerS << "," << reportDr << "," << updateDr << "," << ctrlDelayMs;
        for (double a : airPct)
        {
            csv << "," << a;
        }
        csv << "\n";
    }

    if (replay)
    {
        replay->Write(replayLog, method, scenario, seed, linkMonitor.get());
        std::cout << "Simulasyon kaydi: " << replayLog << std::endl;
    }

    if (!linkCsv.empty())
    {
        linkMonitor->WriteCsv(linkCsv, method, scenario, seed);
        std::cout << "Link olcumleri: " << linkCsv << std::endl;
    }

    Simulator::Destroy();
    return 0;
}
