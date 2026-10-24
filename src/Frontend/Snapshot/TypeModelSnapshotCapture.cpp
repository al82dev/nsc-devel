#include "TypeModelSnapshotCapture.h"

#include "nativesharp/Frontend/ProjectCompilation.h"

namespace nativesharp::frontend
{
namespace
{
bool cleanReference(const ProjectCompilation& reference, const void* model, std::string& error)
{
    if (model != nullptr && reference.diagnostics().diagnostics().empty()) return true;
    error = "reference has no clean model";
    return false;
}
}

bool captureInheritanceSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error)
{
    static_cast<void>(capture);
    const ProjectInheritanceModel* const inheritance = reference.inheritanceModel();
    if (!cleanReference(reference, inheritance, error)) return false;
    return inheritance->writePrecompiledSnapshotInheritance(payload, error);
}

bool captureInterfaceSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error)
{
    static_cast<void>(capture);
    const ProjectInterfaceModel* const interfaces = reference.interfaceModel();
    if (!cleanReference(reference, interfaces, error)) return false;
    return interfaces->writePrecompiledSnapshotInterfaces(payload, error);
}
}
