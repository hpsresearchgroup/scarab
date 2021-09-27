add_library(pin_lib_for_scarab
    STATIC
        message_queue_interface_lib.cc
        message_queue_interface_lib.h
        pin_scarab_common_lib.cc
        pin_scarab_common_lib.h
        uop_generator.c
        uop_generator.h
        x86_decoder.cc
        x86_decoder.h
        x87_stack_delta.h
        x87_stack_delta.cc
        gather_scatter_addresses.h
        gather_scatter_addresses.cc
)
target_include_directories(pin_lib_for_scarab PRIVATE ../..)
target_link_libraries(pin_lib_for_scarab PUBLIC xed)


