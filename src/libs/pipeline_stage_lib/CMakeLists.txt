option(PIPELINE_STAGE_LIB_TESTS "Turn ON/OFF pipeline_stage_lib unit tests" ON)

add_library(pipeline_stage_lib
    INTERFACE
        #stage_data.h
        #pipeline_stage.h
)

if(PIPELINE_STAGE_LIB_TESTS)
    add_library(pipeline_stage_lib_test
        OBJECT
        testing/stage_data_test.cc)
    target_link_libraries(pipeline_stage_lib_test PUBLIC scarab_commons gtest)
endif()