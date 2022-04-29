#include "MonteCarloIntegrator.hpp"
#include "CPUTopology.hpp"

#include <iostream>
#include <sstream>
#include <thread>

float launchMonteCarlo(size_t n) {
    constexpr float l = 0, r = 1;
    static thread_local auto f = [](float x) { return std::exp(x*x*x); };
    static thread_local auto pdf = [&](float x) {
        return (x >= l and x <= r) ? (1 / (r - l)) : 0.0f;
    };
    static thread_local auto icdf = [&](float cdf) {
        return l + (r - l) * cdf;
    };

    float s = 0.0f;
    constexpr size_t int_sz = 10'000;
    size_t k = n / int_sz;
    for (size_t i = 0; i < k; i++) {
        s += monteCarloIntegrate(f, pdf, icdf, int_sz);
    }

    return s / k;
}

float scheduleMonteCarlo(size_t n, size_t n_threads) {
    auto thread_n = n / n_threads;
    std::cout << "Thread work: " << thread_n << "\n";
    std::cout << "Total work: " << thread_n * n_threads << "\n";

    auto topology = getSysCPUTopology();
    std::cout << topology.getCPUCount() << " CPUs\n";

    for (size_t i = 0; i < topology.getCPUCount(); i++) {
        const auto& cpu = topology.selectCPU();

        auto [first, second] = cpu.siblings;

        auto t = std::thread([=] {
            cpu_set_t msk;
            CPU_ZERO(&msk);
            CPU_SET(first, &msk);
            CPU_SET(second, &msk);
            pthread_setaffinity_np(pthread_self(), sizeof(msk), &msk);
            launchMonteCarlo(1);
        });
        t.detach();
    }

    constexpr auto step = 1024;
    std::vector<float> results(n_threads * step);
    std::vector<std::thread> threads(n_threads);
    for (size_t i = 0; i < n_threads; i++) {
        auto& t = threads[i];
        auto out = &results[i * step];
        t = std::thread([=] {
            *out = launchMonteCarlo(thread_n);
        });

        const auto& cpu = topology.selectCPU();
        cpu_set_t msk;
        CPU_ZERO(&msk);
        CPU_SET(cpu.siblings.first, &msk);
        CPU_SET(cpu.siblings.second, &msk);
        pthread_setaffinity_np(t.native_handle(), sizeof(msk), &msk);
    }

    for (auto& t: threads) {
        t.join();
    }

    float s = 0.0f;
    for (size_t i = 0; i < n_threads; i++) {
        s += results[i * step];
    }
    return s / n_threads;
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
