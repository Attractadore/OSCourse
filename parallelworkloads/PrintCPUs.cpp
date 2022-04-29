#include "CPUTopology.hpp"

#include <iostream>

int main() {
    auto topology = getSysCPUTopology();
    for (size_t i = 0; i < topology.getCPUCount(); i++) {
        const auto& cpu = topology.selectCPU();
        std::cout << "CPU " << cpu.id << "\n";
        std::cout << "Siblings " << cpu.siblings.first << ", " << cpu.siblings.second << "\n";
    }
}
