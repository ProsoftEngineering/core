#include <prosoft/core/config/config.h> // first (required for other includes)

#include <prosoft/core/config/config_analyzer.h>
#include <prosoft/core/config/config_apple.h>
#include <prosoft/core/config/config_assert.h>
#include <prosoft/core/config/config_compiler.h>
#include <prosoft/core/config/config_cpp_except.h>
#include <prosoft/core/config/config_cpp.h>
#include <prosoft/core/config/config_cpp_util.h>
#include <prosoft/core/config/config_platform.h>
#include <prosoft/core/config/config_stdlib.h>
#ifdef _WIN32
    #include <prosoft/core/config/config_windows.h>
#endif
#include <prosoft/core/include/byteorder.h>
#include <prosoft/core/include/semaphore.hpp>
#include <prosoft/core/include/stable_hash_wrapper.hpp>
#include <prosoft/core/include/stream_utils.hpp>
#ifdef __APPLE__
    #include <prosoft/core/include/string/apple_convert.hpp>
#endif
#include <prosoft/core/include/string/case_convert.hpp>
#include <prosoft/core/include/string/platform_convert.hpp>
#include <prosoft/core/include/string/string_component.hpp>
#include <prosoft/core/include/string/string_convert.hpp>
#include <prosoft/core/include/string/string_types.hpp>
#include <prosoft/core/include/string/unicode_convert.hpp>
#include <prosoft/core/include/system_error.hpp>
#include <prosoft/core/include/system_utils.hpp>
#include <prosoft/core/include/uniform_access.hpp>
#include <prosoft/core/include/unique_resource.hpp>

#ifndef _MSC_VER
    #define EXPECTED_CPLUSPLUS 201103L  // C++11
#else
    #define EXPECTED_CPLUSPLUS 201402L  // C++14 (MSVC doesn't support C++11)
#endif
#if __cplusplus != EXPECTED_CPLUSPLUS
    #error "Error: unexpected __cplusplus"
#endif

void test_public_headers() {}   // prevent warning "no object file members in the library define global symbols"
