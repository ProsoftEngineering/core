#
# Build and test all ps_core modules with given builders
#
# Tests these configurations with predefined conan profiles:
#  * RelWithDebInfo | PSTEST_HARNESS=OFF
#  * Debug | PSTEST_HARNESS=ON
#
# Usage:
#   mkdir BUILD_DIR && cd BUILD_DIR && cmake [options] -P SOURCE_DIR/test-build-all.cmake
#   Options:
#     -DBUILDERS=MAKE,MSVC,MSVCx86,NINJA,XCODE
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
        if("-DBUILD_TYPE=Debug" IN_LIST BUILD_ARGS)
            list(APPEND BUILD_ARGS "-DCONAN_PROFILE=${CMAKE_CURRENT_LIST_DIR}/conan/profiles/macos_debug")
        else()
            list(APPEND BUILD_ARGS "-DCONAN_PROFILE=${CMAKE_CURRENT_LIST_DIR}/conan/profiles/macos")
        endif()
    elseif(WIN32)
        list(APPEND BUILD_ARGS "-DCONAN_PROFILE=${CMAKE_CURRENT_LIST_DIR}/conan/profiles/windows")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        if("-DBUILD_TYPE=Debug" IN_LIST BUILD_ARGS)
            list(APPEND BUILD_ARGS "-DCONAN_PROFILE=${CMAKE_CURRENT_LIST_DIR}/conan/profiles/linux_debug")
        else()
            list(APPEND BUILD_ARGS "-DCONAN_PROFILE=${CMAKE_CURRENT_LIST_DIR}/conan/profiles/linux")
        endif()
    endif()
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

foreach(BUILDER IN LISTS BUILDERS_LIST)
    if(BUILDER STREQUAL "MAKE")
        run_build_cmake(build_Makefiles_RelWithDebInfo
                        "-DGENERATOR=Unix Makefiles" -DBUILD_TYPE=RelWithDebInfo
                        -DBUILD_TESTS=ON)
        run_build_cmake(build_Makefiles_Debug
                        "-DGENERATOR=Unix Makefiles" -DBUILD_TYPE=Debug
                        -DBUILD_TESTS=ON -DBUILD_PSTEST_HARNESS=ON)
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
            run_build_cmake(build_MSVC_RelWithDebInfo
                            "-DGENERATOR=${GENERATOR}" -DBUILD_TYPE=RelWithDebInfo
                            -DBUILD_TESTS=ON)
            run_build_cmake(build_MSVC_Debug
                            "-DGENERATOR=${GENERATOR}" -DBUILD_TYPE=Debug
                            -DBUILD_TESTS=ON -DBUILD_PSTEST_HARNESS=ON)
        else()
            run_build_cmake(build_MSVCx86_RelWithDebInfo
                            "-DGENERATOR=${GENERATORx86}" -DBUILD_TYPE=RelWithDebInfo
                            ${ARGSx86}
                            -DBUILD_TESTS=ON)
            run_build_cmake(build_MSVCx86_Debug
                            "-DGENERATOR=${GENERATORx86}" -DBUILD_TYPE=Debug
                            ${ARGSx86}
                            -DBUILD_TESTS=ON -DBUILD_PSTEST_HARNESS=ON)
        endif()
    elseif(BUILDER STREQUAL "NINJA")
        run_build_cmake(build_Ninja_RelWithDebInfo
                        -DGENERATOR=Ninja -DBUILD_TYPE=RelWithDebInfo
                        -DBUILD_TESTS=ON)
        run_build_cmake(build_Ninja_Debug
                        -DGENERATOR=Ninja -DBUILD_TYPE=Debug
                        -DBUILD_TESTS=ON -DBUILD_PSTEST_HARNESS=ON)
    elseif(BUILDER STREQUAL "XCODE")
        run_build_cmake(build_Xcode_RelWithDebInfo
                        -DGENERATOR=Xcode -DBUILD_TYPE=RelWithDebInfo
                        -DBUILD_TESTS=ON)
        run_build_cmake(build_Xcode_Debug
                        -DGENERATOR=Xcode -DBUILD_TYPE=Debug
                        -DBUILD_TESTS=ON -DBUILD_PSTEST_HARNESS=ON)
    else()
        message(FATAL_ERROR "Unknown BUILDER (${BUILDER})")
    endif()
endforeach()
