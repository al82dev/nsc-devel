#include "nativesharp/Driver/NumericContract.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Language/NumericSemantics.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>

namespace nativesharp::driver
{
namespace
{
std::string numericContractPayload(const llvm_backend::FloatingPointState& state)
{
    const bool fast = state.mode == llvm_backend::FloatingPointMode::Fast;
    const bool deterministic = state.mode == llvm_backend::FloatingPointMode::Deterministic;
    std::string result = "NSNUMERIC 1.0\n";
    result += "target=windows-x64\n";
    result += "integer-representation=twos-complement\n";
    result += "integer-default-overflow=wrapping\n";
    result += "integer-llvm-no-wrap-flags=forbidden\n";
    result += "integer-self-check=";
    result += language::integerSemanticsSelfCheck() ? "pass\n" : "fail\n";
    result += "fp-mode=" + std::string(llvm_backend::floatingPointModeName(state.mode)) + "\n";
    result += "fp-contract=" +
        std::string(llvm_backend::fmaContractPolicyName(state.fmaContractPolicy)) + "\n";
    result += "fp-rounding=" +
        std::string(llvm_backend::floatingPointRoundingModeName(state.roundingMode)) + "\n";
    result += "fp-denormal=" +
        std::string(llvm_backend::floatingPointDenormalModeName(state.denormalMode)) + "\n";
    result += std::string("fp-reassociation=") + (fast ? "allowed\n" : "forbidden\n");
    result += std::string("fp-nan-and-signed-zero=") +
        (fast ? "not-preserved\n" : "preserved\n");
    result += std::string("fp-deterministic-profile=") +
        (deterministic ? "enabled\n" : "disabled\n");
    result += "linear-algebra-e003-e007=external-certification-required\n";
    return result;
}
}

std::string numericContractFingerprintHex(const llvm_backend::FloatingPointState& state)
{
    const std::string payload = numericContractPayload(state);
    const auto bytes = std::span(
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size());
    return basic::sha256Hex(basic::sha256(bytes));
}

std::string numericContractManifest(const llvm_backend::FloatingPointState& state)
{
    return numericContractPayload(state) +
        "fingerprint=" + numericContractFingerprintHex(state) + "\n";
}

bool writeNumericContract(
    const std::filesystem::path& destination,
    const llvm_backend::FloatingPointState& state,
    std::string& error)
{
    error.clear();
    std::error_code directoryError;
    if (!destination.parent_path().empty())
        std::filesystem::create_directories(destination.parent_path(), directoryError);
    if (directoryError)
    {
        error = "cannot create numeric contract directory '" +
            destination.parent_path().string() + "': " + directoryError.message();
        return false;
    }
    const std::string manifest = numericContractManifest(state);
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    if (!stream || !stream.write(manifest.data(), static_cast<std::streamsize>(manifest.size())))
    {
        error = "cannot write numeric contract '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifyNumericContractFile(
    const std::filesystem::path& source,
    const llvm_backend::FloatingPointState& state,
    std::string& error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open numeric contract '" + source.string() + "'";
        return false;
    }
    std::string actual{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    actual.erase(std::remove(actual.begin(), actual.end(), '\r'), actual.end());
    if (actual != numericContractManifest(state))
    {
        error = "incompatible numeric contract '" + source.string() +
            "': expected fingerprint " + numericContractFingerprintHex(state);
        return false;
    }
    return true;
}
}
