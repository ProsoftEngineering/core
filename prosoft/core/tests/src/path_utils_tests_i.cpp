// Copyright © 2015-2016, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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
// This is an implementation include file for concrete test types.

SECTION("private") {
    using namespace prosoft::filesystem::ifilesystem;

    WHEN("a full path begins with a drive letter") {
        THEN("starts_with_drive_letter is true") {
            CHECK(starts_with_drive_letter(path_t{R"(C:\test)"}));
        }
    }

    WHEN("a relative path begins with a drive letter") {
        THEN("starts_with_drive_letter is true") {
            CHECK(starts_with_drive_letter(path_t{R"(Z:test)"}));
        }
    }

    WHEN("a path does not begin with a drive letter") {
        THEN("starts_with_drive_letter is false") {
            CHECK_FALSE(starts_with_drive_letter(path_t{"test"}));
        }
    }

    WHEN("a path begins with a drive UNC drive letter") {
        THEN("starts_with_drive_letter is false") {
            CHECK_FALSE(starts_with_drive_letter(path_t{R"(\\server\C:\test)"}));
        }
    }
    
    WHEN("a path contains an embedded drive letter") {
        AND_WHEN("the position is specified") {
            THEN("starts_with_drive_letter is true") {
                CHECK_FALSE(starts_with_drive_letter(path_t{R"(\\a\C:\test)", 4}));
            }
        }
    }
    
    WHEN("a path is empty") {
        THEN("starts_with_drive_letter is false") {
            CHECK_FALSE(starts_with_drive_letter(path_t{}));
        }
    }

}

WHEN("appending to a path with no separator") {
    path_t p{"1"};
    auto pw = p;
    
    append(p, path_t{"2"}, path_style::posix);
    append(pw, path_t{"2"}, path_style::windows);
    THEN("a separator is added") {
        CHECK(p == path_t{"1/2"});
        CHECK(pw == path_t{"1\\2"});
    }
}

WHEN("appending to a path with a separator") {
    path_t p{"1/"};
    path_t pw{"1\\"};
    
    append(p, path_t{"2"}, path_style::posix);
    append(pw, path_t{"2"}, path_style::windows);
    THEN("a separator is not added") {
        CHECK(p == path_t{"1/2"});
        CHECK(pw == path_t{"1\\2"});
    }
}

WHEN("appending a path component with a separator") {
    path_t p{"1"};
    auto pw = p;
    
    append(p, path_t{"/2"}, path_style::posix);
    append(pw, path_t{"\\2"}, path_style::windows);
    THEN("a separator is not added") {
        CHECK(p == path_t{"1/2"});
        CHECK(pw == path_t{"1\\2"});
    }
}

WHEN("appending an empty path component") {
    path_t p{"/a/b/c"};
    const auto expected = p;
    append(p, path_t{});
    THEN("the path is not changed") {
        CHECK(p == expected);
    }
}

SECTION("sanitize") {
    CHECK(sanitize(path_t{"C:////Users\\Prosoft/Desktop\\file.txt"}, path_style::windows) == path_t{"C:\\Users\\Prosoft\\Desktop\\file.txt"});
    CHECK(sanitize(path_t{"\\\\?\\C:/Users/prosoft\\Desktop\\\\\\file.txt"}, path_style::windows) == path_t{"\\\\?\\C:\\Users\\prosoft\\Desktop\\file.txt"});
    CHECK(sanitize(path_t{"C:/"}, path_style::windows) == path_t{"C:\\"});
    CHECK(sanitize(path_t{"//?/C:\\Users"}, path_style::windows) == path_t{"\\\\?\\C:\\Users"});
    CHECK(sanitize(path_t{"///Users////Prosoft/Desktop"}, path_style::posix) == path_t{"/Users/Prosoft/Desktop"});
}
