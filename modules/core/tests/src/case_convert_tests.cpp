// Copyright © 2015-2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <prosoft/core/include/string/unicode_convert.hpp>
#include <prosoft/core/include/string/case_convert.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace prosoft;

TEST_CASE("case_convert") {
    SECTION("string case conversion") {
        std::string s("UCASE");
        CHECK(tolower(s) == "ucase");

        s = "lcase";
        CHECK(toupper(s) == "LCASE");
    }

    SECTION("u8 case conversion") {
        u8string s("UCASE");
        CHECK(tolower(s) == "ucase");

        s = as_utf8("lcase");
        CHECK(toupper(s) == "LCASE");

        s = u8string("\xE1\xBA\x9E"); // Capital Sharp S
        CHECK(tolower(s) == "\xC3\x9F"); // small sharp S -- note: small does not convert to Capital!

        s = u8string("\xE1\xBA\xA1"); // lowercase A-with-dot-below
        CHECK(toupper(s) == "\xE1\xBA\xA0");
    }

    SECTION("u16 case conversion") {
        auto u16 = to_u16string<u8string>{};
        u16string s{u16(u8string("UCASE"))};
        CHECK(tolower(s) == u16(u8string("ucase")));

        s = u16(u8string("lcase"));
        CHECK(toupper(s) == u16(u8string("LCASE")));

        u16string::value_type buf[3] = {0, 0, 0};
        buf[0] = 0x1E9E; // Capital Sharp S
        s = buf;
        CHECK(tolower(s) == u16(u8string("\xC3\x9F"))); // small sharp S -- note: small does not convert to Capital!

        buf[0] = 0x1EA1; // lowercase A-with-dot-below;
        s = buf;
        CHECK(toupper(s) == u16(u8string("\xE1\xBA\xA0")));

        // Surragate pairs are not handled (due to conversion-by-char semantics of tolower/toupper)
        buf[0] = 0xD801; // u32 0x10437, small Yee
        buf[1] = 0xDC37;
        s = buf;

        const auto capitalYee = u16(u8string("\xF0\x90\x90\x8F")); // u32 0x1040F
        CHECK(capitalYee[0] == 0xD801);
        CHECK(capitalYee[1] == 0xDC0F);

        const auto uppercaseYee = toupper(s);
        CHECK_FALSE(uppercaseYee == capitalYee);
        CHECK(uppercaseYee == s);
    }
}
