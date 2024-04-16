#
# Build and test all ps_core modules with given options
#
# Usage:
#   mkdir BUILD_DIR && cd BUILD_DIR && cmake [options] -P SOURCE_DIR/test-build.cmake
# Options:
#   -DGENERATOR=...
#   -DARCH=...
#   -DPLATFORM=...
#   -DBUILD_TYPE=...
#   -DBUILD_TESTS=...
#   -DBUILD_PSTEST_HARNESS=...
#
if(NOT GENERATOR)
    set(GENERATOR "Unix Makefiles")
endif()
if(NOT BUILD_TYPE)
    set(BUILD_TYPE "RelWithDebInfo")
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
# Don't use cmake presets because they are created in the source directory.
set(COMMAND cmake "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_BINARY_DIR}/conan_toolchain.cmake")
set(COMMAND ${COMMAND} "-G${GENERATOR}")
if(PLATFORM)
    set(COMMAND ${COMMAND} "-A${PLATFORM}")
endif()
set(COMMAND ${COMMAND} "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}")
set(COMMAND ${COMMAND} "${CMAKE_CURRENT_LIST_DIR}")
if(BUILD_TESTS)
    set(COMMAND ${COMMAND} "-DPS_CORE_BUILD_TESTS=${BUILD_TESTS}")
endif()
if(BUILD_PSTEST_HARNESS)
    set(COMMAND ${COMMAND} "-DPS_CORE_BUILD_PSTEST_HARNESS=${BUILD_PSTEST_HARNESS}")
endif()
list(JOIN COMMAND " " COMMAND_STRING)
message(STATUS "COMMAND: ${COMMAND_STRING}")
execute_process(
    COMMAND ${COMMAND}
    COMMAND_ERROR_IS_FATAL ANY
)

# cmake build
set(COMMAND cmake --build . --config "${BUILD_TYPE}")
list(JOIN COMMAND " " COMMAND_STRING)
message(STATUS "COMMAND: ${COMMAND_STRING}")
execute_process(
    COMMAND ${COMMAND}
    COMMAND_ERROR_IS_FATAL ANY
)

# cmake test
set(COMMAND ctest --build-config "${BUILD_TYPE}")
list(JOIN COMMAND " " COMMAND_STRING)
message(STATUS "COMMAND: ${COMMAND_STRING}")
execute_process(
    COMMAND ${COMMAND}
    COMMAND_ERROR_IS_FATAL ANY
)
