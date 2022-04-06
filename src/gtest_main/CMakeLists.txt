cmake_minimum_required (VERSION 3.5)

project(ScarabGTest)

option(PIPELINE_STAGE_LIB "Turn ON/OFF pipeline_stage_lib unit tests" ON)

include(FetchContent)
FetchContent_Declare(
googletest
URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

include(GoogleTest)

if (PIPELINE_STAGE_LIB)
    add_subdirectory(../libs/pipeline_stage_lib/test pipeline_stage_lib)
    gtest_discover_tests(pipeline_stage_lib)
endif()