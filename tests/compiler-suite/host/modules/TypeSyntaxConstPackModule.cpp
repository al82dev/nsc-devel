#include "TypeSyntaxTestSupport.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"

namespace nsc::suite
{
namespace
{
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using type_syntax_test::addCase;
using type_syntax_test::range;
using type_syntax_test::type;
}

void registerTypeSyntaxConstPackModule(TestRegistry& registry)
{
    addCase(registry, "TA-AST-150", "CA-AST-015", {"ast", "type-syntax", "const-expression"},
        [](TestContext& context) {
            const auto value = type();
            context.expect(value.constExpression() == nullptr,
                "TA-AST-150 const expression must default null");
        });

    addCase(registry, "TA-AST-151", "CA-AST-015", {"ast", "type-syntax", "const-expression"},
        [](TestContext& context) {
            auto value = type(); AstNode expression(AstNodeKind::ExpressionStatement, range(4, 5));
            value.setConstExpression(&expression);
            context.expect(value.constExpression() == &expression,
                "TA-AST-151 const expression pointer identity must round-trip");
            value.setConstExpression(nullptr);
            context.expect(value.constExpression() == nullptr,
                "TA-AST-151 const expression must reset to null");
        });

    addCase(registry, "TA-AST-152", "CA-AST-015", {"ast", "type-syntax", "pack"},
        [](TestContext& context) {
            auto value = type();
            context.expect(!value.isPackExpansion(), "TA-AST-152 pack must default false");
            value.setPackExpansion(); context.expect(value.isPackExpansion(), "TA-AST-152 pack must toggle true");
            value.setPackExpansion(false); context.expect(!value.isPackExpansion(), "TA-AST-152 pack must reset false");
        });

    addCase(registry, "TA-AST-153", "CA-AST-015", {"ast", "type-syntax", "orthogonal"},
        [](TestContext& context) {
            auto value = type(); AstNode expression(AstNodeKind::ExpressionStatement, range(4, 5));
            value.setConstExpression(&expression); value.setPackExpansion();
            context.expect(value.constExpression() == &expression && value.isPackExpansion(),
                "TA-AST-153 const expression and pack marker must coexist independently");
        });

    addCase(registry, "TA-AST-154", "CA-AST-015", {"ast", "type-syntax", "non-owning"},
        [](TestContext& context) {
            AstNode externalExpression(AstNodeKind::ExpressionStatement, range(40, 41, 8202));
            auto value = type(); value.setConstExpression(&externalExpression);
            context.expect(value.constExpression() == &externalExpression,
                "TA-AST-154 expression from another owner must remain the same non-owning pointer");
        });

    addCase(registry, "TA-AST-155", "CA-AST-015", {"ast", "type-syntax", "no-validation"},
        [](TestContext& context) {
            AstNode expression(AstNodeKind::ExpressionStatement, range(4, 5));
            auto value = type("OrdinaryType"); value.setConstExpression(&expression); value.setPackExpansion();
            context.expect(value.baseName() == "OrdinaryType" && value.constExpression() == &expression &&
                    value.isPackExpansion(),
                "TA-AST-155 container must not restrict const/pack carriers to generic-argument context");
        });
}
}
