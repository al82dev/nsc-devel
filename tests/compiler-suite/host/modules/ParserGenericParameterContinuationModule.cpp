#include "ParserFoundationTestSupport.h"

#include <algorithm>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addCase(
    TestRegistry& registry,
    std::string id,
    TestFunction function,
    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), "CP-PAR-008", "Host.Parser", "component", "PR",
        {"parser", "generic", "parameter"}, std::move(function), disposition});
}

AstNode makeDeclaration()
{
    return AstNode(AstNodeKind::FunctionDeclaration, {});
}

void testAbsentParameterList(TestContext& context)
{
    ParserFixture fixture("tail");
    AstNode declaration = makeDeclaration();
    const bool parsed = fixture.parser().parseGenericParameterList(&declaration);
    context.expect(parsed && declaration.genericParameterSyntaxes().empty() &&
            declaration.lifetimeParameters().empty() && fixture.parser().index_ == 0 &&
            fixture.parser().current().kind() == TokenKind::Identifier &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-070 absent '<' must produce an empty list without moving the cursor");
}

void testTwoTypeParameters(TestContext& context)
{
    ParserFixture fixture("<T,U>");
    AstNode declaration = makeDeclaration();
    const bool parsed = fixture.parser().parseGenericParameterList(&declaration);
    const auto& parameters = declaration.genericParameterSyntaxes();
    context.expect(parsed && parameters.size() == 2 && parameters[0].name == "T" &&
            parameters[0].kind == GenericParameterSyntaxKind::Type &&
            parameters[1].name == "U" && parameters[1].kind == GenericParameterSyntaxKind::Type &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-071 two generic type parameters must retain source order and kind");
}

void testLifetimeParameter(TestContext& context)
{
    ParserFixture fixture("<lifetime a>");
    AstNode declaration = makeDeclaration();
    const bool parsed = fixture.parser().parseGenericParameterList(&declaration);
    context.expect(parsed && declaration.genericParameterSyntaxes().empty() &&
            declaration.lifetimeParameters().size() == 1 &&
            declaration.lifetimeParameters()[0].name == "a" &&
            declaration.lifetimeParameters()[0].range.begin().offset() == 1 &&
            declaration.lifetimeParameters()[0].range.end().offset() == 11,
        "TP-PAR-072 lifetime parameter must retain exact kind, name, and source range");
}

void testConstParameter(TestContext& context)
{
    ParserFixture fixture("<const int N>");
    AstNode declaration = makeDeclaration();
    const bool parsed = fixture.parser().parseGenericParameterList(&declaration);
    const auto& parameters = declaration.genericParameterSyntaxes();
    context.expect(parsed && parameters.size() == 1 && parameters[0].name == "N" &&
            parameters[0].kind == GenericParameterSyntaxKind::Constant &&
            parameters[0].constantType != nullptr &&
            parameters[0].constantType->baseName() == "int" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-073 const generic parameter must retain its kind, name, and declared type");
}

void testTypeAndConstPacks(TestContext& context)
{
    ParserFixture typePack("<pack Ts>");
    ParserFixture constPack("<pack const uint Ns>");
    AstNode typeDeclaration = makeDeclaration();
    AstNode constDeclaration = makeDeclaration();
    const bool typeParsed = typePack.parser().parseGenericParameterList(&typeDeclaration);
    const bool constParsed = constPack.parser().parseGenericParameterList(&constDeclaration);
    const auto& typeParameters = typeDeclaration.genericParameterSyntaxes();
    const auto& constParameters = constDeclaration.genericParameterSyntaxes();
    context.expect(typeParsed && typeParameters.size() == 1 && typeParameters[0].name == "Ts" &&
            typeParameters[0].kind == GenericParameterSyntaxKind::TypePack &&
            typeParameters[0].constantType == nullptr && constParsed &&
            constParameters.size() == 1 && constParameters[0].name == "Ns" &&
            constParameters[0].kind == GenericParameterSyntaxKind::ConstantPack &&
            constParameters[0].constantType != nullptr &&
            constParameters[0].constantType->baseName() == "uint",
        "TP-PAR-074 type and const packs must preserve distinct kinds and const element type");
}

void testNonFinalPack(TestContext& context)
{
    ParserFixture fixture("<pack Ts,U>");
    AstNode declaration = makeDeclaration();
    const bool structurallyParsed = fixture.parser().parseGenericParameterList(&declaration);
    context.expect(structurallyParsed && declaration.genericParameterSyntaxes().size() == 2 &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidArgumentCount) == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-075 non-final pack must be fully consumed and diagnosed exactly once");
}

void testDuplicateNamesAcrossParameterClasses(TestContext& context)
{
    ParserFixture fixture("<T,const int T>");
    AstNode declaration = makeDeclaration();
    const bool parsed = fixture.parser().parseGenericParameterList(&declaration);
    context.expect(parsed && declaration.genericParameterSyntaxes().size() == 2 &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::DuplicateSymbol) == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-076 duplicate names across type and const classes must diagnose once");
}

void testEmptyAndMissingCommaLists(TestContext& context)
{
    ParserFixture empty("<>");
    ParserFixture missingComma("<T U>");
    AstNode emptyDeclaration = makeDeclaration();
    AstNode missingDeclaration = makeDeclaration();
    const bool emptyParsed = empty.parser().parseGenericParameterList(&emptyDeclaration);
    const bool missingParsed = missingComma.parser().parseGenericParameterList(&missingDeclaration);
    context.expect(!emptyParsed && empty.parser().current().kind() == TokenKind::Greater &&
            diagnosticCount(empty.diagnostics(), DiagnosticId::ExpectedToken) == 1,
        "TP-PAR-077 empty parameter list must fail at its closing delimiter without consuming it");
    context.expect(!missingParsed && missingDeclaration.genericParameterSyntaxes().size() == 1 &&
            missingComma.parser().current().kind() == TokenKind::Identifier &&
            diagnosticCount(missingComma.diagnostics(), DiagnosticId::ExpectedToken) == 1,
        "TP-PAR-077 missing comma must retain the unexpected parameter token for recovery");
}

void testNestedGenericParameterClose(TestContext& context)
{
    ParserFixture fixture("<const Box<List<int>> N>");
    AstNode declaration = makeDeclaration();
    const bool parsed = fixture.parser().parseGenericParameterList(&declaration);
    const auto& parameters = declaration.genericParameterSyntaxes();
    context.expect(parsed && parameters.size() == 1 && parameters[0].constantType != nullptr &&
            parameters[0].constantType->baseName() == "Box" &&
            parameters[0].constantType->typeArguments().size() == 1 &&
            parameters[0].constantType->typeArguments()[0]->baseName() == "List" &&
            parameters[0].constantType->typeArguments()[0]->typeArguments().size() == 1 &&
            parameters[0].constantType->typeArguments()[0]->typeArguments()[0]->baseName() == "int" &&
            fixture.parser().pendingGenericGreater_ == 0 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-078 nested generic close token must close both the type and parameter list");
}

void testKnownTypeScopeExcludesConstParameters(TestContext& context)
{
    ParserFixture fixture(
        "class Holder<T,const int N,pack Ts> { void Check() { T* first; Ts* second; N* third; } }");
    AstNode* const root = fixture.parse();
    AstNode* const declaration = root != nullptr && !root->children().empty()
        ? root->children()[0] : nullptr;
    AstNode* method = nullptr;
    if (declaration != nullptr)
    {
        for (AstNode* child : declaration->children())
        {
            if (child != nullptr && child->kind() == AstNodeKind::MethodDeclaration)
            {
                method = child;
                break;
            }
        }
    }
    AstNode* const block = method == nullptr ? nullptr : method->first();
    bool firstField = false;
    bool secondField = false;
    bool thirdField = false;
    if (block != nullptr)
    {
        for (const AstNode* child : block->children())
        {
            if (child == nullptr || child->kind() != AstNodeKind::VariableDeclarationStatement) continue;
            firstField = firstField || child->text() == "first";
            secondField = secondField || child->text() == "second";
            thirdField = thirdField || child->text() == "third";
        }
    }
    const std::size_t childCount = block == nullptr ? 0 : block->children().size();
    const std::size_t diagnosticTotal = fixture.diagnostics().diagnostics().size();
    context.expect(block != nullptr && firstField && secondField && !thirdField,
        "TP-PAR-079 type and type-pack parameters must enter type scope while const parameters do not; "
        "children=" + std::to_string(childCount) + " first=" + std::to_string(firstField) +
        " second=" + std::to_string(secondField) + " third=" + std::to_string(thirdField) +
        " diagnostics=" + std::to_string(diagnosticTotal));
}
}

void registerParserGenericParameterContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-070", testAbsentParameterList);
    addCase(registry, "TP-PAR-071", testTwoTypeParameters);
    addCase(registry, "TP-PAR-072", testLifetimeParameter);
    addCase(registry, "TP-PAR-073", testConstParameter);
    addCase(registry, "TP-PAR-074", testTypeAndConstPacks);
    addCase(registry, "TP-PAR-075", testNonFinalPack);
    addCase(registry, "TP-PAR-076", testDuplicateNamesAcrossParameterClasses);
    addCase(registry, "TP-PAR-077", testEmptyAndMissingCommaLists);
    addCase(registry, "TP-PAR-078", testNestedGenericParameterClose);
    addCase(registry, "TP-PAR-079", testKnownTypeScopeExcludesConstParameters);
}
}
