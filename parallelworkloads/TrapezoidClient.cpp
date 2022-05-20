#include "NetworkClient.h"

#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
    argc--;
    argv++;

    constexpr auto l = 0.0f, r = 1000000.0f;
    size_t n = 5'000'000'000;
    if (argv[0]) {
        std::stringstream ss(argv[0]);
        ss >> n;
    }

    IntegrationRequest ireq { l, r, n };
    IntegrationResponse iresp;
    if (auto r = ClientSend(&ireq, &iresp)) {
        std::cerr << "Failed to recieve integration request response\n";
        return -1;
    }

    std::cout << iresp.ival << "\n";
}
