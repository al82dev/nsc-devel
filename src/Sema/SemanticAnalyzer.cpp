#include "nativesharp/Sema/SemanticAnalyzer.h"

#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "Internal/SemanticAnalyzerSupport.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>

namespace nativesharp::sema
{
namespace
{
using lexer::TokenKind;
using semantic_analyzer::bitsFromSigned;
using semantic_analyzer::isNegativeConstant;
using semantic_analyzer::maskForWidth;
using semantic_analyzer::signedFromBits;
using semantic_analyzer::underlyingBinaryOperator;
}

SemanticAnalyzer::SemanticAnalyzer(
    TypeContext& types,
    const target::TargetDataLayout& dataLayout,
    diagnostics::DiagnosticEngine& diagnostics)
    : types_(types),
      dataLayout_(dataLayout),
      diagnostics_(diagnostics)
{
    globalScope_ = createScope(nullptr);
    currentScope_ = globalScope_;
}

SemanticAnalyzer::~SemanticAnalyzer()
{
    for (Symbol* symbol : ownedSymbols_)
    {
        delete symbol;
    }

    for (Scope* scope : ownedScopes_)
    {
        delete scope;
    }
}

bool SemanticAnalyzer::analyze(ast::AstNode* compilationUnit)
{
    if (compilationUnit == nullptr || compilationUnit->kind() != ast::AstNodeKind::CompilationUnit)
    {
        return false;
    }

    // Nominal declarations must exist before any field resolves its type. Keep
    // their publication deferred so globalSymbols() retains exact source order.
    std::unordered_set<std::string> encounteredGlobalNames;
    for (ast::AstNode* declaration : compilationUnit->children())
    {
        if (declaration == nullptr || declaration->text().empty() ||
            !encounteredGlobalNames.insert(declaration->text()).second ||
            declaration->kind() != ast::AstNodeKind::StructDeclaration)
        {
            continue;
        }

        Type* type = types_.createStruct(declaration->text(), declaration);
        Symbol* symbol = createSymbol(SymbolKind::Struct, declaration, type, false);
        globalScope_->declare(symbol);
    }

    for (ast::AstNode* declaration : compilationUnit->children())
    {
        analyzeGlobal(declaration);
    }

    return !diagnostics_.hasErrors();
}

bool SemanticAnalyzer::validateExecutableEntryPoint(ast::AstNode* compilationUnit)
{
    Symbol* mainSymbol = globalScope_->lookupLocal("Main");
    if (mainSymbol == nullptr || mainSymbol->kind() != SymbolKind::Function || !mainSymbol->isDefined())
    {
        const SourceRange range = compilationUnit == nullptr ? SourceRange() : compilationUnit->range();
        report(
            range,
            diagnostics::DiagnosticId::MissingMain,
            "executable compilation requires exactly one definition of int Main()");
        return false;
    }

    return !diagnostics_.hasErrors();
}

const SemanticModel& SemanticAnalyzer::model() const noexcept
{
    return model_;
}

std::span<Symbol* const> SemanticAnalyzer::globalSymbols() const noexcept
{
    return globalSymbols_;
}

Symbol* SemanticAnalyzer::createSymbol(
    SymbolKind kind,
    const ast::AstNode* declaration,
    Type* type,
    bool addToGlobals)
{
    Symbol* symbol = new Symbol(kind, declaration->text(), type, declaration);
    ownedSymbols_.push_back(symbol);
    model_.bindSymbol(declaration, symbol);
    if (addToGlobals)
    {
        globalSymbols_.push_back(symbol);
    }
    return symbol;
}

Scope* SemanticAnalyzer::createScope(Scope* parent)
{
    Scope* scope = new Scope(parent);
    ownedScopes_.push_back(scope);
    return scope;
}

bool SemanticAnalyzer::declareInCurrent(Symbol* symbol, SourceRange range)
{
    if (Symbol* existing = currentScope_->lookupLocal(symbol->name()))
    {
        report(
            range,
            diagnostics::DiagnosticId::DuplicateSymbol,
            "duplicate declaration of '" + symbol->name() + "' in the same scope");
        reportPreviousDeclaration(existing);
        return false;
    }

    for (Scope* scope = currentScope_->parent(); scope != nullptr; scope = scope->parent())
    {
        Symbol* visible = scope->lookupLocal(symbol->name());
        if (visible != nullptr && (visible->kind() == SymbolKind::Struct || visible->kind() == SymbolKind::Enum))
        {
            report(
                range,
                diagnostics::DiagnosticId::DuplicateSymbol,
                "value name '" + symbol->name() + "' cannot shadow a visible type name");
            reportPreviousDeclaration(visible);
            return false;
        }
    }

    currentScope_->declare(symbol);
    return true;
}

Symbol* SemanticAnalyzer::lookup(std::string_view name) const noexcept
{
    return currentScope_ == nullptr ? nullptr : currentScope_->lookup(name);
}

}
