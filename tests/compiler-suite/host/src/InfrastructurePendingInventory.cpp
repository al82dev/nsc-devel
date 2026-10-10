#include "nsc_suite/TestRegistry.h"

#include "../modules/AstInfrastructureTestSupport.h"
#include "../modules/InteropTestSupport.h"
#include "../modules/RuntimeTestSupport.h"
#include "../../modules/LinkerTestSupport.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
void addInventoryRow(std::vector<TestInventoryRow>& rows, std::string id,
    std::string contractId, std::string component)
{
    if (id.empty() || contractId.empty() || component.empty())
    {
        throw std::invalid_argument("infrastructure-pending metadata is incomplete");
    }
    rows.push_back(TestInventoryRow{
        std::move(id), std::move(contractId), std::move(component)});
}
}

std::vector<TestInventoryRow> infrastructurePendingInventory()
{
    std::vector<TestInventoryRow> rows;
    const auto& ast = ast_infrastructure_support::infrastructurePendingCases();
    const auto& interop = interop_support::infrastructurePendingCases();
    const auto& linker = linker_support::infrastructurePendingCases();
    const auto& runtime = runtime_support::infrastructurePendingCases();
    rows.reserve(ast.size() + interop.size() + linker.size() + runtime.size());

    for (const auto& testCase : ast)
    {
        addInventoryRow(rows, testCase.caseIdentifier, testCase.contractIdentifier, "Host.AST");
    }
    for (const auto& testCase : interop)
    {
        addInventoryRow(rows, testCase.caseIdentifier, testCase.contractIdentifier, "Host.Interop");
    }
    for (const auto& testCase : linker)
    {
        addInventoryRow(rows, testCase.caseIdentifier, testCase.contractIdentifier,
            testCase.component);
    }
    for (const auto& testCase : runtime)
    {
        addInventoryRow(rows, testCase.caseIdentifier, testCase.contractIdentifier, "Host.Runtime");
    }

    std::sort(rows.begin(), rows.end(),
        [](const TestInventoryRow& left, const TestInventoryRow& right)
        {
            return left.id < right.id;
        });
    const auto duplicate = std::adjacent_find(rows.begin(), rows.end(),
        [](const TestInventoryRow& left, const TestInventoryRow& right)
        {
            return left.id == right.id;
        });
    if (duplicate != rows.end())
    {
        throw std::invalid_argument(
            "duplicate infrastructure-pending case id: " + duplicate->id);
    }
    return rows;
}
}
