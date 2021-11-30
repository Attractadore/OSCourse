add_library(SemCommon common.h common.c)
target_compile_definitions(SemCommon PUBLIC _GNU_SOURCE)
if (DEBUG_PRINT)
    target_compile_definitions(SemCommon PUBLIC DEBUG)
endif()

add_executable(SemClient client.c)
target_link_libraries(SemClient SemCommon)

add_executable(SemServer server.c)
target_link_libraries(SemServer SemCommon)
