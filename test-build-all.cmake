#
# Build and test all modules with given builders
#
# Usage:
#   mkdir BUILD_DIR && cd BUILD_DIR && cmake [options] -P SOURCE_DIR/test-build-all.cmake
#   Options:
#     -DBUILDERS=MAKE,MSVC,MSVCx86,NINJA,XCODE
#     -DBUILD_TYPES=Release,RelWithDebInfo,Debug
#
cmake_minimum_required(VERSION 3.15)    # foreach(... IN LISTS ...)

if(NOT BUILDERS)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(BUILDERS "XCODE")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(BUILDERS "MSVC,MSVCx86")
    else()
        find_program(NINJA "ninja")
        if(NINJA)
            set(BUILDERS "NINJA")
        else()
            set(BUILDERS "MAKE")
        endif()
    endif()
endif()
if(NOT BUILD_TYPES)
    set(BUILD_TYPES "RelWithDebInfo,Debug")
endif()

string(REPLACE "," ";" BUILDERS_LIST "${BUILDERS}")
string(REPLACE "," ";" BUILD_TYPES_LIST "${BUILD_TYPES}")

function(run_build_cmake BUILD_DIR)
    list(REMOVE_AT ARGV 0)
    set(BUILD_ARGS ${ARGV})
    execute_process(
        COMMAND cmake -E make_directory ${BUILD_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND cmake -E chdir ${BUILD_DIR}
                         cmake ${BUILD_ARGS} -P "${CMAKE_CURRENT_LIST_DIR}/test-build.cmake"
        COMMAND_ERROR_IS_FATAL ANY
    )
endfunction()

function(run_build_cmake_for_build_types GENERATOR_ID GENERATOR)
    list(REMOVE_AT ARGV 0 1)
    set(BUILD_ARGS ${ARGV})
    foreach(BUILD_TYPE IN LISTS BUILD_TYPES_LIST)
        set(BUILD_DIR "build_${GENERATOR_ID}_${BUILD_TYPE}")
        set(BUILD_ARGS "-DBUILD_TYPE=${BUILD_TYPE}" "-DGENERATOR=${GENERATOR}")
        set(BUILD_ARGS ${BUILD_ARGS} -DPS_CORE_BUILD_TESTS=ON)
        run_build_cmake(${BUILD_DIR} ${BUILD_ARGS})
    endforeach()
endfunction()

foreach(BUILDER IN LISTS BUILDERS_LIST)
    if(BUILDER STREQUAL "MAKE")
        run_build_cmake_for_build_types("Makefiles" "Unix Makefiles")
    elseif(BUILDER STREQUAL "MSVC" OR BUILDER STREQUAL "MSVCx86")
        if(EXISTS "C:/Program Files/Microsoft Visual Studio/2022")
            set(GENERATOR "Visual Studio 17 2022")
            set(GENERATORx86 ${GENERATOR})
            set(ARGSx86 -DPLATFORM=Win32 -DARCH=x86)
        elseif(EXISTS "C:/Program Files (x86)/Microsoft Visual Studio/2019")
            set(GENERATOR "Visual Studio 16 2019")
            set(GENERATORx86 ${GENERATOR})
            set(ARGSx86 -DPLATFORM=Win32 -DARCH=x86)
        elseif(EXISTS "C:/Program Files (x86)/Microsoft Visual Studio/2017")
            set(GENERATOR "Visual Studio 15 2017 Win64")
            set(GENERATORx86 "Visual Studio 15 2017")
            set(ARGSx86 -DARCH=x86)
        elseif(EXISTS "C:/Program Files (x86)/Microsoft Visual Studio 14.0")
            set(GENERATOR "Visual Studio 14 2015 Win64")
            set(GENERATORx86 "Visual Studio 14 2015")
            set(ARGSx86 -DARCH=x86)
        else()
            message(FATAL_ERROR "Unknown MSVC version")
        endif()
        if(BUILDER STREQUAL "MSVC")
            run_build_cmake_for_build_types("MSVC" "${GENERATOR}")
        else()
            run_build_cmake_for_build_types("MSVCx86" "${GENERATORx86}")
        endif()
    elseif(BUILDER STREQUAL "NINJA")
        run_build_cmake_for_build_types("Ninja" "Ninja")
    elseif(BUILDER STREQUAL "XCODE")
        run_build_cmake_for_build_types("Xcode" "Xcode")
    else()
        message(FATAL_ERROR "Unknown BUILDER (${BUILDER})")
    endif()
endforeach()
