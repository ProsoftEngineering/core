# Copyright © 2015-2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

macro(ps_get_catch_tests OUTPUT_VARIABLE TARGET_NAME)
    get_target_property(srcfiles ${TARGET_NAME} SOURCES)
    foreach(src ${srcfiles})
        file(STRINGS ${src} srcstrs REGEX "TEST_CASE|SCENARIO\\(\".*\"\\)")
        foreach(str ${srcstrs})
            string(REGEX REPLACE "^.*\\(\"(.*)\"\\).*$" "\\1" testName ${str})
            string(REPLACE " " "_" testName ${testName}) # test names can't have spaces
            list(APPEND ${OUTPUT_VARIABLE} ${testName})
        endforeach()
    endforeach()
endmacro()

# HOST_NAME can be different from TARGET_NAME to support internal library tests
macro(ps_add_ctests_from_catch_tests_to_host TARGET_NAME HOST_NAME)
    ps_get_catch_tests(testNames ${TARGET_NAME})
    foreach(testName ${testNames})
        add_test(NAME ${testName} COMMAND ${HOST_NAME} ${testName})
    endforeach()
endmacro()

macro(ps_add_ctests_from_catch_tests TARGET_NAME)
    ps_add_ctests_from_catch_tests_to_host(${TARGET_NAME} ${TARGET_NAME})
endmacro()
