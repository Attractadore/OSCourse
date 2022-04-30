#include "CPUTopology.hpp"

#include <iostream>

int main() {
    auto topology = getSysCPUTopology();

    auto num_cpus = topology.getCPUCount();
    if (num_cpus) {
        std::cout << num_cpus << " CPUs\n";
    }
    for (auto i = topology.begin(), e = topology.end(); i != e; ++i) {
        std::cout << i->id << "\n";
    }

    auto num_smtcpus = topology.getSMTCPUCount();
    if (num_smtcpus) {
        std::cout << num_smtcpus << " SMT CPUs\n";
    }
    for (auto i = topology.smtbegin(), e = topology.smtend(); i != e; ++i) {
        std::cout << i->id << ", " << i->sibling << "\n";
    }
}
