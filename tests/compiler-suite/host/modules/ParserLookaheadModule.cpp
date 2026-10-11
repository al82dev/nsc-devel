#include "ParserFoundationTestSupport.h"

#include <array>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace parser_support;

void testBuiltinAndKnownTypeStarts(TestContext& context)
{
    ParserFixture fixture(
        "void bool sbyte byte char short ushort int uint long ulong nint nuint i128 u128 "
        "float double string object Custom",
        {"Custom"});
    bool allStartTypes = true;
    for (std::size_t index = 0; index + 1 < fixture.tokens().size(); ++index)
    {
        allStartTypes = allStartTypes && fixture.parser().canStartType(fixture.tokens()[index]);
    }
    context.expect(allStartTypes,
        "TP-PAR-018 every builtin and externally known name must start a type");
}

void testUnknownIdentifierTypeStart(TestContext& context)
{
    ParserFixture unknown("Mystery value");
    ParserFixture explicitModule("Module::Mystery value");
    std::size_t afterType = 0;
    context.expect(!unknown.parser().canStartType(unknown.tokens()[0]) &&
            explicitModule.parser().scanTypeAt(0, afterType) &&
            explicitModule.tokens()[afterType].lexeme() == "value",
        "TP-PAR-019 unknown identifier must fail unless syntax is unambiguously module-qualified");
}

void testOwnershipAndLifetimeScan(TestContext& context)
{
    ParserFixture owned("owned Custom tail", {"Custom"});
    ParserFixture borrowed("borrowed Custom tail", {"Custom"});
    ParserFixture lifetime("borrowed[a] Custom tail", {"Custom"});
    std::size_t ownedAfter = 0;
    std::size_t borrowedAfter = 0;
    std::size_t lifetimeAfter = 0;
    context.expect(owned.parser().scanTypeAt(0, ownedAfter) && ownedAfter == 2 &&
            borrowed.parser().scanTypeAt(0, borrowedAfter) && borrowedAfter == 2 &&
            lifetime.parser().scanTypeAt(0, lifetimeAfter) && lifetimeAfter == 5,
        "TP-PAR-020 ownership and lifetime qualifiers must scan through the complete type");
}

void testQualifiedNameLookahead(TestContext& context)
{
    ParserFixture fixture("A.B.C tail", {"A.B.C"});
    std::size_t afterType = 0;
    context.expect(fixture.parser().scanTypeAt(0, afterType) && afterType == 5,
        "TP-PAR-021 lookahead must consume the complete A.B.C type name");
}

void testNestedTupleAndFunctionLookahead(TestContext& context)
{
    ParserFixture tuple("(int,fn&()->void) tail");
    std::size_t afterTuple = 0;
    context.expect(tuple.parser().scanTypeAt(0, afterTuple) &&
            tuple.tokens()[afterTuple].lexeme() == "tail",
        "TP-PAR-022 nested tuple/function lookahead must balance all delimiters");
}

void testNestedGenericGreaterGreater(TestContext& context)
{
    ParserFixture fixture("Outer<Inner<int>> tail", {"Outer", "Inner"});
    std::size_t afterType = 0;
    context.expect(fixture.parser().scanTypeAt(0, afterType) &&
            fixture.tokens()[afterType].lexeme() == "tail",
        "TP-PAR-023 nested generic >> must close both levels at exact boundary");
}

void testNestedGenericGreaterGreaterEqual(TestContext& context)
{
    ParserFixture fixture("Outer<Inner<int>>= value", {"Outer", "Inner"});
    nativesharp::ast::TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->baseName() == "Outer" &&
            type->typeArguments().size() == 1 &&
            type->typeArguments()[0]->baseName() == "Inner" &&
            type->typeArguments()[0]->typeArguments().size() == 1 &&
            type->typeArguments()[0]->typeArguments()[0]->baseName() == "int" &&
            fixture.parser().pendingGenericGreater_ == 0 &&
            fixture.parser().current().kind() == TokenKind::Equal,
        "TP-PAR-024 >>= must close generic levels while preserving assignment remainder");
}

void testPointerArrayModifierLookahead(TestContext& context)
{
    ParserFixture fixture("Widget*? align(16)[][,,] tail", {"Widget"});
    std::size_t afterType = 0;
    context.expect(fixture.parser().scanTypeAt(0, afterType) &&
            fixture.tokens()[afterType].lexeme() == "tail",
        "TP-PAR-025 pointer and array lookahead must consume every modifier");
}

void testVariableDeclarationKnownVersusUnknown(TestContext& context)
{
    ParserFixture known("T value;", {"T"});
    ParserFixture unknown("Mystery value;");
    context.expect(known.parser().looksLikeVariableDeclaration() &&
            unknown.parser().looksLikeVariableDeclaration(),
        "TP-PAR-026 unambiguous type-shaped declarations must reach semantic name resolution");
}

void testConditionDeclarationLookahead(TestContext& context)
{
    ParserFixture declaration("T value = expression", {"T"});
    ParserFixture noInitializer("T value", {"T"});
    context.expect(declaration.parser().looksLikeConditionDeclaration() &&
            !noInitializer.parser().looksLikeConditionDeclaration(),
        "TP-PAR-027 condition declaration must contain type, name, and initializer");
}

void testParenthesizedTypeLookahead(TestContext& context)
{
    ParserFixture cast("(T)value", {"T"});
    ParserFixture expression("(a+b)");
    context.expect(cast.parser().looksLikeParenthesizedType() &&
            !expression.parser().looksLikeParenthesizedType(),
        "TP-PAR-028 cast lookahead must not confuse parenthesized arithmetic");
}

void testGenericTypeScopeRemoval(TestContext& context)
{
    ParserFixture fixture("struct Box<T> { T value; }");
    AstNode* const root = fixture.parse();
    context.expect(root->children().size() == 1 && !fixture.parser().knownTypeNames_.contains("T"),
        "TP-PAR-029 generic type parameter must be visible in body and removed afterward");
}

void testGenericScopePreservesExteriorShadow(TestContext& context)
{
    ParserFixture fixture("struct Box<T> { T value; }", {"T"});
    AstNode* const root = fixture.parse();
    context.expect(root->children().size() == 1 && fixture.parser().knownTypeNames_.contains("T"),
        "TP-PAR-030 generic scope guard must not erase an exterior type name");
}

void testQualifiedNameTextAndRange(TestContext& context)
{
    ParserFixture simple("A tail");
    std::string simpleName;
    nativesharp::SourceRange simpleRange;
    const bool simpleOk = simple.parser().parseQualifiedName(simpleName, simpleRange);
    ParserFixture qualified("A.B.C tail");
    std::string qualifiedName;
    nativesharp::SourceRange qualifiedRange;
    const bool qualifiedOk = qualified.parser().parseQualifiedName(qualifiedName, qualifiedRange);
    context.expect(simpleOk && simpleName == "A" && simpleRange.begin().offset() == 0 &&
            simpleRange.end().offset() == 1 && qualifiedOk && qualifiedName == "A.B.C" &&
            qualifiedRange.begin().offset() == 0 && qualifiedRange.end().offset() == 5,
        "TP-PAR-031 qualified name text and complete range must be exact");
}

void testQualifiedNameMissingSegment(TestContext& context)
{
    ParserFixture fixture("A.");
    std::string name;
    nativesharp::SourceRange range;
    const bool result = fixture.parser().parseQualifiedName(name, range);
    context.expect(!result && name == "A" && fixture.parser().index_ == 2 &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) == 1,
        "TP-PAR-032 A. must report missing component and preserve completed prefix");
}

void testQualifiedNameRejectsNonIdentifier(TestContext& context)
{
    ParserFixture fixture("int");
    std::string name = "stale";
    nativesharp::SourceRange range;
    const bool result = fixture.parser().parseQualifiedName(name, range);
    context.expect(!result && name.empty() && fixture.parser().index_ == 0 &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) == 1,
        "TP-PAR-033 non-Identifier must fail without falsely completing output");
}

void testAbsentLifetimeAnnotation(TestContext& context)
{
    ParserFixture fixture("T");
    const std::string lifetime = fixture.parser().parseOptionalLifetimeAnnotation("borrow");
    context.expect(lifetime.empty() && fixture.parser().index_ == 0,
        "TP-PAR-034 absent lifetime annotation must leave cursor intact");
}

void testValidAndInvalidLifetimeAnnotation(TestContext& context)
{
    ParserFixture valid("[a] tail");
    const std::string validName = valid.parser().parseOptionalLifetimeAnnotation("borrow");
    ParserFixture invalid("[123] tail");
    const auto invalidRange = invalid.tokens()[1].range();
    const std::string invalidName = invalid.parser().parseOptionalLifetimeAnnotation("borrow");
    const auto& diagnostics = invalid.diagnostics().diagnostics();
    context.expect(validName == "a" && valid.parser().index_ == 3 && invalidName.empty(),
        "TP-PAR-035 valid lifetime must return its name and invalid lifetime must be empty");
    context.expect(!diagnostics.empty() && diagnostics[0].id() == DiagnosticId::ExpectedToken &&
            diagnostics[0].range().has_value() &&
            diagnostics[0].range()->begin() == invalidRange.begin(),
        "TP-PAR-035 invalid lifetime must diagnose the offending range");
}
}

void registerParserLookaheadModule(TestRegistry& registry)
{
    addParserCase(registry, "TP-PAR-018", "CP-PAR-003", "unit", "PR", testBuiltinAndKnownTypeStarts);
    addParserCase(registry, "TP-PAR-019", "CP-PAR-003", "unit", "PR", testUnknownIdentifierTypeStart);
    addParserCase(registry, "TP-PAR-020", "CP-PAR-003", "unit", "PR", testOwnershipAndLifetimeScan);
    addParserCase(registry, "TP-PAR-021", "CP-PAR-003", "unit", "PR", testQualifiedNameLookahead);
    addParserCase(registry, "TP-PAR-022", "CP-PAR-003", "unit", "PR", testNestedTupleAndFunctionLookahead);
    addParserCase(registry, "TP-PAR-023", "CP-PAR-003", "unit", "PR", testNestedGenericGreaterGreater);
    addParserCase(registry, "TP-PAR-024", "CP-PAR-003", "unit", "PR", testNestedGenericGreaterGreaterEqual);
    addParserCase(registry, "TP-PAR-025", "CP-PAR-003", "unit", "PR", testPointerArrayModifierLookahead);
    addParserCase(registry, "TP-PAR-026", "CP-PAR-003", "unit", "PR", testVariableDeclarationKnownVersusUnknown);
    addParserCase(registry, "TP-PAR-027", "CP-PAR-003", "unit", "PR", testConditionDeclarationLookahead);
    addParserCase(registry, "TP-PAR-028", "CP-PAR-003", "unit", "PR", testParenthesizedTypeLookahead);
    addParserCase(registry, "TP-PAR-029", "CP-PAR-003", "unit", "PR", testGenericTypeScopeRemoval);
    addParserCase(registry, "TP-PAR-030", "CP-PAR-003", "unit", "PR", testGenericScopePreservesExteriorShadow);
    addParserCase(registry, "TP-PAR-031", "CP-PAR-004", "unit", "PR", testQualifiedNameTextAndRange);
    addParserCase(registry, "TP-PAR-032", "CP-PAR-004", "unit", "PR", testQualifiedNameMissingSegment);
    addParserCase(registry, "TP-PAR-033", "CP-PAR-004", "unit", "PR", testQualifiedNameRejectsNonIdentifier);
    addParserCase(registry, "TP-PAR-034", "CP-PAR-004", "unit", "PR", testAbsentLifetimeAnnotation);
    addParserCase(registry, "TP-PAR-035", "CP-PAR-004", "unit", "PR", testValidAndInvalidLifetimeAnnotation);
}
}
