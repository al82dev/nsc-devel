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
    bool canStartTypeAt(std::size_t tokenIndex) const noexcept;
    bool scanTypeAt(std::size_t tokenIndex, std::size_t& afterType) const noexcept;
    bool looksLikeVariableDeclaration() const noexcept;
    bool looksLikeLocalFunction() const noexcept;
    ast::AstNode* parseLocalFunctionAsLambda();
    bool looksLikeConditionDeclaration() const noexcept;
    bool looksLikeParenthesizedType() const noexcept;
    bool parseQualifiedName(std::string& name, SourceRange& range);
    ast::TypeSyntax* parseType();
    std::string parseOptionalLifetimeAnnotation(std::string_view context);
    ast::TypeSyntax* parseAllocationElementType();
    bool parseGenericParameterList(ast::AstNode* declaration);
    // NSC-LIMIT-007. Cero o mas clausulas `where T : class, Base, IFoo`, en la posicion donde el
    // lenguaje las admite: despues de la lista de parametros de un metodo o funcion, despues de
    // las bases de una clase, despues de la lista de parametros de un delegado, y despues del
    // nombre en un struct. Devuelve false si alguna clausula estaba mal formada.
    bool parseGenericConstraintClauses(ast::AstNode* declaration);
    bool parseTypeArgumentList(std::vector<ast::TypeSyntax*>& arguments);
    bool consumeGenericGreater();
    static bool canAppearInTypeArgumentList(lexer::TokenKind kind) noexcept;
    bool looksLikeGenericCallTypeArguments() const noexcept;
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
    ast::AstNode* parseGlobalDeclaration();
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
    ast::AstNode* parseAssociatedItem(bool implementation);
    ast::AstNode* parseClassMember(
        std::string_view className,
        bool allowUnresolvedNamedTypes = false,
        bool structContext = false);
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
    ast::AstNode* parseEventTail(
        lexer::TokenKind accessibility,
        bool isNew,
        bool isStatic,
        std::size_t beginIndex,
        bool isPartial = false);
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
    ast::AstNode* parseOperatorTail(
        ast::TypeSyntax* returnType,
        lexer::TokenKind accessibility,
        bool isStatic,
        std::size_t beginIndex,
        bool allowBodyless = false);
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
    ast::AstNode* parseEnumDeclaration();
    ast::AstNode* parseExternCDeclaration();
    void parseExternCBlock(ast::AstNode* declaration);
    bool parseExternCSingle(ast::AstNode* declaration);
    ast::AstNode* parseFunctionOrGlobalVariable();
    ast::AstNode* parseFunctionTail(ast::TypeSyntax* returnType, const lexer::Token& name, bool externalOnly);
    void parseThrowsClause(ast::AstNode* callable);
    void parseFloatingPointModeClause(ast::AstNode* callable);
      void parseFfiUnwindClause(ast::AstNode* callable);
    ast::AstNode* parseParameter();

    ast::AstNode* parseBlock();
    ast::AstNode* parseStatement();
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
    Pattern parsePattern(bool implicitBinding);
    Pattern parseAndPattern(bool implicitBinding);
    Pattern parsePatternPrimary(bool implicitBinding);
    bool patternHasBindings(const Pattern& pattern) const;
    std::vector<Pattern> expandPatternAlternatives(const Pattern& pattern) const;
    ast::AstNode* makeBooleanLiteral(const SourceRange& range, bool value);
    bool patternIsConditionOnly(const Pattern& pattern) const;
    ast::AstNode* makePatternCondition(
        const Pattern& pattern,
        const PatternExpressionFactory& source);
    ast::AstNode* lowerPattern(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* continuation,
        std::string_view temporaryPrefix,
        bool emitBindings = true);
    void emitPatternBindings(
        const Pattern& pattern,
        const PatternExpressionFactory& source,
        ast::AstNode* destination,
        std::string_view temporaryPrefix);
    ast::AstNode* parseConditionalPatternStatement();
    bool statementAlwaysDiverges(const ast::AstNode* statement) const;
    ast::AstNode* parseStaticAssertStatement();
    ast::AstNode* parseInlineAssemblyStatement();
    void parseInlineAssemblyOutputClause(ast::AstNode* statement);
    void parseInlineAssemblyInputClause(ast::AstNode* statement);
    void parseInlineAssemblyClobberClause(ast::AstNode* statement);
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
    ast::AstNode* parseArgument();
    ast::AstNode* parseAssignmentExpression();
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
    ast::AstNode* parseUnaryExpression();
    ast::AstNode* parsePostfixExpression();
    bool questionBeginsConditional() const noexcept;
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
