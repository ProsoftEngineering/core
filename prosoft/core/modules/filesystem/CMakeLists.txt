# Copyright © 2016-2018, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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
project(filesystem)

set(PSFS ${CMAKE_CURRENT_LIST_DIR})

include("${CMAKE_CURRENT_LIST_DIR}/../config_module.cmake")

set(fs_lib ${PROJECT_NAME})
add_library(${fs_lib}
    ${PSFS}/src/attrs.cpp
    ${PSFS}/src/dirops.cpp
    ${PSFS}/src/change_iterator.cpp
    ${PSFS}/src/fsmonitor.cpp
    ${PSFS}/src/iterator.cpp
    ${PSFS}/src/pathops.cpp
    ${PSFS}/src/filesystem.cpp
    ${PSFS}/src/filesystem_acl.cpp
    ${PSFS}/src/snapshot_all.cpp
    ${PSFS}/src/standard_directory_path.cpp
)

ps_core_module_config(${fs_lib})

target_include_directories(${fs_lib}
    PUBLIC
    ${PSFS}
)

if(APPLE)
    target_sources(${fs_lib} PRIVATE
        ${PSFS}/src/fsevents_monitor.cpp
        ${PSFS}/src/snapshot_mac.cpp
        ${PSFS}/src/spawn_mac.mm
    )
    set_source_files_properties(
        ${PSFS}/src/spawn_mac.mm
        PROPERTIES COMPILE_FLAGS -fobjc-arc
    )
    target_link_libraries(${fs_lib} PUBLIC "-framework Foundation -framework CoreServices")
    find_package(nlohmann_json REQUIRED)        # fsevents_monitor.cpp
    target_link_libraries(${fs_lib} PRIVATE nlohmann_json::nlohmann_json)
    if(DEFINED CORETESTS)
        target_include_directories(${fs_lib} PRIVATE ${CORETESTS})      # fsevents_monitor.cpp: #include "src/fstestutils.hpp"
    endif()
endif()

if(WIN32)
    target_sources(${fs_lib} PRIVATE
        ${PSFS}/src/snapshot_win32.cpp
    )
    target_link_libraries(${fs_lib} PUBLIC "shell32" "vssapi")
	ps_core_module_use_external_module(${fs_lib} winutils)
endif()

if(PSLINUX)
    target_link_libraries(${fs_lib} PUBLIC acl)
endif()

if (UNIX AND NOT APPLE)
    target_sources(${fs_lib} PRIVATE
        ${PSFS}/src/snapshot_nop.cpp
    )
endif()

if(MSVC)
    set_target_properties(${fs_lib} PROPERTIES STATIC_LIBRARY_FLAGS
        "/ignore:4221"      # This object file does not define any previously undefined public symbols
    )
endif()

find_package(u8string REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC u8string::u8string)

ps_core_module_use_external_module(${fs_lib} system_identity)

ps_core_enable_target_code_coverage(${fs_lib})
