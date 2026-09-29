#include "nativesharp/LanguageService/LanguageService.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
int failures = 0;

void expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}

int main()
{
    using namespace nativesharp::language_service;
    LanguageService service;
    const std::filesystem::path path = "A:/formatting/Example.ns";
    const std::string source =
        "namespace Demo {  \r\n"
        "class Example {\r\n"
        "void Run() {\r\n"
        "if (true) {\r\n"
        "return;   \r\n"
        "}\r\n"
        "}\r\n"
        "}\r\n"
        "}\r\n";
    std::string error;
    expect(service.openDocument(path, source, 1, error), "document opens for formatting");
    const auto edit = service.formatDocument(path, DocumentFormattingOptions{2, true});
    expect(edit.has_value(), "unformatted document produces one full-document edit");
    expect(edit.has_value() && edit->newText ==
        "namespace Demo {\r\n"
        "  class Example {\r\n"
        "    void Run() {\r\n"
        "      if (true) {\r\n"
        "        return;\r\n"
        "      }\r\n"
        "    }\r\n"
        "  }\r\n"
        "}\r\n", "formatting is deterministic and preserves CRLF");
    expect(edit.has_value() && edit->location.range.end == TextPosition{9, 0},
        "full-document edit uses a UTF-16 end position");

    if (failures == 0) std::cout << "PASS: NativeSharp document formatting\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
