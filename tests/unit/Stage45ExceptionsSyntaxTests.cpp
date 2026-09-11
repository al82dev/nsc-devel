#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstDumper.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <sstream>
#include <string>
#include <vector>

namespace
{
struct ParsedSyntax
{
    nativesharp::source::SourceManager sources;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    nativesharp::ast::AstArena arena;
    std::vector<nativesharp::lexer::Token> tokens;
    nativesharp::ast::AstNode* root = nullptr;
};

void parse(
    ParsedSyntax& syntax,
    std::string source,
    std::vector<std::string> knownTypes = {})
{
    const nativesharp::FileId fileId = syntax.sources.addBuffer("stage45_syntax.ns", std::move(source));
    const nativesharp::source::SourceFile* file = syntax.sources.getFile(fileId);
    nativesharp::lexer::Lexer lexer(*file, syntax.diagnostics);
    syntax.tokens = lexer.lexAll();
    nativesharp::parser::Parser parser(
        syntax.tokens,
        syntax.arena,
        syntax.diagnostics,
        knownTypes);
    syntax.root = parser.parseCompilationUnit();
}

const nativesharp::ast::AstNode* findFunction(
    const nativesharp::ast::AstNode* root,
    std::string_view name)
{
    if (root == nullptr)
    {
        return nullptr;
    }

    for (const nativesharp::ast::AstNode* child : root->children())
    {
        if (child->kind() == nativesharp::ast::AstNodeKind::FunctionDeclaration &&
            child->text() == name)
        {
            return child;
        }
    }
    return nullptr;
}
}

int main()
{
    using namespace nativesharp;
    using namespace nativesharp::ast;
    using nativesharp::tests::TestContext;

    TestContext tests;

    ParsedSyntax syntax;
    parse(
        syntax,
        R"NS(
int Main()
{
    try
    {
        throw new Exception();
    }
    catch (Exception e)
    {
        throw;
    }
    catch
    {
        return 1;
    }
    finally
    {
        int cleanupMarker = 0;
    }

    using (Disposable resource = new Disposable())
    {
        resource.Dispose();
    }

    return 0;
}
)NS",
        {"Exception", "Disposable"});

    tests.expect(!syntax.diagnostics.hasErrors(), "Stage 45.1 exception syntax parses without diagnostics");

    bool sawTryKeyword = false;
    bool sawCatchKeyword = false;
    bool sawFinallyKeyword = false;
    bool sawThrowKeyword = false;
    for (const lexer::Token& token : syntax.tokens)
    {
        sawTryKeyword = sawTryKeyword || token.kind() == lexer::TokenKind::KwTry;
        sawCatchKeyword = sawCatchKeyword || token.kind() == lexer::TokenKind::KwCatch;
        sawFinallyKeyword = sawFinallyKeyword || token.kind() == lexer::TokenKind::KwFinally;
        sawThrowKeyword = sawThrowKeyword || token.kind() == lexer::TokenKind::KwThrow;
    }
    tests.expect(sawTryKeyword, "try is a dedicated keyword token");
    tests.expect(sawCatchKeyword, "catch is a dedicated keyword token");
    tests.expect(sawFinallyKeyword, "finally is a dedicated keyword token");
    tests.expect(sawThrowKeyword, "throw is a dedicated keyword token");

    const AstNode* mainFunction = findFunction(syntax.root, "Main");
    tests.expect(mainFunction != nullptr, "Main function is present in Stage 45.1 AST");
    const AstNode* body = mainFunction != nullptr ? mainFunction->first() : nullptr;
    tests.expect(body != nullptr && body->kind() == AstNodeKind::BlockStatement,
        "Main has a block body");
    tests.expect(body != nullptr && body->children().size() == 3,
        "Main body contains try, using, and return statements");

    const AstNode* tryStatement = body != nullptr && !body->children().empty()
        ? body->children()[0]
        : nullptr;
    tests.expect(tryStatement != nullptr && tryStatement->kind() == AstNodeKind::TryStatement,
        "try is represented by TryStatement");
    tests.expect(tryStatement != nullptr && tryStatement->first() != nullptr &&
            tryStatement->first()->kind() == AstNodeKind::BlockStatement,
        "TryStatement.first stores the protected block");
    tests.expect(tryStatement != nullptr && tryStatement->children().size() == 2,
        "TryStatement children preserve catch order");
    tests.expect(tryStatement != nullptr && tryStatement->second() != nullptr &&
            tryStatement->second()->kind() == AstNodeKind::BlockStatement,
        "TryStatement.second stores the finally block");

    const AstNode* typedCatch = tryStatement != nullptr && !tryStatement->children().empty()
        ? tryStatement->children()[0]
        : nullptr;
    tests.expect(typedCatch != nullptr && typedCatch->kind() == AstNodeKind::CatchClause,
        "typed catch is represented by CatchClause");
    tests.expect(typedCatch != nullptr && typedCatch->typeSyntax() != nullptr &&
            typedCatch->typeSyntax()->baseName() == "Exception",
        "typed catch preserves its exception type");
    tests.expect(typedCatch != nullptr && typedCatch->text() == "e",
        "typed catch preserves its exception variable");
    tests.expect(typedCatch != nullptr && !typedCatch->flag(),
        "typed catch is distinguished from catch-all");

    const AstNode* catchAll = tryStatement != nullptr && tryStatement->children().size() > 1
        ? tryStatement->children()[1]
        : nullptr;
    tests.expect(catchAll != nullptr && catchAll->kind() == AstNodeKind::CatchClause && catchAll->flag(),
        "catch without declaration is represented as catch-all");
    tests.expect(catchAll != nullptr && catchAll->typeSyntax() == nullptr && catchAll->text().empty(),
        "catch-all has no synthetic exception type or variable");

    const AstNode* throwExpression = tryStatement != nullptr && tryStatement->first() != nullptr &&
            !tryStatement->first()->children().empty()
        ? tryStatement->first()->children()[0]
        : nullptr;
    tests.expect(throwExpression != nullptr && throwExpression->kind() == AstNodeKind::ThrowStatement &&
            throwExpression->first() != nullptr,
        "throw expression stores its operand in first");

    const AstNode* rethrow = typedCatch != nullptr && typedCatch->first() != nullptr &&
            !typedCatch->first()->children().empty()
        ? typedCatch->first()->children()[0]
        : nullptr;
    tests.expect(rethrow != nullptr && rethrow->kind() == AstNodeKind::ThrowStatement &&
            rethrow->first() == nullptr,
        "throw; is represented as ThrowStatement without an operand");

    const AstNode* usingStatement = body != nullptr && body->children().size() > 1
        ? body->children()[1]
        : nullptr;
    tests.expect(usingStatement != nullptr && usingStatement->kind() == AstNodeKind::UsingStatement,
        "using declaration is represented by UsingStatement");
    tests.expect(usingStatement != nullptr && usingStatement->typeSyntax() != nullptr &&
            usingStatement->typeSyntax()->baseName() == "Disposable",
        "UsingStatement preserves the resource type");
    tests.expect(usingStatement != nullptr && usingStatement->text() == "resource",
        "UsingStatement preserves the resource variable name");
    tests.expect(usingStatement != nullptr && usingStatement->first() != nullptr,
        "UsingStatement.first stores the initializer expression");
    tests.expect(usingStatement != nullptr && usingStatement->second() != nullptr &&
            usingStatement->second()->kind() == AstNodeKind::BlockStatement,
        "UsingStatement.second stores the using body");

    std::ostringstream dump;
    AstDumper{}.dump(syntax.root, dump);
    const std::string dumpText = dump.str();
    tests.expect(dumpText.find("TryStatement") != std::string::npos,
        "AST dumper exposes TryStatement");
    tests.expect(dumpText.find("CatchClause \"e\" type=Exception") != std::string::npos,
        "AST dumper exposes typed catch metadata");
    tests.expect(dumpText.find("ThrowStatement") != std::string::npos,
        "AST dumper exposes ThrowStatement");
    tests.expect(dumpText.find("UsingStatement \"resource\" type=Disposable") != std::string::npos,
        "AST dumper exposes UsingStatement metadata");

    ParsedSyntax qualifiedContracts;
    parse(
        qualifiedContracts,
        R"NS(
public delegate Contracts.Types.Payload Factory(Contracts.Types.Payload value);

public interface IService : Contracts.Types.IBase
{
    Contracts.Types.Payload Current { get; }
    Contracts.Types.Payload Get(Contracts.Types.Payload value);
    Contracts.Types.Payload this[int index] { get; }
}

public class Service : Contracts.Types.Base, Contracts.Types.IBase
{
    public event Contracts.Types.Factory Changed;
    public static implicit operator Contracts.Types.Payload(Service value) { return null; }

    public void Audit()
    {
        try { }
        catch (Contracts.Types.Failure failure) { }
        using (Contracts.Types.Resource resource = null) { }
    }
}
)NS",
        {"Contracts.Types.Payload",
         "Contracts.Types.IBase",
         "Contracts.Types.Base",
         "Contracts.Types.Factory",
         "Contracts.Types.Failure",
         "Contracts.Types.Resource",
         "Service"});
    tests.expect(!qualifiedContracts.diagnostics.hasErrors(),
        "qualified types parse in delegates, class/interface bases, interface contracts, events, "
        "conversions, catch, and using declarations");

    ParsedSyntax malformedCatch;
    parse(
        malformedCatch,
        "int Main() { try { return 0; } catch (Exception) { return 1; } }",
        {"Exception"});
    tests.expect(malformedCatch.diagnostics.hasErrors(),
        "typed catch requires an exception variable name");

    ParsedSyntax malformedUsing;
    parse(
        malformedUsing,
        "int Main() { using (Disposable d) { return 0; } }",
        {"Disposable"});
    tests.expect(malformedUsing.diagnostics.hasErrors(),
        "using declaration requires an initializer");

    ParsedSyntax unbracedTry;
    parse(unbracedTry, "int Main() { try return 0; catch { return 1; } }");
    tests.expect(unbracedTry.diagnostics.hasErrors(),
        "try body must be braced");

    return tests.result();
}
