// Copyright © 2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_FSEVENTS_MONITOR_INTERNAL_HPP
#define PS_CORE_FSEVENTS_MONITOR_INTERNAL_HPP

#include <CoreServices/CoreServices.h>
#include <sys/stat.h>

#include <atomic>

#include <prosoft/core/modules/filesystem/filesystem.hpp>   // error_code
#include <prosoft/core/modules/filesystem/filesystem_path.hpp>  // path
#include <prosoft/core/modules/filesystem/filesystem_change_monitor.hpp>    // change_registration
#include <prosoft/core/include/unique_resource.hpp> // unique_fsstream
#include "filesystem_private.hpp"
#include "fsmonitor_private.hpp"

namespace fs = prosoft::filesystem::v1;

using cfduration = std::chrono::duration<double, std::chrono::seconds::period>;

namespace prosoft {
namespace filesystem {
inline namespace v1 {

struct dispatch_delete {
    void operator()(dispatch_object_t p) noexcept {
        if (p) { dispatch_release(p); }
    }
};
using unique_dispatch_queue = std::unique_ptr<dispatch_queue_s, dispatch_delete>;

struct fsevents_delete {
    void operator()(FSEventStreamRef p) noexcept {
        if (p) { FSEventStreamRelease(p); }
    }
};
using unique_fsstream = prosoft::unique_cftype<FSEventStreamRef, fsevents_delete>;

struct platform_state : public fs::change_state {
    fs::change_callback m_callback;
    unique_fsstream m_stream;
    unique_dispatch_queue m_dispatch_q;
    std::uintptr_t m_regid;
    int m_rootfd;
    FSEventStreamEventId m_stopid; // event to stop at
    // persistent values //
    prosoft::unique_cftype<CFUUIDRef> m_uuid; // set when constructed and then read-only
    std::string m_uuid_str; // cached for persistence
    std::atomic<FSEventStreamEventId> m_lastid;
    
    platform_state()
        : m_callback()
        , m_stream()
        , m_dispatch_q()
        , m_rootfd(-1)
        , m_stopid(0)
        , m_uuid()
        , m_lastid(kFSEventStreamEventIdSinceNow) {}
    platform_state(const fs::path&, const fs::change_config&, fs::error_code&);
    platform_state(const std::string&, fs::change_thaw_options); // from serialzed data
    virtual ~platform_state();
    
    virtual fs::change_event_id last_event_id() const override;
    
    virtual std::string serialize() const override;
    virtual std::string serialize(fs::change_event_id) const override;
    
    operator FSEventStreamRef() const noexcept(noexcept(m_stream.get())) {
        return m_stream.get();
    }
};

struct dispatch_events {
    static void callout_to_client(platform_state*, fs::change_notifications*, FSEventStreamEventId);

    void operator()(platform_state* state, std::unique_ptr<fs::change_notifications> notes, FSEventStreamEventId lastNoteID) {
        // Don't block the event thread calling out.
        // Use dispatch since std::async 1) is not a synchronous queue and 2) can't fire and forget
        auto rawnotes = notes.release();
        dispatch_async(state->m_dispatch_q.get(), ^{
            callout_to_client(state, rawnotes, lastNoteID);
        });
    }
};

inline bool rescan_required(FSEventStreamEventFlags flags) {
    constexpr FSEventStreamEventFlags mask = kFSEventStreamEventFlagRootChanged|kFSEventStreamEventFlagMustScanSubDirs;
    return (flags & mask) != 0;
}

inline bool root_changed(FSEventStreamEventFlags flags) {
    return (flags & kFSEventStreamEventFlagRootChanged) != 0;
}

inline fs::path canonical_root_path(const platform_state* state) {
    char buf[PATH_MAX];
    if (0 == fcntl(state->m_rootfd, F_GETPATH, buf)) {
        return fs::path{buf};
    }
    return fs::path{};
}

inline void cancel(platform_state* state) {
    if (state->m_stream) { // can be null in tests
        FSEventStreamStop(state->m_stream.get());
    }
}

constexpr FSEventStreamEventId wants_replay = kFSEventStreamEventIdSinceNow;

using shared_state = std::shared_ptr<platform_state>;
shared_state get_shared_state(platform_state*);

inline bool replay(const fs::change_config& cc) {
    if (auto pc = dynamic_cast<const platform_state*>(cc.state)) {
        return pc->m_stopid == wants_replay;
    }
    return false;
}

inline fs::change_event to_event(FSEventStreamEventFlags flags) {
    fs::change_event evts{};
    
    if (0 != (flags & (kFSEventStreamEventFlagMount|kFSEventStreamEventFlagUnmount))) {
        evts |= fs::change_event::rescan;
        return evts;
    }
    
    // Renames are a special case and require extra processing, so the flag must travel with other events (if present)
    if ((flags & kFSEventStreamEventFlagItemRenamed)) {
        evts |= fs::change_event::renamed;
    }
    
    // Order matters due to event coalescing.
    // XXX: removed events can be coalesced with create events that occur AFTER the remove.
    // This seems wrong and is handled specially in the fsevents callback.
    if ((flags & kFSEventStreamEventFlagItemRemoved)) {
        evts |= fs::change_event::removed;
    } else {
        // XXX: create does not override modified as this may screw up clients looking for a modify after a known create where FSEvents has
        // coalesced the two.
        if ((flags & kFSEventStreamEventFlagItemCreated)) {
            evts |= fs::change_event::created;
        }
        if ((flags & kFSEventStreamEventFlagItemModified)) {
            evts |= fs::change_event::content_modified;
        }
        if ((flags & kFSEventStreamEventFlagItemInodeMetaMod)
            || (flags & kFSEventStreamEventFlagItemFinderInfoMod)
            || (flags & kFSEventStreamEventFlagItemChangeOwner)
            || (flags & kFSEventStreamEventFlagItemXattrMod)) {
            evts |= fs::change_event::metadata_modified;
        }
    }
    
    return evts;
}

inline fs::file_type to_type(FSEventStreamEventFlags flags) {
    constexpr FSEventStreamEventFlags mask = kFSEventStreamEventFlagItemIsFile|kFSEventStreamEventFlagItemIsDir
        |kFSEventStreamEventFlagItemIsSymlink;
    switch((flags & mask)) {
        case kFSEventStreamEventFlagItemIsFile:
            return fs::file_type::regular;
        case kFSEventStreamEventFlagItemIsDir:
            return fs::file_type::directory;
        case kFSEventStreamEventFlagItemIsSymlink:
            return fs::file_type::symlink;
        default:
            return fs::file_type::none;
    }
}

template <class Dispatch = dispatch_events>
void fsevents_callback(ConstFSEventStreamRef, void* info, size_t nevents, void* evpaths, const FSEventStreamEventFlags evflags[], const FSEventStreamEventId evids[]) {
    static auto exists = [](const char* p) noexcept { // avoid need for copying string to a path{}
        struct ::stat sb;
        const int err = ::stat(p, &sb);
        return err == 0 || (-1 == err && errno != ENOENT);
    };
    
    try {
        auto state = reinterpret_cast<platform_state*>(info);
        PSASSERT_NOTNULL(state);
        
        auto notes = std::make_unique<fs::change_notifications>();
        
        FSEventStreamEventId lastID{};
        auto paths = reinterpret_cast<const char* *>(evpaths);
        for(size_t i = 0; i < nevents; ++i) {
            const auto flags = evflags[i];
            FSEventStreamEventFlags negated_flags{};
            if (rescan_required(flags)) {
                fs::path rp;
                fs::path np;
                auto ev = fs::change_event::canceled|fs::change_event::rescan;
                if (root_changed(flags)) {
                    rp = fs::path{paths[i]};
                    np = canonical_root_path(state);
                    fs::error_code ec;
                    if (!fs::exists(rp, ec)) {
                        if (!fs::exists(np, ec)) {
                            ev |= fs::change_event::removed;
                        } else {
                            ev |= fs::change_event::renamed;
                            ev &= ~fs::change_event::rescan;
                        }
                    }
                    if (!is_set(ev & fs::change_event::renamed)) {
                        np.clear();
                    }
                } else {
                    rp = fs::path{paths[i]};
                }
                
                fs::change_manager::emplace_back(*notes, std::move(rp), std::move(np), state, evids[i], ev, fs::file_type::directory);
                
                PSASSERT(is_set(ev & fs::change_event::canceled), "Broken assumption");
                cancel(state);
                break; // further events are invalid
            } // rescan
            
            constexpr FSEventStreamEventFlags fsEventChangeFlags = kFSEventStreamEventFlagItemCreated
                |kFSEventStreamEventFlagItemRemoved
                |kFSEventStreamEventFlagItemInodeMetaMod
                |kFSEventStreamEventFlagItemRenamed
                |kFSEventStreamEventFlagItemModified
                |kFSEventStreamEventFlagItemFinderInfoMod
                |kFSEventStreamEventFlagItemChangeOwner
                |kFSEventStreamEventFlagItemXattrMod;
            if (((flags & fsEventChangeFlags) != kFSEventStreamEventFlagItemRemoved) && (flags & kFSEventStreamEventFlagItemRemoved)) {
                // A remove event has been coalesced. FSEvents seems to (stupidly) coalesce remove events that are then followed by create events.
                // Even though there are potential races with stat, we need to rely on it in this case.
                if (exists(paths[i])) {
                    negated_flags = kFSEventStreamEventFlagItemRemoved;
                }
            }
            
            const auto stopid = state->m_stopid;
            const bool historyDone = kFSEventStreamEventFlagHistoryDone == flags;
            if (!historyDone) {
                lastID = evids[i];
                fs::change_manager::emplace_back(*notes, fs::path{paths[i]}, fs::path{}, state, evids[i], to_event(flags & ~negated_flags), to_type(flags));
#if 0
                std::cout << evids[i] << "," << paths[i] << "," << flags << "," << (flags & ~negated_flags) << "\n";
#endif
            }
            
             // historyDone should be enough, but in case there's some stream bug we'll use the stopid as a fallback case
            if (stopid > 0 && (historyDone || lastID >= stopid)) {
                fs::change_manager::emplace_back(*notes, fs::path{}, fs::path{}, state, 0, fs::change_event::replay_end, fs::file_type::none);
                cancel(state);
                break; // further events are invalid
            }
        } // for
        
        if (!notes->empty()) {
            Dispatch{}(state, std::move(notes), lastID);
        }
    } catch(...) {
        prosoft::log_exception(__LINE__);
    }
}

extern CFRunLoopRef monitor_thread_run_loop;

fs::change_registration register_events_monitor(shared_state&& state, fs::change_callback&& cb, fs::error_code& ec);

void unregister_events_monitor(platform_state* state, fs::error_code& ec);

} // namespace v1
} // namespace filesystem
} // namespace prosoft

#endif // PS_CORE_FSEVENTS_MONITOR_INTERNAL_HPP
