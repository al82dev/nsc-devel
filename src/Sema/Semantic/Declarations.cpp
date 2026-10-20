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

void SemanticAnalyzer::analyzeGlobal(ast::AstNode* node)
{
    switch (node->kind())
    {
        case ast::AstNodeKind::ClassDeclaration:
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidType,
                "class declarations and instance-method semantics are handled by the Stage 34 project pipeline; "
                "the single-file procedural semantic pipeline does not lower classes");
            break;
        case ast::AstNodeKind::StructDeclaration:
            analyzeStruct(node);
            break;
        case ast::AstNodeKind::EnumDeclaration:
            analyzeEnum(node);
            break;
        case ast::AstNodeKind::ExternCDeclaration:
            analyzeExternC(node);
            break;
        case ast::AstNodeKind::FunctionDeclaration:
            analyzeFunction(node, false);
            break;
        case ast::AstNodeKind::GlobalVariableDeclaration:
            analyzeGlobalVariable(node);
            break;
        default:
            report(node->range(), diagnostics::DiagnosticId::ExpectedDeclaration, "invalid global declaration node");
            break;
    }
}

void SemanticAnalyzer::analyzeStruct(ast::AstNode* node)
{
    Symbol* structSymbol = globalScope_->lookupLocal(node->text());
    if (structSymbol != nullptr && structSymbol->declaration() != node)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::DuplicateSymbol,
            "global name '" + node->text() + "' is already declared");
        reportPreviousDeclaration(structSymbol);
        return;
    }

    if (structSymbol == nullptr)
    {
        Type* type = types_.createStruct(node->text(), node);
        structSymbol = createSymbol(SymbolKind::Struct, node, type, false);
        globalScope_->declare(structSymbol);
    }
    globalSymbols_.push_back(structSymbol);
    Type* structType = structSymbol->type();

    std::vector<Type*> fieldTypes = analyzeStructFields(node, structSymbol);
    structFieldTypes_[structType] = fieldTypes;
    structType->setComplete(true);

    assignStructFieldLayout(structSymbol, fieldTypes);
}

std::vector<Type*> SemanticAnalyzer::analyzeStructFields(
    ast::AstNode* node,
    Symbol* structSymbol)
{
    std::vector<Type*> fieldTypes;
    for (ast::AstNode* fieldNode : node->children())
    {
        if (Symbol* previousField = structSymbol->findMember(fieldNode->text()))
        {
            report(
                fieldNode->range(),
                diagnostics::DiagnosticId::DuplicateSymbol,
                "duplicate field '" + fieldNode->text() + "' in struct '" + node->text() + "'");
            reportPreviousDeclaration(previousField);
            continue;
        }

        const bool functionType =
            fieldNode->typeSyntax() != nullptr && fieldNode->typeSyntax()->isFunction();
        Type* fieldType = functionType
            ? types_.builtin(TypeKind::Error)
            : resolveType(fieldNode->typeSyntax());
        if (fieldType == nullptr)
        {
            fieldType = types_.builtin(TypeKind::Error);
        }

        bool valid = true;
        if (functionType)
        {
            report(
                fieldNode->range(),
                diagnostics::DiagnosticId::InvalidType,
                "struct field cannot have function type");
            valid = false;
        }
        else if (fieldType->kind() == TypeKind::Void)
        {
            report(fieldNode->range(), diagnostics::DiagnosticId::InvalidType, "struct field cannot have type void");
            valid = false;
        }
        else if (!fieldType->isComplete())
        {
            report(
                fieldNode->range(),
                diagnostics::DiagnosticId::IncompleteType,
                "struct field requires a complete type; direct self-containment is not allowed");
            valid = false;
        }

        Symbol* field = createSymbol(SymbolKind::Field, fieldNode, fieldType, false);
        structSymbol->addMember(field);
        fieldTypes.push_back(valid ? fieldType : types_.builtin(TypeKind::Error));
    }
    return fieldTypes;
}

void SemanticAnalyzer::assignStructFieldLayout(
    Symbol* structSymbol,
    const std::vector<Type*>& fieldTypes)
{
    std::vector<target::TypeLayout> physicalFields;
    physicalFields.reserve(fieldTypes.size());
    bool layoutValid = true;
    for (Type* fieldType : fieldTypes)
    {
        const std::optional<target::TypeLayout> fieldLayout = layoutOfType(fieldType);
        if (!fieldLayout.has_value())
        {
            layoutValid = false;
            break;
        }
        physicalFields.push_back(*fieldLayout);
    }

    if (layoutValid)
    {
        const std::optional<target::StructLayout> layout = dataLayout_.layoutStruct(physicalFields);
        if (layout.has_value() && layout->fields.size() == structSymbol->members().size())
        {
            for (std::size_t index = 0; index < layout->fields.size(); ++index)
            {
                structSymbol->members()[index]->setFieldOffset(layout->fields[index].offset);
            }
        }
    }
}

void SemanticAnalyzer::analyzeEnum(ast::AstNode* node)
{
    if (globalScope_->lookupLocal(node->text()) != nullptr)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::DuplicateSymbol,
            "global name '" + node->text() + "' is already declared");
        reportPreviousDeclaration(globalScope_->lookupLocal(node->text()));
        return;
    }

    // `enum E : byte` NO ESTA EN ESTA RUTA, Y SE DICE EN VOZ ALTA.
    //
    // El tipo subyacente declarado lo implementa la ruta de PROYECTO: alli el ancho y el signo del
    // enum salen de una regla unica (include/nativesharp/Frontend/EnumUnderlyingType.h) que el
    // modelo de layout, el generador de cabeceras C y el bajador consultan por igual. Esta ruta de
    // fichero unico tiene su propio modelo de tipos, su propio layout y su propio bajador, y en los
    // tres un enum mide 32 bits y tiene signo.
    //
    // POR QUE RECHAZAR Y NO IMPLEMENTARLO AQUI TAMBIEN. Aceptar la sintaxis y seguir dando 32 bits
    // es la unica salida que este proyecto no admite: seria una RESPUESTA FALSA, y ademas la peor
    // clase, porque el autor escribio el ancho que queria y el compilador dijo que si. Implementarlo
    // seria una segunda copia de la regla --exactamente el defecto que el tipo subyacente vino a
    // cerrar-- en un camino que ninguna fixture del corpus ejercita, o sea codigo sin medir. Queda
    // el rechazo, que es corto, cierto, y dice adonde ir.
    if (node->typeSyntax() != nullptr)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidEnumInitializer,
            "a declared enum underlying type is only supported by the project pipeline; "
            "in the single-file pipeline an enum is always a signed 32-bit integer");
        return;
    }

    Type* enumType = types_.createEnum(node->text(), node);
    Symbol* enumSymbol = createSymbol(SymbolKind::Enum, node, enumType, true);
    globalScope_->declare(enumSymbol);

    Scope* previousScope = currentScope_;
    currentScope_ = createScope(globalScope_);

    analyzeEnumMembers(node, enumSymbol);

    currentScope_ = previousScope;
}

void SemanticAnalyzer::analyzeEnumMembers(ast::AstNode* node, Symbol* enumSymbol)
{
    std::int64_t nextValue = 0;
    for (ast::AstNode* memberNode : node->children())
    {
        if (Symbol* previousMember = enumSymbol->findMember(memberNode->text()))
        {
            report(
                memberNode->range(),
                diagnostics::DiagnosticId::DuplicateSymbol,
                "duplicate enum member '" + memberNode->text() + "'");
            reportPreviousDeclaration(previousMember);
            continue;
        }

        std::int64_t value = nextValue;
        bool valueValid = value >= std::numeric_limits<std::int32_t>::min() &&
            value <= std::numeric_limits<std::int32_t>::max();
        if (memberNode->first() != nullptr)
        {
            ExpressionInfo initializer = analyzeExpression(memberNode->first());
            const std::optional<std::int64_t> constant = constantAsSigned(initializer);
            if (initializer.type->kind() == TypeKind::Error)
            {
                valueValid = false;
            }
            else if (!initializer.isConstant || constant == std::nullopt || !isInteger(initializer.type))
            {
                report(
                    memberNode->range(),
                    diagnostics::DiagnosticId::InvalidEnumInitializer,
                    "enum initializer must be an integer constant expression");
                valueValid = false;
            }
            else if (*constant < std::numeric_limits<std::int32_t>::min() ||
                     *constant > std::numeric_limits<std::int32_t>::max())
            {
                report(
                    memberNode->range(),
                    diagnostics::DiagnosticId::InvalidEnumInitializer,
                    "enum value must be representable as NativeSharp int");
                valueValid = false;
            }
            else
            {
                value = *constant;
                valueValid = true;
            }
        }
        else if (!valueValid)
        {
            report(
                memberNode->range(),
                diagnostics::DiagnosticId::InvalidEnumInitializer,
                "auto-incremented enum value is not representable as NativeSharp int");
        }

        Symbol* member = createSymbol(
            SymbolKind::EnumMember, memberNode, enumSymbol->type(), false);
        member->setEnumValue(value);
        member->setConstantValue(ConstantValue::signedInteger(value));
        enumSymbol->addMember(member);

        // Inside the enum initializer scope, previous members behave as their underlying int constants.
        Symbol* initializerSymbol = new Symbol(
            SymbolKind::EnumMember,
            memberNode->text(),
            types_.builtin(TypeKind::Int),
            memberNode);
        initializerSymbol->setEnumValue(value);
        initializerSymbol->setConstantValue(ConstantValue::signedInteger(value));
        ownedSymbols_.push_back(initializerSymbol);
        currentScope_->declare(initializerSymbol);

        if (value == std::numeric_limits<std::int32_t>::max())
        {
            nextValue = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1;
        }
        else
        {
            nextValue = value + 1;
        }
    }
}

void SemanticAnalyzer::analyzeExternC(ast::AstNode* node)
{
    for (ast::AstNode* function : node->children())
    {
        analyzeFunction(function, true);
    }
}

void SemanticAnalyzer::analyzeFunction(ast::AstNode* node, bool externC)
{
    Type* returnType = resolveType(node->typeSyntax());
    if (returnType == nullptr)
    {
        returnType = types_.builtin(TypeKind::Error);
    }

    if (returnType->kind() == TypeKind::Array)
    {
        report(node->range(), diagnostics::DiagnosticId::InvalidType, "functions cannot return fixed arrays by value");
    }

    std::vector<Type*> parameterTypes = resolveFunctionParameterTypes(node);

    if (externC)
    {
        validateExternCSignature(node, returnType, parameterTypes);
    }

    Type* functionType = types_.functionType(returnType, parameterTypes);
    Symbol* functionSymbol = declareFunctionSymbol(node, functionType, externC);
    if (functionSymbol == nullptr) return;

    if (node->first() == nullptr)
    {
        return;
    }

    if (externC)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidType,
            "extern C functions cannot have a body in NativeSharp 2.0");
        return;
    }

    analyzeFunctionBody(node, returnType, parameterTypes, functionSymbol, externC);
}

std::vector<Type*> SemanticAnalyzer::resolveFunctionParameterTypes(ast::AstNode* node)
{
    std::vector<Type*> parameterTypes;
    parameterTypes.reserve(node->children().size());
    for (ast::AstNode* parameter : node->children())
    {
        Type* parameterType = resolveType(parameter->typeSyntax());
        if (parameterType == nullptr)
        {
            parameterType = types_.builtin(TypeKind::Error);
        }
        if (parameterType->kind() == TypeKind::Void || parameterType->kind() == TypeKind::Array)
        {
            report(
                parameter->range(),
                diagnostics::DiagnosticId::InvalidType,
                "parameter must be a non-void scalar, enum, struct or pointer type");
        }
        else if (!parameterType->isComplete())
        {
            report(parameter->range(), diagnostics::DiagnosticId::IncompleteType,
                "parameter type must be complete");
        }
        parameterTypes.push_back(parameterType);
    }
    return parameterTypes;
}

Symbol* SemanticAnalyzer::declareFunctionSymbol(
    ast::AstNode* node,
    Type* functionType,
    bool externC)
{
    Symbol* existing = globalScope_->lookupLocal(node->text());
    if (existing == nullptr)
    {
        Symbol* functionSymbol = createSymbol(SymbolKind::Function, node, functionType, true);
        functionSymbol->setExternC(externC);
        globalScope_->declare(functionSymbol);
        return functionSymbol;
    }
    if (existing->kind() != SymbolKind::Function ||
        existing->type() != functionType || existing->isExternC() != externC)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::ConflictingFunctionDeclaration,
            "conflicting declaration of function '" + node->text() + "'");
        reportPreviousDeclaration(existing);
        return nullptr;
    }
    model_.bindSymbol(node, existing);
    return existing;
}

void SemanticAnalyzer::analyzeFunctionBody(
    ast::AstNode* node,
    Type* returnType,
    const std::vector<Type*>& parameterTypes,
    Symbol* functionSymbol,
    bool externC)
{
    if (functionSymbol->isDefined())
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::DuplicateFunctionDefinition,
            "function '" + node->text() + "' already has a definition");
        reportPreviousDeclaration(functionSymbol);
        return;
    }
    functionSymbol->setDefined(true);

    if (node->text() == "Main")
    {
        if (externC || returnType->kind() != TypeKind::Int || !parameterTypes.empty())
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidMainSignature,
                "NativeSharp 2.0 Main must have signature int Main() and NativeSharp linkage");
        }
    }

    Scope* previousScope = currentScope_;
    Symbol* previousFunction = currentFunction_;
    currentScope_ = createScope(globalScope_);
    currentFunction_ = functionSymbol;

    for (std::size_t index = 0; index < node->children().size(); ++index)
    {
        ast::AstNode* parameterNode = node->children()[index];
        if (Symbol* previousParameter = currentScope_->lookupLocal(parameterNode->text()))
        {
            report(
                parameterNode->range(),
                diagnostics::DiagnosticId::DuplicateSymbol,
                "duplicate parameter name '" + parameterNode->text() + "'");
            reportPreviousDeclaration(previousParameter);
            continue;
        }
        Symbol* parameter = createSymbol(SymbolKind::Parameter, parameterNode, parameterTypes[index], false);
        declareInCurrent(parameter, parameterNode->range());
    }

    analyzeBlock(node->first(), false);

    if (returnType->kind() != TypeKind::Void && !statementPreventsFallthrough(node->first()))
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::NotAllPathsReturn,
            "not all reachable control paths return a value from function '" + node->text() + "'");
    }

    currentScope_ = previousScope;
    currentFunction_ = previousFunction;
}

void SemanticAnalyzer::analyzeGlobalVariable(ast::AstNode* node)
{
    Type* type = resolveType(node->typeSyntax());
    if (type == nullptr)
    {
        type = types_.builtin(TypeKind::Error);
    }

    validateObjectType(type, node->range(), true);

    ExpressionInfo initializer;
    bool hasInitializer = node->first() != nullptr;
    bool initializerConvertible = false;
    if (hasInitializer)
    {
        initializer = analyzeExpression(node->first());
        initializerConvertible = canImplicitlyConvert(initializer, type);
        if (!initializerConvertible)
        {
            report(
                node->first()->range(),
                diagnostics::DiagnosticId::InvalidConversion,
                "cannot implicitly convert initializer of type '" + initializer.type->displayName() +
                    "' to global type '" + type->displayName() + "'");
        }
        else
        {
            recordImplicitConversion(node, node->first(), initializer, type);
        }

        if (!node->flag() && !initializer.isConstant)
        {
            report(
                node->first()->range(),
                diagnostics::DiagnosticId::GlobalRequiresConstant,
                "global initializer must be a static constant expression");
        }
    }

    if (node->flag() && (!hasInitializer || !initializer.isConstant))
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::ConstRequiresConstant,
            "const global requires a constant initializer");
    }

    Symbol* symbol = createSymbol(SymbolKind::GlobalVariable, node, type, true);
    symbol->setConst(node->flag());
    if (hasInitializer && initializer.isConstant && initializerConvertible)
    {
        symbol->setConstantValue(convertConstant(initializer.constantValue, initializer.type, type));
    }
    declareInCurrent(symbol, node->range());
}


}
