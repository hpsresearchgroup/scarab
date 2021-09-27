find_package(Git)
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/..
                    RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "Updating git submodules failed: ${GIT_SUBMOD_RESULT}")
    endif()
else()
  message(FATAL_ERROR "Could not find git binary in the system")
endif()

find_package(Python3)
if(Python3_FOUND)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/xed/mfile.py
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/xed"
        COMMAND_ECHO STDOUT
    )
else()
  message(FATAL_ERROR "Could not find python3 in the system")
endif()

set($ENV{TARGET} ia32)
add_library(xed INTERFACE)
target_include_directories(xed INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/xed/include/public/xed/)
target_include_directories(xed INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/xed/obj/)
