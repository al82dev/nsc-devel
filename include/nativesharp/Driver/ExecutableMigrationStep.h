#pragma once

#include "nativesharp/Driver/MigrationFinding.h"

#include <string>

namespace nativesharp::driver
{
struct ExecutableMigrationStep final
{
    MigrationFinding finding;
    std::string before;
    std::string after;
    bool automatic = false;
};
} // namespace nativesharp::driver
