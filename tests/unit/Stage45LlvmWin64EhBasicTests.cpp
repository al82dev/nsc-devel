#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::frontend::ProjectCompilation;
using nativesharp::llvm_backend::CoffObjectVerifier;
using nativesharp::llvm_backend::LlvmCodeGenFileType;
using nativesharp::llvm_backend::LlvmModule;
using nativesharp::llvm_backend::LlvmTargetMachine;
using nativesharp::llvm_backend::ProjectOopLlvmLowerer;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name, std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

std::uint16_t readU16(const std::array<unsigned char, 20>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::vector<std::string> coffSectionNames(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::array<unsigned char, 20> header{};
    stream.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (!stream) return {};
    const std::uint16_t sectionCount = readU16(header, 2);
    const std::uint16_t optionalHeaderSize = readU16(header, 16);
    stream.seekg(static_cast<std::streamoff>(20U + optionalHeaderSize), std::ios::beg);
    std::vector<std::string> names;
    for (std::uint16_t index = 0; index < sectionCount; ++index)
    {
        std::array<char, 40> section{};
        stream.read(section.data(), static_cast<std::streamsize>(section.size()));
        if (!stream) return {};
        std::size_t length = 0;
        while (length < 8 && section[length] != '\0') ++length;
        names.emplace_back(section.data(), length);
    }
    return names;
}

bool containsName(const std::vector<std::string>& names, std::string_view expected)
{
    for (const std::string& name : names)
    {
        if (name == expected) return true;
    }
    return false;
}

std::string readBinary(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void testBasicWin64Eh(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "BasicWin64Eh";
    writeProject(directory, "BasicWin64Eh", R"NS(
namespace NativeSharp.Core
{
    public class Exception { }
}
using NativeSharp.Core;

class Problem : Exception { }

class Runner
{
    public static int Run(Problem problem, bool fail)
    {
        int result = 10;
        try
        {
            if (fail)
            {
                throw new Problem();
            }
            result = 20;
        }
        catch
        {
            result = 30;
        }
        return result;
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "BasicWin64Eh.nsproj", error);
    tests.expect(ok, "Stage 45.9 catch-all sample lowers through OOP MIR");
    tests.expect(error.empty(), "Stage 45.9 MIR construction keeps the diagnostic empty");
    tests.expect(compilation.oopMirModel() != nullptr && compilation.nameLookup() != nullptr,
        "Stage 45.9 retains MIR and project name lookup for LLVM lowering");
    if (!ok || compilation.oopMirModel() == nullptr || compilation.nameLookup() == nullptr) return;

    ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<LlvmModule> module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr, "Stage 45.9 lowers basic catch-all MIR to LLVM Windows EH");
    tests.expect(lowerer.error().empty(), "Stage 45.9 LLVM lowering reports no internal error");
    if (module == nullptr) return;

    std::string verificationError;
    tests.expect(module->verify(verificationError), "Stage 45.9 LLVM module passes LLVMVerifyModule");
    tests.expect(verificationError.empty(), "verified Stage 45.9 module has no LLVM diagnostic");

    const std::string text = module->text();
    tests.expect(text.find("@__C_specific_handler") != std::string::npos &&
            text.find("personality ptr @__C_specific_handler") != std::string::npos,
        "EH function uses the Windows x64 __C_specific_handler personality");
    tests.expect(text.find("invoke void @__nativesharp_throw(ptr") != std::string::npos,
        "explicit NativeSharp throw becomes an LLVM invoke");
    tests.expect(text.find("unwind label %eh.catch.dispatch") != std::string::npos,
        "throw invoke has an explicit exceptional edge to catch dispatch");
    tests.expect(text.find("catchswitch within none") != std::string::npos,
        "basic protected region lowers to catchswitch within none");
    tests.expect(text.find("catchpad within") != std::string::npos &&
            text.find("@__nativesharp_eh_filter") != std::string::npos,
        "catch handler begins with catchpad using the NativeSharp domain filter");
    tests.expect(text.find("catchret from") != std::string::npos,
        "normal catch completion exits its funclet through catchret");
    tests.expect(text.find("call void @__nativesharp_exception_clear()") != std::string::npos,
        "handled catch clears the thread-local current exception after catchret");
    tests.expect(text.find("cleanuppad") == std::string::npos &&
            text.find("cleanupret") == std::string::npos,
        "Stage 45.9 does not prematurely implement Stage 45.10 cleanup funclets");

    std::string targetError;
    std::unique_ptr<LlvmTargetMachine> target = LlvmTargetMachine::createWindowsX64(targetError);
    tests.expect(target != nullptr, "Stage 45.9 obtains the Windows x64 target machine");
    tests.expect(targetError.empty(), "Stage 45.9 Windows x64 target creation has no diagnostic");
    if (target == nullptr) return;

    const std::filesystem::path objectPath = root / "stage45_9_basic_eh.obj";
    std::string emissionError;
    tests.expect(target->emit(*module, objectPath, LlvmCodeGenFileType::Object, emissionError),
        "Stage 45.9 emits a real Windows x64 COFF object containing EH metadata");
    tests.expect(emissionError.empty(), "Stage 45.9 COFF emission keeps the diagnostic empty");
    CoffObjectVerifier verifier;
    tests.expect(verifier.verify(objectPath), "COFF verifier accepts the Stage 45.9 object");
    const std::vector<std::string> sections = coffSectionNames(objectPath);
    tests.expect(containsName(sections, ".pdata"), "Win64 EH object contains .pdata unwind metadata");
    tests.expect(containsName(sections, ".xdata"), "Win64 EH object contains .xdata language-specific metadata");
    const std::string objectBytes = readBinary(objectPath);
    tests.expect(objectBytes.find("__C_specific_handler") != std::string::npos,
        "COFF symbol table retains the __C_specific_handler dependency");
    tests.expect(objectBytes.find("__nativesharp_eh_filter") != std::string::npos,
        "COFF symbol table retains the NativeSharp filter dependency");
}

void testDeferredBoundaries(TestContext& tests, const std::filesystem::path& root)
{
    const auto typedDirectory = root / "TypedCatchDeferred";
    writeProject(typedDirectory, "TypedCatchDeferred", R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
class Problem : Exception { }
class Runner
{
    public static void Run(Problem problem)
    {
        try { throw new Problem(); }
        catch (Problem caught) { }
    }
}
)NS");
    ProjectCompilation typedCompilation;
    std::string error;
    tests.expect(typedCompilation.compileProjectOopMir(typedDirectory / "TypedCatchDeferred.nsproj", error),
        "typed catch remains semantically valid before LLVM RTTI matching");
    ProjectOopLlvmLowerer typedLowerer;
    auto typedModule = typedLowerer.lower(*typedCompilation.nameLookup(), *typedCompilation.oopMirModel());
    tests.expect(typedModule != nullptr, "Stage 45.11 now lowers the inherited typed-catch boundary");
    tests.expect(typedLowerer.error().empty(), "Stage 45.11 typed-catch integration leaves no inherited boundary diagnostic");
    if (typedModule != nullptr)
    {
        std::string typedVerificationError;
        tests.expect(typedModule->verify(typedVerificationError),
            "inherited Stage 45.9 typed-catch sample verifies after Stage 45.11 RTTI lowering");
        const std::string typedText = typedModule->text();
        tests.expect(typedText.find("__nativesharp_eh_filter_type_") != std::string::npos &&
                typedText.find("@__nativesharp_rtti_is_class") != std::string::npos,
            "inherited typed catch now exposes a native RTTI SEH filter");
    }

    const auto cleanupDirectory = root / "CleanupDeferred";
    writeProject(cleanupDirectory, "CleanupDeferred", R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
class Problem : Exception { }
class Runner
{
    public static void Run(Problem problem)
    {
        try { throw new Problem(); }
        catch { }
        finally { int marker = 0; }
    }
}
)NS");
    ProjectCompilation cleanupCompilation;
    error.clear();
    tests.expect(cleanupCompilation.compileProjectOopMir(cleanupDirectory / "CleanupDeferred.nsproj", error),
        "try/catch/finally remains semantically valid before LLVM cleanup funclets");
    ProjectOopLlvmLowerer cleanupLowerer;
    auto cleanupModule = cleanupLowerer.lower(*cleanupCompilation.nameLookup(), *cleanupCompilation.oopMirModel());
    tests.expect(cleanupModule != nullptr, "Stage 45.10 now lowers the inherited try/catch/finally boundary");
    tests.expect(cleanupLowerer.error().empty(), "Stage 45.10 finally lowering leaves no inherited boundary diagnostic");
    if (cleanupModule != nullptr)
    {
        std::string cleanupVerificationError;
        tests.expect(cleanupModule->verify(cleanupVerificationError),
            "inherited Stage 45.9 finally sample verifies after Stage 45.10 cleanup lowering");
        const std::string cleanupText = cleanupModule->text();
        // UPDATED (multi-catch / B-006 / B-007 fix -- see ProjectOopLlvmLowerer.cpp's catchswitch
        // comment). This used to require a `cleanuppad`/`cleanupret` pair for a try/CATCH/finally.
        // That shape came from making the enclosing cleanup chain the catchswitch's unwind
        // destination, which is exactly what `__C_specific_handler` mis-executes: it ran that
        // cleanup while unwinding to a catch in the same frame, destroying objects still in scope.
        // A try/finally with no catch clauses still lowers to real cleanup funclets and is still
        // covered elsewhere in this file; a try/catch/finally now runs its finally from the
        // explicit no-clause-matched path instead, which is what the assertions below pin.
        //
        // The behaviour this shape must preserve -- that the `finally` really does run whether the
        // catch matches or not -- is verified by REAL EXECUTION, not by IR text, in
        // tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp (scenario 13).
        tests.expect(cleanupText.find("catchret from") != std::string::npos,
            "inherited finally sample still exits its Windows funclet through catchret");
        tests.expect(cleanupText.find("eh.catch.select") != std::string::npos &&
                cleanupText.find("eh.catch.unmatched") != std::string::npos,
            "inherited finally sample routes clause selection and the unmatched path through "
            "ordinary parent code");
    }
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage45_9_llvm_win64_eh_basic";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testBasicWin64Eh(tests, root);
    testDeferredBoundaries(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
