#pragma once
#include <cassert>
#include <cstddef>
#include <random>

template<typename F, typename PDF, typename iCDF>
float monteCarloIntegrate(F f, PDF pdf, iCDF icdf, size_t n) {
    static thread_local std::random_device rd{};
    static thread_local std::mt19937 gen{rd()};
    static thread_local std::uniform_real_distribution<float> udist{0.0f, 1.0f};

    float s = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float u = udist(gen);
        float x = icdf(u);
        s += f(x) / pdf(x);
    }

    return s / n;
}
