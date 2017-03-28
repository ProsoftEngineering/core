// Copyright © 2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_WINUTILS_HPP
#define PS_WINUTILS_HPP

#include <prosoft/core/config/config_platform.h>

#if _WIN32
#include <iosfwd>
#include <iomanip>
#include <cstring>
#include <sstream>
#include <system_error>
#include <type_traits>

#include <prosoft/core/config/config.h>
#include <prosoft/core/include/stream_utils.hpp>

#include <windows.h>

namespace prosoft {
namespace windows {

class com_init {
    const HRESULT m_result;

    bool success() const noexcept {
        return  SUCCEEDED(m_result);
    }
public:
    com_init(std::error_code& ec) noexcept(std::is_nothrow_constructible<std::error_code, int, decltype(std::system_category())>::value)
        : m_result(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {
        if (success()) {
            ec.clear();
        } else {
            ec = std::error_code{m_result, std::system_category()};
        }
    }

    ~com_init() {
        if (success()) {
            CoUninitialize();
        }
    }

    PS_DISABLE_COPY(com_init);
    PS_DISABLE_MOVE(com_init);
};

inline bool check(HRESULT result, std::error_code& ec, const std::error_category& cat = std::system_category()) {
    if (SUCCEEDED(result)) {
        return true;
    } else {
        ec = std::error_code{result, cat};
        return false;
    }
}

inline bool ok(HRESULT result, std::error_code& ec, const std::error_category& cat = std::system_category()) {
    if (S_OK == result) {
        return true;
    } else {
        ec = std::error_code{result, cat};
        return false;
    }
}

} // windows
} // prosoft

template <class Char, class Traits>
std::basic_ostream<Char, Traits>& operator<<(std::basic_ostream<Char, Traits>& os, const GUID& g) {
    static const std::basic_string<Char> dash(1, static_cast<Char>('-'));
    prosoft::stream_guard ss{os};
    return os << std::setfill(static_cast<Char>('0')) << std::hex
    // each non-int operator<< resets width to 0
        << std::setw(2) << g.Data1 << dash
        << std::setw(2) << g.Data2 << dash
        << std::setw(2) << g.Data3 << dash
        << std::setw(2) << uint16_t(g.Data4[0]) << uint16_t(g.Data4[1]) << dash
        << std::setw(2) << uint16_t(g.Data4[2]) << uint16_t(g.Data4[3]) << uint16_t(g.Data4[4])
        << uint16_t(g.Data4[5]) << uint16_t(g.Data4[6]) << uint16_t(g.Data4[7]);
}

namespace prosoft {
namespace windows {

enum class guid_string_opts {
    none,
    brace,
    uppercase,
};
PS_ENUM_BITMASK_OPS(guid_string_opts);

template <class Char>
std::basic_string<Char> guid_to_string(const GUID& g, guid_string_opts opts = guid_string_opts::none) {
    std::basic_stringstream<Char> ss;
    if (is_set(opts & guid_string_opts::uppercase)) {
        ss << std::uppercase;
    }

    const bool brace = is_set(opts & guid_string_opts::brace);
    if (brace) {
        ss << static_cast<Char>('{');
    }
    ss << g;
    if (brace) {
        ss << static_cast<Char>('}');
    }
    return ss.str();
}

inline auto guid_string(const GUID& g, guid_string_opts opts = guid_string_opts::none) {
    return guid_to_string<char>(g, opts);
}

inline auto guid_wstring(const GUID& g, guid_string_opts opts = guid_string_opts::none) {
    return guid_to_string<wchar_t>(g, opts);
}

inline auto iid_string(const GUID& g, guid_string_opts opts = guid_string_opts::none) {
    return guid_to_string<char>(g, guid_string_opts::brace|opts);
}

inline auto iid_wstring(const GUID& g, guid_string_opts opts = guid_string_opts::none) {
    return guid_to_string<wchar_t>(g, guid_string_opts::brace|opts);
}

} // windows
} // prosoft

#endif //WIN32

#endif // PS_WINUTILS_HPP
