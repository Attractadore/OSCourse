#pragma once
#include <cassert>
#include <cstddef>
#include <random>

template<typename F, typename PDF, typename iCDF>
float monteCarloIntegrate(F f, PDF pdf, iCDF icdf, size_t n) {
    float s = 0.0f;
    std::random_device rd{};
    std::mt19937 gen{rd()};

    std::uniform_real_distribution<float> udist{0.0f, 1.0f};

    for (size_t i = 0; i < n; i++) {
        float u = udist(gen);
        float x = icdf(u);
        s += f(x) / pdf(x);
    }

    return s / n;
}
