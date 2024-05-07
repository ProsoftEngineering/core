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

#include <pathops_internal.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace prosoft;
using namespace prosoft::filesystem;

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

} // namespace


TEST_CASE("pathops_internal") {
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
