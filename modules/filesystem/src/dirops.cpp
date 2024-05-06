// Copyright © 2016-2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <prosoft/core/include/unique_resource.hpp>
#include <prosoft/core/include/string/platform_convert.hpp>

#include <prosoft/core/modules/filesystem/filesystem.hpp>
#include "dirops_internal.hpp"
#include "filesystem_private.hpp"

namespace prosoft {
namespace filesystem {
inline namespace v1 {

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

} // namespace v1
} // namespace filesystem
} // namespace prosoft

namespace {
using namespace prosoft;
using namespace prosoft::filesystem;

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

} // anon

namespace prosoft {
namespace filesystem {
inline namespace v1 {

bool create_directories(const path& p, perms perm) {
    error_code ec;
    const auto good = create_directories(p, perm, ec);
    PS_THROW_IF(!good, filesystem_error("Could not create directories", p, ec));
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
    PS_THROW_IF(!good, filesystem_error("Could not create directory", p, ec));
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
    PS_THROW_IF(!good, filesystem_error("Could not clone directory", p, cloneFrom, ec));
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

void create_symlink(const path& p, const path& link) {
    error_code ec;
    create_symlink(p, link, ec);
    PS_THROW_IF(ec.value() != 0, filesystem_error("Could not create symlink", p, link, ec));
}

void create_symlink(const path& p, const path& link, error_code& ec) {
#if !_WIN32
    if (0 == symlink(p.c_str(), link.c_str()))
#else
    static_assert(_WIN32_WINNT >= 0x0600, "Vista is required.");
    // Could stat p to see if it's a dir and set the flag for the caller, but the spec says to use create_directory_symlink
    if (CreateSymbolicLinkW(link.c_str(), p.c_str(), 0)) // Vista+ and SE_CREATE_SYMBOLIC_LINK_NAME right (Admin only by default)
#endif
    {
        ec.clear();
    } else {
        system::system_error(ec);
    }
}

#if _WIN32
void create_directory_symlink(const path& p, const path& link) {
    error_code ec;
    create_directory_symlink(p, link, ec);
    PS_THROW_IF(ec.value() != 0, filesystem_error("Could not create directory symlink", p, link, ec));
}

void create_directory_symlink(const path& p, const path& link, error_code& ec) {
    // p may not exist, so don't assert it's a dir
    static_assert(_WIN32_WINNT >= 0x0600, "Vista is required.");
    if (CreateSymbolicLinkW(link.c_str(), p.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY)) {
        ec.clear();
    } else {
        system::system_error(ec);
    }
}
#endif // WIN32

bool remove(const path& p) {
    error_code ec;
    const auto good = remove(p, ec);
    PS_THROW_IF(!good, filesystem_error("Could not remove path", p, ec));
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

void rename(const path& op, const path& np) {
    error_code ec;
    rename(op, np, ec);
    PS_THROW_IF(ec.value() != 0, filesystem_error("Could not rename path", op, np, ec));
}

void rename(const path& op, const path& np, error_code& ec) noexcept {
    ec.clear();
#if !_WIN32
    int err = ::rename(op.c_str(), np.c_str());
    if (-1 == err && EINTR == errno) {
        err = ::rename(op.c_str(), np.c_str());
    }
    if (0 != err) {
        system::system_error(ec);
    }
#else // !_WIN32
    if (!::MoveFileExW(op.c_str(), np.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        system::system_error(ec);
    }
#endif
}

path temp_directory_path() {
    error_code ec;
    auto p = temp_directory_path(ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not get temporary directory", p, ec));
    return p;
}

path temp_directory_path(error_code& ec) {
    ec.clear();

    path p;
#if !_WIN32
    const char* t = ::getenv(ifilesystem::TMPDIR);
#if __APPLE__
    std::unique_ptr<char[]> buf;
#endif
    if (!t || *t == 0) { // null can occur when run via sudo or anything else that sanitizes the environment
        constexpr const char* default_tmpdir = "/tmp";
        t = default_tmpdir;
#if __APPLE__
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
    PS_THROW_IF(ec.value(), filesystem_error("Could not get home directory", p, ec));
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
#ifdef __MINGW32__
    using folder_path_id = int;
#else
    using folder_path_id = REFKNOWNFOLDERID;
#endif
    static auto system_directory = [](folder_path_id t, error_code& ec) -> path {
#ifdef __MINGW32__
        // Mingw posix doesn't yet define SHGetKnownFolderPath()
        WCHAR buf[MAX_PATH+1];
        const auto err = ::SHGetFolderPathW(nullptr, t, nullptr, SHGFP_TYPE_CURRENT, buf);
#else
        windows::unique_taskmem<wchar_t> buf;
        const auto err = ::SHGetKnownFolderPath(t, 0, nullptr, handle(buf));
#endif
        if (S_OK == err) {
#ifdef __MINGW32__
            return path{buf};
#else
            return path{buf.get()};
#endif
        } else {
            ifilesystem::error(err, ec);
            return {};
        }
    };
#ifdef __MINGW32__
    p = system_directory(CSIDL_PROFILE, ec);
#else
    p = system_directory(FOLDERID_Profile, ec);
#endif
#endif
    
    assert_directory_exists(p, ec);
    return p;
}

path unused_drive() {
    error_code ec;
    auto p = unused_drive(ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not get unused drive", ec));
    return p;
}

path unused_drive(error_code& ec) {
#if _WIN32
    return ifilesystem::first_unused_drive_letter(ec);
#else
    ec.clear(); // ENOTSUP?
    return path{};
#endif
}

} // v1
} // filesystem
} // prosoft
