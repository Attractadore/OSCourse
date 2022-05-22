#include "ScheduleTrapezoid.hpp"

#include <cmath>
#include <thread>
#include <system_error>

float launchIntegrate(float a, float b, size_t n) {
    auto f = [](float x) { return std::exp(-x*x/2.0f); };
    return trapezoidIntegrate(f, a, b , n);
}

float scheduleIntegrate(
    float l, float r,
    size_t n, size_t n_threads,
    const CPUTopology& topology
) {
    constexpr auto step = 1024;
    std::vector<float> results(n_threads * step);
    auto dist = distributeWork(topology, n, n_threads);
    std::vector<std::thread> threads(dist.size());

    auto thread_cnt = topology.getThreadCount();

    try {
    for (size_t i = 0, current_n = 0; i < threads.size(); i++) {
        auto ids = std::get<0>(dist[i]);
        auto thread_n = std::get<1>(dist[i]);

        auto out = &results[i * step];

        threads[i] = std::thread([=] {
            if (n <= thread_cnt * 10) {
                cpu_set_t msk;
                CPU_ZERO(&msk);
                CPU_SET(ids.first, &msk);
                CPU_SET(ids.second, &msk);
                pthread_setaffinity_np(pthread_self(), sizeof(msk), &msk);
            }
            float a = l + (r - l) * current_n / n;
            float b = a + (r - l) * thread_n / n;
            *out = launchIntegrate(a, b, thread_n);
        });

        current_n += thread_n;
    }
    } catch (const std::system_error& e) {
        for (auto& t: threads) {
            if (pthread_t pt = t.native_handle()) {
                t.detach();
                pthread_cancel(pt);
            }
        }
        throw e;
    }

    float s = 0.0f;
    for (size_t i = 0; i < n_threads; i++) {
        threads[i].join();
        s += results[i * step];
    }

    return s;
}
