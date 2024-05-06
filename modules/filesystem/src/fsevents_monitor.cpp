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

#if PSTEST_HARNESS
// Internal tests.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
using Catch::Matchers::WithinAbs;

using namespace prosoft::filesystem;
#include "fstestutils.hpp"

// Xcode 8.2.1: using a vector the following test will crash either with EXC_BAD_ACCESS or an ASAN violation in vector::emplace_back.
// Which crash depends on how many events are present. I reduced the callback to an empty emplace and the crash still occurred.
// I can find no reason in code for the crash so can only surmise there's an issue in std::vector.
//
// In the EXC_BAD_ACCESS scenerio, emplace_back ends up with a bad std::forward arg that looks to be due to stack corruption (address of 0x04, 0x01, etc).
//
// However I did create a unique type that was a duplicate of change_notification that was used for testing only
// and there was no crash with that type. The only difference being that nothing else referenced the test type.
// After many, many hours of debugging, I have no idea what is going on.
//
// As of Xcode 10.1 std::vector has no problem. Probably an issue with the Xcode 8 clang version.

void reduced_fsevents_callback_crash(ConstFSEventStreamRef, void*, size_t nevents, void*, const FSEventStreamEventFlags[], const FSEventStreamEventId[]) {
    fs::change_notifications notes;
    for (size_t i = 0; i < nevents; ++i) {
        notes.emplace_back(fs::path{}, fs::path{}, 0ULL, fs::change_event{}, fs::file_type{});
    }
}

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
    
    reduced_fsevents_callback_crash(nullptr, nullptr, paths.size(), paths.data(), flags.data(), ids.data());
    
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
        SECTION("notification") {
            auto state = std::make_shared<platform_state>();
            auto reg = change_manager::make_registration(state);
            CHECK(reg);
            auto note = change_manager::make_notification(PS_TEXT("test"), PS_TEXT(""), state.get(), change_event::created);
            CHECK(note.path() == path{PS_TEXT("test")});
            CHECK(note.renamed_to_path().empty());
            CHECK(note.event() == change_event::created);
            CHECK(note == reg);
            CHECK_FALSE(type_known(note));

            auto p = note.extract_path();
            CHECK(p.native() == PS_TEXT("test"));
            CHECK(note.path().empty());
            CHECK(note.event() == change_event::none);
            CHECK_FALSE(note == reg);

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::created, file_type::regular);
            CHECK(type_known(note));
            CHECK(created(note));

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::removed);
            CHECK(removed(note));

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::renamed);
            CHECK(renamed(note));

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::content_modified);
            CHECK(content_modified(note));
            CHECK(modified(note));

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::metadata_modified);
            CHECK(metadata_modified(note));
            CHECK(modified(note));

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::rescan_required);
            CHECK(rescan(note));
            CHECK(canceled(note));

            // test copy/move into vector
            change_notifications notes;
// Xcode 7&8 ASAN both fire "heap buffer overflow" for change_manager default copy and move if we use reserve.
// The output seems to point to some memory allocated by Catch, it may be the trigger as I cannot reproduce the problem with a simple test binary.
#if 0
            notes.reserve(2);
#endif
            note = change_manager::make_notification(PS_TEXT("test"), path{}, nullptr, change_event::rescan_required);
            notes.push_back(note);
            notes.emplace_back(change_manager::make_notification(PS_TEXT("test"), path{}, nullptr, change_event::rescan_required));
            CHECK(notes.size() == 2);
        }
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
        CHECK(p->m_lastid == kFSEventStreamEventIdSinceNow);
        CHECK(p->m_dispatch_q);
        CHECK(p->m_uuid);
        CHECK(p->m_rootfd > -1);
        CHECK(*p == *p);
        CHECK(canonical_root_path(p.get()).native() == "/");
    }
    
    WHEN("state is created with a valid path and restore state") {
        change_config cfg(change_event::all);
        const auto archive = change_state::serialize(path{"/"});
        CHECK_FALSE(archive.empty());
        auto state = change_state::serialize(archive);
        cfg.state = state.get();
        CHECK_FALSE(replay(cfg));
        error_code ec;
        auto p = std::make_unique<platform_state>("/", cfg, ec);
        CHECK(p);
        CHECK(!ec.value());
        CHECK(p->m_uuid);
        CHECK(p->m_lastid != kFSEventStreamEventIdSinceNow);
        CHECK(p->m_lastid == dynamic_cast<platform_state*>(cfg.state)->m_lastid);
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
        error_code root_ec;
        remove(root, root_ec);
        REQUIRE_FALSE(exists(root, root_ec));
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
        
        WHEN("history done flag is set") {
            platform_state state;
            paths.push_back(PS_TEXT(""));
            flags.push_back(kFSEventStreamEventFlagHistoryDone);
            ids.push_back(1);
            
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            CHECK(notes.size() == 0); // ignored as m_stopid is 0
            
            state.m_stopid = 2;
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE(notes.size() == 1);
            auto& n = notes[0];
            CHECK(n.event() == change_event::replay_end);
            CHECK(n.path().empty());
            CHECK(n.type() == file_type::none);
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
            REQUIRE_THAT(cfl, WithinAbs(1.0, 0.01));
            
            l = change_config::latency_type{2500};
            cfl = std::chrono::duration_cast<cfduration>(l).count();
            REQUIRE_THAT(cfl, WithinAbs(2.5, 0.01));
            
            l = change_config::latency_type{100};
            cfl = std::chrono::duration_cast<cfduration>(l).count();
            REQUIRE_THAT(cfl, WithinAbs(0.1, 0.01));
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
