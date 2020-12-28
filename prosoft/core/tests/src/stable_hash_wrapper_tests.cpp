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

#include <string>
#include <unordered_set>

#include "stable_hash_wrapper.hpp"

#include <catch2/catch.hpp>

TEST_CASE("stable_hash_wrapper") {
    using namespace prosoft;
    
    using string = std::string;
    using wrap = stable_hash_wrapper<string>;
    
    std::hash<string> string_hash;
    std::hash<wrap> wrap_hash;
    std::equal_to<wrap> wrap_eq;
    
    WHEN("constructing") {
        auto w = wrap{};
        CHECK(w.hash() == string_hash(string()));
        CHECK(w == string());
        
        w = wrap{"test"};
        string s{"test"};
        CHECK(w.hash() == string_hash(s));
        CHECK(w == s);
    }
    
    WHEN("copy constructing") {
        string s{"test"};
        auto w = wrap{s};
        auto w2 = w;
        CHECK(wrap_hash(w) == wrap_hash(w2));
        CHECK(wrap_eq(w, w2));
        
        CHECK(w == s);
        CHECK(w.hash() == string_hash(s));
        CHECK(w2 == s);
        CHECK(w2.hash() == string_hash(s));
    }
    
    WHEN("move constructing") {
        string s{"test"};
        auto w = wrap{s};
        auto w2 = std::move(w);
        CHECK(wrap_hash(w) == wrap_hash(w2));
        CHECK_FALSE(wrap_eq(w, w2));
        
        CHECK(w.get().empty());
        CHECK(w.hash() == string_hash(s));
        CHECK(w2 == s);
        CHECK(w2.hash() == string_hash(s));
    }
    
    WHEN("extracting the value") {
        const auto w = wrap("test");
        CHECK_FALSE(w.get().empty());
        
        auto s = w.extract();
        CHECK(w.get().empty());
        
        CHECK(w.hash() == string_hash(s));
        CHECK_FALSE(w == s);
    }
    
    WHEN("using in a collection") {
        std::unordered_set<wrap> set;
        set.emplace("test");
        set.emplace("test2");
        
        auto last = set.end();
        auto i = set.find("test");
        CHECK_FALSE(i == last);
        
        i = set.find("test2");
        CHECK_FALSE(i == last);
        
        auto s = i->extract();
        CHECK(i->get().empty());
        
        auto dead = i;
        
        i = set.find("test2");
        CHECK(i == last);
        
        // We have an empty string in the collection, but depending on how buckets are distributed it may or may not be found.
        // IOW, a subtle bug.
        i = set.find("");
        CHECK((i == dead || i == last));
        
        CHECK(set.size() == 2);
        set.emplace("test2");
        i = set.find("test2");
        CHECK_FALSE(i == last);
        CHECK(set.size() == 3);
        
        set.erase(dead);
        last = set.end();
        i = set.find("");
        CHECK(i == last);
        CHECK(set.size() == 2);
    }
}
