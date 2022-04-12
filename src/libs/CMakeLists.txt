add_subdirectory(pipeline_stage_lib)

add_library(scarab_libs INTERFACE)

target_link_libraries(scarab_libs
    INTERFACE
        pipeline_stage_lib
)