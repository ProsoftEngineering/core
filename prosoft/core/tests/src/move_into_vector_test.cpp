// Copyright © 2016, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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
//
// Standalone test for ASAN change_notification move issue with. See fsmonitor_tests_i.cpp for more info.
//
// cc -O0 -mmacosx-version-min=10.11 -std=c++14 -stdlib=libc++ -fstrict-aliasing -fvisibility=hidden -fsanitize=address -fno-omit-frame-pointer -Wall -Werror -Wextra

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

class test { // same size and layout as change_notification.
    std::string s1;
    size_t s1s;
    bool s1b;
    std::string s2;
    size_t s2s;
    bool s2b;
    std::uint64_t n1;
    std::uint64_t n2;
    std::uint32_t n3;
    std::uint32_t n4;
public:
    test(std::string&& a1, std::string&& a2)
        : s1(std::move(a1))
        , s1s(0)
        , s1b(false)
        , s2(std::move(a2))
        , s2s(0)
        , s2b(false)
        , n1(0)
        , n2(0)
        , n3(0)
        , n4(0) {}
    ~test() = default;
    test(test&&) = default;
    test(const test&) = default;
};

int main(int, char* []) {
    std::vector<test> v;
    v.reserve(2);
    v.emplace_back(test{"hello", "world"});
    std::cout << "done: " << sizeof(test) << "\n";
    return 0;
}
