// Copyright © 2013-2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_UNIQUE_RESOURCE_HPP
#define PS_CORE_UNIQUE_RESOURCE_HPP

#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#elif _WIN32
#include <prosoft/core/config/config_windows.h>
#include <windows.h>
#endif

#include <cstring> // memset
#include <memory>
#include <type_traits>
#include <utility>

#include <prosoft/core/config/config.h>

namespace prosoft {

// malloc for typed anonymous memory
namespace iunique {
struct malloc_delete {
    void operator()(void* p) PS_NOEXCEPT {
        std::free(p);
    }
};
} // iunique

template <typename T>
using unique_malloc = std::unique_ptr<T, iunique::malloc_delete>;

template <typename T>
inline unique_malloc<T> make_malloc(size_t sz) PS_NOEXCEPT {
    return unique_malloc<T>{ (T*)std::malloc(sz) };
}

struct count_t {
    explicit count_t(size_t ct) PS_NOEXCEPT : count(ct) {}
    size_t count;
};

template <typename T>
inline unique_malloc<T> make_malloc(count_t count) PS_NOEXCEPT {
    return unique_malloc<T>{ (T*)std::malloc(sizeof(T) * count.count) };
}

template <typename T>
inline unique_malloc<T> init_malloc(size_t sz, int pattern = 0) PS_NOEXCEPT {
    auto p = make_malloc<T>(sz);
    if (p) {
        std::memset(p.get(), pattern, sz);
    }
    return p;
}

template <typename T>
inline unique_malloc<T> make_malloc(size_t count, size_t sz) PS_NOEXCEPT {
    return unique_malloc<T>{ (T*)std::calloc(count, sz) };
}

template <typename T>
inline unique_malloc<T>& make_malloc(unique_malloc<T>& ptr, size_t sz) PS_NOEXCEPT {
    auto p = ptr.release(); // don't free old ptr
    ptr.reset((T*)std::realloc(p, sz));
    return ptr;
}

template <typename T, typename... Args>
inline unique_malloc<T> make_malloc_throw(Args&&... args) {
    auto p = make_malloc<T>(std::forward<Args>(args)...);
    PS_THROW_IF(!p, std::bad_alloc());
    return p;
}

#if __APPLE__

namespace iunique {
// CF ref types can be both non-const and const
struct cf_delete {
    void operator()(void* p) PS_NOEXCEPT {
        if (p) { ::CFRelease(p); }
    }
    void operator()(const void* p) PS_NOEXCEPT {
        if (p) { ::CFRelease(p); }
    }
};
} // iunique

template <typename CFRefType>
using unique_cftype = std::unique_ptr<typename std::remove_pointer<CFRefType>::type, iunique::cf_delete>;

namespace CF {
// Common CF type aliases
using unique_array = unique_cftype<CFArrayRef>;
using unique_data = unique_cftype<CFDataRef>;
using unique_dictionary = unique_cftype<CFDictionaryRef>;
using unique_error = unique_cftype<CFErrorRef>;
using unique_number = unique_cftype<CFNumberRef>;
using unique_string = unique_cftype<CFStringRef>;
using unique_type = unique_cftype<CFTypeRef>;
using unique_url = unique_cftype<CFURLRef>;
} // CF

#elif _WIN32

namespace iunique {
struct global_delete {
    void operator() (void* p) PS_NOEXCEPT {
        ::GlobalFree(p);
    }
};
// XXX: Heap allocation requires external context other than the pointer and would thus be hard to implement as a unique_ptr.
struct local_delete {
    void operator() (void* p) PS_NOEXCEPT {
        ::LocalFree(p);
    }
};
} // iunique

namespace windows {
template <typename T>
using unique_global = std::unique_ptr<T, iunique::global_delete>;

template <typename T>
using unique_local = std::unique_ptr<T, iunique::local_delete>;

struct handle_traits {
    typedef HANDLE pointer;
    
    bool is_valid(pointer p) const PS_NOEXCEPT {
        return (p && p != INVALID_HANDLE_VALUE);
    }
    
    void operator()(pointer p) PS_NOEXCEPT {
        if (is_valid(p)) {
            ::CloseHandle(p);
        }
    }
};

// Custom class for HANDLE's, mainly because std::unique_ptr does not have a way to customize the invalid ptr value.
// See http://blogs.msdn.com/b/oldnewthing/archive/2004/03/02/82639.aspx for further HANDLE pitfalls.
template <class Ptr, class Traits>
class unique_handle {
    using up_type = std::unique_ptr<typename std::remove_pointer<Ptr>::type, Traits>;
    up_type m_up;
public:
    typedef typename up_type::element_type element_type;
    typedef typename up_type::deleter_type deleter_type;
    typedef typename up_type::pointer pointer;

    PS_DEFAULT_CONSTRUCTOR(unique_handle);
    PS_DEFAULT_DESTRUCTOR(unique_handle);
    PS_DISABLE_COPY(unique_handle);
    PS_DEFAULT_MOVE(unique_handle);
    
    unique_handle(Ptr p) PS_NOEXCEPT : m_up(p) {}
    
    Ptr get() const PS_NOEXCEPT { return m_up.get(); }
    // XXX: HANDLE is void, so operator* and -> make no sense.
    explicit operator bool() const PS_NOEXCEPT { return m_up.get_deleter().is_valid(get()); }
    
    Ptr release() PS_NOEXCEPT { return m_up.release(); }
    void reset(Ptr p = Ptr{}) PS_NOEXCEPT { m_up.reset(p); }
    void operator=(std::nullptr_t) PS_NOEXCEPT { reset(); }
    void swap(unique_handle& uh) PS_NOEXCEPT { m_up.swap(uh.m_up); }
};

using Handle = unique_handle<HANDLE, handle_traits>;

} // windows

#endif // __APPLE__

// CF and Win APIs use handles (the ptr-to-ptr type) quite a lot.
namespace iunique {
template <class T, class D, class UPtr>
class uphandle;

template <class Up>
using uphandle_t = uphandle<typename Up::element_type, typename Up::deleter_type, typename std::decay<Up>::type>;
}

template <class Up>
iunique::uphandle_t<Up> handle(Up&) PS_NOEXCEPT;

namespace iunique {

template <class T, class D, class UPtr>
class uphandle {
    using upointer = UPtr;
    using pointer = typename upointer::pointer;
    upointer* m_u;
    pointer m_p;

    void store() PS_NOEXCEPT {
        if (m_u) {
            m_u->reset(m_p);
        }
    }

    uphandle(upointer& up) PS_NOEXCEPT : m_u(&up), m_p(nullptr) {}

    uphandle(uphandle&& other)
        : m_u(other.m_u)
        , m_p(other.m_p) {
        other.m_u = nullptr;
        other.m_p = nullptr;
    }

    friend uphandle prosoft::handle<upointer>(upointer&) PS_NOEXCEPT;
 
public:
    // Disabling copy and making construct and move private means this class can never bind to an external lvalue.
    // This would be dangerous as leaks coud occur quite easily since the backing store sync only happens on destruction.
    PS_DISABLE_DEFAULT_CONSTRUCTOR(uphandle);
    PS_DISABLE_COPY(uphandle);
    
    ~uphandle() PS_NOEXCEPT {
        store();
    }

    operator pointer*() { return &m_p; }
};

} // iunique

template <class Up>
iunique::uphandle_t<Up> handle(Up& up) PS_NOEXCEPT {
    return iunique::uphandle_t<Up>{up};
}

} // prosoft

#endif // PS_CORE_UNIQUE_RESOURCE_HPP
