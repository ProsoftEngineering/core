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

#if _WIN32

#include "winutils.hpp"
#include "win_token_privs.hpp"

#include "catch.hpp"

using namespace prosoft::windows;

TEST_CASE("winutils") {
    WHEN("initalizing COM") {
        std::error_code ec;
        com_init ci(ec);
        CHECK_FALSE(ec);
    }

    WHEN("HRESULT is successful") {
        std::error_code ec;
        CHECK(check(S_OK, ec));
        CHECK_FALSE(ec);
        CHECK(check(S_FALSE, ec));
        CHECK_FALSE(ec);

        CHECK(ok(S_OK, ec));
        CHECK_FALSE(ec);
        CHECK_FALSE(ok(S_FALSE, ec));
        CHECK(ec);
    }

    WHEN("HRESULT is not successful") {
        std::error_code ec;
        CHECK_FALSE(check(E_INVALIDARG, ec));
        CHECK(ec);
        CHECK(ec.value() == E_INVALIDARG);

        ec.clear();
        CHECK_FALSE(ok(E_INVALIDARG, ec));
        CHECK(ec);
        CHECK(ec.value() == E_INVALIDARG);

        ec.clear();
        CHECK(check(S_OK, ec));
        CHECK_FALSE(ec);
    }

    WHEN("setting a token privilege") {
        std::error_code ec;
        CHECK(enable_process_backup_privilege(ec));
        CHECK_FALSE(ec);

        CHECK_FALSE(modify_process_privilege(PS_TEXT("dummy_name"), privilege_action::enable, ec));
        CHECK(ec);
    }

    WHEN("clearing a token privilege") {
        std::error_code ec;
        CHECK(modify_process_privilege(SE_BACKUP_NAME, privilege_action::disable, ec));
        CHECK_FALSE(ec);

        CHECK_FALSE(modify_process_privilege(PS_TEXT("dummy_name"), privilege_action::disable, ec));
        CHECK(ec);
    }
}

#endif // _WIN32
