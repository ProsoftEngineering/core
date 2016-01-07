// Copyright © 2015-2016, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#if __APPLE__
#include <membership.h>
#elif _WIN32
#include <prosoft/core/config/config_windows.h>
#include <windows.h>
#include <sddl.h>
#endif

#include <identity.hpp>
#include <unique_resource.hpp>

#include "catch.hpp"

TEST_CASE("system_identity") {
    using namespace prosoft;
    using namespace prosoft::system;

    WHEN("constructing well-known identities") {
        THEN("each idenity is valid") {
            auto i = identity::console_user();
            // XXX: console user can be invalid

            i = identity::process_user();
            CHECK(i);
            CHECK(is_user(i));
            CHECK(exists(i));
            CHECK_FALSE(i.account_name().empty());

            i = identity::effective_user();
            CHECK(i);
            CHECK(exists(i));
            CHECK_FALSE(i.account_name().empty());

#if _WIN32
            i = identity::everyone_group();
            CHECK(i);
            CHECK(exists(i));
            CHECK_FALSE(i.account_name().empty());
#else
            // Group test
            i = identity{identity_type::group, 0};
            CHECK(i);
            CHECK(is_group(i));
            CHECK(exists(i));
            CHECK_FALSE(i.account_name().empty());
#endif
        }
    }

#if __APPLE__
    WHEN("constructing an identity via guid") {
        const auto i = identity::process_user();
        guid_t guid;
        ::mbr_uid_to_uuid(i.system_identity(), guid.g_guid);
        THEN("it is valid") {
            auto gi = identity{guid};
            CHECK(gi);
            CHECK((gi == i));
        }
    }

    WHEN("constructing an unknown id via guid") {
        guid_t guid;
        const std::string gs{"D5C93535-940D-4F2E-A98B-6B556E58B9C2"};
        auto err = uuid_parse(gs.c_str(), guid.g_guid);
        REQUIRE(err == 0);
        identity i{guid};
        THEN("the identity is valid") {
            CHECK(i);
            CHECK_FALSE(i.name().empty());
            CHECK(i.account_name() == gs);
        }
        THEN("the identity does not exist") {
            CHECK_FALSE(exists(i));
        }
        THEN("the identity type is unknown") {
            CHECK(i.type() == identity_type::unknown);
            CHECK(i.system_identity() == identity::unknown_system_identity);
        }
        THEN("the identity does not equal a known identity") {
            CHECK(i != identity::process_user());
        }
        THEN("a copy of the identity is equal to the original") {
            const auto c = identity{i};
            CHECK((c == i));
        }
        THEN("a separate unknown identity is not equal to the given unknown") {
            err = uuid_parse("EFF52E18-398F-4976-8348-D8A7DE2879E0", guid.g_guid);
            REQUIRE(err == 0);
            identity i2{guid};
            CHECK((i != i2));
        }
    }
#endif

    WHEN("using an unknown identity") {
#if !_WIN32
        auto sid = identity::unknown_system_identity;
#else
        // Deleted local account SID.
        prosoft::windows::unique_local<void> sidp;
        ::ConvertStringSidToSid(L"S-1-5-21-3067000638-2403337262-3888821796-1007", handle(sidp));
        auto sid = sidp.get();
#endif
        auto i = identity(identity_type::user, sid);
        THEN("the identity is valid") {
            CHECK(i);
            CHECK_FALSE(i.name().empty());
            CHECK_FALSE(i.account_name().empty());
        }
        THEN("the identity does not exist") {
            CHECK_FALSE(exists(i));
        }
        THEN("a copy of the identity is equal to the original") {
            auto c = identity{i};
            CHECK((c == i));
        }
    }

    WHEN("constructing invalid identities") {
        THEN("each idenity is invalid") {
            auto i = identity::invalid_user();
            CHECK_FALSE(i);
            CHECK_FALSE(exists(i));
            CHECK(i.name().empty());
            CHECK(i.account_name().empty());

            i = identity::invalid_group();
            CHECK_FALSE(i);
            CHECK_FALSE(exists(i));
            CHECK(i.name().empty());
            CHECK(i.account_name().empty());
        }
    }

    WHEN("copying an identity") {
        const auto i = identity::process_user();
        const identity i2{i};
        const auto i3 = i;
        THEN("the copy is valid and equal to the original") {
            CHECK(i2);
            CHECK(i3);
            CHECK((i == i2));
            CHECK((i == i3));
        }
    }

    WHEN("moving an identity") {
        auto i = identity::process_user();
        const identity i2{std::move(i)};

        i = identity::process_user();
        const auto i3 = std::move(i);
        THEN("the new instance is valid") {
            // On UNIX, the original instance may still be valid, but not on Windows.
            CHECK(i2);
            CHECK(i3);
        }
    }

    WHEN("swapping identities") {
        auto i = identity::process_user();
        CHECK(i);
        auto i2 = identity::invalid_user();
        CHECK_FALSE(i2);
        swap(i, i2);
        THEN("the identities are swapped") {
            CHECK(i2);
            CHECK_FALSE(i);
        }
    }

    WHEN("comparing separate instances of the same identity") {
        THEN("the identities are equal") {
            auto i = identity::process_user();
            auto i2 = identity::process_user();
            CHECK((i == i2));
        }
    }
}
