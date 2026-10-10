#pragma once

#include "nsc_suite/TestRegistry.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite::type_syntax_test
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::SourceRange;
using nativesharp::ast::TypeSyntax;
using nativesharp::lexer::TokenKind;

inline SourceRange range(std::uint32_t begin, std::uint32_t end, std::uint32_t file = 8101)
{
    return SourceRange(SourceLocation(FileId(file), begin), SourceLocation(FileId(file), end));
}

inline TypeSyntax type(std::string name = "T",
                       TokenKind kind = TokenKind::Identifier,
                       SourceRange sourceRange = range(1, 2))
{
    return TypeSyntax(sourceRange, kind, std::move(name));
}

inline void expectRange(TestContext& context,
                        SourceRange actual,
                        SourceRange expected,
                        std::string_view caseId)
{
    context.expect(actual.begin() == expected.begin() && actual.end() == expected.end(),
        std::string(caseId) + " range must match exactly");
}

inline void addCase(TestRegistry& registry,
                    std::string id,
                    std::string contract,
                    std::vector<std::string> tags,
                    TestFunction function,
                    std::string profile = "PR",
                    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), std::move(contract), "Host.AST", "unit", std::move(profile),
        std::move(tags), std::move(function), disposition});
}
}
