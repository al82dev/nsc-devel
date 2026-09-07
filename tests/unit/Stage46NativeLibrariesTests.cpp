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

#include <algorithm>
#include <array>
#include <cstdlib>
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

ExitCode runProjectAction(
    std::string_view action,
    const std::filesystem::path& project,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = project.string();
    const std::array<std::string_view, 2> args = {action, projectText};
    const ExitCode result = driver.run(args, out, err);
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

int runExecutable(const std::filesystem::path& path)
{
#ifdef _WIN32
    return std::system(("\"" + path.string() + "\"").c_str());
#else
    static_cast<void>(path);
    return 0;
#endif
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
    tests.expect(!contains(info.exportedSymbols, hidden) &&
            std::none_of(info.exportedSymbols.begin(), info.exportedSymbols.end(),
                [](const std::string& symbol) { return symbol.find("Hidden") != std::string::npos; }),
        "Stage 46 does not export private static methods under plain or mangled names");
    tests.expect(info.exportedSymbols.size() == 4,
        "DLL exports each public method through its NativeSharp mangled symbol plus its explicit C wrapper");
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

void testPrecompiledLibraryReferences(TestContext& tests, const std::filesystem::path& root)
{
    const std::string producerSource = R"NS(
namespace Precompiled
{
    public class Math
    {
        public static export extern C int Add(int left, int right = 22) { return left + right; }
        public static int Blend(int left, int right = 22) { return left + right; }
        public static long Blend(long left, long right) { return left + right; }
        public static void Bump(ref int value) { value += 1; }
        public static void Assign(out int value) { value = 0; }
        public static int Read(in int value) { return value; }
        private static int Hidden(int value) { return value + 1; }
    }
}
)NS";
    const std::string consumerSource = R"NS(
namespace Consumer
{
    public class NativeMath
    {
        extern C { public int Add(int left, int right); }
    }

    public class Program
    {
        public static int Main() { return NativeMath.Add(20, 22); }
    }
}
)NS";

    const auto staticRoot = root / "PrecompiledStatic";
    const auto staticProducer = staticRoot / "Producer";
    const auto staticConsumer = staticRoot / "Consumer";
    writeProject(staticProducer, "StaticProducer", "StaticLibrary", producerSource);
    std::string output;
    std::string error;
    const auto staticLibrary = staticProducer / "NativeMath.lib";
    tests.expect(emitLibrary(
            "--emit-project-oop-lib", staticProducer / "StaticProducer.nsproj",
            staticLibrary, output, error, true) == ExitCode::Success,
        "0.9 precompiled static-library producer emits .lib and metadata");
    CoffArchiveVerifier precompiledArchiveVerifier;
    tests.expect(precompiledArchiveVerifier.verify(staticLibrary) &&
            contains(precompiledArchiveVerifier.info().symbolNames, "Add"),
        "COFF archive index exposes the C ABI symbol published by metadata");
    writeFile(staticConsumer / "Main.ns", consumerSource);
    writeFile(
        staticConsumer / "StaticConsumer.nsproj",
        "<Project><PropertyGroup><Name>StaticConsumer</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto staticExecutable = staticConsumer / "StaticConsumer.exe";
    const ExitCode staticResult = emitLibrary(
        "--emit-project-oop-exe", staticConsumer / "StaticConsumer.nsproj",
        staticExecutable, output, error, true);
    tests.expect(staticResult == ExitCode::Success,
        "consumer links a NativeSharp static library from LibraryReference without --link-lib (" +
            error + ")");
    tests.expect(output.find("linked-libraries: 1") != std::string::npos,
        "static consumer reports one validated project library reference");
#ifdef _WIN32
    if (staticResult == ExitCode::Success)
    {
        tests.expect(runExecutable(staticExecutable) == 42,
            "separate static-library producer/consumer executes through the published C ABI");
    }
#endif

    const auto automaticStaticConsumer = staticRoot / "AutomaticConsumer";
    writeFile(automaticStaticConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        public static int Main()
        {
            int probe = -1;
            Precompiled.Math.Bump(ref probe);
            int assigned;
            Precompiled.Math.Assign(out assigned);
            return Precompiled.Math.Blend(left: 20) +
                (int)Precompiled.Math.Blend((long)0, (long)0) +
                probe + Precompiled.Math.Read(in assigned);
        }
    }
}
)NS");
    writeFile(
        automaticStaticConsumer / "AutomaticConsumer.nsproj",
        "<Project><PropertyGroup><Name>AutomaticConsumer</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto automaticStaticExecutable = automaticStaticConsumer / "AutomaticConsumer.exe";
    const ExitCode automaticStaticResult = emitLibrary(
        "--emit-project-oop-exe", automaticStaticConsumer / "AutomaticConsumer.nsproj",
        automaticStaticExecutable, output, error, true);
    tests.expect(automaticStaticResult == ExitCode::Success,
        "metadata-backed static consumer resolves overloads, defaults, and ref/out/in without an "
        "extern C redeclaration (" + error + ")");
#ifdef _WIN32
    if (automaticStaticResult == ExitCode::Success)
    {
        tests.expect(runExecutable(automaticStaticExecutable) == 42,
            "metadata-backed static overload calls use imported defaults and execute to 42");
    }
#endif

    output.clear();
    error.clear();
    tests.expect(runProjectAction(
            "--dump-project-names",
            automaticStaticConsumer / "AutomaticConsumer.nsproj",
            output,
            error) == ExitCode::Success &&
            output.find("StaticProducer::Precompiled.Math") != std::string::npos,
        "metadata type enters name lookup with the producer's canonical module identity");

    const auto hiddenConsumer = staticRoot / "HiddenConsumer";
    writeFile(hiddenConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        public static int Main() { return Precompiled.Math.Hidden(20); }
    }
}
)NS");
    writeFile(
        hiddenConsumer / "HiddenConsumer.nsproj",
        "<Project><PropertyGroup><Name>HiddenConsumer</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary(
            "--emit-project-oop-exe", hiddenConsumer / "HiddenConsumer.nsproj",
            hiddenConsumer / "HiddenConsumer.exe", output, error, true) ==
            ExitCode::CompilationError,
        "metadata-backed consumer cannot call a method that was not published");
    tests.expect(error.find("Hidden") != std::string::npos,
        "unpublished-member diagnostic names the inaccessible API requested by the consumer");

    const auto ambiguousProducer = staticRoot / "AmbiguousProducer";
    writeProject(ambiguousProducer, "OtherMath", "StaticLibrary", R"NS(
namespace Precompiled
{
    public class Math
    {
        public static export extern C int Other(int value) { return value; }
    }
}
)NS");
    output.clear();
    error.clear();
    const auto ambiguousLibrary = ambiguousProducer / "OtherMath.lib";
    tests.expect(emitLibrary(
            "--emit-project-oop-lib", ambiguousProducer / "OtherMath.nsproj",
            ambiguousLibrary, output, error, true) == ExitCode::Success,
        "ambiguity fixture emits a second module with the same public type spelling");

    const auto ambiguousConsumer = staticRoot / "AmbiguousConsumer";
    writeFile(ambiguousConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        public static int Main()
        {
            int probe = -1;
            Precompiled.Math.Bump(ref probe);
            int assigned;
            Precompiled.Math.Assign(out assigned);
            return Precompiled.Math.Blend(20) +
                (int)Precompiled.Math.Blend((long)0, (long)0) +
                probe + Precompiled.Math.Read(in assigned);
        }
    }
}
)NS");
    writeFile(
        ambiguousConsumer / "AmbiguousConsumer.nsproj",
        "<Project><PropertyGroup><Name>AmbiguousConsumer</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
        "<LibraryReference Include=\"../AmbiguousProducer/OtherMath.lib\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary(
            "--emit-project-oop-exe", ambiguousConsumer / "AmbiguousConsumer.nsproj",
            ambiguousConsumer / "AmbiguousConsumer.exe", output, error, true) ==
            ExitCode::CompilationError,
        "two referenced modules with the same public type spelling are rejected as ambiguous");
    tests.expect(error.find("ambiguous") != std::string::npos,
        "cross-library type ambiguity is diagnosed before lowering and link");

    const auto compositeProducer = staticRoot / "CompositeProducer";
    writeFile(compositeProducer / "Main.ns", R"NS(
namespace Precompiled
{
    public class Composite
    {
        extern C { public int Add(int left, int right); }
        public static export extern C int Answer() { return Composite.Add(20, 22); }
    }
}
)NS");
    writeFile(
        compositeProducer / "CompositeProducer.nsproj",
        "<Project><PropertyGroup><Name>CompositeProducer</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto compositeLibrary = compositeProducer / "Composite.lib";
    const ExitCode compositeResult = emitLibrary(
        "--emit-project-oop-lib", compositeProducer / "CompositeProducer.nsproj",
        compositeLibrary, output, error, true);
    tests.expect(compositeResult == ExitCode::Success,
        "static producer consolidates its validated static LibraryReference (" + error + ")");
    CoffArchiveVerifier compositeVerifier;
    tests.expect(compositeResult == ExitCode::Success && compositeVerifier.verify(compositeLibrary) &&
            contains(compositeVerifier.info().symbolNames, "Answer") &&
            contains(compositeVerifier.info().symbolNames, "Add"),
        "consolidated static archive indexes both its own API and dependency symbols");

    const auto compositeConsumer = staticRoot / "CompositeConsumer";
    writeFile(compositeConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Composite
    {
        extern C { public int Answer(); }
    }

    public class Program
    {
        public static int Main() { return Composite.Answer(); }
    }
}
)NS");
    writeFile(
        compositeConsumer / "CompositeConsumer.nsproj",
        "<Project><PropertyGroup><Name>CompositeConsumer</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../CompositeProducer/Composite.lib\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto compositeExecutable = compositeConsumer / "CompositeConsumer.exe";
    const ExitCode compositeConsumerResult = emitLibrary(
        "--emit-project-oop-exe", compositeConsumer / "CompositeConsumer.nsproj",
        compositeExecutable, output, error, true);
    tests.expect(compositeConsumerResult == ExitCode::Success,
        "consumer links the consolidated static product without referencing its dependency (" +
            error + ")");
#ifdef _WIN32
    if (compositeConsumerResult == ExitCode::Success)
    {
        tests.expect(runExecutable(compositeExecutable) == 42,
            "consolidated A-to-B static dependency executes from A alone");
    }
#endif

    const auto badSignatureConsumer = staticRoot / "BadSignatureConsumer";
    writeFile(badSignatureConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class NativeMath
    {
        extern C { public long Add(int left, int right); }
    }

    public class Program
    {
        public static int Main() { return (int)NativeMath.Add(20, 22); }
    }
}
)NS");
    writeFile(
        badSignatureConsumer / "BadSignatureConsumer.nsproj",
        "<Project><PropertyGroup><Name>BadSignatureConsumer</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto badSignatureExecutable = badSignatureConsumer / "BadSignatureConsumer.exe";
    tests.expect(emitLibrary(
            "--emit-project-oop-exe", badSignatureConsumer / "BadSignatureConsumer.nsproj",
            badSignatureExecutable, output, error, true) == ExitCode::CompilationError,
        "LibraryReference rejects an extern C declaration with an ABI-incompatible signature");
    tests.expect(error.find("does not match the metadata signature published by module "
            "'StaticProducer'") != std::string::npos ||
            error.find("conflicting declarations of imported C symbol 'Add'") != std::string::npos,
        "signature mismatch is rejected before link by the imported metadata contract");
    tests.expect(!std::filesystem::exists(badSignatureExecutable),
        "signature mismatch stops before producing an executable");

    const auto wrongArchiveConsumer = staticRoot / "WrongArchiveConsumer";
    writeFile(wrongArchiveConsumer / "Main.ns", consumerSource);
    writeFile(
        wrongArchiveConsumer / "WrongArchiveConsumer.nsproj",
        "<Project><PropertyGroup><Name>WrongArchiveConsumer</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../../StaticLibrary/NativeMath.lib\" "
            "Metadata=\"../Producer/NativeMath.nsmeta\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary(
            "--emit-project-oop-exe", wrongArchiveConsumer / "WrongArchiveConsumer.nsproj",
            wrongArchiveConsumer / "WrongArchiveConsumer.exe", output, error, true) ==
            ExitCode::CompilationError,
        "LibraryReference rejects metadata paired with a different static archive");
    tests.expect(error.find("does not contain C ABI symbol 'Add' published by metadata module "
            "'StaticProducer'") != std::string::npos,
        "static archive/metadata mismatch reports the first missing published symbol");

    const auto dllRoot = root / "PrecompiledDll";
    const auto dllProducer = dllRoot / "Producer";
    const auto dllConsumer = dllRoot / "Consumer";
    writeProject(dllProducer, "DllProducer", "SharedLibrary", producerSource);
    output.clear();
    error.clear();
    const auto dll = dllProducer / "NativeMath.dll";
    tests.expect(emitLibrary(
            "--emit-project-oop-dll", dllProducer / "DllProducer.nsproj",
            dll, output, error, true) == ExitCode::Success,
        "0.9 precompiled DLL producer emits DLL, import .lib, and metadata");
    writeFile(dllConsumer / "Main.ns", consumerSource);
    writeFile(
        dllConsumer / "DllConsumer.nsproj",
        "<Project><PropertyGroup><Name>DllConsumer</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
            "Runtime=\"../Producer/NativeMath.dll\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    // Put the consumer beside the runtime DLL so the Windows loader resolves the exact product
    // that LibraryReference validated.
    const auto dllExecutable = dllProducer / "DllConsumer.exe";
    const ExitCode dllResult = emitLibrary(
        "--emit-project-oop-exe", dllConsumer / "DllConsumer.nsproj",
        dllExecutable, output, error, true);
    tests.expect(dllResult == ExitCode::Success,
        "consumer links a DLL import library from LibraryReference without --link-lib (" +
            error + ")");
    tests.expect(output.find("linked-libraries: 1") != std::string::npos,
        "DLL consumer reports one validated project library reference");
    PeExecutableVerifier consumerVerifier;
    tests.expect(dllResult == ExitCode::Success && consumerVerifier.verify(dllExecutable) &&
            contains(consumerVerifier.info().importedLibraries, "NativeMath.dll"),
        "consumer PE imports the declared NativeSharp runtime DLL by name");
#ifdef _WIN32
    if (dllResult == ExitCode::Success)
    {
        tests.expect(runExecutable(dllExecutable) == 42,
            "separate DLL producer/consumer executes through its import library");
    }
#endif


    const auto automaticDllConsumer = dllRoot / "AutomaticConsumer";
    writeFile(automaticDllConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        public static int Main() { return Precompiled.Math.Add(20); }
    }
}
)NS");
    writeFile(
        automaticDllConsumer / "AutomaticConsumer.nsproj",
        "<Project><PropertyGroup><Name>AutomaticDllConsumer</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
            "Runtime=\"../Producer/NativeMath.dll\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto automaticDllExecutable = dllProducer / "AutomaticConsumer.exe";
    const ExitCode automaticDllResult = emitLibrary(
        "--emit-project-oop-exe", automaticDllConsumer / "AutomaticConsumer.nsproj",
        automaticDllExecutable, output, error, true);
    tests.expect(automaticDllResult == ExitCode::Success,
        "metadata-backed DLL consumer resolves NativeSharp overloads without an extern C redeclaration (" +
            error + ")");
#ifdef _WIN32
    if (automaticDllResult == ExitCode::Success)
    {
        tests.expect(runExecutable(automaticDllExecutable) == 42,
            "metadata-backed DLL overload calls use the import library and execute to 42");
    }
#endif

    const auto wrongProducer = dllRoot / "WrongProducer";
    writeProject(wrongProducer, "WrongProducer", "SharedLibrary", R"NS(
public class Other
{
    public static export extern C int Add(int left, int right) { return left - right; }
}
)NS");
    output.clear();
    error.clear();
    const auto wrongDll = wrongProducer / "Wrong.dll";
    tests.expect(emitLibrary(
            "--emit-project-oop-dll", wrongProducer / "WrongProducer.nsproj",
            wrongDll, output, error, true) == ExitCode::Success,
        "mismatched-runtime negative fixture builds its own valid DLL");

    const auto wrongNativeArchiveConsumer = dllRoot / "WrongNativeArchiveConsumer";
    writeFile(wrongNativeArchiveConsumer / "Main.ns", R"NS(
public class Program
{
    public static int Main() { return Precompiled.Math.Blend(20); }
}
)NS");
    writeFile(
        wrongNativeArchiveConsumer / "WrongNativeArchiveConsumer.nsproj",
        "<Project><PropertyGroup><Name>WrongNativeArchiveConsumer</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../WrongProducer/Wrong.lib\" "
            "Metadata=\"../Producer/NativeMath.nsmeta\" Runtime=\"../WrongProducer/Wrong.dll\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary(
            "--emit-project-oop-exe",
            wrongNativeArchiveConsumer / "WrongNativeArchiveConsumer.nsproj",
            wrongNativeArchiveConsumer / "WrongNativeArchiveConsumer.exe",
            output,
            error,
            true) == ExitCode::CompilationError,
        "a same-shaped C import library cannot impersonate a NativeSharp module");
    tests.expect(error.find("does not contain NativeSharp symbol") != std::string::npos,
        "native archive/metadata mismatch reports the missing mangled symbol before link");

    const auto mismatchConsumer = dllRoot / "MismatchConsumer";
    writeFile(mismatchConsumer / "Main.ns", consumerSource);
    writeFile(
        mismatchConsumer / "MismatchConsumer.nsproj",
        "<Project><PropertyGroup><Name>MismatchConsumer</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
        "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
            "Metadata=\"../Producer/NativeMath.nsmeta\" Runtime=\"../WrongProducer/Wrong.dll\"/>"
        "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary(
            "--emit-project-oop-exe", mismatchConsumer / "MismatchConsumer.nsproj",
            mismatchConsumer / "MismatchConsumer.exe", output, error, true) ==
            ExitCode::CompilationError,
        "LibraryReference rejects a runtime DLL that does not match its metadata");
    tests.expect(error.find("runtime DLL export table does not match metadata contract") != std::string::npos ||
            error.find("linked PE does not import declared runtime DLL") != std::string::npos,
        "same-shaped wrong DLL is rejected by its NativeSharp export set or final PE import identity");
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
    testPrecompiledLibraryReferences(tests, root);
    testOutputKindGuards(tests, root);

    return tests.resultKeeping(root);
}
