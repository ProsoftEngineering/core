// Copyright © 2017-2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <prosoft/core/modules/filesystem/filesystem.hpp>
#include <prosoft/core/modules/filesystem/filesystem_snapshot.hpp>

#if PS_HAVE_FILESYSTEM_SNAPSHOT

#if __APPLE__
#include <sys/mount.h>
#endif

#include <prosoft/core/modules/system_identity/identity.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace prosoft::filesystem;

TEST_CASE("filesystem_snapshot") {
    constexpr auto test_id = PS_TEXT("c4775ab0-fb84-11e6-9598-0800200c9a66");

#if _WIN32
    WHEN("creating an id") {
        CHECK_THROWS(snapshot_id(PS_TEXT("")));
        CHECK_THROWS(snapshot_id(PS_TEXT("hello world")));
        CHECK_NOTHROW(snapshot_id(test_id));
    }
#endif
    
    WHEN("comparing ids") {
        CHECK(snapshot_id() == snapshot_id());
        CHECK_FALSE(snapshot_id() != snapshot_id());
        CHECK(snapshot_id(test_id) == snapshot_id(test_id));
        CHECK(snapshot_id(test_id) != snapshot_id());
        CHECK(snapshot_id(test_id).string() == std::string("c4775ab0-fb84-11e6-9598-0800200c9a66"));
    }

    WHEN("attaching an invalid snapshot") {
        auto snap = snapshot(snapshot_id());
        CHECK_THROWS(attach_snapshot(snap, path{}));
    }

    WHEN("detaching an invalid snapshot") {
        auto snap = snapshot(snapshot_id());
        CHECK_THROWS(detach_snapshot(snap));
    }

    WHEN("deleting an invalid snapshot") {
        auto snap = snapshot(snapshot_id());
        CHECK_THROWS(delete_snapshot(snap));
    }

    static bool flag{};
    static bool run_snap{};
    if (!flag) {
        flag = true;

#if _WIN32
        using namespace prosoft::system;
        std::error_code ec;
        auto admin = is_member(identity::admin_group(), ec);
        if (!admin) {
            std::cout << "snapshot tests require admin" << "\n";
            return;
        }

        BOOL wow{};
        IsWow64Process(GetCurrentProcess(), &wow);
        if (wow) {
            std::cout << "32bit snapshot tests require 32bit Windows" << "\n";
            return;
        }
#elif __APPLE__
        struct statfs sb;
        if (0 != statfs("/", &sb) || std::string{"apfs"} != sb.f_fstypename) {
            return;
        }
#endif
        run_snap = true;
    }

    if (!run_snap) {
        return;
    }

    WHEN("creating a snapshot with an empty path") {
        CHECK_THROWS(create_snapshot(PS_TEXT("")));
    }

    WHEN("creating a snapshot") {
#if _WIN32
        static const auto root = PS_TEXT("C:\\");
        static const auto mount_path = PS_TEXT("S:\\");
        static const auto system_root = PS_TEXT("Windows");
#else
        // attach_snapshot in macOS uses mount_apfs, which on Big Sur requires explicit
        // volume name ("/System/Volumes/Data" instead of "/"). It also works in 10.15.
        const auto root = "/System/Volumes/Data";
        const auto mount_path = temp_directory_path() / path{std::string{"snap"}.append(std::to_string(getpid()))};
        static const auto system_root = "System";
#endif
        error_code ec;
        auto snap = create_snapshot(root, ec); // This can fail normally for a variety of reasons.
        if (!ec) {
            CHECK_THROWS(detach_snapshot(snap));
            CHECK_THROWS(attach_snapshot(snap, PS_TEXT("")));
            const path mount{mount_path};
            REQUIRE_FALSE(exists(mount, ec));
            attach_snapshot(snap, mount, ec);
#if _WIN32
            CHECK(!ec);
#else
            // attach_snapshot on macOS uses mount_apfs, which requires "Full Disk Access"
            // ("mount_apfs: volume could not be mounted: Operation not permitted" error)
            //
            // This fails on Travis xcode12.2 image (macOS 10.15.7).
            if (ec.value() == 0)
#endif
            {
                CHECK_THROWS(delete_snapshot(snap));
                CHECK(exists(mount / system_root, ec));
            }

            const auto snapid{snap.id()};
            {
                snapshot msnap{std::move(snap)};
                CHECK(snap.id() == snapshot_id());
                CHECK_FALSE(snap == msnap);
                // Fall through block which will cause detach and delete on destruct
            }
            snapshot dead{snapid};
            CHECK_THROWS(detach_snapshot(dead));
            #if notyet // This spuriously succeeds on Win Server 2012r2 (AppVeyor). Win10 1607 fails as expected.
            CHECK_THROWS(delete_snapshot(dead));
            #endif
            // There will be another delete attempt on destruction of 'dead'
        } else {
            std::cerr << "WARNING: failed to create test snapshot\n";
        }
    }
}

#endif // PS_HAVE_FILESYSTEM_SNAPSHOT
