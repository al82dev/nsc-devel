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
        parse(
            fixture,
            "class Valid { public int Count { get; set; } = 7; "
            "public static string Name { get; } = \"NativeSharp\"; }\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "automatic properties accept C#-style declaration initializers");
        const AstNode* classNode = fixture.root != nullptr && !fixture.root->children().empty()
            ? fixture.root->children()[0]
            : nullptr;
        bool sawInstanceInitializer = false;
        bool sawStaticInitializer = false;
        if (classNode != nullptr)
        {
            for (const AstNode* property : classNode->children())
            {
                if (property->kind() != AstNodeKind::PropertyDeclaration) continue;
                for (const AstNode* child : property->children())
                {
                    if (child->kind() != AstNodeKind::FieldDeclaration || child->first() == nullptr)
                        continue;
                    sawInstanceInitializer = sawInstanceInitializer ||
                        (property->text() == "Count" &&
                         child->first()->kind() == AstNodeKind::IntegerLiteralExpression);
                    sawStaticInitializer = sawStaticInitializer ||
                        (property->text() == "Name" && child->hasModifier(TokenKind::KwStatic) &&
                         child->first()->kind() == AstNodeKind::StringLiteralExpression);
                }
            }
        }
        tests.expect(sawInstanceInitializer,
            "an instance auto-property initializer is attached to its generated backing field");
        tests.expect(sawStaticInitializer,
            "a static auto-property initializer is attached to its static backing field");
    }

    {
        Fixture fixture;
        parse(fixture, "class Invalid { public int Count { get { return 1; } } = 7; }\n");
        tests.expect(fixture.diagnostics.hasErrors(),
            "an explicit property body cannot pretend to own an automatic initializer");
        tests.expect(hasDiagnostic(fixture, diagnostics::DiagnosticId::ExpectedDeclaration),
            "invalid property initializer reports a declaration diagnostic");
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

    // NSC-LIMIT-009 -- TIPOS ANIDADOS, en el parser.
    //
    // Antes de esta entrega las cinco formas caian en el mismo sitio: `class`, `struct`,
    // `interface`, `enum` y `delegate` no son comienzos de tipo ni identificadores, de modo que
    // `parseClassMember` llegaba a su guarda final y respondia «expected an instance field or
    // method declaration in class body» -- el mensaje de una errata, para algo que no lo era.
    {
        Fixture fixture;
        parse(
            fixture,
            "class Outer {\n"
            "  public struct Item { public int a; }\n"
            "  private class Detalle { public int b; }\n"
            "  internal interface IContrato { int Peso(); }\n"
            "  public enum Modo { A, B }\n"
            "  public delegate int Transformar(int x);\n"
            "  public int Campo;\n"
            "}\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "the five nested type forms parse inside a class body");
        tests.expect(fixture.root != nullptr && fixture.root->children().size() == 1,
            "nested type fixture produces one outer class");
        if (fixture.root != nullptr && fixture.root->children().size() == 1)
        {
            const AstNode* outer = fixture.root->children()[0];
            tests.expect(outer->children().size() == 6,
                "the nested types are children of the outer type, not of the compilation unit");
            if (outer->children().size() == 6)
            {
                tests.expect(outer->children()[0]->kind() == AstNodeKind::StructDeclaration &&
                        outer->children()[0]->text() == "Item",
                    "a nested struct keeps its own declaration kind");
                // La accesibilidad viaja por el MISMO canal que la de un tipo de namespace --el
                // tokenKind del nodo-- y por eso el modelo semantico no necesita saber si el tipo
                // estaba dentro de otro para leerla. Lo unico que cambia con el sitio es el valor
                // POR OMISION, y eso se decide al predeclarar, no aqui.
                tests.expect(outer->children()[0]->tokenKind() == TokenKind::KwPublic,
                    "an explicit accessibility on a nested type reaches the AST");
                tests.expect(outer->children()[1]->kind() == AstNodeKind::ClassDeclaration &&
                        outer->children()[1]->tokenKind() == TokenKind::KwPrivate,
                    "a nested class records 'private' as written");
                tests.expect(outer->children()[2]->kind() == AstNodeKind::InterfaceDeclaration,
                    "a nested interface keeps its own declaration kind");
                tests.expect(outer->children()[3]->kind() == AstNodeKind::EnumDeclaration,
                    "a nested enum keeps its own declaration kind");
                tests.expect(outer->children()[4]->kind() == AstNodeKind::DelegateDeclaration,
                    "a nested delegate keeps its own declaration kind");
                tests.expect(outer->children()[5]->kind() == AstNodeKind::FieldDeclaration,
                    "an ordinary field after five nested types still parses as a field");
            }
        }
    }

    // Un struct tambien contiene tipos: `parseStructDeclaration` usa el mismo `parseClassMember`,
    // de modo que la entrada vale para los dos sin duplicar la gramatica.
    {
        Fixture fixture;
        parse(
            fixture,
            "struct Contenedor {\n"
            "  public struct Dentro { public int c; }\n"
            "  public enum Modo { A, B }\n"
            "}\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "nested types parse inside a struct body as well");
        tests.expect(fixture.root != nullptr && fixture.root->children().size() == 1 &&
                fixture.root->children()[0]->children().size() == 2,
            "a struct keeps its two nested types");
    }

    // Una clase anidada participa del mismo modelo de agregacion que una clase de namespace.
    // El parser debe conservar `partial`; la validez de varias partes se decide despues, cuando
    // ProjectNameLookup entrega el grupo a PartialTypeAggregator.
    {
        Fixture fixture;
        parse(fixture, "class Outer { public partial class Item { int a; } }\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "a nested class accepts the partial modifier");
        const ast::AstNode* outer = fixture.root != nullptr && !fixture.root->children().empty()
            ? fixture.root->children()[0]
            : nullptr;
        const ast::AstNode* item = outer != nullptr && !outer->children().empty()
            ? outer->children()[0]
            : nullptr;
        tests.expect(item != nullptr && item->kind() == AstNodeKind::ClassDeclaration && item->flag(),
            "a nested partial class preserves its aggregation marker in the AST");
    }

    // Los modificadores que no significan nada en estas formas de tipo anidado.
    for (const char* source : {
             "class Outer { static struct Item { int a; } }\n",
             "class Outer { readonly struct Item { int a; } }\n",
             "class Outer { const struct Item { int a; } }\n",
             "class Outer { virtual class Item { int a; } }\n",
             "class Outer { override class Item { int a; } }\n",
             "class Outer { abstract struct Item { int a; } }\n",
             "class Outer { public struct Item<T> { int a; } }\n"})
    {
        Fixture fixture;
        parse(fixture, source);
        tests.expect(fixture.diagnostics.hasErrors(),
            std::string("nested type rejects an incompatible modifier: ") + source);
        tests.expect(hasDiagnostic(fixture, diagnostics::DiagnosticId::ExpectedDeclaration),
            std::string("the nested-type rejection is a declaration diagnostic: ") + source);
    }

    {
        Fixture fixture;
        parse(
            fixture,
            "public static class Utilities { public static int Value = 7; "
            "public static int Get() { return Value; } "
            "public class Nested { public int item; } }\n"
            "class Outer { public static class NestedUtilities { "
            "public static int Get() { return 9; } } }\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "top-level and nested C#-style static classes parse with static members and nested types");
        tests.expect(fixture.root != nullptr && fixture.root->children().size() == 2 &&
                fixture.root->children()[0]->hasModifier(TokenKind::KwStatic) &&
                fixture.root->children()[1]->children()[0]->hasModifier(TokenKind::KwStatic),
            "the static-class marker survives on both top-level and nested declarations");
    }

    for (const char* source : {
             "static class Bad { int value; }\n",
             "static class Bad { int Get() { return 1; } }\n",
             "static class Bad { Bad() { } }\n",
             "static class Bad : Base { public static int Get() { return 1; } }\n"})
    {
        Fixture fixture;
        parse(fixture, source);
        tests.expect(fixture.diagnostics.hasErrors(),
            std::string("static class rejects instance construction/state or bases: ") + source);
        tests.expect(hasDiagnostic(fixture, diagnostics::DiagnosticId::ExpectedDeclaration),
            std::string("static-class rule reports a declaration diagnostic: ") + source);
    }

    // `abstract` y `sealed` SI valen, pero solo sobre una clase anidada. Es el control de la
    // lista de arriba: sin este caso, «se rechaza abstract» podria estar rechazandolo siempre.
    {
        Fixture fixture;
        parse(fixture, "class Outer { public abstract class Base { public int a; } }\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "abstract IS valid on a nested class, which is what makes the struct rejection meaningful");
    }

    // Y `protected` tambien vale, que es el segundo control. El parser lo acepta y lo guarda; que
    // el USO sea legitimo o no lo decide ProjectInheritanceModel, que es la primera fase capaz de
    // responder «el sitio deriva del contenedor?». Aqui solo se mide que llega al AST.
    {
        Fixture fixture;
        parse(fixture, "class Outer { protected struct Item { public int a; } }\n");
        tests.expect(!fixture.diagnostics.hasErrors(),
            "protected IS valid on a nested type; whether a USE of it is legal is decided later");
        tests.expect(fixture.root != nullptr && fixture.root->children().size() == 1 &&
                fixture.root->children()[0]->children().size() == 1 &&
                fixture.root->children()[0]->children()[0]->tokenKind() == TokenKind::KwProtected,
            "the nested type records 'protected' as written");
    }

    return tests.result();
}
