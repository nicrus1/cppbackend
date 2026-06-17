#include <catch2/catch_test_macros.hpp>
#include "../src/htmldecode.h"

using namespace std::literals;

TEST_CASE("Empty string") {
    REQUIRE(HtmlDecode(""sv) == ""s);
}

TEST_CASE("String without HTML entities") {
    REQUIRE(HtmlDecode("Hello World"sv) == "Hello World"s);
    REQUIRE(HtmlDecode("abc123"sv) == "abc123"s);
    REQUIRE(HtmlDecode("Hello, World!"sv) == "Hello, World!"s);
}

TEST_CASE("String with HTML entities (lowercase)") {
    REQUIRE(HtmlDecode("&lt;"sv) == "<"s);
    REQUIRE(HtmlDecode("&gt;"sv) == ">"s);
    REQUIRE(HtmlDecode("&amp;"sv) == "&"s);
    REQUIRE(HtmlDecode("&apos;"sv) == "'"s);
    REQUIRE(HtmlDecode("&quot;"sv) == "\""s);
    
    REQUIRE(HtmlDecode("Johnson&amp;Johnson"sv) == "Johnson&Johnson"s);
    REQUIRE(HtmlDecode("M&amp;M&apos;s"sv) == "M&M's"s);
}

TEST_CASE("String with HTML entities (uppercase)") {
    REQUIRE(HtmlDecode("&LT;"sv) == "<"s);
    REQUIRE(HtmlDecode("&GT;"sv) == ">"s);
    REQUIRE(HtmlDecode("&AMP;"sv) == "&"s);
    REQUIRE(HtmlDecode("&APOS;"sv) == "'"s);
    REQUIRE(HtmlDecode("&QUOT;"sv) == "\""s);
    
    REQUIRE(HtmlDecode("Johnson&amp;Johnson"sv) == "Johnson&Johnson"s);
    REQUIRE(HtmlDecode("Johnson&AMP;Johnson"sv) == "Johnson&Johnson"s);
}

TEST_CASE("String with HTML entities (mixed case)") {
    REQUIRE(HtmlDecode("&lT;"sv) == "<"s);
    REQUIRE(HtmlDecode("&gT;"sv) == ">"s);
    REQUIRE(HtmlDecode("&aMp;"sv) == "&"s);
    REQUIRE(HtmlDecode("&aPoS;"sv) == "'"s);
    REQUIRE(HtmlDecode("&qUoT;"sv) == "\""s);
}

TEST_CASE("HTML entities without semicolon") {
    REQUIRE(HtmlDecode("&lt"sv) == "<"s);
    REQUIRE(HtmlDecode("&gt"sv) == ">"s);
    REQUIRE(HtmlDecode("&amp"sv) == "&"s);
    REQUIRE(HtmlDecode("&apos"sv) == "'"s);
    REQUIRE(HtmlDecode("&quot"sv) == "\""s);
    
    REQUIRE(HtmlDecode("Johnson&ampJohnson"sv) == "Johnson&Johnson"s);
}

TEST_CASE("HTML entities in different positions") {
    REQUIRE(HtmlDecode("&lt;html"sv) == "<html"s);
    REQUIRE(HtmlDecode("html&gt;"sv) == "html>"s);
    REQUIRE(HtmlDecode("&lt;html&gt;"sv) == "<html>"s);
    REQUIRE(HtmlDecode("&amp; &lt; &gt;"sv) == "& < >"s);
    REQUIRE(HtmlDecode("Start &lt;middle&gt; end"sv) == "Start <middle> end"s);
}

TEST_CASE("Incomplete HTML entities") {
    REQUIRE(HtmlDecode("&"sv) == "&"s);
    REQUIRE(HtmlDecode("&l"sv) == "&l"s);
    REQUIRE(HtmlDecode("&am"sv) == "&am"s);
    REQUIRE(HtmlDecode("&ampl"sv) == "&ampl"s);
    REQUIRE(HtmlDecode("&ltx"sv) == "&ltx"s);
    REQUIRE(HtmlDecode("&lt;x"sv) == "<x"s);
}

TEST_CASE("No recursive decoding") {
    REQUIRE(HtmlDecode("&amp;lt;"sv) == "&lt;"s);
    REQUIRE(HtmlDecode("&amp;gt;"sv) == "&gt;"s);
    REQUIRE(HtmlDecode("&amp;amp;"sv) == "&amp;"s);
    REQUIRE(HtmlDecode("&lt;&amp;gt;"sv) == "<&gt;"s);
    REQUIRE(HtmlDecode("&amp;quot;"sv) == "&quot;"s);
}

TEST_CASE("Complex strings") {
    REQUIRE(HtmlDecode("&lt;div&gt;Hello World&lt;/div&gt;"sv) == "<div>Hello World</div>"s);
    REQUIRE(HtmlDecode("a &amp; b &lt; c &gt; d"sv) == "a & b < c > d"s);
    REQUIRE(HtmlDecode("&quot;Hello, World!&quot;"sv) == "\"Hello, World!\""s);
    REQUIRE(HtmlDecode("&apos;Hello&apos;"sv) == "'Hello'"s);
    REQUIRE(HtmlDecode("&lt;&quot;&amp;&apos;&gt;"sv) == "<\"&'>"s);
}