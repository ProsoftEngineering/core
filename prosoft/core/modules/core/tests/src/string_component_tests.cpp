// Copyright © 2014-2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <vector>

#include <prosoft/core/include/string/string_component.hpp>
#include <prosoft/core/modules/u8string/u8string.hpp>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace prosoft;

TEMPLATE_TEST_CASE("string_components", "", std::string, u8string) {
    using S = TestType;
    using C = std::vector<S>;

    SECTION("prefix/suffix") {
        S s{"Hello World"};
        CHECK(starts_with(s, S{"Hello"}));
        CHECK_FALSE(starts_with(s, S{"hello"}));
        CHECK(ends_with(s, S{"World"}));
        CHECK(ends_with(s, S{"d"}));
        CHECK_FALSE(ends_with(s, S{"world"}));

        CHECK(starts_with(s, S{"j"}, S{"He"}, S{"1"}));
        CHECK_FALSE(starts_with(s));

        CHECK(ends_with(s, S{"a"}, S{"b"}, S{"ld"}));
        CHECK_FALSE(ends_with(s));
        CHECK_FALSE(ends_with(s, (s+s)));

        CHECK(prefix(s, S{"j"}, S{"He"}, S{"1"}) == S{"He"});
        CHECK(prefix(s, S{"j"}, S{"e"}, S{"1"}).empty());
        CHECK(suffix(s, S{"a"}, S{"b"}, S{"ld"}) == S{"ld"});
        CHECK(suffix(s, S{"a"}, S{"b"}, S{"c"}).empty());
    }

    SECTION("tokenize") {
        C tokens;

        auto str = S{"Hello World"};
        tokenize(str, S{" "}, tokens);
        REQUIRE(tokens.size() == 2);
        CHECK(tokens.at(0) == "Hello");
        CHECK(tokens.at(1) == "World");
        tokens.clear();

        str = S{"dog,cat,bird"};
        tokenize(str, S{","}, tokens);
        REQUIRE(tokens.size() == 3);
        CHECK(tokens.at(0) == "dog");
        CHECK(tokens.at(1) == "cat");
        CHECK(tokens.at(2) == "bird");
        tokens.clear();

        str = S{"/Users//prosoft/Desktop//"};
        tokenize(str, S{"/"}, tokens, tokenize_options::trim_empty);
        REQUIRE(tokens.size() == 3);
        CHECK(tokens.at(0) == "Users");
        CHECK(tokens.at(1) == "prosoft");
        CHECK(tokens.at(2) == "Desktop");

        tokens.clear();
        tokenize(str, S{"/"}, tokens);
        REQUIRE(tokens.size() == 5);
        CHECK(tokens.at(0) == "Users");
        CHECK(tokens.at(1) == "");
        CHECK(tokens.at(2) == "prosoft");
        CHECK(tokens.at(3) == "Desktop");
        CHECK(tokens.at(4) == "");

        tokens.clear();
        str = S{"/Users/prosoft//Desktop/"};
        tokenize(str, S{"/"}, tokens, tokenize_options::trim_empty);
        REQUIRE(tokens.size() == 3);
        CHECK(tokens.at(0) == "Users");
        CHECK(tokens.at(1) == "prosoft");
        CHECK(tokens.at(2) == "Desktop");

        tokens.clear();
        str = S{"this string has no token"};
        tokenize(str, S{","}, tokens);
        REQUIRE(tokens.size() == 1);
        CHECK(tokens.at(0) == str);

        tokens.clear();
        str = S{"\n\nline1\n\nline2"};
        tokenize(str, S{"\n"}, tokens, tokenize_options::trim_empty);
        REQUIRE(tokens.size() == 2);

        tokens.clear();
        str = S{"\n\nline1\n\n\n\nline2\nline3\n\n"};
        tokenize(str, S{"\n"}, tokens);
        REQUIRE(tokens.size() == 8);

        tokens.clear();
        tokenize(str, S{"\n"}, tokens, tokenize_options::trim_empty);
        REQUIRE(tokens.size() == 3);

        tokens.clear();
        str = S{"\rline1\n\nline2\r\nline3\n\tline4"};
        tokenize(str, S{"\r\n"}, tokens);
        REQUIRE(tokens.size() == 6);

        tokens.clear();
        tokenize(str, S{"\r\n"}, tokens, tokenize_options::trim_empty);
        REQUIRE(tokens.size() == 4);

        tokens.clear();
        str = S{"..."};
        tokenize(str, S{"."}, tokens, tokenize_options::trim_empty);
        REQUIRE(tokens.size() == 0);
        tokens.clear();
        tokenize(str, S{"."}, tokens);
        REQUIRE(tokens.size() == 2);
        CHECK(tokens.at(0).empty());
        CHECK(tokens.at(1).empty());

        tokens.clear();
        str = S{"1.0"};
        tokenize(str, S{"."}, tokens);
        REQUIRE(tokens.size() == 2);

        tokens.clear();
        str = S{"1.2.3"};
        tokenize(str, S{"."}, tokens);
        REQUIRE(tokens.size() == 3);
        CHECK(tokens.at(0) == "1");
        CHECK(tokens.at(1) == "2");
        CHECK(tokens.at(2) == "3");
    }

    SECTION("join/split") {
        auto v = C{S{"hello"}, S{"world"}};
        auto s = join(v, S{" "});
        CHECK(s == "hello world");

        v = C{S{"a"}, S{"test"}, S{"list"}};
        s = join(v, S{","});
        CHECK(s == "a,test,list");

        CHECK(split(s, S{","}) == v); // split is just an alias for tokenize, so this single test suffices

        v = {};
        s = join(v, S{" "});
        CHECK(s.empty());
    }

    SECTION("trim") {
        CHECK(trim(S{"Hello"}) == "Hello");

        auto s = S{"  Hello World\n"};
        trim(s);
        CHECK(s == "Hello World");

        s = S{"  \t\tHello World\r\n"};
        trim(s);
        CHECK(s == "Hello World");

        const auto sc = S{"  \t\tHello World\r\n"};
        CHECK(trim(sc) == "Hello World");
    }

    SECTION("replace_all") {
        S s("a,b,c");
        CHECK(replace_all(s, S{","}, S{"."}) > 0);
        CHECK(s == "a.b.c");
        CHECK(replace_all(s, S{"."}, S{".."}) > 0);
        CHECK(s == "a..b..c");
        s = S{"a,b,c"};
        CHECK_FALSE(replace_all(s, S{"d"}, S{"e"}) > 0);
        CHECK(replace_all(s, S{","}, S{"."}, 2) > 0);
        CHECK(s == "a,b.c");
        s = S{"a,b,c"};
        CHECK(replace_all(s, S{","}, S{"."}, 1) > 0);
        CHECK(s == "a.b.c");
    }

    SECTION("for_each_line") {
#if !PS_PREFERRED_CPP14
        using iterator_type = decltype(S{}.begin());
#endif

        S results;
        size_t count;
        auto cb = [&results,&count]
#if PS_PREFERRED_CPP14
        (auto first, auto last)
#else
        (iterator_type first, iterator_type last)
#endif
        {
            if (std::distance(first, last) > 0) {
                results.append(S{first, last});
            }
            ++count;
        };

        auto clear = [&results,&count]() {
            results.clear();
            count = 0;
        };

        S s;

        clear();
        for_each_line(s, cb);
        CHECK(results == "");
        CHECK(count == 0);

        clear();
        s = S{"abc"};
        for_each_line(s.begin(), s.end(), cb);
        CHECK(results == "abc");
        CHECK(count == 1);

        clear();
        s = S{"a\nb\nc"};
        for_each_line(s.begin(), s.end(), cb);
        CHECK(results == "abc");
        CHECK(count == 3);

        clear();
        s = S{"a\nb\nc\n"};
        for_each_line(s.begin(), s.end(), cb);
        CHECK(results == "abc");
        CHECK(count == 3);

        clear();
        for_each_line(s.begin(), s.end(), cb, for_each_options::want_empty_lines);
        CHECK(results == "abc");
        CHECK(count == 3);

        clear();
        s = S{"\na\nb\nc\n"};
        for_each_line(s.begin(), s.end(), cb, for_each_options::want_empty_lines);
        CHECK(results == "abc");
        CHECK(count == 4);

        clear();
        s = S{"\na\nb\nc\n\n"};
        for_each_line(s.begin(), s.end(), cb, for_each_options::want_empty_lines);
        CHECK(results == "abc");
        CHECK(count == 5);

        clear();
        for_each_line(s, cb, for_each_options::none, '\r');
        CHECK(results == s);
        CHECK(count == 1);

#if PS_PREFERRED_CPP14
        clear();
        for_each_line((char*)0, (char*)0, cb);
        CHECK(results == "");
        CHECK(count == 0);

        clear();
        const S cs{s};
        for_each_line(cs, cb, for_each_options::none, '\r');
        CHECK(results == s);
        CHECK(count == 1);
#endif
    }
}
