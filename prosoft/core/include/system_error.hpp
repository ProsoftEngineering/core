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

#ifndef PS_CORE_SYSTEM_ERROR_HPP
#define PS_CORE_SYSTEM_ERROR_HPP

#include <prosoft/core/config/config_platform.h>

#include <system_error>

#if _WIN32
#include <windows.h>
#endif

namespace prosoft {
namespace system {

inline const std::error_category& error_category() noexcept {
    return std::system_category();
}

inline const std::error_category& posix_category() noexcept {
#if !_WIN32
    return error_category();
#else
    return std::generic_category();
#endif
}

using error_code = std::error_code;

inline void system_error(error_code& ec) noexcept {
// Using a temp var assures the global error is not perturbed by call optimizations that may clear its state.
#if !_WIN32
    const auto e = errno;
#else
    const auto e = ::GetLastError();
#endif
    ec.assign(e, error_category());
}

inline error_code system_error() noexcept {
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
    return std::system_error{ec, msg}; // can throw
}

inline void posix_error(error_code& ec) noexcept {
#if !_WIN32
    return system_error(ec);
#else
    const auto e = errno;
    ec.assign(e, posix_category());
#endif
}

inline error_code posix_error() noexcept {
#if !_WIN32
    return system_error();
#else
    const auto e = errno;
    return error_code{e, posix_category()};
#endif
}

inline std::system_error posix_error(const char* msg) {
#if !_WIN32
    return system_error(msg);
#else
    auto ec = posix_error();
    return std::system_error{ec, msg}; // can throw
#endif
}

} // system
} // prosoft

#endif // PS_CORE_SYSTEM_ERROR_HPP
