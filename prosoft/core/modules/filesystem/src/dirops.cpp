// Copyright © 2016, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <prosoft/core/config/config.h>
#include "fsconfig.h"

#include <stdio.h>

#if !_WIN32
#include <sys/errno.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <windows.h>
#include <shlobj.h>
#include <cwchar>
#endif

#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

// mount_path dependencies
#if PS_FS_HAVE_BSD_STATFS
#include <sys/mount.h>
#include <sys/param.h>
#elif !_WIN32
#include <sys/vfs.h>
#endif
#if PS_FS_HAVE_MNTENT_H
#include <mntent.h>
#endif

#include <unique_resource.hpp>
#include <string/platform_convert.hpp>

#include "filesystem.hpp"
#include "filesystem_private.hpp"

namespace {
using namespace prosoft;
using namespace prosoft::filesystem;

struct FILE_delete {
    void operator()(FILE* p) noexcept {
        (void)::fclose(p);
    }
};
using unique_file = std::unique_ptr<FILE, FILE_delete>;

void assert_directory_exists(path& p, error_code& ec) {
    if (ec || p.empty() || !is_directory(p, ec)) {
        p.clear();
        if (!ec) {
#if !_WIN32
            constexpr int default_err = ENOENT;
#else
            constexpr int default_err = ERROR_PATH_NOT_FOUND;
#endif
            ec.assign(default_err, filesystem_category());
        }
    }
}

class mkdir_err_policy {
    static constexpr auto eexist =
#if !_WIN32
    EEXIST;
#else
    ERROR_ALREADY_EXISTS;
#endif
public:
    int operator()(const path& p) {
        auto ec = system::system_error();
        PSASSERT(ec.value() > 0, "Broken assumption");
        error_code dummy;
        if (eexist == ec.value() && is_directory(p, dummy)) {
            return 0;
        }
        return ec.value();
    }
};

int mkdir(const path& p, perms ap) noexcept {
#if !_WIN32
    auto err = ::mkdir(p.c_str(), static_cast<mode_t>(ap & perms::all));
    if (PS_UNEXPECTED(-1 == err && EINTR == errno)) {
        err = ::mkdir(p.c_str(), static_cast<mode_t>(ap & perms::all));
    }
    if (!err) {
        if (is_set(ap & perms::mask & ~perms::all)) { // set extended bits, mkdir does not support these directly
            (void)::chmod(p.c_str(), static_cast<mode_t>(ap & perms::mask));
        }
        return 0;
    } else {
        return mkdir_err_policy{}(p);
    }
#else
    (void)ap;
    if (::CreateDirectoryW(p.c_str(), nullptr)) {
        return 0;
    } else {
        return mkdir_err_policy{}(p);
    }
#endif
}

#if __APPLE__

CF::unique_url make_url(const path& p) {
    using cfstring = to_CFString<path::string_type>;
    
    auto s = cfstring{}(p, cfstring::nocopy);
    PS_THROW_IF_NULLPTR(s);
    error_code ec;
    const auto isdir = is_directory(p, ec); // XXX: will fail if path does not exist
    return CF::unique_url{ ::CFURLCreateWithFileSystemPath(kCFAllocatorDefault, s.get(), kCFURLPOSIXPathStyle, isdir) };
}

#endif // __APPLE__

bool is_mounttrigger(const path& p, error_code& ec) {
    ec.clear();
#if __APPLE__ && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7
    if (auto url = make_url(p)) {
        CFBooleanRef val = nullptr;
        (void)::CFURLCopyResourcePropertyForKey(url.get(), kCFURLIsMountTriggerKey, &val, nullptr);
        if (val) {
            return static_cast<bool>(CFBooleanGetValue(val));
        } else {
            ifilesystem::error(ENOTSUP, ec);
        }
    } else {
        ifilesystem::error(EINVAL, ec);
    }
#elif _WIN32
    if (ifilesystem::fattrs(p, FILE_ATTRIBUTE_REPARSE_POINT, ec)) {
        ::WIN32_FIND_DATAW data;
        if (::FindFirstFile(p.c_str(), &data)) {
            // MOUNT_POINT is either an actual mount point or a Junction. A junction is like a symlink,
            // but can only refer to absolute directory paths.
            // Old but useful info: http://www.codeproject.com/Articles/21202/Reparse-Points-in-Vista
            return IO_REPARSE_TAG_MOUNT_POINT == data.dwReserved0;
        } else {
            ifilesystem::system_error(ec);
        }
    }
#else
    (void)p;
    ifilesystem::error(ENOTSUP, ec);
#endif
    return false;
}

} // anon

namespace prosoft {
namespace filesystem {
inline namespace v1 {

bool create_directories(const path& p, perms perm) {
    error_code ec;
    const auto good = create_directories(p, perm, ec);
    PS_THROW_IF(!good, filesystem_error("create dirs failed", p, ec));
    return good;
}

bool create_directories(const path& p, perms perm, error_code& ec) noexcept {
    constexpr auto enoent =
#if !_WIN32
    ENOENT;
#else
    ERROR_PATH_NOT_FOUND;
#endif
    
    if (create_directory(p, perm, ec)) {
        return true;
    } else if (enoent == ec.value()) {
        const auto parent = p.parent_path();
        if (create_directories(parent, perm, ec)) {
            return create_directory(p, perm, ec);
        }
    }
    return false;
}

bool create_directory(const path& p, perms perm) {
    error_code ec;
    const auto good = create_directory(p, perm, ec);
    PS_THROW_IF(!good, filesystem_error("create dir failed", p, ec));
    return good;
}

bool create_directory(const path& p, perms perm, error_code& ec) noexcept {
    ec.clear();
    const auto err = mkdir(p, perm);
    if (!err) {
        return true;
    } else {
        ifilesystem::error(err, ec);
        return false;
    }
}

bool create_directory(const path& p, const path& cloneFrom) {
    error_code ec;
    const auto good = create_directory(p, cloneFrom, ec);
    PS_THROW_IF(!good, filesystem_error("clone dir failed", p, cloneFrom, ec));
    return good;
}

bool create_directory(const path& p, const path& cloneFrom, error_code& ec) noexcept {
    ec.clear();
#if !_WIN32
    const auto st = status(cloneFrom, ec);
    if (!ec) {
        const auto err = mkdir(p, st.permissions());
        if (!err) {
            return true;
        } else {
            ifilesystem::error(err, ec);
        }
    }
#else
    if (::CreateDirectoryExW(cloneFrom.c_str(), p.c_str(), nullptr)) {
        return true;
    } else {
        const auto err = mkdir_err_policy{}(p);
        if (!err) {
            return true;
        } else {
            ifilesystem::error(err, ec);
        }
    }
#endif
    return false;
}

bool remove(const path& p) {
    error_code ec;
    const auto good = remove(p, ec);
    PS_THROW_IF(!good, filesystem_error("remove failed", p, ec));
    return good;
}

bool remove(const path& p, error_code& ec) noexcept {
#if !_WIN32
    constexpr auto rem = ::remove;
#else
    // XXX: ::remove() fails with EACCESS in tests. Not sure why.
    static auto rem = [](const wchar_t* p) noexcept -> int {
        if (::DeleteFileW(p)) {
            return 0;
        } else {
            // DeleteFile returns ERROR_ACCESS_DENIED for both a perms error AND if the target is a dir (on Win10).
            // Better to not rely on that odd behavior.
            if (ERROR_FILE_NOT_FOUND != ::GetLastError()) { 
                ::SetLastError(0);
                if (::RemoveDirectoryW(p)) {
                    return 0;
                }   
            }
        }
        return -1;
    };
#endif
    ec.clear();
    if (0 == rem(p.c_str())) {
        return true;
    } else {
        system::system_error(ec);
        return false;
    }
}

path temp_directory_path() {
    error_code ec;
    auto p = temp_directory_path(ec);
    PS_THROW_IF(ec.value(), filesystem_error("temporary dir failed", p, ec));
    return p;
}

path temp_directory_path(error_code& ec) {
    ec.clear();

    path p;
#if !_WIN32
    const char* t = ::getenv(ifilesystem::TMPDIR);
    if (!t || *t == 0) {
        constexpr const char* default_tmpdir = "/tmp";
        t = default_tmpdir;
#if __APPLE__
        std::unique_ptr<char[]> buf;
        const auto len = ::confstr(_CS_DARWIN_USER_TEMP_DIR, nullptr, 0);
        if (len > 0) {
            buf.reset(new char[len]);
            if (::confstr(_CS_DARWIN_USER_TEMP_DIR, buf.get(), len) > 0 && *buf.get() != 0) {
                t = buf.get();
            }
        }
#endif // __APPLE__
    }
    PSASSERT_NOTNULL(t);
    p = t;
#else // !_WIN32
    std::wstring buf(::GetTempPathW(0, nullptr), 0); // It's not documented, but the returned size accounts for the null term.
    if (buf.size() > 1 && ::GetTempPathW(static_cast<DWORD>(buf.size()), &buf[0]) > 1) {
        PSASSERT(*(--buf.end()) == 0, "Broken assumption.");
        buf.erase(--buf.end()); // null term
        p = std::move(buf);
    } else {
        ifilesystem::system_error(ec);
    }
#endif // !_WIN32

    assert_directory_exists(p, ec);
    return p;
}

path home_directory_path() {
    error_code ec;
    auto p = home_directory_path(ec);
    PS_THROW_IF(ec.value(), filesystem_error("home dir failed", p, ec));
    return p;
}

path ifilesystem::home_directory_path(const access_control_identity& cid, error_code& ec) {
    ec.clear();
    
    path p;
#if !_WIN32
    struct passwd pw;
    struct passwd* result;
    char buf[PATH_MAX];
    if (0 == ::getpwuid_r(cid.system_identity(), &pw, buf, sizeof(buf), &result)) {
        if (result) {
            p = result->pw_dir;
        } else {
            ifilesystem::error(ESRCH, ec); // ENOENT is used to represent a non-existent dir
        }
    } else {
        ifilesystem::system_error(ec);
    }
#else
    (void)cid; // TODO?
    static auto system_directory = [](REFKNOWNFOLDERID t, error_code& ec) -> path {
        windows::unique_taskmem<wchar_t> buf;
        const auto err = ::SHGetKnownFolderPath(t, 0, nullptr, handle(buf));
        if (S_OK == err) {
            return path{buf.get()};
        } else {
            ifilesystem::error(err, ec);
            return {};
        }
    };
    p = system_directory(FOLDERID_Profile, ec);
#endif
    
    assert_directory_exists(p, ec);
    return p;
}

bool is_mountpoint(const path& p) {
    error_code ec;
    auto val = is_mountpoint(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("mount point failed", p, ec));
    return val;
}

bool is_mountpoint(const path& p, error_code& ec) {
    ec.clear();
    const auto isdir = is_directory(symlink_status(p, ec));
    const auto match = isdir && equivalent(p, mount_path(p, ec));
#if !_WIN32
    return match || (isdir && is_mounttrigger(p, ec));
#else
    return match || is_mounttrigger(p, ec);
#endif
}

path mount_path(const path& p) {
    error_code ec;
    auto mp = mount_path(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("mount path failed", p, ec));
    return mp;
}

path mount_path(const path& p, error_code& ec) {
    ec.clear();
    
    if (PS_UNEXPECTED(p.empty())) { // Empty may be a valid relative path on some systems (Win32), but we'll treat it as an error.
        ec.assign(EINVAL, system::posix_category());
        return {};
    }
    
#if PS_FS_HAVE_BSD_STATFS
    struct statfs fs;
    if (0 == ::statfs(p.c_str(), &fs)) {
        return path{ fs.f_mntonname };
    } else {
        ifilesystem::system_error(ec);
        return {};
    }
#elif PS_FS_HAVE_MNTENT_H
    // "/proc/mounts" represents the actual state of the system, while /etc/fstab is statically deifned and may not have entries for automount filesystems.
    #if __linux__
    constexpr auto mtab = "/proc/mounts";
    #else
    constexpr auto mtab = _PATH_MNTTAB;
    #endif
    struct stat sb;
    if (0 == ::stat(p.c_str(), &sb)) {
        if (auto mt = unique_file{::setmntent(mtab, "r")}) {
            constexpr size_t bufSize = 4096;
            auto buf = make_malloc_throw<char>(bufSize);
            const auto device = sb.st_dev;
            struct mntent me;
            while (::getmntent_r(mt.get(), &me, buf.get(), bufSize)) { // XXX: getmntent_r() is a GNU extension.
                if (0 == ::stat(me.mnt_dir, &sb) && device == sb.st_dev) {
                    return path{me.mnt_dir};
                }
            }
        }
        ifilesystem::error(ENOENT, ec);
    } else {
        ifilesystem::system_error(ec);
    }
    return {};
#elif _WIN32
    if (!exists(p, ec)) { // For compat with UNIX implementation. GetVolumePathName does not err in this case.
        return {};
    }

    native_string_type buf(1024, 0);
    if (::GetVolumePathNameW(p.c_str(), &buf[0], (DWORD)buf.size())) {
         buf.erase(std::wcslen(buf.data()));
         buf.shrink_to_fit();
    } else {
        ifilesystem::system_error(ec);
        buf = native_string_type{};
    }
    return {buf};
#else
    ifilesystem::error(ENOTSUP, ec);
    return {};
#endif
}

} // v1
} // filesystem
} // prosoft

#if PSTEST_HARNESS
// Internal tests.
#include "catch.hpp"

TEST_CASE("dirops internal") {
    using namespace prosoft::filesystem;
    
    SECTION("directory exists") {
        auto p = ""_p; // empty path fail
        auto ec = error_code{0, filesystem_category()};
        REQUIRE(ec.value() == 0);
        REQUIRE(p.empty());
        assert_directory_exists(p, ec);
        CHECK(ec.value() != 0);

        p = "adfhaosidufbasdfoiuabsdofiubasodbfaosdbfaosbdfoabsdf"_p; // not dir fail
        ec.clear();
        REQUIRE(ec.value() == 0);
        REQUIRE_FALSE(p.empty());
        assert_directory_exists(p, ec);
        CHECK(ec.value() != 0);
        CHECK(p.empty());

        p = "test"_p;
        ec = error_code{5, filesystem_category()}; // error fail
        REQUIRE_FALSE(p.empty());
        assert_directory_exists(p, ec);
        CHECK(ec.value() == 5);
        CHECK(p.empty());
    }
}
#endif // PSTEST_HARNESS
