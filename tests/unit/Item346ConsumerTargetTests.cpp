#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/LLVM/LlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Sema/SemanticAnalyzer.h"
#include "nativesharp/Target/TargetDataLayout.h"
#include "nativesharp/Target/TargetInfo.h"

#include "../support/TestContext.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

int main()
{
    using nativesharp::frontend::FrontendCompilation;
    using nativesharp::llvm_backend::LlvmCodeGenFileType;
    using nativesharp::llvm_backend::LlvmLowerer;
    using nativesharp::llvm_backend::LlvmModule;
    using nativesharp::llvm_backend::LlvmTargetMachine;
    using nativesharp::target::BuiltinLayoutKind;
    using nativesharp::target::TargetDataLayout;
    using nativesharp::target::TargetInfo;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const TargetInfo target = TargetInfo::WindowsArm64();
    tests.expect(target.architectureName() == "arm64" &&
            target.llvmTriple() == "aarch64-pc-windows-msvc" &&
            target.abiName() == "Microsoft Windows ARM64 ABI" &&
            target.cpuBaselineName() == "armv8-a",
        "consumer-selected Windows ARM64 target publishes a complete identity");
    tests.expect(target.supportsCpuFeature("neon") && !target.supportsCpuFeature("avx") &&
            target.isAssemblyRegister("x30") && target.isAssemblyRegister("v31") &&
            !target.isAssemblyRegister("rax"),
        "ARM64 target has an architecture-specific baseline and register catalog");

    const TargetDataLayout layout = TargetDataLayout::WindowsArm64();
    tests.expect(layout.layoutOf(BuiltinLayoutKind::Pointer).size == 8U &&
            layout.layoutOf(BuiltinLayoutKind::Long).size == 8U &&
            layout.layoutOf(BuiltinLayoutKind::Int).size == 4U && layout.isLittleEndian(),
        "Windows ARM64 target preserves the LLP64 NativeSharp data model");

    std::string targetError;
    std::unique_ptr<LlvmTargetMachine> targetMachine =
        LlvmTargetMachine::createWindowsArm64(targetError);
    tests.expect(targetMachine != nullptr && targetError.empty() &&
            targetMachine->triple() == "aarch64-pc-windows-msvc",
        "LLVM provides the requested Windows ARM64 target machine (" + targetError + ")");

    FrontendCompilation compilation;
    const bool compiled = compilation.compileBuffer(
        std::filesystem::path("item346_portable_consumer.ns"),
        "int Add(int a, int b) { return a + b; } int Main() { return Add(20, 22); }",
        false);
    tests.expect(compiled && compilation.semanticAnalyzer() != nullptr &&
            compilation.optimizationModule() != nullptr,
        "portable consumer fixture reaches target-independent optimized MIR");

    std::unique_ptr<LlvmModule> module;
    std::unique_ptr<LlvmLowerer> lowerer;
    if (compiled && compilation.semanticAnalyzer() != nullptr &&
        compilation.optimizationModule() != nullptr)
    {
        lowerer = std::make_unique<LlvmLowerer>(
            compilation.semanticAnalyzer()->globalSymbols());
        module = lowerer->lower(compilation.optimizationModule());
    }
    tests.expect(module != nullptr, "portable consumer fixture lowers to LLVM");

    const std::filesystem::path objectPath =
        std::filesystem::temp_directory_path() / "nativesharp_item346_arm64.obj";
    std::error_code filesystemError;
    std::filesystem::remove(objectPath, filesystemError);
    std::string emissionError;
    const bool emitted = targetMachine != nullptr && module != nullptr &&
        targetMachine->emit(*module, objectPath, LlvmCodeGenFileType::Object, emissionError);
    tests.expect(emitted, "consumer fixture emits Windows ARM64 COFF (" + emissionError + ")");

    std::uint16_t machine = 0U;
    if (emitted)
    {
        std::ifstream stream(objectPath, std::ios::binary);
        unsigned char bytes[2] = {};
        stream.read(reinterpret_cast<char*>(bytes), 2);
        machine = static_cast<std::uint16_t>(bytes[0]) |
            static_cast<std::uint16_t>(bytes[1] << 8U);
    }
    tests.expect(machine == 0xaa64U,
        "physical object declares IMAGE_FILE_MACHINE_ARM64 rather than nominal metadata");

    std::filesystem::remove(objectPath, filesystemError);
    return tests.result();
}
