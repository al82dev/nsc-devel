#include "SemaTestSupport.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Sema/SemanticModel.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/TypeContext.h"

#include <array>
#include <cstddef>
#include <new>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::sema;
using sema_support::addSemaCase;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;

void c078(TestContext& c)
{
    SemanticModel model;
    AstNode node(AstNodeKind::IntegerLiteralExpression, {});
    c.expect(model.get(&node) == nullptr && model.conversions(&node).empty() &&
            model.getOperator(&node) == nullptr && model.symbol(&node) == nullptr,
        "TS-SEMA-078 empty semantic-model getters return null or empty spans");
}

void c079(TestContext& c)
{
    SemanticModel model;
    TypeContext types;
    AstNode node(AstNodeKind::IntegerLiteralExpression, {});
    ExpressionInfo first;
    first.type = types.builtin(TypeKind::Int);
    ExpressionInfo second;
    second.type = types.builtin(TypeKind::Long);
    model.set(&node, first);
    model.set(&node, second);
    c.expect(model.get(&node) != nullptr && model.get(&node)->type == second.type,
        "TS-SEMA-079 repeated expression annotation exposes the last value");
}

void c080(TestContext& c)
{
    SemanticModel model;
    AstNode node(AstNodeKind::BinaryExpression, {});
    ConversionInfo first; first.operandIndex = 2;
    ConversionInfo second; second.operandIndex = 0;
    ConversionInfo third; third.operandIndex = 1;
    model.addConversion(&node, first);
    model.addConversion(&node, second);
    model.addConversion(&node, third);
    const auto conversions = model.conversions(&node);
    c.expect(conversions.size() == 3 && conversions[0].operandIndex == 2 &&
            conversions[1].operandIndex == 0 && conversions[2].operandIndex == 1,
        "TS-SEMA-080 conversion vectors preserve exact insertion order");
}

void c081(TestContext& c)
{
    SemanticModel model;
    AstNode first(AstNodeKind::BinaryExpression, {});
    AstNode second(AstNodeKind::BinaryExpression, {});
    ConversionInfo conversion;
    model.addConversion(&first, conversion);
    c.expect(model.conversions(&first).size() == 1 && model.conversions(&second).empty(),
        "TS-SEMA-081 conversions remain isolated by owner node");
}

void c082(TestContext& c)
{
    SemanticModel model;
    AstNode node(AstNodeKind::BinaryExpression, {});
    ResolvedOperatorInfo first; first.tokenKind = nativesharp::lexer::TokenKind::Plus;
    ResolvedOperatorInfo second; second.tokenKind = nativesharp::lexer::TokenKind::Minus;
    model.setOperator(&node, first);
    model.setOperator(&node, second);
    c.expect(model.getOperator(&node) != nullptr &&
            model.getOperator(&node)->tokenKind == nativesharp::lexer::TokenKind::Minus,
        "TS-SEMA-082 repeated operator annotation exposes the last operator");
}

void c083(TestContext& c)
{
    SemanticModel model;
    AstNode node(AstNodeKind::IdentifierExpression, {});
    Symbol symbol(SymbolKind::Variable, "x", nullptr, nullptr);
    model.bindSymbol(&node, &symbol);
    c.expect(model.symbol(&node) == &symbol,
        "TS-SEMA-083 symbol binding preserves exact identity");
}

void c084(TestContext& c)
{
    SemanticModel model;
    AstNode node(AstNodeKind::IdentifierExpression, {});
    Symbol symbol(SymbolKind::Variable, "x", nullptr, nullptr);
    model.bindSymbol(nullptr, &symbol);
    model.bindSymbol(&node, nullptr);
    c.expect(model.symbol(nullptr) == nullptr && model.symbol(&node) == nullptr,
        "TS-SEMA-084 null node or symbol binding creates no map entry");
}

void c085(TestContext& c)
{
    SemanticModel model;
    TypeContext types;
    AstNode node(AstNodeKind::IdentifierExpression, {});
    Symbol symbol(SymbolKind::Variable, "x", types.builtin(TypeKind::Int), nullptr);
    ExpressionInfo expression; expression.type = symbol.type();
    ConversionInfo conversion; conversion.sourceType = symbol.type();
    ResolvedOperatorInfo operation; operation.resultType = symbol.type();
    model.set(&node, expression);
    model.addConversion(&node, conversion);
    model.setOperator(&node, operation);
    model.bindSymbol(&node, &symbol);
    c.expect(model.get(&node)->type == symbol.type() && model.conversions(&node).size() == 1 &&
            model.getOperator(&node)->resultType == symbol.type() && model.symbol(&node) == &symbol,
        "TS-SEMA-085 all four maps coexist independently for one node");
}

void c086(TestContext& c)
{
    SemanticModel model;
    alignas(AstNode) std::array<std::byte, sizeof(AstNode)> storage{};
    AstNode* first = new (storage.data()) AstNode(AstNodeKind::IntegerLiteralExpression, {});
    ExpressionInfo annotation;
    annotation.isConstant = true;
    annotation.constantValue = ConstantValue::signedInteger(1);
    model.set(first, annotation);
    first->~AstNode();
    AstNode* second = new (storage.data()) AstNode(AstNodeKind::IdentifierExpression, {});
    const ExpressionInfo* observed = model.get(second);
    c.expect(observed != nullptr && observed->constantValue.signedInteger() == 1,
        "TS-SEMA-086 pointer-key identity currently aliases a controlled reused address");
    second->~AstNode();
}
}

void registerSemaSemanticModelModule(TestRegistry& registry)
{
    const std::array<TestFunction, 9> tests = {c078, c079, c080, c081, c082, c083, c084, c085, c086};
    for (std::size_t index = 0; index < tests.size(); ++index)
    {
        const std::size_t ordinal = index + 78;
        addSemaCase(registry, "TS-SEMA-0" + std::to_string(ordinal), "CS-SEMA-010", "unit",
            tests[index], ordinal == 86 ? "NIGHTLY" : "PR");
    }
}
}
