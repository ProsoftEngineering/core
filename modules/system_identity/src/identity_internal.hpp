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

#ifndef PS_CORE_IDENTITY_INTERNAL_HPP
#define PS_CORE_IDENTITY_INTERNAL_HPP

#if !defined(_WIN32) && !defined(__APPLE__)
    #define PS_USING_PASSWD_API
#endif

#ifdef PS_USING_PASSWD_API

#include <grp.h>
#include <pwd.h>
#include <array>
#include <vector>
#include <prosoft/core/modules/system_identity/identity.hpp>

namespace prosoft {
namespace system {

class passwd_entry {
public:
    passwd_entry() = default;   // no-op

    bool init_from_uname(const char* uname) {
        passwd* result;
        if (getpwnam_r(uname, &m_entry, m_buffer.data(), m_buffer.size(), &result) != 0
            || result == nullptr)   // user not found
        {
            return false;
        }
        return true;
    }

    bool init_from_uid(uid_t uid) {
        passwd* result;
        if (getpwuid_r(uid, &m_entry, m_buffer.data(), m_buffer.size(), &result) != 0
            || result == nullptr)   // user not found
        {
            return false;
        }
        return true;
    }

    bool init_from_console_user() {
        const auto uname = getenv("LOGNAME");
        if (uname == nullptr) {     // no login (for example, running in docker)
            return false;
        }
        return init_from_uname(uname);
    }

    const passwd& entry() const {
        return m_entry;
    }

private:
    passwd m_entry;
    std::array<char, 1024> m_buffer;

    // Disable copy and move for heavy object
    passwd_entry(const passwd_entry& ) = delete;
    passwd_entry(passwd_entry&& ) = delete;
};

class group_entry {
public:
    group_entry() = default;    // no-op

    bool init_from_gname(const char* gname) {
        group* result;
        if (getgrnam_r(gname, &m_entry, m_buffer.data(), m_buffer.size(), &result) != 0
            || result == nullptr)   // group not found
        {
            return false;
        }
        return true;
    }

    bool init_from_gid(gid_t gid) {
        group* result;
        if (getgrgid_r(gid, &m_entry, m_buffer.data(), m_buffer.size(), &result) != 0
            || result == nullptr)   // group not found
        {
            return false;
        }
        return true;
    }

    const group& entry() const {
        return m_entry;
    }

private:
    group m_entry;
    std::array<char, 1024> m_buffer;

    // Disable copy and move for heavy object
    group_entry(const group_entry& ) = delete;
    group_entry(group_entry&& ) = delete;
};

prosoft::native_string_type gecos_name(const char* gecos);

} // system
} // prosoft

#endif  // PS_USING_PASSWD_API

#endif // PS_CORE_IDENTITY_INERNAL_HPP
