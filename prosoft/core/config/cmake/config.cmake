# Copyright © 2013-2018, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

cmake_minimum_required(VERSION 3.1)

set(PSCONFIG "${CMAKE_CURRENT_LIST_DIR}/..")
get_filename_component(PSCONFIG ${PSCONFIG} REALPATH)

if(NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR, "CMake build type not set!")
endif()

# PS_BUILD_* are stable build type identifiers. They should never be set anywhere but here.
# This is not true of 'DEBUG'.
if(NOT DEFINED PS_BUILD_DEBUG)
    # CMake doesn't have a case-insensitive string compare
    string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_TOLOWER)
    if(CMAKE_BUILD_TYPE_TOLOWER STREQUAL "release" OR CMAKE_BUILD_TYPE_TOLOWER STREQUAL "relwithdebinfo")
        set(PS_BUILD_DEBUG false)
        set(PS_BUILD_RELEASE true)
    else()
        set(PS_BUILD_DEBUG true)
        set(PS_BUILD_RELEASE false)
    endif()
endif()

if(NOT DEFINED DEBUG)
    set(DEBUG ${PS_BUILD_DEBUG})
endif()

include("${PSCONFIG}/cmake/config_compiler.cmake")
include("${PSCONFIG}/cmake/config_sanitizer.cmake")
include("${PSCONFIG}/cmake/config_cpp.cmake")
include("${PSCONFIG}/cmake/config_platform.cmake")
