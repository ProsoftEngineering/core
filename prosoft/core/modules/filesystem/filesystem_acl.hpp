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

// Spec extension

#ifndef PS_CORE_FILESYSTEM_ACL_HPP
#define PS_CORE_FILESYSTEM_ACL_HPP

#include <memory>
#include <utility>
#include <vector>

#include <prosoft/core/modules/system_identity/identity.hpp>

namespace prosoft {
namespace filesystem {
inline namespace v1 {

enum class access_control_type {
    deny,
    allow
};

enum class access_control_perms : unsigned int {
    none = 0,

    read_data = 0x1,
    write_data = 0x2,
    append_data = 0x4,

    execute = 0x10,
    search = 0x20,
    remove = 0x40, // delete

    list_directory = 0x100,
    add_file = 0x200,
    add_sub_directory = 0x400,
    remove_child = 0x800,

    read_attrs = 0x1000,
    write_attrs = 0x2000,
    read_extended_attrs = 0x4000,
    write_extended_attrs = 0x8000,

    read_security = 0x10000,
    write_security = 0x20000,
    change_owner = 0x40000,

    synchronize = 0x80000000,

    read = read_data | read_attrs | read_extended_attrs | read_security,
    write = write_data | append_data | write_attrs | write_extended_attrs | write_security,
    all_dir = list_directory | add_file | add_sub_directory | remove_child,
    all = read | write | all_dir | execute | search | remove | change_owner | synchronize
};
PS_ENUM_BITMASK_OPS(access_control_perms);

enum class access_control_flags : unsigned int {
    no_inherit = 0,
    inherit = 0x1,
    inherited = 0x2, // read-only
    inherit_file = 0x4,
    inherit_directory = 0x8,
    inherit_limited = 0x10,
    inherit_only = 0x20,
};
PS_ENUM_BITMASK_OPS(access_control_flags);

using access_control_identity_type = system::identity_type;
using access_control_identity = system::identity;

class access_control_entry {
public:
    explicit access_control_entry()
        : access_control_entry(access_control_identity::invalid_user()) {} // not that useful
    explicit access_control_entry(const access_control_identity& i)
        : // default allow none
        access_control_entry(access_control_type::allow, access_control_flags::no_inherit, access_control_perms::none, i) {}
    access_control_entry(access_control_type t, access_control_flags f, access_control_perms p, const access_control_identity& i)
        : m_type(t)
        , m_flags(f)
        , m_perms(p)
        , m_identity(i) {}
    access_control_entry(access_control_type t, access_control_flags f, access_control_perms p, access_control_identity&& i)
        : m_type(t)
        , m_flags(f)
        , m_perms(p)
        , m_identity(std::move(i)) {} // benefits Windows implementation
    PS_DEFAULT_COPY(access_control_entry);
    PS_DEFAULT_MOVE(access_control_entry);
    PS_DEFAULT_DESTRUCTOR(access_control_entry);

    void swap(access_control_entry& other) noexcept {
        using std::swap;
        swap(m_type, other.m_type);
        swap(m_flags, other.m_flags);
        swap(m_perms, other.m_perms);
        swap(m_identity, other.m_identity);
    }

    explicit operator bool() const noexcept {
        return identity().operator bool();
    }

    access_control_type type() const noexcept {
        return m_type;
    }

    void type(access_control_type t) noexcept {
        m_type = t;
    }

    access_control_flags flags() const noexcept {
        return m_flags;
    }

    void flags(access_control_flags f) noexcept {
        m_flags = f;
    }

    access_control_perms perms() const noexcept {
        return m_perms;
    }

    void perms(access_control_perms p) noexcept {
        m_perms = p;
    }

    const access_control_identity& identity() const noexcept {
        return m_identity;
    }

    void identity(const access_control_identity& i) {
        m_identity = i;
    }
    
    void identity(access_control_identity&& i) {
        m_identity = std::move(i);
    }

private:
    access_control_type m_type;
    access_control_flags m_flags;
    access_control_perms m_perms;
    access_control_identity m_identity;
};

inline void swap(access_control_entry& lhs, access_control_entry& rhs) noexcept {
    return lhs.swap(rhs);
}

inline bool operator==(const access_control_entry& lhs, const access_control_entry& rhs) noexcept {
    return (lhs.type() == rhs.type() && lhs.flags() == rhs.flags() && lhs.perms() == rhs.perms() && lhs.identity() == rhs.identity());
}

inline bool operator!=(const access_control_entry& lhs, const access_control_entry& rhs) noexcept {
    return !operator==(lhs, rhs);
}

// path operations

using access_control_list = std::vector<access_control_entry>;

access_control_list acl(const path&, error_code&); // An empty acl is not an error, always check the error code.
access_control_list acl(const path& p);
void acl(const path&, const access_control_list&, error_code&);
void acl(const path&, const access_control_list&);

#define PS_HAVE_SYMLINK_ACL __APPLE__ || __FreeBSD__ // OpenBSD?

// These will error with ENOTSUP on unsupported platforms
access_control_list acl_link(const path&, error_code&); // An empty acl is not an error, always check the error code.
access_control_list acl_link(const path& p);
void acl_link(const path&, const access_control_list&, error_code&);
void acl_link(const path&, const access_control_list&);

// special case ACLs
const access_control_list& all_access() noexcept;
const access_control_list& no_access() noexcept;

} // v1
} // filesystem
} // prosoft

#endif // PS_CORE_FILESYSTEM_ACL_HPP
