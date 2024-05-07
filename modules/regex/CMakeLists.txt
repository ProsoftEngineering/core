# Copyright © 2015-2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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
project(ps_regex)

include("${CMAKE_CURRENT_LIST_DIR}/../config_module.cmake")

add_library(${PROJECT_NAME}
    src/regex.cpp
)

ps_core_module_config(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include)

find_package(ps_core REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC ps::core)

find_package(oniguruma REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE oniguruma::onig)

if(PS_CORE_BUILD_PSTEST_HARNESS)
    message("WARNING: ${PROJECT_NAME} is compiled with PSTEST_HARNESS. Don't use in production!")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PSTEST_HARNESS=1)
    find_package(Catch2 REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2)
endif()

add_library(ps::regex ALIAS ps_regex)

add_library(${PROJECT_NAME}_internal INTERFACE)
target_include_directories(${PROJECT_NAME}_internal INTERFACE src)
target_link_libraries(${PROJECT_NAME}_internal INTERFACE ps::regex)
target_link_libraries(${PROJECT_NAME}_internal INTERFACE oniguruma::onig)
add_library(ps::regex_internal ALIAS ${PROJECT_NAME}_internal)

if(PS_CORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
