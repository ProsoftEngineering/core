// Copyright © 2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_APPLE_CONVERT_HPP
#define PS_CORE_APPLE_CONVERT_HPP

#if __OBJC__
#import <Foundation/Foundation.h>
#else
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <memory>

#include <prosoft/core/include/string/string_convert.hpp>
#include <prosoft/core/modules/u8string/u8string.hpp>

namespace prosoft {

namespace detail {
using char_buf = std::unique_ptr<char[]>;

inline char_buf CFString_to_bytes(CFStringRef s, CFStringEncoding encoding = kCFStringEncodingUTF8) {
    if (s) {
        const auto sz = ::CFStringGetMaximumSizeForEncoding(::CFStringGetLength(s), encoding) + 1 /* NULL term */;
#if PS_HAVE_MAKE_UNIQUE
        auto buf = std::make_unique<char[]>(static_cast<size_t>(sz));
#else
        auto buf = char_buf{new char[sz]};
#endif

        const auto good = ::CFStringGetCString(s, buf.get(), sz, encoding);
        PS_THROW_IF(!good, std::runtime_error{"CFStringGetCString failed"});

        return buf;
    } else {
        auto buf = new char[1];
        *buf = 0;
        return char_buf{buf};
    }
}

} // detail

#if PS_HAVE_INLINE_NAMESPACES
inline namespace conversion {
#endif

template <>
class to_string<u8string, CFStringRef> {
public:
    typedef u8string result_type;
    typedef CFStringRef argument_type;

    result_type operator()(const argument_type s) {
        // Attempt to get existing internal CF representation.
        if (const auto buf = (s ? ::CFStringGetCStringPtr(s, kCFStringEncodingUTF8) : nullptr)) {
            return u8string{buf};
        } else {
            return u8string{detail::CFString_to_bytes(s).get()};
        }
    }
};

template <>
class to_string<std::string, CFStringRef> {
public:
    typedef std::string result_type;
    typedef CFStringRef argument_type;

    result_type operator()(const argument_type s) {
        return to_string<u8string, CFStringRef>{}(s).str();
    }

    result_type operator()(const argument_type s, CFStringEncoding encoding) {
        return result_type{detail::CFString_to_bytes(s, encoding).get()};
    }
};

template <class Result>
using from_CFString = to_string<Result, CFStringRef>;

#if __OBJC__

template <>
class to_string<u8string, NSString*> {
public:
    typedef u8string result_type;
    typedef NSString* argument_type;

    result_type operator()(argument_type s) {
        return to_string<u8string, CFStringRef>{}((__bridge CFStringRef)s);
    }
};

template <>
class to_string<std::string, NSString*> {
public:
    typedef std::string result_type;
    typedef NSString* argument_type;

    result_type operator()(argument_type s) {
        return to_string<u8string, CFStringRef>{}((__bridge CFStringRef)s).str();
    }

    result_type operator()(argument_type s, NSStringEncoding encoding) {
        return to_string<std::string, CFStringRef>{}((__bridge CFStringRef)s, ::CFStringConvertNSStringEncodingToEncoding(encoding));
    }
};

template <class Result>
using from_NSString = to_string<Result, NSString*>;

#endif

#if PS_HAVE_INLINE_NAMESPACES
} // conversion
#endif

} // prosoft

#endif // PS_CORE_APPLE_CONVERT_HPP
