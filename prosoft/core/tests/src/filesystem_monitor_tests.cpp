// Copyright © 2016-2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <thread>

#include "filesystem.hpp"
#include "filesystem_change_monitor.hpp"

#if PS_HAVE_FILESYSTEM_CHANGE_MONITOR

#include "catch.hpp"
#include "fstestutils.hpp"

using namespace prosoft;
using namespace prosoft::filesystem;

TEST_CASE("filesystem_monitor") {
    SECTION("default values") {
        CHECK(is_set(change_event::all & change_event::created));
        CHECK(is_set(change_event::all & change_event::removed));
        CHECK(is_set(change_event::all & change_event::renamed));
        CHECK(is_set(change_event::all & change_event::content_modified));
        CHECK(is_set(change_event::all & change_event::metadata_modified));
    
        change_registration reg;
        CHECK_FALSE(reg);
        CHECK_FALSE(reg == change_registration());
        
        change_config cfg;
        CHECK(cfg.state == nullptr);
        CHECK(cfg.notification_latency > change_config::latency_type());
        CHECK(cfg.events == change_event::all);
    }
    
    WHEN("registration is invalid") {
        change_registration reg;
        REQUIRE_FALSE(reg);
        CHECK_THROWS(stop(reg));
    }
    
    WHEN("args are invalid") {        
        CHECK_THROWS(monitor(PS_TEXT(""), change_event::none, change_callback{}));
        CHECK_THROWS(monitor(PS_TEXT("test"), change_event::none, change_callback{}));
        CHECK_THROWS(monitor(PS_TEXT("test"), change_event::created, change_callback{}));
        
        CHECK_THROWS(recursive_monitor(PS_TEXT(""), change_event::none, change_callback{}));
        CHECK_THROWS(recursive_monitor(PS_TEXT("test"), change_event::none, change_callback{}));
        CHECK_THROWS(recursive_monitor(PS_TEXT("test"), change_event::created, change_callback{}));
    }
    
    WHEN("unique change registration goes out of scope") {
        change_registration reg;
        {
            unique_change_registration ureg{recursive_monitor(temp_directory_path(), [](const change_notifications&) {
            })};
            reg = ureg;
            CHECK(reg);
        }
        // The reg may still be valid (if internal state is still alive) but it should no longer be registered as a monitor.
        CHECK_THROWS(stop(reg));
    }
    
    SECTION("recursive monitor") {
        const auto root = canonical(temp_directory_path()) / process_name("fs17test");
        create_directory(root);
        REQUIRE(exists(root));
        
        std::mutex lock;
        using guard = std::lock_guard<std::mutex>;
        change_notifications notes;
        
        change_config cfg;
        cfg.notification_latency = change_config::latency_type{0}; // keep coalescing to a minimum
        constexpr auto sleep_duration = change_config::latency_type{300};
        
        WHEN("creating a file") {
            const auto p = root / PS_TEXT("1");
            error_code ec;
            REQUIRE_FALSE(exists(p, ec));
            
            unique_change_registration reg{recursive_monitor(root, cfg, [&lock, &notes](const change_notifications& n) {
                guard lg{lock};
                notes.insert(notes.end(), n.begin(), n.end());
            })};
            CHECK(reg);
            
            create_file(p);
            
            std::this_thread::sleep_for(sleep_duration);
            stop(reg);
            CHECK(remove(p));
            
            CHECK_FALSE(notes.empty());
            const auto count = std::count_if(notes.begin(), notes.end(), [](const change_notification& n) {
                return created(n);
            });
            CHECK(count > 0);
        }
        
        WHEN("modifying the content of a file") {
            const auto p = create_file(root / PS_TEXT("1"));
            REQUIRE(exists(p));
            
            unique_change_registration reg{recursive_monitor(root, cfg, [&lock, &notes](const change_notifications& n) {
                guard lg{lock};
                notes.insert(notes.end(), n.begin(), n.end());
            })};
            CHECK(reg);
            
            {
                std::ofstream stream(p.c_str());
                CHECK(stream);
                stream << "hello world" << std::flush;
            }
            
            std::this_thread::sleep_for(sleep_duration);
            stop(reg);
            CHECK(remove(p));
            
            CHECK_FALSE(notes.empty());
            const auto count = std::count_if(notes.begin(), notes.end(), [](const change_notification& n) {
                return modified(n);
            });
            CHECK(count > 0);
        }
        
        WHEN("modifying the metadata of a file") {
            const auto p = create_file(root / PS_TEXT("1"));
            REQUIRE(exists(p));
        
            unique_change_registration reg{recursive_monitor(root, cfg, [&lock, &notes](const change_notifications& n) {
                guard lg{lock};
                notes.insert(notes.end(), n.begin(), n.end());
            })};
            CHECK(reg);
            
            using namespace std::chrono;
            const auto t = std::chrono::system_clock::now() - duration_cast<file_time_type::duration>(seconds(3600));
            CHECK_NOTHROW(last_write_time(p, t));
            
            std::this_thread::sleep_for(sleep_duration);
            stop(reg);
            CHECK(remove(p));
            
            CHECK_FALSE(notes.empty());
            const auto count = std::count_if(notes.begin(), notes.end(), [](const change_notification& n) {
                return modified(n);
            });
            CHECK(count > 0);
        }
        
        WHEN("removing a file") {
            const auto p = create_file(root / PS_TEXT("1"));
            REQUIRE(exists(p));
        
            unique_change_registration reg{recursive_monitor(root, cfg, [&lock, &notes](const change_notifications& n) {
                guard lg{lock};
                notes.insert(notes.end(), n.begin(), n.end());
            })};
            CHECK(reg);
            
            CHECK(remove(p));
            
            std::this_thread::sleep_for(sleep_duration);
            stop(reg);
            
            CHECK_FALSE(notes.empty());
            const auto count = std::count_if(notes.begin(), notes.end(), [](const change_notification& n) {
                return removed(n);
            });
            CHECK(count > 0);
        }
        
        WHEN("renaming a file") {
            const auto p = create_file(root / PS_TEXT("1"));
            REQUIRE(exists(p));
            
            unique_change_registration reg{recursive_monitor(root, cfg, [&lock, &notes](const change_notifications& n) {
                guard lg{lock};
                notes.insert(notes.end(), n.begin(), n.end());
            })};
            CHECK(reg);
            
            const auto np = root / PS_TEXT("2");
            rename(p, np);
            
            std::this_thread::sleep_for(sleep_duration);
            stop(reg);
            
            error_code ec;
            CHECK_FALSE(exists(p, ec));
            CHECK(exists(np));
            CHECK(remove(np));
            
            CHECK_FALSE(notes.empty());
            const auto count = std::count_if(notes.begin(), notes.end(), [](const change_notification& n) {
                return renamed(n);
            });
            CHECK(count > 0);
        }
        
        WHEN("the monitor root is renamed") {
            unique_change_registration reg{recursive_monitor(root, cfg, [&lock, &notes](const change_notifications& n) {
                guard lg{lock};
                notes.insert(notes.end(), n.begin(), n.end());
            })};
            CHECK(reg);
            
            const auto np = root.parent_path() / process_name("fs17test2");
            rename(root, np);
            
            std::this_thread::sleep_for(sleep_duration);
            stop(reg);
            
            rename(np, root); // rename back so cleanup succeeds
            
            CHECK_FALSE(notes.empty());
            const auto i = std::find_if(notes.begin(), notes.end(), [](const change_notification& n) {
                return is_set(n.event() & (change_event::rescan_required|change_event::renamed));
            });
            REQUIRE(i != notes.end());
            if (!i->renamed_to_path().empty()) { // race conditions in event delivery vs. filesystem state can result in an empty path
                CHECK(i->renamed_to_path() == np);
            }
        }
        
        WHEN("the monitor root is removed") {
            unique_change_registration reg{recursive_monitor(root, cfg, [&lock, &notes](const change_notifications& n) {
                guard lg{lock};
                notes.insert(notes.end(), n.begin(), n.end());
            })};
            CHECK(reg);
            
            CHECK(remove(root));
            
            std::this_thread::sleep_for(sleep_duration);
            stop(reg);
            
            create_directory(root); // recreate so cleanup succeeds
            
            CHECK_FALSE(notes.empty());
            const auto count = std::count_if(notes.begin(), notes.end(), [](const change_notification& n) {
                return is_set(n.event() & (change_event::rescan_required|change_event::removed));
            });
            CHECK(count > 0);
        }
        
        REQUIRE(remove(root));
    }
}

#endif // PS_HAVE_FILESYSTEM_CHANGE_MONITOR
