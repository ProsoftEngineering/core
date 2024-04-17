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

#include <prosoft/core/config/config_platform.h>

#include <prosoft/core/modules/filesystem/filesystem.hpp>
#include <prosoft/core/modules/filesystem/filesystem_change_monitor.hpp>
#if PS_HAVE_FILESYSTEM_CHANGE_MONITOR
#include "filesystem_private.hpp"
#include "fsmonitor_private.hpp"

namespace prosoft {
namespace filesystem {
inline namespace v1 {

// public

change_registration monitor(const path& p, const change_config& cfg, change_callback cb) {
    error_code ec;
    auto reg =  monitor(p, cfg, std::move(cb), ec);
    PS_THROW_IF(ec.value(), filesystem_error("Failed to create change monitor", p, ec));
    return reg;
}

change_registration recursive_monitor(const path& p, const change_config& cfg, change_callback cb) {
    error_code ec;
    auto reg =  recursive_monitor(p, cfg, std::move(cb), ec);
    PS_THROW_IF(ec.value(), filesystem_error("Failed to create change monitor", p, ec));
    return reg;
}

void stop(const change_registration& reg, error_code& ec) {
    if (reg) {
        if (auto p = change_manager::state(reg)) {
            ec.clear();
            stop(p.get(), ec);
            return;
        }
    }
    
    ec = einval();
}

void stop(const change_registration& reg) {
    error_code ec;
    stop(reg, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Failed to stop change monitor", ec));
}

// private

bool valid(const fs::change_config& cfg) {
    return cfg.events != fs::change_event::none
        && cfg.notification_latency >= decltype(cfg.notification_latency){}
        && cfg.reserved_flags == 0;
}

std::string platform_error_category::message(int code) const {
    switch(static_cast<platform_error>(code)) {
        case platform_error::convert_path:
            return "Could not convert path to native type.";
        case platform_error::monitor_create:
            return "Could not create filesystem event monitor.";
        case platform_error::monitor_start:
            return "Could not start filesystem event monitor.";
        case platform_error::monitor_thaw:
            return "Could not restore filesystem event monitor.";
        case platform_error::monitor_replay_past:
            return "Could not replay filesystem event monitor as the event stream is in the past.";
        case platform_error::not_supported:
            return "Unsupported filesystem event monitor option.";
        case platform_error::noerr:
            return "";
        default:
            return "Unknown filesystem event monitor error";
    }
}

const std::error_category& platform_category() noexcept(std::is_nothrow_default_constructible<platform_error_category>::value) {
    static const platform_error_category cat;
    return cat;
}

} // v1
} // filesystem
} // prosoft
#endif // PS_HAVE_FILESYSTEM_CHANGE_MONITOR
