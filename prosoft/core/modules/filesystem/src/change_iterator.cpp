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

#include <prosoft/core/config/config_platform.h>

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

using extraction_type = std::vector<fs::path>;

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

inline bool required(fs::change_event ev) {
    return is_set(ev & fs::change_event::rescan_required);
}

using notification_ptr = fs::change_notification*;

static_assert(std::is_pointer<fs::change_iterator_config::filter_type>::value, "Broken assumption, values should probably be changed to refs in call().");

notification_ptr call(fs::change_iterator_config::filter_type f, notification_ptr p) {
    return p && f && !f(*p) ? nullptr : p;
}

notification_ptr call(const fs::change_iterator_config::filters_type& fl, notification_ptr p) {
    if (p && !required(p->event())) {
        for (auto f : fl) {
            if (!call(f, p)) {
                return nullptr;
            }
        }
    }
    return p;
}

using fsiterator_state = fs::ifilesystem::iterator_state;

struct test_state; // for testing

class state : public fsiterator_state {
    using lock_type = std::mutex;
    using lock_guard = std::lock_guard<lock_type>;
    using callback_type = decltype(fs::change_iterator_config::callback);
    
    fs::change_registration m_reg;
    lock_type mutable m_lock;
    std::unordered_set<prosoft::stable_hash_wrapper<fs::path>> m_entries;
    std::atomic_bool m_done{false}; // no more events will be received
    callback_type m_callback;
    fs::change_iterator_config::filters_type m_filters;
    
    static notification_ptr filter(fs::change_notification& n, fs::change_event ev) {
        return is_set(n.event() & ev) ? &n : nullptr;
    }
    static void filter(fs::change_notification&&, fs::change_event) = delete;
    
    notification_ptr PS_ALWAYS_INLINE filter(notification_ptr p) {
        return call(m_filters, p);
    }
    
    void add(notification_ptr);
    
    void abort() noexcept;
    
    void notify() noexcept;
    
    // testing
    friend test_state;
    state() = default;
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
    
    extraction_type extract();
    
    virtual fs::path next(prosoft::system::error_code&) override;
    virtual bool at_end() const override;
};

state::state(const fs::path& p, fs::directory_options opts, fs::change_iterator_config&& c, fs::error_code& ec)
    : fsiterator_state(p, opts, ec)
    , m_reg() {
    if (!ec) {
        m_callback = std::move(c.callback);
        m_filters = std::move(c.filters);
        
        using namespace fs;
        const auto events = to_events(opts);
        change_config cfg;
        cfg.notification_latency = std::chrono::duration_cast<change_config::latency_type>(c.latency);
        m_reg = recursive_monitor(p, cfg, [this, events](change_notifications&& notes) {
            for (auto& n : notes) {
                add(filter(filter(n, events)));
            }
        }, ec);
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

extraction_type state::extract() {
    extraction_type paths;
    lock_guard lg{m_lock};
    for (auto& e : m_entries) {
        paths.emplace_back(e.extract());
    }
    m_entries.clear();
    return paths;
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
ifilesystem::make_iterator_state(const path& p, directory_options opts, change_iterator_traits::configuration_type&& c, error_code& ec) {
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

const change_notification* change_iterator_config::is_regular_filter(const change_notification& n) {
    return n.type() == file_type::regular ? &n : nullptr;
}

const change_notification* change_iterator_config::exists_filter(const change_notification& n) {
    error_code ec;
    return exists(n.renamed_to_path().empty() ? n.path() : n.renamed_to_path(), ec) ? &n : nullptr;
}

bool ifilesystem::change_iterator_traits::canceled(const basic_iterator<change_iterator_traits>& i) {
    auto p = reinterpret_cast<const state*>(i.m_i.get());
    return !p || p->done();
}

bool ifilesystem::change_iterator_traits::equal_to(const basic_iterator<change_iterator_traits>& i, const change_registration& cr) {
    auto p = reinterpret_cast<const state*>(i.m_i.get());
    return p && p->registration() == cr;
}

extraction_type ifilesystem::change_iterator_traits::extract_paths(basic_iterator<change_iterator_traits>& i) {
    auto p = reinterpret_cast<state*>(i.m_i.get());
    return p ? p->extract() : extraction_type{};
}

} // v1
} // filesystem
} // prosoft

#if PSTEST_HARNESS
// Internal tests.
#include "catch.hpp"

namespace {

struct test_state {
    state s;
    
    void add(fs::path p) {
        fs::change_notification n{std::move(p), fs::path{}, 0, fs::change_event::none, fs::file_type::regular};
        s.add(&n);
    }
};

} // anon

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
    
    static auto nop_filter = [](const fs::change_notification& n) -> const fs::change_notification* {
        return &n;
    };
    static auto null_filter = [](const fs::change_notification&) -> const fs::change_notification* {
        return nullptr;
    };
    
    static fs::change_notification note{fs::path{}, fs::path{}, 0, fs::change_event::none, fs::file_type::unknown};
    
    using filters_type = fs::change_iterator_config::filters_type;
    
    WHEN("filter is null") {
        CHECK(call(nullptr, nullptr) == nullptr);
        CHECK(call(nullptr, &note) == &note);
    }
    
    WHEN("filter is not null") {
        CHECK(call(nop_filter, nullptr) == nullptr);
        CHECK(call(filters_type{nop_filter}, nullptr) == nullptr);
        CHECK(call(nop_filter, &note) == &note);
        CHECK(call(null_filter, &note) == nullptr);
        
        CHECK(call(filters_type{nop_filter, null_filter}, &note) == nullptr);
        CHECK(call(filters_type{null_filter, nop_filter}, &note) == nullptr);
        
        fs::change_notification n{fs::path{}, fs::path{}, 0, fs::change_event::rescan_required, fs::file_type::unknown};
        CHECK(call(filters_type{null_filter}, &n) == &n); // rescan must always pass
    }
    
    WHEN("filtering for files") {
        CHECK(call(change_iterator_config::is_regular_filter, nullptr) == nullptr);
        CHECK(call(change_iterator_config::is_regular_filter, &note) == nullptr);
        
        fs::change_notification n{fs::path{}, fs::path{}, 0, fs::change_event::none, fs::file_type::regular};
        CHECK(call(change_iterator_config::is_regular_filter, &n) == &n);
        n = {fs::path{}, fs::path{}, 0, fs::change_event::none, fs::file_type::directory};
        CHECK(call(change_iterator_config::is_regular_filter, &n) == nullptr);
        
        n = {fs::path{}, fs::path{}, 0, fs::change_event::rescan, fs::file_type::directory};
        CHECK(call(filters_type{change_iterator_config::is_regular_filter}, &n) == &n); // rescan must always pass
        n = {fs::path{}, fs::path{}, 0, fs::change_event::canceled, fs::file_type::directory};
        CHECK(call(filters_type{change_iterator_config::is_regular_filter}, &n) == &n); // cancel must always pass
    }
    
    WHEN("filtering for existing paths") {
        CHECK(call(change_iterator_config::exists_filter, nullptr) == nullptr);
        CHECK(call(change_iterator_config::exists_filter, &note) == nullptr);
        
        fs::change_notification n{fs::temp_directory_path(), fs::path{}, 0, fs::change_event::none, fs::file_type::directory};
        CHECK(call(change_iterator_config::exists_filter, &n) == &n);
        n = {fs::path{}, fs::temp_directory_path(), 0, fs::change_event::none, fs::file_type::none}; // dir still exists if type is none
        CHECK(call(change_iterator_config::exists_filter, &n) == &n);
    }
    
    WHEN("extracting paths") {
        test_state ts;
        ts.add(fs::path{PS_TEXT("test")});
        ts.add(fs::path{PS_TEXT("test2")});
        ts.add(fs::path{PS_TEXT("test3")});
        
        auto paths = ts.s.extract();
        CHECK(paths.size() == 3);
        
        paths = ts.s.extract();
        CHECK(paths.empty());
    }
}

#endif // PSTEST_HARNESS

#endif // PS_HAVE_FILESYSTEM_CHANGE_ITERATOR
