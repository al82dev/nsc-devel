#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct PromotionEvidence final
{
    bool externalBaselinePassed = false;
    std::size_t priorityZeroIssues = 0;
    std::size_t unwaivedPriorityOneIssues = 0;
    bool debugConformancePassed = false;
    bool releaseConformancePassed = false;
    bool bilingualDocsInParity = false;
    bool migrationValidated = false;
    bool artifactsReproducible = false;
    bool hashesPublished = false;
};
}
