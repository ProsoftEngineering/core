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

#include <change_iterator_internal.hpp>

#include <catch2/catch_test_macros.hpp>

namespace fs = prosoft::filesystem::v1;

using namespace prosoft::filesystem;

namespace prosoft {
namespace filesystem {
inline namespace v1 {

struct test_state {
    state s;
    
    void add(fs::path p) {
        fs::change_notification n{std::move(p), fs::path{}, 0, fs::change_event::none, fs::file_type::regular};
        s.add(&n);
    }
};

} // namespace v1
} // namespace filesystem
} // namespace prosoft

TEST_CASE("change_iterator_internal") {
    WHEN("converting dir opts to events") {
        CHECK(to_events(directory_options::none) == change_event::rescan_required);
        auto expected = change_event::rescan_required|change_event::created|change_event::renamed;
        CHECK(to_events(directory_options::include_created_events) == expected);
        expected = change_event::rescan_required|change_event::content_modified;
        CHECK(to_events(directory_options::include_modified_events) == expected);
        expected |= change_event::created|change_event::renamed;
        CHECK(to_events(directory_options::include_created_events|directory_options::include_modified_events) == expected);
    }
    
    static auto nop_filter = [](const fs::change_notification& n) -> const fs::change_notification* {
        return &n;
    };
    static auto null_filter = [](const fs::change_notification&) -> const fs::change_notification* {
        return nullptr;
    };
    
    static fs::change_notification note{fs::path{}, fs::path{}, 0, fs::change_event::none, fs::file_type::unknown};
    
    using filters_type = fs::change_iterator_config::filters_type;
    
    WHEN("filter is null") {
        CHECK(call(nullptr, nullptr) == nullptr);
        CHECK(call(nullptr, &note) == &note);
    }
    
    WHEN("filter is not null") {
        CHECK(call(nop_filter, nullptr) == nullptr);
        CHECK(call(filters_type{nop_filter}, nullptr) == nullptr);
        CHECK(call(nop_filter, &note) == &note);
        CHECK(call(null_filter, &note) == nullptr);
        
        CHECK(call(filters_type{nop_filter, null_filter}, &note) == nullptr);
        CHECK(call(filters_type{null_filter, nop_filter}, &note) == nullptr);
        
        fs::change_notification n{fs::path{}, fs::path{}, 0, fs::change_event::rescan_required, fs::file_type::unknown};
        CHECK(call(filters_type{null_filter}, &n) == &n); // rescan must always pass
    }
    
    WHEN("filtering for files") {
        CHECK(call(change_iterator_config::is_regular_filter, nullptr) == nullptr);
        CHECK(call(change_iterator_config::is_regular_filter, &note) == nullptr);
        
        fs::change_notification n{fs::path{}, fs::path{}, 0, fs::change_event::none, fs::file_type::regular};
        CHECK(call(change_iterator_config::is_regular_filter, &n) == &n);
        n = {fs::path{}, fs::path{}, 0, fs::change_event::none, fs::file_type::directory};
        CHECK(call(change_iterator_config::is_regular_filter, &n) == nullptr);
        
        n = {fs::path{}, fs::path{}, 0, fs::change_event::rescan, fs::file_type::directory};
        CHECK(call(filters_type{change_iterator_config::is_regular_filter}, &n) == &n); // rescan must always pass
        n = {fs::path{}, fs::path{}, 0, fs::change_event::canceled, fs::file_type::directory};
        CHECK(call(filters_type{change_iterator_config::is_regular_filter}, &n) == &n); // cancel must always pass
    }
    
    WHEN("filtering for existing paths") {
        CHECK(call(change_iterator_config::exists_filter, nullptr) == nullptr);
        CHECK(call(change_iterator_config::exists_filter, &note) == nullptr);
        
        fs::change_notification n{fs::temp_directory_path(), fs::path{}, 0, fs::change_event::none, fs::file_type::directory};
        CHECK(call(change_iterator_config::exists_filter, &n) == &n);
        n = {fs::path{}, fs::temp_directory_path(), 0, fs::change_event::none, fs::file_type::none}; // dir still exists if type is none
        CHECK(call(change_iterator_config::exists_filter, &n) == &n);
    }
    
    WHEN("extracting paths") {
        test_state ts;
        ts.add(fs::path{PS_TEXT("test")});
        ts.add(fs::path{PS_TEXT("test2")});
        ts.add(fs::path{PS_TEXT("test3")});
        
        auto paths = ts.s.extract();
        CHECK(paths.size() == 3);
        
        paths = ts.s.extract();
        CHECK(paths.empty());
    }
}
