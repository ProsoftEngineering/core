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
project(ps_winutils_tests)

include("${CMAKE_CURRENT_LIST_DIR}/../../config_module.cmake")

add_executable(${PROJECT_NAME}
    src/winutils_tests.cpp
)

ps_core_module_config(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 14     # Catch2 does not support C++11
    CXX_EXTENSIONS OFF
)

find_package(ps_winutils REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ps::winutils)

add_library(${PROJECT_NAME}-test-public-headers
    src/test_public_headers.cpp
)
set_target_properties(${PROJECT_NAME}-test-public-headers PROPERTIES
    CXX_STANDARD 98     # test cxx_std_... in module CMakeFile.txt
    CXX_EXTENSIONS OFF
)
target_link_libraries(${PROJECT_NAME}-test-public-headers PRIVATE ps::winutils)
if(MSVC)
    target_compile_options(${PROJECT_NAME}-test-public-headers PRIVATE "/Zc:__cplusplus")   # enable __cplusplus
endif()

find_package(Catch2 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(${PROJECT_NAME})
