#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <string>
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

void parse(
    ParsedFixture& fixture,
    std::string source,
    const std::vector<std::string>& knownTypes = {})
{
    const nativesharp::FileId fileId = fixture.sources.addBuffer("namespace_syntax.ns", std::move(source));
    const nativesharp::source::SourceFile* file = fixture.sources.getFile(fileId);
    nativesharp::lexer::Lexer lexer(*file, fixture.diagnostics);
    fixture.tokens = lexer.lexAll();
    nativesharp::parser::Parser parser(
        fixture.tokens,
        fixture.arena,
        fixture.diagnostics,
        knownTypes);
    fixture.root = parser.parseCompilationUnit();
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
            "using Game.Shared;\n"
            "using G = Game.Shared;\n"
            "namespace Game.Core\n"
            "{\n"
            "    public struct Point { int X; }\n"
            "    namespace Nested\n"
            "    {\n"
            "        internal enum Mode { A, B }\n"
            "    }\n"
            "}\n");

        tests.expect(!fixture.diagnostics.hasErrors(), "block namespace syntax parses without diagnostics");
        tests.expect(fixture.root != nullptr && fixture.root->children().size() == 3,
            "compilation unit retains two using directives plus namespace declaration");
        if (fixture.root != nullptr && fixture.root->children().size() == 3)
        {
            const AstNode* usingNamespace = fixture.root->children()[0];
            const AstNode* usingAlias = fixture.root->children()[1];
            const AstNode* outer = fixture.root->children()[2];
            tests.expect(usingNamespace->kind() == AstNodeKind::UsingNamespaceDirective &&
                    usingNamespace->text() == "Game.Shared",
                "using namespace AST preserves qualified target");
            tests.expect(usingAlias->kind() == AstNodeKind::UsingAliasDirective &&
                    usingAlias->text() == "G" && usingAlias->auxiliaryText() == "Game.Shared",
                "using alias AST preserves alias and target separately");
            tests.expect(outer->kind() == AstNodeKind::NamespaceDeclaration &&
                    outer->text() == "Game.Core" && !outer->flag(),
                "block namespace AST preserves qualified name and block form");
            tests.expect(outer->children().size() == 2,
                "block namespace owns its type and nested namespace declarations");
            if (outer->children().size() == 2)
            {
                tests.expect(outer->children()[0]->kind() == AstNodeKind::StructDeclaration &&
                        outer->children()[0]->tokenKind() == TokenKind::KwPublic,
                    "public top-level struct accessibility is represented on syntax declaration");
                const AstNode* nested = outer->children()[1];
                tests.expect(nested->kind() == AstNodeKind::NamespaceDeclaration &&
                        nested->text() == "Nested",
                    "nested open namespace is represented structurally");
                tests.expect(!nested->children().empty() &&
                        nested->children()[0]->kind() == AstNodeKind::EnumDeclaration &&
                        nested->children()[0]->tokenKind() == TokenKind::KwInternal,
                    "explicit internal enum accessibility is retained in AST");
            }
        }
    }

    {
        ParsedFixture fixture;
        parse(
            fixture,
            "namespace Game.Client;\n"
            "using Alias = Game.Shared;\n"
            "struct Holder { Game.Shared.Point Value; }\n"
            "int F() { Alias.Point p; return 0; }\n",
            {"Alias.Point", "Game.Shared.Point"});

        tests.expect(!fixture.diagnostics.hasErrors(), "file-scoped namespace and qualified type syntax parse");
        tests.expect(fixture.root != nullptr && fixture.root->children().size() == 1,
            "file-scoped namespace consumes the remaining compilation-unit declarations");
        if (fixture.root != nullptr && fixture.root->children().size() == 1)
        {
            const AstNode* fileNamespace = fixture.root->children()[0];
            tests.expect(fileNamespace->kind() == AstNodeKind::NamespaceDeclaration &&
                    fileNamespace->flag() && fileNamespace->text() == "Game.Client",
                "file-scoped namespace is explicitly marked in AST");
            tests.expect(fileNamespace->children().size() == 3,
                "file-scoped namespace owns using, struct, and function declarations");
            if (fileNamespace->children().size() >= 2)
            {
                const AstNode* holder = fileNamespace->children()[1];
                tests.expect(holder->kind() == AstNodeKind::StructDeclaration &&
                        !holder->children().empty() &&
                        holder->children()[0]->typeSyntax() != nullptr &&
                        holder->children()[0]->typeSyntax()->baseName() == "Game.Shared.Point",
                    "qualified named type is stored as one TypeSyntax base name");
            }
            if (fileNamespace->children().size() >= 3)
            {
                const AstNode* function = fileNamespace->children()[2];
                const AstNode* body = function->first();
                tests.expect(body != nullptr && !body->children().empty() &&
                        body->children()[0]->kind() == AstNodeKind::VariableDeclarationStatement &&
                        body->children()[0]->typeSyntax() != nullptr &&
                        body->children()[0]->typeSyntax()->baseName() == "Alias.Point",
                    "known qualified alias type disambiguates local declaration syntax");
            }
        }
    }

    return tests.result();
}
