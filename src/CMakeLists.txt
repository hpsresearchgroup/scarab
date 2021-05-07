cmake_minimum_required (VERSION 3.5)

project(scarab C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_SCARABOPT "-O3 -DNO_DEBUG")
set(CMAKE_CXX_FLAGS_VALGRIND "-O0 -g3")
set(CMAKE_CXX_FLAGS_GPROF  "${CMAKE_CXX_FLAGS_SCARABOPT} -pg -g3")

set(warn_flags -Wall -Wunused -Wno-long-long -Wpointer-arith -Werror)
set(warn_c_flags ${warn_flags} -Wmissing-declarations -Wmissing-prototypes -Wimplicit -Wno-unused-but-set-variable -Wno-maybe-uninitialized)
set(warn_cxx_flags ${warn_flags})

add_compile_options(
        "$<$<COMPILE_LANGUAGE:C>:${warn_c_flags}>"
        "$<$<COMPILE_LANGUAGE:CXX>:${warn_cxx_flags}>"
)

add_subdirectory(ramulator)
add_subdirectory(pin/pin_lib)

set(scarab_dirs bp debug bp/template_lib dvfs frontend globals isa libs memory power prefetcher .)

set(srcs)
foreach(dir IN LISTS scarab_dirs) 
    file(GLOB dir_srcs
      ${dir}/*.h
      ${dir}/*.cc
      ${dir}/*.c
    )
    set(srcs ${srcs} ${dir_srcs})
endforeach()

add_executable(scarab 
    ${srcs}
)
target_include_directories(scarab PRIVATE .) 
target_link_libraries(scarab
    PRIVATE
        ramulator
        pin_lib_for_scarab
)