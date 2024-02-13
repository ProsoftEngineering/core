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

# Onig now supports native Cmake builds, but this build is customized for our purposes.

cmake_minimum_required(VERSION 3.15)
project(onig)

set(ONIG ${CMAKE_CURRENT_SOURCE_DIR}/onig/src)

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckTypeSize)

check_symbol_exists(alloca "alloca.h" HAVE_ALLOCA)
check_include_files(alloca.h HAVE_ALLOCA_H)
check_include_files(dlfcn.h HAVE_DLFCN_H)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(memory.h HAVE_MEMORY_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
check_include_files(sys/times.h HAVE_SYS_TIMES_H)
check_include_files(sys/time.h HAVE_SYS_TIME_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(limits.h HAVE_LIMITS_H)
# if both are present assume we can include both at the same time
check_include_files("sys/time.h;sys/times.h" TIME_WITH_SYS_TIME)

check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
check_type_size(short SIZEOF_SHORT)
if(NOT SIZEOF_INT OR NOT SIZEOF_LONG OR NOT SIZEOF_SHORT)
    message(FATAL_ERROR "integral sizeof detection failed")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmakein ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set (onig_lib_src
    ${ONIG}/onig_init.c
    ${ONIG}/regerror.c
    ${ONIG}/regparse.c
    ${ONIG}/regext.c
    ${ONIG}/regcomp.c
    ${ONIG}/regexec.c
    ${ONIG}/regenc.c
    ${ONIG}/regsyntax.c
    ${ONIG}/regtrav.c
    ${ONIG}/regversion.c
    ${ONIG}/st.c
# alternative APIs are not included
#   ${ONIG}/reggnu.c
#   ${ONIG}/regposix.c
#   ${ONIG}/regposerr.c
)

# only UTF enodings are supported.
set (onig_enc_src
    ${ONIG}/ascii.c
    ${ONIG}/unicode.c
    ${ONIG}/utf8.c
    ${ONIG}/utf16_le.c
    ${ONIG}/utf16_be.c
    ${ONIG}/unicode_unfold_key.c
    ${ONIG}/unicode_fold1_key.c
    ${ONIG}/unicode_fold2_key.c
    ${ONIG}/unicode_fold3_key.c
)

if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    # disable C4244 (integral conversion loss of data) and C4133 (invalid ptr type conversion) for Win x64
    set_source_files_properties(${onig_lib_src} PROPERTIES COMPILE_FLAGS "/wd4244")
    set_source_files_properties(${onig_enc_src} PROPERTIES COMPILE_FLAGS "/wd4244")
    set_source_files_properties(${ONIG}/regparse.c PROPERTIES COMPILE_FLAGS "/wd4244 /wd4133")

    # New warnings in VS2015
    if(MSVC14)
        # 4311: type cast pointer truncation
        set_source_files_properties(${ONIG}/regcomp.c PROPERTIES COMPILE_FLAGS "/wd4244 /wd4311")
        # 4312:type cast conversion to greater size -- 4456:hides local declaration
        set_source_files_properties(${ONIG}/regexec.c PROPERTIES COMPILE_FLAGS "/wd4244 /wd4311 /wd4312 /wd4456")
        set_source_files_properties(${ONIG}/regparse.c PROPERTIES COMPILE_FLAGS "/wd4244 /wd4133 /wd4456")
    endif()
endif()
if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    # 4018: '<': signed/unsigned mismatch
    set_source_files_properties(${ONIG}/regexec.c PROPERTIES COMPILE_FLAGS "/wd4018")
endif()

set (onig_slib_name onig)

add_library(${onig_slib_name} STATIC
    ${onig_lib_src}
    ${onig_enc_src}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_CONFIG_H NOT_RUBY EXPORT)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${ONIG}
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
