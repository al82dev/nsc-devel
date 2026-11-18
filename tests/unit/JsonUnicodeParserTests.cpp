#include "../../src/Driver/Internal/Json.h"
#include "../../src/Driver/Internal/JsonParser.h"
#include "../support/TestContext.h"

#include <string>
#include <variant>

namespace
{
bool parsesAs(std::string_view text, std::string_view expected)
{
    nativesharp::driver::Json value;
    std::string error;
    return nativesharp::driver::JsonParser(text).parse(value, error) &&
           std::holds_alternative<std::string>(value.value) &&
           std::get<std::string>(value.value) == expected;
}

bool rejects(std::string_view text)
{
    nativesharp::driver::Json value;
    std::string error;
    return !nativesharp::driver::JsonParser(text).parse(value, error) && !error.empty();
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    tests.expect(parsesAs("\"\\uD83D\\uDE00\"", "\xF0\x9F\x98\x80"),
                 "a valid surrogate pair decodes to one UTF-8 character");
    tests.expect(parsesAs("\"\\uD800\\uDC00\"", "\xF0\x90\x80\x80") &&
                 parsesAs("\"\\uDBFF\\uDFFF\"", "\xF4\x8F\xBF\xBF"),
                 "surrogate pair boundaries decode correctly");
    tests.expect(parsesAs("\"A\\u00e9\\u20ac\"", "A\xC3\xA9\xE2\x82\xAC"),
                 "ASCII and ordinary BMP escapes remain unchanged");
    tests.expect(rejects("\"\\uD800\"") && rejects("\"\\uDC00\"") &&
                 rejects("\"\\uD800\\u0041\"") && rejects("\"\\uD800\\uDG00\""),
                 "unpaired or malformed surrogates are rejected");
    return tests.result();
}
