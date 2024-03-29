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

include("${CMAKE_CURRENT_LIST_DIR}/../config_module.cmake")

add_library(${PROJECT_NAME}
    src/attrs.cpp
    src/dirops.cpp
    src/change_iterator.cpp
    src/fsmonitor.cpp
    src/iterator.cpp
    src/pathops.cpp
    src/filesystem.cpp
    src/filesystem_acl.cpp
    src/snapshot_all.cpp
    src/standard_directory_path.cpp
)

ps_core_module_config(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include)

if(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        src/fsevents_monitor.cpp
        src/snapshot_mac.cpp
        src/spawn_mac.mm
    )
    set_source_files_properties(
        src/spawn_mac.mm
        PROPERTIES COMPILE_FLAGS -fobjc-arc
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Foundation" "-framework CoreServices")
    find_package(nlohmann_json REQUIRED)        # fsevents_monitor.cpp
    target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
endif()

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        src/snapshot_win32.cpp
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32 vssapi)
    find_package(winutils REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE winutils::winutils)
endif()

if(PSLINUX)
    target_link_libraries(${PROJECT_NAME} PUBLIC acl)
endif()

if (UNIX AND NOT APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        src/snapshot_nop.cpp
    )
endif()

if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES STATIC_LIBRARY_FLAGS
        "/ignore:4221"      # This object file does not define any previously undefined public symbols
    )
endif()

find_package(ps_core REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC ps::core)

find_package(u8string REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC u8string::u8string)

find_package(system_identity REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC system_identity::system_identity)

if(PS_CORE_BUILD_PSTEST_HARNESS)
    message("WARNING: ${PROJECT_NAME} is compiled with PSTEST_HARNESS. Don't use in production!")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PSTEST_HARNESS=1)
    find_package(Catch2 REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2)
endif()

add_library(filesystem::filesystem ALIAS filesystem)

add_library(filesystem_internal INTERFACE)
target_include_directories(filesystem_internal INTERFACE src)   # fstestutils.hpp
add_library(filesystem::filesystem_internal ALIAS filesystem_internal)

if(PS_CORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
