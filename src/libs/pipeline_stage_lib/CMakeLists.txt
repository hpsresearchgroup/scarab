option(PIPELINE_STAGE_LIB_TESTS "Turn ON/OFF pipeline_stage_lib unit tests" ON)

add_library(pipeline_stage_lib
    STATIC
        stage_data.h
        pipeline_stage.h
)
set_target_properties(pipeline_stage_lib PROPERTIES LINKER_LANGUAGE CXX)

if(PIPELINE_STAGE_LIB_TESTS)
    enable_testing()

    add_executable(pipeline_stage_lib_test
        testing/test_main.cc testing/stage_data_test.cc)
    target_include_directories(pipeline_stage_lib_test PRIVATE ../..)
    target_link_libraries(pipeline_stage_lib_test PRIVATE gtest_main)

    include(GoogleTest)
    gtest_discover_tests(pipeline_stage_lib_test)
endif()