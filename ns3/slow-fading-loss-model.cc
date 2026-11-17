#include "slow-fading-loss-model.h"

#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/mobility-model.h"
#include "ns3/node.h"
#include "ns3/simulator.h"

#include <cmath>

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(SlowFadingLossModel);

TypeId
SlowFadingLossModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::SlowFadingLossModel")
            .SetParent<PropagationLossModel>()
            .AddConstructor<SlowFadingLossModel>()
            .AddAttribute("Sigma",
                          "Yavas bilesenin standart sapmasi (dB)",
                          DoubleValue(3.0),
                          MakeDoubleAccessor(&SlowFadingLossModel::m_sigma),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("Tau",
                          "Yavas bilesenin korelasyon suresi",
                          TimeValue(Seconds(5.0)),
                          MakeTimeAccessor(&SlowFadingLossModel::m_tau),
                          MakeTimeChecker())
            .AddAttribute("Step",
                          "Yavas bilesenin guncellenme adimi",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&SlowFadingLossModel::m_step),
                          MakeTimeChecker())
            .AddAttribute("FastSigma",
                          "Paket basina hizli bilesenin standart sapmasi (dB)",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&SlowFadingLossModel::m_fastSigma),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("StreamBase",
                          "Link basina rastgele sayi akislarinin baslangic numarasi",
                          IntegerValue(2000),
                          MakeIntegerAccessor(&SlowFadingLossModel::m_streamBase),
                          MakeIntegerChecker<int64_t>())
            .AddAttribute("EventRate",
                          "Link basina saniyede bozulma olayi baslama olasiligi (0 = kapali)",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&SlowFadingLossModel::m_eventRate),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("EventDepth",
                          "Bozulma olayi derinligi ust siniri (dB)",
                          DoubleValue(8.0),
                          MakeDoubleAccessor(&SlowFadingLossModel::m_eventDepth),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("EventRamp",
                          "Bozulma olayinda kotulesme/toparlanma suresi",
                          TimeValue(Seconds(10.0)),
                          MakeTimeAccessor(&SlowFadingLossModel::m_eventRamp),
                          MakeTimeChecker())
            .AddAttribute("EventHold",
                          "Bozulma olayinda en kotu seviyede kalma suresi",
                          TimeValue(Seconds(5.0)),
                          MakeTimeAccessor(&SlowFadingLossModel::m_eventHold),
                          MakeTimeChecker());
    return tid;
}

SlowFadingLossModel::SlowFadingLossModel()
{
    m_fast = CreateObject<NormalRandomVariable>();
    m_fast->SetAttribute("Mean", DoubleValue(0.0));
    m_fast->SetAttribute("Variance", DoubleValue(1.0));
}

SlowFadingLossModel::LinkState&
SlowFadingLossModel::Advance(uint32_t nodeA, uint32_t nodeB) const
{
    // Link yönsüz: (3,5) ile (5,3) aynı link
    auto key = std::make_pair(std::min(nodeA, nodeB), std::max(nodeA, nodeB));
    int64_t now = Simulator::Now().GetNanoSeconds() / m_step.GetNanoSeconds();

    auto it = m_links.find(key);
    if (it == m_links.end())
    {
        // İlk kez görülen link: kendi stream'iyle bir üreteç oluştur.
        // Stream numarası düğüm numaralarından türetilir, sorgu sırasından bağımsızdır.
        LinkState s;
        s.rng = CreateObject<NormalRandomVariable>();
        s.rng->SetAttribute("Mean", DoubleValue(0.0));
        s.rng->SetAttribute("Variance", DoubleValue(1.0));
        s.rng->SetStream(m_streamBase + key.first * 1000 + key.second);
        s.value = m_sigma * s.rng->GetValue(); // başlangıç değeri: durağan dağılımdan
        s.step = 0;
        s.evRng = CreateObject<UniformRandomVariable>();
        s.evRng->SetStream(m_streamBase + 100000 + key.first * 1000 + key.second);
        s.evStart = -1;
        s.evDepth = 0.0;
        it = m_links.emplace(key, s).first;
    }

    // Son güncellemeden bu yana geçen her adım için AR(1) güncellemesi
    LinkState& s = it->second;
    double a = std::exp(-m_step.GetSeconds() / m_tau.GetSeconds());
    double b = m_sigma * std::sqrt(1.0 - a * a);
    int64_t rampSteps = m_eventRamp.GetNanoSeconds() / m_step.GetNanoSeconds();
    int64_t holdSteps = m_eventHold.GetNanoSeconds() / m_step.GetNanoSeconds();
    double pStart = m_eventRate * m_step.GetSeconds(); // adım başına olay olasılığı
    while (s.step < now)
    {
        s.value = a * s.value + b * s.rng->GetValue();
        s.step++;
        if (m_eventRate > 0.0)
        {
            // Biten olayı kapat, olay yoksa yeni olay başlayabilir
            if (s.evStart >= 0 && s.step - s.evStart >= 2 * rampSteps + holdSteps)
            {
                s.evStart = -1;
            }
            // Üreteç her adımda çekilir (olay olsa da olmasa da), böylece sayı dizisi
            // sadece zamana bağlı kalır
            double u = s.evRng->GetValue();
            double d = s.evRng->GetValue(0.5, 1.0);
            if (s.evStart < 0 && u < pStart)
            {
                s.evStart = s.step;
                s.evDepth = m_eventDepth * d;
            }
        }
    }
    return s;
}

double
SlowFadingLossModel::EventLoss(const LinkState& s) const
{
    if (s.evStart < 0)
    {
        return 0.0;
    }
    double ramp = m_eventRamp.GetSeconds();
    double hold = m_eventHold.GetSeconds();
    double t = (s.step - s.evStart) * m_step.GetSeconds(); // olay başından beri geçen süre
    if (t < ramp)
    {
        return s.evDepth * t / ramp; // kötüleşiyor
    }
    if (t < ramp + hold)
    {
        return s.evDepth; // en kötü seviye
    }
    return s.evDepth * std::max(0.0, 1.0 - (t - ramp - hold) / ramp); // toparlanıyor
}

double
SlowFadingLossModel::GetSlowLossDb(uint32_t nodeA, uint32_t nodeB) const
{
    const LinkState& s = Advance(nodeA, nodeB);
    return s.value + EventLoss(s);
}

double
SlowFadingLossModel::DoCalcRxPower(double txPowerDbm,
                                   Ptr<MobilityModel> a,
                                   Ptr<MobilityModel> b) const
{
    // Mobilite modeli düğüme bağlıdır; düğüm numarasını oradan alırız
    uint32_t idA = a->GetObject<Node>()->GetId();
    uint32_t idB = b->GetObject<Node>()->GetId();
    const LinkState& s = Advance(idA, idB);
    double slow = s.value + EventLoss(s);
    double fast = m_fastSigma * m_fast->GetValue();
    // Pozitif değer = ek kayıp, negatif değer = sinyal normalden güçlü
    return txPowerDbm - slow - fast;
}

int64_t
SlowFadingLossModel::DoAssignStreams(int64_t stream)
{
    m_fast->SetStream(stream);
    return 1;
}

} // namespace ns3
