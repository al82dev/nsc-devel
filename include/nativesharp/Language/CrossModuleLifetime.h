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
// LKF-416: the part of a contract shared by every type of one producer module.
// LKF-416: la parte de un contrato que comparten todos los tipos de un módulo productor.
std::string moduleLifetimeFingerprint(const CrossModuleLifetimeContract& contract);
}
