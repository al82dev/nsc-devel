#include "nativesharp/Sema/SemanticAnalyzer.h"

#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "../Internal/SemanticAnalyzerSupport.h"

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

void SemanticAnalyzer::analyzeBlock(ast::AstNode* node, bool createNestedScope)
{
    if (node == nullptr)
    {
        return;
    }

    Scope* previous = currentScope_;
    if (createNestedScope)
    {
        currentScope_ = createScope(currentScope_);
    }

    for (ast::AstNode* statement : node->children())
    {
        analyzeStatement(statement);
    }

    currentScope_ = previous;
}

void SemanticAnalyzer::analyzeStatement(ast::AstNode* node)
{
    switch (node->kind())
    {
        case ast::AstNodeKind::BlockStatement:
            analyzeBlock(node, node->auxiliaryText() != "declaration-list");
            break;
        case ast::AstNodeKind::VariableDeclarationStatement:
            analyzeVariable(node);
            break;
        case ast::AstNodeKind::IfStatement:
            analyzeIf(node);
            break;
        case ast::AstNodeKind::WhileStatement:
            analyzeWhile(node);
            break;
        case ast::AstNodeKind::ForStatement:
            analyzeFor(node);
            break;
        case ast::AstNodeKind::BreakStatement:
            if (loopDepth_ == 0)
            {
                report(node->range(), diagnostics::DiagnosticId::BreakOutsideLoop, "break is only valid inside a loop");
            }
            break;
        case ast::AstNodeKind::ContinueStatement:
            if (loopDepth_ == 0)
            {
                report(
                    node->range(),
                    diagnostics::DiagnosticId::ContinueOutsideLoop,
                    "continue is only valid inside a loop");
            }
            break;
        case ast::AstNodeKind::ReturnStatement:
            analyzeReturn(node);
            break;
        case ast::AstNodeKind::StaticAssertStatement:
            analyzeStaticAssert(node);
            break;
        case ast::AstNodeKind::InlineAssemblyStatement:
            analyzeInlineAssemblyStatement(node);
            break;
        case ast::AstNodeKind::ExpressionStatement:
            analyzeExpression(node->first());
            break;
        default:
            break;
    }
}

void SemanticAnalyzer::analyzeInlineAssemblyStatement(ast::AstNode* node)
{
    for (ast::AstNode* child : node->children())
    {
        if (child->kind() == ast::AstNodeKind::InlineAssemblyInputOperand &&
            child->first() != nullptr)
        {
            const ExpressionInfo input = analyzeExpression(child->first());
            if (child->auxiliaryText() == "i" && input.type != nullptr &&
                input.type->kind() != TypeKind::Error &&
                (!input.isConstant || !isInteger(input.type)))
            {
                report(
                    child->first()->range(),
                    diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
                    "asm immediate input requires a compile-time integral constant");
            }
            else if (child->auxiliaryText() == "m" && input.type != nullptr &&
                input.type->kind() != TypeKind::Error && !input.addressable)
            {
                report(
                    child->first()->range(),
                    diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
                    "asm memory input requires an addressable storage location");
            }
        }
        else if (child->kind() == ast::AstNodeKind::InlineAssemblyOutputOperand &&
            child->first() != nullptr)
        {
            const ExpressionInfo target = analyzeExpression(child->first());
            if (target.type != nullptr && target.type->kind() != TypeKind::Error &&
                !target.modifiable)
            {
                report(
                    child->first()->range(),
                    diagnostics::DiagnosticId::InvalidAssignmentTarget,
                    "asm output must be a modifiable lvalue");
            }
            else if ((child->auxiliaryText() == "=m" ||
                      child->auxiliaryText() == "+m") &&
                target.type != nullptr && target.type->kind() != TypeKind::Error &&
                !target.addressable)
            {
                report(
                    child->first()->range(),
                    diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
                    "asm memory output requires an addressable storage location");
            }
        }
    }
    report(
        node->range(),
        diagnostics::DiagnosticId::InlineAssemblyNotAvailable,
            "integrated assembly is recognized but is not executable until its operand and lowering contracts "
            "are complete");
}

void SemanticAnalyzer::analyzeStaticAssert(ast::AstNode* node)
{
    ExpressionInfo condition = analyzeExpression(node->first());
    if (condition.type->kind() != TypeKind::Bool && condition.type->kind() != TypeKind::Error)
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::InvalidCondition,
            "static_assert condition must have type bool");
        return;
    }
    if (condition.type->kind() == TypeKind::Error)
    {
        return;
    }
    if (!condition.isConstant || condition.constantValue.kind() != ConstantKind::Boolean)
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::ConstRequiresConstant,
            "static_assert condition must be a compile-time constant");
        return;
    }
    if (!condition.constantValue.boolean())
    {
        std::string message = "static assertion failed";
        if (!node->text().empty()) message += ": " + node->text();
        report(node->range(), diagnostics::DiagnosticId::StaticAssertionFailed, std::move(message));
    }
}

void SemanticAnalyzer::analyzeVariable(ast::AstNode* node)
{
    Type* type = resolveType(node->typeSyntax());
    if (type == nullptr)
    {
        type = types_.builtin(TypeKind::Error);
    }
    validateObjectType(type, node->range(), true);

    ExpressionInfo initializer;
    bool initializerConvertible = false;
    if (node->first() != nullptr)
    {
        initializer = analyzeExpression(node->first());
        initializerConvertible = type->kind() != TypeKind::Array && canImplicitlyConvert(initializer, type);
        if (!initializerConvertible)
        {
            report(
                node->first()->range(),
                diagnostics::DiagnosticId::InvalidConversion,
                "cannot initialize local '" + node->text() + "' of type '" + type->displayName() +
                    "' from '" + initializer.type->displayName() + "'");
        }
        else
        {
            recordImplicitConversion(node, node->first(), initializer, type);
        }
    }

    if (node->flag() && (node->first() == nullptr || !initializer.isConstant))
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::ConstRequiresConstant,
            "const local requires a constant expression initializer");
    }

    Symbol* symbol = createSymbol(SymbolKind::Variable, node, type, false);
    symbol->setConst(node->flag());
    if (node->flag() && node->first() != nullptr && initializer.isConstant && initializerConvertible)
    {
        symbol->setConstantValue(convertConstant(initializer.constantValue, initializer.type, type));
    }
    declareInCurrent(symbol, node->range());
}

void SemanticAnalyzer::analyzeIf(ast::AstNode* node)
{
    ExpressionInfo condition = analyzeExpression(node->first());
    if (condition.type->kind() != TypeKind::Bool && condition.type->kind() != TypeKind::Error)
    {
        report(node->first()->range(), diagnostics::DiagnosticId::InvalidCondition, "if condition must have type bool");
    }
    analyzeBlock(node->second(), true);
    if (node->third() != nullptr)
    {
        if (node->third()->kind() == ast::AstNodeKind::IfStatement)
        {
            analyzeIf(node->third());
        }
        else
        {
            analyzeBlock(node->third(), true);
        }
    }
}

void SemanticAnalyzer::analyzeWhile(ast::AstNode* node)
{
    ExpressionInfo condition = analyzeExpression(node->first());
    if (condition.type->kind() != TypeKind::Bool && condition.type->kind() != TypeKind::Error)
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::InvalidCondition,
            "while condition must have type bool");
    }

    ++loopDepth_;
    analyzeBlock(node->second(), true);
    --loopDepth_;
}

void SemanticAnalyzer::analyzeFor(ast::AstNode* node)
{
    Scope* previous = currentScope_;
    currentScope_ = createScope(currentScope_);

    if (node->first() != nullptr)
    {
        if (node->first()->kind() == ast::AstNodeKind::VariableDeclarationStatement)
        {
            analyzeVariable(node->first());
        }
        else if (node->first()->kind() == ast::AstNodeKind::BlockStatement &&
                 node->first()->auxiliaryText() == "declaration-list")
        {
            analyzeBlock(node->first(), false);
        }
        else
        {
            analyzeExpression(node->first());
        }
    }

    if (node->second() != nullptr)
    {
        ExpressionInfo condition = analyzeExpression(node->second());
        if (condition.type->kind() != TypeKind::Bool && condition.type->kind() != TypeKind::Error)
        {
            report(
                node->second()->range(),
                diagnostics::DiagnosticId::InvalidCondition,
                "for condition must have type bool");
        }
    }

    if (node->third() != nullptr)
    {
        analyzeExpression(node->third());
    }

    ++loopDepth_;
    if (!node->children().empty())
    {
        analyzeBlock(node->children().front(), true);
    }
    --loopDepth_;

    currentScope_ = previous;
}

void SemanticAnalyzer::analyzeReturn(ast::AstNode* node)
{
    if (currentFunction_ == nullptr)
    {
        report(node->range(), diagnostics::DiagnosticId::InvalidReturn, "return is not inside a function");
        return;
    }

    Type* returnType = currentFunction_->type()->returnType();
    if (returnType->kind() == TypeKind::Void)
    {
        if (node->first() != nullptr)
        {
            report(node->range(), diagnostics::DiagnosticId::InvalidReturn, "void function cannot return a value");
        }
        return;
    }

    if (node->first() == nullptr)
    {
        report(node->range(), diagnostics::DiagnosticId::InvalidReturn, "non-void function must return a value");
        return;
    }

    ExpressionInfo value = analyzeExpression(node->first());
    if (!canImplicitlyConvert(value, returnType))
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::InvalidConversion,
            "cannot convert return expression from '" + value.type->displayName() + "' to '" +
                returnType->displayName() + "'");
    }
    else
    {
        recordImplicitConversion(node, node->first(), value, returnType);
    }
}


}
