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

if(NOT PSCONFIG)
	message(FATAL_ERROR, "PSCONFIG is missing!")
endif()

macro(ps_core_config_cpp_version TARGET_NAME)
	include(CheckCXXSourceCompiles)
	if(NOT MSVC)
		# For MSVC, their implementation of the latest version of C++ is always enabled.
		# Only GCC 4.7+/Clang accept "c++11", while GCC 4.3+ accepts -std=c++0x.
		# GCC 4.8+ has removed -std=c++0x.
		# NOTE: use of CMAKE_CXX_FLAGS/etc can interfere with CMake's compiler flag checking.
		include(CheckCXXCompilerFlag)
		check_cxx_compiler_flag("-std=c++14" HAVE_STD_CPP14_FLAG)
		check_cxx_compiler_flag("-std=c++1y" HAVE_STD_CPP1y_FLAG)
		check_cxx_compiler_flag("-std=c++11" HAVE_STD_CPP11_FLAG)
		# Clang 3.5 release (though not pre-release) and GCC 4.9.2 are the only compilers to accept "c++14".
		# We only enable C++14 if the std flag is implemented with an exception made for clang 3.4+ and gcc 4.9+ .            
		if(HAVE_STD_CPP14_FLAG)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
		elseif(HAVE_STD_CPP1y_FLAG AND (PSCLANG OR GXX_VERSION VERSION_EQUAL 4.9 OR GXX_VERSION VERSION_GREATER 4.9))
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
		elseif(HAVE_STD_CPP11_FLAG)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
		else()
			message(FATAL_ERROR "Your compiler doesn't support C++11.")
		endif()
		# Link to libc++ for 10.7+. XXX: libc++ DOES NOT support all C++14 stdlib features as of 10.11.0.
		check_cxx_compiler_flag("-stdlib=libc++" HAVE_STDLIB_LIBCPP_FLAG)
		if(HAVE_STDLIB_LIBCPP_FLAG)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
			target_link_libraries(${TARGET_NAME} "-stdlib=libc++")
		endif()
		# Turn on _GLIBCXX_USE_CXX11_ABI to use GCC 5.1 C++11 ABI
		if(CMAKE_COMPILER_IS_GNUCXX)
			target_compile_definitions(${TARGET_NAME} PRIVATE _GLIBCXX_USE_CXX11_ABI=1)
		endif()
	 endif()
endmacro()
