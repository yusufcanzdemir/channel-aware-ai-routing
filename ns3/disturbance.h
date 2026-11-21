// NS-3'ün kendi mekanizmalarıyla oluşturulan bozulma senaryoları.
//
// fading/degrade senaryoları bizim yazdığımız kanal modelini kullanır ve tahmin edilebilir
// bir yapıya sahiptir. Buradaki senaryolar ise kanalı DEĞİŞTİRMEZ; ağda gerçek olaylar
// yaratır. Modelin "kendi kanalımızı ezberleyip ezberlemediğini" sınamak için kullanılır.
//
//   interference : Ağın ortasına yerleştirilen 3 ek verici rastgele açılıp kapanır; açık
//                  oldukları sürede yayın hızları kademeli olarak artar. Ortak kanalı meşgul
//                  eder, çarpışma yaratır (bölgesel, artan girişim).
//   jamming      : Merkezdeki yüksek güçlü (20 dBm) bir verici, rastgele zamanlı 2 saldırı
//                  boyunca kanalı sürekli meşgul eder. (Wi-Fi kurallarına uyan "sürekli
//                  yayın yapan" jammer benzetimi; kurallara uymayan jammer Yans modelinde yok.)
//   nodefail     : Trafiğin kaynağı/hedefi olmayan 3 düğümün radyosu rastgele anlarda
//                  10-25 s boyunca kapanır (arıza, pil bitmesi gibi).
//   congestion   : 3 ek arka plan UDP akışı (96 kbps) rastgele dönemlerde devreye girer;
//                  bu akışlar da yönlendirilir ve kuyruklarda yük oluşturur.
//
// Bütün rastgele seçimler sabit numaralı ayrı akışlardan (stream 1100+) yapılır: aynı seed'de
// her yöntem birebir aynı bozulmayı yaşar.

#ifndef DISTURBANCE_H
#define DISTURBANCE_H

#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

#include <string>
#include <utility>
#include <vector>

namespace ns3
{

class Disturbance
{
  public:
    // Bu senaryo adı bu modül tarafından mı ele alınıyor?
    static bool IsDisturbance(const std::string& scenario);

    Disturbance(const std::string& scenario,
                NodeContainer nodes,
                NetDeviceContainer devices,
                double spacing,
                uint32_t gridWidth,
                double start,
                double stop);

    // Girişim kaynaklarının / jammer'ın yerleşeceği merkez. Verilmezse ızgaranın ortası
    // (spacing ve gridWidth'ten). Rastgele yerleşimde alanın ortası verilir.
    // InstallRadios'tan önce çağrılmalı.
    void SetCenter(Vector center);

    // Ek radyolu düğümleri (girişim kaynağı, jammer) oluşturur. Ana düğümlerin Wi-Fi
    // kurulumundan HEMEN SONRA, aynı yardımcılarla çağrılmalı (aynı kanal).
    void InstallRadios(WifiHelper& wifi, YansWifiPhyHelper& phy, WifiMacHelper& mac);

    // Olayları zamanlar. IP adresleri atandıktan ve ana akışlar seçildikten sonra
    // çağrılmalı (tıkanıklık akışları IP ister; arızalı düğüm akış uçlarından seçilmez).
    void Schedule(Ipv4InterfaceContainer interfaces,
                  std::vector<std::pair<uint32_t, uint32_t>> flows);

    // Simülasyon başında ne olacağını ekrana yazar (hangi düğüm ne zaman, vb.)
    void PrintPlan() const;

  private:
    // Ek vericinin bir yayın çerçevesi gönderip bir sonrakini zamanlaması
    void Emit(uint32_t k, double onStart, double onEnd, double rateStart, double rateEnd,
              uint32_t size);

    std::string m_scenario;
    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
    double m_spacing;
    uint32_t m_gridWidth;
    bool m_hasCenter = false;
    Vector m_center;
    std::vector<std::pair<uint32_t, uint32_t>> m_flows;
    double m_start;
    double m_stop;

    NodeContainer m_extra;          // girişim kaynakları / jammer
    NetDeviceContainer m_extraDevs;
    Ptr<UniformRandomVariable> m_rng;
    std::vector<std::string> m_plan; // insan okunur olay listesi
};

} // namespace ns3

#endif
