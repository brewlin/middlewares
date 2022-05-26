include_directories(. ./)

aux_source_directory(. dir_src)

add_library(lib ${dir_src})