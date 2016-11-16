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

#include <filesystem/filesystem.hpp>

#include "catch.hpp"

using namespace prosoft;
using namespace prosoft::filesystem;

#include "fstestutils.hpp"

TEST_CASE("filesystem_iterator") {
    WHEN("reserved options are set") {
        CHECK(make_public(directory_options::reserved_state_mask) == directory_options::none);
    }

    static const directory_entry empty;

    SECTION("iterator common") {
        using iterator_type = directory_iterator;
        #include "filesystem_iterator_common_tests_i.cpp"
    }
    
    SECTION("recursive iterator common") {
        using iterator_type = recursive_directory_iterator;
        #include "filesystem_iterator_common_tests_i.cpp"
    }
    
    SECTION("iterator") {
        SECTION("recurse option") {
            const auto p = create_file(temp_directory_path() / PS_TEXT("fs17test"));
            REQUIRE(exists(p));
            
            directory_iterator i{temp_directory_path()};
            CHECK(is_set(i.options() & directory_options::skip_subdirectory_descendants));
            i = directory_iterator{temp_directory_path(), directory_options::follow_directory_symlink};
            CHECK(is_set(i.options() & directory_options::skip_subdirectory_descendants));
            
            REQUIRE(remove(p));
        }
    }
    
    SECTION("recursive iterator") {
        WHEN("creating a default recursive iterator") {
            recursive_directory_iterator i;
            CHECK(i == end(i));
            CHECK(i->path().empty());
            CHECK(i.depth() == iterator_depth_type());
            CHECK_FALSE(i.recursion_pending());
            i.disable_recursion_pending();
            i.pop();
            CHECK_FALSE(i.recursion_pending());
            CHECK(i == end(i));
        }
        
        SECTION("recurse option") {
            const auto p = create_file(temp_directory_path() / PS_TEXT("fs17test"));
            REQUIRE(exists(p));
        
            recursive_directory_iterator i{temp_directory_path()};
            CHECK(!is_set(i.options() & directory_options::skip_subdirectory_descendants));
            i = recursive_directory_iterator{temp_directory_path(), directory_options::follow_directory_symlink};
            CHECK(!is_set(i.options() & directory_options::skip_subdirectory_descendants));
            i = recursive_directory_iterator{temp_directory_path(), directory_options::follow_directory_symlink|directory_options::skip_subdirectory_descendants};
            CHECK(!is_set(i.options() & directory_options::skip_subdirectory_descendants));
            
            REQUIRE(remove(p));
        }
    }
}
