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

#include <prosoft/core/config/config.h>
#include "fsconfig.h"

#if !_WIN32
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <windows.h>
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

#include <limits>

#include <uniform_access.hpp>
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

#if __APPLE__

CF::unique_url make_url(const path& p) {
    error_code ec;
    const auto isdir = is_directory(p, ec); // XXX: will fail if path does not exist
    return CF::unique_url{ ::CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, as_utf8(data(p.native())), data_size(p.native()), isdir) };
}

inline fs::error_code error(CFErrorRef e) {
    PSASSERT_NOTNULL(e);
    const auto code = e ? CFErrorGetCode(e) : CFIndex{-1};
    using code_type = decltype(code);
    constexpr auto min = code_type{std::numeric_limits<int>::min()};
    constexpr auto max = code_type{std::numeric_limits<int>::max()};
    return fs::error_code{static_cast<int>(prosoft::clamp(code, min, max)), std::generic_category()};
}

template <typename CFT>
unique_cftype<CFT> get_property(const path& p, CFStringRef key, error_code& ec) {
    PSASSERT_NOTNULL(key);
    auto url = make_url(p.native());
    unique_cftype<CFT> value;
    CF::unique_error err;
    if (CFURLCopyResourcePropertyForKey(url.get(), key, handle(value), handle(err))) {
        ec.clear();
    } else {
        ec = error(err.get());
    }
    return value;
}

#endif // __APPLE__

#if PS_FS_HAVE_BSD_STATFS
inline bool has_flag(const struct stat& sb, unsigned f) {
    return f == (sb.st_flags & f);
}

bool has_flag(const path& p, unsigned f, error_code& ec) {
    PSASSERT(f != 0, "BUG?");
    struct stat sb;
    if (0 == ::lstat(p.c_str(), &sb)) {
        return has_flag(sb, f);
    } else {
        prosoft::system::system_error(ec);
        return false;
    }
}
#endif

inline bool is_dotfile(const path& p) {
    const auto leaf = p.filename();
    return (!leaf.empty() && leaf.native()[0] == path::dot); // first condition should not be necessary, but just to be safe
}

bool is_mounttrigger(const path& p, error_code& ec) {
    ec.clear();
#if __APPLE__ && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7
    auto value = get_property<CFBooleanRef>(p, kCFURLIsMountTriggerKey, ec);
    return value && CFBooleanGetValue(value.get());
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

bool is_hidden(const path& p) {
    error_code ec;
    auto val = is_hidden(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not get hidden value", p, ec));
    return val;
}

bool is_hidden(const path& p, error_code& ec) {
    ec.clear();
#if _WIN32
    return ifilesystem::fattrs(p, FILE_ATTRIBUTE_HIDDEN, ec);
#else
    if (is_dotfile(p)) {
        return true;
    }
    
    // No need to take the CF overhead hit for Apple.
#if PS_FS_HAVE_BSD_STATFS
    return has_flag(p, UF_HIDDEN, ec);
#else
    // Linux has an Ext2 IOCTL that the lsattr/chattr commands uses; we should check it.
    return false;
#endif
    
#endif // _WIN32
}

bool is_mountpoint(const path& p) {
    error_code ec;
    auto val = is_mountpoint(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not get mount point", p, ec));
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
    PS_THROW_IF(ec.value(), filesystem_error("Could not get mount path", p, ec));
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
    // "/proc/mounts" represents the actual state of the system, while /etc/fstab is statically defined and may not have entries for automount filesystems.
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

bool is_package(const path& p) {
    error_code ec;
    auto val = is_package(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not get package value", p, ec));
    return val;
}

bool is_package(const path& p, error_code& ec) {
#if __APPLE__
    auto value = get_property<CFBooleanRef>(p, kCFURLIsPackageKey, ec);
    return value && CFBooleanGetValue(value.get());
#else
    (void)p;
    ec.clear(); // ENOTSUP?
    return false;
#endif
}

} // v1
} // filesystem
} // prosoft
