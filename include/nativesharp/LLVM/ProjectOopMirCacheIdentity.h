#pragma once

#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
class ProjectOopMirModel;
}

namespace nativesharp::source
{
class SourceManager;
}

namespace nativesharp::driver
{
class CompilationWorkerPool;
}

namespace nativesharp::llvm_backend
{
class ProjectOopMirCacheIdentityIndex final
{
public:
    explicit ProjectOopMirCacheIdentityIndex(
        const frontend::ProjectOopMirModel& model);

    // EN: Same digests as the single-argument constructor; per-function digests are computed in
    //     independent slots on the pool when it is given (null means sequential).
    // ES: Mismos digests que el constructor de un argumento; los digests por función se calculan
    //     en ranuras independientes en el pool cuando se da (nulo significa secuencial).
    ProjectOopMirCacheIdentityIndex(
        const frontend::ProjectOopMirModel& model,
        driver::CompilationWorkerPool* workerPool);

    std::string identity(
        std::span<const std::size_t> definitionOrdinals,
        std::span<const std::size_t> declarationOrdinals) const;

private:
    void computeFunctionDigests(
        const frontend::ProjectOopMirModel& model, std::size_t begin, std::size_t end);


    std::string commonDigest_;
    std::vector<std::string> declarationDigests_;
    std::vector<std::string> definitionDigests_;
};

std::string projectOopMirCacheIdentity(
    const frontend::ProjectOopMirModel& model,
    std::span<const std::size_t> definitionOrdinals,
    std::span<const std::size_t> declarationOrdinals);

std::string projectOopLineTableSourceIdentity(
    const source::SourceManager& sourceManager,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const SourceRange> ranges);
}
