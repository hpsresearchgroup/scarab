add_library(cache_lib
    STATIC
        cache.h
        repl.cc
        repl.h
)
target_link_libraries(cache_lib PRIVATE scarab_build_rules)
