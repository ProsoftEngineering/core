// Copyright © 2015-2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Prosoft nor the names of its contributors may be
//       used to endorse or promote products derived from this software without
//       specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL PROSOFT ENGINEERING, INC. BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <prosoft/core/config/config_platform.h>

#include <string>

#include <string/unicode_convert.hpp>
#include <prosoft/core/modules/regex/regex.hpp>
#include <prosoft/core/modules/u8string/u8string.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace prosoft;

TEST_CASE("regex-string") {

SECTION("invalid encoding") {
    using invalid_traits = basic_regex_traits<char, std::string, static_cast<iregex::encoding>(0x7f)>;
    CHECK_THROWS(basic_regex<invalid_traits>{"test"});
}

using traits = u8regex_traits<std::string>;

auto string = [](const char* s) -> traits::string_type {
    return traits::string_type{s};
};

#include "regex_tests_i.cpp"

}

TEST_CASE("regex-u8string") {

using traits = u8regex_traits<u8string>;

auto string = [](const char* s) -> traits::string_type {
    return traits::string_type{s};
};

#include "regex_tests_i.cpp"

}

TEST_CASE("regex-u16string") {

using traits = u16regex_traits<u16string>;

#if PS_HAVE_CONSTEXPR
static_assert(traits::encoding() == iregex::utf16(), "WTF?");
#endif

auto string = [](const char* s) -> traits::string_type {
    return to_string<traits::string_type, u8string>{}(u8string{s});
};

#include "regex_tests_i.cpp"

}
