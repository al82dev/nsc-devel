#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

int nscClassDeclarationLegacyEntry();
int nscClassMemberSyntaxLegacyEntry();
int nscConstructorSyntaxLegacyEntry();
int nscItem317PlacementConstructionSyntaxLegacyEntry();
int nscItem327BitFieldSyntaxLegacyEntry();
int nscNamespaceSyntaxLegacyEntry();
int nscNativeSharp3T01ExpressionLegacyEntry();
int nscNativeSharp3T02AggregateLegacyEntry();
int nscParserLegacyEntry();
int nscStage45ExceptionsSyntaxLegacyEntry();
int nscStage50_1OwnershipSyntaxLegacyEntry();

namespace nsc::test
{
namespace
{
using LegacyEntry = int (*)();

void addLegacyEntry(
    TestRegistry& registry,
    std::string id,
    std::string name,
    std::vector<std::string> tags,
    LegacyEntry entry)
{
    // Keep every historical assertion active while the consolidated registry
    // replaces per-file executables. Tanda 18 removes this transition bridge
    // only after the old and consolidated paths have demonstrated parity.
    registry.add(TestCase{
        std::move(id),
        "parser",
        std::move(name),
        std::move(tags),
        [entry](TestContext& context) {
            if (entry() != 0)
            {
                context.fail("legacy parser contract reported at least one failed assertion");
            }
        },
    });
}
}

void registerParserUnitTests(TestRegistry& registry)
{
    addLegacyEntry(
        registry,
        "nsc.class-declarations",
        "class declaration syntax",
        {"class", "declaration"},
        nscClassDeclarationLegacyEntry);
    addLegacyEntry(
        registry,
        "nsc.class-member-syntax",
        "class member syntax",
        {"class", "member"},
        nscClassMemberSyntaxLegacyEntry);
    addLegacyEntry(
        registry,
        "nsc.constructor-syntax",
        "constructor syntax",
        {"constructor"},
        nscConstructorSyntaxLegacyEntry);
    addLegacyEntry(
        registry,
        "nsc.item317.placement-construction-syntax",
        "placement construction syntax",
        {"construction", "placement"},
        nscItem317PlacementConstructionSyntaxLegacyEntry);
    addLegacyEntry(
        registry,
        "nsc.item327.bitfield-syntax",
        "bit-field syntax",
        {"bitfield"},
        nscItem327BitFieldSyntaxLegacyEntry);
    addLegacyEntry(
        registry,
        "nsc.namespace-syntax",
        "namespace syntax",
        {"namespace"},
        nscNamespaceSyntaxLegacyEntry);
    addLegacyEntry(
        registry,
        "nsc.ns3.t01.expressions",
        "expression syntax",
        {"expression"},
        nscNativeSharp3T01ExpressionLegacyEntry);
    addLegacyEntry(
        registry,
        "nsc.ns3.t02.aggregates",
        "aggregate syntax",
        {"aggregate"},
        nscNativeSharp3T02AggregateLegacyEntry);
    addLegacyEntry(
        registry,
        "nsc.parser",
        "parser grammar and recovery",
        {"grammar", "recovery"},
        nscParserLegacyEntry);
    addLegacyEntry(
        registry,
        "nsc.stage45.exceptions-syntax",
        "exception syntax",
        {"exception"},
        nscStage45ExceptionsSyntaxLegacyEntry);
    addLegacyEntry(
        registry,
        "nsc.stage50_1.ownership-syntax",
        "ownership syntax",
        {"ownership"},
        nscStage50_1OwnershipSyntaxLegacyEntry);
}
}
