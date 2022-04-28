#include "CPUTopology.hpp"

#include <iostream>

int main() {
    auto topology = getSysCPUTopology();
    for (size_t i = 0; i < topology.getCPUCount(); i++) {
        std::cout << topology.selectCPU() << "\n";
    }
}
