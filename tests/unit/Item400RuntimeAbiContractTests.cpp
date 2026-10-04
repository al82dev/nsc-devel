#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/Runtime/RuntimeAbiContract.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

int main()
{
    using namespace nativesharp::runtime;
    using nativesharp::llvm_backend::LlvmModule;
    using nativesharp::tests::TestContext;

    TestContext tests;
    tests.expect(kRuntimeAbiMajor == 1 && kRuntimeAbiMinor == 2,
        "runtime ABI has one authoritative semantic version");
    tests.expect(kRuntimeSymbolSetVersion == 4 && kRuntimeAbiSymbols.size() == 99,
        "runtime symbol set 4 freezes all 99 NativeSharp-prefixed exports");
    tests.expect(std::is_sorted(kRuntimeAbiSymbols.begin(), kRuntimeAbiSymbols.end(),
                     [](const RuntimeAbiSymbol& left, const RuntimeAbiSymbol& right) {
                         return left.name < right.name;
                     }),
        "runtime ABI symbols have deterministic canonical order");

    const std::string manifest = runtimeAbiContractManifest();
    tests.expect(manifest.starts_with("NSRUNTIMEABI 1.2\nsymbol_set=4\n"),
        "runtime ABI manifest carries independent ABI and symbol-set versions");
    tests.expect(manifest.find("target=x86_64-pc-windows-msvc\n") != std::string::npos &&
                     manifest.find("calling_convention=win64-c\n") != std::string::npos,
        "runtime ABI manifest freezes target and calling convention");
    tests.expect(manifest.find("function __nativesharp_alloc ptr(i64)\n") != std::string::npos &&
                     manifest.find("function __nativesharp_console_readline ptr(ptr)\n") !=
                         std::string::npos &&
                     manifest.find("data __nativesharp_exception_tls_index i32\n") != std::string::npos,
        "runtime ABI manifest distinguishes exact function and data types");
    tests.expect(runtimeAbiContractFingerprintHex() == "2cddeff864c7dfc6",
        "runtime ABI contract fingerprint changes only with reviewed contract evidence");

    const std::filesystem::path evidence =
        nativesharp::tests::evidenceRoot("nativesharp_item400_runtime_abi_contract");
    std::error_code filesystemError;
    std::filesystem::remove_all(evidence, filesystemError);
    filesystemError.clear();
    const std::filesystem::path artifact = evidence / "nested/NativeSharpRuntime.abi";
    std::string error;
    tests.expect(writeRuntimeAbiContract(artifact, error) && error.empty(),
        "runtime ABI writer creates the canonical distributable artifact");
    tests.expect(verifyRuntimeAbiContractFile(artifact, error) && error.empty(),
        "runtime ABI artifact verifies against the compiler's exact contract");
    {
        std::ofstream altered(artifact, std::ios::binary | std::ios::app);
        altered << "function __nativesharp_future void()\n";
    }
    tests.expect(!verifyRuntimeAbiContractFile(artifact, error) &&
                         error.find("expected fingerprint 2cddeff864c7dfc6") != std::string::npos,
        "runtime ABI artifact rejects unversioned contract drift");

    WindowsRuntimeBuilder builder;
    std::unique_ptr<LlvmModule> executable = builder.build(error);
    tests.expect(executable != nullptr && error.empty(),
        "executable runtime is constructed only after formal ABI verification");
    if (executable != nullptr)
    {
        const std::string text = executable->text();
        tests.expect(verifyRuntimeAbiContract(text, RuntimeAbiImageKind::Executable, error) && error.empty(),
            "formal verifier accepts the complete executable runtime");

        std::string wrongType = text;
        const std::string original = "define ptr @__nativesharp_alloc(i64";
        const std::size_t typePosition = wrongType.find(original);
        if (typePosition != std::string::npos)
        {
            wrongType.replace(typePosition, original.size(), "define i64 @__nativesharp_alloc(i64");
        }
        tests.expect(!verifyRuntimeAbiContract(
                         wrongType, RuntimeAbiImageKind::Executable, error) &&
                         error.find("type mismatch") != std::string::npos &&
                         error.find("__nativesharp_alloc") != std::string::npos,
            "formal verifier rejects a signature drift with the symbol named");

        std::string unexpected = text;
        unexpected += "\ndefine void @__nativesharp_uncontracted() { ret void }\n";
        tests.expect(!verifyRuntimeAbiContract(
                         unexpected, RuntimeAbiImageKind::Executable, error) &&
                         error.find("uncontracted runtime ABI symbol") != std::string::npos,
            "formal verifier rejects exports absent from the versioned contract");
    }

    std::unique_ptr<LlvmModule> support = builder.buildSupport(error);
    tests.expect(support != nullptr && error.empty(),
        "support-library runtime is constructed through its explicit ABI image kind");
    if (support != nullptr)
    {
        tests.expect(verifyRuntimeAbiContract(
                         support->text(), RuntimeAbiImageKind::SupportLibrary, error) && error.empty(),
            "support-library contract permits only the separately supplied entry point to be absent");
    }

    return tests.result();
}
