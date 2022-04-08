add_library(isa
    STATIC 
        isa_macros.h
        isa.c
        isa.h
)
target_link_libraries(isa PUBLIC scarab_build_rules)