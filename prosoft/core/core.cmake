# Copyright © 2015-2019, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

set(PSCORE "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(PSCORE ${PSCORE} REALPATH)

include("${PSCORE}/config/cmake/config.cmake")

macro(ps_core_include TARGET_NAME)
    target_include_directories(${TARGET_NAME} PRIVATE
        "${PSCORE}/../.." # Allow unambigous includes. e.g. <prosoft/core/include/...>
        "${PSCORE}/include"
        "${PSCORE}/modules"
    )
endmacro()

# This global will enable the sanitizer for any call to ps_core_configure_required() if true
# For newer versions of clang ASAN is all or nothing due to the folloowing issue:
# https://stackoverflow.com/questions/43389185/manual-poisoning-of-stdvector
if(NOT DEFINED PS_CORE_ENABLE_SANITIZER)
    set(PS_CORE_ENABLE_SANITIZER ${PS_BUILD_DEBUG})
endif()

macro(ps_core_configure_required TARGET_NAME)
    ps_core_include(${TARGET_NAME})
    # PS_BUILD_* are stable build type defines. They should never be set anywhere but here.
    # Unlike 'DEBUG' which can be set externally and even '=1' for release.
    if(PS_BUILD_DEBUG)
        target_compile_definitions(${TARGET_NAME} PRIVATE DEBUG=1 PS_BUILD_DEBUG=1 PS_BUILD_RELEASE=0)
    else()
        # For legacy reasons (#ifdef instead of #if) DEBUG should not be defined in release builds.
        target_compile_definitions(${TARGET_NAME} PRIVATE PS_BUILD_DEBUG=0 PS_BUILD_RELEASE=1)
    endif()
    if(WIN32 AND BUILD_SHARED_LIBS)
        get_target_property(TGTYPE ${TARGET_NAME} TYPE)
        if(${TGTYPE} STREQUAL "EXECUTABLE")
            target_compile_definitions(${TARGET_NAME} PRIVATE PS_LIB_IMPORTS=1)
        endif()
    endif()
    if(PS_CORE_ENABLE_SANITIZER)
        ps_core_config_sanitizer(${TARGET_NAME})
    endif()
endmacro()

macro(ps_core_configure TARGET_NAME)
    ps_core_configure_required(${TARGET_NAME})
    ps_core_config_compiler_defaults(${TARGET_NAME})
    ps_core_config_cpp_version(${TARGET_NAME})
    ps_core_config_platform_required(${TARGET_NAME})
endmacro()
