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

#ifndef PS_CORE_PATH_UTILS_HPP
#define PS_CORE_PATH_UTILS_HPP

#include <array>

#include <prosoft/core/include/string/string_component.hpp>

namespace prosoft {
namespace filesystem {
inline namespace v1 {

enum class path_style {
    native,
    posix,
    windows,
};

namespace ifilesystem {

constexpr inline path_style native_style(path_style sty = path_style::native) {
    return sty == path_style::native ?
#if _WIN32
        path_style::windows
#else
        path_style::posix
#endif
    : sty;
}

template <class T>
const T& posix_separator() {
    static const T s{static_cast<typename T::value_type>('/')};
    return s;
}

template <class T>
const T& windows_separator() {
    static const T s{static_cast<typename T::value_type>('\\')};
    return s;
}

template <typename T>
const T& delimiter_for_style(path_style& sty) // XXX: sty is changed for clients if == style::native
{
    if ((sty = native_style(sty)) == path_style::windows) {
        return windows_separator<T>();
    } else {
        return posix_separator<T>();
    }
}

// convert_to_native_delimiter: Windows supports mixing of forward and backward slashes
// in its paths, but we only support backward slashes to simplify the code.
template <typename T>
void convert_to_native_delimiter(T& path, path_style sty) {
    if (native_style(sty) == path_style::windows) {
        replace_all(path, posix_separator<T>(), windows_separator<T>());
    }
}

// collapse_delimiters: replace multiple consecutive delimiters (////) with a single one (/).
template <typename T>
void collapse_delimiters(T& path, path_style sty) {
    auto&& del1 = delimiter_for_style<T>(sty);
    const T del2 = del1 + del1;
    // start at 3rd character for Windows paths since server/UNC paths can start with 2 slashes
    while (replace_all(path, del2, del1, (sty == path_style::windows ? 2 : 0))) {}
}

template <typename T>
const T& unc_prefix() {
    static const std::array<typename T::value_type, 2> a{{static_cast<typename T::value_type>('\\'), static_cast<typename T::value_type>('\\')}};
    static const T s{a.data(), a.size()};
    return s;
}

template <typename T>
const T& unc_prefix_raw() { // disables all Win32 path parsing and more importantly, removes MAX_PATH restriction
    static const std::array<typename T::value_type, 4> a{{static_cast<typename T::value_type>('\\'), static_cast<typename T::value_type>('\\'),
        static_cast<typename T::value_type>('?'), static_cast<typename T::value_type>('\\')}};
    static const T s{a.data(), a.size()};
    return s;
}

template <typename T>
const T& unc_prefix_device() { // access Win32 device driver
    static const std::array<typename T::value_type, 4> a{{static_cast<typename T::value_type>('\\'), static_cast<typename T::value_type>('\\'),
        static_cast<typename T::value_type>('.'), static_cast<typename T::value_type>('\\')}};
    static const T s{a.data(), a.size()};
    return s;
}

template <typename T>
inline PS_WARN_UNUSED_RESULT T unc_prefix(const T& path) {
    // XXX: unc_prefix() must be last, otherwise it will match the other prefixes
    return prefix(path, ifilesystem::unc_prefix_raw<T>(), ifilesystem::unc_prefix_device<T>(), ifilesystem::unc_prefix<T>());
}

template <typename T>
bool starts_with_drive_letter(const T& path, typename T::size_type pos = 0) {
    if (((path.length() - pos) >= 2) && (path.at(pos + 1) == ':')) {
        typename T::value_type char0 = path.at(pos);
        if ((char0 >= 'A' && char0 <= 'Z') || (char0 >= 'a' && char0 <= 'z')) {
            return true;
        }
    }
    return false;
}

} // ifilesystem

template <typename T>
void append(T& path, const T& comp, path_style sty = path_style::native) {
    if (!comp.empty()) {
        auto&& delimiter = ifilesystem::delimiter_for_style<T>(sty);
        if (!path.empty() && !ends_with<T>(path, delimiter) && !starts_with<T>(comp, delimiter)) {
            path.append(delimiter);
        }
        path.append(comp);
    }
}

template <typename T>
inline T& sanitize(T& path, path_style sty = path_style::native) {
    ifilesystem::convert_to_native_delimiter(path, sty);
    ifilesystem::collapse_delimiters(path, sty);
    return path;
}

template <typename T>
inline PS_WARN_UNUSED_RESULT T sanitize(const T& path, path_style sty = path_style::native) {
    T pathCopy{path};
    sanitize(pathCopy, sty);
    return pathCopy;
}

} // v1
} // filesystem
} // prosoft

#endif // PS_CORE_PATH_UTILS_HPP
