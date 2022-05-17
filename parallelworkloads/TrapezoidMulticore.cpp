#include "ScheduleTrapezoid.hpp"

#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
    argc--;
    argv++;

    auto n_threads = [&] {
        size_t n_threads = 0;
        std::stringstream ss(argv[0]);
        ss >> n_threads;
        return n_threads;
    } ();

    size_t n = 5'000'000'000;
    if (argv[1]) {
        std::stringstream ss(argv[1]);
        ss >> n;
    }

    constexpr auto l = 0.0f, r = 1000000.0f;
    std::cout << scheduleIntegrate(l, r, n, n_threads) << "\n";
}
