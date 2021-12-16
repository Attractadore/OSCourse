cmake_minimum_required(VERSION 3.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)

project(OSCourse C)

add_subdirectory(send)
add_subdirectory(nprocesses)
add_subdirectory(semaphores)
add_subdirectory(pipeline)
