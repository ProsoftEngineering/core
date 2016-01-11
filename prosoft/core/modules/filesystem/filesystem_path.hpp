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

#ifndef PS_CORE_FILESYSTEM_PATH_HPP
#define PS_CORE_FILESYSTEM_PATH_HPP

#include <algorithm>
#include <iterator>
#include <locale>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <prosoft/core/config/config.h>
#include <prosoft/core/include/string/unicode_convert.hpp>

#include "path_utils.hpp"

namespace prosoft {
namespace filesystem {
inline namespace v1 {

template <class String>
class basic_path {
public:
    // Extensions //
    static constexpr const path_style preferred_separator_style =
#if !_WIN32
        path_style::posix;
#else
        path_style::windows;
#endif
    // Extensions //

    typedef String string_type;
    typedef typename string_type::value_type value_type;
    typedef typename string_type::const_pointer const_pointer;
    typedef typename std::remove_pointer<typename std::remove_const<const_pointer>::type>::type encoding_value_type; // for u8string this will be char
    static constexpr const value_type preferred_separator = (preferred_separator_style == path_style::posix ? static_cast<value_type>('/') : static_cast<value_type>('\\'));

    basic_path() noexcept(std::is_nothrow_default_constructible<string_type>::value);
    basic_path(const basic_path&);
    basic_path(basic_path&&) noexcept(std::is_nothrow_move_constructible<string_type>::value);
    template <class Source>
    basic_path(const Source&);
    template <class InputIterator>
    basic_path(InputIterator, InputIterator);
    /*
    template <class Source>
    basic_path(const Source&, const std::locale&);
    template <class InputIterator>
    basic_path(InputIterator, InputIterator, const std::locale&);
    */
    ~basic_path() = default;

    basic_path& operator=(const basic_path&);
    basic_path& operator=(basic_path&&) noexcept(std::is_nothrow_move_assignable<string_type>::value);
    template <class Source>
    basic_path& operator=(const Source&);
    template <class Source>
    basic_path& assign(const Source&);
    template <class InputIterator>
    basic_path& assign(InputIterator, InputIterator);

    basic_path& operator/=(const basic_path&);
    template <class Source>
    basic_path& operator/=(const Source&);
    template <class Source>
    basic_path& append(const Source&);
    template <class InputIterator>
    basic_path& append(InputIterator, InputIterator);

    basic_path& operator+=(const basic_path&);
    basic_path& operator+=(const string_type&);
    basic_path& operator+=(const_pointer);
    basic_path& operator+=(value_type);
    /*
    template <class Source>
    basic_path& operator+=(const Source&);
    template <class EcharT>
    basic_path& operator+=(EcharT);
    */
    template <class Source>
    basic_path& concat(const Source&);
    template <class InputIterator>
    basic_path& concat(InputIterator, InputIterator);

    void clear() noexcept(noexcept(std::declval<string_type>().clear()));
    basic_path& make_preferred();
    basic_path& remove_filename();
    basic_path& replace_filename(const basic_path&);
    basic_path& replace_extension(const basic_path& replacement = basic_path());
    void swap(basic_path&) noexcept(noexcept(std::swap(std::declval<string_type&>(), std::declval<string_type&>())));

    const string_type& native() const noexcept;
    const const_pointer c_str() const noexcept(noexcept(std::declval<string_type>().c_str()));
    operator string_type() const;

    /*
    template <class EcharT, class traits = std::char_traits<EcharT>, class Allocator = std::allocator<EcharT>>
    std::basic_stringbuf<EcharT, traits, Allocator> string(const Allocator& a = Allocator()) const;
    */
    std::string string() const;
    prosoft::u8string u8string() const;
    prosoft::u16string u16string() const;
    prosoft::u32string u32string() const;

    prosoft::u16string wstring() const { // XXX: not-spec
        return u16string();
    }

    /*
    template <class EcharT, class traits = std::char_traits<EcharT>, class Allocator = std::allocator<EcharT>>
    std::basic_stringbuf<EcharT, traits, Allocator> generic_string(const Allocator& a = Allocator()) const;
    */
    std::string generic_string() const;
    prosoft::u8string generic_u8string() const;
    prosoft::u16string generic_u16string() const;
    prosoft::u32string generic_u32string() const;

    prosoft::u16string generic_wstring() const { // XXX: not-spec
        return wstring();
    }

    int compare(const basic_path&) const noexcept(noexcept(std::declval<string_type>().compare(std::declval<basic_path>())));
    int compare(const string_type&) const;
    int compare(const const_pointer) const;

    basic_path root_name() const;
    basic_path root_directory() const;
    basic_path root_path() const;
    basic_path relative_path() const;
    basic_path parent_path() const;
    basic_path filename() const;
    basic_path stem() const;
    basic_path extension() const;

    bool empty() const noexcept(noexcept(std::declval<string_type>().empty()));
    bool has_root_name() const;
    bool has_root_directory() const;
    bool has_relative_path() const;
    bool has_parent_path() const;
    bool has_filename() const;
    bool has_stem() const;
    bool has_extension() const;
    bool is_absolute() const;
    bool is_relative() const;

    class iterator;
    typedef iterator const_iterator;

    iterator begin() const;
    iterator end() const;

    // Extensions //
    basic_path(string_type&& s) noexcept(std::is_nothrow_move_constructible<string_type>::value)
        : m_pathname(std::move(s)) {}
    // Extensions //

private:
    const string_type& raw() const& noexcept {
        return native();
    }
    string_type raw() && noexcept(std::is_nothrow_move_assignable<string_type>::value) {
        return std::move(m_pathname);
    }

    string_type m_pathname;
}; // path

// statics

template <class String>
constexpr const path_style basic_path<String>::preferred_separator_style;

template <class String>
constexpr const typename basic_path<String>::value_type basic_path<String>::preferred_separator;

// private

namespace ifilesystem {
template <class String, class Source>
struct to_string_type {
    String operator()(const Source&);
    String operator()(Source&&);
    template <class InputIterator>
    String operator()(InputIterator, InputIterator);
};

template <class Source>
struct to_string_type<u8string, Source> {
    using String = u8string;

    String operator()(const Source& source) {
        return String{source};
    }

    String operator()(Source&& source) {
        return String{std::move(source)};
    }

    template <class InputIterator>
    String operator()(InputIterator i1, InputIterator i2) {
        return String{std::string{i1, i2}};
    }
};

template <class Source>
struct to_string_type<std::string, Source> {
    using String = std::string;

    String operator()(const Source& source) {
        return String{source};
    }

    String operator()(Source&& source) {
        return String{std::move(source)};
    }

    template <class InputIterator>
    String operator()(InputIterator i1, InputIterator i2) {
        return String{i1, i2};
    }
};

template <class Source>
struct to_string_type<prosoft::u16string, Source> {
    using String = prosoft::u16string;

    String operator()(const Source& source) {
        return from_u8string<String>{}(u8string{source});
    }

    String operator()(Source&& source) {
        return String{std::move(source)};
    }

    template <class InputIterator>
    String operator()(InputIterator i1, InputIterator i2) {
        return String{i1, i2};
    }
};

template <>
struct to_string_type<prosoft::u16string, u8string> {
    using String = prosoft::u16string;
    using Source = u8string;

    String operator()(const Source& source) {
        return unicode::u16(source);
    }

    String operator()(Source&& source) {
        return unicode::u16(source);
    }

    String operator()(const char* source, size_t len = 0) {
        return operator()(u8string{source, len});
    }

    template <class InputIterator>
    String operator()(InputIterator i1, InputIterator i2) {
        return operator()(to_string_type<u8string, u8string>{}(i1, i2));
    }
};

// for format observer conversion
template <>
struct to_string_type<std::string, prosoft::u8string> {
    using String = std::string;
    using Source = u8string;

    String operator()(const Source& source) {
        return String{source.str()};
    }
};

template <>
struct to_string_type<std::string, prosoft::u16string> {
    using String = std::string;
    using Source = prosoft::u16string;

    String operator()(const Source& source) {
        return String{u8string{source}.str()};
    }
};

template <>
struct to_string_type<prosoft::u32string, prosoft::u8string> {
    using String = prosoft::u32string;
    using Source = u8string;

    String operator()(const Source& source) {
        return unicode::u32(source);
    }
};

template <>
struct to_string_type<prosoft::u32string, prosoft::u16string> {
    using String = prosoft::u32string;
    using Source = prosoft::u16string;

    String operator()(const Source& source) {
        return unicode::u32(u8string{source});
    }
};
}

// construct //

template <class String>
inline basic_path<String>::basic_path() noexcept(std::is_nothrow_default_constructible<string_type>::value)
    : m_pathname() {}

template <class String>
inline basic_path<String>::basic_path(const basic_path& other)
    : m_pathname(other.m_pathname) {}

template <class String>
inline basic_path<String>::basic_path(basic_path&& other) noexcept(std::is_nothrow_move_constructible<string_type>::value)
    : m_pathname(std::move(other.m_pathname)) {}

template <class String>
template <class Source>
inline basic_path<String>::basic_path(const Source& s)
    : m_pathname(ifilesystem::to_string_type<string_type, Source>{}(s)) {
}

template <class String>
template <class InputIterator>
inline basic_path<String>::basic_path(InputIterator i1, InputIterator i2)
    : m_pathname(ifilesystem::to_string_type<string_type, string_type>{}(i1, i2)) {
}

// assign //

template <class String>
basic_path<String>& basic_path<String>::operator=(const basic_path& other) {
    if (this != &other) {
        m_pathname = other.m_pathname;
    }
    return *this;
}

template <class String>
basic_path<String>& basic_path<String>::operator=(basic_path&& other) noexcept(std::is_nothrow_move_assignable<string_type>::value) {
    if (this != &other) {
        m_pathname = std::move(other.m_pathname);
    }
    return *this;
}

template <class String>
template <class Source>
inline basic_path<String>& basic_path<String>::operator=(const Source& s) {
    return assign(s);
}

template <class String>
template <class Source>
inline basic_path<String>& basic_path<String>::assign(const Source& s) {
    m_pathname = ifilesystem::to_string_type<string_type, Source>{}(s);
    return *this;
}

template <class String>
template <class InputIterator>
inline basic_path<String>& basic_path<String>::assign(InputIterator i1, InputIterator i2) {
    m_pathname = ifilesystem::to_string_type<string_type, string_type>{}(i1, i2);
    return *this;
}

// append //

template <class String>
basic_path<String>& basic_path<String>::operator/=(const basic_path& p) {
    if (this != &p) {
        filesystem::append(m_pathname, p.native(), preferred_separator_style);
    }
    return *this;
}

template <class String>
template <class Source>
inline basic_path<String>& basic_path<String>::operator/=(const Source& s) {
    return append(s);
}

template <class String>
template <class Source>
inline basic_path<String>& basic_path<String>::append(const Source& s) {
    filesystem::append(m_pathname, ifilesystem::to_string_type<string_type, Source>{}(s), preferred_separator_style);
    return *this;
}

template <class String>
template <class InputIterator>
inline basic_path<String>& basic_path<String>::append(InputIterator i1, InputIterator i2) {
    filesystem::append(m_pathname, ifilesystem::to_string_type<string_type, string_type>{}(i1, i2), preferred_separator_style);
    return *this;
}

// concat //
// The std makes no mention of separator eliding (as it does with append) so we assume this is just a straight concat.

template <class String>
inline basic_path<String>& basic_path<String>::operator+=(const basic_path& p) {
    m_pathname += p.native();
    return *this;
}

template <class String>
inline basic_path<String>& basic_path<String>::operator+=(const string_type& s) {
    m_pathname += s;
    return *this;
}

template <class String>
inline basic_path<String>& basic_path<String>::operator+=(const_pointer s) {
    m_pathname += string_type{s};
    return *this;
}

template <class String>
inline basic_path<String>& basic_path<String>::operator+=(value_type c) {
    m_pathname.push_back(c);
    return *this;
}

template <class String>
template <class Source>
inline basic_path<String>& basic_path<String>::concat(const Source& s) {
    m_pathname += ifilesystem::to_string_type<string_type, Source>{}(s);
    return *this;
}

template <class String>
template <class InputIterator>
inline basic_path<String>& basic_path<String>::concat(InputIterator i1, InputIterator i2) {
    m_pathname += ifilesystem::to_string_type<string_type, string_type>{}(i1, i2);
    return *this;
}

// iteration //

template <class String>
class basic_path<String>::iterator : public std::iterator<std::bidirectional_iterator_tag, basic_path<String>> {
    typedef typename String::const_iterator base_iterator;
    typedef typename String::const_reverse_iterator base_reverse_iterator;

public:
    using path_type = basic_path<String>;

    iterator() {}

    explicit iterator(const path_type& p)
        : mStart(p.native().begin())
        , mEnd(p.native().end())
        , mPos()
        , mRootName(mEnd) {
        if (path_type::preferred_separator_style != path_style::windows) {
            get_element(mStart, mEnd); // retrieve the 1st path element w/o additional processing of next_element()
            mPos = mStart;
        } else {
            const auto unc = ifilesystem::unc_prefix(p.native());
            if (!unc.empty()) {
                const auto uncSize = unc.size();
                mRootName = mStart;
                mStart = std::next(mStart, uncSize);
            }
            get_element(mStart, mEnd); // retrieve the 1st path element w/o additional processing of next_element()
            const auto& s = mElement.native();
            const bool hasDriveLetter = ifilesystem::starts_with_drive_letter(s);
            if (hasDriveLetter) {
                mElement = s.substr(0, 2); // extract the drive letter
            }

            const bool hasRootElement = hasDriveLetter || !unc.empty();
            if (hasRootElement) {
                if (mRootName == mEnd) {
                    mRootName = mStart;
                }
                mPos = mStart = std::next(mStart, mElement.native().size()); // reset for possible root dir
            }

            if (!unc.empty()) {
                mElement = unc + mElement.native(); // update element
            }

            mPos = hasRootElement ? mRootName : mStart; // reset to begin
        }
    }

    explicit iterator(const path_type& p, base_iterator pos)
        : iterator(p) {
        mPos = pos;
        mElement = path_type{};
    }

    iterator(const iterator&) = default;
    iterator& operator=(const iterator&) = default;
    iterator(iterator&&) = default;
    iterator& operator=(iterator&&) = default;
    const path_type& operator*() const { return mElement; }
    const path_type* operator->() const { return &operator*(); }

    bool operator==(const iterator& other) const {
        // can't use mStart as it will break comparison against end iterator
        return mEnd == other.mEnd && mPos == other.mPos;
    }

    bool operator!=(const iterator& other) const {
        return !operator==(other);
    }

    iterator& operator++() {
        next_element();
        return *this;
    }

    iterator operator++(int) {
        auto tmp = *this;
        next_element();
        return tmp;
    }

    iterator& operator--() {
        previous_element();
        return *this;
    }

    iterator operator--(int) {
        auto tmp = *this;
        previous_element();
        return tmp;
    }

private:
    path_type extract_element(base_iterator start, base_iterator fin) const {
        return path_type{start, fin};
    }

    path_type extract_rootname() const {
        return extract_element(mRootName, mStart);
    }

    void get_element(base_iterator start, base_iterator fin) {
        if (start != fin) {
            auto i = std::find(start, fin, path_type::preferred_separator);
            if (i != fin) {
                if (i == start) {
                    mElement = *i;
                } else {
                    mElement = extract_element(start, i);
                }
            } else {
                mElement = extract_element(start, fin);
            }
        } else {
            mElement.clear();
        }
    }

    bool is_separator(const base_iterator& i) {
        return (i != mEnd && *i == path_type::preferred_separator);
    }

    bool is_separator(const base_reverse_iterator& i) {
        return (i.base() != mStart && *i == path_type::preferred_separator);
    }

    template <class Iterator>
    size_t skip_separators(Iterator& i) {
        size_t count = 0;
        while (is_separator(i)) {
            ++i;
            ++count;
        }
        return count;
    }

    bool at_start(const base_iterator& i) const {
        return i == mStart;
    }

    bool has_rootname() const {
        return mRootName != mEnd;
    }

    bool at_rootname(const base_iterator& i) const {
        return has_rootname() && i == mRootName;
    }

    void next_element() {
        // pos points at the current element
        if (at_rootname(mPos)) {
            get_element(mStart, mEnd); // get 1st path element
            mPos = mStart;
            return;
        }

        const auto oldPos = mPos;
        size_t separatorCount = skip_separators(mPos);
        const bool atEnd = mPos == mEnd;
        if (separatorCount > 0) {
            if (atEnd && (oldPos != mStart || separatorCount > 1) /* ignore root dir only */) {
                if (mElement != dot_char) { // XXX: corner case: /a/b/c/./ will break
                    // XXX: we set the dot element, but pos is still at the end, reset it to the first trailing separator.
                    // Next increment will hit the actual end.
                    mPos = oldPos;
                }
                mElement = dot_char;
                return;
            }
        } else if (!atEnd) { // not a separator
            mPos = std::find(mPos, mEnd, path_type::preferred_separator);
            PSASSERT(mPos == mEnd || *mPos == path_type::preferred_separator, "Broken assumption");
            next_element();
            return;
        }

        get_element(mPos, mEnd);
    }

    void previous_element() {
        // pos points at the current element
        if (has_rootname() && mPos <= mStart) {
            mElement = extract_rootname();
            mPos = mRootName;
            return;
        }

        const bool atEnd = mPos == mEnd;
        auto i = base_reverse_iterator{mPos};
        size_t separatorCount = skip_separators(i);
        if (separatorCount > 0) {
            const auto& where = i.base();
            if (atEnd && where > mStart /* ignore root dir only */) {
                mPos = i.base();
                mElement = dot_char;
                return;
            } else if (at_start(i.base())) {
                mPos = i.base();
                mElement = path_type::preferred_separator;
                return;
            }
        }

        mPos = std::find(i, base_reverse_iterator{mStart}, path_type::preferred_separator).base(); // base points 1 head of reverse
        PSASSERT(mPos == mStart || *mPos != path_type::preferred_separator, "Broken assumption");
        get_element(mPos, mEnd);
    }

    static constexpr const typename path_type::value_type dot_char = static_cast<typename path_type::value_type>('.');

    path_type mElement;
    base_iterator mStart;
    base_iterator mEnd;
    base_iterator mPos;
    base_iterator mRootName; // Win32 only
};

template <class String>
constexpr const typename basic_path<String>::iterator::path_type::value_type basic_path<String>::iterator::dot_char;

template <class String>
inline typename basic_path<String>::iterator basic_path<String>::begin() const {
    return iterator{*this};
}

template <class String>
inline typename basic_path<String>::iterator basic_path<String>::end() const {
    return iterator{*this, m_pathname.end()};
}

// modify //

template <class String>
inline void basic_path<String>::clear() noexcept(noexcept(std::declval<string_type>().clear())) {
    m_pathname.clear();
}

// Spec only mentions changing non-preferred separators. It makes no mention of eliding duplicates, so we'll just do it for all platforms.
template <class String>
inline basic_path<String>& basic_path<String>::make_preferred() {
    sanitize(m_pathname, preferred_separator_style);
    return *this;
}

template <class String>
inline basic_path<String>& basic_path<String>::remove_filename() {
    m_pathname = parent_path().raw();
    return *this;
}

template <class String>
inline basic_path<String>& basic_path<String>::replace_filename(const basic_path& p) {
    remove_filename();
    operator/=(p);
    return *this;
}

template <class String>
basic_path<String>& basic_path<String>::replace_extension(const basic_path& e) // replacement
{
    const auto basename = stem();
    remove_filename();
    operator/=(basename);

    if (!e.empty()) {
        const auto dot = static_cast<value_type>('.');
        if (e.native()[0] != dot) {
            operator+=(dot);
        }
        operator+=(e);
    }
    return *this;
}

template <class String>
void basic_path<String>::swap(basic_path& other) noexcept(noexcept(std::swap(std::declval<string_type&>(), std::declval<string_type&>()))) {
    if (this != &other) {
        using std::swap;
        swap(m_pathname, other.m_pathname);
    }
}

// observers //

template <class String>
inline const typename basic_path<String>::string_type& basic_path<String>::native() const noexcept {
    return m_pathname;
}

template <class String>
inline const typename basic_path<String>::const_pointer basic_path<String>::c_str() const noexcept(noexcept(std::declval<string_type>().c_str())) {
    return m_pathname.c_str();
}

template <class String>
inline basic_path<String>::operator string_type() const {
    return m_pathname;
}

template <class String>
inline std::string basic_path<String>::string() const {
    return ifilesystem::to_string_type<std::string, String>{}(m_pathname);
}

template <class String>
inline prosoft::u8string basic_path<String>::u8string() const {
    return ifilesystem::to_string_type<prosoft::u8string, String>{}(m_pathname);
}

template <class String>
inline prosoft::u16string basic_path<String>::u16string() const {
    return ifilesystem::to_string_type<prosoft::u16string, String>{}(m_pathname);
}

template <class String>
inline prosoft::u32string basic_path<String>::u32string() const {
    return ifilesystem::to_string_type<prosoft::u32string, String>{}(m_pathname);
}

template <class String>
inline std::string basic_path<String>::generic_string() const {
    return string();
}

template <class String>
inline prosoft::u8string basic_path<String>::generic_u8string() const {
    return u8string();
}

template <class String>
inline prosoft::u16string basic_path<String>::generic_u16string() const {
    return u16string();
}

template <class String>
inline prosoft::u32string basic_path<String>::generic_u32string() const {
    return u32string();
}

// compare //

template <class String>
int basic_path<String>::compare(const basic_path& other) const noexcept(noexcept(std::declval<string_type>().compare(std::declval<basic_path>()))) {
    auto i = begin();
    const auto last = end();
    auto oi = other.begin();
    const auto olast = other.end();

    for (; oi != olast; ++oi, ++i) {
        if (i != last) {
            if (int retval = (*i).native().compare((*oi).native())) {
                return retval;
            }
        } else {
            return -1;
        }
    }

    const bool iend = i == last;
    const bool oiend = oi == olast;
    if (iend && oiend) {
        return 0;
    } else {
        return (iend ? -1 : 1);
    }
}

template <class String>
inline int basic_path<String>::compare(const string_type& s) const {
    return m_pathname.compare(s);
}

template <class String>
inline int basic_path<String>::compare(const const_pointer s) const {
    return m_pathname.compare(s);
}

// decomposition //

namespace ifilesystem {

template <class String, path_style>
struct root_name {
};

template <class String>
struct root_name<String, path_style::posix> {
    using path_type = basic_path<String>;
    path_type operator()(const path_type&) {
        return path_type{};
    }
};

template <class String>
struct root_name<String, path_style::windows> {
    using path_type = basic_path<String>;
    path_type operator()(const path_type& p) {
        if (!p.empty()) {
            using namespace ifilesystem;
            auto name = *(p.begin());
            if (starts_with<String>(name.native(), unc_prefix<String>()) || starts_with_drive_letter<String>(name.native())) {
                return name;
            }
        }

        return path_type{};
    }
};

} // ifilesystem

template <class String>
inline basic_path<String> basic_path<String>::root_name() const {
    return ifilesystem::root_name<String, preferred_separator_style>{}(*this);
}

template <class String>
basic_path<String> basic_path<String>::root_directory() const {
    if (!empty()) {
        auto i = begin();
        if (has_root_name()) {
            ++i;
        }
        return (i != end() && *i == preferred_separator ? preferred_separator : basic_path{});
    } else {
        return basic_path{};
    }
}

template <class String>
inline basic_path<String> basic_path<String>::root_path() const {
    return root_name() / root_directory();
}

template <class String>
basic_path<String> basic_path<String>::relative_path() const {
    const auto root = root_path().native();
    auto p = native();
    if (!root.empty()) {
        p.erase(0, root.size());
        // now trim any duplicate separators
        size_t count = 0;
        for (auto c : p) {
            if (c != preferred_separator) {
                break;
            }
            ++count;
        }
        if (count) {
            p.erase(0, count);
        }
    }
    return basic_path{std::move(p)};
}

namespace ifilesystem {

template <class String>
typename basic_path<String>::iterator last_component(const basic_path<String>& p) {
    PSASSERT(!p.empty(), "Bad arg");
    auto last = --p.end();
    const auto& s = p.native();
    if (ends_with<String>(s, String{basic_path<String>::preferred_separator}) && (*last).native() == PS_TEXT(".")) {
        --last; // ignore trailing separators
    }
    return last;
}

} // ifilesystem

template <class String>
basic_path<String> basic_path<String>::parent_path() const {
    basic_path p;
    if (!empty()) {
        auto i = begin();
        auto last = ifilesystem::last_component(*this);

        if (preferred_separator_style == path_style::windows && i != last && is_relative()) {
            using namespace ifilesystem;
            const auto& s = (*i).native();
            if (s.size() >= 2 && starts_with_drive_letter(s, s.size() - 2)) {
                // Create relative root from the first two components (drive + relative folder)
                p += *i++;
                if (i != last) {
                    p += *i++;
                }
            }
        }

        while (i != last) {
            p /= *i++;
        }
    }
    return p;
}

template <class String>
basic_path<String> basic_path<String>::filename() const {
    return (!empty() ? *ifilesystem::last_component(*this) : basic_path{});
}

namespace ifilesystem {

template <class String>
typename String::size_type find_extension(const String& s) {
    const auto dot = PS_TEXT(".");
    const auto dotdot = PS_TEXT("..");
    typedef typename basic_path<String>::string_type string_type;
    if (s != dot && s != dotdot) {
        return s.rfind(static_cast<typename string_type::value_type>('.'));
    }
    return string_type::npos;
}
}

template <class String>
basic_path<String> basic_path<String>::stem() const {
    auto p = filename();
    const auto& s = p.native();
    const auto i = ifilesystem::find_extension(s);
    if (i != string_type::npos) {
        return basic_path{s.substr(0, i)};
    }

    return p;
}

template <class String>
basic_path<String> basic_path<String>::extension() const {
    auto p = filename();
    const auto& s = p.native();
    const auto i = ifilesystem::find_extension(s);
    if (i != string_type::npos) {
        // std does not specify what happens when the extension consists solely of a period.
        // It does say though that stem() + extension() must equal filename(), so to not over-complicate stem() we'll just return
        return basic_path{s.substr(i)};
    }

    return basic_path{};
}

// query //

template <class String>
inline bool basic_path<String>::empty() const noexcept(noexcept(std::declval<string_type>().empty())) {
    return m_pathname.empty();
}

template <class String>
inline bool basic_path<String>::has_root_name() const {
    return !root_name().empty();
}

template <class String>
inline bool basic_path<String>::has_root_directory() const {
    return !root_directory().empty();
}

template <class String>
inline bool basic_path<String>::has_relative_path() const {
    return !relative_path().empty();
}

template <class String>
inline bool basic_path<String>::has_parent_path() const {
    return !parent_path().empty();
}

template <class String>
inline bool basic_path<String>::has_filename() const {
    return !filename().empty();
}

template <class String>
inline bool basic_path<String>::has_stem() const {
    return !stem().empty();
}

template <class String>
inline bool basic_path<String>::has_extension() const {
    return !extension().empty();
}

template <class String>
inline bool basic_path<String>::is_absolute() const {
    return has_root_directory() && (preferred_separator_style == path_style::posix || has_root_name());
}

template <class String>
inline bool basic_path<String>::is_relative() const {
    return !is_absolute();
}

// concrete type //

#ifndef PS_CPP17_FILESYSTEM_PATH_USES_NATIVE_ENCODING
#define PS_CPP17_FILESYSTEM_PATH_USES_NATIVE_ENCODING 1
#endif

#if !_WIN32 || !PS_CPP17_FILESYSTEM_PATH_USES_NATIVE_ENCODING
using path = basic_path<u8string>;
#else
using path = basic_path<u16string>;
#endif

// Extensions //
inline namespace literals {
inline namespace path_literals {

inline path operator"" _p(path::const_pointer s, size_t len) {
    return path{path::string_type{s, len}};
}

#if _WIN32 && PS_CPP17_FILESYSTEM_PATH_USES_NATIVE_ENCODING
inline path operator"" _p(const char* s, size_t len) { // TEXT() macro does not work with UDLs
    return path{ifilesystem::to_string_type<path::string_type, u8string>{}(s, len)};
}
#endif
}
}

} // v1
} // filesystem
} // prosoft

#endif // PS_CORE_FILESYSTEM_PATH_HPP
