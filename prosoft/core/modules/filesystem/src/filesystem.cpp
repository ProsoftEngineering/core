// Copyright © 2015-2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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
#include <sys/time.h>
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

struct to_times {
    ::timespec to_timespec(const file_time_type& t) const {
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch());
        return ::timespec{
            .tv_sec = ns.count() / std::chrono::nanoseconds::period::den,
            .tv_nsec = ns.count() % std::chrono::nanoseconds::period::den,
        };
    }
    
    // XXX: timeval is platform specific (nsec on BSD and usec on Linux)
    ::timeval to_timeval(const file_time_type& t) const {
        ::timeval tv;
        const auto ts = to_timespec(t);
        TIMESPEC_TO_TIMEVAL(&tv, &ts);
        return tv;
    }
    
#if PS_FS_HAVE_BSD_STATFS
    #define PS_ST_MTIME st_mtimespec
    #define PS_ST_CTIME st_ctimespec
    #define PS_ST_ATIME st_atimespec
#if defined(st_birthtime)
    #define PS_ST_BTIME st_birthtimespec
#endif
    file_time_type from(const struct timespec& ts) const {
        using namespace std::chrono;
        using fduration = typename file_time_type::duration;
        return file_time_type{ duration_cast<fduration>(seconds{ts.tv_sec}) + duration_cast<fduration>(nanoseconds{ts.tv_nsec}) };
    }
#else
    #define PS_ST_MTIME st_mtime
    #define PS_ST_CTIME st_ctime
    #define PS_ST_ATIME st_atime
    file_time_type from(::time_t t) const {
        return file_time_type::clock::from_time_t(t);
    }
#endif
    
    times operator()(const stat_buf& sb) const {
        times t;
        t.modified(from(sb.PS_ST_MTIME));
        t.metadata_modified(from(sb.PS_ST_CTIME));
        t.accessed(from(sb.PS_ST_ATIME));
#if defined(PS_ST_BTIME)
        t.created(from(sb.PS_ST_BTIME));
#endif
        return t;
    }
    
    #undef PS_ST_MTIME
    #undef PS_ST_CTIME
    #undef PS_ST_ATIME
#if defined(PS_ST_BTIME)
    #undef PS_ST_BTIME
#endif
};

file_status file_stat(decltype(::stat) statcall, const path& p, status_info what, error_code& ec) {
    stat_buf sb;
    if (0 == statcall(p.c_str(), &sb)) {
        ec.clear();
        // Not a big type conversion cost, so just do minimal/complete
        if (status_info::basic == what) {
            return file_status{to_file_type{}(sb)};
        } else {
            static_assert(sizeof(file_size_type) >= sizeof(sb.st_size), "Broken assumption");
            return file_status{to_file_type{}(sb), to_perms{}(sb), file_size_type(sb.st_size), to_owner{}(sb), to_times{}(sb)};
        }
    } else {
        ifilesystem::system_error(ec);
        return file_status{to_file_type{}(ec)};
    }
}

inline file_status file_stat(const path& p, status_info what, error_code& ec) {
    return file_stat(::stat, p, what, ec);
}

inline file_status link_stat(const path& p, status_info what, error_code& ec) {
    return file_stat(::lstat, p, what, ec);
}

#else

inline bool is_device_path(const path& p) {
    return prosoft::starts_with(p.native(), ifilesystem::unc_prefix_device<path::string_type>());
}

struct to_file_type {
    file_type operator()(const ifilesystem::native_path_type& p, DWORD attrs, bool link) const noexcept { // resolve reparse points
        auto ft = file_type::unknown; // default to unknown
        if (link && (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
            ::WIN32_FIND_DATAW data;
            auto h = ::FindFirstFile(p.c_str(), &data);
            if (INVALID_HANDLE_VALUE != h) {
                if (IO_REPARSE_TAG_SYMLINK == data.dwReserved0) {
                    ft = file_type::symlink;
                } else if (IO_REPARSE_TAG_MOUNT_POINT == data.dwReserved0) {
                    ft = file_type::directory;
                }
                ::FindClose(h);
            }
        } else if (is_device_path(p)) {
            ft = file_type::character;
        } else {
            ft = operator()(attrs);
        }
        return ft;
    }

    file_type operator()(DWORD attrs) const noexcept { // don't resolve reparse points
        auto ft = file_type::unknown; // default to unknown
        if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
            ft = file_type::directory; // backwards compat with pre-NT Win32 which did not have a notion of symlinks
        } else if ((attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            ft = file_type::directory;
        } else if ((attrs & FILE_ATTRIBUTE_DEVICE)) {
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
                // XXX: if an identity exists more than once in the ACL, find only returns the first instance.
                auto i = find(first, last, o.user());
                if (i != last) {
                    pp |= user_perms(i->perms());
                }

                i = find(first, last, o.group());
                if (i != last) {
                    pp |= group_perms(i->perms());
                }
                
                // if we are not the owner check our membership in groups for "other" perms
                if (o.user() != access_control_identity::effective_user()) {
                    for (const auto& ae : al) {
                        std::error_code ec;
                        if (is_member(ae.identity(), ec)) {
                            pp |= other_perms(ae.perms());
                        }
                    }
                }

                if (perms::none == pp) {
                    // owner and group were not found in the ACL. This can happen with devices or other system files.
                    // (e.g. C: is owned by TrustedInstaller, but that ID may not have an explicit ACE set).
                    pp = perms::owner_read; // guess
                }

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

#ifndef __MINGW32__
// MSVC: std:: clock wraps GetSystemTimeAsFileTime, so ticks are 100ns intervals.
static_assert(file_time_type::clock::period::den == 10000000LL, "Broken assumption");
using clock_tick = file_time_type::clock::duration;
#else
static_assert(file_time_type::clock::period::den != 10000000LL, "Broken assumption");
using clock_tick = std::chrono::duration<long long, std::ratio<1LL, 10000000LL>>;
#endif
constexpr clock_tick filetime_epoch_to_utc_offset{116444736000000000LL};

inline FILETIME to_filetime(const clock_tick& t) {
    return FILETIME{static_cast<DWORD>(t.count()), static_cast<DWORD>(t.count()>>32)};
}

inline clock_tick to_clocktick(const FILETIME& ft) {
    return clock_tick{ static_cast<long long>(ft.dwLowDateTime) | (static_cast<long long>(ft.dwHighDateTime)<<32) };
}

inline file_time_type::clock::duration to_utc(const FILETIME& ft) {
    return
#if __MINGW__
    std::chrono::duration_cast<file_time_type::clock::duration>(
#endif
    to_clocktick(ft) - filetime_epoch_to_utc_offset
#if __MINGW__
    )
#endif
    ;
}

inline FILETIME from_utc(const clock_tick& t) {
    return to_filetime(t + filetime_epoch_to_utc_offset);
}

#ifdef __MINGW32__
inline FILETIME from_utc(const file_time_type::clock::duration& t) {
    return to_filetime(std::chrono::duration_cast<clock_tick>(t) + filetime_epoch_to_utc_offset);
}
#endif

struct to_times {
    file_time_type from(const ::FILETIME& ft) {
        if ((ft.dwLowDateTime > 0 || ft.dwHighDateTime > 0)) { // Assuming FILETIME epoch.
            return file_time_type{to_utc(ft)};
        } else {
            return times::make_invalid();
        }
    }

    ::FILETIME to_filetime(const file_time_type& t) {
        return from_utc(t.time_since_epoch());
    }
    
    times operator()(const ::BY_HANDLE_FILE_INFORMATION& info) {
        times t;
        t.modified(from(info.ftLastWriteTime));
        t.metadata_modified(t.modified());
        t.accessed(from(info.ftLastAccessTime));
        t.created(from(info.ftCreationTime));
        return t;
    }
    
    times operator()(const path& p, error_code& ec) {
        ::BY_HANDLE_FILE_INFORMATION info;
        if (ifilesystem::finfo(p, &info, ec)) {
            return operator()(info);
        }
        return times{};
    }
};

file_size_type to_size(const ::BY_HANDLE_FILE_INFORMATION& info) {
    return file_size_type(info.nFileSizeLow) | (file_size_type(info.nFileSizeHigh) >> 32);
}

file_status file_stat(const path& p, status_info what, error_code& ec, bool link) noexcept {
    if (const auto attrs = ifilesystem::fattrs(p, ec)) {
        ec.clear();
        error_code lec; // ignored
        const auto get_type = to_file_type{};
        auto o = owner::invalid_owner();
        perms ap = perms::unknown;
        if (is_set(what & status_info::perms)) {
            ap = to_perms{}(p, o, lec);
        }
        auto&& np = ifilesystem::to_native_path{}(p.native());
        times t;
        file_size_type sz{};
        if (is_set(what & (status_info::times|status_info::size)) {
            ::BY_HANDLE_FILE_INFORMATION info;
            if (ifilesystem::finfo(np, &info, lec)) {
                t = to_times{}(info);
                if (get_type(attrs) == file_type::regular) {
                    sz = to_size(&info);
                }
            }
        }
        return file_status{get_type(np, attrs, link), ap, sz, std::move(o), t};
    } else {
        if (is_device_path(p)) {
            return file_status{to_file_type{}(FILE_ATTRIBUTE_DEVICE)}; // We know the type from the path, so return it.
        } else {
            return file_status{to_file_type{}(ec)};
        }
    }
}

inline file_status file_stat(const path& p, status_info what, error_code& ec) noexcept {
    return file_stat(p, what, ec, false);
}

inline file_status link_stat(const path& p, status_info what, error_code& ec) noexcept {
    return file_stat(p, what, ec, true);
}

#endif // !_WIN32

} // anonymous

namespace prosoft {
namespace filesystem {
inline namespace v1 {

constexpr file_time_type times::m_invalidTime;

bool equivalent(const path& p1, const path& p2) {
    error_code ec;
    auto val = equivalent(p1, p2, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not determine file equivalence", p1, p2, ec));
    return val;
}

bool equivalent(const path& p1, const path& p2, error_code& ec) noexcept {
    ec.clear();
#if !_WIN32
    struct stat sb;
    sb.st_ino = 0;
    sb.st_dev = 0;
    if (0 != ::stat(p1.c_str(), &sb)) {
        ifilesystem::system_error(ec);
        return false;
    }
    
    const auto ino = sb.st_ino;
    const auto dev = sb.st_dev;
    
    sb.st_ino = 0;
    sb.st_dev = 0;
    if (0 != ::stat(p2.c_str(), &sb)) {
        ifilesystem::system_error(ec);
        return false;
    }
    
    return (dev == sb.st_dev && ino == sb.st_ino);
#else
    ::BY_HANDLE_FILE_INFORMATION info;
    if (!ifilesystem::finfo(p1, &info, ec)) {
        return false;
    }

    const uint64_t ino = uint64_t(info.nFileIndexLow) |  (uint64_t(info.nFileIndexHigh) << 32ULL);
    const auto vsn = info.dwVolumeSerialNumber;

    info.nFileIndexLow = info.nFileIndexHigh = 0;
    info.dwVolumeSerialNumber = 0;
    if (!ifilesystem::finfo(p2, &info, ec)) {
        return false;
    }

    const uint64_t ino2 = uint64_t(info.nFileIndexLow) |  (uint64_t(info.nFileIndexHigh) << 32ULL);
    return (vsn == info.dwVolumeSerialNumber && ino == ino2);
#endif
}

file_status status(const path& p, status_info what) {
    error_code ec;
    auto fs = status(p, what, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not get status", p, ec));
    return fs;
}

file_status status(const path& p, status_info what, error_code& ec) noexcept {
    return file_stat(p, what, ec);
}

file_status symlink_status(const path& p, status_info what) {
    error_code ec;
    auto fs = symlink_status(p, what, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not get symlink status", p, ec));
    return fs;
}

file_status symlink_status(const path& p, status_info what, error_code& ec) noexcept {
    return link_stat(p, what, ec);
}

bool exists(const path& p) {
    error_code ec;
    auto fs = exists(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not get status", p, ec));
    return fs;
}

bool exists(const path& p, error_code& ec) noexcept {
#if !_WIN32
    struct ::stat sb;
    const int err = ::stat(p.c_str(), &sb);
    if (err == 0) {
        return true;
    } else {
        ifilesystem::system_error(ec);
        return ec.value() != ENOENT;
    }
#else
    if (const auto attrs = ifilesystem::fattrs(p, ec)) {
        ec.clear();
        return true;
    } else {
        return to_file_type{}(ec) != file_type::not_found;
    }
#endif
}

void last_write_time(const path& p, file_time_type t) {
    error_code ec;
    last_write_time(p, t, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not set write time", p, ec));
}

void last_write_time(const path& p, file_time_type t, error_code& ec) noexcept {
#if !_WIN32
    struct stat sb;
    if (0 == ::stat(p.c_str(), &sb)) {
        ::timeval utvals[2];
        utvals[1] = to_times{}.to_timeval(t);
        #if PS_FS_HAVE_BSD_STATFS
        TIMESPEC_TO_TIMEVAL(&utvals[0], &sb.st_atimespec);
        #else
        utvals[0].tv_sec = sb.st_atime;
        utvals[0].tv_usec = 0;
        #endif
        
        if (0 != ::utimes(p.c_str(), utvals)) {
            ec = system::system_error();
        }
    } else {
        ec = system::system_error();
    }
#else
    if (auto h = ifilesystem::open(p, FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, ec)) {
        const auto ft = to_times{}.to_filetime(t);
        if (SetFileTime(h.get(), nullptr, nullptr, &ft)) {
            ec.clear();
        } else {
            system::system_error(ec);
        }
    } else {
        system::system_error(ec);
    }
#endif
}

#if _WIN32
namespace ifilesystem { // private API

windows::Handle open(const path& p, DWORD accessMode, DWORD shareMode, DWORD createMode, DWORD flags, error_code& ec) {
    if (fattrs(p, FILE_ATTRIBUTE_DIRECTORY)) {
        flags |= FILE_FLAG_BACKUP_SEMANTICS; // Required to open a dir handle. Can also bypass security restrictions.
        flags &= ~FILE_ATTRIBUTE_NORMAL;
        // http://stackoverflow.com/questions/10198420/open-directory-using-createfile
    }
    if (ec.value()) {
        return {};
    }
    auto h = ::CreateFileW(p.c_str(), accessMode, shareMode, nullptr, createMode, flags, nullptr);
    if (!h) {
        system_error(ec);
    }
    return {h};
}

bool finfo(const path& p, ::BY_HANDLE_FILE_INFORMATION* info, error_code& ec) {
    if (auto h = open(p, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, ec)) {
        if (::GetFileInformationByHandle(h.get(), info)) {
            return true;
        } else {
            system_error(ec); // fall through to false
        }
    }
    return false;
}

file_time_type to_filetime(const ::FILETIME& ft) {
    return to_times{}(ft);
}

} // ifilesystem
#endif // _WIN32

} // v1
} // filesystem
} // prosoft

#if PSTEST_HARNESS
// Internal tests.
#include "catch.hpp"

TEST_CASE("filesystem_internal") {
    using namespace prosoft::filesystem;
    
    SECTION("file type") {
        auto tft = to_file_type{};
#if !_WIN32
        struct stat sb;
        sb.st_mode = S_IFBLK;
        CHECK(tft(sb) == file_type::block);
        sb.st_mode = S_IFCHR;
        CHECK(tft(sb) == file_type::character);
        sb.st_mode = S_IFDIR;
        CHECK(tft(sb) == file_type::directory);
        sb.st_mode = S_IFIFO;
        CHECK(tft(sb) == file_type::fifo);
        sb.st_mode = S_IFREG;
        CHECK(tft(sb) == file_type::regular);
        sb.st_mode = S_IFLNK;
        CHECK(tft(sb) == file_type::symlink);
        sb.st_mode = S_IFSOCK;
        CHECK(tft(sb) == file_type::socket);

        CHECK(tft(error_code{ENOENT, filesystem_category()}) == file_type::not_found);
        CHECK(tft(error_code{EPERM, filesystem_category()}) == file_type::unknown);
        CHECK(tft(error_code{EACCES, filesystem_category()}) == file_type::unknown);
        CHECK(tft(error_code{EBUSY, filesystem_category()}) == file_type::none);
#else
        CHECK(tft(FILE_ATTRIBUTE_REPARSE_POINT) == file_type::directory);
        CHECK(tft(FILE_ATTRIBUTE_DEVICE) == file_type::character);
    
        CHECK(tft(error_code{ERROR_FILE_NOT_FOUND, filesystem_category()}) == file_type::not_found);
        CHECK(tft(error_code{ERROR_PATH_NOT_FOUND, filesystem_category()}) == file_type::not_found);
        CHECK(tft(error_code{ERROR_ACCESS_DENIED, filesystem_category()}) == file_type::unknown);
        CHECK(tft(error_code{ERROR_SHARING_VIOLATION, filesystem_category()}) == file_type::unknown);
        CHECK(tft(error_code{ERROR_TOO_MANY_OPEN_FILES, filesystem_category()}) == file_type::unknown);
        CHECK(tft(error_code{ERROR_BAD_NETPATH, filesystem_category()}) == file_type::unknown);
        CHECK(tft(error_code{ERROR_INVALID_HANDLE, filesystem_category()}) == file_type::none);
#endif
    }
    
    SECTION("time conversion") {
#if !_WIN32
    auto val = to_times{}.to_timespec(file_time_type{});
    CHECK(val.tv_sec == 0);
    CHECK(val.tv_nsec == 0);
    
    using namespace std::chrono;
    val = to_times{}.to_timespec(file_time_type{duration_cast<file_time_type::duration>(seconds(1))});
    CHECK(val.tv_sec == 1);
    CHECK(val.tv_nsec == 0);
    
    val = to_times{}.to_timespec(file_time_type{duration_cast<file_time_type::duration>(milliseconds(10))});
    CHECK(val.tv_sec == 0);
    CHECK(val.tv_nsec == 10000000);
#else
    constexpr auto offset_low = (DWORD)filetime_epoch_to_utc_offset.count();
    constexpr auto offset_high = (DWORD)(filetime_epoch_to_utc_offset.count()>>32);
    
    using ft_duration = file_time_type::clock::duration;
    
    auto ft = to_times{}.to_filetime(file_time_type{});
    CHECK(ft.dwLowDateTime == offset_low);
    CHECK(ft.dwHighDateTime == offset_high);

    auto utc = to_times{}.from(ft);
    CHECK(utc.time_since_epoch() == ft_duration());

    using namespace std::chrono;
    ft = to_times{}.to_filetime(file_time_type{duration_cast<file_time_type::duration>(seconds(1))});
    CHECK(ft.dwLowDateTime == offset_low + duration_cast<clock_tick>(seconds(1)).count());
    CHECK(ft.dwHighDateTime == offset_high);

    utc = to_times{}.from(ft);
    CHECK(utc.time_since_epoch() == duration_cast<ft_duration>(seconds(1)));
    
    ft = to_times{}.to_filetime(file_time_type{duration_cast<file_time_type::duration>(milliseconds(10))});
    CHECK(ft.dwLowDateTime == offset_low + duration_cast<clock_tick>(milliseconds(10)).count());
    CHECK(ft.dwHighDateTime == offset_high);

    utc = to_times{}.from(ft);
    CHECK(utc.time_since_epoch() == duration_cast<ft_duration>(milliseconds(10)));
#endif
    }
}
#endif // PSTEST_HARNESS
