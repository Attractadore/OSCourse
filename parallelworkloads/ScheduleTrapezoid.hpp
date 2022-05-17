#pragma once
#include "CPUTopology.hpp"
#include "TrapezoidIntegrator.hpp"

float launchIntegrate(float a, float b, size_t n);

float scheduleIntegrate(
    float l, float r,
    size_t n, size_t n_threads,
    const CPUTopology& topology = getSysCPUTopology()
);
