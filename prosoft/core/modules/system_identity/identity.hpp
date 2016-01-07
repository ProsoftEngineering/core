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

#ifndef PS_CORE_IDENTITY_HPP
#define PS_CORE_IDENTITY_HPP

#include <climits>
#include <limits>
#include <memory>

#include <prosoft/core/config/config.h>

#if !_WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

#if __APPLE__
#include <sys/kernel_types.h> // for guid_t
#include <uuid/uuid.h>
#endif

#include <prosoft/core/include/string/string_convert.hpp>

namespace prosoft {
namespace system {

enum class identity_type {
    user,
    group,
    // may want to expand to include all Windows types
    unknown, // Identity does not resolve to an account on the current system (deleted account, or missing network account)
    other = INT_MAX,
};

class identity {
public:
    PS_DEFAULT_DESTRUCTOR(identity);
#if !_WIN32
    typedef ::uid_t system_identity_type;
    // -1
    static constexpr const auto invalid_system_identity = std::numeric_limits<system_identity_type>::max();
    // XXX: -3: assuming we never see this in the wild. OS X uses -2 for user nobody that.
    static constexpr const auto unknown_system_identity = invalid_system_identity - 2;

    explicit identity(identity_type t, system_identity_type sid) noexcept : m_type(t), m_sid(sid) {}
#if __APPLE__
    explicit identity(const guid_t*);
    explicit identity(const guid_t& g)
        : identity(&g) {}
#endif
    PS_DEFAULT_COPY(identity);
    PS_DEFAULT_MOVE(identity);
#else
    typedef ::PSID system_identity_type;
    using system_identity_ptr = std::unique_ptr<::SID>;
    static constexpr const system_identity_type invalid_system_identity = nullptr;

    explicit identity(const system_identity_type sid);
    explicit identity(identity_type, const system_identity_type sid)
        : identity(sid) {}

    identity(const identity& other)
        : identity(other.system_identity()) {}
    identity(identity&& other) noexcept : m_type(other.m_type), m_sid(std::move(other.m_sid)) {}

    identity& operator=(identity&& other) noexcept {
        if (this != &other) {
            m_type = other.type();
            m_sid = std::move(other.m_sid);
            other.m_sid = nullptr;
        }
        return *this;
    }

    identity& operator=(const identity& other) {
        if (this != &other) {
            (void)operator=(std::move(identity(other.system_identity())));
        }
        return *this;
    }
#endif

    void swap(identity& other) noexcept {
        if (this != &other) {
            using std::swap;
            swap(m_type, other.m_type);
            swap(m_sid, other.m_sid); // Win32: swap ptrs without copying mem
        }
    }

    explicit operator bool() const noexcept {
        return valid_sid();
    }

    int compare(const identity& other) const noexcept { // XXX: equality compare only!
        return !equal_sid(other);
    }

    identity_type type() const noexcept {
        return m_type;
    }

    system_identity_type system_identity() const noexcept {
#if !_WIN32
        return m_sid;
#else
        return m_sid.get();
#endif
    }

    native_string_type name() const;
    native_string_type account_name() const;

#if __APPLE__
    guid_t guid() const;
#endif

    // Well-known identities
    
    // For OSX/Windows this is the active user session.
    // For UNIX, it's the active login session if one can be found.
    // If no user is logged in (or we can't determine if one is logged in) invalid_user() is returned.
    static identity console_user();

    static identity process_user()
#if !_WIN32
    {
        return identity(identity_type::user, ::getuid());
    }
#else
        ;
#endif

    static identity effective_user() {
#if !_WIN32
        return identity(identity_type::user, ::geteuid());
#else
        return thread_user();
#endif
    }

#if _WIN32
    static identity thread_user();
    static const identity& everyone_group();
#endif

    // Invalid identities
    static identity invalid_user() {
        identity aci;
#if !_WIN32
        aci.m_sid = invalid_system_identity;
#endif
        aci.m_type = identity_type::user;
        return aci;
    }

    static identity invalid_group() {
        identity aci;
#if !_WIN32
        aci.m_sid = invalid_system_identity;
#endif
        aci.m_type = identity_type::group;
        return aci;
    }

private:
    identity_type m_type;
#if !_WIN32
    system_identity_type m_sid;
#else
    system_identity_ptr m_sid;
#endif

#if __APPLE__
    guid_t m_guid; // this is strictly for tracking unknown ids
#endif

    identity() = default;
#if _WIN32
    identity(native_string_type::const_pointer);
#endif

    bool valid_sid() const noexcept {
#if !_WIN32
        return m_sid != invalid_system_identity;
#else
        return (m_sid && ::IsValidSid(m_sid.get()));
#endif
    }

#if __APPLE__
    bool equal_sid(const guid_t& guid) const {
        return 0 == std::memcmp(&m_guid, &guid, sizeof(guid_t));
    }
#endif

    bool equal_sid(const identity& other) const noexcept {
#if __APPLE__
        return (m_type != identity_type::unknown && m_sid == other.system_identity()) || equal_sid(other.m_guid);
#elif !_WIN32
        return m_sid == other.system_identity();
#else
        auto oid = other.system_identity();
        return (m_sid.get() == oid || (m_sid && oid && ::EqualSid(m_sid.get(), oid)));
#endif
    }
};

inline void swap(identity& lhs, identity& rhs) noexcept {
    return lhs.swap(rhs);
}

inline bool operator==(const identity& lhs, const identity& rhs) noexcept {
    return 0 == lhs.compare(rhs);
}

inline bool operator!=(const identity& lhs, const identity& rhs) noexcept {
    return 0 != lhs.compare(rhs);
}

inline bool is_user(const identity& lhs) noexcept {
    return lhs.type() == identity_type::user;
}

inline bool is_group(const identity& lhs) noexcept {
    return lhs.type() == identity_type::group;
}

bool exists(const identity&); // false if unknown or if known and the identity does not exist on the current system
} // system
} // prosoft

#endif // PS_CORE_IDENTITY_HPP
