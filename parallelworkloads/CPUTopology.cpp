#include "CPUTopology.hpp"

#include <thread>
#include <fstream>
#include <sstream>

std::string readThreadSiblingsList(cpu_id_t cpu_id) {
    auto sbuf = "/sys/devices/system/cpu/cpu" +
                std::to_string(cpu_id) +
                "/topology/thread_siblings_list";
    std::ifstream f(sbuf, std::ios::binary);
    constexpr auto rsz = 4096 - 1;
    sbuf.resize(rsz);
    f.read(sbuf.data(), rsz);
    auto sz = f.gcount();
    assert(sz < rsz);
    sbuf.resize(sz);
    std::replace(sbuf.begin(), sbuf.end(), ',', ' ');
    return sbuf;
}

std::pair<cpu_id_t, cpu_id_t> parseCPUSiblings(std::string s) {
    std::stringstream ss(std::move(s));
    std::pair<cpu_id_t, cpu_id_t> siblings;
    ss >> siblings.first;
    siblings.second = siblings.second;
    ss >> siblings.second;
    return siblings;
}

std::vector<CPU> getSysCPUs() {
    std::vector<CPU> cpus(std::thread::hardware_concurrency());
    for (size_t i = 0; i < cpus.size(); i++) {
        auto tsl_contents = readThreadSiblingsList(i);
        cpus[i].id = i;
        cpus[i].siblings = parseCPUSiblings(std::move(tsl_contents));
    }
    return cpus;
}

CPUTopology getSysCPUTopology() {
    auto cpus = getSysCPUs();
    return CPUTopology(cpus.begin(), cpus.end());
}
