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

#ifndef PS_CORE_CASE_CONVERT_HPP
#define PS_CORE_CASE_CONVERT_HPP

#include <prosoft/core/config/config.h>

#include <algorithm>
#include <iterator>

#include <prosoft/core/include/string/string_convert.hpp>

namespace prosoft {

#if PS_HAVE_INLINE_NAMESPACES
inline namespace conversion {
#endif

template <typename T>
inline T tolower(const T &str) {
	T s;
    s.reserve(str.size());
	std::transform(str.begin(), str.end(), std::back_inserter<T>(s), lowercase<typename T::value_type>{});
	return s;
}

template <typename T>
inline T toupper(const T &str) {
    T s;
    s.reserve(str.size());
    std::transform(str.begin(), str.end(), std::back_inserter<T>(s), uppercase<typename T::value_type>{});
    return s;
}

#if PS_HAVE_INLINE_NAMESPACES
} // conversion
#endif

} // prosoft

#endif // PS_CORE_CASE_CONVERT_HPP
