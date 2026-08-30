#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <string>
#include <utility>
#include <vector>

namespace
{
struct ParsedFixture final
{
    nativesharp::source::SourceManager sources;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    nativesharp::ast::AstArena arena;
    std::vector<nativesharp::lexer::Token> tokens;
    nativesharp::ast::AstNode* root = nullptr;
};

void parse(ParsedFixture& fixture, std::string source)
{
    const nativesharp::FileId fileId = fixture.sources.addBuffer(
        "class_declaration.ns",
        std::move(source));
    const nativesharp::source::SourceFile* file = fixture.sources.getFile(fileId);
    nativesharp::lexer::Lexer lexer(*file, fixture.diagnostics);
    fixture.tokens = lexer.lexAll();
    nativesharp::parser::Parser parser(fixture.tokens, fixture.arena, fixture.diagnostics);
    fixture.root = parser.parseCompilationUnit();
}

bool hasDiagnostic(
    const ParsedFixture& fixture,
    nativesharp::diagnostics::DiagnosticId id)
{
    for (const nativesharp::diagnostics::Diagnostic& diagnostic :
         fixture.diagnostics.diagnostics())
    {
        if (diagnostic.id() == id)
        {
            return true;
        }
    }
    return false;
}
}

int main()
{
    using namespace nativesharp;
    using namespace nativesharp::ast;
    using namespace nativesharp::lexer;
    using nativesharp::tests::TestContext;

    TestContext tests;

    {
        ParsedFixture fixture;
        parse(
            fixture,
            "class Plain {}\n"
            "public class PublicType {}\n"
            "internal class InternalType {}\n"
            "partial class OnePart {}\n"
            "public partial class PublicPartial {}\n"
            "partial public class ModifierOrder {}\n"
            "int partial;\n"
            "struct partial { int X; }\n"
            "partial contextualValue;\n");

        tests.expect(!fixture.diagnostics.hasErrors(),
            "Stage 26 top-level class forms parse without diagnostics");
        tests.expect(fixture.root != nullptr && fixture.root->children().size() == 9,
            "all class declarations plus contextual-keyword variable are retained");
        if (fixture.root != nullptr && fixture.root->children().size() == 9)
        {
            const AstNode* plain = fixture.root->children()[0];
            const AstNode* publicType = fixture.root->children()[1];
            const AstNode* internalType = fixture.root->children()[2];
            const AstNode* onePart = fixture.root->children()[3];
            const AstNode* publicPartial = fixture.root->children()[4];
            const AstNode* modifierOrder = fixture.root->children()[5];
            const AstNode* contextualVariable = fixture.root->children()[6];
            const AstNode* contextualType = fixture.root->children()[7];
            const AstNode* contextualTypeUse = fixture.root->children()[8];

            tests.expect(
                plain->kind() == AstNodeKind::ClassDeclaration &&
                    plain->text() == "Plain" &&
                    plain->tokenKind() == TokenKind::Invalid &&
                    !plain->flag(),
                "plain class AST preserves name, default accessibility and non-partial state");
            tests.expect(
                publicType->kind() == AstNodeKind::ClassDeclaration &&
                    publicType->tokenKind() == TokenKind::KwPublic,
                "public class accessibility is retained on the declaration AST");
            tests.expect(
                internalType->kind() == AstNodeKind::ClassDeclaration &&
                    internalType->tokenKind() == TokenKind::KwInternal,
                "explicit internal class accessibility is retained");
            tests.expect(
                onePart->kind() == AstNodeKind::ClassDeclaration && onePart->flag(),
                "a single partial class is represented as partial without requiring another part");
            tests.expect(
                publicPartial->kind() == AstNodeKind::ClassDeclaration &&
                    publicPartial->flag() &&
                    publicPartial->tokenKind() == TokenKind::KwPublic,
                "public partial class preserves both independent modifiers");
            tests.expect(
                modifierOrder->kind() == AstNodeKind::ClassDeclaration &&
                    modifierOrder->flag() &&
                    modifierOrder->tokenKind() == TokenKind::KwPublic,
                "partial/public modifier order does not change declaration semantics");
            tests.expect(
                contextualVariable->kind() == AstNodeKind::GlobalVariableDeclaration &&
                    contextualVariable->text() == "partial",
                "partial remains legal as an identifier outside type-modifier position");
            tests.expect(
                contextualType->kind() == AstNodeKind::StructDeclaration &&
                    contextualType->text() == "partial",
                "partial remains legal as a declared type name");
            tests.expect(
                contextualTypeUse->kind() == AstNodeKind::GlobalVariableDeclaration &&
                    contextualTypeUse->typeSyntax() != nullptr &&
                    contextualTypeUse->typeSyntax()->baseName() == "partial",
                "contextual partial can be parsed as a type name when it does not introduce a modifier");
        }
    }

    {
        ParsedFixture fixture;
        parse(
            fixture,
            "namespace Game.Core\n"
            "{\n"
            "    partial class BlockType {}\n"
            "}\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "partial class parses inside a block namespace");
        tests.expect(
            fixture.root != nullptr &&
                fixture.root->children().size() == 1 &&
                fixture.root->children()[0]->children().size() == 1 &&
                fixture.root->children()[0]->children()[0]->kind() == AstNodeKind::ClassDeclaration,
            "block namespace structurally owns its class declaration");
    }

    {
        ParsedFixture fixture;
        parse(
            fixture,
            "namespace Game.Client;\n"
            "public partial class FileScopedType {}\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "public partial class parses inside a file-scoped namespace");
        tests.expect(
            fixture.root != nullptr &&
                fixture.root->children().size() == 1 &&
                fixture.root->children()[0]->flag() &&
                fixture.root->children()[0]->children().size() == 1 &&
                fixture.root->children()[0]->children()[0]->kind() == AstNodeKind::ClassDeclaration,
            "file-scoped namespace structurally owns its class declaration");
    }

    {
        ParsedFixture fixture;
        parse(
            fixture,
            "class HasMembers { private int Value; public int Get() { return this.Value; } }\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "Stage 26 accepts instance field and method declarations in class bodies");
        tests.expect(
            fixture.root != nullptr && fixture.root->children().size() == 1 &&
                fixture.root->children()[0]->children().size() == 2 &&
                fixture.root->children()[0]->children()[0]->kind() == AstNodeKind::FieldDeclaration &&
                fixture.root->children()[0]->children()[1]->kind() == AstNodeKind::MethodDeclaration,
            "class AST retains field and method members in source order");
    }

    {
        ParsedFixture fixture;
        parse(fixture, "partial partial class DuplicatePartial {}\n");
        tests.expect(fixture.diagnostics.hasErrors(),
            "duplicate partial modifier is rejected");
        tests.expect(hasDiagnostic(fixture, diagnostics::DiagnosticId::ExpectedDeclaration),
            "duplicate partial modifier produces a syntax diagnostic");
    }

    {
        ParsedFixture fixture;
        parse(fixture, "public internal class ConflictingAccess {}\n");
        tests.expect(fixture.diagnostics.hasErrors(),
            "conflicting top-level accessibility modifiers are rejected");
    }

    {
        ParsedFixture fixture;
        parse(fixture, "partial struct NotYetSupported { int X; }\n");
        tests.expect(fixture.diagnostics.hasErrors(),
            "Stage 26 does not accidentally enable partial struct syntax");
    }

    return tests.result();
}
