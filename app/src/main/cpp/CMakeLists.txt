cmake_minimum_required(VERSION 3.22.1)
project("stethox")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
add_library(${CMAKE_PROJECT_NAME} SHARED stethox.cpp plt.c classloader.cpp utils.cpp)

target_link_libraries(${CMAKE_PROJECT_NAME} android log)
