#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <string>
#include <utility>
#include <vector>

namespace
{
struct ParsedSyntax final
{
    nativesharp::source::SourceManager sources;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    nativesharp::ast::AstArena arena;
    std::vector<nativesharp::lexer::Token> tokens;
    nativesharp::ast::AstNode* root = nullptr;
};

void parse(ParsedSyntax& syntax, std::string source)
{
    const nativesharp::FileId fileId =
        syntax.sources.addBuffer("placement_syntax.ns", std::move(source));
    const nativesharp::source::SourceFile* file = syntax.sources.getFile(fileId);
    nativesharp::lexer::Lexer lexer(*file, syntax.diagnostics);
    syntax.tokens = lexer.lexAll();
    const std::vector<std::string> knownTypes = {"Widget"};
    nativesharp::parser::Parser parser(
        syntax.tokens, syntax.arena, syntax.diagnostics, knownTypes);
    syntax.root = parser.parseCompilationUnit();
}

const nativesharp::ast::AstNode* initializerOfFirstLocal(const ParsedSyntax& syntax)
{
    if (syntax.root == nullptr || syntax.root->children().empty()) return nullptr;
    const nativesharp::ast::AstNode* type = syntax.root->children().front();
    if (type == nullptr || type->children().empty()) return nullptr;
    const nativesharp::ast::AstNode* method = type->children().back();
    const nativesharp::ast::AstNode* body = method == nullptr ? nullptr : method->first();
    const nativesharp::ast::AstNode* local =
        body != nullptr && !body->children().empty() ? body->children().front() : nullptr;
    return local == nullptr ? nullptr : local->first();
}
}

int main()
{
    using namespace nativesharp;
    using namespace nativesharp::ast;
    using nativesharp::tests::TestContext;

    TestContext tests;

    ParsedSyntax valid;
    parse(valid, R"NS(
class Widget
{
    Widget(int value) { }
    Widget* Build(Widget* storage)
    {
        Widget* result = new (storage) Widget(42);
        return result;
    }
}
)NS");
    tests.expect(!valid.diagnostics.hasErrors(),
        "placement construction grammar parses without diagnostics");
    const AstNode* placement = initializerOfFirstLocal(valid);
    tests.expect(placement != nullptr &&
            placement->kind() == AstNodeKind::PlacementConstructionExpression,
        "placement construction has an explicit AST identity");
    tests.expect(placement != nullptr && placement->first() != nullptr &&
            placement->first()->kind() == AstNodeKind::IdentifierExpression &&
            placement->first()->text() == "storage",
        "the destination storage is retained separately from constructor arguments");
    tests.expect(placement != nullptr && placement->typeSyntax() != nullptr &&
            placement->typeSyntax()->baseName() == "Widget" &&
            placement->children().size() == 1 &&
            placement->children().front()->kind() == AstNodeKind::IntegerLiteralExpression,
        "the constructed type and ordered argument list remain explicit");
    tests.expect(placement != nullptr && placement->range().isValid(),
        "the placement node covers a valid complete source range");

    ParsedSyntax addressExpression;
    parse(addressExpression, R"NS(
class Widget
{
    Widget() { }
    Widget* Build(Widget* storage, int offset)
    {
        Widget* result = new (storage + offset) Widget();
        return result;
    }
}
)NS");
    const AstNode* offsetPlacement = initializerOfFirstLocal(addressExpression);
    tests.expect(!addressExpression.diagnostics.hasErrors() &&
            offsetPlacement != nullptr && offsetPlacement->first() != nullptr &&
            offsetPlacement->first()->kind() == AstNodeKind::BinaryExpression,
        "a full parenthesized expression can designate placement storage");

    ParsedSyntax ordinary;
    parse(ordinary, R"NS(
class Widget
{
    Widget() { }
    Widget Build() { Widget value = new Widget(); return value; }
}
)NS");
    tests.expect(!ordinary.diagnostics.hasErrors() &&
            initializerOfFirstLocal(ordinary) != nullptr &&
            initializerOfFirstLocal(ordinary)->kind() == AstNodeKind::ObjectCreationExpression,
        "ordinary new T(args) retains its existing AST form");

    ParsedSyntax missingArguments;
    parse(missingArguments, R"NS(
class Widget
{
    Widget* Build(Widget* storage)
    {
        Widget* result = new (storage) Widget;
        return result;
    }
}
)NS");
    tests.expect(missingArguments.diagnostics.hasErrors(),
        "placement construction requires an explicit constructor argument list");

    frontend::FrontendCompilation boundary;
    const bool compiled = boundary.compileBuffer(
        "placement_not_lowered.ns",
        "int Main() { int* storage = stackalloc int[1]; new (storage) int(); return 0; }");
    bool sawBoundary = false;
    for (const diagnostics::Diagnostic& diagnostic : boundary.diagnostics().diagnostics())
    {
        sawBoundary = sawBoundary ||
            diagnostic.id() == diagnostics::DiagnosticId::PlacementConstructionNotAvailable;
    }
    tests.expect(!compiled && sawBoundary && boundary.hirError().empty(),
        "pre-lowering placement construction is rejected as a source diagnostic");

    return tests.result();
}
