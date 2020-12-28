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

#include <prosoft/core/config/config_platform.h>

#include <algorithm>
#include <thread>

#include "filesystem.hpp"

#if PS_HAVE_FILESYSTEM_CHANGE_ITERATOR

#include "filesystem_change_monitor.hpp"

#include <catch2/catch.hpp>
#include "fstestutils.hpp"

using namespace prosoft;
using namespace prosoft::filesystem;

TEST_CASE("filesystem_change_iterator") {
    using traits_type = changed_directory_iterator::traits_type;
    using config_type = traits_type::configuration_type;
    
    WHEN("constructed with invalid dir options") {
        CHECK_THROWS(changed_directory_iterator(temp_directory_path(), directory_options::none));
    }
    
    WHEN("constructed with invalid serialized data") {
        config_type c;
        c.serialize_data = "junk";
        CHECK_THROWS(changed_directory_iterator(temp_directory_path(), traits_type::defaults, std::move(c)));
    }
    
    SECTION("event test") {
        const auto root = canonical(temp_directory_path()) / process_name("fs17test");
        create_directory(root);
        REQUIRE(exists(root));
        PS_RAII_REMOVE(root);
        
        changed_directory_iterator i{root, traits_type::defaults, config_type{config_type::latency_type{0}}};
        CHECK(i != change_registration());
        
        const auto f1 = create_file( root / PS_TEXT("1") );
        PS_RAII_REMOVE(f1);
        
        const auto subdir = root / PS_TEXT("2");
        create_directory(subdir);
        PS_RAII_REMOVE(subdir);
        
        auto p = create_file( subdir / PS_TEXT("1") );
        
        const auto f2 = subdir / PS_TEXT("2");
        rename(p, f2);
        PS_RAII_REMOVE(f2);
        
        // sleep enough to deliver the above events, otherwise we might see the root rename only
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        const auto newroot = root.parent_path() / process_name("fs17test2");
        rename(root, newroot); // this will terminate the iterator
        
        int j{};
        while (!canceled(i) && ++j < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        REQUIRE(canceled(i));
        
        // Ranged for loop test -- easier to do it here instead of in another section requiring duplicate setup
        CHECK((*i).empty());
        CHECK_FALSE(begin(i)->empty());
        
        std::vector<directory_entry> entries;
        for (auto& e : i) {
            if (!e.path().empty()) {
                entries.push_back(e);
            }
        }
        
        CHECK(i.extract().empty());
        
        auto last = entries.end();
        CHECK(last != std::find(entries.begin(), last, f1));
        CHECK(last != std::find(entries.begin(), last, subdir));
        CHECK(last != std::find(entries.begin(), last, f2));
        
        rename(newroot, root); // rename back so we can cleanup
    }
}

#endif // PS_HAVE_FILESYSTEM_CHANGE_ITERATOR
