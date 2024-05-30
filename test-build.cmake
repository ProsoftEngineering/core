#
# Build and test all modules with given options
#
# Usage:
#   mkdir BUILD_DIR && cd BUILD_DIR && cmake [options] -P SOURCE_DIR/test-build.cmake
# Options:
#   -DBUILDER=MAKE|NINJA|XCODE|MSVC
#   -DARCH=x86_64|x86                           # conan --settings arch=...
#   -DBUILD_TYPE=Release|RelWithDebInfo|Debug   # cmake -DCMAKE_BUILD_TYPE=...
#   -DPS_CORE_BUILD_TESTS=ON|OFF
#
if(NOT BUILDER)
    set(BUILDER "MAKE")
endif()
if(NOT BUILD_TYPE)
    set(BUILD_TYPE "RelWithDebInfo")
endif()

if(BUILDER STREQUAL "MAKE")
    set(GENERATOR "Unix Makefiles")
elseif(BUILDER STREQUAL "NINJA")
    set(GENERATOR "Ninja")
elseif(BUILDER STREQUAL "XCODE")
    set(GENERATOR "Xcode")
    set(GENERATOR_MULTICONFIG TRUE)
elseif(BUILDER STREQUAL "MSVC")
    if(EXISTS "C:/Program Files/Microsoft Visual Studio/2022")
        set(GENERATOR "Visual Studio 17 2022")
        if(NOT ARCH OR ARCH STREQUAL "x86_64")  # default
        elseif(ARCH STREQUAL "x86")
            set(GENERATOR_ARGS "-A" "Win32")
        else()
            message(FATAL_ERROR "Unknown ARCH (${ARCH})")
        endif()
    elseif(EXISTS "C:/Program Files (x86)/Microsoft Visual Studio/2019")
        set(GENERATOR "Visual Studio 16 2019")
        if(NOT ARCH OR ARCH STREQUAL "x86_64")  # default
        elseif(ARCH STREQUAL "x86")
            set(GENERATOR_ARGS "-A" "Win32")
        else()
            message(FATAL_ERROR "Unknown ARCH (${ARCH})")
        endif()
    elseif(EXISTS "C:/Program Files (x86)/Microsoft Visual Studio/2017")
        if(NOT ARCH OR ARCH STREQUAL "x86_64")  # default
            set(GENERATOR "Visual Studio 15 2017 Win64")
        elseif(ARCH STREQUAL "x86")
            set(GENERATOR "Visual Studio 15 2017")
        else()
            message(FATAL_ERROR "Unknown ARCH (${ARCH})")
        endif()
    elseif(EXISTS "C:/Program Files (x86)/Microsoft Visual Studio 14.0")
        if(NOT ARCH OR ARCH STREQUAL "x86_64")  # default
            set(GENERATOR "Visual Studio 14 2015 Win64")
        elseif(ARCH STREQUAL "x86")
            set(GENERATOR "Visual Studio 14 2015")
        else()
            message(FATAL_ERROR "Unknown ARCH (${ARCH})")
        endif()
    else()
        message(FATAL_ERROR "Unknown MSVC version")
    endif()
    set(GENERATOR_MULTICONFIG TRUE)
else()
    message(FATAL_ERROR "Unknown BUILDER (${BUILDER})")
endif()

# Initialize conan packages. Only single-config with CMAKE_BUILD_TYPE is supported.
set(COMMAND conan install)
set(COMMAND ${COMMAND} --conf "&:tools.cmake.cmaketoolchain:generator=${GENERATOR}")
if(ARCH)
    set(COMMAND ${COMMAND} --settings "arch=${ARCH}")
endif()
if(BUILD_TYPE STREQUAL "Release")
    set(CONAN_BUILD_TYPE "release")
elseif(BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CONAN_BUILD_TYPE "relwithdebinfo")
elseif(BUILD_TYPE STREQUAL "Debug")
    set(CONAN_BUILD_TYPE "debug")
else()
    message(FATAL_ERROR "Unknown BUILD_TYPE (${BUILD_TYPE})")
endif()
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(CONAN_PROFILE "macos_${CONAN_BUILD_TYPE}")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(CONAN_PROFILE "windows_${CONAN_BUILD_TYPE}")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(CONAN_PROFILE "linux_${CONAN_BUILD_TYPE}")
else()
    message(FATAL_ERROR "Unknown CMAKE_HOST_SYSTEM_NAME (${CMAKE_HOST_SYSTEM_NAME})")
endif()
set(COMMAND ${COMMAND} --profile "${CMAKE_CURRENT_LIST_DIR}/conan/profiles/${CONAN_PROFILE}")
execute_process(COMMAND conan --version OUTPUT_VARIABLE CONAN_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX REPLACE "^Conan version ([0-9]+\\.[0-9]+\\.[0-9]+)$" "\\1" CONAN_VERSION ${CONAN_VERSION})
if(NOT (CONAN_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$" AND CONAN_VERSION VERSION_GREATER_EQUAL "2.2.2"))
    message(FATAL_ERROR "Conan version 2.2.2 and greater required (CONAN_VERSION: \"${CONAN_VERSION}\")")
endif()
set(COMMAND ${COMMAND} --output-folder=.)
set(COMMAND ${COMMAND} --build=missing "${CMAKE_CURRENT_LIST_DIR}")
list(JOIN COMMAND " " COMMAND_STRING)
message(STATUS "COMMAND: ${COMMAND_STRING}")
execute_process(
    COMMAND ${COMMAND}
    COMMAND_ERROR_IS_FATAL ANY
)

# cmake configure
set(COMMAND cmake "-G" "${GENERATOR}")
if(GENERATOR_ARGS)
    set(COMMAND ${COMMAND} ${GENERATOR_ARGS})
endif()
# Don't use cmake presets because they are created in the source directory.
set(COMMAND ${COMMAND} "-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake")
# Set CMAKE_BUILD_TYPE even if GENERATOR_MULTICONFIG for checks in CMakeLists
set(COMMAND ${COMMAND} "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}")
set(COMMAND ${COMMAND} "${CMAKE_CURRENT_LIST_DIR}")
if(PS_CORE_BUILD_TESTS)
    set(COMMAND ${COMMAND} "-DPS_CORE_BUILD_TESTS=${PS_CORE_BUILD_TESTS}")
endif()
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    # Currently "Visual Studio" can work without conanbuild environment,
    # but in future (to use tools built with conan) PATH will be required.
    # Other generators (like "Ninja") require conanbuild PATH for compiler.
    set(COMMAND conanbuild.bat && ${COMMAND})
endif()
list(JOIN COMMAND " " COMMAND_STRING)
message(STATUS "COMMAND: ${COMMAND_STRING}")
execute_process(
    COMMAND ${COMMAND}
    COMMAND_ERROR_IS_FATAL ANY
)

# cmake build
set(COMMAND cmake --build .)
if(GENERATOR_MULTICONFIG)
    set(COMMAND ${COMMAND} --config "${BUILD_TYPE}")
endif()
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(COMMAND conanbuild.bat && ${COMMAND})   # see comment above
endif()
list(JOIN COMMAND " " COMMAND_STRING)
message(STATUS "COMMAND: ${COMMAND_STRING}")
execute_process(
    COMMAND ${COMMAND}
    COMMAND_ERROR_IS_FATAL ANY
)

# cmake test
set(COMMAND ctest)
if(GENERATOR_MULTICONFIG)
    set(COMMAND ${COMMAND} --config "${BUILD_TYPE}")
endif()
list(JOIN COMMAND " " COMMAND_STRING)
message(STATUS "COMMAND: ${COMMAND_STRING}")
execute_process(
    COMMAND ${COMMAND}
    COMMAND_ERROR_IS_FATAL ANY
)
