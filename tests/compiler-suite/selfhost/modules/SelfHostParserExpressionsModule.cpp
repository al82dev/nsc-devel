#include "SelfHostParserTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_parser_support::PendingContractRange;

constexpr std::string_view kExpressionRequirement =
    "the controller has no selfhost expression-parser executor with exact cursor snapshots, "
    "syntax-tree ownership, diagnostic capture, and independent precedence reconstruction";

constexpr std::array<PendingContractRange, 4> kContracts{{
    {480, 486, "CSH-PAR-005", "component", "cursor before/after and syntax shape",
     kExpressionRequirement},
    {487, 495, "CSH-PAR-006", "component", "nested tree arguments and cursor",
     kExpressionRequirement},
    {496, 503, "CSH-PAR-007", "component", "node kind operator children type text and diagnostics",
     kExpressionRequirement},
    {504, 512, "CSH-PAR-008", "component", "tree shape operators ranges and cursor",
     kExpressionRequirement},
}};
}

void registerSelfHostParserExpressionsModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_parser_support::addInfrastructurePendingRange(registry, range);
    }
}
}
