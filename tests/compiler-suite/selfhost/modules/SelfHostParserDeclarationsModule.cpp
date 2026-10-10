#include "SelfHostParserTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_parser_support::PendingContractRange;

constexpr std::string_view kDeclarationRequirement =
    "the controller has no selfhost declaration/compilation-unit executor with complete token "
    "input, AST ownership, modifier/throws metadata, diagnostics, frozen root, and EOF oracle";

constexpr std::array<PendingContractRange, 4> kContracts{{
    {546, 553, "CSH-PAR-013", "component", "node metadata operand order and diagnostic",
     kDeclarationRequirement},
    {554, 561, "CSH-PAR-014", "component", "node fields children modifiers throws and diagnostics",
     kDeclarationRequirement},
    {562, 569, "CSH-PAR-015", "component", "tree hierarchy metadata order and diagnostics",
     kDeclarationRequirement},
    {570, 578, "CSH-PAR-016", "integration", "root children frozen state first error and EOF consumption",
     kDeclarationRequirement},
}};
}

void registerSelfHostParserDeclarationsModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_parser_support::addInfrastructurePendingRange(registry, range);
    }
}
}
