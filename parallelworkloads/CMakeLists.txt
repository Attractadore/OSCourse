cmake_minimum_required(VERSION 3.8)
project(ParallelWorkloads)

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

add_library(TrapezoidIntegrator
    INTERFACE TrapezoidIntegrator.hpp
)

add_library(ScheduleTrapezoid
    ScheduleTrapezoid.cpp
)
target_link_libraries(ScheduleTrapezoid
    PUBLIC  CPUTopology
    PRIVATE TrapezoidIntegrator
)
target_compile_features(ScheduleTrapezoid
    PRIVATE cxx_std_17
)

add_executable(TrapezoidMulticore
    TrapezoidMulticore.cpp
)
target_link_libraries(TrapezoidMulticore
    ScheduleTrapezoid
    CPUTopology
)
