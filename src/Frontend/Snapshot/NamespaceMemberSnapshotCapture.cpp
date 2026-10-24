#include "NamespaceMemberSnapshotCapture.h"

#include "nativesharp/Frontend/ProjectCompilation.h"

namespace nativesharp::frontend
{
bool captureNamespaceMemberSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error)
{
    const ProjectNamespaceFunctionModel* const functions = reference.namespaceFunctionModel();
    if (functions == nullptr || !reference.diagnostics().diagnostics().empty())
    {
        error = "reference has no clean namespace function model";
        return false;
    }
    return functions->writePrecompiledSnapshotNamespaceMembers(capture.index, payload, error);
}
}
