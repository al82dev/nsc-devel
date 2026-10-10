#include "SelfHostParserTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_parser_support::PendingContractRange;

constexpr std::string_view kParserRequirement =
    "the controller has no selfhost Parser executor with owned source, lexer/token stream, AST "
    "arena, diagnostics, cursor trace, and malformed-input mutation";

constexpr std::array<PendingContractRange, 4> kContracts{{
    {452, 458, "CSH-PAR-001", "unit", "cursor trace consumed ranges and first error",
     kParserRequirement},
    {459, 465, "CSH-PAR-002", "unit", "node fields known-type set and auxiliary text",
     kParserRequirement},
    {466, 472, "CSH-PAR-003", "component", "tree shape type text cursor and diagnostics",
     kParserRequirement},
    {473, 479, "CSH-PAR-004", "unit", "node kind text and diagnostic", kParserRequirement},
}};
}

void registerSelfHostParserTypesLiteralsModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_parser_support::addInfrastructurePendingRange(registry, range);
    }
}
}
