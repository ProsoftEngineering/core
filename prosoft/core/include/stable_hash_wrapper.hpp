// Copyright © 2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_STABLE_HASH_WRAPPER_HPP
#define PS_STABLE_HASH_WRAPPER_HPP

/*
A stable_hash_wrapper allows move semantics to be used with hash collections that enforce const value access to maintain internal integrity.
(e.g. unordered_set.) The tradeoff made is that you can now have "dead" objects in such a collection. This can result in hash collisions
and possible collection performance degradation. It could also result in subtle bugs (depending on collection and/or value type implementation details).
Therefore the recommended usage is only for move-out-on-erase scenerios.
*/

#include <functional>
#include <type_traits>

#include <prosoft/core/config/config.h>

namespace prosoft {

template <class T, typename = typename std::enable_if<
    std::is_move_constructible<T>::value
    && std::is_constructible<std::hash<T>>::value
    && std::is_constructible<std::equal_to<T>>::value
    >::type>
class stable_hash_wrapper {
public:
    using value_type = T;
    using hash_type = decltype(std::hash<T>()(std::declval<T>()));
private:
    T mutable m_val;
    hash_type m_hash;
public:
    template <class... Args>
    stable_hash_wrapper(Args&&... args) : m_val(std::forward<Args>(args)...), m_hash(std::hash<T>{}(m_val)) {
    }
    ~stable_hash_wrapper() = default;
    PS_DEFAULT_MOVE(stable_hash_wrapper);
    PS_DEFAULT_COPY(stable_hash_wrapper);
    
    hash_type hash() const noexcept(std::is_integral<hash_type>::value) {
        return m_hash;
    }
    
    const value_type& get() const noexcept {
        return m_val;
    }
    
    operator const value_type& () const noexcept {
        return get();
    }
    
    value_type extract() const noexcept { // don't need the noexcept move conditional as the class requires it
        return std::move(m_val);
    }
    
    bool operator==(const stable_hash_wrapper& other) const noexcept(noexcept(std::equal_to<T>()(std::declval<T>(), std::declval<T>()))) {
        using namespace std;
        return this == &other || equal_to<T>{}(get(), other.get());
    }
    
    bool operator!=(const stable_hash_wrapper& other) const noexcept(noexcept(std::equal_to<T>()(std::declval<T>(), std::declval<T>()))) {
        return !operator==(other);
    }
};

} // prosoft

namespace std {

template <class T>
struct hash<prosoft::stable_hash_wrapper<T>> {
    typedef prosoft::stable_hash_wrapper<T> argument_type;
    typedef typename prosoft::stable_hash_wrapper<T>::hash_type result_type;
    result_type operator()(const argument_type& a) const noexcept(std::is_integral<result_type>::value) {
        return a.hash();
    };
};

template <class T>
struct equal_to<prosoft::stable_hash_wrapper<T>> {
    typedef prosoft::stable_hash_wrapper<T> first_argument_type;
    typedef prosoft::stable_hash_wrapper<T> second_argument_type;
    typedef typename prosoft::stable_hash_wrapper<T>::hash_type result_type;
    result_type operator()(const first_argument_type& a1, const second_argument_type& a2) const {
        return a1.operator==(a2);
    }
};

} // std

#endif // PS_STABLE_HASH_WRAPPER_HPP
