#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Linker/CoffArchiveVerifier.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"
#include "nativesharp/LLVM/LlvmModule.h"

#include "../support/TestContext.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::compilation::ProjectLoader;
using nativesharp::compilation::ProjectOutputKind;
using nativesharp::driver::Driver;
using nativesharp::linker::CoffArchiveVerifier;
using nativesharp::linker::LldLinker;
using nativesharp::linker::PeExecutableVerifier;
using nativesharp::runtime::WindowsRuntimeBuilder;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view outputType,
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name><OutputType>" + std::string(outputType) +
            "</OutputType></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
            "</ItemGroup></Project>");
    writeFile(directory / "Library.ns", source);
}

ExitCode emitLibrary(
    std::string_view action,
    const std::filesystem::path& project,
    const std::filesystem::path& outputPath,
    std::string& output,
    std::string& error,
    bool optimize)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = project.string();
    const std::string outputText = outputPath.string();
    ExitCode result = ExitCode::CommandLineError;
    if (optimize)
    {
        const std::array<std::string_view, 5> args = {
            "-O2", action, projectText, "-o", outputText};
        result = driver.run(args, out, err);
    }
    else
    {
        const std::array<std::string_view, 4> args = {
            action, projectText, "-o", outputText};
        result = driver.run(args, out, err);
    }
    output = out.str();
    error = err.str();
    return result;
}

bool contains(const std::span<const std::string> values, std::string_view expected)
{
    for (const std::string& value : values)
    {
        if (value == expected)
        {
            return true;
        }
    }
    return false;
}

void testProjectKinds(TestContext& tests, const std::filesystem::path& root)
{
    writeProject(root / "StaticKind", "StaticKind", "StaticLibrary", "class A { }");
    writeProject(root / "LegacyKind", "LegacyKind", "Library", "class A { }");
    writeProject(root / "SharedKind", "SharedKind", "SharedLibrary", "class A { }");

    ProjectLoader loader;
    std::string error;
    auto project = loader.load(root / "StaticKind/StaticKind.nsproj", error);
    tests.expect(project.has_value() && project->outputKind() == ProjectOutputKind::Library,
        "Stage 46 parses StaticLibrary as the static-library project product");
    error.clear();
    project = loader.load(root / "LegacyKind/LegacyKind.nsproj", error);
    tests.expect(project.has_value() && project->outputKind() == ProjectOutputKind::Library,
        "legacy OutputType Library remains accepted as static-library compatible");
    error.clear();
    project = loader.load(root / "SharedKind/SharedKind.nsproj", error);
    tests.expect(project.has_value() && project->outputKind() == ProjectOutputKind::SharedLibrary,
        "Stage 46 parses SharedLibrary as the DLL project product");
}

void testRuntimeSupportMode(TestContext& tests)
{
    WindowsRuntimeBuilder builder;
    std::string error;
    auto module = builder.buildSupport(error);
    tests.expect(module != nullptr && error.empty(),
        "DLL/static-library runtime support builds without an executable entry contract");
    if (module == nullptr)
    {
        return;
    }
    const std::string text = module->text();
    tests.expect(text.find("@__nativesharp_start") == std::string::npos,
        "library runtime support deliberately omits __nativesharp_start");
    tests.expect(text.find("@__nativesharp_alloc") != std::string::npos &&
            text.find("@__nativesharp_free") != std::string::npos,
        "library runtime support preserves NativeSharp allocation/deallocation helpers");
    tests.expect(text.find("@__nativesharp_throw") != std::string::npos,
        "library runtime support preserves native exception transport");
}

void testStaticLibrary(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "StaticLibrary";
    writeProject(directory, "StaticLibrary", "StaticLibrary", R"NS(
namespace Stage46.Static
{
    public class Math
    {
        public static int Add(int left, int right) { return left + right; }
    }
}
)NS");

    const std::filesystem::path library = directory / "NativeMath.lib";
    std::string output;
    std::string error;
    tests.expect(emitLibrary(
            "--emit-project-oop-lib",
            directory / "StaticLibrary.nsproj",
            library,
            output,
            error,
            true) == ExitCode::Success,
        "Stage 46 driver emits a native static .lib from a StaticLibrary project");
    tests.expect(error.empty(), "successful static-library generation keeps stderr empty");
    tests.expect(output.find("project OOP static library emission: ok") != std::string::npos &&
            output.find("format: COFF archive (.lib)") != std::string::npos,
        "static-library driver output reports the physical COFF archive product");
    tests.expect(std::filesystem::is_regular_file(library) && std::filesystem::file_size(library) > 512,
        "static-library generation creates a non-empty .lib artifact");

    CoffArchiveVerifier verifier;
    tests.expect(verifier.verify(library), "COFF archive verifier accepts the generated static library");
    if (verifier.verify(library))
    {
        tests.expect(verifier.info().memberCount >= 2,
            "static library contains archive metadata plus NativeSharp program/runtime members");
    }

    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    tests.expect(linker != nullptr, "Stage 46 static-library test discovers lld-link");
    if (linker != nullptr)
    {
        const std::array<std::filesystem::path, 0> noObjects{};
        tests.expect(!linker->createStaticLibrary(noObjects, directory / "Empty.lib", error) &&
                error.find("requires at least one COFF object") != std::string::npos,
            "static-library linker rejects an empty object set deterministically");
    }

    const std::filesystem::path unoptimizedLibrary = directory / "NativeMathO0.lib";
    output.clear();
    error.clear();
    tests.expect(emitLibrary(
            "--emit-project-oop-lib",
            directory / "StaticLibrary.nsproj",
            unoptimizedLibrary,
            output,
            error,
            false) == ExitCode::Success,
        "Stage 46 static-library pipeline also closes at O0");
    tests.expect(verifier.verify(unoptimizedLibrary),
        "O0 static-library output remains a structurally valid COFF archive");
}

void testDynamicLibrary(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "DynamicLibrary";
    writeProject(directory, "DynamicLibrary", "SharedLibrary", R"NS(
namespace Stage46.Shared
{
    public class Math
    {
        public static export extern C int Add(int left, int right) { return left + right; }
        public static export extern C int Multiply(int left, int right) { return left * right; }
        private static int Hidden(int value) { return value + 1; }
    }
}
)NS");

    const std::filesystem::path dll = directory / "NativeMath.dll";
    const std::filesystem::path importLibrary = directory / "NativeMath.lib";
    std::string output;
    std::string error;
    tests.expect(emitLibrary(
            "--emit-project-oop-dll",
            directory / "DynamicLibrary.nsproj",
            dll,
            output,
            error,
            true) == ExitCode::Success,
        "Stage 46 driver emits a native PE DLL from a SharedLibrary project");
    tests.expect(error.empty(), "successful DLL generation keeps stderr empty");
    tests.expect(std::filesystem::is_regular_file(dll) && std::filesystem::file_size(dll) > 1024,
        "DLL generation creates a non-trivial PE image");
    tests.expect(std::filesystem::is_regular_file(importLibrary) &&
            std::filesystem::file_size(importLibrary) > 512,
        "DLL generation creates the matching COFF import .lib");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verifyDynamicLibrary(dll),
        "PE verifier accepts the generated NativeSharp dynamic library");
    if (!verifier.verifyDynamicLibrary(dll))
    {
        return;
    }
    const auto& info = verifier.info();
    const std::string add = "Add";
    const std::string multiply = "Multiply";
    const std::string hidden = "Hidden";
    tests.expect(info.machine == 0x8664 && info.optionalHeaderMagic == 0x020b,
        "NativeSharp DLL is AMD64 PE32+");
    tests.expect((info.characteristics & 0x2000U) != 0,
        "NativeSharp dynamic library carries IMAGE_FILE_DLL");
    tests.expect(info.clrDirectoryRva == 0,
        "NativeSharp DLL contains no CLR runtime header");
    tests.expect(info.exportDirectoryRva != 0 && info.exportDirectorySize != 0,
        "NativeSharp DLL materializes a PE export directory");
    tests.expect(contains(info.exportedSymbols, add) && contains(info.exportedSymbols, multiply),
        "Stage 46 library product remains valid under the Stage 47 stable C ABI export policy");
    tests.expect(!contains(info.exportedSymbols, hidden),
        "Stage 46 does not export private static methods");
    tests.expect(info.exportedSymbols.size() == 2,
        "current explicit export selection is exact for the Stage 46 sample library");
    tests.expect(info.exceptionDirectoryRva != 0,
        "NativeSharp DLL retains Win64 unwind metadata from its native runtime support");

    CoffArchiveVerifier importVerifier;
    tests.expect(importVerifier.verify(importLibrary),
        "generated DLL import library is a structurally valid COFF archive");
    tests.expect(output.find("project OOP DLL emission: ok") != std::string::npos &&
            output.find(add) != std::string::npos,
        "DLL driver output exposes the exact stable C ABI export symbol");

    const std::filesystem::path unoptimizedDll = directory / "NativeMathO0.dll";
    const std::filesystem::path unoptimizedImportLibrary = directory / "NativeMathO0.lib";
    output.clear();
    error.clear();
    tests.expect(emitLibrary(
            "--emit-project-oop-dll",
            directory / "DynamicLibrary.nsproj",
            unoptimizedDll,
            output,
            error,
            false) == ExitCode::Success,
        "Stage 46 DLL pipeline also closes at O0");
    tests.expect(verifier.verifyDynamicLibrary(unoptimizedDll),
        "O0 DLL remains a verified PE32+ AMD64 dynamic library");
    if (verifier.verifyDynamicLibrary(unoptimizedDll))
    {
        tests.expect(contains(verifier.info().exportedSymbols, add) &&
                contains(verifier.info().exportedSymbols, multiply),
            "O0 DLL preserves the same explicit C ABI export set");
    }
    tests.expect(importVerifier.verify(unoptimizedImportLibrary),
        "O0 DLL generation also creates a valid matching import library");
}

void testOutputKindGuards(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "WrongKind";
    writeProject(directory, "WrongKind", "Exe", R"NS(
class Program
{
    public static int Main() { return 0; }
    public static int Add(int a, int b) { return a + b; }
}
)NS");

    std::string output;
    std::string error;
    tests.expect(emitLibrary(
            "--emit-project-oop-lib",
            directory / "WrongKind.nsproj",
            directory / "WrongKind.lib",
            output,
            error,
            false) == ExitCode::CompilationError,
        "static-library emission rejects an Exe root project");
    tests.expect(error.find("requires root OutputType Library or StaticLibrary") != std::string::npos,
        "wrong static-library project kind reports a precise Stage 46 diagnostic");

    output.clear();
    error.clear();
    tests.expect(emitLibrary(
            "--emit-project-oop-dll",
            directory / "WrongKind.nsproj",
            directory / "WrongKind.dll",
            output,
            error,
            false) == ExitCode::CompilationError,
        "DLL emission rejects an Exe root project");
    tests.expect(error.find("requires root OutputType SharedLibrary") != std::string::npos,
        "wrong DLL project kind reports a precise Stage 46 diagnostic");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_stage46_native_libraries");
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root, cleanupError);

    testProjectKinds(tests, root);
    testRuntimeSupportMode(tests);
    testStaticLibrary(tests, root);
    testDynamicLibrary(tests, root);
    testOutputKindGuards(tests, root);

    return tests.resultKeeping(root);
}
