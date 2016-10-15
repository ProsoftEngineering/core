// Copyright © 2014-2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <stdexcept>
#include <cstring>

#include <byteorder.h>
#include <u8string/u8string.hpp>

#include "catch.hpp"

using namespace prosoft;

namespace {
const char* u8test = "\xE2\x84\xAB\xE1\xBA\xA1\xC4\x81\xE2\x80\xA6"; // 4 u16 codepoints (see u16test), u8 precomposed literal
const u16string::value_type u16test[] = {0x212b, 0x1ea1, 0x0101, 0x2026, 0}; // ditto, but a u16 literal
const u16string::value_type u32test[] = {0xD840, 0xDC0B, 0}; // 1 u32 codepoint (0x0002000B), u16 literal

template <typename T>
void test_ascii_compare() {
    CHECK(T("a") < T("b"));
    CHECK(T("b") > T("a"));
    CHECK(T("hello") < T("world"));
    CHECK(T("world") > T("hello"));
    std::set<T> s;
    s.insert(T("a"));
    s.insert(T("b"));
    CHECK(s.size() == 2);
    s.clear();
    s.insert(T("hello"));
    s.insert(T("world"));
    CHECK(s.size() == 2);
    CHECK(T("a") <= T("a"));
    CHECK(T("a") >= T("a"));
    CHECK(T("a") <= T("b"));
    CHECK(T("b") >= T("a"));
    CHECK(T("hello") <= T("hello"));
    CHECK(T("hello") >= T("hello"));
    CHECK(T("hello") <= T("world"));
    CHECK(T("world") >= T("hello"));
    CHECK(T("a") == T("a"));
    CHECK(T("a") != T("b"));
    CHECK(T("hello") == T("hello"));
    CHECK(T("hello") != T("world"));
}
} // anon

TEST_CASE("u8string") {

    SECTION("construction") {
        // default
        u8string s;
        CHECK(s.empty());
        CHECK(s.is_ascii());

        // std::string
        std::string ss("ascii");
        u8string s2(ss);
        CHECK(s2.is_ascii());
        CHECK(ss == s2.str());
        CHECK(s2.data_size() == s2.length());

        // const char*
        u8string s3(ss.c_str());
        CHECK(std::strlen(ss.c_str()) == s3.length());
        CHECK(s3.data_size() == s3.length());
        s3 = u8string{"test", 0}; // cstr conversion implicit length of 0 asks to get to the length from the string itself
        CHECK(!s3.empty());

        // unicode literals
        ss = u8test;
        u8string s4(ss);
        CHECK_FALSE(s4.is_ascii());
        CHECK(s4 == ss);
        CHECK((s4.length() >= 4 && s4.length() < s4.data_size()));

        u8string s5(ss.c_str());
        CHECK(s5 == ss);
        CHECK((s5.length() >= 4 && s5.length() < s5.data_size()));

        u16string us(u16test);
        u8string s6(us);
        CHECK_FALSE(s6.is_ascii());
        CHECK(s6.length() >= us.length());
        CHECK((s6.length() >= 4 && s6.length() < s6.data_size()));

        us = u32test;
        u8string s7(us);
        CHECK_FALSE(s7.is_ascii());
        CHECK(us.length() > s7.length());
        CHECK((s7.length() >= 1 && s7.length() < s7.data_size()));

        const u8string::value_type u32codepoint = 0x0002000BU;
        u8string s8(u32string(1, u32codepoint));
        CHECK_FALSE(s8.is_ascii());
        CHECK(s8.length() == 1);
        CHECK(s8[0] == u32codepoint);
        auto u32 = unicode::u32(s8);
        CHECK(u32.size() == 1);
        CHECK(u32[0] == u32codepoint);

        u8string s9(u16test);
        CHECK_FALSE(s9.is_ascii());
        CHECK(s9.length() >= 4);

        {
            u8string s10(u32codepoint);
            CHECK_FALSE(s10.is_ascii());
            CHECK(s10.length() >= 1);
        }

        {
            const decltype(u32codepoint) a[] = {u32codepoint, u32codepoint, u32codepoint, 0};
            u8string s11(a);
            CHECK_FALSE(s11.is_ascii());
            CHECK(s11.length() >= 3);
        }

        u8string c1(s2);
        CHECK(c1 == s2);
        u8string c2(s6);
        CHECK(c2 == s6);

        u8string m1(std::move(c1));
        CHECK((m1 == s2 && c1.empty()));

        s.clear();
        s.reserve(1024);
        CHECK(s.capacity() >= 1024);

        // bad utf8
        ss = "\x0C5"; // ISO 8859-1 capital Angstrom
        CHECK_THROWS_AS(u8string sbad(ss), u8string::invalid_utf8);

        uint8_t data[] = {'a', 'b', 'c', 0, 0, 'd', 'e', 'f'};
        u8string n1(data, sizeof(data));
        CHECK(sizeof(data) == n1.data_size());
        CHECK(n1.is_ascii());

        data[sizeof(data) - 2] = 0xC2;
        data[sizeof(data) - 1] = 0xA1;
        u8string n2(data, sizeof(data));
        CHECK(sizeof(data) == n2.data_size());
        CHECK((sizeof(data) - 1) == n2.length());
        CHECK_FALSE(n2.is_ascii());

        CHECK_THROWS(u8string{(const char*)nullptr});
        CHECK_THROWS(u8string{(const uint8_t*)nullptr});
        CHECK_THROWS(u8string{(u16string::const_pointer) nullptr});

#if PS_HAVE_UDL
        s = "\xC3\xA9"_u8;
        CHECK(s.length() == 1);
        CHECK_FALSE(s.is_ascii());
#endif
    }

    SECTION("assignment") {
        u8string s(u8test);
        CHECK((s.length() >= 4 && s.length() < s.data_size()));

        u16string s16 = u16test;
        s = s16;
        CHECK_FALSE(s.is_ascii());
        CHECK((s.length() >= 4 && s.length() < s.data_size()));

        s16 = u32test;
        s = s16;
        CHECK_FALSE(s.is_ascii());
        CHECK((s.length() >= 1 && s.length() < s.data_size()));

        u8string s2;
        s2 = s;
        CHECK(s2 == s);
        CHECK(s2.is_ascii() == s.is_ascii());

        s2.clear();
        CHECK(s2.empty());
        CHECK(s2.is_ascii());

        auto len = s.length();
        auto wasASCII = s.is_ascii();
        s2 = std::move(s);
        CHECK((s.empty() && s2.length() == len));
        CHECK(s2.is_ascii() == wasASCII);

        s2.clear();
        s = as_utf8(u8test);
        len = s.length();
        wasASCII = s.is_ascii();
        s2.swap(s);
        CHECK((s.empty() && s2.length() == len));
        CHECK(s2.is_ascii() == wasASCII);

        s = u8string("abc");
        CHECK(s.is_ascii());

        // ISO 8859-1 capital Angstrom
        CHECK_THROWS_AS(s = as_utf8("\x0C5"), u8string::invalid_utf8);
    }

    SECTION("compare") {
        CHECK(u8string::bom == u8string::bom);

        CHECK(u8string() == u8string());

        u8string s(u8test);
        CHECK(s == u8test);
        CHECK(s == std::string(u8test));
        CHECK(s != "\x0C5"); // ISO 8859-1 capital Angstrom

        u8string prefix("/te");
        u8string path("/test/path");
        CHECK(0 == path.compare(0, prefix.length(), prefix));
        CHECK(0 != path.compare(1, prefix.length(), prefix));

        prefix = as_utf8("/2/test/path");
        CHECK(0 == path.compare(1, path.length() - 1, prefix, 3, u8string::npos));

        s = u8string("ICASE COMPARE");
        CHECK(0 == s.compare(u8string("icase compare"), true));

        s = u8string("\xE1\xBA\xA0");
        CHECK(0 == s.compare(u8string("\xE1\xBA\xA1"), true));

        // Capital Sharp S added in 5.1, should be the uppercase form of the small "sharp s". (Prior to 2010 it was common, though not standard, to use "SS".)
        s = u8string("\xE1\xBA\x9E");
        CHECK(0 == s.compare(u8string("\xC3\x9F"), true));

        s = u8string("aaa");
        CHECK(-1 == s.compare(u8string("aaaa"), false));
        CHECK(-1 == s.compare(u8string("AAAA"), true));

        CHECK(s == "aaa");
        CHECK("aaa" == s);
        CHECK(s == std::string("aaa"));
        CHECK(std::string("aaa") == s);
        CHECK(s != "AAA");
        CHECK("AAA" != s);
        CHECK(s != std::string("AAA"));
        CHECK(std::string("AAA") != s);

        CHECK(s < u8string("aaaa"));
        CHECK(u8string("aaaa") > s);

        s = u8string("bbb");
        CHECK(s > u8string("aaaa"));
        CHECK(s < u8string("bbba"));
        CHECK(s >= u8string("bbb"));
        CHECK(s <= u8string("bbb"));

        const char* actue_aaa = "\xC3\xA1\xC3\xA1\xC3\xA1"; // precomposed
        const char* actue_AAA = "\xC3\x81\xC3\x81\xC3\x81"; // ditto
        s = u8string(actue_aaa); // precomposed acute a
        CHECK(s == actue_aaa);
        CHECK(actue_AAA != s);
        CHECK(0 == s.compare(u8string(actue_AAA), true));

        CHECK(s > u8string("\xC3\xA0\xC3\xA0\xC3\xA0\xC3\xA0")); // precomposed grave a
        CHECK(s < (s + u8string("b")));
        CHECK(s >= u8string(actue_aaa));
        CHECK(s <= u8string(actue_aaa));

        std::equal_to<u8string> eq;
        CHECK(eq(u8string("aaa"), u8string("aaa")));
        CHECK_FALSE(eq(u8string("aaa"), u8string("AAA")));

        std::hash<u8string> hash;
        CHECK(hash(u8string("aaa")) == hash(u8string("aaa")));

        s = u8string("short");
        prefix = u8string("longer");
        CHECK(s.length() < prefix.length());
        CHECK_FALSE(s == prefix);
        CHECK(0 != s.compare(0, prefix.length(), prefix));

        s = u8string("sh\xC3\x86rt");
        prefix = u8string("longer");
        CHECK(s.length() < prefix.length());
        CHECK_FALSE(s == prefix);
        CHECK(0 != s.compare(0, prefix.length(), prefix));
        CHECK(0 == s.compare(0, 2, s, 0, 2));

        // test with std::string to ensure same behavior
        test_ascii_compare<std::string>();
        test_ascii_compare<u8string>();
    }

    SECTION("iteration") {
        u8string s(u16test);

        // For this test we are assuming the bytes stay the same once converted. The raw test string is precomposed.
        CHECK(0 == u8string::compare(u16test[0], s[0]));
        CHECK(0 == u8string::compare(u16test[1], s[1]));
        CHECK(0 == u8string::compare(u16test[2], s[2]));
        CHECK(0 == u8string::compare(u16test[3], s[3]));

        CHECK_FALSE(s.is_valid(s[s.length() + 1]));

        CHECK(s.front() == s[0]);
        CHECK(s.back() == s[s.length() - 1]);

        auto i = s.cbegin();
        CHECK(0 == u8string::compare(u16test[0], *i++));
        CHECK(0 == u8string::compare(u16test[1], *i++));
        CHECK(0 == u8string::compare(u16test[2], *i++));
        CHECK(0 == u8string::compare(u16test[3], *i++));
        CHECK((s.length() > 4 || i == s.cend()));

        auto ri = s.crbegin();
        CHECK(0 == u8string::compare(u16test[3], *ri++));
        CHECK(0 == u8string::compare(u16test[2], *ri++));
        CHECK(0 == u8string::compare(u16test[1], *ri++));
        CHECK(0 == u8string::compare(u16test[0], *ri++));
        CHECK((s.length() > 4 || ri == s.crend()));

        // http://www.unicode.org/faq/char_combmark.html
        static const char32_t u[] = {0x61U, 0x928U, 0x93FU, 0x4E9CU, 0x10083U};
        s = u8string(u32string(u, 5));
        CHECK(s.length() == 5);
        {
            auto start = s.cbegin();
            auto fin = s.cend();
            --fin;
            u8string s2(start, fin);
            CHECK(s2.length() == s.length() - 1);
            CHECK(0 == s2.compare(s.substr(0, 4)));
        }
        
        {
            auto start = s.begin();
            auto fin = s.end();
            --fin;
            u8string s2(start, fin);
            CHECK(s2.length() == s.length() - 1);
            CHECK(0 == s2.compare(s.substr(0, 4)));
        }
        
        WHEN("iterator is constructed with an invalid range") {
            auto data = s.str();
            auto start = ++data.begin();
            THEN("an exception is thrown") {
                CHECK_THROWS(make_ranged_iterator<u8string::iterator>(data.begin(), start, data.end()));
            }
        }
        
        // MSVC checked iterators will fire their own assert in Debug builds and abort.
#if !_MSC_VER || !DEBUG
        WHEN("iterators of different strings are compared") {
            u8string s1{"test"};
            u8string s2{"test2"};
            auto a = s1.begin();
            auto b = s2.begin();
            THEN("an exception is thrown") {
                CHECK_THROWS(void(a == b));
            }
        }
#endif
    }

    SECTION("find") {
        u8string s("Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum");
        u8string needle("Lorem");
        CHECK(0 == s.find(needle));
        CHECK(0 == s.rfind(needle));
        CHECK(0 == s.find(needle.str())); // test conversion
        CHECK(0 == s.rfind(needle.str())); // ditto

        needle = u8string("sed do");
        auto pos = s.str().find(needle.str());
        CHECK(pos == s.find(needle, pos - 1));
        CHECK(pos == s.rfind(needle, pos + needle.length()));
        CHECK(u8string::npos == s.rfind(needle, pos - needle.length()));

        s = u8string(u8test);
        CHECK_FALSE(s.is_ascii());
        needle = s;
        CHECK_FALSE(needle.is_ascii());
        CHECK(0 == s.find(needle));
        CHECK(0 == s.rfind(needle));

        needle = s.substr(1);
        CHECK(needle.length() == (s.length() - 1));
        CHECK_FALSE(needle.is_ascii());
        CHECK(1 == s.find(needle, 1));
        needle = needle.substr(1, 2);
        CHECK(u8string::npos == s.rfind(needle, 1));
        needle = s.substr(1, 2);
        CHECK(1 == s.rfind(needle, 3));

        s = u8string("abcdefghijklmnopqrstuvwxyz");
        CHECK(0 == s.find('a'));
        CHECK(2 == s.find('c', 1));
        CHECK(25 == s.rfind('z'));
        CHECK(22 == s.rfind('w', 24));
        CHECK(22 == s.rfind('w', 22));
        CHECK(22 == s.rfind("w", 22));

        CHECK(0 == s.find_first_of("abc"));
        CHECK(1 == s.find_first_of("Abc"));
        CHECK(2 == s.find_last_of("Abc"));
        CHECK(2 == s.find_first_of("abcd", 2));

        CHECK(25 == s.find_last_of("zyx"));
        CHECK(24 == s.find_last_of("Zyx"));
        CHECK(23 == s.find_last_of("zyxw", 23));

        CHECK(0 == s.find_first_of('a'));
        CHECK(25 == s.find_last_of('z'));

        CHECK((u8string::npos == s.find("A", 1)));
        CHECK((u8string::npos == s.find("Z", s.length() - 1)));
        CHECK((u8string::npos == s.find('A', 1)));
        CHECK((u8string::npos == s.find('Z', s.length() - 1)));
        CHECK((u8string::npos == s.find_first_of("A", 1)));
        CHECK((u8string::npos == s.find_first_of("Z", s.length() - 1)));
        CHECK((u8string::npos == s.find_first_of('A', 1)));
        CHECK((u8string::npos == s.find_first_of('Z', s.length() - 1)));

        CHECK((u8string::npos == s.rfind("A", 1)));
        CHECK((u8string::npos == s.rfind("Z", s.length() - 1)));
        CHECK((u8string::npos == s.rfind('A', 1)));
        CHECK((u8string::npos == s.rfind('Z', s.length() - 1)));
        CHECK((u8string::npos == s.find_last_of("A", 1)));
        CHECK((u8string::npos == s.find_last_of("Z", s.length() - 1)));
        CHECK((u8string::npos == s.find_last_of('A', 1)));
        CHECK((u8string::npos == s.find_last_of('Z', s.length() - 1)));

        CHECK((u8string::npos == s.find("A", s.length())));
        CHECK((u8string::npos == s.rfind("A", s.length())));
        CHECK((u8string::npos == s.find_first_of("A", s.length())));
        CHECK((u8string::npos == s.find_last_of("A", s.length())));

        CHECK(s.is_ascii());
        auto len = s.length();
        s += u8string("\xC3\xA9"); // precomposed
        CHECK_FALSE(s.is_ascii());
        CHECK(s.length() == (len + 1));
        CHECK(0 == s.find('a'));
        CHECK(2 == s.find('c', 1));
        CHECK(25 == s.rfind('z'));
        CHECK(u8string::npos == s.rfind('z', 24));
        CHECK(0 == s.find_first_of("abc"));
        CHECK(1 == s.find_first_of("Abc"));
        CHECK(2 == s.find_last_of("Abc"));
        CHECK(0 == s.find_first_of('a'));
        CHECK(25 == s.find_last_of('z'));

        CHECK(26 == s.find("\xC3\xA9"));
        CHECK(26 == s.find_first_of("\xC3\xA9"));
        CHECK(26 == s.find_first_of("\xC3\xA9", 26));
        CHECK(26 == s.find_first_of("\xC3\xA9", 25));
        CHECK(26 == s.find_last_of("\xC3\xA9"));
        CHECK(26 == s.find_last_of("\xC3\xA9", 26));
        CHECK(26 == s.find_last_of("\xC3\xB1\xC3\xA9", 26));
        CHECK(u8string::npos == s.find_last_of("\xC3\xA9", 25));

        CHECK((u8string::npos == s.find('A', 1)));
        CHECK((u8string::npos == s.find('Z', s.length() - 1)));
        CHECK((u8string::npos == s.find_first_of("A", 1)));
        CHECK((u8string::npos == s.find_first_of("Z", s.length() - 1)));

        s = u8string(u32test);
        CHECK(0 == s.find(s[0]));
        CHECK(0 == s.rfind(s[0]));

        s = u8string("Hello World");
        CHECK(4 == s.find("o"));
        CHECK(4 == s.find("o", 4));
        CHECK(7 == s.find("o", 5));
        CHECK(2 == s.find("l"));
        CHECK(3 == s.find("l", 3));
        CHECK(u8string::npos == s.find("hello"));
        CHECK(0 == s.find("Hello"));
        CHECK(6 == s.find("World"));
        CHECK(0 == s.find("hello", 0, u8string::find_options::case_insensitive));
        CHECK(6 == s.find("wOrLd", 0, u8string::find_options::case_insensitive));
        CHECK(1 == s.find('e'));
        CHECK(u8string::npos == s.find('E'));
        CHECK(1 == s.find('E', 0, u8string::find_options::case_insensitive));

        s = u8string("ttest\xEF\xA3\xBF");
        CHECK(0 == s.find("t"));
        CHECK(1 == s.find("test"));
        CHECK(5 == s.find("\xEF\xA3\xBF"));
        CHECK(1 == s.find("t", 1));
        CHECK(4 == s.find("t", 2));
        CHECK(5 == s.find("\xEF\xA3\xBF", 0, u8string::find_options::case_insensitive));
        CHECK(2 == s.find('e'));
        CHECK(u8string::npos == s.find('E'));
        CHECK(2 == s.find('E', 0, u8string::find_options::case_insensitive));
    }

    SECTION("substr") {
        u8string s("abcde");

        auto ss = s.substr();
        CHECK(ss == "abcde");
        CHECK(ss.is_ascii());

        ss = s.substr(1);
        CHECK(ss == "bcde");
        CHECK(ss.is_ascii());

        ss = s.substr(42);
        CHECK(ss == "");

        ss = s.substr(1, 3);
        CHECK(ss == "bcd");
        CHECK(ss.is_ascii());

        s += u8string("\xC3\xA9"); // precomposed
        ss = s.substr(1, 3);
        CHECK(ss == "bcd");
        CHECK(ss.is_ascii());

        ss = s.substr(3, 3);
        CHECK(ss == "de"
                    "\xC3\xA9");
        CHECK_FALSE(ss.is_ascii());
    }

    SECTION("precomposed and decomposed") {
        u8string pc("Am"
                    "\xC3\xA9"
                    "lie"); // acute e is precomposed as one code point
        u8string dc("Ame"
                    "\xCC\x81"
                    "lie"); // actue e is deomcposed as ASCII 'e' + combining code point
        CHECK(pc == dc);

        std::string raw(decomposed(pc));
        // XXX: this is a backdoor way to get a NFD u8string. This string IS NOT compatible with other u8strings.
        auto i = u8string::iterator(raw.begin(), raw.begin(), raw.end());
        auto j = u8string::iterator(raw.end(), raw.begin(), raw.end());
        u8string nfd(i, j);
        CHECK_FALSE(pc == nfd);
        CHECK_FALSE(dc == nfd);
        CHECK(raw == nfd.str());

        raw = precomposed(dc);
        CHECK(raw == pc.str());
        CHECK(raw == dc.str());
    }

    SECTION("insert") {
        u8string s("Ame"
                   "\xCC\x81"
                   "lie"); // decomposed
        auto len = s.length();
        s.insert(4, u8string("\xC3\xA9")); // precomposed
        CHECK(s.length() == len + 1);
        CHECK(0x000000E9U == s[4]);
        CHECK(s == "Ame"
                   "\xCC\x81"
                   "l"
                   "\xC3\xA9"
                   "ie");

        s = u8string("abc");
        s.insert(2, u8string("d"));
        CHECK(s.is_ascii());
        CHECK(s.length() == 4);
        CHECK(s == "abdc");

        s.insert(3, u8string("\xC3\xA9")); // precomposed
        CHECK_FALSE(s.is_ascii());
        CHECK(s.length() == 5);
        CHECK(s == "abd"
                   "\xC3\xA9"
                   "c");

        u8string::iterator i = s.begin();
        std::advance(i, 4);
        CHECK((char32_t)'c' == *i);
        u8string s2{"zxy"};
        len = s.length();
        i = s.insert(i, s2.cbegin(), s2.cend());
        CHECK(s.length() == len + s2.length());
        CHECK((char32_t)'z' == *i);

        s = as_utf8("Amel");
        CHECK(s.is_ascii());
        i = s.begin();
        std::advance(i, 3);
        CHECK((char32_t)'l' == *i);
        s2 = as_utf8("\xCC\x81"
                     "lie"); // decomposed
        i = s.insert(i, s2.cbegin(), s2.cend());
        CHECK(!s.is_ascii());
        CHECK(0x000000E9U == *i);
        CHECK(s == "Am"
                   "\xC3\xA9"
                   "liel");

        s = as_utf8("abce");
        i = s.begin();
        std::advance(i, 3);
        CHECK((char32_t)'e' == *i);
        i = s.insert(i, 'd');
        CHECK((char32_t)'d' == *i);
        CHECK(s == "abcde");

        s = as_utf8("Amel");
        CHECK(s.is_ascii());
        i = s.begin();
        std::advance(i, 3);
        CHECK((char32_t)'l' == *i);
        i = s.insert(i, 0x000000E9U);
        CHECK(0x000000E9U == *i);
        CHECK(s == "Ame\xC3\xA9l");
    }

    SECTION("append") {
        u8string s("hello");
        s += u8string("world");
        CHECK(s == "helloworld");

        s = s + u8string("2.0");
        CHECK(s == "helloworld2.0");

        u8string a("Ame");
        u8string b("\xCC\x81");
        u8string c("lie"); // decomposed components
        CHECK((a + b + c) == "Am"
                             "\xC3\xA9"
                             "lie"); // precomposed

        s.clear();
        s.push_back(0x0002000BU);
        CHECK_FALSE(s.is_ascii());
        CHECK(s.length() == 1);

        s.push_back('a');
        CHECK_FALSE(s.is_ascii());
        CHECK(s.length() == 2);

        s.clear();
        s.push_back('a');
        CHECK(s.is_ascii());
        CHECK(s.length() == 1);

        // test push_back() on a new string
        u8string indent;
        indent.push_back('\t');
        CHECK(indent.length() == 1);
        indent.push_back('\t');
        CHECK(indent.length() == 2);
        CHECK(indent.is_ascii());
    }

    SECTION("erase") {
        const char* estr = "Ame"
                           "\xCC\x81"
                           "l"
                           "\xC3\xA9"
                           "ie"; // decomposed and precomposed sequences
        u8string s(estr);
        auto len = s.length();
        s.erase(4, 1);
        CHECK(s.length() == len - 1);
        CHECK(0x00000069U == s[4]);
        CHECK(s == "Ame"
                   "\xCC\x81"
                   "lie"); // decomposed

        s.erase();
        CHECK(s.empty());

        s = as_utf8(estr);
        auto i = s.begin();
        std::advance(i, 4);
        auto j = i;
        std::advance(j, 1);
        i = s.erase(i, j);
        CHECK(s.length() == len - 1);
        CHECK(0x00000069U == s[4]);
        CHECK(s == "Ame"
                   "\xCC\x81"
                   "lie");
        CHECK((char32_t)'i' == *i);

        for (i = s.begin(), j = s.end(); i != j; j = s.end()) {
            i = s.erase(i);
        }
        CHECK(s.empty());

        s = u8string("abcd");
        s.erase(1, 2);
        CHECK(s.is_ascii());
        CHECK(s.length() == 2);
        CHECK(s == "ad");
    }

    SECTION("pop_back") {
        u8string s;

        s = u8string("test");
        s.pop_back();
        CHECK(s == "tes");
        s.pop_back();
        CHECK(s == "te");
        s.pop_back();
        CHECK(s == "t");
        s.pop_back();
        CHECK(s.empty());

        s = u8string("Ame"
                     "\xCC\x81"
                     "l"
                     "\xC3\xA9"
                     "ie"); // decomposed and precomposed sequences
        CHECK_FALSE(s.is_ascii());
        s.pop_back();
        CHECK(s == "Ame"
                   "\xCC\x81"
                   "l"
                   "\xC3\xA9"
                   "i");
        s.pop_back();
        CHECK(s == "Ame"
                   "\xCC\x81"
                   "l"
                   "\xC3\xA9");
        s.pop_back();
        CHECK(s == "Ame"
                   "\xCC\x81"
                   "l");
        s.pop_back();
        CHECK(s == "Ame"
                   "\xCC\x81");
        s.pop_back();
        CHECK(s == "Am");
        s.pop_back();
        CHECK(s == "A");
        s.pop_back();
        CHECK(s.empty());
        s.pop_back();
        CHECK(s.empty());
        CHECK(s.is_ascii());
    }

    SECTION("replace") {
        // UTF
        u8string s("Ame"
                   "\xCC\x81"
                   "l"
                   "\xC3\xA9"
                   "ie"); // decomposed and precomposed sequences
        auto len = s.length();
        s.replace(2, 3, u8string("el"));
        CHECK(s.length() == len - 1);
        CHECK(0x00000065U == s[2]);
        CHECK(s == "Amelie");
        // the string is ASCII now, but replace does not currently mark it as such, just assert that assumption
        CHECK_FALSE(s.is_ascii());
        CHECK(u8string::is_ascii(s.str()));

        len = s.length();
        s.replace(2, 1, u8string("\xC3\xA9"));
        CHECK(s.length() == len);
        CHECK(0x000000E9U == s[2]);
        CHECK(s == "Ame"
                   "\xCC\x81"
                   "lie"); // decomposed
        CHECK_FALSE(s.is_ascii());

        s.replace(3, u8string::npos, u8string());
        CHECK(s == "Ame"
                   "\xCC\x81"); // decomposed

        CHECK_THROWS_AS(s.replace(s.length() + 1, 1, u8string()), std::out_of_range);
        s.replace(s.length(), 1, u8string("lie"));
        CHECK(s == "Ame"
                   "\xCC\x81"
                   "lie"); // decomposed

        u8string::iterator i = s.begin();
        std::advance(i, 2);
        CHECK(0x000000E9U == *i);
        auto j = i;
        std::advance(j, 1);
        u8string s2{"abcdeifg3"};
        s.replace(i, j, s2.cbegin(), (--s2.cend()));
        CHECK(s == "Amabcdeifglie");

        // ASCII
        s = u8string("abcd");
        s.replace(2, 2, u8string("eiou"));
        CHECK(s.is_ascii());
        CHECK(s.length() == 6);
        CHECK(s == "abeiou");

        s.replace(2, u8string::npos, u8string("c"));
        CHECK(s == "abc");

        CHECK_THROWS_AS(s.replace(s.length() + 1, 1, u8string()), std::out_of_range);

        s.replace(s.length(), 1, u8string("d"));
        CHECK(s == "abcd");

        // ASCII to UTF
        s = u8string("hi");
        CHECK(s.is_ascii());
        s.replace(1, 1, u8string("\xC3\xA9"));
        CHECK(s == "h\xC3\xA9");
        CHECK_FALSE(s.is_ascii());

        // UTF to ASCII
        s = u8string("\xC3\xA9");
        CHECK_FALSE(s.is_ascii());
        s.replace(1, 1, u8string("hi"));
        CHECK(s == "\xC3\xA9hi");
        CHECK_FALSE(s.is_ascii());
    }

    SECTION("grapheme clusters") {
        // grapheme clusters are not currently natively supported
        u8string s("\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"); // US flag icon, a cluster composed of U+1F1FA and U+1F1F8
        CHECK(s.size() == 2); // a native implementation using clusters would count this as 1 "character"
    }

    SECTION("external data") {
        bool ascii;
        CHECK((u8string::is_valid("abc", &ascii) && ascii));
        CHECK((u8string::is_valid(std::string("abc"), &ascii) && ascii));
        CHECK((u8string::is_valid('a', &ascii) && ascii));
        CHECK(u8string::is_ascii(std::string("abc")));

        CHECK((u8string::is_valid("abc"
                                  "\xCC\x81",
                   &ascii) && !ascii));
        CHECK((u8string::is_valid(std::string("abc"
                                              "\xCC\x81"),
                   &ascii) && !ascii));
        CHECK((u8string::is_valid(0x000000E9, &ascii) && !ascii));

        CHECK_FALSE(u8string::is_valid("abc"
                                       "\x0C5"));
        CHECK_FALSE(u8string::is_valid(std::string("abc"
                                                   "\x0C5")));
        CHECK_FALSE(u8string::is_valid(0xffffffffU));
        CHECK_FALSE(u8string::is_ascii("abc"
                                       "\x0C5"));

        // endian tests -- u8string only supports native-endian u16 and u32 currently
        u8string native(u16test);
        native.erase(2, u8string::npos);
        CHECK(native.length() == 2);
        u16string::value_type u16_reversed[] = {byteswap16(u16test[0]), byteswap16(u16test[1]), 0, 0};
        u8string s(u16_reversed);
        CHECK_FALSE(s == native);

        // TODO: u8string should handle UTF16 (and 32) properly when a BOM is present
        u16_reversed[0] = byteswap16(static_cast<uint16_t>(0xFEFF));
        u16_reversed[1] = byteswap16(u16test[0]);
        u16_reversed[2] = byteswap16(u16test[1]);
        CHECK(u16_reversed[3] == 0);
        // XXX: prior to utf8proc 1.3, utf8cpp would create bad utf8 and utf8proc would error during normalization, causing an invalid_utf8 exception.
        // However, utf8proc 1.3 implements Unicode 8, which has expended the number of codepoints so it no longer errors.
        s = u16string(u16_reversed);
        CHECK_FALSE(s == native);
    }

    SECTION("BOM") {
        auto s = u8string::bom;
        CHECK((3 == s.data_size() && 1 == s.length()));
        CHECK(s.has_bom());
        CHECK_FALSE(u8string("abc").has_bom());

        u16string u16(u16test);
        u16.insert(0, 1, static_cast<u16string::value_type>(0xFEFF));
        s = u16;
        // TODO: should remove u16 BOM
        CHECK(s.length() == 5);
    }
}
