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

#include <fsevents_monitor_internal.hpp>

#include <catch2/catch_test_macros.hpp>

namespace fs = prosoft::filesystem::v1;

using namespace prosoft::filesystem;

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
using Catch::Matchers::WithinAbs;

using namespace prosoft::filesystem;
#include <fstestutils.hpp>

// Xcode 8.2.1: using a vector the following test will crash either with EXC_BAD_ACCESS or an ASAN violation in vector::emplace_back.
// Which crash depends on how many events are present. I reduced the callback to an empty emplace and the crash still occurred.
// I can find no reason in code for the crash so can only surmise there's an issue in std::vector.
//
// In the EXC_BAD_ACCESS scenerio, emplace_back ends up with a bad std::forward arg that looks to be due to stack corruption (address of 0x04, 0x01, etc).
//
// However I did create a unique type that was a duplicate of change_notification that was used for testing only
// and there was no crash with that type. The only difference being that nothing else referenced the test type.
// After many, many hours of debugging, I have no idea what is going on.
//
// As of Xcode 10.1 std::vector has no problem. Probably an issue with the Xcode 8 clang version.

void reduced_fsevents_callback_crash(ConstFSEventStreamRef, void*, size_t nevents, void*, const FSEventStreamEventFlags[], const FSEventStreamEventId[]) {
    fs::change_notifications notes;
    for (size_t i = 0; i < nevents; ++i) {
        notes.emplace_back(fs::path{}, fs::path{}, 0ULL, fs::change_event{}, fs::file_type{});
    }
}

TEST_CASE("fsevents_vector_crash") {
    struct TestDispatcher {
        void operator()(platform_state*, std::unique_ptr<fs::change_notifications>, FSEventStreamEventId) {
        }
    };
    
    std::vector<const char*> paths;
    std::vector<FSEventStreamEventFlags> flags;
    std::vector<FSEventStreamEventId> ids;
    const path::string_type p1{"/private/var/folders/bs/b0d7mq1x7gb73k66lvxgfcsm0000gn/T/fs17test/1"};
    const path::string_type p2{"/private/var/folders/bs/b0d7mq1x7gb73k66lvxgfcsm0000gn/T/fs17test/2"};
    const path::string_type p3{"/private/var/folders/bs/b0d7mq1x7gb73k66lvxgfcsm0000gn/T/fs17test/2/1"};
    const path::string_type p4{"/private/var/folders/bs/b0d7mq1x7gb73k66lvxgfcsm0000gn/T/fs17test/2/2"};
    
    paths.push_back(p1.c_str());
    flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemIsFile);
    ids.push_back(18158642889452409530ULL);
    
    paths.push_back(p2.c_str());
    flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemIsDir);
    ids.push_back(18158642889452409533ULL);
    
    paths.push_back(p3.c_str());
    flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemXattrMod|kFSEventStreamEventFlagItemIsFile);
    ids.push_back(18158642889452409539ULL);
    
    paths.push_back(p4.c_str());
    flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemIsFile);
    ids.push_back(18158642889452409540ULL);
    
    paths.push_back(p1.c_str());
    flags.push_back(kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemIsFile);
    ids.push_back(18158642889452409541ULL);
    
    reduced_fsevents_callback_crash(nullptr, nullptr, paths.size(), paths.data(), flags.data(), ids.data());
    
    platform_state state;
    fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
}

class set_monitor_runloop_guard {
    CFRunLoopRef oldval;
public:
    set_monitor_runloop_guard(CFRunLoopRef rl) {
        oldval = monitor_thread_run_loop;
        monitor_thread_run_loop = rl;
    }
    ~set_monitor_runloop_guard() {
        monitor_thread_run_loop = oldval;
    }
    PS_DISABLE_COPY(set_monitor_runloop_guard);
};

TEST_CASE("filesystem_monitor_internal") {
    SECTION("common") {
        SECTION("notification") {
            auto state = std::make_shared<platform_state>();
            auto reg = change_manager::make_registration(state);
            CHECK(reg);
            auto note = change_manager::make_notification(PS_TEXT("test"), PS_TEXT(""), state.get(), change_event::created);
            CHECK(note.path() == path{PS_TEXT("test")});
            CHECK(note.renamed_to_path().empty());
            CHECK(note.event() == change_event::created);
            CHECK(note == reg);
            CHECK_FALSE(type_known(note));

            auto p = note.extract_path();
            CHECK(p.native() == PS_TEXT("test"));
            CHECK(note.path().empty());
            CHECK(note.event() == change_event::none);
            CHECK_FALSE(note == reg);

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::created, file_type::regular);
            CHECK(type_known(note));
            CHECK(created(note));

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::removed);
            CHECK(removed(note));

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::renamed);
            CHECK(renamed(note));

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::content_modified);
            CHECK(content_modified(note));
            CHECK(modified(note));

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::metadata_modified);
            CHECK(metadata_modified(note));
            CHECK(modified(note));

            note = change_manager::make_notification(PS_TEXT(""), PS_TEXT(""), nullptr, change_event::rescan_required);
            CHECK(rescan(note));
            CHECK(canceled(note));

            // test copy/move into vector
            change_notifications notes;
// Xcode 7&8 ASAN both fire "heap buffer overflow" for change_manager default copy and move if we use reserve.
// The output seems to point to some memory allocated by Catch, it may be the trigger as I cannot reproduce the problem with a simple test binary.
#if 0
            notes.reserve(2);
#endif
            note = change_manager::make_notification(PS_TEXT("test"), path{}, nullptr, change_event::rescan_required);
            notes.push_back(note);
            notes.emplace_back(change_manager::make_notification(PS_TEXT("test"), path{}, nullptr, change_event::rescan_required));
            CHECK(notes.size() == 2);
        }
    }
    
    WHEN("state is created with an invalid path") {
        change_config cfg(change_event::all);
        error_code ec;
        auto p = std::make_unique<platform_state>("test", cfg, ec);
        CHECK(p);
        CHECK(ec.value() != 0);
        CHECK_FALSE(p->m_stream);
        CHECK_FALSE(p->m_dispatch_q);
        CHECK_FALSE(p->m_uuid);
        CHECK(p->m_rootfd == -1);
        CHECK(*p == *p);
        CHECK(canonical_root_path(p.get()).empty());
    }
    
    WHEN("state is created with a valid path") {
        change_config cfg(change_event::all);
        error_code ec;
        auto p = std::make_unique<platform_state>("/", cfg, ec);
        CHECK(p);
        CHECK(!ec.value());
        CHECK(p->m_stream);
        CHECK(p->m_lastid == kFSEventStreamEventIdSinceNow);
        CHECK(p->m_dispatch_q);
        CHECK(p->m_uuid);
        CHECK(p->m_rootfd > -1);
        CHECK(*p == *p);
        CHECK(canonical_root_path(p.get()).native() == "/");
    }
    
    WHEN("state is created with a valid path and restore state") {
        change_config cfg(change_event::all);
        const auto archive = change_state::serialize(path{"/"});
        CHECK_FALSE(archive.empty());
        auto state = change_state::serialize(archive);
        cfg.state = state.get();
        CHECK_FALSE(replay(cfg));
        error_code ec;
        auto p = std::make_unique<platform_state>("/", cfg, ec);
        CHECK(p);
        CHECK(!ec.value());
        CHECK(p->m_uuid);
        CHECK(p->m_lastid != kFSEventStreamEventIdSinceNow);
        CHECK(p->m_lastid == dynamic_cast<platform_state*>(cfg.state)->m_lastid);
    }
    
    WHEN("state is not registered") {
        error_code ec;
        auto p = std::make_unique<platform_state>();
        stop(p.get(), ec);
        CHECK(ec.value() != 0);
        CHECK_FALSE(get_shared_state(p.get()));
    }
    
    WHEN("registering a monitor") {
        change_config cfg(change_event::all);
        error_code ec;
        auto ss = std::make_shared<platform_state>("/", cfg, ec);
        auto p = ss.get();
        
        set_monitor_runloop_guard rlg{CFRunLoopGetCurrent()}; // ignore the actual runloop
        auto reg = register_events_monitor(std::move(ss), [](auto){}, ec);
        CHECK(reg);
        CHECK_FALSE(ss);
        ss = get_shared_state(p);
        CHECK(ss.use_count() == 2);
        ec.clear();
        unregister_events_monitor(p, ec);
        CHECK(!ec.value());
        CHECK_FALSE(get_shared_state(p));
        CHECK(ss.use_count() == 1);
    }
    
    WHEN("registering a monitor fails") {
        change_config cfg(change_event::all);
        error_code ec;
        auto ss = std::make_shared<platform_state>("/", cfg, ec);
        auto p = ss.get();
        
        set_monitor_runloop_guard rlg{nullptr}; // Force FSEventStreamStart to fail (will print console messages)
        auto reg = register_events_monitor(std::move(ss), [](auto){}, ec);
        CHECK_FALSE(reg);
        CHECK(ec.value() == platform_error::monitor_start);
        CHECK_FALSE(get_shared_state(p));
    }
    
    SECTION("conversion") {
        CHECK(to_event(0) == change_event::none);
        CHECK(to_event(kFSEventStreamEventFlagMount) == change_event::rescan);
        CHECK(to_event(kFSEventStreamEventFlagUnmount) == change_event::rescan);
        // Mount/Unmount are should be mutually exclusive, check that assumption.
        CHECK(to_event(kFSEventStreamEventFlagMount|kFSEventStreamEventFlagItemCreated) == change_event::rescan);
        CHECK(to_event(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemModified|kFSEventStreamEventFlagItemRemoved) == change_event::removed);
        CHECK(to_event(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemModified) == (change_event::created|change_event::content_modified));
        CHECK(to_event(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemRenamed) == (change_event::created|change_event::renamed));
        CHECK(to_event(kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemRenamed) == (change_event::removed|change_event::renamed));
        constexpr auto allmod = change_event::content_modified|change_event::metadata_modified;
        CHECK(to_event(kFSEventStreamEventFlagItemModified|kFSEventStreamEventFlagItemInodeMetaMod) == allmod);
        CHECK(to_event(kFSEventStreamEventFlagItemModified|kFSEventStreamEventFlagItemFinderInfoMod) == allmod);
        CHECK(to_event(kFSEventStreamEventFlagItemModified|kFSEventStreamEventFlagItemChangeOwner) == allmod);
        CHECK(to_event(kFSEventStreamEventFlagItemModified|kFSEventStreamEventFlagItemXattrMod) == allmod);
        
        CHECK(to_type(0) == file_type::none);
        CHECK(to_type(kFSEventStreamEventFlagItemIsFile) == file_type::regular);
        CHECK(to_type(kFSEventStreamEventFlagItemIsDir) == file_type::directory);
        CHECK(to_type(kFSEventStreamEventFlagItemIsSymlink) == file_type::symlink);
        
        CHECK(rescan_required(kFSEventStreamEventFlagRootChanged));
        CHECK(rescan_required(kFSEventStreamEventFlagMustScanSubDirs));
        CHECK(rescan_required(kFSEventStreamEventFlagRootChanged|kFSEventStreamEventFlagMustScanSubDirs));
        CHECK_FALSE(rescan_required(kFSEventStreamEventFlagItemCreated));
        
        CHECK(root_changed(kFSEventStreamEventFlagRootChanged));
        CHECK_FALSE(root_changed(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagMustScanSubDirs));
    }
    
    SECTION("event handler") {
        static change_notifications notes;
        static FSEventStreamEventId lastID{};
        struct TestDispatcher {
            void operator()(platform_state*, std::unique_ptr<fs::change_notifications> nn, FSEventStreamEventId lastNoteID) {
                notes = std::move(*nn.release());
                lastID = lastNoteID;
            }
        };
        
        std::vector<const char*> paths;
        std::vector<FSEventStreamEventFlags> flags;
        std::vector<FSEventStreamEventId> ids;
        
        const auto root = canonical(temp_directory_path()) / PS_TEXT("fs17test");
        error_code root_ec;
        remove(root, root_ec);
        REQUIRE_FALSE(exists(root, root_ec));
        notes.clear();
        lastID = 0;
        
        WHEN("there are no events") {
            platform_state state;
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            CHECK(notes.empty());
        }
        
        WHEN("a root change event is present") {
            platform_state state;
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagRootChanged);
            flags.push_back(0);
            paths.push_back("test");
            flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE_FALSE(notes.empty());
            auto& n = notes[0];
            CHECK(n.event() == (change_event::removed|change_event::canceled|change_event::rescan));
            CHECK(n.path() == root);
            CHECK(n.renamed_to_path().empty());
            CHECK(notes.size() == 1); // all events after the canceled event should be dropped
        }
        
        WHEN("a scan sub dirs event is present") {
            platform_state state;
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagMustScanSubDirs);
            ids.push_back(0);
            paths.push_back("test");
            flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE_FALSE(notes.empty());
            auto& n = notes[0];
            CHECK(n.event() == (change_event::canceled|change_event::rescan));
            CHECK(n.path() == root);
            CHECK(n.renamed_to_path().empty());
            CHECK(notes.size() == 1); // all events after the canceled event should be dropped
        }
        
        WHEN("root is renamed") {
            create_file(root);
            REQUIRE(exists(root));
            
            change_config cfg(change_event::all);
            error_code ec;
            auto ss = std::make_unique<platform_state>(root, cfg, ec);
            auto state = ss.get();
            REQUIRE(state->m_rootfd > -1);
            
            const auto np = canonical(temp_directory_path()) / PS_TEXT("fs17test2");
            rename(root, np);
            REQUIRE_FALSE(exists(root, ec));
            REQUIRE(exists(np));
            
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagRootChanged);
            ids.push_back(0);
            fsevents_callback<TestDispatcher>(nullptr, state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE(remove(np)); // remove before any other REQUIREs
            REQUIRE_FALSE(notes.empty());
            auto& n = notes[0];
            CHECK(n.event() == (change_event::renamed|change_event::canceled));
            CHECK(n.path() == root);
            CHECK(n.renamed_to_path() == np);
            CHECK(n.type() == file_type::directory);
        }
        
        WHEN("a remove event is present with no other events and the item does not exist") {
            platform_state state;
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE_FALSE(notes.empty());
            auto& n = notes[0];
            CHECK(n.event() == change_event::removed);
            CHECK(n.path() == root);
            CHECK(n.type() == file_type::regular);
        };
        
        WHEN("a remove event is present with no other events and the item does exist") {
            platform_state state;
            create_file(root);
            REQUIRE(exists(root));
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE_FALSE(notes.empty());
            auto& n = notes[0];
            CHECK(n.event() == change_event::removed); // event is still removed as only stat the file if other events are present
            CHECK(n.path() == root);
            CHECK(n.type() == file_type::regular);
        };
        
        WHEN("a remove event is present with other events and the item does not exist") {
            platform_state state;
            error_code ec;
            REQUIRE_FALSE(exists(root, ec));
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            auto& n = notes[0];
            CHECK(n.event() == change_event::removed);
            CHECK(n.path() == root);
            CHECK(n.type() == file_type::regular);
        };
        
        WHEN("a remove event is present with other events and the item exists") {
            platform_state state;
            create_file(root);
            REQUIRE(exists(root));
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagItemCreated|kFSEventStreamEventFlagItemRemoved|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE(remove(root));
            auto& n = notes[0];
            CHECK(n.event() == change_event::created); // event is not removed as the file was stat'd
            CHECK(n.path() == root);
            CHECK(n.type() == file_type::regular);
        };
        
        WHEN("a history done event is present") {
            platform_state state;
            paths.push_back(root.c_str());
            flags.push_back(kFSEventStreamEventFlagHistoryDone);
            ids.push_back(0);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            CHECK(notes.empty());
        }
        
        WHEN("a mount/unmount event is present") {
            platform_state state;
            const auto p = root / PS_TEXT("test");
            paths.push_back(p.c_str());
            flags.push_back(kFSEventStreamEventFlagMount);
            ids.push_back(1);
            paths.push_back(p.c_str());
            flags.push_back(kFSEventStreamEventFlagUnmount);
            ids.push_back(2);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE_FALSE(notes.empty());
            auto* n = &notes[0];
            CHECK(n->event() == change_event::rescan);
            CHECK(n->path() == p);
            CHECK(n->renamed_to_path().empty());
            CHECK_FALSE(type_known(*n));
            REQUIRE(notes.size() > 1);
            n = &notes[1];
            CHECK(n->event() == change_event::rescan);
            CHECK(n->path() == p);
            CHECK(n->renamed_to_path().empty());
            CHECK_FALSE(type_known(*n));
            CHECK(lastID == 2);
        }
        
        WHEN("there are 2 rename events with matching ids") {
            platform_state state;
            const auto p = root / PS_TEXT("test");
            const auto np = root / PS_TEXT("test2");
            paths.push_back(p.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            paths.push_back(np.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            CHECK(notes.size() == 2);
            fs::change_manager::process_renames(notes);
            REQUIRE(notes.size() == 1);
            auto& n = notes[0];
            CHECK(n.event() == change_event::renamed);
            CHECK(n.path() == p);
            CHECK(n.renamed_to_path() == np);
            CHECK(n.type() == file_type::regular);
            CHECK(lastID == 1);
        }
        
        WHEN("history done flag is set") {
            platform_state state;
            paths.push_back(PS_TEXT(""));
            flags.push_back(kFSEventStreamEventFlagHistoryDone);
            ids.push_back(1);
            
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            CHECK(notes.size() == 0); // ignored as m_stopid is 0
            
            state.m_stopid = 2;
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE(notes.size() == 1);
            auto& n = notes[0];
            CHECK(n.event() == change_event::replay_end);
            CHECK(n.path().empty());
            CHECK(n.type() == file_type::none);
        }
        
        WHEN("there are 2 rename events with matching ids and the 2nd event has the removed flag set") {
            platform_state state;
            const auto p = root / PS_TEXT("test");
            const auto np = root / PS_TEXT("test2");
            paths.push_back(p.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemIsFile);
            ids.push_back(1);
            paths.push_back(np.c_str());
            flags.push_back(kFSEventStreamEventFlagItemRenamed|kFSEventStreamEventFlagItemIsFile|kFSEventStreamEventFlagItemRemoved);
            ids.push_back(1);
            fsevents_callback<TestDispatcher>(nullptr, &state, paths.size(), paths.data(), flags.data(), ids.data());
            REQUIRE(notes.size() == 2);
            auto* n = &notes[0];
            CHECK(n->event() == change_event::renamed);
            CHECK(n->renamed_to_path().empty());
            n = &notes[1];
            CHECK(n->event() == (change_event::renamed|change_event::removed));
            fs::change_manager::process_renames(notes);
            REQUIRE(notes.size() == 2);
            n = &notes[0];
            CHECK(n->event() == change_event::renamed);
            CHECK(n->renamed_to_path().empty());
            n = &notes[1];
            CHECK(n->event() == change_event::removed);
        }
    }
    
    SECTION("platform assumptions") {
        WHEN("converting config latency to native time") {
            change_config::latency_type l{1000};
            auto cfl = std::chrono::duration_cast<cfduration>(l).count();
            REQUIRE_THAT(cfl, WithinAbs(1.0, 0.01));
            
            l = change_config::latency_type{2500};
            cfl = std::chrono::duration_cast<cfduration>(l).count();
            REQUIRE_THAT(cfl, WithinAbs(2.5, 0.01));
            
            l = change_config::latency_type{100};
            cfl = std::chrono::duration_cast<cfduration>(l).count();
            REQUIRE_THAT(cfl, WithinAbs(0.1, 0.01));
        }
    
        WHEN("when a file is renamed") {
            const auto temp = canonical(temp_directory_path()); // /var symlinks to /private/var
            
            const auto p = create_file(temp / PS_TEXT("fs17test"));
            REQUIRE(exists(p));
            
            int fd = open(p.c_str(), O_EVTONLY);
            CHECK(fd > -1);
            
            const auto np = temp / PS_TEXT("fs17test2");
            int ec = rename(p.c_str(), np.c_str());
            CHECK(ec == 0);
            
            char buf[PATH_MAX];
            ec = fcntl(fd, F_GETPATH, buf);
            CHECK(ec == 0);
            CHECK(np.native() == buf);
            close(fd);
            
            REQUIRE(remove(np));
        }
        
        WHEN("when a file is deleted") {
            const auto temp = canonical(temp_directory_path()); // /var symlinks to /private/var
            const auto p = create_file(temp / PS_TEXT("fs17test"));
            REQUIRE(exists(p));
            
            int fd = open(p.c_str(), O_EVTONLY);
            CHECK(fd > -1);
            
            REQUIRE(remove(p));
            
            char buf[PATH_MAX];
            int ec = fcntl(fd, F_GETPATH, buf);
            CHECK(ec == 0);
            CHECK(p.native() == buf);
            close(fd);
        }
    }
}
