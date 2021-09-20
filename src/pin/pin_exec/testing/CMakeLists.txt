option(PIN_EXEC_TESTS "Turn ON/OFF pin_exec tests" OFF)

if (PIN_EXEC_TESTS)
    # The unit tests use a few assembly and C programs that need to be compiled separate from the actual unit test programs.
    set(asm_ubenchmarks jump_near_boundary  nop_mode_bad_store  nop_mode_nonret_direct  nop_mode_nonret_indirect
                        nop_mode_not_taken  nop_mode_ret  simple_loop  wrong_path_exception)

    set(c_ubenchmarks c_program_with_ud2  simple_c_program)

    set(all_ubenchmarks ${asm_ubenchmarks} ${c_ubenchmarks})

    foreach(ubenchmark IN LISTS asm_ubenchmarks) 
        add_executable(${ubenchmark} ${ubenchmark}.s)
        # The tests are written with the assumption that the first instruction of the assembly programs is located at 0x400000
        target_link_options(${ubenchmark} PRIVATE -nostdlib  -Ttext=400000 -Wl,--build-id=none -no-pie)
    endforeach()

    foreach(ubenchmark IN LISTS c_ubenchmarks) 
        add_executable(${ubenchmark} ${ubenchmark}.c)
        target_link_options(${ubenchmark} PRIVATE -static)
    endforeach()

    get_filename_component(pin_exec_path ../obj-intel64/pin_exec.so ABSOLUTE)

    include(FetchContent)
    FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
    )
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(pin_exec_test
        exception_tests.cc  execution_tests.cc  fake_scarab.cc  main.cc  nop_mode_tests.cc  utils.cc
        fake_scarab.h  utils.h)
    target_include_directories(pin_exec_test  PRIVATE ../../..)
    target_link_libraries(pin_exec_test PRIVATE pin_lib_for_scarab gtest_main)
    target_compile_definitions(pin_exec_test 
        PRIVATE
            PIN_EXEC_TOOL_PATH=\"${pin_exec_path}\"
            SIMPLE_LOOP_BINARY=\"$<TARGET_FILE:simple_loop>\"
            NOP_MODE_RET_BINARY=\"$<TARGET_FILE:nop_mode_ret>\"
            NOP_MODE_NONRET_DIRECT_BINARY=\"$<TARGET_FILE:nop_mode_nonret_direct>\"
            NOP_MODE_NONRET_INDIRECT_BINARY=\"$<TARGET_FILE:nop_mode_nonret_indirect>\"
            NOP_MODE_NOT_TAKEN_BINARY=\"$<TARGET_FILE:nop_mode_not_taken>\"
            NOP_MODE_BAD_STORE_BINARY=\"$<TARGET_FILE:nop_mode_bad_store>\"
            WRONG_PATH_EXCEPTION_BINARY=\"$<TARGET_FILE:wrong_path_exception>\"
            C_PROGRAM_WITH_UD2_BINARY=\"$<TARGET_FILE:c_program_with_ud2>\"
            SIMPLE_C_PROGRAM=\"$<TARGET_FILE:simple_c_program>\"
            JUMP_NEAR_BOUNDARY_BINARY=\"$<TARGET_FILE:jump_near_boundary>\"
    )        

    include(GoogleTest)
    gtest_discover_tests(pin_exec_test)

endif()