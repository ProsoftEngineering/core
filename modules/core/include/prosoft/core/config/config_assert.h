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

#ifndef PS_CORE_CONFIG_ASSERT_H
#define PS_CORE_CONFIG_ASSERT_H

#ifndef PS_CORE_CONFIG_COMPILER_H
#error "missing config_compiler"
#endif

// Compile time asserts (the condition must be a constant expression)
#if PS_CPP11
#define PS_STATIC_ASSERT(condition, msg) static_assert(condition, msg)
#elif __has_extension(c_static_assert) || PS_GCC_REQ(4, 4, 6)
#define PS_STATIC_ASSERT(condition, msg) _Static_assert(condition, msg)
#else
#define PS_STATIC_ASSERT(condition, msg)
#endif

// Asserts are debug only, the condition will NOT be executed in release builds.
// Checks are asserts in debug builds, but will execute in release builds with the results ignored.

#if DEBUG

#include <stdio.h>

#define PSASSERT(condition, fmt) \
    do { \
        if (0 == (condition)) { \
            (void) fprintf(stderr, "!!ASSERT FIRED!! (%s:%d,%s) cond=(%s): " fmt "\n", __FILE__, __LINE__, __FUNCTION__, #condition); \
            fflush(stderr); \
            __builtin_debugger(); \
        } \
    } while (0)

#define PSASSERT_WITH_ARGS(condition, fmt, ...) \
    do { \
        if (0 == (condition)) { \
            (void) fprintf(stderr, "!!ASSERT FIRED!! (%s:%d,%s) cond=(%s): " fmt "\n", __FILE__, __LINE__, __FUNCTION__, #condition, __VA_ARGS__); \
            fflush(stderr); \
            __builtin_debugger(); \
        } \
    } while (0)

#define PSASSERT_UNREACHABLE(fmt) \
    do { \
        (void) fprintf(stderr, "!!ASSERT FIRED!! (%s:%d,%s) UNREACHABLE: " fmt "\n", __FILE__, __LINE__, __FUNCTION__); \
        fflush(stderr); \
        __builtin_debugger(); \
    } while (0)

#define PSASSERT_UNREACHABLE_WITH_ARGS(fmt, ...) \
    do { \
        (void) fprintf(stderr, "!!ASSERT FIRED!! (%s:%d,%s) UNREACHABLE: " fmt "\n", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); \
        fflush(stderr); \
        __builtin_debugger(); \
    } while (0)


#ifdef __cplusplus
#define PSASSERT_NOTNULL(obj_) PSASSERT(nullptr != obj_, "null!")
#define PSASSERT_NULL(obj_) PSASSERT(nullptr == obj_, "not null!")
#else
#define PSASSERT_NOTNULL(obj_) PSASSERT(NULL != obj_, "null!")
#define PSASSERT_NULL(obj_) PSASSERT(NULL == obj_, "not null!")
#endif

#if __APPLE__ || __FreeBSD__
PS_EXTERN_C int pthread_main_np(void);
#define PS_IS_MAIN_THREAD() (0 != pthread_main_np())
#elif __linux__
#include <sys/syscall.h>
#include <unistd.h>
#define PS_IS_MAIN_THREAD() (syscall(SYS_gettid) == getpid())
#endif // No way to do this easily on Windows. Solaris has thr_main().

#ifdef PS_IS_MAIN_THREAD
#define PSASSERT_MAIN() PSASSERT(PS_IS_MAIN_THREAD(), "not main thread")
#else
#define PSASSERT_MAIN()
#endif

#else // DEBUG

#define PSASSERT(condition, fmt)
#define PSASSERT_WITH_ARGS(condition, fmt, ...)
#define PSASSERT_UNREACHABLE(fmt)
#define PSASSERT_UNREACHABLE_WITH_ARGS(fmt, ...)
#define PSASSERT_NOTNULL(obj_)
#define PSASSERT_NULL(obj_)
#define PSASSERT_MAIN()

#endif // DEBUG

//// Checks ////

#if DEBUG

#define PSCHECK_TRUE(F) PSASSERT(TRUE == (F), "error!")
#define PSCHECK_FALSE(F) PSASSERT(FALSE == (F), "error!")

#define PSCHECK_NOERR(F) PSASSERT(0 == (F), "error!")
// same as check noerr, except a single error code can be ignored
#define PSCHECK_NOERR_EX(E, F)              \
    do {                                    \
        int err___ = (F);                   \
        if (-1 == err___) {                 \
            err___ = errno;                 \
        }                                   \
        if (0 != err___ && (E) != err___) { \
            PSASSERT_UNREACHABLE("error!"); \
        }                                   \
    } while (0)

#define PSCHECK_NULL(V) PSASSERT_NULL((V))

#else // DEBUG

// could possibly log something
#define PSCHECK_TRUE(F) (void) F
#define PSCHECK_FALSE(F) (void) F
#define PSCHECK_NOERR(F) (void) F
#define PSCHECK_NOERR_EX(E, F) (void) F
#define PSCHECK_NULL(V) (void) V

#endif // DEBUG

#endif // PS_CORE_CONFIG_ASSERT_H
