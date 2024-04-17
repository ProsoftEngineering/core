// Copyright © 2016-2018, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

// Analyzation macros

#ifndef PS_CONFIG_ANALYZER_H
#define PS_CONFIG_ANALYZER_H

#include <stddef.h> // size_t

// https://github.com/llvm-mirror/compiler-rt/blob/master/include/sanitizer/lsan_interface.h
#if PS_HAVE_LSAN_INTERFACE_H
#include <sanitizer/lsan_interface.h>
#define PS_INTENTIONAL_MEMORY_LEAK_BEGIN __lsan_disable();
#define PS_INTENTIONAL_MEMORY_LEAK_END __lsan_enable();
#define PS_INTENTIONAL_MEMORY_LEAK(Ptr) __lsan_ignore_object((Ptr))
#else
#define PS_INTENTIONAL_MEMORY_LEAK_BEGIN
#define PS_INTENTIONAL_MEMORY_LEAK_END
#define PS_INTENTIONAL_MEMORY_LEAK(Ptr)
#endif

#ifdef __cplusplus
namespace prosoft {

class intentional_leak_guard {
public:
    intentional_leak_guard() {
        PS_INTENTIONAL_MEMORY_LEAK_BEGIN
    }
    ~intentional_leak_guard() {
        PS_INTENTIONAL_MEMORY_LEAK_END
    }
    
    intentional_leak_guard(const intentional_leak_guard&) = delete;
    intentional_leak_guard(intentional_leak_guard&&) = delete;
    intentional_leak_guard& operator=(const intentional_leak_guard&) = delete;
    intentional_leak_guard& operator=(intentional_leak_guard&&) = delete;
    
    void* operator new(size_t) = delete;
    void* operator new(size_t, void*) = delete;
    void* operator new[](size_t) = delete;
    void* operator new[](size_t, void*) = delete;
};

} // prosoft
#endif

#endif // PS_CONFIG_ANALYZER_H
