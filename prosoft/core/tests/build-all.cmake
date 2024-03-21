#
# Usage:
#   mkdir BUILD_DIR && cd BUILD_DIR && cmake [options] -P SOURCE_DIR/build-all.cmake
#

find_program(NINJA_PROGRAM ninja)
if(NINJA_PROGRAM)
    execute_process(
        COMMAND cmake -E make_directory build_Ninja_RelWithDebInfo
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND cmake -E chdir build_Ninja_RelWithDebInfo
                         cmake -DGENERATOR=Ninja -DBUILD_TYPE=RelWithDebInfo
                               -P "${CMAKE_CURRENT_LIST_DIR}/build.cmake"
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND cmake -E make_directory build_Ninja_Debug
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND cmake -E chdir build_Ninja_Debug
                         cmake -DGENERATOR=Ninja -DBUILD_TYPE=Debug
                               -P "${CMAKE_CURRENT_LIST_DIR}/build.cmake"
        COMMAND_ERROR_IS_FATAL ANY
    )
else()  # "Unix Makefiles"
    execute_process(
        COMMAND cmake -E make_directory build_Makefiles_RelWithDebInfo
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND cmake -E chdir build_Makefiles_RelWithDebInfo
                         cmake "-DGENERATOR=Unix Makefiles" -DBUILD_TYPE=RelWithDebInfo
                               -P "${CMAKE_CURRENT_LIST_DIR}/build.cmake"
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND cmake -E make_directory build_Makefiles_Debug
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND cmake -E chdir build_Makefiles_Debug
                         cmake "-DGENERATOR=Unix Makefiles" -DBUILD_TYPE=Debug
                               -P "${CMAKE_CURRENT_LIST_DIR}/build.cmake"
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
