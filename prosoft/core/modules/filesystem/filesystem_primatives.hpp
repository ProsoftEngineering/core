// Copyright © 2015-2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_FILESYSTEM_PRIMATIVES_HPP
#define PS_CORE_FILESYSTEM_PRIMATIVES_HPP

#include <chrono>

namespace prosoft {
namespace filesystem {
inline namespace v1 {

using file_time_type = std::chrono::time_point<std::chrono::system_clock>;

enum class file_type {
    none = 0,
    not_found = -1,
    regular = 1,
    directory = 2,
    symlink = 3,
    block = 4,
    character = 5,
    fifo = 6,
    socket = 7,
    unknown = 8
};

enum class perms {
    none = 0,
    owner_read = 0400,
    owner_write = 0200,
    owner_exec = 0100,
    owner_all = 0700,

    group_read = 040,
    group_write = 020,
    group_exec = 010,
    group_all = 070,

    others_read = 04,
    others_write = 02,
    others_exec = 01,
    others_all = 07,

    all = 0777,
    set_uid = 04000,
    set_gid = 02000,
    sticky_bit = 01000,

    mask = 07777,

    unknown = 0xffff,

    add_perms = 0x10000,
    remove_perms = 0x20000,
    resolve_symlinks = 0x40000,
};

PS_ENUM_BITMASK_OPS(perms);

class times {
    file_time_type m_modifyTime;
    file_time_type m_changeTime;
    file_time_type m_accessTime;
    file_time_type m_createTime;
    
    static constexpr file_time_type m_invalidTime = file_time_type::min();
    
public:
    times()
        : m_modifyTime(m_invalidTime)
        , m_changeTime(m_invalidTime)
        , m_accessTime(m_invalidTime)
        , m_createTime(m_invalidTime) {}
    PS_DEFAULT_COPY(times);
    PS_DEFAULT_MOVE(times);
    
    const file_time_type& modified() const noexcept {
        return m_modifyTime;
    }
    
    void modified(const file_time_type& val) {
        m_modifyTime = val;
    }
    
    bool has_modified() const noexcept {
        return m_modifyTime != m_invalidTime;
    }
    
    const file_time_type& metadata_modified() const noexcept {
        return m_changeTime;
    }
    
    void metadata_modified(const file_time_type& val) {
        m_changeTime = val;
    }
    
    bool has_metadata_modified() const noexcept {
        return m_changeTime != m_invalidTime;
    }
    
    const file_time_type& accessed() const noexcept {
        return m_accessTime;
    }
    
    void accessed(const file_time_type& val) {
        m_accessTime = val;
    }
    
    bool has_accessed() const noexcept {
        return m_accessTime != m_invalidTime;
    }
    
    const file_time_type& created() const noexcept {
        return m_createTime;
    }
    
    void created(const file_time_type& val) {
        m_createTime = val;
    }
    
    bool has_created() const noexcept {
        return m_createTime != m_invalidTime;
    }

    static constexpr file_time_type make_invalid() {
        return m_invalidTime;
    }
};

} // v1
} // filesystem
} // prosoft

#endif // PS_CORE_FILESYSTEM_PRIMATIVES_HPP

