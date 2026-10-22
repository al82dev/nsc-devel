#include "nativesharp/Frontend/ProjectSyntaxUnit.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceFile.h"

#include <utility>

namespace nativesharp::frontend
{
ProjectSyntaxUnit::ProjectSyntaxUnit(
    std::size_t projectOrdinal,
    std::size_t sourceOrdinal,
    std::string moduleName,
    FileId fileId,
    bool precompiled)
    : projectOrdinal_(projectOrdinal),
      sourceOrdinal_(sourceOrdinal),
      moduleName_(std::move(moduleName)),
      fileId_(fileId),
      arena_(std::make_unique<ast::AstArena>()),
      precompiled_(precompiled)
{
}

bool ProjectSyntaxUnit::lex(
    const source::SourceFile& sourceFile,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!tokens_.empty() || syntaxTree_ != nullptr)
    {
        return false;
    }

    const std::size_t errorsBefore = diagnostics.errorCount();
    lexer::Lexer lexer(sourceFile, diagnostics);
    tokens_ = lexer.lexAll();
    lexicallyValid_ = diagnostics.errorCount() == errorsBefore;
    return lexicallyValid_;
}

bool ProjectSyntaxUnit::parse(
    diagnostics::DiagnosticEngine& diagnostics,
    std::span<const std::string> knownTypeNames)
{
    if (!lexicallyValid_ || tokens_.empty() || syntaxTree_ != nullptr)
    {
        return false;
    }

    const std::size_t errorsBefore = diagnostics.errorCount();
    parser::Parser parser(tokens_, *arena_, diagnostics, knownTypeNames);
    syntaxTree_ = parser.parseCompilationUnit();
    syntaxValid_ = syntaxTree_ != nullptr && diagnostics.errorCount() == errorsBefore;
    return syntaxValid_;
}

bool ProjectSyntaxUnit::parse(
    diagnostics::DiagnosticEngine& diagnostics,
    const parser::KnownTypeNameSet& sharedKnownTypeNames)
{
    if (!lexicallyValid_ || tokens_.empty() || syntaxTree_ != nullptr)
    {
        return false;
    }

    const std::size_t errorsBefore = diagnostics.errorCount();
    parser::Parser parser(tokens_, *arena_, diagnostics, sharedKnownTypeNames);
    syntaxTree_ = parser.parseCompilationUnit();
    syntaxValid_ = syntaxTree_ != nullptr && diagnostics.errorCount() == errorsBefore;
    return syntaxValid_;
}

bool ProjectSyntaxUnit::parse(
    const source::SourceFile& sourceFile,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!lex(sourceFile, diagnostics))
    {
        return false;
    }
    return parse(diagnostics);
}

std::size_t ProjectSyntaxUnit::projectOrdinal() const noexcept
{
    return projectOrdinal_;
}

std::size_t ProjectSyntaxUnit::sourceOrdinal() const noexcept
{
    return sourceOrdinal_;
}

std::string_view ProjectSyntaxUnit::moduleName() const noexcept
{
    return moduleName_;
}

FileId ProjectSyntaxUnit::fileId() const noexcept
{
    return fileId_;
}

std::span<const lexer::Token> ProjectSyntaxUnit::tokens() const noexcept
{
    return tokens_;
}

ast::AstNode* ProjectSyntaxUnit::syntaxTree() const noexcept
{
    return syntaxTree_;
}

ast::AstArena& ProjectSyntaxUnit::arena() noexcept
{
    return *arena_;
}

bool ProjectSyntaxUnit::isLexicallyValid() const noexcept
{
    return lexicallyValid_;
}

bool ProjectSyntaxUnit::isSyntaxValid() const noexcept
{
    return syntaxValid_;
}

bool ProjectSyntaxUnit::isPrecompiled() const noexcept
{
    return precompiled_;
}

}
