#include "SelfHostCodeGenTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_codegen_support::PendingContract;

constexpr std::array<PendingContract, 9> kLoweringEmissionContracts{{
    {11U, 121U, 7U, 1U, "component", "cell layout call branch arguments and result phi"},
    {12U, 128U, 7U, 1U, "component", "address provenance GEP offsets and value type"},
    {13U, 135U, 7U, 1U, "component", "opcode constants branches calls memory and final LLVM type"},
    {14U, 142U, 7U, 1U, "component", "CFG edges phi values and side-effect reachability"},
    {15U, 149U, 8U, 1U, "component",
     "template constraint string call convention side effects and outputs"},
    {16U, 157U, 7U, 1U, "component",
     "block/edge sequence terminators return values and cleanup calls"},
    {17U, 164U, 8U, 2U, "integration",
     "function bodies verifier result and diagnostic identity"},
    {18U, 172U, 7U, 1U, "unit", "exact level and pipeline text"},
    {19U, 179U, 9U, 2U, "integration",
     "exit code artifact kind module verification and emitted file structure"},
}};

constexpr std::string_view kRequirement =
    "the controller cannot compile and invoke LlvmBackend with isolated SelfHost.IR/Sema producers, "
    "NativeSharp.LLVM.C, Windows x64 ABI state, filesystem outputs, and diagnostic capture";
}

// Materializes TSH-00121..00187 with canonical contract, layer, profile and oracle lineage. The
// excluded external LLVM binding and missing process boundary remain visible infrastructure work.
void registerSelfHostCodeGenLoweringEmissionModule(TestRegistry& registry)
{
    for (const PendingContract& contract : kLoweringEmissionContracts)
    {
        selfhost_codegen_support::addInfrastructureContract(registry, contract, kRequirement);
    }
}
}
