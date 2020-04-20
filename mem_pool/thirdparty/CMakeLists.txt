file(WRITE ${CMAKE_BINARY_DIR}/tmp/test_posix_memalign.c "
    int main(){void *p;
        posix_memalign(p,16,16);
        return 0;
    }
")
file(WRITE ${CMAKE_BINARY_DIR}/tmp/test_memalign.c "
    int main(){
        void *p = memalign(16,16);
        return 0;
    }
")
try_compile(
    HAS_POSIX_MEMALIGN
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/tmp/test_posix_memalign.c 
)
try_compile(
    HAS_MEMALIGN
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/tmp/test_memalign.c 
)
if(HAS_MEMALIGN)
    add_definitions(-D HAS_MEMALIGN)
endif(HAS_MEMALIGN)

if(HAS_POSIX_MEMALIGN)
    add_definitions(-D HAS_POSIX_MEMALIGN)
endif(HAS_POSIX_MEMALIGN)

include_directories(. ./include)

add_library(mempool ./src/mem_pool.c)