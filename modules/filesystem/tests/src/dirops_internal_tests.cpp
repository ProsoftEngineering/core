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

#include <dirops_internal.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dirops_internal") {
    using namespace prosoft::filesystem;

    SECTION("directory exists") {
        auto p = ""_p; // empty path fail
        auto ec = error_code{0, filesystem_category()};
        REQUIRE(ec.value() == 0);
        REQUIRE(p.empty());
        assert_directory_exists(p, ec);
        CHECK(ec.value() != 0);

        p = "adfhaosidufbasdfoiuabsdofiubasodbfaosdbfaosbdfoabsdf"_p; // not dir fail
        ec.clear();
        REQUIRE(ec.value() == 0);
        REQUIRE_FALSE(p.empty());
        assert_directory_exists(p, ec);
        CHECK(ec.value() != 0);
        CHECK(p.empty());

        p = "test"_p;
        ec = error_code{5, filesystem_category()}; // error fail
        REQUIRE_FALSE(p.empty());
        assert_directory_exists(p, ec);
        CHECK(ec.value() == 5);
        CHECK(p.empty());
    }
}
