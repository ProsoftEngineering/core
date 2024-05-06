// Copyright © 2017-2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <prosoft/core/modules/filesystem/filesystem.hpp>   // PS_HAVE_FILESYSTEM_CHANGE_ITERATOR

#if PS_HAVE_FILESYSTEM_CHANGE_ITERATOR

#include "change_iterator_internal.hpp"
#include "filesystem_private.hpp"

namespace prosoft {
namespace filesystem {
inline namespace v1 {

state::state(const fs::path& p, fs::directory_options opts, fs::change_iterator_config&& c, fs::error_code& ec)
    : fsiterator_state(p, opts, ec)
    , m_reg() {
    if (!ec) {
        m_callback = std::move(c.callback);
        m_filters = std::move(c.filters);
        
        using namespace fs;
        const auto events = to_events(opts);
        change_config cfg;
        auto state = change_state::serialize(c.serialize_data);
        cfg.state = state.get();
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

fs::path state::next(fsiterator_cache&, prosoft::system::error_code&) {
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

ifilesystem::change_iterator_traits::serialize_type
ifilesystem::change_iterator_traits::serialize(const basic_iterator<change_iterator_traits>& i) {
    if (auto p = reinterpret_cast<const state*>(i.m_i.get())) {
        return p->serialize();
    }
    return "";
}

} // v1
} // filesystem
} // prosoft

#if PSTEST_HARNESS
// Internal tests.
#include <catch2/catch_test_macros.hpp>

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
