// Copyright © 2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include "filesystem.hpp"
#include "filesystem_snapshot.hpp"

#if PS_HAVE_FILESYSTEM_SNAPSHOT

#include "identity.hpp"

#include "catch.hpp"

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
    }

    WHEN("attaching an invalid snapshot") {
        CHECK_THROWS(attach_snapshot(snapshot(snapshot_id()), path{}));
    }

    WHEN("detaching an invalid snapshot") {
        CHECK_THROWS(detach_snapshot(snapshot(snapshot_id())));
    }

    WHEN("deleting an invalid snapshot") {
        CHECK_THROWS(delete_snapshot(snapshot(snapshot_id())));
    }

    static bool flag{};
    static bool run_snap{};
    if (!flag) {
        flag = true;

        using namespace prosoft::system;
        std::error_code ec;
        auto admin = is_member(identity::admin_group(), ec);
        if (!admin) {
            std::cout << "snapshot tests require admin" << "\n";
            return;
        }

#if _WIN32
        BOOL wow{};
        IsWow64Process(GetCurrentProcess(), &wow);
        if (wow) {
            std::cout << "32bit snapshot tests require 32bit Windows" << "\n";
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
        error_code ec;
        auto snap = create_snapshot(PS_TEXT("C:\\"), ec);
        if (!ec) {
            CHECK_THROWS(detach_snapshot(snap));
            CHECK_THROWS(attach_snapshot(snap, PS_TEXT("")));
            const path mount{PS_TEXT("S:\\")};
            REQUIRE_FALSE(exists(mount, ec));
            attach_snapshot(snap, mount, ec);
            CHECK(!ec);
            CHECK_THROWS(delete_snapshot(snap));

            CHECK(exists(mount / PS_TEXT("Windows"), ec));

            const auto snapid{snap.id()};
            {
                snapshot msnap{std::move(snap)};
                CHECK(snap.id() == snapshot_id());
                CHECK_FALSE(snap == msnap);
                // Fall through block which will cause detach and delete on destruct
            }
            snapshot dead{snapid};
            CHECK_THROWS(detach_snapshot(dead));
            CHECK_THROWS(delete_snapshot(dead));
            // There will be another delete attempt ono destruction of 'dead'
        }
    }
}

#endif // PS_HAVE_FILESYSTEM_SNAPSHOT
