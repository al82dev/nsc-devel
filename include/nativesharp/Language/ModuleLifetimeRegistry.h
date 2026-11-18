#pragma once

#include "nativesharp/Language/CrossModuleLifetimeContract.h"
#include "nativesharp/Language/ModuleLifetimeState.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace nativesharp::language
{
class ModuleLifetimeRegistry final
{
public:
    bool registerObject(const CrossModuleLifetimeContract& contract, std::string& error);
    bool unregisterObject(const CrossModuleLifetimeContract& contract, std::string& error);
    bool requestUnload(std::string_view producerModule, std::string& error);
    std::size_t liveObjects(std::string_view producerModule) const noexcept;

private:
    using ModuleState = ModuleLifetimeState;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ModuleState> modules_;
    // EN: TL-LANG-085: live objects per complete type contract (crossModuleLifetimeFingerprint), so a release
    //     must name the contract that registered the object, not only its module.
    // ES: TL-LANG-085: objetos vivos por contrato completo de tipo (crossModuleLifetimeFingerprint); una
    //     liberación debe nombrar el contrato que registró el objeto, no sólo su módulo.
    std::unordered_map<std::string, std::size_t> typeContracts_;
};
}
