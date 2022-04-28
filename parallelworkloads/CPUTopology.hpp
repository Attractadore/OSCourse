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
    std::vector<unsigned> m_cpus; 
    size_t m_id = 0;

public:
    template<typename I>
    CPUTopology(I first, I last);

    cpu_id_t selectCPU();
    size_t getCPUCount() const;
};

template<typename I>
CPUTopology::CPUTopology(I first, I last) {
    std::vector<CPU> cpus(first, last);   
    auto cpu_is_main = [](const CPU& cpu) {
        assert(cpu.id == cpu.siblings.first or cpu.id == cpu.siblings.second);
        return cpu.id == cpu.siblings.first;
    };
    auto cpu_comp = [&](const CPU& l, const CPU& r) {
        int l_is_main = cpu_is_main(l);
        int r_is_main = cpu_is_main(r);
        return (l_is_main != r_is_main)? l_is_main > r_is_main: l.id < r.id;
    };
    std::sort(cpus.begin(), cpus.end(), cpu_comp);
    m_cpus.resize(cpus.size());
    std::transform(cpus.begin(), cpus.end(), m_cpus.begin(), [](const CPU& cpu) { return cpu.id; });
}

inline cpu_id_t CPUTopology::selectCPU() {
    auto id = m_cpus[m_id];
    m_id = (m_id + 1) % m_cpus.size();
    return id;
}

inline size_t CPUTopology::getCPUCount() const {
    return m_cpus.size();
}

std::vector<CPU> getSysCPUs();
CPUTopology getSysCPUTopology();
