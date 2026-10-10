#include "HIRContractSupport.h"

#include "nativesharp/HIR/HirArena.h"
#include "nativesharp/HIR/HirVerifier.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/ConversionKind.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/TypeContext.h"

#include <string_view>

namespace nsc::suite
{
namespace
{
using hir_contract_support::addMatrix;
using nativesharp::SourceRange;
using nativesharp::hir::HirArena;
using nativesharp::hir::HirNode;
using nativesharp::hir::HirNodeKind;
using nativesharp::hir::HirVerifier;
using nativesharp::lexer::TokenKind;
using nativesharp::sema::ConstantValue;
using nativesharp::sema::ConversionKind;
using nativesharp::sema::Symbol;
using nativesharp::sema::SymbolKind;
using nativesharp::sema::Type;
using nativesharp::sema::TypeContext;
using nativesharp::sema::TypeKind;

struct GraphFixture final
{
    GraphFixture()
        : functionType(types.functionType(types.builtin(TypeKind::Void), {})),
          functionSymbol(SymbolKind::Function, "F", functionType, nullptr)
    {
        root = arena.make(HirNodeKind::CompilationUnit, SourceRange());
        function = arena.make(HirNodeKind::FunctionDeclaration, SourceRange());
        function->setSymbol(&functionSymbol);
        function->setType(functionType);
        body = arena.make(HirNodeKind::Block, SourceRange());
        function->setFirst(body);
        root->addChild(function);
    }

    HirNode* addExpression(HirNode* expression)
    {
        HirNode* statement = arena.make(HirNodeKind::ExpressionStatement, SourceRange());
        statement->setFirst(expression);
        body->addChild(statement);
        return statement;
    }

    HirNode* constant(TypeKind kind = TypeKind::Int, std::int64_t value = 1)
    {
        HirNode* node = arena.make(HirNodeKind::Constant, SourceRange());
        node->setType(types.builtin(kind));
        node->setConstant(ConstantValue::signedInteger(value));
        return node;
    }

    TypeContext types;
    HirArena arena;
    Type* functionType;
    Symbol functionSymbol;
    HirNode* root = nullptr;
    HirNode* function = nullptr;
    HirNode* body = nullptr;
};

void expectVerdict(TestContext& context, const HirNode* root, bool accepted, std::string_view contract)
{
    HirVerifier verifier;
    const bool actual = verifier.verify(root);
    context.expect(actual == accepted, std::string(contract) + " verifier verdict must be exact");
    context.expect(actual == verifier.error().empty(),
        std::string(contract) + " success and first-error state must agree");
}

void classifierOracle(TestContext& context, std::size_t variant)
{
    GraphFixture graph;
    HirNode* node = graph.constant(variant % 2 == 0 ? TypeKind::Int : TypeKind::Bool, 1);
    if (variant == 2) node->setType(nullptr);
    if (variant == 3) node->setConstant(ConstantValue());
    graph.addExpression(node);
    expectVerdict(context, graph.root, variant != 2, "CF-HIR-023");
}

void declarationOracle(TestContext& context, std::size_t variant)
{
    if (variant == 0)
    {
        HirArena arena;
        expectVerdict(context, arena.make(HirNodeKind::CompilationUnit, SourceRange()), true, "CF-HIR-024");
        return;
    }
    GraphFixture graph;
    if (variant == 1) graph.function->setSymbol(nullptr);
    if (variant == 2) graph.function->setType(nullptr);
    if (variant == 3) graph.function->setFirst(nullptr);
    if (variant == 4) graph.root->addChild(graph.constant());
    if (variant == 5)
    {
        HirNode* structure = graph.arena.make(HirNodeKind::StructDeclaration, SourceRange());
        graph.root->addChild(structure);
    }
    if (variant == 6)
    {
        HirNode* parameter = graph.arena.make(HirNodeKind::ParameterDeclaration, SourceRange());
        graph.function->addChild(parameter);
    }
    if (variant == 7)
    {
        for (std::size_t index = 0; index < 256; ++index)
        {
            HirNode* declaration = graph.arena.make(HirNodeKind::GlobalVariableDeclaration, SourceRange());
            graph.root->addChild(declaration);
        }
    }
    expectVerdict(context, graph.root, false, "CF-HIR-024");
}

void statementOracle(TestContext& context, std::size_t variant)
{
    GraphFixture graph;
    bool accepted = false;
    if (variant == 0)
    {
        graph.body->addChild(graph.arena.make(HirNodeKind::Return, SourceRange()));
        accepted = true;
    }
    else if (variant == 1)
    {
        graph.body->addChild(graph.arena.make(HirNodeKind::Break, SourceRange()));
    }
    else if (variant == 2)
    {
        graph.body->addChild(graph.arena.make(HirNodeKind::Continue, SourceRange()));
    }
    else if (variant == 3)
    {
        HirNode* condition = graph.constant(TypeKind::Int);
        HirNode* branch = graph.arena.make(HirNodeKind::If, SourceRange());
        branch->setFirst(condition);
        branch->setSecond(graph.arena.make(HirNodeKind::Block, SourceRange()));
        graph.body->addChild(branch);
    }
    else if (variant == 4)
    {
        HirNode* loop = graph.arena.make(HirNodeKind::While, SourceRange());
        loop->setFirst(graph.constant(TypeKind::Bool));
        HirNode* loopBody = graph.arena.make(HirNodeKind::Block, SourceRange());
        loopBody->addChild(graph.arena.make(HirNodeKind::Break, SourceRange()));
        loop->setSecond(loopBody);
        graph.body->addChild(loop);
        accepted = true;
    }
    else if (variant == 5)
    {
        HirNode* loop = graph.arena.make(HirNodeKind::For, SourceRange());
        loop->setThird(graph.arena.make(HirNodeKind::Block, SourceRange()));
        loop->addChild(graph.arena.make(HirNodeKind::Block, SourceRange()));
        graph.body->addChild(loop);
    }
    else if (variant == 6)
    {
        graph.addExpression(nullptr);
    }
    else
    {
        for (std::size_t index = 0; index < 256; ++index)
        {
            graph.body->addChild(graph.arena.make(HirNodeKind::Break, SourceRange()));
        }
    }
    expectVerdict(context, graph.root, accepted, "CF-HIR-025");
}

void storageOracle(TestContext& context, std::size_t variant)
{
    GraphFixture graph;
    Type* valueType = graph.types.builtin(TypeKind::Int);
    Type* pointerType = graph.types.pointerTo(valueType);
    Symbol variable(SymbolKind::Variable, "x", valueType, nullptr);
    HirNode* address = graph.arena.make(HirNodeKind::SymbolAddress, SourceRange());
    address->setType(pointerType);
    address->setSymbol(&variable);
    HirNode* node = nullptr;
    bool accepted = false;
    if (variant == 0)
    {
        node = graph.constant();
        accepted = true;
    }
    else if (variant == 1)
    {
        node = address;
        accepted = true;
    }
    else if (variant == 2)
    {
        node = graph.arena.make(HirNodeKind::Load, SourceRange());
        node->setType(valueType);
        node->setFirst(address);
        accepted = true;
    }
    else if (variant == 3)
    {
        node = graph.arena.make(HirNodeKind::Load, SourceRange());
        node->setType(valueType);
        node->setFirst(graph.constant());
    }
    else
    {
        node = graph.arena.make(HirNodeKind::Store, SourceRange());
        node->setType(valueType);
        node->setFirst(variant == 4 ? address : graph.constant());
        node->setSecond(variant == 5 ? graph.constant(TypeKind::Long) : graph.constant());
        accepted = variant == 4 || variant >= 6;
    }
    graph.addExpression(node);
    expectVerdict(context, graph.root, accepted, "CF-HIR-026");
}

void operatorOracle(TestContext& context, std::size_t variant)
{
    GraphFixture graph;
    HirNode* node = nullptr;
    bool accepted = false;
    if (variant < 2)
    {
        node = graph.arena.make(HirNodeKind::Conversion, SourceRange());
        node->setType(graph.types.builtin(TypeKind::Long));
        if (variant == 0)
        {
            node->setFirst(graph.constant());
            node->setConversionKind(ConversionKind::ImplicitNumeric);
            accepted = true;
        }
    }
    else if (variant < 4)
    {
        node = graph.arena.make(HirNodeKind::Unary, SourceRange());
        node->setType(graph.types.builtin(TypeKind::Int));
        node->setOperationType(graph.types.builtin(TypeKind::Int));
        node->setOperation(variant == 2 ? TokenKind::Minus : TokenKind::Invalid);
        node->setFirst(graph.constant());
        accepted = variant == 2;
    }
    else if (variant < 6)
    {
        node = graph.arena.make(HirNodeKind::Binary, SourceRange());
        node->setType(graph.types.builtin(TypeKind::Int));
        node->setOperationType(graph.types.builtin(TypeKind::Int));
        node->setOperation(TokenKind::Plus);
        node->setFirst(graph.constant());
        if (variant == 4) node->setSecond(graph.constant());
        accepted = variant == 4;
    }
    else
    {
        node = graph.arena.make(HirNodeKind::Increment, SourceRange());
        node->setType(graph.types.builtin(TypeKind::Int));
        node->setOperation(variant == 6 ? TokenKind::PlusPlus : TokenKind::Plus);
        accepted = false;
    }
    graph.addExpression(node);
    expectVerdict(context, graph.root, accepted, "CF-HIR-027");
}

void addressOracle(TestContext& context, std::size_t variant)
{
    GraphFixture graph;
    Type* integer = graph.types.builtin(TypeKind::Int);
    Type* pointer = graph.types.pointerTo(integer);
    HirNode* node = graph.arena.make(HirNodeKind::IndexAddress, SourceRange());
    node->setType(pointer);
    node->setOperationType(variant == 1 ? integer : pointer);
    HirNode* base = graph.arena.make(HirNodeKind::Constant, SourceRange());
    base->setType(pointer);
    base->setConstant(ConstantValue::nullValue());
    node->setFirst(variant == 2 ? nullptr : base);
    node->setSecond(variant == 3 ? graph.constant(TypeKind::Float) : graph.constant());
    graph.addExpression(node);
    expectVerdict(context, graph.root, variant == 0 || variant >= 4, "CF-HIR-028");
}

void memoryOracle(TestContext& context, std::size_t variant)
{
    GraphFixture graph;
    Type* integer = graph.types.builtin(TypeKind::Int);
    Type* pointer = graph.types.pointerTo(integer);
    HirNode* node = nullptr;
    bool accepted = false;
    if (variant < 3)
    {
        node = graph.arena.make(HirNodeKind::Allocation, SourceRange());
        node->setType(variant == 1 ? integer : pointer);
        node->setOperationType(integer);
        node->setArrayForm(variant == 2);
        if (variant == 2) node->setFirst(graph.constant());
        accepted = variant != 1;
    }
    else if (variant < 6)
    {
        node = graph.arena.make(HirNodeKind::Deallocation, SourceRange());
        node->setType(graph.types.builtin(TypeKind::Void));
        node->setFirst(variant == 3 ? graph.constant(TypeKind::NullLiteral) :
            (variant == 4 ? graph.constant() : nullptr));
        accepted = variant == 3;
    }
    else
    {
        node = graph.arena.make(HirNodeKind::StackAllocation, SourceRange());
        node->setType(pointer);
        node->setOperationType(integer);
        node->setFirst(variant == 6 ? graph.constant() : graph.constant(TypeKind::Float));
        accepted = variant == 6;
    }
    graph.addExpression(node);
    expectVerdict(context, graph.root, accepted, "CF-HIR-029");
}
}

// Registers CF-HIR-023..029 verifier truth tables and first-error contracts.
void registerHIRVerifierModule(TestRegistry& registry)
{
    addMatrix(registry, 23, "unit", classifierOracle);
    addMatrix(registry, 24, "component", declarationOracle);
    addMatrix(registry, 25, "component", statementOracle);
    addMatrix(registry, 26, "component", storageOracle);
    addMatrix(registry, 27, "component", operatorOracle);
    addMatrix(registry, 28, "component", addressOracle);
    addMatrix(registry, 29, "component", memoryOracle);
}
}
