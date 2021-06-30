cmake_minimum_required (VERSION 3.9)                  # CMake minimum version
project(lsoracle)

include_directories(../lib/kahypar/include)
#include_directories(${PROJECT_SOURCE_DIR}/lib/Galois/lonestar)

include_directories(${PROJECT_SOURCE_DIR}/algorithms/classification/fplus/include)
include_directories(${PROJECT_SOURCE_DIR}/algorithms/classification/eigen)
include_directories(${PROJECT_SOURCE_DIR}/algorithms/classification/json/include)
include_directories(${PROJECT_SOURCE_DIR}/algorithms/classification/fdeep_keras/include)

set(CMAKE_CXX_STANDARD 17)                            # enable c++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(lsoracle lsoracle.cpp)
target_include_directories(lsoracle PRIVATE ../lib/kahypar/include)

add_library(liblsoracle lsoracle.cpp)
target_include_directories(liblsoracle INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(liblsoracle INTERFACE ../lib/kahypar/include)
set_target_properties(liblsoracle PROPERTIES PREFIX "")

target_link_libraries(lsoracle alice mockturtle kahypar)
target_link_libraries(liblsoracle alice mockturtle kahypar)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0 OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(lsoracle stdc++fs)
  target_link_libraries(lsoracle-lib stdc++fs)
endif()
