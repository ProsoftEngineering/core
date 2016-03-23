# Copyright © 2015-2016, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

macro(ps_core_configure_required TARGET_NAME)
    ps_core_include(${TARGET_NAME})
    if(DEBUG)
        target_compile_definitions(${TARGET_NAME} PRIVATE DEBUG=1)
    endif()
    if(WIN32 AND BUILD_SHARED_LIBS)
        get_target_property(TGTYPE ${TARGET_NAME} TYPE)
        if(${TGTYPE} STREQUAL "EXECUTABLE")
            target_compile_definitions(${TARGET_NAME} PRIVATE PS_LIB_IMPORTS=1)
        endif()
    endif()
endmacro()

macro(ps_core_configure TARGET_NAME)
    ps_core_configure_required(${TARGET_NAME})
    ps_core_config_compiler_defaults(${TARGET_NAME})
    ps_core_config_cpp_version(${TARGET_NAME})
    ps_core_config_platform_required(${TARGET_NAME})
    if(DEBUG)
        ps_core_config_sanitizer(${TARGET_NAME})
    endif()
endmacro()

macro(ps_core_use_filesystem TARGET_NAME)
    ps_core_use_u8string(${TARGET_NAME})
    ps_core_use_system_identity(${TARGET_NAME})
    if(NOT TARGET filesystem)
        add_subdirectory(${PSCORE}/modules/filesystem ${CMAKE_BINARY_DIR}/psfilesystem)
    endif()
    target_link_libraries(${TARGET_NAME} PUBLIC filesystem)
endmacro()

macro(ps_core_use_u8string TARGET_NAME)
    if(NOT TARGET u8string)
        add_subdirectory(${PSCORE}/modules/u8string ${CMAKE_BINARY_DIR}/u8string)
    endif()
    target_link_libraries(${TARGET_NAME} PUBLIC u8string)
endmacro()

macro(ps_core_use_regex TARGET_NAME)
    if(NOT TARGET regex)
        add_subdirectory(${PSCORE}/modules/regex ${CMAKE_BINARY_DIR}/psregex)
    endif()
    target_link_libraries(${TARGET_NAME} PUBLIC regex)
endmacro()

macro(ps_core_use_system_identity TARGET_NAME)
    ps_core_use_u8string(${TARGET_NAME})
    if(NOT TARGET system_identity)
        add_subdirectory(${PSCORE}/modules/system_identity ${CMAKE_BINARY_DIR}/pssystem_identity)
    endif()
    target_link_libraries(${TARGET_NAME} PUBLIC system_identity)
endmacro()
