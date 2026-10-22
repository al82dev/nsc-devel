#include "Internal/CompilationInputManifestFactory.h"
#include "Internal/DriverInternal.h"

namespace nativesharp::driver
{
namespace
{
void addCommonCompilationIdentity(
    compilation::CompilationInputManifest& manifest)
{
    const target::TargetInfo targetInfo = target::TargetInfo::WindowsX64();
    manifest.addIdentityField(
        "codegen", std::string(llvm_backend::kCodegenCacheIdentity));
    manifest.addIdentityField("target", std::string(targetInfo.llvmTriple()));
    manifest.addIdentityField("target-abi", std::string(targetInfo.abiName()));
    manifest.addIdentityField(
        "runtime-abi", runtime::runtimeAbiContractFingerprintHex());
    manifest.addIdentityField(
        "floating-point-mode",
        std::string(llvm_backend::floatingPointModeName(
            llvm_backend::activeFloatingPointMode())));
    manifest.addIdentityField(
        "fma-contract",
        std::string(llvm_backend::fmaContractPolicyName(
            llvm_backend::activeFmaContractPolicy())));
    manifest.addIdentityField(
        "floating-point-rounding",
        std::string(llvm_backend::floatingPointRoundingModeName(
            llvm_backend::activeFloatingPointRoundingMode())));
    manifest.addIdentityField(
        "floating-point-denormal",
        std::string(llvm_backend::floatingPointDenormalModeName(
            llvm_backend::activeFloatingPointDenormalMode())));
}
}

compilation::CompilationInputManifest createCompilationInputManifest(
    compilation::ProjectGraph graph,
    compilation::ProjectLoadOptions loadOptions)
{
    compilation::CompilationInputManifest manifest(
        std::move(graph), std::move(loadOptions));
    addCommonCompilationIdentity(manifest);
    return manifest;
}
}
