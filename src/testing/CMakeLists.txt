option(PIPELINE_STAGE_LIB_TESTS "Turn ON/OFF pipeline_stage_lib unit tests" OFF)

enable_testing()

add_executable(scarab_test test_main.cc ../sim.c ../cmp_model.c)

if(PIPELINE_STAGE_LIB_TESTS)
    target_link_libraries(scarab_test PRIVATE
        pipeline_stage_lib_test
    )
endif()

target_link_libraries(scarab_test PRIVATE
    globals
    scarab_debug
    scarab_build_rules
    gtest_main
)

gtest_discover_tests(scarab_test)