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

#include <type_traits>

#if _WIN32
#include <system_error>

#include <prosoft/core/config/config.h>
#include <prosoft/core/config/config_windows.h>
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

#endif //WIN32

#endif // PS_WINUTILS_HPP
