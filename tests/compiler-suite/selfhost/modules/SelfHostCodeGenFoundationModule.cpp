#include "SelfHostCodeGenTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_codegen_support::PendingContract;

constexpr std::array<PendingContract, 10> kFoundationContracts{{
    {1U, 52U, 6U, 0U, "unit", "lookup precedence push/pop and break-state transitions"},
    {2U, 58U, 9U, 2U, "component", "reachable symbol set and selected entry"},
    {3U, 67U, 6U, 0U, "unit", "LLVM kind width element and aggregate representation"},
    {4U, 73U, 6U, 0U, "unit", "parameter count types receiver return and call convention"},
    {5U, 79U, 8U, 1U, "component", "exact names linkage initializers and declaration set"},
    {6U, 87U, 8U, 1U, "component", "selected symbol signature and unresolved diagnostics"},
    {7U, 95U, 6U, 0U, "unit", "opcode source/destination widths signedness and result type"},
    {8U, 101U, 6U, 0U, "unit", "instruction kind ordering alignment mask and stored bytes"},
    {9U, 107U, 7U, 1U, "component", "length bytes allocation calls and comparison result"},
    {10U, 114U, 7U, 1U, "integration", "selected hook signature opcode ordering and result"},
}};

constexpr std::string_view kRequirement =
    "the controller has no SelfHost.CodeGen executor or controlled NativeSharp.LLVM.C boundary "
    "for observing LLVM types, values, instructions, CFG, ABI, diagnostics, and artifacts";
}

// Materializes TSH-00052..00120. No case enters ordinary execution because replacing LLVM C and
// self-host semantic producers with copied logic would test a surrogate rather than the product.
void registerSelfHostCodeGenFoundationModule(TestRegistry& registry)
{
    for (const PendingContract& contract : kFoundationContracts)
    {
        selfhost_codegen_support::addInfrastructureContract(registry, contract, kRequirement);
    }
}
}
