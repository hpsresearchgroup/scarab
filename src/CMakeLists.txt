cmake_minimum_required (VERSION 3.5)

project(scarab C CXX ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(enable_memtrace 0)
set(flags_enable_memtrace "")

if(DEFINED ENV{SCARAB_ENABLE_MEMTRACE})
  set(flags_enable_memtrace "-DENABLE_MEMTRACE")
endif()

set(CMAKE_C_FLAGS_SCARABOPT   "-O3 -DNO_DEBUG -DLINUX -DX86_64 ${flags_enable_memtrace}")
set(CMAKE_CXX_FLAGS_SCARABOPT "-O3 -DNO_DEBUG -DLINUX -DX86_64 ${flags_enable_memtrace}")
set(CMAKE_C_FLAGS_RELEASE     ${CMAKE_C_FLAGS_SCARABOPT})
set(CMAKE_CXX_FLAGS_RELEASE   ${CMAKE_CXX_FLAGS_SCARABOPT})
set(CMAKE_C_FLAGS_VALGRIND    "-O0 -g3 -DLINUX -DX86_64 ${flags_enable_memtrace}")
set(CMAKE_CXX_FLAGS_VALGRIND  "-O0 -g3 -DLINUX -DX86_64 ${flags_enable_memtrace}")
set(CMAKE_C_FLAGS_GPROF       "${CMAKE_CXX_FLAGS_SCARABOPT} -pg -g3 ${flags_enable_memtrace}")
set(CMAKE_CXX_FLAGS_GPROF     "${CMAKE_CXX_FLAGS_SCARABOPT} -pg -g3 ${flags_enable_memtrace}")

#build dependencies with default warn flags, otherwise dynamorio will not build
add_subdirectory(deps)
include(GoogleTest)
include(testing/CMakeTestingOptions.cmake)

#Create Build rules for everyone in project
add_library(scarab_build_rules INTERFACE)
target_include_directories(scarab_build_rules INTERFACE .)

set(warn_flags -Wall -Wunused -Wno-long-long
  -Wpointer-arith
  -Werror)
set(warn_c_flags ${warn_flags}
  -Wmissing-declarations
  -Wmissing-prototypes
  -Wimplicit -Wno-unused-but-set-variable -Wno-maybe-uninitialized)
set(warn_cxx_flags ${warn_flags})

add_compile_options(
        "$<$<COMPILE_LANGUAGE:C>:${warn_c_flags}>"
        "$<$<COMPILE_LANGUAGE:CXX>:${warn_cxx_flags}>"
)

add_subdirectory(ramulator)
add_subdirectory(globals)
add_subdirectory(debug)
add_subdirectory(libs)
add_subdirectory(isa)
add_subdirectory(testing)

add_subdirectory(pin/pin_lib)
add_subdirectory(pin/pin_exec/testing)

set(scarab_dirs bp bp/template_lib dvfs frontend libs memory power prefetcher .)
if(DEFINED ENV{SCARAB_ENABLE_MEMTRACE})
  set(scarab_dirs ${scarab_dirs} frontend/memtrace)
endif()

set(srcs)
foreach(dir IN LISTS scarab_dirs) 
    file(GLOB dir_srcs
      ${dir}/*.h
      ${dir}/*.cc
      ${dir}/*.c
    )
    set(srcs ${srcs} ${dir_srcs})
endforeach()

file(GLOB main_file_full_path ./main.c)
#get_filename_component(main_file_full_path ${CMAKE_CURRENT_SOURCE_DIR}/./main.c ABSOLUTE)
list(REMOVE_ITEM srcs ${main_file_full_path})

add_library(scarab_no_main 
  STATIC
    ${srcs}
)
target_link_libraries(scarab_no_main
  PUBLIC
    globals
    isa
    scarab_libs
    scarab_debug
    scarab_build_rules
    ramulator
    pin_lib_for_scarab
)

add_executable(scarab ${main_file_full_path})

target_link_libraries(scarab
  PUBLIC
    scarab_no_main
)

if(DEFINED ENV{SCARAB_ENABLE_MEMTRACE})
  target_link_libraries(scarab PRIVATE dynamorio memtrace)
endif()
