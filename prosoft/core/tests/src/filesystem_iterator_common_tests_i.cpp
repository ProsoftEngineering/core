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

// DO NOT INCLUDE IN BUILD TARGET SOURCE.
// This is an implementation include file for concrete test types.

WHEN("creating a default iterator") {
    iterator_type i;
    CHECK(i.options() == directory_options::none);
    CHECK(empty == *i);
    CHECK(i->path().empty());
    CHECK(i == end(i));
    CHECK(i++ == end(i));
    CHECK(i++ == end(i));
}

WHEN("creating an iterator with an invalid path") {
    CHECK_THROWS(iterator_type(PS_TEXT("")));
    error_code ec;
    iterator_type i{PS_TEXT(""), ec};
    CHECK(i == end(i));
    
    const auto p = create_file(temp_directory_path() / PS_TEXT("fs17test"));
    REQUIRE(exists(p));
    CHECK_THROWS((void)iterator_type(p));
    REQUIRE(remove(p));
}

WHEN("creating an iterator with a valid path") {
    iterator_type i{temp_directory_path()};
    CHECK(i.options() == i.default_options());
    CHECK(i != end(i));
}

WHEN("creating an iterator with reserved options") {
    iterator_type i{temp_directory_path(), directory_options::follow_directory_symlink|directory_options::reserved_state_mask};
    CHECK_FALSE(is_set(i.options() & directory_options::reserved_state_mask));
}
