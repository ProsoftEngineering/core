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

#ifndef PS_CORE_UNICODE_CONVERT_HPP
#define PS_CORE_UNICODE_CONVERT_HPP

#include <prosoft/core/include/string/string_convert.hpp>
#include <prosoft/core/modules/u8string/u8string.hpp>

namespace prosoft {

#if PS_HAVE_INLINE_NAMESPACES
inline namespace conversion {
#endif

template <>
class to_string<std::string, u8string> {
public:
    typedef std::string result_type;
    typedef u8string argument_type;
    result_type operator()(const argument_type& us) {
        return us.str();
    }
};

template <>
class to_string<u16string, u8string> {
public:
    typedef u16string result_type;
    typedef u8string argument_type;
    result_type operator()(const argument_type& us) {
        return unicode::u16(us);
    }
};

template <>
class to_string<u16string, std::string> {
public:
    typedef u16string result_type;
    typedef std::string argument_type;
    result_type operator()(const argument_type& s) {
        return operator()(s.c_str());
    }
    result_type operator()(const char* s) {
        return to_string<result_type, u8string>{}(u8string{s});
    }
};

template <>
class to_string<u8string, u16string> {
public:
    typedef u8string result_type;
    typedef u16string argument_type;
    result_type operator()(const argument_type& us) {
        return result_type{us};
    }
};

template <>
class to_string<std::string, u16string> {
public:
    typedef std::string result_type;
    typedef u16string argument_type;
    result_type operator()(const argument_type& us) {
        return to_string<u8string, argument_type>{}(us).str();
    }
};

template <class Argument>
using to_u8string = to_string<u8string, Argument>;

template <class Result>
using from_u8string = to_string<Result, u8string>;

template <class Argument>
using to_u16string = to_string<u16string, Argument>;

template <class Result>
using from_u16string = to_string<Result, u16string>;

template <>
struct lowercase<u8string::unicode_type> {
    typedef u8string::unicode_type char_type;
    char_type operator()(char_type c) {
        return prosoft::unicode::tolower(c);
    }
};

template <>
struct uppercase<u8string::unicode_type> {
    typedef u8string::unicode_type char_type;
    char_type operator()(char_type c) {
        return prosoft::unicode::toupper(c);
    }
};

namespace iunicode {
// UTF16 lower/upper, still lossy but better than std:: ASCII
template <class UnaryOperator>
u16string::value_type u16convert(u16string::value_type c, UnaryOperator converter) {
    if (!utf8::internal::is_surrogate(c)) {
        const auto uu32 = converter(static_cast<u8string::unicode_type>(c));
        if (uu32 <= 0xffffU) {
            return static_cast<u16string::value_type>((uu32 & 0xffffU));
        }
    }

    return c;
}
} // udetail

template <>
struct lowercase<u16string::value_type> {
    typedef u16string::value_type char_type;
    char_type operator()(char_type c) {
        return iunicode::u16convert(c, prosoft::unicode::tolower);
    }
};

template <>
struct uppercase<u16string::value_type> {
    typedef u16string::value_type char_type;
    char_type operator()(char_type c) {
        return iunicode::u16convert(c, prosoft::unicode::toupper);
    }
};

#if PS_HAVE_INLINE_NAMESPACES
} // conversion::
#endif

} // prosoft

#endif // PS_CORE_UNICODE_CONVERT_HPP
