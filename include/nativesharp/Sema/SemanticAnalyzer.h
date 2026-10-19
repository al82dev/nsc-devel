#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Sema/ConversionKind.h"
#include "nativesharp/Sema/ExpressionInfo.h"
#include "nativesharp/Sema/Scope.h"
#include "nativesharp/Sema/SemanticModel.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/TypeContext.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace nativesharp::sema
{
class SemanticAnalyzer final
{
public:
    SemanticAnalyzer(
        TypeContext& types,
        const target::TargetDataLayout& dataLayout,
        diagnostics::DiagnosticEngine& diagnostics);
    SemanticAnalyzer(const SemanticAnalyzer&) = delete;
    SemanticAnalyzer& operator=(const SemanticAnalyzer&) = delete;
    ~SemanticAnalyzer();

    bool analyze(ast::AstNode* compilationUnit);
    bool validateExecutableEntryPoint(ast::AstNode* compilationUnit);

    const SemanticModel& model() const noexcept;
    std::span<Symbol* const> globalSymbols() const noexcept;

private:
    Symbol* createSymbol(SymbolKind kind, const ast::AstNode* declaration, Type* type, bool addToGlobals);
    Scope* createScope(Scope* parent);
    bool declareInCurrent(Symbol* symbol, SourceRange range);
    Symbol* lookup(std::string_view name) const noexcept;

    void analyzeGlobal(ast::AstNode* node);
    void analyzeStruct(ast::AstNode* node);
    std::vector<Type*> analyzeStructFields(ast::AstNode* node, Symbol* structSymbol);
    void assignStructFieldLayout(Symbol* structSymbol, const std::vector<Type*>& fieldTypes);
    void analyzeEnum(ast::AstNode* node);
    void analyzeEnumMembers(ast::AstNode* node, Symbol* enumSymbol);
    void analyzeExternC(ast::AstNode* node);
    void analyzeFunction(ast::AstNode* node, bool externC);
    std::vector<Type*> resolveFunctionParameterTypes(ast::AstNode* node);
    Symbol* declareFunctionSymbol(ast::AstNode* node, Type* functionType, bool externC);
    void analyzeFunctionBody(
        ast::AstNode* node,
        Type* returnType,
        const std::vector<Type*>& parameterTypes,
        Symbol* functionSymbol,
        bool externC);
    void analyzeGlobalVariable(ast::AstNode* node);

    void analyzeBlock(ast::AstNode* node, bool createNestedScope);
    void analyzeStatement(ast::AstNode* node);
    void analyzeInlineAssemblyStatement(ast::AstNode* node);
    void analyzeStaticAssert(ast::AstNode* node);
    void analyzeVariable(ast::AstNode* node);
    void analyzeIf(ast::AstNode* node);
    void analyzeWhile(ast::AstNode* node);
    void analyzeFor(ast::AstNode* node);
    void analyzeReturn(ast::AstNode* node);

    ExpressionInfo analyzeExpression(ast::AstNode* node, bool allowFunctionDesignator = false);
    ExpressionInfo analyzeExtendedExpression(ast::AstNode* node);
    ExpressionInfo analyzeIdentifier(ast::AstNode* node, bool allowFunctionDesignator);
    ExpressionInfo analyzeLiteral(ast::AstNode* node);
    ExpressionInfo analyzeIntegerLiteral(ast::AstNode* node);
    ExpressionInfo analyzeUnary(ast::AstNode* node);
    std::optional<ExpressionInfo> analyzeStorageUnary(
        ast::AstNode* node,
        const ExpressionInfo& operand,
        lexer::TokenKind operation);
    ExpressionInfo analyzeBinary(ast::AstNode* node);
    std::optional<ExpressionInfo> analyzeLogicalBinary(
        ast::AstNode* node,
        const ExpressionInfo& left,
        const ExpressionInfo& right,
        lexer::TokenKind operation,
        bool shortCircuit);
    std::optional<ExpressionInfo> analyzeEqualityBinary(
        ast::AstNode* node,
        const ExpressionInfo& left,
        const ExpressionInfo& right,
        lexer::TokenKind operation,
        bool shortCircuit);
    std::optional<ExpressionInfo> analyzeRelationalBinary(
        ast::AstNode* node,
        const ExpressionInfo& left,
        const ExpressionInfo& right,
        lexer::TokenKind operation,
        bool shortCircuit);
    std::optional<ExpressionInfo> analyzeArithmeticBinary(
        ast::AstNode* node,
        const ExpressionInfo& left,
        const ExpressionInfo& right,
        lexer::TokenKind operation,
        bool shortCircuit);
    ExpressionInfo analyzeConditional(ast::AstNode* node);
    ExpressionInfo analyzeAssignment(ast::AstNode* node);
    ExpressionInfo analyzeCall(ast::AstNode* node);
    ExpressionInfo analyzeIndex(ast::AstNode* node);
    bool validateIndexOperands(
        ast::AstNode* node,
        const ExpressionInfo& base,
        const std::vector<std::pair<ast::AstNode*, ExpressionInfo>>& indices);
    Type* resolveIndexedElement(
        ast::AstNode* node,
        const ExpressionInfo& base,
        const std::vector<std::pair<ast::AstNode*, ExpressionInfo>>& indices,
        bool& fixedArray);
    ExpressionInfo analyzeMember(ast::AstNode* node, bool pointerForm);
    ExpressionInfo analyzeCast(ast::AstNode* node);
    ExpressionInfo analyzeSizeof(ast::AstNode* node, bool typeForm);
    ExpressionInfo analyzeAlignof(ast::AstNode* node, bool typeForm);
    ExpressionInfo analyzeOffsetof(ast::AstNode* node);
    ExpressionInfo analyzeNew(ast::AstNode* node);
    ExpressionInfo analyzeDelete(ast::AstNode* node);
    ExpressionInfo analyzeStackalloc(ast::AstNode* node);

    Type* resolveType(ast::TypeSyntax* syntax);
    Type* resolveBaseType(ast::TypeSyntax* syntax);
    bool validateObjectType(Type* type, SourceRange range, bool allowArray);
    bool validateExternCSignature(
        ast::AstNode* functionNode,
        Type* returnType,
        std::span<Type* const> parameterTypes);
    bool isCAbiCompatibleDirectType(Type* type, bool allowVoid) const;
    bool isCAbiCompatibleStruct(Type* type) const;
    bool isCAbiCompatibleStructField(Type* type) const;

    bool isInteger(Type* type) const noexcept;
    bool isSignedInteger(Type* type) const noexcept;
    bool isUnsignedInteger(Type* type) const noexcept;
    bool isFloating(Type* type) const noexcept;
    bool isNumeric(Type* type) const noexcept;
    bool isPointer(Type* type) const noexcept;
    bool isScalarIncrementType(Type* type) const noexcept;
    bool isSized(Type* type) const noexcept;
    std::uint32_t integerWidth(Type* type) const noexcept;

    Type* promoteInteger(Type* type) const noexcept;
    Type* commonIntegralType(Type* left, Type* right) const noexcept;
    Type* commonNumericType(Type* left, Type* right) const noexcept;

    bool canImplicitlyConvert(const ExpressionInfo& source, Type* destination) const;
    bool canStandardImplicitConvert(Type* source, Type* destination) const noexcept;
    bool canExplicitlyConvert(Type* source, Type* destination) const noexcept;
    ConversionKind classifyImplicitConversion(const ExpressionInfo& source, Type* destination) const noexcept;
    ConversionKind classifyExplicitConversion(Type* source, Type* destination) const noexcept;
    void recordImplicitConversion(
        const ast::AstNode* owner,
        const ast::AstNode* operand,
        const ExpressionInfo& source,
        Type* destination,
        int operandIndex = -1);
    void recordIntegralPromotion(
        const ast::AstNode* owner,
        const ast::AstNode* operand,
        Type* source,
        Type* destination,
        int operandIndex = -1);
    void recordExplicitConversion(
        const ast::AstNode* owner,
        const ast::AstNode* operand,
        Type* source,
        Type* destination);
    void recordOperator(
        const ast::AstNode* node,
        lexer::TokenKind tokenKind,
        Type* leftType,
        Type* rightType,
        Type* operationType,
        Type* resultType,
        bool unary);
    bool constantFits(const ConstantValue& value, Type* destination) const noexcept;
    ConstantValue convertConstant(const ConstantValue& value, Type* source, Type* destination) const;

    std::optional<target::TypeLayout> layoutOfType(Type* type) const;
    std::optional<std::uint64_t> sizeOf(Type* type) const;
    std::optional<std::int64_t> constantAsSigned(const ExpressionInfo& info) const noexcept;
    std::optional<std::uint64_t> constantAsUnsigned(const ExpressionInfo& info) const noexcept;
    ExpressionInfo makeError(ast::AstNode* node);
    ExpressionInfo makeRValue(Type* type) const noexcept;
    ExpressionInfo record(ast::AstNode* node, ExpressionInfo info);

    ExpressionInfo evaluateConstantUnary(ast::AstNode* node, const ExpressionInfo& operand, ExpressionInfo result);
    ExpressionInfo evaluateConstantBinary(
        ast::AstNode* node,
        const ExpressionInfo& left,
        const ExpressionInfo& right,
        ExpressionInfo result);
    ExpressionInfo evaluateConstantLogical(
        ast::AstNode* node,
        const ExpressionInfo& left,
        const ExpressionInfo& right,
        ExpressionInfo result);
    ExpressionInfo evaluateConstantEquality(
        ast::AstNode* node,
        const ExpressionInfo& left,
        const ExpressionInfo& right,
        ExpressionInfo result);
    ExpressionInfo evaluateConstantRelational(
        ast::AstNode* node,
        const ExpressionInfo& left,
        const ExpressionInfo& right,
        ExpressionInfo result);
    ExpressionInfo evaluateConstantFloating(
        ast::AstNode* node,
        const ExpressionInfo& left,
        const ExpressionInfo& right,
        ExpressionInfo result);
    ExpressionInfo evaluateConstantInteger(
        ast::AstNode* node,
        const ExpressionInfo& left,
        const ExpressionInfo& right,
        ExpressionInfo result);
    ExpressionInfo evaluateConstantShift(
        ast::AstNode* node,
        const ExpressionInfo& right,
        Type* common,
        std::uint32_t width,
        std::uint64_t bits,
        ExpressionInfo result);
    ExpressionInfo evaluateConstantDivision(
        ast::AstNode* node,
        Type* common,
        std::uint32_t width,
        std::uint64_t left,
        std::uint64_t right,
        ExpressionInfo result);
    ExpressionInfo finishConstantInteger(
        Type* common,
        std::uint32_t width,
        std::uint64_t bits,
        ExpressionInfo result) const;

    bool statementPreventsFallthrough(ast::AstNode* node) const;
    bool containsBreakTargetingLoop(ast::AstNode* node) const;
    std::optional<bool> constantBoolean(ast::AstNode* node) const noexcept;
    void report(SourceRange range, diagnostics::DiagnosticId id, std::string message);
    void reportPreviousDeclaration(const Symbol* symbol);

    TypeContext& types_;
    const target::TargetDataLayout& dataLayout_;
    diagnostics::DiagnosticEngine& diagnostics_;
    SemanticModel model_;

    std::vector<Symbol*> ownedSymbols_;
    std::vector<Scope*> ownedScopes_;
    std::vector<Symbol*> globalSymbols_;
    Scope* globalScope_ = nullptr;
    Scope* currentScope_ = nullptr;
    Symbol* currentFunction_ = nullptr;
    int loopDepth_ = 0;
    int constantTrapSuppressionDepth_ = 0;

    std::unordered_map<Type*, std::vector<Type*>> structFieldTypes_;
};
}
