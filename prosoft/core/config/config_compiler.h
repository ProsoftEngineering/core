// Copyright © 2006-2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

// Compiler (and CPU) related configuration.

#ifndef PS_CORE_CONFIG_COMPILER_H
#define PS_CORE_CONFIG_COMPILER_H

//// Early defines ////

// Convert MSVC define
#if _DEBUG == 1 && !defined(DEBUG)
#define DEBUG 1
#endif

// DEBUG is not defined for Release builds, define something that always has a constant value (which static asserts require)
#if !defined(PS_DEBUG_BUILD)
#if !defined(DEBUG) || !DEBUG
#define PS_DEBUG_BUILD 0
#else
#define PS_DEBUG_BUILD 1
#endif
#endif

// __ppc__ is either an Apple only define or an old GCC define that has been removed in favor of __PPC__
#if defined(__PPC__) && !defined(__ppc__)
#define __ppc__ __PPC__
#endif

//// Compiler versions ////

#define PS_CLANG_REQ(_M, _m, _p) (defined(__clang__) && ((__clang_major__ > _M) || (__clang_major__ >= _M && __clang_minor__ > _m) || (__clang_major__ >= _M && __clang_minor__ >= _m) && __clang_patchlevel__ >= _p))

#define PS_GCC_REQ(_M, _m, _p) (defined(__GNUC__) && ((__GNUC__ > _M) || (__GNUC__ >= _M && __GNUC_MINOR__ > _m) || (__GNUC__ >= _M && __GNUC_MINOR__ >= _m) && __GNUC_PATCHLEVEL__ >= _p))

// No patch level.
// XXX: MSVC's (<= 2013, 2015?) non-c99 preprocessor does not support nested "defined()" clauses in a #define properly.
#ifdef _MSC_VER
#define PS_MSVC_REQ(_M, _m) (_MSC_VER >= (_M * 100 + _m * 10))
#else
#define PS_MSVC_REQ(_M, _m) 0
#endif

//// Link defines ////
// For compatibility reasons PS_EXTERN includes 'extern'
#if !_MSC_VER && !__MINGW32__
#define PS_EXPORT __attribute__((visibility("default")))
#define PS_EXTERN extern __attribute__((visibility("hidden")))
#elif _WIN32
// Win32 link has always been explicit export.
#if defined(PS_LIB_EXPORTS)
#define PS_EXPORT __declspec(dllexport)
#elif defined(PS_LIB_IMPORTS)
#define PS_EXPORT __declspec(dllimport)
#else
#define PS_EXPORT
#endif
#define PS_EXTERN extern
#endif

//// MSVC GCC aliases ////

#if _MSC_VER
#if !defined(__x86_64__) && defined(_M_X64)
#define __x86_64__ _M_X64
#endif
#if !defined(__i386__) && defined(_M_IX86)
#define __i386__ _M_IX86
#endif
// XXX: ARM?
#if !defined(__LLP64__) && defined(_WIN64)
#define __LLP64__ 1
#endif
#endif // _MSC_VER

//// Clang macro aliases ////

#if !defined(__has_builtin)
#define __has_builtin(x) 0
#endif

#if !defined(__has_feature)
#define __has_feature(x) 0
#endif

#if !defined(__has_attribute)
#define __has_attribute(x) 0
#endif

#if !defined(__has_extension) // clang 3.0+
#define __has_extension(x) __has_feature(x)
#endif

//// Compiler C++ language support ////

// C++11/14 support -- Clang and GCC are basically 100% 11 compliant as of 3.3 and 4.8.1 respectively.
// VS 2013 is decent, and 2015 much better (basically complete excepting SFINAE)
// Clang: http://clang.llvm.org/cxx_status.html
// GCC: https://gcc.gnu.org/projects/cxx0x.html, https://gcc.gnu.org/projects/cxx1y.html
// MSVC: http://msdn.microsoft.com/en-us/library/vstudio/hh567368.aspx
//

#if defined(__cplusplus)
// GCC < 4.7 sets __cplusplus to 1 for compat with an old Solaris version.
// MS sets their __cplusplus to 199711.
// http://connect.microsoft.com/VisualStudio/feedback/details/763051/a-value-of-predefined-macro-cplusplus-is-still-199711l
//
#define PS_CPP11 (__cplusplus >= 201103L || __GXX_EXPERIMENTAL_CXX0X__ || PS_MSVC_REQ(16, 0))
#if PS_CPP11
// Superset of PS_CPP11. "Preferred" compiler versions that implement the majority of C++11 features.
#define PS_PREFERRED_CPP11 (PS_CLANG_REQ(3, 2, 0) || PS_GCC_REQ(4, 7, 0) || PS_MSVC_REQ(18, 0))
// Superset of PS_PREFERRED_CPP11. 100% compliant C++11 compilers (XXX: this does not imply a 100% compliant std lib -- e.g. std::regex is not implemented in GCC until 4.9)
#define PS_COMPLETE_CPP11 (PS_CLANG_REQ(3, 3, 0) || PS_GCC_REQ(4, 8, 1))
#else // PS_CPP11
#define PS_PREFERRED_CPP11 0
#define PS_COMPLETE_CPP11 0
#endif // PS_CPP11

#define PS_CPP14 (__cplusplus > 201103L || PS_MSVC_REQ(19, 0))
#if PS_CPP14
// Superset of PS_CPP14. "Preferred" compiler versions that implement the majority of C++14 features.
#define PS_PREFERRED_CPP14 (PS_CLANG_REQ(3, 4, 0) || PS_GCC_REQ(4, 9, 0) || PS_MSVC_REQ(19, 0))
// Superset of PS_PREFERRED_CPP14. 100% compliant C++14 compilers.
#define PS_COMPLETE_CPP14 (PS_CLANG_REQ(3, 4, 0) || PS_GCC_REQ(5, 0, 0))
#else // PS_CPP14
#define PS_PREFERRED_CPP14 0
#define PS_COMPLETE_CPP14 0
#endif // PS_CPP14
#else // __cplusplus
#define PS_CPP11 0
#define PS_PREFERRED_CPP11 0
#define PS_COMPLETE_CPP11 0
#define PS_CPP14 0
#define PS_PREFERRED_CPP14 0
#define PS_COMPLETE_CPP14 0
#endif // __cplusplus

//// Compiler name ////

#if defined(__clang__)
#define PS_COMPILER_NAME "LLVM/Clang"
#elif defined(__GNUC__)
#if !__MINGW32__
#define PS_COMPILER_NAME "GCC"
#else
#define PS_COMPILER_NAME "Mingw"
#endif
#elif defined(_MSC_VER)
#define PS_COMPILER_NAME "MSVC"
#else
#error "unknown compiler"
#endif

//// Compiler features ////

#if !__has_builtin(__builtin_debugger)
// We use custom ASM because __builtin_trap assumes execution will halt and not continue. Under a debugger that's a bad assumption.
#if __clang__ || __GNUC__
#if __x86_64__ || __i386__
#define __builtin_debugger() __asm__ volatile("int $3")
#elif __ppc__
#define __builtin_debugger() __asm__ volatile("trap")
#elif __arm__
// __builtin_trap() just calls abort()
// There's still debug issues with the ASM; namely step/stepi/continue don't work, but at least you can do some inspection.
// See ARM comments for more info and a possible workaround: https://github.com/scottt/debugbreak/blob/master/debugbreak.h
#if !__thumb__
#define __builtin_debugger() __asm__ volatile(".inst 0xe7f001f0")
#else
#define __builtin_debugger() __asm__ volatile(".inst 0xde01")
#endif
#else
#warning "unknown architecture"
#define __builtin_debugger() __builtin_trap()
#endif
#elif _MSC_VER
#define __builtin_debugger() __debugbreak()
#else
#error "unknown compiler"
#endif // __clang__ || __GNUC__
#endif // __builtin_debugger

// XXX: some platforms (SPARC) may require the return value to be wrapped in __builtin_extract_return_address
#if !__has_builtin(__builtin_return_address) && !PS_GCC_REQ(4, 0, 0) // not sure when this appeared in GCC
#if _MSC_VER
#define __builtin_return_address(Level__) __ReturnAddress // Level is ignored
#else
#define __builtin_return_address(Level__)
#endif
#endif

// __builtin_unreachable was added to GCC in version 4.5
#if !__has_builtin(__builtin_unreachable) && !PS_GCC_REQ(4, 5, 0)
#if _MSC_VER
// http://social.msdn.microsoft.com/Search/en-US?query=__assume
#define __builtin_unreachable() __assume(0)
#else
#define __builtin_unreachable()
#endif
#endif

// Branch probability hint.
// XXX: When comparing pointers, you should always use NULL == ptr, instead of !ptr.
#if __has_builtin(__builtin_expect) || defined(__GNUC__)
#define PS_EXPECTED(x) (__builtin_expect((x), 1))
#define PS_UNEXPECTED(x) (__builtin_expect((x), 0))
#else
#define PS_EXPECTED(x) (x)
#define PS_UNEXPECTED(x) (x)
#endif

#if __clang__ || __GNUC__
#define PS_HAVE_TYPEOF 1
#define PS_TYPEOF(T) __typeof__(T)
#elif _MSC_VER || PS_CPP11
#define PS_HAVE_TYPEOF 0
// decltype is a bit "special", so the behavior is not exactly the same as __typeof__
#define PS_TYPEOF(T) decltype(T)
#endif

//// Compiler attributes ////

#if __clang__ || __GNUC__
#define PS_ALWAYS_INLINE __attribute__((always_inline))
#elif _MSC_VER
#define PS_ALWAYS_INLINE __forceinline
#endif

#if __cplusplus
#define PS_INLINE inline
#elif __clang__ || __GNUC__
#define PS_INLINE static __inline__
#elif _MSC_VER
#define PS_INLINE static __inline
#endif

#ifndef PS_ARGS_NULL_TERMINATED
#if defined(__GNUC__) || defined(__clang__)
#define PS_ARGS_NULL_TERMINATED __attribute__((sentinel))
#else
#define PS_ARGS_NULL_TERMINATED
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PS_FORMAT_PRINTF(x, y) __attribute__((format(printf, x, y)))
#else
#define PS_FORMAT_PRINTF(x, y)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PS_NORETURN __attribute__((noreturn))
#else
#define PS_NORETURN
#endif

#if defined(__clang__) || defined(__GNUC__)
#define PS_UNUSED __attribute__((unused)) // Mostly unnecessary with C++.
#define PS_USED __attribute__((used)) // C/C++ module globals.
#else
#define PS_UNUSED
#define PS_USED
#endif

#if __has_attribute(warn_unused_result) || defined(__GNUC__)
#define PS_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif _MSC_VER
#define PS_WARN_UNUSED_RESULT _Check_return_
#else
#define PS_WARN_UNUSED_RESULT
#endif

// Pre-main module constructors. Mostly unnecessary with C++.
#if __clang__ || __GNUC__
#define PS_CONSTRUCTOR(f) \
    static void __attribute__((constructor)) f(void)
#elif _MSC_VER
#pragma section(".CRT$XCU", read)
#define PS_CONSTRUCTOR(f)                                            \
    static void __cdecl f(void);                                     \
    __declspec(allocate(".CRT$XCU")) void(__cdecl * f##_)(void) = f; \
    static void __cdecl f(void)
#else
#error "unknown compiler"
#endif

// PS_PACKED can be used for declaring a struct as packed. Example:
// PS_PACKED(struct MyFileHeader {
//   uint32_t magic;
//   uint8_t version;
// });
#ifndef PS_PACKED
#if defined(_MSC_VER)
#define PS_PACKED(x) __pragma(pack(push, 1)) x __pragma(pack(pop))
#elif defined(__GNUC__) || defined(__clang__)
#define PS_PACKED(x) x __attribute__((__packed__))
#else
#error "unknown compiler"
#endif
#endif // PS_PACKED

#endif // PS_CORE_CONFIG_COMPILER_H
