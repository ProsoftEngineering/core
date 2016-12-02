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

#include <fstream>
#include <iosfwd>
#include <limits>
#include <type_traits>

#include <filesystem/filesystem.hpp>
#include <filesystem/src/filesystem_private.hpp>

#include "catch.hpp"

using namespace prosoft;
using namespace prosoft::filesystem;

#include "fstestutils.hpp"

TEST_CASE("filesystem") {

    SECTION("filesystem_error") {
        const auto p = path{"/a/b/c/"}.make_preferred();
        const std::string description{"test"};
        
        WHEN("creating an error with standard args") {
            filesystem_error e{description, error_code{1, filesystem_category()}};
            CHECK(e.what() != nullptr); // what() is implementation defined by std::system_error(), but it will include the description
            CHECK(e.code().value() == 1);
            CHECK(e.path1().empty());
            CHECK(e.path2().empty());
        }
        WHEN("creating an error with a single path") {
            filesystem_error e{description, p, error_code{1, filesystem_category()}};
            CHECK(e.what() != nullptr);
            CHECK(e.code().value() == 1);
            CHECK(e.path1() == p);
            CHECK(e.path2().empty());
        }
        WHEN("creating an error with a two paths") {
            filesystem_error e{description, p, p, error_code{1, filesystem_category()}};
            CHECK(e.what() != nullptr);
            CHECK(e.code().value() == 1);
            CHECK(e.path1() == p);
            CHECK(e.path2() == p);
        }
    }
    
    SECTION("file_status") {
        file_status st{};
        
        WHEN("constructing status with default args") {
            THEN("type and perms are the expected value") {
                CHECK((st.type() == file_type::none));
                CHECK((st.permissions() == perms::unknown));
                CHECK((st.owner() == owner{}));
            }
        }
        
        WHEN("setting properties") {
            st.type(file_type::regular);
            st.permissions(perms::owner_read|perms::owner_write|perms::owner_exec);
            st.owner(owner::process_owner());
            THEN("the properties are updated") {
                CHECK((st.type() == file_type::regular));
                CHECK((st.permissions() == perms::owner_all));
                CHECK((st.owner() == owner::process_owner()));
            }
        }
    }
    
    SECTION("directory_entry") {
        WHEN("constructing a default entry") {
            auto de = directory_entry{};
            THEN("the path is empty") {
                CHECK(de.path().empty());
            }
        }
        
        WHEN("constructing an entry with a path") {
            const auto p = path{"/a/b/c/"}.make_preferred();
            auto de = directory_entry{p};
            THEN("the entry path equals the input path") {
                CHECK(de.path() == p);
            }
        }
        
        // Extension
        WHEN("constructing an entry with an rvalue path") {
            auto p = path{"/a/b/c/"}.make_preferred();
            auto de = directory_entry{std::move(p)};
            THEN("the entry path is moved") {
                CHECK_FALSE(de.path().empty());
                CHECK(p.empty());
            }
        }
        
        WHEN("assigning a path to an entry") {
            auto de = directory_entry{};
            const auto p = path{"/a/b/c/"}.make_preferred();
            de.assign(p);
            THEN("the entry path equals the input path") {
                CHECK(de.path() == p);
            }
        }
        
        WHEN("replacing an entry filename") {
            const auto p = path{"/a/b/c"}.make_preferred();
            auto de = directory_entry{p};
            de.replace_filename(path{"d"});
            THEN("the entry path equals the input path") {
                CHECK(de.path() == p.parent_path()/path{"d"});
            }
        }
        
        WHEN("comparing entries") {
            const auto de = directory_entry{path{"a"}};
            THEN("compare result is the expected value") {
                CHECK((de == directory_entry{path{"a"}}));
                CHECK((de != directory_entry{path{"b"}}));
                CHECK((de < directory_entry{path{"b"}}));
                CHECK((de <= directory_entry{path{"b"}}));
                CHECK((directory_entry{path{"c"}} > de));
                CHECK((directory_entry{path{"c"}} >= de));
            }
        }
    }
    
    WHEN("a path does not exist") {
        const auto p = path{"/a/b/c/does_not_exist"}.make_preferred();
        THEN("status throws a filesystem error") {
            CHECK_THROWS_AS(status(p), filesystem_error);
            CHECK_THROWS_AS(symlink_status(p), filesystem_error);
        }
        AND_THEN("the file type is not_found and last write is invalid") {
            error_code ec;
            const auto s = status(p, ec);
            CHECK((s.type() == file_type::not_found));

            CHECK(last_write_time(p, ec) == times::make_invalid());
            CHECK_THROWS(last_write_time(p, std::chrono::system_clock::now()));
        }
        AND_THEN("the file does not exist") {
            error_code ec;
            CHECK_FALSE(exists(p, ec));
        }
    }
    
    WHEN("path is a file") {
        const auto p = temp_directory_path() / PS_TEXT("fs17test");
        {
            create_file(p);
        }
        const auto st = status(p);
        CHECK(is_regular_file(p));
        CHECK(is_regular_file(st));
        
        CHECK_FALSE(is_directory(st));
        CHECK_FALSE(is_symlink(st));
        CHECK_FALSE(is_socket(st));
        CHECK_FALSE(is_fifo(st));
        CHECK_FALSE(is_device_file(st));

        CHECK(last_write_time(p) > times::make_invalid());
        
        using namespace std::chrono;
        const auto now = std::chrono::system_clock::now();
        const auto t = now - duration_cast<file_time_type::duration>(seconds(3600));
#if !_WIN32
        CHECK_NOTHROW(last_write_time(p, t));
        CHECK(last_write_time(p) < now);
#else
        CHECK_THROWS(last_write_time(p, t)); // not implemented yet
#endif
        
        REQUIRE(remove(p));
    }

    WHEN("path is a symlink/junction") {
#if _WIN32
        auto p = path{R"(C:\Users\All Users)"}; // symlink (Since Vista)
        CHECK_FALSE(is_directory(p));
        CHECK(is_symlink(p));

        p = path{R"(C:\Users\Default User)"}; // junction (ditto)
        CHECK_FALSE(is_symlink(p));
        CHECK(is_directory(p));
        CHECK(is_mountpoint(p));
#endif
    }
    
    WHEN("path is a device") {
        path p {
#if !_WIN32
            "/dev/random"
#else
            R"(\\.\C:\)"
#endif
        };
        CHECK(is_device_file(p));
    }
    
    WHEN("getting the temp dir") {
        error_code ec;
        const auto p = temp_directory_path(ec);
        CHECK_FALSE(p.empty());
        THEN("the path exists") {
            CHECK(exists(p, ec));
        } AND_THEN("the path is a directory") {
            CHECK(is_directory(p, ec));
        }
    }
    
#if !_WIN32
    WHEN("getting the temp dir and TMPDIR is not set") {
        const char* t = ::getenv(ifilesystem::TMPDIR);
        if (!t) {
            t = "";
        }
        std::string save{t};
        if (!save.empty()) {
            ::unsetenv(ifilesystem::TMPDIR);
            std::unique_ptr<std::string, void(*)(const std::string*)>{&save, [](const std::string* s){ ::setenv(ifilesystem::TMPDIR, s->c_str(), 0); }};
        }
        
        error_code ec;
        const auto p = temp_directory_path(ec);
        CHECK_FALSE(p.empty());
        const auto st = status(p, ec);
        THEN("the path exists") {
            CHECK(exists(st));
        } AND_THEN("the path is a directory") {
            CHECK(is_directory(st));
        }
    }
#endif

    WHEN("getting the current path") {
        error_code ec;
        const auto p = current_path(ec);
        CHECK_FALSE(p.empty());
        const auto st = status(p, ec);
        THEN("the path exists") {
            CHECK(exists(st));
        } AND_THEN("the path is a directory") {
            CHECK(is_directory(st));
        }
    }

    const auto uniqueName = path{PS_TEXT("EC160FB0-4E55-46F5-B16D-8149A260FA27")};

    WHEN("comparing path equivalence") {
        const auto p = temp_directory_path();
        CHECK(equivalent(p, temp_directory_path()));
        CHECK_FALSE(equivalent(p.parent_path(), p));

        const auto nop = current_path() / uniqueName;
        error_code ec;
        REQUIRE_FALSE(exists(nop, ec));
        CHECK_FALSE(equivalent(p, nop, ec));
        CHECK_FALSE(equivalent(nop, p, ec));
    }
    
    WHEN("setting the current path") {
        error_code ec;
        const auto p = temp_directory_path();
        CHECK_NOTHROW(current_path(p));
        THEN("the path is changed") {
            CHECK(equivalent(current_path(), p)); // current path may have resolved symlinks now while p does not
        }
    }

    WHEN("setting the current path to an invalid dir") {
        CHECK_THROWS(current_path(current_path().root_path() / uniqueName));
    }

    WHEN("resolving a relative path") {
        const auto base = current_path();
        const auto p = uniqueName;
        CHECK(canonical(p) == base / p);
    }

    WHEN("resolving a partial path") {
        const auto base = current_path();
        const auto p = path{PS_TEXT("a")} / uniqueName;
        CHECK(canonical(p) == base / p);
    }

    WHEN("resolving an absolute path") {
        const auto p = current_path() / uniqueName;
        CHECK(canonical(p) == p);
    }
    
    WHEN("resolving a tilde-prefixed path") {
        const auto tilde  = path{PS_TEXT("~")};
        const auto home = canonical(home_directory_path()); // home path may be a symlink
        CHECK(canonical(tilde) == home);
        const auto subp = path{PS_TEXT("a/b/c/d")}.make_preferred();
        CHECK(canonical(tilde / subp) == home / subp);
    }
    
    WHEN("getting the home dir") {
        error_code ec;
        const auto p = home_directory_path(ec);
        CHECK_FALSE(p.empty());
        const auto st = status(p, ec);
        THEN("the path exists") {
            CHECK(exists(st));
        } AND_THEN("the path is a directory") {
            CHECK(is_directory(st));
        }
    }

#if !_WIN32
    WHEN("getting the home dir for an invalid user") {
        error_code ec;
        const auto p = ifilesystem::home_directory_path(access_control_identity::invalid_user(), ec);
        CHECK(p.empty());
        CHECK(ec.value() != 0);
    }
#endif

    WHEN("getting the mount path for an empty path") {
        CHECK_THROWS(mount_path(""_p));
        CHECK_THROWS(is_mountpoint(""_p));
    }
    
    WHEN("getting the mount path for a non-existant path") {
        auto p = temp_directory_path() / uniqueName;
        error_code ec;
        REQUIRE_FALSE(exists(p, ec));
        CHECK_THROWS(mount_path(p));
        CHECK_THROWS(is_mountpoint(p));
        
        p = ".."_p / uniqueName;
        REQUIRE_FALSE(exists(p, ec));
        CHECK_THROWS(mount_path(p));
        CHECK_THROWS(is_mountpoint(p));
    }
    
    WHEN("getting the mount path for a path") {
        const auto p = temp_directory_path();
        error_code ec;
        REQUIRE(exists(p, ec));
        
        const auto mp = mount_path(p, ec);
        CHECK(ec.value() == 0);
        REQUIRE_FALSE(mp.empty());
        CHECK(is_directory(mp));
        CHECK(is_mountpoint(mp));
        
        // Relative path check
        const auto fp = p / uniqueName;
        REQUIRE_FALSE(exists(fp, ec));
        {
            create_file(fp);
        }
        CHECK_FALSE(is_mountpoint(fp));
        
        current_path(temp_directory_path());
        const auto rp = fp.filename();
        REQUIRE(equivalent(canonical(rp), fp));
        CHECK(mount_path(rp, ec) == mp);
        
        REQUIRE(remove(fp));
    };
    
    WHEN("getting the mount path for a mount path") {
        error_code ec;
        const auto mp = mount_path(temp_directory_path(), ec);
        REQUIRE_FALSE(mp.empty());
        CHECK(mount_path(mp, ec) == mp);
    }

    WHEN("getting the cache dir") {
        error_code ec;
        CHECK_NOTHROW(cache_directory_path());
        const auto p = cache_directory_path(ec);
        CHECK_FALSE(p.empty());
        const auto st = status(p, ec);
        CHECK(exists(st));
        CHECK(is_directory(st));
    }
    
    WHEN("getting standard dir paths") {
        path p;
        CHECK_NOTHROW(p = standard_directory_path(domain::user, standard_directory::app_data, standard_directory_options::create));
        CHECK_NOTHROW(is_directory(p));
        
        CHECK_NOTHROW(p = standard_directory_path(domain::shared, standard_directory::app_data, standard_directory_options::create));
        CHECK_NOTHROW(is_directory(p));
    }
    
    WHEN("getting an invalid standard dir path") {
        constexpr auto invalid = static_cast<standard_directory>(std::numeric_limits<typename std::underlying_type<standard_directory>::type>{}.max());
        CHECK_THROWS(standard_directory_path(domain::user, invalid, standard_directory_options::none));
    }

    SECTION("create and remove dirs") {
        const auto p  = temp_directory_path() / PS_TEXT("fs17test");
        
        WHEN("creating a directory that does not exist") {
            error_code ec;
            REQUIRE_FALSE(is_directory(p, ec));
            CHECK(create_directory(p));
            REQUIRE(remove(p));
        }
        
        WHEN("creating a directory that exists") {
            const auto d = temp_directory_path();
            REQUIRE(is_directory(d));
            CHECK(create_directory(d));
        }
        
        WHEN("creating a directory that exists as a non-dir") {
            error_code ec;
            REQUIRE_FALSE(is_directory(p, ec));
            {
                create_file(p);
            }
            CHECK_THROWS(create_directory(p));
            REQUIRE(remove(p));
        }
        
        WHEN("creating a directory with attributes from another directory") {
            error_code ec;
            REQUIRE(create_directory(p));
            
            const auto np = temp_directory_path() / PS_TEXT("fs17test2");
            REQUIRE_FALSE(exists(np, ec));
            CHECK(create_directory(np, p));
            
            // Check create succeeds with an existing dir
            REQUIRE(exists(np, ec));
            CHECK(create_directory(np, p));
            
            // Check fails with existing non-dir
            REQUIRE(remove(np));
            {
                create_file(np);
            }
            CHECK_THROWS(create_directory(np, p));

            REQUIRE(remove(p));
            REQUIRE(remove(np));
        }
        
        WHEN("creating a directory that has missing parent components") {
            error_code ec;
            REQUIRE_FALSE(is_directory(p, ec));
            // Check create succeeds for a single non-existant dir
            CHECK(create_directories(p));
            REQUIRE(remove(p));
            
            const auto fp = p / PS_TEXT("1") / PS_TEXT("2");
            CHECK_THROWS(create_directory(fp));
            
            CHECK(create_directories(fp));
            
            REQUIRE(is_directory(fp, ec));
            // Check create succeeds with an existing dir
            CHECK(create_directories(fp));
            
            REQUIRE(remove(fp));
            REQUIRE(remove(fp.parent_path()));
            REQUIRE(remove(p));
        }
        
        WHEN("creating a directory that has non-dir parent components") {
            error_code ec;
            REQUIRE_FALSE(is_directory(p, ec));
            REQUIRE(create_directory(p));
            
            const auto fp = p / PS_TEXT("1") / PS_TEXT("2");
            const auto ndp = fp.parent_path();
            {
                create_file(ndp);
            }
            CHECK_THROWS(create_directories(fp));
            
            REQUIRE(remove(ndp));
            REQUIRE(remove(p));
        }
        
         WHEN("removing an item that does not exist") {
            error_code ec;
            REQUIRE_FALSE(exists(p, ec));
            CHECK_THROWS(remove(p));
        }
    }
    
    SECTION("rename") {
        const auto op = temp_directory_path() / PS_TEXT("fs171");
        const auto np = temp_directory_path() / PS_TEXT("fs172");
        error_code ec;
        remove(np, ec);
        REQUIRE_FALSE(exists(np, ec));
        
        WHEN("renaming a non-existant path") {
            remove(op, ec);
            REQUIRE_FALSE(exists(op, ec));
            CHECK_THROWS(rename(op, np));
            REQUIRE_FALSE(exists(np, ec));
        }
        
        WHEN("renaming a file to a non-existant path") {
            create_file(op);
            REQUIRE(exists(op));
            
            CHECK_NOTHROW(rename(op, np));
            
            CHECK_FALSE(exists(op, ec));
            REQUIRE(remove(np));
        }
        
        WHEN("renaming a file to an existing file") {
            create_file(op);
            REQUIRE(exists(op));
            
            create_file(np);
            REQUIRE(exists(np));
            
            CHECK_NOTHROW(rename(op, np));
            
            CHECK_FALSE(exists(op, ec));
            REQUIRE(remove(np));
        }
        
        WHEN("renaming a file to an existing directory") {
            create_file(op);
            REQUIRE(exists(op));
            
            REQUIRE(create_directory(np));
            REQUIRE(exists(np));
            
            CHECK_THROWS(rename(op, np));
            
            REQUIRE(remove(op));
            REQUIRE(remove(np));
        }
        
        WHEN("renaming a dir to a non-existant path") {
            create_directory(op);
            REQUIRE(exists(op));
            
            CHECK_NOTHROW(rename(op, np));
            
            CHECK_FALSE(exists(op, ec));
            REQUIRE(remove(np));
        }
        
#if !_WIN32 // POSIX behavior that Windows does not support
        WHEN("renaming a dir to an existing dir") {
            create_directory(op);
            REQUIRE(exists(op));
            
            create_directory(np);
            REQUIRE(exists(np));
            
            CHECK_NOTHROW(rename(op, np));
            
            CHECK_FALSE(exists(op, ec));
            REQUIRE(remove(np));
        }
        
        WHEN("renaming a dir to an existing file") {
            create_directory(op);
            REQUIRE(exists(op));
            
            create_file(np);
            REQUIRE(exists(np));
            
            CHECK_THROWS(rename(op, np));
            
            REQUIRE(remove(op));
            REQUIRE(remove(np));
        }
#endif // !_WIN32
    }
}
