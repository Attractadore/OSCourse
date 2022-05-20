cmake_minimum_required(VERSION 3.8)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

project(ParallelWorkloads)

add_library(CPUTopology 
    CPUTopology.hpp
    CPUTopology.cpp
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

add_executable(TrapezoidMulticore
    TrapezoidMulticore.cpp
)
target_link_libraries(TrapezoidMulticore
    ScheduleTrapezoid
)

add_library(NetworkClient
    NetworkClient.c NetworkCommon.c
)
target_compile_definitions(NetworkClient
    PUBLIC NETDEBUG=1
)

add_executable(TrapezoidClient
    TrapezoidClient.cpp
)
target_link_libraries(TrapezoidClient
    ScheduleTrapezoid
    NetworkClient
)

add_library(NetworkServer
    NetworkServer.c NetworkCommon.c
)
target_compile_definitions(NetworkServer
    PUBLIC NETDEBUG=1
)

add_executable(TrapezoidServer
    TrapezoidServer.cpp
)
target_link_libraries(TrapezoidServer
    ScheduleTrapezoid
    NetworkServer
)
