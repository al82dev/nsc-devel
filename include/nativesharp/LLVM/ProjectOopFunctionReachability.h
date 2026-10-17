#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace nativesharp::frontend
{
class ProjectOopMirModel;
}

namespace nativesharp::llvm_backend
{
// Returns one byte per MIR function. A set byte means that the current consumer must emit the
// definition. Source-owned functions are roots; metadata-transported inline bodies enter only
// through the iterative call/member closure of those roots.
std::vector<std::uint8_t> projectOopReachableDefinitionMask(
    const frontend::ProjectOopMirModel& mirModel);

// NSC305-L06F. Selects only static-field declarations required by one physical LLVM module.
// A monolithic module retains the complete model. The partition coordinator owns every local
// field and the dependencies of local initializers; ordinary units retain fields referenced by
// the MIR bodies they define.
std::vector<std::uint8_t> projectOopStaticFieldDeclarationMask(
    const frontend::ProjectOopMirModel& mirModel,
    std::span<const std::size_t> definitionOrdinals,
    bool partitioned,
    bool ownsModuleGlobals);

// NSC305-L06F. The coordinator defines globals but no ordinary MIR bodies. It needs declarations
// for local executable symbols used by vtables/wrappers/guards plus callees referenced by local
// field initializers, not the complete imported API catalogue.
std::vector<std::size_t> projectOopCoordinatorFunctionDeclarations(
    const frontend::ProjectOopMirModel& mirModel);
}
