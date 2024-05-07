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

#include <snapshot_win32_internal.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace prosoft::filesystem;

namespace fs = prosoft::filesystem::v1;

TEST_CASE("snapshot_internal") {
    WHEN("changing snapshot state") {
        using namespace fs;
        snapshot snap{snapshot_id{reinterpret_cast<const unsigned char*>(&GUID_DEVINTERFACE_FLOPPY)}};
        CHECK(snap.reserved() == 0);
        snapshot_manager::set(snap, snapshot_attached);
        CHECK(snap.reserved() == snapshot_attached);
        snapshot_manager::clear(snap, snapshot_attached);
        CHECK(snap.reserved() == 0);
        snapshot_manager::set(snap, snapshot_attached);
        CHECK(snap.reserved() != 0);
        snapshot_manager::clear(snap);
        CHECK(guid(snap) == GUID_NULL);
        CHECK(snap.reserved() == 0);
    }

    WHEN("clearing a writer") {
        vss_writer w;
        CHECK_FALSE(w);
        w.name = decltype(w.name){L"test"};
        CHECK(w.name);
        w.components_added.emplace_back(vss_writer::selectable::optional, L"test");
        CHECK_FALSE(w.components_added.empty());

        clear(w);
        CHECK_FALSE(w.name);
        CHECK(w.components_added.empty());
    }

    WHEN("clearing a component") {
        vss_writer_component c;
        CHECK_FALSE(c);
        clear(c);
    }

    WHEN("processing components") {
        static auto set = [](auto& v, wchar_t* name, wchar_t* path = nullptr, bool selectable = false) {
            auto i = &v.second;
            i->bstrComponentName = name;
            i->bstrLogicalPath = path;
            i->bSelectable = selectable;
            v.first.info = i;
        };

        vss_writer w;

        using val = std::pair<vss_writer_component, VSS_COMPONENTINFO>;
        // From MSDN "Logical Pathing of Components" doc.
        val executables;
        set(executables, L"Executables");
        auto c = &executables.first;
        CHECK_FALSE(c->optional());
        CHECK(c->required());
        CHECK(c->root());
        CHECK(c->absolute_path().native() == c->name());
        CHECK(c->parent_path().empty());
        CHECK(should_add(*c, w));
        insert(*c, w);
        REQUIRE(w.components_added.size() > 0);
        CHECK(w.components_added.back().first == vss_writer::selectable::required);
        CHECK(w.components_added.back().second.native() == c->absolute_path());

        val configFiles;
        set(configFiles, L"ConfigFiles", executables.first.name());
        c = &configFiles.first;
        CHECK_FALSE(c->optional());
        CHECK(c->required());
        CHECK_FALSE(c->root());
        CHECK(c->absolute_path().native() == L"Executables\\ConfigFiles");
        CHECK(c->parent_path().native() == executables.first.name());
        CHECK(should_add(*c, w));
        insert(*c, w);

        val licenseInfo;
        set(licenseInfo, L"LicenseInfo", nullptr, true);
        c = &licenseInfo.first;
        CHECK(c->optional());
        CHECK_FALSE(c->required());
        CHECK(should_add(*c, w));
        insert(*c, w);

        val security;
        set(security, L"Security", nullptr, true);
        c = &security.first;
        CHECK(should_add(*c, w));
        insert(*c, w);

        val userInfo;
        set(userInfo, L"UserInfo", security.first.name());
        c = &userInfo.first;
        CHECK_FALSE(should_add(*c, w));

        val certificates;
        set(certificates, L"Certificates", security.first.name());
        c = &certificates.first;
        CHECK_FALSE(should_add(*c, w));

        val writerData;
        set(writerData, L"writerData", nullptr, true);
        c = &writerData.first;
        CHECK(should_add(*c, w));
        insert(*c, w);

        val set1;
        set(set1, L"Set1", writerData.first.name());
        c= &set1.first;
        CHECK_FALSE(should_add(*c, w));

        val jan;
        set(jan, L"Jan", L"writerData\\Set1");
        c = &jan.first;
        CHECK(c->absolute_path().native() == L"writerData\\Set1\\Jan");
        CHECK(c->parent_path().native() == c->logical_path());
        CHECK_FALSE(should_add(*c, w)); // Set1 is not a set, but writerData is

        val dec;
        set(dec, L"Dec", L"writerData\\Set1");
        c = &dec.first;
        CHECK_FALSE(should_add(*c, w));

        val set2;
        set(set2, L"Set2", writerData.first.name());
        c = &set2.first;
        CHECK_FALSE(should_add(*c, w));

        val jan2;
        set(jan2, L"Jan", L"writerData\\Set2");
        c = &jan2.first;
        CHECK_FALSE(should_add(*c, w));

        val dec2;
        set(dec2, L"Dec", L"writerData\\Set2");
        c = &dec2.first;
        CHECK_FALSE(should_add(*c, w));

        val query;
        set(query, L"Query", L"writerData\\QueryLogs");
        c = &query.first;
        CHECK_FALSE(should_add(*c, w));

        val usage;
        set(usage, L"Usage", writerData.first.name(), true);
        c = &usage.first;
        CHECK_FALSE(should_add(*c, w));

        val janU;
        set(janU, L"Jan", L"writerData\\Usage");
        c = &janU.first;
        CHECK_FALSE(should_add(*c, w));

        val decU;
        set(decU, L"Dec", L"writerData\\Usage");
        c = &decU.first;
        CHECK_FALSE(should_add(*c, w));

        clear(w);
        writerData.second.bSelectable = false; // no longer a set root
        c = &writerData.first;
        CHECK(c->required());
        CHECK(should_add(*c, w));
        insert(*c, w);

        c = &set1.first;
        CHECK(should_add(*c, w));
        insert(*c, w);

        c = &jan.first;
        CHECK(should_add(*c, w));
        insert(*c, w);

        c = &usage.first;
        CHECK(should_add(*c, w));
        insert(*c, w);
    }
}
