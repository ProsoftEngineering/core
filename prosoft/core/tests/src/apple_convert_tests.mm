// Copyright © 2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <string/platform_convert.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace prosoft;

TEST_CASE("apple_string_convert") {

    WHEN("converting from null") {
        const auto us = from_CFString<u8string>{}(nullptr);
        const auto s = from_NSString<std::string>{}(nullptr);
        THEN("the string is emtpy") {
            CHECK(us.empty());
            CHECK(s.empty());
        }
    }

    WHEN("converting from non-null, but empty") {
        const auto us = from_CFString<u8string>{}(CFSTR(""));
        const auto s = from_NSString<std::string>{}([NSString stringWithUTF8String:""]);
        THEN("the string is emtpy") {
            CHECK(us.empty());
            CHECK(s.empty());
        }
    }

    WHEN("converting from Unicode") {
        const auto us = from_CFString<u8string>{}(CFSTR("Libert\u00E9"));
        const auto s = from_NSString<std::string>{}([NSString stringWithUTF8String:"Libert\u00E9"]);
        THEN("the string contains the expected value") {
            CHECK(us == "Libert\u00E9");
            CHECK(s == "Libert\u00E9");
        }
    }

    WHEN("converting from ASCII") {
        const auto us = from_CFString<u8string>{}(CFSTR("test"));
        const auto s = from_NSString<std::string>{}([NSString stringWithUTF8String:"test"]);
        THEN("the string contains the expected value") {
            CHECK(us == "test");
            CHECK(s == "test");
        }
    }

    WHEN("converting to non-UTF8") {
        const auto s = from_NSString<std::string>{}([NSString stringWithUTF8String:"Libert\u00E9"], NSMacOSRomanStringEncoding);
        THEN("the string contains the expected value") {
            CHECK(s == "Libert\x8E");
        }
    }
    
    WHEN("converting to CFString/NSString") {
        auto cfs = to_CFString<u8string>{}(u8string{"test"}); // check temp string
        CHECK(cfs);
        
        const u8string us{"test"};
        cfs = to_CFString<u8string>{}(us, to_CFString<u8string>::nocopy);
        CHECK(cfs);
        
        id nss = to_NSString<u8string>{}(us); // Catch fails to link if type is NSString* __strong (ARC)
        CHECK(nil != nss);
    }
}
