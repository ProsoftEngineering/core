# Copyright © 2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

macro(ps_core_config_xcode_asan)
    # The normal compile flag check does not work with the Xcode generator as CMAKE_REQUIRED_FLAGS are not set in the Xcode file for the linker.
    # This hack works around that by checking if the Xcode generator is enabled and if Apple's clang version is correct (Xcode 7 has Apple clang version 7.0.0).
    if(CMAKE_GENERATOR STREQUAL "Xcode" AND PSCLANG AND NOT CMAKE_C_COMPILER_VERSION MATCHES "^[0-6]\..*")
        set(HAVE_XCODE_ADDRESS_SANITIZER true)
    endif()
endmacro()

macro(ps_core_config_asan TARGET_NAME)
    # Xcode exposes the setting through schemes only and will copy the ASan library to bundled apps.
    # xcodebuild has a separate arg to explicitly enable/disable ASan.
    
    include(CheckCCompilerFlag)
    set(SAVED_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
    
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
    check_c_compiler_flag("-fsanitize=address" HAVE_ADDRESS_SANITIZER)
    
    if(NOT HAVE_ADDRESS_SANITIZER)
        ps_core_config_xcode_asan()
        if(HAVE_XCODE_ADDRESS_SANITIZER)
            set (HAVE_ADDRESS_SANITIZER true)
            message(STATUS "Xcode sanitizer enabled")
        endif()
    endif()
    
    # WARNING: For bundled Apple apps this will not copy the ASan lib to the bundle as Xcode schemes do.
    # So the app will only run on machines where Xcode is installed.
    if(HAVE_ADDRESS_SANITIZER)
        target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=address" "-fno-omit-frame-pointer")
        target_link_libraries(${TARGET_NAME} PUBLIC "-fsanitize=address")
    endif()
    
    set(CMAKE_REQUIRED_FLAGS ${SAVED_CMAKE_REQUIRED_FLAGS})
endmacro()

macro(ps_core_config_ubsan TARGET_NAME)
    # AS of Xcode 7 ubsan is not supported.
        
    # FreeBSD 10.x passes the compiler check but then fails to link. It appears to be missing the actual support lib.
    # Not sure why the test is passing then.
    # Also not sure about 11.x. May need to expand check to a version.
    #
    # GCC 5.x UBSAN has internal crashes.
    if(NOT PSFREEBSD AND (NOT PSGCC OR NOT CMAKE_C_COMPILER_VERSION MATCHES "^5\..*"))
        include(CheckCCompilerFlag)
        set(SAVED_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})

        set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined -fno-omit-frame-pointer")
        check_c_compiler_flag("-fsanitize=undefined" HAVE_UNDEFINED_SANITIZER)
        if(HAVE_UNDEFINED_SANITIZER)
            target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=undefined" "-fno-omit-frame-pointer")
            target_link_libraries(${TARGET_NAME} PUBLIC "-fsanitize=undefined")
        endif()

        set(CMAKE_REQUIRED_FLAGS ${SAVED_CMAKE_REQUIRED_FLAGS})
    endif()
endmacro()

macro(ps_core_config_sanitizer TARGET_NAME)
    # Clang (3.1+) and GCC (4.8+) support these. For Apple, Xcode 7 is required. (See notes above.)
    # The leak sanitizer is mutally exclusive with address and thread sanitizers.
    # The general advise to get decent runtime performance with ASan is to use at least -O1.
    # But these should probably only be enabled for debug builds.
    #
    # break on __asan_report_error to catch ASan asserts in the debugger.
    
    # Xcode 8.0 has a few different linker errors when turning on the sanitizer, so disable until it's fixed
    if(NOT CMAKE_GENERATOR STREQUAL "Xcode" OR "${XCODE_VERSION}" VERSION_LESS "8.0")
        # ASan on PPC is broken (internal crash) as of GCC 4.8.1. It may be fixed in 4.8.2 or 4.9.
        # HOST_SYSTEM_PROCESSOR may technically not be the same as the target CPU, but it's the best we have.
        if(NOT ${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "(ppc|powerpc)(32|64)?")
            ps_core_config_asan(${TARGET_NAME})
        endif()
    
        ps_core_config_ubsan(${TARGET_NAME})
    endif()
    
    # tsan is not currently enabled
endmacro()
