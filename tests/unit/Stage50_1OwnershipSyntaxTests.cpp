#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstDumper.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <sstream>
#include <string>
#include <string_view>
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

void parse(ParsedSyntax& syntax, std::string source)
{
    const nativesharp::FileId fileId = syntax.sources.addBuffer("stage50_1.ns", std::move(source));
    const nativesharp::source::SourceFile* file = syntax.sources.getFile(fileId);
    nativesharp::lexer::Lexer lexer(*file, syntax.diagnostics);
    syntax.tokens = lexer.lexAll();
    nativesharp::parser::Parser parser(syntax.tokens, syntax.arena, syntax.diagnostics);
    syntax.root = parser.parseCompilationUnit();
}

const nativesharp::ast::AstNode* findGlobalFunction(
    const nativesharp::ast::AstNode* root,
    std::string_view name)
{
    if (root == nullptr)
    {
        return nullptr;
    }

    for (const nativesharp::ast::AstNode* child : root->children())
    {
        if (child->kind() == nativesharp::ast::AstNodeKind::FunctionDeclaration && child->text() == name)
        {
            return child;
        }
    }
    return nullptr;
}

const nativesharp::ast::AstNode* findGlobalClass(
    const nativesharp::ast::AstNode* root,
    std::string_view name)
{
    if (root == nullptr)
    {
        return nullptr;
    }

    for (const nativesharp::ast::AstNode* child : root->children())
    {
        if (child->kind() == nativesharp::ast::AstNodeKind::ClassDeclaration && child->text() == name)
        {
            return child;
        }
    }
    return nullptr;
}

bool hasToken(const ParsedSyntax& syntax, nativesharp::lexer::TokenKind kind)
{
    for (const nativesharp::lexer::Token& token : syntax.tokens)
    {
        if (token.kind() == kind)
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
    using nativesharp::tests::TestContext;

    TestContext tests;

    ParsedSyntax syntax;
    parse(
        syntax,
        R"NS(
class Resource { }
class Box<T> { }
class Holder
{
    public owned Resource Owner;
    public borrowed Resource View;
}

class GenericOwner
{
    public static owned T Transfer<T>(owned T value) where T : class
    {
        return move(value);
    }
}

owned Resource Create(borrowed Resource source)
{
    borrowed Resource view = source;
    owned Resource local = new Resource();
    return move(local);
}

borrowed Resource Observe(owned Resource source)
{
    return source;
}

void AcceptGeneric(owned Box<borrowed Resource> box)
{
}
)NS");

    tests.expect(!syntax.diagnostics.hasErrors(),
        "Stage 50.1 ownership-qualified declarations parse without diagnostics");
    tests.expect(hasToken(syntax, lexer::TokenKind::KwOwned),
        "owned is a dedicated keyword token");
    tests.expect(hasToken(syntax, lexer::TokenKind::KwBorrowed),
        "borrowed is a dedicated keyword token");
    tests.expect(lexer::tokenKindName(lexer::TokenKind::KwOwned) == "KwOwned",
        "owned token kind has stable diagnostic/dump spelling");
    tests.expect(lexer::tokenKindName(lexer::TokenKind::KwBorrowed) == "KwBorrowed",
        "borrowed token kind has stable diagnostic/dump spelling");

    const AstNode* create = findGlobalFunction(syntax.root, "Create");
    tests.expect(create != nullptr && create->typeSyntax() != nullptr,
        "Create function is present with a return type");
    tests.expect(create != nullptr && create->typeSyntax() != nullptr &&
            create->typeSyntax()->ownershipQualifier() == OwnershipQualifierSyntaxKind::Owned,
        "owned return qualifier is represented explicitly in TypeSyntax");
    tests.expect(create != nullptr && !create->children().empty() &&
            create->children()[0]->typeSyntax() != nullptr &&
            create->children()[0]->typeSyntax()->ownershipQualifier() == OwnershipQualifierSyntaxKind::Borrowed,
        "borrowed parameter qualifier is represented explicitly in TypeSyntax");

    const AstNode* createBody = create != nullptr ? create->first() : nullptr;
    tests.expect(createBody != nullptr && createBody->children().size() >= 2,
        "Create body preserves ownership-qualified local declarations");
    tests.expect(createBody != nullptr && !createBody->children().empty() &&
            createBody->children()[0]->typeSyntax() != nullptr &&
            createBody->children()[0]->typeSyntax()->ownershipQualifier() == OwnershipQualifierSyntaxKind::Borrowed,
        "borrowed local qualifier is preserved in the AST");
    tests.expect(createBody != nullptr && createBody->children().size() > 1 &&
            createBody->children()[1]->typeSyntax() != nullptr &&
            createBody->children()[1]->typeSyntax()->ownershipQualifier() == OwnershipQualifierSyntaxKind::Owned,
        "owned local qualifier is preserved in the AST");

    const AstNode* observe = findGlobalFunction(syntax.root, "Observe");
    tests.expect(observe != nullptr && observe->typeSyntax() != nullptr &&
            observe->typeSyntax()->ownershipQualifier() == OwnershipQualifierSyntaxKind::Borrowed,
        "borrowed return qualifier is preserved in the AST");
    tests.expect(observe != nullptr && !observe->children().empty() &&
            observe->children()[0]->typeSyntax() != nullptr &&
            observe->children()[0]->typeSyntax()->ownershipQualifier() == OwnershipQualifierSyntaxKind::Owned,
        "owned parameter qualifier is preserved in the AST");

    const AstNode* generic = findGlobalFunction(syntax.root, "AcceptGeneric");
    const TypeSyntax* genericParameter = generic != nullptr && !generic->children().empty()
        ? generic->children()[0]->typeSyntax()
        : nullptr;
    tests.expect(genericParameter != nullptr &&
            genericParameter->ownershipQualifier() == OwnershipQualifierSyntaxKind::Owned &&
            genericParameter->typeArguments().size() == 1 &&
            genericParameter->typeArguments()[0]->ownershipQualifier() == OwnershipQualifierSyntaxKind::Borrowed,
        "ownership qualifiers survive nested generic type syntax");

    const AstNode* holder = findGlobalClass(syntax.root, "Holder");
    tests.expect(holder != nullptr && holder->children().size() == 2 &&
            holder->children()[0]->typeSyntax() != nullptr &&
            holder->children()[0]->typeSyntax()->ownershipQualifier() == OwnershipQualifierSyntaxKind::Owned &&
            holder->children()[1]->typeSyntax() != nullptr &&
            holder->children()[1]->typeSyntax()->ownershipQualifier() == OwnershipQualifierSyntaxKind::Borrowed,
        "field syntax preserves owned and borrowed qualifiers for later Stage 50 lifetime validation");

    std::ostringstream dump;
    AstDumper{}.dump(syntax.root, dump);
    const std::string dumpText = dump.str();
    tests.expect(dumpText.find("FunctionDeclaration \"Create\" type=owned Resource") != std::string::npos,
        "AST dump exposes owned return syntax");
    tests.expect(dumpText.find("ParameterDeclaration \"source\" type=borrowed Resource") != std::string::npos,
        "AST dump exposes borrowed parameter syntax");
    tests.expect(dumpText.find("type=owned Box<borrowed Resource>") != std::string::npos,
        "AST dump exposes ownership qualifiers recursively in generic types");

    ParsedSyntax caseSensitivity;
    parse(caseSensitivity, "class Owned { } class Borrowed { } class ownedValue { } class borrowedValue { }");
    tests.expect(!caseSensitivity.diagnostics.hasErrors(),
        "ownership keywords remain case-sensitive and do not reserve unrelated identifiers");

    ParsedSyntax malformedMissingType;
    parse(malformedMissingType, "owned ; int Main() { return 0; }");
    tests.expect(malformedMissingType.diagnostics.hasErrors(),
        "ownership qualifier without an underlying type is rejected");
    tests.expect(findGlobalFunction(malformedMissingType.root, "Main") != nullptr,
        "parser recovery continues after a malformed ownership-qualified declaration");

    ParsedSyntax malformedDoubleQualifier;
    parse(malformedDoubleQualifier, "class Resource { } owned borrowed Resource Bad();");
    tests.expect(malformedDoubleQualifier.diagnostics.hasErrors(),
        "multiple ownership qualifiers on one type are rejected");

    return tests.result();
}
