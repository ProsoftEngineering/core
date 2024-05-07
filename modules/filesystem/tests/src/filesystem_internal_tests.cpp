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

#include <filesystem_internal.hpp>

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
