// Copyright © 2014-2022, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_REGEX_HPP
#define PS_CORE_REGEX_HPP

#include <prosoft/core/config/config_platform.h>

#include <exception>
#include <functional>
#include <locale>
#include <type_traits>
#include <utility>
#include <vector>

#include <prosoft/core/config/config.h>
#include <prosoft/core/include/byteorder.h>
#include <prosoft/core/include/uniform_access.hpp>

struct re_pattern_buffer; // private Onig type

namespace prosoft {

namespace regex_constants {

enum syntax_option_type {
    icase = 1 << 0,
    nosubs = 1 << 1,
#if not_available
    optimize = 1 << 2,
    collate = 1 << 3,
#endif
// One (and only one) of these six grammar flags needs to be set for the bitmask to have a valid value.
// std:: syntax options, ECMA (JavaScript) is the std:: default
#if not_available
    ECMAScript = 1 << 4,
#endif
    basic = 1 << 5,
    extended = 1 << 6,
#if not_available
    awk = 1 << 7,
#endif
    grep = 1 << 8,
    egrep = 1 << 9,

    // std:: extensions
    ruby = 0, // default syntax
    // Newlines are ignored.
    // If enabled not_bol may not have the intended effect, since "beginning of string" which will always match '^'.
    noendl = 1 << 31
};

PS_ENUM_BITMASK_OPS(syntax_option_type);

enum match_flag_type {
    match_default = 0,
    match_not_bol = 1 << 0,
    match_not_eol = 1 << 1,
#if not_available
    match_not_bow = 1 << 2,
    match_not_eow = 1 << 3,
#endif
#if notyet
    match_any = 1 << 4,
    match_not_null = 1 << 5,
    match_continuous = 1 << 6,
#endif
    match_prev_avail = 1 << 7, // ignore not_bol
};

PS_ENUM_BITMASK_OPS(match_flag_type);

enum error_type {
    error_onig = -1,
    error_ctype = 1,
    error_escape,
    error_backref,
    error_paren,
    error_brace,
    error_badbrace,
    error_range,
    error_space,
    error_badrepeat,
    error_complexity,
    error_stack
};

} // regex_constants

namespace iregex {

using Regex = struct re_pattern_buffer*;
using uchar = unsigned char;
enum class encoding {
    utf8,
    utf16,
    utf16le,
    utf16be
};

#if BYTE_ORDER == LITTLE_ENDIAN
inline PS_CONSTEXPR encoding utf16() { return encoding::utf16le; }
#elif BYTE_ORDER == BIG_ENDIAN
inline PS_CONSTEXPR encoding utf16() { return encoding::utf16be; }
#endif

} // iregex

class regex_error : public std::runtime_error {
public:
    explicit regex_error(regex_constants::error_type, int onigErr = -1);
    virtual ~regex_error() PS_NOEXCEPT{};

    regex_constants::error_type code() const PS_NOEXCEPT { return _code; }
    
    int engine_code() const PS_NOEXCEPT {
        return _onigErr;
    }
    
private:
    regex_constants::error_type _code;
    int _onigErr;
};

template <class CharT, class String, iregex::encoding Encoding>
struct basic_regex_traits {
    typedef CharT char_type;
    typedef String string_type;
    typedef std::locale locale_type;
    typedef std::ctype_base::mask char_class_type;

    // extensions
    typedef typename string_type::const_iterator string_iterator;

    static PS_CONSTEXPR iregex::encoding encoding() {
        return m_encoding != iregex::encoding::utf16 ? m_encoding : iregex::utf16();
    }

private:
    static PS_CONSTEXPR const auto m_encoding = Encoding;
};

template <class CharT, class String, iregex::encoding Encoding>
PS_CONSTEXPR const iregex::encoding basic_regex_traits<CharT, String, Encoding>::m_encoding;

template <class String>
using u8regex_traits = basic_regex_traits<char, String, iregex::encoding::utf8>;
template <class String>
using u16regex_traits = basic_regex_traits<char16_t, String, iregex::encoding::utf16>;

template <class Traits>
class basic_regex {
public:
    typedef regex_constants::syntax_option_type flag_type;
    typedef Traits traits_type;
    typedef typename traits_type::locale_type locale_type;
    typedef typename traits_type::string_type string_type;
    typedef typename traits_type::char_type value_type;

    basic_regex() PS_NOEXCEPT : mPattern(), mFlags(default_flags), mRx(nullptr) {}
    basic_regex(const basic_regex& other)
        : basic_regex() {
        basic_regex tmp(other.mPattern, other.flags());
        swap(tmp);
    }
    basic_regex(basic_regex&& other) PS_NOEXCEPT : basic_regex() {
        swap(other);
    }
    basic_regex(const string_type& s, flag_type flags)
        : mPattern(s)
        , mFlags(flags)
        , mRx(nullptr) {
        compile();
    }

    ~basic_regex() {
        clear();
    }

    void swap(basic_regex&) PS_NOEXCEPT;

    basic_regex& assign(const basic_regex& other) {
        basic_regex tmp(other.mPattern, other.flags());
        swap(tmp);
        return *this;
    }

    basic_regex& assign(basic_regex&& other) PS_NOEXCEPT {
        clear();
        swap(other);
        return *this;
    }

    basic_regex& assign(const string_type&, flag_type flags = default_flags) {
        basic_regex tmp(mPattern, flags);
        swap(tmp);
        return *this;
    }

    basic_regex& operator=(const basic_regex& other) {
        return assign(other);
    }
    basic_regex& operator=(basic_regex&& other) PS_NOEXCEPT {
        return assign(std::move(other));
    }
    basic_regex& operator=(const string_type& other) {
        return assign(other);
    }

    flag_type flags() const { return mFlags; }

    // these are nops currently
    unsigned mark_count() const { return 0; }
    locale_type getloc() { return std::locale::classic(); }
    locale_type imbue(locale_type) { return getloc(); }

    static PS_CONSTEXPR const regex_constants::syntax_option_type icase = regex_constants::syntax_option_type::icase;
    static PS_CONSTEXPR const regex_constants::syntax_option_type nosubs = regex_constants::syntax_option_type::nosubs;

    // extensions to std::regex API
    basic_regex(string_type&& s, flag_type flags = default_flags)
        : mPattern(std::move(s))
        , mFlags(flags)
        , mRx(nullptr) {
        compile();
    }

    basic_regex& assign(string_type&& pattern, flag_type flags = default_flags) {
        basic_regex tmp(std::move(pattern), flags);
        swap(tmp);
        return *this;
    }

    basic_regex& operator=(string_type&& s) {
        return assign(std::move(s));
    }

    static string_type escaped_pattern(const string_type&);

    iregex::Regex crx() const { return mRx; }
    const string_type& pattern() const { return mPattern; }

private:
    static PS_CONSTEXPR const flag_type default_flags = flag_type::ruby;

    string_type mPattern;
    flag_type mFlags;
    iregex::Regex mRx;

    void clear();
    void compile();
};

template <class Traits>
PS_CONSTEXPR const regex_constants::syntax_option_type basic_regex<Traits>::icase;
template <class Traits>
PS_CONSTEXPR const regex_constants::syntax_option_type basic_regex<Traits>::nosubs;
template <class Traits>
PS_CONSTEXPR const regex_constants::syntax_option_type basic_regex<Traits>::default_flags;

namespace iregex {

extern void compile(Regex*, const uchar*, const uchar*, regex_constants::syntax_option_type, encoding);

template <class Traits>
inline void compile(Regex* r, const typename Traits::string_type& pattern, regex_constants::syntax_option_type flags) {
    const auto start = reinterpret_cast<const uchar*>(bytes(pattern));
    const auto end = start + byte_size(pattern);
    compile(r, start, end, flags, Traits::encoding());
}

void free(Regex);

} // iregex

template <class Traits>
void basic_regex<Traits>::swap(basic_regex& other) PS_NOEXCEPT {
    using std::swap;
    swap(mFlags, other.mFlags);
    swap(mRx, other.mRx);
    swap(mPattern, other.mPattern);
}

template <class Traits>
void basic_regex<Traits>::clear() {
    iregex::free(mRx);
    mRx = nullptr;
    mPattern.clear();
    mFlags = default_flags;
}

template <class Traits>
inline void basic_regex<Traits>::compile() {
    iregex::compile<Traits>(&mRx, mPattern, mFlags);
}

template <class Traits>
typename basic_regex<Traits>::string_type basic_regex<Traits>::escaped_pattern(const string_type& pattern) {
    string_type escaped;
    escaped.reserve(data_size(pattern));
    for (auto c : pattern) {
        switch (c) {
            case '\\':
            case '^':
            case '$':
            case '.':
            case '?':
            case '*':
            case '+':
            case '|':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}': {
                escaped.push_back(string_type::traits_type::to_char_type('\\'));
            }
            // fall through

            default:
                escaped.push_back(c);
                break;
        }
    }
    return escaped;
}

template <class Traits>
inline void swap(basic_regex<Traits>& lhs, basic_regex<Traits>& rhs) {
    lhs.swap(rhs);
}

// XXX: not necessarily equivalent to std:: types in design nor functionality.
template <class Traits>
using sub_match = typename Traits::string_type;

template <class Traits>
class basic_regex_iterator; // forward

template <class Traits>
class basic_match_results {
public:
    typedef sub_match<Traits> value_type;
    typedef typename Traits::string_type string_type;

    const value_type& operator[](size_t n) const { // [0] is match, [1-N] are captures
        return _results.at(n).second; // will throw a range exception if n is invalid
    }

    bool empty() const { return _results.empty(); }
    size_t size() const { return _results.size(); }

    const string_type& str(size_t n = 0) const { return operator[](n); }
    size_t length(size_t n = 0) const { return operator[](n).length(); }

    // extensions
    void clear() { _results.clear(); } // XXX: all sub match references will be invalidated

    template <typename... Args>
    void emplace_back(Args&&... args) {
        _results.emplace_back(std::forward<Args>(args)...);
    }

private:
    friend class basic_regex_iterator<Traits>;
    size_t position(size_t n = 0) const { return _results[n].first; } // XXX: raw byte position in underlying string

    typedef std::vector<std::pair<size_t, value_type>> results_type;
    results_type _results;
};

template <class Traits>
class basic_regex_iterator {
    typedef typename Traits::string_iterator input_iterator;

public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = basic_match_results<Traits>;
    using difference_type = std::ptrdiff_t;
    using pointer = basic_match_results<Traits>*;
    using reference = basic_match_results<Traits>&;

    typedef basic_regex<Traits> regex_type;

    basic_regex_iterator() PS_NOEXCEPT : _first(), _last(), _flags(regex_constants::match_default), _rx(nullptr), _results() {}
    basic_regex_iterator(input_iterator first, input_iterator last, const regex_type& rx, regex_constants::match_flag_type flags = regex_constants::match_default)
        : _first(first)
        , _last(last)
        , _flags(flags)
        , _rx(&rx)
        , _results() {
        (void)regex_search(_first, _last, _results, *_rx, _flags);
    }
    PS_DEFAULT_COPY(basic_regex_iterator);
    PS_DEFAULT_MOVE(basic_regex_iterator);

    bool operator==(const basic_regex_iterator&) const;

    const value_type& operator*() const PS_NOEXCEPT { return _results; }
    const value_type* operator->() const PS_NOEXCEPT { return &_results; }

    basic_regex_iterator& operator++(); // XXX: invalidates any previous values
    basic_regex_iterator operator++(int) { // ditto
        auto i = *this;
        ++(*this);
        return i;
    }

private:
    input_iterator _first, _last;
    regex_constants::match_flag_type _flags;
    const regex_type* _rx;
    value_type _results;
};

template <class Traits>
basic_regex_iterator<Traits>& basic_regex_iterator<Traits>::operator++() {
    if (!_results.empty()) {
        auto offset = _results.position() + data_size(_results.str());
        auto base = prosoft::base(_first) + offset;
        if (base < prosoft::base(_last)) {
            auto start = make_ranged_iterator<input_iterator>(base, prosoft::base(_first), prosoft::base(_last));
            if (regex_search(start, _last, _results, *_rx, _flags | regex_constants::match_prev_avail)) {
                // XXX: our results positions will be offsets from start not _first, so we need to fix them up.
                for (auto& p : _results._results) {
                    p.first += offset;
                }
            }
        } else {
            _results.clear();
        }
    }
    return *this;
}

template <class Traits>
bool basic_regex_iterator<Traits>::operator==(const basic_regex_iterator& other) const {
    if (_results.empty() && other._results.empty()) {
        return true;
    }
    if (_results.empty() || other._results.empty()) {
        return false;
    }
    return _flags == other._flags && _first == other._first && _last == other._last && _rx == other._rx && _results[0] == other._results[0];
}

template <class Traits>
inline bool operator!=(const basic_regex_iterator<Traits>& lhs, const basic_regex_iterator<Traits>& rhs) {
    return !lhs.operator==(rhs);
}

namespace iregex {

typedef std::function<void(size_t pos, size_t length)> match_handler;
extern bool rsearch(Regex, const uchar*, size_t length, regex_constants::match_flag_type, match_handler, bool exact);

template <class Traits>
inline bool search(Regex crx, typename Traits::string_iterator first, typename Traits::string_iterator last, regex_constants::match_flag_type flags, match_handler handler, bool exact = false) {
    const auto p = reinterpret_cast<const uchar*>(bytes(first, last));
    const auto byteCount = byte_size(first, last);
    return rsearch(crx, p, byteCount, flags, handler, exact);
}

using range = std::pair<size_t, size_t>;
template <class Traits>
inline range make_range(size_t pos, size_t len) {
    return {pos / sizeof(typename Traits::char_type), len / sizeof(typename Traits::char_type)};
}

template <class Iterator>
using iterator_range = std::pair<Iterator, Iterator>;

template <class Iterator>
inline iterator_range<Iterator> make_range(Iterator first, Iterator last, const range& r) {
    return {
        make_ranged_iterator<Iterator>(base(first) + r.first, base(first), base(last)),
        make_ranged_iterator<Iterator>(base(first) + r.first + r.second, base(first), base(last))
    };
}

} // iregex

template <class Traits>
bool regex_match(typename Traits::string_iterator first, typename Traits::string_iterator last, const basic_regex<Traits>& rx, regex_constants::match_flag_type flags = regex_constants::match_default) {
    if (PS_UNEXPECTED(first == last)) {
        return false;
    }
    auto crx = rx.crx();
    return (nullptr != crx && iregex::search<Traits>(crx, first, last, flags, nullptr, true));
}

template <class Traits>
bool regex_match(typename Traits::string_iterator first, typename Traits::string_iterator last, basic_match_results<Traits>& results, const basic_regex<Traits>& rx, regex_constants::match_flag_type flags = regex_constants::match_default) {
    results.clear();

    if (PS_UNEXPECTED(first == last)) {
        return false;
    }

    auto crx = rx.crx();
    if (nullptr != crx) {
        (void)iregex::search<Traits>(crx, first, last, flags, [&](size_t bpos, size_t blength) {
            // u8string: fastest way to create a substring when we have existing iters
            const auto r = iregex::make_range<Traits>(bpos, blength);
            auto ir  = iregex::make_range(first, last, r);
            results.emplace_back(r.first, typename  Traits::string_type(ir.first, ir.second));
        }, true);
    }
    return (results.size() > 0);
}

template <class Traits>
inline bool regex_match(const typename Traits::string_type& s, const basic_regex<Traits>& rx, regex_constants::match_flag_type flags = regex_constants::match_default) {
    return regex_match(s.cbegin(), s.cend(), rx, flags);
}

template <class Traits>
inline bool regex_match(const typename Traits::string_type& s, basic_match_results<Traits>& results, const basic_regex<Traits>& rx, regex_constants::match_flag_type flags = regex_constants::match_default) {
    return regex_match(s.cbegin(), s.cend(), results, rx, flags);
}

template <class Traits>
bool regex_search(typename Traits::string_iterator first, typename Traits::string_iterator last, const basic_regex<Traits>& rx, regex_constants::match_flag_type flags = regex_constants::match_default) {
    if (PS_UNEXPECTED(first == last)) {
        return false;
    }
    auto crx = rx.crx();
    return (nullptr != crx && iregex::search<Traits>(crx, first, last, flags, nullptr));
}

template <class Traits>
bool regex_search(typename Traits::string_iterator first, typename Traits::string_iterator last, basic_match_results<Traits>& results, const basic_regex<Traits>& rx, regex_constants::match_flag_type flags = regex_constants::match_default) {
    results.clear();

    if (PS_UNEXPECTED(first == last)) {
        return false;
    }

    auto crx = rx.crx();
    if (nullptr != crx) {
        iregex::search<Traits>(crx, first, last, flags, [&](size_t bpos, size_t blength) {
            const auto r = iregex::make_range<Traits>(bpos, blength);
            auto ir  = iregex::make_range(first, last, r);
            results.emplace_back(r.first, typename  Traits::string_type(ir.first, ir.second));
        });
    }
    return (results.size() > 0);
}

template <class Traits>
inline bool regex_search(const typename Traits::string_type& s, const basic_regex<Traits>& rx, regex_constants::match_flag_type flags = regex_constants::match_default) {
    return regex_search(s.cbegin(), s.cend(), rx, flags);
}

template <class Traits>
inline bool regex_search(const typename Traits::string_type& s, basic_match_results<Traits>& results, const basic_regex<Traits>& rx, regex_constants::match_flag_type flags = regex_constants::match_default) {
    return regex_search(s.cbegin(), s.cend(), results, rx, flags);
}

// std:: names

template <class Traits>
using regex_iterator = basic_regex_iterator<Traits>;

template <class Traits>
using match_results = basic_match_results<Traits>;

} // prosoft

#endif // PS_CORE_REGEX_HPP
