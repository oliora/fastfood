cmake_minimum_required(VERSION 3.1)

project(fastfood VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wpedantic -Wall")
endif()

# Boost.Fusion supports std::tuple starting from Boost 1.58.
find_package(Boost 1.58 REQUIRED COMPONENTS system program_options thread)

include_directories(${Boost_INCLUDE_DIRS})

enable_testing()

add_subdirectory(src)
add_subdirectory(test)
