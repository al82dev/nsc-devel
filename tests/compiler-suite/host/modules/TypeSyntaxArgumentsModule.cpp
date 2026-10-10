#include "TypeSyntaxTestSupport.h"

#include <string>

namespace nsc::suite
{
namespace
{
using type_syntax_test::addCase;
using type_syntax_test::type;
}

void registerTypeSyntaxArgumentsModule(TestRegistry& registry)
{
    addCase(registry, "TA-AST-124", "CA-AST-013", {"ast", "type-syntax", "argument"},
        [](TestContext& context) {
            const auto value = type();
            context.expect(value.typeArguments().empty(), "TA-AST-124 type arguments must default empty");
        });

    addCase(registry, "TA-AST-125", "CA-AST-013", {"ast", "type-syntax", "null"},
        [](TestContext& context) {
            auto value = type();
            value.addTypeArgument(nullptr);
            context.expect(value.typeArguments().size() == 1 && value.typeArguments()[0] == nullptr,
                "TA-AST-125 addTypeArgument must preserve null");
        });

    addCase(registry, "TA-AST-126", "CA-AST-013", {"ast", "type-syntax", "order"},
        [](TestContext& context) {
            auto value = type(); auto a = type("A"); auto b = type("B");
            value.addTypeArgument(&a); value.addTypeArgument(&b); value.addTypeArgument(&a);
            context.expect(value.typeArguments().size() == 3 && value.typeArguments()[0] == &a &&
                    value.typeArguments()[1] == &b && value.typeArguments()[2] == &a,
                "TA-AST-126 arguments must preserve insertion order and duplicates");
        });

    addCase(registry, "TA-AST-127", "CA-AST-013", {"ast", "type-syntax", "clear"},
        [](TestContext& context) {
            auto value = type(); auto argument = type("A");
            value.addTypeArgument(&argument); value.clearTypeArguments();
            context.expect(value.typeArguments().empty(), "TA-AST-127 clearTypeArguments must empty the vector");
        });

    addCase(registry, "TA-AST-128", "CA-AST-013", {"ast", "type-syntax", "tuple"},
        [](TestContext& context) {
            auto value = type();
            context.expect(!value.isTuple(), "TA-AST-128 tuple must default false");
            value.setTuple(); context.expect(value.isTuple(), "TA-AST-128 default setter must enable tuple");
            value.setTuple(false); context.expect(!value.isTuple(), "TA-AST-128 explicit false must clear tuple");
        });

    addCase(registry, "TA-AST-129", "CA-AST-013", {"ast", "type-syntax", "tuple-element"},
        [](TestContext& context) {
            auto value = type(); auto a = type("A"); auto b = type("B");
            value.addTupleElement(&a, "left"); value.addTupleElement(&b, "right");
            context.expect(value.isTuple(), "TA-AST-129 addTupleElement must force tuple mode");
            context.expect(value.typeArguments().size() == 2 && value.tupleElementNames().size() == 2 &&
                    value.typeArguments()[0] == &a && value.typeArguments()[1] == &b &&
                    value.tupleElementNames()[0] == "left" && value.tupleElementNames()[1] == "right",
                "TA-AST-129 tuple types and names must remain index-aligned");
        });

    addCase(registry, "TA-AST-130", "CA-AST-013", {"ast", "type-syntax", "tuple-name"},
        [](TestContext& context) {
            auto value = type(); auto a = type("A"); auto b = type("B");
            value.addTupleElement(&a); value.addTupleElement(&b, "named");
            context.expect(value.tupleElementNames().size() == 2 &&
                    value.tupleElementNames()[0].empty() && value.tupleElementNames()[1] == "named",
                "TA-AST-130 empty and non-empty tuple names must remain exact");
        });

    addCase(registry, "TA-AST-131", "CA-AST-013", {"ast", "type-syntax", "misalignment"},
        [](TestContext& context) {
            auto value = type(); auto a = type("A"); auto b = type("B");
            value.addTupleElement(&a, "a"); value.addTypeArgument(&b);
            context.expect(value.typeArguments().size() == 2 && value.tupleElementNames().size() == 1,
                "TA-AST-131 direct argument append must expose the current tuple-name misalignment");
        });

    addCase(registry, "TA-AST-132", "CA-AST-013", {"ast", "type-syntax", "clear"},
        [](TestContext& context) {
            auto value = type(); auto element = type("E");
            value.addTupleElement(&element, "kept"); value.clearTypeArguments();
            context.expect(value.typeArguments().empty() && value.tupleElementNames().size() == 1 &&
                    value.tupleElementNames()[0] == "kept",
                "TA-AST-132 clearing types must leave tuple names intact");
        });

    addCase(registry, "TA-AST-133", "CA-AST-013", {"ast", "type-syntax", "lifetime"},
        [](TestContext& context) {
            auto value = type(); auto external = type("External");
            { std::string temporary = "owned-name"; value.addTupleElement(&external, std::move(temporary)); }
            context.expect(value.typeArguments()[0] == &external,
                "TA-AST-133 tuple type pointer must remain non-owning identity");
            context.expect(value.tupleElementNames()[0] == "owned-name",
                "TA-AST-133 tuple name must own temporary input bytes");
        });
}
}
