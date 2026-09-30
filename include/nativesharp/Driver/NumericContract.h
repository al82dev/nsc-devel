#pragma once

#include "nativesharp/LLVM/FloatingPointMode.h"

#include <filesystem>
#include <string>

namespace nativesharp::driver
{
std::string numericContractManifest(const llvm_backend::FloatingPointState& state);
std::string numericContractFingerprintHex(const llvm_backend::FloatingPointState& state);
bool writeNumericContract(
    const std::filesystem::path& destination,
    const llvm_backend::FloatingPointState& state,
    std::string& error);
bool verifyNumericContractFile(
    const std::filesystem::path& source,
    const llvm_backend::FloatingPointState& state,
    std::string& error);
}
