// Kanal durumuna göre yapay zekâ tabanlı yönlendirme - ana simülasyon programı.
// Şimdilik sadece kurulumun ve parametrelerin çalıştığını test eden iskelet.

#include "ns3/core-module.h"

#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AiRouting");

int
main(int argc, char* argv[])
{
    std::string routing = "aodv";  // aodv | olsr | ai
    std::string scenario = "none"; // ileride: interference, pathloss, nodefail, congestion, jamming
    uint32_t seed = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("routing", "Yonlendirme yontemi: aodv | olsr | ai", routing);
    cmd.AddValue("scenario", "Bozulma senaryosu", scenario);
    cmd.AddValue("seed", "Rastgelelik icin calistirma numarasi", seed);
    cmd.Parse(argc, argv);

    // Aynı seed ile her zaman aynı sonucu almak için
    RngSeedManager::SetRun(seed);

    std::cout << "Kurulum calisiyor!" << std::endl;
    std::cout << "  routing  = " << routing << std::endl;
    std::cout << "  scenario = " << scenario << std::endl;
    std::cout << "  seed     = " << seed << std::endl;

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
