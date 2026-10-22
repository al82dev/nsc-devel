#pragma once

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Lexer/Token.h"
#include "nativesharp/Parser/KnownTypeNameSet.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::source
{
class SourceFile;
}

namespace nativesharp::frontend
{
class ProjectSyntaxUnit final
{
    friend class ProjectCompilation;

public:
    ProjectSyntaxUnit(
        std::size_t projectOrdinal,
        std::size_t sourceOrdinal,
        std::string moduleName,
        FileId fileId,
        bool precompiled = false);

    bool lex(const source::SourceFile& sourceFile, diagnostics::DiagnosticEngine& diagnostics);
    bool parse(
        diagnostics::DiagnosticEngine& diagnostics,
        std::span<const std::string> knownTypeNames = {});
    bool parse(
        diagnostics::DiagnosticEngine& diagnostics,
        const parser::KnownTypeNameSet& sharedKnownTypeNames);
    bool parse(const source::SourceFile& sourceFile, diagnostics::DiagnosticEngine& diagnostics);

    std::size_t projectOrdinal() const noexcept;
    std::size_t sourceOrdinal() const noexcept;
    std::string_view moduleName() const noexcept;
    FileId fileId() const noexcept;
    std::span<const lexer::Token> tokens() const noexcept;
    ast::AstNode* syntaxTree() const noexcept;
    ast::AstArena& arena() noexcept;
    bool isLexicallyValid() const noexcept;
    bool isSyntaxValid() const noexcept;
    bool isPrecompiled() const noexcept;

private:
    std::size_t projectOrdinal_ = 0;
    std::size_t sourceOrdinal_ = 0;
    std::string moduleName_;
    FileId fileId_;
    std::vector<lexer::Token> tokens_;
    std::unique_ptr<ast::AstArena> arena_;
    ast::AstNode* syntaxTree_ = nullptr;
    bool lexicallyValid_ = false;
    bool syntaxValid_ = false;
    bool precompiled_ = false;
};
}
