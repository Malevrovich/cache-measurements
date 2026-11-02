cmake_minimum_required(VERSION 3.16)
project(l1_cache LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(l1-cache
  src/main.cpp
  src/memory.cpp
  src/measurement.cpp
  src/experiments.cpp
)

target_compile_features(l1-cache PRIVATE cxx_std_20)
target_compile_options(l1-cache PRIVATE -Wall -Wextra -Wpedantic)
