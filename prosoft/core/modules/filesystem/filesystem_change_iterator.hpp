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

#ifndef PS_CORE_FILESYSTEM_CHANGE_ITERATOR_HPP
#define PS_CORE_FILESYSTEM_CHANGE_ITERATOR_HPP

#include <chrono>
#include <vector>

#include "filesystem_have_change_monitor.hpp"

#define PS_HAVE_FILESYSTEM_CHANGE_ITERATOR PS_HAVE_RECURISIVE_FILESYSTEM_CHANGE_MONITOR
#if PS_HAVE_FILESYSTEM_CHANGE_ITERATOR

namespace prosoft {
namespace filesystem {
inline namespace v1 {

class change_registration; // forward
class change_notification;

struct change_iterator_config {
    // For notification of a change in the iterator (either new content or EOF).
    // Expect the callback on a background thread and remember iterators are NOT thread safe.
    using callback_type = std::function<void (const change_registration&)>;
    callback_type callback;
    // Return null to ignore the change.
    using filter_type = const change_notification*(*)(const change_notification&);
    filter_type filter;
    // Passed on to FS monitor.
    using latency_type = std::chrono::milliseconds;
    latency_type latency;
    
    change_iterator_config(callback_type cb = callback_type{}, filter_type f = filter_type(), latency_type l = latency_type{1000L})
        : callback(std::move(cb))
        , filter(f)
        , latency(l) {}
    change_iterator_config(latency_type l)
        : change_iterator_config(callback_type{}, filter_type{}, l) {}
    ~change_iterator_config() = default;
    PS_DEFAULT_COPY(change_iterator_config);
    PS_DEFAULT_MOVE(change_iterator_config);
    
    static const change_notification* files_only_filter(const change_notification&);
};

namespace ifilesystem {

struct change_iterator_traits {
    static constexpr directory_options required = directory_options::none;
    static constexpr directory_options not_supported = directory_options::skip_subdirectory_descendants;
    static constexpr directory_options defaults = directory_options::include_created_events|directory_options::include_modified_events;
    using configuration_type = change_iterator_config;
    
    static bool canceled(const basic_iterator<change_iterator_traits>&);
    static bool equal_to(const basic_iterator<change_iterator_traits>&, const change_registration&);
    static std::vector<path> extract_paths(basic_iterator<change_iterator_traits>&);
};

iterator_state_ptr make_iterator_state(const path&, directory_options, change_iterator_traits::configuration_type&&, error_code&, change_iterator_traits);

using change_iterator_t = basic_iterator<ifilesystem::change_iterator_traits>;

} // ifilesystem

// XXX: change iterators will not match end() until canceled.
// Therefore make sure to check for an empty entry and manually break a loop.
inline ifilesystem::change_iterator_t begin(ifilesystem::change_iterator_t& i) {
    // current will pretty much always be empty on construction, therefore we need to increment
    return (*i).empty() ? ++i : i;
}

inline bool canceled(const ifilesystem::change_iterator_t& i) {
    return ifilesystem::change_iterator_traits::canceled(i);
}

// A more efficient way than a range loop to grab all available paths. 
inline std::vector<path> extract_paths(ifilesystem::change_iterator_t& i) {
    return ifilesystem::change_iterator_traits::extract_paths(i);
}

// for finding the iterator from a callback
inline bool operator==(const ifilesystem::change_iterator_t& i, const change_registration& cr) {
    return ifilesystem::change_iterator_traits::equal_to(i, cr);
}

inline bool operator!=(const ifilesystem::change_iterator_t& i, const change_registration& cr) {
    return !operator==(i, cr);
}

} // v1
} // filesystem
} // prosoft

#endif // PS_HAVE_FILESYSTEM_CHANGE_ITERATOR
#endif // PS_CORE_FILESYSTEM_CHANGE_ITERATOR_HPP
