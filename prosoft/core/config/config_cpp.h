// Copyright © 2013-2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_CONFIG_CPP_H
#define PS_CORE_CONFIG_CPP_H

#ifndef PS_CORE_CONFIG_COMPILER_H
#error "missing config_compiler"
#endif

#if defined(__cplusplus)

#include <ciso646> // Lib version detection

#define PS_EXTERN_C extern "C"
// clang-format off
#define PS_EXTERN_C_BEGIN PS_EXTERN_C {
#define PS_EXTERN_C_END }
// clang-format on

//// C++ versions ////

#define PS_CPP11_STDLIB (PS_CPP11 && (_LIBCPP_VERSION || PS_MSVC_REQ(17, 0) || PS_GCC_REQ(4, 6, 0)))
#define PS_PREFERRED_CPP11_STDLIB (PS_CPP11_STDLIB && PS_PREFERRED_CPP11)
// GCC libstdc++ C++11 compliance: https://gcc.gnu.org/onlinedocs/libstdc++/manual/status.html#status.iso.200x
#define PS_COMPLETE_CPP11_STDLIB (PS_CPP11_STDLIB && (PS_CLANG_REQ(3, 3, 0) || (PS_GCC_REQ(5, 1, 0) && _GLIBCXX_USE_CXX11_ABI == 1)))

#define PS_CPP14_STDLIB (PS_CPP14 && (_LIBCPP_STD_VER > 11 || PS_MSVC_REQ(19, 0) || PS_GCC_REQ(4, 9, 0)))
// Clang 3.4 (and pre-release 3.5) are using the draft 'shared_mutex' instead of the ratified 'shared_timed_mutex'
// VS2015 final added shared_mutex support, that's our criteria for "preferred" status.
#if !__APPLE__
#define PS_PREFERRED_CPP14_STDLIB (PS_CPP14_STDLIB && (PS_CLANG_REQ(3, 5, 0) || PS_GCC_REQ(4, 9, 0) || PS_MSVC_REQ(19, 0)))
#define PS_COMPLETE_CPP14_STDLIB (PS_CPP14_STDLIB && (PS_CLANG_REQ(3, 5, 0) || PS_GCC_REQ(5, 1, 0)))
#else
// XXX: Xcode 6.1 claims to fully support C++14.
// However the libc++ lib does not provide C++14 support until 10.12. (e.g. shared_timed_mutex fails to link)
#include <AvailabilityMacros.h>
#ifdef MAC_OS_X_VERSION_10_12
#define PS_PREFERRED_CPP14_STDLIB (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12)
#define PS_COMPLETE_CPP14_STDLIB (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12)
#else
#define PS_PREFERRED_CPP14_STDLIB 0
#define PS_COMPLETE_CPP14_STDLIB 0
#endif
#endif // !__APPLE__

//// C++ features ////

#if (__has_feature(cxx_noexcept) || PS_GCC_REQ(4, 6, 0)) || PS_MSVC_REQ(19, 0)
#define PS_NOEXCEPT noexcept
#define PS_CONDITIONAL_NOEXCEPT(x) noexcept(x)
#else
#define PS_NOEXCEPT throw()
#define PS_CONDITIONAL_NOEXCEPT(x)
#endif

// MSVC 2015 constexpr support is limited to C++11 spec only
#if (__has_feature(cxx_constexpr) || PS_GCC_REQ(4, 6, 0) || PS_MSVC_REQ(19, 0))
#define PS_HAVE_CONSTEXPR 1
#define PS_CONSTEXPR constexpr
#define PS_MAKE_CONSTEXPR static constexpr
#else
#define PS_HAVE_CONSTEXPR 0
#define PS_CONSTEXPR
#define PS_MAKE_CONSTEXPR
#endif

// GCC 4.9.x has problems with certain legal C++14 constexpr expressions (namely involving references).
#if PS_HAVE_CONSTEXPR && PS_COMPLETE_CPP14
#define PS_CONSTEXPR_IF_CPP14 constexpr
#else
#define PS_CONSTEXPR_IF_CPP14
#endif

#if (__has_feature(cxx_inline_namespaces) || PS_GCC_REQ(4, 6, 0) || PS_MSVC_REQ(19, 0))
#define PS_HAVE_INLINE_NAMESPACES 1
#else
#define PS_HAVE_INLINE_NAMESPACES 0
#endif

#if (__has_feature(cxx_reference_qualified_functions) || PS_GCC_REQ(4, 8, 1) || PS_MSVC_REQ(19, 0))
#define PS_HAVE_REFERENCE_QUALIFIED_FUNCTIONS 1
#define PS_REFERENCE_QUALIFIED_LVALUE &
#define PS_REFERENCE_QUALIFIED_RVALUE &&
#else
#define PS_HAVE_REFERENCE_QUALIFIED_FUNCTIONS 0
#define PS_REFERENCE_QUALIFIED_LVALUE
#define PS_REFERENCE_QUALIFIED_RVALUE
#endif

#define PS_HAVE_UDL (PS_COMPLETE_CPP11 || PS_MSVC_REQ(19, 0))

#define PS_HAVE_DEFAULT_MOVE (PS_CPP11 && (!_MSC_VER || PS_MSVC_REQ(19, 0)))

//// Stdlib features ////

// Determine if the stdlib has C++14 constexpr additions (e.g. std::pair<>)
// MSVC 2015 constexpr support is limited to C++11, thus the library is limited as well.
#if PS_HAVE_CONSTEXPR && (_LIBCPP_STD_VER > 11 || PS_GCC_REQ(4, 9, 0))
#define PS_CONSTEXPR_IF_CPP14_STDLIB constexpr
#else
#define PS_CONSTEXPR_IF_CPP14_STDLIB
#endif

#if (PS_CPP14_STDLIB || PS_MSVC_REQ(18, 0))
#define PS_HAVE_MAKE_UNIQUE 1
#else
#define PS_HAVE_MAKE_UNIQUE 0
#endif

#else // __cplusplus

#define PS_EXTERN_C
#define PS_EXTERN_C_BEGIN
#define PS_EXTERN_C_END

#define PS_CPP11_STDLIB 0
#define PS_PREFERRED_CPP11_STDLIB 0
#define PS_COMPLETE_CPP11_STDLIB 0
#define PS_CPP14_STDLIB 0
#define PS_PREFERRED_CPP14_STDLIB 0
#define PS_COMPLETE_CPP14_STDLIB 0

#endif // __cplusplus

#endif // PS_CORE_CONFIG_CPP_H
