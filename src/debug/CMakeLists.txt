add_library(scarab_debug 
    OBJECT 
        debug_macros.h
        debug.param.h
        debug_print.c
        debug_print.h
        memview.c
        memview.h
        pipeview.c
        pipeview.h
)
target_link_libraries(scarab_debug PRIVATE scarab_build_rules)