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
    auto topology = getSysCPUTopology();
    if (auto smtcpu_count = topology.getSMTCPUCount(); smtcpu_count) {
        std::cout << smtcpu_count << " SMT CPUs\n";
    }
    if (auto cpu_count = topology.getCPUCount(); cpu_count) {
        std::cout << cpu_count << " CPUs\n";
    }

    auto warm_up = [] (const cpu_set_t& msk) {
        pthread_setaffinity_np(pthread_self(), sizeof(msk), &msk);
        launchMonteCarlo(1);
    };

    for (auto i = topology.begin(), e = topology.end(); i != e; ++i) {
        auto id = i->id;
        auto t = std::thread([=] {
            cpu_set_t msk;
            CPU_ZERO(&msk);
            CPU_SET(id, &msk);
            warm_up(msk);
        });
        t.detach();
    }
    for (auto i = topology.smtbegin(), e = topology.smtend(); i != e; ++i) {
        auto id = i->id;
        auto sid = i->sibling;
        auto t = std::thread([=] {
            cpu_set_t msk;
            CPU_ZERO(&msk);
            CPU_SET(id, &msk);
            CPU_SET(sid, &msk);
            warm_up(msk);
        });
        t.detach();
    }

    constexpr auto step = 1024;
    std::vector<float> results(n_threads * step);
    auto dist = distributeWork(topology, n, n_threads);
    std::vector<std::thread> threads(dist.size());
    for (size_t i = 0; i < threads.size(); i++) {
        auto [ids, thread_n] = dist[i];
        auto out = &results[i * step];

        threads[i] = std::thread([=] {
            cpu_set_t msk;
            CPU_ZERO(&msk);
            CPU_SET(ids.first, &msk);
            CPU_SET(ids.second, &msk);
            pthread_setaffinity_np(pthread_self(), sizeof(msk), &msk);
            *out = launchMonteCarlo(thread_n);
        });
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
