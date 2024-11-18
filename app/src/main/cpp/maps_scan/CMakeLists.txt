project(maps_scan)

add_library(maps_scan STATIC maps_scan.cpp)
target_include_directories(maps_scan PUBLIC include)
