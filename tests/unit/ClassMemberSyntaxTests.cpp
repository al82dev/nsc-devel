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
    const nativesharp::FileId fileId = fixture.sources.addBuffer(
        "class_members.ns",
        std::move(source));
    const nativesharp::source::SourceFile* file = fixture.sources.getFile(fileId);
    nativesharp::lexer::Lexer lexer(*file, fixture.diagnostics);
    fixture.tokens = lexer.lexAll();
    nativesharp::parser::Parser parser(fixture.tokens, fixture.arena, fixture.diagnostics);
    fixture.root = parser.parseCompilationUnit();
}

bool hasDiagnostic(const Fixture& fixture, nativesharp::diagnostics::DiagnosticId id)
{
    for (const nativesharp::diagnostics::Diagnostic& diagnostic : fixture.diagnostics.diagnostics())
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
            "  int DefaultField;\n"
            "  private int PrivateField;\n"
            "  protected int ProtectedField;\n"
            "  internal int InternalField;\n"
            "  public int PublicField;\n"
            "  public int Get() { return this.PublicField; }\n"
            "  public int Add(int value) { return this.PublicField + value; }\n"
            "  public int Add(float value) { return this.PublicField; }\n"
            "}\n");

        tests.expect(!fixture.diagnostics.hasErrors(),
            "Stage 28 class fields, methods, overload syntax, access modifiers, and this parse cleanly");
        tests.expect(fixture.root != nullptr && fixture.root->children().size() == 1,
            "member syntax fixture produces one class declaration");
        if (fixture.root != nullptr && fixture.root->children().size() == 1)
        {
            const AstNode* classNode = fixture.root->children()[0];
            tests.expect(classNode->children().size() == 8,
                "class retains all eight members in lexical order");
            if (classNode->children().size() == 8)
            {
                tests.expect(classNode->children()[0]->kind() == AstNodeKind::FieldDeclaration &&
                        classNode->children()[0]->tokenKind() == TokenKind::Invalid,
                    "default-access field is represented without a written accessibility token");
                tests.expect(classNode->children()[1]->tokenKind() == TokenKind::KwPrivate,
                    "private field accessibility is preserved in AST");
                tests.expect(classNode->children()[2]->tokenKind() == TokenKind::KwProtected,
                    "protected field accessibility is preserved in AST");
                tests.expect(classNode->children()[3]->tokenKind() == TokenKind::KwInternal,
                    "internal field accessibility is preserved in AST");
                tests.expect(classNode->children()[4]->tokenKind() == TokenKind::KwPublic,
                    "public field accessibility is preserved in AST");
                tests.expect(classNode->children()[5]->kind() == AstNodeKind::MethodDeclaration &&
                        classNode->children()[5]->text() == "Get",
                    "instance method has its own AST kind instead of masquerading as a module function");
                tests.expect(classNode->children()[6]->children().size() == 1 &&
                        classNode->children()[7]->children().size() == 1,
                    "method parameter declarations remain attached to each method overload");

                const AstNode* getMethod = classNode->children()[5];
                const AstNode* block = getMethod->first();
                const AstNode* returnNode = block != nullptr && !block->children().empty()
                    ? block->children()[0]
                    : nullptr;
                const AstNode* access = returnNode == nullptr ? nullptr : returnNode->first();
                tests.expect(access != nullptr && access->kind() == AstNodeKind::MemberExpression &&
                        access->first() != nullptr &&
                        access->first()->kind() == AstNodeKind::ThisExpression &&
                        access->text() == "PublicField",
                    "this.Member is represented explicitly as ThisExpression plus MemberExpression");
            }
        }

        bool sawPrivate = false;
        bool sawProtected = false;
        bool sawThis = false;
        for (const lexer::Token& token : fixture.tokens)
        {
            sawPrivate = sawPrivate || token.kind() == TokenKind::KwPrivate;
            sawProtected = sawProtected || token.kind() == TokenKind::KwProtected;
            sawThis = sawThis || token.kind() == TokenKind::KwThis;
        }
        tests.expect(sawPrivate && sawProtected && sawThis,
            "private, protected, and this are Stage 28 reserved tokens");
    }

    {
        Fixture fixture;
        parse(fixture, "class Invalid { void value; }\n");
        tests.expect(fixture.diagnostics.hasErrors(), "void instance field is rejected");
        tests.expect(hasDiagnostic(fixture, diagnostics::DiagnosticId::InvalidType),
            "void field rejection uses InvalidType");
    }


    {
        Fixture fixture;
        parse(
            fixture,
            "class Pointers { void* Data; int Count(void* p) { return 0; } }\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "void* remains a valid instance field and method parameter type");
    }

    {
        Fixture fixture;
        parse(fixture, "class Widget { int Widget() { return 0; } }\n");
        tests.expect(fixture.diagnostics.hasErrors(),
            "ordinary method cannot reuse the containing class name reserved for constructors");
        tests.expect(hasDiagnostic(fixture, diagnostics::DiagnosticId::ExpectedDeclaration),
            "class-name member collision produces an explicit declaration diagnostic");
    }

    {
        Fixture fixture;
        parse(fixture, "class Valid { int value = 1; static int count = 2; readonly int id; const int seed = 3; }\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "Stage 37 accepts instance/static field initializers plus readonly and const field modifiers");
    }

    {
        Fixture fixture;
        parse(fixture, "class Invalid { const int value; }\n");
        tests.expect(fixture.diagnostics.hasErrors(), "const field requires a declaration initializer in Stage 37");
        tests.expect(hasDiagnostic(fixture, diagnostics::DiagnosticId::ExpectedDeclaration),
            "missing const initializer is an explicit declaration diagnostic");
    }

    {
        Fixture fixture;
        parse(fixture, "class Valid { public Valid() { } }\n");
        tests.expect(!fixture.diagnostics.hasErrors(), "Stage 28 constructor syntax is accepted by the class-member parser");
        const AstNode* classNode = fixture.root != nullptr && !fixture.root->children().empty()
            ? fixture.root->children()[0]
            : nullptr;
        tests.expect(classNode != nullptr && classNode->children().size() == 1 &&
                classNode->children()[0]->kind() == AstNodeKind::ConstructorDeclaration,
            "constructor receives a dedicated AST kind rather than masquerading as a method");
    }

    {
        Fixture fixture;
        parse(fixture, "class Invalid { int Get(); }\n");
        tests.expect(fixture.diagnostics.hasErrors(), "bodyless ordinary method is rejected in Stage 28");
        tests.expect(hasDiagnostic(fixture, diagnostics::DiagnosticId::ExpectedToken),
            "bodyless method rejection records the missing method body");
    }

    {
        Fixture fixture;
        parse(fixture, "class Invalid { public private int value; }\n");
        tests.expect(fixture.diagnostics.hasErrors(), "conflicting member accessibility modifiers are rejected");
        tests.expect(hasDiagnostic(fixture, diagnostics::DiagnosticId::ExpectedDeclaration),
            "conflicting member accessibility is a declaration diagnostic");
    }

    {
        Fixture fixture;
        parse(fixture, "class Invalid { const int value; }\n");
        tests.expect(fixture.diagnostics.hasErrors(), "const/static-like class member is outside Stage 28");
    }

    return tests.result();
}
