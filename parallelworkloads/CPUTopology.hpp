#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

using cpu_id_t = unsigned;

struct CPU {
    cpu_id_t id;
};

struct SMTCPU {
    cpu_id_t id;
    cpu_id_t sibling;
};

class CPUTopology {
    std::vector<CPU> m_cpus;
    std::vector<SMTCPU> m_smtcpus;

public:
    template<typename I, typename SMTI>
    CPUTopology(I first, I last, SMTI smtfirst, SMTI smtlast):
        m_cpus(first, last),
        m_smtcpus(smtfirst, smtlast) {}

    auto getCPUCount() const {
        return m_cpus.size();
    }

    const auto& getCPU(size_t i) const {
        return m_cpus[i];
    }

    auto begin() const {
        return m_cpus.begin();
    }

    auto end() const {
        return m_cpus.end();
    }

    auto getSMTCPUCount() const {
        return m_smtcpus.size();
    }

    const auto& getSMTCPU(size_t i) const {
        return m_smtcpus[i];
    }

    auto smtbegin() const {
        return m_smtcpus.begin();
    }

    auto smtend() const {
        return m_smtcpus.end();
    }

    auto getThreadCount() const {
        return getCPUCount() + 2 * getSMTCPUCount();
    }
};

CPUTopology getSysCPUTopology();

using work_dist_t = std::pair<std::pair<cpu_id_t, cpu_id_t>, size_t>;

std::vector<work_dist_t> distributeWork(
    const CPUTopology& topology, size_t num_work_items, size_t num_threads
);
