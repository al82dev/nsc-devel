#pragma once

#include <cstdint>
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
}
