cmake_minimum_required(VERSION 3.8)
project(LinkedList)

add_library(LinkedList
    LinkedList.c
    LinkedList.h
)
target_include_directories(LinkedList INTERFACE .)
target_compile_features(LinkedList PRIVATE c_std_11)

target_compile_options(LinkedList PRIVATE --coverage)
target_link_options(LinkedList PUBLIC --coverage)

include(CTest)
add_subdirectory(test)

add_custom_target(coverage
    ${CMAKE_CTEST_COMMAND} -T Test && gcov -m "$$(" find -name *.gcda ")"
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)

add_custom_target(coverage-report
    ${CMAKE_COMMAND} -E cat *.gcov
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    DEPENDS coverage
)

add_custom_target(memcheck
    ${CMAKE_CTEST_COMMAND} -T MemCheck
)
