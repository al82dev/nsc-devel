#pragma once

#include "nativesharp/Language/CrossModuleLifetimeContract.h"
#include "nativesharp/Language/ModuleLifetimeRegistry.h"
#include "nativesharp/Language/ModuleUnloadPolicy.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace nativesharp::language
{
CrossModuleLifetimeContract makeCrossModuleLifetimeContract(
    std::string_view producerModule,
    std::string_view typeIdentity,
    std::string_view allocatorIdentity,
    ModuleUnloadPolicy unloadPolicy);
bool validateCrossModuleLifetimeContract(
    const CrossModuleLifetimeContract& contract,
    std::string& error);
std::string crossModuleLifetimeFingerprint(const CrossModuleLifetimeContract& contract);
}
