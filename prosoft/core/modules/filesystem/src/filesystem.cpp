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

#include <prosoft/core/config/config.h>

#if !_WIN32
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#include <cstring>

#include <unique_resource.hpp>

#include "filesystem.hpp"
#include "filesystem_private.hpp"

namespace {
using namespace prosoft::filesystem::v1;

#if !_WIN32

typedef struct ::stat stat_buf; // GCC complains with a 'using' directive here.

struct to_file_type {
    file_type operator()(const stat_buf& sb) const noexcept {
        switch ((sb.st_mode & S_IFMT)) {
            case S_IFBLK: return file_type::block;
            case S_IFCHR: return file_type::character;
            case S_IFDIR: return file_type::directory;
            case S_IFIFO: return file_type::fifo;
            case S_IFREG: return file_type::regular;
            case S_IFLNK: return file_type::symlink;
            case S_IFSOCK: return file_type::socket;
            default: PSASSERT_UNREACHABLE("WTF?"); return file_type::unknown;
        }
    };

    file_type operator()(const error_code& ec) const noexcept {
        switch (ec.value()) {
            case ENOENT: return file_type::not_found;
            case EPERM:
            case EACCES: return file_type::unknown;
            default: return file_type::none;
        };
    }
};

struct to_perms {
    perms operator()(const stat_buf& sb) const noexcept {
        return static_cast<perms>(sb.st_mode) & perms::mask;
    };
};

struct to_owner {
    owner operator()(const stat_buf& sb) const noexcept {
        return owner{
            access_control_identity{access_control_identity_type::user, sb.st_uid},
            access_control_identity{access_control_identity_type::group, sb.st_gid}};
    }
};

file_status file_stat(decltype(::stat) statcall, const path& p, error_code& ec) noexcept {
    stat_buf sb;
    if (0 == statcall(p.c_str(), &sb)) {
        ec.clear();
        return file_status{to_file_type{}(sb), to_perms{}(sb), to_owner{}(sb)};
    } else {
        ifilesystem::system_error(ec);
        return file_status{to_file_type{}(ec)};
    }
}

inline file_status file_stat(const path& p, error_code& ec) noexcept {
    return file_stat(::stat, p, ec);
}

inline file_status link_stat(const path& p, error_code& ec) noexcept {
    return file_stat(::lstat, p, ec);
}

#else

struct to_file_type {
    file_type operator()(const ifilesystem::native_path_type& p, DWORD attrs) const noexcept { // resolve reparse points
        auto ft = file_type::unknown; // default to unknown
        if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
            ::WIN32_FIND_DATAW data;
            if (::FindFirstFile(p.c_str(), &data)) {
                if (IO_REPARSE_TAG_SYMLINK == data.dwReserved0) {
                    ft = file_type::symlink;
                } else if (IO_REPARSE_TAG_MOUNT_POINT == data.dwReserved0) {
                    ft = file_type::directory;
                }
            }
        } else {
            ft = operator()(attrs);
        }
        return ft;
    }

    file_type operator()(DWORD attrs) const noexcept { // don't resolve reparse points
        auto ft = file_type::unknown; // default to unknown
        if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
            ft = file_type::directory; // backwards compat with pre-NT Win32 which did not have a notion of symlinks
        } else if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            ft = file_type::directory;
        } else if (attrs & FILE_ATTRIBUTE_DEVICE) {
            ft = file_type::character;
        } else {
            ft = file_type::regular;
        }
        return ft;
    }

    file_type operator()(const error_code& ec) const noexcept {
        switch (ec.value()) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND: return file_type::not_found;

            case ERROR_ACCESS_DENIED:
            case ERROR_SHARING_VIOLATION:
            case ERROR_TOO_MANY_OPEN_FILES:
            case ERROR_BAD_NETPATH:
#ifndef __MINGW32__
            case ERROR_TRANSACTIONAL_CONFLICT:
#endif
                return file_type::unknown;

            default: return file_type::none;
        }
    }
};

struct to_perms {
    perms user_perms(access_control_perms acp) {
        perms pp{};
        if (is_set(acp & access_control_perms::read_data)) { pp |= perms::owner_read; }
        if (is_set(acp & access_control_perms::write_data)) { pp |= perms::owner_write; }
        if (is_set(acp & access_control_perms::execute)) { pp |= perms::owner_exec; }
        return pp;
    }

    perms group_perms(access_control_perms acp) {
        perms pp{};
        if (is_set(acp & access_control_perms::read_data)) { pp |= perms::group_read; }
        if (is_set(acp & access_control_perms::write_data)) { pp |= perms::group_write; }
        if (is_set(acp & access_control_perms::execute)) { pp |= perms::group_exec; }
        return pp;
    }

    perms other_perms(access_control_perms acp) {
        perms pp{};
        if (is_set(acp & access_control_perms::read_data)) { pp |= perms::others_read; }
        if (is_set(acp & access_control_perms::write_data)) { pp |= perms::others_write; }
        if (is_set(acp & access_control_perms::execute)) { pp |= perms::others_exec; }
        return pp;
    }

    perms operator()(const path& p, owner& ident, error_code& ec) {
        perms pp = perms::unknown;
        const auto al = acl(p, ec);
        if (!al.empty()) {
            if (all_access() == al) {
                return perms::all;
            } else if (no_access() == al) {
                return perms::none;
            }

            auto o = ifilesystem::make_owner(p, ec);
            if (!ec) {
                pp = perms::none;
                const auto first = al.begin();
                const auto last = al.end();
                auto i = find(first, last, o.user());
                if (i != last) {
                    pp |= user_perms(i->perms());
                }

                i = find(first, last, o.group());
                if (i != last) {
                    pp |= group_perms(i->perms());
                }
                // if everyone is present it may define perms that the group does not
                i = find(first, last, access_control_identity::everyone_group());
                if (i != last) {
                    pp |= group_perms(i->perms());
                }

                PSASSERT(pp != perms::none, "WTF?");
                ident = std::move(o);
            }
        }
        return pp;
    }

private:
    template <class Iterator>
    Iterator find(Iterator first, Iterator last, const access_control_identity& ai) {
        return std::find_if(first, last, [&ai](const access_control_entry& ae) { return ae.identity() == ai; });
    }
};

file_status file_stat(const path& p, error_code& ec, bool link) noexcept {
    auto&& np = ifilesystem::to_native_path{}(p.native());
    const auto attrs = ::GetFileAttributesW(np.c_str());
    if (INVALID_FILE_ATTRIBUTES != attrs) {
        ec.clear();
        const auto get_type = to_file_type{};
        auto o = owner::invalid_owner();
        auto ap = to_perms{}(p, o, ec);
        return file_status{!link ? get_type(np, attrs) : get_type(attrs), ap, std::move(o)};
    } else {
        ifilesystem::system_error(ec);
        if (prosoft::starts_with(p.native(), ifilesystem::unc_prefix_device<path::string_type>())) {
            return file_status{to_file_type{}(FILE_ATTRIBUTE_DEVICE)}; // We know the type from the path, so return it.
        } else {
            return file_status{to_file_type{}(ec)};
        }
    }
}

inline file_status file_stat(const path& p, error_code& ec) noexcept {
    return file_stat(p, ec, false);
}

inline file_status link_stat(const path& p, error_code& ec) noexcept {
    return file_stat(p, ec, true);
}

#endif // !_WIN32

} // anonymous

namespace prosoft {
namespace filesystem {
inline namespace v1 {

file_status status(const path& p) {
    error_code ec;
    auto fs = status(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("file status failed", p, ec));
    return fs;
}

file_status status(const path& p, error_code& ec) noexcept {
    return file_stat(p, ec);
}

file_status symlink_status(const path& p) {
    error_code ec;
    auto fs = symlink_status(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("link status failed", p, ec));
    return fs;
}

file_status symlink_status(const path& p, error_code& ec) noexcept {
    return link_stat(p, ec);
}

} // v1
} // filesystem
} // prosoft
