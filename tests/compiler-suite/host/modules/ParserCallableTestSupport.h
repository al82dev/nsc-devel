#pragma once

#include "ParserDeclarationTestSupport.h"

#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite::parser_callable_support
{
using namespace parser_declaration_support;

inline AstNode* member(const DeclarationFixture& fixture, std::size_t ordinal = 0) noexcept
{
    AstNode* const type = declaration(fixture);
    return type != nullptr && ordinal < type->children().size() ? type->children()[ordinal] : nullptr;
}

inline bool diagnosticContains(const DeclarationFixture& fixture, std::string_view text) noexcept
{
    for (const auto& diagnostic : fixture.diagnostics())
    {
        if (diagnostic.message().find(text) != std::string::npos) return true;
    }
    return false;
}

inline void addCallableCase(
    TestRegistry& registry,
    std::string id,
    std::string contract,
    TestFunction function)
{
    registry.add(TestCase{std::move(id), std::move(contract), "Host.Parser", "component", "PR",
        {"parser", "callables"}, std::move(function)});
}
}
