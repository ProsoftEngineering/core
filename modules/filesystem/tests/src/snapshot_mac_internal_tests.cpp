// Copyright © 2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <snapshot_mac_internal.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace prosoft::filesystem;

namespace fs = prosoft::filesystem::v1;

TEST_CASE("snapshot_internal") {
    using namespace prosoft::filesystem;
    CHECK(datestr(snapshot_id("")).empty());
    CHECK(datestr(snapshot_id("com.apple.TimeMachine.2018-02-15-195835")) == "2018-02-15-195835");
    CHECK(datestr(snapshot_id("com.apple.TimeMachine.2020-01-22-144343.local")) == "2020-01-22-144343");

    std::error_code ec;
    auto sid = tmutil_getsnapshot("Created local snapshot with date: 2018-02-15-195835\n", ec);
    CHECK(sid == "2018-02-15-195835");
    CHECK(!ec);
    
    // not trimmed
    sid = tmutil_getsnapshot("com.apple.TimeMachine.2018-02-15-193329\ncom.apple.TimeMachine.2018-02-15-195835\n", sid, ec);
    CHECK(sid == "com.apple.TimeMachine.2018-02-15-195835");
    CHECK(!ec);
    CHECK(datestr(snapshot_id(sid)) == "2018-02-15-195835");
    
    // trimmed
    const char* input = "com.apple.TimeMachine.2018-02-15-193329\ncom.apple.TimeMachine.2020-01-22-144343.local\ncom.apple.TimeMachine.2020-01-22-154343.local";
    sid = tmutil_getsnapshot(input, "2018-02-15-193329", ec);
    CHECK(sid == "com.apple.TimeMachine.2018-02-15-193329");
    CHECK(!ec);
    CHECK(datestr(snapshot_id(sid)) == "2018-02-15-193329");
    
    sid = tmutil_getsnapshot(input, "2020-01-22-144343", ec);
    CHECK(sid == "com.apple.TimeMachine.2020-01-22-144343.local");
    CHECK(!ec);
    CHECK(datestr(snapshot_id(sid)) == "2020-01-22-144343");
    
    sid = tmutil_getsnapshot(input, "2020-01-22-154343", ec);
    CHECK(sid == "com.apple.TimeMachine.2020-01-22-154343.local");
    CHECK(!ec);
    CHECK(datestr(snapshot_id(sid)) == "2020-01-22-154343");

    CHECK(mount_opts(snapshot(snapshot_id(""), 0)) == "rdonly");
    CHECK(mount_opts(snapshot(snapshot_id(""), snapshot_create_options::nobrowse)) == "rdonly,nobrowse");

    struct statfs sb;
    sb.f_flags = 0;
    strlcpy(sb.f_fstypename, "hfs", sizeof(sb.f_fstypename));
    CHECK_FALSE(can_snapshot(sb, ec));
    CHECK(ec);

    ec.clear();
    sb.f_flags = MNT_RDONLY;
    CHECK_FALSE(can_snapshot(sb, ec));
    CHECK(ec);

    ec.clear();
    sb.f_flags = MNT_RDONLY;
    strlcpy(sb.f_fstypename, "apfs", sizeof(sb.f_fstypename));
    CHECK_FALSE(can_snapshot(sb, ec));
    CHECK(ec);

    sb.f_flags = 0;
    CHECK(can_snapshot(sb, ec));
    CHECK(!ec);
}
