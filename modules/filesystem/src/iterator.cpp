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

#include "iterator_internal.hpp"

using namespace prosoft::filesystem;    // native_dir

namespace {

class iterator_error_category : public std::error_category {
public:
	iterator_error_category() noexcept : std::error_category() {}

	virtual const char* name() const noexcept override {
		return "Filesystem iterator";
    }

	virtual std::string message(int ec) const override {
        switch (static_cast<iterator_error>(ec)) {
            case iterator_error::encoding_is_not_utf8:
                return "A non-UTF8 path name was encountered. It has been skipped.";
            break;
            
            default:
                PSASSERT_UNREACHABLE("BUG");
#if !_WIN32
                return std::strerror(EINVAL);
#else
                return "Unknown error";
#endif
            break;
        }
	}
};

} // namespace

namespace prosoft {
namespace filesystem {
inline namespace v1 {

const std::error_category& iterator_category() {
    static iterator_error_category cat;
    return cat;
}

native_dir* const INVALID_DIR = (native_dir*)((uintptr_t)0xbaadf00dUL);

} // namespace v1
} // namespace filesystem
} // namespace prosoft

namespace {

struct dir_ops {
    PS_ALWAYS_INLINE static native_dir* open(const fs::path& p) {
        return open_dir(p);
    }

    PS_ALWAYS_INLINE static native_dirent* read(native_dir* d) {
        return read_dir(d);
    }
    
    PS_ALWAYS_INLINE static int close(native_dir* d) {
        return close_dir(d);
    }
};

} // anon

namespace prosoft {
namespace filesystem {
inline namespace v1 {

ifilesystem::iterator_state_ptr
ifilesystem::make_iterator_state(const path& p, directory_options opts, iterator_traits::configuration_type, error_code& ec) {
    auto s = std::make_shared<state<dir_ops>>(p, opts, ec);
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

constexpr file_size_type directory_entry::unknown_size;

void directory_entry::refresh() {
    error_code ec;
    refresh(ec);
    PS_THROW_IF(ec.value() != 0, filesystem_error("Failed to refresh dir ent.", m_path, ec));
}

void directory_entry::refresh(error_code& ec) {
    constexpr status_info info = status_info::basic | status_info::times | status_info::size;
    const auto st = filesystem::symlink_status(m_path, info, ec);
    if (!ec) {
        m_type = st.type();
        m_size = st.size();
        m_last_write = st.times().modified().time_since_epoch().count();
    } else {
        clear_cache();
    }
}

} // v1
} // filesystem
} // prosoft
