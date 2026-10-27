#include "MemberSnapshotCapture.h"

#include "nativesharp/Frontend/ProjectCompilation.h"

namespace nativesharp::frontend
{
bool captureMemberSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error)
{
    const ProjectMemberModel* const members = reference.memberModel();
    if (members == nullptr || !reference.diagnostics().diagnostics().empty())
    {
        error = "reference has no clean member model";
        return false;
    }
    return members->writePrecompiledSnapshotMembers(capture.index, payload, error);
}
}
