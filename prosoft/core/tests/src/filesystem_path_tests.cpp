// Copyright © 2015-2016, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <cstring>

#include <filesystem/filesystem.hpp>

#include "catch.hpp"

// The spec does not define a gloabl operator+ for path. Most likely because append ignores separators and it would be too easy
// to make a mistake when building a path. We define one for testing simply to make odd construction patterns a bit easier.
namespace {
prosoft::filesystem::path operator+(const prosoft::filesystem::path& p1, const prosoft::filesystem::path& p2) {
    return prosoft::filesystem::path{p1} += p2;
}
}

using namespace prosoft;

TEST_CASE("filesystem path") {
    using path = filesystem::path;
    using string = path::string_type;
    
    const auto test_string = PS_TEXT("test");
    
    WHEN("path is default constructed") {
        path empty;
        THEN("path is empty") {
            CHECK(empty.empty());
        }
    }
    
    WHEN("path is constructed with a string") {
        path p{test_string};
        THEN("path contains string") {
            CHECK(p.native() == test_string);
        }
    }
    
    WHEN("path is constructed with another path") {
        path p{test_string};
        path p2{p};
        THEN("path contains contents of input path") {
            CHECK(p2.native() == p.native());
        }
    }
    
    WHEN("path is constructed with a temporary path") {
        path p{test_string};
        path p2{std::move(p)};
        THEN("path contains contents of input path and input path is empty") {
            CHECK(p2.native() == test_string);
            CHECK(p.empty());
        }
    }
    
    WHEN("path is constructed with input range") {
        const u8string us{test_string};
        const auto& s = us.str();
        path p{s.begin(), s.end()};
        THEN("path contains contents of input range") {
            CHECK(p.native() == test_string);
        }
    }
    
    // Extensions
    WHEN("path is constructed with a rvalue string") {
        path::string_type s{test_string};
        path p{std::move(s)};
        THEN("path contains string and source string is empty") {
            CHECK(p.native() == test_string);
            CHECK(s.empty());
        }
    }
    
    using namespace filesystem::literals::path_literals;
    WHEN("path is constructed via the path UDL") {
        const auto p = "test"_p;
        THEN("path contains string") {
            CHECK(p.native() == test_string);
        }
    }
    
    SECTION("assign") {
        path p{"aaa"};
        
        WHEN("path is assigned another path")
        {
            path p2;
            p2 = p;
            THEN("path contains contents of input path") {
                CHECK(p.native() == p2.native());
            }
        }
        
        WHEN("path is assigned a temporary path") {
            path p2{p};
            THEN("path contains contents of input path and input path is empty") {
                const auto expected = p.native();
                p.clear();
                CHECK(p.empty());
            
                p = std::move(p2);
                CHECK(p.native() == expected);
                CHECK(p2.empty());
            }
        }
        
        WHEN("path is assigned a string") {
            p = test_string;
            THEN("path contains contents of string") {
                CHECK(p.native() == test_string);
            }
        }
        
        WHEN("path assign() is given a string") {
            p.assign(test_string);
            THEN("path contains contents of string") {
                CHECK(p.native() == test_string);
            }
        }
        
        WHEN("path assign is given an input range") {
            p.clear();
            CHECK(p.empty());
            
            const u8string us{test_string};
            const auto& s = us.str();
            p.assign(s.begin(), s.end());
            THEN("path contains contents of input range") {
                CHECK(p.native() == test_string);
            }
        }
    }
    
    SECTION("append") {
        const string separator{path::preferred_separator};
        
        path p;
        path comp = PS_TEXT("test");
        string expect;
        WHEN("path ends in a separator") {
            p = string{PS_TEXT("folder")} + separator;
            THEN("a new separator is not appended") {
                expect = p.native() + comp.native();
                p /= comp;
                CHECK(p.native() == expect);
                
                p = p.native() + separator;
                expect += separator + comp.native();
                p.append(comp.native());
                CHECK(p.native() == expect);
                
                p = p.native() + separator;
                expect += separator + comp.native();
                p.append(comp.native().begin(), comp.native().end());
                CHECK(p.native() == expect);
            }
        }
        
        WHEN("component starts with a separator") {
            comp = separator + string{PS_TEXT("test")};
            THEN("a new separator is not appended") {
                expect = p.native() + comp.native();
                p /= comp;
                CHECK(p.native() == expect);
                
                expect = p.native() + comp.native();
                p.append(comp.native());
                CHECK(p.native() == expect);
                
                expect = p.native() + comp.native();
                p.append(comp.native().begin(), comp.native().end());
                CHECK(p.native() == expect);
            }
        }
        
        WHEN("path is empty") {
            p.clear();
            THEN("resulting path is relative") {
                expect = comp.native();
                p /= comp;
                CHECK(p.native() == expect);
                
                p.clear();
                p.append(comp.native());
                CHECK(p.native() == expect);
                
                p.clear();
                p.append(comp.native().begin(), comp.native().end());
                CHECK(p.native() == expect);
            }
        }
        
        WHEN("component is empty") {
            p = "folder/test";
            comp.clear();
            THEN("resulting path is unchanged") {
                expect = p.native();
                p /= comp;
                CHECK(p.native() == expect);
                
                p.append(comp.native());
                CHECK(p.native() == expect);
                
                p.append(comp.native().begin(), comp.native().end());
                CHECK(p.native() == expect);
            }
        }
        
        WHEN("paths are joined with the / operator") {
            p = string{PS_TEXT("folder")} + separator + string{PS_TEXT("test")};
            const path p2 = string{PS_TEXT("folder")} + separator + string{PS_TEXT("test2")};
            THEN("a new path is created with the contents of the input paths as separate components") {
                auto expected = p.native() + separator + p2.native();
                auto joined = p / p2;
                CHECK(joined.native() == expected);
                
                expected = joined.native() + separator + p2.native() + separator + p.native() + separator + p2.native();
                joined = joined / p2 / p / p2;
                CHECK(joined.native() == expected);
            }
        }
    }
    
    SECTION("concat") {
        WHEN("fragment is a path") {
            path p{"/folder/test"};
            const path p2{"/folder2/test"};
            const auto expected = p.native() + p2.native();
            p += p2;
            THEN("path is previous contents + fragment contents") {
                CHECK(p.native() == expected);
            }
        }
        
        WHEN("fragment is a native string") {
            path p{"/folder/test"};
            const string s{PS_TEXT("/folder2/test")};
            const auto expected = p.native() + s;
            p += s;
            THEN("path is previous contents + fragment contents") {
                CHECK(p.native() == expected);
            }
        }
        
        WHEN("fragment is a null-term native character array") {
            path p{"/folder/test"};
            const string s{PS_TEXT("/folder2/test")};
            const auto expected = p.native() + s;
            p += s.c_str();
            THEN("path is previous contents + fragment contents") {
                CHECK(p.native() == expected);
            }
        }
        
        WHEN("fragment is a native character") {
            path p{"/folder/test"};
            const path::value_type c = path::preferred_separator;
            const auto expected = p.native() + string{c};
            p += c;
            THEN("path is previous contents + fragment contents") {
                CHECK(p.native() == expected);
            }
        }
        
        WHEN("fragment is a non-native string") {
            path p{"/folder/test"};
            const u32string s(3, 0x0002000BU);
            auto expected = p.native() + from_u8string<string>{}(u8string{s});
            p += s;
            THEN("path is previous contents + fragment contents") {
                CHECK(p.native() == expected);
                
                expected += from_u8string<string>{}(u8string{s});
                p.concat(s);
                CHECK(p.native() == expected);
            }
        }
        
        WHEN("fragment is a non-native character") {
            path p{"/folder/test"};
            const u16string::value_type c = 0x2026;
            const auto expected = p.native() + string{c};
            p += c;
            THEN("path is previous contents + fragment contents") {
                CHECK(p.native() == expected);
            }
        }
        
        WHEN("fragment is a range") {
            path p{"/folder/test"};
            const u8string us{"/folder2/test"};
            const auto& s = us.str();
            const auto expected = p.native() + from_u8string<string>{}(us);
            p.concat(s.begin(), s.end());
            THEN("path is previous contents + fragment contents") {
                CHECK(p.native() == expected);
            }
        }
    }
    
    SECTION("modify") {
        WHEN("path is cleared") {
            path p{test_string};
            CHECK_FALSE(p.empty());
            p.clear();
            THEN("path is empty") {
                CHECK(p.empty());
            }
        }
        
        WHEN("path contains only non-duplicate preferred separators") {
            path p {PS_TEXT("folder\\test")};
            path p2 {PS_TEXT("folder\\test\\test2")};
            THEN("make_preferred() will not change the path") {
                auto expected = p.native();
                p.make_preferred();
                CHECK(p.native() == expected);
                
                expected = p2.native();
                p2.make_preferred();
                CHECK(p2.native() == expected);
            }
        }
        
        WHEN("path contains non-preferred separators") {
            path p {PS_TEXT("folder//test")};
            path p2 {PS_TEXT("folder//test/test2")};
            THEN("make_preferred() will change all separators to preferred") {
                auto expected = filesystem::sanitize(p.native(), path::preferred_separator_style);
                p.make_preferred();
                CHECK(p.native() == expected);
                
                expected = filesystem::sanitize(p2.native(), path::preferred_separator_style);
                p2.make_preferred();
                CHECK(p2.native() == expected);
            }
        }
        
        WHEN("path contains duplicate preferred separators") {
            path p {PS_TEXT("folder//////test/test2///test3//")};
            THEN("make_preferred() will remove all duplicate separators") {
                const auto expected = filesystem::sanitize(p.native(), path::preferred_separator_style);
                p.make_preferred();
                CHECK(p.native() == expected);
            }
        }
        
        WHEN("path is empty") {
            path p;
            CHECK_FALSE(p.has_filename());
            
            THEN("remove_filename has no effect") {
                p.remove_filename();
                CHECK_FALSE(p.has_filename());
                CHECK(p.empty());
            }
            THEN("replace_filename appends the new component") {
                p.replace_filename(path{PS_TEXT("item.txt")});
                CHECK(p.native() == PS_TEXT("item.txt"));
                CHECK(p.has_filename());
            }
            THEN("replace_extension appends the new extension") {
                p.replace_extension(path{PS_TEXT("txt")});
                CHECK(p.native() == PS_TEXT(".txt"));
                CHECK(p.has_filename());
                CHECK_FALSE(p.has_stem());
                CHECK(p.has_extension());
            }
        }
        
        WHEN("path is not empty") {
            auto p = path{PS_TEXT("/a/b/c/d.txt")}.make_preferred();
            auto rp = path{PS_TEXT("b/c/d.txt")}.make_preferred();
            
            THEN("remove_filename will remove the last path component") {
                CHECK(p.has_filename());
                p.remove_filename();
                CHECK(p.has_filename());
                CHECK(p == path{PS_TEXT("/a/b/c")}.make_preferred());
                CHECK(p.filename().native() == PS_TEXT("c"));
                
                CHECK(rp.has_filename());
                rp.remove_filename();
                CHECK(rp.has_filename());
                CHECK(rp == path{PS_TEXT("b/c")}.make_preferred());
                CHECK(p.filename().native() == PS_TEXT("c"));
            }
            THEN("replace_filename results in the expected new path") {
                CHECK(p.has_filename());
                p.replace_filename(path{PS_TEXT("item.txt")});
                CHECK(p == path{PS_TEXT("/a/b/c/item.txt")}.make_preferred());
                CHECK(p.has_filename());
                CHECK(p.filename().native() == PS_TEXT("item.txt"));
                
                CHECK(rp.has_filename());
                rp.replace_filename(path{PS_TEXT("item.txt")});
                CHECK(rp == path{PS_TEXT("b/c/item.txt")}.make_preferred());
                CHECK(rp.has_filename());
                CHECK(rp.filename().native() == PS_TEXT("item.txt"));
            }
            THEN("replace_extension results in the expected new path") {
                CHECK((p.has_extension()));
                CHECK(p.extension().native() == PS_TEXT(".txt"));
                p.replace_extension(path{PS_TEXT(".data")});
                CHECK(p == path{PS_TEXT("/a/b/c/d.data")}.make_preferred());
                CHECK(p.has_filename());
                CHECK(p.filename().native() == PS_TEXT("d.data"));
                CHECK((p.has_extension()));
                CHECK(p.extension().native() == PS_TEXT(".data"));
                
                CHECK((rp.has_extension()));
                rp.replace_extension(path{PS_TEXT(".data")});
                CHECK(rp == path{PS_TEXT("b/c/d.data")}.make_preferred());
                CHECK(rp.has_filename());
                CHECK(rp.filename().native() == PS_TEXT("d.data"));
                CHECK((rp.has_extension()));
                CHECK(rp.extension().native() == PS_TEXT(".data"));
            }
            
            THEN("replace_filename with an empty component results in the expected new path") {
                p.replace_filename(path{});
                CHECK(p == path{PS_TEXT("/a/b/c")}.make_preferred());
            }
            THEN("replace_extension with an empty extension results in the expected new path") {
                p.replace_extension(path{});
                CHECK(p == path{PS_TEXT("/a/b/c/d")}.make_preferred());
            }
        }
        
        WHEN("path contains a single path component") {
            path p {PS_TEXT("item.txt")};
            THEN("remove_filename will result in an empty path") {
                CHECK(p.has_filename());
                p.remove_filename();
                CHECK_FALSE(p.has_filename());
                CHECK(p.empty());
            }
            THEN("replace_filename results in the expected new path") {
                CHECK(p.has_filename());
                p.replace_filename(path{PS_TEXT("data.txt")});
                CHECK(p.has_filename());
                CHECK(p.filename().native() == PS_TEXT("data.txt"));
                CHECK(p.native() == PS_TEXT("data.txt"));
            }
            THEN("replace_extension results in the expected new path") {
                CHECK(p.has_extension());
                CHECK(p.extension().native() == PS_TEXT(".txt"));
                p.replace_extension(path{PS_TEXT(".data")});
                CHECK(p.has_extension());
                CHECK(p.extension().native() == PS_TEXT(".data"));
                CHECK(p.native() == PS_TEXT("item.data"));
            }
        }
        
        WHEN("path contains a single path component with no extension") {
            path p {PS_TEXT("item")};
            THEN("remove_filename will result in an empty path") {
                CHECK(p.has_filename());
                p.remove_filename();
                CHECK_FALSE(p.has_filename());
                CHECK(p.empty());
            }
            THEN("replace_filename results in the expected new path") {
                CHECK(p.has_filename());
                p.replace_filename(path{PS_TEXT("data.txt")});
                CHECK(p.has_filename());
                CHECK(p.filename().native() == PS_TEXT("data.txt"));
                CHECK(p.native() == PS_TEXT("data.txt"));
            }
            THEN("replace_extension results in the expected new path") {
                CHECK_FALSE(p.has_extension());
                p.replace_extension(path{PS_TEXT("data")});
                CHECK(p.has_extension());
                CHECK(p.extension().native() == PS_TEXT(".data"));
                CHECK(p.native() == PS_TEXT("item.data"));
            }
        }
        
        WHEN("path is posix root") {
            auto p = path{PS_TEXT("/")}.make_preferred();
            THEN("remove_filename will result in an empty path") {
                CHECK(p.has_filename());
                p.remove_filename();
                CHECK_FALSE(p.has_filename());
                CHECK(p.empty());
            }
            THEN("replace_filename results in the expected new path") {
                CHECK(p.has_filename());
                p.replace_filename(path{PS_TEXT(".")});
                CHECK(p.has_filename());
                CHECK(p.filename().native() == PS_TEXT("."));
                CHECK(p.native() == PS_TEXT("."));
            }
            THEN("replace_extension results in the expected new path") {
                CHECK_FALSE(p.has_extension());
                p.replace_extension(path{PS_TEXT("data")});
                CHECK(p.has_extension());
                CHECK(p.extension().native() == PS_TEXT(".data"));
                CHECK(p == path{PS_TEXT("/.data")}.make_preferred());
            }
        }
        
        WHEN("path is swapped with another path") {
            path p {PS_TEXT("folder/test")};
            path p2 {PS_TEXT("folder/test2")};
            const auto expected = p2.native();
            const auto expected2 = p.native();
            p.swap(p2);
            THEN("the contents of the respective paths are swapped") {
                CHECK(p.native() == expected);
                CHECK(p2.native() == expected2);
                
                using std::swap;
                swap(p, p2);
                
                CHECK(p.native() == expected2);
                CHECK(p2.native() == expected);
            }
        }
    }
    
    const bool win32Paths = filesystem::path::preferred_separator_style == filesystem::path_style::windows;
    
    SECTION("iteration") {
        path p;
        path::iterator i;
        
        WHEN("path is empty") {
            i  = p.begin();
            THEN("iterator is empty") {
                CHECK((i == p.end()));
            }
        }
        
        const path::string_type separator{path::preferred_separator};
        const auto testPath = path{separator} / path{PS_TEXT("folder")} / path{PS_TEXT("item.txt")};
        
        WHEN("incrementing a path") {
            p = testPath;
            i = p.begin();
            THEN("each path component is found") {
                CHECK((*i++).native() == separator); // root dir
                CHECK((*i++).native() == PS_TEXT("folder"));
                CHECK((*i++).native() == PS_TEXT("item.txt"));
                CHECK((i == p.end()));
            }
        }
        
        WHEN("incrementing a path containing duplicate path separators") {
            if (!win32Paths) {
                p = PS_TEXT("//folder///item.txt/////////");
            } else {
                p = PS_TEXT(R"(\folder\\\item.txt\\\\\\\\\)"); // XXX: single root dir, otherwise it's a UNC path
            }
            i = p.begin();
            THEN("each path component is found") {
                CHECK((*i++).native() == separator); // root dir
                CHECK((*i++).native() == PS_TEXT("folder"));
                CHECK((*i++).native() == PS_TEXT("item.txt"));
                
                THEN("1+N terminating separators result in a current directory '.' component") {
                    CHECK((*i++).native() == PS_TEXT("."));
                    CHECK((i == p.end()));
                }
            }
        }
        
         WHEN("incrementing a relative path") {
            p  = path{PS_TEXT("folder")} / path{PS_TEXT("item.txt")};
            i = p.begin();
            THEN("each path component is found") {
                CHECK((*i++).native() == PS_TEXT("folder"));
                CHECK((*i++).native() == PS_TEXT("item.txt"));
                CHECK((i == p.end()));
            }
         }
        
         WHEN("incrementing a single-component path") {
            p  = path{PS_TEXT("item.txt")};
            i = p.begin();
            THEN("the path component is found") {
                CHECK((*i++).native() == PS_TEXT("item.txt"));
                CHECK((i == p.end()));
            }
         }
        
         WHEN("incrementing a root only path") {
            p  = path{separator};
            i = p.begin();
            THEN("the path component is found") {
                CHECK((*i++).native() == separator);
                CHECK((i == p.end()));
            }
         }
        
         WHEN("decrementing a root only path") {
            p  = path{separator};
            i = p.end();
            THEN("the path component is found") {
                CHECK((*--i).native() == separator);
                CHECK((i == p.begin()));
            }
         }
        
         WHEN("prefix incrementing a path") {
            p = testPath;
            i = p.begin();
            THEN("each path component is found") {
                CHECK((*i).native() == separator); // root dir
                CHECK((*++i).native() == PS_TEXT("folder"));
                CHECK((*++i).native() == PS_TEXT("item.txt"));
                CHECK((++i == p.end()));
            }
         }
        
         WHEN("decrementing a path terminating with a separator") {
            p = testPath / path{separator};
            i = p.end();
            THEN("each path component is found") {
                CHECK((*i).empty());
                CHECK((*--i).native() == PS_TEXT("."));
                CHECK((*--i).native() == PS_TEXT("item.txt"));
                CHECK((*--i).native() == PS_TEXT("folder"));
                CHECK((*--i).native() == separator); // root dir
                CHECK((i == p.begin()));
                
                THEN("incrementing the same path each path component is found") {
                    CHECK((*i++).native() == separator);
                    CHECK((*i++).native() == PS_TEXT("folder"));
                    CHECK((*i++).native() == PS_TEXT("item.txt"));
                    CHECK((*i++).native() == PS_TEXT("."));
                    CHECK((i == p.end()));
                }
            }
        }
        
        WHEN("both decrementing and incrementing a path terminating with a separator") {
            p = testPath / path{separator};
            i = p.end();
            THEN("components are found in the expected order") {
                CHECK((*--i).native() == PS_TEXT("."));
                CHECK((*--i).native() == PS_TEXT("item.txt"));
                CHECK((*--i).native() == PS_TEXT("folder"));
                CHECK((*++i).native() == PS_TEXT("item.txt"));
                CHECK((*--i).native() == PS_TEXT("folder"));
                CHECK((*++i).native() == PS_TEXT("item.txt"));
                CHECK((*--i).native() == PS_TEXT("folder"));
                CHECK((*--i).native() == separator);
                CHECK((*++i).native() == PS_TEXT("folder"));
                CHECK((*++i).native() == PS_TEXT("item.txt"));
                CHECK((*++i).native() == PS_TEXT("."));
                CHECK((++i == p.end()));
            }
        }
        
        WHEN("decrementing a relative path") {
            p = path{PS_TEXT("folder")} / path{PS_TEXT("item.txt")};
            i = p.end();
            THEN("each path component is found") {
                CHECK((*i).empty());
                CHECK((*--i).native() == PS_TEXT("item.txt"));
                CHECK((*--i).native() == PS_TEXT("folder"));
                CHECK((i == p.begin()));
                
                THEN("incrementing the same path each path component is found") {
                    CHECK((*i++).native() == PS_TEXT("folder"));
                    CHECK((*i++).native() == PS_TEXT("item.txt"));
                    CHECK((i == p.end()));
                }
            }
        }
        
        WHEN("iterating a corner case path") {
            p = path{PS_TEXT("/a/b/c/./")}.make_preferred();
            i = p.end();
            THEN("tail path components are found, but incremeenting to the end beahves unexpectedly") {
                CHECK((*i).empty());
                CHECK((*--i).native() == PS_TEXT("."));
                CHECK((*--i).native() == PS_TEXT("."));
                CHECK((*--i).native() == PS_TEXT("c"));
                CHECK((*++i).native() == PS_TEXT("."));
                CHECK((*++i).native() == PS_TEXT("."));
                CHECK((i == p.end())); // XXX: we should have to increment again to get to the end, iterator::see next_element()
            }
        }
    }
    
    if (win32Paths) { SECTION("windows iteration") {
        const path::string_type separator{path::preferred_separator};
        path p {PS_TEXT(R"(\\server\folder\item.txt)")};
        auto i = p.begin();
        
        WHEN("incrementing a UNC path") {
            THEN("each path component is found") {
                CHECK((*i++).native() == PS_TEXT(R"(\\server)")); // root name
                CHECK((*i++).native() == PS_TEXT(R"(\)")); // root dir
                CHECK((*i++).native() == PS_TEXT("folder"));
                CHECK((*i++).native() == PS_TEXT("item.txt"));
                CHECK((i == p.end()));
            }
        }
        
        WHEN("incrementing a raw UNC path") {
            p = PS_TEXT(R"(\\?\C:\folder\item.txt)");
            i = p.begin();
            THEN("each path component is found") {
                CHECK((*i++).native() == PS_TEXT(R"(\\?\C:)")); // root name
                CHECK((*i++).native() == PS_TEXT(R"(\)")); // root dir
                CHECK((*i++).native() == PS_TEXT("folder"));
                CHECK((*i++).native() == PS_TEXT("item.txt"));
                CHECK((i == p.end()));
            }
        }
        
        WHEN("incrementing a UNC path containing duplicate path separators") {
            p = PS_TEXT(R"(\\C:\\folder\\\item.txt\\\\\\\\\)");
            i = p.begin();
            THEN("each path component is found") {
                CHECK((*i++).native() == PS_TEXT(R"(\\C:)")); // root name
                CHECK((*i++).native() == PS_TEXT(R"(\)")); // root dir
                CHECK((*i++).native() == PS_TEXT("folder"));
                CHECK((*i++).native() == PS_TEXT("item.txt"));
                THEN("1+N terminating separators result in a current directory '.' component") {
                    CHECK((*i++).native() == PS_TEXT("."));
                    CHECK((i++ == p.end()));
                }
            }
        }
        
        WHEN("incrementing a relative UNC path") {
            p = PS_TEXT(R"(\\C:item.txt)"); // relative UNC
            THEN("each path component is found") {
                i = p.begin();
                CHECK((*i++).native() == PS_TEXT(R"(\\C:)")); // root name
                // no root dir
                CHECK((*i++).native() == PS_TEXT("item.txt"));
                CHECK((i == p.end()));
            }
        }
        
        WHEN("incrementing a win32 path") {
            p = PS_TEXT(R"(C:\folder\item.txt)");
            i = p.begin();
            THEN("each path component is found") {
                CHECK((*i++).native() == PS_TEXT(R"(C:)")); // root name
                CHECK((*i++).native() == PS_TEXT(R"(\)")); // root dir
                CHECK((*i++).native() == PS_TEXT("folder"));
                CHECK((*i++).native() == PS_TEXT("item.txt"));
                CHECK((i == p.end()));
            }
        }
        
        WHEN("incrementing a relative win32 path") {
            p = PS_TEXT(R"(C:folder\item.txt)"); // relative
            i = p.begin();
            THEN("each path component is found") {
                CHECK((*i++).native() == PS_TEXT(R"(C:)")); // root name
                // no root dir
                CHECK((*i++).native() == PS_TEXT("folder"));
                CHECK((*i++).native() == PS_TEXT("item.txt"));
                CHECK((i == p.end()));
            }
        }
        
        WHEN("incrementing a local-root UNC path") {
            p = PS_TEXT(R"(\\C:\)");
            i = p.begin();
            THEN("each path component is found") {
                CHECK((*i++).native() == PS_TEXT(R"(\\C:)")); // root name
                CHECK((*i++).native() == PS_TEXT(R"(\)")); // root dir
                CHECK((i == p.end()));
            }
        }
        
        WHEN("incrementing a relative-root UNC path") {
            p = PS_TEXT(R"(\\C:)");
            i = p.begin();
            THEN("the path component is found") {
                CHECK((*i++).native() == PS_TEXT(R"(\\C:)")); // root name
                CHECK((i == p.end()));
            }
        }
        
        WHEN("incrementing a root only path") {
            p = PS_TEXT(R"(C:\)");
            i = p.begin();
            THEN("each path component is found") {
                CHECK((*i++).native() == PS_TEXT(R"(C:)")); // root name
                CHECK((*i++).native() == PS_TEXT(R"(\)")); // root dir
                CHECK((i == p.end()));
            }
        }
        
        WHEN("incrementing a relative-root only path") {
            p = PS_TEXT(R"(C:)");
            i = p.begin();
            THEN("the path component is found") {
                CHECK((*i++).native() == PS_TEXT(R"(C:)")); // root name
                CHECK((i == p.end()));
            }
        }
        
        WHEN("prefix incrementing a relative-root only path") {
            p = R"(C:\folder\item.txt)"; // prefix increment test
            i = p.begin();
            THEN("each path component is found") {
                CHECK((*i).native() == PS_TEXT(R"(C:)")); // root name
                CHECK((*++i).native() == PS_TEXT(R"(\)")); // root dir
                CHECK((*++i).native() == PS_TEXT("folder"));
                CHECK((*++i).native() == PS_TEXT("item.txt"));
                CHECK((++i == p.end()));
            }
        }
        
        WHEN("decrementing a UNC path terminating with a separator") {
            p = PS_TEXT(R"(\\?\C:\folder\item.txt\)");
            i = p.end();
            THEN("each path component is found") {
                CHECK((*i).empty());
                CHECK((*--i).native() == PS_TEXT("."));
                CHECK((*--i).native() == PS_TEXT("item.txt"));
                CHECK((*--i).native() == PS_TEXT("folder"));
                CHECK((*--i).native() == PS_TEXT(R"(\)")); // root dir
                CHECK((*--i).native() == PS_TEXT(R"(\\?\C:)")); // root name
                CHECK((i == p.begin()));
                
                THEN("incrementing the same path each path component is found") {
                    CHECK((*i++).native() == PS_TEXT(R"(\\?\C:)")); // root name
                    CHECK((*i++).native() == PS_TEXT(R"(\)")); // root dir
                    CHECK((*i++).native() == PS_TEXT("folder"));
                    CHECK((*i++).native() == PS_TEXT("item.txt"));
                    CHECK((*i++).native() == PS_TEXT("."));
                    CHECK((i == p.end()));
                }
            }
        }
        
        WHEN("both decrementing and incrementing a UNC path terminating with a separator") {
            p = PS_TEXT(R"(\\?\C:\folder\item.txt\)");
            i = p.end();
            THEN("components are found in the expected order") {
                CHECK((*--i).native() == PS_TEXT("."));
                CHECK((*--i).native() == PS_TEXT("item.txt"));
                CHECK((*--i).native() == PS_TEXT("folder"));
                CHECK((*++i).native() == PS_TEXT("item.txt"));
                CHECK((*--i).native() == PS_TEXT("folder"));
                CHECK((*++i).native() == PS_TEXT("item.txt"));
                CHECK((*--i).native() == PS_TEXT("folder"));
                CHECK((*--i).native() == separator);
                CHECK((*++i).native() == PS_TEXT("folder"));
                CHECK((*++i).native() == PS_TEXT("item.txt"));
                CHECK((*++i).native() == PS_TEXT("."));
                CHECK((++i == p.end()));
            }
        }
    }}
    
    SECTION("compare") {
        WHEN("comparing an empty path to another empty path") {
            const path p1;
            const path p2;
            THEN("the paths are equal and  0 is returned") {
                CHECK(0 == p1.compare(p2));
                CHECK(p1 == p2);
                CHECK(p1 >= p2);
                CHECK(p1 <= p2);
                
                CHECK_FALSE(p1 != p2);
                CHECK_FALSE(p1 < p2);
                CHECK_FALSE(p1 > p2);
            }
        }
        
        const auto a = path{PS_TEXT("a")};
        const auto b = path{PS_TEXT("b")};
        const auto c = path{PS_TEXT("c")};
        const auto d = path{PS_TEXT("d")};
        const auto e = path{PS_TEXT("e")};
        const auto p1 = a/b/c/d;
        const path separator{path::preferred_separator};
        
        WHEN("comparing a path to an empty path") {
            const path p2;
            THEN("the path is greater and 1 is returned") {
                CHECK(1 == p1.compare(p2));
                CHECK(p1 > p2);
                CHECK(p1 >= p2);
                
                CHECK(p1 != p2);
                CHECK_FALSE(p1 < p2);
                CHECK_FALSE(p1 <= p2);
            }
        }
        
        WHEN("comparing a path to another path that is lexicographically equal") {
            const auto p2 = p1;
            THEN("the paths are equal and 0 is returned") {
                CHECK(0 == p1.compare(p2));
                CHECK(p1 == p2);
                CHECK(p1 >= p2);
                CHECK(p1 <= p2);
            }
        }
        
        WHEN("comparing a path to another path that is lexicographically less than") {
            const auto p2 = a/b/c/c;
            THEN("the path is greater than and 1 is returned") {
                CHECK(1 == p1.compare(p2));
                CHECK(p1 > p2);
                CHECK(p1 >= p2);
            }
        }
        
        WHEN("comparing a path to another path that is a partial substring of the path") {
            const auto p2 = a/b/c;
            THEN("the path is greater than and 1 is returned") {
                CHECK(1 == p1.compare(p2));
                CHECK(p1 > p2);
                CHECK(p1 >= p2);
            }
        }
        
        WHEN("comparing a path to another path that is lexicographically greater than") {
            const auto p2 = a/b/c/e;
            THEN("the path is less than and -1 is returned") {
                CHECK(-1 == p1.compare(p2));
                CHECK(p1 < p2);
                CHECK(p1 <= p2);
            }
        }
        
        WHEN("comparing a path to another path that contains the path as a partial substring") {
            const auto p2 = a/b/c/d/e;
            THEN("the path is less than and -1 is returned") {
                CHECK(-1 == p1.compare(p2));
                CHECK(p1 < p2);
                CHECK(p1 <= p2);
            }
        }
        
        WHEN("comparing a path to another path with the same components but extra separators") {
            const path p2 = a/b + separator + separator + separator + c + separator + separator + d;
            THEN("the paths are equal and 0 is returned") {
                CHECK(0 == p1.compare(p2));
                CHECK(p1 == p2);
                CHECK(p1 >= p2);
                CHECK(p1 <= p2);
            }
        }
        
        WHEN("comparing a path to another path with the same components but trailing separators") {
            const path p2 = p1 + separator;
            THEN("the paths are not equal") {
                CHECK(0 != p1.compare(p2));
            }
        }
        
        WHEN("comparing a relative path to an absolute path with the same components") {
            const path p2 = separator + p1;
            THEN("the paths are not equal and non-zero is returned") {
                CHECK(0 != p1.compare(p2)); // XXX: not testing specific return value since platform separators may compare <  or > than alpha chars
                CHECK(p1 != p2);
            }
        }
        
        if (win32Paths) {
            WHEN("comparing a UNC path to another UNC path with the same components") {
                const path p2 = path{PS_TEXT(R"(\\?\C:\)")} + p1;
                const path p3 = p2;
                THEN("the paths are equal and 0 is returned") {
                    CHECK(0 == p2.compare(p3));
                    CHECK(p2 == p3);
                    CHECK(p2 >= p3);
                    CHECK(p2 <= p3);
                }
            }
        
            WHEN("comparing a non-UNC path to a UNC path") {
                const path p2 = path{PS_TEXT(R"(\\?\C:\)")} + p1;
                THEN("the paths are not equal") {
                    CHECK(0 != p1.compare(p2));
                    CHECK(p1 != p2);
                }
            }
        }
        
        WHEN("two paths are equal") {
            const path p {PS_TEXT("folder/test")};
            const path p2 {p};
            CHECK(p == p2);
            
            THEN("their hash values are also equal") {
                using namespace filesystem;
                CHECK(hash_value(p) == hash_value(p2));
            }
        }
    }
    
    SECTION("decomposition") {
        WHEN("path is empty") {
            const path p;
            THEN("root name is empty") {
                CHECK(p.root_name().empty());
                CHECK_FALSE(p.has_root_name());
            }
            THEN("root directory is empty") {
                CHECK(p.root_directory().empty());
                CHECK_FALSE(p.has_root_directory());
            }
            THEN("root path is empty") {
                CHECK(p.root_path().empty());
            }
            THEN("relative path is empty") {
                CHECK(p.relative_path().empty());
                CHECK_FALSE(p.has_relative_path());
            }
            THEN("parent path is empty") {
                CHECK(p.parent_path().empty());
                CHECK_FALSE(p.has_parent_path());
            }
            THEN("absolute is false") {
                CHECK_FALSE(p.is_absolute());
            }
            THEN("relative is true") {
                CHECK(p.is_relative());
            }
            THEN("filename is empty") {
                CHECK(p.filename().empty());
                CHECK_FALSE(p.has_filename());
            }
            THEN("stem is empty") {
                CHECK(p.stem().empty());
                CHECK_FALSE(p.has_stem());
            }
            THEN("extension is empty") {
                CHECK(p.extension().empty());
                CHECK_FALSE(p.has_extension());
            }
        }
        
        WHEN("path is posix absolute") {
            const path p = path{PS_TEXT("/a/b/c/d/")}.make_preferred();
            THEN("root name is empty") {
                CHECK(p.root_name().empty());
                CHECK_FALSE(p.has_root_name());
            }
            THEN("root directory is not empty") {
                CHECK_FALSE(p.root_directory().empty());
                CHECK(p.has_root_directory());
            }
            THEN("root path equals root directory") {
                CHECK_FALSE(p.root_path().empty());
                CHECK(p.root_path() == p.root_directory());
            }
            THEN("relative path is the expected value") {
                CHECK(p.relative_path() == path{PS_TEXT("a/b/c/d/")}.make_preferred());
                CHECK(p.has_relative_path());
            }
            THEN("parent path is the expected value") {
                CHECK(p.parent_path() == path{PS_TEXT("/a/b/c")}.make_preferred());
                CHECK(p.has_parent_path());
            }
            THEN("absolute is the expected platform value") {
                if (!win32Paths) {
                    CHECK(p.is_absolute());
                } else {
                    CHECK_FALSE(p.is_absolute());
                }
            }
            THEN("relative is the expected platform value") {
                if (!win32Paths) {
                    CHECK_FALSE(p.is_relative());
                } else {
                    CHECK(p.is_relative());
                }
            }
            THEN("filename is the expected value") {
                CHECK(p.filename().native() == PS_TEXT("d"));
                CHECK(p.has_filename());
            }
            THEN("stem is the expected value") {
                CHECK(p.stem().native() == PS_TEXT("d"));
                CHECK(p.has_stem());
            }
            THEN("extension is the expected value") {
                CHECK(p.extension().empty());
                CHECK_FALSE(p.has_extension());
            }
        }
        
        WHEN("path is relative") {
            const path p = path{PS_TEXT("a/b/c/d.txt")}.make_preferred();
            THEN("root name is empty") {
                CHECK(p.root_name().empty());
                CHECK_FALSE(p.has_root_name());
            }
            THEN("root directory is empty") {
                CHECK(p.root_directory().empty());
                CHECK_FALSE(p.has_root_directory());
            }
            THEN("root path is empty") {
                CHECK(p.root_path().empty());
            }
            THEN("relative path is the expected value") {
                CHECK(p.relative_path() == p);
                CHECK(p.has_relative_path());
            }
            THEN("parent path is the expected value") {
                CHECK(p.parent_path() == path{PS_TEXT("a/b/c")}.make_preferred());
                CHECK(p.has_parent_path());
            }
            THEN("absolute is false") {
                CHECK_FALSE(p.is_absolute());
            }
            THEN("relative is true") {
                CHECK(p.is_relative());
            }
            THEN("filename is the expected value") {
                CHECK(p.filename().native() == PS_TEXT("d.txt"));
                CHECK(p.has_filename());
            }
            THEN("stem is the expected value") {
                CHECK(p.stem().native() == PS_TEXT("d"));
                CHECK(p.has_stem());
            }
            THEN("extension is the expected value") {
                CHECK(p.extension().native() == PS_TEXT(".txt"));
                CHECK(p.has_extension());
                
                CHECK((p.stem() + p.extension()) == p.filename());
            }
        }
        
        WHEN("path is posix root only") {
            const path p = path{"/"}.make_preferred();
            THEN("root name is empty") {
                CHECK(p.root_name().empty());
                CHECK_FALSE(p.has_root_name());
            }
            THEN("root directory is not empty") {
                CHECK_FALSE(p.root_directory().empty());
                CHECK(p.has_root_directory());
            }
            THEN("root path equals root directory") {
                CHECK_FALSE(p.root_path().empty());
                CHECK(p.root_path() == p.root_directory());
            }
            THEN("relative path is empty") {
                CHECK(p.relative_path().empty());
                CHECK_FALSE(p.has_relative_path());
            }
            THEN("parent path is empty") {
                CHECK(p.parent_path().empty());
                CHECK_FALSE(p.has_parent_path());
            }
            THEN("absolute is the expected platform value") {
                if (!win32Paths) {
                    CHECK(p.is_absolute());
                } else {
                    CHECK_FALSE(p.is_absolute());
                }
            }
            THEN("relative is the expected platform value") {
                if (!win32Paths) {
                    CHECK_FALSE(p.is_relative());
                } else {
                    CHECK(p.is_relative());
                }
            }
            THEN("filename is the expected value") {
                CHECK(p.filename().native() == path::string_type{path::preferred_separator});
                CHECK(p.has_filename());
            }
            THEN("stem is the expected value") {
                CHECK(p.stem().native() == path::string_type{path::preferred_separator});
                CHECK(p.has_stem());
            }
            THEN("extension empty") {
                CHECK(p.extension().empty());
                CHECK_FALSE(p.has_extension());
            }
        }
        
        WHEN("path is dot") {
            const path p = path{"."};
            THEN("root name is empty") {
                CHECK(p.root_name().empty());
                CHECK_FALSE(p.has_root_name());
            }
            THEN("root directory is empty") {
                CHECK(p.root_directory().empty());
                CHECK_FALSE(p.has_root_directory());
            }
            THEN("root path is empty") {
                CHECK(p.root_path().empty());
            }
            THEN("relative path is the expected value") {
                CHECK(p.relative_path().native() == PS_TEXT("."));
                CHECK(p.has_relative_path());
            }
            THEN("parent path is empty") {
                CHECK(p.parent_path().empty());
                CHECK_FALSE(p.has_parent_path());
            }
            THEN("absolute is false") {
                CHECK_FALSE(p.is_absolute());
            }
            THEN("relative is true") {
                CHECK(p.is_relative());
            }
            THEN("filename is the expected value") {
                CHECK(p.filename().native() == PS_TEXT("."));
                CHECK(p.has_filename());
            }
            THEN("stem is the expected value") {
                CHECK(p.stem().native() == PS_TEXT("."));
                CHECK(p.has_stem());
            }
            THEN("extension empty") {
                CHECK(p.extension().empty());
                CHECK_FALSE(p.has_extension());
            }
        }
        
        WHEN("path is dot-dot") {
            const path p = path{".."};
            THEN("root name is empty") {
                CHECK(p.root_name().empty());
                CHECK_FALSE(p.has_root_name());
            }
            THEN("root directory is empty") {
                CHECK(p.root_directory().empty());
                CHECK_FALSE(p.has_root_directory());
            }
            THEN("root path is empty") {
                CHECK(p.root_path().empty());
            }
            THEN("relative path is the expected value") {
                CHECK(p.relative_path().native() == PS_TEXT(".."));
                CHECK(p.has_relative_path());
            }
            THEN("parent path is empty") {
                CHECK(p.parent_path().empty());
                CHECK_FALSE(p.has_parent_path());
            }
            THEN("absolute is false") {
                CHECK_FALSE(p.is_absolute());
            }
            THEN("relative is true") {
                CHECK(p.is_relative());
            }
            THEN("filename is the expected value") {
                CHECK(p.filename().native() == PS_TEXT(".."));
                CHECK(p.has_filename());
            }
            THEN("stem is the expected value") {
                CHECK(p.stem().native() == PS_TEXT(".."));
                CHECK(p.has_stem());
            }
            THEN("extension is empty") {
                CHECK(p.extension().empty());
                CHECK_FALSE(p.has_extension());
            }
        }
        
        WHEN("filename terminates in a .") {
            const path p = path{"item."};
            THEN("root name is empty") {
                CHECK(p.root_name().empty());
                CHECK_FALSE(p.has_root_name());
            }
            THEN("root directory is empty") {
                CHECK(p.root_directory().empty());
                CHECK_FALSE(p.has_root_directory());
            }
            THEN("root path is empty") {
                CHECK(p.root_path().empty());
            }
            THEN("relative path is the expected value") {
                CHECK(p.relative_path().native() == PS_TEXT("item."));
                CHECK(p.has_relative_path());
            }
            THEN("parent path is empty") {
                CHECK(p.parent_path().empty());
                CHECK_FALSE(p.has_parent_path());
            }
            THEN("absolute is false") {
                CHECK_FALSE(p.is_absolute());
            }
            THEN("relative is true") {
                CHECK(p.is_relative());
            }
            THEN("filename is the expected value") {
                CHECK(p.filename().native() == PS_TEXT("item."));
                CHECK(p.has_filename());
            }
            THEN("stem is the expected value") {
                CHECK(p.stem().native() == PS_TEXT("item"));
                CHECK(p.has_stem());
            }
            THEN("extension is the expected value") {
                CHECK(p.extension().native() == PS_TEXT("."));
                CHECK(p.has_extension());
                
                CHECK((p.stem() + p.extension()) == p.filename());
            }
        }
        
        if (win32Paths) {
            WHEN("path is a UNC path") {
                const path server {PS_TEXT(R"(\\server\folder\item.txt)")};
                const path raw {PS_TEXT(R"(\\?\C:\folder\item.txt)")};
                const path absolute {PS_TEXT(R"(\\C:\\folder\\\item.txt\\\\\\\\\)")};
                const path relative {PS_TEXT(R"(\\C:item.txt)")};
                const path rootOnly {PS_TEXT(R"(\\C:\)")};
                const path relativeRootOnly {PS_TEXT(R"(\\C:)")};
                THEN("root name is the expected component") {
                    CHECK(server.root_name().native() == PS_TEXT(R"(\\server)"));
                    CHECK(raw.root_name().native() == PS_TEXT(R"(\\?\C:)"));
                    CHECK(absolute.root_name().native() == PS_TEXT(R"(\\C:)"));
                    CHECK(relative.root_name().native() == PS_TEXT(R"(\\C:)"));
                    CHECK(rootOnly.root_name().native() == PS_TEXT(R"(\\C:)"));
                    CHECK(relativeRootOnly.root_name().native() == PS_TEXT(R"(\\C:)"));
                    
                    CHECK(server.has_root_name());
                    CHECK(raw.has_root_name());
                    CHECK(absolute.has_root_name());
                    CHECK(relative.has_root_name());
                    CHECK(rootOnly.has_root_name());
                    CHECK(relativeRootOnly.has_root_name());
                }
                THEN("root directory is present or not as expected") {
                    CHECK_FALSE(server.root_directory().empty());
                    CHECK_FALSE(raw.root_directory().empty());
                    CHECK_FALSE(absolute.root_directory().empty());
                    CHECK(relative.root_directory().empty());
                    CHECK_FALSE(rootOnly.root_directory().empty());
                    CHECK(relativeRootOnly.root_directory().empty());
                    
                    CHECK(server.has_root_directory());
                    CHECK(raw.has_root_directory());
                    CHECK(absolute.has_root_directory());
                    CHECK_FALSE(relative.has_root_directory());
                    CHECK(rootOnly.has_root_directory());
                    CHECK_FALSE(relativeRootOnly.has_root_directory());
                }
                THEN("root path is the expected value") {
                    CHECK(server.root_path().native() == PS_TEXT(R"(\\server\)"));
                    CHECK(raw.root_path().native() == PS_TEXT(R"(\\?\C:\)"));
                    CHECK(absolute.root_path().native() == PS_TEXT(R"(\\C:\)"));
                    CHECK(relative.root_path().native() == PS_TEXT(R"(\\C:)"));
                    CHECK(rootOnly.root_path().native() == PS_TEXT(R"(\\C:\)"));
                    CHECK(relativeRootOnly.root_path().native() == PS_TEXT(R"(\\C:)"));
                }
                THEN("relative path is the expected value") {
                    CHECK(server.relative_path().native() == PS_TEXT(R"(folder\item.txt)"));
                    CHECK(raw.relative_path().native() == PS_TEXT(R"(folder\item.txt)"));
                    CHECK(absolute.relative_path().native() == PS_TEXT(R"(folder\\\item.txt\\\\\\\\\)"));
                    CHECK(relative.relative_path().native() == PS_TEXT("item.txt"));
                    CHECK(rootOnly.relative_path().native() == PS_TEXT(""));
                    CHECK(relativeRootOnly.relative_path().native() == PS_TEXT(""));
                    
                    CHECK(server.has_relative_path());
                    CHECK(raw.has_relative_path());
                    CHECK(absolute.has_relative_path());
                    CHECK(relative.has_relative_path());
                    CHECK_FALSE(rootOnly.has_relative_path());
                    CHECK_FALSE(relativeRootOnly.has_relative_path());
                }
                THEN("parent path is the expected value") {
                    CHECK(server.parent_path().native() == PS_TEXT(R"(\\server\folder)"));
                    CHECK(raw.parent_path().native() == PS_TEXT(R"(\\?\C:\folder)"));
                    CHECK(absolute.parent_path().native() == PS_TEXT(R"(\\C:\folder)"));
                    CHECK(relative.parent_path().native() == PS_TEXT(R"(\\C:)"));
                    CHECK(rootOnly.parent_path().native() == PS_TEXT(R"(\\C:)"));
                    CHECK(relativeRootOnly.parent_path().native() == PS_TEXT(""));
                    
                    CHECK(server.has_parent_path());
                    CHECK(raw.has_parent_path());
                    CHECK(absolute.has_parent_path());
                    CHECK(relative.has_parent_path());
                    CHECK(rootOnly.has_parent_path());
                    CHECK_FALSE(relativeRootOnly.has_parent_path());
                }
                THEN("absolute is the expected value") {
                    CHECK(server.is_absolute());
                    CHECK(raw.is_absolute());
                    CHECK(absolute.is_absolute());
                    CHECK_FALSE(relative.is_absolute());
                    CHECK(rootOnly.is_absolute());
                    CHECK_FALSE(relativeRootOnly.is_absolute());
                }
                THEN("relative is the expected value") {
                    CHECK_FALSE(server.is_relative());
                    CHECK_FALSE(raw.is_relative());
                    CHECK_FALSE(absolute.is_relative());
                    CHECK(relative.is_relative());
                    CHECK_FALSE(rootOnly.is_relative());
                    CHECK(relativeRootOnly.is_relative());
                }
                THEN("filename is the expected value") {
                    CHECK(server.filename().native() == PS_TEXT("item.txt"));
                    CHECK(raw.filename().native() == PS_TEXT("item.txt"));
                    CHECK(absolute.filename().native() == PS_TEXT("item.txt"));
                    CHECK(relative.filename().native() == PS_TEXT("item.txt"));
                    CHECK(rootOnly.filename().native() == PS_TEXT(R"(\)"));
                    CHECK(relativeRootOnly.filename().native() == PS_TEXT(R"(\\C:)"));
                    
                    CHECK(server.has_filename());
                    CHECK(raw.has_filename());
                    CHECK(absolute.has_filename());
                    CHECK(relative.has_filename());
                    CHECK(rootOnly.has_filename());
                    CHECK(relativeRootOnly.has_filename());
                }
                THEN("stem is the expected value") {
                    CHECK(server.stem().native() == PS_TEXT("item"));
                    CHECK(raw.stem().native() == PS_TEXT("item"));
                    CHECK(absolute.stem().native() == PS_TEXT("item"));
                    CHECK(relative.stem().native() == PS_TEXT("item"));
                    CHECK(rootOnly.stem().native() == PS_TEXT(R"(\)"));
                    CHECK(relativeRootOnly.stem().native() == PS_TEXT(R"(\\C:)"));
                    
                    CHECK(server.has_stem());
                    CHECK(raw.has_stem());
                    CHECK(absolute.has_stem());
                    CHECK(relative.has_stem());
                    CHECK(rootOnly.has_stem());
                    CHECK(relativeRootOnly.has_stem());
                }
                THEN("extension is the expected value") {
                    CHECK(server.extension().native() == PS_TEXT(".txt"));
                    CHECK(raw.extension().native() == PS_TEXT(".txt"));
                    CHECK(absolute.extension().native() == PS_TEXT(".txt"));
                    CHECK(relative.extension().native() == PS_TEXT(".txt"));
                    CHECK(rootOnly.extension().native() == PS_TEXT(""));
                    CHECK(relativeRootOnly.extension().native() == PS_TEXT(""));
                    
                    CHECK(server.has_extension());
                    CHECK(raw.has_extension());
                    CHECK(absolute.has_extension());
                    CHECK(relative.has_extension());
                    CHECK_FALSE(rootOnly.has_extension());
                    CHECK_FALSE(relativeRootOnly.has_extension());
                }
            }
            
            WHEN("path begins with a drive letter") {
                const path absolute {PS_TEXT(R"(C:\folder\item.txt)")};
                const path relative {PS_TEXT(R"(D:folder\item.txt)")};
                const path rootOnly {PS_TEXT(R"(E:\)")};
                const path relativeRootOnly {PS_TEXT("C:")};
                THEN("root name is the expected component") {
                    CHECK(absolute.root_name().native() == PS_TEXT("C:"));
                    CHECK(relative.root_name().native() == PS_TEXT("D:"));
                    CHECK(rootOnly.root_name().native() == PS_TEXT("E:"));
                    CHECK(relativeRootOnly.root_name().native() == PS_TEXT("C:"));
                    
                    CHECK(absolute.has_root_name());
                    CHECK(relative.has_root_name());
                    CHECK(rootOnly.has_root_name());
                    CHECK(relativeRootOnly.has_root_name());
                }
                THEN("root directory is present or not as expected") {
                    CHECK_FALSE(absolute.root_directory().empty());
                    CHECK(relative.root_directory().empty());
                    CHECK_FALSE(rootOnly.root_directory().empty());
                    CHECK(relativeRootOnly.root_directory().empty());
                    
                    CHECK(absolute.has_root_directory());
                    CHECK_FALSE(relative.has_root_directory());
                    CHECK(rootOnly.has_root_directory());
                    CHECK_FALSE(relativeRootOnly.has_root_directory());
                }
                THEN("root path is the expected value") {
                    CHECK(absolute.root_path().native() == PS_TEXT(R"(C:\)"));
                    CHECK(relative.root_path().native() == PS_TEXT("D:"));
                    CHECK(rootOnly.root_path().native() == PS_TEXT(R"(E:\)"));
                    CHECK(relativeRootOnly.root_path().native() == PS_TEXT("C:"));
                }
                THEN("relative path is the expected value") {
                    CHECK(absolute.relative_path().native() == PS_TEXT(R"(folder\item.txt)"));
                    CHECK(relative.relative_path().native() == PS_TEXT(R"(folder\item.txt)"));
                    CHECK(rootOnly.relative_path().native() == PS_TEXT(""));
                    CHECK(relativeRootOnly.relative_path().native() == PS_TEXT(""));
                    
                    CHECK(absolute.has_relative_path());
                    CHECK(relative.has_relative_path());
                    CHECK_FALSE(rootOnly.has_relative_path());
                    CHECK_FALSE(relativeRootOnly.has_relative_path());
                }
                THEN("parent path is the expected value") {
                    CHECK(absolute.parent_path().native() == PS_TEXT(R"(C:\folder)"));
                    CHECK(relative.parent_path().native() == PS_TEXT(R"(D:folder)"));
                    CHECK(rootOnly.parent_path().native() == PS_TEXT(R"(E:)"));
                    CHECK(relativeRootOnly.parent_path().native() == PS_TEXT(""));
                    
                    CHECK(absolute.has_parent_path());
                    CHECK(relative.has_parent_path());
                    CHECK(rootOnly.has_parent_path());
                    CHECK_FALSE(relativeRootOnly.has_parent_path());
                }
                THEN("absolute is the expected value") {
                    CHECK(absolute.is_absolute());
                    CHECK_FALSE(relative.is_absolute());
                    CHECK(rootOnly.is_absolute());
                    CHECK_FALSE(relativeRootOnly.is_absolute());
                }
                THEN("relative is the expected value") {
                    CHECK_FALSE(absolute.is_relative());
                    CHECK(relative.is_relative());
                    CHECK_FALSE(rootOnly.is_relative());
                    CHECK(relativeRootOnly.is_relative());
                }
                THEN("filename is the expected value") {
                    CHECK(absolute.filename().native() == PS_TEXT("item.txt"));
                    CHECK(relative.filename().native() == PS_TEXT("item.txt"));
                    CHECK(rootOnly.filename().native() == PS_TEXT(R"(\)"));
                    CHECK(relativeRootOnly.filename().native() == PS_TEXT("C:"));
                    
                    CHECK(absolute.has_filename());
                    CHECK(relative.has_filename());
                    CHECK(rootOnly.has_filename());
                    CHECK(relativeRootOnly.has_filename());
                }
                THEN("stem is the expected value") {
                    CHECK(absolute.stem().native() == PS_TEXT("item"));
                    CHECK(relative.stem().native() == PS_TEXT("item"));
                    CHECK(rootOnly.stem().native() == PS_TEXT(R"(\)"));
                    CHECK(relativeRootOnly.stem().native() == PS_TEXT("C:"));
                    
                    CHECK(absolute.has_stem());
                    CHECK(relative.has_stem());
                    CHECK(rootOnly.has_stem());
                    CHECK(relativeRootOnly.has_stem());
                }
                THEN("extension is the expected value") {
                    CHECK(absolute.extension().native() == PS_TEXT(".txt"));
                    CHECK(relative.extension().native() == PS_TEXT(".txt"));
                    CHECK(rootOnly.extension().native() == PS_TEXT(""));
                    CHECK(relativeRootOnly.extension().native() == PS_TEXT(""));
                    
                    CHECK(absolute.has_extension());
                    CHECK(relative.has_extension());
                    CHECK_FALSE(rootOnly.has_extension());
                    CHECK_FALSE(relativeRootOnly.has_extension());
                }
            }
        }
    }
    
    SECTION("conversion to string") {
        const auto p = path{PS_TEXT("/a/b/c")}.make_preferred();
        
        auto s = p.string();
        CHECK(s == filesystem::sanitize(std::string{"/a/b/c"}, path::preferred_separator_style));
        auto u8s = p.u8string();
        CHECK(u8s.str() == filesystem::sanitize(std::string{"/a/b/c"}, path::preferred_separator_style));
        auto u16s = p.u16string();
        CHECK_FALSE(u16s.empty());
        auto u32s = p.u32string();
        CHECK_FALSE(u32s.empty());
    }
    
    SECTION("stream insert") {
        const auto p = path{PS_TEXT("/a/b/c")}.make_preferred();
        std::stringstream ss;
        ss << p;
        
        std::string expected;
        ss >> expected; // necessary to strip quotes added for C++14
        CHECK(ss.str() == expected);
    }
    
    SECTION("stream extract") {
        const auto string = filesystem::sanitize(std::string{"/a/b/c"}, path::preferred_separator_style);
        std::stringstream ss;
        ss << string;
        path p;
        ss >> p;
        CHECK(p == path{PS_TEXT("/a/b/c")}.make_preferred());
    }
    
    SECTION("explicit UTF8 conversion") {
        const auto precomposed_actue_aaa = "/\xC3\xA1/\xC3\xA1/\xC3\xA1";
        
        WHEN("path is created from a UTF8 string") {
            const auto p = filesystem::u8path(precomposed_actue_aaa);
            THEN("path contains the expected content") {
                CHECK(to_u8string<typename path::string_type>{}(p.native()) == as_utf8(precomposed_actue_aaa));
            }
        }
        WHEN("path is created from a UTF8 range") {
            const auto p = filesystem::u8path(precomposed_actue_aaa, precomposed_actue_aaa+std::strlen(precomposed_actue_aaa));
            THEN("path contains the expected content") {
                CHECK(to_u8string<typename path::string_type>{}(p.native()) == as_utf8(precomposed_actue_aaa));
            }
        }
    }
}
