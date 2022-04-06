enable_testing()

add_executable(scarab_test test_main.cc)
target_include_directories(scarab_test PRIVATE ..)

target_link_libraries(scarab_test PRIVATE
    #pipeline_stage_lib_test
    gtest_main
)

include(GoogleTest)
gtest_discover_tests(scarab_test)