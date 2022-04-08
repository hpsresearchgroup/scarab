add_library(globals
    OBJECT
        assert.h
        enum.c
        enum.h
        global_defs.h
        global_types.h
        global_vars.h
        op_pool.c
        op_pool.h
        param_enum_headers.h
        utils.c
        utils.h
    )
target_link_libraries(globals PRIVATE scarab_build_rules)