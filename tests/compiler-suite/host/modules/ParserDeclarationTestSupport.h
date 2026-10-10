#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite::parser_declaration_support
{
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::TokenKind;

class DeclarationFixture final
{
public:
    explicit DeclarationFixture(std::string source, std::vector<std::string> knownTypes = {})
    {
        const nativesharp::FileId id = sources_.addBuffer("phase2_parser_declaration.ns", std::move(source));
        const nativesharp::source::SourceFile* file = sources_.getFile(id);
        nativesharp::lexer::Lexer lexer(*file, diagnostics_);
        tokens_ = lexer.lexAll();
        nativesharp::parser::Parser parser(tokens_, arena_, diagnostics_, knownTypes);
        root_ = parser.parseCompilationUnit();
    }

    [[nodiscard]] AstNode* root() const noexcept { return root_; }
    [[nodiscard]] const auto& diagnostics() const noexcept { return diagnostics_.diagnostics(); }

    [[nodiscard]] std::size_t diagnosticCount(DiagnosticId id) const noexcept
    {
        return static_cast<std::size_t>(std::ranges::count_if(
            diagnostics(), [id](const auto& diagnostic) { return diagnostic.id() == id; }));
    }

private:
    nativesharp::source::SourceManager sources_;
    nativesharp::diagnostics::DiagnosticEngine diagnostics_;
    nativesharp::ast::AstArena arena_;
    std::vector<nativesharp::lexer::Token> tokens_;
    AstNode* root_ = nullptr;
};

inline AstNode* declaration(const DeclarationFixture& fixture, std::size_t index = 0) noexcept
{
    return fixture.root() != nullptr && index < fixture.root()->children().size()
        ? fixture.root()->children()[index]
        : nullptr;
}

inline AstNode* childNamed(const AstNode* node, std::string_view name) noexcept
{
    if (node == nullptr) return nullptr;
    const auto iterator = std::ranges::find_if(
        node->children(), [name](const AstNode* child) { return child != nullptr && child->text() == name; });
    return iterator == node->children().end() ? nullptr : *iterator;
}

inline bool hasMarker(const AstNode* node, std::string_view marker) noexcept
{
    return node != nullptr && node->auxiliaryText().find(marker) != std::string::npos;
}

inline void addDeclarationCase(
    TestRegistry& registry,
    std::string id,
    std::string contract,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{std::move(id), std::move(contract), "Host.Parser", "component",
        std::move(profile), {"parser", "declarations"}, std::move(function)});
}
}
