#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Sema/SemanticAnalyzer.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::llvm_backend::CoffObjectVerifier;
using nativesharp::llvm_backend::LlvmCodeGenFileType;
using nativesharp::llvm_backend::LlvmLowerer;
using nativesharp::llvm_backend::LlvmModule;
using nativesharp::llvm_backend::LlvmTargetMachine;

std::unique_ptr<LlvmModule> compileAndLower(
    std::string_view source,
    FrontendCompilation& compilation,
    std::unique_ptr<LlvmLowerer>& lowerer)
{
    if (!compilation.compileBuffer(
            std::filesystem::path("llvm_codegen_test.ns"),
            std::string(source),
            false))
    {
        return nullptr;
    }
    if (compilation.semanticAnalyzer() == nullptr || compilation.optimizationModule() == nullptr)
    {
        return nullptr;
    }

    lowerer = std::make_unique<LlvmLowerer>(compilation.semanticAnalyzer()->globalSymbols());
    return lowerer->lower(compilation.optimizationModule());
}

std::string readText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

void writeBytes(const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}
}

int main()
{
    using nativesharp::tests::TestContext;
    TestContext tests;

    std::string targetError;
    std::unique_ptr<LlvmTargetMachine> targetMachine = LlvmTargetMachine::createWindowsX64(targetError);
    tests.expect(targetMachine != nullptr, "Windows x64 LLVM target machine is available");
    tests.expect(targetError.empty(), "target-machine creation keeps the diagnostic empty on success");
    if (targetMachine != nullptr)
    {
        tests.expect(targetMachine->triple() == "x86_64-pc-windows-msvc",
            "target machine uses the NativeSharp Windows x64 triple");
        tests.expect(targetMachine->dataLayout().find("e-m:w") == 0,
            "target-machine data layout reports little-endian Windows mangling");
        tests.expect(targetMachine->dataLayout().find("i64:64") != std::string::npos,
            "target-machine data layout keeps 64-bit integer alignment");
    }

    FrontendCompilation compilation;
    std::unique_ptr<LlvmLowerer> lowerer;
    std::unique_ptr<LlvmModule> module = compileAndLower(
        "int Add(int a, int b) { return a + b; } int Main() { return Add(20, 22); }",
        compilation,
        lowerer);
    tests.expect(module != nullptr, "code-generation fixture lowers to verified LLVM input");

    const std::filesystem::path objectPath =
        std::filesystem::temp_directory_path() / "nativesharp_codegen_test.obj";
    const std::filesystem::path assemblyPath =
        std::filesystem::temp_directory_path() / "nativesharp_codegen_test.asm";
    std::error_code cleanupError;
    std::filesystem::remove(objectPath, cleanupError);
    std::filesystem::remove(assemblyPath, cleanupError);

    if (targetMachine != nullptr && module != nullptr)
    {
        std::string emissionError;
        tests.expect(targetMachine->emit(*module, objectPath, LlvmCodeGenFileType::Object, emissionError),
            "TargetMachine emits a COFF object file");
        tests.expect(emissionError.empty(), "successful object emission keeps diagnostic empty");
        tests.expect(std::filesystem::exists(objectPath) && std::filesystem::file_size(objectPath) > 20,
            "emitted object exists and is larger than a COFF file header");

        CoffObjectVerifier verifier;
        tests.expect(verifier.verify(objectPath), "NativeSharp COFF verifier accepts emitted object");
        tests.expect(verifier.info().machine == 0x8664,
            "emitted COFF object declares IMAGE_FILE_MACHINE_AMD64");
        tests.expect(verifier.info().sectionCount > 0, "emitted COFF object contains sections");
        tests.expect(verifier.info().optionalHeaderSize == 0,
            "emitted relocatable COFF object has no PE optional header");

        emissionError.clear();
        tests.expect(targetMachine->emit(*module, assemblyPath, LlvmCodeGenFileType::Assembly, emissionError),
            "TargetMachine emits textual x86-64 assembly");
        tests.expect(emissionError.empty(), "successful assembly emission keeps diagnostic empty");
        const std::string assembly = readText(assemblyPath);
        tests.expect(assembly.find("\t.text") != std::string::npos,
            "assembly output contains a text section");
        tests.expect(assembly.find("Main:") != std::string::npos,
            "assembly output contains the NativeSharp Main symbol");
        tests.expect(assembly.find("Add:") != std::string::npos,
            "assembly output contains the NativeSharp Add symbol");
        tests.expect(assembly.find("retq") != std::string::npos,
            "assembly output contains x86-64 return instructions");

        const std::filesystem::path missingDirectory =
            std::filesystem::temp_directory_path() / "nativesharp_missing_codegen_dir" / "bad.obj";
        std::filesystem::remove_all(missingDirectory.parent_path(), cleanupError);
        emissionError.clear();
        tests.expect(!targetMachine->emit(*module, missingDirectory, LlvmCodeGenFileType::Object, emissionError),
            "object emission rejects an output path inside a missing directory");
        tests.expect(!emissionError.empty(), "failed target emission reports an LLVM diagnostic");
    }

    {
        CoffObjectVerifier verifier;
        const std::filesystem::path tinyPath =
            std::filesystem::temp_directory_path() / "nativesharp_tiny_codegen_test.obj";
        writeBytes(tinyPath, "tiny");
        tests.expect(!verifier.verify(tinyPath), "COFF verifier rejects a truncated object");
        tests.expect(verifier.error().find("20-byte") != std::string::npos,
            "truncated-object rejection identifies the missing COFF header");
        std::filesystem::remove(tinyPath, cleanupError);
    }

    {
        CoffObjectVerifier verifier;
        const std::filesystem::path wrongMachinePath =
            std::filesystem::temp_directory_path() / "nativesharp_wrong_machine.obj";
        std::string header(20, '\0');
        header[0] = static_cast<char>(0x4c);
        header[1] = static_cast<char>(0x01);
        header[2] = static_cast<char>(0x01);
        writeBytes(wrongMachinePath, header);
        tests.expect(!verifier.verify(wrongMachinePath), "COFF verifier rejects a non-AMD64 machine type");
        tests.expect(verifier.error().find("AMD64") != std::string::npos,
            "wrong-machine rejection names the required architecture");
        std::filesystem::remove(wrongMachinePath, cleanupError);
    }

    std::filesystem::remove(objectPath, cleanupError);
    std::filesystem::remove(assemblyPath, cleanupError);

    return tests.result();
}
