// Copyright © 2018-2020, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#import <Foundation/Foundation.h>
#include <sys/signal.h>

#include "spawn.hpp"

static_assert(__has_feature(objc_arc), "ARC is required");
static_assert(OS_OBJECT_USE_OBJC_RETAIN_RELEASE, "Dispatch ARC is required");

namespace {

template <class Duration>
dispatch_time_t dispatch_now(Duration delta) {
    return delta.count() == 0 ? DISPATCH_TIME_FOREVER
                              : dispatch_time(DISPATCH_TIME_NOW, std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count());
}

} // anon

void prosoft::spawn(const spawn_path& path, const spawn_args& args, spawn_cout& cout, spawn_cerr& cerr, std::chrono::seconds timeout, std::system_error& err) {
    err = std::system_error(0, std::system_category());
    cout.clear();
    cerr.clear();
    @autoreleasepool { // Necessary to not leak pipe handles on non-main thread.
        @try {
            NSTask* task = [[NSTask alloc] init];
            task.launchPath = @"/usr/bin/env"; // This gives us a sanitized environment with a PATH only we control.
            NSMutableArray* targs = [NSMutableArray arrayWithObjects:@"PATH=/bin:/usr/bin:/sbin:/usr/sbin", @(path.c_str()), nil];
            for (const auto& a : args) {
                [targs addObject:@(a.c_str())];
            }
            task.arguments = targs;
            if (NSPipe* p = [[NSPipe alloc] init]) { // pipe can fail
                task.standardOutput = p;
            }
            if (NSPipe* p = [[NSPipe alloc] init]) {
                task.standardError = p;
            }

            NSFileHandle* outp = ((NSPipe*)task.standardOutput).fileHandleForReading;
            NSFileHandle* errp = ((NSPipe*)task.standardError).fileHandleForReading;

            // [waitUntilExit] will spin the run loop, which can cause unexpected reentrancy on the current thread
            auto sem = dispatch_semaphore_create(0);
            task.terminationHandler = ^(NSTask*) {
                dispatch_semaphore_signal(sem);
            };

            [task launch];
            // if NSTask throws an exception it's likely this:
            // https://openradar.appspot.com/radar?id=1387401
            // Which is a bug in apps that leak pipe handles.
            
            // Pipe channels can fill with data and block the child until drained.
            // This can lead to a deadlock while waiting on the semaphore, therefore we asynchronously read data as it becomes available.
            // The readability handler is called from an internal NSFileHandle serial queue, so no need for locking.
            // If the semaphore times out, cout and cerr can contain partial data.
            if (outp) {
                const auto coutp = &cout;
                outp.readabilityHandler = ^(NSFileHandle* f) {
                    NSData* d = f.availableData;
                    if (d.length > 0) {
                        coutp->append((const char*)d.bytes, d.length);
                    }
                };
            }
            
            if (errp) {
                const auto cerrp = &cerr;
                errp.readabilityHandler = ^(NSFileHandle* f) {
                    NSData* d = f.availableData;
                    if (d.length > 0) {
                        cerrp->append((const char*)d.bytes, d.length);
                    }
                };
            }
            
            const auto waitfor = dispatch_now(timeout);
            const bool timedout = 0 != dispatch_semaphore_wait(sem, waitfor);
            if (timedout && task.running) {
                [task terminate];
                [NSThread sleepForTimeInterval:0.2];
                if (task.running) {
                    kill(task.processIdentifier, SIGKILL);
                }
                err = std::system_error(EAGAIN, std::system_category());
                return;
            }

            auto status = task.terminationStatus;
            if (status) {
                if (status == 126) { // See 'env' man page.
                    status = ENOEXEC;
                } else if (status == 127) {
                    status = ENOENT;
                }
            }
            err = std::system_error(status, std::system_category());
        } @catch (NSException* nex) {
            std::string what = "Task Exception: ";
            if (auto s = nex.reason.UTF8String) {
                what.append(s);
            }
            err = std::system_error(ECHILD, std::system_category(), what);
        } @catch (...) {
            err = std::system_error(ECHILD, std::system_category(), "Unknown Task Exception");
        } // @try
    } // autoreleasepool
}
