#pragma once

#include "nativesharp/Language/MigrationIssue.h"

#include <string>

namespace nativesharp::language
{
struct MigrationFix final
{
    MigrationIssue issue;
    std::string replacement;
    bool meaningProvenEquivalent = false;
};
}
