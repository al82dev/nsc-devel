#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/FileId.h"

#include <array>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::SourceRange;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using nativesharp::lexer::TokenKind;

// Builds an explicit range used to verify AstNode identity and replacement semantics. Complexity is O(1).
constexpr SourceRange makeRange(std::uint32_t file, std::uint32_t begin, std::uint32_t end)
{
    return SourceRange(SourceLocation(FileId{file}, begin), SourceLocation(FileId{file}, end));
}

// Compares all public coordinates of two ranges. Complexity is O(1).
constexpr bool sameRange(SourceRange left, SourceRange right)
{
    return left.begin() == right.begin() && left.end() == right.end();
}

// Registers one AstNode identity or scalar-text case. Complexity is O(1).
void addIdentityCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{
        std::move(id), "CA-AST-005", "Host.AST", "unit", "PR", {"ast", "node", "identity"},
        std::move(function),
    });
}

// Registers constructor preservation across representative node kinds and ranges. Complexity is O(n).
void registerConstructorCase(TestRegistry& registry)
{
    addIdentityCase(registry, "TA-AST-041", [](TestContext& context) {
        constexpr std::array kinds{
            AstNodeKind::CompilationUnit, AstNodeKind::FunctionDeclaration, AstNodeKind::BlockStatement};
        for (std::size_t index = 0; index < kinds.size(); ++index)
        {
            const SourceRange range = makeRange(41, static_cast<std::uint32_t>(index),
                static_cast<std::uint32_t>(index + 1));
            const AstNode node(kinds[index], range);
            context.expect(node.kind() == kinds[index], "TA-AST-041 constructor kind must be exact");
            context.expect(sameRange(node.range(), range), "TA-AST-041 constructor range must be exact");
        }
    });
}

// Registers last-write-wins replacement for the mutable source range. Complexity is O(1).
void registerSetRangeCase(TestRegistry& registry)
{
    addIdentityCase(registry, "TA-AST-042", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, makeRange(42, 0, 1));
        node.setRange(makeRange(42, 2, 3));
        const SourceRange last = makeRange(42, 8, 13);
        node.setRange(last);
        context.expect(sameRange(node.range(), last), "TA-AST-042 final range must replace prior ranges");
    });
}

// Registers exact owned text for empty, nonempty, and UTF-8 byte sequences. Complexity is O(n).
void registerTextCase(TestRegistry& registry)
{
    addIdentityCase(registry, "TA-AST-043", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setText("");
        context.expect(node.text().empty(), "TA-AST-043 empty text must remain empty");
        node.setText("alpha");
        context.expect(node.text() == "alpha", "TA-AST-043 ASCII text must be exact");
        const std::string utf8{"na\xc3\xafve"};
        node.setText(utf8);
        context.expect(node.text() == utf8, "TA-AST-043 UTF-8 bytes must be exact");
    });
}

// Registers that AstNode owns text after the caller's temporary string is destroyed. Complexity is O(n).
void registerTemporaryTextCase(TestRegistry& registry)
{
    addIdentityCase(registry, "TA-AST-044", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setText(std::string("temporary-owned-text"));
        context.expect(node.text() == "temporary-owned-text", "TA-AST-044 text must survive temporary input");
    });
}

// Registers independent auxiliary-text and named-argument channels. Complexity is O(n).
void registerIndependentStringsCase(TestRegistry& registry)
{
    addIdentityCase(registry, "TA-AST-045", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setAuxiliaryText("auxiliary");
        node.setArgumentName("argument");
        context.expect(node.auxiliaryText() == "auxiliary", "TA-AST-045 auxiliary text must be exact");
        context.expect(node.argumentName() == "argument", "TA-AST-045 argument name must be exact");
        context.expect(node.text().empty(), "TA-AST-045 ordinary text must remain independent");
    });
}

// Registers the invalid-token default. Complexity is O(1).
void registerTokenDefaultCase(TestRegistry& registry)
{
    addIdentityCase(registry, "TA-AST-046", [](TestContext& context) {
        const AstNode node(AstNodeKind::CompilationUnit, {});
        context.expect(node.tokenKind() == TokenKind::Invalid, "TA-AST-046 token kind must default Invalid");
    });
}

// Registers representative punctuation, keyword, identifier, and literal token assignments. Complexity is O(n).
void registerTokenKindsCase(TestRegistry& registry)
{
    addIdentityCase(registry, "TA-AST-047", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        constexpr std::array kinds{
            TokenKind::Identifier, TokenKind::IntegerLiteral, TokenKind::KwPublic, TokenKind::Plus};
        for (const TokenKind kind : kinds)
        {
            node.setTokenKind(kind);
            context.expect(node.tokenKind() == kind, "TA-AST-047 assigned token kind must be exact");
        }
    });
}

// Registers repeated writes without cross-channel mutation among owned strings. Complexity is O(n).
void registerStringReplacementCase(TestRegistry& registry)
{
    addIdentityCase(registry, "TA-AST-048", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setText("first");
        node.setAuxiliaryText("aux-first");
        node.setArgumentName("arg-first");
        node.setText("last");
        node.setAuxiliaryText("aux-last");
        node.setArgumentName("arg-last");
        context.expect(node.text() == "last", "TA-AST-048 text must retain its last value");
        context.expect(node.auxiliaryText() == "aux-last", "TA-AST-048 auxiliary must retain its last value");
        context.expect(node.argumentName() == "arg-last", "TA-AST-048 argument must retain its last value");
    });
}

// Registers immutable kind identity across every mutable setter in this contract. Complexity is O(1).
void registerImmutableKindCase(TestRegistry& registry)
{
    addIdentityCase(registry, "TA-AST-049", [](TestContext& context) {
        AstNode node(AstNodeKind::FunctionDeclaration, makeRange(49, 1, 2));
        node.setRange(makeRange(49, 3, 4));
        node.setText("text");
        node.setAuxiliaryText("aux");
        node.setArgumentName("arg");
        node.setTokenKind(TokenKind::KwPublic);
        context.expect(node.kind() == AstNodeKind::FunctionDeclaration,
            "TA-AST-049 kind must remain immutable after setters");
    });
}

// Registers preservation of an externally constructed invalid range. Complexity is O(1).
void registerInvalidRangeCase(TestRegistry& registry)
{
    addIdentityCase(registry, "TA-AST-050", [](TestContext& context) {
        const SourceRange invalid = makeRange(50, 9, 2);
        const AstNode node(AstNodeKind::CompilationUnit, invalid);
        context.expect(!node.range().isValid(), "TA-AST-050 invalid range must remain invalid");
        context.expect(sameRange(node.range(), invalid), "TA-AST-050 invalid range bytes must remain exact");
    });
}
}

// Registers CA-AST-005 constructor, range, owned-text, token, and immutable-kind contracts.
void registerAstNodeIdentityModule(TestRegistry& registry)
{
    registerConstructorCase(registry);
    registerSetRangeCase(registry);
    registerTextCase(registry);
    registerTemporaryTextCase(registry);
    registerIndependentStringsCase(registry);
    registerTokenDefaultCase(registry);
    registerTokenKindsCase(registry);
    registerStringReplacementCase(registry);
    registerImmutableKindCase(registry);
    registerInvalidRangeCase(registry);
}
}
