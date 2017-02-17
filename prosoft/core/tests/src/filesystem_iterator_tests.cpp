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

#include <filesystem/filesystem.hpp>

#include "catch.hpp"

using namespace prosoft;
using namespace prosoft::filesystem;

#include "fstestutils.hpp"

TEST_CASE("filesystem_iterator") {
    WHEN("reserved options are set") {
        CHECK(make_public(directory_options::reserved_state_mask) == directory_options::none);
    }

    static const directory_entry empty;

    SECTION("iterator common") {
        using iterator_type = directory_iterator;
        #include "filesystem_iterator_common_tests_i.cpp"
    }
    
    SECTION("recursive iterator common") {
        using iterator_type = recursive_directory_iterator;
        #include "filesystem_iterator_common_tests_i.cpp"
    }
    
    SECTION("iterator") {
        SECTION("recurse option is always disabled") {
            const auto p = create_file(temp_directory_path() / PS_TEXT("fs17test"));
            REQUIRE(exists(p));
            
            directory_iterator i{temp_directory_path()};
            CHECK(is_set(i.options() & directory_options::skip_subdirectory_descendants));
            i = directory_iterator{temp_directory_path(), directory_options::follow_directory_symlink};
            CHECK(is_set(i.options() & directory_options::skip_subdirectory_descendants));
            
            REQUIRE(remove(p));
        }
    }
    
    SECTION("recursive iterator") {
        WHEN("creating a default recursive iterator") {
            recursive_directory_iterator i;
            CHECK(i == end(i));
            CHECK(i->path().empty());
            CHECK(i.depth() == iterator_depth_type());
            CHECK_FALSE(i.recursion_pending());
            i.disable_recursion_pending();
            i.pop();
            CHECK_FALSE(i.recursion_pending());
            CHECK(i == end(i));
        }
        
        SECTION("recurse option is always enabled") {
            recursive_directory_iterator i{home_directory_path()};
            CHECK(!is_set(i.options() & directory_options::skip_subdirectory_descendants));
            i = recursive_directory_iterator{home_directory_path(), directory_options::follow_directory_symlink};
            CHECK(!is_set(i.options() & directory_options::skip_subdirectory_descendants));
            i = recursive_directory_iterator{home_directory_path(), directory_options::follow_directory_symlink|directory_options::skip_subdirectory_descendants};
            CHECK(!is_set(i.options() & directory_options::skip_subdirectory_descendants));
        }
        
        SECTION("operations") {
            const auto root = temp_directory_path() / PS_TEXT("fs17iter");
            create_directory(root);
            REQUIRE(exists(root));
            PS_RAII_REMOVE(root);
            
            const auto dir = root / PS_TEXT("1");
            create_directory(dir);
            REQUIRE(exists(dir));
            PS_RAII_REMOVE(dir);
            
            const auto f = create_file(dir / PS_TEXT("._2")); // orphan Apple double pattern
            REQUIRE(exists(f));
            PS_RAII_REMOVE(f);
            
            WHEN("recursion is enabled") {
                recursive_directory_iterator i{root};
                CHECK(i != end(i));
                CHECK(i.depth() == 0);
                
                auto e = *i;
                CHECK(i.recursion_pending());
                CHECK(e.path().filename().native() == PS_TEXT("1"));
                e = *i++;
                CHECK(e.path().filename().native() == PS_TEXT("._2"));
                CHECK(i.depth() == 1);
                CHECK_FALSE(i.recursion_pending());
                CHECK(i != end(i));
                i++;
                CHECK(i == end(i));
            }
            
            WHEN("recursion is disabled") {
                recursive_directory_iterator i{root};
                CHECK(i.depth() == 0);
                
                auto& e = *i;
                CHECK(i.recursion_pending());
                CHECK(e.path().filename().native() == PS_TEXT("1"));
                i.disable_recursion_pending();
                CHECK_FALSE(i.recursion_pending());
                CHECK(i != end(i));
                i++;
                CHECK(i == end(i));
            }
            
            WHEN("iterator is popped") {
                recursive_directory_iterator i{root};
                CHECK(i != end(i));
                CHECK(i.depth() == 0);
                
                auto e = *i;
                CHECK(e.path().filename().native() == PS_TEXT("1"));
                CHECK(i.depth() == 0);
                e = *i++;
                CHECK(e.path().filename().native() == PS_TEXT("._2"));
                CHECK(i.depth() == 1);
                i.pop();
                CHECK(e.path().filename().native() == PS_TEXT("._2")); // pop does not change the current entry
                CHECK(i.depth() == 0);
                CHECK(i != end(i));
                i.pop();
                CHECK(i == end(i));
            }
            
            WHEN("skip permission denied is disabled") {
#if __APPLE__
                REQUIRE(geteuid() != 0);
                CHECK_THROWS(recursive_directory_iterator("/var/root"));
#endif // __APPLE__
            }
            
            WHEN("skip permission denied is enabled") {
#if __APPLE__
                recursive_directory_iterator i{"/var/root", recursive_directory_iterator::default_options()|directory_options::skip_permission_denied};
                CHECK(i == end(i));
                
                i = recursive_directory_iterator{"/var", recursive_directory_iterator::default_options()|directory_options::skip_permission_denied};
                bool rootFound{};
                for (auto& e : i) {
                    if (e.path().filename().native() == "root") {
                        CHECK(is_directory(e.status()));
                        CHECK_FALSE(i.recursion_pending());
                        const auto d = i.depth();
                        ++i;
                        CHECK(i.depth() == d);
                        rootFound = true;
                        break;
                    } else if (i.recursion_pending()) {
                        i.disable_recursion_pending();
                    }
                }
                CHECK(rootFound);
#endif // __APPLE__
            }
            
#if !_WIN32 // we'd have to create a junction point for Win32
            WHEN("a directory symlink is present") {
                const auto lnk = root / PS_TEXT("lnk");
                (void)symlink(dir.c_str(), lnk.c_str());
                REQUIRE(exists(lnk));
                PS_RAII_REMOVE(lnk);
                
                AND_WHEN("follow dir symlink is disabled") {
                    recursive_directory_iterator i{root};
                    bool linkFound{};
                    for (auto& e : i) {
                        if (e.path().filename().native() == PS_TEXT("lnk")) {
                            CHECK_FALSE(i.recursion_pending());
                            CHECK(e.path() == lnk);
                            linkFound = true;
                            break;
                        }
                    }
                    CHECK(linkFound);
                }
                
                AND_WHEN("follow dir symlink is enabled") {
                    recursive_directory_iterator i{root, recursive_directory_iterator::default_options()|directory_options::follow_directory_symlink};
                    bool linkFound{};
                    for (auto e : i) {
                        if (e.path().filename().native() == PS_TEXT("lnk")) {
                            CHECK(i.recursion_pending());
                            CHECK(e.path() == lnk);
                            linkFound = true;
                            // Increment into the dir and check the resulting path.
                            // The parent path should be the canonical dir path and not the symlink path.
                            e = *i++;
                            CHECK(i.depth() == 1);
                            CHECK(i != end(i));
                            CHECK(e.path().parent_path() == canonical(dir));
                            break;
                        }
                    }
                    CHECK(linkFound);
                }
            }
#endif // !_WIN32

#if __APPLE__ || __linux__ // both have known mountpoints in /
            WHEN("a mountpoint is present") {
#if __APPLE__
                constexpr auto mount = "dev";
#else
                constexpr auto mount = "proc";
#endif
                // Skip perm denied is required as we may hit restricted entries in the root or the mount.
                constexpr auto opts = recursive_directory_iterator::default_options()|directory_options::skip_permission_denied;
                AND_WHEN("follow mountpoints is disabled") {
                    recursive_directory_iterator i{path{"/"}, opts};
                    bool mountFound{};
                    for (auto& e : i) {
                        if (e.path().filename().native() == mount) {
                            CHECK_FALSE(i.recursion_pending());
                            mountFound = true;
                            break;
                        } else if (i.recursion_pending()) {
                            i.disable_recursion_pending();
                        }
                    }
                    CHECK(mountFound);
                }
                
                AND_WHEN("follow mountpoints is enabled") {
                    recursive_directory_iterator i{path{"/"}, opts|directory_options::follow_mountpoints};
                    bool mountFound{};
                    for (auto& e : i) {
                        if (e.path().filename().native() == mount) {
                            CHECK(i.recursion_pending());
                            CHECK(i.depth() == 0);
                            ++i;
                            CHECK(i.depth() == 1);
                            mountFound = true;
                            break;
                        } else if (i.recursion_pending()) {
                            i.disable_recursion_pending();
                        }
                    }
                    CHECK(mountFound);
                }
            }
#endif
            
            WHEN("skip hidden is enabled") {
                recursive_directory_iterator i{root, recursive_directory_iterator::default_options()|directory_options::skip_hidden_descendants};
                CHECK(i.depth() == 0);
                
                auto& e = *i;
                CHECK(e.path().filename().native() == PS_TEXT("1"));
                CHECK(i != end(i));
            #if _WIN32
                // . files are not considered hidden. We'd need to set the hidden attribute on the item.
                i++
            #endif
                i++;
                CHECK(i == end(i));
            }
            
            WHEN("post order directories is enabled") {
                recursive_directory_iterator i{root, recursive_directory_iterator::default_options()|directory_options::include_postorder_directories};
                
                auto e = *i;
                CHECK(i.recursion_pending());
                CHECK(e.path().filename().native() == PS_TEXT("1"));
                e = *i++;
                CHECK(e.path().filename().native() == PS_TEXT("._2"));
                CHECK(i.depth() == 1);
                CHECK_FALSE(i.recursion_pending());
                CHECK(i != end(i));
                e = *i++;
                CHECK(i.is_postorder());
                CHECK(e.path().filename().native() == PS_TEXT("1"));
                CHECK(i.depth() == 0);
                CHECK_FALSE(i.recursion_pending());
                CHECK(i != end(i));
                i++;
                CHECK(i == end(i));
            }
            
#if __APPLE__
            WHEN("a package is present") {
                AND_WHEN("skip package contents is disabled") {
                    recursive_directory_iterator i{"/System/Library/CoreServices"};
                    bool foundPackage{};
                    for (auto& e : i) {
                        if (is_package(e.path())) {
                            CHECK(i.recursion_pending());
                            const auto d = i.depth();
                            ++i;
                            CHECK(i.depth() == d+1);
                            foundPackage = true;
                            break;
                        } else if (i.recursion_pending()) {
                            i.disable_recursion_pending();
                        }
                    }
                    CHECK(foundPackage);
                }
                
                AND_WHEN("skip package contents is enabled") {
                    constexpr auto opts = recursive_directory_iterator::default_options()|directory_options::skip_package_content_descendants;
                    recursive_directory_iterator i{"/System/Library/CoreServices", opts};
                    bool foundPackage{};
                    for (auto& e : i) {
                        if (is_package(e.path())) {
                            CHECK_FALSE(i.recursion_pending());
                            const auto d = i.depth();
                            ++i;
                            CHECK(i.depth() == d);
                            foundPackage = true;
                            break;
                        } else if (i.recursion_pending()) {
                            i.disable_recursion_pending();
                        }
                    }
                    CHECK(foundPackage);
                }
            }

            WHEN("an apple double pair is present") {
                // create a "root" for the apple double pair
                const auto adr = create_file(dir / PS_TEXT("2"));
                REQUIRE(exists(adr));
                PS_RAII_REMOVE(adr);
                
                AND_WHEN("include apple double is disabled") {
                    recursive_directory_iterator i{root};
                    auto e = *i;
                    CHECK(e.path().filename().native() == "1");
                    CHECK(i != end(i));
                    e = *i++;
                    CHECK(e.path().filename().native() == "2");
                    CHECK(i != end(i));
                    e = *i++;
                    CHECK(i == end(i));
                }
                
                AND_WHEN("include apple double is enabled") {
                    recursive_directory_iterator i{root, recursive_directory_iterator::default_options()|directory_options::include_apple_double_files};
                    auto e = *i;
                    CHECK(e.path().filename().native() == "1");
                    CHECK(i != end(i));
                    e = *i++;
                    CHECK(i.depth() == 1);
                    auto leaf = e.path().filename().native();
                    CHECK((leaf == "2" || leaf == "._2"));
                    CHECK(i != end(i));
                    CHECK(i.depth() == 1);
                    e = *i++;
                    const auto expected = leaf == "2" ? "._2" : "2";
                    CHECK(e.path().filename().native() == expected);
                    CHECK(i != end(i));
                    e = *i++;
                    CHECK(i == end(i));
                }
                
                AND_WHEN("include apple double and skip hidden are enabled") {
                    constexpr auto opts = recursive_directory_iterator::default_options()|directory_options::include_apple_double_files|directory_options::skip_hidden_descendants;
                    recursive_directory_iterator i{root, opts};
                    auto e = *i;
                    CHECK(e.path().filename().native() == "1");
                    CHECK(i != end(i));
                    e = *i++;
                    CHECK(e.path().filename().native() == "2");
                    CHECK(i != end(i));
                    e = *i++;
                    CHECK(i == end(i));
                }
            }
#endif // __APPLE__
        }
    }
}
