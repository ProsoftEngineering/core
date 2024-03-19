# Copyright © 2015-2018, Prosoft Engineering, Inc. (A.K.A "Prosoft")
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Prosoft nor the names of its contributors may be
#       used to endorse or promote products derived from this software without
#       specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL PROSOFT ENGINEERING, INC. BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 3.15)
project(coretests)

set(CORETESTS ${CMAKE_CURRENT_LIST_DIR})

include("${CMAKE_CURRENT_LIST_DIR}/../core.cmake")

if (PSGCC)
# Coverage is not enabled for clang as Xcode 7.2 clang hangs compiling regex_tests.cpp.
# I'm thinking it has something to do with the .cpp includes.
set(PSCOVERAGE ${DEBUG})
endif()

include("${CORETESTS}/../config/cmake/config_coverage.cmake")

set(SRCTESTS
    ${CORETESTS}/src/byteorder_tests.cpp
    ${CORETESTS}/src/case_convert_tests.cpp
    ${CORETESTS}/src/filesystem_tests.cpp
    ${CORETESTS}/src/filesystem_acl_tests.cpp
    ${CORETESTS}/src/filesystem_change_iterator_tests.cpp
    ${CORETESTS}/src/filesystem_iterator_tests.cpp
    ${CORETESTS}/src/filesystem_monitor_tests.cpp
    ${CORETESTS}/src/filesystem_path_tests.cpp
	${CORETESTS}/src/filesystem_snapshot_tests.cpp
    ${CORETESTS}/src/path_utils_tests.cpp
    ${CORETESTS}/src/regex_tests.cpp
    ${CORETESTS}/src/semaphore_tests.cpp
    ${CORETESTS}/src/stable_hash_wrapper_tests.cpp
	${CORETESTS}/src/stream_utils_tests.cpp
    ${CORETESTS}/src/string_component_tests.cpp
    ${CORETESTS}/src/string_convert_tests.cpp
    ${CORETESTS}/src/system_error_tests.cpp
    ${CORETESTS}/src/system_identity_tests.cpp
    ${CORETESTS}/src/uniform_access_tests.cpp
    ${CORETESTS}/src/unique_resource_tests.cpp
    ${CORETESTS}/src/u8string_tests.cpp
)

add_executable(${PROJECT_NAME}
    ${CORETESTS}/src/test_main.cpp
    ${SRCTESTS}
)

if(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        ${CORETESTS}/src/apple_convert_tests.mm
        ${CORETESTS}/src/spawn_tests.cpp
    )
    set_source_files_properties(
        ${CORETESTS}/src/apple_convert_tests.mm
        PROPERTIES COMPILE_FLAGS -fobjc-arc
    )
endif()

if(WIN32)
	 target_sources(${PROJECT_NAME} PRIVATE
        ${CORETESTS}/src/winutils_tests.cpp
    )
endif()

find_package(Catch2 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2)

if (UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PUBLIC pthread) # required for std::thread
endif()

ps_core_configure(${PROJECT_NAME})

# This must be done before using modules so that the modules can setup their internal tests (if any)
enable_testing()

ps_core_use_filesystem(${PROJECT_NAME}) # brings in identity, u8string and winutils
ps_core_use_regex(${PROJECT_NAME})

# We add all of our tests to coverage as they will produce the coverage for header-only implementations
ps_core_enable_target_code_coverage(${PROJECT_NAME})

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME})
