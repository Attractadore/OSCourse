cmake_minimum_required(VERSION 3.0)
project(ParallelWorkloads)

add_library(MonteCarloIntegrator
    INTERFACE MonteCarloIntegrator.hpp
)

add_library(CPUTopology 
    CPUTopology.hpp
    CPUTopology.cpp
)
target_compile_features(CPUTopology
    PRIVATE cxx_std_17
)

add_executable(PrintCPUs
    PrintCPUs.cpp
)
target_link_libraries(PrintCPUs CPUTopology)

add_executable(MonteCarloMulticore
    MonteCarloMulticore.cpp
)
target_link_libraries(MonteCarloMulticore
    MonteCarloIntegrator
    CPUTopology
)
