// Copyright © 2014-2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

using regex = basic_regex<traits>;
using rx_match_results = basic_match_results<traits>;
using rx_iterator = basic_regex_iterator<traits>;
using string_type = traits::string_type;

regex rx;

SECTION("create") {
    static PS_CONSTEXPR const auto rx_default_flags = regex_constants::syntax_option_type::ruby;
    CHECK(rx.flags() == rx_default_flags);
    // NULL is used instead of nullptr because MSVC considers nullptr ambiguous for operator<<().
    CHECK(rx.crx() == NULL);
    
    const auto s = string("abcd");
    rx = s;
    CHECK(rx.crx() != NULL);
 
    regex rx2(string("abcd"));
    CHECK(rx2.flags() == rx_default_flags);
    CHECK(rx2.crx() != NULL);
    
    auto crx = rx2.crx();
    auto flags = rx2.flags();
    auto pat = rx2.pattern();
    
    regex rx3(std::move(rx2));
    CHECK(crx == rx3.crx());
    CHECK(flags == rx3.flags());
    CHECK(pat == rx3.pattern());
    CHECK(rx2.crx() == NULL);
    CHECK(rx2.flags() == rx_default_flags);
    CHECK(rx2.pattern().size() == 0);
    
    regex rx4(string("ABCD"), regex::icase);
    CHECK(rx4.flags() == regex::icase);
    CHECK(rx4.crx() != NULL);
    CHECK(rx4.pattern() == string("ABCD"));
    
    // operator= are just aliases for assign
    CHECK_FALSE(rx4.flags() == rx3.flags());
    CHECK_FALSE(rx4.pattern() == rx3.pattern());
    rx4 = rx3;
    CHECK(rx4.flags() == rx3.flags());
    CHECK(rx4.pattern() == rx3.pattern());
    CHECK_FALSE(rx4.crx() == rx3.crx());
    CHECK(rx4.crx() != NULL);
    
    rx4.assign(string("ABCD"), regex::icase);
    CHECK_FALSE(rx4.flags() == rx3.flags());
    CHECK_FALSE(rx4.pattern() == rx3.pattern());
    CHECK(rx4.flags() == regex::icase);
    CHECK(rx4.crx() != NULL);
    CHECK(rx4.pattern() == string("ABCD"));
    
    crx = rx4.crx();
    flags = rx4.flags();
    pat = rx4.pattern();
    rx3 = std::move(rx4);
    CHECK(crx == rx3.crx());
    CHECK(flags == rx3.flags());
    CHECK(pat == rx3.pattern());
    CHECK(rx4.crx() == NULL);
    CHECK(rx4.flags() == rx_default_flags);
    CHECK(rx4.pattern().size() == 0);
    
    swap(rx4, rx3);
    CHECK(crx == rx4.crx());
    CHECK(flags == rx4.flags());
    CHECK(pat == rx4.pattern());
    CHECK(rx3.crx() == NULL);
    CHECK(rx3.flags() == rx_default_flags);
    CHECK(rx3.pattern().size() == 0);
}

SECTION("match") {
    rx = string("abcd");
    CHECK_FALSE(regex_match(string_type{}, rx));
    CHECK_FALSE(regex_search(string_type{}, rx));
    CHECK(regex_match(string("abcd"), rx));
    
    rx_match_results results;
    CHECK_FALSE(regex_match(string_type{}, results, rx));
    CHECK_FALSE(regex_search(string_type{}, results, rx));
    CHECK(regex_match(string("abcd"), results, rx));
    CHECK(results.size() == 1);
    
    CHECK_FALSE(regex_match(string("ABCD"), rx));
    
    CHECK_FALSE(regex_match(string("abcde"), rx));
    CHECK(regex_search(string("abcde"), rx));
    
    rx.assign(rx.pattern(), regex::icase);
    CHECK(regex_match(string("ABCD"), rx));
    CHECK_FALSE(regex_match(string("ABCDE"), rx));
    CHECK(regex_search(string("ABCDE"), rx));
    
    rx = string("^abcd$");
    CHECK(regex_match(string("abcd"), rx));
    CHECK(regex_search(string("abcd"), rx));
    CHECK_FALSE(regex_search(string("xabcd"), rx));
    CHECK_FALSE(regex_search(string("abcdx"), rx));
    CHECK(regex_search(string("x\nabcd"), rx));

    rx = string("/Library/Audio/.+");
    CHECK(regex_match(string("/Library/Audio/Apple Loops/Apple/Apple Loops for GarageBand/Cop Show Clav 07.aif"), rx));
    
    rx = string("[A-Z,0-9]{4,5}-[A-Z,0-9]{4,5}-[A-Z,0-9]{4,5}-[A-Z,0-9]{4,5}-[A-Z,0-9]{5}");
    CHECK(regex_match(string("H7C1-0LAH-3144-11G1-32650"), rx));
    CHECK(regex_match(string("HKSN1-K3HG4-91FTV-N14TP-1X9DA"), rx));

    rx = string(".*fran" "\xC3\xA7" "ais:");
    const auto haystack = string("Wikip" "\xCC\xA9" "dia en fran" "\xC3\xA7" "ais:");
    CHECK(regex_match(haystack, rx));
}

SECTION("anchors") {
    rx = string("^abcd");
    CHECK(regex_match(string("abcd"), rx));
    CHECK_FALSE(regex_match(string("dcba\nabcd"), rx));
    CHECK(regex_search(string("dcba\nabcd"), rx));
    
    CHECK_FALSE(regex_match(string("abcd"), rx, regex_constants::match_flag_type::match_not_bol));
    CHECK_FALSE(regex_match(string("dcba\nabcd"), rx, regex_constants::match_flag_type::match_not_bol));
    CHECK_FALSE(regex_search(string("abcd"), rx, regex_constants::match_flag_type::match_not_bol));
    CHECK(regex_search(string("dcba\nabcd"), rx, regex_constants::match_flag_type::match_not_bol));
    
    rx.assign(rx.pattern(), regex_constants::syntax_option_type::noendl);
    // ^ now means "beginning of string"
    CHECK(regex_match(string("abcd"), rx));
    CHECK(regex_match(string("abcd"), rx, regex_constants::match_flag_type::match_not_bol));
    // newlines are ignored so this now fails
    CHECK_FALSE(regex_search(string("dcba\nabcd"), rx, regex_constants::match_flag_type::match_not_bol));
    
    rx = string("abcd$");
    CHECK(regex_match(string("abcd"), rx));
    CHECK_FALSE(regex_match(string("abcd"), rx, regex_constants::match_flag_type::match_not_eol));
    CHECK(regex_search(string("abcd\ndcba"), rx, regex_constants::match_flag_type::match_not_eol));
    
    rx.assign(rx.pattern(), regex_constants::syntax_option_type::noendl);
    // $ now means "end of string" which is still equivalent to eol in this case
    CHECK(regex_match(string("abcd"), rx));
    CHECK_FALSE(regex_match(string("abcd"), rx, regex_constants::match_flag_type::match_not_eol));
    // newlines are ignored so this now fails
    CHECK_FALSE(regex_search(string("abcd\ndcba"), rx, regex_constants::match_flag_type::match_not_eol));
}

SECTION("capture") {
    auto haystack = string("Scrooge refused to pay the author his known rate.");
    rx = string("(efused)|(uthor)|(known)");
    rx_match_results results;
    CHECK(regex_search(haystack, results, rx));
    //std::copy(results.begin(), results.end(), std::ostream_iterator<sub_match>(std::cout,"\n"));
    REQUIRE(results.size() == 2); // match and 1 capture
    // REQUIRE will stop the section here if the test fails so that we don't crash accessing invalid elements
    CHECK(results[0] == string("efused"));
    CHECK(results[1] == string("efused"));
    
    rx = string(".*(efused).*(uthor).*(known).*");
    CHECK(regex_match(haystack, results, rx));
    REQUIRE(results.size() == 4); // match and 3 captures
    CHECK(results[1] == string("efused"));
    CHECK(results[2] == string("uthor"));
    CHECK(results[3] == string("known"));
    
    haystack = string("Eric: 4:40, Karl: 3:35, Francesca: 2:32");
    rx = string("(\\d):(\\d\\d)");
    CHECK(regex_search(haystack, results, rx));
    REQUIRE(results.size() == 3); // match and 2 captures
    CHECK(results[1] == string("4"));
    CHECK(results[2] == string("40"));
}

SECTION("iterators") {
    // URL capture -- https://gist.github.com/gruber/8891611/4d9b5f0f308b17044987df12b3f1f48b43b295f6
    const char* pat = R"PAT((?i)\b((?:https?:(?:/{1,3}|[a-z0-9%])|[a-z0-9.\-]+[.](?:com|net|org|edu|gov|mil|aero|asia|biz|cat|coop|info|int|jobs|mobi|museum|name|post|pro|tel|travel|xxx|ac|ad|ae|af|ag|ai|al|am|an|ao|aq|ar|as|at|au|aw|ax|az|ba|bb|bd|be|bf|bg|bh|bi|bj|bm|bn|bo|br|bs|bt|bv|bw|by|bz|ca|cc|cd|cf|cg|ch|ci|ck|cl|cm|cn|co|cr|cs|cu|cv|cx|cy|cz|dd|de|dj|dk|dm|do|dz|ec|ee|eg|eh|er|es|et|eu|fi|fj|fk|fm|fo|fr|ga|gb|gd|ge|gf|gg|gh|gi|gl|gm|gn|gp|gq|gr|gs|gt|gu|gw|gy|hk|hm|hn|hr|ht|hu|id|ie|il|im|in|io|iq|ir|is|it|je|jm|jo|jp|ke|kg|kh|ki|km|kn|kp|kr|kw|ky|kz|la|lb|lc|li|lk|lr|ls|lt|lu|lv|ly|ma|mc|md|me|mg|mh|mk|ml|mm|mn|mo|mp|mq|mr|ms|mt|mu|mv|mw|mx|my|mz|na|nc|ne|nf|ng|ni|nl|no|np|nr|nu|nz|om|pa|pe|pf|pg|ph|pk|pl|pm|pn|pr|ps|pt|pw|py|qa|re|ro|rs|ru|rw|sa|sb|sc|sd|se|sg|sh|si|sj|Ja|sk|sl|sm|sn|so|sr|ss|st|su|sv|sx|sy|sz|tc|td|tf|tg|th|tj|tk|tl|tm|tn|to|tp|tr|tt|tv|tw|tz|ua|ug|uk|us|uy|uz|va|vc|ve|vg|vi|vn|vu|wf|ws|ye|yt|yu|za|zm|zw)/)(?:[^\s()<>{}\[\]]+|\([^\s()]*?\([^\s()]+\)[^\s()]*?\)|\([^\s]+?\))+(?:\([^\s()]*?\([^\s()]+\)[^\s()]*?\)|\([^\s]+?\)|[^\s`!()\[\]{};:'".,<>?«»“”‘’])|(?:(?<!@)[a-z0-9]+(?:[.\-][a-z0-9]+)*[.](?:com|net|org|edu|gov|mil|aero|asia|biz|cat|coop|info|int|jobs|mobi|museum|name|post|pro|tel|travel|xxx|ac|ad|ae|af|ag|ai|al|am|an|ao|aq|ar|as|at|au|aw|ax|az|ba|bb|bd|be|bf|bg|bh|bi|bj|bm|bn|bo|br|bs|bt|bv|bw|by|bz|ca|cc|cd|cf|cg|ch|ci|ck|cl|cm|cn|co|cr|cs|cu|cv|cx|cy|cz|dd|de|dj|dk|dm|do|dz|ec|ee|eg|eh|er|es|et|eu|fi|fj|fk|fm|fo|fr|ga|gb|gd|ge|gf|gg|gh|gi|gl|gm|gn|gp|gq|gr|gs|gt|gu|gw|gy|hk|hm|hn|hr|ht|hu|id|ie|il|im|in|io|iq|ir|is|it|je|jm|jo|jp|ke|kg|kh|ki|km|kn|kp|kr|kw|ky|kz|la|lb|lc|li|lk|lr|ls|lt|lu|lv|ly|ma|mc|md|me|mg|mh|mk|ml|mm|mn|mo|mp|mq|mr|ms|mt|mu|mv|mw|mx|my|mz|na|nc|ne|nf|ng|ni|nl|no|np|nr|nu|nz|om|pa|pe|pf|pg|ph|pk|pl|pm|pn|pr|ps|pt|pw|py|qa|re|ro|rs|ru|rw|sa|sb|sc|sd|se|sg|sh|si|sj|Ja|sk|sl|sm|sn|so|sr|ss|st|su|sv|sx|sy|sz|tc|td|tf|tg|th|tj|tk|tl|tm|tn|to|tp|tr|tt|tv|tw|tz|ua|ug|uk|us|uy|uz|va|vc|ve|vg|vi|vn|vu|wf|ws|ye|yt|yu|za|zm|zw)\b/?(?!@))))PAT";
    rx.assign(string(pat), regex::icase);
    auto haystack = string("A test URL follows: https://www.prosofteng.com/products/drive_genius_faq.php?act=viewall. And here's another: www.apple.com/itunes.");
    
    rx_iterator i(haystack.cbegin(), haystack.cend(), rx);
    rx_iterator end;
    CHECK(i != end);
    
    REQUIRE(i->size() == 2); // match and 1 capture
    CHECK((*i)[0] == string("https://www.prosofteng.com/products/drive_genius_faq.php?act=viewall"));
    
    ++i;
    CHECK(i != end);
    REQUIRE(i->size() == 2);
    CHECK((*i)[0] == string("www.apple.com/itunes"));
    
    auto j = i++;
    CHECK(j != end);
    CHECK(i == end);
    CHECK((*i).empty());
    
    haystack = string("Wikip" "\xCC\xA9" "dia en fran" "\xC3\xA7" "ais:" "http://fr.wikipedia.org/wiki/. Wikipedia en espa" "\xC3\xB1" "ol: es.wikipedia.org/Energ" "\xC3\xAD" "a_solar_fotovoltaica.");
    i = {haystack.cbegin(), haystack.cend(), rx};
    REQUIRE_FALSE(i->empty());
    CHECK((*i)[0] == string("http://fr.wikipedia.org/wiki/"));
    ++i;
    CHECK(i != end);
    REQUIRE_FALSE(i->empty());
    CHECK((*i)[0] == string("es.wikipedia.org/Energ" "\xC3\xAD" "a_solar_fotovoltaica"));
    ++i;
    CHECK(i == end);
    
    rx = string("abcd");
    haystack = string("zzabcd11abcd22abcd33abcd99");
    int count = 0;
    for (i = {haystack.cbegin(), haystack.cend(), rx}; i != end; ++i) {
        CHECK(i->str() == rx.pattern());
        ++count;
    }
    CHECK(4 == count);
    
    i = {haystack.cbegin(), haystack.cend(), rx};
    j = {haystack.cbegin(), haystack.cend(), rx};
    CHECK(i == j);
}

SECTION("errors") {
    CHECK_THROWS_AS(rx = string("[ab+"), regex_error);
    CHECK_THROWS_AS(rx = string("[ab]+\\"), regex_error);
    CHECK_THROWS_AS(rx = string("(\\d:"), regex_error);
}

SECTION("pattern escape") {
    CHECK(regex::escaped_pattern(string("\\^$.*+")) == string("\\\\\\^\\$\\.\\*\\+"));
}
