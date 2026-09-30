#include "nativesharp/Driver/NumericContract.h"

#include "../support/TestContext.h"

#include <filesystem>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp;

    const llvm_backend::FloatingPointState precise;
    llvm_backend::FloatingPointState fast;
    fast.mode = llvm_backend::FloatingPointMode::Fast;
    fast.fmaContractPolicy = llvm_backend::FmaContractPolicy::Fast;
    llvm_backend::FloatingPointState deterministic;
    deterministic.mode = llvm_backend::FloatingPointMode::Deterministic;
    deterministic.roundingMode = llvm_backend::FloatingPointRoundingMode::NearestEven;

    const std::string preciseManifest = driver::numericContractManifest(precise);
    const std::string fastManifest = driver::numericContractManifest(fast);
    tests.expect(preciseManifest.find("NSNUMERIC 1.0") != std::string::npos &&
            preciseManifest.find("integer-self-check=pass") != std::string::npos,
        "numeric contract is versioned and guarded by independent integer semantics checks");
    tests.expect(preciseManifest.find("fp-mode=precise") != std::string::npos &&
            preciseManifest.find("fp-reassociation=forbidden") != std::string::npos,
        "precise contract preserves the productive default floating-point policy");
    tests.expect(fastManifest.find("fp-mode=fast") != std::string::npos &&
            fastManifest.find("fp-contract=fast") != std::string::npos &&
            fastManifest.find("fp-reassociation=allowed") != std::string::npos,
        "fast contract exposes every relaxed floating-point decision");
    tests.expect(driver::numericContractManifest(deterministic).find(
            "fp-deterministic-profile=enabled") != std::string::npos,
        "deterministic mode is explicit rather than inferred from optimization level");
    tests.expect(driver::numericContractFingerprintHex(precise) !=
            driver::numericContractFingerprintHex(fast),
        "floating-point policy changes the numeric contract fingerprint");
    tests.expect(preciseManifest.find(
            "linear-algebra-e003-e007=external-certification-required") != std::string::npos,
        "compiler contract does not claim unexecuted LinearAlgebra certification");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "nsc_ns3_t14_numeric_contract";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::string error;
    const std::filesystem::path contract = root / "NativeSharpNumeric.contract";
    tests.expect(driver::writeNumericContract(contract, precise, error),
        "numeric contract writes into an explicit A: destination");
    tests.expect(driver::verifyNumericContractFile(contract, precise, error),
        "numeric contract round-trip verifies for the same compiler policy");
    tests.expect(!driver::verifyNumericContractFile(contract, fast, error) &&
            error.find("expected fingerprint") != std::string::npos,
        "numeric contract rejects a mismatched floating-point policy causally");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
