// Copyright © 2016-2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_FILESYSTEM_CHANGE_MONITOR_HPP
#define PS_CORE_FILESYSTEM_CHANGE_MONITOR_HPP

// In general there are two types of change notifications
// node (file/dir) changes
// and tree (recursive) changes
// These are platform specific.

// XXX: WARNING: There is subtle filesystem bug that affects FSEvents on every pre-10.11 macOS release.
// https://github.com/bdkjones/fseventsbug/wiki/realpath()-And-FSEvents

#include "filesystem_have_change_monitor.hpp"
#if PS_HAVE_FILESYSTEM_CHANGE_MONITOR

#include <chrono>
#include <cstdint>
#include <deque>
#include <iostream>
#include <functional>
#include <memory>

#include <prosoft/core/config/config.h>

namespace prosoft {
namespace filesystem {
inline namespace v1 {

enum class change_event : unsigned {
    none = 0,
    created = 1U<<1,
    // There may be no distinction between content and metadata modification on some systems.
    // In which case both of these flags will always be set.
    content_modified = 1U<<2,
    metadata_modified = 1U<<3,
    modified = content_modified|metadata_modified,
    removed = 1U<<4,
    // Renames may come in pairs or as a single event, depending on various system specifics.
    // We do our best to generate a single event, but unfortunately it's not reliable.
    renamed = 1U<<5,
    
    all = created|removed|renamed|content_modified|metadata_modified,
    
    // A full rescan of the tree is suggested.
    // This may be set in conjuction with canceled due to an error (in which case the rescan is required),
    // or it may be a standalone event that contains a path in the tree that has been hidden or exposed due to a volume mount/unmount.
    rescan = 1<<29,
    
    // Special flag indicating the event (created or removed) was a side effect of change made outside of the watched tree.
    // e.g A file in the tree was moved outside the tree or a file outside the tree was moved into the tree.
    // XXX: This should not be considered stable.
    // It may not be available on some platforms and even if available, it may not be properly set in all cases.
    outside_tree = 1<<30,
    
    // Special flag indicating the monitor was canceled, most likely due to some kind of change to the monitored path
    // e.g. a rename or remove (in which case the corresponding flag above will also be set).
    // Other changes can occur (such as the containing volume being unmounted) that will not be reflected by the above flags
    // it is up to the caller to decide how to handle the event (in the case of rename you likely want to start a new monitor).
    // No more events will be received after a cancel event and the client should stop() the monitor to release resources.
    canceled = 1U<<31,
    
    rescan_required = rescan|canceled
};
PS_ENUM_BITMASK_OPS(change_event);

class change_manager;
class change_registration;

class change_notification {
    using path_type = prosoft::filesystem::path;
    using platform_event_id_type = std::uint64_t;
    using regid_type = std::uintptr_t;
    path_type m_path;
    path_type m_newpath; // rename
    regid_type m_regid;
    platform_event_id_type m_eventid;
    change_event m_event;
    file_type m_type;
    
    change_notification() = default;
    friend change_manager;
public:
    ~change_notification() = default;
    PS_DEFAULT_COPY(change_notification);
    PS_DEFAULT_MOVE(change_notification);
    
    const path_type& path() const noexcept {
        return m_path;
    }
    
    const path_type& renamed_to_path() const noexcept {
        return m_newpath;
    }
    
    change_event event() const noexcept {
        return m_event;
    }
    
    regid_type registration_id() const noexcept {
        return m_regid;
    }
    
    file_type type() const noexcept {
        return m_type;
    }
    
    // Change events are usually frequent and ephemeral. This allows moving a path out of the event without a copy.
    // This invalidates the notification!
    path_type extract_path() noexcept(std::is_nothrow_move_constructible<path_type>::value) {
        m_event = change_event::none;
        m_regid = 0;
        return !m_newpath.empty() ? std::move(m_newpath) : std::move(m_path);
    }
};

inline bool type_known(change_notification n) noexcept {
    return n.type() != file_type::none;
}

inline bool created(const change_notification& n) noexcept {
    return is_set(n.event() & change_event::created);
}

inline bool removed(const change_notification& n) noexcept {
    return is_set(n.event() & change_event::removed);
}

inline bool renamed(const change_notification& n) noexcept {
    return is_set(n.event() & change_event::renamed);
}

inline bool content_modified(const change_notification& n) noexcept {
    return is_set(n.event() & change_event::content_modified);
}

inline bool metadata_modified(const change_notification& n) noexcept {
    return is_set(n.event() & change_event::metadata_modified);
}

inline bool modified(const change_notification& n) noexcept {
    return is_set(n.event() & change_event::modified);
}

inline bool rescan(const change_notification& n) noexcept {
    return is_set(n.event() & change_event::rescan);
}

inline bool canceled(const change_notification& n) noexcept {
    return is_set(n.event() & change_event::canceled);
}

// XXX: see "fsevents_vector_crash" test for why vector is not used.
using change_notifications = std::vector<change_notification>;
using change_callback = std::function<void (change_notifications&&)>;

// System specific state.
struct change_state {
    virtual ~change_state() = default;
    PS_DISABLE_COPY(change_state);
    PS_DISABLE_MOVE(change_state);

protected:
    change_state() = default;
};
std::ostream& operator<<(std::ostream&, const change_state&);

bool operator==(const change_state&, const change_state&);
inline bool operator!=(const change_state& lhs, const change_state& rhs) {
    return !operator==(lhs, rhs);
}

// Change monitor state.
bool operator==(const change_registration&, const change_notification&);

class change_registration {
    std::weak_ptr<change_state> m_state;
    friend change_manager;
    friend bool operator==(const change_registration& lhs, const change_notification&);
public:
    change_registration() noexcept : m_state() {}
    ~change_registration() = default;
    PS_DEFAULT_COPY(change_registration);
    PS_DEFAULT_MOVE(change_registration);
    
    explicit operator bool() const noexcept {
        return !m_state.expired();
    }
    
    bool operator==(const change_registration& other) const {
        auto p = m_state.lock();
        if (p) {
            auto op = other.m_state.lock();
            return op && *p == *op;
        }
        return false;
    }
    
    std::ostream& operator<<(std::ostream& os) const {
        if (auto p = m_state.lock()) {
            os << *p;
        }
        return os;
    }
};

inline bool operator==(const change_registration& lhs, const change_notification& rhs) {
    return lhs && rhs.registration_id() == reinterpret_cast<std::uintptr_t>(lhs.m_state.lock().get());
}

inline bool operator==(const change_notification& lhs, const change_registration& rhs) {
    return operator==(rhs, lhs);
}

struct change_config {
    using latency_type = std::chrono::milliseconds;
    change_state* state;
    latency_type notification_latency; // how often to post notifications, a larger # allows notifications to be coalesced into fewer callbacks
    change_event events;
    unsigned reserved_flags;
    
    constexpr change_config() noexcept
        : state()
        , notification_latency(1000)
        , events(change_event::all)
        , reserved_flags() {}
    ~change_config() = default;
    PS_DEFAULT_COPY(change_config);
    PS_DEFAULT_MOVE(change_config);
    
    change_config(change_event evts) : change_config() {
        events = evts;
    }
};

// XXX: callbacks will occur in the background and must be thread safe

change_registration monitor(const path&, const change_config&, change_callback, error_code&);
change_registration monitor(const path&, const change_config&, change_callback);
inline change_registration monitor(const path& p, change_callback cb) {
    return monitor(p, change_config{}, std::move(cb));
}

#if PS_HAVE_RECURISIVE_FILESYSTEM_CHANGE_MONITOR
change_registration recursive_monitor(const path&, const change_config&, change_callback, error_code&);
change_registration recursive_monitor(const path&, const change_config&, change_callback);
inline change_registration recursive_monitor(const path& p, change_callback cb) {
    return recursive_monitor(p, change_config{}, std::move(cb));
}
#endif

void stop(const change_registration&, error_code&);
void stop(const change_registration&);

class unique_change_registration {
    change_registration m_reg;
public:
    unique_change_registration(change_registration&& reg) noexcept(std::is_nothrow_move_constructible<change_registration>::value)
        : m_reg(std::move(reg)) {
    }
    ~unique_change_registration() {
        if (m_reg) {
            error_code ec;
            PSIgnoreCppException(stop(m_reg, ec));
        }
    }
    PS_DISABLE_COPY(unique_change_registration);
    PS_DEFAULT_MOVE(unique_change_registration);
    
    explicit operator bool() const noexcept {
        return m_reg.operator bool();
    }
    
    operator const change_registration&() const noexcept {
        return m_reg;
    }
};

} // v1
} // filesystem
} // prosoft

#endif // PS_HAVE_FILESYSTEM_CHANGE_MONITOR
#endif // PS_CORE_FILESYSTEM_CHANGE_MONITOR_HPP
