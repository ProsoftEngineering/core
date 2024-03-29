# Copyright © 2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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
project(ps_core_tests)

include("${CMAKE_CURRENT_LIST_DIR}/../../config_module.cmake")

add_executable(${PROJECT_NAME}
    src/byteorder_tests.cpp
    src/case_convert_tests.cpp
    src/semaphore_tests.cpp
    src/stable_hash_wrapper_tests.cpp
    src/stream_utils_tests.cpp
    src/string_component_tests.cpp
    src/string_convert_tests.cpp
    src/system_error_tests.cpp
    src/uniform_access_tests.cpp
    src/unique_resource_tests.cpp
)
if(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        src/apple_convert_tests.mm
    )
    set_source_files_properties(
        src/apple_convert_tests.mm
        PROPERTIES COMPILE_FLAGS -fobjc-arc
    )
endif()

ps_core_module_config(${PROJECT_NAME})

find_package(ps_core REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ps::core)

find_package(Catch2 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(${PROJECT_NAME})
