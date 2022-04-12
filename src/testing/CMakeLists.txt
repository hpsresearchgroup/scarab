enable_testing()

add_executable(scarab_test test_main.cc)

if(PIPELINE_STAGE_LIB_TESTS)
    target_link_libraries(scarab_test PRIVATE
        pipeline_stage_lib_test
    )
endif()

target_link_libraries(scarab_test PRIVATE
    scarab_no_main
    gtest_main
)

gtest_discover_tests(scarab_test)