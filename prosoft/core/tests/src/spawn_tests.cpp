// Copyright © 2018, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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


// Currently, private to filesystem module for Mac only.

#include <filesystem/src/spawn.hpp>
#include <string/string_component.hpp>

#include <catch2/catch.hpp>

TEST_CASE("spawn") {
    using namespace prosoft;
    spawn_cout cout;
    spawn_cerr cerr;
    
    std::system_error err{1, std::system_category()};
    CHECK(err.code().value() == 1);
    prosoft::spawn("sw_vers", spawn_args{"-productName"}, cout, cerr, err);
    CHECK(err.code().value() == 0);
    CHECK(trim(cout) == "Mac OS X");
    CHECK(cerr.empty());
    
    err = std::system_error{1, std::system_category()};
    CHECK(err.code().value() == 1);
    prosoft::spawn("8F48A42AFE184DD1B0F43B9D95BC22E1", spawn_args{}, cout, cerr, err);
    CHECK(err.code().value() == ENOENT);
    CHECK(cout.empty());
    CHECK(cerr.find("8F48A42AFE184DD1B0F43B9D95BC22E1") != spawn_cerr::npos);
    
    err = std::system_error{1, std::system_category()};
    CHECK(err.code().value() == 1);
    prosoft::spawn("top", spawn_args{}, cout, cerr, std::chrono::seconds{1}, err);
    CHECK(err.code().value() == EAGAIN);
}
