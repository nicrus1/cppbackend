#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include "../src/loot_generator.h"

// Более подробная версия тестов с использованием SECTION
TEST_CASE("HtmlDecode comprehensive tests") {
    SECTION("Empty string") {
        CHECK(HtmlDecode("") == "");
    }
    
    SECTION("No entities") {
        CHECK(HtmlDecode("Plain text without any entities") == "Plain text without any entities");
        CHECK(HtmlDecode("1234567890") == "1234567890");
        CHECK(HtmlDecode("!@#$%^&*()") == "!@#$%^&*()");
    }
    
    SECTION("Lowercase entities with semicolon") {
        CHECK(HtmlDecode("&lt;") == "<");
        CHECK(HtmlDecode("&gt;") == ">");
        CHECK(HtmlDecode("&amp;") == "&");
        CHECK(HtmlDecode("&apos;") == "'");
        CHECK(HtmlDecode("&quot;") == "\"");
    }
    
    SECTION("Uppercase entities with semicolon") {
        CHECK(HtmlDecode("&LT;") == "<");
        CHECK(HtmlDecode("&GT;") == ">");
        CHECK(HtmlDecode("&AMP;") == "&");
        CHECK(HtmlDecode("&APOS;") == "'");
        CHECK(HtmlDecode("&QUOT;") == "\"");
    }
    
    SECTION("Lowercase entities without semicolon") {
        CHECK(HtmlDecode("&lt") == "<");
        CHECK(HtmlDecode("&gt") == ">");
        CHECK(HtmlDecode("&amp") == "&");
        CHECK(HtmlDecode("&apos") == "'");
        CHECK(HtmlDecode("&quot") == "\"");
    }
    
    SECTION("Uppercase entities without semicolon") {
        CHECK(HtmlDecode("&LT") == "<");
        CHECK(HtmlDecode("&GT") == ">");
        CHECK(HtmlDecode("&AMP") == "&");
        CHECK(HtmlDecode("&APOS") == "'");
        CHECK(HtmlDecode("&QUOT") == "\"");
    }
    
    SECTION("Mixed case (should not decode)") {
        CHECK(HtmlDecode("&Lt;") == "&Lt;");
        CHECK(HtmlDecode("&Gt;") == "&Gt;");
        CHECK(HtmlDecode("&aPos;") == "&aPos;");
        CHECK(HtmlDecode("&Quot;") == "&Quot;");
    }
    
    SECTION("Unknown entities") {
        CHECK(HtmlDecode("&unknown;") == "&unknown;");
        CHECK(HtmlDecode("&abcdef;") == "&abcdef;");
        CHECK(HtmlDecode("&xyz") == "&xyz");
    }
    
    SECTION("No recursive decoding") {
        CHECK(HtmlDecode("&amp;lt;") == "&lt;");
        CHECK(HtmlDecode("&amp;amp;") == "&amp;");
        CHECK(HtmlDecode("&lt;&amp;gt;") == "<&gt;");
    }
    
    SECTION("Real world examples") {
        CHECK(HtmlDecode("Johnson&amp;Johnson") == "Johnson&Johnson");
        CHECK(HtmlDecode("M&amp;M&APOSs") == "M&M's");
        CHECK(HtmlDecode("&lt;p&gt;Hello&lt;/p&gt;") == "<p>Hello</p>");
        CHECK(HtmlDecode("a &lt; b &amp;&amp; c &gt; d") == "a < b && c > d");
    }
    
    SECTION("Multiple entities in one string") {
        CHECK(HtmlDecode("&lt;&gt;&amp;&apos;&quot;") == "<>&'\"");
        CHECK(HtmlDecode("&lt;&lt;&lt;") == "<<<");
        CHECK(HtmlDecode("&amp;&amp;&amp;") == "&&&");
    }
    
    SECTION("Entities at boundaries") {
        CHECK(HtmlDecode("&lt;start") == "<start");
        CHECK(HtmlDecode("end&amp;") == "end&");
        CHECK(HtmlDecode("&lt;") == "<");
        CHECK(HtmlDecode("&amp;end") == "&end");
    }
}