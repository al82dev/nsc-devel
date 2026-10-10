#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstNode.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using nativesharp::ast::FfiUnwindSyntaxKind;
using nativesharp::ast::FloatingPointModeSyntaxKind;

// Registers one independent scalar-state case with canonical metadata. Complexity is O(1).
void addScalarCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{
        std::move(id), "CA-AST-010", "Host.AST", "unit", "PR", {"ast", "node", "scalar"},
        std::move(function),
    });
}

// Registers every boolean, policy, and numeric default. Complexity is O(1).
void registerScalarDefaultsCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-090", [](TestContext& context) {
        const AstNode node(AstNodeKind::CompilationUnit, {});
        context.expect(!node.flag() && !node.isReference() && !node.isVariadic() && !node.isUnsafe(),
            "TA-AST-090 boolean channels must default false");
        context.expect(node.floatingPointMode() == FloatingPointModeSyntaxKind::Inherit,
            "TA-AST-090 floating-point mode must default Inherit");
        context.expect(node.ffiUnwindPolicy() == FfiUnwindSyntaxKind::Default,
            "TA-AST-090 unwind policy must default Default");
        context.expect(node.integerValue() == 0, "TA-AST-090 integer value must default zero");
        context.expect(node.floatingValue() == 0.0, "TA-AST-090 floating value must default zero");
    });
}

// Registers true and false round trips for the generic flag channel. Complexity is O(1).
void registerFlagCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-091", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setFlag(true);
        context.expect(node.flag(), "TA-AST-091 flag true must round-trip");
        node.setFlag(false);
        context.expect(!node.flag(), "TA-AST-091 flag false must round-trip");
    });
}

// Registers reference state independently from the generic flag. Complexity is O(1).
void registerReferenceCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-092", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setFlag(true);
        node.setReference(true);
        context.expect(node.flag() && node.isReference(), "TA-AST-092 flag and reference may both be true");
        node.setReference(false);
        context.expect(node.flag() && !node.isReference(), "TA-AST-092 reference reset must not change flag");
    });
}

// Registers variadic state independently from other booleans. Complexity is O(1).
void registerVariadicCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-093", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setReference(true);
        node.setVariadic(true);
        context.expect(node.isReference() && node.isVariadic(), "TA-AST-093 reference and variadic may both be true");
        node.setVariadic(false);
        context.expect(node.isReference() && !node.isVariadic(),
            "TA-AST-093 variadic reset must not change reference");
    });
}

// Registers unsafe state independently from all other booleans. Complexity is O(1).
void registerUnsafeCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-094", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setVariadic(true);
        node.setUnsafe(true);
        context.expect(node.isVariadic() && node.isUnsafe(), "TA-AST-094 variadic and unsafe may both be true");
        node.setUnsafe(false);
        context.expect(node.isVariadic() && !node.isUnsafe(), "TA-AST-094 unsafe reset must not change variadic");
    });
}

// Registers round trips for all floating-point modes. Complexity is O(n).
void registerFloatingModesCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-095", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        constexpr std::array modes{FloatingPointModeSyntaxKind::Inherit, FloatingPointModeSyntaxKind::Strict,
            FloatingPointModeSyntaxKind::Precise, FloatingPointModeSyntaxKind::Fast};
        for (const auto mode : modes)
        {
            node.setFloatingPointMode(mode);
            context.expect(node.floatingPointMode() == mode, "TA-AST-095 floating-point mode must round-trip");
        }
    });
}

// Registers round trips for all FFI unwind policies. Complexity is O(n).
void registerUnwindPoliciesCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-096", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        constexpr std::array policies{
            FfiUnwindSyntaxKind::Default, FfiUnwindSyntaxKind::Forbidden, FfiUnwindSyntaxKind::Abort};
        for (const auto policy : policies)
        {
            node.setFfiUnwindPolicy(policy);
            context.expect(node.ffiUnwindPolicy() == policy, "TA-AST-096 unwind policy must round-trip");
        }
    });
}

// Registers exact integer storage at both uint64 boundaries. Complexity is O(1).
void registerIntegerBoundariesCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-097", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setIntegerValue(0);
        context.expect(node.integerValue() == 0, "TA-AST-097 zero must round-trip");
        node.setIntegerValue(std::numeric_limits<std::uint64_t>::max());
        context.expect(node.integerValue() == std::numeric_limits<std::uint64_t>::max(),
            "TA-AST-097 uint64 maximum must round-trip");
    });
}

// Registers semantic and signed-zero preservation for representative double values. Complexity is O(1).
void registerFloatingValuesCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-098", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setFloatingValue(0.0);
        context.expect(node.floatingValue() == 0.0 && !std::signbit(node.floatingValue()),
            "TA-AST-098 positive zero sign must be preserved");
        node.setFloatingValue(-0.0);
        context.expect(node.floatingValue() == 0.0 && std::signbit(node.floatingValue()),
            "TA-AST-098 negative zero sign must be preserved");
        node.setFloatingValue(17.25);
        context.expect(node.floatingValue() == 17.25, "TA-AST-098 finite value must be exact");
        node.setFloatingValue(std::numeric_limits<double>::infinity());
        context.expect(std::isinf(node.floatingValue()), "TA-AST-098 infinity must remain infinity");
        node.setFloatingValue(std::numeric_limits<double>::quiet_NaN());
        context.expect(std::isnan(node.floatingValue()), "TA-AST-098 NaN must remain NaN");
    });
}

// Registers simultaneous activation of every scalar channel without cross-field mutation. Complexity is O(1).
void registerCombinedStateCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-099", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setFlag(true);
        node.setReference(true);
        node.setVariadic(true);
        node.setUnsafe(true);
        node.setFloatingPointMode(FloatingPointModeSyntaxKind::Fast);
        node.setFfiUnwindPolicy(FfiUnwindSyntaxKind::Abort);
        node.setIntegerValue(99);
        node.setFloatingValue(9.5);
        context.expect(node.flag() && node.isReference() && node.isVariadic() && node.isUnsafe(),
            "TA-AST-099 all boolean channels must coexist");
        context.expect(node.floatingPointMode() == FloatingPointModeSyntaxKind::Fast &&
                node.ffiUnwindPolicy() == FfiUnwindSyntaxKind::Abort,
            "TA-AST-099 both policy channels must coexist");
        context.expect(node.integerValue() == 99 && node.floatingValue() == 9.5,
            "TA-AST-099 both numeric channels must coexist");
    });
}

// Registers scalar values that are semantically unrelated to the selected kind. Complexity is O(1).
void registerIncoherentKindCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-100", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setReference(true);
        node.setVariadic(true);
        node.setUnsafe(true);
        node.setIntegerValue(1234);
        node.setFloatingValue(56.75);
        context.expect(node.kind() == AstNodeKind::CompilationUnit,
            "TA-AST-100 semantically unrelated setters must not alter kind");
        context.expect(node.isReference() && node.isVariadic() && node.isUnsafe(),
            "TA-AST-100 incoherent booleans must be retained");
        context.expect(node.integerValue() == 1234 && node.floatingValue() == 56.75,
            "TA-AST-100 incoherent numerics must be retained");
    });
}

// Registers independent last-write-wins behavior for every scalar channel. Complexity is O(1).
void registerResetSequenceCase(TestRegistry& registry)
{
    addScalarCase(registry, "TA-AST-101", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setFlag(true);
        node.setFlag(false);
        node.setReference(true);
        node.setReference(false);
        node.setVariadic(true);
        node.setVariadic(false);
        node.setUnsafe(true);
        node.setUnsafe(false);
        node.setFloatingPointMode(FloatingPointModeSyntaxKind::Fast);
        node.setFloatingPointMode(FloatingPointModeSyntaxKind::Precise);
        node.setFfiUnwindPolicy(FfiUnwindSyntaxKind::Abort);
        node.setFfiUnwindPolicy(FfiUnwindSyntaxKind::Forbidden);
        node.setIntegerValue(1);
        node.setIntegerValue(2);
        node.setFloatingValue(1.0);
        node.setFloatingValue(2.0);
        context.expect(!node.flag() && !node.isReference() && !node.isVariadic() && !node.isUnsafe(),
            "TA-AST-101 final boolean values must be false");
        context.expect(node.floatingPointMode() == FloatingPointModeSyntaxKind::Precise &&
                node.ffiUnwindPolicy() == FfiUnwindSyntaxKind::Forbidden,
            "TA-AST-101 final policy values must be exact");
        context.expect(node.integerValue() == 2 && node.floatingValue() == 2.0,
            "TA-AST-101 final numeric values must be exact");
    });
}
}

// Registers CA-AST-010 orthogonal boolean, policy, and numeric state contracts.
void registerAstNodeScalarStateModule(TestRegistry& registry)
{
    registerScalarDefaultsCase(registry);
    registerFlagCase(registry);
    registerReferenceCase(registry);
    registerVariadicCase(registry);
    registerUnsafeCase(registry);
    registerFloatingModesCase(registry);
    registerUnwindPoliciesCase(registry);
    registerIntegerBoundariesCase(registry);
    registerFloatingValuesCase(registry);
    registerCombinedStateCase(registry);
    registerIncoherentKindCase(registry);
    registerResetSequenceCase(registry);
}
}
