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

#include <unique_resource.hpp>

namespace {

using namespace prosoft;
using namespace prosoft::filesystem;

struct SystemCwdProvider {
    using pointer = path::encoding_value_type*;
#if !_WIN32
    pointer operator()(pointer buf, size_t sz) const {
        return ::getcwd(buf, sz);
    }
#else
    DWORD operator()(DWORD sz, pointer buf) const {
        return ::GetCurrentDirectory(sz, buf);
    }
#endif
};

template <class Provider>
path current_path(const Provider& cwd, error_code& ec) {
#if !_WIN32
    unique_malloc<char> p{cwd(nullptr, 0)}; // POSIX extension that's supported by *BSD and Linux
    if (p) {
        return {p.get()};
    } else {
        if (ERANGE == errno) {
            p = make_malloc<char>(PATH_MAX);
            PS_THROW_IF(!p, std::bad_alloc{});
            if (cwd(p.get(), PATH_MAX)) {
                return {p.get()};
            }
        }
        ifilesystem::system_error(ec);
    }
#else
    std::wstring buf(cwd(0, nullptr), 0); // GetCurrentDirecotry will account for the null term.
    if (buf.size() > 1 && cwd(static_cast<DWORD>(buf.size()), &buf[0]) > 1) {
        buf.erase(--buf.end()); // null term
        return {std::move(buf)};
    } else {
        ifilesystem::system_error(ec);
    }
#endif
    return {};
}

#if _WIN32
template <typename T>
T* fullpath(const T*) {PSASSERT(0, "BUG"); return nullptr;}
template<>
inline char* fullpath<char>(const char* p) {
    return ::_fullpath(nullptr, p, 0);
}
template<>
inline wchar_t* fullpath<wchar_t>(const wchar_t* p) {
    return ::_wfullpath(nullptr, p, 0);
}
#endif

}

namespace prosoft {
namespace filesystem {
inline namespace v1 {

path canonical(const path& p, const path& base) {
    error_code ec;
    path rp = canonical(p, base, ec);
    PS_THROW_IF(ec.value(), filesystem_error("canonical failed", p, ec));
    return rp;
}

path canonical(const path& p, const path& base, error_code& ec) {
    ec.clear();
    
#if !_WIN32
    static const path rootp{path::preferred_separator};

    unique_malloc<char> resolved{::realpath(p.c_str(), nullptr)}; // POSIX extension that's supported by *BSD and Linux
    if (resolved) {
        return {resolved.get()};
    } else if (ENOENT == errno && p != rootp) {
        auto parent = p.parent_path(); // attempt to resolve parents
        if (!parent.empty()) {
            return canonical(parent, ec) / p.filename();
        } else if (!base.empty()) { // add the base -- XXX: this assumes that path is a relative leaf with no separators and thus no recursion was performed
            return base / p;
        } else {
            ifilesystem::error(ENOENT, ec);
        }
    } else {
        ifilesystem::system_error(ec);
    }
    return path{p};
#else
    auto rp = sanitize(p.native(), path::preferred_separator_style);
    unique_malloc<path::encoding_value_type> tmp{fullpath(rp.c_str())};
    if (tmp) {
        return {tmp.get()};
    } else {
        return {std::move(rp)};
    }
#endif
}

path current_path() {
    error_code ec;
    path p = current_path(ec);
    PS_THROW_IF(ec.value(), filesystem_error("current path failed", ec));
    return p;
}

path current_path(error_code& ec) {
    ec.clear();
    return current_path(SystemCwdProvider{}, ec);
}

void current_path(const path& p) {
    error_code ec;
    current_path(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("change current path failed", p, ec));
}

void current_path(const path& p, error_code& ec) {
    ec.clear();
#if !_WIN32
    if (0 != ::chdir(p.c_str()))
#else
    // Limited to MAX_PATH-1 chars. A terminating '\' is required and added if misssing so it could be MAX_PATH-2 chars.
    if (!::SetCurrentDirectory(p.c_str()))
#endif
    {
        ifilesystem::system_error(ec);
    }
}

} // v1
} // filesystem
} // prosoft

#if PSTEST_HARNESS

// Internal tests.
#include "catch.hpp"

struct ErrorCwdProvider {
    using pointer = path::encoding_value_type*;
#if !_WIN32
    pointer operator()(pointer, size_t) const {
        errno = EINVAL;
        return nullptr;
    }
#else
    DWORD operator()(DWORD, pointer) const {
        SetLastError(1);
        return 0;
    }
#endif
};

#if !_WIN32
struct RangeErrorCwdProvider {
    using pointer = path::encoding_value_type*;
    pointer operator()(pointer p, size_t sz) const {
        if (nullptr == p) {
            errno = ERANGE;
            return nullptr;
        } else {
            return ::getcwd(p, sz);
        }
    }
};
#endif

TEST_CASE("pathops internal") {
    using namespace prosoft::filesystem;
    
    WHEN("current_path fails") {
        error_code ec;
        (void)current_path(ErrorCwdProvider{}, ec);
        CHECK(ec.value() != 0);
    }
    
#if !_WIN32
    WHEN("the system does not support the getcwd malloc extension") {
        error_code ec;
        CHECK(RangeErrorCwdProvider{}(nullptr, 0) == nullptr);
        auto p = current_path(RangeErrorCwdProvider{}, ec);
        CHECK_FALSE(p.empty());
        CHECK(ec.value() == 0);
    }
#endif
}

#endif // PSTEST_HARNESS
