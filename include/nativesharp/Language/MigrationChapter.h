#pragma once

#include <string>

namespace nativesharp::language
{
struct MigrationChapter final
{
    std::string identity;
    int order = 0;
    bool hasBeforeExample = false;
    bool hasAfterExample = false;
    bool identifiesManualDecisions = false;
};
}
