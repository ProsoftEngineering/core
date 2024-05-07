// Copyright © 2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <regex_internal.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("regex_internal") {
    using namespace prosoft;
    using namespace prosoft::regex_constants;

    SECTION("compile options") {
        const auto expected = ONIG_OPTION_IGNORECASE|ONIG_OPTION_DONT_CAPTURE_GROUP|ONIG_OPTION_SINGLELINE;
        CHECK(options(syntax_option_type::icase|syntax_option_type::nosubs|syntax_option_type::noendl) == expected);
        CHECK(ONIG_OPTION_NONE == options(syntax_option_type::basic));

        OnigSyntaxPtr syntax = nullptr;
        options(syntax_option_type::basic, syntax);
        CHECK(ONIG_SYNTAX_POSIX_BASIC == syntax);

        syntax = nullptr;
        options(syntax_option_type::extended, syntax);
        CHECK(ONIG_SYNTAX_POSIX_EXTENDED == syntax);

        syntax = nullptr;
        options(syntax_option_type::grep, syntax);
        CHECK(ONIG_SYNTAX_GREP == syntax);

        syntax = nullptr;
        const auto opt = options(syntax_option_type::egrep, syntax);
        CHECK(ONIG_SYNTAX_GREP == syntax);
        CHECK(0 != (opt & ONIG_OPTION_EXTEND));

        syntax = nullptr;
        options(syntax_option_type::icase, syntax);
        CHECK(ONIG_SYNTAX_RUBY == syntax); // default syntax
    }

    SECTION("encodings") {
        using namespace prosoft::iregex;
        CHECK(ONIG_ENCODING_UTF8 == onig_encoding(encoding::utf8));
        CHECK(ONIG_ENCODING_UTF16_LE == onig_encoding(encoding::utf16le));
        CHECK(ONIG_ENCODING_UTF16_BE == onig_encoding(encoding::utf16be));

        CHECK_THROWS(onig_encoding(encoding::utf16)); // utf16 is a platform specific meta-value handled at higher levels.
    }

    SECTION("ONIG errors") {
        try { throw_onig_error(ONIGERR_TOO_BIG_BACKREF_NUMBER); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_backref); }
        try { throw_onig_error(ONIGERR_INVALID_BACKREF); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_backref); }
        try { throw_onig_error(ONIGERR_NUMBERED_BACKREF_OR_CALL_NOT_ALLOWED); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_backref); }

        try { throw_onig_error(ONIGERR_END_PATTERN_AT_LEFT_BRACE); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_brace); }

        try { throw_onig_error(ONIGERR_CHAR_CLASS_VALUE_AT_END_OF_RANGE); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_range); }
        try { throw_onig_error(ONIGERR_CHAR_CLASS_VALUE_AT_START_OF_RANGE); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_range); }

        try { throw_onig_error(ONIGERR_MEMORY); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_space); }

        try { throw_onig_error(ONIGERR_TARGET_OF_REPEAT_OPERATOR_NOT_SPECIFIED); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_badrepeat); }
        try { throw_onig_error(ONIGERR_TARGET_OF_REPEAT_OPERATOR_INVALID); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_badrepeat); }
        try { throw_onig_error(ONIGERR_NESTED_REPEAT_OPERATOR); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_badrepeat); }
        try { throw_onig_error(ONIGERR_TOO_BIG_NUMBER_FOR_REPEAT_RANGE); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_badrepeat); }
        try { throw_onig_error(ONIGERR_UPPER_SMALLER_THAN_LOWER_IN_REPEAT_RANGE); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_badrepeat); }
        try { throw_onig_error(ONIGERR_INVALID_REPEAT_RANGE_PATTERN); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_badrepeat); }

        try { throw_onig_error(ONIGERR_MATCH_STACK_LIMIT_OVER); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_stack); };

        try { throw_onig_error(ONIGERR_PARSER_BUG); } catch(const regex_error& e) { CHECK(e.code() == error_type::error_onig); };
    }
}
