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

add_executable(${PROJECT_NAME}
    ${CORETESTS}/src/test_main.cpp
)

find_package(Catch2 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2)

if (UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PUBLIC pthread) # required for std::thread
endif()

ps_core_configure(${PROJECT_NAME})

# This must be done before using modules so that the modules can setup their internal tests (if any)
enable_testing()

# We add all of our tests to coverage as they will produce the coverage for header-only implementations
ps_core_enable_target_code_coverage(${PROJECT_NAME})

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME})
