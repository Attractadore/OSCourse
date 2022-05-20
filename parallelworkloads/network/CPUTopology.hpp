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
    using CPUIterator = decltype(m_cpus)::const_iterator;
    using SMTCPUIterator = decltype(m_smtcpus)::const_iterator;

    template<typename I, typename SMTI>
    CPUTopology(I first, I last, SMTI smtfirst, SMTI smtlast):
        m_cpus(first, last),
        m_smtcpus(smtfirst, smtlast) {}

    size_t getCPUCount() const {
        return m_cpus.size();
    }

    const CPU& getCPU(size_t i) const {
        return m_cpus[i];
    }

    CPUIterator begin() const {
        return m_cpus.begin();
    }

    CPUIterator end() const {
        return m_cpus.end();
    }

    size_t getSMTCPUCount() const {
        return m_smtcpus.size();
    }

    const SMTCPU& getSMTCPU(size_t i) const {
        return m_smtcpus[i];
    }

    SMTCPUIterator smtbegin() const {
        return m_smtcpus.begin();
    }

    SMTCPUIterator smtend() const {
        return m_smtcpus.end();
    }

    size_t getThreadCount() const {
        return getCPUCount() + 2 * getSMTCPUCount();
    }
};

CPUTopology getSysCPUTopology();

using work_dist_t = std::pair<std::pair<cpu_id_t, cpu_id_t>, size_t>;

std::vector<work_dist_t> distributeWork(
    const CPUTopology& topology, size_t num_work_items, size_t num_threads
);
