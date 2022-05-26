cmake_minimum_required(VERSION 3.10)

project(demo)

add_definitions("-g")

include_directories(./src)

add_subdirectory(./src)

add_executable(demo main.c)

target_link_libraries(demo co-lib pthread)