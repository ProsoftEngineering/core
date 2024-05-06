// Copyright © 2015-2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#if _WIN32
#include <array>
#endif

#include <cstring>

#include <prosoft/core/include/unique_resource.hpp>

#include <prosoft/core/modules/filesystem/filesystem.hpp>
#include "filesystem_internal.hpp"
#include "filesystem_private.hpp"

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
    return to_times{}.from(ft);
}

std::wstring first_unused_drive_letter(DWORD bits) {
    constexpr std::array<wchar_t*, 26> letters {
        L"A:\\",
        L"B:\\",
        L"C:\\",
        L"D:\\",
        L"E:\\",
        L"F:\\",
        L"G:\\",
        L"H:\\",
        L"I:\\",
        L"J:\\",
        L"K:\\",
        L"L:\\",
        L"M:\\",
        L"N:\\",
        L"O:\\",
        L"P:\\",
        L"Q:\\",
        L"R:\\",
        L"S:\\",
        L"T:\\",
        L"U:\\",
        L"V:\\",
        L"W:\\",
        L"X:\\",
        L"Y:\\",
        L"Z:\\",
    };
    for (decltype(letters.size()) i = 3; i < letters.size(); ++i) { // Start at D:
        if (0 == (bits & (1<<i))) { 
            return std::wstring{letters[i]};
        }
    }
    return L"";
}

std::wstring first_unused_drive_letter(error_code& ec) {
    if (const auto bits = GetLogicalDrives()) {
        ec.clear();
        return first_unused_drive_letter(bits);
    } else {
        system::system_error(ec);
        return L"";
    }
}

} // ifilesystem
#endif // _WIN32

} // v1
} // filesystem
} // prosoft

#if PSTEST_HARNESS
// Internal tests.
#include <catch2/catch_test_macros.hpp>

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
