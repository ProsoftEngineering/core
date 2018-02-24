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

// DO NOT INCLUDE IN BUILD TARGET SOURCE.
// This is an implementation include file.

WHEN("getting the mount path for an empty path") {
    CHECK_THROWS(mount_path(""_p));
    CHECK_THROWS(is_mountpoint(""_p));
}

WHEN("getting the mount path for a non-existant path") {
    auto p = temp_directory_path() / uniqueName;
    error_code ec;
    REQUIRE_FALSE(exists(p, ec));
    CHECK_THROWS(mount_path(p));
    CHECK_THROWS(is_mountpoint(p));
    
    p = ".."_p / uniqueName;
    REQUIRE_FALSE(exists(p, ec));
    CHECK_THROWS(mount_path(p));
    CHECK_THROWS(is_mountpoint(p));
}

WHEN("getting the mount path for a path") {
    const auto p = temp_directory_path();
    error_code ec;
    REQUIRE(exists(p, ec));
    
    const auto mp = mount_path(p, ec);
    CHECK(ec.value() == 0);
    REQUIRE_FALSE(mp.empty());
    CHECK(is_directory(mp));
    CHECK(is_mountpoint(mp));
    
    // Relative path check
    const auto fp = p / uniqueName;
    REQUIRE_FALSE(exists(fp, ec));
    {
        create_file(fp);
    }
    CHECK_FALSE(is_mountpoint(fp));
    
    current_path(temp_directory_path());
    const auto rp = fp.filename();
    REQUIRE(equivalent(canonical(rp), fp));
    CHECK(mount_path(rp, ec) == mp);
    
    REQUIRE(remove(fp));
};

WHEN("getting the mount path for a mount path") {
    error_code ec;
    const auto mp = mount_path(temp_directory_path(), ec);
    REQUIRE_FALSE(mp.empty());
    CHECK(mount_path(mp, ec) == mp);
}

WHEN("getting the hidden attribute") {
    error_code ec;
    CHECK_FALSE(is_hidden(home_directory_path(), ec));
    CHECK_FALSE(is_hidden(temp_directory_path() / PS_TEXT("abcdefghij"), ec)); // non-existent path
#if !_WIN32
    CHECK(is_hidden(temp_directory_path() / ".abcdefghij", ec)); // all dot files are considered hidden, even non-existent ones
    CHECK(is_hidden(".abcdefghij", ec));
    auto p = home_directory_path() / ".ssh";
    if (exists(p, ec)) {
        CHECK(is_hidden(p, ec));
    }
    p = home_directory_path() / ".profile";
    if (exists(p, ec)) {
        CHECK(is_hidden(p, ec));
    }
#else
    path p{PS_TEXT("C:\\Windows\\Installer")};
    if (exists(p, ec)) {
        CHECK(is_hidden(p, ec));
    }
#endif

#if __APPLE__
    CHECK(is_hidden("/Volumes", ec));
#endif
}

WHEN("getting the package attribute") {
    error_code ec;
    CHECK_FALSE(is_package(temp_directory_path(), ec));
    CHECK_FALSE(is_package(home_directory_path(), ec));
    CHECK_FALSE(is_package(temp_directory_path() / PS_TEXT("abcdefghij"), ec)); // non-existent path
#ifdef MAC_OS_X_VERSION_MIN_REQUIRED
    CHECK(is_package("/System/Library/CoreServices/Finder.app", ec));
#endif
}
