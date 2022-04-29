#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

using cpu_id_t = unsigned;

struct CPU {
    cpu_id_t id;
    std::pair<cpu_id_t, cpu_id_t> siblings;
};

class CPUTopology {
    std::vector<CPU> m_cpus;
    size_t m_id = 0;

public:
    template<typename I>
    CPUTopology(I first, I last);

    const CPU& selectCPU();
    size_t getCPUCount() const;
};

template<typename I>
CPUTopology::CPUTopology(I first, I last) {
    auto cpu_is_main = [](const CPU& cpu) {
        assert(cpu.id == cpu.siblings.first or cpu.id == cpu.siblings.second);
        return cpu.id == cpu.siblings.first;
    };
    m_cpus.reserve(128);
    std::copy_if(first, last, std::back_inserter(m_cpus), cpu_is_main);
}

inline const CPU& CPUTopology::selectCPU() {
    const auto& cpu = m_cpus[m_id];
    m_id = (m_id + 1) % m_cpus.size();
    return cpu;
}

inline size_t CPUTopology::getCPUCount() const {
    return m_cpus.size();
}

std::vector<CPU> getSysCPUs();
CPUTopology getSysCPUTopology();
