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

// Useful to extract live events from a system:
// https://github.com/dlcowen/FSEventsParser

#include <CoreServices/CoreServices.h>
#include <sys/stat.h>

#include <atomic>
#include <future>
#include <limits>
#include <thread>
#include <vector>

#include "filesystem.hpp"
#include "filesystem_change_monitor.hpp"
#include "filesystem_private.hpp"
#include "fsmonitor_private.hpp"
#include "string/platform_convert.hpp"
#include "unique_resource.hpp"

namespace {

using cfduration = std::chrono::duration<double, std::chrono::seconds::period>;

constexpr FSEventStreamEventFlags platform_flag_defaults = kFSEventStreamCreateFlagWatchRoot|kFSEventStreamCreateFlagFileEvents|kFSEventStreamCreateFlagNoDefer;
constexpr FSEventStreamEventFlags valid_reserved_flags_mask = kFSEventStreamCreateFlagIgnoreSelf|kFSEventStreamCreateFlagMarkSelf;

FSEventStreamCreateFlags platform_flags(const fs::change_config& cfg) {
    constexpr fs::change_config cfg_default;
    static_assert(cfg_default.notification_latency > decltype(cfg_default.notification_latency){}, "Broken assumption");
    
    return platform_flag_defaults | (cfg.reserved_flags & valid_reserved_flags_mask);
};

struct fsevents_delete {
    void operator()(FSEventStreamRef p) noexcept {
        if (p) { FSEventStreamRelease(p); }
    }
};
using unique_fsstream = prosoft::unique_cftype<FSEventStreamRef, fsevents_delete>;

struct dispatch_delete {
    void operator()(dispatch_object_t p) noexcept {
        if (p) { dispatch_release(p); }
    }
};
using unique_dispatch_queue = std::unique_ptr<dispatch_queue_s, dispatch_delete>;

struct platform_state : public fs::change_state {
    fs::change_callback m_callback;
    unique_fsstream m_stream;
    unique_dispatch_queue m_dispatch_q;
    std::uintptr_t m_regid;
    int m_rootfd;
    // persistent values //
    prosoft::unique_cftype<CFUUIDRef> m_uuid; // set when constructed and then read-only
    std::atomic<FSEventStreamEventId> m_lastid;
    
    platform_state()
        : m_callback()
        , m_stream()
        , m_dispatch_q()
        , m_rootfd(-1)
        , m_uuid()
        , m_lastid(kFSEventStreamEventIdSinceNow) {}
    platform_state(const fs::path&, const fs::change_config&, fs::error_code&);
    virtual ~platform_state();
    
    operator FSEventStreamRef() const noexcept(noexcept(m_stream.get())) {
        return m_stream.get();
    }
};

using shared_state = std::shared_ptr<platform_state>;
shared_state get_shared_state(platform_state*);

fs::change_event to_event(FSEventStreamEventFlags flags) {
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

fs::file_type to_type(FSEventStreamEventFlags flags) {
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

void dispatch_events::callout_to_client(platform_state* state, fs::change_notifications* ownedNotes, FSEventStreamEventId lastNoteID) {
    std::unique_ptr<fs::change_notifications> n{ownedNotes};
    if (auto ss = get_shared_state(state)) {
        if (lastNoteID > 0) { // 0 is a reserved ID for root changed events and should not escape
            // Client updates the lastid field so the id is in sync with the actual events processed by the client.
            // Before the callback so the client can archive the state with the correct id.
            ss->m_lastid = lastNoteID;
        }
        PSIgnoreCppException(fs::change_manager::process_renames(*n); ss->m_callback(std::move(*n)));
    }
}

inline bool rescan_required(FSEventStreamEventFlags flags) {
    constexpr FSEventStreamEventFlags mask = kFSEventStreamEventFlagRootChanged|kFSEventStreamEventFlagMustScanSubDirs;
    return (flags & mask) != 0;
}

inline bool root_changed(FSEventStreamEventFlags flags) {
    return (flags & kFSEventStreamEventFlagRootChanged);
}

fs::path canonical_root_path(const platform_state* state) {
    char buf[PATH_MAX];
    if (0 == fcntl(state->m_rootfd, F_GETPATH, buf)) {
        return fs::path{buf};
    }
    return fs::path{};
}

template <class Dispatch = dispatch_events>
void fsevents_callback(ConstFSEventStreamRef, void* info, size_t nevents, void* evpaths, const FSEventStreamEventFlags evflags[], const FSEventStreamEventId evids[]) {
    // A bit faster than a full FS::status() call since it's not doing any type conversion.
    static auto exists = [](const char* p) noexcept {
        struct ::stat sb;
        const int err = ::stat(p, &sb);
        return err == 0 || (-1 == err && errno != ENOENT);
    };
    
    try {
        auto state = reinterpret_cast<platform_state*>(info);
        PSASSERT_NOTNULL(state);
        
        auto notes = std::make_unique<fs::change_notifications>();
        // Don't use reserve for now. There's an Xcode ASAN overflow (possibly related to Catch).
        
        FSEventStreamEventId lastID = 0;
        auto paths = reinterpret_cast<const char* *>(evpaths);
        for(size_t i = 0; i < nevents; ++i) {
            const auto flags = evflags[i];
            auto negated_flags = 0;
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
                
                notes->emplace_back(fs::change_manager::make_notification(std::move(rp), std::move(np), state, ev, fs::file_type::directory, evids[i]));
                
                PSASSERT(is_set(ev & fs::change_event::canceled), "Broken assumption");
                if (state->m_stream) { // can be null in test harness
                    FSEventStreamStop(state->m_stream.get());
                }
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
            
            if (kFSEventStreamEventFlagHistoryDone == flags) {
                continue;
            }
            
            lastID = evids[i];
            notes->emplace_back(fs::change_manager::make_notification(fs::path{paths[i]}, fs::path{}, state, to_event(flags & ~negated_flags), to_type(flags), evids[i]));
#if 0 && PSTEST_HARNESS
            std::cout << evids[i] << "," << paths[i] << "," << flags << "," << (flags & ~negated_flags) << "\n";
#endif
        } // for
        
        if (!notes->empty()) {
            Dispatch{}(state, std::move(notes), lastID);
        }
    } catch(...) {
        prosoft::log_exception(__LINE__);
    }
}

platform_state::platform_state(const fs::path& p, const fs::change_config& cfg, fs::error_code& ec)
    : platform_state() {
    using namespace fs;
    auto cfp = prosoft::to_CFString<fs::path::string_type>{}(p);
    if (!cfp) {
        ec = error_code(platform_error::convert_path, platform_category());
        return;
    }
    
    // XXX: handle cfg state restore
    
    const void* values = cfp.get();
    if (auto cfpa = prosoft::CF::unique_array{ CFArrayCreate(kCFAllocatorDefault, &values, 1, &kCFTypeArrayCallBacks) }) {
        FSEventStreamContext ctx {
            .version = 0,
            .info = this,
            .retain = nullptr,
            .release = nullptr,
            .copyDescription = nullptr
        };
        const auto latency = std::chrono::duration_cast<cfduration>(cfg.notification_latency).count();
        if (auto stream = FSEventStreamCreate(kCFAllocatorDefault, fsevents_callback, &ctx, cfpa.get(), m_lastid, latency, platform_flags(cfg))) {
            m_stream.reset(stream);
            struct stat sb;
            if (0 == lstat(p.c_str(), &sb)) {
                m_rootfd = open(p.c_str(), O_EVTONLY); // for root renames
                m_uuid.reset(FSEventsCopyUUIDForDevice(sb.st_dev));
                m_dispatch_q.reset(dispatch_queue_create("fse_monitor", nullptr));
            } else {
                ec = prosoft::system::system_error();
                m_stream.reset();
            }
            return;
        }
    }
    
    ec = fs::error_code(platform_error::monitor_create, platform_category());
}

platform_state::~platform_state() {
    if (-1 != m_rootfd) {
        close(m_rootfd);
    }
}

CFRunLoopRef monitor_thread_run_loop{};

void monitor_thread() {
    pthread_setname_np("fsevents_monitor");
    monitor_thread_run_loop = CFRunLoopGetCurrent();
    static auto tcb = [](auto,auto){};
    CFRunLoopTimerRef keepAliveTimer = CFRunLoopTimerCreate(kCFAllocatorDefault, 1.0, 1.0e10, 0, 0, tcb, nullptr);
    CFRunLoopAddTimer(monitor_thread_run_loop, keepAliveTimer, kCFRunLoopCommonModes);
    PSASSERT(CFRunLoopContainsTimer(monitor_thread_run_loop, keepAliveTimer, kCFRunLoopCommonModes), "WTF?");
    // CFRunLoop has an internal un-handled exception catch that just terminates. So trying to catch and continue is useless.
    CFRunLoopRun();
    CFRelease(keepAliveTimer);
    PSASSERT_UNREACHABLE("WTF?");
    throw std::logic_error("FSEvents thread exit!");
}

void start_monitor_thread() {
    struct start_thread {
        start_thread() {
            std::thread t{monitor_thread};
            t.detach();
            CFRunLoopRef rl;
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds{10});
            } while (!(rl = monitor_thread_run_loop)); // not strictly thread safe, but should be ok in practice
        }
    };
    
    static start_thread s{};
    PSASSERT_NOTNULL(monitor_thread_run_loop);
}

bool start_events_monitor(platform_state* state, fs::change_callback&& cb) {
    PSASSERT_NOTNULL(state);
    state->m_callback = std::move(cb);
    
    static auto do_start = [](platform_state* s) {
        FSEventStreamScheduleWithRunLoop(*s, monitor_thread_run_loop, kCFRunLoopCommonModes);
        if (FSEventStreamStart(*s)) {
            return true;
        } else {
            FSEventStreamInvalidate(*s);
            return false;
        }
    };
    
#if PSTEST_HARNESS // tests may use null
    if (!monitor_thread_run_loop || CFRunLoopGetCurrent() == monitor_thread_run_loop) {
        return do_start(state);
    }
#endif
    
    PSASSERT_NOTNULL(monitor_thread_run_loop);
    PSASSERT(CFRunLoopGetCurrent() != monitor_thread_run_loop, "BUG");
    
    using result_t = std::promise<bool>;
    auto p = new result_t{};
    auto f = p->get_future();
    
    CFRunLoopPerformBlock(monitor_thread_run_loop, kCFRunLoopDefaultMode, ^{
        std::unique_ptr<result_t> up{p};
        bool val = false;
        if (auto ss = get_shared_state(state)) {
            val = do_start(ss.get());
        }
        up->set_value(val);
    });
    CFRunLoopWakeUp(monitor_thread_run_loop);
    
    return f.get();
}

void stop_events_monitor(shared_state& ss) {
    PSASSERT(ss, "NULL");
    
    static auto do_stop = [](platform_state* s) {
        FSEventStreamStop(*s);
        FSEventStreamInvalidate(*s);
    };
    
#if PSTEST_HARNESS // tests may use null
    if (!monitor_thread_run_loop || CFRunLoopGetCurrent() == monitor_thread_run_loop) {
        do_stop(ss.get());
        return;
    }
#endif
    
    PSASSERT_NOTNULL(monitor_thread_run_loop);
    PSASSERT(CFRunLoopGetCurrent() != monitor_thread_run_loop, "BUG");
    
    using sync_t = std::promise<void>;
    auto p = new sync_t{};
    auto f = p->get_future();
    auto state = ss.get();
    
    CFRunLoopPerformBlock(monitor_thread_run_loop, kCFRunLoopDefaultMode, ^{
        std::unique_ptr<sync_t> up{p};
        do_stop(state);
        p->set_value();
    });
    CFRunLoopWakeUp(monitor_thread_run_loop);
    
    return f.get();
}

std::vector<shared_state> monitor_registrations;
std::mutex monitor_registration_lck;
using reg_guard = std::lock_guard<decltype(monitor_registration_lck)>;

enum class get_state_opts {
    none,
    delete_master
};

shared_state get_shared_state(platform_state* state, get_state_opts opts) {
    reg_guard lg{monitor_registration_lck};
    auto i = std::find_if(monitor_registrations.begin(), monitor_registrations.end(), [state](const shared_state& p) {
        return state == p.get();
    });
    if (i != monitor_registrations.end()) {
        shared_state ss{*i};
        if (get_state_opts::delete_master == opts) {
            monitor_registrations.erase(i);
        }
        return ss;
    } else {
        return shared_state{};
    }
}

shared_state get_shared_state(platform_state* state) {
    return get_shared_state(state, get_state_opts::none);
}

fs::change_registration register_events_monitor(shared_state&& state, fs::change_callback&& cb, fs::error_code& ec) {
    auto reg = fs::change_manager::make_registration(state);
    auto p = state.get();
    PSASSERT_NOTNULL(p);
    {
        reg_guard lg{monitor_registration_lck};
        monitor_registrations.emplace_back(std::move(state));
    }
    
    if (start_events_monitor(p, std::move(cb))) {
        ec.clear();
        return reg;
    } else {
        ec = fs::error_code(fs::platform_error::monitor_start, fs::platform_category());
        get_shared_state(p, get_state_opts::delete_master);
        return fs::change_registration{};
    }
}

void unregister_events_monitor(platform_state* state, fs::error_code& ec) {
    PSASSERT_NOTNULL(state);
    if (shared_state ss = get_shared_state(state, get_state_opts::delete_master)) {
        stop_events_monitor(ss);
    } else {
        ec = fs::error_code{ENOENT, std::system_category()};
    }
}

} // anon

namespace prosoft {
namespace filesystem {
inline namespace v1 {

std::ostream& operator<<(std::ostream& os, const change_state&) { // TODO
    //auto& fes = dynamic_cast<const platform_state&>(state);
    return os;
}

bool operator==(const change_state& lhs, const change_state& rhs) {
    return &lhs == &rhs; // All copies of change_registration point to a shared platform state
}

change_registration monitor(const path& p, const change_config& cfg, change_callback cb, error_code& ec) {
    if (p.empty() || !valid(cfg) || !cb) {
        ec = einval();
        return change_registration{};
    }
    
    // We'd need to use dispatch (or kqueue for *BSD support) to watch file and non-recursive dir changes. (Though we could filter FSEvents for root dir changes).
    ec = fs::error_code(platform_error::not_supported, platform_category());
    return change_registration{};
}

change_registration recursive_monitor(const path& p, const change_config& cfg, change_callback cb, error_code& ec) {
    if (p.empty() || !valid(cfg) || !cb) {
        ec = einval();
        return change_registration{};
    }
    
    auto state = std::make_shared<platform_state>(p, cfg, ec);
    if (!ec) {
        start_monitor_thread();
        return register_events_monitor(std::move(state), std::move(cb), ec);
    }
    
    return change_registration{};
}

void stop(change_state* state, error_code& ec) {
    PSASSERT_NOTNULL(state);
    if (auto fsep = dynamic_cast<platform_state*>(state)) {
        unregister_events_monitor(fsep, ec);
        // fsep is probably bad now
    } else {
        throw std::bad_cast{}; // should never happen or something's gone south
    }
}

void change_manager::process_renames(fs::change_notifications& notes) {
    size_t count = notes.size();
    for (size_t i = 0; i < count; ++i) {
        auto& n = notes[i];
        // XXX: this fails if FS events are merged (e.g. create and rename) as the merged event will not have the same id as the pure rename event
        if (is_set(n.event() & fs::change_event::renamed) && n.m_eventid > 0) {
            for (size_t j = i+1; j < count; ++j) {
                auto& nn = notes[j];
                if (is_set(nn.event() & fs::change_event::renamed) && n.m_eventid == nn.m_eventid) {
                    if (!is_set(nn.event() & fs::change_event::removed)) {
                        // rename within the tree and within the same latency period.
                        n.m_newpath = std::move(nn.path());
                        notes.erase(notes.begin()+j); // need to maintain order, swap() trick with last() will not work
                        --count;
                    } else {
                        nn.m_event &= ~fs::change_event::renamed;
                    }
                    break;
                }
            }
        }
        
        // Other methods of detecting a rename (such as using stat on the paths of renamed events) are prone to race conditions.
    }
}

} // v1
} // filesystem
} // prosoft

#if PSTEST_HARNESS
// Internal tests.
#include "catch.hpp"

using namespace prosoft::filesystem;
#include "src/fstestutils.hpp"

// Xcode 8.2.1: using a vector the following test will crash either with EXC_BAD_ACCESS or an ASAN violation in vector::emplace_back.
// Which crash depends on how many events are present. I reduced the callback to a an empty emplace and the crash still occurred.
// I can find no reason in code for the crash so can only surmise there's an issue in std::vector.
//
// In the EXC_BAD_ACCESS scenerio, emplace_back ends with a bad std::forward arg that looks to be due to stack corruption.
//
// However I did create a unique type that was a duplicate of change_notification that was used for testing only
// and there was no crash with that type. The only difference being that nothing else referenced the test type.
// After many, many hours of debugging, I have no idea what is going on.
TEST_CASE("fsevents_vector_crash") {
    struct TestDispatcher {
        void operator()(platform_state*, std::unique_ptr<fs::change_notifications>, FSEventStreamEventId) {
        }
    };
    
    std::vector<const char*> paths;
    std::vector<FSEventStreamEventFlags> flags;
    std::vector<FSEventStreamEventId> ids;
    const path::string_type p1{"/private/var/folders/bs/b0d7mq1x7gb73k66lvxgfcsm0000gn/T/fs17test/1"};
    const path::string_type p2{"/private/var/folders/bs/b0d7mq1x7gb73k66lvxgfcsm0000gn/T/fs17test/2"};
    const path::string_type p3{"/private/var/folders/bs/b0d7mq1x7gb73k66lvxgfcsm0000gn/T/fs17test/2/1"};
    const path::string_type p4{"/private/var/folders/bs/b0d7mq1x7gb73k66lvxgfcsm0000gn/T/fs17test/2/2"};
    
    paths.push_back(p1.c_str());
    flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemIsFile);
    ids.push_back(18158642889452409530ULL);
    
    paths.push_back(p2.c_str());
    flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemIsDir);
    ids.push_back(18158642889452409533ULL);
    
    paths.push_back(p3.c_str());
    flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemXattrMod|kFSEventStreamEventFlagItemIsFile);
    ids.push_back(18158642889452409539ULL);
    
    paths.push_back(p4.c_str());
    flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemIsFile);
    ids.push_back(18158642889452409540ULL);
    
    paths.push_back(p1.c_str());
    flags.push_back(kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemIsFile);
    ids.push_back(18158642889452409541ULL);
    
    platform_state state;
    fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
}

class set_monitor_runloop_guard {
    CFRunLoopRef oldval;
public:
    set_monitor_runloop_guard(CFRunLoopRef rl) {
        oldval = monitor_thread_run_loop;
        monitor_thread_run_loop = rl;
    }
    ~set_monitor_runloop_guard() {
        monitor_thread_run_loop = oldval;
    }
    PS_DISABLE_COPY(set_monitor_runloop_guard);
};

TEST_CASE("filesystem_monitor_internal") {
    SECTION("common") {
    #include "fsmonitor_tests_i.cpp"
    }
    
    WHEN("state is created with an invalid path") {
        change_config cfg(change_event::all);
        error_code ec;
        auto p = std::make_unique<platform_state>("test", cfg, ec);
        CHECK(p);
        CHECK(ec.value() != 0);
        CHECK_FALSE(p->m_stream);
        CHECK_FALSE(p->m_dispatch_q);
        CHECK_FALSE(p->m_uuid);
        CHECK(p->m_rootfd == -1);
        CHECK(*p == *p);
        CHECK(canonical_root_path(p.get()).empty());
    }
    
    WHEN("state is created with a valid path") {
        change_config cfg(change_event::all);
        error_code ec;
        auto p = std::make_unique<platform_state>("/", cfg, ec);
        CHECK(p);
        CHECK(!ec.value());
        CHECK(p->m_stream);
        CHECK(p->m_dispatch_q);
        CHECK(p->m_uuid);
        CHECK(p->m_rootfd > -1);
        CHECK(*p == *p);
        CHECK(canonical_root_path(p.get()).native() == "/");
    }
    
    WHEN("state is not registered") {
        error_code ec;
        auto p = std::make_unique<platform_state>();
        stop(p.get(), ec);
        CHECK(ec.value() != 0);
        CHECK_FALSE(get_shared_state(p.get()));
    }
    
    WHEN("registering a monitor") {
        change_config cfg(change_event::all);
        error_code ec;
        auto ss = std::make_shared<platform_state>("/", cfg, ec);
        auto p = ss.get();
        
        set_monitor_runloop_guard rlg{CFRunLoopGetCurrent()}; // ignore the actual runloop
        auto reg = register_events_monitor(std::move(ss), [](auto){}, ec);
        CHECK(reg);
        CHECK_FALSE(ss);
        ss = get_shared_state(p);
        CHECK(ss.use_count() == 2);
        ec.clear();
        unregister_events_monitor(p, ec);
        CHECK(!ec.value());
        CHECK_FALSE(get_shared_state(p));
        CHECK(ss.use_count() == 1);
    }
    
    WHEN("registering a monitor fails") {
        change_config cfg(change_event::all);
        error_code ec;
        auto ss = std::make_shared<platform_state>("/", cfg, ec);
        auto p = ss.get();
        
        set_monitor_runloop_guard rlg{nullptr}; // Force FSEventStreamStart to fail (will print console messages)
        auto reg = register_events_monitor(std::move(ss), [](auto){}, ec);
        CHECK_FALSE(reg);
        CHECK(ec.value() == platform_error::monitor_start);
        CHECK_FALSE(get_shared_state(p));
    }
    
    SECTION("conversion") {
        CHECK(to_event(0) == change_event::none);
        CHECK(to_event(kFSEventStreamEventFlagMount) == change_event::rescan);
        CHECK(to_event(kFSEventStreamEventFlagUnmount) == change_event::rescan);
        // Mount/Unmount are should be mutually exclusive, check that assumption.
        CHECK(to_event(kFSEventStreamEventFlagMount|kFSEventStreamEventFlagItemCreated) == change_event::rescan);
        CHECK(to_event(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemModified|kFSEventStreamEventFlagItemRemoved) == change_event::removed);
        CHECK(to_event(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemModified) == (change_event::created|change_event::content_modified));
        CHECK(to_event(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemRenamed) == (change_event::created|change_event::renamed));
        CHECK(to_event(kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemRenamed) == (change_event::removed|change_event::renamed));
        constexpr auto allmod = change_event::content_modified|change_event::metadata_modified;
        CHECK(to_event(kFSEventStreamEventFlagItemModified|kFSEventStreamEventFlagItemInodeMetaMod) == allmod);
        CHECK(to_event(kFSEventStreamEventFlagItemModified|kFSEventStreamEventFlagItemFinderInfoMod) == allmod);
        CHECK(to_event(kFSEventStreamEventFlagItemModified|kFSEventStreamEventFlagItemChangeOwner) == allmod);
        CHECK(to_event(kFSEventStreamEventFlagItemModified|kFSEventStreamEventFlagItemXattrMod) == allmod);
        
        CHECK(to_type(0) == file_type::none);
        CHECK(to_type(kFSEventStreamEventFlagItemIsFile) == file_type::regular);
        CHECK(to_type(kFSEventStreamEventFlagItemIsDir) == file_type::directory);
        CHECK(to_type(kFSEventStreamEventFlagItemIsSymlink) == file_type::symlink);
        
        CHECK(rescan_required(kFSEventStreamEventFlagRootChanged));
        CHECK(rescan_required(kFSEventStreamEventFlagMustScanSubDirs));
        CHECK(rescan_required(kFSEventStreamEventFlagRootChanged|kFSEventStreamEventFlagMustScanSubDirs));
        CHECK_FALSE(rescan_required(kFSEventStreamEventFlagItemCreated));
        
        CHECK(root_changed(kFSEventStreamEventFlagRootChanged));
        CHECK_FALSE(root_changed(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagMustScanSubDirs));
    }
    
    SECTION("event handler") {
        static change_notifications notes;
        static FSEventStreamEventId lastID{};
        struct TestDispatcher {
            void operator()(platform_state*, std::unique_ptr<fs::change_notifications> nn, FSEventStreamEventId lastNoteID) {
                notes = std::move(*nn.release());
                lastID = lastNoteID;
            }
        };
        
        std::vector<const char*> paths;
        std::vector<FSEventStreamEventFlags> flags;
        std::vector<FSEventStreamEventId> ids;
        
        const auto root = canonical(temp_directory_path()) / PS_TEXT("fs17test");
        error_code ec;
        remove(root, ec);
        REQUIRE_FALSE(exists(root, ec));
        notes.clear();
        lastID = 0;
        
        WHEN("there are no events") {
            platform_state state;
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            CHECK(notes.empty());
        }
        
        WHEN("a root change event is present") {
            platform_state state;
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagRootChanged);
            flags.push_back(0);
            paths.push_back("test");
            flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE_FALSE(notes.empty());
            auto& n = notes[0];
            CHECK(n.event() == (change_event::removed|change_event::canceled|change_event::rescan));
            CHECK(n.path() == root);
            CHECK(n.renamed_to_path().empty());
            CHECK(notes.size() == 1); // all events after the canceled event should be dropped
        }
        
        WHEN("a scan sub dirs event is present") {
            platform_state state;
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagMustScanSubDirs);
            ids.push_back(0);
            paths.push_back("test");
            flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE_FALSE(notes.empty());
            auto& n = notes[0];
            CHECK(n.event() == (change_event::canceled|change_event::rescan));
            CHECK(n.path() == root);
            CHECK(n.renamed_to_path().empty());
            CHECK(notes.size() == 1); // all events after the canceled event should be dropped
        }
        
        WHEN("root is renamed") {
            create_file(root);
            REQUIRE(exists(root));
            
            change_config cfg(change_event::all);
            error_code ec;
            auto ss = std::make_unique<platform_state>(root, cfg, ec);
            auto state = ss.get();
            REQUIRE(state->m_rootfd > -1);
            
            const auto np = canonical(temp_directory_path()) / PS_TEXT("fs17test2");
            rename(root, np);
            REQUIRE_FALSE(exists(root, ec));
            REQUIRE(exists(np));
            
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagRootChanged);
            ids.push_back(0);
            fsevents_callback<TestDispatcher>(nullptr, state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE(remove(np)); // remove before any other REQUIREs
            REQUIRE_FALSE(notes.empty());
            auto& n = notes[0];
            CHECK(n.event() == (change_event::renamed|change_event::canceled));
            CHECK(n.path() == root);
            CHECK(n.renamed_to_path() == np);
            CHECK(n.type() == file_type::directory);
        }
        
        WHEN("a remove event is present with no other events and the item does not exist") {
            platform_state state;
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE_FALSE(notes.empty());
            auto& n = notes[0];
            CHECK(n.event() == change_event::removed);
            CHECK(n.path() == root);
            CHECK(n.type() == file_type::regular);
        };
        
        WHEN("a remove event is present with no other events and the item does exist") {
            platform_state state;
            create_file(root);
            REQUIRE(exists(root));
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE_FALSE(notes.empty());
            auto& n = notes[0];
            CHECK(n.event() == change_event::removed); // event is still removed as only stat the file if other events are present
            CHECK(n.path() == root);
            CHECK(n.type() == file_type::regular);
        };
        
        WHEN("a remove event is present with other events and the item does not exist") {
            platform_state state;
            error_code ec;
            REQUIRE_FALSE(exists(root, ec));
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            auto& n = notes[0];
            CHECK(n.event() == change_event::removed);
            CHECK(n.path() == root);
            CHECK(n.type() == file_type::regular);
        };
        
        WHEN("a remove event is present with other events and the item exists") {
            platform_state state;
            create_file(root);
            REQUIRE(exists(root));
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE(remove(root));
            auto& n = notes[0];
            CHECK(n.event() == change_event::created); // event is not removed as the file was stat'd
            CHECK(n.path() == root);
            CHECK(n.type() == file_type::regular);
        };
        
        WHEN("a history done event is present") {
            platform_state state;
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagHistoryDone);
            ids.push_back(0);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            CHECK(notes.empty());
        }
        
        WHEN("a mount/unmount event is present") {
            platform_state state;
            const auto p = root / PS_TEXT("test");
            paths.push_back(p.c_str());
            flags.push_back(kFSEventStreamEventFlagMount);
            ids.push_back(1);
            paths.push_back(p.c_str());
            flags.push_back(kFSEventStreamEventFlagUnmount);
            ids.push_back(2);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE_FALSE(notes.empty());
            auto* n = &notes[0];
            CHECK(n->event() == change_event::rescan);
            CHECK(n->path() == p);
            CHECK(n->renamed_to_path().empty());
            CHECK_FALSE(type_known(*n));
            REQUIRE(notes.size() > 1);
            n = &notes[1];
            CHECK(n->event() == change_event::rescan);
            CHECK(n->path() == p);
            CHECK(n->renamed_to_path().empty());
            CHECK_FALSE(type_known(*n));
            CHECK(lastID == 2);
        }
        
        WHEN("there are 2 rename events with matching ids") {
            platform_state state;
            const auto p = root / PS_TEXT("test");
            const auto np = root / PS_TEXT("test2");
            paths.push_back(p.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            paths.push_back(np.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            CHECK(notes.size() == 2);
            fs::change_manager::process_renames(notes);
            REQUIRE(notes.size() == 1);
            auto& n = notes[0];
            CHECK(n.event() == change_event::renamed);
            CHECK(n.path() == p);
            CHECK(n.renamed_to_path() == np);
            CHECK(n.type() == file_type::regular);
            CHECK(lastID == 1);
        }
        
        WHEN("there are 2 rename events with matching ids and the 2nd event has the removed flag set") {
            platform_state state;
            const auto p = root / PS_TEXT("test");
            const auto np = root / PS_TEXT("test2");
            paths.push_back(p.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            paths.push_back(np.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemIsFile|kFSEventStreamEventFlagItemRemoved);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE(notes.size() == 2);
            auto* n = &notes[0];
            CHECK(n->event() == change_event::renamed);
            CHECK(n->renamed_to_path().empty());
            n = &notes[1];
            CHECK(n->event() == (change_event::renamed|change_event::removed));
            fs::change_manager::process_renames(notes);
            REQUIRE(notes.size() == 2);
            n = &notes[0];
            CHECK(n->event() == change_event::renamed);
            CHECK(n->renamed_to_path().empty());
            n = &notes[1];
            CHECK(n->event() == change_event::removed);
        }
    }
    
    SECTION("platform assumptions") {
        WHEN("converting config latency to native time") {
            change_config::latency_type l{1000};
            auto cfl = std::chrono::duration_cast<cfduration>(l).count();
            CHECK(cfl == Approx(1.0));
            
            l = change_config::latency_type{2500};
            cfl = std::chrono::duration_cast<cfduration>(l).count();
            CHECK(cfl == Approx(2.5));
            
            l = change_config::latency_type{100};
            cfl = std::chrono::duration_cast<cfduration>(l).count();
            CHECK(cfl == Approx(0.1));
        }
    
        WHEN("when a file is renamed") {
            const auto temp = canonical(temp_directory_path()); // /var symlinks to /private/var
            
            const auto p = create_file(temp / PS_TEXT("fs17test"));
            REQUIRE(exists(p));
            
            int fd = open(p.c_str(), O_EVTONLY);
            CHECK(fd > -1);
            
            const auto np = temp / PS_TEXT("fs17test2");
            int ec = rename(p.c_str(), np.c_str());
            CHECK(ec == 0);
            
            char buf[PATH_MAX];
            ec = fcntl(fd, F_GETPATH, buf);
            CHECK(ec == 0);
            CHECK(np.native() == buf);
            close(fd);
            
            REQUIRE(remove(np));
        }
        
        WHEN("when a file is deleted") {
            const auto temp = canonical(temp_directory_path()); // /var symlinks to /private/var
            const auto p = create_file(temp / PS_TEXT("fs17test"));
            REQUIRE(exists(p));
            
            int fd = open(p.c_str(), O_EVTONLY);
            CHECK(fd > -1);
            
            REQUIRE(remove(p));
            
            char buf[PATH_MAX];
            int ec = fcntl(fd, F_GETPATH, buf);
            CHECK(ec == 0);
            CHECK(p.native() == buf);
            close(fd);
        }
    }
}
#endif // PSTEST_HARNESS
