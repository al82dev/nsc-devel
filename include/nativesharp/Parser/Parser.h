#pragma once

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Token.h"
#include "nativesharp/Language/LanguageEdition.h"

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
class Parser final
{
public:
    Parser(
        std::span<const lexer::Token> tokens,
        ast::AstArena& arena,
        diagnostics::DiagnosticEngine& diagnostics,
        std::span<const std::string> knownTypeNames = {},
        language::LanguageEdition languageEdition = language::LanguageEdition::V2);

    ast::AstNode* parseCompilationUnit();

private:
    const lexer::Token& current() const noexcept;
    const lexer::Token& peek(std::size_t distance = 1) const noexcept;
    bool at(lexer::TokenKind kind) const noexcept;
    bool atContextualKeyword(std::string_view keyword) const noexcept;
    bool expectContextualKeyword(std::string_view keyword, std::string_view description);
    bool consumeIf(lexer::TokenKind kind) noexcept;
    const lexer::Token& consume() noexcept;
    bool expect(lexer::TokenKind kind, std::string_view description);

    SourceRange join(SourceRange begin, SourceRange end) const noexcept;
    SourceRange tokenRangeFrom(std::size_t beginIndex) const noexcept;

    bool canStartType(const lexer::Token& token) const noexcept;
    bool canStartTypeAt(std::size_t tokenIndex) const noexcept;
    bool scanTypeAt(std::size_t tokenIndex, std::size_t& afterType) const noexcept;
    bool looksLikeVariableDeclaration() const noexcept;
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
    ast::AstNode* parseExtensionDeclaration();
    ast::AstNode* parseUsingDirective();
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
    // NativeSharp 3.0 T03. A closed tagged sum is materialized as an ordinary struct whose
    // payload carrier is an in-place union. Keeping the generated representation in the parser
    // means name lookup, generics, layout, HIR, MIR and LLVM all consume the same productive AST
    // shapes they already understand instead of a second, partially integrated backend path.
    ast::AstNode* parseVariantDeclaration();
    // NativeSharp 3.0 T04-F05. An opaque alias is represented by the ordinary nominal
    // single-field struct machinery. The parser synthesizes the private representation and the
    // module-internal explicit boundary operations, so every later phase sees one productive
    // value type rather than a parallel alias-only backend path.
    ast::AstNode* parseOpaqueTypeDeclaration();
    ast::AstNode* parseStructBody(
        ast::AstNode* declaration,
        std::string_view structName,
        std::size_t beginIndex);
    ast::AstNode* parseClassDeclaration(
        bool isPartial,
        bool isAbstract,
        bool isSealed,
        bool isStatic = false);
    // NativeSharp 3.0 T05-F02. A trait impl is represented as a class extension whose second()
    // names the target and whose sole base specification names the trait. Later phases merge the
    // extension with the locally-owned class and retain this source node for reusable NSMETA.
    ast::AstNode* parseTraitImplementationDeclaration();
    ast::AstNode* parseInterfaceDeclaration(bool isPartial, bool isTrait = false);
    ast::AstNode* parseDelegateDeclaration();
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
        std::size_t beginIndex);
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
        bool interfaceContext);
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
        bool interfaceContext);
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
        bool isNoHeap,
        std::size_t beginIndex);
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
        std::size_t beginIndex,
        bool allowBodyless = false);
    ast::AstNode* parseEnumDeclaration();
    ast::AstNode* parseExternCDeclaration();
    ast::AstNode* parseFunctionOrGlobalVariable();
    ast::AstNode* parseFunctionTail(ast::TypeSyntax* returnType, const lexer::Token& name, bool externalOnly);
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
        Or
    };
    enum class PatternBindingMode
    {
        Value,
        Reference,
        Move
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
        std::vector<std::string> fieldNames;
        std::vector<Pattern> children;
    };
    using PatternExpressionFactory = std::function<ast::AstNode*()>;
    Pattern parsePattern(bool implicitBinding);
    Pattern parsePatternPrimary(bool implicitBinding);
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
    ast::AstNode* parseVariableDeclaration(bool expectSemicolon);
    // C7 "aggregate initializer": the braced right-hand side of a declaration.
    ast::AstNode* parseAggregateInitializer();
    ast::AstNode* parseEmbeddedStatementBlock();
    ast::AstNode* parseIfStatement();
    ast::AstNode* parseWhileStatement();
    ast::AstNode* parseDoWhileStatement();
    ast::AstNode* parseSwitchStatement();
    // T03 statement match is lowered early to one hidden governing local plus an if/else decision
    // tree over generated case tests and guarded payload extractors. The source expression is
    // therefore evaluated exactly once and no runtime allocation or boxing protocol is introduced.
    ast::AstNode* parseMatchStatement();
    // True when the cursor sits on `case` or on the `default` OF A LABEL. `default` is also an
    // expression keyword (`default(T)`), so it only opens a label when a colon follows it
    // immediately -- otherwise a section that begins with `default(int)` would be misread.
    bool atSwitchLabel() const noexcept;
    ast::AstNode* parseForStatement();
    ast::AstNode* parseForeachStatement();
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
    ast::AstNode* parsePrimaryExpression();
    ast::AstNode* parseDeconstructionPattern();
    ast::AstNode* parseWithExpression(ast::AstNode* source);
    bool requireEdition3(SourceRange range, std::string_view feature);

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
    std::size_t nextMatchTemporary_ = 0;
    std::size_t nextDeferTemporary_ = 0;
    bool parsingExtensionBody_ = false;
    std::unordered_set<std::string> knownTypeNames_;
    language::LanguageEdition languageEdition_ = language::LanguageEdition::V2;
};
}
