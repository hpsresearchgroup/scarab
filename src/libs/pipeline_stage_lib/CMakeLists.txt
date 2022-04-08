option(PIPELINE_STAGE_LIB_TESTS "Turn ON/OFF pipeline_stage_lib unit tests" ON)

add_library(pipeline_stage_lib
    STATIC
        stage_data.h
        stage_data.cc
        pipeline_stage.h
)
target_link_libraries(pipeline_stage_lib PRIVATE scarab_commons)

if(PIPELINE_STAGE_LIB_TESTS)
    add_library(pipeline_stage_lib_test
        OBJECT
        testing/stage_data_test.cc)
    target_link_libraries(pipeline_stage_lib_test
        PUBLIC
        pipeline_stage_lib
        scarab_commons
        globals
        debug
        gtest)
endif()