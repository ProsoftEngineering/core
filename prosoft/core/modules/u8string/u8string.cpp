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

#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <cstdlib>

#include "utf8proc.h"

#include "u8string.hpp"

namespace prosoft {

namespace {

PS_CONSTEXPR const u8string::unicode_type nbounds = 0xffffffff; // value returned for invalid indexes -- u32 valid range is {0,0x7fffffff}
PS_CONSTEXPR const size_t seq_size = 8; // unicode codepoint sequence size -- 8 for alignment and NULL term

// NFC is more compact than NFD, and has a closer mapping of codepoints to the logical notion of a "character" (though not exact).
// NFD is the more accepted "modern" form.
// OSX prefers NFD but will accept NFC as input and convert to NFD internally. However output (e.g. from readdir()) is always NFD.
// Windows/Linux file API's prefer NFC and will likely fail in odd ways (such as filenames not matching) if given NFD.
//
// XXX: CASEFOLD is for icase comparison use only. It does not represent any case and is therefore not suitable for presentation to a user.
PS_CONSTEXPR const int uprecomposed = UTF8PROC_STABLE | UTF8PROC_COMPOSE;
PS_CONSTEXPR const int uprecomposed_icase = uprecomposed | UTF8PROC_CASEFOLD;
PS_CONSTEXPR const int udecomposed = UTF8PROC_STABLE | UTF8PROC_DECOMPOSE;
#if notyet
PS_CONSTEXPR const int udecomposed_icase = udecomposed | UTF8PROC_CASEFOLD;
#endif
// We normalize EVERYTHING so we have a stable representation for comparison and persistence.
// This should NEVER change unless a versioning scheme is made public for clients.
//
// We use NFC because of its closer mapping to characters. This is important with any method that takes a "char" position,
// as clients would have to be aware of decomposed code points to avoid breaking decomposed characters by inserting/erasing between
// a base code point and its combining code point. It's much easier and less fragile to have clients think of the string in char terms.
// It's also much easier for clients on Windows/Linux to not have to remember to convert to NFC for file API use.
PS_CONSTEXPR const int stable_normalization = uprecomposed;
PS_CONSTEXPR const int stable_icase_normalization = uprecomposed_icase;

std::string normalize(const char* s, size_t nbytes = 0, int options = stable_normalization) {
    uint8_t* normalized = nullptr;
    if (0 == nbytes) {
        options |= UTF8PROC_NULLTERM;
    }

    utf8proc_ssize_t result = ::utf8proc_map(as_utf8(s), nbytes, &normalized, static_cast<utf8proc_option_t>(options));
    if (nullptr != normalized) {
        PSASSERT(result >= 0, "BUG");
        std::string u8(reinterpret_cast<const char*>(normalized), result);
        std::free(normalized);
        return u8;
    }

    if (UTF8PROC_ERROR_NOMEM == result) {
        throw std::bad_alloc();
    }

    PSASSERT(UTF8PROC_ERROR_INVALIDUTF8 == result, "Broken assumption");
    throw u8string::invalid_utf8(0);
}

utf8proc_ssize_t normalize(u8string::unicode_type c, int32_t* buf, utf8proc_ssize_t len, int options = stable_normalization) {
    PSASSERT(0 == (options & UTF8PROC_NULLTERM), "BUG. Invalid option.");

    utf8proc_ssize_t bytes = utf8proc_decompose_char(c, buf, len, static_cast<utf8proc_option_t>(options), nullptr);
    if (PS_UNEXPECTED(bytes > len)) {
        PSASSERT_UNREACHABLE("Broken assumption");
        throw std::runtime_error("UTF decomp buffer overflow");
    } else if (PS_UNEXPECTED(bytes < 0)) {
        if (UTF8PROC_ERROR_NOMEM == bytes) {
            throw std::bad_alloc();
        }
        throw std::runtime_error("Unknown UTF decomp error");
    }
    PSASSERT(bytes >= 0, "Broken assumption");
    return bytes;
};

inline std::string normalize(const std::string& s) {
    return normalize(s.data(), s.size());
}

inline bool _is_ascii(u8string::unicode_type c) {
    return (c <= 127);
}

inline bool is_combining_codepoint(u8string::unicode_type c) {
    const utf8proc_property_t* p = ::utf8proc_get_property(c);
    return (p->combining_class > 0);
}

template <typename octet_iterator>
octet_iterator find_invalid(octet_iterator start, octet_iterator end, bool& ascii, bool* normalized = nullptr) {
    ascii = false;

    size_t count = 0, acount = 0, nfdcount = 0;

    octet_iterator result = start;
    while (result != end) {
        try {
            uint32_t c = utf8::next(result, end);
            ++count;
            if (_is_ascii(c)) {
                ++acount;
            }
            if (is_combining_codepoint(c)) {
                ++nfdcount;
            }
        } catch (const std::exception&) {
            return result;
        }
    }

    ascii = count == acount;
    if (nullptr != normalized) {
        *normalized = 0 == nfdcount;
    }
    return result;
}

template <typename Iter>
inline void advance(Iter& i, u8string::size_type pos, u8string::size_type max) {
    std::advance(i, std::min(pos, max));
}

template <typename Iter>
void advance(Iter& begin, Iter& end, u8string::size_type pos, u8string::size_type len, u8string::size_type max) {
    std::advance(begin, pos);
    if (u8string::npos != len && (pos + len) <= max) {
        end = begin;
        std::advance(end, len);
    }
}

template <typename Iter>
void advance(Iter& begin, const Iter& end, u8string::size_type distance) {
    for (u8string::size_type i = 0; i < distance && begin != end; ++i) {
        std::advance(begin, 1);
    }
}

// this avoids having to decode the stream a second time just to get a count
template <typename Iter>
inline typename std::iterator_traits<Iter>::difference_type udistance(Iter first, Iter last) {
#if DEBUG
    typename std::iterator_traits<Iter>::difference_type d;
    PSASSERT((d = std::distance(first, last)) == (last.movement() - first.movement()), "BUG");
#endif
    return last.movement() - first.movement();
}

// Iter validation
#if DEBUG
template <typename Iter>
inline void PS_U8_ASSERT_ITER(const Iter& i, const u8string& s) {
    PSASSERT(i.base() >= s.begin().base() && i.base() <= s.end().base(), "BUG: invalid iter");
}
template <typename Iter>
inline void PS_U8_ASSERT_ITER_RANGE(const Iter& first, const Iter& last, const u8string& s) {
    PS_U8_ASSERT_ITER<Iter>(first, s);
    PS_U8_ASSERT_ITER<Iter>(last, s);
}
template <typename Iter>
inline void PS_U8_ASSERT_ITER__FWD_RANGE(const Iter& first, const Iter& last, const u8string& s) {
    PS_U8_ASSERT_ITER_RANGE(first, last, s);
    PSASSERT(first.base() <= last.base(), "BUG: invalid fwd iter");
}
#else
#define PS_U8_ASSERT_ITER(i, s)
#define PS_U8_ASSERT_ITER_RANGE(i, j, s)
#define PS_U8_ASSERT_ITER__FWD_RANGE(i, j, s)
#endif

// find predicates
struct is_equal_pre_normalized : public std::unary_function<u8string::unicode_type, bool> {
    // XXX: Assumes c1 and c2 are of the same normalization. u8string enforces normalization for all data, but external data does not.
    bool operator()(argument_type c1, argument_type c2) {
        return c1 == c2;
    }
};

struct is_equal_icase : public std::unary_function<u8string::unicode_type, bool> {
    bool operator()(argument_type c1, argument_type c2) {
        return 0 == u8string::compare(c1, c2, true);
    }
};

} // anon

void u8string::_init(const std::string& other) {
    bool normalized = false;
    auto i = find_invalid(other.begin(), other.end(), _u8._ascii, &normalized);
    if (i == other.end()) {
        _u8._s = ascii() || normalized ? other : normalize(other); // avoid conversion for ascii (which should be the most common case)
    } else {
        throw invalid_utf8(*i);
    }
}

u8string::u8string(const_iterator& start, const_iterator& fin)
    // XXX: The iters are external; thus we cannot assume that movement() is correct for either. Therefore we don't cache the length here.
    : u8string(std::string(start.base(), fin.base()), npos, false) {
}

// Unlike std::, utf8 does not provide conversion from non-const to const iters.
u8string::u8string(iterator& start, iterator& fin)
    : u8string(std::string(start.base(), fin.base()), npos, false) {
}

u8string::u8string(std::string&& other, size_type count, bool ASCII) {
    PSASSERT((ASCII ? is_ascii(other) : is_valid(other)), "Broken assumption");
    // XXX: other is assumed to be a copy/substr of our container string, so we don't normalize. DO NOT BREAK THIS ASSUMPTION.
    _u8._s = std::move(other);
    _u8._ct = count;
    _u8._ascii = ASCII ? ASCII : is_ascii(_u8._s);
}

u8string::u8string(const char* other, size_type len) {
    if (PS_UNEXPECTED(nullptr == other)) {
        throw std::invalid_argument("u8string NULL");
        __builtin_unreachable();
    }
    if (0 == len) {
        len = std::strlen(other);
    }
    bool normalized;
    auto i = find_invalid(other, other + len, _u8._ascii, &normalized);
    if (i == (other + len)) {
        if (ascii() || normalized) { // avoid conversion for ascii (which should be the most common case)
            _u8._s.assign(other, len);
        } else {
            _u8._s = normalize(other, len);
        }
    } else {
        throw invalid_utf8(*i);
    }
}

u8string::u8string(const u16string& other) {
    utf8::utf16to8(other.begin(), other.end(), std::back_inserter(_u8._s));
    _u8._s = normalize(_u8._s);
}

u8string::u8string(const u32string& other) {
    utf8::utf32to8(other.begin(), other.end(), std::back_inserter(_u8._s));
    _u8._s = normalize(_u8._s);
}

u8string::u8string(u16string::const_pointer other, size_type len) {
    if (PS_UNEXPECTED(nullptr == other)) {
        throw std::invalid_argument("u8string NULL");
        __builtin_unreachable();
    }
    if (0 == len) {
        len = u16string::traits_type{}.length(other);
    }
    utf8::utf16to8(other, other + len, std::back_inserter(_u8._s));
    _u8._s = normalize(_u8._s);
}

u8string::u8string(const value_type* other, size_type len) {
    if (PS_UNEXPECTED(nullptr == other)) {
        throw std::invalid_argument("u8string NULL");
        __builtin_unreachable();
    }
    if (0 == len) {
        len = u32string::traits_type{}.length(other);
    }
    utf8::utf32to8(other, other + len, std::back_inserter(_u8._s));
    _u8._s = normalize(_u8._s);
}

const u8string& u8string::operator=(const u16string& other) {
    *this = u8string(other);
    return *this;
}

void u8string::append(const u8string& other) {
    if (ascii() && other.ascii()) {
        _u8._s.append(other.str());
        _u8._ct = str().length();
    } else if (!other.empty()) {
        _u8._ascii = false;
        _invalidate_cache();
        _u8._s.append(other._u8._s);
        // XXX: this is necessary to handle the corner case of individual decomposed code points being combined to form a full precomposed codepoint.
        if (is_combining_codepoint(*other.cbegin())) {
            _u8._s = normalize(_u8._s);
        }
    }
}

void u8string::push_back(value_type c) {
    if (ascii() && _is_ascii(c)) {
        _u8._s.push_back(static_cast<container_type::value_type>(c));
        _u8._ct = str().length();
    } else {
        u8string tmp(u32string(1, c));
        append(tmp);
    }
}

// XXX: as of C++11 std::string::erase() and replace() take const_iterator; however GCC < 4.9.0 wrongly declares them with non-const iterator.
// And while conversion from non-const to const is allowed, const to non-const is not allowed.

void u8string::pop_back() {
    if (length() <= 1) {
        clear();
        return;
    }
    if (ascii() || _is_ascii(_u8._s.back())) {
        _u8._s.pop_back();
    } else {
        auto fin = end();
        auto start = fin;
        --start;
        (void)_u8._s.erase(start.base(), fin.base());
    }
    --_u8._ct;
}

u8string::iterator u8string::erase(iterator start, iterator fin) {
    PS_U8_ASSERT_ITER__FWD_RANGE(start, fin, *this);

    auto where = _u8._s.erase(start.base(), fin.base());
    if (ascii()) {
        _u8._ct = str().length();
    } else {
        _invalidate_cache();
    }
    return make_iterator(where);
}

u8string& u8string::erase(size_type pos, size_type len) {
    if (ascii()) {
        _u8._s.erase(pos, len);
        _u8._ct = str().length();
    } else {
        auto i = begin();
        auto j = end();
        advance(i, j, pos, len, length());

        _invalidate_cache();
        (void)_u8._s.erase(i.base(), j.base());
    }
    return *this;
}

u8string::iterator u8string::insert(iterator i, value_type c) {
    if (ascii() && _is_ascii(c)) {
        auto where = _u8._s.insert(i.base(), static_cast<container_type::value_type>(c));
        _u8._ct = str().length();
        return make_iterator(where);
    } else {
        u8string tmp(u32string(1, c));
        return insert(i, tmp.cbegin(), tmp.cend());
    }
}

u8string::iterator u8string::insert(iterator i, const_iterator start, const_iterator fin) {
    PS_U8_ASSERT_ITER(i, *this);

    PSASSERT(!is_combining_codepoint(*start) || i != begin(), "stray combining codepoint -- are you sure want this?");

    const bool emptySequence = start == fin;
// Neither GCC 4.x nor MSVC 2013 return an iterator as required by C++11.
#if !__ps_complete_cpp11_stdlib
    const auto bytePos = i.base() - _u8._s.begin();
#else
    auto where =
#endif
    _u8._s.insert(i.base(), start.base(), fin.base());
#if !__ps_complete_cpp11_stdlib
    auto where = _u8._s.begin() + bytePos;
    PSASSERT(emptySequence || *where == *(start.base()), "Broken assumption");
#endif

    _u8._ascii = false; // XXX: we don't know if the new sequence is ASCII or not so we have to assume not.
    _invalidate_cache();
    // XXX: this is necessary to handle the corner case of individual decomposed code points being combined to form a full precomposed codepoint.
    if (PS_UNEXPECTED(!emptySequence && is_combining_codepoint(*start))) {
        // normalize will combine *start with the codepoint preceding i to form a new precomposed codepoint.
        // We'll return an iterator to this new cp. XXX: normalize() invalidates where
        i = make_iterator(where); // points to the combining cp
        --i;
        const auto pos = i.base() - begin().base(); // distance to the cp preceding the combining cp
        _u8._s = normalize(_u8._s);
        // XXX: we are assuming the codepoint+combining codepoint forms a new precomposed codepoint -- this is not 100% guaranteed.
        // It's possible to have a sequence of combining codepoints that cannot be composed to form a new codepoint but can be composed to form a grapheme.
        // However, this should still point us to the expected position.
        where = _u8._s.begin() + pos;
    }
    return make_iterator(where);
}

u8string& u8string::insert(size_type pos, const u8string& other) {
    if (ascii() && other.ascii()) {
        _u8._s.insert(pos, other.str());
        _u8._ct = str().length();
    } else {
        auto len = length();
        if (PS_UNEXPECTED(pos > len)) {
            throw std::out_of_range("u8string insert");
        }

        iterator i;
        if (pos < len) {
            i = begin();
            advance(i, pos, len);
        } else {
            i = end();
        }
        (void)insert(i, other.cbegin(), other.cend());
    }
    return *this;
}

u8string& u8string::replace(iterator start, iterator fin, const_iterator first, const_iterator last) {
    PS_U8_ASSERT_ITER__FWD_RANGE(start, fin, *this);

    (void)_u8._s.replace(start.base(), fin.base(), first.base(), last.base());
    _invalidate_cache();
    _u8._ascii = false;
    // XXX: technically this could turn us into ASCII, but we don't worry about it right now
    return *this;
}

u8string& u8string::replace(size_type pos, size_type len, const u8string& other) {
    auto max = length();
    if (pos == max) {
        append(other);
        return *this;
    }
    if (ascii() && other.ascii()) {
        _u8._s.replace(pos, len, other.str());
        _u8._ct = str().length();
    } else {
        if (PS_UNEXPECTED(pos > max)) {
            throw std::out_of_range("u8string replace");
        }
        auto start = begin();
        advance(start, pos, max);
        iterator fin;
        if (len < npos && (pos + len) < max) {
            fin = start;
            advance(fin, len, max);
        } else {
            fin = end();
        }
        (void)replace(start, fin, other);
    }
    return *this;
}

int u8string::compare(const u8string& other, bool icase) const {
    return compare(0, npos, other, 0, npos, icase);
}

int u8string::compare(size_type pos, size_type count, const u8string& other, bool icase) const {
    return compare(pos, count, other, 0, npos, icase);
}

int u8string::compare(size_type pos, size_type count, const u8string& other, size_type pos2, size_type count2, bool icase) const {
    int retval;
    if (!icase && ascii() && other.ascii()) {
        retval = str().compare(pos, count, other.str(), pos2, count2);
    } else {
        auto i = cbegin();
        std::advance(i, pos); // XXX: expensive

        auto j = other.cbegin();
        std::advance(j, pos2); // XXX: expensive

        auto stop = npos == count ? cend() : i;
        if (npos != count) {
            advance(stop, cend(), count);
        }

        auto ostop = npos == count2 ? other.cend() : j;
        if (npos != count2) {
            advance(ostop, other.cend(), count2);
        }

        retval = 0; // if both strings are empty return equality
        for (; i != stop && j != ostop; ++i, ++j) {
            if (0 != (retval = compare(*i, *j, icase))) {
                break;
            }
        }

        if (0 == retval) {
            PSASSERT(i == stop || j == ostop, "BUG");
            if (i != stop || j != ostop) {
                retval = i == stop ? -1 : 1;
            }
        }
    }
    return retval;
}

int u8string::compare(unicode_type c1, unicode_type c2, bool icase) {
    if (!icase && _is_ascii(c1) && _is_ascii(c2)) { // ASCII case-compare shortcut
        return (c1 == c2 ? 0 : (c1 > c2 ? 1 : -1));
    }

    if (PS_UNEXPECTED(!is_valid(c1) || !is_valid(c2))) {
        return -1;
    }

    int options = !icase ? stable_normalization : stable_icase_normalization;

    int32_t u1[seq_size];
    auto len1 = normalize(c1, u1, sizeof(u1), options);

    int32_t u2[seq_size];
    auto len2 = normalize(c2, u2, sizeof(u2), options);

    for (auto i = 0, j = 0; i < len1 && j < len2 && i < (int)seq_size; ++i, ++j) {
        if (u1[i] != u2[i]) {
            return u1[i] > u2[i] ? 1 : -1;
        }
    }
    return (len1 == len2 ? 0 : (len1 > len2 ? 1 : -1));
}

u8string::unicode_type u8string::operator[](u8string::size_type pos) const {
    if (pos < length()) {
        auto i = cbegin();
        std::advance(i, pos);
        return *i;
    }
    return nbounds;
}

u8string::size_type u8string::length() const {
    size_type count;
    if (PS_UNEXPECTED(npos == _u8._ct)) {
        _u8._ct = count = utf8::distance(_u8._s.cbegin(), _u8._s.cend());
    } else {
        count = _u8._ct;
        PSASSERT(count == (size_type)utf8::distance(_u8._s.cbegin(), _u8._s.cend()), "cached count is out-of-sync");
    }
    return count;
}

u8string u8string::substr(size_type pos, size_type len) const {
    const auto max = length();
    if (PS_UNEXPECTED(pos >= max)) {
        return u8string{};
    }

    if (len > max) {
        len = max;
    }
    if ((pos + len) > max) {
        len = max - pos;
    }

    if (ascii()) {
        return u8string(str().substr(pos, len), len, true);
    } else {
        auto start = cbegin();
        auto fin = cend();
        advance(start, fin, pos, len, max);
        return u8string(std::string(start.base(), fin.base()), len, false);
    }
}

bool u8string::is_ascii() const {
    return _u8._ascii;
}

bool u8string::has_bom() const {
    static_assert(sizeof(utf8::bom) >= 3, "broken assumption");
    auto start = _u8._s.cbegin();
    return (_u8._s.length() >= sizeof(utf8::bom) && utf8::starts_with_bom(start, start + sizeof(utf8::bom)));
}

u8string::size_type u8string::find(const u8string& other, size_type pos, find_options opts) const {
    if (ascii() && other.ascii() && opts == find_options::none) {
        return str().find(other.str(), pos);
    }

    auto i = cbegin();
    advance(i, pos, length());

    auto fin = cend();
    u8string::const_iterator where;
    if (opts == find_options::case_insensitive) {
        where = std::search(i, fin, other.cbegin(), other.cend(), is_equal_icase());
    } else {
        where = std::search(i, fin, other.cbegin(), other.cend(), is_equal_pre_normalized());
    }
    // Further tuning:
    // 2) could also optimize by caching the decoded code points so we don't have to decode with each loop iter
    return (where != fin ? (pos + udistance(i, where)) : npos);
}

u8string::size_type u8string::find(value_type c, size_type pos, find_options opts) const {
    if (ascii() && opts == find_options::none) {
        return str().find(static_cast<container_type::value_type>(c), pos);
    }

    auto i = cbegin();
    advance(i, pos, length());

    auto fin = cend();
    for (; i != fin; ++i, ++pos) {
        if (0 == compare(*i, c, opts == find_options::case_insensitive)) { // have to use compare to make sure 'c' is normalized
            return pos;
        }
    }
    return npos;
}

u8string::size_type u8string::find(const std::string& s, size_type pos, find_options opts) const {
    return (is_valid(s) ? find(u8string(s), pos, opts) : npos);
}

u8string::size_type u8string::rfind(const u8string& other, size_type pos) const {
    if (ascii() && other.ascii()) {
        return str().rfind(other.str(), pos);
    }

    auto mylen = length();
    if (pos >= mylen) {
        pos = mylen - 1;
    }
    pos += other.length();

    auto start = cbegin();
    auto fin = start;
    advance(fin, pos, mylen);

    auto where = std::find_end(start, fin, other.cbegin(), other.cend(), is_equal_pre_normalized());
    return (where != fin ? udistance(start, where) : npos);
}

u8string::size_type u8string::rfind(value_type c, size_type pos) const {
    if (ascii()) {
        return str().rfind(static_cast<container_type::value_type>(c), pos);
    }

    auto mylen = length();
    if (pos >= mylen) {
        pos = mylen - 1;
    }

    auto i = cbegin();
    std::advance(i, pos);
    for (; pos > 0; --i, --pos) {
        if (0 == compare(*i, c)) { // have to use compare to make sure 'c' is normalized
            return pos;
        }
    }
    // XXX: We do the last test outside of the loop so we don't decrement i beyond it's range and throw an exception.
    PSASSERT(pos == 0, "Broken assumption");
    return (0 == compare(*i, c) ? pos : npos);
}

u8string::size_type u8string::rfind(const std::string& s, size_type pos) const {
    return (is_valid(s) ? rfind(u8string(s), pos) : npos);
}

u8string::size_type u8string::find_first_of(const u8string& other, size_type pos) const {
    if (ascii() && other.ascii()) {
        return str().find_first_of(other.str(), pos);
    }

    auto i = cbegin();
    advance(i, pos, length());
    
    auto fin = cend();
    const_iterator where = std::find_first_of(i, fin, other.cbegin(), other.cend(), is_equal_pre_normalized());
    // Further tuning:
    // 2) could also optimze by caching the decoded code points so we don't have to decode with each loop iter
    return (where != fin ? (pos + udistance(i, where)) : npos);
}

u8string::size_type u8string::find_first_of(value_type c, size_type pos) const {
    return find(c, pos);
}

u8string::size_type u8string::find_first_of(const std::string& s, size_type pos) const {
    return (is_valid(s) ? find_first_of(u8string(s), pos) : npos);
}

u8string::size_type u8string::find_last_of(const u8string& other, size_type pos) const {
    if (ascii() && other.ascii()) {
        return str().find_last_of(other.str(), pos);
    }

    auto mylen = length();
    if (pos >= mylen) {
        pos = mylen - 1;
    }
    ++pos; // XXX: need to move 1 beyond the wanted position for the reverse iter below

    auto i = cbegin();
    std::advance(i, pos);
    auto start = const_reverse_iterator(i);
    auto fin = crend();

    auto where = std::find_first_of(start, fin, other.cbegin(), other.cend(), is_equal_pre_normalized());
    // XXX: re-adjust pos and reverse the distance iters to account for our backwards movement
    return (where != fin ? ((pos - 1) - udistance(where.base(), i)) : npos);
}

u8string::size_type u8string::find_last_of(value_type c, size_type pos) const {
    return rfind(c, pos);
}

// type conversions -- more expensive
u8string::size_type u8string::find_last_of(const std::string& s, size_type pos) const {
    return (is_valid(s) ? find_last_of(u8string(s), pos) : npos);
}

bool u8string::is_valid(const char* s, bool* ascii) {
    auto len = std::strlen(s);
    bool a;
    auto i = find_invalid(s, s + len, a);
    if (nullptr != ascii) {
        *ascii = a;
    }
    return (i == (s + len));
}

bool u8string::is_valid(const std::string& s, bool* ascii) {
    bool a;
    auto i = find_invalid(s.begin(), s.end(), a);
    if (nullptr != ascii) {
        *ascii = a;
    }
    return (i == s.end());
}

bool u8string::is_valid(unicode_type c, bool* ascii) {
    static_assert(std::is_unsigned<unicode_type>::value && sizeof(unicode_type) == sizeof(uint32_t), "broken assumption");
    if (nullptr != ascii) {
        *ascii = _is_ascii(c);
    }
    return ::utf8proc_codepoint_valid(c);
}

bool u8string::is_ascii(const std::string& s) // optimized for UTF
{
    bool a = true;
    for (signed char c : s) {
        if (c < 0) {
            a = false;
            break;
        }
    }
    return a;
}

// ==

void u8string::_store::swap(u8string::_store& other) {
    _s.swap(other._s);
    auto c = _ct.exchange(other._ct);
    other._ct = c;

    using std::swap;
    swap(_ascii, other._ascii);
}

void u8string::_store::clear() {
    _s.clear();
    _ct = 0;
    _ascii = true;
}

void u8string::_store::invalidate() {
    // _ct will get updated in u8string::length()
    _ct = npos;
}

// ==

std::string precomposed(const u8string& us) {
    static_assert(stable_normalization == uprecomposed, "Broken assumption");
    return {us.str()};
}

std::string decomposed(const u8string& us) {
    return normalize(us.c_str(), us.data_size(), udecomposed);
}

namespace unicode {

u16string u16(const u8string& us) {
    u16string buf;
    // XXX: unchecked assumes we have valid UTF8 -- this is an implementation detail so our method is not marked as noexcept()
    utf8::unchecked::utf8to16(us.str().cbegin(), us.str().cend(), std::back_inserter(buf));
    return buf;
}

u32string u32(const u8string& us) {
    u32string buf;
    // XXX: unchecked assumes we have valid UTF8 -- this is an implementation detail so our method is not marked as noexcept()
    utf8::unchecked::utf8to32(us.str().cbegin(), us.str().cend(), std::back_inserter(buf));
    return buf;
}

u8string::unicode_type tolower(u8string::unicode_type c) {
    return ::utf8proc_tolower(c);
}

u8string::unicode_type toupper(u8string::unicode_type c) {
    return ::utf8proc_toupper(c);
}

} // unicode

namespace {
const u8string _bom(utf8::bom, sizeof(utf8::bom));
}
const u8string& u8string::bom = _bom;

const u8string::size_type u8string::npos = static_cast<u8string::size_type>(-1);

} // prosoft
