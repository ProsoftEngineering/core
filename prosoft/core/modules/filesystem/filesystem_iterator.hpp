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

#ifndef PS_CORE_FILESYSTEM_ITERATOR_HPP
#define PS_CORE_FILESYSTEM_ITERATOR_HPP

#include <iterator>
#include <memory>
#include <type_traits>

namespace prosoft {
namespace filesystem {
inline namespace v1 {

class file_status;

class directory_entry {
    // C++ states that a name within a scope must have a unique meaning.
    // Therefore path() conflicts with the use of the 'path' type from our namespace and is ambigous.
    // GCC warns about this, clang does not.
    using path_type = prosoft::filesystem::path;
    path_type m_path;
    const path_type& get_path() const noexcept {
        return m_path;
    }

public:
    directory_entry() noexcept(std::is_nothrow_constructible<path_type>::value) : m_path() {}
    ~directory_entry() = default;
    PS_DEFAULT_COPY(directory_entry);
    PS_DEFAULT_MOVE(directory_entry);
    explicit directory_entry(const path_type& p)
        : m_path(p) {}
    // Extensions //
    explicit directory_entry(path_type&& p) noexcept(std::is_nothrow_move_constructible<path_type>::value)
        : m_path(std::move(p)) {}
    
    void assign(path_type&& p) {
        m_path = std::move(p);
    }
    // Extensions //

    void assign(const path_type& p) {
        m_path = p;
    }

    void replace_filename(const path_type& p) {
        m_path.replace_filename(p);
    }

    operator const path_type&() const noexcept {
        return get_path();
    }

    const path_type& path() const noexcept {
        return get_path();
    }

    file_status status() const;
    file_status status(error_code&) const noexcept;

    file_status symlink_status() const;
    file_status symlink_status(error_code&) const noexcept;
};

enum class directory_options : unsigned {
    none,
    follow_directory_symlink,
    skip_permission_denied,
    
    // Extensions
    skip_subdirectory_descendants = 1U<<20,
    skip_hidden_descendants = 1U<<21,
    skip_package_descendants = 1U<<22, // macOS
    follow_mountpoints = 1U<<23,
    include_postorder_directories = 1U<<24,
    // By default, we mimic the behavior of NSDirectoryEnumerator and skip "._" files.
    // However, unlike NSDE, we only skip "._" files that have a sibling of the same name. Orphan "._" files are always returned.
    // If for some reason you want paired "._" files too, set this. Normally it should not be set as the system automatically handles pairs.
    include_apple_double_files = 1U<<25, // macOS

    // Internal state
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
class iterator_state {
    path m_root;
    directory_entry m_current;
    directory_options m_opts;
    
protected:
    virtual path next(error_code&) = 0;
    
    void set(directory_options opts) noexcept {
        m_opts |= opts & directory_options::reserved_state_mask;
    }
    
    void clear(directory_options opts) noexcept {
        m_opts &= ~(opts & directory_options::reserved_state_mask);
    }
    
public:
    iterator_state() = default;
    iterator_state(const path& p, directory_options opts, error_code&)
        : m_root(p)
        , m_current()
        , m_opts(opts) {}
    virtual ~iterator_state() = default;
    
    const path& root() const noexcept {
        return m_root;
    }
    
    const directory_entry& current() const noexcept {
        return m_current;
    }
    
    directory_options options() const noexcept {
        return m_opts;
    }
    
    void increment(error_code& ec) {
        m_current = directory_entry{next(ec)};
    }
    
    virtual bool equal_to(const iterator_state*) const;
    
    virtual void pop() {}
    
    virtual iterator_depth_type depth() const noexcept {
        return 0;
    }
    
    virtual void skip_descendants() {}
    
    virtual bool at_end() const {
        return m_current.path().empty();
    }
};

using iterator_state_ptr = std::shared_ptr<ifilesystem::iterator_state>;

struct iterator_traits {
    static constexpr directory_options required = directory_options::skip_subdirectory_descendants;
    static constexpr directory_options not_supported = directory_options::none;
    static constexpr directory_options defaults = required;
};

struct recursive_iterator_traits {
    static constexpr directory_options required = directory_options::none;
    static constexpr directory_options not_supported = directory_options::skip_subdirectory_descendants;
    static constexpr directory_options defaults = required;
};

template <class Traits>
struct is_recursive {
    static constexpr bool value = is_set(Traits::not_supported & directory_options::skip_subdirectory_descendants);
};

static_assert(!is_recursive<iterator_traits>::value, "WTF?");
static_assert(is_recursive<recursive_iterator_traits>::value, "WTF?");

iterator_state_ptr make_iterator_state(const path&, directory_options, error_code&, iterator_traits);

inline iterator_state_ptr make_iterator_state(const path& p, directory_options opts, error_code& ec, recursive_iterator_traits) {
    return make_iterator_state(p, opts, ec, iterator_traits{});
}

const error_code& permission_denied_error(iterator_traits);

inline const error_code& permission_denied_error(recursive_iterator_traits) {
    return permission_denied_error(iterator_traits{});
}

template <class Traits>
constexpr directory_options make_options(directory_options opts) {
    return (make_public(opts) | Traits::required) & ~Traits::not_supported;
}
} // ifilesystem

template <class Traits>
class basic_iterator {
    ifilesystem::iterator_state_ptr m_i;
    
    const path& root_or_empty() const {
        static const path empty;
        return m_i ? m_i->root() : empty;
    }
    
    void clear_if_denied(error_code& ec) const;
    
    template <typename Recurse>
    struct resurse_only : public std::enable_if<ifilesystem::is_recursive<Traits>::value, Recurse> {};
    
    template <typename Recurse>
    using recurse_only_t = typename resurse_only<Recurse>::type;
    
public:
    using value_type = directory_entry;
    using difference_type = ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;
    using iterator_category = std::input_iterator_tag;
    
    basic_iterator() noexcept(std::is_nothrow_constructible<decltype(m_i)>::value) = default;
    explicit basic_iterator(const path&);
    basic_iterator(const path& p, directory_options options);
    basic_iterator(const path& p, directory_options options, error_code& ec);
    basic_iterator(const path& p, error_code& ec);
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
        return m_i ? !is_set(m_i->options() & directory_options::reserved_state_skip_descendants) : false;
    }
    
    template <typename Recurse = void>
    void pop(recurse_only_t<Recurse>* = 0) {
        if (m_i) {
            m_i->pop();
        }
    }
    
    template <typename Recurse = void>
    void disable_recursion_pending(recurse_only_t<Recurse>* = 0) {
        if (m_i) {
            m_i->skip_descendants();
        }
    }
    
    // Extensions
    basic_iterator& operator++(int);
    
    static constexpr directory_options default_options() {
        return Traits::defaults;
    }
};

template <class Traits>
inline basic_iterator<Traits>::basic_iterator(const path& p)
    : basic_iterator<Traits>::basic_iterator(p, default_options()) {
}

template <class Traits>
inline basic_iterator<Traits>::basic_iterator(const path& p, directory_options opts, error_code& ec)
    : m_i(ifilesystem::make_iterator_state(p, ifilesystem::make_options<Traits>(opts), ec, Traits{})) {
    increment(ec);
    clear_if_denied(ec);
}

template <class Traits>
inline basic_iterator<Traits>::basic_iterator(const path& p, error_code& ec)
    : basic_iterator<Traits>::basic_iterator(p, default_options(), ec) {
}

template <class Traits>
const directory_entry& basic_iterator<Traits>::operator*() const {
    static const directory_entry empty{};
    return m_i ? m_i->current() : empty;
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
    return ti == oi || (ti && oi && ti->equal_to(oi));
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

// Private
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

} // v1
} // filesystem
} // prosoft

#endif // PS_CORE_FILESYSTEM_ITERATOR_HPP
