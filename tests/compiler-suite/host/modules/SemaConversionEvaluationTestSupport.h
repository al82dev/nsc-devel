#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Sema/ConversionInfo.h"
#include "nativesharp/Sema/SemanticDumper.h"
#include "nativesharp/Sema/SemanticModel.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolDumper.h"
#include "nativesharp/Sema/TypeContext.h"
#include "nativesharp/Target/TargetDataLayout.h"

// The Phase-2 component suite needs direct observation of the legacy host analyzer helpers.
// This changes access only in this test translation unit; the product declaration is untouched.
#define private public
#include "nativesharp/Sema/SemanticAnalyzer.h"
#undef private

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite::sema_conversion_evaluation_support
{
using nativesharp::SourceRange;
using nativesharp::ast::AstArena;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using nativesharp::ast::OwnershipQualifierSyntaxKind;
using nativesharp::ast::TypeModifierSyntax;
using nativesharp::ast::TypeModifierSyntaxKind;
using nativesharp::ast::TypeSyntax;
using nativesharp::diagnostics::DiagnosticEngine;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::TokenKind;
using nativesharp::sema::ConstantKind;
using nativesharp::sema::ConstantValue;
using nativesharp::sema::ConversionInfo;
using nativesharp::sema::ConversionKind;
using nativesharp::sema::ConversionRole;
using nativesharp::sema::ExpressionInfo;
using nativesharp::sema::OwnershipKind;
using nativesharp::sema::SemanticAnalyzer;
using nativesharp::sema::SemanticDumper;
using nativesharp::sema::SemanticModel;
using nativesharp::sema::Symbol;
using nativesharp::sema::SymbolDumper;
using nativesharp::sema::SymbolKind;
using nativesharp::sema::Type;
using nativesharp::sema::TypeContext;
using nativesharp::sema::TypeKind;
using nativesharp::sema::ValueCategory;
using nativesharp::target::BuiltinLayoutKind;
using nativesharp::target::TargetDataLayout;
using nativesharp::target::TypeLayout;

struct SemaFixture
{
    TypeContext types;
    TargetDataLayout layout = TargetDataLayout::WindowsX64();
    DiagnosticEngine diagnostics;
    SemanticAnalyzer analyzer{types, layout, diagnostics};
    AstArena arena;

    AstNode* node(AstNodeKind kind)
    {
        return arena.make<AstNode>(kind, SourceRange{});
    }

    AstNode* integer(std::uint64_t value, std::string spelling = {})
    {
        AstNode* result = node(AstNodeKind::IntegerLiteralExpression);
        result->setIntegerValue(value);
        result->setText(spelling.empty() ? std::to_string(value) : std::move(spelling));
        return result;
    }

    AstNode* floating(double value, std::string spelling = "0.0")
    {
        AstNode* result = node(AstNodeKind::FloatingLiteralExpression);
        result->setFloatingValue(value);
        result->setText(std::move(spelling));
        return result;
    }

    AstNode* boolean(bool value)
    {
        AstNode* result = node(AstNodeKind::BooleanLiteralExpression);
        result->setTokenKind(value ? TokenKind::KwTrue : TokenKind::KwFalse);
        return result;
    }

    AstNode* nullLiteral()
    {
        return node(AstNodeKind::NullLiteralExpression);
    }

    AstNode* unary(TokenKind operation, AstNode* operand)
    {
        AstNode* result = node(AstNodeKind::UnaryExpression);
        result->setTokenKind(operation);
        result->setFirst(operand);
        return result;
    }

    AstNode* binary(TokenKind operation, AstNode* left, AstNode* right)
    {
        AstNode* result = node(AstNodeKind::BinaryExpression);
        result->setTokenKind(operation);
        result->setFirst(left);
        result->setSecond(right);
        return result;
    }

    TypeSyntax* typeSyntax(TokenKind keyword, std::string name = {})
    {
        return arena.make<TypeSyntax>(SourceRange{}, keyword, std::move(name));
    }

    void initializeGlobalScope()
    {
        if (analyzer.globalScope_ == nullptr)
        {
            analyzer.globalScope_ = analyzer.createScope(nullptr);
            analyzer.currentScope_ = analyzer.globalScope_;
        }
    }
};

inline bool hasDiagnostic(const DiagnosticEngine& engine, DiagnosticId id)
{
    return std::any_of(engine.diagnostics().begin(), engine.diagnostics().end(),
        [id](const auto& diagnostic) { return diagnostic.id() == id; });
}

inline ExpressionInfo signedConstant(Type* type, std::int64_t value)
{
    ExpressionInfo result;
    result.type = type;
    result.isConstant = true;
    result.constantValue = ConstantValue::signedInteger(value);
    return result;
}

inline ExpressionInfo unsignedConstant(Type* type, std::uint64_t value)
{
    ExpressionInfo result;
    result.type = type;
    result.isConstant = true;
    result.constantValue = ConstantValue::unsignedInteger(value);
    return result;
}

inline ExpressionInfo floatingConstant(Type* type, double value)
{
    ExpressionInfo result;
    result.type = type;
    result.isConstant = true;
    result.constantValue = ConstantValue::floating(value);
    return result;
}

inline ExpressionInfo booleanConstant(Type* type, bool value)
{
    ExpressionInfo result;
    result.type = type;
    result.isConstant = true;
    result.constantValue = ConstantValue::boolean(value);
    return result;
}

inline void addSemaCase(
    TestRegistry& registry,
    std::string id,
    std::string contract,
    std::string layer,
    TestFunction function,
    std::string profile = "PR",
    TestDisposition disposition = TestDisposition::Required,
    std::vector<std::string> tags = {})
{
    tags.push_back("sema");
    tags.push_back("phase-2");
    registry.add(TestCase{std::move(id), std::move(contract), "Host.Sema", std::move(layer),
        std::move(profile), std::move(tags), std::move(function), disposition});
}

inline std::vector<Type*> allRepresentativeTypes(SemaFixture& fixture)
{
    std::vector<Type*> result;
    for (TypeKind kind : {TypeKind::Error, TypeKind::Void, TypeKind::Bool, TypeKind::Byte, TypeKind::Char,
             TypeKind::Short, TypeKind::UShort, TypeKind::Int, TypeKind::UInt, TypeKind::Long, TypeKind::ULong,
             TypeKind::NInt, TypeKind::NUInt, TypeKind::Float, TypeKind::Double, TypeKind::NullLiteral})
    {
        result.push_back(fixture.types.builtin(kind));
    }
    Type* intType = fixture.types.builtin(TypeKind::Int);
    result.push_back(fixture.types.pointerTo(intType));
    result.push_back(fixture.types.arrayOf(intType, 3));
    Type* structure = fixture.types.createStruct("Record", nullptr);
    structure->setComplete(true);
    fixture.analyzer.structFieldTypes_[structure] = {intType};
    result.push_back(structure);
    result.push_back(fixture.types.createEnum("Choice", nullptr));
    result.push_back(fixture.types.functionType(fixture.types.builtin(TypeKind::Void), {}));
    result.push_back(fixture.types.stringLiteral(4));
    return result;
}
}
