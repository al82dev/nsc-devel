#include "SelfHostParserTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_parser_support::PendingContractRange;

constexpr std::string_view kStatementRequirement =
    "the controller has no selfhost statement-parser executor with block ownership, recovery "
    "progress, label/section ordering, diagnostics, and partial-tree rejection";

constexpr std::array<PendingContractRange, 4> kContracts{{
    {513, 519, "CSH-PAR-009", "component", "tree order cursor and diagnostics",
     kStatementRequirement},
    {520, 527, "CSH-PAR-010", "component", "node children flags cursor and diagnostics",
     kStatementRequirement},
    {528, 536, "CSH-PAR-011", "component", "section ordering labels and cursor",
     kStatementRequirement},
    {537, 545, "CSH-PAR-012", "component", "tree roles order and diagnostics",
     kStatementRequirement},
}};
}

void registerSelfHostParserStatementsModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_parser_support::addInfrastructurePendingRange(registry, range);
    }
}
}
