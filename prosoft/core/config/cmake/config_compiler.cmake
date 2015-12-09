# Copyright © 2013-2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(PSCLANG true)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(PSGCC true)
endif()

if (CMAKE_GENERATOR STREQUAL "Xcode")
    set(PSXCODE true)
endif()

macro(ps_core_config_compiler_default_flags TARGET_NAME)
    if(MSVC)
        # /MP is for multi-processor compilation
        # /4127 shows up with /W4
        target_compile_options(${TARGET_NAME} PRIVATE "/W3" "/WX" "/MP" "/sdl" "/wd4127")
    else()
        target_compile_options(${TARGET_NAME} PRIVATE "-Wall" "-Werror" "-Wextra")
        if(NOT APPLE)
            target_compile_definitions(${TARGET_NAME} PRIVATE __STDC_LIMIT_MACROS __STDC_FORMAT_MACROS _FILE_OFFSET_BITS=64)
            target_compile_options(${TARGET_NAME} PRIVATE -fno-strict-aliasing)
        endif()
    endif()
endmacro()

macro(ps_core_config_compiler_maximum_warnings TARGET_NAME)
    include(CheckCCompilerFlag)
    
    # XXX: leading space is required when appending
    check_c_compiler_flag(-Wbool-conversion HAS_BOOL_CONVERSION_WARNING)
    if(HAS_BOOL_CONVERSION_WARNING)
        target_compile_options(${TARGET_NAME} PRIVATE "-Wbool-conversion")
    endif()
    
    check_c_compiler_flag(-Wconstant-conversion HAS_CONST_CONVERSION_WARNING)
    if(HAS_CONST_CONVERSION_WARNING)
        target_compile_options(${TARGET_NAME} PRIVATE "-Wconstant-conversion")
    endif()
    
    check_c_compiler_flag(-Wempty-body HAS_EMPTY_BODY_WARNING)
    if(HAS_EMPTY_BODY_WARNING)
        target_compile_options(${TARGET_NAME} PRIVATE "-Wempty-body")
    endif()
    
    check_c_compiler_flag(-Wenum-conversion HAS_ENUM_CONVERSION_WARNING)
    if(HAS_ENUM_CONVERSION_WARNING)
        target_compile_options(${TARGET_NAME} PRIVATE "-Wenum-conversion")
    endif()
    
    # GCC 5.1 only has -Wint-conversion for C/Obj-C while Clang doesn't complain for C++
    if(PSCLANG)
        check_c_compiler_flag(-Wint-conversion HAS_INT_CONVERSION_WARNING)
        if(HAS_INT_CONVERSION_WARNING)
            target_compile_options(${TARGET_NAME} PRIVATE "-Wint-conversion")
        endif()
    endif()
    
    if(APPLE)
        check_c_compiler_flag(-Wimplicit-retain-self HAS_RETAIN_SELF_WARNING)
        if(HAS_RETAIN_SELF_WARNING)
            target_compile_options(${TARGET_NAME} PRIVATE "-Wimplicit-retain-self")
        endif()
    
        check_c_compiler_flag(-Warc-repeated-use-of-weak HAS_REPEATED_WEAK_WARNING)
        if(HAS_REPEATED_WEAK_WARNING)
            target_compile_options(${TARGET_NAME} PRIVATE "-Warc-repeated-use-of-weak")
        endif()
    endif()
    
    # too many errors for now
    #-Wconversion
    
    if(PSCLANG)
        # GCC -Wshadow warns for local vars that shadow C++ member functions. This is just too common of a practice. I fixed a bunch and just got fed up.
        # Clang does not warn in this scenerio.
        check_c_compiler_flag(-Wshadow HAS_SHADOW_WARNING)
        if(HAS_SHADOW_WARNING)
            target_compile_options(${TARGET_NAME} PRIVATE "-Wshadow")
        endif()
    endif()
    
    include(CheckCXXCompilerFlag)
    if(PSCLANG)
        # GCC errors when C++ warnings are given for non-C++ sources (C). Clang does not care.
        # Unfortunately Cmake has no target-level CXX_FLAGS only the global CMAKE_CXX_FLAGS.
        # We'd have to use regex on the source file list to set the flag for each C++ file -- a pain.
        check_cxx_compiler_flag(-Woverloaded-virtual HAS_OVERLOADED_VIRTUAL_WARNING)
        if(HAS_OVERLOADED_VIRTUAL_WARNING)
            target_compile_options(${TARGET_NAME} PRIVATE "-Woverloaded-virtual")
        endif()
    endif()
endmacro()

macro(ps_core_config_symbols_hidden TARGET_NAME)
    include(CheckCCompilerFlag)
    if (NOT DEFINED HAS_FVISIBILITY_FLAG AND NOT MINGW)
        include(CheckCCompilerFlag)
        check_c_compiler_flag(-fvisibility=hidden HAS_FVISIBILITY_FLAG)
    endif()
    if (HAS_FVISIBILITY_FLAG)
        target_compile_options(${TARGET_NAME} PRIVATE "-fvisibility=hidden")
    endif()
endmacro()

macro(ps_core_config_compiler_defaults TARGET_NAME)
    ps_core_config_compiler_default_flags(${TARGET_NAME})
    ps_core_config_compiler_maximum_warnings(${TARGET_NAME})
    ps_core_config_symbols_hidden(${TARGET_NAME})
endmacro()
