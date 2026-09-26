#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace nativesharp::frontend
{
class ProjectOopMirModel;
}

namespace nativesharp::llvm_backend
{
std::string projectOopMirCacheIdentity(
    const frontend::ProjectOopMirModel& model,
    std::span<const std::size_t> definitionOrdinals,
    std::span<const std::size_t> declarationOrdinals);
}
