include_directories(. ./)

aux_source_directory(. dir_src)

add_library(co-lib ${dir_src})