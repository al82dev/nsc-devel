#include "PrecompiledSnapshotValidation.h"

namespace nativesharp::frontend
{
namespace
{
// EN: The loaded digest of `moduleName`, or nullptr when the consumer does not load that module.
// ES: El digest cargado de `moduleName`, o nullptr si el consumidor no carga ese módulo.
const std::string* loadedDigest(
    std::span<const compilation::PrecompiledSnapshotDependency> loadedDependencies,
    const std::string& moduleName)
{
    for (const compilation::PrecompiledSnapshotDependency& loaded : loadedDependencies)
    {
        if (loaded.moduleName == moduleName) return &loaded.metadataDigest;
    }
    return nullptr;
}
}

bool validatePrecompiledSnapshot(
    const compilation::PrecompiledSemanticSnapshot& snapshot,
    std::span<const compilation::PrecompiledSnapshotDependency> loadedDependencies,
    const PrecompiledSyntaxNodeIndex& apiIndex,
    std::string& reason)
{
    for (const compilation::PrecompiledSnapshotDependency& dependency : snapshot.dependencies)
    {
        const std::string* digest = loadedDigest(loadedDependencies, dependency.moduleName);
        if (digest == nullptr)
        {
            reason = "dependency-not-loaded:" + dependency.moduleName;
            return false;
        }
        if (*digest != dependency.metadataDigest)
        {
            reason = "dependency-digest:" + dependency.moduleName;
            return false;
        }
    }
    if (snapshot.nodeCount != apiIndex.nodes().size() || snapshot.typeCount != apiIndex.types().size())
    {
        reason = "node-count";
        return false;
    }
    if (snapshot.nodeKindDigest != apiIndex.kindDigest())
    {
        reason = "node-kinds";
        return false;
    }
    reason.clear();
    return true;
}
}
