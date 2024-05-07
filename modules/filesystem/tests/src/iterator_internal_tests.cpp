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

#include <iterator_internal.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace prosoft::filesystem;

namespace fs = prosoft::filesystem::v1;

namespace {

struct test_ops {
    std::vector<native_dirent> m_ents;
    native_dirent m_cur;
    
    void push_back(fs::path::const_pointer name, fs::file_type t) {
        m_ents.emplace_back();
        auto& e = m_ents.back();
#if PS_FS_HAVE_BSD_STATFS
        e.d_namlen = prosoft::data_size(name);
#endif
        memcpy(e.d_name, name, std::min(prosoft::byte_size(name), sizeof(e.d_name)-sizeof(fs::path::encoding_value_type)));
        switch(t) {
#if !_WIN32
            case fs::file_type::regular:
                e.d_type = DT_REG;
            break;
            case fs::file_type::directory:
                e.d_type = DT_DIR;
            break;
            case fs::file_type::symlink:
                e.d_type = DT_LNK;
            break;
#else
            case fs::file_type::regular:
                e.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            break;
            case fs::file_type::directory:
                e.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            break;
            case fs::file_type::symlink:
                e.dwFileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
            break;
#endif
            default:
                PSASSERT_UNREACHABLE("Unhandled type");
            break;
        }
    }
    
    virtual native_dir* open(const fs::path& p) {
        return open_dir(p);
    }

    native_dirent* read(native_dir*) {
        errno = 0;
#if _WIN32
        SetLastError(0);
#endif
        if (m_ents.empty()) {
            std::memset(&m_cur, 0, sizeof(m_cur));
            return nullptr;
        }
        
        m_cur = m_ents[0];
        m_ents.erase(m_ents.begin());
        return &m_cur;
    }
    
    // XXX: close should not access any member data as a new instance is always created
    virtual int close(native_dir* d) {
        return close_dir(d);
    }
};

struct test_nopen : test_ops {
    virtual native_dir* open(const fs::path& p) override {
        (void)p;    // unused
        return (native_dir*)0x55UL;
    }
    
    virtual int close(native_dir* d) override {
        (void)d;    // unused
        return 0;
    }
};

template <class T, class... Args>
std::unique_ptr<T> make_ptr(Args&& ...a) {
    return std::unique_ptr<T>{ new T(std::forward<Args>(a)...) };
}

} // namespace

TEST_CASE("filesystem_iterator_internal") {
    WHEN("path is empty") {
#if !_WIN32
        constexpr int expected = -1;
#else
        constexpr int expected = 0;
#endif
        auto d = open_dir(PS_TEXT(""));
        CHECK(expected == close_dir(d));
        
        CHECK_FALSE(is_apple_double(temp_directory_path(), PS_TEXT("")));
    }
    
    WHEN("path is apple double prefix") {
        CHECK_FALSE(is_apple_double(temp_directory_path(), PS_TEXT("._")));
    }
    
    WHEN("native dir is null") {
        CHECK(-1 == close_dir(nullptr));
        CHECK(nullptr == read_dir(nullptr));
    }
    
    WHEN("leaf path contains dot component") {
        CHECK(leaf_is_dot_or_dot_dot(PS_TEXT(".")));
        CHECK(leaf_is_dot_or_dot_dot(PS_TEXT("..")));
        
        CHECK_FALSE(leaf_is_dot_or_dot_dot(PS_TEXT("/.")));
        CHECK_FALSE(leaf_is_dot_or_dot_dot(PS_TEXT("/..")));
        
        CHECK_FALSE(leaf_is_dot_or_dot_dot(PS_TEXT(".a")));
        CHECK_FALSE(leaf_is_dot_or_dot_dot(PS_TEXT("../a")));
        CHECK_FALSE(leaf_is_dot_or_dot_dot(PS_TEXT("...")));
    }
    
    using tops = state<test_ops>;
    using nops = state<test_nopen>;
    
    WHEN("using a non-recursive iterator") {
        error_code ec;
        auto s = make_ptr<tops>(temp_directory_path(), directory_iterator::default_options(), ec);
        CHECK(0 == ec.value());
        CHECK(is_set(s->options() & fs::directory_options::reserved_state_will_recurse));
        CHECK_FALSE(s->recurse());
        CHECK(s->size() == 1);
        CHECK(s->is_valid());
        CHECK_FALSE(s->is_child());
        // None of the following are legal with non-recursive iterators, we are just testing internal state.
        CHECK(s->depth() == 0);
        s->pop();
        CHECK(s->size() == 0);
        
        s = make_ptr<tops>(temp_directory_path(), directory_iterator::default_options(), ec);
        auto p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.empty());
        CHECK_FALSE(is_set(s->options() & fs::directory_options::reserved_state_mask));
        CHECK(s->size() == 0);
        
        s = make_ptr<tops>(temp_directory_path(), directory_iterator::default_options(), ec);
        s->m_ops.push_back(PS_TEXT("."), fs::file_type::directory);
        s->m_ops.push_back(PS_TEXT(".."), fs::file_type::directory);
        s->m_ops.push_back(PS_TEXT("testf"), fs::file_type::regular);
        s->m_ops.push_back(PS_TEXT("testd"), fs::file_type::directory);
        s->m_ops.push_back(PS_TEXT("tests"), fs::file_type::symlink);
        s->m_ops.push_back(PS_TEXT("._testad"), fs::file_type::regular); // orphan AppleDouble
        
        auto ents = s->m_ops.m_ents;
        REQUIRE(ents.size() > 5);
        auto i = 2;
        p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.filename().native() == ents[i++].d_name);
        CHECK(s->size() == 1);
        CHECK(s->depth() == 0);
        p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.filename().native() == ents[i++].d_name);
        CHECK(s->size() == 1);
        CHECK(s->depth() == 0);
        p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.filename().native() == ents[i++].d_name);
        CHECK(s->size() == 1);
        CHECK(s->depth() == 0);
        p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.filename().native() == ents[i++].d_name);
        CHECK(s->size() == 1);
        CHECK(s->depth() == 0);
        p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.empty());
        CHECK(s->size() == 0);
    }
    
    WHEN("using a recursive iterator") {
        error_code ec;
        {
            auto s = make_ptr<tops>(temp_directory_path(), recursive_directory_iterator::default_options(), ec);
            CHECK(0 == ec.value());
            CHECK(is_set(s->options() & fs::directory_options::reserved_state_will_recurse));
            CHECK(s->recurse());
            CHECK(s->size() == 1);
            CHECK(s->is_valid());
            CHECK_FALSE(s->is_child());
            CHECK(s->depth() == 0);
            s->pop();
            CHECK(s->size() == 0);
        }
        
        auto s = make_ptr<nops>(temp_directory_path(), recursive_directory_iterator::default_options(), ec);
        s->m_ops.push_back(PS_TEXT("."), fs::file_type::directory);
        s->m_ops.push_back(PS_TEXT(".."), fs::file_type::directory);
        s->m_ops.push_back(PS_TEXT("testf"), fs::file_type::regular);
        s->m_ops.push_back(PS_TEXT("testd"), fs::file_type::directory);
        
        CHECK(s->depth() == 0);
        
        auto ents = s->m_ops.m_ents;
        REQUIRE(ents.size() > 3);
        auto p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.filename().native() == ents[2].d_name);
        CHECK_FALSE(is_set(s->options() & fs::directory_options::reserved_state_will_recurse));
        CHECK(s->size() == 1);
        CHECK(s->depth() == 0);
        p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.filename().native() == ents[3].d_name);
        CHECK(is_set(s->options() & fs::directory_options::reserved_state_will_recurse));
        CHECK(s->size() == 2);
        CHECK(s->depth() == 0);
        p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.empty());
        CHECK(s->size() == 0);
    }
    
    WHEN("a sub-directory fails to open") {
        error_code ec;
        auto s = make_ptr<tops>(temp_directory_path(), recursive_directory_iterator::default_options(), ec);
        s->m_ops.push_back(PS_TEXT("."), fs::file_type::directory);
        s->m_ops.push_back(PS_TEXT(".."), fs::file_type::directory);
        s->m_ops.push_back(PS_TEXT("testd"), fs::file_type::directory);
        
        auto ents = s->m_ops.m_ents;
        REQUIRE(ents.size() > 2);
        auto p = s->next(ec);
        CHECK_FALSE(0 == ec.value());
        CHECK(s->size() == 2);
        CHECK(s->depth() == 0);
        CHECK_FALSE(p.empty());
        p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.empty());
        CHECK(s->size() == 0);
    }
    
    WHEN("postorder option is set") {
        error_code ec;
        auto s = make_ptr<nops>(temp_directory_path(), recursive_directory_iterator::default_options()|directory_options::include_postorder_directories, ec);
        s->m_ops.push_back(PS_TEXT("."), fs::file_type::directory);
        s->m_ops.push_back(PS_TEXT(".."), fs::file_type::directory);
        s->m_ops.push_back(PS_TEXT("testd"), fs::file_type::directory);
        
        auto ents = s->m_ops.m_ents;
        REQUIRE(ents.size() > 2);
        auto p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.filename().native() == ents[2].d_name);
        CHECK(is_set(s->options() & fs::directory_options::reserved_state_will_recurse));
        CHECK(s->size() == 2);
        CHECK(s->depth() == 0);
        CHECK_FALSE(is_set(s->options() & directory_options::reserved_state_postorder));
        p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.filename().native() == ents[2].d_name);
        CHECK_FALSE(is_set(s->options() & fs::directory_options::reserved_state_will_recurse));
        CHECK(s->size() == 1);
        CHECK(s->depth() == 0);
        CHECK(is_set(s->options() & directory_options::reserved_state_postorder));
        p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.empty());
        CHECK(s->size() == 0);
    }
    
#if !_WIN32
    WHEN("invalid UTF8 is encountered") {
        error_code ec;
        auto s = make_ptr<tops>(temp_directory_path(), directory_iterator::default_options(), ec);
        s->m_ops.push_back(PS_TEXT("\x0C5") /* ISO 8859-1 capital Angstrom */, fs::file_type::directory);
        auto p = s->next(ec);
        CHECK(static_cast<int>(iterator_error::encoding_is_not_utf8) == ec.value());
        CHECK(p.empty());
        p = s->next(ec);
        CHECK(0 == ec.value());
        CHECK(p.empty());
    }
#endif
}
