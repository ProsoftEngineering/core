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

// Defines string template conversion functors.

#ifndef PS_CORE_STRING_CONVERT_HPP
#define PS_CORE_STRING_CONVERT_HPP

#include <cctype>
#include <string>

#include <prosoft/core/config/config.h>
#include <prosoft/core/include/string/string_types.hpp>

namespace prosoft {

#if PS_HAVE_INLINE_NAMESPACES
inline namespace conversion {
#endif

// Like std::to_string, but can convert to/from any type.
template <class Result, class Arg>
class to_string {
public:
    typedef Result result_type;
    typedef Arg argument_type;

    // Default implements the case where the types are the same.
    // Specializations are needed for actual conversion.
    template <typename = typename std::enable_if<std::is_same<result_type, argument_type>::value>>
    const argument_type& operator()(const argument_type& us) {
        return us;
    }

    template <typename = typename std::enable_if<std::is_same<result_type, argument_type>::value>>
    argument_type operator()(argument_type&& us) {
        return result_type{std::move(us)};
    }
};

// These are mainly for use by case_convert, but can be used independently.
template <typename C>
struct lowercase {
    typedef C char_type;
    char_type operator()(char_type c) {
        return std::tolower(c);
    }
};

template <typename C>
struct uppercase {
    typedef C char_type;
    char_type operator()(char_type c) {
        return std::toupper(c);
    }
};

// Placeholder for future use, both args should be constant C strings
#if !defined(PSLocalizedString)
#define PSLocalizedString(key, comment) key
#endif

template <class Result>
using from_localized_string = to_string<Result, std::string>;

#if PS_HAVE_INLINE_NAMESPACES
} // conversion
#endif

} // prosoft

#endif // PS_CORE_STRING_CONVERT_HPP
