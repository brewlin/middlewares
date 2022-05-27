include_directories(. ./)

add_definitions("-W -g")

aux_source_directory(. dir_src)

add_library(co-lib ${dir_src})