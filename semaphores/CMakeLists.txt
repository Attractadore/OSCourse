add_library(SemCommon common.h common.c)

add_executable(SemClient client.c)
target_link_libraries(SemClient SemCommon)
target_compile_definitions(SemClient PRIVATE _GNU_SOURCE)

add_executable(SemServer server.c common.c)
target_link_libraries(SemServer SemCommon)
target_compile_definitions(SemServer PRIVATE _GNU_SOURCE)

if (DEBUG_PRINT)
    target_compile_definitions(SemClient PRIVATE DEBUG)
    target_compile_definitions(SemServer PRIVATE DEBUG)
endif()
