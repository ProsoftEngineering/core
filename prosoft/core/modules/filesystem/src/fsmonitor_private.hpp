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

#ifndef PS_CORE_FSMONITOR_PRIVATE_HPP
#define PS_CORE_FSMONITOR_PRIVATE_HPP

namespace prosoft {
namespace filesystem {
inline namespace v1 {

bool valid(const fs::change_config&);
void stop(change_state*, error_code&);

class change_manager {
    using evid_type = change_notification::platform_event_id_type;
public:
    static change_notification make_notification(path&& p, path&& np, const change_state* reg, change_event event, file_type ft = file_type::none, evid_type evid = {}) {
        change_notification n(std::move(p), std::move(np), evid, event, ft);
        n.m_regid = reinterpret_cast<std::uintptr_t>(reg);
        return n;
    }
    
    static void emplace_back(change_notifications& notes, path&& p, path&& np, const change_state* reg, evid_type evid, change_event event, file_type ft) {
        notes.emplace_back(std::move(p), std::move(np), evid, event, ft);
        notes.back().m_regid = reinterpret_cast<std::uintptr_t>(reg);
    }
    
    static change_registration make_registration(std::shared_ptr<change_state> const & s) {
        change_registration reg;
        reg.m_state = s;
        return reg;
    }
    
    static std::shared_ptr<change_state> state(const change_registration& reg) noexcept {
        return reg.m_state.lock();
    }
    
    static void process_renames(fs::change_notifications&);
};

enum platform_error {
    noerr,
    convert_path,
    monitor_create,
    monitor_start,
    not_supported
};

class platform_error_category : public std::error_category {
public:
    platform_error_category() noexcept(std::is_nothrow_default_constructible<std::error_category>::value)
        : std::error_category() {}
    virtual ~platform_error_category() = default;
    
    virtual const char* name() const noexcept override {
        return "Platform filesystem monitor";
    }
    
    virtual std::string message(int) const override;
};

const std::error_category& platform_category() noexcept(std::is_nothrow_default_constructible<platform_error_category>::value);

} // v1
} // filesystem
} // prosoft

#endif // PS_CORE_FSMONITOR_PRIVATE_HPP
