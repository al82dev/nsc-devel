#pragma once

#include "ParserFoundationTestSupport.h"

#include "nativesharp/AST/AstNodeKind.h"

#include <string>
#include <utility>

namespace nsc::suite::expression_support
{
using parser_support::ParserFixture;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;

// Parses exactly one expression through the product parser's expression entry point. Complexity is O(n).
inline AstNode* parse(ParserFixture& fixture)
{
    return fixture.parser().parseExpression();
}

// Registers one expression-parser case with canonical metadata. Complexity is O(1).
inline void addCase(
    TestRegistry& registry,
    std::string id,
    std::string contract,
    TestFunction function,
    std::string profile = "PR",
    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), std::move(contract), "Host.Parser", "component",
        std::move(profile), {"parser", "expressions"}, std::move(function), disposition});
}
}
