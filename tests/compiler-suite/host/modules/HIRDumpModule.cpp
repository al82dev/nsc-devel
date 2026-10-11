#include "HIRContractSupport.h"

#include "nativesharp/HIR/HirArena.h"
#include "nativesharp/HIR/HirDumper.h"
#include "nativesharp/Sema/ConstantValue.h"

#include <array>
#include <sstream>
#include <string>

namespace nsc::suite
{
namespace
{
using hir_contract_support::addMatrix;
using nativesharp::SourceRange;
using nativesharp::hir::HirArena;
using nativesharp::hir::HirDumper;
using nativesharp::hir::HirNode;
using nativesharp::hir::HirNodeKind;
using nativesharp::sema::ConstantValue;

std::string dump(const HirNode* node)
{
    std::ostringstream output;
    HirDumper().dump(node, output);
    return output.str();
}

void constantDumpOracle(TestContext& context, std::size_t variant)
{
    HirArena arena;
    HirNode* node = arena.make(HirNodeKind::Constant, SourceRange());
    const std::array values{ConstantValue::signedInteger(-7), ConstantValue::unsignedInteger(9),
        ConstantValue::floating(1.25), ConstantValue::boolean(true), ConstantValue::nullValue(),
        ConstantValue::stringValue(std::string("a\\\"\n\r\t\0b", 8)), ConstantValue::stringValue(std::string(1, '\x01')),
        ConstantValue::stringValue("plain")};
    constexpr std::array<std::string_view, 8> expected{"Constant constant=-7\n", "Constant constant=9u\n",
        "Constant constant=1.25\n", "Constant constant=true\n", "Constant constant=null\n",
        "Constant constant=\"a\\\\\\\"\\n\\r\\t\\0b\"\n", "Constant constant=\"\\x01\"\n",
        "Constant constant=\"plain\"\n"};
    node->setConstant(values[variant]);
    context.expect(dump(node) == expected[variant],
        "CF-HIR-021 constant dump must match the byte-exact encoding matrix");
}

void recursiveDumpOracle(TestContext& context, std::size_t variant)
{
    HirArena arena;
    if (variant == 2)
    {
        context.expect(dump(nullptr).empty(), "CF-HIR-022 a null root must emit no bytes");
        return;
    }
    HirNode* root = arena.make(HirNodeKind::CompilationUnit, SourceRange());
    HirNode* first = arena.make(HirNodeKind::Constant, SourceRange());
    HirNode* second = arena.make(HirNodeKind::Load, SourceRange());
    HirNode* third = arena.make(HirNodeKind::Return, SourceRange());
    if (variant != 3) root->setFirst(first);
    if (variant != 4) root->setSecond(second);
    if (variant != 5) root->setThird(third);
    const std::size_t childCount = variant == 7 ? 256 : variant;
    for (std::size_t index = 0; index < childCount; ++index)
    {
        root->addChild(arena.make(HirNodeKind::Block, SourceRange()));
    }
    const std::string text = dump(root);
    context.expect(text.starts_with("CompilationUnit\n"),
        "CF-HIR-022 dump must begin with the root in preorder");
    if (root->first() != nullptr)
    {
        context.expect(text.find("  first: Constant\n") != std::string::npos,
            "CF-HIR-022 first edge label must be exact");
    }
    const std::size_t firstPosition = text.find("first:");
    const std::size_t secondPosition = text.find("second:");
    const std::size_t thirdPosition = text.find("third:");
    context.expect(firstPosition == std::string::npos || secondPosition == std::string::npos ||
            firstPosition < secondPosition,
        "CF-HIR-022 first edge must precede second when both exist");
    context.expect(secondPosition == std::string::npos || thirdPosition == std::string::npos ||
            secondPosition < thirdPosition,
        "CF-HIR-022 second edge must precede third when both exist");
}
}

// Registers CF-HIR-021..022 byte-exact constant and graph dump matrices.
void registerHIRDumpModule(TestRegistry& registry)
{
    addMatrix(registry, 21, "unit", constantDumpOracle);
    addMatrix(registry, 22, "unit", recursiveDumpOracle);
}
}
