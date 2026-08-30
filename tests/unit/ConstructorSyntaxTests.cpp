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
struct Fixture final
{
    nativesharp::source::SourceManager sources;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    nativesharp::ast::AstArena arena;
    std::vector<nativesharp::lexer::Token> tokens;
    nativesharp::ast::AstNode* root = nullptr;
};

void parse(Fixture& fixture, std::string source)
{
    const nativesharp::FileId fileId = fixture.sources.addBuffer("constructors.ns", std::move(source));
    const nativesharp::source::SourceFile* file = fixture.sources.getFile(fileId);
    nativesharp::lexer::Lexer lexer(*file, fixture.diagnostics);
    fixture.tokens = lexer.lexAll();
    const std::vector<std::string> knownTypes = {"Widget"};
    nativesharp::parser::Parser parser(
        fixture.tokens, fixture.arena, fixture.diagnostics, knownTypes);
    fixture.root = parser.parseCompilationUnit();
}

bool hasDiagnostic(const Fixture& fixture, nativesharp::diagnostics::DiagnosticId id)
{
    for (const auto& diagnostic : fixture.diagnostics.diagnostics())
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
        Fixture fixture;
        parse(
            fixture,
            "class Widget {\n"
            "  Widget() { }\n"
            "  public Widget(int value) { this.Value = value; }\n"
            "  private Widget(float value) { }\n"
            "  int Value;\n"
            "  public Widget Make() { return new Widget(1); }\n"
            "}\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "constructors, constructor overloads, this, and object creation parse cleanly");
        const AstNode* type = fixture.root != nullptr && !fixture.root->children().empty()
            ? fixture.root->children()[0]
            : nullptr;
        tests.expect(type != nullptr && type->children().size() == 5,
            "constructor fixture retains three constructors, one field, and one method");
        if (type != nullptr && type->children().size() == 5)
        {
            tests.expect(type->children()[0]->kind() == AstNodeKind::ConstructorDeclaration &&
                    type->children()[0]->tokenKind() == TokenKind::Invalid,
                "constructor without accessibility keeps default-access marker");
            tests.expect(type->children()[1]->kind() == AstNodeKind::ConstructorDeclaration &&
                    type->children()[1]->tokenKind() == TokenKind::KwPublic &&
                    type->children()[1]->children().size() == 1,
                "public constructor preserves accessibility and parameter syntax");
            tests.expect(type->children()[2]->tokenKind() == TokenKind::KwPrivate,
                "private constructor accessibility is preserved");

            const AstNode* make = type->children()[4];
            const AstNode* block = make->first();
            const AstNode* ret = block != nullptr && !block->children().empty()
                ? block->children()[0]
                : nullptr;
            const AstNode* creation = ret == nullptr ? nullptr : ret->first();
            tests.expect(creation != nullptr && creation->kind() == AstNodeKind::ObjectCreationExpression &&
                    creation->children().size() == 1 && creation->typeSyntax() != nullptr &&
                    creation->typeSyntax()->baseName() == "Widget",
                "new Widget(1) uses ObjectCreationExpression with constructor arguments");
        }
    }

    {
        Fixture fixture;
        parse(fixture, "class Widget { Widget; }\n");
        tests.expect(fixture.diagnostics.hasErrors(),
            "class-name token without constructor parameter list is not a constructor");
    }

    {
        Fixture fixture;
        parse(fixture, "class Widget { Widget(); }\n");
        tests.expect(fixture.diagnostics.hasErrors(),
            "bodyless constructor is rejected in Stage 28");
        tests.expect(hasDiagnostic(fixture, diagnostics::DiagnosticId::ExpectedDeclaration) ||
                hasDiagnostic(fixture, diagnostics::DiagnosticId::ExpectedToken),
            "bodyless constructor reports a declaration/body diagnostic");
    }

    {
        Fixture fixture;
        parse(fixture, "class Widget { public private Widget() { } }\n");
        tests.expect(fixture.diagnostics.hasErrors(),
            "conflicting constructor accessibility modifiers are rejected");
    }

    {
        Fixture fixture;
        parse(fixture, "class Widget { int Widget() { return 0; } }\n");
        tests.expect(fixture.diagnostics.hasErrors(),
            "ordinary method still cannot reuse containing class name");
    }

    {
        Fixture fixture;
        parse(fixture, "class Widget { public Widget Make() { return new Widget; } }\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "legacy procedural new T remains syntactically distinct for later semantic rejection on class types");
        const AstNode* method = fixture.root->children()[0]->children()[0];
        const AstNode* expression = method->first()->children()[0]->first();
        tests.expect(expression != nullptr && expression->kind() == AstNodeKind::NewExpression,
            "new Widget without parentheses remains the procedural NewExpression AST form");
    }

    return tests.result();
}
