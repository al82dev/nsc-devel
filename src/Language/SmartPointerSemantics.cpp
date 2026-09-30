#include "nativesharp/Language/SmartPointerSemantics.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>

namespace nativesharp::language
{
namespace
{
const char* threadModelName(SmartPointerThreadModel model) noexcept
{
    switch (model)
    {
        case SmartPointerThreadModel::UniqueTransfer: return "unique-transfer-if-payload-send";
        case SmartPointerThreadModel::LocalOnly: return "local-only";
        case SmartPointerThreadModel::AtomicShared: return "atomic-shared-if-payload-send-sync";
    }
    return "unknown";
}

bool isStrongShared(SmartPointerFamily family) noexcept
{
    return family == SmartPointerFamily::Rc || family == SmartPointerFamily::Arc;
}

bool isWeak(SmartPointerFamily family) noexcept
{
    return family == SmartPointerFamily::WeakRc || family == SmartPointerFamily::WeakArc;
}

bool countCanIncrement(std::uint64_t count) noexcept
{
    return count != std::numeric_limits<std::uint64_t>::max();
}
}

const SmartPointerFamilyContract& smartPointerFamilyContract(SmartPointerFamily family) noexcept
{
    return kSmartPointerFamilies[static_cast<std::size_t>(family)];
}

bool validateSmartPointerOperation(
    const SmartPointerOperationContext& context,
    std::string& error)
{
    error.clear();
    const SmartPointerFamilyContract& family = smartPointerFamilyContract(context.family);
    switch (context.operation)
    {
        case SmartPointerOperation::Move:
            return true;
        case SmartPointerOperation::ImplicitCopy:
            error = std::string(family.name) + " is never implicitly copyable; use Move or explicit Clone";
            return false;
        case SmartPointerOperation::Clone:
            if (!family.explicitClone)
            {
                error = std::string(family.name) + " does not support Clone";
                return false;
            }
            if (!countCanIncrement(family.weak ? context.weakCount : context.strongCount))
            {
                error = std::string(family.name) + " reference count overflow";
                return false;
            }
            return true;
        case SmartPointerOperation::Downgrade:
            if (!isStrongShared(context.family))
            {
                error = "Downgrade requires Rc or Arc";
                return false;
            }
            if (!countCanIncrement(context.weakCount))
            {
                error = std::string(family.name) + " weak reference count overflow";
                return false;
            }
            return true;
        case SmartPointerOperation::Upgrade:
            if (!isWeak(context.family))
            {
                error = "Upgrade requires WeakRc or WeakArc";
                return false;
            }
            if (context.strongCount == 0)
            {
                error = std::string(family.name) + " is expired; Upgrade cannot resurrect the payload";
                return false;
            }
            if (!countCanIncrement(context.strongCount))
            {
                error = std::string(family.name) + " strong reference count overflow";
                return false;
            }
            return true;
        case SmartPointerOperation::Release:
        case SmartPointerOperation::Reset:
            if (context.family != SmartPointerFamily::Unique)
            {
                error = std::string(family.name) + " does not expose Unique ownership transfer operations";
                return false;
            }
            return true;
        case SmartPointerOperation::CrossThread:
            if (family.threadModel == SmartPointerThreadModel::LocalOnly)
            {
                error = std::string(family.name) + " is local-only; use Arc/WeakArc for shared cross-thread ownership";
                return false;
            }
            if (!context.payloadSendSync)
            {
                error = std::string(family.name) + " cannot cross threads because the payload is not Send/Sync";
                return false;
            }
            if (!context.producerModulePinned)
            {
                error = std::string(family.name) + " cannot outlive its producer module destroy/deallocate thunks";
                return false;
            }
            return true;
    }
    error = "unknown smart pointer operation";
    return false;
}

std::string smartPointerContractManifest()
{
    std::string result = "NSSMARTPTR " + std::to_string(kSmartPointerContractMajor) + "." +
        std::to_string(kSmartPointerContractMinor) + "\n";
    result += "target=x86_64-pc-windows-msvc\n";
    result += "ownership_basis=owned-T\n";
    result += "null_access=checked-failure\n";
    result += "count_overflow=reject-before-wrap\n";
    result += "weak_upgrade=no-resurrection\n";
    result += "cycle_policy=explicit-weak-no-implicit-gc\n";
    result += "control_block=strong+weak+allocator+destroy+deallocate+module-provenance\n";
    result += "module_unload=forbidden-while-control-block-live\n";
    result += "allocation_policy=unique-one;shared-single-allocation-factory\n";
    for (const SmartPointerFamilyContract& family : kSmartPointerFamilies)
    {
        result += "family ";
        result += family.name;
        result += " nullable=";
        result += family.nullable ? "yes" : "no";
        result += " implicit-copy=";
        result += family.implicitCopy ? "yes" : "no";
        result += " explicit-clone=";
        result += family.explicitClone ? "yes" : "no";
        result += " weak=";
        result += family.weak ? "yes" : "no";
        result += " thread=";
        result += threadModelName(family.threadModel);
        result.push_back('\n');
    }
    result += "api Unique=MakeUnique,Get,Borrow,Release,Reset\n";
    result += "api Rc=MakeRc,Get,Borrow,Clone,Downgrade,StrongCount,WeakCount\n";
    result += "api WeakRc=Clone,Upgrade,Expired,WeakCount\n";
    result += "api Arc=MakeArc,Get,Borrow,Clone,Downgrade,StrongCount,WeakCount\n";
    result += "api WeakArc=Clone,Upgrade,Expired,WeakCount\n";
    return result;
}

std::string smartPointerContractFingerprintHex()
{
    constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t value = offsetBasis;
    for (const char byte : smartPointerContractManifest())
    {
        value ^= static_cast<unsigned char>(byte);
        value *= prime;
    }
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << value;
    return stream.str();
}

bool writeSmartPointerContract(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "smart pointer contract destination is empty";
        return false;
    }
    const std::filesystem::path parent = destination.parent_path();
    if (!parent.empty())
    {
        std::error_code filesystemError;
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError)
        {
            error = "cannot create smart pointer contract directory '" + parent.string() +
                "': " + filesystemError.message();
            return false;
        }
    }
    const std::string manifest = smartPointerContractManifest();
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    if (!stream || !stream.write(manifest.data(), static_cast<std::streamsize>(manifest.size())))
    {
        error = "cannot write smart pointer contract '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifySmartPointerContractFile(const std::filesystem::path& source, std::string& error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open smart pointer contract '" + source.string() + "'";
        return false;
    }
    std::string actual{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    actual.erase(std::remove(actual.begin(), actual.end(), '\r'), actual.end());
    if (actual != smartPointerContractManifest())
    {
        error = "incompatible smart pointer contract '" + source.string() +
            "': expected fingerprint " + smartPointerContractFingerprintHex();
        return false;
    }
    return true;
}
}
