add_library(pipeline_stage_lib
    STATIC
        stage_data.h
        stage_data.cc
        pipeline_stage.h
)
target_link_libraries(pipeline_stage_lib PRIVATE scarab_build_rules)

if(PIPELINE_STAGE_LIB_TESTS)
    add_library(pipeline_stage_lib_test
        OBJECT
        testing/stage_data_test.cc)

    target_link_libraries(pipeline_stage_lib_test
        PUBLIC
        pipeline_stage_lib
        scarab_build_rules
        globals
        scarab_debug
        gtest)
endif()