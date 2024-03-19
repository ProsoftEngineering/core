// Copyright © 2016-2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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
#include <vector>

#include <uniform_access.hpp>
#include <u8string.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("uniform_access") {
    using namespace prosoft;
    
    SECTION("data") {
        std::string s{"hello"};
        CHECK(data(s) == s.c_str());
        CHECK(data_size(s) == 5);
        CHECK(bytes(s) == s.c_str());
        CHECK(byte_size(s) == 5);
        
        std::wstring ws{L"hello"};
        CHECK(data(ws) == ws.c_str());
        CHECK(data_size(ws) == 5);
        CHECK(bytes(ws) == reinterpret_cast<access_traits_base::const_byte_pointer>(ws.c_str()));
        CHECK(byte_size(ws) == 5*sizeof(wchar_t));
        
        u8string u8s{"hell\xc3\xb6"};
        CHECK(data(u8s) == u8s.data());
        CHECK(data_size(u8s) == 6);
        CHECK(bytes(u8s) == u8s.data());
        CHECK(byte_size(u8s) == 6);
        
        std::vector<int> v{'h', 'e', 'l', 'l', 'o'};
        CHECK(data(v) == v.data());
        CHECK(data_size(v) == 5);
        CHECK(bytes(v) == reinterpret_cast<access_traits_base::const_byte_pointer>(v.data()));
        CHECK(byte_size(v) == 5*sizeof(int));
        
        constexpr auto cs = "hello";
        CHECK(data(cs) == cs);
        CHECK(data_size(cs) == 5);
        CHECK(bytes(cs) == cs);
        CHECK(byte_size(cs) == 5);
        
        constexpr auto nullcs = "abc\0def";
        CHECK(data(nullcs) == nullcs);
        CHECK(data_size(nullcs) == 3);
        CHECK(bytes(nullcs) == nullcs);
        CHECK(byte_size(nullcs) == 3);
        
        constexpr auto wcs = L"hello";
        CHECK(data(wcs) == wcs);
        CHECK(data_size(wcs) == 5);
        CHECK(bytes(wcs) == reinterpret_cast<access_traits_base::const_byte_pointer>(wcs));
        CHECK(byte_size(wcs) == 5*sizeof(wchar_t));
        
        // check constexpr -- compilation will fail if function is not constexpr
        constexpr auto p = data("");
        constexpr auto sz = data_size(p);
        (void)sz;
    }
}
