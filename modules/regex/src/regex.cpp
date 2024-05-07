// Copyright © 2014-2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <prosoft/core/modules/regex/regex.hpp>
#include "regex_internal.hpp"

namespace prosoft {

OnigOptionType options(prosoft::regex_constants::syntax_option_type flags) {
    using namespace prosoft::regex_constants;

    OnigOptionType opt = ONIG_OPTION_NONE;

    if (0 != (flags & syntax_option_type::icase)) {
        opt |= ONIG_OPTION_IGNORECASE;
    }
    if (0 != (flags & syntax_option_type::nosubs)) {
        opt |= ONIG_OPTION_DONT_CAPTURE_GROUP;
    }

    if (0 != (flags & syntax_option_type::noendl)) {
        // Treat '^' and '$' as Ruby '\A' and '\Z' respectively (begin and end of string -- basically newlines are ignored).
        // On by default for POSIX, PERL and JAVA syntaxes, but can be disabled with ONIG_OPTION_NEGATE_SINGLELINE.
        // XXX: If on, NOTBOL and NOTEOL are ignored.
        opt |= ONIG_OPTION_SINGLELINE;
    }

    // ONIG_OPTION_MULTILINE -- Allow . to match any character, including line separators.
    return opt;
}

OnigOptionType options(prosoft::regex_constants::syntax_option_type flags, OnigSyntaxPtr& syntax) {
    static PS_CONSTEXPR auto syntax_mask = regex_constants::syntax_option_type::basic | regex_constants::syntax_option_type::extended | regex_constants::syntax_option_type::grep | regex_constants::syntax_option_type::egrep;

    auto opt = prosoft::options(flags);
    syntax = ONIG_SYNTAX_RUBY;
    if (PS_UNEXPECTED(0 != (flags & syntax_mask))) {
        if (0 != (flags & regex_constants::syntax_option_type::basic)) {
            syntax = ONIG_SYNTAX_POSIX_BASIC;
        } else if (0 != (flags & regex_constants::syntax_option_type::extended)) {
            syntax = ONIG_SYNTAX_POSIX_EXTENDED;
        } else if (0 != (flags & regex_constants::syntax_option_type::grep)) {
            syntax = ONIG_SYNTAX_GREP;
        } else if (0 != (flags & regex_constants::syntax_option_type::egrep)) {
            syntax = ONIG_SYNTAX_GREP;
            opt |= ONIG_OPTION_EXTEND;
        } else {
            PSASSERT_UNREACHABLE("bad syntax");
        }
    }

    return opt;
}

OnigEncoding onig_encoding(prosoft::iregex::encoding enc) {
    using namespace prosoft::iregex;

    OnigEncoding oenc;
    switch (enc) {
        case encoding::utf8:
            oenc = ONIG_ENCODING_UTF8;
            break;

        case encoding::utf16le:
            oenc = ONIG_ENCODING_UTF16_LE;
            break;

        case encoding::utf16be:
            oenc = ONIG_ENCODING_UTF16_BE;
            break;

        default:
            oenc = nullptr;
            break;
    }
    PS_THROW_IF(nullptr == oenc, std::invalid_argument{"regex encoding is not supported"});
    return oenc;
}

regex_error::regex_error(regex_constants::error_type err, int onigErr)
    : std::runtime_error("regex error")
    , _code(err)
    , _onigErr(onigErr) {}

void prosoft::iregex::compile(Regex* rx, const uchar* first, const uchar* last, regex_constants::syntax_option_type flags, encoding enc) {
    using namespace prosoft::regex_constants;

    OnigSyntaxPtr syntax;
    const auto opt = options(flags, syntax);
    int err;
    if (ONIG_NORMAL != (err = ::onig_new(rx, first, last, opt, onig_encoding(enc), syntax, nullptr))) {
        throw_onig_error(err);
    }
}

void prosoft::iregex::free(Regex rx) {
    if (rx) {
        ::onig_free(rx);
    }
}

namespace {

struct Region {
    Region()
        : _r(onig_region_new()) {}
    ~Region() {
        onig_region_free(_r, 1);
    }

    operator OnigRegion*() PS_NOEXCEPT { return _r; }
    OnigRegion* operator->() PS_NOEXCEPT { return _r; }

    OnigRegion* _r;
};

inline OnigOptionType match_flags_to_onig(regex_constants::match_flag_type flags) {
    OnigOptionType opt = ONIG_OPTION_NONE;
    switch (flags) {
        case regex_constants::match_not_bol:
            opt |= ONIG_OPTION_NOTBOL;
            break;
        case regex_constants::match_not_eol:
            opt |= ONIG_OPTION_NOTEOL;
            break;
        case regex_constants::match_prev_avail:
            opt &= ~ONIG_OPTION_NOTBOL;
            break;
#if notyet
        // These are all compile options in onig.
        case regex_constants::match_any:
        case regex_constants::match_not_null: // ONIG_OPTION_FIND_NOT_EMPTY
        case regex_constants::match_continuous:
#endif
        default:
            break;
    }
    return (opt);
}

/* Description of std::'s difference between match and search.
The entire target sequence must match the regular expression for this function to return true
(i.e., without any additional characters before or after the match).
For a function that returns true when the match is only part of the sequence, see regex_search.
*/

// Onig results buffer [0] is the match, >[0] are group captures.
inline void _rxparse_results(Region& region, prosoft::iregex::match_handler callback) {
    for (int i = 0; i < region->num_regs; ++i) {
        if (region->beg[i] >= 0) {
            size_t count = region->end[i] - region->beg[i];
            callback(region->beg[i], count);
        }
    }
}

} // namespace

void throw_onig_error(int oerr) {
    PSASSERT(ONIG_NORMAL != oerr, "BUG");
    regex_constants::error_type rerr;
    switch (oerr) {
        case ONIGERR_EMPTY_CHAR_CLASS:
        case ONIGERR_PREMATURE_END_OF_CHAR_CLASS:
            rerr = regex_constants::error_type::error_ctype;
            break;

        case ONIGERR_END_PATTERN_AT_ESCAPE:
            rerr = regex_constants::error_type::error_escape;
            break;

        case ONIGERR_TOO_BIG_BACKREF_NUMBER:
        case ONIGERR_INVALID_BACKREF:
        case ONIGERR_NUMBERED_BACKREF_OR_CALL_NOT_ALLOWED:
            rerr = regex_constants::error_type::error_backref;
            break;

        case ONIGERR_UNMATCHED_CLOSE_PARENTHESIS:
        case ONIGERR_END_PATTERN_WITH_UNMATCHED_PARENTHESIS:
            rerr = regex_constants::error_type::error_paren;
            break;

        case ONIGERR_END_PATTERN_AT_LEFT_BRACE:
            rerr = regex_constants::error_type::error_brace;
            break;

        case ONIGERR_CHAR_CLASS_VALUE_AT_END_OF_RANGE:
        case ONIGERR_CHAR_CLASS_VALUE_AT_START_OF_RANGE:
            rerr = regex_constants::error_type::error_range;
            break;

        case ONIGERR_MEMORY:
            rerr = regex_constants::error_type::error_space;
            break;

        case ONIGERR_TARGET_OF_REPEAT_OPERATOR_NOT_SPECIFIED:
        case ONIGERR_TARGET_OF_REPEAT_OPERATOR_INVALID:
        case ONIGERR_NESTED_REPEAT_OPERATOR:
        case ONIGERR_TOO_BIG_NUMBER_FOR_REPEAT_RANGE:
        case ONIGERR_UPPER_SMALLER_THAN_LOWER_IN_REPEAT_RANGE:
        case ONIGERR_INVALID_REPEAT_RANGE_PATTERN:
            rerr = regex_constants::error_type::error_badrepeat;
            break;

        case ONIGERR_MATCH_STACK_LIMIT_OVER:
            rerr = regex_constants::error_type::error_stack;
            break;

        default:
            rerr = regex_constants::error_type::error_onig;
            break;
    };
    throw regex_error(rerr, oerr);
}

bool prosoft::iregex::rsearch(OnigRegex rx, const uchar* haystack, size_t length, regex_constants::match_flag_type flags, match_handler callback, bool exact) {
    OnigOptionType opt = match_flags_to_onig(flags);
    auto start = reinterpret_cast<const OnigUChar*>(haystack);
    auto end = start + length;
    Region region;
    if (::onig_search(rx, start, end, start, end, region, opt) >= 0) {
        size_t count;
        if (!exact || (count = region->end[0] - region->beg[0]) == length) {
            if (nullptr != callback) {
                _rxparse_results(region, callback);
            }
            return true;
        }
    }

    return false;
}

} // prosoft
