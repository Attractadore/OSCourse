#include "ScheduleTrapezoid.hpp"
#include "NetworkServer.h"

#include <iostream>
#include <sstream>
#include <system_error>
#include <thread>

namespace {
double RunBenchmark(size_t n_threads, const CPUTopology& topology) {
    NetDebugPrint("Start throughput benchmark\n");
    size_t n = 100 * 1000 * 1000;
    double t, d;
    do {
        auto t0 = std::chrono::steady_clock::now();
        float l = 0.0f;
        float r = (l + n) / 1000.0f;
        scheduleIntegrate(l, r, n, n_threads, topology);
        auto t1 = std::chrono::steady_clock::now();
        d = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1e9;
        t = n / d;
        n *= 2;
    } while(d < 1.0);
    NetDebugPrint("Finished throughput benchmark\n");
    return t;
}

void StartDiscoveryService(const DiscoveryInfo& dinfo) {
    std::thread([dinfo] {
        bool quit = false;
        while(!quit) {
            if (auto r = ServerProcessDiscoveryRequest(&dinfo)) {
                auto msg = "Failed to process discovery request";
                if (r < 0) {
                    std::cerr << "FATAL: " << msg << "\n";
                    exit(-1);
                } else {
                    std::cerr << "TEMP: " << msg << "\n";
                    continue;
                }
            }
        }
    }).detach();
}

struct IntegrationResponseGen {
    IntegrationResponse iresp;
    bool valid = false;

    explicit operator bool() const {
        return valid;
    }

    IntegrationResponse& operator*() {
        return iresp;
    }
};

IntegrationResponseGen GenerateIntegrationResponse(
    const IntegrationRequest& ireq, size_t n_threads, const CPUTopology& topology
) {
    float l = ireq.l;
    float r = ireq.r;
    size_t n = ireq.n;
    IntegrationResponseGen ret;
    try {
        float ival =  scheduleIntegrate(l, r, n, n_threads, topology);
        ret.valid = true;
        ret.iresp.ival = ival;
    } catch (const std::system_error& e) {}
    return ret;
}

int ServerLoop(const ListenInfo& linfo, const DiscoveryInfo& dinfo, size_t n_threads, const CPUTopology& topology) {
    StartDiscoveryService(dinfo);

    bool quit = false;
    while(!quit) {
        RequestInfo req_info;
        if (auto r = ServerRecieve(&linfo, &req_info)) {
            auto msg = "Failed to accept integration request";
            if (r < 0) {
                std::cerr << "FATAL: " << msg << "\n";
                return -1;
            } else {
                std::cerr << "TEMP: " << msg << "\n";
                continue;
            }
        }

        auto iresp = GenerateIntegrationResponse(
            req_info.integration_request, n_threads, topology
        );
        if (!iresp) {
            ServerRespondError(&req_info);
            std::cerr << "FATAL: Failed to integrate\n";
            return -1;
        }

        ResponseInfo resp_info = {
            *iresp, req_info.socket
        };
        if (auto r = ServerRespond(&resp_info)) {
            auto msg = "Failed to send integration response";
            if (r < 0) {
                std::cerr << "FATAL: " << msg << "\n";
                return -1;
            } else {
                std::cerr << "TEMP: " << msg << "\n";
                continue;
            }
        }
    }

    return 0;
}
}

int main(int argc, char* argv[]) {
    argc--;
    argv++;

    auto n_threads = [&] {
        size_t n_threads = 1;
        if (argv[0]) {
            std::stringstream ss(argv[0]);
            ss >> n_threads;
        }
        return n_threads;
    } ();

    ListenInfo linfo;
    if (ServerListen(CALCULATE_PORT_V, &linfo) < 0) {
        std::cerr << "FATAL: Failed to start listening on port " << CALCULATE_PORT << "\n";
        return -1;
    }
    DiscoveryInfo dinfo;
    if (ServerStartDiscovery(DISCOVER_PORT_V, &dinfo) < 0) {
        std::cerr << "FATAL: Failed to start discovery service on port " << DISCOVER_PORT << "\n";
        return -1;
    }
    auto topology = getSysCPUTopology();
    dinfo.response.props.thread_count = n_threads;
    try {
        dinfo.response.props.throughput = RunBenchmark(n_threads, topology);
    } catch (const std::system_error& e) {
        std::cerr << "FATAL: Failed to run throughput benchmark\n";
        return -1;
    }
    ServerLoop(linfo, dinfo, n_threads, topology);
    return 0;
}
