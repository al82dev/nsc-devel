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

namespace nativesharp::llvm_backend
{
class ProjectOopMirCacheIdentityIndex final
{
public:
    explicit ProjectOopMirCacheIdentityIndex(
        const frontend::ProjectOopMirModel& model);

    std::string identity(
        std::span<const std::size_t> definitionOrdinals,
        std::span<const std::size_t> declarationOrdinals) const;

private:
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
