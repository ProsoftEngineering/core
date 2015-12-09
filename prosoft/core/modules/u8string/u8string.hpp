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

#ifndef PS_CORE_U8STRING_HPP
#define PS_CORE_U8STRING_HPP

#include <atomic>
#include <string>
#include <limits>
#include <utility>
#include <cstdint>

#include <prosoft/core/config/config.h>
#include <prosoft/core/include/string/string_types.hpp>
#include <prosoft/core/include/uniform_access.hpp>

// "utf8.h" may conflict
#include "utf8/checked.h"
#include "utf8/unchecked.h"
#include "u8string_iterator.hpp"

namespace prosoft {

class u8string {
    typedef std::string container_type;

public:
    typedef detail::unicode_type unicode_type;
    typedef unicode_type value_type;
    typedef std::char_traits<value_type> traits_type;
    typedef container_type::size_type size_type;
    typedef container_type::difference_type difference_type;
    typedef container_type::const_pointer const_pointer;

    typedef container_type data_type;
    typedef const data_type& const_data_reference;
    typedef data_type::const_pointer const_data_pointer;

    typedef detail::u8_iterator<container_type::iterator> iterator;
    typedef detail::u8_iterator<container_type::const_iterator> const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    // conversion exceptions
    typedef utf8::invalid_utf8 invalid_utf8;
    typedef utf8::invalid_utf16 invalid_utf16;
    typedef utf8::invalid_code_point invalid_unicode; // thrown with bad u32string data

    u8string() PS_CONDITIONAL_NOEXCEPT(std::is_nothrow_default_constructible<container_type>::value)
        : _u8() {
        _u8._ascii = true;
    }

    u8string(const u8string& other) PS_CONDITIONAL_NOEXCEPT(std::is_nothrow_copy_constructible<container_type>::value) {
        _u8 = other._u8;
    }

    u8string(u8string&& other) PS_CONDITIONAL_NOEXCEPT(std::is_nothrow_move_constructible<container_type>::value) {
        _u8 = std::move(other._u8);
    }

    PS_EXPORT u8string(const_iterator&, const_iterator&); // substring extraction only
    PS_EXPORT u8string(iterator&, iterator&);

    // XXX: conversions from other types are explicit as they may throw exceptions
    // XXX: DO NOT RELY on the resulting u8string raw bytes (str(), c_str()) comparing to the raw input bytes!
    //      (Which are likely be transformed during normalization.)
    //      Only use the provided comparison methods to compare bytes in any form.
    //      Codepoint -- not char -- count (length) may also be different as well as byte count.
    explicit u8string(const std::string& other) {
        _init(other);
    }
    PS_EXPORT explicit u8string(const char*, size_type nbytes = 0);

    // not explicit to allow for automatic conversion during assignment
    u8string(const uint8_t* other, size_type nbytes = 0)
        : u8string(reinterpret_cast<const char*>(other), nbytes) {
    }

    PS_EXPORT explicit u8string(const u16string&);
    PS_EXPORT explicit u8string(const u32string&);
    PS_EXPORT explicit u8string(u16string::const_pointer, size_type count = 0); // mainly for Windows to convert from a whcar_t pointer.
    PS_EXPORT explicit u8string(const value_type*, size_type count = 0);
    explicit u8string(value_type c)
        : u8string(&c, 1) {
    }

    // ==

    const u8string& operator=(const u8string& other) PS_CONDITIONAL_NOEXCEPT(std::is_nothrow_copy_assignable<container_type>::value) {
        if (this != &other) {
            _u8 = other._u8;
        }
        return *this;
    }

    const u8string& operator=(u8string&& other) PS_CONDITIONAL_NOEXCEPT(std::is_nothrow_move_assignable<container_type>::value) {
        if (this != &other) {
            _u8 = std::move(other._u8);
        }
        return *this;
    }

    PS_EXPORT const u8string& operator=(const u16string&);

    // == Modifiers -- these invalidate existing iterators.

    void swap(u8string& other) {
        _u8.swap(other._u8);
    }

    void reserve(size_type nbytes = 0) {
        _u8._s.reserve(nbytes);
    };
    void shrink_to_fit() {
        reserve();
    }
    void clear() {
        _u8.clear();
    }

    PS_EXPORT void append(const u8string& other);
    const u8string& operator+=(const u8string& other) {
        append(other);
        return *this;
    }

    PS_EXPORT void push_back(value_type);
    const u8string& operator+=(const value_type& val) {
        push_back(val);
        return *this;
    }
    PS_EXPORT void pop_back();

    // XXX: the iterator variants are potentially MUCH more efficient if you have existing iters; especially when working with large UTF data.
    PS_EXPORT iterator erase(iterator, iterator);
    iterator erase(iterator start) {
        auto f = start;
        ++f;
        return erase(start, f);
    }
    PS_EXPORT u8string& erase(size_type pos = 0, size_type len = npos);

    PS_EXPORT iterator insert(iterator, value_type);
    PS_EXPORT iterator insert(iterator, const_iterator, const_iterator);
    PS_EXPORT u8string& insert(size_type pos, const u8string& str);

    PS_EXPORT u8string& replace(iterator, iterator, const_iterator, const_iterator);
    u8string& replace(iterator start, iterator fin, const u8string& other) {
        return replace(start, fin, other.cbegin(), other.cend());
    }
    PS_EXPORT u8string& replace(size_type pos, size_type len, const u8string& str);

    // ==

    PS_EXPORT int compare(const u8string&, bool icase = false) const;
    PS_EXPORT int compare(size_type pos, size_type count, const u8string&, bool icase = false) const;
    PS_EXPORT int compare(size_type pos, size_type count, const u8string& other, size_type pos2, size_type count2, bool icase = false) const;

    PS_EXPORT static int compare(unicode_type, unicode_type, bool icase = false); // normalized codepoint compare

    // ==

    PS_EXPORT unicode_type operator[](size_type) const;

    unicode_type at(size_type pos) const {
        return operator[](pos);
    }

    unicode_type back() const {
        return at(length() - 1);
    }

    unicode_type front() const {
        return at(0);
    }

    iterator begin() {
        return make_iterator(_u8._s.begin());
    }
    const_iterator begin() const {
        return make_iterator(_u8._s.cbegin());
    }

    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }
    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(end());
    }

    iterator end() {
        return make_iterator(_u8._s.end());
    }
    const_iterator end() const {
        return make_iterator(_u8._s.cend());
    }

    reverse_iterator rend() {
        return reverse_iterator(begin());
    }
    const_reverse_iterator rend() const {
        return const_reverse_iterator(begin());
    }

    const_iterator cbegin() const {
        return begin();
    }
    const_iterator cend() const {
        return end();
    }
    const_reverse_iterator crbegin() const {
        return rbegin();
    }
    const_reverse_iterator crend() const {
        return rend();
    }

    // ==

    PS_EXPORT size_type length() const; // codepoint count -- not "character" count

    size_type size() const {
        return length();
    }

    size_type capacity() const {
        return _u8._s.capacity();
    }

    bool empty() const PS_CONDITIONAL_NOEXCEPT(noexcept(std::declval<container_type>().empty())) {
        return _u8._s.empty();
    }

    const_data_pointer data() const PS_CONDITIONAL_NOEXCEPT(noexcept(std::declval<container_type>().data())) {
        return _u8._s.data();
    }
    
    size_type data_size() const {
        return _u8._s.length();
    }

    const_data_reference str() const PS_REFERENCE_QUALIFIED_LVALUE PS_NOEXCEPT {
        return _u8._s;
    }

#if PS_HAVE_REFERENCE_QUALIFIED_FUNCTIONS
    container_type str() && PS_NOEXCEPT {
        _invalidate_cache();
        return std::move(_u8._s);
    }
#endif

    const_data_pointer c_str() const PS_CONDITIONAL_NOEXCEPT(noexcept(std::declval<container_type>().c_str())) {
        return _u8._s.c_str();
    }

    // the range constructor is generally more efficient for UTF
    PS_EXPORT u8string substr(size_type pos = 0, size_type length = npos) const;

    PS_EXPORT bool is_ascii() const;
    PS_EXPORT bool is_valid() const;
    PS_EXPORT bool has_bom() const;

    // ==

    PS_EXPORT size_type find(const u8string&, size_type pos = 0) const;
    PS_EXPORT size_type find(value_type, size_type pos = 0) const;
    // type conversions -- more expensive
    PS_EXPORT size_type find(const std::string&, size_type pos = 0) const;

    PS_EXPORT size_type rfind(const u8string&, size_type pos = npos) const;
    PS_EXPORT size_type rfind(value_type, size_type pos = npos) const;
    // type conversions -- more expensive
    PS_EXPORT size_type rfind(const std::string&, size_type pos = npos) const;

    PS_EXPORT size_type find_first_of(const u8string&, size_type pos = 0) const;
    PS_EXPORT size_type find_first_of(value_type, size_type pos = 0) const;
    // type conversions -- more expensive
    PS_EXPORT size_type find_first_of(const std::string&, size_type pos = 0) const;

    PS_EXPORT size_type find_last_of(const u8string&, size_type pos = npos) const;
    PS_EXPORT size_type find_last_of(value_type, size_type pos = npos) const;
    // type conversions -- more expensive
    PS_EXPORT size_type find_last_of(const std::string&, size_type pos = npos) const;

    // ==

    PS_EXPORT static bool is_valid(const char*, bool* ascii = nullptr);
    PS_EXPORT static bool is_valid(const std::string&, bool* ascii = nullptr);
    PS_EXPORT static bool is_valid(unicode_type c, bool* ascii = nullptr);

    static bool is_ascii(const char* s) {
        bool ascii;
        return (is_valid(s, &ascii) && ascii);
    }
    PS_EXPORT static bool is_ascii(const std::string&);

    PS_EXPORT static const u8string& bom;
    PS_EXPORT static const size_type npos;

private:
    struct _store {
        container_type _s;
        // C++11 requires any std:: type or any type used by std:: to be const thread-safe.
        // http://channel9.msdn.com/posts/C-and-Beyond-2012-Herb-Sutter-You-dont-know-blank-and-blank
        mutable std::atomic<size_type> _ct{npos}; // cached codepoint count
        bool _ascii = false;

        PS_EXPORT void swap(_store& other);
        PS_EXPORT void clear();

        PS_EXPORT void invalidate(); // invaldate cached data

        _store() = default;
        ~_store() = default;

        _store(const _store& s) {
            _s = s._s;
            _ct = s._ct.load();
            _ascii = s._ascii;
        }
        _store& operator=(const _store& s) {
            _store tmp{s};
            swap(tmp);
            return *this;
        }

        _store(_store&& other) {
            swap(other);
        }
        _store& operator=(_store&& other) {
            swap(other);
            return *this;
        }

    } _u8;

    bool ascii() const {
        return _u8._ascii;
    }

    PS_EXPORT void _init(const std::string&);

    void _invalidate_cache() {
        _u8.invalidate();
    }

    PS_EXPORT u8string(std::string&&, size_type count, bool ascii);

    iterator make_iterator(container_type::iterator i) {
        return iterator(i, _u8._s.begin(), _u8._s.end());
    }
    const_iterator make_iterator(container_type::const_iterator i) const {
        return const_iterator(i, _u8._s.begin(), _u8._s.end());
    }
};

inline void swap(u8string& lhs, u8string& rhs) {
    lhs.swap(rhs);
}

inline bool operator==(const u8string& lhs, const u8string& rhs) {
    return (0 == lhs.compare(rhs));
}

inline bool operator!=(const u8string& lhs, const u8string& rhs) {
    return (!operator==(lhs, rhs));
}

inline bool operator>(const u8string& lhs, const u8string& rhs) {
    return (lhs.compare(rhs) > 0);
}

inline bool operator<(const u8string& lhs, const u8string& rhs) {
    return (lhs.compare(rhs) < 0);
}

inline bool operator>=(const u8string& lhs, const u8string& rhs) {
    return (lhs.compare(rhs) >= 0);
}

inline bool operator<=(const u8string& lhs, const u8string& rhs) {
    return (lhs.compare(rhs) <= 0);
}

inline u8string operator+(const u8string& lhs, const u8string& rhs) {
    return (u8string(lhs) += rhs);
}

inline std::ostream& operator<<(std::ostream& lhs, const u8string& rhs) {
    return lhs << rhs.str();
}

// conversion operators -- is_valid makes sure we always return false for invalid utf8 and never throw an exception
inline bool operator==(const u8string& lhs, const std::string& rhs) {
    return (u8string::is_valid(rhs) && 0 == lhs.compare(u8string(rhs)));
}

inline bool operator==(const std::string& lhs, const u8string& rhs) {
    return operator==(rhs, lhs);
}

inline bool operator==(const u8string& lhs, const char* rhs) { // ditto above
    return (u8string::is_valid(rhs) && 0 == lhs.compare(u8string(rhs)));
}

inline bool operator==(const char*& lhs, const u8string& rhs) {
    return operator==(rhs, lhs);
}

inline bool operator!=(const u8string& lhs, const std::string& rhs) {
    return (!operator==(lhs, rhs));
}

inline bool operator!=(const std::string& lhs, const u8string& rhs) {
    return operator!=(rhs, lhs);
}

inline bool operator!=(const u8string& lhs, const char* rhs) {
    return (!operator==(lhs, rhs));
}

inline bool operator!=(const char*& lhs, const u8string& rhs) {
    return operator!=(rhs, lhs);
}

inline const uint8_t* as_utf8(const char* p) { // allows implicit construction of u8string via char*
    return reinterpret_cast<const uint8_t*>(p);
}

#if PS_HAVE_UDL
inline namespace literals {
inline namespace string_literals {
inline u8string operator"" _u8(const char* s, size_t len) {
    return u8string{as_utf8(s), len};
}
} // string_literals
} // literals
#endif

// For cases where a sepcific normalized form is required.
// These may not be compatible with u8string, hence the return type.
PS_EXPORT std::string precomposed(const u8string&);
PS_EXPORT std::string decomposed(const u8string&);

namespace unicode {
// Conversion support, prefer <string/unicode_convert.hpp>
PS_EXPORT u16string u16(const u8string&);
PS_EXPORT u32string u32(const u8string&);
PS_EXPORT u8string::unicode_type tolower(u8string::unicode_type);
PS_EXPORT u8string::unicode_type toupper(u8string::unicode_type);
} // unicode

// Uniform access specializations

template<>
struct access_traits<u8string> : access_traits_base {
    size_t data_size(const u8string& s) const {
        return s.data_size();
    }
    
    u8string::const_data_pointer data(const u8string& s) const {
        return s.data();
    }

    size_t byte_size(const u8string& s) const {
        return s.data_size();
    }
    
    const_byte_pointer bytes(const u8string& s) const {
        return reinterpret_cast<const_byte_pointer>(s.data());
    }
};

template <class Iterator>
struct u8string_iterator_access_traits : access_traits_base {
    typedef typename Iterator::iterator_type iterator_base_type;

    size_t size(const Iterator& first, const Iterator& last) {
        return std::distance(first.base(), last.base());
    }
    
    const_byte_pointer bytes(const Iterator& i) const {
        return reinterpret_cast<const_byte_pointer>(&i.base()[0]);
    }
    
    iterator_base_type base(const Iterator& i) {
        return i.base();
    }
    
    template <class... Args>
    Iterator ranged_iterator(Args... args) {
        return Iterator{std::forward<Args>(args)...};
    }
};

template<>
struct iterator_access_traits<u8string::iterator> : u8string_iterator_access_traits<u8string::iterator> {
};

template<>
struct iterator_access_traits<u8string::const_iterator> : u8string_iterator_access_traits<u8string::const_iterator> {
};

} // prosoft

// std:: specializations

#include <functional>

namespace std {
template <>
struct hash<prosoft::u8string> {
    typedef prosoft::u8string argument_type;
    typedef std::size_t result_type;
    result_type operator()(const argument_type& s) const PS_NOEXCEPT {
        return hash<prosoft::u8string::data_type>()(s.data());
    };
};

template <>
struct equal_to<prosoft::u8string> {
    typedef prosoft::u8string first_argument_type;
    typedef prosoft::u8string second_argument_type;
    typedef bool result_type;
    result_type operator()(const first_argument_type& s1, const second_argument_type& s2) const {
        return (s1 == s2);
    }
};
}

#endif // PS_CORE_U8STRING_HPP
