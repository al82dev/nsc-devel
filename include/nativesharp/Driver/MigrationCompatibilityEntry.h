#pragma once

#include <string>

namespace nativesharp::driver
{
struct MigrationCompatibilityEntry final
{
    std::string domain;
    std::string identity;
    std::string detail;
    std::string sourceClassification;
    std::string abiClassification;
    bool requiresConsumerRebuild = false;
};
} // namespace nativesharp::driver
