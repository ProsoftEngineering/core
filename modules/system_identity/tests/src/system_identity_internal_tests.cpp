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

#include <identity_internal.hpp>

#ifdef PS_USING_PASSWD_API

#include <catch2/catch_test_macros.hpp>

TEST_CASE("system_identity_internal") {
    using namespace prosoft::system;

    SECTION("gecos parse") {
        WHEN("gecos is empty") {
            THEN("gecos name is empty") {
                CHECK(gecos_name(",,,").empty());
            }
        }
        WHEN("gecos is invalid") {
            THEN("gecos name is empty") {
                CHECK(gecos_name(nullptr).empty());
                CHECK(gecos_name("").empty());
                CHECK(gecos_name("Prosoft Engineeering").empty());
            }
        }
        WHEN("gecos contains no name") {
            THEN("gecos name is empty") {
                CHECK(gecos_name(",test,test,test").empty());
            }
        }
        WHEN("gecos contains a name") {
            THEN("gecos name is the expected value") {
                CHECK(gecos_name("Prosoft Engineeering,,,") == "Prosoft Engineeering");
            }
        }
        WHEN("gecos contains embedded commas") {
            THEN("gecos name is the expected value") {
                CHECK(gecos_name("Prosoft Engineeering\\, Inc.,,,") == "Prosoft Engineeering\\, Inc.");
            }
        }
    }

    SECTION("passwd lookup") {
        WHEN("known id is used") {
            THEN("entry is valid") {
                auto pe = std::make_unique<passwd_entry>();
                CHECK(pe->init_from_uname("root"));
                CHECK(pe->init_from_uid(uid_t{0}));
            }
        }
    }
}

#endif  // PS_USING_PASSWD_API
