cmake_minimum_required (VERSION 3.5)

project(scarab C CXX ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(enable_memtrace 0)
set(flags_enable_memtrace "")

if(DEFINED ENV{ENABLE_MEMTRACE})
  set(enable_memtrace 1)
  set(flags_enable_memtrace "-DENABLE_MEMTRACE")
endif()

set(CMAKE_C_FLAGS_SCARABOPT   "-O3 -DNO_DEBUG -DLINUX -DX86_64 ${flags_enable_memtrace}")
set(CMAKE_CXX_FLAGS_SCARABOPT "-O3 -DNO_DEBUG -DLINUX -DX86_64 ${flags_enable_memtrace}")
set(CMAKE_C_FLAGS_VALGRIND    "-O0 -g3 -DLINUX -DX86_64 ${flags_enable_memtrace}")
set(CMAKE_CXX_FLAGS_VALGRIND  "-O0 -g3 -DLINUX -DX86_64 ${flags_enable_memtrace}")
set(CMAKE_C_FLAGS_GPROF       "${CMAKE_CXX_FLAGS_SCARABOPT} -pg -g3 ${flags_enable_memtrace}")
set(CMAKE_CXX_FLAGS_GPROF     "${CMAKE_CXX_FLAGS_SCARABOPT} -pg -g3 ${flags_enable_memtrace}")

#build dependencies with default warn flags, otherwise dynamorio will not build
add_subdirectory(deps)

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
add_subdirectory(pin/pin_lib)
add_subdirectory(pin/pin_exec/testing)

set(scarab_dirs bp debug bp/template_lib dvfs frontend globals isa libs memory power prefetcher .)
if(enable_memtrace)
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

add_executable(scarab 
    ${srcs}
)

target_include_directories(scarab PRIVATE .)

if(enable_memtrace)
  target_include_directories(scarab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/include/)
  target_include_directories(scarab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/dynamorio/ext/drcovlib/)
  target_include_directories(scarab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/dynamorio/ext/drmgr/)
  target_include_directories(scarab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/dynamorio/ext/drcontainers/)
  target_include_directories(scarab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/dynamorio/core/lib/)
  target_include_directories(scarab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/dynamorio/clients/drcachesim/reader/)
  target_include_directories(scarab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/dynamorio/clients/drcachesim/common/)
  target_include_directories(scarab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/dynamorio/clients/drcachesim/tracer/)
  target_include_directories(scarab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/dynamorio/clients/drcachesim/)
endif()

target_link_libraries(scarab
    PRIVATE
        ramulator
        pin_lib_for_scarab
)

if(enable_memtrace)
  target_link_libraries(scarab
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/clients/lib64/release/libdrmemtrace_raw2trace.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/clients/lib64/release/libdirectory_iterator.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/lib64/libdrfrontendlib.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/ext/lib64/release/libdrutil_static.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/ext/lib64/release/libdrmgr_static.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/lib64/libdrmemfuncs.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/lib64/release/libdynamorio_static.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/lib64/libdrlibc.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/ext/lib64/release/libdrcovlib_static.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/ext/lib64/release/libdrx_static.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/ext/lib64/release/libdrreg_static.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/ext/lib64/release/libdrcontainers.a
        ${CMAKE_CURRENT_SOURCE_DIR}/build/opt/deps/dynamorio/clients/lib64/release/libdrmemtrace_analyzer.a
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/xed/obj/libxed.a
        snappy
        dl
        config++
        z
        rt
        pthread
  )
endif()
