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

#ifndef PS_CORE_FILESYSTEM_ITERATOR_HPP
#define PS_CORE_FILESYSTEM_ITERATOR_HPP

#include <atomic>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>

#include "filesystem_primatives.hpp"

namespace prosoft {
namespace filesystem {
inline namespace v1 {

class file_status;

namespace ifilesystem {
class iterator_state;
}

// define and not constexpr as not all libraries implement time_since_epoch as constexpr
#define PS_FS_ENTRY_INVALID_TIME_VALUE times::make_invalid().time_since_epoch().count()

class directory_entry {
    // C++ states that a name within a scope must have a unique meaning.
    // Therefore path() conflicts with the use of the 'path' type from our namespace and is ambigous.
    // GCC warns about this, clang does not.
    using path_type = prosoft::filesystem::path;
    path_type m_path;
    
public:
    static constexpr file_size_type unknown_size = std::numeric_limits<file_size_type>::max();
    
    directory_entry() noexcept(std::is_nothrow_constructible<path_type>::value)
        : m_path()
        , m_type(file_type::none)
        , m_size(unknown_size)
        , m_last_write(PS_FS_ENTRY_INVALID_TIME_VALUE)
    {
    }
    explicit directory_entry(const path_type& p)
        : m_path(p)
        , m_type(file_type::none)
        , m_size(unknown_size)
        , m_last_write(PS_FS_ENTRY_INVALID_TIME_VALUE)
    {
        
    }
    ~directory_entry() = default;
    directory_entry(const directory_entry& other)
        : m_path(other.m_path)
        , m_type(other.m_type.load())
        , m_size(other.m_size.load())
        , m_last_write(other.m_last_write.load()) {
    }
    directory_entry(directory_entry&& other) noexcept(std::is_nothrow_move_constructible<path_type>::value)
        : m_path(std::move(other.m_path))
        , m_type(other.m_type.load())
        , m_size(other.m_size.load())
        , m_last_write(other.m_last_write.load()) {
    }
    
    directory_entry& operator=(const directory_entry& other) {
        m_path = other.m_path;
        m_type = other.m_type.load();
        m_size = other.m_size.load();
        m_last_write = other.m_last_write.load();
        return *this;
    }
    
    directory_entry& operator=(directory_entry&& other) noexcept(std::is_nothrow_move_assignable<path_type>::value) {
        m_path = std::move(other.m_path);
        m_type = other.m_type.load();
        m_size = other.m_size.load();
        m_last_write = other.m_last_write.load();
        return *this;
    }
    
    // Extensions //
    explicit directory_entry(path_type&& p) noexcept(std::is_nothrow_move_constructible<path_type>::value)
        : m_path(std::move(p))
        , m_type(file_type::none)
        , m_size(unknown_size)
        , m_last_write(PS_FS_ENTRY_INVALID_TIME_VALUE) {
    }
    
    void assign(path_type&& p) {
        m_path = std::move(p);
    }
    
    bool empty() const noexcept(noexcept(std::declval<path_type>().empty())) {
        return m_path.empty();
    }
    // Extensions //

    void assign(const path_type& p) {
        m_path = p;
    }

    void replace_filename(const path_type& p) {
        m_path.replace_filename(p);
    }

    operator const path_type&() const noexcept {
        return this->path();
    }

    const path_type& path() const & noexcept {
        return m_path;
    }
    
    PS_WARN_UNUSED_RESULT path_type path() && noexcept(std::is_nothrow_move_constructible<path_type>::value) {
        return path_type{std::move(m_path)};
    }

    file_status status() const;
    file_status status(error_code&) const noexcept;

    file_status symlink_status() const;
    file_status symlink_status(error_code&) const noexcept;
    
    // Cache: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0317r1.html
    void refresh();
    void refresh(error_code&);
    
    bool exists() const;
    bool exists(error_code& ec) const noexcept;
    bool is_block_file() const;
    bool is_block_file(error_code& ec) const noexcept;
    bool is_character_file() const;
    bool is_character_file(error_code& ec) const noexcept;
    bool is_directory() const;
    bool is_directory(error_code& ec) const noexcept;
    bool is_fifo() const;
    bool is_fifo(error_code& ec) const noexcept;
    bool is_other() const;
    bool is_other(error_code& ec) const noexcept;
    bool is_regular_file() const;
    bool is_regular_file(error_code& ec) const noexcept;
    bool is_socket() const;
    bool is_socket(error_code& ec) const noexcept;
    bool is_symlink() const;
    bool is_symlink(error_code& ec) const noexcept;
    file_size_type file_size() const;
    file_size_type file_size(error_code& ec) const noexcept;
    file_time_type last_write_time() const;
    file_time_type last_write_time(error_code& ec) const noexcept;
    
    // for testing
    file_type cached_type() const {
        return m_type.load();
    }
    file_size_type cached_size() const {
        return m_size.load();
    }
    file_time_type::duration::rep cached_write_time() const {
        return m_last_write.load();
    }
    
private:
    // Cache
    friend ifilesystem::iterator_state;
    std::atomic<file_type> mutable m_type;
    std::atomic<file_size_type> mutable m_size;
    std::atomic<file_time_type::duration::rep> mutable m_last_write;

    template <typename T>
    T load(std::atomic<T>& aval, T badVal) const {
        auto val = aval.load();
        if (badVal != val) {
            return val;
        } else {
            const_cast<directory_entry*>(this)->refresh();
            return aval.load();
        }
    }
    
    template <typename T>
    T load(std::atomic<T>& aval, T badVal, error_code& ec) const {
        auto val = aval.load();
        if (badVal != val) {
            return val;
        } else {
            const_cast<directory_entry*>(this)->refresh(ec);
            return aval.load();
        }
    }
    
    file_type get_type() const {
        return load(m_type, file_type::none);
    }
    file_type get_type(error_code& ec) const {
        return load(m_type, file_type::none, ec);
    }
    
    file_size_type get_size() const {
        return load(m_size, unknown_size);
    }
    file_size_type get_size(error_code& ec) const {
        return load(m_size, unknown_size, ec);
    }
    
    file_time_type get_last_write() const {
        return file_time_type{file_time_type::duration{load(m_last_write, PS_FS_ENTRY_INVALID_TIME_VALUE)}};
    }
    
    file_time_type get_last_write(error_code& ec) const {
        return file_time_type{file_time_type::duration{load(m_last_write, PS_FS_ENTRY_INVALID_TIME_VALUE, ec)}};
    }
};

enum class directory_options : unsigned {
    none,
    follow_directory_symlink,
    skip_permission_denied,
    
    // Extensions
    include_created_events = 1U<<16, // change_iterator
    include_modified_events = 1U<<17, // change_iterator
    
    skip_subdirectory_descendants = 1U<<20,
    skip_hidden_descendants = 1U<<21,
    skip_package_content_descendants = 1U<<22, // macOS
    follow_mountpoints = 1U<<23,
    include_postorder_directories = 1U<<24,
    // By default, we mimic the behavior of NSDirectoryEnumerator and skip "._" files.
    // However, unlike NSDE, we only skip "._" files that have a sibling of the same name. Orphan "._" files are always returned.
    // If for some reason you want paired "._" files too, set this. Normally it should not be set as the system automatically handles pairs.
    include_apple_double_files = 1U<<25, // macOS

    // Internal state
    reserved_state_will_recurse = 1U<<29,
    reserved_state_skip_descendants = 1U<<30,
    reserved_state_postorder = 1U<<31,
    reserved_state_mask = 0xf0000000,
};
PS_ENUM_BITMASK_OPS(directory_options);

constexpr directory_options make_public(directory_options opts) {
    return opts & ~directory_options::reserved_state_mask;
}

using iterator_depth_type = int;

namespace ifilesystem {
struct cache_info {
    file_type ftype;
#if _WIN32
    file_size_type fsize;
    file_time_type fwrite_time;
#endif
    cache_info()
        : ftype(file_type::none)
#if _WIN32
        , fsize(directory_entry::unknown_size)
        , fwrite_time(times::make_invalid())
#endif
    {
    }
};

class iterator_state {
    directory_entry m_current;
    directory_options m_opts;
    
protected:
    virtual path next(cache_info&, error_code&) = 0;
    
    void set(directory_options opts) noexcept {
        m_opts |= opts & directory_options::reserved_state_mask;
    }
    
    void clear(directory_options opts) noexcept {
        m_opts &= ~(opts & directory_options::reserved_state_mask);
    }
    
    bool is_current_empty() const {
        return m_current.path().empty();
    }
    
public:
    iterator_state() = default;
    iterator_state(const path&, directory_options opts, error_code& ec)
        : m_current()
        , m_opts(opts) {
        ec.clear();
    }
    virtual ~iterator_state() = default;
    
    const directory_entry& current() const noexcept {
        return m_current;
    }
    
    directory_options options() const noexcept {
        return m_opts;
    }
    
    void increment(error_code& ec) {
        cache_info cinfo;
        m_current = directory_entry{next(cinfo, ec)};
        if (cinfo.ftype != file_type::unknown) {
            m_current.m_type = cinfo.ftype;
        }
#if _WIN32
        if (cinfo.fsize != directory_entry::unknown_size) {
            m_current.m_size = cinfo.fsize;
        }
        if (cinfo.fwrite_time != times::make_invalid()) {
            m_current.m_last_write = cinfo.fwrite_time.time_since_epoch().count();
        }
#endif
    }
    
    PS_WARN_UNUSED_RESULT directory_entry extract() noexcept(std::is_nothrow_move_constructible<directory_entry>::value) {
        return directory_entry{std::move(m_current)};
    }
    
    virtual void pop() {}
    
    virtual iterator_depth_type depth() const noexcept {
        return 0;
    }
    
    virtual void skip_descendants() {}
    
    virtual bool at_end() const {
        return is_current_empty();
    }
};

using iterator_state_ptr = std::shared_ptr<ifilesystem::iterator_state>;

struct iterator_config {};

struct iterator_traits {
    static constexpr directory_options required = directory_options::skip_subdirectory_descendants;
    static constexpr directory_options not_supported = directory_options::include_postorder_directories;
    static constexpr directory_options defaults = required;
    using configuration_type = iterator_config;
};

struct recursive_iterator_traits {
    static constexpr directory_options required = directory_options::none;
    static constexpr directory_options not_supported = directory_options::skip_subdirectory_descendants;
    static constexpr directory_options defaults = required;
    using configuration_type = iterator_config;
};

template <class Traits>
struct is_recursive {
    static constexpr bool value = !is_set(Traits::required & directory_options::skip_subdirectory_descendants);
};

static_assert(!is_recursive<iterator_traits>::value, "WTF?");
static_assert(is_recursive<recursive_iterator_traits>::value, "WTF?");

iterator_state_ptr make_iterator_state(const path&, directory_options, iterator_config, error_code&);

const error_code& permission_denied_error();

template <class Traits>
inline const error_code& permission_denied_error(Traits) {
    return permission_denied_error();
}

template <class Traits>
constexpr directory_options make_options(directory_options opts) {
    return (make_public(opts) | Traits::required) & ~Traits::not_supported;
}
} // ifilesystem

template <class Traits>
class basic_iterator {
    ifilesystem::iterator_state_ptr m_i;
    
    const directory_entry& empty_entry() const {
        static const directory_entry empty;
        return empty;
    }
    
    void clear_if_denied(error_code& ec) const;
    
    template <typename Recurse>
    struct resurse_only : public std::enable_if<ifilesystem::is_recursive<Traits>::value, Recurse> {};
    
    template <typename Recurse>
    using recurse_only_t = typename resurse_only<Recurse>::type;
    
    friend Traits;
public:
    using traits_type = Traits;
    using configuration_type = typename traits_type::configuration_type;
    using value_type = directory_entry;
    using difference_type = ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;
    using iterator_category = std::input_iterator_tag;
    
    basic_iterator() noexcept(std::is_nothrow_constructible<decltype(m_i)>::value) = default;
    explicit basic_iterator(const path&);
    basic_iterator(const path&, error_code&);
    basic_iterator(const path&, directory_options);
    basic_iterator(const path&, directory_options, error_code&);
    basic_iterator(const path&, directory_options, configuration_type&&);
    basic_iterator(const path&, directory_options, configuration_type&&, error_code&);
    basic_iterator(const basic_iterator&) = default;
    basic_iterator(basic_iterator&&) noexcept(std::is_nothrow_move_constructible<decltype(m_i)>::value) = default;
    ~basic_iterator() = default;
    
    directory_options options() const;
    const directory_entry& operator*() const;
    const directory_entry* operator->() const;
    
    basic_iterator& operator=(const basic_iterator&) = default;
    basic_iterator& operator=(basic_iterator&&) noexcept(std::is_nothrow_move_constructible<decltype(m_i)>::value) = default;
    basic_iterator& operator++();
    basic_iterator& increment(error_code&);
    
    bool operator==(const basic_iterator&) const;
    
    // Recursive only
    template <typename Recurse = void>
    iterator_depth_type depth(recurse_only_t<Recurse>* = 0) const noexcept {
        return m_i ? m_i->depth() : iterator_depth_type{};
    }
    
    template <typename Recurse = void>
    bool recursion_pending(recurse_only_t<Recurse>* = 0) const noexcept {
        return m_i ? is_set(m_i->options() & directory_options::reserved_state_will_recurse) : false;
    }
    
    template <typename Recurse = void>
    void pop(recurse_only_t<Recurse>* = 0) {
        if (m_i) {
            m_i->pop();
            if (m_i->at_end()) {
                m_i.reset(); // become end
            }
        }
    }
    
    template <typename Recurse = void>
    void disable_recursion_pending(recurse_only_t<Recurse>* = 0) {
        if (m_i) {
            m_i->skip_descendants();
        }
    }
    
    // Extensions
    template <typename Recurse = void>
    bool is_postorder(recurse_only_t<Recurse>* = 0) {
        return m_i && is_set(m_i->options() & directory_options::reserved_state_postorder);
    }
    
    basic_iterator& operator++(int);
    
    // moves current entry out -- be careful
    PS_WARN_UNUSED_RESULT directory_entry extract() noexcept(std::is_nothrow_move_constructible<directory_entry>::value);
    
    static constexpr directory_options default_options() {
        return Traits::defaults;
    }
    
private:
    // Semi-private option to skip increment-on-init behavior.
    template <typename, typename = void>
    struct skip_init_increment : std::false_type {};
    
    // In C++11 unused template alias params were ignored for SFINAE.
    // For C++14 this could be used instead: template <typename... Args> using void_t = void;
    // GCC < 5 requires the more verbose definition.
    template<typename... Args> struct make_void {
        typedef void type;
    };
    template<typename... Args> using void_t = typename make_void<Args...>::type;
    
    template <class T>
    struct skip_init_increment<T, void_t<typename std::is_integral<decltype(T::skip_init_increment)>::type>> : std::true_type {};
    
    template <class T, class Void>
    using skip_init_increment_t = typename std::enable_if<skip_init_increment<T>::value, Void>::type;
    
    template <class T, class Void>
    using do_init_increment_t = typename std::enable_if<!skip_init_increment<T>::value, Void>::type;
    
    template <typename Void = void>
    void init_increment(error_code&, skip_init_increment_t<traits_type, Void>* = 0) const {
    }
    
    template <typename Void = void>
    constexpr void init_increment(skip_init_increment_t<traits_type, Void>* = 0) const {
    }
    
    template <typename Void = void>
    void init_increment(error_code& ec, do_init_increment_t<traits_type, Void>* = 0) {
        increment(ec);
    }
    
    template <typename Void = void>
    void init_increment(do_init_increment_t<traits_type, Void>* = 0) {
        operator++();
    }
    
    static_assert(!skip_init_increment<ifilesystem::iterator_traits>::value, "WTF?");
};

template <class Traits>
inline basic_iterator<Traits>::basic_iterator(const path& p)
    : basic_iterator<Traits>::basic_iterator(p, default_options()) {
}

template <class Traits>
inline basic_iterator<Traits>::basic_iterator(const path& p, error_code& ec)
    : basic_iterator<Traits>::basic_iterator(p, default_options(), ec) {
}

template <class Traits>
basic_iterator<Traits>::basic_iterator(const path& p, directory_options opts)
    : basic_iterator<Traits>::basic_iterator(p, opts, configuration_type{}) {
}

template <class Traits>
inline basic_iterator<Traits>::basic_iterator(const path& p, directory_options opts, error_code& ec)
    : basic_iterator<Traits>::basic_iterator(p, opts, configuration_type{}, ec) {
}

template <class Traits>
const directory_entry& basic_iterator<Traits>::operator*() const {
    return m_i ? m_i->current() : empty_entry();
}

template <class Traits>
inline const directory_entry* basic_iterator<Traits>::operator->() const {
    return &operator*();
}

template <class Traits>
directory_options basic_iterator<Traits>::options() const {
    return m_i ? make_public(m_i->options()) : directory_options::none;
}

template <class Traits>
basic_iterator<Traits>& basic_iterator<Traits>::increment(error_code& ec) {
    if (m_i) {
        m_i->increment(ec);
        if (m_i->at_end()) {
            m_i.reset(); // become end
        }
    }
    return *this;
}

template <class Traits>
bool basic_iterator<Traits>::operator==(const basic_iterator<Traits>& other) const {
    auto ti = m_i.get();
    auto oi = other.m_i.get();
    return ti == oi;
}

template <class Traits>
inline bool operator!=(const basic_iterator<Traits>& lhs, const basic_iterator<Traits>& rhs) {
    return !lhs.operator==(rhs);
}

template <class Traits>
inline basic_iterator<Traits> begin(basic_iterator<Traits> i) {
    return i;
}

template <class Traits>
inline basic_iterator<Traits> end(const basic_iterator<Traits>&) {
    return basic_iterator<Traits>{};
}

template <class Traits>
void basic_iterator<Traits>::clear_if_denied(error_code& ec) const {
    if (is_set(options() & directory_options::skip_permission_denied) && ec == ifilesystem::permission_denied_error(Traits{})) {
        ec.clear();
    }
}

// Extensions
template <class Traits>
inline basic_iterator<Traits>& basic_iterator<Traits>::operator++(int) {
    return operator++(); // // XXX: Input iter one-pass semantics.
}

template <class Traits>
directory_entry basic_iterator<Traits>::extract() noexcept(std::is_nothrow_move_constructible<directory_entry>::value) {
    return m_i ? m_i->extract() : directory_entry{};
}

} // v1
} // filesystem
} // prosoft

#endif // PS_CORE_FILESYSTEM_ITERATOR_HPP
