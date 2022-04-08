enable_testing()

add_executable(scarab_test test_main.cc)

target_link_libraries(scarab_test PRIVATE
    pipeline_stage_lib_test
    scarab_commons
    gtest_main
)

gtest_discover_tests(scarab_test)