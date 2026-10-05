#include "nativesharp/Language/CrossModuleLifetime.h"

#include <iomanip>
#include <limits>
#include <sstream>

namespace nativesharp::language
{
namespace
{
std::uint64_t hash(std::string_view text) noexcept
{
    std::uint64_t value = 14695981039346656037ULL;
    for (const unsigned char byte : text) { value ^= byte; value *= 1099511628211ULL; }
    return value;
}

std::string suffix(std::string_view module, std::string_view type)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash(std::string(module) + "::" + std::string(type));
    return out.str();
}
}

CrossModuleLifetimeContract makeCrossModuleLifetimeContract(
    std::string_view producerModule,
    std::string_view typeIdentity,
    std::string_view allocatorIdentity,
    ModuleUnloadPolicy unloadPolicy)
{
    CrossModuleLifetimeContract result;
    result.producerModule = producerModule;
    result.typeIdentity = typeIdentity;
    result.allocatorIdentity = allocatorIdentity;
    result.unloadPolicy = unloadPolicy;
    const std::string id = suffix(producerModule, typeIdentity);
    const std::string version = "_v" + std::to_string(kCrossModuleLifetimeAbiMajor) + "_";
    result.createThunk = "__nsc_xmod" + version + "create_" + id;
    result.destroyThunk = "__nsc_xmod" + version + "destroy_" + id;
    result.deallocateThunk = "__nsc_xmod" + version + "deallocate_" + id;
    return result;
}

bool validateCrossModuleLifetimeContract(
    const CrossModuleLifetimeContract& contract,
    std::string& error)
{
    error.clear();
    if (contract.abiMajor != kCrossModuleLifetimeAbiMajor)
    { error = "cross-module lifetime ABI major is incompatible"; return false; }
    if (contract.producerModule.empty() || contract.typeIdentity.empty() ||
        contract.allocatorIdentity.empty())
    { error = "cross-module lifetime contract requires producer, type, and allocator identities"; return false; }
    if (contract.createThunk.empty() || contract.destroyThunk.empty() || contract.deallocateThunk.empty())
    { error = "cross-module lifetime contract requires create, destroy, and deallocate thunks"; return false; }
    return true;
}

std::string crossModuleLifetimeFingerprint(const CrossModuleLifetimeContract& contract)
{
    const std::string material = std::to_string(contract.abiMajor) + "." +
        std::to_string(contract.abiMinor) + "|" + contract.producerModule + "|" +
        contract.typeIdentity + "|" + contract.allocatorIdentity + "|" +
        contract.createThunk + "|" + contract.destroyThunk + "|" + contract.deallocateThunk +
        "|" + std::to_string(static_cast<unsigned>(contract.unloadPolicy));
    return suffix("contract", material);
}

bool ModuleLifetimeRegistry::registerObject(
    const CrossModuleLifetimeContract& contract,
    std::string& error)
{
    if (!validateCrossModuleLifetimeContract(contract, error)) return false;
    std::scoped_lock lock(mutex_);
    ModuleState& state = modules_[contract.producerModule];
    const std::string fingerprint = crossModuleLifetimeFingerprint(contract);
    if (state.contractFingerprint.empty()) state.contractFingerprint = fingerprint;
    if (state.contractFingerprint != fingerprint)
    { error = "producer module attempted to mix incompatible allocator/destruction contracts"; return false; }
    if (state.unloading)
    { error = "producer module is unloading and cannot create another cross-module object"; return false; }
    if (state.liveObjects == std::numeric_limits<std::size_t>::max())
    { error = "producer module live-object counter overflow"; return false; }
    ++state.liveObjects;
    return true;
}

bool ModuleLifetimeRegistry::unregisterObject(
    const CrossModuleLifetimeContract& contract,
    std::string& error)
{
    if (!validateCrossModuleLifetimeContract(contract, error)) return false;
    std::scoped_lock lock(mutex_);
    const auto found = modules_.find(contract.producerModule);
    if (found == modules_.end() || found->second.liveObjects == 0)
    { error = "cross-module object release has no matching live registration"; return false; }
    if (found->second.contractFingerprint != crossModuleLifetimeFingerprint(contract))
    { error = "cross-module object must be destroyed and deallocated by its producer contract"; return false; }
    --found->second.liveObjects;
    return true;
}

bool ModuleLifetimeRegistry::requestUnload(std::string_view producerModule, std::string& error)
{
    std::scoped_lock lock(mutex_);
    ModuleState& state = modules_[std::string(producerModule)];
    if (state.liveObjects != 0)
    {
        error = "module unload rejected: " + std::to_string(state.liveObjects) +
            " cross-module object(s) still require producer destroy/deallocate thunks";
        return false;
    }
    state.unloading = true;
    error.clear();
    return true;
}

std::size_t ModuleLifetimeRegistry::liveObjects(std::string_view producerModule) const noexcept
{
    std::scoped_lock lock(mutex_);
    const auto found = modules_.find(std::string(producerModule));
    return found == modules_.end() ? 0U : found->second.liveObjects;
}
}
