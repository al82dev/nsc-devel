#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/Token.h"
#include "nativesharp/Source/SourceManager.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

// The canonical plan explicitly targets Parser's cursor and lookahead contracts, which have no
// public seam. This test-only view follows the private-access precedent already used by the new
// host suite and changes neither the product header nor its ABI.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "nativesharp/Parser/Parser.h"
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace nsc::suite::parser_support
{
using nativesharp::ast::AstNode;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::Token;
using nativesharp::lexer::TokenKind;
using nativesharp::parser::Parser;

class ParserFixture final
{
public:
    explicit ParserFixture(std::string source, std::vector<std::string> knownTypes = {})
        : knownTypes_(std::move(knownTypes))
    {
        const nativesharp::FileId id = sources_.addBuffer("phase2_parser.ns", std::move(source));
        const nativesharp::source::SourceFile* file = sources_.getFile(id);
        nativesharp::lexer::Lexer lexer(*file, diagnostics_);
        tokens_ = lexer.lexAll();
        parser_ = std::make_unique<Parser>(tokens_, arena_, diagnostics_, knownTypes_);
    }

    AstNode* parse()
    {
        return parser_->parseCompilationUnit();
    }

    [[nodiscard]] Parser& parser() noexcept
    {
        return *parser_;
    }

    [[nodiscard]] const std::vector<Token>& tokens() const noexcept
    {
        return tokens_;
    }

    [[nodiscard]] nativesharp::diagnostics::DiagnosticEngine& diagnostics() noexcept
    {
        return diagnostics_;
    }

private:
    nativesharp::source::SourceManager sources_;
    nativesharp::diagnostics::DiagnosticEngine diagnostics_;
    nativesharp::ast::AstArena arena_;
    std::vector<std::string> knownTypes_;
    std::vector<Token> tokens_;
    std::unique_ptr<Parser> parser_;
};

inline std::size_t diagnosticCount(
    const nativesharp::diagnostics::DiagnosticEngine& diagnostics,
    DiagnosticId id)
{
    std::size_t count = 0;
    for (const auto& diagnostic : diagnostics.diagnostics())
    {
        count += diagnostic.id() == id ? 1U : 0U;
    }
    return count;
}

inline void addParserCase(
    TestRegistry& registry,
    std::string id,
    std::string contract,
    std::string layer,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{std::move(id), std::move(contract), "Host.Parser", std::move(layer),
        std::move(profile), {"parser", "foundation"}, std::move(function)});
}
}
