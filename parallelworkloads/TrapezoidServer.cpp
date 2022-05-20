#include "ScheduleTrapezoid.hpp"
#include "NetworkServer.h"

#include <iostream>
#include <optional>
#include <sstream>
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

std::optional<IntegrationResponse> GenerateIntegrationResponse(
    const IntegrationRequest& ireq, size_t n_threads, const CPUTopology& topology
) {
    auto [l, r, n] = ireq;
    try {
        float ival =  scheduleIntegrate(l, r, n, n_threads, topology);
        return IntegrationResponse{ival};
    } catch (std::system_error& e) {
        return std::nullopt;
    }
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
    ServerLoop(linfo, dinfo, n_threads);
    return 0;
}
