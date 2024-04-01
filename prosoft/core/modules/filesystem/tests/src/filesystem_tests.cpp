// Copyright © 2015-2018, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <fstream>
#include <iosfwd>
#include <limits>
#include <type_traits>

#include <prosoft/core/modules/filesystem/filesystem.hpp>
#include <filesystem_private.hpp>

#include <catch2/catch_test_macros.hpp>
#include "fsdirent_catch_fix.hpp"

using namespace prosoft;
using namespace prosoft::filesystem;

#include <fstestutils.hpp>

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
            } AND_THEN ("status is invalid") {
                CHECK_THROWS_AS(de.refresh(), filesystem_error);
                error_code ec;
                CHECK_FALSE(de.exists(ec));
                CHECK(ec);
                ec.assign(0, ec.category());
                CHECK(de.file_size(ec) == 0);
                CHECK(ec);
                CHECK(de.cached_size() == directory_entry::unknown_size);
                ec.assign(0, ec.category());
                CHECK(de.last_write_time(ec) == times::make_invalid());
                CHECK(ec);
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
            auto de = directory_entry{file_type::unknown, 0, file_time_type::duration(0)};
            CHECK(de.cached_type() == file_type::unknown);
            CHECK(de.cached_size() == 0);
            CHECK(de.cached_write_time() == 0);
            
            const auto p = path{"/a/b/c/"}.make_preferred();
            de.assign(p);
            THEN("the entry path equals the input path") {
                CHECK(de.path() == p);
            } AND_THEN("the cached values update") {
                // Assuming path does not exist
                CHECK(de.cached_type() == file_type::none);
                CHECK(de.cached_size() == de.unknown_size);
                CHECK(de.cached_write_time() == PS_FS_ENTRY_INVALID_TIME_VALUE);
            }
        }
        
        WHEN("replacing an entry filename") {
            const auto p = path{"/a/b/c"}.make_preferred();
            auto de = directory_entry{file_type::unknown, 0, file_time_type::duration(0)};
            de.assign_no_refresh(p);
            CHECK(de.cached_type() == file_type::unknown);
            CHECK(de.cached_size() == 0);
            CHECK(de.cached_write_time() == 0);
            
            de.replace_filename(path{"d"});
            THEN("the entry path equals the input path") {
                CHECK(de.path() == p.parent_path()/path{"d"});
            } AND_THEN("the cached values update") {
                // Assuming path does not exist
                CHECK(de.cached_type() == file_type::none);
                CHECK(de.cached_size() == de.unknown_size);
                CHECK(de.cached_write_time() == PS_FS_ENTRY_INVALID_TIME_VALUE);
            }
        }
        
        WHEN("extracting path") {
            const auto p = path{"/a/b/c"}.make_preferred();
            auto de = directory_entry{file_type::unknown, 0, file_time_type::duration(0)};
            de.assign_no_refresh(p);
            CHECK(de.cached_type() == file_type::unknown);
            CHECK(de.cached_size() == 0);
            CHECK(de.cached_write_time() == 0);
            
            auto ep = std::move(de).path();
            THEN("then entry path is empty") {
                CHECK(de.path().empty());
            } AND_THEN("the cached values update") {
                // Assuming path does not exist
                CHECK(de.cached_type() == file_type::none);
                CHECK(de.cached_size() == de.unknown_size);
                CHECK(de.cached_write_time() == PS_FS_ENTRY_INVALID_TIME_VALUE);
            }
        }
        
        WHEN("comparing entries") {
            const auto de = directory_entry{path{"a"}};
            THEN("compare result is the expected value") {
                CHECK((de == directory_entry{path{"a"}}));
                CHECK(de == path("a"));
                CHECK(path("a") == de);
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
        const auto p = temp_directory_path() / process_name("fs17test");
        {
            create_file(p);
            std::fstream os{p.string().c_str()};
            os << "hello";
        }
        const auto st = status(p);
        CHECK(is_regular_file(p));
        CHECK(is_regular_file(st));
        
        CHECK(st.times().has_modified());
        CHECK_FALSE(status(p, status_info::basic).times().has_modified());
        
        CHECK_FALSE(is_directory(st));
        CHECK_FALSE(is_symlink(st));
        CHECK_FALSE(is_socket(st));
        CHECK_FALSE(is_fifo(st));
        CHECK_FALSE(is_device_file(st));

        CHECK(last_write_time(p) > times::make_invalid());
        CHECK(file_size(p) > 0);
        
        using namespace std::chrono;
        const auto now = std::chrono::system_clock::now();
        const auto t = now - duration_cast<file_time_type::duration>(seconds(3600));
        CHECK_NOTHROW(last_write_time(p, t));
        CHECK(last_write_time(p) < now);
        
        REQUIRE(remove(p));
    }

    WHEN("path is a symlink/junction") {
#if _WIN32
        auto p = path{R"(C:\Users\All Users)"}; // symlink (Since Vista)
        CHECK(is_directory(p));
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
        const auto& p = uniqueName;
        CHECK(canonical(p) == base / p);
        CHECK(weakly_canonical(p) == base / p);
        CHECK(absolute(p) == base / p);
    }

    WHEN("resolving a partial path") {
        const auto base = current_path();
        const auto p = path{PS_TEXT("a")} / uniqueName;
        CHECK(canonical(p) == base / p);
        CHECK(weakly_canonical(p) == base / p);
        CHECK(absolute(p) == base / p);
    }

    WHEN("resolving an absolute path") {
        const auto p = current_path() / uniqueName;
        CHECK(canonical(p) == p);
        CHECK(weakly_canonical(p) == p);
        CHECK(absolute(p) == p);
        CHECK(system_complete(p) == p);
    }
    
    WHEN("resolving a tilde-prefixed path") {
        const auto tilde  = path{PS_TEXT("~")};
        const auto home = canonical(home_directory_path()); // home path may be a symlink
        CHECK(canonical(tilde) == home);
        CHECK(weakly_canonical(tilde) == home);
        const auto subp = path{PS_TEXT("a/b/c/d")}.make_preferred();
        CHECK(canonical(tilde / subp) == home / subp);
    }

    WHEN("resolving a relative path with a specified base") {
        const auto base = mount_path(temp_directory_path());
        const auto& p = uniqueName;
        CHECK(canonical(p, base) == base / p);
        CHECK(absolute(p, base) == base / p);
    }

    WHEN("path is empty") {
        const auto cur = current_path();
        CHECK_NOTHROW(canonical(path()));
        CHECK_NOTHROW(weakly_canonical(path()));
        CHECK(absolute(path()) == cur);
        CHECK(system_complete(path()) == cur);

        // pathological case
        CHECK(absolute(path(), path()) == path());
    }
    
#if _WIN32
    WHEN("resolving a root-dir relative path") {
        const path p{PS_TEXT("\\a\\b\\c")};
        const auto cur = current_path();
        CHECK(absolute(p) == cur.root_name() / p);
        CHECK(system_complete(p) == cur.root_name() / p);
    }

    WHEN("resolving a root-name relative path") {
        path p{PS_TEXT("C:a")};
        const auto cur = current_path();
        CHECK(absolute(p) == cur / p.relative_path());
        CHECK(system_complete(p) == cur / p.relative_path());

        p = path{PS_TEXT("Y:a")};
        CHECK(absolute(p) == p.root_name() / cur.relative_path() / p.relative_path());
        // XXX: if the drive exists, it may have a different CWD and this test could fail as system_complete() is aware of the drive specific CWD
        CHECK(system_complete(p) == p.root_name() / p.relative_path());
    }

    WHEN("path is >= than MAX_PATH") {
        path::string_type s;
        std::generate_n(std::back_inserter(s), MAX_PATH, [](){return L'a';});
        using namespace prosoft::filesystem;
        const path base{L"C:\\"};
        const path prefix{LR"(\\?\)"};
        CHECK(canonical(s, base) == prefix / base / s); // relative
        CHECK(canonical(base / s) ==  prefix / base / s); // absolute
    }
#endif

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

    WHEN("getting an unused drive") {
        error_code ec;
#if _WIN32
        CHECK_FALSE(unused_drive(ec).empty());

        // A:, B: and C: can never be returned
        CHECK(ifilesystem::first_unused_drive_letter(0) == L"D:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x1) == L"D:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x2) == L"D:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x4) == L"D:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x7) == L"D:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x8) == L"E:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x1A) == L"F:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x3A) == L"G:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x7A) == L"H:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0xFA) == L"I:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x1FA) == L"J:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x3FA) == L"K:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x7FA) == L"L:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0xFFA) == L"M:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x1FFA) == L"N:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x3FFA) == L"O:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x7FFA) == L"P:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0xFFFA) == L"Q:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x1FFFA) == L"R:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x3FFFA) == L"S:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x7FFFA) == L"T:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0xFFFFA) == L"U:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x1FFFFA) == L"V:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x3FFFFA) == L"W:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x7FFFFA) == L"X:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0xFFFFFA) == L"Y:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x1FFFFFA) == L"Z:\\");
        CHECK(ifilesystem::first_unused_drive_letter(0x3FFFFFF).empty());
        CHECK(ifilesystem::first_unused_drive_letter(0xFFFFFFFF).empty());
#else
        CHECK(unused_drive(ec).empty());
#endif
    }

    SECTION("attribute extensions") {
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
        }

        WHEN("getting the mount path for a mount path") {
            error_code ec;
            const auto mp = mount_path(temp_directory_path(), ec);
            REQUIRE_FALSE(mp.empty());
            CHECK(mount_path(mp, ec) == mp);
        }

        WHEN("getting the hidden attribute") {
            error_code ec;
            CHECK_FALSE(is_hidden(home_directory_path(), ec));
            CHECK_FALSE(is_hidden(temp_directory_path() / PS_TEXT("abcdefghij"), ec)); // non-existent path
#if !_WIN32
            CHECK(is_hidden(temp_directory_path() / ".abcdefghij", ec)); // all dot files are considered hidden, even non-existent ones
            CHECK(is_hidden(".abcdefghij", ec));
            auto p = home_directory_path() / ".ssh";
            if (exists(p, ec)) {
                CHECK(is_hidden(p, ec));
            }
            p = home_directory_path() / ".profile";
            if (exists(p, ec)) {
                CHECK(is_hidden(p, ec));
            }
#else
            path p{PS_TEXT("C:\\Windows\\Installer")};
            if (exists(p, ec)) {
                CHECK(is_hidden(p, ec));
            }
#endif

#if __APPLE__
            CHECK(is_hidden("/Volumes", ec));
#endif
        }

        WHEN("getting the package attribute") {
            error_code ec;
            CHECK_FALSE(is_package(temp_directory_path(), ec));
            CHECK_FALSE(is_package(home_directory_path(), ec));
            CHECK_FALSE(is_package(temp_directory_path() / PS_TEXT("abcdefghij"), ec)); // non-existent path
#ifdef MAC_OS_X_VERSION_MIN_REQUIRED
            CHECK(is_package("/System/Library/CoreServices/Finder.app", ec));
#endif
        }
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
        const auto p  = temp_directory_path() / process_name("fs17test");
        
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
            
            const auto np = temp_directory_path() / process_name("fs17test2");
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
    } // create/remove dirs

    SECTION("symlink") {
        const auto root = temp_directory_path() / process_name("fs17test");

        const auto lnk = root / PS_TEXT("lnk");

        static auto noerr_or_win32_denied = [](const error_code& e) {
 #if !_WIN32
            return !e;
 #else
            return !e || e.value() == ERROR_PRIVILEGE_NOT_HELD;
 #endif
        };
        
        error_code ec;
        // Attempting to RAII_remove each time through results in weird win32 isues where the root dir can end up with an empty DACL and thus no rights.
        // So we'll do a setup/cleanup stage.
        SECTION("setup") {
            create_directory(root, ec);
            REQUIRE(exists(root, ec));
        }

        WHEN("creating a symlink to a file") {
            const auto f  = create_file(root / PS_TEXT("file"));
            REQUIRE(exists(f, ec));
            PS_RAII_REMOVE(f);

            create_symlink(f, lnk, ec);
            CHECK(noerr_or_win32_denied(ec));

            if (!ec) {
                CHECK(is_symlink(lnk, ec));
                REQUIRE(remove(lnk, ec));
            }
            CHECK(exists(f, ec));
        }

        WHEN("creating a symlink to a dir") {
            const auto d = root / PS_TEXT("dir");
            CHECK(create_directory(d, ec));
            REQUIRE(exists(d, ec));
            PS_RAII_REMOVE(d);

            create_directory_symlink(d, lnk, ec);
            CHECK(noerr_or_win32_denied(ec));

            if (!ec) {
                CHECK(is_symlink(lnk, ec));
                REQUIRE(remove(lnk, ec));
            }
            CHECK(exists(d, ec));
        }

        WHEN("creating a symlink to a non-existent path") {
            const auto f  = root / PS_TEXT("file1234567890");
            REQUIRE_FALSE(exists(f, ec));

            create_symlink(f, lnk, ec);
            CHECK(noerr_or_win32_denied(ec));

            if (!ec) {
                CHECK(is_symlink(lnk, ec));
                REQUIRE(remove(lnk, ec));
            }
        }

        SECTION("cleanup") {
            REQUIRE(remove(root, ec));
        }
    } // symlink
    
    SECTION("rename") {
        const auto op = temp_directory_path() / process_name("fs171");
        const auto np = temp_directory_path() / process_name("fs172");
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
