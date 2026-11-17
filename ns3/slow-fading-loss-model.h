// Yavaş sönümleme (gölgeleme) kanal modeli.
//
// Her düğüm çifti (link) için zamanla YAVAŞ değişen ek bir kayıp (dB) üretir.
// Gerçek dünyada bu; insanların, araçların, kapıların sinyali kısmen kapatmasıyla
// oluşan ve birkaç saniye süren zayıflamalara benzer. Değişim yavaş olduğu için
// geçmiş ölçümlerden yakın gelecek tahmin edilebilir: tahmin modelimizin öğreneceği şey bu.
//
// Model üç bileşenden oluşur:
//  1) Yavaş bileşen: birinci dereceden otoregresif süreç (AR(1), Gauss-Markov):
//       x[k+1] = a * x[k] + sigma * sqrt(1 - a^2) * N(0,1),   a = exp(-adim / tau)
//     sigma: dalgalanmanın büyüklüğü (dB), tau: ne kadar yavaş değiştiği (s).
//  2) Hızlı bileşen: her pakette bağımsız küçük Gauss gürültüsü (dB). Tahmin edilemez,
//     ölçümleri gerçekçi şekilde "gürültülü" yapar.
//  3) Kademeli bozulma olayları (isteğe bağlı, EventRate > 0): linkte rastgele anlarda
//     bir olay başlar; ek kayıp Ramp süresinde 0'dan Depth dB'e doğrusal artar, Hold
//     süresi kadar öyle kalır, sonra Ramp süresinde geri iner. (Yaklaşan bir engel,
//     artan girişim gibi.) Bu bileşende geçmişteki eğilim geleceği haber verir.
//
// Adil karşılaştırma için önemli: her linkin yavaş bileşeni kendi rastgele sayı
// akışından (stream) ve sabit zaman adımlarıyla üretilir. Böylece hangi yönlendirme
// protokolü kullanılırsa kullanılsın, aynı seed'de kanal aynı şekilde değişir.
// Sinyal iki yönde aynı şekilde etkilenir (link simetrik).
//
// Kâhin (oracle) desteği: Her linkin yavaş bileşeni kendi akışından sırayla üretildiği için
// gelecekteki adımları erken üretmek değerleri DEĞİŞTİRMEZ. Bu yüzden üretilen değerler bir
// diziye (yörünge) kaydedilir; GetSlowLossDbAt() gelecekteki bir anın değerini verebilir.
// Bu sadece "teorik üst sınır" deneyi içindir; gerçek bir sistem geleceği bilemez.

#ifndef SLOW_FADING_LOSS_MODEL_H
#define SLOW_FADING_LOSS_MODEL_H

#include "ns3/nstime.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/random-variable-stream.h"

#include <map>
#include <utility>
#include <vector>

namespace ns3
{

class SlowFadingLossModel : public PropagationLossModel
{
  public:
    static TypeId GetTypeId();
    SlowFadingLossModel();

    // Belirli bir linkin o anki yavaş kayıp değeri (dB). Kayıt/hata ayıklama için.
    double GetSlowLossDb(uint32_t nodeA, uint32_t nodeB) const;
    // Belirli bir linkin t anındaki (gelecek de olabilir) yavaş kayıp değeri (dB)
    double GetSlowLossDbAt(uint32_t nodeA, uint32_t nodeB, Time t) const;
    // Paket başına hızlı bileşenin standart sapması (dB)
    double GetFastSigma() const;
    // Yavaş bileşenin zaman adımı
    Time GetStep() const;

  private:
    double DoCalcRxPower(double txPowerDbm,
                         Ptr<MobilityModel> a,
                         Ptr<MobilityModel> b) const override;
    int64_t DoAssignStreams(int64_t stream) override;

    // Bir linkin yavaş bileşeninin durumu
    struct LinkState
    {
        Ptr<NormalRandomVariable> rng; // bu linke özel rastgele sayı üreteci
        double value;                  // şu anki ek kayıp (dB)
        int64_t step;                  // value'nun ait olduğu zaman adımı
        Ptr<UniformRandomVariable> evRng; // bozulma olayları için ayrı üreteç
        int64_t evStart;               // aktif olayın başladığı adım (-1: olay yok)
        double evDepth;                // aktif olayın derinliği (dB)
        std::vector<double> traj;      // traj[k]: k. adımdaki toplam yavaş kayıp (dB)
    };

    // Linkin durumunu döndürür (ilk kez görülüyorsa oluşturur)
    LinkState& GetLink(uint32_t nodeA, uint32_t nodeB) const;
    // Linkin yörüngesini k. adıma kadar üretir
    void ExtendTo(LinkState& s, int64_t k) const;
    // Linkin k. adımdaki toplam yavaş kaybı (dB)
    double LossAtStep(uint32_t nodeA, uint32_t nodeB, int64_t k) const;
    // Aktif bozulma olayının şu anki ek kaybı (dB)
    double EventLoss(const LinkState& s) const;

    double m_sigma;        // yavaş bileşenin standart sapması (dB)
    Time m_tau;            // korelasyon süresi
    Time m_step;           // yavaş bileşenin güncellenme adımı
    double m_fastSigma;    // hızlı bileşenin standart sapması (dB)
    int64_t m_streamBase;  // link akışlarının başlangıç stream numarası
    double m_eventRate;    // link başına saniyede olay başlama olasılığı (0 = kapalı)
    double m_eventDepth;   // olay derinliği üst sınırı (dB); her olay [Depth/2, Depth]
    Time m_eventRamp;      // kötüleşme / toparlanma süresi
    Time m_eventHold;      // en kötü seviyede kalma süresi

    Ptr<NormalRandomVariable> m_fast; // hızlı bileşen için
    // const fonksiyonlardan güncellenebilmesi için mutable
    mutable std::map<std::pair<uint32_t, uint32_t>, LinkState> m_links;
};

} // namespace ns3

#endif
