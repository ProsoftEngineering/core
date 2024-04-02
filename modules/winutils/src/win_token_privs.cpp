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

#include <prosoft/core/include/system_error.hpp>
#include <prosoft/core/include/unique_resource.hpp>

#include <prosoft/core/modules/winutils/win_token_privs.hpp>

bool prosoft::windows::modify_privilege(HANDLE token, privilege_name priv, privilege_action act, std::error_code& ec) {
    LUID luid;
    if (::LookupPrivilegeValue(nullptr, priv, &luid)) {
        TOKEN_PRIVILEGES val;
        val.PrivilegeCount = 1;
        val.Privileges[0].Luid = luid;
        val.Privileges[0].Attributes = act == privilege_action::enable ? SE_PRIVILEGE_ENABLED : 0;
        if (::AdjustTokenPrivileges(token, false, &val, 0, nullptr, nullptr)) {
            return true;
        }
    }

    prosoft::system::system_error(ec);
    return false;
}

bool prosoft::windows::modify_process_privilege(privilege_name priv, privilege_action act, std::error_code& ec) {
    prosoft::windows::Handle token;
    if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, prosoft::handle(token))) {
        return modify_privilege(token.get(), priv, act, ec);
    } else {
        prosoft::system::system_error(ec);
        return false;
    }
}

