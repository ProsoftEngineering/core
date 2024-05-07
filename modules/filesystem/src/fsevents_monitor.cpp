// Copyright © 2016-2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <future>
#include <thread>
#include <vector>

#include "fsevents_monitor_internal.hpp"
#include <prosoft/core/include/string/platform_convert.hpp>
#include <prosoft/core/config/config_analyzer.h>

#include <nlohmann/json.hpp>

using namespace prosoft::filesystem;

namespace {

using json = nlohmann::json;

constexpr FSEventStreamEventFlags platform_flag_defaults = kFSEventStreamCreateFlagWatchRoot|kFSEventStreamCreateFlagFileEvents|kFSEventStreamCreateFlagNoDefer;
constexpr FSEventStreamEventFlags valid_reserved_flags_mask = kFSEventStreamCreateFlagIgnoreSelf|kFSEventStreamCreateFlagMarkSelf;

FSEventStreamCreateFlags platform_flags(const fs::change_config& cfg) {
    constexpr fs::change_config cfg_default;
    static_assert(cfg_default.notification_latency > decltype(cfg_default.notification_latency){}, "Broken assumption");
    
    FSEventStreamCreateFlags clear_flags{};
    if (cfg.notification_latency > cfg_default.notification_latency) {
        clear_flags = kFSEventStreamCreateFlagNoDefer; // enable batch mode
    }
    
    return (platform_flag_defaults & ~clear_flags) | (cfg.reserved_flags & valid_reserved_flags_mask);
};

enum class get_state_opts {
    none,
    delete_master
};

} // namespace

namespace prosoft {
namespace filesystem {
inline namespace v1 {

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

} // namespace v1
} // namespace filesystem
} // namespace prosoft

namespace {

dev_t device(const fs::path& p, fs::error_code& ec) {
    struct stat sb;
    if (0 == lstat(p.c_str(), &sb)) {
        ec.clear();
        return sb.st_dev;
    } else {
        ec = prosoft::system::system_error();
        return 0; // assuming major,minor of 0,0 is invalid
    }
}

FSEventStreamEventId current_eventid(dev_t dev) {
    return FSEventsGetLastEventIdForDeviceBeforeTime(dev, CFAbsoluteTimeGetCurrent() + kCFAbsoluteTimeIntervalSince1970);
}

FSEventStreamEventId eventid(const fs::change_config& cc, CFUUIDRef fsUUID, std::error_code& ec) {
    if (auto pc = dynamic_cast<const platform_state*>(cc.state)) {
        if (pc->m_uuid && fsUUID) {
            const auto evid = pc->m_lastid.load();
            if (CFEqual(pc->m_uuid.get(), fsUUID) && evid > 0) {
                return evid;
            } else {
                ec.assign(EINVAL, std::system_category());
            }
        }
    }
    return kFSEventStreamEventIdSinceNow;
}

class gstate {
    dispatch_queue_t rootq = nullptr;
public:
    std::vector<shared_state> registrations;
    std::mutex lck;
    
    gstate() = default;
    PS_DISABLE_COPY(gstate);
    PS_DISABLE_MOVE(gstate);
    
    dispatch_queue_t root_queue(dev_t);
};

PS_NOINLINE
gstate& gs() {
    prosoft::intentional_leak_guard lg;
    static auto gp = new gstate;
    return *gp;
}

using g_guard = std::lock_guard<decltype(gstate::lck)>;

#if MAC_OS_X_VERSION_10_12 && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12
PS_NOINLINE
dispatch_queue_t gstate::root_queue(dev_t) {
    // In the future we could use device-specific roots for better granularity (if needed).
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        this->rootq = dispatch_queue_create_with_target("ps_fse_root", nullptr, nullptr);
    });
    return this->rootq;
}
#endif

PS_WARN_UNUSED_RESULT
dispatch_queue_t make_monitor_queue(dev_t dev) {
#if MAC_OS_X_VERSION_10_12 && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12
    return dispatch_queue_create_with_target("ps_fse_client", nullptr, gs().root_queue(dev));
#else
    return dispatch_queue_create("ps_fse_client", nullptr);
#endif
}

constexpr const char* json_key_uuid = "uuid";
constexpr const char* json_key_evid = "evid";

} // namespace

namespace prosoft {
namespace filesystem {
inline namespace v1 {

platform_state::platform_state(const fs::path& p, const fs::change_config& cfg, fs::error_code& ec)
    : platform_state() {
    using namespace fs;
    auto cfp = prosoft::to_CFString<fs::path::string_type>{}(p);
    if (!cfp) {
        ec = error_code(platform_error::convert_path, platform_category());
        return;
    }
    
    const void* values = cfp.get();
    if (auto cfpa = prosoft::CF::unique_array{ CFArrayCreate(kCFAllocatorDefault, &values, 1, &kCFTypeArrayCallBacks) }) {
        FSEventStreamContext ctx {
            .version = 0,
            .info = this,
            .retain = nullptr,
            .release = nullptr,
            .copyDescription = nullptr
        };
        const auto dev = device(p, ec);
        if (dev != 0) {
            m_uuid.reset(FSEventsCopyUUIDForDevice(dev));
            if (!m_uuid) {
                // most likely a read-only volume
                ec = fs::error_code(ENOTSUP, std::system_category());
                return;
            }
            
            m_lastid = eventid(cfg, m_uuid.get(), ec);
            if (m_lastid == kFSEventStreamEventIdSinceNow && ec) {
                ec = fs::error_code(platform_error::monitor_thaw, platform_category());
                m_uuid.reset();
                return;
            }
            
            if (replay(cfg)) {
                m_stopid = current_eventid(dev);
                if (m_stopid < m_lastid) {
                    // This can happen if the clock drifts/resets. NTP is off. Or the user makes an explicit clock change.
                    ec = fs::error_code(platform_error::monitor_replay_past, platform_category());
                    m_uuid.reset();
                    return;
                }
            } else {
                PSASSERT(m_stopid == 0, "Broken assumption");
            }
            
            const auto latency = std::chrono::duration_cast<cfduration>(cfg.notification_latency).count();
            if (auto stream = FSEventStreamCreate(kCFAllocatorDefault, fsevents_callback, &ctx, cfpa.get(), m_lastid, latency, platform_flags(cfg))) {
                m_stream.reset(stream);
                m_dispatch_q.reset(make_monitor_queue(dev));
                m_rootfd = open(p.c_str(), O_EVTONLY); // for root renames
                using namespace prosoft;
                CF::unique_string uid{CFUUIDCreateString(kCFAllocatorDefault, m_uuid.get())};
                m_uuid_str = from_CFString<std::string>{}(uid.get());
                return;
            } else {
                m_uuid.reset();
            }
        }
    }
    
    ec = fs::error_code(platform_error::monitor_create, platform_category());
}

platform_state::platform_state(const std::string& s, fs::change_thaw_options opts)
    : platform_state() {    
    // throws for invalid json, but not empty string
    auto j = !s.empty() ? json::parse(s) : json{};
    auto i = j.find(json_key_uuid);
    if (i != j.end()) {
        if (auto ss = prosoft::CF::unique_string{CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, i->get_ref<const std::string&>().c_str(), kCFStringEncodingASCII, kCFAllocatorNull)}) {
            m_uuid.reset(CFUUIDCreateFromString(kCFAllocatorDefault, ss.get()));
            m_uuid_str = *i;
        }
    }
    i = j.find(json_key_evid);
    if (i != j.end()) {
        m_lastid = i->get<FSEventStreamEventId>();
        if (is_set(opts & fs::change_thaw_options::replay_to_current_event)) {
            m_stopid = wants_replay;
        }
    }
}

platform_state::~platform_state() {
    if (-1 != m_rootfd) {
        close(m_rootfd);
    }
}

fs::change_event_id platform_state::last_event_id() const {
    return m_lastid.load();
}

std::string platform_state::serialize() const {
    return this->serialize(m_lastid.load());
}

std::string platform_state::serialize(fs::change_event_id evid) const {
    if (!m_uuid_str.empty() && evid > 0) {
        json j {
            {json_key_uuid, m_uuid_str},
            {json_key_evid, evid}
        };
        return j.dump();
    }
    return "";
}

CFRunLoopRef monitor_thread_run_loop;

} // namespace v1
} // namespace filesystem
} // namespace prosoft

namespace {

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
    
    // tests may use null
    if (!monitor_thread_run_loop || CFRunLoopGetCurrent() == monitor_thread_run_loop) {
        return do_start(state);
    }
    
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
    
    // tests may use null
    if (!monitor_thread_run_loop || CFRunLoopGetCurrent() == monitor_thread_run_loop) {
        do_stop(ss.get());
        return;
    }
    
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

} // namespace

namespace prosoft {
namespace filesystem {
inline namespace v1 {

shared_state get_shared_state(platform_state* state, get_state_opts opts) {
    auto& g = gs();
    g_guard lg{g.lck};
    auto i = std::find_if(g.registrations.begin(), g.registrations.end(), [state](const shared_state& p) {
        return state == p.get();
    });
    if (i != g.registrations.end()) {
        shared_state ss{*i};
        if (get_state_opts::delete_master == opts) {
            g.registrations.erase(i);
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
        auto& g = gs();
        g_guard lg{g.lck};
        g.registrations.emplace_back(std::move(state));
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

struct change_token {
    dev_t m_device;
    std::string m_uuid;
    
    change_token(const path&, error_code&);
};

change_token::change_token(const path& p, error_code& ec) {
    m_device = device(p, ec);
    if (0 != m_device) {
        if (auto uuid = unique_cftype<CFUUIDRef>{FSEventsCopyUUIDForDevice(m_device)}) {
            using namespace prosoft;
            if (CF::unique_string us{CFUUIDCreateString(kCFAllocatorDefault, uuid.get())}) {
                m_uuid = from_CFString<std::string>{}(us.get());
            } else {
                ec.assign(ENOMEM, std::system_category());
            }
        } else {
            ec.assign(ENOTSUP, std::system_category());
        }
    }
}

change_state::token_type change_state::serialize_token(const path& p, error_code& ec) {
    auto ct = std::make_shared<change_token>(p, ec);
    if (ec.value() == 0) {
        return ct;
    } else {
        return {};
    }
}

change_state::token_type change_state::serialize_token(const path& p) {
    fs::error_code ec;
    auto s = serialize_token(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not serialize filesystem monitor state", p, ec));
    return s;
}

std::string change_state::serialize(const token_type& token, error_code& ec) {
    (void)ec;   // unused
    if (token) {
        json j {
            {json_key_uuid, token->m_uuid},
            {json_key_evid, current_eventid(token->m_device)}
        };
        return j.dump();
    }
    return "";
}

std::string change_state::serialize(const token_type& token) {
    fs::error_code ec;
    auto s = serialize(token, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not serialize filesystem monitor state", ec));
    return s;
}
    
std::string change_state::serialize(const path& p, error_code& ec) {
    if (auto token = serialize_token(p, ec)) {
        return serialize(token, ec);
    }
    return "";
}

std::string change_state::serialize(const path& p) {
    fs::error_code ec;
    auto s = serialize(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not serialize filesystem monitor state", p, ec));
    return s;
}
    
std::unique_ptr<change_state> change_state::serialize(const std::string& s, change_thaw_options opts) {
#if PS_HAVE_MAKE_UNIQUE
    return std::make_unique<platform_state>(s, opts);
#else
    return std::unique_ptr<platform_state>{new platform_state{s, opts}};
#endif
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
