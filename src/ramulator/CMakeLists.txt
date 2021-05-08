file(GLOB srcs *.cpp *.h)
add_library(ramulator STATIC ${srcs})
target_compile_definitions(ramulator PRIVATE RAMULATOR)
target_compile_options(ramulator PRIVATE ${ramulator_warnings})