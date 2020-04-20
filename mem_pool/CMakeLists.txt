project(demo)

add_definitions("-Wall -g")

include_directories(./thirdparty/include)

add_subdirectory(./thirdparty/)

add_executable(demo main.c)

target_link_libraries(demo mempool)