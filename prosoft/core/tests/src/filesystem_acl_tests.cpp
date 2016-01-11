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

#include <filesystem/filesystem.hpp>

#include "catch.hpp"

using namespace prosoft;
using namespace prosoft::filesystem;

TEST_CASE("filesystem acl") {

    WHEN("constructing a default entry") {
        access_control_entry a;
        THEN("the entry is invalid") {
            CHECK(!a);
        }
    }
    
    WHEN("constructing two entries with the same parameters") {
        access_control_entry a1;
        access_control_entry a2;
        THEN("they are equal") {
            CHECK((a1 == a2));
        }
    }
    
    WHEN("constructing an entry with a valid identity") {
        access_control_entry a{access_control_identity::process_user()};
        THEN("the entry is valid") {
            CHECK(a);
        }
    }
    
    WHEN("copying an entry") {
        const access_control_entry a1{access_control_type::allow, access_control_flags::inherit, access_control_perms::all, access_control_identity::process_user()};
        const access_control_entry a2{a1};
        THEN("they are equal") {
            CHECK((a1 == a2));
        }
    }
    
    WHEN("swapping entries") {
        access_control_entry a1{access_control_type::allow, access_control_flags::inherit, access_control_perms::all, access_control_identity::process_user()};
        access_control_entry a2;
        CHECK(a1);
        CHECK_FALSE(a2);
        
        const auto expected = a1;
        using std::swap;
        swap(a1, a2);
        THEN("the entry contents are swapped") {
            CHECK_FALSE(a1);
            CHECK(a1 == access_control_entry{});
            
            CHECK(a2);
            CHECK(a2 == expected);
        }
    }
    
    WHEN("setting entry properties") {
        access_control_entry a;
        a.type(access_control_type::allow);
        a.flags(access_control_flags::inherit);
        a.perms(access_control_perms::all);
        CHECK_FALSE(a);
        
        const auto i = access_control_identity::process_user();
        a.identity(i);
        THEN("the properties are updated") {
            CHECK(a);
            CHECK(a.type() == access_control_type::allow);
            CHECK(a.flags() == access_control_flags::inherit);
            CHECK(a.perms() == access_control_perms::all);
            CHECK(a.identity() == i);
        }
    }
}
