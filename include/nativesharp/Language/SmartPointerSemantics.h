#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace nativesharp::language
{
inline constexpr std::uint16_t kSmartPointerContractMajor = 1;
inline constexpr std::uint16_t kSmartPointerContractMinor = 0;

enum class SmartPointerFamily : std::uint8_t
{
    Unique,
    Rc,
    WeakRc,
    Arc,
    WeakArc
};

enum class SmartPointerThreadModel : std::uint8_t
{
    UniqueTransfer,
    LocalOnly,
    AtomicShared
};

struct SmartPointerFamilyContract final
{
    SmartPointerFamily family;
    std::string_view name;
    SmartPointerThreadModel threadModel;
    bool nullable;
    bool implicitCopy;
    bool explicitClone;
    bool weak;
};

inline constexpr std::array kSmartPointerFamilies = {
    SmartPointerFamilyContract{SmartPointerFamily::Unique, "Unique", SmartPointerThreadModel::UniqueTransfer,
        true, false, false, false},
    SmartPointerFamilyContract{SmartPointerFamily::Rc, "Rc", SmartPointerThreadModel::LocalOnly,
        true, false, true, false},
    SmartPointerFamilyContract{SmartPointerFamily::WeakRc, "WeakRc", SmartPointerThreadModel::LocalOnly,
        true, false, true, true},
    SmartPointerFamilyContract{SmartPointerFamily::Arc, "Arc", SmartPointerThreadModel::AtomicShared,
        true, false, true, false},
    SmartPointerFamilyContract{SmartPointerFamily::WeakArc, "WeakArc", SmartPointerThreadModel::AtomicShared,
        true, false, true, true},
};

enum class SmartPointerOperation : std::uint8_t
{
    Move,
    ImplicitCopy,
    Clone,
    Downgrade,
    Upgrade,
    Release,
    Reset,
    CrossThread
};

struct SmartPointerOperationContext final
{
    SmartPointerFamily family = SmartPointerFamily::Unique;
    SmartPointerOperation operation = SmartPointerOperation::Move;
    std::uint64_t strongCount = 1;
    std::uint64_t weakCount = 0;
    bool payloadSendSync = false;
    bool producerModulePinned = true;
};

const SmartPointerFamilyContract& smartPointerFamilyContract(SmartPointerFamily family) noexcept;
bool validateSmartPointerOperation(const SmartPointerOperationContext& context, std::string& error);

std::string smartPointerContractManifest();
std::string smartPointerContractFingerprintHex();
bool writeSmartPointerContract(const std::filesystem::path& destination, std::string& error);
bool verifySmartPointerContractFile(const std::filesystem::path& source, std::string& error);
}
