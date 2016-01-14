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

// C++17 Filesystem implementation: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4100.pdf

#ifndef PS_CORE_FILESYSTEM_HPP
#define PS_CORE_FILESYSTEM_HPP

#include <functional>
#include <iomanip>
#include <iostream>
#include <system_error>

// clang-format off
namespace prosoft { namespace filesystem { inline namespace v1 {
using error_code = std::error_code; // Extension
}}}
// clang-format on

#include "filesystem_path.hpp"
#include "filesystem_acl.hpp"

#include <prosoft/core/include/system_error.hpp>

namespace prosoft {
namespace filesystem {
inline namespace v1 {

inline void swap(path& lhs, path& rhs) noexcept(noexcept(std::declval<path>().swap(std::declval<path&>()))) {
    lhs.swap(rhs);
}

inline size_t hash_value(const path& p) {
    using hash = std::hash<typename path::string_type>;
    return hash{}(p);
}

inline bool operator<(const path& lhs, const path& rhs) {
    return lhs.compare(rhs) < 0;
}

inline bool operator<=(const path& lhs, const path& rhs) {
    return lhs.compare(rhs) <= 0;
}

inline bool operator>(const path& lhs, const path& rhs) {
    return lhs.compare(rhs) > 0;
}

inline bool operator>=(const path& lhs, const path& rhs) {
    return lhs.compare(rhs) >= 0;
}

inline bool operator==(const path& lhs, const path& rhs) {
    return lhs.compare(rhs) == 0;
}

inline bool operator!=(const path& lhs, const path& rhs) {
    return !(lhs == rhs);
}

inline path operator/(const path& lhs, const path& rhs) {
    return path{lhs} /= rhs;
}

template <class CharT, class Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const path& p) {
    os <<
#if __ps_cpp14_stdlib
        std::quoted(
#endif
            ifilesystem::to_string_type<std::basic_string<CharT, Traits>, path::string_type>{}(p.native())
#if __ps_cpp14_stdlib
                )
#endif
        ;
    return os;
}

template <class CharT, class Traits>
std::basic_istream<CharT, Traits>& operator>>(std::basic_istream<CharT, Traits>& is, path& p) {
    std::basic_string<CharT, Traits> s;
    is >>
#if __ps_cpp14_stdlib
        std::quoted(
#endif
            s
#if __ps_cpp14_stdlib
            )
#endif
        ;
    p = s;
    return is;
}

// Explicit conversion from raw UTF8 input.
template <class Source>
inline path u8path(const Source& s) {
    return path{u8string{s}};
}

template <class InputIterator>
inline path u8path(InputIterator first, InputIterator last) {
    return path{u8string{std::string{first, last}}};
}

// Extensions
inline const std::error_category& filesystem_category() {
    return system::error_category();
}
// Extensions

class filesystem_error : public std::system_error {
public:
    filesystem_error(const std::string& msg, error_code ec)
        : std::system_error(ec, msg) {}
    filesystem_error(const std::string& msg, path p, error_code ec)
        : std::system_error(ec, msg)
        , mPath1(p) {}
    filesystem_error(const std::string& msg, path p, path p2, error_code ec)
        : std::system_error(ec, msg)
        , mPath1(p)
        , mPath2(p2) {}
    virtual PS_DEFAULT_DESTRUCTOR(filesystem_error);

    PS_DEFAULT_COPY(filesystem_error);
    PS_DEFAULT_MOVE(filesystem_error);

    const path& path1() noexcept {
        return mPath1;
    }

    const path& path2() noexcept {
        return mPath2;
    }

private:
    path mPath1;
    path mPath2;
};

enum class file_type {
    none = 0,
    not_found = -1,
    regular = 1,
    directory = 2,
    symlink = 3,
    block = 4,
    character = 5,
    fifo = 6,
    socket = 7,
    unknown = 8
};

enum class perms {
    none = 0,
    owner_read = 0400,
    owner_write = 0200,
    owner_exec = 0100,
    owner_all = 0700,

    group_read = 040,
    group_write = 020,
    group_exec = 010,
    group_all = 070,

    others_read = 04,
    others_write = 02,
    others_exec = 01,
    others_all = 07,

    all = 0777,
    set_uid = 04000,
    set_gid = 02000,
    sticky_bit = 01000,

    mask = 07777,

    unknown = 0xffff,

    add_perms = 0x10000,
    remove_perms = 0x20000,
    resolve_symlinks = 0x40000,
};

PS_ENUM_BITMASK_OPS(perms);

// Extensions

class owner {
    using owner_type = std::pair<access_control_identity, access_control_identity>;
    owner_type m_owner;

public:
    template <class Identity>
    owner(Identity&& user, Identity&& group)
        : m_owner(std::forward<Identity>(user), std::forward<Identity>(group)) {}

    explicit owner()
        : owner(invalid_owner()) {}

    PS_DEFAULT_DESTRUCTOR(owner);
    PS_DEFAULT_COPY(owner);
    PS_DEFAULT_MOVE(owner);

    void swap(owner& other) noexcept {
        using std::swap;
        swap(m_owner, other.m_owner);
    }

    bool operator==(const owner& other) const {
        return m_owner == other.m_owner;
    }

    const access_control_identity& user() const noexcept {
        return m_owner.first;
    }

    const access_control_identity& group() const noexcept {
        return m_owner.second;
    }

    template <class Identity>
    void user(Identity&& user) {
        m_owner.first = std::forward<Identity>(user);
    }

    template <class Identity>
    void group(Identity&& group) {
        m_owner.second = std::forward<Identity>(group);
    }

    static owner process_owner() {
        return owner{access_control_identity::process_user(), access_control_identity::invalid_group()};
    }

    static owner invalid_owner() {
        return owner{access_control_identity::invalid_user(), access_control_identity::invalid_group()};
    }
};

inline void swap(owner& lhs, owner& rhs) {
    lhs.swap(rhs);
}

inline bool operator!=(const owner& lhs, const owner& rhs) {
    return !lhs.operator==(rhs);
}

// Extensions

class file_status {
    using owner_type = filesystem::owner;

public:
    file_status(file_type ft, perms p, owner_type&& o)
        : m_owner(std::move(o))
        , m_type(ft)
        , m_perms(p) {}
    file_status(file_type ft, perms p, const owner_type& o)
        : file_status(ft, p, owner_type{o}) {}
    explicit file_status(file_type ft = file_type::none, perms p = perms::unknown)
        : file_status(ft, p, owner_type::invalid_owner()) {}
    PS_DEFAULT_DESTRUCTOR(file_status);
    PS_DEFAULT_COPY(file_status);
    PS_DEFAULT_MOVE(file_status);

    file_type type() const noexcept {
        return m_type;
    }

    void type(file_type ft) noexcept {
        m_type = ft;
    }

    perms permissions() const noexcept {
        return m_perms;
    }

    void permissions(perms p) noexcept {
        m_perms = p;
    }

    const owner_type& owner() const noexcept {
        return m_owner;
    }

    void owner(const owner_type& o) {
        m_owner = o;
    }

    void owner(owner_type&& o) {
        m_owner = std::move(o);
    }

private:
    owner_type m_owner;
    file_type m_type;
    perms m_perms;
};

class directory_entry {
    // C++ states that a name within a scope must have a unique meaning.
    // Therefore path() conflicts with the use of the 'path' type from our namespace and is ambigous.
    // GCC warns about this, clang does not.
    using path_type = prosoft::filesystem::path;
    path_type m_path;
    const path_type& get_path() const {
        return m_path;
    }

public:
    PS_DEFAULT_CLASS(directory_entry);
    explicit directory_entry(const path_type& p)
        : m_path(p) {}
    // Extensions //
    explicit directory_entry(path_type&& p) noexcept(std::is_nothrow_move_constructible<path_type>::value)
        : m_path(std::move(p)) {}
    // Extensions //

    void assign(const path_type& p) {
        m_path = p;
    }

    void replace_filename(const path_type& p) {
        m_path.replace_filename(p);
    }

    operator const path_type&() const noexcept {
        return get_path();
    }

    const path_type& path() const noexcept {
        return get_path();
    }

    file_status status() const;
    file_status status(error_code&) const noexcept;

    file_status symlink_status() const;
    file_status symlink_status(error_code&) const noexcept;

    bool operator==(const directory_entry& other) const noexcept {
        return get_path() == other.get_path();
    }
    bool operator!=(const directory_entry& other) const noexcept {
        return get_path() != other.get_path();
    }
    bool operator<(const directory_entry& other) const noexcept {
        return get_path() < other.get_path();
    }
    bool operator<=(const directory_entry& other) const noexcept {
        return get_path() <= other.get_path();
    }
    bool operator>(const directory_entry& other) const noexcept {
        return get_path() > other.get_path();
    }
    bool operator>=(const directory_entry& other) const noexcept {
        return get_path() >= other.get_path();
    }
};

// operations
file_status status(const path&);
file_status status(const path&, error_code&) noexcept;

file_status symlink_status(const path&);
file_status symlink_status(const path&, error_code&) noexcept;

inline bool status_known(file_status s) noexcept {
    return s.type() != file_type::none;
}

inline bool exists(file_status s) noexcept {
    return status_known(s) && s.type() != file_type::not_found;
}
inline bool exists(const path& p) {
    return exists(status(p));
}
inline bool exists(const path& p, error_code& ec) noexcept {
    return exists(status(p, ec));
}

inline bool is_block_file(file_status s) noexcept {
    return s.type() == file_type::block;
}
inline bool is_block_file(const path& p) {
    return is_block_file(status(p));
}
inline bool is_block_file(const path& p, error_code& ec) noexcept {
    return is_block_file(status(p, ec));
}

inline bool is_character_file(file_status s) noexcept {
    return s.type() == file_type::character;
}
inline bool is_character_file(const path& p) {
    return is_character_file(status(p));
}
inline bool is_character_file(const path& p, error_code& ec) noexcept {
    return is_character_file(status(p, ec));
}

inline bool is_directory(file_status s) noexcept {
    return s.type() == file_type::directory;
}
inline bool is_directory(const path& p) {
    return is_directory(status(p));
}
inline bool is_directory(const path& p, error_code& ec) noexcept {
    return is_directory(status(p, ec));
}

inline bool is_fifo(file_status s) noexcept {
    return s.type() == file_type::fifo;
}
inline bool is_fifo(const path& p) {
    return is_fifo(status(p));
}
inline bool is_fifo(const path& p, error_code& ec) noexcept {
    return is_fifo(status(p, ec));
}

inline bool is_regular_file(file_status s) noexcept {
    return s.type() == file_type::regular;
}
inline bool is_regular_file(const path& p) {
    return is_regular_file(status(p));
}
inline bool is_regular_file(const path& p, error_code& ec) noexcept {
    return is_regular_file(status(p, ec));
}

inline bool is_socket(file_status s) noexcept {
    return s.type() == file_type::socket;
}
inline bool is_socket(const path& p) {
    return is_socket(status(p));
}
inline bool is_socket(const path& p, error_code& ec) noexcept {
    return is_socket(status(p, ec));
}

inline bool is_symlink(file_status s) noexcept {
    return s.type() == file_type::symlink;
}
inline bool is_symlink(const path& p) {
    return is_symlink(status(p));
}
inline bool is_symlink(const path& p, error_code& ec) noexcept {
    return is_symlink(status(p, ec));
}

inline bool is_other(file_status s) noexcept {
    return exists(s) && !is_regular_file(s) && !is_directory(s) && !is_symlink(s);
}
inline bool is_other(const path& p) {
    return is_other(status(p));
}
inline bool is_other(const path& p, error_code& ec) noexcept {
    return is_other(status(p, ec));
}

// Extensions

// FreeBSD dropped block devices a while ago, Windows never supported block devices and Linux does not have disk char devices.
// Therefore in most cases all you should care about is if the file is a device file and not what type of device.

inline bool is_device_file(file_status s) noexcept {
    return is_character_file(s) || is_block_file(s);
}
inline bool is_device_file(const path& p) {
    return is_device_file(status(p));
}
inline bool is_device_file(const path& p, error_code& ec) noexcept {
    return is_device_file(status(p, ec));
}

// Extensions

// dir entry ops
inline file_status directory_entry::status() const {
    return prosoft::filesystem::status(get_path());
}
inline file_status directory_entry::status(error_code& ec) const noexcept {
    return prosoft::filesystem::status(get_path(), ec);
}
inline file_status directory_entry::symlink_status() const {
    return prosoft::filesystem::symlink_status(get_path());
}
inline file_status directory_entry::symlink_status(error_code& ec) const noexcept {
    return prosoft::filesystem::symlink_status(get_path(), ec);
}

path temp_directory_path();
path temp_directory_path(error_code&);

} // v1
} // filesystem
} // prosoft

#endif // PS_CORE_FILESYSTEM_HPP
