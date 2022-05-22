#include "CPUTopology.hpp"

#include <thread>
#include <fstream>
#include <sstream>

namespace {
using sibligs_list_t = std::pair<cpu_id_t, cpu_id_t>;

std::string readThreadSiblingsList(cpu_id_t cpu_id) {
    std::string sbuf = "/sys/devices/system/cpu/cpu" +
                std::to_string(cpu_id) +
                "/topology/thread_siblings_list";
    std::ifstream f(sbuf, std::ios::binary);
    constexpr auto rsz = 4096 - 1;
    sbuf.resize(rsz);
    f.read(&sbuf[0], rsz);
    auto sz = f.gcount();
    assert(sz < rsz);
    sbuf.resize(sz);
    std::replace(sbuf.begin(), sbuf.end(), ',', ' ');
    return sbuf;
}

sibligs_list_t parseCPUSiblings(std::string s) {
    std::stringstream ss(std::move(s));
    sibligs_list_t siblings;
    ss >> siblings.first;
    siblings.second = siblings.first;
    ss >> siblings.second;
    return siblings;
}

std::tuple<
    std::vector<CPU>, std::vector<SMTCPU>
> getSysCPUs() {
    std::vector<CPU> cpus;
    std::vector<SMTCPU> smtcpus;
    auto num_cpus = std::thread::hardware_concurrency();
    auto is_smt = [](const sibligs_list_t& sl) { return sl.first != sl.second; };
    auto is_main = [](cpu_id_t id, const sibligs_list_t& sl) { return id == sl.first; };
    auto get_sibling = [](cpu_id_t, const sibligs_list_t& sl) { return sl.second; };
    for (cpu_id_t id = 0; id < num_cpus; id++) {
        auto tsl_contents = readThreadSiblingsList(id);
        auto sl = parseCPUSiblings(std::move(tsl_contents));
        if (is_smt(sl)) {
            if (is_main(id, sl)) {
                SMTCPU cpu = {
                    .id = id,
                    .sibling = get_sibling(id, sl),
                };
                smtcpus.emplace_back(cpu);
            }
        }
        else {
            CPU cpu = {
                .id = id,
            };
            cpus.emplace_back(cpu);
        }
    }
    return std::tuple<decltype(cpus), decltype(smtcpus)>{std::move(cpus), std::move(smtcpus)};
}
}

CPUTopology getSysCPUTopology() {
    auto cpu_tup = getSysCPUs();
    auto& cpus = std::get<0>(cpu_tup);
    auto& smtcpus = std::get<1>(cpu_tup);
    return CPUTopology(cpus.begin(), cpus.end(), smtcpus.begin(), smtcpus.end());
}

// Schedule all work to smt cores, then non smt cores, then hyperthreads
std::vector<work_dist_t> distributeWork(
    const CPUTopology& topology, size_t work, size_t launch_threads
) {
    constexpr auto hthread_weight = 0.3;

    std::vector<work_dist_t> dist;
    dist.reserve(launch_threads);

    auto smt_count = topology.getSMTCPUCount();
    auto core_count = topology.getCPUCount();
    auto thread_count = topology.getThreadCount();

    auto distribute = [&](size_t num_threads) {
        auto smtcores = std::min(num_threads, smt_count);
        num_threads -= smtcores;
        auto cores = std::min(num_threads, core_count);
        num_threads -= cores;
        auto hthreads = num_threads;
        return std::tuple<decltype(smtcores), decltype(cores), decltype(hthreads)>{smtcores, cores, hthreads};
    };

    auto hw_threads = std::min(launch_threads, thread_count);
    auto ret1 = distribute(hw_threads);
    auto& active_smtcores = std::get<0>(ret1);
    auto& active_cores   = std::get<1>(ret1);
    auto& active_hthreads = std::get<2>(ret1);
    auto total_weight = (active_smtcores + active_cores) + hthread_weight * active_hthreads;

    auto base_threads = launch_threads / thread_count;
    auto leftover_threads = launch_threads % thread_count;
    auto ret2 = distribute(leftover_threads);
    auto& leftover_smtcores = std::get<0>(ret2);
    auto& leftover_cores   = std::get<1>(ret2);
    auto& leftover_hthreads = std::get<2>(ret2);

    for (size_t i = 0; i < smt_count; i++) {
        auto local_weight = 1 + hthread_weight * (i < active_hthreads);
        auto local_work = (local_weight / total_weight) * work;
        auto local_threads = 2 * base_threads + (i < leftover_smtcores) + (i < leftover_hthreads);
        if (local_threads == 0) break;
        auto local_thread_work = local_work / local_threads;
        const auto& cpu = topology.getSMTCPU(i);
        work_dist_t d = {
            {cpu.id, cpu.sibling},
            local_thread_work
        };
        for (size_t k = 0; k < local_threads; k++) {
            dist.push_back(d);
        }
    }

    for (size_t i = 0; i < core_count; i++) {
        auto local_work = work / total_weight;
        auto local_threads = base_threads + (i < leftover_cores);
        if (local_threads == 0) break;
        auto local_thread_work = local_work / local_threads;
        const auto& cpu = topology.getCPU(i);
        work_dist_t d = {
            {cpu.id, cpu.id},
            local_thread_work
        };
        for (size_t k = 0; k < local_threads; k++) {
            dist.push_back(d);
        }
    }

    return dist;
}
