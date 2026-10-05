#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace nativesharp::language
{
inline constexpr std::uint16_t kCrossModuleLifetimeAbiMajor = 1;
inline constexpr std::uint16_t kCrossModuleLifetimeAbiMinor = 0;

enum class ModuleUnloadPolicy : std::uint8_t
{
    RejectWhileObjectsLive,
    PinUntilObjectsDestroyed
};

struct CrossModuleLifetimeContract final
{
    std::uint16_t abiMajor = kCrossModuleLifetimeAbiMajor;
    std::uint16_t abiMinor = kCrossModuleLifetimeAbiMinor;
    std::string producerModule;
    std::string typeIdentity;
    std::string allocatorIdentity;
    std::string createThunk;
    std::string destroyThunk;
    std::string deallocateThunk;
    ModuleUnloadPolicy unloadPolicy = ModuleUnloadPolicy::RejectWhileObjectsLive;
};

CrossModuleLifetimeContract makeCrossModuleLifetimeContract(
    std::string_view producerModule,
    std::string_view typeIdentity,
    std::string_view allocatorIdentity,
    ModuleUnloadPolicy unloadPolicy);
bool validateCrossModuleLifetimeContract(
    const CrossModuleLifetimeContract& contract,
    std::string& error);
std::string crossModuleLifetimeFingerprint(const CrossModuleLifetimeContract& contract);

class ModuleLifetimeRegistry final
{
public:
    bool registerObject(const CrossModuleLifetimeContract& contract, std::string& error);
    bool unregisterObject(const CrossModuleLifetimeContract& contract, std::string& error);
    bool requestUnload(std::string_view producerModule, std::string& error);
    std::size_t liveObjects(std::string_view producerModule) const noexcept;

private:
    struct ModuleState final
    {
        std::string contractFingerprint;
        std::size_t liveObjects = 0;
        bool unloading = false;
    };
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ModuleState> modules_;
};
}
