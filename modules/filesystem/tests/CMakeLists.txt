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
project(ps_filesystem_tests)

include("${CMAKE_CURRENT_LIST_DIR}/../../config_module.cmake")

add_executable(${PROJECT_NAME}
    src/dirops_internal_tests.cpp
    src/filesystem_acl_tests.cpp
    src/filesystem_change_iterator_tests.cpp
    src/filesystem_internal_tests.cpp
    src/filesystem_iterator_tests.cpp
    src/filesystem_monitor_tests.cpp
    src/filesystem_path_tests.cpp
    src/filesystem_snapshot_tests.cpp
    src/filesystem_tests.cpp
    src/iterator_internal_tests.cpp
    src/path_utils_tests.cpp
    src/pathops_internal_tests.cpp
)
if(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        src/change_iterator_internal_tests.cpp
        src/fsevents_monitor_internal_tests.cpp
        src/snapshot_mac_internal_tests.cpp
        src/spawn_tests.cpp
    )
elseif(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        src/snapshot_win32_internal_tests.cpp
        src/crtdbg_win32.cpp
    )
    #
    # Fix 'atltrace.h(339) : Assertion failed: false && "Too many categories defined"'
    # multiple errors after latest Visual Studio Community 2022 upgrade (17.10.1).
    # Also reproduced in GitHub Actions "windows-latest" image (Windows Server 2022)
    # with Visual Studio Enterprise 2022 (17.9.34902.65).
    #
    # The problem is that the linker does not remove duplicates of global initializers,
    # declared with __declspec(selectany) in atltrace.h as
    #   __declspec(selectany) CTraceCategoryEx<CTraceCategoryEx<>::TraceGeneral> atlTraceGeneral(_T("atlTraceGeneral"));
    # Two copies of each can be found in the generated code ("dumpbin.exe /DISASM ps_filesystem_tests.exe"):
    #   ATL::`dynamic initializer for 'atlTraceGeneral'':
    #   000000014000649D: 48 8D 0D A8 AB 11  lea         rcx,[?atlTraceGeneral@ATL@@3V?$CTraceCategoryEx@$00$0A@@1@A]
    #   ATL::`dynamic initializer for 'atlTraceGeneral'':
    #   000000014000696D: 48 8D 0D D8 A6 11  lea         rcx,[?atlTraceGeneral@ATL@@3V?$CTraceCategoryEx@$00$0A@@1@A]
    # One is from snapshot_win32.cpp.obj (ps_filesystem static library objects).
    # Another one is from snapshot_win32_internal_tests.cpp.obj (ps_filesystem_tests executable objects).
    # (Both include <atlbase.h>/<atltrace.h> with these initializers via snapshot_win32_internal.hpp.)
    #
    # MSDN: "__declspec(selectany) - At link time, if multiple definitions of a COMDAT are seen,
    #       the linker picks one and discards the rest."
    # Which it doesn't after latest VS update in this particilar case, when the duplicates are
    # both in static library and executable objects. Let's add a workaround for "Debug" mode.
    #       "... If the linker option /OPT:REF (Optimizations) is selected, then COMDAT elimination
    #       will occur to remove all the unreferenced data items in the linker output."
    #       "... By default, /OPT:REF is enabled by the linker unless /OPT:NOREF or /DEBUG is specified."
    #
    # Note: this workaround does not break global atltrace initialization:
    #   ATLTRACE(atlTraceGeneral, 0, _T("TEST ATLTRACE\n"));    // works as expected
    #
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(${PROJECT_NAME} PRIVATE "/OPT:REF")
        # Fix 'warning LNK4075: ignoring '/INCREMENTAL' due to '/OPT:REF' specification'
        target_link_options(${PROJECT_NAME} PRIVATE "/OPT:REF" "/INCREMENTAL:NO")
    endif()
endif()

ps_core_module_config(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 14     # Catch2 does not support C++11
    CXX_EXTENSIONS OFF
)

find_package(ps_filesystem REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    ps::filesystem
    ps::filesystem_internal
)
if(WIN32)
    find_package(ps_winutils REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ps::winutils)
endif()

add_library(${PROJECT_NAME}-test-public-headers
    src/test_public_headers.cpp
)
set_target_properties(${PROJECT_NAME}-test-public-headers PROPERTIES
    CXX_STANDARD 98     # test cxx_std_... in module CMakeLists.txt
    CXX_EXTENSIONS OFF
)
target_link_libraries(${PROJECT_NAME}-test-public-headers PRIVATE ps::filesystem)
if(MSVC)
    target_compile_options(${PROJECT_NAME}-test-public-headers PRIVATE "/Zc:__cplusplus")   # enable __cplusplus
endif()

find_package(Catch2 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(${PROJECT_NAME})
