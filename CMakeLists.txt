cmake_minimum_required(VERSION 2.6)
project(milo_test C)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ansi -pedantic -Wall")
endif()

add_library(milo milo.c)
add_executable(milo_test test.c)

target_link_libraries(milo_test milo)
