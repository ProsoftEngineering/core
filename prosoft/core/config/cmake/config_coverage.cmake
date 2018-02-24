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

macro(ps_enable_code_coverage TARGET_FILES)
    if(NOT WIN32)
        set_property(SOURCE ${TARGET_FILES} APPEND_STRING PROPERTY COMPILE_FLAGS " -coverage ")
    endif()
endmacro()

macro(ps_link_code_coverage TARGET_NAME)
    if(NOT WIN32)
        set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS -coverage)
    endif()
endmacro()

macro(ps_enable_target_code_coverage TARGET_NAME)
    get_target_property(srcfiles ${TARGET_NAME} SOURCES)
    ps_enable_code_coverage("${srcfiles}")
    ps_link_code_coverage(${TARGET_NAME})
endmacro()

macro(ps_core_enable_code_coverage TARGET_FILES)
    if(PSCOVERAGE)
        ps_enable_code_coverage("${TARGET_FILES}")
    endif()
endmacro()

macro(ps_core_link_code_coverage TARGET_NAME)
    if(PSCOVERAGE)
        ps_link_code_coverage(${TARGET_NAME})
    endif()
endmacro()

macro(ps_core_enable_target_code_coverage TARGET_NAME)
    if(PSCOVERAGE)
        ps_enable_target_code_coverage(${TARGET_NAME})
    endif()
endmacro()
