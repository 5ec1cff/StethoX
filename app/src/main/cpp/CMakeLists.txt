cmake_minimum_required(VERSION 3.22.1)
project("stethox")

include_directories(include)

find_package(cxx REQUIRED CONFIG)
link_libraries(cxx::cxx)

add_library(${CMAKE_PROJECT_NAME} SHARED stethox.cpp classloader.cpp utils.cpp jvmti/stethox_jvmti.cpp art.cpp reflection.cpp)

target_link_libraries(${CMAKE_PROJECT_NAME} log elf_parser maps_scan)
add_subdirectory(elf_parser)
add_subdirectory(maps_scan)
