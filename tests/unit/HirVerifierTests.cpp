#include "nativesharp/HIR/HirArena.h"
#include "nativesharp/HIR/HirVerifier.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/TypeContext.h"
#include "../support/TestContext.h"

using nativesharp::SourceRange;
using nativesharp::hir::HirArena;
using nativesharp::hir::HirNode;
using nativesharp::hir::HirNodeKind;
using nativesharp::hir::HirVerifier;
using nativesharp::sema::Symbol;
using nativesharp::sema::SymbolKind;
using nativesharp::sema::Type;
using nativesharp::sema::TypeContext;
using nativesharp::sema::TypeKind;

namespace
{
HirNode* addFunction(HirArena& arena, HirNode* root, Symbol* symbol, Type* functionType)
{
    HirNode* function = arena.make(HirNodeKind::FunctionDeclaration, SourceRange());
    function->setSymbol(symbol);
    function->setType(functionType);

    HirNode* body = arena.make(HirNodeKind::Block, SourceRange());
    function->setFirst(body);
    root->addChild(function);
    return body;
}

HirNode* addExpressionStatement(HirArena& arena, HirNode* body, HirNode* expression)
{
    HirNode* statement = arena.make(HirNodeKind::ExpressionStatement, SourceRange());
    statement->setFirst(expression);
    body->addChild(statement);
    return statement;
}
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;
    TypeContext types;

    {
        HirArena arena;
        HirNode* root = arena.make(HirNodeKind::CompilationUnit, SourceRange());
        HirVerifier verifier;
        tests.expect(verifier.verify(root), "empty CompilationUnit is structurally valid HIR");
    }

    {
        HirArena arena;
        HirNode* root = arena.make(HirNodeKind::CompilationUnit, SourceRange());
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", functionType, nullptr);
        HirNode* body = addFunction(arena, root, &functionSymbol, functionType);

        HirNode* load = arena.make(HirNodeKind::Load, SourceRange());
        load->setType(types.builtin(TypeKind::Int));
        HirNode* notAddress = arena.make(HirNodeKind::Constant, SourceRange());
        notAddress->setType(types.builtin(TypeKind::Int));
        notAddress->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        load->setFirst(notAddress);
        addExpressionStatement(arena, body, load);

        HirVerifier verifier;
        tests.expect(!verifier.verify(root), "verifier rejects Load from a non-pointer expression");
        tests.expect(!verifier.error().empty(), "verifier explains malformed Load");
    }

    {
        HirArena arena;
        HirNode* root = arena.make(HirNodeKind::CompilationUnit, SourceRange());
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", functionType, nullptr);
        HirNode* body = addFunction(arena, root, &functionSymbol, functionType);

        HirNode* conversion = arena.make(HirNodeKind::Conversion, SourceRange());
        conversion->setType(types.builtin(TypeKind::Long));
        addExpressionStatement(arena, body, conversion);

        HirVerifier verifier;
        tests.expect(!verifier.verify(root), "verifier rejects Conversion without operand/kind");
    }

    {
        HirArena arena;
        HirNode* root = arena.make(HirNodeKind::CompilationUnit, SourceRange());
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", functionType, nullptr);
        HirNode* body = addFunction(arena, root, &functionSymbol, functionType);

        HirNode* index = arena.make(HirNodeKind::IndexAddress, SourceRange());
        index->setType(types.pointerTo(types.builtin(TypeKind::Int)));
        index->setOperationType(types.pointerTo(types.builtin(TypeKind::Int)));
        addExpressionStatement(arena, body, index);

        HirVerifier verifier;
        tests.expect(!verifier.verify(root), "verifier rejects incomplete IndexAddress");
    }

    {
        HirArena arena;
        HirNode* root = arena.make(HirNodeKind::CompilationUnit, SourceRange());
        HirNode* constant = arena.make(HirNodeKind::Constant, SourceRange());
        constant->setType(types.builtin(TypeKind::Int));
        constant->setConstant(nativesharp::sema::ConstantValue::signedInteger(1));
        root->addChild(constant);

        HirVerifier verifier;
        tests.expect(!verifier.verify(root), "verifier rejects expression directly under CompilationUnit");
    }

    {
        HirArena arena;
        HirNode* root = arena.make(HirNodeKind::CompilationUnit, SourceRange());
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", functionType, nullptr);
        HirNode* body = addFunction(arena, root, &functionSymbol, functionType);
        body->addChild(arena.make(HirNodeKind::Break, SourceRange()));

        HirVerifier verifier;
        tests.expect(!verifier.verify(root), "verifier rejects break outside a loop");
    }

    {
        HirArena arena;
        HirNode* root = arena.make(HirNodeKind::CompilationUnit, SourceRange());
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", functionType, nullptr);
        HirNode* body = addFunction(arena, root, &functionSymbol, functionType);

        HirNode* forNode = arena.make(HirNodeKind::For, SourceRange());
        forNode->setThird(arena.make(HirNodeKind::Block, SourceRange()));
        forNode->addChild(arena.make(HirNodeKind::Block, SourceRange()));
        body->addChild(forNode);

        HirVerifier verifier;
        tests.expect(!verifier.verify(root), "verifier rejects a Block used as the for increment expression");
    }

    {
        HirArena arena;
        HirNode* root = arena.make(HirNodeKind::CompilationUnit, SourceRange());
        Type* functionType = types.functionType(types.builtin(TypeKind::Void), {});
        Symbol functionSymbol(SymbolKind::Function, "F", functionType, nullptr);
        HirNode* body = addFunction(arena, root, &functionSymbol, functionType);

        HirNode* whileNode = arena.make(HirNodeKind::While, SourceRange());
        HirNode* condition = arena.make(HirNodeKind::Constant, SourceRange());
        condition->setType(types.builtin(TypeKind::Bool));
        condition->setConstant(nativesharp::sema::ConstantValue::boolean(true));
        whileNode->setFirst(condition);

        HirNode* loopBody = arena.make(HirNodeKind::Block, SourceRange());
        loopBody->addChild(arena.make(HirNodeKind::Break, SourceRange()));
        whileNode->setSecond(loopBody);
        body->addChild(whileNode);

        HirVerifier verifier;
        tests.expect(verifier.verify(root), "verifier accepts break inside a while body");
    }

    return tests.result();
}
