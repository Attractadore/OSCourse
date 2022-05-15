#pragma once
#include <cstddef>

#include <iostream>

template<typename F>
float trapezoidIntegrate(F f, float a, float b, size_t n) {
    float s = 0.0f;
    auto step = (b - a) / n;
    float x = a;
    auto f_c = f(x);
    for (size_t i = 0; i < n; i++) {
        x += step;
        auto f_n = f(x);
        s += (f_n + f_c) / 2.0f * step;
        f_c = f_n;
    }
    return s;
}
