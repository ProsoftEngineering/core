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

#if !_WIN32
#include <dirent.h>
#include <sys/errno.h>
#else
#include <windows.h>
#endif

#include <cstring>
#include <vector>

#include <system_error.hpp>

#include "fsconfig.h"
#include "filesystem.hpp"
#include "filesystem_private.hpp"

namespace fs = prosoft::filesystem::v1;

namespace {

enum class iterator_error {
    none,
    encoding_is_not_utf8,
};

class iterator_error_category : public std::error_category {
public:
	iterator_error_category() noexcept : std::error_category() {}

	virtual const char* name() const noexcept override {
		return "Filesystem iterator";
    }

	virtual std::string message(int ec) const override {
        switch (static_cast<iterator_error>(ec)) {
            case iterator_error::encoding_is_not_utf8:
                return "A non-UTF8 path name was encountered. It has been skipped.";
            break;
            
            default:
                PSASSERT_UNREACHABLE("BUG");
#if !_WIN32
                return std::strerror(EINVAL);
#else
                return "Unknown error";
#endif
            break;
        }
	}
};

const std::error_category& iterator_category() {
    static iterator_error_category cat;
    return cat;
}

#if !_WIN32
using native_dir = ::DIR;
using native_dirent = ::dirent;

inline bool is_directory(const native_dirent* e) {
    return e->d_type == DT_DIR;
}

inline bool is_symlink(const native_dirent* e) {
    return e->d_type == DT_LNK;
}

void cache_info(fs::ifilesystem::cache_info& ci, const native_dirent* e) {
    switch(e->d_type) {
        case DT_REG:
            ci.ftype = fs::file_type::regular;
        break;
        case DT_DIR:
            ci.ftype = fs::file_type::directory;
        break;
        case DT_LNK:
            ci.ftype = fs::file_type::symlink;
        break;
        case DT_BLK:
            ci.ftype = fs::file_type::block;
        break;
        case DT_CHR:
            ci.ftype = fs::file_type::character;
        break;
        case DT_FIFO:
            ci.ftype = fs::file_type::fifo;
        break;
        case DT_SOCK:
            ci.ftype = fs::file_type::socket;
        break;
        default:
            ci.ftype = fs::file_type::unknown;
        break;
    }
}
#else
using native_dirent = ::WIN32_FIND_DATAW;
struct native_dir {
    native_dirent ent;
    HANDLE handle;
    bool firstent;
    native_dir()
        : handle()
        , firstent(false) {
        std::memset(&ent, 0, sizeof(ent));
    }
};
#define d_name cFileName
inline bool is_directory(const native_dirent* e) {
    return 0 != (e->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

inline bool is_symlink(const native_dirent* e) {
    return 0 != (e->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

void cache_info(fs::ifilesystem::cache_info& ci, const native_dirent* e) {
    if (is_directory(e)) {
        ci.ftype = fs::file_type::directory;
    } else if (is_symlink(e)) {
        ci.ftype = fs::file_type::symlink;
    } else if ((e->dwFileAttributes & FILE_ATTRIBUTE_DEVICE)) {
        ci.ftype = fs::file_type::character;
    } else {
        ci.ftype = fs::file_type::regular;
    }
    
    ci.fwrite_time = fs::ifilesystem::to_filetime(e->ftLastWriteTime);
    
    if (fs::file_type::regular == ci.ftype) {
        ci.fsize = fs::file_size_type(e->nFileSizeLow) | (fs::file_size_type(e->nFileSizeHigh) >> 32);
    } else {
        ci.fsize = 0;
    }
}
#endif //!_WIN32

native_dir* open_dir(const fs::path& p) {
#if !_WIN32
    return ::opendir(p.c_str());
#else
    using namespace prosoft;
    auto np = to_string<native_string_type, fs::path::string_type>{}(p);
    if (np.empty() || (*(--np.end()) == fs::path::preferred_separator)) {
        np.append(PS_TEXT("*"));
    } else {
        np.append(PS_TEXT("\\*"));
    }
    
    auto d = new native_dir;
    d->handle = ::FindFirstFileW(np.c_str(), &d->ent);
    if (INVALID_HANDLE_VALUE != d->handle) {
        d->firstent = true;
    } else {
        delete d;
        d = nullptr;
    }
    return d;
#endif //!_WIN32
}

int close_dir(native_dir* d) {
    if (d) {
#if !_WIN32
        return ::closedir(d);
#else
        ::FindClose(d->handle);
        delete d;
        return 0;
#endif
    } else {
#if !_WIN32
        errno = einval().value();
#else
        ::SetLastError(static_cast<DWORD>(einval().value()));
#endif
        return -1;
    }
}

native_dirent* read_dir(native_dir* d) {
    if (d) {
#if !_WIN32
        errno = 0; // POSIX requires this to determine EOF
        return ::readdir(d);
#else
        if (d->firstent) {
            d->firstent = false;
            return &d->ent;
        }
        if (::FindNextFileW(d->handle, &d->ent)) {
            return &d->ent;
        } else {
            return nullptr;
        }
#endif
    } else {
#if !_WIN32
        errno = einval().value();
#else
        ::SetLastError(static_cast<DWORD>(einval().value()));
#endif
        return nullptr;
    }
}

#if __APPLE__
bool is_apple_double(const fs::path& dir, const fs::path& leaf) {
    static const fs::path::string_type dot_underscore_prefix{"._"};
    
    using namespace prosoft;
    if (starts_with(leaf.native(), dot_underscore_prefix) && leaf.native().size() > dot_underscore_prefix.size()) {
        auto adp = dir / leaf;
        fs::error_code ec;
        if (!fs::is_directory(adp, ec)) {
            adp = dir / leaf.native().substr(2);
            return exists(adp, ec);
        }
    }
    return false;
}
#else
inline PS_CONSTEXPR_IF_CPP14 bool is_apple_double(const fs::path&, const fs::path&) {return false;}
#endif

native_dir* const INVALID_DIR = (native_dir*)((uintptr_t)0xbaadf00dUL);

struct dir_ops {
    PS_ALWAYS_INLINE static native_dir* open(const fs::path& p) {
        return open_dir(p);
    }

    PS_ALWAYS_INLINE static native_dirent* read(native_dir* d) {
        return read_dir(d);
    }
    
    PS_ALWAYS_INLINE static int close(native_dir* d) {
        return close_dir(d);
    }
};

template <class Ops>
struct stack_entry {
    native_dir* m_dir;
    fs::path m_path;
    
    stack_entry(native_dir* d, fs::path&& p) noexcept(std::is_nothrow_move_constructible<fs::path>::value)
        : m_dir(d)
        , m_path(std::move(p)) {}
    stack_entry(native_dir* d, const fs::path& p)
        : stack_entry(d, fs::path{p}) {}
    ~stack_entry() {
        if (INVALID_DIR != m_dir) {
            Ops{}.close(m_dir);
        }
    }
    stack_entry(stack_entry&& other) noexcept(std::is_nothrow_move_constructible<fs::path>::value)
        : m_dir(other.m_dir)
        , m_path(std::move(other.m_path)) {
        other.m_dir = INVALID_DIR;
    }
    
    PS_DISABLE_COPY(stack_entry);
};

using fsiterator_state = fs::ifilesystem::iterator_state;
using fsiterator_cache = fs::ifilesystem::cache_info;

template <class Ops> // Template used for testing
class state : public fsiterator_state {
    using base = fsiterator_state;
// XXX: each level of recursion adds another open file descriptor.
// If this becomes an issue (thousands of subdirs), a solution would be to:
// save subdirs as they are found, process all files, close parent and then recurse saved subdirs.
    using entry = stack_entry<Ops>;
    std::vector<entry> m_stack;
    
#if PSTEST_HARNESS
public:
#endif
    Ops m_ops;

    bool recurse() const noexcept {
        return !is_set(options() & fs::directory_options::skip_subdirectory_descendants);
    }
    
    size_t size() const {
        return m_stack.size();
    }
    
    bool is_valid() const {
        PSASSERT(m_stack.size() > 0, "Broken assumption");
        return INVALID_DIR != m_stack.back().m_dir;
    }
    
    bool is_child() const {
        return size() > 1;
    }
    
    const entry& peek_unsafe() const {
        PSASSERT(m_stack.size() > 0, "BUG");
        return m_stack.back();
    }
    
    const entry* peek_valid(); // there may not be a valid entry, hence the ptr
    
    bool push(fs::path&&, fs::error_code&);
    
    bool push(const fs::path& p, fs::error_code& ec) {
        return push(fs::path{p}, ec);
    }
    
    void push_placeholder(fs::path&& dir) {
        m_stack.emplace_back(INVALID_DIR, std::move(dir));
    }

public:
    using fsiterator_state::fsiterator_state;
    
    state(const fs::path&, fs::directory_options, fs::error_code&);
    
    virtual ~state() {};
    
    virtual fs::path next(fsiterator_cache&, prosoft::system::error_code&) override;
#if PSTEST_HARNESS
    fs::path next(prosoft::system::error_code& ec) {
        fsiterator_cache dummy;
        return next(dummy, ec);
    }
#endif
    
    virtual void pop() override;
    
    virtual fs::iterator_depth_type depth() const noexcept override;
    
    virtual void skip_descendants() override;
    
    virtual bool at_end() const override;
};

template <class Ops>
const typename state<Ops>::entry* state<Ops>::peek_valid() {
    if (size() > 0) {
        const auto& e = m_stack.back();
        if (INVALID_DIR != e.m_dir) {
            return &e;
        } else {
            pop();
            return peek_valid();
        }
    } else {
        return nullptr;
    }
}

inline bool is_permssion_denied(const fs::error_code& ec) {
#if !_WIN32
    return ec.value() == EACCES
#if __APPLE__
    // 'User Data Protection' dirs in 10.14 (WWDC 2018 S-702) and certain iCloud drive tmp folders.
    || ec.value() == EPERM;
#endif
    ;
#else
    return ec.value() == ERROR_ACCESS_DENIED;
#endif
}

inline bool is_no_entries(const fs::error_code& ec) {
#if !_WIN32
    return ec.value() == ENOENT;
#else
    return ec.value() == ERROR_NO_MORE_FILES;
#endif
}

inline bool clear_if(fs::error_code& ec, bool condition) {
    if (condition) {
        ec.clear();
    }
    return condition;
}

template <class Ops>
bool state<Ops>::push(fs::path&& p, fs::error_code& ec) {
    if (auto d = m_ops.open(p)) {
        set(fs::directory_options::reserved_state_will_recurse);
        m_stack.emplace_back(d, std::move(p));
        ec.clear();
        return true;
    } else {
        ec = prosoft::system::system_error();
        base::clear(fs::directory_options::reserved_state_will_recurse);
        // XXX: push a bad entry so clients can still get a listing of a dir that can't be opened and call skip_descendants() w/o unexpected results.
        push_placeholder(std::move(p));
        return clear_if(ec, is_set(options() & fs::directory_options::skip_permission_denied) && is_permssion_denied(ec));
    }
}

bool leaf_is_dot_or_dot_dot(const fs::path::string_type& leaf) {
    const auto sz = leaf.size();
    const bool isdot = sz == 1 && leaf[0] == fs::path::dot;
    return isdot || (sz == 2 && leaf[0] == fs::path::dot && leaf[1] == fs::path::dot);
}

#if !_WIN32 || !PS_CPP17_FILESYSTEM_PATH_USES_NATIVE_ENCODING
inline bool leaf_is_dot_or_dot_dot(const fs::path& leaf) {
    return leaf_is_dot_or_dot_dot(leaf.native());
}
#endif

template <class Ops>
state<Ops>::state(const fs::path& p, fs::directory_options opts, fs::error_code& ec)
    : fsiterator_state(p, opts, ec) {
#if _WIN32
    // Empty path is valid in Win32 (implicit "."), but not POSIX. Use POSIX behavior for Windows.
    if (p.empty()) {
        ec = einval();
        return;
    }
#endif
    if (!ec) {
        push(p, ec);
    }
}

template <class Ops>
fs::path state<Ops>::next(fsiterator_cache& cinfo, prosoft::system::error_code& ec) {
    const bool postorder = is_set(options() & fs::directory_options::include_postorder_directories);

    base::clear(fs::directory_options::reserved_state_mask);
    ec.clear();
    
    if (postorder && is_child() && !is_valid()) {
        // Handle post order event for the current directory that we failed to open.
        set(fs::directory_options::reserved_state_postorder);
        auto p = peek_unsafe().m_path;
        pop();
        cinfo.ftype = fs::file_type::directory;
        return p;
    }
    
    while (auto e = peek_valid()) {
        PSASSERT(!e->m_path.empty(), "WTF?");
        for (;;) {
            if (auto ent = m_ops.read(e->m_dir)) {
#if DT_WHT // BSD whiteout flag used for Union filesystems -- should never be hit in the realworld
                if (DT_WHT == ent->d_type) {
                    continue;
                }
#endif
                size_t namelen;
                #if PS_FS_HAVE_BSD_STATFS
                namelen = ent->d_namlen;
                #elif !_WIN32
                namelen = ::strlen(ent->d_name);
                #else
                namelen = ::wcslen(ent->d_name);
                #endif
                
                fs::path leaf;
                PSSilenceCppException(leaf = fs::path(fs::path::string_type(ent->d_name, namelen)));
                if (leaf.empty()) {
                    // should only happen on non-Apple UNIX when the path is not encoded as UTF8
                    ec = fs::error_code{static_cast<int>(iterator_error::encoding_is_not_utf8), iterator_category()};
                    break;
                }
                
                if (leaf_is_dot_or_dot_dot(leaf)) {
                    continue;
                }
                
                fs::path cpath{e->m_path};
                if (!is_set(options() & fs::directory_options::include_apple_double_files) && is_apple_double(cpath, leaf)) {
                    continue;
                }
                
                cpath /= leaf;
                
                fs::error_code derr;
                if (is_set(options() & fs::directory_options::skip_hidden_descendants) && is_hidden(cpath, derr)) {
                    continue;
                }
                
                if (recurse()
                    && (is_directory(ent)
                        || (is_set(options() & fs::directory_options::follow_directory_symlink) && is_symlink(ent) && is_directory(cpath, derr)))
                    ) {
                    if ((!is_set(options() & fs::directory_options::follow_mountpoints) && is_mountpoint(cpath, derr))
                        || (is_set(options() & fs::directory_options::skip_package_content_descendants) && is_package(cpath, derr))
                    ) {
                        // push a placeholder so clients can call skipDescendants() w/o unexpected results.
                        push_placeholder(fs::path{cpath});
                    } else {
                        static auto copy_link_path = [](const fs::path& p, native_dirent* e) -> fs::path {
                            if (is_symlink(e)) {
                                fs::error_code ec;
                                auto np = fs::canonical(p, ec);
                                if (!np.empty()) {
                                    return np;
                                }
                            }
                            return p;
                        };
                        
                        if (!push(copy_link_path(cpath, ent), ec)) {
                            PSASSERT(peek_unsafe().m_path == copy_link_path(cpath, ent), "Broken assumption"); // assuming placeholder is pushed
                            pop();
                            break;
                        }
                    }
                }
                
                cache_info(cinfo, ent);
                return cpath;
            } else {
                // we've read all entries in the current dir
                prosoft::system::system_error(ec);
                clear_if(ec, is_no_entries(ec));
                if (!ec && postorder && is_child()) { // don't include root
                    set(fs::directory_options::reserved_state_postorder);
                    auto p = e->m_path;
                    #if DEBUG
                    fs::error_code derr;
                    #endif
                    PSASSERT(fs::is_directory(p, derr) || !exists(p, derr), "BUG"); // could be a possible race where the dir has been removed
                    pop();
                    return p;
                } else {
                    pop();
                    break;
                }
            }
        } // for
        
        if (ec.value()) {
            break;
        }
    }
    
    return fs::path{};
}

template <class Ops>
void state<Ops>::pop() {
    if (size() > 0) {
        m_stack.pop_back();
    } else {
        PSASSERT_UNREACHABLE("BUG");
    }
}

template <class Ops>
fs::iterator_depth_type state<Ops>::depth() const noexcept {
    auto sz = static_cast<fs::iterator_depth_type>(size());
    if (sz > 0) {
        sz -= 1;
        const bool adjust = is_set(options() & fs::directory_options::reserved_state_will_recurse) || !is_valid();
        if (adjust && sz > 0) {
            sz -= 1;
        }
        return sz;
    } else {
        PSASSERT_UNREACHABLE("CLIENT BUG");
        return 0;
    }
}

template <class Ops>
void state<Ops>::skip_descendants() {
    PSASSERT(is_child(), "BUG"); // root dir should not happen
    PSASSERT(is_set(fs::directory_options::reserved_state_will_recurse) || !is_valid(), "BUG");
    pop();
    base::clear(fs::directory_options::reserved_state_will_recurse);
}

template <class Ops>
bool state<Ops>::at_end() const {
    return size() == 0;
}

} // anon

namespace prosoft {
namespace filesystem {
inline namespace v1 {

ifilesystem::iterator_state_ptr
ifilesystem::make_iterator_state(const path& p, directory_options opts, iterator_traits::configuration_type, error_code& ec) {
    auto s = std::make_shared<state<dir_ops>>(p, opts, ec);
    if (ec) {
        s.reset(); // null is the end iterator
    }
    return s;
}

const error_code& ifilesystem::permission_denied_error() {
#if !_WIN32
    constexpr int ec = EACCES;
#else
    constexpr int ec = ERROR_ACCESS_DENIED;
#endif
    static const error_code denied{ec, system::error_category()};
    return denied;
}

constexpr file_size_type directory_entry::unknown_size;

void directory_entry::refresh() {
    error_code ec;
    refresh(ec);
    PS_THROW_IF(ec.value() != 0, filesystem_error("Failed to refresh dir ent.", m_path, ec));
}

void directory_entry::refresh(error_code& ec) {
    constexpr status_info info = status_info::basic | status_info::times | status_info::size;
    const auto st = filesystem::symlink_status(m_path, info, ec);
    if (!ec) {
        m_type = st.type();
        m_size = st.size();
        m_last_write = st.times().modified().time_since_epoch().count();
    } else {
        clear_cache();
    }
}

} // v1
} // filesystem
} // prosoft

#if PSTEST_HARNESS
// Internal tests.
#include "catch.hpp"

using namespace prosoft::filesystem;

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
        return (native_dir*)0x55UL;
    }
    
    virtual int close(native_dir* d) override {
        return 0;
    }
};

template <class T, class... Args>
std::unique_ptr<T> make_ptr(Args&& ...a) {
    return std::unique_ptr<T>{ new T(std::forward<Args>(a)...) };
}

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
        CHECK(s->size() == 1);
        CHECK(s->depth() == 0);
        CHECK(p.empty());
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

#endif // PSTEST_HARNESS
