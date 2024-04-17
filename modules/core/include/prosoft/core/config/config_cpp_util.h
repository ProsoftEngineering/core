// Copyright © 2013-2016, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_CONFIG_CPP_UTIL_H
#define PS_CORE_CONFIG_CPP_UTIL_H

#ifndef PS_CORE_CONFIG_CPP_H
#error "missing config_cpp"
#endif

// C++ utility macros

#if defined(__cplusplus)

#include <algorithm>
#include <type_traits>

#define PS_DEFAULT_CONSTRUCTOR(Class) Class() = default
#define PS_DEFAULT_DESTRUCTOR(Class) ~Class() = default
#define PS_DEFAULT_COPY(Class)     \
    Class(const Class&) = default; \
    Class& operator=(const Class&) = default
#if PS_HAVE_DEFAULT_MOVE
#define PS_DEFAULT_MOVE(Class) \
    Class(Class&&) = default;  \
    Class& operator=(Class&&) = default
#else
#define PS_DEFAULT_MOVE(Class)
#endif

#define PS_DEFAULT_CLASS(Class)    \
    PS_DEFAULT_CONSTRUCTOR(Class); \
    PS_DEFAULT_DESTRUCTOR(Class);  \
    PS_DEFAULT_COPY(Class);        \
    PS_DEFAULT_MOVE(Class)

#define PS_DISABLE_DEFAULT_CONSTRUCTOR(Class) Class() = delete
#define PS_DISABLE_COPY(Class)    \
    Class(const Class&) = delete; \
    Class& operator=(const Class&) = delete
#if PS_HAVE_DEFAULT_MOVE
#define PS_DISABLE_MOVE(Class) \
    Class(Class&&) = delete;   \
    Class& operator=(Class&&) = delete
#else
#define PS_DISABLE_MOVE(Class)
#endif

// clang-format off
// XXX: Clang will throw a "unused function" if these are declared in an anon namespace.
#define PS_ENUM_BITMASK_OPS(ET) \
inline PS_CONSTEXPR ET operator|(ET lhs, ET rhs) { using int_t = std::underlying_type<ET>::type; return ET(int_t(lhs) | int_t(rhs)); } \
inline PS_CONSTEXPR_IF_CPP14 ET& operator|=(ET& lhs, ET rhs) { lhs = operator|(lhs, rhs); return lhs; }                                \
inline PS_CONSTEXPR ET operator&(ET lhs, ET rhs) { using int_t = std::underlying_type<ET>::type; return ET(int_t(lhs) & int_t(rhs)); } \
inline PS_CONSTEXPR_IF_CPP14 ET& operator&=(ET& lhs, ET rhs) { lhs = operator&(lhs, rhs); return lhs; }                                \
inline PS_CONSTEXPR ET operator^(ET lhs, ET rhs) { using int_t = std::underlying_type<ET>::type; return ET(int_t(lhs) ^ int_t(rhs)); } \
inline PS_CONSTEXPR_IF_CPP14 ET& operator^=(ET& lhs, ET rhs) { lhs = operator&(lhs, rhs); return lhs; }                                \
inline PS_CONSTEXPR ET operator~(ET lhs) { using int_t = std::underlying_type<ET>::type; return ET(~ int_t(lhs)); }                    \
inline PS_CONSTEXPR bool is_set(ET lhs)  { using int_t = std::underlying_type<ET>::type; return 0 != int_t(lhs); } // no implicit conversion to bool
// clang-format on

namespace prosoft {
template <typename T>
PS_CONSTEXPR_IF_CPP14 inline const T& clamp(const T& v, const T& min, const T& max) {
    return std::min(max, (std::max(min, v)));
}
}
#define PS_CLAMP(v, min, max) prosoft::clamp(v, min, max)

namespace prosoft {
// ssize_t is avaialble on most (all?) UNIX systems, but not Windows.
#if !__LLP64__
typedef long s_size_t;
#else
typedef long long s_size_t;
#endif
};
static_assert(sizeof(prosoft::s_size_t) == sizeof(size_t), "Broken assumption");

#endif // __cplusplus

#endif // PS_CORE_CONFIG_CPP_UTIL_H
