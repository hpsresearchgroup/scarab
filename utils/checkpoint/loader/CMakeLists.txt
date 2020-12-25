cmake_minimum_required(VERSION 3.5.0)

project(scarab_checkpoint_loader CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(warn_c_flags -Wall -Wunused -Wmissing-prototypes -Wmissing-declarations -Wno-long-long -Wpointer-arith -Wimplicit -Werror)
set(warn_cxx_flags -Wall -Wunused -Wmissing-declarations -Wno-long-long -Wpointer-arith -Werror)

add_library(loader_lib
        ptrace_interface.cc ptrace_interface.h
        checkpoint_reader.cc checkpoint_reader.h
        gtree.c gtree.h
        hconfig.c hconfig.h
        read_mem_map.cc read_mem_map.h
        utils.cc utils.h
)
target_compile_options(loader_lib
    PUBLIC
        "$<$<COMPILE_LANGUAGE:C>:${warn_c_flags}>"
        "$<$<COMPILE_LANGUAGE:CXX>:${warn_cxx_flags}>"
)
target_compile_definitions(loader_lib 
    PUBLIC
        "$<$<CONFIG:RELEASE>:DEBUG_EN=0>"
        "$<$<NOT:$<CONFIG:RELEASE>>:DEBUG_EN=1>"
)

add_executable(test_memcpy
    test_memcpy.cc 
)
target_link_libraries(test_memcpy PUBLIC loader_lib -static)

add_executable(checkpoint_loader
    checkpoint_loader.cc 
)
target_link_libraries(checkpoint_loader PUBLIC loader_lib)