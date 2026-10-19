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
};
}
