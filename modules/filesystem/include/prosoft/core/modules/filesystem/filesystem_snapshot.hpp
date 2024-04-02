// Copyright © 2017-2018, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_FILESYSTEM_SNAPSHOT_HPP
#define PS_CORE_FILESYSTEM_SNAPSHOT_HPP

// XXX: Windows snapshots require a 32bit OS for 32bit apps. WOW64 is not supported.
// Mingw spits out warnings that the VSS COM API "has not been verified". In addition, ATL is not available.
#define PS_HAVE_FILESYSTEM_SNAPSHOT (MAC_OS_X_VERSION_MIN_REQUIRED > 0 || (_WIN32 && !__MINGW32__))

#include <memory>
#include <system_error>
#include <type_traits>

#include "filesystem_path.hpp"

namespace prosoft {
namespace filesystem {
inline namespace v1 {

struct snapshot_id {
#if _WIN32
    using id_type = unsigned char[16]; // GUID
    id_type m_id;

    snapshot_id() noexcept {
        std::memset(m_id, 0, sizeof(m_id));
    }

    snapshot_id(const id_type id) noexcept {
        memcpy_s(m_id, sizeof(m_id), id, sizeof(m_id));
    }

    snapshot_id(native_string_type::const_pointer); // mainly for testing

    PS_DEFAULT_COPY(snapshot_id);
    PS_DEFAULT_MOVE(snapshot_id);

    int compare(const snapshot_id& other) const noexcept {
        return std::memcmp(m_id, other.m_id, sizeof(m_id));
    }

    PS_WARN_UNUSED_RESULT
    std::string string() const;
#else
    native_string_type m_id;
    path m_from;
    path m_to;

    snapshot_id(native_string_type&& s) noexcept(std::is_nothrow_move_constructible<native_string_type>::value)
        : m_id(std::move(s)) {
    }
    snapshot_id(const native_string_type& s)
        : m_id(s) {
    }
    PS_DEFAULT_CLASS(snapshot_id);

    int compare(const snapshot_id& other) const {
        return m_id.compare(other.m_id);
    }

    const std::string& string() const noexcept {
        return m_id;
    }
#endif
};

inline bool operator==(const snapshot_id& lhs, const snapshot_id& rhs) noexcept(noexcept(std::declval<snapshot_id>().compare(std::declval<snapshot_id&>()))) {
    return lhs.compare(rhs) == 0;
}

inline bool operator!=(const snapshot_id& lhs, const snapshot_id& rhs) noexcept(noexcept(operator==(std::declval<snapshot_id&>(), std::declval<snapshot_id&>()))) {
    return !operator==(lhs, rhs);
}

class snapshot_manager;

class snapshot {
    friend snapshot_manager;
    snapshot_id m_id;
    using flags_type = unsigned;
    flags_type m_flags;

    void clear() {
        m_id = snapshot_id{};
        m_flags = flags_type{};
    }

public:
    explicit snapshot(snapshot_id&& sid, flags_type f = 0)
        : m_id(std::move(sid))
        , m_flags(f) {
    }
    
    explicit snapshot(const snapshot_id& sid, flags_type f = 0)
        : m_id(sid)
        , m_flags(f) {
    }

    snapshot(snapshot&& other) noexcept(std::is_nothrow_move_constructible<snapshot_id>::value)
        : m_id(std::move(other.m_id))
        , m_flags(other.m_flags) {
        other.clear();
    }

    PS_DISABLE_COPY(snapshot); // implicit reap not supported
    snapshot& operator=(snapshot&&) = delete; // implicit reap not supported

    ~snapshot();

    const snapshot_id& id() const noexcept {
        return m_id;
    }

    flags_type reserved() const noexcept {
        return m_flags;
    }
};

inline bool operator==(const snapshot& lhs, const snapshot& rhs) noexcept(noexcept(operator==(std::declval<snapshot_id&>(), std::declval<snapshot_id&>()))) {
    return lhs.id() == rhs.id();
}

inline bool operator!=(const snapshot& lhs, const snapshot& rhs) noexcept(noexcept(operator==(std::declval<snapshot&>(), std::declval<snapshot&>()))) {
    return !operator==(lhs, rhs);
}

#define PS_FILESYSTEM_SNAPSHOT_CAN_CREATE_ID 0

struct snapshot_create_options {
    enum : unsigned {
        none = 0,
        nobrowse = 0x1, // do not show in GUI -- not supported for Windows
        defaults = none,
    } m_flags = defaults;
#if PS_FILESYSTEM_SNAPSHOT_CAN_CREATE_ID
    snapshot_id m_id;
#endif
};

PS_WARN_UNUSED_RESULT snapshot create_snapshot(const path&, const snapshot_create_options& opts = snapshot_create_options{});
PS_WARN_UNUSED_RESULT snapshot create_snapshot(const path&, const snapshot_create_options&, std::error_code&);
inline PS_WARN_UNUSED_RESULT snapshot create_snapshot(const path& p, std::error_code& ec) {
    return create_snapshot(p, snapshot_create_options{}, ec);
}

void attach_snapshot(snapshot&, const path& mount);
void attach_snapshot(snapshot&, const path& mount, std::error_code&);

// Snapshot lifecycles are automatically managed, but detach/delete are provided for explicit control.
void detach_snapshot(snapshot&);
void detach_snapshot(snapshot&, std::error_code&);

void delete_snapshot(snapshot&);
void delete_snapshot(snapshot&, std::error_code&);

} // v1
} // filesystem
} // prosoft

#endif // PS_CORE_FILESYSTEM_SNAPSHOT_HPP
