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

#include <string>
#include <type_traits>
#include <unordered_map>

#if __APPLE__
#include <NSSystemDirectories.h>
#endif

#if _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#include "filesystem.hpp"
#include "filesystem_private.hpp"

// C++11 std did not support hash<> with enum classes. This was considered a defect and resolved in C++14.
// GCC however has not implemented support for this in either the 4.9.x series or the latest 5.x series.
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=60970
#if !PS_CPP14_STDLIB || (!__clang__ && __GNUC__)
namespace std {
template <>
struct hash<prosoft::filesystem::standard_directory> {
    size_t operator() (prosoft::filesystem::standard_directory t) const { return static_cast<size_t>(t); }
};
} // std
#endif

namespace {
using namespace prosoft;
using namespace prosoft::filesystem;

constexpr domain get_domain(domain d) noexcept {
#if _WIN32
    return d;
#else
    return d != domain::user_local ? d : domain::user;
#endif
}

#if __APPLE__
NSSearchPathDomainMask get_domainmask(domain d) {
    switch(d) {
        case domain::user:
        case domain::user_local:
            return NSUserDomainMask;
        case domain::shared:
            return NSLocalDomainMask;
    };
}
#endif

std::string tostring(domain d) {
    switch (get_domain(d)) {
        case domain::user:
            return "user";
        break;
        case domain::user_local:
            return "user local";
        break;
        case domain::shared:
            return "shared";
        break;
#if !__clang__
        default:
        break;
#endif
    }
    return "invalid filesystem search domain";
}

std::string tostring(standard_directory sd) {
    switch (sd) {
        case standard_directory::app_data:
            return "appdata";
        break;
        case standard_directory::cache:
            return "cache";
        break;
#if !__clang__
        default:
        break;
#endif
    };
    return "invalid filesystem search directory";
}

std::string tostring(domain d, standard_directory sd) {
    return tostring(d) + "/" + tostring(sd);
}

} // anon

namespace {
template <typename NativeSearchType>
NativeSearchType search(domain, standard_directory sd, error_code& ec) {
    static const std::unordered_map<standard_directory, NativeSearchType> sdmap {
#if __APPLE__
        {standard_directory::app_data, NSApplicationSupportDirectory},
        {standard_directory::cache, NSCachesDirectory}
#elif _WIN32
#ifdef __MINGW32__
        {standard_directory::app_data, CSIDL_APPDATA},
        {standard_directory::cache, CSIDL_LOCAL_APPDATA}
#else
        {standard_directory::app_data, FOLDERID_RoamingAppData},
        {standard_directory::cache, FOLDERID_LocalAppData}
#endif
#else
        {standard_directory::app_data, "~/.config"},
        {standard_directory::cache, "~/.cache"}
#endif
    };
    const auto i = sdmap.find(sd);
    if (i != sdmap.end()) {
        return i->second;
    } else {
        ifilesystem::error(
#if !_WIN32
            EINVAL,
#else
            ERROR_FILE_NOT_FOUND,
#endif
            ec);
        static typename std::remove_reference<NativeSearchType>::type const invalidType{};
        return invalidType;
    }
}

typedef
#if __APPLE__
    NSSearchPathDirectory
#elif _WIN32
#ifdef __MINGW32__
    int
#else
    REFKNOWNFOLDERID
#endif
#else
    const char*
#endif
    native_search_type;
    
#if !_WIN32
void create(const path& p, domain d, standard_directory_options sdo, error_code& ec) {
    if (is_set(sdo & standard_directory_options::create)) {
        (void)create_directory(p, (d != domain::shared ? perms::owner_all : perms::all|perms::sticky_bit), ec);
    }
}
#endif

path post_process(const path::encoding_value_type* buf, domain d, standard_directory_options sdo, error_code& ec) {
    PSASSERT_NOTNULL(buf);
#if !_WIN32
    path p { *buf == '/' ? path{buf} : canonical(buf, ec) };
    create(p, d, sdo, ec);
    return !ec.value() ? p : path{};
#else
    // No post-processing is necessary assuming paths were obtained via SHGetKnownFolderPath.
    (void)d;
    (void)sdo;
    (void)ec;
    return path{buf};
#endif
};
} // anon

namespace prosoft {
namespace filesystem {
inline namespace v1 {

path standard_directory_path(domain d, standard_directory sd, standard_directory_options sdo) {
    error_code ec;
    auto p = standard_directory_path(d, sd, sdo, ec);
    PS_THROW_IF(ec.value(), filesystem_error("standard dir search failed", path{tostring(d,sd)}, ec));
    return p;
}

path standard_directory_path(domain d, standard_directory sd, standard_directory_options sdo, error_code& ec) {
    ec.clear();
    const auto nsd = search<native_search_type>(d, sd, ec);
    if (ec.value()) {
        return {};
    }
    
#if __APPLE__
    char buf[PATH_MAX];
    auto state = ::NSStartSearchPathEnumeration(nsd, get_domainmask(d));
    if (::NSGetNextSearchPathEnumeration(state, buf)) {
        return post_process(buf, d, sdo, ec);
    } else {
        ifilesystem::error(ENOENT, ec);
    }
    return {};
#elif _WIN32
#ifdef __MINGW32__
    // Mingw posix doesn't yet define SHGetKnownFolderPath()
    WCHAR buf[MAX_PATH+1];
    const auto err = ::SHGetFolderPathW(nullptr, nsd, nullptr, SHGFP_TYPE_CURRENT, buf);
#else
    windows::unique_taskmem<wchar_t> buf;
    const auto err = ::SHGetKnownFolderPath(nsd, 0, nullptr, handle(buf));
#endif
    if (S_OK == err) {
#ifdef __MINGW32__
        return post_process(buf, d, sdo, ec);
#else
        return post_process(buf.get(), d, sdo, ec);
#endif
    } else {
        ifilesystem::error(err, ec);
    }
    return {};
#else
    return post_process(nsd, d, sdo, ec);
#endif
}

} // v1
} // filesystem
} // prosoft
