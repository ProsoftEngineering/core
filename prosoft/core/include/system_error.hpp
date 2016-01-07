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

#ifndef PS_CORE_SYSTEM_ERROR_HPP
#define PS_CORE_SYSTEM_ERROR_HPP

#include <system_error>

#include <prosoft/core/config/config.h>

#if _WIN32
#include <prosoft/core/config/config_windows.h>
#include <windows.h>
#endif

namespace prosoft {
namespace system {

inline const std::error_category& error_category() {
    return std::system_category();
}

using error_code = std::error_code;

inline void system_error(error_code& ec) {
// Using a temp var assures the global error is not perturbed by call optimizations that may clear its state.
#if !_WIN32
    const auto e = errno;
#else
    const auto e = ::GetLastError();
#endif
    ec.assign(e, error_category());
}

inline error_code system_error() {
// Ditto on the temp var.
#if !_WIN32
    const auto e = errno;
#else
    const int e = ::GetLastError();
#endif
    return error_code{e, error_category()};
}

// This takes a raw string because std::string may alloc mem and clear the global error state.
inline std::system_error system_error(const char* msg) {
    auto ec = system_error(); // Ditto on temp var.
    return std::system_error{ec, msg};
}
} // system
} // prosoft

#endif // PS_CORE_SYSTEM_ERROR_HPP
