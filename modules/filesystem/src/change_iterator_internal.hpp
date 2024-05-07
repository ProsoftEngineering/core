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

#ifndef PS_CORE_CHANGE_ITERATOR_INTERNAL_HPP
#define PS_CORE_CHANGE_ITERATOR_INTERNAL_HPP

#include <atomic>
#include <unordered_set>

#include <prosoft/core/include/stable_hash_wrapper.hpp>

#include <prosoft/core/modules/filesystem/filesystem.hpp>   // error_code
#include <prosoft/core/modules/filesystem/filesystem_path.hpp>  // path
#include <prosoft/core/modules/filesystem/filesystem_change_monitor.hpp>    // change_registration

namespace fs = prosoft::filesystem::v1;

namespace prosoft {
namespace filesystem {
inline namespace v1 {

using extraction_type = std::vector<fs::path>;

inline fs::change_event to_events(fs::directory_options opts) {
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

inline notification_ptr call(fs::change_iterator_config::filter_type f, notification_ptr p) {
    return p && f && !f(*p) ? nullptr : p;
}

inline notification_ptr call(const fs::change_iterator_config::filters_type& fl, notification_ptr p) {
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
using fsiterator_cache = fs::ifilesystem::cache_info;

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
    
    virtual fs::path next(fsiterator_cache&, prosoft::system::error_code&) override;
    virtual bool at_end() const override;
    
    std::string serialize() const {
        return m_reg.serialize();
    }
};

} // namespace v1
} // namespace filesystem
} // namespace prosoft

#endif // PS_CORE_CHANGE_ITERATOR_INTERNAL_HPP
