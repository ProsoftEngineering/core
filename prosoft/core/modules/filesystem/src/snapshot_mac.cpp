// Copyright © 2018-2020, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <sys/mount.h>
#include <sys/stat.h>

#include "filesystem.hpp"
#include "filesystem_snapshot.hpp"
#include "spawn.hpp"
#include <string/string_component.hpp>

#include "filesystem_private.hpp"

namespace {

enum tmutil_err : int {
    tmutil_createsnapshot = 65535,
    tmutil_createsnapshot_unknown_output,
    tmutil_listsnapshots,
    tmutil_listsnapshots_unknown_output,
    tmutil_deletesnapshot,
};

class tmutil_error_category : public std::error_category {
public:
    tmutil_error_category() noexcept
        : std::error_category() {}

    virtual const char* name() const noexcept override {
        return "tmutil";
    }

    virtual std::string message(int ec) const override {
        switch (ec) {
            case tmutil_createsnapshot:
                return "Failed to create snapshot.";

            case tmutil_createsnapshot_unknown_output:
                return "Unexpected snapshot creation result.";

            case tmutil_listsnapshots:
                return "Failed to find snapshot.";

            case tmutil_listsnapshots_unknown_output:
                return "Unexpected snapshot ID format.";

            case tmutil_deletesnapshot:
                return "Failed to delete snapshot.";

            default:
                return "Unknown error.";
        };
    }
};

const std::error_category& tmutil_category() {
    static const tmutil_error_category cat;
    return cat;
}

std::string datestr(const prosoft::filesystem::snapshot_id& sid) {
    using namespace prosoft;
    static const auto tmprefix = std::string{"com.apple.TimeMachine."};
    if (starts_with(sid.m_id, tmprefix)) {
        auto d = sid.m_id.substr(tmprefix.length());
        // remove '.local' suffix
        const auto i = d.find(".");
        if (i != std::string::npos) {
            d.erase(i);
        }
        return d;
    } else {
        return "";
    }
}

std::string tmutil_getsnapshot(const std::string& cout, const std::string& date, std::error_code& ec) { // cout should be trimmed
    /*
    ...
    com.apple.TimeMachine.2018-02-15-113536
    com.apple.TimeMachine.2018-02-15-130128
    com.apple.TimeMachine.2018-02-15-173245
    com.apple.TimeMachine.2018-02-15-183113
    com.apple.TimeMachine.2018-02-15-193329
    com.apple.TimeMachine.2018-02-15-195835
    \n
    -- 10.15 --
    com.apple.TimeMachine.2020-01-22-144343.local
    */
    const auto searchid = std::string("com.apple.TimeMachine.").append(date);
    const auto where = cout.find(searchid);
    if (where == std::string::npos) {
        if (cout.find(date) == std::string::npos) {
            ec.assign(ENOTSUP, std::system_category()); // Volume must not be in TM backup.
        } else {
            ec.assign(tmutil_listsnapshots_unknown_output, tmutil_category());
        }
        return "";
    }
    
    const auto i = cout.find("\n", where);
    if (i == where) {
        ec.assign(tmutil_listsnapshots_unknown_output, tmutil_category());
        return "";
    }
    
    ec.clear();
    
    if (i != std::string::npos) {
        return cout.substr(where, i-where);
    }
    return cout.substr(where);
}

std::string tmutil_findsnapshot(const prosoft::filesystem::path& path, const std::string& date, std::error_code& ec) {
    using namespace prosoft;
    const auto mp = filesystem::mount_path(path, ec);
    if (!mp.empty()) {
        spawn_cout cout;
        spawn_cerr cerr;
        std::system_error err{0, std::system_category()};
        spawn("tmutil", spawn_args{"listlocalsnapshots", mp.string()}, cout, cerr, err);
        ec = err.code();
        if (!ec) {
            trim(cout);
            return tmutil_getsnapshot(cout, date, ec);
        }
    } else {
        ec.assign(ENOENT, std::system_category());
    }
    return "";
}

void tmutil_delete(const std::string& date, std::error_code& ec) {
    using namespace prosoft;
    spawn_cout cout;
    spawn_cerr cerr;
    std::system_error err{0, std::system_category()};
    spawn("tmutil", spawn_args{"deletelocalsnapshots", date}, cout, cerr, std::chrono::seconds{5}, err); // This can "hang" if backupd is running.
    ec = err.code();
    if (ec) {
        if (ec.value() != EAGAIN) {
            ec.assign(tmutil_deletesnapshot, tmutil_category());
        }
    }
}

void tmutil_delete(const prosoft::filesystem::snapshot_id& snap, std::error_code& ec) {
    return tmutil_delete(datestr(snap.m_id), ec);
}

// This will create a snapshot for APFS volumes that are backed up with Time Machine. Any other APFS volumes are ignored.
// If Time Machine is off, only the root volume is supported.
std::string tmutil_getsnapshot(const std::string& cout, std::error_code& ec) { // cout should be trimmed
    // Created local snapshot with date: 2018-02-15-195835\n
    static const std::string token{"snapshot with date:"};
    auto i = cout.find(token);
    if (i != std::string::npos) {
        auto snapid = cout.substr(i + token.size());
        prosoft::trim(snapid);
        ec.clear();
        return snapid;
    } else {
        ec.assign(tmutil_createsnapshot_unknown_output, tmutil_category());
    }
    return "";
}

std::string tmutil_snapshot(const prosoft::filesystem::path& path, std::error_code& ec) {
    using namespace prosoft;
    spawn_cout cout;
    spawn_cerr cerr;
    std::system_error err{0, std::system_category()};
    spawn("tmutil", spawn_args{"localsnapshot"}, cout, cerr, std::chrono::seconds{5}, err); // This can "hang" if backupd is running.
    ec = err.code();
    if (ec) {
        if (err.code().value() != EAGAIN) {
            ec.assign(tmutil_createsnapshot, tmutil_category());
        }
        return "";
    }

    trim(cout);
    return tmutil_getsnapshot(cout, ec);
}

std::string mount_opts(const prosoft::filesystem::snapshot& snap) {
    using namespace prosoft::filesystem;
    // ro is implicit for snapshots, but doesn't hurt to specify it
    if (snap.reserved() & snapshot_create_options::nobrowse) {
        return "rdonly,nobrowse";
    } else {
        return "rdonly";
    }
}

void mount_snapshot(const prosoft::filesystem::snapshot& snap, const prosoft::filesystem::path& mp, std::error_code& ec) {
    using namespace prosoft;

    const auto& sid = snap.id();
    spawn_cout cout;
    spawn_cerr cerr;
    std::system_error err{0, std::system_category()};
    spawn("mount_apfs", spawn_args{"-o", mount_opts(snap), "-s", sid.m_id, sid.m_from.string(), mp.string()}, cout, cerr, err);
    ec = err.code();
}

struct force_unmount {
    bool value{false};
};
constexpr force_unmount force{true};
void unmount_snapshot(const prosoft::filesystem::snapshot& snap, std::error_code& ec, force_unmount f = force_unmount{}) {
    using namespace prosoft;
    spawn_cout cout;
    spawn_cerr cerr;
    std::system_error err{0, std::system_category()};
    // umount will sometimes fail with 'resource busy' and recommend diskutil, so just use it instead
    spawn_args args{"unmount"};
    if (f.value) {
        args.push_back("force");
    }
    args.push_back(snap.id().m_to.string());
    spawn("diskutil", args, cout, cerr, err);
    ec = err.code();
}

bool can_snapshot(const struct statfs& sb, std::error_code& ec) {
    static const std::string apfs{"apfs"};
    const bool readonly = (sb.f_flags & MNT_RDONLY);
    if (!readonly && apfs == sb.f_fstypename) {
        ec.clear();
        return true;
    } else {
        if (!readonly) {
            ec.assign(ENOTSUP, std::system_category());
        } else {
            ec.assign(EROFS, std::system_category());
        }
    }
    return false;
}

bool can_snapshot(const prosoft::filesystem::path& path, std::error_code& ec) {
    struct statfs sb;
    if (0 == statfs(path.c_str(), &sb)) {
        // For 10.15 APFS vgroup, root will be mounted read-only and data mounted read-write.
        // statfs is not aware of this, so we have to query the URL resource properties
        std::error_code lec{};
        if (!prosoft::filesystem::ifilesystem::is_mounted_readonly(path, lec)) {
            if (!lec) {
                sb.f_flags &= ~MNT_RDONLY;
            }
        }
        return can_snapshot(sb, ec);
    } else {
        ec.assign(errno, std::system_category());
    }
    return false;
}

enum delete_flags : unsigned {
    detach_force = 0xf0f0f0f0U,
};

} // anon

namespace prosoft {
namespace filesystem {
inline namespace v1 {
class snapshot_manager {
public:
    static snapshot_id& id(snapshot& snap) {
        return snap.m_id;
    }
};

} // v1
} // filesystem
} // prosoft;

prosoft::filesystem::v1::snapshot::~snapshot() {
    std::error_code ec;
    detach_snapshot(*this, ec);
    m_flags = detach_force;
    delete_snapshot(*this, ec);
}

using snapshot = prosoft::filesystem::v1::snapshot;

snapshot prosoft::filesystem::v1::create_snapshot(const path& p, const snapshot_create_options& opts, std::error_code& ec) {
    if (can_snapshot(p, ec)) {
        auto mp = mount_path(p, ec);
        if (ec) {
            return snapshot{snapshot_id{}};
        }

        auto snapid = tmutil_snapshot(p, ec);
        if (!ec) {
            snapid = tmutil_findsnapshot(p, snapid, ec);
            if (!ec) {
                auto sid = snapshot_id{std::move(snapid)};
                sid.m_from = std::move(mp);
                return snapshot{std::move(sid), opts.m_flags};
            } else {
                // remove the snapshot from other disks
                std::error_code ignore;
                tmutil_delete(snapid, ignore);
            }
        }
    }
    return snapshot{snapshot_id{}};
}

void prosoft::filesystem::v1::attach_snapshot(snapshot& snap, const path& mp, std::error_code& ec) {
    if (snap.id().m_id.empty() || snap.id().m_from.empty()) {
        ec.assign(EINVAL, std::system_category());
        return;
    }
    if (snap.id().m_to.empty()) {
        create_directory(mp, ec);
        if (!ec) {
            mount_snapshot(snap, mp, ec);
            if (!ec) {
                snapshot_manager::id(snap).m_to = mp;
            }
        }
    } else {
        ec.assign(EBUSY, std::system_category());
    }
}

void prosoft::filesystem::v1::detach_snapshot(snapshot& snap) {
    error_code ec;
    detach_snapshot(snap, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not detach snapshot", ec));
}

namespace {

void clear_detach_state(prosoft::filesystem::snapshot& snap) {
    std::error_code ec; // ignored
    remove(snap.id().m_to, ec);
    prosoft::filesystem::snapshot_manager::id(snap).m_to.clear();
}

} // anon

void prosoft::filesystem::v1::detach_snapshot(snapshot& snap, std::error_code& ec) {
    if (snap.id().m_id.empty() || snap.id().m_to.empty()) {
        ec.assign(EINVAL, std::system_category());
        return;
    }
    unmount_snapshot(snap, ec);
    if (!ec) {
        clear_detach_state(snap);
    }
}

void prosoft::filesystem::v1::delete_snapshot(snapshot& snap) {
    error_code ec;
    delete_snapshot(snap, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not delete snapshot", ec));
}

void prosoft::filesystem::v1::delete_snapshot(snapshot& snap, std::error_code& ec) {
    if (snap.id().m_id.empty()) {
        ec.assign(EINVAL, std::system_category());
        return;
    }

    if (!snap.id().m_to.empty()) {
        if (snap.reserved() != detach_force) {
            ec.assign(EBUSY, std::system_category());
            return;
        } else {
            unmount_snapshot(snap, ec, force);
            if (!ec) {
                clear_detach_state(snap);
            } else {
                return;
            }
        }
    }

    tmutil_delete(snap.id(), ec);
    if (!ec) {
        snapshot_manager::id(snap).m_id.clear();
    }
}

#if PSTEST_HARNESS
// Internal tests.

#include <catch2/catch_test_macros.hpp>

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

#endif // PSTEST_HARNESS
