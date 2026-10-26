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
#include <iostream>
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

void testCleanupFunclets(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "CleanupFunclets";
    writeProject(directory, "CleanupFunclets", R"NS(
namespace NativeSharp.Core
{
    public class Exception { }
    public interface IDisposable { void Dispose(); }
}
using NativeSharp.Core;

class Resource : IDisposable
{
    public void Dispose() { int disposed = 1; }
}

class Runner
{
    public static void ThrowFromCallee(Exception problem) throws NativeSharp.Core.Exception
    {
        throw new Exception();
    }

    public static void CallThroughUsing(Resource input, Exception problem)
    {
        try
        {
            using (Resource resource = input)
            {
                ThrowFromCallee(problem);
            }
        }
        catch
        {
            int handled = 1;
        }
        finally
        {
            int finalized = 2;
        }
    }

    public static void RethrowThroughUsing(Resource input, Exception problem) throws NativeSharp.Core.Exception
    {
        try
        {
            throw new Exception();
        }
        catch
        {
            using (Resource resource = input)
            {
                throw;
            }
        }
        finally
        {
            int finalized = 3;
        }
    }

    public static int ReturnFromCatch(Exception problem)
    {
        try
        {
            throw new Exception();
        }
        catch
        {
            return 7;
        }
        finally
        {
            int finalized = 4;
        }
    }

    public static void Loop(Resource input, bool leave)
    {
        while (true)
        {
            using (Resource resource = input)
            {
                if (leave)
                {
                    break;
                }
                continue;
            }
        }
    }

    public static void InterfaceCleanup(IDisposable input)
    {
        using (IDisposable resource = input)
        {
            return;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "CleanupFunclets.nsproj", error);
    tests.expect(ok, "Stage 45.10 cleanup sample lowers through project OOP MIR");
    tests.expect(error.empty(), "Stage 45.10 MIR construction keeps the diagnostic empty");
    tests.expect(compilation.oopMirModel() != nullptr && compilation.nameLookup() != nullptr,
        "Stage 45.10 retains MIR and name lookup for LLVM cleanup lowering");
    if (!ok || compilation.oopMirModel() == nullptr || compilation.nameLookup() == nullptr) return;

    ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<LlvmModule> module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr, "Stage 45.10 lowers using/finally cleanup MIR to LLVM Win64 funclets");
    tests.expect(lowerer.error().empty(), "Stage 45.10 LLVM cleanup lowering reports no internal error");
    if (module == nullptr)
    {
        std::cerr << "Stage45.10 lowerer error: " << lowerer.error() << '\n';
        return;
    }

    std::string verificationError;
    tests.expect(module->verify(verificationError), "Stage 45.10 LLVM cleanup module passes LLVMVerifyModule");
    tests.expect(verificationError.empty(), "verified Stage 45.10 module has no LLVM diagnostic");

    const std::string text = module->text();
    tests.expect(text.find("personality ptr @__C_specific_handler") != std::string::npos,
        "cleanup-bearing functions retain the Windows x64 EH personality");
    tests.expect(text.find("cleanuppad within") != std::string::npos,
        "exceptional using/finally paths contain real cleanuppad funclets");
    tests.expect(text.find("cleanupret from") != std::string::npos,
        "cleanup funclets continue unwinding with cleanupret");
    tests.expect(text.find("\"funclet\"(token") != std::string::npos,
        "calls emitted inside catch/cleanup funclets carry the required funclet operand bundle");
    tests.expect(text.find("invoke void @__nativesharp_throw(ptr") != std::string::npos,
        "explicit throw/rethrow uses invoke when a cleanup or handler remains active");
    tests.expect(text.find("@__nativesharp_exception_current") != std::string::npos,
        "catch funclets capture the thread-local current NativeSharp exception for rethrow");
    tests.expect(text.find("catchret from") != std::string::npos,
        "normal and structured catch exits leave the catch funclet through catchret");
    tests.expect(text.find("eh.return.addr") != std::string::npos,
        "return values crossing catch/finally are spilled before cleanup traversal");
    tests.expect(text.find("Dispose") != std::string::npos,
        "using cleanup retains the statically resolved Dispose target in LLVM IR");
    tests.expect(text.find("dispose.interface.map") != std::string::npos,
        "IDisposable-typed using preserves native interface dispatch during cleanup");
    tests.expect(text.find("using.resource.isnull") != std::string::npos,
        "using cleanup preserves null-as-no-op semantics");
    tests.expect(text.find("eh.cleanup") != std::string::npos,
        "exceptional cleanup blocks receive dedicated stable LLVM block names");
    tests.expect(text.find("invoke void @") != std::string::npos &&
            text.find("ThrowFromCallee") != std::string::npos,
        "a potentially-throwing user call inside using is represented by an invoke rather than a plain call");

    std::string targetError;
    std::unique_ptr<LlvmTargetMachine> target = LlvmTargetMachine::createWindowsX64(targetError);
    tests.expect(target != nullptr, "Stage 45.10 obtains the Windows x64 target machine");
    tests.expect(targetError.empty(), "Stage 45.10 Windows x64 target creation has no diagnostic");
    if (target == nullptr) return;

    const std::filesystem::path objectPath = root / "stage45_10_cleanup_funclets.obj";
    std::string emissionError;
    tests.expect(target->emit(*module, objectPath, LlvmCodeGenFileType::Object, emissionError),
        "Stage 45.10 emits a real Windows x64 COFF object with cleanup unwind metadata");
    tests.expect(emissionError.empty(), "Stage 45.10 COFF emission keeps the diagnostic empty");
    CoffObjectVerifier verifier;
    tests.expect(verifier.verify(objectPath), "COFF verifier accepts the Stage 45.10 cleanup object");
    const std::vector<std::string> sections = coffSectionNames(objectPath);
    tests.expect(containsName(sections, ".pdata"), "Stage 45.10 cleanup COFF retains .pdata unwind metadata");
    tests.expect(containsName(sections, ".xdata"), "Stage 45.10 cleanup COFF retains .xdata language-specific metadata");
}

void testTypedCatchIntegrated(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "TypedCatchDeferred";
    writeProject(directory, "TypedCatchDeferred", R"NS(
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

    ProjectCompilation compilation;
    std::string error;
    tests.expect(compilation.compileProjectOopMir(directory / "TypedCatchDeferred.nsproj", error),
        "typed catch remains valid in the inherited Stage 45.10 MIR sample");
    ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr, "Stage 45.11 now composes typed catch RTTI with Stage 45.10 cleanup funclets");
    tests.expect(lowerer.error().empty(), "typed catch + cleanup integration leaves no LLVM diagnostic");
    if (module != nullptr)
    {
        std::string verifyError;
        tests.expect(module->verify(verifyError), "typed catch + cleanup inherited sample verifies");
        const std::string text = module->text();
        tests.expect(text.find("__nativesharp_eh_filter_type_") != std::string::npos,
            "typed catch + cleanup inherited sample uses an RTTI-specific SEH filter");
    }
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage45_10_llvm_cleanup_funclets";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testCleanupFunclets(tests, root);
    testTypedCatchIntegrated(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
