#include "TypeSyntaxTestSupport.h"

#include <limits>
#include <string>

namespace nsc::suite
{
namespace
{
using type_syntax_test::addCase;
using type_syntax_test::type;
}

void registerTypeSyntaxOriginModule(TestRegistry& registry)
{
    addCase(registry, "TA-AST-170", "CA-AST-017", {"ast", "type-syntax", "origin"},
        [](TestContext& context) {
            const auto value = type();
            context.expect(!value.originProjectOrdinal() && !value.originNamespace(),
                "TA-AST-170 both origin overrides must default nullopt");
        });

    addCase(registry, "TA-AST-171", "CA-AST-017", {"ast", "type-syntax", "project-origin"},
        [](TestContext& context) {
            auto value = type(); value.setOriginProjectOrdinal(0);
            context.expect(value.originProjectOrdinal() && *value.originProjectOrdinal() == 0,
                "TA-AST-171 project ordinal zero must populate exactly");
            value.setOriginProjectOrdinal(std::numeric_limits<std::size_t>::max());
            context.expect(value.originProjectOrdinal() &&
                    *value.originProjectOrdinal() == std::numeric_limits<std::size_t>::max(),
                "TA-AST-171 maximum project ordinal must populate exactly");
        });

    addCase(registry, "TA-AST-172", "CA-AST-017", {"ast", "type-syntax", "namespace-origin"},
        [](TestContext& context) {
            auto value = type(); value.setOriginNamespace("");
            context.expect(value.originNamespace() && value.originNamespace()->empty(),
                "TA-AST-172 empty namespace must be populated, not nullopt");
            value.setOriginNamespace("A.B");
            context.expect(value.originNamespace() && *value.originNamespace() == "A.B",
                "TA-AST-172 non-empty namespace must remain exact");
        });

    addCase(registry, "TA-AST-173", "CA-AST-017", {"ast", "type-syntax", "ownership"},
        [](TestContext& context) {
            auto value = type();
            { std::string temporary = "Temporary.Namespace"; value.setOriginNamespace(std::move(temporary)); }
            context.expect(value.originNamespace() && *value.originNamespace() == "Temporary.Namespace",
                "TA-AST-173 namespace override must own temporary input");
        });

    addCase(registry, "TA-AST-174", "CA-AST-017", {"ast", "type-syntax", "orthogonal"},
        [](TestContext& context) {
            auto value = type(); value.setOriginProjectOrdinal(17); value.setOriginNamespace("N");
            context.expect(value.originProjectOrdinal() && *value.originProjectOrdinal() == 17 &&
                    value.originNamespace() && *value.originNamespace() == "N",
                "TA-AST-174 project and namespace origins must coexist independently");
        });

    addCase(registry, "TA-AST-175", "CA-AST-017", {"ast", "type-syntax", "replacement"},
        [](TestContext& context) {
            auto value = type();
            value.setOriginProjectOrdinal(1); value.setOriginProjectOrdinal(2);
            value.setOriginNamespace("First"); value.setOriginNamespace("Second");
            context.expect(value.originProjectOrdinal() && *value.originProjectOrdinal() == 2 &&
                    value.originNamespace() && *value.originNamespace() == "Second",
                "TA-AST-175 repeated setters must keep the last values");
        });

    addCase(registry, "TA-AST-176", "CA-AST-017", {"ast", "type-syntax", "identity"},
        [](TestContext& context) {
            auto value = type("Unqualified");
            value.setOriginProjectOrdinal(8); value.setOriginNamespace("Context.Namespace");
            context.expect(value.baseName() == "Unqualified",
                "TA-AST-176 setting origins must leave baseName identical");
        });

    addCase(registry, "TA-AST-177", "CA-AST-017", {"ast", "type-syntax", "no-clear"},
        [](TestContext& context) {
            auto value = type(); value.setOriginProjectOrdinal(0); value.setOriginNamespace("");
            value.setBaseName("Changed"); value.setRange(type_syntax_test::range(20, 30));
            context.expect(value.originProjectOrdinal().has_value() && value.originNamespace().has_value(),
                "TA-AST-177 unrelated setters must not clear populated origins");
        });
}
}
