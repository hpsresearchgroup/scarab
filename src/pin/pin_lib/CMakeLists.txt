add_library(pin_lib_for_scarab
    STATIC
        message_queue_interface_lib.cc
        message_queue_interface_lib.h
        pin_scarab_common_lib.cc
        pin_scarab_common_lib.h
        uop_generator.c
        uop_generator.h
)
target_include_directories(pin_lib_for_scarab PRIVATE ../..)