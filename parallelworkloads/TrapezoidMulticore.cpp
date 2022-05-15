#include "TrapezoidIntegrator.hpp"
#include "CPUTopology.hpp"

#include <cmath>
#include <iostream>
#include <sstream>
#include <thread>

float launchIntegrate(float a, float b, size_t n) {
    auto f = [](float x) { return std::exp(-x*x/2.0f); };
    return trapezoidIntegrate(f, a, b , n);
}

float scheduleIntegrate(float l, float r, size_t n, size_t n_threads) {
    auto topology = getSysCPUTopology();

    constexpr auto step = 1024;
    std::vector<float> results(n_threads * step);
    auto dist = distributeWork(topology, n, n_threads);
    std::vector<std::thread> threads(dist.size());

    for (size_t i = 0, current_n = 0; i < threads.size(); i++) {
        auto [ids, thread_n] = dist[i];
        auto out = &results[i * step];

        threads[i] = std::thread([=] {
            cpu_set_t msk;
            CPU_ZERO(&msk);
            CPU_SET(ids.first, &msk);
            CPU_SET(ids.second, &msk);
            pthread_setaffinity_np(pthread_self(), sizeof(msk), &msk);
            float a = l + (r - l) * current_n / n;
            float b = a + (r - l) * thread_n / n;
            *out = launchIntegrate(a, b, thread_n);
        });

        current_n += thread_n;
    }

    float s = 0.0f;
    for (size_t i = 0; i < n_threads; i++) {
        threads[i].join();
        s += results[i * step];
    }

    return s;
}

int main(int argc, char* argv[]) {
    argc--;
    argv++;

    auto n_threads = [&] {
        size_t n_threads = 0;
        std::stringstream ss(argv[0]);
        ss >> n_threads;
        return n_threads;
    } ();

    size_t n = 5'000'000'000;
    if (argv[1]) {
        std::stringstream ss(argv[1]);
        ss >> n;
    }

    constexpr auto l = 0.0f, r = 1000000.0f;
    std::cout << scheduleIntegrate(l, r, n, n_threads) << "\n";
}