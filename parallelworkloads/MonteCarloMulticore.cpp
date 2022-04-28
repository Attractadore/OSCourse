#include "MonteCarloIntegrator.hpp"
#include "CPUTopology.hpp"

#include <iostream>
#include <sstream>
#include <thread>

float launchMonteCarlo(size_t n) {
    float l = 0, r = 1;
    auto f = [](float x) { return std::exp(x*x*x); };
    auto pdf = [&](float x) {
        return (x >= l and x <= r) ? (1 / (r - l)) : 0.0f;
    };
    auto icdf = [&](float cdf) {
        return l + (r - l) * cdf;
    };

    constexpr size_t int_sz = 10'000;
    size_t k = n / int_sz;
    float s = 0;
    for (size_t i = 0; i < k; i++) {
        s += monteCarloIntegrate(f, pdf, icdf, int_sz);
    }
    return s / k;
}

float scheduleMonteCarlo(size_t n, size_t n_threads) {
    auto thread_n = n / n_threads;
    std::cout << "Thread work: " << thread_n << "\n";
    std::cout << "Total work: " << thread_n * n_threads << "\n";

#define tplg
#ifdef tplg
    auto topology = getSysCPUTopology();
    std::cout << topology.getCPUCount() << " CPUs\n";
#endif

    std::vector<float> results(n_threads);
    std::vector<std::thread> threads(n_threads);
    for (size_t i = 0; i < n_threads; i++) {
        threads[i] = std::thread([=, &results] {
            results[i] = launchMonteCarlo(thread_n);
        });

#ifdef tplg
        auto id = topology.selectCPU();
        cpu_set_t msk;
        CPU_ZERO(&msk);
        CPU_SET(id, &msk);
        pthread_setaffinity_np(threads[i].native_handle(), sizeof(msk), &msk);
#endif
    }

    for (auto& t: threads) {
        t.join();
    }

    return std::accumulate(results.begin(), results.end(), 0.0f) / n_threads;
}

int main(int argc, char* argv[]) {
    argc--;
    argv++;

    size_t n_threads = 0;
    std::stringstream ss(argv[0]);
    ss >> n_threads;

    constexpr auto n = 1'000'000'000;
    std::cout << scheduleMonteCarlo(n, n_threads) << "\n";
}
