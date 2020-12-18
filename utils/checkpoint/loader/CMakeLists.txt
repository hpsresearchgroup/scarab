cmake_minimum_required(VERSION 3.5.0)

project(scarab_checkpoint_loader CXX C)

add_executable(
    test_memcpy
    test_memcpy.cc 
    ptrace_interface.cc
    checkpoint_reader.cc
    gtree.c
    hconfig.c
    read_mem_map.cc
    utils.cc)
target_compile_features(test_memcpy PRIVATE cxx_std_17)