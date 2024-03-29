// Copyright © 2014-2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

// C++17 introduces uniform container access. This expands on that.
// https://isocpp.org/files/papers/n4280.pdf

#ifndef PS_CORE_UNIFORM_ACCESS__HPP
#define PS_CORE_UNIFORM_ACCESS__HPP

#include <iterator>
#include <type_traits>
#include <utility>

#include <prosoft/core/config/config.h>

namespace prosoft {

struct access_traits_base {
    typedef const char* const_byte_pointer;
};

template <class T, class E = void>
struct access_traits : public access_traits_base {
    using const_value_pointer = const typename T::value_type*;
    
    PS_CONSTEXPR_IF_CPP14 size_t data_size(const T& t) const {
        return t.size();
    }
    
    PS_CONSTEXPR_IF_CPP14 const_value_pointer data(const T& t) const {
        return t.data();
    }
    
    PS_CONSTEXPR_IF_CPP14 size_t byte_size(const T& t) const {
        return t.size() * sizeof(typename T::value_type);
    }
    
    PS_CONSTEXPR_IF_CPP14 const_byte_pointer bytes(const T& t) const {
        return reinterpret_cast<const_byte_pointer>(t.data());
    }
};

template <class T>
struct access_traits<T*, typename std::enable_if<std::is_integral<T>::value>::type> : public access_traits_base {
    using const_value_pointer = const T*;
    
    constexpr size_t data_size(const_value_pointer t) const {
#if PS_COMPLETE_CPP14
// Bug in VS 2017.0: https://developercommunity.visualstudio.com/content/problem/10720/constexpr-function-accessing-character-array-leads.html
// Requires C++14 expanded constexpr.
        int i = 0;
        while (t[i]) {
            i++;
        }
        return i;
#else
        return *t ? 1 + data_size(t + 1) : 0;
#endif
    }
    
    constexpr const_value_pointer data(const_value_pointer t) const {
        return t;
    }
    
    constexpr size_t byte_size(const_value_pointer t) const {
        return data_size(t) * sizeof(typename std::remove_pointer<T>::type);
    }
    
    constexpr const_byte_pointer bytes(const_value_pointer t) const {
        return reinterpret_cast<typename access_traits_base::const_byte_pointer>(t);
    }
};

template <class T>
using access_traits_base_type = typename std::remove_reference<typename std::remove_const<typename std::decay<T>::type>::type>::type;

template <class T>
using access_traits_type = access_traits<access_traits_base_type<T>>;

template <class T>
constexpr typename access_traits_type<T>::const_value_pointer data(T&& t) {
    return access_traits_type<T>{}.data(std::forward<T>(t));
}

template <class T>
constexpr inline size_t data_size(T&& t) {
    return access_traits_type<T>{}.data_size(std::forward<T>(t));
}

template <class T>
constexpr inline typename access_traits_type<T>::const_byte_pointer bytes(T&& t) {
    return access_traits_type<T>{}.bytes(std::forward<T>(t));
}

template <class T>
constexpr inline size_t byte_size(T&& t) {
    return access_traits_type<T>{}.byte_size(std::forward<T>(t));
}

template <class Iterator>
struct iterator_access_traits : access_traits_base {
    typedef Iterator iterator_base_type;

    size_t size(const Iterator& first, const Iterator& last) {
        return std::distance(first, last) * sizeof(typename Iterator::value_type);
    }
    
    const_byte_pointer bytes(const Iterator& i) const {
        return reinterpret_cast<const_byte_pointer>(&i[0]);
    }
    
    iterator_base_type base(const Iterator& i) {
        return i;
    }
    
    template <class InputIterator, class... Args>
    Iterator ranged_iterator(InputIterator&& i, Args...) {
        return Iterator{std::forward<Iterator>(i)};
    }
};

template <class Iterator>
inline typename iterator_access_traits<Iterator>::const_byte_pointer bytes(const Iterator& first, const Iterator&) {
    return iterator_access_traits<Iterator>{}.bytes(first);
}

template <class Iterator>
inline size_t byte_size(const Iterator& first, const Iterator& last) {
    return iterator_access_traits<Iterator>{}.size(first, last);
}

template <class Iterator>
inline typename iterator_access_traits<Iterator>::iterator_base_type base(const Iterator& i) {
    return iterator_access_traits<Iterator>{}.base(i);
}

// ranged iterators are used by u8string
template <class Iterator, class... Args>
inline Iterator make_ranged_iterator(Args... args) {
    return iterator_access_traits<Iterator>{}.ranged_iterator(args...);
}

} // prosoft

#endif // PS_CORE_UNIFORM_ACCESS__HPP
