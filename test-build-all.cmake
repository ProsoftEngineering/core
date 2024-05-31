#
# Build and test all modules with given builders
#
# Usage:
#   mkdir BUILD_DIR && cd BUILD_DIR && cmake [options] -P SOURCE_DIR/test-build-all.cmake
#   Options:
#     -DBUILDERS=MAKE,NINJA,XCODE,MSVC
#     -DARCHS=DEFAULT,x86_64,x86                    # DEFAULT - don't set (placeholder)
#     -DBUILD_TYPES=Release,RelWithDebInfo,Debug
#
cmake_minimum_required(VERSION 3.15)    # foreach(... IN LISTS ...)

if(NOT BUILDERS)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(BUILDERS "XCODE")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(BUILDERS "MSVC")
    else()
        find_program(NINJA "ninja")
        if(NINJA)
            set(BUILDERS "NINJA")
        else()
            set(BUILDERS "MAKE")
        endif()
    endif()
endif()
if(NOT ARCHS)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(ARCHS "DEFAULT,x86")
    else()
        set(ARCHS "DEFAULT")
    endif()
endif()
if(NOT BUILD_TYPES)
    set(BUILD_TYPES "RelWithDebInfo,Debug")
endif()

string(REPLACE "," ";" BUILDERS_LIST "${BUILDERS}")
string(REPLACE "," ";" ARCHS_LIST "${ARCHS}")
string(REPLACE "," ";" BUILD_TYPES_LIST "${BUILD_TYPES}")

foreach(BUILDER IN LISTS BUILDERS_LIST)
    foreach(ARCH IN LISTS ARCHS_LIST)
        if(ARCH STREQUAL "DEFAULT")
            set(ARCH_ID "")
        else()
            set(ARCH_ID "${ARCH}")
        endif()
        foreach(BUILD_TYPE IN LISTS BUILD_TYPES_LIST)
            set(BUILD_DIR "build_${BUILDER}${ARCH_ID}_${BUILD_TYPE}")
            execute_process(
                COMMAND cmake -E make_directory ${BUILD_DIR}
                COMMAND_ERROR_IS_FATAL ANY
            )
            set(BUILD_ARGS "-DBUILDER=${BUILDER}")
            if(NOT ARCH STREQUAL "DEFAULT")
                set(BUILD_ARGS ${BUILD_ARGS} "-DARCH=${ARCH}")
            endif()
            set(BUILD_ARGS ${BUILD_ARGS} "-DBUILD_TYPE=${BUILD_TYPE}")
            set(BUILD_ARGS ${BUILD_ARGS} "-DPS_CORE_BUILD_TESTS=ON")
            execute_process(
                COMMAND cmake -E chdir ${BUILD_DIR}
                                 cmake ${BUILD_ARGS} -P "${CMAKE_CURRENT_LIST_DIR}/test-build.cmake"
                COMMAND_ERROR_IS_FATAL ANY
            )
        endforeach()
    endforeach()
endforeach()
