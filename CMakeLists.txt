cmake_minimum_required(VERSION 3.17 FATAL_ERROR)

project(eems LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(pugixml REQUIRED)
find_package(range-v3 REQUIRED)
find_package(toml11 REQUIRED)
find_package(Flatbuffers REQUIRED)
find_package(leveldb REQUIRED)
find_package(date REQUIRED)

# TODO: Implement proper detection.
add_library(std::coroutines INTERFACE IMPORTED)
#target_compile_options(std::coroutines INTERFACE -fcoroutines)

add_subdirectory(src)
