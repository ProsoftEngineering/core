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

#if !_WIN32
// FTS is not thread safe at all.
// readdir() is thread safe (on all modern OS versions) for multiple instances, but a single instance is only safe for a single thread to access.
#include <dirent.h>
#include <sys/errno.h>
#else
#include <windows.h>
#endif

#include "filesystem.hpp"

namespace fs = prosoft::filesystem::v1;

namespace {

using fsiterator_state = fs::ifilesystem::iterator_state;

class state : public fsiterator_state {
// XXX: TEMPORARY TESTING
    fs::path m_root;
// XXX
    bool recurse() const noexcept {
        return !is_set(options() & fs::directory_options::skip_subdirectory_descendants);
    }

public:
    using fsiterator_state::fsiterator_state;
    
    state(const fs::path&, fs::directory_options, fs::error_code&);
    
    virtual fs::path next(prosoft::system::error_code&) override;
    
    virtual void pop() override {}
    
    virtual fs::iterator_depth_type depth() const noexcept override {
        return 0;
    }
    
    virtual void skip_descendants() override;
};

state::state(const fs::path& p, fs::directory_options opts, fs::error_code& ec)
    : fsiterator_state(p, opts, ec) {
    if (!ec) {
// XXX: TEMPORARY TESTING -- should just attempt to open the path for reading
        m_root = p;
        auto st = fs::status(p, ec);
        if (!ec && !fs::is_directory(st)) {
            ec.assign(ENOTDIR, prosoft::system::posix_category());
        }
// XXX
    }
}

fs::path state::next(prosoft::system::error_code&) {
    if (recurse()) {
        clear(fs::directory_options::reserved_state_skip_descendants);
    }
// XXX: TEMPORARY TESTING
    if (current().path().empty()) {
        return m_root;
    }
// XXX
    return fs::path{};
}

void state::skip_descendants() {
    if (recurse()) {
        set(fs::directory_options::reserved_state_skip_descendants);
    }
}

} // anon

namespace prosoft {
namespace filesystem {
inline namespace v1 {

bool ifilesystem::iterator_state::equal_to(const ifilesystem::iterator_state*) const {
    return true;
}

ifilesystem::iterator_state_ptr
ifilesystem::make_iterator_state(const path& p, directory_options opts, iterator_traits::configuration_type, error_code& ec, iterator_traits) {
    auto s = std::make_shared<state>(p, opts, ec);
    if (ec) {
        s.reset(); // null is the end iterator
    }
    return s;
}

const error_code& ifilesystem::permission_denied_error() {
#if !_WIN32
    constexpr int ec = EACCES;
#else
    constexpr int ec = ERROR_ACCESS_DENIED;
#endif
    static const error_code denied{ec, system::error_category()};
    return denied;
}

} // v1
} // filesystem
} // prosoft
