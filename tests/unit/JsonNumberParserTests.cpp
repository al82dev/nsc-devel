#include "../../src/Driver/Internal/Json.h"
#include "../../src/Driver/Internal/JsonParser.h"
#include "../support/TestContext.h"

#include <clocale>
#include <cstdio>
#include <string>
#include <variant>

namespace
{
bool parsesAs(std::string_view text, double expected)
{
    nativesharp::driver::Json value;
    std::string error;
    return nativesharp::driver::JsonParser(text).parse(value, error) &&
           std::holds_alternative<double>(value.value) &&
           std::get<double>(value.value) == expected;
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
    tests.expect(parsesAs("1.5", 1.5) && parsesAs("-2.5e+2", -250.0),
                 "JSON numbers parse with decimal points and exponents");
    tests.expect(rejects("1e") && rejects("1-2") && rejects("01"),
                 "incomplete and malformed JSON numbers are rejected");
    tests.expect(rejects("1e309") && rejects("1e-9999"),
                 "JSON numbers outside the double range are rejected");

    const char* previous = std::setlocale(LC_NUMERIC, nullptr);
    const std::string savedLocale = previous != nullptr ? previous : "C";
    const char* commaLocale = std::setlocale(LC_NUMERIC, "de-DE");
    if (commaLocale == nullptr) commaLocale = std::setlocale(LC_NUMERIC, "de_DE.UTF-8");
    if (commaLocale != nullptr && std::string(std::localeconv()->decimal_point) == ",")
        tests.expect(parsesAs("1.5", 1.5) && parsesAs("-2.5e+2", -250.0),
                     "JSON numbers do not depend on the process numeric locale");
    else
        std::fputs("SKIP: comma-decimal locale unavailable\n", stderr);
    std::setlocale(LC_NUMERIC, savedLocale.c_str());
    return tests.result();
}
