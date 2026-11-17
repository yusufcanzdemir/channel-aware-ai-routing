// Cihazda uygulanabilirlik ölçümü: AI kontrolcünün bir karar döngüsünün maliyeti.
//
// NS-3'ten bağımsızdır; her cihazda (PC, Raspberry Pi, vb.) şöyle derlenip çalıştırılır:
//     g++ -O2 -std=c++17 -o bench_controller device/bench_controller.cc
//     ./bench_controller ml/models/linear2_h3.txt
//
// Ölçülen iş, ns3/ai-controller.cc'deki ile aynıdır:
//   1) Her yönlü link için 37 öznitelik (model v2) hesaplanır
//   2) Lineer model ile teslim oranı tahmin edilir (37 çarpma-toplama)
//   3) Maliyet = 1 / (p_gidiş · p_dönüş), p < 0.3 olan link kullanılmaz
//   4) Her kaynak için Dijkstra (16 düğümde basit dizi; büyük ağda da aynı algoritma)
// İki durum ölçülür:
//   - "tüm çiftler": bizim simülasyondaki gibi n·(n-1) yönlü linkin hepsi tahmin edilir
//   - "sadece komşular": gerçek bir cihazda yalnızca ölçümü olan linkler (ızgarada ~4 komşu)
//     tahmin edilir; bu, gerçek bir uygulamanın yapacağı iştir
//
// Girdi verisi rastgele üretilir (zamanlama veriden bağımsızdır).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <sys/resource.h>
#include <vector>

static const int kHistory = 10;
static const double kDetect = -82.0;
static const double kUnknown = -3.0;

struct Link
{
    double pdr[kHistory];
    double snr[kHistory]; // NaN: bilinmiyor
    double rssi[kHistory];
};

struct Model
{
    double intercept = 0;
    std::vector<std::pair<std::string, double>> coef;
};

static Model
LoadModel(const char* path)
{
    Model m;
    std::ifstream in(path);
    std::string name;
    double v;
    while (in >> name >> v)
    {
        if (name == "intercept")
            m.intercept = v;
        else
            m.coef.emplace_back(name, v);
    }
    return m;
}

// ns3/ai-controller.cc OwnFeatures ile aynı hesap (öznitelik adı -> değer)
static void
OwnFeatures(const Link& l, std::map<std::string, double>& f, const std::string& pre)
{
    double s5 = 0, s10 = 0;
    for (int k = 0; k < kHistory; ++k)
    {
        if (pre.empty())
            f["pdr_l" + std::to_string(k)] = l.pdr[k];
        s10 += l.pdr[k];
        if (k < 5)
            s5 += l.pdr[k];
    }
    f[pre + "pdr_l0"] = l.pdr[0];
    f[pre + "pdr_l1"] = l.pdr[1];
    f[pre + "pdr_l2"] = l.pdr[2];
    f[pre + "pdr_mean5"] = s5 / 5;
    f[pre + "pdr_mean10"] = s10 / kHistory;
    f[pre + "pdr_trend"] = l.pdr[0] - l.pdr[2];
    if (pre.empty())
    {
        for (int k = 0; k < 3; ++k)
        {
            bool known = !std::isnan(l.snr[k]);
            f["snr_l" + std::to_string(k)] = known ? l.snr[k] : 0;
            f["snrk_l" + std::to_string(k)] = known ? 1 : 0;
        }
        bool both = !std::isnan(l.snr[0]) && !std::isnan(l.snr[2]);
        f["snr_trend"] = both ? l.snr[0] - l.snr[2] : 0;
    }
    double mar[5], sm = 0, mx = -1e9;
    for (int k = 0; k < 5; ++k)
    {
        mar[k] = std::isnan(l.rssi[k]) ? kUnknown : l.rssi[k] - kDetect;
        if (pre.empty())
            f["mar_l" + std::to_string(k)] = mar[k];
        sm += mar[k];
        mx = std::max(mx, mar[k]);
    }
    f[pre + "mar_l0"] = mar[0];
    f[pre + "mar_mean5"] = sm / 5;
    if (pre.empty())
        f["mar_max5"] = mx;
    f[pre + "mar_slope"] = (2 * mar[0] + mar[1] - mar[3] - 2 * mar[4]) / 10;
}

static double
Predict(const Model& m, const Link& own, const Link& rev)
{
    std::map<std::string, double> f;
    OwnFeatures(own, f, "");
    OwnFeatures(rev, f, "rev_");
    double y = m.intercept;
    for (const auto& [name, c] : m.coef)
        y += c * f[name];
    return std::clamp(y, 0.0, 1.0);
}

// Bir karar döngüsü: tahmin + maliyet + her kaynaktan Dijkstra. Dönüş: kullanılan link sayısı
static int
Cycle(const Model& m, int n, const std::vector<Link>& links, const std::vector<char>& active,
      std::vector<int>& nextHop)
{
    const double inf = std::numeric_limits<double>::infinity();
    std::vector<double> pred(n * n, 0.0);
    for (int u = 0; u < n; ++u)
        for (int v = 0; v < n; ++v)
            if (u != v && active[u * n + v])
                pred[u * n + v] = Predict(m, links[u * n + v], links[v * n + u]);
    std::vector<double> cost(n * n, inf);
    int used = 0;
    for (int u = 0; u < n; ++u)
        for (int v = 0; v < n; ++v)
        {
            double a = pred[u * n + v], b = pred[v * n + u];
            if (u != v && a >= 0.3 && b >= 0.3)
            {
                cost[u * n + v] = 1.0 / (a * b);
                used++;
            }
        }
    for (int s = 0; s < n; ++s)
    {
        std::vector<double> dist(n, inf);
        std::vector<int> first(n, -1);
        std::vector<char> done(n, 0);
        dist[s] = 0;
        for (int it = 0; it < n; ++it)
        {
            int u = -1;
            for (int i = 0; i < n; ++i)
                if (!done[i] && (u < 0 || dist[i] < dist[u]))
                    u = i;
            if (u < 0 || dist[u] == inf)
                break;
            done[u] = 1;
            for (int v = 0; v < n; ++v)
            {
                double c = cost[u * n + v];
                if (!done[v] && c < inf && dist[u] + c < dist[v])
                {
                    dist[v] = dist[u] + c;
                    first[v] = (u == s) ? v : first[u];
                }
            }
        }
        for (int d = 0; d < n; ++d)
            nextHop[s * n + d] = first[d];
    }
    return used;
}

int
main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "ml/models/linear2_h3.txt";
    Model m = LoadModel(path);
    if (m.coef.empty())
    {
        std::fprintf(stderr, "Model okunamadi: %s\n", path);
        return 1;
    }
    std::printf("Model: %s (%zu katsayi, %zu bayt)\n", path, m.coef.size(),
                (m.coef.size() + 1) * sizeof(double));

    // Tek bir link tahmininin süresi
    std::mt19937 rng(1);
    std::uniform_real_distribution<double> U(0, 1);
    auto randLink = [&]() {
        Link l;
        for (int k = 0; k < kHistory; ++k)
        {
            l.pdr[k] = U(rng);
            l.snr[k] = U(rng) < 0.1 ? NAN : 10 + 5 * U(rng);
            l.rssi[k] = U(rng) < 0.1 ? NAN : -84 + 6 * U(rng);
        }
        return l;
    };
    {
        Link a = randLink(), b = randLink();
        const int R = 200000;
        volatile double sink = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < R; ++r)
            sink = sink + Predict(m, a, b);
        double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                        .count() / R;
        std::printf("Tek link tahmini: %.2f us\n\n", us);
    }

    std::printf("%6s %18s %18s %16s\n", "dugum", "tum ciftler (ms)", "sadece komsu (ms)",
                "bellek (KB)");
    for (int n : {16, 25, 50, 100})
    {
        int w = int(std::lround(std::sqrt(double(n))));
        std::vector<Link> links(n * n);
        for (auto& l : links)
            l = randLink();
        std::vector<char> all(n * n, 1), nb(n * n, 0);
        for (int u = 0; u < n; ++u)
            for (int v = 0; v < n; ++v)
            {
                int du = std::abs(u % w - v % w), dv = std::abs(u / w - v / w);
                nb[u * n + v] = (du + dv == 1); // ızgarada 4 komşu
            }
        std::vector<int> nh(n * n);
        double res[2];
        for (int mode = 0; mode < 2; ++mode)
        {
            const auto& act = mode == 0 ? all : nb;
            int R = n <= 25 ? 50 : (n <= 50 ? 10 : 3);
            auto t0 = std::chrono::steady_clock::now();
            for (int r = 0; r < R; ++r)
                Cycle(m, n, links, act, nh);
            res[mode] = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - t0).count() / R;
        }
        // Kontrolcünün tuttuğu durum: link geçmişi + maliyet/tahmin dizileri + rota tablosu
        double kb = (n * n * sizeof(Link) + 2.0 * n * n * sizeof(double) + n * n * sizeof(int)) /
                    1024.0;
        std::printf("%6d %18.3f %18.3f %16.1f\n", n, res[0], res[1], kb);
    }
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    std::printf("\nSurecin en yuksek bellek kullanimi: %.1f MB\n", ru.ru_maxrss / 1024.0);
    return 0;
}
