// Copyright © 2016, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <prosoft/core/config/config.h>

#if !_WIN32
#include <sys/errno.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#include "filesystem.hpp"
#include "filesystem_private.hpp"

namespace prosoft {
namespace filesystem {
inline namespace v1 {

path temp_directory_path() {
    error_code ec;
    auto p = temp_directory_path(ec);
    PS_THROW_IF(ec.value(), filesystem_error("temporary dir failed", p, ec));
    return p;
}

path temp_directory_path(error_code& ec) {
    ec.clear();

    path p;
#if !_WIN32
    const char* t = ::getenv(ifilesystem::TMPDIR);
    if (!t || *t == 0) {
        constexpr const char* default_tmpdir = "/tmp";
        t = default_tmpdir;
#if __APPLE__
        std::unique_ptr<char[]> buf;
        const auto len = ::confstr(_CS_DARWIN_USER_TEMP_DIR, nullptr, 0);
        if (len > 0) {
            buf.reset(new char[len]);
            if (::confstr(_CS_DARWIN_USER_TEMP_DIR, buf.get(), len) > 0 && *buf.get() != 0) {
                t = buf.get();
            }
        }
#endif // __APPLE__
    }
    PSASSERT_NOTNULL(t);
    p = t;
#else // !_WIN32
    std::wstring buf(::GetTempPathW(0, nullptr) + 1, 0);
    if (buf.size() > 1 && ::GetTempPathW(static_cast<DWORD>(buf.size()), &buf[0]) > 1) {
        buf.erase(--buf.end()); // null term
        p = std::move(buf);
    } else {
        ifilesystem::system_error(ec);
    }
#endif // !_WIN32

    if (ec || p.empty() || !is_directory(p, ec)) {
        p.clear();
        if (!ec) {
#if !_WIN32
            constexpr int default_err = ENOENT;
#else
            constexpr int default_err = ERROR_PATH_NOT_FOUND;
#endif
            ec.assign(default_err, filesystem_category());
        }
    }
    return p;
}

} // v1
} // filesystem
} // prosoft
