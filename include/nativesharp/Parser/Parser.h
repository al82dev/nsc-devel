#pragma once

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Token.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace nativesharp::language
{
struct InterpolationPart;
}

namespace nativesharp::parser
{
struct ParsedVariantCase;

class Parser final
{
public:
    Parser(
        std::span<const lexer::Token> tokens,
        ast::AstArena& arena,
        diagnostics::DiagnosticEngine& diagnostics,
        std::span<const std::string> knownTypeNames = {});

    ast::AstNode* parseCompilationUnit();

private:
    const lexer::Token& current() const noexcept;
    const lexer::Token& peek(std::size_t distance = 1) const noexcept;
    bool at(lexer::TokenKind kind) const noexcept;
    bool atContextualKeyword(std::string_view keyword) const noexcept;
    bool startsParenthesizedBlock(std::string_view keyword) const noexcept;
    bool expectContextualKeyword(std::string_view keyword, std::string_view description);
    bool consumeIf(lexer::TokenKind kind) noexcept;
    const lexer::Token& consume() noexcept;
    bool expect(lexer::TokenKind kind, std::string_view description);

    SourceRange join(SourceRange begin, SourceRange end) const noexcept;
    SourceRange tokenRangeFrom(std::size_t beginIndex) const noexcept;

    bool canStartType(const lexer::Token& token) const noexcept;
    bool isAccessibilityModifier(lexer::TokenKind kind) const noexcept;
    bool isInvalidMemberObjectType(const ast::TypeSyntax* type) const noexcept;
    bool isMemberOperatorToken(lexer::TokenKind kind) const noexcept;
    std::string_view memberOperatorSpelling(lexer::TokenKind kind) const noexcept;
    std::string memberOperatorMethodName(lexer::TokenKind kind, std::size_t arity) const;
    void applyMemberFloatingPointMode(
        ast::AstNode* node,
        ast::FloatingPointModeSyntaxKind mode);
    bool canStartTypeAt(std::size_t tokenIndex) const noexcept;
    enum class TypeScanTaskKind : std::uint8_t
    {
        Begin,
        FunctionParameters,
        FunctionAfterParameter,
        FunctionAfterResult,
        FunctionAfterError,
        TupleElements,
        TupleAfterElement
    };
    struct TypeScanTask final
    {
        TypeScanTaskKind kind = TypeScanTaskKind::Begin;
        std::size_t count = 0;
        bool flag = false;
    };
    bool scanTypeAt(std::size_t tokenIndex, std::size_t& afterType) const noexcept;
    bool beginTypeScan(
        std::size_t& cursor,
        std::vector<TypeScanTask>& tasks) const noexcept;
    bool advanceFunctionTypeScan(
        std::size_t& cursor,
        TypeScanTask task,
        std::vector<TypeScanTask>& tasks) const noexcept;
    bool scheduleFunctionResultScan(
        std::size_t& cursor,
        std::vector<TypeScanTask>& tasks) const noexcept;
    bool advanceTupleTypeScan(
        std::size_t& cursor,
        TypeScanTask task,
        std::vector<TypeScanTask>& tasks) const noexcept;
    bool finishTupleTypeScan(
        std::size_t& cursor,
        std::size_t elementCount,
        bool sawComma) const noexcept;
    bool scanOwnershipTypePrefix(std::size_t& tokenIndex) const noexcept;
    bool scanFunctionCarrierAt(std::size_t& cursor) const noexcept;
    void scanFunctionEffectsAt(std::size_t& cursor) const noexcept;
    bool scanNamedTypeAt(std::size_t tokenIndex, std::size_t& afterType) const noexcept;
    bool scanNamedTypeIdentityAt(
        std::size_t tokenIndex,
        std::size_t& cursor,
        bool& builtin) const noexcept;
    bool scanGenericTypeArgumentsAt(std::size_t& cursor) const noexcept;
    bool scanTypeSuffixesAt(std::size_t& cursor) const noexcept;
    std::size_t scanPointerQualifiersAt(std::size_t cursor) const noexcept;
    bool scanArraySuffixAt(std::size_t& cursor) const noexcept;
    bool looksLikeVariableDeclaration() const noexcept;
    bool looksLikeLocalFunction() const noexcept;
    ast::AstNode* parseLocalFunctionAsLambda();
    bool looksLikeConditionDeclaration() const noexcept;
    bool looksLikeParenthesizedType() const noexcept;
    bool parseQualifiedName(std::string& name, SourceRange& range);
    ast::TypeSyntax* parseType();

    struct TypeParseState final
    {
        ast::OwnershipQualifierSyntaxKind ownershipQualifier =
            ast::OwnershipQualifierSyntaxKind::None;
        ast::ReferenceQualifierSyntaxKind referenceQualifier =
            ast::ReferenceQualifierSyntaxKind::None;
        std::string lifetimeName;
        SourceRange qualifierRange{};
        bool dynamicTrait = false;
        SourceRange dynamicRange{};
        bool opaqueResult = false;
        SourceRange opaqueRange{};
        SourceRange longDoubleRange{};
        bool recoverLongDoubleAsDouble = false;
    };

    struct FunctionTypeParseState final
    {
        ast::FunctionTypeCarrierSyntaxKind carrier =
            ast::FunctionTypeCarrierSyntaxKind::Pointer;
        ast::FunctionCallingConventionSyntaxKind convention =
            ast::FunctionCallingConventionSyntaxKind::NativeSharp;
        bool nullable = false;
        std::string lifetimeName;
    };

    enum class TypeParseFramePhase : std::uint8_t
    {
        Begin,
        FunctionParameters,
        FunctionParameterReady,
        FunctionResultReady,
        FunctionErrorReady,
        TupleElements,
        TupleElementReady,
        NamedArguments,
        NamedArgumentReady
    };

    enum class TypeParseStep : std::uint8_t
    {
        Continue,
        PushChild,
        Complete,
        Failed
    };

    struct TypeParseFrame final
    {
        TypeParseFramePhase phase = TypeParseFramePhase::Begin;
        TypeParseState typeState;
        FunctionTypeParseState functionState;
        ast::TypeSyntax* type = nullptr;
        SourceRange start{};
        SourceRange completeRange{};
        std::vector<ast::TypeSyntax*> genericArguments;
        bool sawParameter = false;
    };

    ast::TypeSyntax* parseTypeImpl();
    TypeParseStep advanceTypeParseFrame(TypeParseFrame& frame);
    TypeParseStep acceptTypeParseChild(
        TypeParseFrame& frame,
        ast::TypeSyntax* child);
    TypeParseStep beginTypeParseFrame(TypeParseFrame& frame);
    TypeParseStep beginFunctionTypeFrame(TypeParseFrame& frame);
    TypeParseStep advanceFunctionTypeFrame(TypeParseFrame& frame);
    TypeParseStep acceptFunctionTypeChild(
        TypeParseFrame& frame,
        ast::TypeSyntax* child);
    TypeParseStep closeFunctionParameters(TypeParseFrame& frame);
    void finishFunctionTypeFrame(TypeParseFrame& frame);
    TypeParseStep beginTupleTypeFrame(TypeParseFrame& frame);
    TypeParseStep advanceTupleTypeFrame(TypeParseFrame& frame);
    TypeParseStep acceptTupleTypeChild(
        TypeParseFrame& frame,
        ast::TypeSyntax* child);
    TypeParseStep finishTupleTypeFrame(TypeParseFrame& frame);
    TypeParseStep beginNamedTypeFrame(TypeParseFrame& frame);
    TypeParseStep advanceNamedTypeFrame(TypeParseFrame& frame);
    TypeParseStep acceptNamedTypeChild(
        TypeParseFrame& frame,
        ast::TypeSyntax* child);
    TypeParseStep finishNamedTypeArgument(TypeParseFrame& frame);
    void finishNamedTypeFrame(TypeParseFrame& frame);
    bool parseTypeQualifiers(TypeParseState& state);
    bool parseOwnershipTypeQualifier(TypeParseState& state);
    void parseReferenceTypeQualifier(TypeParseState& state);
    void parseTraitTypeQualifiers(TypeParseState& state);
    void parseDeferredTypePrefix(TypeParseState& state);
    bool parseFunctionCarrier(SourceRange start, FunctionTypeParseState& state);
    void parseFunctionCallingConvention(FunctionTypeParseState& state);
    void validateFunctionCarrier(
        SourceRange start,
        const FunctionTypeParseState& state);
    ast::TypeSyntax* makeFunctionType(
        SourceRange start,
        TypeParseState& typeState,
        FunctionTypeParseState& functionState);
    void parseFunctionTypeEffects(ast::TypeSyntax* function);
    void validateFunctionVariadic(
        ast::TypeSyntax* function,
        SourceRange start,
        const FunctionTypeParseState& state);
    ast::TypeSyntax* parseNamedTypeBase(
        TypeParseState& state,
        SourceRange& completeRange);
    static void normalizeBuiltinTypeName(std::string& baseName);
    void parseTypePostfixModifiers(ast::TypeSyntax* type, SourceRange& completeRange);
    void parsePointeeConst(
        ast::TypeSyntax* type,
        SourceRange& completeRange,
        bool& pointeeConst,
        SourceRange& pointeeConstRange);
    void parsePointerTypeModifier(ast::TypeSyntax* type, SourceRange& completeRange);
    void parsePointerTypeQualifiers(ast::TypeModifierSyntax& pointer);
    void parsePointerTypeQualifier(ast::TypeModifierSyntax& pointer);
    void parsePointerProvenanceQualifier(ast::TypeModifierSyntax& pointer);
    void parsePointerNumericQualifier(
        ast::TypeModifierSyntax& pointer,
        std::string_view qualifier,
        SourceRange qualifierStart);
    void parseArrayTypeModifier(ast::TypeSyntax* type, SourceRange& completeRange);
    bool parseSpecialArrayTypeModifier(
        ast::TypeSyntax* type,
        SourceRange left,
        SourceRange& completeRange);
    void parseRankArrayTypeModifier(
        ast::TypeSyntax* type,
        SourceRange left,
        SourceRange& completeRange);
    void parseFixedArrayTypeModifier(
        ast::TypeSyntax* type,
        SourceRange left,
        SourceRange& completeRange);
    std::string parseOptionalLifetimeAnnotation(std::string_view context);
    ast::TypeSyntax* parseAllocationElementType();

    enum class GenericConstraintBranchResult : std::uint8_t
    {
        NotMatched,
        Parsed,
        Failed
    };

    enum class GenericCapabilityKind : std::uint8_t
    {
        Class,
        Struct,
        DefaultConstructor,
        Copy,
        Move,
        Lifetime,
        Unmanaged,
        NotNull,
        RefLike
    };

    struct GenericConstraintParseState final
    {
        std::size_t clauseBegin = 0;
        SourceRange parameterRange{};
        std::string rootParameter;
        ast::GenericConstraintClause clause;
        const ast::GenericParameterSyntax* parameterSyntax = nullptr;
        bool known = false;
        bool knownLifetime = false;
        bool associatedEquality = false;
    };

    bool parseGenericParameterList(ast::AstNode* declaration);
    bool parseGenericParameter(
        ast::AstNode* declaration,
        std::unordered_set<std::string>& names);
    bool parseGenericParameterKind(
        ast::GenericParameterSyntax& syntax,
        bool& lifetimeParameter);
    bool parseConstGenericParameterType(
        ast::GenericParameterSyntax& syntax,
        bool pack);
    static bool supportsConstGenericParameterType(const ast::TypeSyntax* type) noexcept;
    void finishGenericParameter(
        ast::AstNode* declaration,
        ast::GenericParameterSyntax syntax,
        bool lifetimeParameter,
        std::size_t parameterBegin,
        std::unordered_set<std::string>& names);
    void validateGenericPackPlacement(const ast::AstNode* declaration);
    // NSC-LIMIT-007. Cero o mas clausulas `where T : class, Base, IFoo`, en la posicion donde el
    // lenguaje las admite: despues de la lista de parametros de un metodo o funcion, despues de
    // las bases de una clase, despues de la lista de parametros de un delegado, y despues del
    // nombre en un struct. Devuelve false si alguna clausula estaba mal formada.
    bool parseGenericConstraintClauses(ast::AstNode* declaration);
    bool beginGenericConstraintClause(
        ast::AstNode* declaration,
        std::unordered_set<std::string>& constrained,
        GenericConstraintParseState& state,
        bool& ok);
    bool parseLifetimeConstraintClause(
        ast::AstNode* declaration,
        const GenericConstraintParseState& state,
        bool& ok);
    bool parseLifetimeConstraintItem(
        ast::AstNode* declaration,
        const GenericConstraintParseState& state,
        std::unordered_set<std::string>& written,
        bool& ok);
    GenericConstraintBranchResult parseGenericEqualityConstraint(
        ast::AstNode* declaration,
        GenericConstraintParseState& state,
        bool& ok);
    ast::TypeSyntax* parseGenericEqualityTarget(
        const GenericConstraintParseState& state,
        bool parameterIsConstant);
    void validateGenericEqualityTarget(
        ast::AstNode* declaration,
        const GenericConstraintParseState& state,
        bool parameterIsConstant,
        bool& ok);
    bool parseGenericCapabilityConstraints(
        ast::AstNode* declaration,
        GenericConstraintParseState& state,
        bool& ok);
    std::optional<GenericCapabilityKind> currentGenericCapability() const noexcept;
    bool parseGenericCapability(
        ast::GenericConstraintClause& clause,
        GenericCapabilityKind kind,
        bool firstConstraint,
        bool& ok);
    bool parseGenericNominalConstraint(
        const ast::AstNode* declaration,
        ast::GenericConstraintClause& clause,
        std::unordered_set<std::string>& written,
        bool& ok);
    bool parseTypeArgumentList(std::vector<ast::TypeSyntax*>& arguments);
    bool parseTypeArgument(std::vector<ast::TypeSyntax*>& arguments);
    bool looksLikeTupleTypeArgument() const noexcept;
    ast::TypeSyntax* parseConstExpressionTypeArgument();
    ast::TypeSyntax* parseConstLiteralTypeArgument();
    bool consumeGenericGreater();
    static bool canAppearInTypeArgumentList(lexer::TokenKind kind) noexcept;
    bool genericLookaheadTokenIsAllowed(
        std::size_t cursor,
        int parenthesisDepth) const noexcept;
    bool looksLikeGenericCallTypeArguments() const noexcept;
    bool scanClosedGenericTypePrefix(std::size_t& cursor) const noexcept;
    bool looksLikeClosedGenericTypeReference() const noexcept;

    ast::AstNode* parseNamespaceMember();
    ast::AstNode* parseNamespaceDeclaration();
    ast::AstNode* parseExtensionDeclaration(lexer::TokenKind accessibility = lexer::TokenKind::Invalid);
    std::vector<std::string> scanExtensionGenericArguments() const;
    ast::AstNode* makeExtensionReceiver(
        ast::TypeSyntax* receiverType,
        lexer::TokenKind receiverPass);
    void addExtensionMember(
        ast::AstNode* extension,
        ast::AstNode* member,
        ast::TypeSyntax* receiverType,
        lexer::TokenKind receiverPass,
        lexer::TokenKind accessibility,
        const std::vector<std::string>& genericArguments);
    void addExtensionPropertyAccessors(
        ast::AstNode* extension,
        ast::AstNode* property,
        ast::TypeSyntax* receiverType,
        lexer::TokenKind receiverPass);
    ast::AstNode* cloneDeclarationNode(const ast::AstNode* node);
    ast::TypeSyntax* cloneDeclarationType(const ast::TypeSyntax* type);
    ast::AstNode* parseUsingDirective();
    ast::AstNode* parseUsingAliasDirective(
        std::size_t beginIndex,
        const lexer::Token& first);
    ast::AstNode* parseUsingNamespaceDirective(
        std::size_t beginIndex,
        const lexer::Token& first);
    // NativeSharp 3.0 T12-F02. Explicit imports lower to the existing using nodes so name
    // lookup, generic materialization and extension lookup all observe one deterministic table.
    enum class ExplicitImportKind : std::uint64_t
    {
        Module = 1,
        Symbol = 2,
        Trait = 3,
        Extensions = 4,
        RestrictedGlob = 5
    };
    ast::AstNode* parseImportDirective(bool reexport = false);
    ExplicitImportKind parseExplicitImportKind();
    bool parseExplicitImportTarget(
        ExplicitImportKind& kind,
        lexer::Token& first,
        lexer::Token& last,
        std::string& target);
    bool parseExplicitImportAlias(std::string& alias, lexer::Token& last);
    ast::AstNode* makeExplicitImport(
        std::size_t beginIndex,
        bool reexport,
        ExplicitImportKind kind,
        const lexer::Token& first,
        const lexer::Token& last,
        std::string target,
        std::string alias);
    struct GlobalDeclarationAttributeState final
    {
        std::size_t declarationBeginIndex = 0;
        lexer::TokenKind accessibility = lexer::TokenKind::Invalid;
        std::string granularVisibility;
        bool isPartial = false;
        bool isAbstract = false;
        bool isSealed = false;
        bool isStatic = false;
        bool isExportedFunction = false;
        bool isInlineFunction = false;
        bool isForceInlineFunction = false;
        bool isNoInlineFunction = false;
        bool isReadonlyType = false;
        bool isRecordStruct = false;
        bool isCopyType = false;
        bool isUnsafeTraitContract = false;
        bool isBorrowedView = false;
        bool isAsyncMethod = false;
        bool returnDirectly = false;
        std::optional<std::uint64_t> layoutPack;
        std::optional<std::uint64_t> layoutAlign;
        std::optional<std::uint64_t> layoutSize;
        std::optional<std::string> representation;
    };
    ast::AstNode* parseGlobalDeclaration();
    void parseGlobalDeclarationPrefixes(GlobalDeclarationAttributeState& state);
    bool tryParseGlobalRepresentationPrefix(GlobalDeclarationAttributeState& state);
    bool tryParseGlobalLayoutPrefix(GlobalDeclarationAttributeState& state);
    bool tryParseGlobalFunctionPrefix(GlobalDeclarationAttributeState& state);
    bool tryParseGlobalValueTypePrefix(GlobalDeclarationAttributeState& state);
    bool tryParseGlobalInheritancePrefix(GlobalDeclarationAttributeState& state);
    ast::AstNode* dispatchGlobalDeclaration(GlobalDeclarationAttributeState& state);
    bool tryDispatchReferenceGlobal(
        GlobalDeclarationAttributeState& state,
        ast::AstNode*& declaration);
    bool tryDispatchValueGlobal(
        GlobalDeclarationAttributeState& state,
        ast::AstNode*& declaration);
    bool tryDispatchExternGlobal(
        GlobalDeclarationAttributeState& state,
        ast::AstNode*& declaration);
    ast::AstNode* dispatchGlobalFallback(GlobalDeclarationAttributeState& state);
    void applyGlobalFunctionAttributes(
        ast::AstNode* declaration,
        const GlobalDeclarationAttributeState& state);
    void applyGlobalIdentityAttributes(
        ast::AstNode* declaration,
        const GlobalDeclarationAttributeState& state);
    void applyGlobalLayoutAttributes(
        ast::AstNode* declaration,
        const GlobalDeclarationAttributeState& state);
    void finishGlobalDeclarationAttributes(
        ast::AstNode* declaration,
        const GlobalDeclarationAttributeState& state);
    bool parseFrameStorageAttribute(std::string& frameStorage);
    void applyNamespaceCallableAttributes(
        ast::AstNode* declaration,
        bool isAsync,
        const std::string& frameStorage);
    // C2.2 "union and explicit layout": a union is parsed by the same routine as a struct --
    // they differ only in where their fields land, which is a layout question, not a syntax one.
    enum class StructLayoutForm
    {
        Struct,
        Union
    };
    ast::AstNode* parseStructDeclaration(
        StructLayoutForm form = StructLayoutForm::Struct,
        bool isBorrowedView = false,
        bool isRecordStruct = false,
        bool isCopyType = false);
    void appendStructMarker(ast::AstNode* declaration, std::string_view marker);
    void applyStructMarkers(
        ast::AstNode* declaration,
        StructLayoutForm form,
        bool isBorrowedView,
        bool isRecordStruct,
        bool isCopyType);
    void parseStructPrimaryConstructor(
        ast::AstNode* declaration,
        const lexer::Token& name,
        std::string_view structName,
        bool isRecordStruct);
    ast::AstNode* parsePromotedStructParameter(
        ast::AstNode* declaration,
        std::unordered_set<std::string>& names,
        bool isRecordStruct);
    void parseStructBaseTypes(ast::AstNode* declaration);
    void parseRecordDerives(
        ast::AstNode* declaration,
        bool isRecordStruct,
        bool& sawRecordDeriveClause);
    // NativeSharp 3.0 T03. A closed tagged sum is materialized as an ordinary struct whose
    // payload carrier is an in-place union. Keeping the generated representation in the parser
    // means name lookup, generics, layout, HIR, MIR and LLVM all consume the same productive AST
    // shapes they already understand instead of a second, partially integrated backend path.
    ast::AstNode* parseVariantDeclaration();
    void parseVariantCases(
        std::vector<ParsedVariantCase>& cases,
        std::unordered_set<std::string>& caseNames);
    void parseVariantPayload(ParsedVariantCase& item, const lexer::Token& caseName);
    void validateVariantGeneratedNames(const std::vector<ParsedVariantCase>& cases);
    ast::TypeSyntax* makeVariantTagType(
        SourceRange range,
        lexer::TokenKind tagKind,
        std::string_view tagTypeName);
    ast::TypeSyntax* makeVariantSelfType(
        SourceRange range,
        std::string_view variantName,
        const ast::AstNode* declaration);
    ast::AstNode* makeVariantBlock(SourceRange range);
    ast::AstNode* makeVariantReturn(ast::AstNode* expression, SourceRange range);
    ast::AstNode* makeVariantMove(ast::AstNode* expression, SourceRange range);
    void markVariantPublic(ast::AstNode* member);
    void markVariantPrivate(ast::AstNode* member);
    void addVariantStorage(
        ast::AstNode* declaration,
        const std::vector<ParsedVariantCase>& cases,
        SourceRange range,
        lexer::TokenKind tagKind,
        std::string_view tagTypeName,
        std::string_view variantName,
        std::string& payloadFieldName);
    void addVariantCaseMembers(
        ast::AstNode* declaration,
        const ParsedVariantCase& item,
        std::size_t caseIndex,
        SourceRange variantRange,
        std::string_view variantName,
        std::string_view payloadFieldName);
    void addVariantFactory(
        ast::AstNode* declaration,
        const ParsedVariantCase& item,
        std::size_t caseIndex,
        SourceRange variantRange,
        std::string_view variantName,
        std::string_view payloadFieldName);
    void addVariantTest(
        ast::AstNode* declaration,
        const ParsedVariantCase& item,
        std::size_t caseIndex);
    void addVariantExtract(
        ast::AstNode* declaration,
        const ParsedVariantCase& item,
        std::size_t caseIndex,
        std::string_view payloadFieldName);
    void addVariantTagMethod(
        ast::AstNode* declaration,
        SourceRange range,
        lexer::TokenKind tagKind,
        std::string_view tagTypeName);
    void setVariantMetadata(
        ast::AstNode* declaration,
        const std::vector<ParsedVariantCase>& cases,
        std::string_view tagTypeName);
    // NativeSharp 3.0 T04-F05. An opaque alias is represented by the ordinary nominal
    // single-field struct machinery. The parser synthesizes the private representation and the
    // module-internal explicit boundary operations, so every later phase sees one productive
    // value type rather than a parallel alias-only backend path.
    ast::AstNode* parseOpaqueTypeDeclaration(bool explicitCopy);
    ast::TypeSyntax* makeOpaqueSelfType(
        SourceRange range,
        std::string_view typeName,
        const ast::AstNode* declaration);
    ast::AstNode* makeOpaqueReturn(ast::AstNode* expression);
    ast::AstNode* makeOpaqueMove(ast::AstNode* expression);
    void addOpaqueRepresentationField(
        ast::AstNode* declaration,
        SourceRange range,
        ast::TypeSyntax* representation,
        std::string_view representationField);
    void markOpaqueInternal(ast::AstNode* member);
    void addOpaqueFromRepresentation(
        ast::AstNode* declaration,
        SourceRange range,
        std::string_view typeName,
        ast::TypeSyntax* representation,
        std::string_view representationField,
        bool explicitCopy);
    void addOpaqueToRepresentation(
        ast::AstNode* declaration,
        SourceRange range,
        ast::TypeSyntax* representation,
        std::string_view representationField);
    ast::AstNode* parseStructBody(
        ast::AstNode* declaration,
        std::string_view structName,
        std::size_t beginIndex);
    ast::AstNode* parseClassDeclaration(
        bool isPartial,
        bool isAbstract,
        bool isSealed,
        bool isStatic = false);
    void applyClassModifiers(
        ast::AstNode* declaration,
        bool isAbstract,
        bool isSealed,
        bool isStatic);
    void parseClassPrimaryConstructor(
        ast::AstNode* declaration,
        const lexer::Token& name,
        std::string_view className);
    ast::AstNode* parsePromotedClassParameter(
        ast::AstNode* declaration,
        std::unordered_set<std::string>& names);
    void addClassPrimaryConstructor(
        ast::AstNode* declaration,
        const lexer::Token& name,
        std::string_view className,
        const std::vector<ast::AstNode*>& parameters);
    void parseClassBaseTypes(ast::AstNode* declaration, bool isStatic);
    void parseClassBody(
        ast::AstNode* declaration,
        std::string_view className,
        bool isStatic);
    // NativeSharp 3.0 T05-F02. A trait impl is represented as a class extension whose second()
    // names the target and whose sole base specification names the trait. Later phases merge the
    // extension with the locally-owned class and retain this source node for reusable NSMETA.
    ast::AstNode* parseTraitImplementationDeclaration();
    void validateTraitImplementationTarget(ast::AstNode* declaration, ast::TypeSyntax* target);
    void parseTraitImplementationBody(ast::AstNode* declaration);
    ast::AstNode* parseInterfaceDeclaration(bool isPartial, bool isTrait = false);
    void parseInterfaceBaseTypes(ast::AstNode* declaration);
    void parseInterfaceBody(ast::AstNode* declaration, bool isTrait);
    ast::AstNode* parseDelegateDeclaration();
    void parseDelegateParameters(ast::AstNode* declaration);
    ast::AstNode* parseInterfaceMember(bool isTrait);
    ast::AstNode* parseInterfaceMemberImpl(bool isTrait);
    struct InterfaceMemberState final
    {
        lexer::TokenKind accessibility = lexer::TokenKind::KwPublic;
        bool sawPublic = false;
        bool sawAbstract = false;
        bool sawNoHeap = false;
        bool sawStatic = false;
    };
    void parseInterfaceMemberModifiers(
        bool isTrait,
        bool& sawPublic,
        bool& sawAbstract,
        bool& sawNoHeap,
        bool& sawStatic);
    ast::AstNode* parseInterfaceTypedMember(
        bool isTrait,
        std::size_t beginIndex,
        InterfaceMemberState& state,
        ast::TypeSyntax* returnType,
        bool isReferenceReturn,
        std::string returnLifetime);
    ast::AstNode* parseInterfaceOperatorMember(
        bool isTrait,
        std::size_t beginIndex,
        const InterfaceMemberState& state,
        ast::TypeSyntax* returnType);
    ast::AstNode* parseInterfaceNamedMember(
        bool isTrait,
        std::size_t beginIndex,
        const InterfaceMemberState& state,
        ast::TypeSyntax* returnType,
        bool isReferenceReturn,
        std::string returnLifetime);
    ast::AstNode* parseAssociatedItem(bool implementation);
    ast::AstNode* parseClassMember(
        std::string_view className,
        bool allowUnresolvedNamedTypes = false,
        bool structContext = false);
    ast::AstNode* parseClassMemberImpl(
        std::string_view className,
        bool allowUnresolvedNamedTypes,
        bool structContext);
    struct MemberParseState final
    {
        std::size_t beginIndex = 0;
        std::string frameStorage;
        lexer::TokenKind accessibility = lexer::TokenKind::Invalid;
        bool isAsync = false;
        bool isPartial = false;
        bool isPartialMember = false;
        bool isNew = false;
        bool isStatic = false;
        bool isReadonly = false;
        bool isVolatile = false;
        bool isConst = false;
        bool isVirtual = false;
        bool isOverride = false;
        bool isAbstract = false;
        bool isSealed = false;
        bool isExported = false;
        bool isExternC = false;
        bool isInline = false;
        bool isForceInline = false;
        bool isNoInline = false;
        bool isNoHeap = false;
        bool isRequired = false;
        bool isUnsafe = false;
        bool isControlledSpecialization = false;
        SourceRange controlledSpecializationRange{};
        bool isBorrowedView = false;
        bool isCopyType = false;
        std::optional<std::uint64_t> nestedLayoutPack;
        std::optional<std::uint64_t> nestedLayoutAlign;
        std::optional<std::uint64_t> nestedLayoutSize;
        std::optional<std::uint64_t> explicitFieldOffset;
        ast::AstNode* completedDeclaration = nullptr;
    };
    void parseMemberFrameStorage(MemberParseState& state);
    void parseMemberModifiers(MemberParseState& state);
    void parseExplicitFieldOffsets(MemberParseState& state);
    bool tryParseUnsafeMemberModifier(MemberParseState& state);
    bool tryParseSpecializeMemberModifier(MemberParseState& state);
    bool tryParsePartialMemberModifier(MemberParseState& state);
    bool tryParseNestedLayoutModifier(MemberParseState& state);
    bool tryParseNestedValueModifier(MemberParseState& state);
    bool tryParseStandardMemberModifier(MemberParseState& state);
    bool tryParseMemberStorageModifier(MemberParseState& state, const lexer::Token& modifier);
    bool tryParseMemberDispatchModifier(MemberParseState& state, const lexer::Token& modifier);
    bool tryParseMemberCodegenModifier(MemberParseState& state, const lexer::Token& modifier);
    bool tryParseMemberInteropModifier(MemberParseState& state, const lexer::Token& modifier);
    void setMemberFlag(
        bool& flag,
        const lexer::Token& modifier,
        std::string_view duplicateMessage);
    ast::AstNode* parseExternCMemberImportBlock(
        MemberParseState& state,
        const lexer::Token& modifier);
    ast::AstNode* parseExternCImportedFunction(std::string_view dllName);
    void parseExternCImportedParameters(ast::AstNode* imported);
    void validateMemberModifiers(const MemberParseState& state);
    void validateMemberInheritanceModifiers(const MemberParseState& state);
    void validateMemberBodyModifiers(const MemberParseState& state);
    void validateMemberFieldModifiers(const MemberParseState& state);
    void rejectControlledSpecialization(
        const MemberParseState& state,
        std::string_view declarationKind);
    ast::AstNode* dispatchClassMember(
        std::string_view className,
        bool structContext,
        MemberParseState& state);
    ast::AstNode* parseDestructorMember(
        std::string_view className,
        MemberParseState& state);
    ast::AstNode* parseConstructorMember(
        std::string_view className,
        MemberParseState& state);
    ast::AstNode* parseConversionMember(MemberParseState& state);
    ast::AstNode* parseEventMember(MemberParseState& state);
    ast::AstNode* parseAnonymousAggregateMember(
        bool structContext,
        MemberParseState& state);
    void validateAnonymousAggregateMember(
        bool structContext,
        const lexer::Token& keyword,
        const MemberParseState& state);
    ast::AstNode* createAnonymousAggregateDeclaration(
        bool anonymousUnion,
        const lexer::Token& keyword,
        std::string_view synthesizedName,
        MemberParseState& state);
    ast::AstNode* createAnonymousAggregateField(
        ast::AstNode* declaration,
        const lexer::Token& keyword,
        std::string_view synthesizedName,
        MemberParseState& state);
    void applyAnonymousFieldModifiers(
        ast::AstNode* field,
        const MemberParseState& state);
    ast::AstNode* parseNestedClassMember(MemberParseState& state);
    ast::AstNode* parseTypedClassMember(
        std::string_view className,
        bool structContext,
        MemberParseState& state);
    ast::AstNode* parseUnnamedBitFieldMember(
        ast::TypeSyntax* type,
        bool isReferenceReturn,
        MemberParseState& state);
    ast::AstNode* parseOperatorClassMember(
        ast::TypeSyntax* type,
        bool isReferenceReturn,
        MemberParseState& state);
    ast::AstNode* parseIndexerClassMember(
        ast::TypeSyntax* type,
        bool structContext,
        bool isReferenceReturn,
        MemberParseState& state);
    ast::AstNode* parseNamedClassMember(
        std::string_view className,
        bool structContext,
        ast::TypeSyntax* type,
        bool isReferenceReturn,
        std::string returnLifetime,
        MemberParseState& state);
    ast::AstNode* parseMethodClassMember(
        const lexer::Token& name,
        bool structContext,
        ast::TypeSyntax* type,
        bool isReferenceReturn,
        std::string returnLifetime,
        MemberParseState& state);
    void finalizeMethodClassMember(
        ast::AstNode* method,
        bool structContext,
        bool isReferenceReturn,
        std::string returnLifetime,
        MemberParseState& state);
    ast::AstNode* parsePropertyClassMember(
        const lexer::Token& name,
        bool structContext,
        ast::TypeSyntax* type,
        bool isReferenceReturn,
        MemberParseState& state);
    ast::AstNode* parseFieldClassMember(
        const lexer::Token& name,
        ast::TypeSyntax* type,
        bool isReferenceReturn,
        MemberParseState& state);
    void validateFieldMember(
        const lexer::Token& name,
        ast::TypeSyntax* type,
        const MemberParseState& state);
    ast::AstNode* createFieldDeclaration(
        const lexer::Token& name,
        ast::TypeSyntax* type,
        bool isReferenceReturn);
    ast::AstNode* parseFieldBitWidth(
        ast::AstNode* field,
        const lexer::Token& name,
        bool isReferenceReturn,
        const MemberParseState& state);
    void applyFieldMetadata(
        ast::AstNode* field,
        const MemberParseState& state);
    void applyFieldModifiers(
        ast::AstNode* field,
        const MemberParseState& state);
    void parseFieldInitializer(
        ast::AstNode* field,
        const lexer::Token& name,
        ast::AstNode* bitWidth,
        const MemberParseState& state);
    // NSC-LIMIT-009. Los modificadores que `parseClassMember` ya leyo cuando descubre que lo que
    // sigue no es un campo ni un metodo sino una DECLARACION DE TIPO. Viajan juntos porque el
    // diagnostico tiene que poder decir cual de ellos sobra, y decirlo por su nombre.
    struct NestedTypeModifiers final
    {
        lexer::TokenKind accessibility = lexer::TokenKind::Invalid;
        bool isPartial = false;
        bool isNew = false;
        bool isStatic = false;
        bool isReadonly = false;
        bool isConst = false;
        bool isVirtual = false;
        bool isOverride = false;
        bool isAbstract = false;
        bool isSealed = false;
        bool isExported = false;
        bool isExternC = false;
        bool hasExplicitFieldOffset = false;
        bool isBorrowedView = false;
        bool isCopyType = false;
        std::optional<std::uint64_t> layoutPack;
        std::optional<std::uint64_t> layoutAlign;
        std::optional<std::uint64_t> layoutSize;
    };
    // NSC-LIMIT-009. Cierto cuando el token actual abre una declaracion de tipo anidada.
    bool atNestedTypeDeclaration() const noexcept;
    // NSC-LIMIT-009. Delega en los CINCO parsers de tipo que ya existen --no reimplementa ninguna
    // gramatica-- y despues aplica al nodo devuelto lo que el contexto de miembro aporta: la
    // accesibilidad escrita y el rechazo de todo modificador que no signifique nada en un tipo.
    ast::AstNode* parseNestedTypeDeclaration(
        const NestedTypeModifiers& modifiers,
        std::size_t beginIndex);
    ast::AstNode* parseNestedTypeDeclarationImpl(
        const NestedTypeModifiers& modifiers,
        std::size_t beginIndex);
    void refuseNestedTypeModifier(
        std::size_t beginIndex,
        const char* modifierName,
        const char* reason);
    bool validateNestedTypeModifiers(
        const NestedTypeModifiers& modifiers,
        std::size_t beginIndex);
    ast::AstNode* createNestedTypeDeclaration(const NestedTypeModifiers& modifiers);
    void applyNestedTypeIdentity(
        ast::AstNode* declaration,
        const NestedTypeModifiers& modifiers,
        bool nestedReadonlyStruct,
        std::size_t beginIndex);
    void applyNestedTypeLayout(
        ast::AstNode* declaration,
        const NestedTypeModifiers& modifiers);
    ast::AstNode* parseEventTail(
        lexer::TokenKind accessibility,
        bool isNew,
        bool isStatic,
        std::size_t beginIndex,
        bool isPartial = false);
    void addEventBackingField(
        ast::AstNode* event,
        const lexer::Token& name,
        ast::TypeSyntax* eventType,
        bool isStatic);
    void parseEventAccessors(
        ast::AstNode* event,
        const lexer::Token& name,
        ast::TypeSyntax* eventType,
        std::size_t beginIndex,
        bool isPartial);
    ast::AstNode* parseEventAccessor(
        const lexer::Token& name,
        ast::TypeSyntax* eventType,
        std::size_t beginIndex,
        bool isPartial,
        bool isAdd);
    ast::AstNode* parseDestructorTail(std::string_view className, std::size_t beginIndex);
    ast::AstNode* parsePropertyTail(
        ast::TypeSyntax* propertyType,
        const lexer::Token& name,
        lexer::TokenKind accessibility,
        bool isNew,
        bool isStatic,
        bool isVirtual,
        bool isOverride,
        bool isAbstract,
        bool isSealed,
        std::size_t beginIndex,
        bool interfaceContext,
        bool allowReadonlyGetter,
        bool isReferenceReturn,
        bool isPartial = false);
    struct PropertyParseState final
    {
        ast::AstNode* property = nullptr;
        ast::TypeSyntax* type = nullptr;
        const lexer::Token* name = nullptr;
        lexer::TokenKind accessibility = lexer::TokenKind::Invalid;
        std::size_t beginIndex = 0;
        bool isNew = false;
        bool isStatic = false;
        bool isVirtual = false;
        bool isOverride = false;
        bool isAbstract = false;
        bool isSealed = false;
        bool interfaceContext = false;
        bool allowReadonlyGetter = false;
        bool isReferenceReturn = false;
        bool isPartial = false;
        bool sawGet = false;
        bool sawSet = false;
        bool sawAutomatic = false;
        bool sawExplicit = false;
        std::vector<ast::AstNode*> accessors;
        std::string propertyName;
        std::string backingName;
        ast::AstNode* initializer = nullptr;
    };
    ast::AstNode* createPropertyDeclaration(PropertyParseState& state);
    void parsePropertyAccessors(PropertyParseState& state);
    ast::AstNode* parsePropertyAccessor(PropertyParseState& state);
    ast::AstNode* createPropertyAccessorNode(
        PropertyParseState& state,
        bool getter,
        bool initOnly,
        bool accessorReadonly,
        lexer::TokenKind accessibility,
        SourceRange range);
    void parsePropertyAccessorBody(
        PropertyParseState& state,
        ast::AstNode* accessor,
        bool getter,
        SourceRange accessorRange);
    void parsePropertyInitializer(PropertyParseState& state);
    void finalizePropertyDeclaration(PropertyParseState& state);
    ast::AstNode* parseIndexerTail(
        ast::TypeSyntax* elementType,
        lexer::TokenKind accessibility,
        bool isNew,
        bool isVirtual,
        bool isOverride,
        bool isAbstract,
        bool isSealed,
        bool isReferenceReturn,
        std::size_t beginIndex,
        bool interfaceContext,
        bool allowReadonlyGetter);
    struct IndexerParseState final
    {
        ast::AstNode* indexer = nullptr;
        ast::TypeSyntax* type = nullptr;
        lexer::TokenKind accessibility = lexer::TokenKind::Invalid;
        std::size_t beginIndex = 0;
        bool isNew = false;
        bool isVirtual = false;
        bool isOverride = false;
        bool isAbstract = false;
        bool isSealed = false;
        bool isReferenceReturn = false;
        bool interfaceContext = false;
        bool allowReadonlyGetter = false;
        bool sawGet = false;
        bool sawSet = false;
        bool sawBody = false;
        std::vector<ast::AstNode*> parameters;
    };
    ast::AstNode* createIndexerDeclaration(IndexerParseState& state);
    void parseIndexerParameters(IndexerParseState& state);
    void parseIndexerAccessors(IndexerParseState& state);
    ast::AstNode* parseIndexerAccessor(IndexerParseState& state);
    ast::AstNode* createIndexerAccessorNode(
        IndexerParseState& state,
        bool getter,
        bool initOnly,
        bool accessorReadonly,
        lexer::TokenKind accessibility,
        SourceRange range);
    void parseIndexerAccessorBody(
        IndexerParseState& state,
        ast::AstNode* accessor,
        SourceRange accessorRange);
    ast::AstNode* parseOperatorTail(
        ast::TypeSyntax* returnType,
        lexer::TokenKind accessibility,
        bool isStatic,
        std::size_t beginIndex,
        bool allowBodyless = false);
    void parseCallableParameterList(ast::AstNode* callable);
    void validateOperatorArity(
        ast::AstNode* method,
        const lexer::Token& operation);
    void parseOperatorBody(ast::AstNode* method, bool allowBodyless);
    ast::AstNode* parseConversionOperatorTail(
        bool implicitConversion,
        lexer::TokenKind accessibility,
        bool isStatic,
        std::size_t beginIndex);
    ast::AstNode* makeAccessorMethod(
        std::string name,
        ast::TypeSyntax* returnType,
        lexer::TokenKind accessibility,
        bool isStatic,
        bool isVirtual,
        bool isOverride,
        bool isAbstract,
        bool isSealed,
        std::string metadata,
        SourceRange range);
    ast::AstNode* makeAutoGetterBody(
        std::string_view backingFieldName,
        SourceRange range);
    ast::AstNode* makeAutoSetterBody(
        std::string_view backingFieldName,
        SourceRange range,
        bool transferOwnership);
    ast::AstNode* parseConstructorTail(
        std::string_view className,
        lexer::TokenKind accessibility,
        bool isStatic,
        bool isNoHeap,
        std::size_t beginIndex,
        bool isPartial = false);
    void parseConstructorInitializer(
        ast::AstNode* constructor,
        bool isStatic);
    void parseConstructorBody(ast::AstNode* constructor, bool isPartial);
    ast::AstNode* parseMethodTail(
        ast::TypeSyntax* returnType,
        const lexer::Token& name,
        lexer::TokenKind accessibility,
        bool isNew,
        bool isStatic,
        bool isVirtual,
        bool isOverride,
        bool isAbstract,
        bool isSealed,
        bool isExported,
        bool isExternC,
        bool isInline,
        bool isForceInline,
        bool isNoInline,
        bool isNoHeap,
          bool isUnsafe,
        std::size_t beginIndex,
        bool allowBodyless = false,
        bool isPartialMethod = false);
    void applyMethodModifiers(
        ast::AstNode* method,
        lexer::TokenKind accessibility,
        bool isNew,
        bool isStatic,
        bool isVirtual,
        bool isOverride,
        bool isAbstract,
        bool isSealed,
        bool isExported,
        bool isExternC,
        bool isInline,
        bool isForceInline,
        bool isNoInline,
        bool isNoHeap,
        bool isUnsafe);
    void parseMethodBody(
        ast::AstNode* method,
        bool isAbstract,
        bool allowBodyless,
        bool isPartialMethod);
    ast::AstNode* parseEnumDeclaration();
    ast::AstNode* parseExternCDeclaration();
    void parseExternCBlock(ast::AstNode* declaration);
    bool parseExternCSingle(ast::AstNode* declaration);
    ast::AstNode* parseFunctionOrGlobalVariable();
    ast::AstNode* parseFunctionTail(ast::TypeSyntax* returnType, const lexer::Token& name, bool externalOnly);
    bool parseVariadicCallableParameters(ast::AstNode* callable);
    void validateGlobalFunctionConstraints(ast::AstNode* function);
    void parseGlobalFunctionBody(ast::AstNode* function, bool externalOnly);
    void parseThrowsClause(ast::AstNode* callable);
    void parseFloatingPointModeClause(ast::AstNode* callable);
    void parseFfiUnwindClause(ast::AstNode* callable);
    ast::AstNode* parseParameter();
    void parseParameterContracts(ast::AstNode* parameter);
    void appendParameterContract(ast::AstNode* parameter, std::string contract);

    ast::AstNode* parseBlock();
    ast::AstNode* parseBlockImpl();
    ast::AstNode* parseStatement();
    ast::AstNode* parseStatementImpl();
    ast::AstNode* parseFloatingPointScopeStatement();
    ast::AstNode* parseUnsafeScopeStatement();
    std::optional<ast::AstNode*> tryParseScopedStatement();
    std::optional<ast::AstNode*> tryParseControlStatement();
    ast::AstNode* parseYieldStatement();
    ast::AstNode* parseStaticLocalStatement();
    ast::AstNode* parseJumpStatement();
    std::optional<ast::AstNode*> tryParseKeywordStatement();
    std::optional<ast::AstNode*> tryParseDeclarationStatement();
    ast::AstNode* parseExpressionStatement();
    enum class PatternKind
    {
        Wildcard,
        Literal,
        Binding,
        Tuple,
        Struct,
        Variant,
        Range,
        Relational,
        Not,
        And,
        List,
        Or,
        // NS304-T03-F04. `T(a, b, c)` -- deconstruccion posicional de un nominal/tupla.
        Positional
    };
    enum class PatternBindingMode
    {
        Value,
        Reference,
        Move,
        Copy,
        ReferenceMutable
    };
    struct Pattern final
    {
        PatternKind kind = PatternKind::Wildcard;
        PatternBindingMode bindingMode = PatternBindingMode::Value;
        std::string name;
        SourceRange range;
        ast::AstNode* firstValue = nullptr;
        ast::AstNode* secondValue = nullptr;
        std::optional<std::int64_t> firstInteger;
        std::optional<std::int64_t> secondInteger;
        std::optional<std::size_t> restIndex;
        std::vector<std::string> fieldNames;
        std::vector<Pattern> children;
    };
    struct ForeachParseState final
    {
        std::size_t beginIndex = 0;
        std::string temporaryPrefix;
        std::string sourceName;
        std::string indexName;
        std::string enumeratorName;
        std::string rangeBeginName;
        std::string rangeEndName;
        std::string rangeStepName;
        std::string rangeCursorName;
        std::string elementTemporaryName;
        std::string patternTemporaryPrefix;
        bool elementByReference = false;
        bool elementMutableReference = false;
        bool elementIsRaii = false;
        ast::TypeSyntax* elementType = nullptr;
        bool elementDeconstruction = false;
        Pattern elementPattern;
        std::string elementName;
        ast::AstNode* collection = nullptr;
        ast::AstNode* collectionProbe = nullptr;
    };
    struct IfParseState final
    {
        std::size_t beginIndex = 0;
        ast::AstNode* conditionDeclaration = nullptr;
        ast::AstNode* condition = nullptr;
        ast::AstNode* thenBlock = nullptr;
    };
    class StructuredControlParser;
    // Carries a while header across heap-backed body parsing without retaining a C++ frame.
    struct WhileParseState final
    {
        std::size_t beginIndex = 0;
        ast::AstNode* conditionDeclaration = nullptr;
        ast::AstNode* condition = nullptr;
    };
    // Carries the source start of a do-while until its body and trailing condition are complete.
    struct DoWhileParseState final
    {
        std::size_t beginIndex = 0;
    };
    // Carries the three for-clause components while its block body is parsed iteratively.
    struct ForParseState final
    {
        std::size_t beginIndex = 0;
        ast::AstNode* initializer = nullptr;
        ast::AstNode* condition = nullptr;
        ast::AstNode* increment = nullptr;
    };
    using PatternExpressionFactory = std::function<ast::AstNode*()>;
    class PatternIterationParser;
    class PatternAlternativeExpander;
    Pattern parsePattern(bool implicitBinding);
    bool patternLiteralStartsHere() const noexcept;
    std::pair<ast::AstNode*, std::optional<std::int64_t>>
        parseSignedPatternIntegerLiteral();
    Pattern parseBindingPattern(std::size_t patternBegin);
    Pattern parseRelationalPattern(std::size_t patternBegin);
    Pattern parseLiteralOrRangePattern(
        std::size_t patternBegin,
        SourceRange start);
    bool patternHasBindings(const Pattern& pattern) const;
    std::vector<Pattern> expandPatternAlternatives(const Pattern& pattern) const;
    ast::AstNode* makeBooleanLiteral(const SourceRange& range, bool value);
    bool patternIsConditionOnly(const Pattern& pattern) const;
    ast::AstNode* makeRelationalPatternCondition(
        const Pattern& pattern,
        const PatternExpressionFactory& source);
    ast::AstNode* makeStructPatternCondition(
        const Pattern& pattern,
        const PatternExpressionFactory& source);
    ast::AstNode* makeVariantPatternCondition(
        const Pattern& pattern,
        const PatternExpressionFactory& source);
    ast::AstNode* makeLogicalPatternCondition(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        lexer::TokenKind operation);
    ast::AstNode* makeListPatternCondition(
        const Pattern& pattern,
        const PatternExpressionFactory& source);
    ast::AstNode* makeNotPatternCondition(
        const Pattern& pattern,
        const PatternExpressionFactory& source);
    ast::AstNode* makePatternCondition(
        const Pattern& pattern,
        const PatternExpressionFactory& source);
    ast::AstNode* lowerBindingPattern(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* continuation,
        bool emitBindings);
    ast::AstNode* lowerOrPattern(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* continuation,
        std::string_view temporaryPrefix,
        bool emitBindings);
    ast::AstNode* lowerTuplePattern(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* continuation,
        std::string_view temporaryPrefix,
        bool emitBindings);
    ast::AstNode* lowerStructPattern(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* continuation,
        std::string_view temporaryPrefix,
        bool emitBindings);
    ast::AstNode* lowerListPattern(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* continuation,
        std::string_view temporaryPrefix,
        bool emitBindings);
    ast::AstNode* lowerPositionalPattern(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* continuation,
        std::string_view temporaryPrefix,
        bool emitBindings);
    ast::AstNode* lowerVariantPattern(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* continuation,
        std::string_view temporaryPrefix,
        bool emitBindings);
    ast::AstNode* lowerPattern(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* continuation,
        std::string_view temporaryPrefix,
        bool emitBindings = true);
    void collectUniquePatternBindings(
        const Pattern& pattern,
        std::vector<std::pair<std::string, PatternBindingMode>>& bindings) const;
    void applyPatternBindingMode(
        ast::AstNode* declaration,
        PatternBindingMode mode) const;
    void emitOrPatternBindings(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* destination,
        std::string_view temporaryPrefix);
    void emitSinglePatternBinding(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* destination);
    void emitTuplePatternBindings(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* destination,
        std::string_view temporaryPrefix);
    void emitPositionalPatternBindings(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* destination,
        std::string_view temporaryPrefix);
    void emitStructPatternBindings(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* destination,
        std::string_view temporaryPrefix);
    void emitListPatternBindings(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* destination,
        std::string_view temporaryPrefix);
    void emitVariantPatternBindings(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* destination,
        std::string_view temporaryPrefix);
    void emitPatternBindings(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* destination,
        std::string_view temporaryPrefix);
    ast::AstNode* parseConditionalPatternStatement();
    bool statementAlwaysDiverges(const ast::AstNode* statement) const;
    ast::AstNode* parseStaticAssertStatement();
    ast::AstNode* parseInlineAssemblyStatement();
    void parseInlineAssemblyHeader(std::string& targetMetadata, std::string& dialectMetadata);
    void parseInlineAssemblyBody(ast::AstNode* statement, std::string& exactBody);
    void parseInlineAssemblyOperandClauses(ast::AstNode* statement, std::size_t beginIndex);
    void parseInlineAssemblyOutputClause(ast::AstNode* statement);
    void parseInlineAssemblyInputClause(ast::AstNode* statement);
    void parseInlineAssemblyClobberClause(ast::AstNode* statement);
    bool validateInlineAssemblyClobberName(
        ast::AstNode* statement,
        const lexer::Token& registerToken,
        const std::string& registerName,
        bool specialClobber,
        std::unordered_set<std::string>& seen);
    void appendInlineAssemblyClobber(
        ast::AstNode* statement,
        const lexer::Token& registerToken,
        const std::string& registerName,
        bool specialClobber,
        std::uint64_t ordinal);
    std::string parseInlineAssemblyConstraint(bool output, bool& explicitConstraint);
    void validateInlineAssemblyControlFlow(ast::AstNode* statement);
    void validateInlineAssemblyPlaceholders(ast::AstNode* statement);
    ast::AstNode* parseVariableDeclarator(
        ast::TypeSyntax* type,
        bool cloneType,
        bool isConst,
        bool isRaii,
        bool isVolatile,
        bool isReferenceLocal,
        bool isPinned);
    ast::AstNode* parseVariableDeclaration(bool expectSemicolon, bool isStaticLocal = false);
    ast::AstNode* finishVariableDeclaration(
        std::size_t beginIndex,
        ast::TypeSyntax* type,
        bool expectSemicolon,
        bool isStaticLocal,
        bool isConst,
        bool isRaii,
        bool isVolatile,
        bool isReferenceLocal,
        bool isPinned,
        bool isScope);
    // C7 "aggregate initializer": the braced right-hand side of a declaration.
    ast::AstNode* parseAggregateInitializer();
    ast::AstNode* parseEmbeddedStatementBlock();
    ast::AstNode* parseIfStatement();
    ast::AstNode* parseStructuredControlStatement();
    void parseIfHeader(IfParseState& state);
    ast::AstNode* lowerIfStatement(const IfParseState& state, ast::AstNode* elseNode);
    ast::AstNode* parseWhileStatement();
    void parseWhileHeader(WhileParseState& state);
    ast::AstNode* lowerWhileStatement(const WhileParseState& state, ast::AstNode* body);
    ast::AstNode* parseDoWhileStatement();
    ast::AstNode* lowerDoWhileStatement(const DoWhileParseState& state, ast::AstNode* body);
    ast::AstNode* parseSwitchStatement();
    // T03 statement match is lowered early to one hidden governing local plus an if/else decision
    // tree over generated case tests and guarded payload extractors. The source expression is
    // therefore evaluated exactly once and no runtime allocation or boxing protocol is introduced.
    ast::AstNode* parseMatchStatement();
    bool matchPatternIsCatchAll(const Pattern& pattern) const;
    std::vector<std::pair<std::string, PatternBindingMode>> collectMatchPatternBindings(
        const Pattern& pattern) const;
    void validateMatchOrBindings(const Pattern& pattern);
    void collectMatchCoverage(
        const Pattern& pattern,
        std::vector<std::string>& cases,
        std::vector<std::pair<std::size_t, bool>>& listLengths,
        bool& wildcard,
        bool& partial) const;
    void appendMatchCoverage(
        const Pattern& pattern,
        const ast::AstNode* guard,
        std::string& coverage,
        bool& firstArm) const;
    void appendMatchExpressionAlternative(
        ast::AstNode* match,
        const Pattern& pattern,
        ast::AstNode* guard,
        ast::AstNode* value,
        bool referenceResult,
        const std::string& temporary,
        std::size_t armBegin,
        std::size_t alternativeIndex,
        std::string& coverage,
        bool& firstArm);
    void parseMatchExpressionArm(
        ast::AstNode* match,
        const std::string& temporary,
        std::string& coverage,
        bool& firstArm,
        bool& hasReferenceBinding);
    ast::AstNode* parseMatchExpression();
    ast::AstNode* parseLoopExpression();
    ast::AstNode* parseNameOfExpression();
    ast::AstNode* parseOverflowPolicyExpression();
    ast::AstNode* parseCollectionExpression();
    ast::AstNode* parseComprehensionExpression();
    ast::AstNode* parseComprehensionYieldStatement();
    // True when the cursor sits on `case` or on the `default` OF A LABEL. `default` is also an
    // expression keyword (`default(T)`), so it only opens a label when a colon follows it
    // immediately -- otherwise a section that begins with `default(int)` would be misread.
    bool atSwitchLabel() const noexcept;
    ast::AstNode* parseForStatement();
    void parseForHeader(ForParseState& state);
    ast::AstNode* parseForExpressionList(std::size_t beginIndex);
    ast::AstNode* lowerForStatement(const ForParseState& state, ast::AstNode* body);
    ast::AstNode* parseForeachStatement();
    bool parseForeachHeader(ForeachParseState& state);
    ast::AstNode* lowerForeachStatement(ForeachParseState&& state, ast::AstNode* body);
    ast::TypeSyntax* makeForeachVarMarker(SourceRange range);
    void configureForeachElementBinding(
        const ForeachParseState& state,
        ast::AstNode* declaration);
    void emitForeachElementBindings(
        const ForeachParseState& state,
        SourceRange range,
        ast::AstNode* destination);
    ast::AstNode* lowerForeachRange(
        ForeachParseState& state,
        ast::AstNode* body,
        SourceRange range);
    void reportInvalidForeachRangeReference(
        const ForeachParseState& state,
        const ast::AstNode* collection);
    void appendForeachRangeAssertion(
        const ForeachParseState& state,
        SourceRange range,
        ast::AstNode* lowered);
    ast::AstNode* makeForeachArrayCandidate(
        const ForeachParseState& state,
        ast::AstNode* body,
        SourceRange range);
    ast::AstNode* makeForeachSpanCandidate(
        const ForeachParseState& state,
        ast::AstNode* body,
        SourceRange range);
    ast::AstNode* makeForeachPatternCandidate(
        const ForeachParseState& state,
        ast::AstNode* body,
        SourceRange range);
    void appendParsedStatement(ast::AstNode*& destination, ast::AstNode* statement);
    ast::AstNode* makeIdentifierExpression(const SourceRange& range, std::string name);
    ast::AstNode* makeIntegerLiteralExpression(const SourceRange& range, std::uint64_t value);
    ast::AstNode* makeMemberExpression(ast::AstNode* receiver, std::string memberName);
    ast::AstNode* makeZeroArgCallExpression(ast::AstNode* callee);
    ast::AstNode* makeIndexExpression(ast::AstNode* receiver, ast::AstNode* index);
    ast::AstNode* makeBinaryExpression(lexer::TokenKind operatorKind, ast::AstNode* left, ast::AstNode* right);
    ast::AstNode* makeAssignmentExpression(ast::AstNode* target, ast::AstNode* value);
    ast::AstNode* makeExpressionStatement(ast::AstNode* expression);
    ast::AstNode* makeVariableDeclaration(
        ast::TypeSyntax* type, std::string name, ast::AstNode* initializer, bool isRaii, bool isConst = false);
    ast::AstNode* parseReturnStatement();
    // NativeSharp 3.0 T06-F05. `defer [ref x, move y] { ... }` is lowered while parsing the
    // enclosing block to nested try/finally regions. This deliberately reuses the one cleanup
    // pipeline already used by finally, using, RAII and structured exits.
    ast::AstNode* parseDeferStatement();
    ast::AstNode* parseTryStatement();
    ast::AstNode* parseCatchClause();
    ast::AstNode* parseThrowStatement();
    ast::AstNode* parseUsingStatement();

    ast::AstNode* parseExpression();
    ast::AstNode* parseExpressionImpl();
    ast::AstNode* parseArgument();
    ast::AstNode* parseAssignmentExpression();
    bool hasLambdaFatArrowBeforeBoundary(std::size_t cursor) const noexcept;
    std::size_t skipLambdaGenericHead(std::size_t begin) const noexcept;
    bool looksLikeParenthesizedLambdaHead(std::size_t begin) const noexcept;
    bool looksLikeLambdaParameterHead(std::size_t begin) const noexcept;
    bool looksLikeLambdaCaptureHead() const noexcept;
    void parseLambdaPrefix(
        ast::AstNode* lambda,
        std::string& effects,
        std::string& callingConvention);
    std::string parseLambdaCaptureMode();
    std::string parseLambdaCaptureName(SourceRange& range);
    void parseLambdaCaptureList(std::string& contract);
    void parseLambdaCaptureOrAttribute(
        std::string& captureContract,
        std::string& attributeContract);
    ast::AstNode* parseLambdaParameter();
    std::vector<ast::AstNode*> parseLambdaParameters();
    void materializeLambdaAutoParameters(
        ast::AstNode* lambda,
        const std::vector<ast::AstNode*>& parameters);
    void applyLambdaContracts(
        ast::AstNode* lambda,
        std::string capture,
        const std::string& effects,
        const std::string& callingConvention,
        const std::string& attribute);
    ast::AstNode* parseLambdaExpression();
    bool looksLikeLambdaExpression() const noexcept;
    ast::AstNode* parseConditionalExpression();
    ast::AstNode* parseNullCoalescingExpression();
    ast::AstNode* parseRangeExpression();
    ast::AstNode* parseLogicalOrExpression();
    ast::AstNode* parseLogicalAndExpression();
    ast::AstNode* parseBitwiseOrExpression();
    ast::AstNode* parseBitwiseXorExpression();
    ast::AstNode* parseBitwiseAndExpression();
    ast::AstNode* parseEqualityExpression();
    ast::AstNode* parseRelationalExpression();
    ast::AstNode* parseShiftExpression();
    ast::AstNode* parseAdditiveExpression();
    ast::AstNode* parseMultiplicativeExpression();
    ast::AstNode* parseAwaitUnaryExpression();
    ast::AstNode* parseFromEndUnaryExpression();
    ast::AstNode* parseMoveUnaryExpression();
    ast::AstNode* parseOrdinaryUnaryExpression();
    ast::AstNode* parseNamedCastExpression();
    ast::AstNode* parseParenthesizedCastExpression();
    ast::AstNode* parseOffsetofExpression();
    bool looksLikeSizeofTypeForm(std::size_t cursor) const noexcept;
    ast::AstNode* parseSizeofOrAlignofExpression();
    ast::AstNode* parseDefaultValueExpression();
    bool parenthesizedNewIsPlacement() const noexcept;
    void appendCallArguments(ast::AstNode* node, lexer::TokenKind closingToken);
    ast::AstNode* parseTargetTypedNew(const SourceRange& start);
    void parsePlacementOptions(
        ast::AstNode*& trailingCapacity,
        std::string& failureProfile);
    void attachPlacementResultType(ast::AstNode* creation, ast::TypeSyntax* type);
    ast::AstNode* parsePlacementNew(const SourceRange& start);
    void parseArrayAllocationDimensions(
        ast::AstNode* node,
        std::vector<SourceRange>& omittedDimensions,
        std::size_t& countedDimensions);
    void finishArrayAllocation(
        ast::AstNode* node,
        ast::TypeSyntax* type,
        const std::vector<SourceRange>& omittedDimensions,
        std::size_t countedDimensions);
    ast::AstNode* parseTypedNew(const SourceRange& start);
    ast::AstNode* parseNewExpression();
    ast::AstNode* parseDeleteExpression();
    ast::AstNode* parseDestroyExpression();
    ast::AstNode* parseStackallocExpression();
    ast::AstNode* parseUnaryExpression();
    ast::AstNode* parsePostfixBaseExpression();
    ast::AstNode* parseCallSuffix(
        ast::AstNode* expression,
        std::vector<ast::TypeSyntax*> genericArguments,
        bool explicitGenericArguments);
    ast::AstNode* parseIndexArgument();
    ast::AstNode* parseIndexSuffix(ast::AstNode* expression, bool conditional);
    ast::AstNode* parseNullConditionalMemberSuffix(ast::AstNode* expression);
    ast::AstNode* parseMemberSuffix(ast::AstNode* expression);
    ast::AstNode* parsePostfixOperatorSuffix(ast::AstNode* expression);
    ast::AstNode* parseResultPropagationSuffix(ast::AstNode* expression);
    ast::AstNode* parsePostfixExpression();
    bool questionBeginsConditional() const noexcept;
    bool comprehensionExpressionStartsHere() const noexcept;
    std::size_t pureGroupingDepth() const noexcept;
    ast::AstNode* parseTupleElement();
    ast::AstNode* parseParenthesizedPrimaryExpression();
    std::optional<ast::AstNodeKind> classifyPrimaryToken(
        lexer::TokenKind kind) const noexcept;
    ast::AstNode* makeInterpolationStringLiteral(
        const lexer::Token& token,
        std::string value);
    ast::AstNode* makeInterpolationBooleanLiteral(
        const lexer::Token& token,
        bool value);
    ast::AstNode* makeInterpolationCall(
        const lexer::Token& token,
        std::string method,
        std::vector<ast::AstNode*> arguments);
    std::optional<std::uint64_t> parseInterpolationWidth(
        std::string_view format) const noexcept;
    ast::AstNode* parseInterpolationFragment(
        const lexer::Token& token,
        const language::InterpolationPart& part,
        std::string_view label,
        const std::vector<std::string>& knownTypes,
        bool requireEnd);
    void appendInterpolationHoles(
        ast::AstNode* node,
        const lexer::Token& token,
        const std::vector<language::InterpolationPart>& parts);
    ast::AstNode* formatRuntimeInterpolationHole(
        const lexer::Token& token,
        const language::InterpolationPart& part,
        ast::AstNode* hole);
    ast::AstNode* lowerRuntimeInterpolation(
        const lexer::Token& token,
        const std::vector<language::InterpolationPart>& parts);
    ast::AstNode* finishInterpolatedString(
        ast::AstNode* node,
        const lexer::Token& token,
        const std::string& value);
    ast::AstNode* makeTokenPrimaryExpression(
        const lexer::Token& token,
        ast::AstNodeKind kind);
    ast::AstNode* parsePrimaryExpression();
    ast::AstNode* parseDeconstructionPattern();
    ast::AstNode* parseWithExpression(ast::AstNode* source);
    ast::AstNode* parseBinaryLevel(
        ast::AstNode* (Parser::*nextLevel)(),
        std::span<const lexer::TokenKind> operators,
        ast::AstNodeKind nodeKind);

    ast::AstNode* makeErrorExpression();
    void synchronizeGlobal();
    void synchronizeStatement();

    std::span<const lexer::Token> tokens_;
    ast::AstArena& arena_;
    diagnostics::DiagnosticEngine& diagnostics_;
    std::size_t index_ = 0;
    std::size_t pendingGenericGreater_ = 0;
    bool pendingGenericEqual_ = false;
    std::optional<lexer::Token> pendingGenericEqualToken_;
    std::size_t nextMatchTemporary_ = 0;
    std::size_t nextDeferTemporary_ = 0;
    struct ComprehensionContext final
    {
        std::string builderName;
        std::string loopLabel;
    };
    std::vector<ComprehensionContext> comprehensionContexts_;
    std::size_t nextComprehensionTemporary_ = 0;
    bool parsingExtensionBody_ = false;
    std::unordered_set<std::string> knownTypeNames_;
};
}
