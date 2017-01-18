// Copyright © 2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <atomic>
#include <thread>
#include <unordered_set>

#include "stable_hash_wrapper.hpp"

#include "filesystem.hpp"
#include "filesystem_private.hpp"
#include "filesystem_change_monitor.hpp"

#if PS_HAVE_FILESYSTEM_CHANGE_ITERATOR

namespace fs = prosoft::filesystem::v1;

namespace {

fs::change_event to_events(fs::directory_options opts) {
    using namespace fs;
    fs::change_event ev = change_event::rescan_required;
    if (is_set(opts & directory_options::include_created_events)) {
        ev |= change_event::created|change_event::renamed;
    }
    if (is_set(opts & directory_options::include_modified_events)) {
        ev |= change_event::content_modified;
    }
    return ev;
}

using fsiterator_state = fs::ifilesystem::iterator_state;

class state : public fsiterator_state {
    using lock_type = std::mutex;
    using lock_guard = std::lock_guard<lock_type>;
    using callback_type = decltype(fs::change_iterator_config::callback);
    
    fs::change_registration m_reg;
    lock_type mutable m_lock;
    std::unordered_set<prosoft::stable_hash_wrapper<fs::path>> m_entries;
    std::atomic_bool m_done{false}; // no more events will be received
    callback_type m_callback;
    
    using notification_ptr = fs::change_notification*;
    static inline notification_ptr filter(fs::change_notification& n, fs::change_event ev) {
        return is_set(n.event() & ev) ? &n : nullptr;
    }
    static void filter(fs::change_notification&&, fs::change_event) = delete;
    
    void add(notification_ptr);
    
    void abort() noexcept;
    
    void notify() noexcept;
public:
    using fsiterator_state::fsiterator_state;
    
    state(const fs::path&, fs::directory_options, fs::change_iterator_config&&, fs::error_code&);
    virtual ~state();
    
    bool done() const {
        return m_done;
    }
    
    const fs::change_registration& registration() const noexcept {
        return m_reg;
    }
    
    virtual fs::path next(prosoft::system::error_code&) override;
    virtual bool at_end() const override;
};

state::state(const fs::path& p, fs::directory_options opts, fs::change_iterator_config&& c, fs::error_code& ec)
    : fsiterator_state(p, opts, ec)
    , m_reg() {
    if (!ec) {
        using namespace fs;
        const auto events = to_events(opts);
        
        change_config cfg;
        cfg.notification_latency = std::chrono::duration_cast<change_config::latency_type>(c.latency);
        m_reg = recursive_monitor(p, cfg, [this, events](change_notifications&& notes) {
            for (auto& n : notes) {
                add(filter(n, events));
            }
        }, ec);
        m_callback = std::move(c.callback);
    }
}

state::~state() {
    abort();
}

void state::add(notification_ptr np) {
    using namespace fs;
    if (np) {
        auto& n = *np;
        if (rescan(n) || canceled(n)) {
            m_done = true;
            abort();
            notify();
            return;
        }
        
        {
            lock_guard lg{m_lock};
            m_entries.emplace(n.extract_path());
        }
        notify();
    }
}

void state::abort() noexcept {
    fs::unique_change_registration u{std::move(m_reg)};
}

void state::notify() noexcept {
    if (m_callback) {
        PSIgnoreCppException(m_callback(m_reg));
    }
}

fs::path state::next(prosoft::system::error_code&) {
    {
        lock_guard lg{m_lock};
        if (!m_entries.empty()) {
            auto i = m_entries.begin();
            fs::path p{i->extract()};
            m_entries.erase(i);
            return p;
        }
    }
    return fs::path{};
}

bool state::at_end() const {
    const bool empty = is_current_empty();
    if (empty && m_done) {
        lock_guard lg{m_lock};
        return m_entries.empty();
    } else {
        return false;
    }
}

constexpr auto make_opts_required = fs::directory_options::include_created_events|fs::directory_options::include_modified_events;

} // anon

namespace prosoft {
namespace filesystem {
inline namespace v1 {

ifilesystem::iterator_state_ptr
ifilesystem::make_iterator_state(const path& p, directory_options opts, change_iterator_traits::configuration_type&& c, error_code& ec, change_iterator_traits) {
    if (!is_set(opts & make_opts_required)) {
        ec = einval();
        return {};
    }
    
    auto s = std::make_shared<state>(p, opts, std::move(c), ec);
    if (ec) {
        s.reset(); // null is the end iterator
    }
    return s;
}

bool ifilesystem::change_iterator_traits::canceled(const basic_iterator<change_iterator_traits>& i) {
    auto p = reinterpret_cast<const state*>(i.m_i.get());
    return !p || p->done();
}

bool ifilesystem::change_iterator_traits::equal_to(const basic_iterator<change_iterator_traits>& i, const change_registration& cr) {
    auto p = reinterpret_cast<const state*>(i.m_i.get());
    return p && p->registration() == cr;
}

} // v1
} // filesystem
} // prosoft

#if PSTEST_HARNESS
// Internal tests.
#include "catch.hpp"

using namespace prosoft::filesystem;

TEST_CASE("change_iterator_internal") {
    WHEN("converting dir opts to events") {
        CHECK(to_events(directory_options::none) == change_event::rescan_required);
        auto expected = change_event::rescan_required|change_event::created|change_event::renamed;
        CHECK(to_events(directory_options::include_created_events) == expected);
        expected = change_event::rescan_required|change_event::content_modified;
        CHECK(to_events(directory_options::include_modified_events) == expected);
        expected |= change_event::created|change_event::renamed;
        CHECK(to_events(directory_options::include_created_events|directory_options::include_modified_events) == expected);
    }
}

#endif // PSTEST_HARNESS

#endif // PS_HAVE_FILESYSTEM_CHANGE_ITERATOR
