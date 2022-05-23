#include "ScheduleTrapezoid.hpp"
#include "NetworkServer.h"

#include <iostream>
#include <sstream>
#include <system_error>
#include <thread>

namespace {
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

int ServerLoop(const ListenInfo& linfo, const DiscoveryInfo& dinfo, size_t n_threads) {
    StartDiscoveryService(dinfo);
    auto topology = getSysCPUTopology();

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
    dinfo.response.thread_count = n_threads;
    ServerLoop(linfo, dinfo, n_threads);
    return 0;
}
