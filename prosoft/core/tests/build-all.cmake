#
# Usage:
#   mkdir BUILD_DIR && cd BUILD_DIR && cmake [options] -P SOURCE_DIR/build-all.cmake
#   Options:
#     -DBUILDERS=MAKE,MSVC,NINJA,XCODE
#
cmake_minimum_required(VERSION 3.15)    # foreach(... IN LISTS ...)

if(NOT BUILDERS)
    set(BUILDERS "MAKE")
endif()

string(REPLACE "," ";" BUILDERS_LIST "${BUILDERS}")

function(run_build_cmake BUILD_DIR)
    list(REMOVE_AT ARGV 0)
    set(BUILD_ARGS ${ARGV})
    if(APPLE)
        list(APPEND BUILD_ARGS "-DCONAN_PROFILE=${CMAKE_CURRENT_LIST_DIR}/conan/profiles/macos")
    endif()
    execute_process(
        COMMAND cmake -E make_directory ${BUILD_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND cmake -E chdir ${BUILD_DIR}
                         cmake ${BUILD_ARGS} -P "${CMAKE_CURRENT_LIST_DIR}/build.cmake"
        COMMAND_ERROR_IS_FATAL ANY
    )
endfunction()

foreach(BUILDER IN LISTS BUILDERS_LIST)
    if(BUILDER STREQUAL "MAKE")
        run_build_cmake(build_Makefiles_RelWithDebInfo
                        "-DGENERATOR=Unix Makefiles" -DBUILD_TYPE=RelWithDebInfo)
        run_build_cmake(build_Makefiles_Debug
                        "-DGENERATOR=Unix Makefiles" -DBUILD_TYPE=Debug)
    elseif(BUILDER STREQUAL "MSVC")
        message("MSVC")
    elseif(BUILDER STREQUAL "NINJA")
        run_build_cmake(build_Ninja_RelWithDebInfo
                        -DGENERATOR=Ninja -DBUILD_TYPE=RelWithDebInfo)
        run_build_cmake(build_Ninja_Debug
                        -DGENERATOR=Ninja -DBUILD_TYPE=Debug)
    elseif(BUILDER STREQUAL "XCODE")
        run_build_cmake(build_Xcode_RelWithDebInfo
                        -DGENERATOR=Xcode -DBUILD_TYPE=RelWithDebInfo)
        run_build_cmake(build_Xcode_Debug
                        -DGENERATOR=Xcode -DBUILD_TYPE=Debug)
    else()
        message(FATAL_ERROR "Unknown BUILDER (${BUILDER})")
    endif()
endforeach()
