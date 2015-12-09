// Copyright © 2012-2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_BYTEORDER_H
#define PS_CORE_BYTEORDER_H

#include <prosoft/core/config/config.h>

#if _MSC_VER
#include <stdlib.h>
PS_STATIC_ASSERT(sizeof(short) == 2 && sizeof(int) == 4 && sizeof(long) == 4, "Broken assumption.");
#define __builtin_bswap16(x) _byteswap_ushort((x))
#define __builtin_bswap32(x) _byteswap_ulong((x))
#define __builtin_bswap64(x) _byteswap_uint64((x))
// XXX: are _byteswap_* intrinsics or library functions?
#define PS_BSWAP_CONSTEXPR

#if !defined(__BYTE_ORDER__)
// ARM can be big or little, Windows ARM is little.
#define __ORDER_BIG_ENDIAN__ 4321
#define __ORDER_LITTLE_ENDIAN__ 1234
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#endif
#else // __MSC_VER
#define PS_BSWAP_CONSTEXPR constexpr
#endif // _MSC_VER

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ == 0 || (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__ && __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__)
#error "unknown byte order"
#endif

#ifdef __cplusplus
#include <cstdint>

namespace prosoft {

// Templates are used to avoid signed/unsigned casting.
// Depending on the version of GCC the intrinsics may not produce the most optimal code for non-X86 archs.
// Linux pollutes the preprocessor with bswapXX macros, thus we have to use byteswapXX
template <typename T>
PS_BSWAP_CONSTEXPR inline T byteswap16(T x) {
    static_assert(2 == sizeof(x), "BUG");
    return static_cast<T>(__builtin_bswap16(static_cast<uint16_t>(x & 0xFFFFU)));
}

template <typename T>
PS_BSWAP_CONSTEXPR inline T byteswap32(T x) {
    static_assert(4 == sizeof(x), "BUG");
    return static_cast<T>(__builtin_bswap32(static_cast<uint32_t>(x & 0xFFFFFFFFU)));
}

template <typename T>
PS_BSWAP_CONSTEXPR inline T byteswap64(T x) {
    static_assert(8 == sizeof(x), "BUG");
    return static_cast<T>(__builtin_bswap64(static_cast<uint64_t>(x)));
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_le16(T x) { return x; }
template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_le32(T x) { return x; }
template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_le64(T x) { return x; }

template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_be16(T x) { return byteswap16(x); }
template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_be32(T x) { return byteswap32(x); }
template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_be64(T x) { return byteswap64(x); }

template <typename T>
PS_BSWAP_CONSTEXPR inline T le16_to_host(T x) { return x; }
template <typename T>
PS_BSWAP_CONSTEXPR inline T le32_to_host(T x) { return x; }
template <typename T>
PS_BSWAP_CONSTEXPR inline T le64_to_host(T x) { return x; }

template <typename T>
PS_BSWAP_CONSTEXPR inline T be16_to_host(T x) { return byteswap16(x); }
template <typename T>
PS_BSWAP_CONSTEXPR inline T be32_to_host(T x) { return byteswap32(x); }
template <typename T>
PS_BSWAP_CONSTEXPR inline T be64_to_host(T x) { return byteswap64(x); }

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_le16(T x) { return byteswap16(x); }
template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_le32(T x) { return byteswap32(x); }
template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_le64(T x) { return byteswap64(x); }

template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_be16(T x) { return x; }
template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_be32(T x) { return x; }
template <typename T>
PS_BSWAP_CONSTEXPR inline T host_to_be64(T x) { return x; }

template <typename T>
PS_BSWAP_CONSTEXPR inline T le16_to_host(T x) { return byteswap16(x); }
template <typename T>
PS_BSWAP_CONSTEXPR inline T le32_to_host(T x) { return byteswap32(x); }
template <typename T>
PS_BSWAP_CONSTEXPR inline T le64_to_host(T x) { return byteswap64(x); }

template <typename T>
PS_BSWAP_CONSTEXPR inline T be16_to_host(T x) { return x; }
template <typename T>
PS_BSWAP_CONSTEXPR inline T be32_to_host(T x) { return x; }
template <typename T>
PS_BSWAP_CONSTEXPR inline T be64_to_host(T x) { return x; }

#else

#error "unknown byte order"

#endif

} // prosoft

#endif // __cplusplus

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define ps_le16_to_host(x) (x)
#define ps_le32_to_host(x) (x)
#define ps_le64_to_host(x) (x)
#define ps_be16_to_host(x) __builtin_bswap16((x))
#define ps_be32_to_host(x) __builtin_bswap32((x))
#define ps_be64_to_host(x) __builtin_bswap64((x))
#define ps_host_to_le16(x) (x)
#define ps_host_to_le32(x) (x)
#define ps_host_to_le64(x) (x)
#define ps_host_to_be16(x) __builtin_bswap16((x))
#define ps_host_to_be32(x) __builtin_bswap32((x))
#define ps_host_to_be64(x) __builtin_bswap64((x))

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#define ps_le16_to_host(x) __builtin_bswap16((x))
#define ps_le32_to_host(x) __builtin_bswap32((x))
#define ps_le64_to_host(x) __builtin_bswap64((x))
#define ps_be16_to_host(x) (x)
#define ps_be32_to_host(x) (x)
#define ps_be64_to_host(x) (x)
#define ps_host_to_le16(x) __builtin_bswap16((x))
#define ps_host_to_le32(x) __builtin_bswap32((x))
#define ps_host_to_le64(x) __builtin_bswap64((x))
#define ps_host_to_be16(x) (x)
#define ps_host_to_be32(x) (x)
#define ps_host_to_be64(x) (x)

#else

#error "unknown byte order"

#endif

#endif // PS_CORE_BYTEORDER_H
