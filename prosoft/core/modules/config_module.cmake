# Copyright © 2013-2019, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

cmake_minimum_required(VERSION 3.1)

# Private module config

include("${CMAKE_CURRENT_LIST_DIR}/../config/cmake/config.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../config/cmake/config_coverage.cmake")

set(PS_CORE_MODULE_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../.."
	"${CMAKE_CURRENT_LIST_DIR}/../include"
)

if (DEFINED CORETESTS)
    include("${CMAKE_CURRENT_LIST_DIR}/../tests/test_utils.cmake")
endif()

macro(ps_core_module_config TARGET_NAME)
	if(DEBUG)
        target_compile_definitions(${TARGET_NAME} PRIVATE DEBUG=1)
    endif()
	if(WIN32 AND BUILD_SHARED_LIBS)
		target_compile_definitions(${TARGET_NAME} PRIVATE PS_LIB_EXPORTS=1)
	endif()
	target_include_directories(${TARGET_NAME} PRIVATE ${PS_CORE_MODULE_INCLUDE_DIRS})
	ps_core_config_cpp_version(${TARGET_NAME})
	ps_core_config_platform_required(${TARGET_NAME})
	ps_core_config_symbols_hidden(${TARGET_NAME})
	if(PS_CORE_ENABLE_SANITIZER)
        ps_core_config_sanitizer(${TARGET_NAME})
    endif()
	
	if (DEFINED CORETESTS)
	    # Allows modules to define internal tests
	    find_package(Catch2 REQUIRED)
	    target_link_libraries(${TARGET_NAME} PRIVATE Catch2::Catch2)
	    target_compile_definitions(${TARGET_NAME} PRIVATE PSTEST_HARNESS=1)
	    ps_add_ctests_from_catch_tests_to_host(${TARGET_NAME} $<TARGET_FILE:coretests>)
	endif()
endmacro()

macro(ps_core_module_use_external_module TARGET_NAME MODULE_NAME)
    # This will not bring in the headers of the module
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../${MODULE_NAME}")
    target_link_libraries(${TARGET_NAME} PUBLIC ${MODULE_NAME})
endmacro()

macro(ps_core_module_use_u8string_module TARGET_NAME)
    ps_core_module_use_external_module(${TARGET_NAME} u8string)
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../u8string/vendor/utf8cpp/source")
endmacro()

