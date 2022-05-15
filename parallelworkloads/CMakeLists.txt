cmake_minimum_required(VERSION 3.8)
project(ParallelWorkloads)

add_library(MonteCarloIntegrator
    INTERFACE MonteCarloIntegrator.hpp
)
add_library(TrapezoidIntegrator
    INTERFACE TrapezoidIntegrator.hpp
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
target_compile_features(MonteCarloMulticore
    PRIVATE cxx_std_17
)

add_executable(TrapezoidMulticore
    TrapezoidMulticore.cpp
)
target_link_libraries(TrapezoidMulticore
    TrapezoidIntegrator
    CPUTopology
)
target_compile_features(TrapezoidMulticore
    PRIVATE cxx_std_17
)
