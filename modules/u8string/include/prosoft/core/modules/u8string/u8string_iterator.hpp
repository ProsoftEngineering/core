// Copyright © 2014-2018, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_U8STRING_ITERATOR_HPP
#define PS_CORE_U8STRING_ITERATOR_HPP
// XXX: This is only to be included by psustring.hpp and thus includes no external dependencies.

#include <iterator>

namespace prosoft {
namespace iu8string {

// char32_t ensures we use the type_traits<char32_t> specialization which has the proper int_type.
// uint32_t is used by utf8cpp but the types should be equivalent.
typedef char32_t unicode_type;

// XXX: NOT THREAD SAFE
template <typename octet_iterator> // see notes below for reference and pointer type why.
class u8_iterator {
public:
    typedef unicode_type value_type;
    typedef ptrdiff_t  difference_type;
    typedef unicode_type  pointer;
    typedef unicode_type reference;
    typedef std::bidirectional_iterator_tag  iterator_category;
    
    typedef octet_iterator iterator_type;

private:
    iterator_type mI;
    iterator_type mStart;
    iterator_type mEnd;
    mutable difference_type mMovement; // optimization hack to avoid use of std::distance() to get a count

public:
    u8_iterator()
        : mI()
        , mStart()
        , mEnd()
        , mMovement(0) {
    }
    explicit u8_iterator(const iterator_type& i, const iterator_type& start, const iterator_type& end)
        : mI(i)
        , mStart(start)
        , mEnd(end)
        , mMovement(0) {
        if (PS_UNEXPECTED(mI < mStart || mI > mEnd)) {
            throw std::out_of_range("Invalid utf-8 iterator position");
        }
    }
    PS_DEFAULT_COPY(u8_iterator);
    PS_DEFAULT_MOVE(u8_iterator);

    iterator_type base() const { return mI; }

    reference operator*() const {
        auto tmp = mI;
        return utf8::next(tmp, mEnd);
    }

    pointer operator->() const { return operator*(); }

    bool operator==(const u8_iterator& other) const {
        if (PS_UNEXPECTED(mStart != other.mStart || mEnd != other.mEnd)) {
            throw std::logic_error("Comparing utf-8 iterators defined with different ranges");
        }
        return mI == other.mI;
    }

    u8_iterator& operator++() {
        utf8::next(mI, mEnd);
        ++mMovement;
        return *this;
    }

    u8_iterator operator++(int) {
        auto tmp = *this;
        utf8::next(mI, mEnd);
        ++mMovement;
        return tmp;
    }

    u8_iterator& operator--() {
        utf8::prior(mI, mStart);
        --mMovement;
        return *this;
    }

    u8_iterator operator--(int) {
        auto tmp = *this;
        utf8::prior(mI, mStart);
        --mMovement;
        return tmp;
    }

    difference_type movement() const { return mMovement; }
};

template <typename octet_iterator>
inline bool operator==(const u8_iterator<octet_iterator>& lhs, const u8_iterator<octet_iterator>& rhs) {
    return lhs == rhs;
}

template <typename octet_iterator>
inline bool operator!=(const u8_iterator<octet_iterator>& lhs, const u8_iterator<octet_iterator>& rhs) {
    return !(lhs == rhs);
}

template <typename octet_iterator>
inline bool operator<(const u8_iterator<octet_iterator>& lhs, const u8_iterator<octet_iterator>& rhs) {
    return lhs.base() < rhs.base();
}

template <typename octet_iterator>
inline bool operator<=(const u8_iterator<octet_iterator>& lhs, const u8_iterator<octet_iterator>& rhs) {
    return lhs.base() <= rhs.base();
}

template <typename octet_iterator>
inline bool operator>(const u8_iterator<octet_iterator>& lhs, const u8_iterator<octet_iterator>& rhs) {
    return lhs.base() > rhs.base();
}

template <typename octet_iterator>
inline bool operator>=(const u8_iterator<octet_iterator>& lhs, const u8_iterator<octet_iterator>& rhs) {
    return lhs.base() >= rhs.base();
}

} // iu8string
} // prosoft

#endif // PS_CORE_U8STRING_ITERATOR_HPP

/*
u8_iterator reference and pointer types are defined as integral copies so we don't have to use a stashing iterator to return true reference and pointer types.

In order to support a true reference type we'd have to stash a copy of the current code point, however,
GCC and MSVC reverse iterators conform to the C++03 spec and do not work with stashing iterators.
The C++11 spec originaly resolved this deficiency in the C++O3 spec, but GCC and MSVC opened a errata to revert the behavior to C++03 semantics.
https://gcc.gnu.org/bugzilla/show_bug.cgi?id=51823
https://connect.microsoft.com/VisualStudio/feedback/details/813613/tempories-created-by-std-reverse-iterator-return-garbage-values-with-certain-base-iterators
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3473.html#2204

Clang later followed suit after the errata was finalized.


In addition, std::search() and std::find_if (possibly more) in GCC 4.9+ libstdc++ is also broken with stashing iterators
std::search was rewritten
4.8.x: https://github.com/gcc-mirror/gcc/blob/gcc-4_8-branch/libstdc%2B%2B-v3/include/bits/stl_algo.h __search_n()
4.9.x+: https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/include/bits/stl_algo.h __search

I believe the 4.9+ incompatibility stems from:

std::__find_if(__first1, __last1, __gnu_cxx::__ops::__iter_comp_iter(__predicate, __first2));

where __iter_comp_iter() returns an instance of struct _Iter_comp_to_iter
https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/include/bits/predefined_ops.h

_Iter_comp_to_iter holds a reference type internally and if that reference goes out of scope, it's garbage.
*/
