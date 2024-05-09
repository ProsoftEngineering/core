#include <prosoft/core/modules/regex/regex.hpp>

#ifndef _MSC_VER
    #define EXPECTED_CPLUSPLUS 201103L  // C++11
#else
    #define EXPECTED_CPLUSPLUS 201402L  // C++14 (MSVC doesn't support C++11)
#endif
#if __cplusplus != EXPECTED_CPLUSPLUS
    #error "Error: unexpected __cplusplus"
#endif

void test_public_headers() {}   // prevent warning "no object file members in the library define global symbols"
