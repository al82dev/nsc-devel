#include "nativesharp/Sema/SemanticAnalyzer.h"

#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace nativesharp::sema
{
namespace
{
using lexer::TokenKind;



TokenKind underlyingBinaryOperator(TokenKind kind) noexcept
{
    switch (kind)
    {
        case TokenKind::PlusEqual: return TokenKind::Plus;
        case TokenKind::MinusEqual: return TokenKind::Minus;
        case TokenKind::StarEqual: return TokenKind::Star;
        case TokenKind::SlashEqual: return TokenKind::Slash;
        case TokenKind::PercentEqual: return TokenKind::Percent;
        case TokenKind::AmpEqual: return TokenKind::Amp;
        case TokenKind::PipeEqual: return TokenKind::Pipe;
        case TokenKind::CaretEqual: return TokenKind::Caret;
        case TokenKind::LessLessEqual: return TokenKind::LessLess;
        case TokenKind::GreaterGreaterEqual: return TokenKind::GreaterGreater;
        default: return kind;
    }
}

std::uint64_t maskForWidth(std::uint32_t width) noexcept
{
    return width >= 64 ? std::numeric_limits<std::uint64_t>::max() : ((std::uint64_t{1} << width) - 1);
}

std::int64_t signedFromBits(std::uint64_t bits, std::uint32_t width) noexcept
{
    if (width >= 64)
    {
        return std::bit_cast<std::int64_t>(bits);
    }

    const std::uint64_t mask = maskForWidth(width);
    bits &= mask;
    const std::uint64_t sign = std::uint64_t{1} << (width - 1);
    if ((bits & sign) == 0)
    {
        return static_cast<std::int64_t>(bits);
    }

    const std::uint64_t magnitude = ((~bits) + 1) & mask;
    return -static_cast<std::int64_t>(magnitude);
}

std::uint64_t bitsFromSigned(std::int64_t value, std::uint32_t width) noexcept
{
    return std::bit_cast<std::uint64_t>(value) & maskForWidth(width);
}

bool isNegativeConstant(const ExpressionInfo& info) noexcept
{
    return info.isConstant && info.constantValue.kind() == ConstantKind::SignedInteger &&
        info.constantValue.signedInteger() < 0;
}
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
    if (globalScope_->lookupLocal(node->text()) != nullptr)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::DuplicateSymbol,
            "global name '" + node->text() + "' is already declared");
        reportPreviousDeclaration(globalScope_->lookupLocal(node->text()));
        return;
    }

    Type* structType = types_.createStruct(node->text(), node);
    Symbol* structSymbol = createSymbol(SymbolKind::Struct, node, structType, true);
    globalScope_->declare(structSymbol);

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

        Type* fieldType = resolveType(fieldNode->typeSyntax());
        if (fieldType == nullptr)
        {
            fieldType = types_.builtin(TypeKind::Error);
        }

        bool valid = true;
        if (fieldType->kind() == TypeKind::Void)
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

    structFieldTypes_[structType] = fieldTypes;
    structType->setComplete(true);

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

    Type* enumType = types_.createEnum(node->text(), node);
    Symbol* enumSymbol = createSymbol(SymbolKind::Enum, node, enumType, true);
    globalScope_->declare(enumSymbol);

    Scope* previousScope = currentScope_;
    currentScope_ = createScope(globalScope_);

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
            if (!initializer.isConstant || constant == std::nullopt || !isInteger(initializer.type))
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

        Symbol* member = createSymbol(SymbolKind::EnumMember, memberNode, enumType, false);
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

    currentScope_ = previousScope;
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
            report(parameter->range(), diagnostics::DiagnosticId::IncompleteType, "parameter type must be complete");
        }
        parameterTypes.push_back(parameterType);
    }

    if (externC)
    {
        validateExternCSignature(node, returnType, parameterTypes);
    }

    Type* functionType = types_.functionType(returnType, parameterTypes);
    Symbol* existing = globalScope_->lookupLocal(node->text());
    Symbol* functionSymbol = nullptr;

    if (existing == nullptr)
    {
        functionSymbol = createSymbol(SymbolKind::Function, node, functionType, true);
        functionSymbol->setExternC(externC);
        globalScope_->declare(functionSymbol);
    }
    else if (existing->kind() != SymbolKind::Function ||
             existing->type() != functionType ||
             existing->isExternC() != externC)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::ConflictingFunctionDeclaration,
            "conflicting declaration of function '" + node->text() + "'");
        reportPreviousDeclaration(existing);
        return;
    }
    else
    {
        functionSymbol = existing;
        model_.bindSymbol(node, functionSymbol);
    }

    if (node->first() == nullptr)
    {
        return;
    }

    if (externC)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidType,
            "extern C functions cannot have a body in NativeSharp 0.1");
        return;
    }

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
                "NativeSharp 0.1 Main must have signature int Main() and NativeSharp linkage");
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

        if (!initializer.isConstant)
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
            analyzeBlock(node, true);
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
        case ast::AstNodeKind::ExpressionStatement:
            analyzeExpression(node->first());
            break;
        default:
            break;
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

ExpressionInfo SemanticAnalyzer::analyzeExpression(ast::AstNode* node, bool allowFunctionDesignator)
{
    if (node == nullptr)
    {
        return makeRValue(types_.builtin(TypeKind::Void));
    }

    switch (node->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
            return analyzeIdentifier(node, allowFunctionDesignator);
        case ast::AstNodeKind::ThisExpression:
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidMemberAccess,
                "'this' is available only in the Stage 34 instance-method project pipeline");
            return makeError(node);
        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::FloatingLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
        case ast::AstNodeKind::StringLiteralExpression:
        case ast::AstNodeKind::BooleanLiteralExpression:
        case ast::AstNodeKind::NullLiteralExpression:
            return analyzeLiteral(node);
        case ast::AstNodeKind::UnaryExpression:
        case ast::AstNodeKind::PostfixExpression:
            return analyzeUnary(node);
        case ast::AstNodeKind::BinaryExpression:
            return analyzeBinary(node);
        case ast::AstNodeKind::AssignmentExpression:
            return analyzeAssignment(node);
        case ast::AstNodeKind::CallExpression:
            return analyzeCall(node);
        case ast::AstNodeKind::IndexExpression:
            return analyzeIndex(node);
        case ast::AstNodeKind::MemberExpression:
            return analyzeMember(node, false);
        case ast::AstNodeKind::PointerMemberExpression:
            return analyzeMember(node, true);
        case ast::AstNodeKind::CastExpression:
            return analyzeCast(node);
        case ast::AstNodeKind::SizeofTypeExpression:
            return analyzeSizeof(node, true);
        case ast::AstNodeKind::SizeofExpression:
            return analyzeSizeof(node, false);
        case ast::AstNodeKind::NewExpression:
            return analyzeNew(node);
        case ast::AstNodeKind::DeleteExpression:
            return analyzeDelete(node);
        case ast::AstNodeKind::StackallocExpression:
            return analyzeStackalloc(node);
        default:
            return makeError(node);
    }
}

ExpressionInfo SemanticAnalyzer::analyzeIdentifier(ast::AstNode* node, bool allowFunctionDesignator)
{
    Symbol* symbol = lookup(node->text());
    if (symbol == nullptr)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::UnknownIdentifier,
            "unknown identifier '" + node->text() + "'");
        return makeError(node);
    }

    if (symbol->kind() == SymbolKind::Struct || symbol->kind() == SymbolKind::Enum)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidOperand,
            "type name '" + node->text() + "' cannot be used as a runtime expression");
        return makeError(node);
    }

    if (symbol->kind() == SymbolKind::Function)
    {
        ExpressionInfo info = makeRValue(symbol->type());
        info.symbol = symbol;
        if (!allowFunctionDesignator)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "function name can only be used as the designator of a call in NativeSharp 0.1");
            info.type = types_.builtin(TypeKind::Error);
        }
        return record(node, info);
    }

    ExpressionInfo info;
    info.type = symbol->type();
    info.category = ValueCategory::LValue;
    info.modifiable = !symbol->isConst() && info.type->kind() != TypeKind::Array;
    info.addressable = !symbol->isConst();
    info.symbol = symbol;

    if (symbol->kind() == SymbolKind::EnumMember)
    {
        info.category = ValueCategory::RValue;
        info.modifiable = false;
        info.addressable = false;
        info.isConstant = true;
        info.constantValue = ConstantValue::signedInteger(symbol->enumValue());
    }
    else if (symbol->isConst() && symbol->hasConstantValue())
    {
        info.isConstant = true;
        info.constantValue = symbol->constantValue();
    }

    return record(node, info);
}

ExpressionInfo SemanticAnalyzer::analyzeLiteral(ast::AstNode* node)
{
    ExpressionInfo info;
    info.category = ValueCategory::RValue;
    info.isConstant = true;

    switch (node->kind())
    {
        case ast::AstNodeKind::IntegerLiteralExpression:
        {
            const std::string& text = node->text();
            bool unsignedSuffix = false;
            bool longSuffix = false;
            for (char value : text)
            {
                if (value == 'u' || value == 'U') unsignedSuffix = true;
                if (value == 'l' || value == 'L') longSuffix = true;
            }

            const std::uint64_t value = node->integerValue();
            if (unsignedSuffix && longSuffix)
            {
                info.type = types_.builtin(TypeKind::ULong);
                info.constantValue = ConstantValue::unsignedInteger(value);
            }
            else if (unsignedSuffix)
            {
                if (value <= std::numeric_limits<std::uint32_t>::max())
                {
                    info.type = types_.builtin(TypeKind::UInt);
                }
                else
                {
                    info.type = types_.builtin(TypeKind::ULong);
                }
                info.constantValue = ConstantValue::unsignedInteger(value);
            }
            else if (longSuffix)
            {
                if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                {
                    report(
                        node->range(),
                        diagnostics::DiagnosticId::InvalidNumericLiteral,
                        "L-suffixed integer literal must fit in NativeSharp long");
                    return makeError(node);
                }
                info.type = types_.builtin(TypeKind::Long);
                info.constantValue = ConstantValue::signedInteger(static_cast<std::int64_t>(value));
            }
            else if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
            {
                info.type = types_.builtin(TypeKind::Int);
                info.constantValue = ConstantValue::signedInteger(static_cast<std::int64_t>(value));
            }
            else if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            {
                info.type = types_.builtin(TypeKind::Long);
                info.constantValue = ConstantValue::signedInteger(static_cast<std::int64_t>(value));
            }
            else
            {
                report(
                    node->range(),
                    diagnostics::DiagnosticId::InvalidNumericLiteral,
                    "unsuffixed integer literal does not fit in NativeSharp long; use an unsigned suffix");
                return makeError(node);
            }
            break;
        }

        case ast::AstNodeKind::FloatingLiteralExpression:
            info.type = (!node->text().empty() &&
                         (node->text().back() == 'f' || node->text().back() == 'F'))
                ? types_.builtin(TypeKind::Float)
                : types_.builtin(TypeKind::Double);
            info.constantValue = ConstantValue::floating(
                info.type->kind() == TypeKind::Float
                    ? static_cast<double>(static_cast<float>(node->floatingValue()))
                    : node->floatingValue());
            break;

        case ast::AstNodeKind::CharacterLiteralExpression:
            info.type = types_.builtin(TypeKind::Char);
            info.constantValue = ConstantValue::unsignedInteger(node->integerValue());
            break;

        case ast::AstNodeKind::StringLiteralExpression:
            info.type = types_.stringLiteral(static_cast<std::uint64_t>(node->text().size()) + 1);
            info.constantValue = ConstantValue::stringValue(node->text());
            break;

        case ast::AstNodeKind::BooleanLiteralExpression:
            info.type = types_.builtin(TypeKind::Bool);
            info.constantValue = ConstantValue::boolean(node->tokenKind() == TokenKind::KwTrue);
            break;

        case ast::AstNodeKind::NullLiteralExpression:
            info.type = types_.builtin(TypeKind::NullLiteral);
            info.constantValue = ConstantValue::nullValue();
            break;

        default:
            return makeError(node);
    }

    return record(node, info);
}

ExpressionInfo SemanticAnalyzer::analyzeUnary(ast::AstNode* node)
{
    ExpressionInfo operand = analyzeExpression(node->first());
    if (operand.type->kind() == TypeKind::Error)
    {
        return makeError(node);
    }

    const TokenKind operation = node->tokenKind();
    ExpressionInfo result;

    if (operation == TokenKind::Amp)
    {
        if (!operand.addressable)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidAddressOf,
                "operand of & must be addressable and non-const");
            return makeError(node);
        }
        result = makeRValue(types_.pointerTo(operand.type));
        recordOperator(node, operation, operand.type, nullptr, operand.type, result.type, true);
        return record(node, result);
    }

    if (operation == TokenKind::Star)
    {
        if (!isPointer(operand.type) || operand.type->elementType()->kind() == TypeKind::Void ||
            !operand.type->elementType()->isComplete())
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidDereference,
                "operand of * must point to a complete non-void type");
            return makeError(node);
        }
        result.type = operand.type->elementType();
        result.category = ValueCategory::LValue;
        result.modifiable = result.type->kind() != TypeKind::Array;
        result.addressable = true;
        recordOperator(node, operation, operand.type, nullptr, operand.type, result.type, true);
        return record(node, result);
    }

    if (operation == TokenKind::PlusPlus || operation == TokenKind::MinusMinus)
    {
        if (!operand.modifiable || !isScalarIncrementType(operand.type))
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                "increment/decrement requires a modifiable integer, floating or non-void pointer lvalue");
            return makeError(node);
        }
        result = makeRValue(operand.type);
        recordOperator(node, operation, operand.type, nullptr, operand.type, result.type, true);
        return record(node, result);
    }

    if (operation == TokenKind::Bang)
    {
        if (operand.type->kind() != TypeKind::Bool)
        {
            report(node->range(), diagnostics::DiagnosticId::InvalidOperand, "operator ! requires bool operand");
            return makeError(node);
        }
        result = makeRValue(types_.builtin(TypeKind::Bool));
        recordOperator(node, operation, operand.type, nullptr, operand.type, result.type, true);
        return record(node, evaluateConstantUnary(node, operand, result));
    }

    if (operation == TokenKind::Tilde)
    {
        if (!isInteger(operand.type))
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "operator ~ requires non-bool integer operand");
            return makeError(node);
        }
        result = makeRValue(promoteInteger(operand.type));
        recordIntegralPromotion(node, node->first(), operand.type, result.type, 0);
        recordOperator(node, operation, operand.type, nullptr, result.type, result.type, true);
        return record(node, evaluateConstantUnary(node, operand, result));
    }

    if (operation == TokenKind::Plus || operation == TokenKind::Minus)
    {
        if (!isNumeric(operand.type))
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "unary numeric operator requires numeric operand");
            return makeError(node);
        }
        result = makeRValue(isInteger(operand.type) ? promoteInteger(operand.type) : operand.type);
        if (isInteger(operand.type))
        {
            recordIntegralPromotion(node, node->first(), operand.type, result.type, 0);
        }
        recordOperator(node, operation, operand.type, nullptr, result.type, result.type, true);
        return record(node, evaluateConstantUnary(node, operand, result));
    }

    return makeError(node);
}

ExpressionInfo SemanticAnalyzer::analyzeBinary(ast::AstNode* node)
{
    const TokenKind operation = node->tokenKind();
    ExpressionInfo left = analyzeExpression(node->first());

    const bool shortCircuit = left.isConstant && left.constantValue.kind() == ConstantKind::Boolean &&
        ((operation == TokenKind::AmpAmp && !left.constantValue.boolean()) ||
         (operation == TokenKind::PipePipe && left.constantValue.boolean()));

    if (shortCircuit)
    {
        ++constantTrapSuppressionDepth_;
    }
    ExpressionInfo right = analyzeExpression(node->second());
    if (shortCircuit)
    {
        --constantTrapSuppressionDepth_;
    }

    if (left.type->kind() == TypeKind::Error || right.type->kind() == TypeKind::Error)
    {
        return makeError(node);
    }

    ExpressionInfo result;

    if (operation == TokenKind::AmpAmp || operation == TokenKind::PipePipe)
    {
        if (left.type->kind() != TypeKind::Bool || right.type->kind() != TypeKind::Bool)
        {
            report(node->range(), diagnostics::DiagnosticId::InvalidOperand, "logical operators require bool operands");
            return makeError(node);
        }
        result = makeRValue(types_.builtin(TypeKind::Bool));
        recordOperator(node, operation, left.type, right.type, types_.builtin(TypeKind::Bool), result.type, false);
        if (shortCircuit)
        {
            result.isConstant = true;
            result.constantValue = ConstantValue::boolean(operation == TokenKind::PipePipe);
            return record(node, result);
        }
        return record(node, evaluateConstantBinary(node, left, right, result));
    }

    if (operation == TokenKind::EqualEqual || operation == TokenKind::BangEqual)
    {
        bool valid = false;
        if (isNumeric(left.type) && isNumeric(right.type) && commonNumericType(left.type, right.type) != nullptr)
        {
            valid = true;
        }
        else if (left.type->kind() == TypeKind::Bool && right.type->kind() == TypeKind::Bool)
        {
            valid = true;
        }
        else if (left.type->kind() == TypeKind::Enum && left.type == right.type)
        {
            valid = true;
        }
        else if ((isPointer(left.type) && isPointer(right.type) &&
                  (left.type == right.type || left.type->elementType()->kind() == TypeKind::Void ||
                   right.type->elementType()->kind() == TypeKind::Void)) ||
                 (isPointer(left.type) && right.type->kind() == TypeKind::NullLiteral) ||
                 (isPointer(right.type) && left.type->kind() == TypeKind::NullLiteral))
        {
            valid = true;
        }

        if (!valid)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "operands are not compatible for equality comparison");
            return makeError(node);
        }
        result = makeRValue(types_.builtin(TypeKind::Bool));
        Type* operationType = left.type;
        if (isNumeric(left.type) && isNumeric(right.type))
        {
            operationType = commonNumericType(left.type, right.type);
            recordImplicitConversion(node, node->first(), left, operationType, 0);
            recordImplicitConversion(node, node->second(), right, operationType, 1);
        }
        else if (isPointer(left.type) && right.type->kind() == TypeKind::NullLiteral)
        {
            recordImplicitConversion(node, node->second(), right, left.type, 1);
        }
        else if (isPointer(right.type) && left.type->kind() == TypeKind::NullLiteral)
        {
            recordImplicitConversion(node, node->first(), left, right.type, 0);
            operationType = right.type;
        }
        else if (isPointer(left.type) && isPointer(right.type) && left.type != right.type)
        {
            Type* voidPointer = left.type->elementType()->kind() == TypeKind::Void ? left.type : right.type;
            if (left.type != voidPointer)
            {
                recordImplicitConversion(node, node->first(), left, voidPointer, 0);
            }
            if (right.type != voidPointer)
            {
                recordImplicitConversion(node, node->second(), right, voidPointer, 1);
            }
            operationType = voidPointer;
        }
        recordOperator(node, operation, left.type, right.type, operationType, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }

    if (operation == TokenKind::Less || operation == TokenKind::LessEqual ||
        operation == TokenKind::Greater || operation == TokenKind::GreaterEqual)
    {
        if (!isNumeric(left.type) || !isNumeric(right.type) || commonNumericType(left.type, right.type) == nullptr)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "relational operators require compatible numeric operands");
            return makeError(node);
        }
        result = makeRValue(types_.builtin(TypeKind::Bool));
        Type* common = commonNumericType(left.type, right.type);
        recordImplicitConversion(node, node->first(), left, common, 0);
        recordImplicitConversion(node, node->second(), right, common, 1);
        recordOperator(node, operation, left.type, right.type, common, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }

    if (operation == TokenKind::Amp || operation == TokenKind::Pipe || operation == TokenKind::Caret)
    {
        Type* common = commonIntegralType(left.type, right.type);
        if (common == nullptr)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "bitwise operators require compatible integer operands");
            return makeError(node);
        }
        result = makeRValue(common);
        recordImplicitConversion(node, node->first(), left, common, 0);
        recordImplicitConversion(node, node->second(), right, common, 1);
        recordOperator(node, operation, left.type, right.type, common, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }

    if (operation == TokenKind::LessLess || operation == TokenKind::GreaterGreater)
    {
        if (!isInteger(left.type) || !isInteger(right.type))
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "shift operators require integer operands");
            return makeError(node);
        }
        result = makeRValue(promoteInteger(left.type));
        recordIntegralPromotion(node, node->first(), left.type, result.type, 0);
        recordOperator(node, operation, left.type, right.type, result.type, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }

    if (operation == TokenKind::Plus || operation == TokenKind::Minus)
    {
        if (isPointer(left.type) && isInteger(right.type) && left.type->elementType()->kind() != TypeKind::Void &&
            left.type->elementType()->isComplete())
        {
            recordOperator(node, operation, left.type, right.type, left.type, left.type, false);
            return record(node, makeRValue(left.type));
        }
        if (operation == TokenKind::Plus && isInteger(left.type) && isPointer(right.type) &&
            right.type->elementType()->kind() != TypeKind::Void && right.type->elementType()->isComplete())
        {
            recordOperator(node, operation, left.type, right.type, right.type, right.type, false);
            return record(node, makeRValue(right.type));
        }
        if (operation == TokenKind::Minus && isPointer(left.type) && isPointer(right.type) && left.type == right.type &&
            left.type->elementType()->kind() != TypeKind::Void)
        {
            Type* resultType = types_.builtin(TypeKind::NInt);
            recordOperator(node, operation, left.type, right.type, left.type, resultType, false);
            return record(node, makeRValue(resultType));
        }
    }

    if (operation == TokenKind::Percent)
    {
        Type* common = commonIntegralType(left.type, right.type);
        if (common == nullptr)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "operator % requires compatible integer operands");
            return makeError(node);
        }
        result = makeRValue(common);
        recordImplicitConversion(node, node->first(), left, common, 0);
        recordImplicitConversion(node, node->second(), right, common, 1);
        recordOperator(node, operation, left.type, right.type, common, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }

    if (operation == TokenKind::Plus || operation == TokenKind::Minus || operation == TokenKind::Star ||
        operation == TokenKind::Slash)
    {
        Type* common = commonNumericType(left.type, right.type);
        if (common == nullptr)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "arithmetic operands do not have a common numeric type");
            return makeError(node);
        }
        result = makeRValue(common);
        recordImplicitConversion(node, node->first(), left, common, 0);
        recordImplicitConversion(node, node->second(), right, common, 1);
        recordOperator(node, operation, left.type, right.type, common, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }

    return makeError(node);
}

ExpressionInfo SemanticAnalyzer::analyzeAssignment(ast::AstNode* node)
{
    ExpressionInfo left = analyzeExpression(node->first());
    ExpressionInfo right = analyzeExpression(node->second());
    if (!left.modifiable)
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            "left side of assignment must be a modifiable lvalue");
        return makeError(node);
    }

    if (node->tokenKind() == TokenKind::Equal)
    {
        if (!canImplicitlyConvert(right, left.type))
        {
            report(
                node->second()->range(),
                diagnostics::DiagnosticId::InvalidConversion,
                "cannot implicitly convert '" + right.type->displayName() + "' to '" + left.type->displayName() + "'");
            return makeError(node);
        }
        recordImplicitConversion(node, node->second(), right, left.type, 1);
        recordOperator(node, node->tokenKind(), left.type, right.type, left.type, left.type, false);
        return record(node, makeRValue(left.type));
    }

    const TokenKind binaryOperation = underlyingBinaryOperator(node->tokenKind());
    bool valid = false;

    if (isPointer(left.type) && (binaryOperation == TokenKind::Plus || binaryOperation == TokenKind::Minus))
    {
        valid = isInteger(right.type) && left.type->elementType()->kind() != TypeKind::Void &&
            left.type->elementType()->isComplete();
    }
    else if (binaryOperation == TokenKind::Percent || binaryOperation == TokenKind::Amp ||
             binaryOperation == TokenKind::Pipe || binaryOperation == TokenKind::Caret)
    {
        valid = commonIntegralType(left.type, right.type) != nullptr;
    }
    else if (binaryOperation == TokenKind::LessLess || binaryOperation == TokenKind::GreaterGreater)
    {
        valid = isInteger(left.type) && isInteger(right.type);
    }
    else if (binaryOperation == TokenKind::Plus || binaryOperation == TokenKind::Minus ||
             binaryOperation == TokenKind::Star || binaryOperation == TokenKind::Slash)
    {
        valid = commonNumericType(left.type, right.type) != nullptr;
    }

    if (!valid)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidOperand,
            "invalid operand types for compound assignment");
        return makeError(node);
    }

    Type* operationType = left.type;
    if (!isPointer(left.type))
    {
        operationType = (binaryOperation == TokenKind::LessLess || binaryOperation == TokenKind::GreaterGreater)
            ? promoteInteger(left.type)
            : (binaryOperation == TokenKind::Percent || binaryOperation == TokenKind::Amp ||
                       binaryOperation == TokenKind::Pipe || binaryOperation == TokenKind::Caret
                   ? commonIntegralType(left.type, right.type)
                   : commonNumericType(left.type, right.type));
        recordImplicitConversion(node, node->first(), left, operationType, 0);
        if (binaryOperation != TokenKind::LessLess && binaryOperation != TokenKind::GreaterGreater)
        {
            recordImplicitConversion(node, node->second(), right, operationType, 1);
        }
        if (operationType != left.type)
        {
            model_.addConversion(
                node,
                ConversionInfo{
                    classifyExplicitConversion(operationType, left.type),
                    ConversionRole::Result,
                    nullptr,
                    operationType,
                    left.type,
                    -1});
        }
    }
    recordOperator(node, binaryOperation, left.type, right.type, operationType, left.type, false);
    return record(node, makeRValue(left.type));
}

ExpressionInfo SemanticAnalyzer::analyzeCall(ast::AstNode* node)
{
    ExpressionInfo callee = analyzeExpression(node->first(), true);
    if (callee.symbol == nullptr ||
        callee.symbol->kind() != SymbolKind::Function ||
        callee.type->kind() != TypeKind::Function)
    {
        report(node->range(), diagnostics::DiagnosticId::InvalidCall, "call target is not a NativeSharp function");
        return makeError(node);
    }

    const std::vector<Type*>& parameters = callee.type->parameterTypes();
    if (parameters.size() != node->children().size())
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidArgumentCount,
            "function '" + callee.symbol->name() + "' expects " + std::to_string(parameters.size()) +
                " arguments but received " + std::to_string(node->children().size()));
    }

    const std::size_t count = std::min(parameters.size(), node->children().size());
    for (std::size_t index = 0; index < count; ++index)
    {
        ExpressionInfo argument = analyzeExpression(node->children()[index]);
        if (!canImplicitlyConvert(argument, parameters[index]))
        {
            report(
                node->children()[index]->range(),
                diagnostics::DiagnosticId::InvalidConversion,
                "argument " + std::to_string(index + 1) + " cannot convert from '" + argument.type->displayName() +
                    "' to '" + parameters[index]->displayName() + "'");
        }
        else
        {
            recordImplicitConversion(
                node,
                node->children()[index],
                argument,
                parameters[index],
                static_cast<int>(index));
        }
    }

    for (std::size_t index = count; index < node->children().size(); ++index)
    {
        analyzeExpression(node->children()[index]);
    }

    return record(node, makeRValue(callee.type->returnType()));
}

ExpressionInfo SemanticAnalyzer::analyzeIndex(ast::AstNode* node)
{
    ExpressionInfo base = analyzeExpression(node->first());
    ExpressionInfo index = analyzeExpression(node->second());
    if (!isInteger(index.type))
    {
        report(
            node->second()->range(),
            diagnostics::DiagnosticId::InvalidIndex,
            "index expression must have a non-bool integer type");
        return makeError(node);
    }

    Type* element = nullptr;
    bool fixedArray = false;
    std::uint64_t count = 0;
    if (base.type->kind() == TypeKind::Array)
    {
        element = base.type->elementType();
        fixedArray = true;
        count = base.type->count();
    }
    else if (base.type->kind() == TypeKind::StringLiteral)
    {
        element = types_.builtin(TypeKind::Char);
        fixedArray = true;
        count = base.type->count();
    }
    else if (isPointer(base.type) &&
             base.type->elementType()->kind() != TypeKind::Void &&
             base.type->elementType()->isComplete())
    {
        element = base.type->elementType();
    }
    else
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::InvalidIndex,
            "index base must be a fixed array or non-void pointer");
        return makeError(node);
    }

    ExpressionInfo result;
    result.type = element;
    result.category = ValueCategory::LValue;
    result.modifiable = base.type->kind() != TypeKind::StringLiteral && element->kind() != TypeKind::Array;
    result.addressable = true;
    result.requiresBoundsCheck = fixedArray;

    if (fixedArray && index.isConstant)
    {
        const std::optional<std::int64_t> signedValue = constantAsSigned(index);
        const std::optional<std::uint64_t> unsignedValue = constantAsUnsigned(index);
        const bool negative = signedValue.has_value() && *signedValue < 0;
        const bool tooLarge = unsignedValue.has_value() && *unsignedValue >= count;
        if (negative || tooLarge)
        {
            report(
                node->second()->range(),
                diagnostics::DiagnosticId::ConstantIndexOutOfBounds,
                "constant fixed-array index is outside [0, " + std::to_string(count) + ")");
        }
    }

    return record(node, result);
}

ExpressionInfo SemanticAnalyzer::analyzeMember(ast::AstNode* node, bool pointerForm)
{
    if (!pointerForm && node->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        Symbol* qualifier = lookup(node->first()->text());
        if (qualifier != nullptr && qualifier->kind() == SymbolKind::Enum)
        {
            Symbol* member = qualifier->findMember(node->text());
            if (member == nullptr)
            {
                report(
                    node->range(),
                    diagnostics::DiagnosticId::UnknownMember,
                    "enum '" + qualifier->name() + "' has no member '" + node->text() + "'");
                return makeError(node);
            }

            ExpressionInfo result = makeRValue(qualifier->type());
            result.isConstant = true;
            result.constantValue = ConstantValue::signedInteger(member->enumValue());
            result.symbol = member;
            return record(node, result);
        }
    }

    ExpressionInfo base = analyzeExpression(node->first());
    Type* structType = base.type;
    if (pointerForm)
    {
        if (!isPointer(base.type) || base.type->elementType()->kind() != TypeKind::Struct ||
            !base.type->elementType()->isComplete())
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidMemberAccess,
                "operator -> requires pointer to complete struct");
            return makeError(node);
        }
        structType = base.type->elementType();
    }
    else if (structType->kind() != TypeKind::Struct)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidMemberAccess,
            "operator . requires a struct value or enum type qualifier");
        return makeError(node);
    }

    Symbol* structSymbol = globalScope_->lookupLocal(structType->name());
    Symbol* field = structSymbol == nullptr ? nullptr : structSymbol->findMember(node->text());
    if (field == nullptr)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::UnknownMember,
            "struct '" + structType->name() + "' has no field '" + node->text() + "'");
        return makeError(node);
    }

    ExpressionInfo result;
    result.type = field->type();
    if (pointerForm || base.category == ValueCategory::LValue)
    {
        result.category = ValueCategory::LValue;
        result.modifiable = pointerForm
            ? field->type()->kind() != TypeKind::Array
            : base.modifiable && field->type()->kind() != TypeKind::Array;
        result.addressable = pointerForm ? true : base.addressable;
    }
    else
    {
        result.category = ValueCategory::RValue;
    }
    result.symbol = field;
    return record(node, result);
}

ExpressionInfo SemanticAnalyzer::analyzeCast(ast::AstNode* node)
{
    Type* destination = resolveType(node->typeSyntax());
    ExpressionInfo source = analyzeExpression(node->first());
    if (destination == nullptr)
    {
        return makeError(node);
    }

    if (destination->kind() == TypeKind::Void)
    {
        recordExplicitConversion(node, node->first(), source.type, destination);
        return record(node, makeRValue(destination));
    }

    if (!canExplicitlyConvert(source.type, destination))
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidConversion,
            "explicit conversion from '" + source.type->displayName() + "' to '" + destination->displayName() +
                "' is not allowed");
        return makeError(node);
    }

    recordExplicitConversion(node, node->first(), source.type, destination);
    ExpressionInfo result = makeRValue(destination);
    if (source.isConstant && isFloating(source.type) && isInteger(destination))
    {
        const double value = source.constantValue.floating();
        const long double truncated = std::trunc(static_cast<long double>(value));
        bool outOfRange = !std::isfinite(value);

        if (!outOfRange && isSignedInteger(destination))
        {
            const std::uint32_t width = integerWidth(destination);
            const long double minimum = width == 64
                ? static_cast<long double>(std::numeric_limits<std::int64_t>::min())
                : -static_cast<long double>(std::uint64_t{1} << (width - 1));
            const long double maximum = width == 64
                ? static_cast<long double>(std::numeric_limits<std::int64_t>::max())
                : static_cast<long double>((std::uint64_t{1} << (width - 1)) - 1);
            outOfRange = truncated < minimum || truncated > maximum;
        }
        else if (!outOfRange)
        {
            const long double maximum = static_cast<long double>(maskForWidth(integerWidth(destination)));
            outOfRange = truncated < 0.0L || truncated > maximum;
        }

        if (outOfRange)
        {
            if (constantTrapSuppressionDepth_ == 0)
            {
                report(
                    node->range(),
                    diagnostics::DiagnosticId::ConstantEvaluationTrap,
                    "constant floating-to-integer conversion would trap at runtime");
                return makeError(node);
            }
            return record(node, result);
        }
    }

    if (source.isConstant &&
        (isNumeric(source.type) || isInteger(source.type) || source.type->kind() == TypeKind::Enum) &&
        (isNumeric(destination) || destination->kind() == TypeKind::Enum))
    {
        result.isConstant = true;
        result.constantValue = convertConstant(source.constantValue, source.type, destination);
    }
    else if (source.isConstant && source.type->kind() == TypeKind::NullLiteral && isPointer(destination))
    {
        result.isConstant = true;
        result.constantValue = ConstantValue::nullValue();
    }
    return record(node, result);
}

ExpressionInfo SemanticAnalyzer::analyzeSizeof(ast::AstNode* node, bool typeForm)
{
    Type* targetType = nullptr;
    if (typeForm)
    {
        targetType = resolveType(node->typeSyntax());
    }
    else
    {
        ++constantTrapSuppressionDepth_;
        targetType = analyzeExpression(node->first(), true).type;
        --constantTrapSuppressionDepth_;
    }

    const std::optional<std::uint64_t> size = sizeOf(targetType);
    if (!size.has_value())
    {
        report(node->range(), diagnostics::DiagnosticId::InvalidSizeof, "sizeof requires a complete sized object type");
        return makeError(node);
    }

    ExpressionInfo result = makeRValue(types_.builtin(TypeKind::NUInt));
    result.isConstant = true;
    result.constantValue = ConstantValue::unsignedInteger(*size);
    return record(node, result);
}

ExpressionInfo SemanticAnalyzer::analyzeNew(ast::AstNode* node)
{
    Type* elementType = resolveType(node->typeSyntax());
    if (!validateObjectType(elementType, node->range(), false) || elementType->kind() == TypeKind::Array)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidAllocationType,
            "new requires complete sized non-void non-array element type");
        return makeError(node);
    }

    if (node->flag())
    {
        ExpressionInfo count = analyzeExpression(node->first());
        if (!isInteger(count.type))
        {
            report(
                node->first()->range(),
                diagnostics::DiagnosticId::InvalidAllocationType,
                "new[] count must have non-bool integer type");
            return makeError(node);
        }
        if (isNegativeConstant(count) && constantTrapSuppressionDepth_ == 0)
        {
            report(
                node->first()->range(),
                diagnostics::DiagnosticId::ConstantEvaluationTrap,
                "negative new[] count is a defined runtime trap and is rejected when constant");
        }
        else if (count.isConstant)
        {
            const std::optional<std::uint64_t> value = constantAsUnsigned(count);
            const std::optional<target::TypeLayout> elementLayout = layoutOfType(elementType);
            if (value.has_value() && *value != 0 && elementLayout.has_value() &&
                !dataLayout_.layoutArray(*elementLayout, *value).has_value() && constantTrapSuppressionDepth_ == 0)
            {
                report(
                    node->first()->range(),
                    diagnostics::DiagnosticId::ConstantEvaluationTrap,
                    "constant new[] byte-size calculation overflows nuint");
            }
        }
    }

    return record(node, makeRValue(types_.pointerTo(elementType)));
}

ExpressionInfo SemanticAnalyzer::analyzeDelete(ast::AstNode* node)
{
    ExpressionInfo operand = analyzeExpression(node->first());
    if (!isPointer(operand.type) && operand.type->kind() != TypeKind::NullLiteral)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidDeleteOperand,
            "delete/delete[] operand must be pointer or null");
        return makeError(node);
    }
    return record(node, makeRValue(types_.builtin(TypeKind::Void)));
}

ExpressionInfo SemanticAnalyzer::analyzeStackalloc(ast::AstNode* node)
{
    Type* elementType = resolveType(node->typeSyntax());
    if (!validateObjectType(elementType, node->range(), false) || elementType->kind() == TypeKind::Array)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidStackalloc,
            "stackalloc requires complete sized non-void non-array element type");
        return makeError(node);
    }

    ExpressionInfo count = analyzeExpression(node->first());
    if (!isInteger(count.type))
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::InvalidStackalloc,
            "stackalloc count must have non-bool integer type");
        return makeError(node);
    }
    if (isNegativeConstant(count) && constantTrapSuppressionDepth_ == 0)
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::ConstantEvaluationTrap,
            "negative stackalloc count is a defined runtime trap and is rejected when constant");
    }
    else if (count.isConstant)
    {
        const std::optional<std::uint64_t> value = constantAsUnsigned(count);
        const std::optional<target::TypeLayout> elementLayout = layoutOfType(elementType);
        if (value.has_value() && *value != 0 && elementLayout.has_value() &&
            !dataLayout_.layoutArray(*elementLayout, *value).has_value() && constantTrapSuppressionDepth_ == 0)
        {
            report(
                node->first()->range(),
                diagnostics::DiagnosticId::ConstantEvaluationTrap,
                "constant stackalloc byte-size calculation overflows nuint");
        }
    }
    return record(node, makeRValue(types_.pointerTo(elementType)));
}

Type* SemanticAnalyzer::resolveType(ast::TypeSyntax* syntax)
{
    if (syntax == nullptr)
    {
        return types_.builtin(TypeKind::Error);
    }

    Type* type = resolveBaseType(syntax);
    if (type == nullptr)
    {
        return types_.builtin(TypeKind::Error);
    }

    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer)
        {
            type = types_.pointerTo(type);
            continue;
        }

        if (type->kind() == TypeKind::Void || type->kind() == TypeKind::Array || !type->isComplete())
        {
            report(
                modifier.range,
                diagnostics::DiagnosticId::InvalidArrayCount,
                "fixed array element type must be complete, non-void and non-array in NativeSharp 0.1");
            type = types_.builtin(TypeKind::Error);
            continue;
        }

        ExpressionInfo countInfo = analyzeExpression(modifier.arrayCount);
        const std::optional<std::int64_t> signedCount = constantAsSigned(countInfo);
        const std::optional<std::uint64_t> unsignedCountValue = constantAsUnsigned(countInfo);
        if (!countInfo.isConstant || !isInteger(countInfo.type) ||
            (signedCount.has_value() && *signedCount <= 0) ||
            !unsignedCountValue.has_value() ||
            *unsignedCountValue == 0)
        {
            report(
                modifier.range,
                diagnostics::DiagnosticId::InvalidArrayCount,
                "fixed array count must be a positive integer constant expression representable as nuint");
            type = types_.builtin(TypeKind::Error);
            continue;
        }

        const std::uint64_t unsignedCount = *unsignedCountValue;
        const std::optional<target::TypeLayout> elementLayout = layoutOfType(type);
        if (!elementLayout.has_value() || !dataLayout_.layoutArray(*elementLayout, unsignedCount).has_value())
        {
            report(
                modifier.range,
                diagnostics::DiagnosticId::InvalidArrayCount,
                "fixed array size overflows the target object-size calculation");
            type = types_.builtin(TypeKind::Error);
            continue;
        }

        type = types_.arrayOf(type, unsignedCount);
    }

    OwnershipKind ownership = OwnershipKind::None;
    switch (syntax->ownershipQualifier())
    {
        case ast::OwnershipQualifierSyntaxKind::None:
            ownership = OwnershipKind::None;
            break;
        case ast::OwnershipQualifierSyntaxKind::Owned:
            ownership = OwnershipKind::Owned;
            break;
        case ast::OwnershipQualifierSyntaxKind::Borrowed:
            ownership = OwnershipKind::Borrowed;
            break;
    }

    return types_.qualified(type, ownership);
}

Type* SemanticAnalyzer::resolveBaseType(ast::TypeSyntax* syntax)
{
    switch (syntax->baseTokenKind())
    {
        case TokenKind::KwVoid: return types_.builtin(TypeKind::Void);
        case TokenKind::KwBool: return types_.builtin(TypeKind::Bool);
        case TokenKind::KwByte: return types_.builtin(TypeKind::Byte);
        case TokenKind::KwChar: return types_.builtin(TypeKind::Char);
        case TokenKind::KwShort: return types_.builtin(TypeKind::Short);
        case TokenKind::KwUShort: return types_.builtin(TypeKind::UShort);
        case TokenKind::KwInt: return types_.builtin(TypeKind::Int);
        case TokenKind::KwUInt: return types_.builtin(TypeKind::UInt);
        case TokenKind::KwLong: return types_.builtin(TypeKind::Long);
        case TokenKind::KwULong: return types_.builtin(TypeKind::ULong);
        case TokenKind::KwNInt: return types_.builtin(TypeKind::NInt);
        case TokenKind::KwNUInt: return types_.builtin(TypeKind::NUInt);
        case TokenKind::KwFloat: return types_.builtin(TypeKind::Float);
        case TokenKind::KwDouble: return types_.builtin(TypeKind::Double);
        case TokenKind::Identifier:
        {
            Symbol* symbol = globalScope_->lookupLocal(syntax->baseName());
            if (symbol == nullptr || (symbol->kind() != SymbolKind::Struct && symbol->kind() != SymbolKind::Enum))
            {
                report(
                    syntax->range(),
                    diagnostics::DiagnosticId::UnknownTypeName,
                    "unknown type name '" + syntax->baseName() + "'");
                return types_.builtin(TypeKind::Error);
            }
            return symbol->type();
        }
        default:
            break;
    }

    report(syntax->range(), diagnostics::DiagnosticId::ExpectedType, "invalid type syntax");
    return types_.builtin(TypeKind::Error);
}

bool SemanticAnalyzer::validateObjectType(Type* type, SourceRange range, bool allowArray)
{
    if (type == nullptr || type->kind() == TypeKind::Error)
    {
        return false;
    }
    if (type->kind() == TypeKind::Void)
    {
        report(range, diagnostics::DiagnosticId::InvalidType, "object cannot have type void");
        return false;
    }
    if (!allowArray && type->kind() == TypeKind::Array)
    {
        report(range, diagnostics::DiagnosticId::InvalidType, "array type is not valid in this context");
        return false;
    }
    if (!type->isComplete())
    {
        report(range, diagnostics::DiagnosticId::IncompleteType, "object requires a complete type");
        return false;
    }
    return true;
}

bool SemanticAnalyzer::validateExternCSignature(
    ast::AstNode* functionNode,
    Type* returnType,
    std::span<Type* const> parameterTypes)
{
    bool valid = true;
    if (!isCAbiCompatibleDirectType(returnType, true))
    {
        report(
            functionNode->range(),
            diagnostics::DiagnosticId::InvalidExternCSignature,
            "extern C return type '" + returnType->displayName() + "' is not ABI-compatible in NativeSharp 0.1");
        valid = false;
    }

    for (std::size_t index = 0; index < parameterTypes.size(); ++index)
    {
        Type* parameterType = parameterTypes[index];
        if (!isCAbiCompatibleDirectType(parameterType, false))
        {
            SourceRange range = functionNode->range();
            if (index < functionNode->children().size())
            {
                range = functionNode->children()[index]->range();
            }
            report(
                range,
                diagnostics::DiagnosticId::InvalidExternCSignature,
                "extern C parameter type '" + parameterType->displayName() +
                    "' is not ABI-compatible in NativeSharp 0.1");
            valid = false;
        }
    }
    return valid;
}

bool SemanticAnalyzer::isCAbiCompatibleDirectType(Type* type, bool allowVoid) const
{
    if (type == nullptr || type->kind() == TypeKind::Error)
    {
        return true;
    }
    if (type->kind() == TypeKind::Void)
    {
        return allowVoid;
    }
    if (!type->isComplete())
    {
        return false;
    }

    switch (type->kind())
    {
        case TypeKind::Bool:
        case TypeKind::Byte:
        case TypeKind::Char:
        case TypeKind::Short:
        case TypeKind::UShort:
        case TypeKind::Int:
        case TypeKind::UInt:
        case TypeKind::Long:
        case TypeKind::ULong:
        case TypeKind::NInt:
        case TypeKind::NUInt:
        case TypeKind::Float:
        case TypeKind::Double:
        case TypeKind::Pointer:
        case TypeKind::Enum:
            return true;
        case TypeKind::Struct:
            return isCAbiCompatibleStruct(type);
        default:
            return false;
    }
}

bool SemanticAnalyzer::isCAbiCompatibleStruct(Type* type) const
{
    if (type != nullptr)
    {
        type = type->unqualifiedType();
    }
    if (type == nullptr || type->kind() != TypeKind::Struct || !type->isComplete())
    {
        return false;
    }

    const auto iterator = structFieldTypes_.find(type);
    if (iterator == structFieldTypes_.end() || iterator->second.empty())
    {
        return false;
    }

    for (Type* fieldType : iterator->second)
    {
        if (!isCAbiCompatibleStructField(fieldType))
        {
            return false;
        }
    }
    return layoutOfType(type).has_value();
}

bool SemanticAnalyzer::isCAbiCompatibleStructField(Type* type) const
{
    if (type == nullptr || type->kind() == TypeKind::Error || !type->isComplete())
    {
        return false;
    }

    if (type->kind() == TypeKind::Array)
    {
        return type->count() != 0 && isCAbiCompatibleStructField(type->elementType());
    }
    if (type->kind() == TypeKind::Struct)
    {
        return isCAbiCompatibleStruct(type);
    }
    return isCAbiCompatibleDirectType(type, false);
}

bool SemanticAnalyzer::isInteger(Type* type) const noexcept
{
    if (type == nullptr)
    {
        return false;
    }
    return type->kind() == TypeKind::Byte || type->kind() == TypeKind::Char || type->kind() == TypeKind::Short ||
        type->kind() == TypeKind::UShort || type->kind() == TypeKind::Int || type->kind() == TypeKind::UInt ||
        type->kind() == TypeKind::Long || type->kind() == TypeKind::ULong || type->kind() == TypeKind::NInt ||
        type->kind() == TypeKind::NUInt;
}

bool SemanticAnalyzer::isSignedInteger(Type* type) const noexcept
{
    return type != nullptr && (type->kind() == TypeKind::Short || type->kind() == TypeKind::Int ||
        type->kind() == TypeKind::Long || type->kind() == TypeKind::NInt);
}

bool SemanticAnalyzer::isUnsignedInteger(Type* type) const noexcept
{
    return isInteger(type) && !isSignedInteger(type);
}

bool SemanticAnalyzer::isFloating(Type* type) const noexcept
{
    return type != nullptr && (type->kind() == TypeKind::Float || type->kind() == TypeKind::Double);
}

bool SemanticAnalyzer::isNumeric(Type* type) const noexcept
{
    return isInteger(type) || isFloating(type);
}

bool SemanticAnalyzer::isPointer(Type* type) const noexcept
{
    return type != nullptr && type->kind() == TypeKind::Pointer;
}

bool SemanticAnalyzer::isScalarIncrementType(Type* type) const noexcept
{
    return isNumeric(type) ||
        (isPointer(type) &&
         type->elementType()->kind() != TypeKind::Void &&
         type->elementType()->isComplete());
}

bool SemanticAnalyzer::isSized(Type* type) const noexcept
{
    return sizeOf(type).has_value();
}

std::uint32_t SemanticAnalyzer::integerWidth(Type* type) const noexcept
{
    switch (type->kind())
    {
        case TypeKind::Byte:
        case TypeKind::Char:
            return 8;
        case TypeKind::Short:
        case TypeKind::UShort:
            return 16;
        case TypeKind::Int:
        case TypeKind::UInt:
            return 32;
        case TypeKind::Long:
        case TypeKind::ULong:
        case TypeKind::NInt:
        case TypeKind::NUInt:
            return 64;
        default:
            return 0;
    }
}

Type* SemanticAnalyzer::promoteInteger(Type* type) const noexcept
{
    if (type->kind() == TypeKind::Byte || type->kind() == TypeKind::Char || type->kind() == TypeKind::Short ||
        type->kind() == TypeKind::UShort)
    {
        return types_.builtin(TypeKind::Int);
    }
    return type;
}

Type* SemanticAnalyzer::commonIntegralType(Type* left, Type* right) const noexcept
{
    if (!isInteger(left) || !isInteger(right))
    {
        return nullptr;
    }

    left = promoteInteger(left);
    right = promoteInteger(right);
    if (left == right)
    {
        return left;
    }

    const std::array<TypeKind, 6> candidates = {
        TypeKind::Int,
        TypeKind::UInt,
        TypeKind::Long,
        TypeKind::ULong,
        TypeKind::NInt,
        TypeKind::NUInt};

    for (TypeKind kind : candidates)
    {
        Type* candidate = types_.builtin(kind);
        if (canStandardImplicitConvert(left, candidate) && canStandardImplicitConvert(right, candidate))
        {
            return candidate;
        }
    }
    return nullptr;
}

Type* SemanticAnalyzer::commonNumericType(Type* left, Type* right) const noexcept
{
    if (!isNumeric(left) || !isNumeric(right))
    {
        return nullptr;
    }

    if (left->kind() == TypeKind::Double || right->kind() == TypeKind::Double)
    {
        Type* destination = types_.builtin(TypeKind::Double);
        return canStandardImplicitConvert(left, destination) && canStandardImplicitConvert(right, destination)
            ? destination
            : nullptr;
    }

    if (left->kind() == TypeKind::Float || right->kind() == TypeKind::Float)
    {
        Type* destination = types_.builtin(TypeKind::Float);
        return canStandardImplicitConvert(left, destination) && canStandardImplicitConvert(right, destination)
            ? destination
            : nullptr;
    }

    return commonIntegralType(left, right);
}

ConversionKind SemanticAnalyzer::classifyImplicitConversion(
    const ExpressionInfo& source,
    Type* destination) const noexcept
{
    if (source.type == destination)
    {
        return ConversionKind::Identity;
    }

    if (isInteger(source.type) && destination->kind() == TypeKind::Int &&
        (source.type->kind() == TypeKind::Byte || source.type->kind() == TypeKind::Char ||
         source.type->kind() == TypeKind::Short || source.type->kind() == TypeKind::UShort))
    {
        return ConversionKind::IntegralPromotion;
    }

    if (source.isConstant && isInteger(source.type) && isInteger(destination) &&
        !canStandardImplicitConvert(source.type, destination) && constantFits(source.constantValue, destination))
    {
        return ConversionKind::ImplicitConstantInteger;
    }

    if (source.type->kind() == TypeKind::NullLiteral && isPointer(destination))
    {
        return ConversionKind::ImplicitNullPointer;
    }

    if (source.type->kind() == TypeKind::StringLiteral && isPointer(destination) &&
        destination->elementType()->kind() == TypeKind::Char)
    {
        return ConversionKind::ImplicitStringLiteralToCharPointer;
    }

    if (isPointer(source.type) && isPointer(destination) && destination->elementType()->kind() == TypeKind::Void)
    {
        return ConversionKind::ImplicitPointerToVoid;
    }

    return ConversionKind::ImplicitNumeric;
}

ConversionKind SemanticAnalyzer::classifyExplicitConversion(Type* source, Type* destination) const noexcept
{
    if (source == destination)
    {
        return ConversionKind::Identity;
    }
    if (destination->kind() == TypeKind::Void)
    {
        return ConversionKind::ToVoid;
    }
    if (source->kind() == TypeKind::NullLiteral && isPointer(destination))
    {
        return ConversionKind::ExplicitNullPointer;
    }
    if ((source->kind() == TypeKind::Enum && isInteger(destination)) ||
        (isInteger(source) && destination->kind() == TypeKind::Enum))
    {
        return ConversionKind::ExplicitEnum;
    }
    if ((isPointer(source) && (destination->kind() == TypeKind::NInt || destination->kind() == TypeKind::NUInt)) ||
        (isPointer(destination) && (source->kind() == TypeKind::NInt || source->kind() == TypeKind::NUInt)))
    {
        return ConversionKind::ExplicitPointerInteger;
    }
    if (isPointer(source) && isPointer(destination))
    {
        return ConversionKind::ExplicitPointer;
    }
    return ConversionKind::ExplicitNumeric;
}

void SemanticAnalyzer::recordImplicitConversion(
    const ast::AstNode* owner,
    const ast::AstNode* operand,
    const ExpressionInfo& source,
    Type* destination,
    int operandIndex)
{
    if (owner == nullptr ||
        operand == nullptr ||
        source.type == nullptr ||
        destination == nullptr ||
        source.type == destination ||
        source.type->kind() == TypeKind::Error || destination->kind() == TypeKind::Error)
    {
        return;
    }

    model_.addConversion(
        owner,
        ConversionInfo{
            classifyImplicitConversion(source, destination),
            operandIndex >= 0 ? ConversionRole::Operand : ConversionRole::Contextual,
            operand,
            source.type,
            destination,
            operandIndex});
}

void SemanticAnalyzer::recordIntegralPromotion(
    const ast::AstNode* owner,
    const ast::AstNode* operand,
    Type* source,
    Type* destination,
    int operandIndex)
{
    if (owner == nullptr || operand == nullptr || source == nullptr || destination == nullptr || source == destination)
    {
        return;
    }

    model_.addConversion(
        owner,
        ConversionInfo{
            ConversionKind::IntegralPromotion,
            ConversionRole::Operand,
            operand,
            source,
            destination,
            operandIndex});
}

void SemanticAnalyzer::recordExplicitConversion(
    const ast::AstNode* owner,
    const ast::AstNode* operand,
    Type* source,
    Type* destination)
{
    if (owner == nullptr || operand == nullptr || source == nullptr || destination == nullptr)
    {
        return;
    }

    model_.addConversion(
        owner,
        ConversionInfo{
            classifyExplicitConversion(source, destination),
            ConversionRole::Contextual,
            operand,
            source,
            destination,
            0});
}

void SemanticAnalyzer::recordOperator(
    const ast::AstNode* node,
    lexer::TokenKind tokenKind,
    Type* leftType,
    Type* rightType,
    Type* operationType,
    Type* resultType,
    bool unary)
{
    if (node == nullptr)
    {
        return;
    }

    model_.setOperator(
        node,
        ResolvedOperatorInfo{tokenKind, leftType, rightType, operationType, resultType, unary});
}

bool SemanticAnalyzer::canImplicitlyConvert(const ExpressionInfo& source, Type* destination) const
{
    if (source.type == destination || source.type->kind() == TypeKind::Error || destination->kind() == TypeKind::Error)
    {
        return true;
    }

    if (source.isConstant &&
        isInteger(source.type) &&
        isInteger(destination) &&
        constantFits(source.constantValue, destination))
    {
        return true;
    }

    if (source.type->kind() == TypeKind::NullLiteral && isPointer(destination))
    {
        return true;
    }

    if (source.type->kind() == TypeKind::StringLiteral && isPointer(destination) &&
        destination->elementType()->kind() == TypeKind::Char)
    {
        return true;
    }

    if (isPointer(source.type) && isPointer(destination) && destination->elementType()->kind() == TypeKind::Void)
    {
        return true;
    }

    return canStandardImplicitConvert(source.type, destination);
}

bool SemanticAnalyzer::canStandardImplicitConvert(Type* source, Type* destination) const noexcept
{
    if (source == destination)
    {
        return true;
    }

    switch (source->kind())
    {
        case TypeKind::Byte:
        case TypeKind::Char:
            return destination->kind() == TypeKind::Short || destination->kind() == TypeKind::UShort ||
                destination->kind() == TypeKind::Int || destination->kind() == TypeKind::UInt ||
                destination->kind() == TypeKind::Long || destination->kind() == TypeKind::ULong ||
                destination->kind() == TypeKind::NInt || destination->kind() == TypeKind::NUInt ||
                destination->kind() == TypeKind::Float || destination->kind() == TypeKind::Double;
        case TypeKind::Short:
            return destination->kind() == TypeKind::Int || destination->kind() == TypeKind::Long ||
                destination->kind() == TypeKind::NInt || destination->kind() == TypeKind::Float ||
                destination->kind() == TypeKind::Double;
        case TypeKind::UShort:
            return destination->kind() == TypeKind::Int || destination->kind() == TypeKind::UInt ||
                destination->kind() == TypeKind::Long || destination->kind() == TypeKind::ULong ||
                destination->kind() == TypeKind::NInt || destination->kind() == TypeKind::NUInt ||
                destination->kind() == TypeKind::Float || destination->kind() == TypeKind::Double;
        case TypeKind::Int:
            return destination->kind() == TypeKind::Long || destination->kind() == TypeKind::NInt ||
                destination->kind() == TypeKind::Float || destination->kind() == TypeKind::Double;
        case TypeKind::UInt:
            return destination->kind() == TypeKind::Long || destination->kind() == TypeKind::ULong ||
                destination->kind() == TypeKind::NUInt || destination->kind() == TypeKind::Float ||
                destination->kind() == TypeKind::Double;
        case TypeKind::Long:
        case TypeKind::ULong:
        case TypeKind::NInt:
        case TypeKind::NUInt:
            return destination->kind() == TypeKind::Float || destination->kind() == TypeKind::Double;
        case TypeKind::Float:
            return destination->kind() == TypeKind::Double;
        default:
            return false;
    }
}

bool SemanticAnalyzer::canExplicitlyConvert(Type* source, Type* destination) const noexcept
{
    if (source == destination)
    {
        return true;
    }
    if (destination->kind() == TypeKind::Void)
    {
        return true;
    }
    if (source->kind() == TypeKind::Bool || destination->kind() == TypeKind::Bool)
    {
        return false;
    }
    if (isNumeric(source) && isNumeric(destination))
    {
        return true;
    }
    if ((source->kind() == TypeKind::Enum && isInteger(destination)) ||
        (isInteger(source) && destination->kind() == TypeKind::Enum))
    {
        return true;
    }
    if (isPointer(source) && isPointer(destination))
    {
        return true;
    }
    if ((isPointer(source) && (destination->kind() == TypeKind::NInt || destination->kind() == TypeKind::NUInt)) ||
        (isPointer(destination) && (source->kind() == TypeKind::NInt || source->kind() == TypeKind::NUInt)))
    {
        return true;
    }
    if (source->kind() == TypeKind::NullLiteral && isPointer(destination))
    {
        return true;
    }
    return false;
}

bool SemanticAnalyzer::constantFits(const ConstantValue& value, Type* destination) const noexcept
{
    if (!isInteger(destination))
    {
        return false;
    }

    const std::uint32_t width = integerWidth(destination);
    if (value.kind() == ConstantKind::SignedInteger)
    {
        const std::int64_t source = value.signedInteger();
        if (isSignedInteger(destination))
        {
            if (width == 64) return true;
            const std::int64_t minimum = -(std::int64_t{1} << (width - 1));
            const std::int64_t maximum = (std::int64_t{1} << (width - 1)) - 1;
            return source >= minimum && source <= maximum;
        }
        if (source < 0)
        {
            return false;
        }
        const std::uint64_t maximum = maskForWidth(width);
        return static_cast<std::uint64_t>(source) <= maximum;
    }

    if (value.kind() == ConstantKind::UnsignedInteger)
    {
        const std::uint64_t source = value.unsignedInteger();
        if (isUnsignedInteger(destination))
        {
            return source <= maskForWidth(width);
        }
        const std::uint64_t maximum = width == 64
            ? static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
            : (std::uint64_t{1} << (width - 1)) - 1;
        return source <= maximum;
    }

    return false;
}

ConstantValue SemanticAnalyzer::convertConstant(
    const ConstantValue& value,
    Type* source,
    Type* destination) const
{
    if (destination->kind() == TypeKind::Enum)
    {
        destination = types_.builtin(TypeKind::Int);
    }
    if (source->kind() == TypeKind::Enum)
    {
        source = types_.builtin(TypeKind::Int);
    }

    if (isInteger(source) && isInteger(destination))
    {
        std::uint64_t bits = 0;
        if (value.kind() == ConstantKind::SignedInteger)
        {
            bits = bitsFromSigned(value.signedInteger(), integerWidth(source));
        }
        else
        {
            bits = value.unsignedInteger();
        }
        bits &= maskForWidth(integerWidth(destination));
        return isSignedInteger(destination)
            ? ConstantValue::signedInteger(signedFromBits(bits, integerWidth(destination)))
            : ConstantValue::unsignedInteger(bits);
    }

    if (isInteger(source) && isFloating(destination))
    {
        const double number = value.kind() == ConstantKind::SignedInteger
            ? static_cast<double>(value.signedInteger())
            : static_cast<double>(value.unsignedInteger());
        return ConstantValue::floating(
            destination->kind() == TypeKind::Float
                ? static_cast<double>(static_cast<float>(number))
                : number);
    }

    if (isFloating(source) && isFloating(destination))
    {
        const double number = value.floating();
        return ConstantValue::floating(
            destination->kind() == TypeKind::Float
                ? static_cast<double>(static_cast<float>(number))
                : number);
    }

    if (isFloating(source) && isInteger(destination))
    {
        const double truncated = std::trunc(value.floating());
        if (isSignedInteger(destination))
        {
            return ConstantValue::signedInteger(static_cast<std::int64_t>(truncated));
        }
        return ConstantValue::unsignedInteger(static_cast<std::uint64_t>(truncated));
    }

    return value;
}

std::optional<target::TypeLayout> SemanticAnalyzer::layoutOfType(Type* type) const
{
    if (type != nullptr)
    {
        type = type->unqualifiedType();
    }
    if (type == nullptr || !type->isComplete())
    {
        return std::nullopt;
    }

    using target::BuiltinLayoutKind;
    switch (type->kind())
    {
        case TypeKind::Bool: return dataLayout_.layoutOf(BuiltinLayoutKind::Bool);
        case TypeKind::Byte: return dataLayout_.layoutOf(BuiltinLayoutKind::Byte);
        case TypeKind::Char: return dataLayout_.layoutOf(BuiltinLayoutKind::Char);
        case TypeKind::Short: return dataLayout_.layoutOf(BuiltinLayoutKind::Short);
        case TypeKind::UShort: return dataLayout_.layoutOf(BuiltinLayoutKind::UShort);
        case TypeKind::Int: return dataLayout_.layoutOf(BuiltinLayoutKind::Int);
        case TypeKind::UInt: return dataLayout_.layoutOf(BuiltinLayoutKind::UInt);
        case TypeKind::Long: return dataLayout_.layoutOf(BuiltinLayoutKind::Long);
        case TypeKind::ULong: return dataLayout_.layoutOf(BuiltinLayoutKind::ULong);
        case TypeKind::NInt: return dataLayout_.layoutOf(BuiltinLayoutKind::NInt);
        case TypeKind::NUInt: return dataLayout_.layoutOf(BuiltinLayoutKind::NUInt);
        case TypeKind::Float: return dataLayout_.layoutOf(BuiltinLayoutKind::Float);
        case TypeKind::Double: return dataLayout_.layoutOf(BuiltinLayoutKind::Double);
        case TypeKind::Pointer: return dataLayout_.layoutOf(BuiltinLayoutKind::Pointer);
        case TypeKind::Enum: return dataLayout_.layoutOf(BuiltinLayoutKind::Enum);
        case TypeKind::StringLiteral:
            return target::TypeLayout{type->count(), 1};
        case TypeKind::Array:
        {
            const std::optional<target::TypeLayout> element = layoutOfType(type->elementType());
            if (!element.has_value()) return std::nullopt;
            const auto layout = dataLayout_.layoutArray(*element, type->count());
            if (!layout.has_value()) return std::nullopt;
            return target::TypeLayout{layout->size, layout->alignment};
        }
        case TypeKind::Struct:
        {
            const auto iterator = structFieldTypes_.find(type);
            if (iterator == structFieldTypes_.end()) return std::nullopt;
            std::vector<target::TypeLayout> fields;
            fields.reserve(iterator->second.size());
            for (Type* fieldType : iterator->second)
            {
                const std::optional<target::TypeLayout> field = layoutOfType(fieldType);
                if (!field.has_value()) return std::nullopt;
                fields.push_back(*field);
            }
            const auto layout = dataLayout_.layoutStruct(fields);
            if (!layout.has_value()) return std::nullopt;
            return target::TypeLayout{layout->size, layout->alignment};
        }
        default:
            return std::nullopt;
    }
}

std::optional<std::uint64_t> SemanticAnalyzer::sizeOf(Type* type) const
{
    const std::optional<target::TypeLayout> layout = layoutOfType(type);
    return layout.has_value() ? std::optional<std::uint64_t>(layout->size) : std::nullopt;
}

std::optional<std::int64_t> SemanticAnalyzer::constantAsSigned(const ExpressionInfo& info) const noexcept
{
    if (!info.isConstant)
    {
        return std::nullopt;
    }
    if (info.constantValue.kind() == ConstantKind::SignedInteger)
    {
        return info.constantValue.signedInteger();
    }
    if (info.constantValue.kind() == ConstantKind::UnsignedInteger &&
        info.constantValue.unsignedInteger() <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
    {
        return static_cast<std::int64_t>(info.constantValue.unsignedInteger());
    }
    return std::nullopt;
}

std::optional<std::uint64_t> SemanticAnalyzer::constantAsUnsigned(const ExpressionInfo& info) const noexcept
{
    if (!info.isConstant)
    {
        return std::nullopt;
    }
    if (info.constantValue.kind() == ConstantKind::UnsignedInteger)
    {
        return info.constantValue.unsignedInteger();
    }
    if (info.constantValue.kind() == ConstantKind::SignedInteger && info.constantValue.signedInteger() >= 0)
    {
        return static_cast<std::uint64_t>(info.constantValue.signedInteger());
    }
    return std::nullopt;
}

ExpressionInfo SemanticAnalyzer::makeError(ast::AstNode* node)
{
    return record(node, makeRValue(types_.builtin(TypeKind::Error)));
}

ExpressionInfo SemanticAnalyzer::makeRValue(Type* type) const noexcept
{
    ExpressionInfo info;
    info.type = type;
    info.category = ValueCategory::RValue;
    return info;
}

ExpressionInfo SemanticAnalyzer::record(ast::AstNode* node, ExpressionInfo info)
{
    model_.set(node, info);
    return info;
}

ExpressionInfo SemanticAnalyzer::evaluateConstantUnary(
    ast::AstNode* node,
    const ExpressionInfo& operand,
    ExpressionInfo result)
{
    if (!operand.isConstant)
    {
        return result;
    }

    const TokenKind operation = node->tokenKind();
    if (operation == TokenKind::Bang && operand.constantValue.kind() == ConstantKind::Boolean)
    {
        result.isConstant = true;
        result.constantValue = ConstantValue::boolean(!operand.constantValue.boolean());
        return result;
    }

    if (isInteger(result.type))
    {
        const ConstantValue converted = convertConstant(operand.constantValue, operand.type, result.type);
        const std::uint32_t width = integerWidth(result.type);
        std::uint64_t bits = converted.kind() == ConstantKind::SignedInteger
            ? bitsFromSigned(converted.signedInteger(), width)
            : converted.unsignedInteger();

        if (operation == TokenKind::Minus)
        {
            bits = (~bits + 1) & maskForWidth(width);
        }
        else if (operation == TokenKind::Tilde)
        {
            bits = (~bits) & maskForWidth(width);
        }
        else if (operation != TokenKind::Plus)
        {
            return result;
        }

        result.isConstant = true;
        result.constantValue = isSignedInteger(result.type)
            ? ConstantValue::signedInteger(signedFromBits(bits, width))
            : ConstantValue::unsignedInteger(bits);
        return result;
    }

    if (isFloating(result.type) && operand.constantValue.kind() == ConstantKind::Floating)
    {
        result.isConstant = true;
        result.constantValue = ConstantValue::floating(
            operation == TokenKind::Minus ? -operand.constantValue.floating() : operand.constantValue.floating());
    }
    return result;
}

ExpressionInfo SemanticAnalyzer::evaluateConstantBinary(
    ast::AstNode* node,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    ExpressionInfo result)
{
    if (!left.isConstant || !right.isConstant)
    {
        return result;
    }

    const TokenKind operation = node->tokenKind();
    if ((operation == TokenKind::AmpAmp || operation == TokenKind::PipePipe) &&
        left.constantValue.kind() == ConstantKind::Boolean && right.constantValue.kind() == ConstantKind::Boolean)
    {
        result.isConstant = true;
        result.constantValue = ConstantValue::boolean(operation == TokenKind::AmpAmp
            ? left.constantValue.boolean() && right.constantValue.boolean()
            : left.constantValue.boolean() || right.constantValue.boolean());
        return result;
    }

    if (operation == TokenKind::EqualEqual || operation == TokenKind::BangEqual)
    {
        bool equal = false;
        bool known = true;
        if (left.constantValue.kind() == ConstantKind::Boolean && right.constantValue.kind() == ConstantKind::Boolean)
        {
            equal = left.constantValue.boolean() == right.constantValue.boolean();
        }
        else if (left.constantValue.kind() == ConstantKind::Null && right.constantValue.kind() == ConstantKind::Null)
        {
            equal = true;
        }
        else if (isNumeric(left.type) && isNumeric(right.type))
        {
            Type* common = commonNumericType(left.type, right.type);
            if (common == nullptr)
            {
                known = false;
            }
            else if (isFloating(common))
            {
                const ConstantValue leftValue = convertConstant(left.constantValue, left.type, common);
                const ConstantValue rightValue = convertConstant(right.constantValue, right.type, common);
                equal = leftValue.floating() == rightValue.floating();
            }
            else
            {
                const ConstantValue leftValue = convertConstant(left.constantValue, left.type, common);
                const ConstantValue rightValue = convertConstant(right.constantValue, right.type, common);
                equal = isSignedInteger(common)
                    ? leftValue.signedInteger() == rightValue.signedInteger()
                    : leftValue.unsignedInteger() == rightValue.unsignedInteger();
            }
        }
        else if (left.type->kind() == TypeKind::Enum && left.type == right.type)
        {
            equal = left.constantValue.signedInteger() == right.constantValue.signedInteger();
        }
        else
        {
            known = false;
        }

        if (known)
        {
            result.isConstant = true;
            result.constantValue = ConstantValue::boolean(operation == TokenKind::EqualEqual ? equal : !equal);
        }
        return result;
    }

    if (operation == TokenKind::Less || operation == TokenKind::LessEqual ||
        operation == TokenKind::Greater || operation == TokenKind::GreaterEqual)
    {
        Type* common = commonNumericType(left.type, right.type);
        if (common == nullptr)
        {
            return result;
        }

        bool comparison = false;
        if (isFloating(common))
        {
            const double a = convertConstant(left.constantValue, left.type, common).floating();
            const double b = convertConstant(right.constantValue, right.type, common).floating();
            if (operation == TokenKind::Less) comparison = a < b;
            if (operation == TokenKind::LessEqual) comparison = a <= b;
            if (operation == TokenKind::Greater) comparison = a > b;
            if (operation == TokenKind::GreaterEqual) comparison = a >= b;
        }
        else if (isSignedInteger(common))
        {
            const std::int64_t a = convertConstant(left.constantValue, left.type, common).signedInteger();
            const std::int64_t b = convertConstant(right.constantValue, right.type, common).signedInteger();
            if (operation == TokenKind::Less) comparison = a < b;
            if (operation == TokenKind::LessEqual) comparison = a <= b;
            if (operation == TokenKind::Greater) comparison = a > b;
            if (operation == TokenKind::GreaterEqual) comparison = a >= b;
        }
        else
        {
            const std::uint64_t a = convertConstant(left.constantValue, left.type, common).unsignedInteger();
            const std::uint64_t b = convertConstant(right.constantValue, right.type, common).unsignedInteger();
            if (operation == TokenKind::Less) comparison = a < b;
            if (operation == TokenKind::LessEqual) comparison = a <= b;
            if (operation == TokenKind::Greater) comparison = a > b;
            if (operation == TokenKind::GreaterEqual) comparison = a >= b;
        }

        result.isConstant = true;
        result.constantValue = ConstantValue::boolean(comparison);
        return result;
    }

    if (isFloating(result.type))
    {
        Type* common = commonNumericType(left.type, right.type);
        if (common == nullptr || !isFloating(common))
        {
            return result;
        }
        const double a = convertConstant(left.constantValue, left.type, common).floating();
        const double b = convertConstant(right.constantValue, right.type, common).floating();
        double value = 0.0;
        if (operation == TokenKind::Plus) value = a + b;
        else if (operation == TokenKind::Minus) value = a - b;
        else if (operation == TokenKind::Star) value = a * b;
        else if (operation == TokenKind::Slash) value = a / b;
        else return result;
        result.isConstant = true;
        result.constantValue = ConstantValue::floating(
            result.type->kind() == TypeKind::Float
                ? static_cast<double>(static_cast<float>(value))
                : value);
        return result;
    }

    Type* common = result.type;
    if (!isInteger(common))
    {
        common = commonIntegralType(left.type, right.type);
    }
    if (common == nullptr || !isInteger(common))
    {
        return result;
    }

    const std::uint32_t width = integerWidth(common);
    const ConstantValue aValue = convertConstant(left.constantValue, left.type, common);
    const ConstantValue bValue = convertConstant(right.constantValue, right.type, common);
    std::uint64_t a = isSignedInteger(common)
        ? bitsFromSigned(aValue.signedInteger(), width)
        : aValue.unsignedInteger();
    std::uint64_t b = isSignedInteger(common)
        ? bitsFromSigned(bValue.signedInteger(), width)
        : bValue.unsignedInteger();
    a &= maskForWidth(width);
    b &= maskForWidth(width);

    std::uint64_t bits = 0;
    if (operation == TokenKind::Plus) bits = a + b;
    else if (operation == TokenKind::Minus) bits = a - b;
    else if (operation == TokenKind::Star) bits = a * b;
    else if (operation == TokenKind::Amp) bits = a & b;
    else if (operation == TokenKind::Pipe) bits = a | b;
    else if (operation == TokenKind::Caret) bits = a ^ b;
    else if (operation == TokenKind::LessLess || operation == TokenKind::GreaterGreater)
    {
        const std::optional<std::int64_t> shift = constantAsSigned(right);
        if (!shift.has_value() || *shift < 0 || static_cast<std::uint64_t>(*shift) >= width)
        {
            if (constantTrapSuppressionDepth_ == 0)
            {
                report(
                node->range(),
                diagnostics::DiagnosticId::ConstantEvaluationTrap,
                    "constant shift count is outside the valid range for the promoted left operand");
            }
            return result;
        }
        if (operation == TokenKind::LessLess)
        {
            bits = a << static_cast<unsigned>(*shift);
        }
        else if (isSignedInteger(common))
        {
            const std::int64_t signedA = signedFromBits(a, width);
            bits = bitsFromSigned(signedA >> static_cast<unsigned>(*shift), width);
        }
        else
        {
            bits = a >> static_cast<unsigned>(*shift);
        }
    }
    else if (operation == TokenKind::Slash || operation == TokenKind::Percent)
    {
        if (b == 0)
        {
            if (constantTrapSuppressionDepth_ == 0)
            {
                report(
                    node->range(),
                    diagnostics::DiagnosticId::ConstantEvaluationTrap,
                    "division or remainder by zero is a defined runtime trap and is rejected in a constant expression");
            }
            return result;
        }

        if (isSignedInteger(common))
        {
            const std::int64_t signedA = signedFromBits(a, width);
            const std::int64_t signedB = signedFromBits(b, width);
            const std::int64_t minimum = width == 64
                ? std::numeric_limits<std::int64_t>::min()
                : -(std::int64_t{1} << (width - 1));
            if (signedA == minimum && signedB == -1)
            {
                if (constantTrapSuppressionDepth_ == 0)
                {
                    report(
                        node->range(),
                        diagnostics::DiagnosticId::ConstantEvaluationTrap,
                        "signed MIN / -1 is a defined runtime trap and is rejected in a constant expression");
                }
                return result;
            }
            const std::int64_t computed = operation == TokenKind::Slash
                ? signedA / signedB
                : signedA % signedB;
            bits = bitsFromSigned(computed, width);
        }
        else
        {
            bits = operation == TokenKind::Slash ? a / b : a % b;
        }
    }
    else
    {
        return result;
    }

    bits &= maskForWidth(width);
    result.isConstant = true;
    result.constantValue = isSignedInteger(common)
        ? ConstantValue::signedInteger(signedFromBits(bits, width))
        : ConstantValue::unsignedInteger(bits);
    return result;
}

bool SemanticAnalyzer::statementPreventsFallthrough(ast::AstNode* node) const
{
    if (node == nullptr)
    {
        return false;
    }

    switch (node->kind())
    {
        case ast::AstNodeKind::ReturnStatement:
        case ast::AstNodeKind::BreakStatement:
        case ast::AstNodeKind::ContinueStatement:
            return true;

        case ast::AstNodeKind::BlockStatement:
            for (ast::AstNode* child : node->children())
            {
                if (statementPreventsFallthrough(child))
                {
                    return true;
                }
            }
            return false;

        case ast::AstNodeKind::IfStatement:
        {
            const std::optional<bool> condition = constantBoolean(node->first());
            if (condition.has_value())
            {
                if (*condition)
                {
                    return statementPreventsFallthrough(node->second());
                }
                return node->third() != nullptr && statementPreventsFallthrough(node->third());
            }

            return node->second() != nullptr && node->third() != nullptr &&
                statementPreventsFallthrough(node->second()) && statementPreventsFallthrough(node->third());
        }

        case ast::AstNodeKind::WhileStatement:
        {
            const std::optional<bool> condition = constantBoolean(node->first());
            return condition.has_value() && *condition && !containsBreakTargetingLoop(node->second());
        }

        case ast::AstNodeKind::ForStatement:
        {
            bool alwaysTrue = node->second() == nullptr;
            if (node->second() != nullptr)
            {
                const std::optional<bool> condition = constantBoolean(node->second());
                alwaysTrue = condition.has_value() && *condition;
            }

            ast::AstNode* body = node->children().empty() ? nullptr : node->children().front();
            return alwaysTrue && !containsBreakTargetingLoop(body);
        }

        default:
            return false;
    }
}

bool SemanticAnalyzer::containsBreakTargetingLoop(ast::AstNode* node) const
{
    if (node == nullptr)
    {
        return false;
    }

    if (node->kind() == ast::AstNodeKind::BreakStatement)
    {
        return true;
    }

    if (node->kind() == ast::AstNodeKind::WhileStatement || node->kind() == ast::AstNodeKind::ForStatement)
    {
        return false;
    }

    if (node->kind() == ast::AstNodeKind::BlockStatement)
    {
        for (ast::AstNode* child : node->children())
        {
            if (containsBreakTargetingLoop(child))
            {
                return true;
            }
            if (statementPreventsFallthrough(child))
            {
                return false;
            }
        }
        return false;
    }

    if (node->kind() == ast::AstNodeKind::IfStatement)
    {
        const std::optional<bool> condition = constantBoolean(node->first());
        if (condition.has_value())
        {
            return *condition
                ? containsBreakTargetingLoop(node->second())
                : containsBreakTargetingLoop(node->third());
        }

        return containsBreakTargetingLoop(node->second()) || containsBreakTargetingLoop(node->third());
    }

    return false;
}

std::optional<bool> SemanticAnalyzer::constantBoolean(ast::AstNode* node) const noexcept
{
    const ExpressionInfo* info = model_.get(node);
    if (info == nullptr || !info->isConstant || info->constantValue.kind() != ConstantKind::Boolean)
    {
        return std::nullopt;
    }
    return info->constantValue.boolean();
}

void SemanticAnalyzer::report(
    SourceRange range,
    diagnostics::DiagnosticId id,
    std::string message)
{
    diagnostics_.report(id, diagnostics::DiagnosticSeverity::Error, range, std::move(message));
}

void SemanticAnalyzer::reportPreviousDeclaration(const Symbol* symbol)
{
    if (symbol == nullptr || symbol->declaration() == nullptr)
    {
        return;
    }

    diagnostics_.report(
        diagnostics::DiagnosticId::PreviousDeclaration,
        diagnostics::DiagnosticSeverity::Note,
        symbol->declaration()->range(),
        "previous declaration is here");
}
}
