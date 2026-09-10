#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Linker/CoffArchiveVerifier.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"
#include "nativesharp/LLVM/LlvmModule.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <cctype>
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
using nativesharp::interop::LibraryMetadata;
using nativesharp::interop::LibraryMetadataCodec;
using nativesharp::linker::CoffArchiveVerifier;
using nativesharp::linker::LldLinker;
using nativesharp::linker::PeExecutableVerifier;
using nativesharp::runtime::WindowsRuntimeBuilder;
using nativesharp::tests::TestContext;

std::string utf8Path(const std::filesystem::path& path)
{
    const std::u8string value = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

bool usesNsLinker()
{
    std::string value;
#ifdef _WIN32
    char* linker = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&linker, &length, "NSC_LLD_LINK") == 0 && linker != nullptr)
    {
        value.assign(linker);
        std::free(linker);
    }
#else
    if (const char* linker = std::getenv("NSC_LLD_LINK"); linker != nullptr) value.assign(linker);
#endif
    if (value.empty()) return false;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value.find("nslink") != std::string::npos;
}

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

void writeProject(const std::filesystem::path& directory,
                  std::string_view name,
                  std::string_view outputType,
                  std::string_view source)
{
    writeFile(directory / (std::string(name) + ".nsproj"),
              "<Project><PropertyGroup><Name>" + std::string(name) + "</Name><OutputType>" + std::string(outputType) +
                  "</OutputType></PropertyGroup><ItemGroup><Compile "
                  "Include=\"**/*.ns\" />"
                  "</ItemGroup></Project>");
    writeFile(directory / "Library.ns", source);
}

ExitCode emitLibrary(std::string_view action,
                     const std::filesystem::path& project,
                     const std::filesystem::path& outputPath,
                     std::string& output,
                     std::string& error,
                     bool optimize)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = utf8Path(project);
    const std::string outputText = utf8Path(outputPath);
    ExitCode result = ExitCode::CommandLineError;
    if (optimize)
    {
        const std::array<std::string_view, 5> args = { "-O2", action, projectText, "-o", outputText };
        result = driver.run(args, out, err);
    } else
    {
        const std::array<std::string_view, 4> args = { action, projectText, "-o", outputText };
        result = driver.run(args, out, err);
    }
    output = out.str();
    error = err.str();
    return result;
}

ExitCode
runProjectAction(std::string_view action, const std::filesystem::path& project, std::string& output, std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = project.string();
    const std::array<std::string_view, 2> args = { action, projectText };
    const ExitCode result = driver.run(args, out, err);
    output = out.str();
    error = err.str();
    return result;
}

ExitCode verifyLibraryArtifacts(const std::filesystem::path& metadata,
                                const std::filesystem::path& library,
                                const std::filesystem::path& runtime,
                                std::string& output,
                                std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string metadataText = metadata.string();
    const std::string libraryText = library.string();
    const std::string runtimeText = runtime.string();
    const std::array<std::string_view, 4> args = {
        "--verify-library-artifacts", metadataText, libraryText, runtimeText
    };
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
    return _wsystem((L"\"" + path.wstring() + L"\"").c_str());
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
                 "legacy OutputType Library remains accepted as static-library "
                 "compatible");
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
                 "DLL/static-library runtime support builds without an "
                 "executable entry contract");
    if (module == nullptr)
    {
        return;
    }
    const std::string text = module->text();
    tests.expect(text.find("@__nativesharp_start") == std::string::npos,
                 "library runtime support deliberately omits __nativesharp_start");
    tests.expect(text.find("@__nativesharp_alloc") != std::string::npos &&
                     text.find("@__nativesharp_free") != std::string::npos,
                 "library runtime support preserves NativeSharp "
                 "allocation/deallocation helpers");
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
    tests.expect(
        emitLibrary("--emit-project-oop-lib", directory / "StaticLibrary.nsproj", library, output, error, true) ==
            ExitCode::Success,
        "Stage 46 driver emits a native static .lib from a "
        "StaticLibrary project");
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
                     "static library contains archive metadata plus NativeSharp "
                     "program/runtime members");
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
    tests.expect(
        emitLibrary(
            "--emit-project-oop-lib", directory / "StaticLibrary.nsproj", unoptimizedLibrary, output, error, false) ==
            ExitCode::Success,
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
    tests.expect(emitLibrary("--emit-project-oop-dll", directory / "DynamicLibrary.nsproj", dll, output, error, true) ==
                     ExitCode::Success,
                 "Stage 46 driver emits a native PE DLL from a SharedLibrary project");
    tests.expect(error.empty(), "successful DLL generation keeps stderr empty");
    tests.expect(std::filesystem::is_regular_file(dll) && std::filesystem::file_size(dll) > 1024,
                 "DLL generation creates a non-trivial PE image");
    tests.expect(std::filesystem::is_regular_file(importLibrary) && std::filesystem::file_size(importLibrary) > 512,
                 "DLL generation creates the matching COFF import .lib");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verifyDynamicLibrary(dll), "PE verifier accepts the generated NativeSharp dynamic library");
    if (!verifier.verifyDynamicLibrary(dll))
    {
        return;
    }
    const auto& info = verifier.info();
    const std::string add = "Add";
    const std::string multiply = "Multiply";
    const std::string hidden = "Hidden";
    tests.expect(info.machine == 0x8664 && info.optionalHeaderMagic == 0x020b, "NativeSharp DLL is AMD64 PE32+");
    tests.expect((info.characteristics & 0x2000U) != 0, "NativeSharp dynamic library carries IMAGE_FILE_DLL");
    tests.expect(info.clrDirectoryRva == 0, "NativeSharp DLL contains no CLR runtime header");
    tests.expect(info.exportDirectoryRva != 0 && info.exportDirectorySize != 0,
                 "NativeSharp DLL materializes a PE export directory");
    tests.expect(contains(info.exportedSymbols, add) && contains(info.exportedSymbols, multiply),
                 "Stage 46 library product remains valid under the Stage 47 "
                 "stable C ABI export policy");
    tests.expect(!contains(info.exportedSymbols, hidden) &&
                     std::none_of(info.exportedSymbols.begin(),
                                  info.exportedSymbols.end(),
                                  [](const std::string& symbol) { return symbol.find("Hidden") != std::string::npos; }),
                 "Stage 46 does not export private static methods under plain or mangled "
                 "names");
    tests.expect(info.exportedSymbols.size() == 6 &&
                     std::ranges::any_of(info.exportedSymbols, [](const std::string& symbol)
                                         { return symbol.starts_with("__nsc_get_classdesc_"); }) &&
                     std::ranges::any_of(info.exportedSymbols, [](const std::string& symbol)
                                         { return symbol.starts_with("__nsc_ctor_"); }),
                 "DLL exports each public method plus the public class "
                 "descriptor and implicit constructor required "
                 "by precompiled object construction");
    tests.expect(info.exceptionDirectoryRva != 0,
                 "NativeSharp DLL retains Win64 unwind metadata from its native "
                 "runtime support");

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
    tests.expect(
        emitLibrary(
            "--emit-project-oop-dll", directory / "DynamicLibrary.nsproj", unoptimizedDll, output, error, false) ==
            ExitCode::Success,
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

void testPrecompiledDllRegressionCorpus(TestContext& tests, const std::filesystem::path& root)
{
    const auto corpusRoot = root / "PrecompiledDllRegressionCorpus";
    const auto genericProducer = corpusRoot / "GenericProducer";
    const auto typeProducer = corpusRoot / "TypeProducer";
    const auto products = corpusRoot / "Products";
    const auto consumer = corpusRoot / "Consumer";
    std::filesystem::create_directories(products);

    // DLL-P0 keeps the four external failures independent of any binding repository. The
    // producer combines an interface-only closed-generic implementation, a concrete generic
    // struct constructor, and a private native dependency that must not enter the imported API.
    writeProject(genericProducer, "DllP0Generics", "SharedLibrary", R"NS(
namespace DllP0
{
    public interface IValue
    {
        public int Get();
    }

    public class Box<T> : IValue
    {
        private T Stored;
        public Box(T value) { Stored = value; }
        public int Get() { return 42; }
    }

    public struct ValueBox<T>
    {
        private T Stored;
        public ValueBox(T value) { Stored = value; }
        public T Read() { return Stored; }
    }

    public struct Catalog<T>
    {
        public T Value;
    }

    public class NativeSupport
    {
        extern C
        {
            public void Copy(void* destination, void* source, long count);
        }
    }

    public class Api
    {
        private static int Read(IValue value) { return value.Get(); }

        public static int Run()
        {
            Box<int> box = new Box<int>(42);
            ValueBox<int> answer = new ValueBox<int>(Read(box));
            return answer.Read();
        }
    }
}
)NS");

    // These supported metadata declaration shapes complement the source-project reduction: the
    // consumer is the first project to specialize Catalog<T>, while T is declared by a different
    // producer. The Stage 52 regression retains the nested and transitive source cases.
    writeProject(typeProducer, "DllP0Types", "SharedLibrary", R"NS(
namespace DllP0.Types
{
    public interface IAsset { public int Kind(); }

    public class Texture
    {
        public Texture() { }
    }

    public struct Song
    {
        public int Id;
    }

    public class Shader : IAsset
    {
        public Shader() { }
        public int Kind() { return 2; }
    }

    public class Api
    {
        public static int Zero() { return 0; }
    }
}
)NS");

    std::string output;
    std::string error;
    const auto genericDll = products / "DllP0Generics.dll";
    const auto genericLibrary = products / "DllP0Generics.lib";
    const auto genericMetadata = products / "DllP0Generics.nsmeta";
    const ExitCode genericResult = emitLibrary(
        "--emit-project-oop-dll",
        genericProducer / "DllP0Generics.nsproj",
        genericDll,
        output,
        error,
        true);
    tests.expect(genericResult == ExitCode::Success,
                 "DLL-P0 generic producer emits DLL, import library, and NSMETA (" + error + ")");
    output.clear();
    error.clear();
    tests.expect(genericResult == ExitCode::Success &&
                     verifyLibraryArtifacts(genericMetadata, genericLibrary, genericDll, output, error) ==
                         ExitCode::Success,
                 "DLL-P0 generic producer passes joint DLL/import-library/NSMETA verification (" +
                     error + ")");

    const auto typeDll = products / "DllP0Types.dll";
    const auto typeLibrary = products / "DllP0Types.lib";
    const auto typeMetadata = products / "DllP0Types.nsmeta";
    output.clear();
    error.clear();
    const ExitCode typeResult = emitLibrary(
        "--emit-project-oop-dll",
        typeProducer / "DllP0Types.nsproj",
        typeDll,
        output,
        error,
        true);
    tests.expect(typeResult == ExitCode::Success,
                 "DLL-P0 external-type producer emits DLL, import library, and NSMETA (" + error + ")");
    output.clear();
    error.clear();
    tests.expect(typeResult == ExitCode::Success &&
                     verifyLibraryArtifacts(typeMetadata, typeLibrary, typeDll, output, error) == ExitCode::Success,
                 "DLL-P0 external-type producer passes joint artifact verification (" + error + ")");

    writeFile(consumer / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        DllP0.Catalog<DllP0.Types.Texture> textures;
        DllP0.Catalog<DllP0.Types.Song> songs;
        DllP0.Catalog<DllP0.Types.IAsset> assets;
        DllP0.Catalog<DllP0.Types.Shader> shaders;
        return DllP0.Api.Run() + DllP0.Types.Api.Zero();
    }
}
)NS");
    writeFile(consumer / "DllP0Consumer.nsproj",
              "<Project><PropertyGroup><Name>DllP0Consumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/DllP0Generics.lib\" "
              "Runtime=\"../Products/DllP0Generics.dll\"/>"
              "<LibraryReference Include=\"../Products/DllP0Types.lib\" "
              "Runtime=\"../Products/DllP0Types.dll\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto executable = products / "DllP0Consumer.exe";
    const ExitCode consumerResult = emitLibrary(
        "--emit-project-oop-exe", consumer / "DllP0Consumer.nsproj", executable, output, error, true);
    tests.expect(consumerResult == ExitCode::Success,
                 "DLL-P0 source-free consumer compiles against both verified metadata products (" + error + ")");
#ifdef _WIN32
    if (consumerResult == ExitCode::Success)
    {
        tests.expect(runExecutable(executable) == 42,
                     "DLL-P0 source-free consumer executes the interface-only generic body to 42");
    }
#endif
}

void testDllP4ImportedSignaturesAndEvents(TestContext& tests, const std::filesystem::path& root)
{
    const auto corpusRoot = root / "DllP4ImportedSignaturesAndEvents";
    const auto products = corpusRoot / "Products";
    const auto importedTypes = corpusRoot / "ImportedTypes";
    const auto importedApi = corpusRoot / "ImportedApi";
    const auto importedConsumer = corpusRoot / "ImportedConsumer";
    const auto eventProducer = corpusRoot / "EventProducer";
    const auto eventConsumer = corpusRoot / "EventConsumer";
    std::filesystem::create_directories(products);

    writeProject(importedTypes, "ImportedTypes", "SharedLibrary", R"NS(
namespace ImportedTypes
{
    public struct Vector
    {
        public float X;
        public float Y;
        public float Z;
    }

    public class TypesMarker
    {
        public static int Ready() { return 42; }
    }
}
)NS");
    writeProject(importedApi, "ImportedTypeApi", "SharedLibrary", R"NS(
namespace ImportedTypeApi
{
    using ImportedTypes;

    public class Vectors
    {
        public Vectors() { }
        public Vector EchoInstance(Vector value) { return value; }
        public static Vector EchoValue(Vector value) { return value; }
        public static Vector* Echo(Vector* value)
        {
            if (TypesMarker.Ready() != 42) { return null; }
            return value;
        }
    }
}
)NS");
    writeFile(importedApi / "ImportedTypeApi.nsproj",
              "<Project><PropertyGroup><Name>ImportedTypeApi</Name><OutputType>SharedLibrary</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/ImportedTypes.lib\" "
              "Metadata=\"../Products/ImportedTypes.nsmeta\" "
              "Runtime=\"../Products/ImportedTypes.dll\"/>"
              "</ItemGroup></Project>");
    writeFile(importedConsumer / "Main.ns", R"NS(
namespace ImportedTypeConsumer
{
    using ImportedTypeApi;
    using ImportedTypes;

    public class Program
    {
        public static int Main()
        {
            Vector value;
            value.X = 42.0f;
            value.Y = 0.0f;
            value.Z = 0.0f;
            raii Vectors vectors = new Vectors();
            Vector instance = vectors.EchoInstance(value);
            Vector direct = Vectors.EchoValue(instance);
            return (int)direct.X;
        }
    }
}
)NS");
    writeFile(importedConsumer / "ImportedTypeConsumer.nsproj",
              "<Project><PropertyGroup><Name>ImportedTypeConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/ImportedTypes.lib\" "
              "Metadata=\"../Products/ImportedTypes.nsmeta\" "
              "Runtime=\"../Products/ImportedTypes.dll\"/>"
              "<LibraryReference Include=\"../Products/ImportedTypeApi.lib\" "
              "Metadata=\"../Products/ImportedTypeApi.nsmeta\" "
              "Runtime=\"../Products/ImportedTypeApi.dll\"/>"
              "</ItemGroup></Project>");

    std::string output;
    std::string error;
    const auto typesDll = products / "ImportedTypes.dll";
    const auto apiDll = products / "ImportedTypeApi.dll";
    const ExitCode typesResult = emitLibrary(
        "--emit-project-oop-dll", importedTypes / "ImportedTypes.nsproj", typesDll, output, error, true);
    tests.expect(typesResult == ExitCode::Success,
                 "DLL-P4 imported-type producer emits its DLL triplet (" + error + ")");
    output.clear();
    error.clear();
    tests.expect(typesResult == ExitCode::Success &&
                     verifyLibraryArtifacts(products / "ImportedTypes.nsmeta",
                                            products / "ImportedTypes.lib",
                                            typesDll,
                                            output,
                                            error) == ExitCode::Success,
                 "DLL-P4 imported-type producer passes physical and public-source verification (" + error + ")");

    output.clear();
    error.clear();
    const ExitCode apiResult = emitLibrary(
        "--emit-project-oop-dll", importedApi / "ImportedTypeApi.nsproj", apiDll, output, error, true);
    tests.expect(apiResult == ExitCode::Success,
                 "DLL-P4 API producer accepts public static and instance signatures from another NSMETA (" +
                     error + ")");
    output.clear();
    error.clear();
    tests.expect(apiResult == ExitCode::Success &&
                     verifyLibraryArtifacts(products / "ImportedTypeApi.nsmeta",
                                            products / "ImportedTypeApi.lib",
                                            apiDll,
                                            output,
                                            error) == ExitCode::Success,
                 "DLL-P4 verifier expects the imported-type instance export and materializes its API (" + error +
                     ")");

    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const std::string level = optimize ? "O2" : "O0";
        const auto executable = products / ("ImportedTypeConsumer-" + level + ".exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            importedConsumer / "ImportedTypeConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     "DLL-P4 source-free imported-type consumer compiles at " + level + " (" + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "DLL-P4 imported-type instance consumer executes to 42 at " + level);
#endif
    }

    writeProject(eventProducer, "PrecompiledEvents", "SharedLibrary", R"NS(
namespace PrecompiledEvents
{
    public delegate void ChangedHandler(int value);

    public class Source
    {
        public event ChangedHandler Changed;

        public Source() { Changed += Ignore; }
        private static void Ignore(int value) { }
        public void Raise(int value) { Changed(value); }
    }
}
)NS");
    writeFile(eventConsumer / "Main.ns", R"NS(
using PrecompiledEvents;

namespace PrecompiledEventConsumer
{
    public class Program
    {
        private static int observed;
        private static void OnChanged(int value) { observed = value; }

        public static int Main()
        {
            Source source = new Source();
            source.Changed += OnChanged;
            source.Raise(42);
            return observed;
        }
    }
}
)NS");
    writeFile(eventConsumer / "PrecompiledEventConsumer.nsproj",
              "<Project><PropertyGroup><Name>PrecompiledEventConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/PrecompiledEvents.lib\" "
              "Metadata=\"../Products/PrecompiledEvents.nsmeta\" "
              "Runtime=\"../Products/PrecompiledEvents.dll\"/>"
              "</ItemGroup></Project>");

    output.clear();
    error.clear();
    const auto eventDll = products / "PrecompiledEvents.dll";
    const ExitCode eventResult = emitLibrary(
        "--emit-project-oop-dll", eventProducer / "PrecompiledEvents.nsproj", eventDll, output, error, true);
    tests.expect(eventResult == ExitCode::Success,
                 "DLL-P4 public-event producer emits its DLL triplet (" + error + ")");
    output.clear();
    error.clear();
    tests.expect(eventResult == ExitCode::Success &&
                     verifyLibraryArtifacts(products / "PrecompiledEvents.nsmeta",
                                            products / "PrecompiledEvents.lib",
                                            eventDll,
                                            output,
                                            error) == ExitCode::Success,
                 "DLL-P4 verifier parses the exact public API synthesized from event NSMETA (" + error + ")");

    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const std::string level = optimize ? "O2" : "O0";
        const auto executable = products / ("PrecompiledEventConsumer-" + level + ".exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            eventConsumer / "PrecompiledEventConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     "DLL-P4 source-free event consumer compiles at " + level + " (" + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "DLL-P4 precompiled event consumer executes to 42 at " + level);
#endif
    }
}

void testDllP4ImportedBaseContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto corpusRoot = root / "DllP4ImportedBaseContract";
    const auto products = corpusRoot / "Products";
    const auto baseProducer = corpusRoot / "Base";
    const auto derivedProducer = corpusRoot / "Derived";
    const auto consumer = corpusRoot / "Consumer";
    std::filesystem::create_directories(products);

    writeProject(baseProducer, "ImportedBase", "SharedLibrary", R"NS(
namespace ImportedBase
{
    public interface IScore
    {
        public int Score();
    }

    public struct Payload
    {
        public int Number;
    }

    public enum Mode : byte
    {
        Answer = 42
    }

    public delegate void ChangedHandler(int value);
    public delegate void BaseChangedHandler(int value);

    public class Base
    {
        public event BaseChangedHandler BaseChanged;
        public Base() { BaseChanged += Ignore; }
        private static void Ignore(int value) { }
        public void RaiseBaseChanged(int value) { BaseChanged(value); }
        public virtual int Value() { return 40; }
    }
}
)NS");
    writeProject(derivedProducer, "ImportedDerived", "SharedLibrary", R"NS(
using ImportedBase;

namespace ImportedDerived
{
    public interface IExtended : IScore
    {
        public int Extra();
    }

    public class Derived : Base, IExtended
    {
        public override int Value() { return 42; }
        public int Score() { return 42; }
        public int Extra() { return 42; }
    }

    public class ContractSurface
    {
        public Payload Stored;
        public event ChangedHandler Changed;

        public ContractSurface(Payload value)
        {
            Stored = value;
            Changed += Ignore;
        }

        private static void Ignore(int value) { }
        public Payload Echo(Payload value) { return value; }
        public Mode CurrentMode() { return Mode.Answer; }
        public void Raise(int value) { Changed(value); }
    }
}
)NS");
    writeFile(derivedProducer / "ImportedDerived.nsproj",
              "<Project><PropertyGroup><Name>ImportedDerived</Name><OutputType>SharedLibrary</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/ImportedBase.lib\" "
              "Metadata=\"../Products/ImportedBase.nsmeta\" "
              "Runtime=\"../Products/ImportedBase.dll\"/>"
              "</ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using ImportedBase;
using ImportedDerived;

namespace ImportedBaseConsumer
{
    public class Program
    {
        private static int observed;
        private static void OnChanged(int value) { observed = value; }

        public static int Main()
        {
            Derived value = new Derived();
            Base asBase = value;
            IScore asScore = value;
            IExtended extended = value;
            if (asBase.Value() != 42 || asScore.Score() != 42 || extended.Extra() != 42) { return 1; }
            value.BaseChanged += OnChanged;
            value.RaiseBaseChanged(42);
            if (observed != 42) { return 3; }

            Payload payload;
            payload.Number = 42;
            ContractSurface surface = new ContractSurface(payload);
            Payload echoed = surface.Echo(surface.Stored);
            if (echoed.Number != 42 || surface.CurrentMode() != Mode.Answer) { return 2; }
            surface.Changed += OnChanged;
            surface.Raise(42);
            return observed;
        }
    }
}
)NS");
    writeFile(consumer / "ImportedBaseConsumer.nsproj",
              "<Project><PropertyGroup><Name>ImportedBaseConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/ImportedBase.lib\" "
              "Metadata=\"../Products/ImportedBase.nsmeta\" "
              "Runtime=\"../Products/ImportedBase.dll\"/>"
              "<LibraryReference Include=\"../Products/ImportedDerived.lib\" "
              "Metadata=\"../Products/ImportedDerived.nsmeta\" "
              "Runtime=\"../Products/ImportedDerived.dll\"/>"
              "</ItemGroup></Project>");

    std::string output;
    std::string error;
    const auto baseDll = products / "ImportedBase.dll";
    const ExitCode baseResult = emitLibrary(
        "--emit-project-oop-dll", baseProducer / "ImportedBase.nsproj", baseDll, output, error, true);
    tests.expect(baseResult == ExitCode::Success,
                 "DLL-P4 imported-base producer emits its DLL triplet (" + error + ")");
    output.clear();
    error.clear();
    const ExitCode baseVerifyResult = baseResult == ExitCode::Success
        ? verifyLibraryArtifacts(products / "ImportedBase.nsmeta",
                                 products / "ImportedBase.lib",
                                 baseDll,
                                 output,
                                 error)
        : ExitCode::CompilationError;
    tests.expect(baseVerifyResult == ExitCode::Success,
                 "DLL-P4 imported-base producer passes artifact and public-source verification (" + error + ")");

    output.clear();
    error.clear();
    const auto derivedDll = products / "ImportedDerived.dll";
    const ExitCode derivedResult = emitLibrary(
        "--emit-project-oop-dll", derivedProducer / "ImportedDerived.nsproj", derivedDll, output, error, true);
    tests.expect(derivedResult == ExitCode::Success,
                 "DLL-P4 derived producer accepts a base class owned by peer NSMETA (" + error + ")");
    output.clear();
    error.clear();
    const ExitCode derivedVerifyResult = derivedResult == ExitCode::Success
        ? verifyLibraryArtifacts(products / "ImportedDerived.nsmeta",
                                 products / "ImportedDerived.lib",
                                 derivedDll,
                                 output,
                                 error)
        : ExitCode::CompilationError;
    tests.expect(derivedVerifyResult == ExitCode::Success,
                 "DLL-P4 verifier parses a derived public API with an external base (" + error + ")");

    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const std::string level = optimize ? "O2" : "O0";
        const auto executable = products / ("ImportedBaseConsumer-" + level + ".exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            consumer / "ImportedBaseConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     "DLL-P4 source-free imported-base consumer compiles at " + level + " (" + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "DLL-P4 imported-base dispatch executes to 42 at " + level);
#endif
    }
}

void testCompileOnlyMetadataReferenceDoesNotPromiseRuntime(TestContext& tests, const std::filesystem::path& root)
{
    const auto corpusRoot = root / "CompileOnlyMetadataReference";
    const auto products = corpusRoot / "Products";
    const auto typesProducer = corpusRoot / "TypesProducer";
    const auto apiProducer = corpusRoot / "ApiProducer";
    const auto consumer = corpusRoot / "Consumer";
    const auto falseRuntimeConsumer = corpusRoot / "FalseRuntimeConsumer";
    std::filesystem::create_directories(products);

    writeProject(typesProducer, "MetadataTypes", "SharedLibrary", R"NS(
namespace MetadataTypes
{
    public struct Token
    {
        public int Value;
    }

    public class PhysicalApi
    {
        public static int Answer() { return 42; }
    }
}
)NS");
    writeFile(apiProducer / "Library.ns", R"NS(
using MetadataTypes;

namespace MetadataOnlyApi
{
    public class Api
    {
        public static Token Echo(Token value) { return value; }
    }
}
)NS");
    writeFile(apiProducer / "MetadataOnlyApi.nsproj",
              "<Project><PropertyGroup><Name>MetadataOnlyApi</Name><OutputType>SharedLibrary</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/MetadataTypes.lib\" "
              "Metadata=\"../Products/MetadataTypes.nsmeta\"/>"
              "</ItemGroup></Project>");
    const std::string consumerSource = R"NS(
using MetadataTypes;
using MetadataOnlyApi;

public class Program
{
    public static int Main()
    {
        Token value;
        value.Value = 42;
        Token echoed = Api.Echo(value);
        return echoed.Value;
    }
}
)NS";
    writeFile(consumer / "Main.ns", consumerSource);
    writeFile(consumer / "Consumer.nsproj",
              "<Project><PropertyGroup><Name>MetadataOnlyConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/MetadataTypes.lib\" "
              "Metadata=\"../Products/MetadataTypes.nsmeta\"/>"
              "<LibraryReference Include=\"../Products/MetadataOnlyApi.lib\" "
              "Metadata=\"../Products/MetadataOnlyApi.nsmeta\" "
              "Runtime=\"../Products/MetadataOnlyApi.dll\"/>"
              "</ItemGroup></Project>");
    writeFile(falseRuntimeConsumer / "Main.ns", consumerSource);
    writeFile(falseRuntimeConsumer / "FalseRuntimeConsumer.nsproj",
              "<Project><PropertyGroup><Name>FalseRuntimeConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/MetadataTypes.lib\" "
              "Metadata=\"../Products/MetadataTypes.nsmeta\" "
              "Runtime=\"../Products/MetadataTypes.dll\"/>"
              "<LibraryReference Include=\"../Products/MetadataOnlyApi.lib\" "
              "Metadata=\"../Products/MetadataOnlyApi.nsmeta\" "
              "Runtime=\"../Products/MetadataOnlyApi.dll\"/>"
              "</ItemGroup></Project>");

    std::string output;
    std::string error;
    const auto typesDll = products / "MetadataTypes.dll";
    const ExitCode typesResult = emitLibrary(
        "--emit-project-oop-dll", typesProducer / "MetadataTypes.nsproj", typesDll, output, error, true);
    tests.expect(typesResult == ExitCode::Success,
                 "compile-only metadata fixture emits its independent DLL contract (" + error + ")");

    output.clear();
    error.clear();
    const auto apiDll = products / "MetadataOnlyApi.dll";
    const ExitCode apiResult = emitLibrary(
        "--emit-project-oop-dll", apiProducer / "MetadataOnlyApi.nsproj", apiDll, output, error, true);
    tests.expect(apiResult == ExitCode::Success,
                 "DLL producer may use a peer contract's types without promising its runtime (" + error + ")");
    PeExecutableVerifier apiVerifier;
    tests.expect(apiResult == ExitCode::Success && apiVerifier.verifyDynamicLibrary(apiDll) &&
                     !contains(apiVerifier.info().importedLibraries, "MetadataTypes.dll"),
                 "metadata-only type use does not invent a physical DLL import");
    output.clear();
    error.clear();
    tests.expect(apiResult == ExitCode::Success &&
                     verifyLibraryArtifacts(products / "MetadataOnlyApi.nsmeta",
                                            products / "MetadataOnlyApi.lib",
                                            apiDll,
                                            output,
                                            error) == ExitCode::Success,
                 "metadata-only producer triplet verifies without a false runtime edge (" + error + ")");

    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const std::string level = optimize ? "O2" : "O0";
        const auto executable = products / ("MetadataOnlyConsumer-" + level + ".exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            consumer / "Consumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     "consumer resolves the compile-only type contract at " + level + " (" + error + ")");
        PeExecutableVerifier consumerVerifier;
        tests.expect(result == ExitCode::Success && consumerVerifier.verify(executable) &&
                         contains(consumerVerifier.info().importedLibraries, "MetadataOnlyApi.dll") &&
                         !contains(consumerVerifier.info().importedLibraries, "MetadataTypes.dll"),
                     "consumer imports only the runtime that contributes a physical symbol at " + level);
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "compile-only metadata consumer executes to 42 at " + level);
#endif
    }

    output.clear();
    error.clear();
    tests.expect(emitLibrary("--emit-project-oop-exe",
                             falseRuntimeConsumer / "FalseRuntimeConsumer.nsproj",
                             products / "FalseRuntimeConsumer.exe",
                             output,
                             error,
                             true) == ExitCode::CompilationError,
                 "declaring a runtime with no direct or transitive PE import remains an error");
    tests.expect(error.find("linked PE does not import declared runtime DLL 'MetadataTypes.dll'") !=
                     std::string::npos,
                 "false runtime promise reports the exact unreferenced DLL");
}

void testDllP4ProtectedConsumerCallableContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto corpusRoot = root / "DllP4ProtectedConsumerCallable";
    const auto products = corpusRoot / "Products";
    const auto producer = corpusRoot / "Producer";
    const auto consumer = corpusRoot / "Consumer";
    std::filesystem::create_directories(products);

    writeProject(producer, "ProtectedVirtuals", "SharedLibrary", R"NS(
namespace ProtectedVirtuals
{
    public class Base
    {
        protected int Seed;
        protected Base(int seed) { Seed = seed; }
        protected virtual int Value() { return Seed; }
        protected int Plain() { return Seed + 1; }
        public int Read() { return Value(); }
    }

    public class Derived : Base
    {
        protected Derived() : base(40) { }
        protected override int Value() { return 42; }
    }
}
)NS");
    writeFile(consumer / "Main.ns", R"NS(
using ProtectedVirtuals;

namespace ProtectedVirtualConsumer
{
    public class Leaf : Derived
    {
        public Leaf() : base() { }
        public int ReadBaseOverride() { return base.Value(); }
        public int ReadProtectedPlain() { return Plain(); }
    }

    public class Program
    {
        public static int Main()
        {
            Leaf value = new Leaf();
            if (value.Read() != 42 || value.ReadBaseOverride() != 42 || value.ReadProtectedPlain() != 41)
            {
                return 1;
            }
            return 42;
        }
    }
}
)NS");
    writeFile(consumer / "ProtectedVirtualConsumer.nsproj",
              "<Project><PropertyGroup><Name>ProtectedVirtualConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/ProtectedVirtuals.lib\" "
              "Metadata=\"../Products/ProtectedVirtuals.nsmeta\" "
              "Runtime=\"../Products/ProtectedVirtuals.dll\"/>"
              "</ItemGroup></Project>");

    std::string output;
    std::string error;
    const auto runtime = products / "ProtectedVirtuals.dll";
    const ExitCode producerResult = emitLibrary(
        "--emit-project-oop-dll", producer / "ProtectedVirtuals.nsproj", runtime, output, error, true);
    tests.expect(producerResult == ExitCode::Success,
                 "DLL-P4 protected-callable producer emits public classes and protected entries (" + error + ")");

    output.clear();
    error.clear();
    const ExitCode verifyResult = producerResult == ExitCode::Success
        ? verifyLibraryArtifacts(products / "ProtectedVirtuals.nsmeta",
                                 products / "ProtectedVirtuals.lib",
                                 runtime,
                                 output,
                                 error)
        : ExitCode::CompilationError;
    tests.expect(verifyResult == ExitCode::Success,
                 "DLL-P4 verifier requires each concrete protected callable published by NSMETA (" + error + ")");

    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const std::string level = optimize ? "O2" : "O0";
        const auto executable = products / ("ProtectedVirtualConsumer-" + level + ".exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            consumer / "ProtectedVirtualConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     "DLL-P4 consumer can derive, chain protected construction, and call base/protected members at " +
                         level + " (" + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "DLL-P4 protected direct and virtual calls execute to 42 at " + level);
#endif
    }
}

void testDllP4ProtectedVirtualAccessors(TestContext& tests, const std::filesystem::path& root)
{
    const auto corpusRoot = root / "DllP4ProtectedVirtualAccessors";
    const auto products = corpusRoot / "Products";
    const auto producer = corpusRoot / "Producer";
    const auto consumer = corpusRoot / "Consumer";
    std::filesystem::create_directories(products);

    writeProject(producer, "ProtectedAccessors", "SharedLibrary", R"NS(
namespace ProtectedAccessors
{
    public abstract class AccessBase
    {
        protected abstract int Value { get; set; }
        protected abstract int this[int index] { get; set; }
        public int ReadProperty() { return Value; }
        public int ReadIndex(int index) { return this[index]; }
    }

    public class AccessDerived : AccessBase
    {
        private static int stored;
        public AccessDerived(int seed) { stored = seed; }
        protected override int Value
        {
            get { return stored; }
            set { stored = value; }
        }
        protected override int this[int index]
        {
            get { return stored + index; }
            set { stored = value - index; }
        }
    }

    public class AutoSurface
    {
        public int Automatic { get; set; }
        public AutoSurface(int value) { Automatic = value; }
    }
}
)NS");
    writeFile(consumer / "Main.ns", R"NS(
using ProtectedAccessors;

namespace ProtectedAccessorConsumer
{
    public class Leaf : AccessDerived
    {
        public Leaf() : base(40) { }
        public int GetProtected() { return Value; }
        public void SetProtected(int next) { Value = next; }
        public int GetProtectedIndex(int index) { return this[index]; }
        public void SetProtectedIndex(int index, int next) { this[index] = next; }
    }

    public class Program
    {
        public static int Main()
        {
            Leaf value = new Leaf();
            if (value.ReadProperty() != 40) { return 10; }
            if (value.GetProtected() != 40) { return 11; }
            if (value.ReadIndex(2) != 42) { return 12; }
            if (value.GetProtectedIndex(2) != 42) { return 13; }
            AutoSurface automatic = new AutoSurface(42);
            if (automatic.Automatic != 42) { return 14; }
            value.SetProtected(41);
            value.SetProtectedIndex(1, 43);
            return value.ReadProperty();
        }
    }
}
)NS");
    writeFile(consumer / "ProtectedAccessorConsumer.nsproj",
              "<Project><PropertyGroup><Name>ProtectedAccessorConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/ProtectedAccessors.lib\" "
              "Metadata=\"../Products/ProtectedAccessors.nsmeta\" "
              "Runtime=\"../Products/ProtectedAccessors.dll\"/>"
              "</ItemGroup></Project>");

    std::string output;
    std::string error;
    const auto runtime = products / "ProtectedAccessors.dll";
    const ExitCode producerResult = emitLibrary(
        "--emit-project-oop-dll", producer / "ProtectedAccessors.nsproj", runtime, output, error, true);
    tests.expect(producerResult == ExitCode::Success,
                 "DLL-P4 protected virtual property/indexer producer emits its DLL contract (" + error + ")");
    output.clear();
    error.clear();
    const ExitCode verifyResult = producerResult == ExitCode::Success
        ? verifyLibraryArtifacts(products / "ProtectedAccessors.nsmeta",
                                 products / "ProtectedAccessors.lib",
                                 runtime,
                                 output,
                                 error)
        : ExitCode::CompilationError;
    tests.expect(verifyResult == ExitCode::Success,
                 "DLL-P4 verifier reconstructs protected abstract and override accessors (" + error + ")");

    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const std::string level = optimize ? "O2" : "O0";
        const auto executable = products / ("ProtectedAccessorConsumer-" + level + ".exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            consumer / "ProtectedAccessorConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     "DLL-P4 consumer binds protected property/indexer accessors at " + level + " (" + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "DLL-P4 protected property/indexer dispatch executes to 42 at " + level);
#endif
    }
}

void testDllP4QualifiedBaseIdentity(TestContext& tests, const std::filesystem::path& root)
{
    const auto corpusRoot = root / "DllP4QualifiedBaseIdentity";
    const auto products = corpusRoot / "Products";
    const auto producerA = corpusRoot / "ProducerA";
    const auto producerB = corpusRoot / "ProducerB";
    const auto derived = corpusRoot / "Derived";
    const auto consumer = corpusRoot / "Consumer";
    std::filesystem::create_directories(products);

    writeProject(producerA, "ModuleA", "SharedLibrary", R"NS(
namespace A
{
    public class Base { public virtual int Value() { return 20; } }
}
)NS");
    writeProject(producerB, "ModuleB", "SharedLibrary", R"NS(
namespace B
{
    public class Base { public virtual int Value() { return 22; } }
}
)NS");
    writeFile(derived / "Library.ns", R"NS(
using A;
using B;
using ABase = A.Base;
using BBase = B.Base;

namespace QualifiedDerived
{
    public class FromA : ABase { public override int Value() { return 20; } }
    public class FromB : BBase { public override int Value() { return 22; } }
}
)NS");
    writeFile(derived / "QualifiedDerived.nsproj",
              "<Project><PropertyGroup><Name>QualifiedDerived</Name><OutputType>SharedLibrary</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/ModuleA.lib\" Metadata=\"../Products/ModuleA.nsmeta\" "
              "Runtime=\"../Products/ModuleA.dll\"/>"
              "<LibraryReference Include=\"../Products/ModuleB.lib\" Metadata=\"../Products/ModuleB.nsmeta\" "
              "Runtime=\"../Products/ModuleB.dll\"/>"
              "</ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using QualifiedDerived;

public class Program
{
    public static int Main()
    {
        FromA left = new FromA();
        FromB right = new FromB();
        return left.Value() + right.Value();
    }
}
)NS");
    writeFile(consumer / "QualifiedConsumer.nsproj",
              "<Project><PropertyGroup><Name>QualifiedConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/ModuleA.lib\" Metadata=\"../Products/ModuleA.nsmeta\" "
              "Runtime=\"../Products/ModuleA.dll\"/>"
              "<LibraryReference Include=\"../Products/ModuleB.lib\" Metadata=\"../Products/ModuleB.nsmeta\" "
              "Runtime=\"../Products/ModuleB.dll\"/>"
              "<LibraryReference Include=\"../Products/QualifiedDerived.lib\" "
              "Metadata=\"../Products/QualifiedDerived.nsmeta\" Runtime=\"../Products/QualifiedDerived.dll\"/>"
              "</ItemGroup></Project>");

    std::string output;
    std::string error;
    for (const auto& [name, project] : std::array<std::pair<std::string, std::filesystem::path>, 2>{
             std::pair<std::string, std::filesystem::path>{"ModuleA", producerA / "ModuleA.nsproj"},
             std::pair<std::string, std::filesystem::path>{"ModuleB", producerB / "ModuleB.nsproj"}})
    {
        output.clear();
        error.clear();
        tests.expect(emitLibrary("--emit-project-oop-dll", project, products / (name + ".dll"), output, error, true) ==
                         ExitCode::Success,
                     "DLL-P4 same-simple-name base producer " + name + " emits (" + error + ")");
    }
    output.clear();
    error.clear();
    const auto derivedRuntime = products / "QualifiedDerived.dll";
    const ExitCode derivedResult = emitLibrary(
        "--emit-project-oop-dll", derived / "QualifiedDerived.nsproj", derivedRuntime, output, error, true);
    tests.expect(derivedResult == ExitCode::Success,
                 "DLL-P4 producer preserves A.Base and B.Base as distinct qualified bases (" + error + ")");
    output.clear();
    error.clear();
    tests.expect(derivedResult == ExitCode::Success &&
                     verifyLibraryArtifacts(products / "QualifiedDerived.nsmeta",
                                            products / "QualifiedDerived.lib",
                                            derivedRuntime,
                                            output,
                                            error) == ExitCode::Success,
                 "DLL-P4 verifier preserves module/namespace identity for duplicate simple base names (" + error + ")");
    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const std::string level = optimize ? "O2" : "O0";
        const auto executable = products / ("QualifiedConsumer-" + level + ".exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            consumer / "QualifiedConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     "DLL-P4 consumer resolves duplicate simple base names at " + level + " (" + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "DLL-P4 qualified base identities execute independently at " + level);
#endif
    }
}

void testDllP4FoundationAndImportedCleanup(TestContext& tests, const std::filesystem::path& root)
{
    const auto corpusRoot = root / "DllP4FoundationAndImportedCleanup";
    const auto products = corpusRoot / "Products";
    const auto foundation = corpusRoot / "Foundation";
    const auto lifetime = corpusRoot / "Lifetime";
    const auto consumer = corpusRoot / "Consumer";
    std::filesystem::create_directories(products);

    writeProject(foundation, "Foundation", "SharedLibrary", R"NS(
namespace NativeSharp.Core
{
    public class Object { }
    public class Exception : Object { }
}
)NS");
    writeFile(lifetime / "Library.ns", R"NS(
using NativeSharp.Core;

namespace ImportedLifetime
{
    public class Problem : Exception { public Problem() { } }

    public class Lifetime
    {
        private static int destroyed;
        public static void Reset() { destroyed = 0; }
        public static void Record(int value) { destroyed += value; }
        public static int Destroyed() { return destroyed; }
    }

    public class Base
    {
        public Base() { }
        ~Base() { Lifetime.Record(1); }
    }

    public class Derived : Base
    {
        public Derived() : base() { }
        ~Derived() { Lifetime.Record(20); }
    }
}
)NS");
    writeFile(lifetime / "ImportedLifetime.nsproj",
              "<Project><PropertyGroup><Name>ImportedLifetime</Name><OutputType>SharedLibrary</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/Foundation.lib\" "
              "Metadata=\"../Products/Foundation.nsmeta\" Runtime=\"../Products/Foundation.dll\"/>"
              "</ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using NativeSharp.Core;
using ImportedLifetime;

public class Program
{
    private static void Normal()
    {
        raii Base value = new Derived();
    }

    private static void Exceptional()
    {
        raii Base value = new Derived();
        raii Problem problem = new Problem();
        throw problem;
    }

    public static int Main()
    {
        Lifetime.Reset();
        Normal();
        try { Exceptional(); }
        catch (Problem caught) { }
        return Lifetime.Destroyed();
    }
}
)NS");
    writeFile(consumer / "ImportedCleanupConsumer.nsproj",
              "<Project><PropertyGroup><Name>ImportedCleanupConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Products/Foundation.lib\" "
              "Metadata=\"../Products/Foundation.nsmeta\" Runtime=\"../Products/Foundation.dll\"/>"
              "<LibraryReference Include=\"../Products/ImportedLifetime.lib\" "
              "Metadata=\"../Products/ImportedLifetime.nsmeta\" Runtime=\"../Products/ImportedLifetime.dll\"/>"
              "</ItemGroup></Project>");

    std::string output;
    std::string error;
    const auto foundationDll = products / "Foundation.dll";
    const ExitCode foundationResult = emitLibrary(
        "--emit-project-oop-dll", foundation / "Foundation.nsproj", foundationDll, output, error, true);
    tests.expect(foundationResult == ExitCode::Success,
                 "DLL-P4 Foundation emits the imported Object root contract (" + error + ")");
    output.clear();
    error.clear();
    tests.expect(foundationResult == ExitCode::Success &&
                     verifyLibraryArtifacts(products / "Foundation.nsmeta",
                                            products / "Foundation.lib",
                                            foundationDll,
                                            output,
                                            error) == ExitCode::Success,
                 "DLL-P4 Foundation Object root passes public-source verification (" + error + ")");

    output.clear();
    error.clear();
    const auto lifetimeDll = products / "ImportedLifetime.dll";
    const ExitCode lifetimeResult = emitLibrary(
        "--emit-project-oop-dll", lifetime / "ImportedLifetime.nsproj", lifetimeDll, output, error, true);
    tests.expect(lifetimeResult == ExitCode::Success,
                 "DLL-P4 dependent DLL implicitly inherits NativeSharp.Core.Object from Foundation (" + error + ")");
    output.clear();
    error.clear();
    tests.expect(lifetimeResult == ExitCode::Success &&
                     verifyLibraryArtifacts(products / "ImportedLifetime.nsmeta",
                                            products / "ImportedLifetime.lib",
                                            lifetimeDll,
                                            output,
                                            error) == ExitCode::Success,
                 "DLL-P4 verifier reconstructs the Foundation-rooted inheritance and destruction entries (" +
                     error + ")");

    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const std::string level = optimize ? "O2" : "O0";
        const auto executable = products / ("ImportedCleanupConsumer-" + level + ".exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            consumer / "ImportedCleanupConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     "DLL-P4 consumer preserves imported-base cleanup on normal and exceptional exits at " +
                         level + " (" + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "DLL-P4 imported-base destruction executes twice to 42 at " + level);
#endif
    }
}

void testDllP4PrivateStagingSurface(TestContext& tests, const std::filesystem::path& root)
{
    const auto corpusRoot = root / "DllP4PrivateStagingSurface";
    const auto products = corpusRoot / "Products";
    const auto producer = corpusRoot / "Producer";
    const auto validConsumer = corpusRoot / "ValidConsumer";
    const auto stagingConsumer = corpusRoot / "StagingConsumer";
    const auto nativeImportConsumer = corpusRoot / "NativeImportConsumer";
    std::filesystem::create_directories(products);

    writeProject(producer, "CleanSurface", "SharedLibrary", R"NS(
namespace CleanSurface
{
    internal class __NsbNative_Helper
    {
        extern C from "kernel32.dll"
        {
            public static uint GetCurrentProcessId();
        }

        public static int Secret() { return 1; }
    }

    public class Api
    {
        public static int Answer() { return 42; }
    }
}
)NS");
    const std::string reference =
        "<LibraryReference Include=\"../Products/CleanSurface.lib\" "
        "Metadata=\"../Products/CleanSurface.nsmeta\" Runtime=\"../Products/CleanSurface.dll\"/>";
    writeFile(validConsumer / "Main.ns", R"NS(
using CleanSurface;
public class Program { public static int Main() { return Api.Answer(); } }
)NS");
    writeFile(validConsumer / "ValidConsumer.nsproj",
              "<Project><PropertyGroup><Name>ValidConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>" + reference +
                  "</ItemGroup></Project>");
    writeFile(stagingConsumer / "Main.ns", R"NS(
using CleanSurface;
public class Program { public static int Main() { return __NsbNative_Helper.Secret(); } }
)NS");
    writeFile(stagingConsumer / "StagingConsumer.nsproj",
              "<Project><PropertyGroup><Name>StagingConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>" + reference +
                  "</ItemGroup></Project>");
    writeFile(nativeImportConsumer / "Main.ns", R"NS(
using CleanSurface;
public class Program { public static int Main() { return (int)Api.GetCurrentProcessId(); } }
)NS");
    writeFile(nativeImportConsumer / "NativeImportConsumer.nsproj",
              "<Project><PropertyGroup><Name>NativeImportConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>" + reference +
                  "</ItemGroup></Project>");

    std::string output;
    std::string error;
    const auto runtime = products / "CleanSurface.dll";
    const ExitCode producerResult = emitLibrary(
        "--emit-project-oop-dll", producer / "CleanSurface.nsproj", runtime, output, error, true);
    tests.expect(producerResult == ExitCode::Success,
                 "DLL-P4 producer accepts private native imports and internal staging helpers (" + error + ")");
    output.clear();
    error.clear();
    tests.expect(producerResult == ExitCode::Success &&
                     verifyLibraryArtifacts(products / "CleanSurface.nsmeta",
                                            products / "CleanSurface.lib",
                                            runtime,
                                            output,
                                            error) == ExitCode::Success,
                 "DLL-P4 materialized API excludes private native imports and staging types (" + error + ")");

    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const std::string level = optimize ? "O2" : "O0";
        const auto executable = products / ("CleanSurfaceConsumer-" + level + ".exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            validConsumer / "ValidConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     "DLL-P4 clean public surface remains consumable at " + level + " (" + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "DLL-P4 clean public surface executes to 42 at " + level);
#endif
    }

    output.clear();
    error.clear();
    tests.expect(runProjectAction("--check-project-method-bodies",
                                  stagingConsumer / "StagingConsumer.nsproj",
                                  output,
                                  error) == ExitCode::CompilationError,
                 "DLL-P4 consumer cannot name an internal __NsbNative staging type");
    output.clear();
    error.clear();
    tests.expect(runProjectAction("--check-project-method-bodies",
                                  nativeImportConsumer / "NativeImportConsumer.nsproj",
                                  output,
                                  error) == ExitCode::CompilationError,
                 "DLL-P4 consumer cannot call a producer-private extern C import");
}

void testPrecompiledStructConstructorContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto contractRoot = root / "PrecompiledStructConstructor";
    const auto producer = contractRoot / "Producer";
    const auto products = contractRoot / "Products";

    for (const bool dynamic : {false, true})
    {
        writeProject(producer,
                     "StructConstructor",
                     dynamic ? "SharedLibrary" : "StaticLibrary",
                     R"NS(
namespace SurfaceParity
{
    public struct Vector3
    {
        public int X;
        public int Y;
        public int Z;

        public Vector3(int x, int y, int z)
        {
            X = x;
            Y = y;
            Z = z;
        }
    }
}
)NS");
        for (const bool optimize : {false, true})
        {
            const std::string productName = std::string(dynamic ? "Dll" : "Static") +
                (optimize ? "O2" : "O0");
            const auto productDirectory = products / productName;
            std::filesystem::create_directories(productDirectory);
            const auto library = productDirectory / "StructConstructor.lib";
            const auto runtime = productDirectory / "StructConstructor.dll";
            std::string output;
            std::string error;
            const ExitCode producerResult = emitLibrary(dynamic ? "--emit-project-oop-dll" : "--emit-project-oop-lib",
                                                         producer / "StructConstructor.nsproj",
                                                         dynamic ? runtime : library,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(producerResult == ExitCode::Success,
                         productName + " producer preserves the public struct constructor (" + error + ")");
            if (producerResult != ExitCode::Success) continue;

            const auto metadata = productDirectory / "StructConstructor.nsmeta";
            output.clear();
            error.clear();
            tests.expect(verifyLibraryArtifacts(metadata,
                                                library,
                                                dynamic ? runtime : std::filesystem::path(),
                                                output,
                                                error) == ExitCode::Success,
                         productName + " struct-constructor artifacts verify as one contract (" + error + ")");

            const auto consumer = contractRoot / ("Consumer" + productName);
            writeFile(consumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static int Main()
    {
        Vector3 value = new Vector3(10, 11, 21);
        return value.X + value.Y + value.Z;
    }
}
)NS");
            const std::string runtimeAttribute = dynamic
                ? " Runtime=\"../Products/" + productName + "/StructConstructor.dll\""
                : std::string();
            writeFile(consumer / "Consumer.nsproj",
                      "<Project><PropertyGroup><Name>StructConstructorConsumer</Name>"
                      "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                      "<Compile Include=\"**/*.ns\"/>"
                      "<LibraryReference Include=\"../Products/" + productName +
                          "/StructConstructor.lib\"" + runtimeAttribute + "/>"
                      "</ItemGroup></Project>");
            output.clear();
            error.clear();
            const auto executable = consumer / "Consumer.exe";
            const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                         consumer / "Consumer.nsproj",
                                                         executable,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(consumerResult == ExitCode::Success,
                         productName + " consumer materializes new Vector3(x, y, z) from NSMETA (" + error + ")");
#ifdef _WIN32
            if (consumerResult == ExitCode::Success)
                tests.expect(runExecutable(executable) == 42,
                             productName + " precompiled struct constructor executes to 42");
#endif
        }
    }
}

void testPrecompiledNumericConstantContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto contractRoot = root / "PrecompiledNumericConstants";
    const auto producer = contractRoot / "Producer";
    const auto products = contractRoot / "Products";
    const std::string producerSource = R"NS(
namespace SurfaceParity
{
    public class Constants
    {
        public const int Signed = 44;
        public const int Negative = -4;
        public const uint Unsigned = 2u;
        public const bool Enabled = true;
        public const float Left = 44.0f;
        public const double Right = -2.0;
        public static int Anchor() { return 0; }
    }
}
)NS";

    for (const bool dynamic : {false, true})
    {
        writeProject(producer,
                     "NumericConstants",
                     dynamic ? "SharedLibrary" : "StaticLibrary",
                     producerSource);
        for (const bool optimize : {false, true})
        {
            const std::string productName = std::string(dynamic ? "Dll" : "Static") +
                (optimize ? "O2" : "O0");
            const auto productDirectory = products / productName;
            std::filesystem::create_directories(productDirectory);
            const auto library = productDirectory / "NumericConstants.lib";
            const auto runtime = productDirectory / "NumericConstants.dll";
            std::string output;
            std::string error;
            const ExitCode producerResult = emitLibrary(dynamic ? "--emit-project-oop-dll" : "--emit-project-oop-lib",
                                                         producer / "NumericConstants.nsproj",
                                                         dynamic ? runtime : library,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(producerResult == ExitCode::Success,
                         productName + " producer records numeric constants (" + error + ")");
            if (producerResult != ExitCode::Success) continue;

            output.clear();
            error.clear();
            tests.expect(verifyLibraryArtifacts(productDirectory / "NumericConstants.nsmeta",
                                                library,
                                                dynamic ? runtime : std::filesystem::path(),
                                                output,
                                                error) == ExitCode::Success,
                         productName + " numeric-constant artifacts verify as one contract (" + error + ")");

            const auto consumer = contractRoot / ("Consumer" + productName);
            writeFile(consumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static int Main()
    {
        if (!Constants.Enabled) { return 1; }
        int integral = Constants.Signed + Constants.Negative + (int)Constants.Unsigned;
        int floating = (int)Constants.Left + (int)Constants.Right;
        return integral == 42 && floating == 42 ? 42 + Constants.Anchor() : 2;
    }
}
)NS");
            const std::string runtimeAttribute = dynamic
                ? " Runtime=\"../Products/" + productName + "/NumericConstants.dll\""
                : std::string();
            writeFile(consumer / "Consumer.nsproj",
                      "<Project><PropertyGroup><Name>NumericConstantConsumer</Name>"
                      "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                      "<Compile Include=\"**/*.ns\"/>"
                      "<LibraryReference Include=\"../Products/" + productName +
                          "/NumericConstants.lib\"" + runtimeAttribute + "/>"
                      "</ItemGroup></Project>");
            output.clear();
            error.clear();
            const auto executable = consumer / "Consumer.exe";
            const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                         consumer / "Consumer.nsproj",
                                                         executable,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(consumerResult == ExitCode::Success,
                         productName + " consumer materializes numeric constants from NSMETA (" + error + ")");
#ifdef _WIN32
            if (consumerResult == ExitCode::Success)
                tests.expect(runExecutable(executable) == 42,
                             productName + " precompiled numeric constants execute to 42");
#endif
        }
    }
}

void testPrecompiledStringConstantContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto contractRoot = root / "PrecompiledStringConstant";
    const auto producer = contractRoot / "Producer";
    const auto products = contractRoot / "Products";
    const std::string producerSource = R"NS(
namespace SurfaceParity
{
    public class TextContract
    {
        public const string Value = "NativeSharp";
        public static int Anchor() { return 0; }
    }
}
)NS";

    for (const bool dynamic : {false, true})
    {
        writeProject(producer,
                     "StringConstant",
                     dynamic ? "SharedLibrary" : "StaticLibrary",
                     producerSource);
        for (const bool optimize : {false, true})
        {
            const std::string productName = std::string(dynamic ? "Dll" : "Static") +
                (optimize ? "O2" : "O0");
            const auto productDirectory = products / productName;
            std::filesystem::create_directories(productDirectory);
            const auto library = productDirectory / "StringConstant.lib";
            const auto runtime = productDirectory / "StringConstant.dll";
            std::string output;
            std::string error;
            const ExitCode producerResult = emitLibrary(dynamic ? "--emit-project-oop-dll" : "--emit-project-oop-lib",
                                                         producer / "StringConstant.nsproj",
                                                         dynamic ? runtime : library,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(producerResult == ExitCode::Success,
                         productName + " producer records the string constant payload (" + error + ")");
            if (producerResult != ExitCode::Success) continue;

            output.clear();
            error.clear();
            tests.expect(verifyLibraryArtifacts(productDirectory / "StringConstant.nsmeta",
                                                library,
                                                dynamic ? runtime : std::filesystem::path(),
                                                output,
                                                error) == ExitCode::Success,
                         productName + " string-constant artifacts verify as one contract (" + error + ")");

            const auto consumer = contractRoot / ("Consumer" + productName);
            writeFile(consumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static int Main()
    {
        return TextContract.Value == "NativeSharp" ? 42 + TextContract.Anchor() : 1;
    }
}
)NS");
            const std::string runtimeAttribute = dynamic
                ? " Runtime=\"../Products/" + productName + "/StringConstant.dll\""
                : std::string();
            writeFile(consumer / "Consumer.nsproj",
                      "<Project><PropertyGroup><Name>StringConstantConsumer</Name>"
                      "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                      "<Compile Include=\"**/*.ns\"/>"
                      "<LibraryReference Include=\"../Products/" + productName +
                          "/StringConstant.lib\"" + runtimeAttribute + "/>"
                      "</ItemGroup></Project>");
            output.clear();
            error.clear();
            const auto executable = consumer / "Consumer.exe";
            const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                         consumer / "Consumer.nsproj",
                                                         executable,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(consumerResult == ExitCode::Success,
                         productName + " consumer materializes the string constant from NSMETA (" + error + ")");
#ifdef _WIN32
            if (consumerResult == ExitCode::Success)
                tests.expect(runExecutable(executable) == 42,
                             productName + " precompiled string constant executes to 42");
#endif
        }
    }
}

void testPrecompiledNewVirtualSlotContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto contractRoot = root / "PrecompiledNewVirtualSlot";
    const auto producer = contractRoot / "Producer";
    const auto products = contractRoot / "Products";
    const std::string producerSource = R"NS(
namespace SurfaceParity
{
    public class Base
    {
        public virtual int Value() { return 20; }
    }

    public class Derived : Base
    {
        public new virtual int Value() { return 42; }
    }
}
)NS";

    for (const bool dynamic : {false, true})
    {
        writeProject(producer,
                     "NewVirtualSlot",
                     dynamic ? "SharedLibrary" : "StaticLibrary",
                     producerSource);
        for (const bool optimize : {false, true})
        {
            const std::string productName = std::string(dynamic ? "Dll" : "Static") +
                (optimize ? "O2" : "O0");
            const auto productDirectory = products / productName;
            std::filesystem::create_directories(productDirectory);
            const auto library = productDirectory / "NewVirtualSlot.lib";
            const auto runtime = productDirectory / "NewVirtualSlot.dll";
            std::string output;
            std::string error;
            const ExitCode producerResult = emitLibrary(dynamic ? "--emit-project-oop-dll" : "--emit-project-oop-lib",
                                                         producer / "NewVirtualSlot.nsproj",
                                                         dynamic ? runtime : library,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(producerResult == ExitCode::Success,
                         productName + " producer records the independent virtual slot (" + error + ")");
            if (producerResult != ExitCode::Success) continue;

            output.clear();
            error.clear();
            tests.expect(verifyLibraryArtifacts(productDirectory / "NewVirtualSlot.nsmeta",
                                                library,
                                                dynamic ? runtime : std::filesystem::path(),
                                                output,
                                                error) == ExitCode::Success,
                         productName + " new-virtual artifacts verify as one contract (" + error + ")");

            const auto consumer = contractRoot / ("Consumer" + productName);
            writeFile(consumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static int Main()
    {
        raii Derived derived = new Derived();
        borrowed Base baseValue = derived;
        return derived.Value() + baseValue.Value() - 20;
    }
}
)NS");
            const std::string runtimeAttribute = dynamic
                ? " Runtime=\"../Products/" + productName + "/NewVirtualSlot.dll\""
                : std::string();
            writeFile(consumer / "Consumer.nsproj",
                      "<Project><PropertyGroup><Name>NewVirtualSlotConsumer</Name>"
                      "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                      "<Compile Include=\"**/*.ns\"/>"
                      "<LibraryReference Include=\"../Products/" + productName +
                          "/NewVirtualSlot.lib\"" + runtimeAttribute + "/>"
                      "</ItemGroup></Project>");
            output.clear();
            error.clear();
            const auto executable = consumer / "Consumer.exe";
            const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                         consumer / "Consumer.nsproj",
                                                         executable,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(consumerResult == ExitCode::Success,
                         productName + " consumer preserves new virtual dispatch from NSMETA (" + error + ")");
#ifdef _WIN32
            if (consumerResult == ExitCode::Success)
                tests.expect(runExecutable(executable) == 42,
                             productName + " precompiled new virtual slot executes to 42");
#endif
        }
    }
}

void testPrecompiledTypeSemanticFlags(TestContext& tests, const std::filesystem::path& root)
{
    const auto contractRoot = root / "PrecompiledTypeSemanticFlags";
    const auto producer = contractRoot / "Producer";
    const auto products = contractRoot / "Products";
    const std::string producerSource = R"NS(
namespace SurfaceParity
{
    public borrowed struct Slice
    {
        public int* Start;
        public int Count;
    }

    public class Views
    {
        public static Slice FromPointer(int* source, int count)
        {
            Slice result;
            result.Start = source;
            result.Count = count;
            return result;
        }
    }

    public static class Utilities
    {
        public static int Answer() { return 42; }
    }
}
)NS";

    for (const bool dynamic : {false, true})
    {
        writeProject(producer,
                     "TypeSemanticFlags",
                     dynamic ? "SharedLibrary" : "StaticLibrary",
                     producerSource);
        for (const bool optimize : {false, true})
        {
            const std::string productName = std::string(dynamic ? "Dll" : "Static") +
                (optimize ? "O2" : "O0");
            const auto productDirectory = products / productName;
            std::filesystem::create_directories(productDirectory);
            const auto library = productDirectory / "TypeSemanticFlags.lib";
            const auto runtime = productDirectory / "TypeSemanticFlags.dll";
            std::string output;
            std::string error;
            const ExitCode producerResult = emitLibrary(dynamic ? "--emit-project-oop-dll" : "--emit-project-oop-lib",
                                                         producer / "TypeSemanticFlags.nsproj",
                                                         dynamic ? runtime : library,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(producerResult == ExitCode::Success,
                         productName + " producer records borrowed/static type semantics (" + error + ")");
            if (producerResult != ExitCode::Success) continue;

            output.clear();
            error.clear();
            tests.expect(verifyLibraryArtifacts(productDirectory / "TypeSemanticFlags.nsmeta",
                                                library,
                                                dynamic ? runtime : std::filesystem::path(),
                                                output,
                                                error) == ExitCode::Success,
                         productName + " semantic-type artifacts verify as one contract (" + error + ")");

            const std::string runtimeAttribute = dynamic
                ? " Runtime=\"../Products/" + productName + "/TypeSemanticFlags.dll\""
                : std::string();
            const auto writeConsumerProject = [&](const std::filesystem::path& directory,
                                                  const std::string& name) {
                writeFile(directory / "Consumer.nsproj",
                          "<Project><PropertyGroup><Name>" + name + "</Name>"
                          "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                          "<Compile Include=\"**/*.ns\"/>"
                          "<LibraryReference Include=\"../Products/" + productName +
                              "/TypeSemanticFlags.lib\"" + runtimeAttribute + "/>"
                          "</ItemGroup></Project>");
            };

            const auto borrowedConsumer = contractRoot / ("BorrowedConsumer" + productName);
            writeFile(borrowedConsumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static Slice Leak()
    {
        int[] values = new int[1];
        return Views.FromPointer(&values[0], 1);
    }
    public static int Main() { return 42; }
}
)NS");
            writeConsumerProject(borrowedConsumer, "BorrowedEscapeConsumer");
            output.clear();
            error.clear();
            tests.expect(emitLibrary("--emit-project-oop-exe",
                                     borrowedConsumer / "Consumer.nsproj",
                                     borrowedConsumer / "Consumer.exe",
                                     output,
                                     error,
                                     optimize) == ExitCode::CompilationError,
                         productName + " consumer rejects escape of an imported borrowed struct");

            const auto staticConsumer = contractRoot / ("StaticConsumer" + productName);
            writeFile(staticConsumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static int Main()
    {
        Utilities invalid = new Utilities();
        return Utilities.Answer();
    }
}
)NS");
            writeConsumerProject(staticConsumer, "StaticInstantiationConsumer");
            output.clear();
            error.clear();
            tests.expect(emitLibrary("--emit-project-oop-exe",
                                     staticConsumer / "Consumer.nsproj",
                                     staticConsumer / "Consumer.exe",
                                     output,
                                     error,
                                     optimize) == ExitCode::CompilationError,
                         productName + " consumer rejects construction of an imported static class");
        }
    }
}

void testPrecompiledOperatorsAndConversions(TestContext& tests, const std::filesystem::path& root)
{
    const auto contractRoot = root / "PrecompiledOperatorsAndConversions";
    const auto producer = contractRoot / "Producer";
    const auto products = contractRoot / "Products";
    const std::string producerSource = R"NS(
namespace SurfaceParity
{
    public struct Source
    {
        public int Number;
        public static Source Create(int number)
        {
            Source result;
            result.Number = number;
            return result;
        }
        public static Source operator +(Source left, Source right)
        {
            return Create(left.Number + right.Number);
        }
    }

    public struct Target
    {
        public int Number;
        public static implicit operator Target(Source value)
        {
            Target result;
            result.Number = value.Number;
            return result;
        }
    }
}
)NS";

    for (const bool dynamic : {false, true})
    {
        writeProject(producer,
                     "OperatorsAndConversions",
                     dynamic ? "SharedLibrary" : "StaticLibrary",
                     producerSource);
        for (const bool optimize : {false, true})
        {
            const std::string productName = std::string(dynamic ? "Dll" : "Static") +
                (optimize ? "O2" : "O0");
            const auto productDirectory = products / productName;
            std::filesystem::create_directories(productDirectory);
            const auto library = productDirectory / "OperatorsAndConversions.lib";
            const auto runtime = productDirectory / "OperatorsAndConversions.dll";
            std::string output;
            std::string error;
            const ExitCode producerResult = emitLibrary(dynamic ? "--emit-project-oop-dll" : "--emit-project-oop-lib",
                                                         producer / "OperatorsAndConversions.nsproj",
                                                         dynamic ? runtime : library,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(producerResult == ExitCode::Success,
                         productName + " producer publishes operators and conversions (" + error + ")");
            if (producerResult != ExitCode::Success) continue;

            output.clear();
            error.clear();
            tests.expect(verifyLibraryArtifacts(productDirectory / "OperatorsAndConversions.nsmeta",
                                                library,
                                                dynamic ? runtime : std::filesystem::path(),
                                                output,
                                                error) == ExitCode::Success,
                         productName + " operator artifacts verify as one contract (" + error + ")");

            const auto consumer = contractRoot / ("Consumer" + productName);
            writeFile(consumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static int Main()
    {
        Source left = Source.Create(20);
        Source right = Source.Create(22);
        Source sum = left + right;
        Target converted = sum;
        return converted.Number;
    }
}
)NS");
            const std::string runtimeAttribute = dynamic
                ? " Runtime=\"../Products/" + productName + "/OperatorsAndConversions.dll\""
                : std::string();
            writeFile(consumer / "Consumer.nsproj",
                      "<Project><PropertyGroup><Name>OperatorConsumer</Name>"
                      "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                      "<Compile Include=\"**/*.ns\"/>"
                      "<LibraryReference Include=\"../Products/" + productName +
                          "/OperatorsAndConversions.lib\"" + runtimeAttribute + "/>"
                      "</ItemGroup></Project>");
            output.clear();
            error.clear();
            const auto executable = consumer / "Consumer.exe";
            const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                         consumer / "Consumer.nsproj",
                                                         executable,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(consumerResult == ExitCode::Success,
                         productName + " consumer materializes operator and conversion syntax (" + error + ")");
#ifdef _WIN32
            if (consumerResult == ExitCode::Success)
                tests.expect(runExecutable(executable) == 42,
                             productName + " precompiled operator and conversion execute to 42");
#endif
        }
    }
}

void testPrecompiledMutableStaticFieldContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto contractRoot = root / "PrecompiledMutableStaticField";
    const auto producer = contractRoot / "Producer";
    const auto products = contractRoot / "Products";
    const std::string producerSource = R"NS(
namespace SurfaceParity
{
    public class State
    {
        public static int Value = 40;
    }
}
)NS";

    for (const bool dynamic : {false, true})
    {
        writeProject(producer,
                     "MutableStaticField",
                     dynamic ? "SharedLibrary" : "StaticLibrary",
                     producerSource);
        for (const bool optimize : {false, true})
        {
            const std::string productName = std::string(dynamic ? "Dll" : "Static") +
                (optimize ? "O2" : "O0");
            const auto productDirectory = products / productName;
            std::filesystem::create_directories(productDirectory);
            const auto library = productDirectory / "MutableStaticField.lib";
            const auto runtime = productDirectory / "MutableStaticField.dll";
            std::string output;
            std::string error;
            const ExitCode producerResult = emitLibrary(dynamic ? "--emit-project-oop-dll" : "--emit-project-oop-lib",
                                                         producer / "MutableStaticField.nsproj",
                                                         dynamic ? runtime : library,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(producerResult == ExitCode::Success,
                         productName + " producer publishes mutable static storage accessor (" + error + ")");
            if (producerResult != ExitCode::Success) continue;

            output.clear();
            error.clear();
            tests.expect(verifyLibraryArtifacts(productDirectory / "MutableStaticField.nsmeta",
                                                library,
                                                dynamic ? runtime : std::filesystem::path(),
                                                output,
                                                error) == ExitCode::Success,
                         productName + " mutable static field artifacts verify as one contract (" + error + ")");

            const auto consumer = contractRoot / ("Consumer" + productName);
            writeFile(consumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static int Main()
    {
        int initial = State.Value;
        State.Value = initial + 2;
        return State.Value;
    }
}
)NS");
            const std::string runtimeAttribute = dynamic
                ? " Runtime=\"../Products/" + productName + "/MutableStaticField.dll\""
                : std::string();
            writeFile(consumer / "Consumer.nsproj",
                      "<Project><PropertyGroup><Name>MutableStaticFieldConsumer</Name>"
                      "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                      "<Compile Include=\"**/*.ns\"/>"
                      "<LibraryReference Include=\"../Products/" + productName +
                          "/MutableStaticField.lib\"" + runtimeAttribute + "/>"
                      "</ItemGroup></Project>");
            output.clear();
            error.clear();
            const auto executable = consumer / "Consumer.exe";
            const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                         consumer / "Consumer.nsproj",
                                                         executable,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(consumerResult == ExitCode::Success,
                         productName + " consumer reads and writes producer-owned static storage (" + error + ")");
#ifdef _WIN32
            if (consumerResult == ExitCode::Success)
                tests.expect(runExecutable(executable) == 42,
                             productName + " mutable static field preserves one producer-owned value");
#endif
        }
    }
}

void testPrecompiledImplicitClassConstructionContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto contractRoot = root / "PrecompiledImplicitClassConstruction";
    const auto producer = contractRoot / "Producer";
    const auto products = contractRoot / "Products";
    const std::string producerSource = R"NS(
namespace SurfaceParity
{
    public class Initialized
    {
        public int Value = 42;
    }

    public class ZeroInitialized
    {
        public int Value;
    }
}
)NS";

    for (const bool dynamic : {false, true})
    {
        writeProject(producer,
                     "ImplicitClassConstruction",
                     dynamic ? "SharedLibrary" : "StaticLibrary",
                     producerSource);
        for (const bool optimize : {false, true})
        {
            const std::string productName = std::string(dynamic ? "Dll" : "Static") +
                (optimize ? "O2" : "O0");
            const auto productDirectory = products / productName;
            std::filesystem::create_directories(productDirectory);
            const auto library = productDirectory / "ImplicitClassConstruction.lib";
            const auto runtime = productDirectory / "ImplicitClassConstruction.dll";
            std::string output;
            std::string error;
            const ExitCode producerResult = emitLibrary(dynamic ? "--emit-project-oop-dll" : "--emit-project-oop-lib",
                                                         producer / "ImplicitClassConstruction.nsproj",
                                                         dynamic ? runtime : library,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(producerResult == ExitCode::Success,
                         productName + " producer publishes implicit class constructors (" + error + ")");
            if (producerResult != ExitCode::Success) continue;

            output.clear();
            error.clear();
            tests.expect(verifyLibraryArtifacts(productDirectory / "ImplicitClassConstruction.nsmeta",
                                                library,
                                                dynamic ? runtime : std::filesystem::path(),
                                                output,
                                                error) == ExitCode::Success,
                         productName + " implicit class artifacts verify as one contract (" + error + ")");

            const auto consumer = contractRoot / ("Consumer" + productName);
            writeFile(consumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static int Main()
    {
        Initialized initialized = new Initialized();
        ZeroInitialized zero = new ZeroInitialized();
        int result = initialized.Value + zero.Value;
        delete initialized;
        delete zero;
        return result;
    }
}
)NS");
            const std::string runtimeAttribute = dynamic
                ? " Runtime=\"../Products/" + productName + "/ImplicitClassConstruction.dll\""
                : std::string();
            writeFile(consumer / "Consumer.nsproj",
                      "<Project><PropertyGroup><Name>ImplicitClassConstructionConsumer</Name>"
                      "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                      "<Compile Include=\"**/*.ns\"/>"
                      "<LibraryReference Include=\"../Products/" + productName +
                          "/ImplicitClassConstruction.lib\"" + runtimeAttribute + "/>"
                      "</ItemGroup></Project>");
            output.clear();
            error.clear();
            const auto executable = consumer / "Consumer.exe";
            const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                         consumer / "Consumer.nsproj",
                                                         executable,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(consumerResult == ExitCode::Success,
                         productName + " consumer materializes both implicit construction shapes (" + error + ")");
#ifdef _WIN32
            if (consumerResult == ExitCode::Success)
                tests.expect(runExecutable(executable) == 42,
                             productName + " implicit constructors preserve initialized and zeroed fields");
#endif
        }
    }
}

void testPrecompiledNestedTypeContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto contractRoot = root / "PrecompiledNestedType";
    const auto producer = contractRoot / "Producer";
    const auto products = contractRoot / "Products";
    const std::string producerSource = R"NS(
namespace SurfaceParity
{
    public class Outer
    {
        public struct Inner
        {
            public int Number;
        }

        public static Inner Create()
        {
            Inner result;
            result.Number = 42;
            return result;
        }
    }
}
)NS";

    for (const bool dynamic : {false, true})
    {
        writeProject(producer, "NestedType", dynamic ? "SharedLibrary" : "StaticLibrary", producerSource);
        for (const bool optimize : {false, true})
        {
            const std::string productName = std::string(dynamic ? "Dll" : "Static") +
                (optimize ? "O2" : "O0");
            const auto productDirectory = products / productName;
            std::filesystem::create_directories(productDirectory);
            const auto library = productDirectory / "NestedType.lib";
            const auto runtime = productDirectory / "NestedType.dll";
            std::string output;
            std::string error;
            const ExitCode producerResult = emitLibrary(dynamic ? "--emit-project-oop-dll" : "--emit-project-oop-lib",
                                                         producer / "NestedType.nsproj",
                                                         dynamic ? runtime : library,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(producerResult == ExitCode::Success,
                         productName + " producer preserves the nested nominal identity (" + error + ")");
            if (producerResult != ExitCode::Success) continue;

            output.clear();
            error.clear();
            tests.expect(verifyLibraryArtifacts(productDirectory / "NestedType.nsmeta",
                                                library,
                                                dynamic ? runtime : std::filesystem::path(),
                                                output,
                                                error) == ExitCode::Success,
                         productName + " nested-type artifacts verify as one contract (" + error + ")");

            const auto consumer = contractRoot / ("Consumer" + productName);
            writeFile(consumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static int Main()
    {
        Outer.Inner value = Outer.Create();
        return value.Number;
    }
}
)NS");
            const std::string runtimeAttribute = dynamic
                ? " Runtime=\"../Products/" + productName + "/NestedType.dll\""
                : std::string();
            writeFile(consumer / "Consumer.nsproj",
                      "<Project><PropertyGroup><Name>NestedTypeConsumer</Name>"
                      "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                      "<Compile Include=\"**/*.ns\"/>"
                      "<LibraryReference Include=\"../Products/" + productName +
                          "/NestedType.lib\"" + runtimeAttribute + "/>"
                      "</ItemGroup></Project>");
            output.clear();
            error.clear();
            const auto executable = consumer / "Consumer.exe";
            const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                         consumer / "Consumer.nsproj",
                                                         executable,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(consumerResult == ExitCode::Success,
                         productName + " consumer resolves Outer.Inner without flattening it (" + error + ")");
#ifdef _WIN32
            if (consumerResult == ExitCode::Success)
                tests.expect(runExecutable(executable) == 42,
                             productName + " nested type contract executes to 42");
#endif
        }
    }
}

void testPrecompiledExternCImportContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto contractRoot = root / "PrecompiledExternCImport";
    const auto producer = contractRoot / "Producer";
    const auto products = contractRoot / "Products";
    const std::string producerSource = R"NS(
namespace SurfaceParity
{
    public class Kernel
    {
        extern C from "kernel32.dll"
        {
            public uint GetCurrentThreadId();
        }
    }

    public class ProducerAnchor
    {
        public static int Touch() { return 0; }
    }
}
)NS";

    for (const bool dynamic : {false, true})
    {
        writeProject(producer,
                     "ExternCImport",
                     dynamic ? "SharedLibrary" : "StaticLibrary",
                     producerSource);
        for (const bool optimize : {false, true})
        {
            const std::string productName = std::string(dynamic ? "Dll" : "Static") +
                (optimize ? "O2" : "O0");
            const auto productDirectory = products / productName;
            std::filesystem::create_directories(productDirectory);
            const auto library = productDirectory / "ExternCImport.lib";
            const auto runtime = productDirectory / "ExternCImport.dll";
            std::string output;
            std::string error;
            const ExitCode producerResult = emitLibrary(dynamic ? "--emit-project-oop-dll" : "--emit-project-oop-lib",
                                                         producer / "ExternCImport.nsproj",
                                                         dynamic ? runtime : library,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(producerResult == ExitCode::Success,
                         productName + " producer records the external DLL import (" + error + ")");
            if (producerResult != ExitCode::Success) continue;

            output.clear();
            error.clear();
            tests.expect(verifyLibraryArtifacts(productDirectory / "ExternCImport.nsmeta",
                                                library,
                                                dynamic ? runtime : std::filesystem::path(),
                                                output,
                                                error) == ExitCode::Success,
                         productName + " extern-C artifacts verify as one contract (" + error + ")");

            const auto consumer = contractRoot / ("Consumer" + productName);
            writeFile(consumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static int Main()
    {
        return Kernel.GetCurrentThreadId() == 0u ? 1 : 42 + ProducerAnchor.Touch();
    }
}
)NS");
            const std::string runtimeAttribute = dynamic
                ? " Runtime=\"../Products/" + productName + "/ExternCImport.dll\""
                : std::string();
            writeFile(consumer / "Consumer.nsproj",
                      "<Project><PropertyGroup><Name>ExternCImportConsumer</Name>"
                      "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                      "<Compile Include=\"**/*.ns\"/>"
                      "<LibraryReference Include=\"../Products/" + productName +
                          "/ExternCImport.lib\"" + runtimeAttribute + "/>"
                      "</ItemGroup></Project>");
            output.clear();
            error.clear();
            const auto executable = consumer / "Consumer.exe";
            const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                         consumer / "Consumer.nsproj",
                                                         executable,
                                                         output,
                                                         error,
                                                         optimize);
            tests.expect(consumerResult == ExitCode::Success,
                         productName + " consumer rebuilds extern C from kernel32.dll (" + error + ")");
#ifdef _WIN32
            if (consumerResult == ExitCode::Success)
                tests.expect(runExecutable(executable) == 42,
                             productName + " precompiled extern C import executes to 42");
#endif
        }
    }
}

void testMetadataOnlyDllContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto contractRoot = root / "MetadataOnlyDll";
    const auto producer = contractRoot / "Producer";
    const auto products = contractRoot / "Products";
    writeProject(producer, "MetadataOnly", "SharedLibrary", R"NS(
namespace SurfaceParity
{
    public enum Mode
    {
        Zero = 0
    }

    public struct Box<T>
    {
        public T Value;
    }

    public interface IValue
    {
        int Read();
    }

    public static class Algorithms
    {
        public static int Invoke<T>(T value) where T : class, IValue
        {
            return value.Read();
        }
    }

}

)NS");

    for (const bool optimize : {false, true})
    {
        const std::string level = optimize ? "O2" : "O0";
        const auto productDirectory = products / level;
        std::filesystem::create_directories(productDirectory);
        const auto library = productDirectory / "MetadataOnly.lib";
        const auto runtime = productDirectory / "MetadataOnly.dll";
        std::string output;
        std::string error;
        const ExitCode producerResult = emitLibrary("--emit-project-oop-dll",
                                                     producer / "MetadataOnly.nsproj",
                                                     runtime,
                                                     output,
                                                     error,
                                                     optimize);
        tests.expect(producerResult == ExitCode::Success,
                     "metadata-only DLL emits without inventing a public export at " + level + " (" + error + ")");
        if (producerResult != ExitCode::Success) continue;

        PeExecutableVerifier peVerifier;
        tests.expect(peVerifier.verifyDynamicLibrary(runtime) && peVerifier.info().exportedSymbols.empty(),
                     "metadata-only DLL has a valid PE image and zero fabricated exports at " + level);
        output.clear();
        error.clear();
        tests.expect(verifyLibraryArtifacts(productDirectory / "MetadataOnly.nsmeta",
                                            library,
                                            runtime,
                                            output,
                                            error) == ExitCode::Success,
                     "metadata-only DLL, COFF companion, and NSMETA verify together at " + level + " (" + error + ")");

        const auto consumer = contractRoot / ("Consumer" + level);
        writeFile(consumer / "Main.ns", R"NS(
using SurfaceParity;

public class Item : IValue
{
    public int Read() { return 42; }
}

public class Program
{
    public static int Main()
    {
        Box<int> value;
        value.Value = Algorithms.Invoke<Item>(new Item());
        return value.Value + (int)Mode.Zero;
    }
}
)NS");
        writeFile(consumer / "Consumer.nsproj",
                  "<Project><PropertyGroup><Name>MetadataOnlyConsumer</Name>"
                  "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                  "<Compile Include=\"**/*.ns\"/>"
                  "<LibraryReference Include=\"../Products/" + level + "/MetadataOnly.lib\"/>"
                  "</ItemGroup></Project>");
        output.clear();
        error.clear();
        const auto executable = consumer / "Consumer.exe";
        const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                     consumer / "Consumer.nsproj",
                                                     executable,
                                                     output,
                                                     error,
                                                     optimize);
        tests.expect(consumerResult == ExitCode::Success,
                     "metadata-only LibraryReference materializes enum, generic struct, and generic method at " +
                         level + " (" + error + ")");
#ifdef _WIN32
        if (consumerResult == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "metadata-only DLL contract executes to 42 at " + level);
#endif
    }

    const auto genericOnlyProducer = contractRoot / "GenericOnlyProducer";
    writeProject(genericOnlyProducer, "GenericOnly", "SharedLibrary", R"NS(
namespace SurfaceParity
{
    public struct OnlyBox<T>
    {
        public T Value;
    }
}
)NS");
    for (const bool optimize : {false, true})
    {
        const std::string level = optimize ? "O2" : "O0";
        const auto productDirectory = products / ("GenericOnly" + level);
        std::filesystem::create_directories(productDirectory);
        const auto runtime = productDirectory / "GenericOnly.dll";
        const auto library = productDirectory / "GenericOnly.lib";
        std::string output;
        std::string error;
        const ExitCode producerResult = emitLibrary("--emit-project-oop-dll",
                                                     genericOnlyProducer / "GenericOnly.nsproj",
                                                     runtime,
                                                     output,
                                                     error,
                                                     optimize);
        tests.expect(producerResult == ExitCode::Success,
                     "DLL containing only a public generic struct emits at " + level + " (" + error + ")");
        if (producerResult != ExitCode::Success) continue;

        PeExecutableVerifier peVerifier;
        tests.expect(peVerifier.verifyDynamicLibrary(runtime) && peVerifier.info().exportedSymbols.empty(),
                     "generic-struct-only DLL has zero fabricated exports at " + level);
        output.clear();
        error.clear();
        tests.expect(verifyLibraryArtifacts(productDirectory / "GenericOnly.nsmeta",
                                            library,
                                            runtime,
                                            output,
                                            error) == ExitCode::Success,
                     "generic-struct-only DLL contract verifies at " + level + " (" + error + ")");

        const auto consumer = contractRoot / ("GenericOnlyConsumer" + level);
        writeFile(consumer / "Main.ns", R"NS(
using SurfaceParity;

public class Program
{
    public static int Main()
    {
        OnlyBox<int> value;
        value.Value = 42;
        return value.Value;
    }
}
)NS");
        writeFile(consumer / "Consumer.nsproj",
                  "<Project><PropertyGroup><Name>GenericOnlyConsumer</Name>"
                  "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                  "<Compile Include=\"**/*.ns\"/>"
                  "<LibraryReference Include=\"../Products/GenericOnly" + level + "/GenericOnly.lib\"/>"
                  "</ItemGroup></Project>");
        output.clear();
        error.clear();
        const auto executable = consumer / "Consumer.exe";
        const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                     consumer / "Consumer.nsproj",
                                                     executable,
                                                     output,
                                                     error,
                                                     optimize);
        tests.expect(consumerResult == ExitCode::Success,
                     "generic-struct-only LibraryReference materializes at " + level + " (" + error + ")");
#ifdef _WIN32
        if (consumerResult == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "generic-struct-only DLL contract executes to 42 at " + level);
#endif
    }
}

void testSynthesizedSymbolHygiene(TestContext& tests, const std::filesystem::path& root)
{
    const auto hygieneRoot = root / "SynthesizedSymbolHygiene";
    const auto first = hygieneRoot / "First";
    const auto second = hygieneRoot / "Second";
    writeProject(first, "FirstLibrary", "StaticLibrary", R"NS(
namespace FirstContract
{
    public static class State
    {
        public static int Value = 20;
        public static string Label = "First";
        public static int Read() { return Label == "First" ? Value : 1; }
    }
}
)NS");
    writeProject(second, "SecondLibrary", "StaticLibrary", R"NS(
namespace SecondContract
{
    public static class State
    {
        public static int Value = 22;
        public static string Label = "Second";
        public static int Read() { return Label == "Second" ? Value : 1; }
    }
}
)NS");

    for (const bool optimize : {false, true})
    {
        const std::string level = optimize ? "O2" : "O0";
        const auto products = hygieneRoot / ("Products" + level);
        std::filesystem::create_directories(products);
        const auto firstLibrary = products / "FirstLibrary.lib";
        const auto secondLibrary = products / "SecondLibrary.lib";
        std::string output;
        std::string error;
        const ExitCode firstResult = emitLibrary("--emit-project-oop-lib",
                                                 first / "FirstLibrary.nsproj",
                                                 firstLibrary,
                                                 output,
                                                 error,
                                                 optimize);
        tests.expect(firstResult == ExitCode::Success,
                     "first synthesized-symbol hygiene library emits at " + level + " (" + error + ")");
        output.clear();
        error.clear();
        const ExitCode secondResult = emitLibrary("--emit-project-oop-lib",
                                                  second / "SecondLibrary.nsproj",
                                                  secondLibrary,
                                                  output,
                                                  error,
                                                  optimize);
        tests.expect(secondResult == ExitCode::Success,
                     "second synthesized-symbol hygiene library emits at " + level + " (" + error + ")");
        if (firstResult != ExitCode::Success || secondResult != ExitCode::Success) continue;

        const auto consumer = hygieneRoot / ("Consumer" + level);
        writeFile(consumer / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        return FirstContract.State.Read() + SecondContract.State.Read();
    }
}
)NS");
        writeFile(consumer / "Consumer.nsproj",
                  "<Project><PropertyGroup><Name>HygieneConsumer</Name>"
                  "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                  "<Compile Include=\"**/*.ns\"/>"
                  "<LibraryReference Include=\"../Products" + level + "/FirstLibrary.lib\"/>"
                  "<LibraryReference Include=\"../Products" + level + "/SecondLibrary.lib\"/>"
                  "</ItemGroup></Project>");
        output.clear();
        error.clear();
        const auto executable = consumer / "Consumer.exe";
        const ExitCode consumerResult = emitLibrary("--emit-project-oop-exe",
                                                     consumer / "Consumer.nsproj",
                                                     executable,
                                                     output,
                                                     error,
                                                     optimize);
        tests.expect(consumerResult == ExitCode::Success,
                     "independent synthesized symbols link together at " + level + " (" + error + ")");
#ifdef _WIN32
        if (consumerResult == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         "two libraries with static initialization and strings execute to 42 at " + level);
#endif
    }
}

void testPrecompiledLibraryReferences(TestContext& tests, const std::filesystem::path& root)
{
    // C8's optional Object root must remain usable when it arrives through NSMETA. The consumer's
    // Program has no explicit base, so the inheritance model attaches the imported Object and the
    // LLVM descriptor must resolve it through the producer's PE-safe descriptor accessor.
    const auto objectRoot = root / "PrecompiledObjectRoot";
    const auto objectProducer = objectRoot / "Producer";
    const auto objectConsumer = objectRoot / "Consumer";
    writeProject(objectProducer, "ObjectProducer", "StaticLibrary", R"NS(
namespace NativeSharp.Core
{
    public class Object { }
    public class MathUtil
    {
        public static int Max(int left, int right) { return left > right ? left : right; }
    }
}
)NS");
    std::string output;
    std::string error;
    const auto objectLibrary = objectProducer / "ObjectProducer.lib";
    const ExitCode objectProducerResult = emitLibrary(
        "--emit-project-oop-lib",
        objectProducer / "ObjectProducer.nsproj",
        objectLibrary,
        output,
        error,
        false);
    tests.expect(objectProducerResult == ExitCode::Success,
                 "precompiled Object-root producer emits its static archive and NSMETA (" + error + ")");
    writeFile(objectConsumer / "Main.ns", R"NS(
namespace Consumer
{
    using NativeSharp.Core;
    public class Program
    {
        public static int Main() { return MathUtil.Max(4, 42); }
    }
}
)NS");
    writeFile(objectConsumer / "ObjectConsumer.nsproj",
              "<Project><PropertyGroup><Name>ObjectConsumer</Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/ObjectProducer.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto objectExecutable = objectConsumer / "ObjectConsumer.exe";
    const ExitCode objectConsumerResult = emitLibrary(
        "--emit-project-oop-exe",
        objectConsumer / "ObjectConsumer.nsproj",
        objectExecutable,
        output,
        error,
        false);
    tests.expect(objectConsumerResult == ExitCode::Success,
                 "consumer resolves its implicit precompiled Object base through the published "
                 "descriptor accessor (" + error + ")");
#ifdef _WIN32
    if (objectConsumerResult == ExitCode::Success)
        tests.expect(runExecutable(objectExecutable) == 42,
                     "consumer with an implicit precompiled Object base executes to 42");
#endif

    const std::string producerSource = R"NS(
namespace NativeSharp.Core
{
    public class Exception { }
}
using NativeSharp.Core;

namespace Precompiled
{
    public class NativeSupport
    {
        extern C
        {
            public void Copy(void* destination, void* source, long count);
        }
    }

    public struct Pair
    {
        public int X;
        public int Y;

        public int Sum() { return X + Y; }
        public void Offset(int delta) { X += delta; }
    }

    public union Overlay
    {
        public int Number;
        public float Floating;
    }

    public pack(1) size(5) struct Packed
    {
        public byte Tag;
        public int Value;
    }

    public pack(1) size(13) struct Packet
    {
        public Packed Header;
        public long Tail;
    }

    public enum Mode : byte
    {
        Zero = 0,
        Active = 1
    }

    public extern C delegate int Transform(int value);
    public delegate int ManagedTransform(int value);

    public class ManagedCallbacks
    {
        private int Bias;

        public ManagedCallbacks(int bias) { Bias = bias; }
        public int Add(int value) { return value + Bias; }
        private static int PlusTwo(int value) { return value + 2; }

        public static owned ManagedTransform MakeStatic() { return ManagedCallbacks.PlusTwo; }
        public static owned ManagedTransform MakeClosure(int bias) { return value => value + bias; }
        public static owned ManagedTransform MakeInstance(borrowed ManagedCallbacks target)
        {
            return target.Add;
        }
        public static int Invoke(borrowed ManagedTransform callback, int value)
        {
            return callback(value);
        }
    }

    public class Problem : Exception
    {
        public int Code;
        public Problem(int code) { Code = code; }
    }

    public class CrossModuleExceptions
    {
        public static void Raise(borrowed Problem problem) { throw problem; }
    }

    public class Counter
    {
        private int Value;

        public Counter(int value) { Value = value; }
        public int Add(int delta) { Value += delta; return Value; }
        public int Read() { return Value; }
    }

    public interface IScore
    {
        public int Score();
    }

    public abstract class CounterBase
    {
        protected int Value;

        public CounterBase(int value) { Value = value; }
        public virtual int Score() { return Value; }
    }

    public sealed class BonusCounter : CounterBase, IScore
    {
        public BonusCounter(int value) : base(value) { }
        public override int Score() { return Value + 2; }
    }

    public class Objects
    {
        public static owned Counter Create(int value) { return new Counter(value); }
        public static int Read(borrowed Counter value) { return value.Read(); }
        public static void Destroy(owned Counter value) { }
    }

    public class Lifetime
    {
        private static int DestroyedCount;
        public static void Reset() { DestroyedCount = 0; }
        public static void Record() { DestroyedCount += 1; }
        public static int Destroyed() { return DestroyedCount; }
    }

    public class Tracked
    {
        public Tracked() { }
        ~Tracked() { Lifetime.Record(); }
    }

    public class Callbacks
    {
        public static int Invoke(Transform callback, int value) { return callback(value); }
    }

    public class Math
    {
        public static export extern C int Add(int left, int right = 22) { return left + right; }
        public static int Blend(int left, int right = 22) { return left + right; }
        public static long Blend(long left, long right) { return left + right; }
        public static Pair Echo(Pair value) { return value; }
        public static int ReadOverlay(Overlay value) { return value.Number; }
        public static Packet EchoPacket(Packet value) { return value; }
        public static Mode EchoMode(Mode value) { return value; }
        public static void Bump(ref int value) { value += 1; }
        public static void Assign(out int value) { value = 0; }
        public static int Read(in int value) { return value; }
        private static int Hidden(int value) { return value + 1; }
    }

    public struct Box<T>
    {
        public T Value;
        public T Read() { return Value; }
    }

    public class GenericAlgorithms
    {
        private static T ClosureLeaf<T>(T value) { return value; }
        private static T ClosureMiddle<T>(T value) { return ClosureLeaf<T>(value); }
        public static T ClosureRoot<T>(T value) { return ClosureMiddle<T>(value); }
        public static T Identity<T>(T value) { return value; }
        public static int Pick<T>(T value) { return 1; }
        public static int Pick<T>(T[] values) { return 2; }
        public static int AddDefault<T>(T value, int extra = 2) { return extra; }
        public static owned T Transfer<T>(owned T value) where T : class
        {
            return move(value);
        }
        private static T Secret<T>(T value) { return value; }
        public static int ScoreOf<T>(borrowed T value) where T : IScore
        {
            return value.Score();
        }
        public static int Warm()
        {
            Box<int> box;
            box.Value = Identity<int>(1);
            return box.Read();
        }
    }

    public class ClosureOuter
    {
        public static class NestedAlgorithms
        {
            private static T NestedLeaf<T>(T value) { return value; }
            public static T NestedRoot<T>(T value) { return NestedLeaf<T>(value); }
        }
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
    const auto staticLibrary = staticProducer / "NativeMath.lib";
    tests.expect(
        emitLibrary(
            "--emit-project-oop-lib", staticProducer / "StaticProducer.nsproj", staticLibrary, output, error, true) ==
            ExitCode::Success,
        "0.9 precompiled static-library producer emits .lib and metadata");
    CoffArchiveVerifier precompiledArchiveVerifier;
    tests.expect(precompiledArchiveVerifier.verify(staticLibrary) &&
                     contains(precompiledArchiveVerifier.info().symbolNames, "Add"),
                 "COFF archive index exposes the C ABI symbol published by metadata");
    writeFile(staticConsumer / "Main.ns", consumerSource);
    writeFile(staticConsumer / "StaticConsumer.nsproj",
              "<Project><PropertyGroup><Name>StaticConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto staticExecutable = staticConsumer / "StaticConsumer.exe";
    const ExitCode staticResult = emitLibrary(
        "--emit-project-oop-exe", staticConsumer / "StaticConsumer.nsproj", staticExecutable, output, error, true);
    tests.expect(staticResult == ExitCode::Success,
                 "consumer links a NativeSharp static library from "
                 "LibraryReference without --link-lib (" +
                     error + ")");
    tests.expect(output.find("linked-libraries: 1") != std::string::npos,
                 "static consumer reports one validated project library reference");
#ifdef _WIN32
    if (staticResult == ExitCode::Success)
    {
        tests.expect(runExecutable(staticExecutable) == 42,
                     "separate static-library producer/consumer executes through "
                     "the published C ABI");
    }
#endif

    const auto duplicateModuleProducer = staticRoot / "DuplicateModuleProducer";
    writeProject(duplicateModuleProducer, "StaticProducer", "StaticLibrary", R"NS(
namespace DuplicateContract
{
    public class Service { public static int Answer() { return 42; } }
}
)NS");
    output.clear();
    error.clear();
    tests.expect(emitLibrary("--emit-project-oop-lib",
                             duplicateModuleProducer / "StaticProducer.nsproj",
                             duplicateModuleProducer / "DuplicateModule.lib",
                             output,
                             error,
                             true) == ExitCode::Success,
                 "P6 duplicate-module fixture emits a valid archive with an independently signed contract");
    const auto duplicateModuleConsumer = staticRoot / "DuplicateModuleConsumer";
    writeFile(duplicateModuleConsumer / "Main.ns", consumerSource);
    writeFile(duplicateModuleConsumer / "DuplicateModuleConsumer.nsproj",
              "<Project><PropertyGroup><Name>DuplicateModuleConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "<LibraryReference Include=\"../DuplicateModuleProducer/DuplicateModule.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto duplicateModuleExecutable = duplicateModuleConsumer / "DuplicateModuleConsumer.exe";
    tests.expect(emitLibrary("--emit-project-oop-exe",
                             duplicateModuleConsumer / "DuplicateModuleConsumer.nsproj",
                             duplicateModuleExecutable,
                             output,
                             error,
                             true) == ExitCode::CompilationError,
                 "P6 rejects two precompiled products with the same canonical module identity");
    tests.expect(error.find("duplicate module name 'StaticProducer'") != std::string::npos,
                 "P6 duplicate-module diagnostic names the conflicting canonical identity");
    tests.expect(!std::filesystem::exists(duplicateModuleExecutable),
                 "P6 duplicate-module rejection leaves no partial executable");

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
    writeFile(automaticStaticConsumer / "AutomaticConsumer.nsproj",
              "<Project><PropertyGroup><Name>AutomaticConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto automaticStaticExecutable = automaticStaticConsumer / "AutomaticConsumer.exe";
    const ExitCode automaticStaticResult = emitLibrary("--emit-project-oop-exe",
                                                       automaticStaticConsumer / "AutomaticConsumer.nsproj",
                                                       automaticStaticExecutable,
                                                       output,
                                                       error,
                                                       true);
    tests.expect(automaticStaticResult == ExitCode::Success,
                 "metadata-backed static consumer resolves overloads, defaults, "
                 "and ref/out/in without an "
                 "extern C redeclaration (" +
                     error + ")");
#ifdef _WIN32
    if (automaticStaticResult == ExitCode::Success)
    {
        tests.expect(runExecutable(automaticStaticExecutable) == 42,
                     "metadata-backed static overload calls use imported defaults "
                     "and execute to 42");
    }
#endif

    const auto nominalStaticConsumer = staticRoot / "NominalConsumer";
    writeFile(nominalStaticConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        public static int Main()
        {
            Precompiled.Pair value;
            value.X = 10;
            value.Y = 9;
            Precompiled.Pair copy = Precompiled.Math.Echo(value);
            copy.Offset(1);
            Precompiled.Overlay overlay;
            overlay.Number = 1;
            Precompiled.Packet packet;
            packet.Header.Tag = 1;
            packet.Header.Value = 10;
            packet.Tail = 10;
            Precompiled.Packet packetCopy = Precompiled.Math.EchoPacket(packet);
            Precompiled.Mode mode = Precompiled.Math.EchoMode(Precompiled.Mode.Zero);
            return copy.Sum() + Precompiled.Math.ReadOverlay(overlay) +
                packetCopy.Header.Tag + packetCopy.Header.Value +
                (int)packetCopy.Tail + (int)mode;
        }
    }
}
)NS");
    writeFile(nominalStaticConsumer / "NominalConsumer.nsproj",
              "<Project><PropertyGroup><Name>NominalStaticConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : { false, true })
    {
        output.clear();
        error.clear();
        const auto executable = nominalStaticConsumer / (optimize ? "NominalStaticO2.exe" : "NominalStaticO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            nominalStaticConsumer / "NominalConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     std::string("metadata-backed static consumer imports struct "
                                 "layout and enum identity at ") +
                         (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
        {
            tests.expect(runExecutable(executable) == 42,
                         std::string("static nominal ABI executes to 42 at ") + (optimize ? "O2" : "O0"));
        }
#endif
    }

    const auto invalidGenericConsumer = staticRoot / "InvalidGenericConsumer";
    writeFile(invalidGenericConsumer / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        raii Precompiled.Counter value = new Precompiled.Counter(42);
        return Precompiled.GenericAlgorithms.ScoreOf<Precompiled.Counter>(value);
    }
}
)NS");
    writeFile(invalidGenericConsumer / "InvalidGenericConsumer.nsproj",
              "<Project><PropertyGroup><Name>InvalidGenericConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(runProjectAction("--check-project-generic-constraints",
                                  invalidGenericConsumer / "InvalidGenericConsumer.nsproj", output,
                                  error) == ExitCode::CompilationError,
                 "consumer rejects an imported generic specialization whose interface "
                 "constraint is false");
    tests.expect(error.find("requires an implementation of 'IScore'") != std::string::npos,
                 "cross-library constraint diagnostic names the producer's "
                 "canonical interface contract");

    const auto privateGenericConsumer = staticRoot / "PrivateGenericConsumer";
    writeFile(privateGenericConsumer / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        return Precompiled.GenericAlgorithms.ClosureLeaf<int>(42);
    }
}
)NS");
    writeFile(privateGenericConsumer / "PrivateGenericConsumer.nsproj",
              "<Project><PropertyGroup><Name>PrivateGenericConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(runProjectAction("--check-project-generics", privateGenericConsumer / "PrivateGenericConsumer.nsproj",
                                  output, error) == ExitCode::CompilationError,
                 "consumer cannot instantiate a private materialization-only generic method");
    tests.expect(error.find("has no visible Stage 44 generic method template") != std::string::npos,
                  "private generic method fails during generic selection, "
                  "before name lookup or link");

    const auto genericStaticConsumer = staticRoot / "GenericConsumer";
    writeFile(genericStaticConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        public static int Main()
        {
            Precompiled.Box<int> box;
            box.Value = Precompiled.GenericAlgorithms.Identity<int>(42);
            Precompiled.Box<long> fresh;
            fresh.Value = Precompiled.GenericAlgorithms.Identity<long>((long)0);
            int[] probes = new int[1];
            raii Precompiled.Counter original = new Precompiled.Counter(37);
            raii Precompiled.Counter transferred =
                Precompiled.GenericAlgorithms.Transfer<Precompiled.Counter>(move(original));
            if (Precompiled.GenericAlgorithms.Pick<int>(0) != 1) { return 1; }
            if (Precompiled.GenericAlgorithms.Pick<int>(probes) != 2) { return 2; }
            if (Precompiled.GenericAlgorithms.AddDefault<int>(0) != 2) { return 3; }
            if (transferred.Read() != 37) { return 4; }
            if (Precompiled.GenericAlgorithms.ClosureRoot<int>(42) != 42) { return 5; }
            if (Precompiled.ClosureOuter.NestedAlgorithms.NestedRoot<int>(42) != 42) { return 6; }
            return box.Read() + (int)fresh.Read() + Precompiled.Math.Add(0, 0);
        }
    }
}
)NS");
    writeFile(genericStaticConsumer / "GenericConsumer.nsproj",
              "<Project><PropertyGroup><Name>GenericStaticConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const auto executable = genericStaticConsumer / (optimize ? "GenericStaticO2.exe" : "GenericStaticO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe", genericStaticConsumer / "GenericConsumer.nsproj",
                                            executable, output, error, optimize);
        tests.expect(result == ExitCode::Success, std::string("static LibraryReference materializes a new "
                                                              "generic type and method at ") +
                                                      (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42, std::string("cross-library static generic specializations "
                                                                      "execute to 42 at ") +
                                                              (optimize ? "O2" : "O0"));
#endif
    }

    const auto callbackStaticConsumer = staticRoot / "CallbackConsumer";
    writeFile(callbackStaticConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        private static int PlusTwo(int value) { return value + 2; }

        public static int Main()
        {
            Precompiled.Transform callback = Program.PlusTwo;
            return Precompiled.Callbacks.Invoke(callback, 40);
        }
    }
}
)NS");

    // E3: el delegate ordinario ya no se degrada a extern C al reconstruirse. Los tres casos
    // ejercitan codigo estático, entorno de closure y receptor de instancia creados en el
    // productor; el owner y su cleanup viven en el consumidor.
    const auto managedStaticConsumer = staticRoot / "ManagedDelegateConsumer";
    writeFile(managedStaticConsumer / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        raii Precompiled.ManagedTransform direct = Precompiled.ManagedCallbacks.MakeStatic();
        raii Precompiled.ManagedTransform closure = Precompiled.ManagedCallbacks.MakeClosure(2);
        raii Precompiled.ManagedCallbacks target = new Precompiled.ManagedCallbacks(2);
        raii Precompiled.ManagedTransform instance =
            Precompiled.ManagedCallbacks.MakeInstance(target);
        return Precompiled.ManagedCallbacks.Invoke(direct, 10) +
            Precompiled.ManagedCallbacks.Invoke(closure, 10) +
            Precompiled.ManagedCallbacks.Invoke(instance, 8) + 8;
    }
}
)NS");
    writeFile(managedStaticConsumer / "ManagedDelegateConsumer.nsproj",
              "<Project><PropertyGroup><Name>ManagedDelegateStaticConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const auto executable = managedStaticConsumer /
            (optimize ? "ManagedDelegateStaticO2.exe" : "ManagedDelegateStaticO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            managedStaticConsumer / "ManagedDelegateConsumer.nsproj",
                                            executable, output, error, optimize);
        tests.expect(result == ExitCode::Success,
                     std::string("ordinary delegates and closures cross a static-library boundary at ") +
                         (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         std::string("static-library delegate code/context ABI executes to 42 at ") +
                             (optimize ? "O2" : "O0"));
#endif
    }

    const auto exceptionStaticConsumer = staticRoot / "ExceptionConsumer";
    writeFile(exceptionStaticConsumer / "Main.ns", R"NS(
using Precompiled;

public class Program
{
    public static int Main()
    {
        int finallyCount = 0;
        raii Precompiled.Problem problem = new Precompiled.Problem(40);
        try
        {
            Precompiled.CrossModuleExceptions.Raise(problem);
        }
        catch (Problem caught)
        {
            return caught.Code + finallyCount + 2;
        }
        finally
        {
            finallyCount += 1;
        }
        return 1;
    }
}
)NS");
    writeFile(exceptionStaticConsumer / "ExceptionConsumer.nsproj",
              "<Project><PropertyGroup><Name>ExceptionStaticConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const auto executable = exceptionStaticConsumer /
            (optimize ? "ExceptionStaticO2.exe" : "ExceptionStaticO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            exceptionStaticConsumer / "ExceptionConsumer.nsproj",
                                            executable, output, error, optimize);
        tests.expect(result == ExitCode::Success,
                     std::string("typed exceptions and finally cross a static-library boundary at ") +
                         (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         std::string("static-library exception RTTI/finally executes to 42 at ") +
                             (optimize ? "O2" : "O0"));
#endif
    }
    writeFile(callbackStaticConsumer / "CallbackConsumer.nsproj",
              "<Project><PropertyGroup><Name>CallbackStaticConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : { false, true })
    {
        output.clear();
        error.clear();
        const auto executable =
            callbackStaticConsumer / (optimize ? "CallbackStaticO2.exe" : "CallbackStaticO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            callbackStaticConsumer / "CallbackConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     std::string("metadata-backed static consumer reconstructs an "
                                                              "extern C delegate callback at ") +
                         (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         std::string("static precompiled callback executes to 42 at ") +
                             (optimize ? "O2" : "O0"));
#endif
    }

    const auto opaqueStaticConsumer = staticRoot / "OpaqueClassConsumer";
    writeFile(opaqueStaticConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        public static int Main()
        {
            raii Precompiled.Counter value = new Precompiled.Counter(40);
            int answer = value.Add(2);
            if (Precompiled.Objects.Read(value) != 42) { return 1; }
            raii Precompiled.BonusCounter polymorphic = new Precompiled.BonusCounter(40);
            borrowed Precompiled.CounterBase baseView = polymorphic;
            borrowed Precompiled.IScore interfaceView = polymorphic;
            if (baseView.Score() != 42) { return 2; }
            if (interfaceView.Score() != 42) { return 3; }
            if (Precompiled.GenericAlgorithms.ScoreOf<Precompiled.BonusCounter>(polymorphic) != 42)
            {
                return 4;
            }
            Precompiled.Lifetime.Reset();
            {
                raii Precompiled.Tracked tracked = new Precompiled.Tracked();
            }
            if (Precompiled.Lifetime.Destroyed() != 1) { return 5; }
            return answer;
        }
    }
}
)NS");
    writeFile(opaqueStaticConsumer / "OpaqueClassConsumer.nsproj",
              "<Project><PropertyGroup><Name>OpaqueClassStaticConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : { false, true })
    {
        output.clear();
        error.clear();
        const auto executable =
            opaqueStaticConsumer / (optimize ? "OpaqueClassStaticO2.exe" : "OpaqueClassStaticO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            opaqueStaticConsumer / "OpaqueClassConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     std::string("metadata-backed static consumer constructs a "
                                                              "precompiled class and calls a "
                                                              "concrete instance receiver at ") +
                         (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
        {
            tests.expect(runExecutable(executable) == 42,
                         std::string("static precompiled construction and ownership "
                                                                      "execute to 42 at ") +
                             (optimize ? "O2" : "O0"));
        }
#endif
    }

    const auto badLayoutRoot = staticRoot / "BadNominalLayout";
    LibraryMetadata nominalMetadata;
    LibraryMetadataCodec metadataCodec;
    error.clear();
    tests.expect(metadataCodec.read(staticProducer / "NativeMath.nsmeta", nominalMetadata, error),
                 "nominal ABI negative fixture reads the producer metadata");
    const auto packetType = std::ranges::find_if(
        nominalMetadata.types, [](const auto& type) { return type.fullyQualifiedName == "Precompiled.Packet"; });
    if (packetType != nominalMetadata.types.end()) packetType->size = 12;
    const auto badMetadata = badLayoutRoot / "NativeMath.nsmeta";
    tests.expect(packetType != nominalMetadata.types.end() &&
                     metadataCodec.writeModel(nominalMetadata, badMetadata, error),
                 "negative fixture recomputes a self-consistent fingerprint for a false "
                 "struct size");
    writeFile(badLayoutRoot / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        Precompiled.Packet value;
        value.Header.Tag = 1;
        value.Header.Value = 20;
        value.Tail = 21;
        return (int)Precompiled.Math.EchoPacket(value).Tail;
    }
}
)NS");
    writeFile(badLayoutRoot / "BadNominalLayout.nsproj",
              "<Project><PropertyGroup><Name>BadNominalLayout</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
              "Metadata=\"NativeMath.nsmeta\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto badLayoutExecutable = badLayoutRoot / "BadNominalLayout.exe";
    tests.expect(emitLibrary("--emit-project-oop-exe",
                             badLayoutRoot / "BadNominalLayout.nsproj",
                             badLayoutExecutable,
                             output,
                             error,
                             true) == ExitCode::CompilationError,
                 "consumer rejects self-consistent metadata whose nominal size "
                 "cannot be materialized");
    tests.expect(error.find("declares size(12)") != std::string::npos && error.find("13 bytes") != std::string::npos &&
                     !std::filesystem::exists(badLayoutExecutable),
                 "nominal ABI mismatch reports recorded and reconstructed sizes "
                 "before link");

    const auto badReferenceRoot = staticRoot / "BadReferenceAbi";
    LibraryMetadata badReferenceMetadata = nominalMetadata;
    const auto counterType = std::ranges::find_if(badReferenceMetadata.types, [](const auto& type) {
        return type.fullyQualifiedName == "Precompiled.Counter";
    });
    if (counterType != badReferenceMetadata.types.end()) counterType->referenceSize = 4;
    error.clear();
    tests.expect(counterType != badReferenceMetadata.types.end() &&
                     metadataCodec.writeModel(badReferenceMetadata,
                                              badReferenceRoot / "NativeMath.nsmeta",
                                              error),
                 "negative fixture recomputes a valid fingerprint for a false "
                 "class reference carrier");
    writeFile(badReferenceRoot / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        raii Precompiled.Counter value = Precompiled.Objects.Create(42);
        Precompiled.Objects.Destroy(move(value));
        return 42;
    }
}
)NS");
    writeFile(badReferenceRoot / "BadReferenceAbi.nsproj", "<Project><PropertyGroup><Name>BadReferenceAbi</"
                                                           "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
                                                           "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
                                                           "Metadata=\"NativeMath.nsmeta\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary("--emit-project-oop-exe",
                             badReferenceRoot / "BadReferenceAbi.nsproj",
                             badReferenceRoot / "BadReferenceAbi.exe",
                             output,
                             error,
                             true) == ExitCode::CompilationError &&
                     error.find("reference ABI") != std::string::npos &&
                     error.find("4/8") != std::string::npos && error.find("8/8") != std::string::npos,
                 "consumer rejects a self-consistent class reference ABI "
                 "mismatch before link");

    output.clear();
    error.clear();
    tests.expect(
        runProjectAction("--dump-project-names", automaticStaticConsumer / "AutomaticConsumer.nsproj", output, error) ==
                ExitCode::Success &&
            output.find("StaticProducer::Precompiled.Math") != std::string::npos,
        "metadata type enters name lookup with the producer's canonical module "
        "identity");

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
    writeFile(hiddenConsumer / "HiddenConsumer.nsproj",
              "<Project><PropertyGroup><Name>HiddenConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary("--emit-project-oop-exe",
                             hiddenConsumer / "HiddenConsumer.nsproj",
                             hiddenConsumer / "HiddenConsumer.exe",
                             output,
                             error,
                             true) == ExitCode::CompilationError,
                 "metadata-backed consumer cannot call a method that was not published");
    tests.expect(error.find("Hidden") != std::string::npos,
                 "unpublished-member diagnostic names the inaccessible API "
                 "requested by the consumer");

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
    tests.expect(
        emitLibrary(
            "--emit-project-oop-lib", ambiguousProducer / "OtherMath.nsproj", ambiguousLibrary, output, error, true) ==
            ExitCode::Success,
        "ambiguity fixture emits a second module with the same public "
        "type spelling");

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
    writeFile(ambiguousConsumer / "AmbiguousConsumer.nsproj",
              "<Project><PropertyGroup><Name>AmbiguousConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "<LibraryReference Include=\"../AmbiguousProducer/OtherMath.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary("--emit-project-oop-exe",
                             ambiguousConsumer / "AmbiguousConsumer.nsproj",
                             ambiguousConsumer / "AmbiguousConsumer.exe",
                             output,
                             error,
                             true) == ExitCode::CompilationError,
                 "two referenced modules with the same public type spelling are "
                 "rejected as ambiguous");
    tests.expect(error.find("ambiguous") != std::string::npos,
                 "cross-library type ambiguity is diagnosed before lowering and link");

    const auto ambiguousGenericProducer = staticRoot / "AmbiguousGenericProducer";
    writeProject(ambiguousGenericProducer, "OtherGeneric", "StaticLibrary", R"NS(
namespace Precompiled
{
    public struct Box<T> { public T Value; }
}
)NS");
    output.clear();
    error.clear();
    const auto ambiguousGenericLibrary = ambiguousGenericProducer / "OtherGeneric.lib";
    tests.expect(emitLibrary("--emit-project-oop-lib", ambiguousGenericProducer / "OtherGeneric.nsproj",
                             ambiguousGenericLibrary, output, error, true) == ExitCode::Success,
                 "generic ambiguity fixture emits a second public Box<T> contract");

    const auto ambiguousGenericConsumer = staticRoot / "AmbiguousGenericConsumer";
    writeFile(ambiguousGenericConsumer / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        Precompiled.Box<int> value;
        value.Value = 42;
        return value.Value;
    }
}
)NS");
    writeFile(ambiguousGenericConsumer / "AmbiguousGenericConsumer.nsproj",
              "<Project><PropertyGroup><Name>AmbiguousGenericConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "<LibraryReference "
              "Include=\"../AmbiguousGenericProducer/OtherGeneric.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(runProjectAction("--check-project-generics",
                                  ambiguousGenericConsumer / "AmbiguousGenericConsumer.nsproj", output,
                                  error) == ExitCode::CompilationError,
                 "two visible library templates with the same qualified spelling "
                 "are rejected");
    tests.expect(error.find("does not name a unique visible generic class/struct template") != std::string::npos,
                 "generic ambiguity is diagnosed during template selection before "
                 "materialization");

    const auto compositeProducer = staticRoot / "CompositeProducer";
    writeFile(compositeProducer / "Main.ns", R"NS(
namespace Precompiled
{
    public class Composite
    {
        public static export extern C int Answer()
        {
            Box<long> value;
            value.Value = GenericAlgorithms.Identity<long>((long)41);
            return (int)value.Read() + Math.Add(0, 1);
        }
    }
}
)NS");
    writeFile(compositeProducer / "CompositeProducer.nsproj",
              "<Project><PropertyGroup><Name>CompositeProducer</"
              "Name><OutputType>StaticLibrary</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto compositeLibrary = compositeProducer / "Composite.lib";
    const ExitCode compositeResult = emitLibrary("--emit-project-oop-lib",
                                                 compositeProducer / "CompositeProducer.nsproj",
                                                 compositeLibrary,
                                                 output,
                                                 error,
                                                 true);
    tests.expect(compositeResult == ExitCode::Success,
                 "static producer consolidates its validated static LibraryReference (" + error + ")");
    CoffArchiveVerifier compositeVerifier;
    tests.expect(compositeResult == ExitCode::Success && compositeVerifier.verify(compositeLibrary) &&
                     contains(compositeVerifier.info().symbolNames, "Answer") &&
                     contains(compositeVerifier.info().symbolNames, "Add"),
                 "consolidated static archive indexes both its own API and "
                 "dependency symbols");

    const auto compositeConsumer = staticRoot / "CompositeConsumer";
    writeFile(compositeConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        public static int Main() { return Precompiled.Composite.Answer(); }
    }
}
)NS");
    writeFile(compositeConsumer / "CompositeConsumer.nsproj",
              "<Project><PropertyGroup><Name>CompositeConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../CompositeProducer/Composite.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto compositeExecutable = compositeConsumer / "CompositeConsumer.exe";
    const ExitCode compositeConsumerResult = emitLibrary("--emit-project-oop-exe",
                                                         compositeConsumer / "CompositeConsumer.nsproj",
                                                         compositeExecutable,
                                                         output,
                                                         error,
                                                         true);
    tests.expect(compositeConsumerResult == ExitCode::Success,
                 "metadata consumer links the consolidated static product "
                 "without referencing its dependency (" +
                     error + ")");
#ifdef _WIN32
    if (compositeConsumerResult == ExitCode::Success)
    {
        tests.expect(runExecutable(compositeExecutable) == 42,
                     "consolidated A-to-B nominal dependency executes from A alone");
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
    writeFile(badSignatureConsumer / "BadSignatureConsumer.nsproj",
              "<Project><PropertyGroup><Name>BadSignatureConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto badSignatureExecutable = badSignatureConsumer / "BadSignatureConsumer.exe";
    tests.expect(emitLibrary("--emit-project-oop-exe",
                             badSignatureConsumer / "BadSignatureConsumer.nsproj",
                             badSignatureExecutable,
                             output,
                             error,
                             true) == ExitCode::CompilationError,
                 "LibraryReference rejects an extern C declaration with an "
                 "ABI-incompatible signature");
    tests.expect(error.find("does not match the metadata signature published by module "
                            "'StaticProducer'") != std::string::npos ||
                     error.find("conflicting declarations of imported C symbol 'Add'") != std::string::npos,
                 "signature mismatch is rejected before link by the imported metadata "
                 "contract");
    tests.expect(!std::filesystem::exists(badSignatureExecutable),
                 "signature mismatch stops before producing an executable");

    const auto wrongArchiveConsumer = staticRoot / "WrongArchiveConsumer";
    writeFile(wrongArchiveConsumer / "Main.ns", consumerSource);
    writeFile(wrongArchiveConsumer / "WrongArchiveConsumer.nsproj",
              "<Project><PropertyGroup><Name>WrongArchiveConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../../StaticLibrary/NativeMath.lib\" "
              "Metadata=\"../Producer/NativeMath.nsmeta\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary("--emit-project-oop-exe",
                             wrongArchiveConsumer / "WrongArchiveConsumer.nsproj",
                             wrongArchiveConsumer / "WrongArchiveConsumer.exe",
                             output,
                             error,
                             true) == ExitCode::CompilationError,
                 "LibraryReference rejects metadata paired with a different "
                 "static archive");
    tests.expect(error.find("does not contain C ABI symbol 'Add' published by metadata module "
                            "'StaticProducer'") != std::string::npos,
                 "static archive/metadata mismatch reports the first missing published "
                 "symbol");

    const auto dllRoot = root / "PrecompiledDll";
    const auto dllProducer = dllRoot / "Producer";
    const auto dllConsumer = dllRoot / "Consumer";
    writeProject(dllProducer, "DllProducer", "SharedLibrary", producerSource);
    output.clear();
    error.clear();
    const auto dll = dllProducer / "NativeMath.dll";
    tests.expect(emitLibrary("--emit-project-oop-dll", dllProducer / "DllProducer.nsproj", dll, output, error, true) ==
                     ExitCode::Success,
                 "0.9 precompiled DLL producer emits DLL, import .lib, and metadata");
    writeFile(dllConsumer / "Main.ns", consumerSource);
    writeFile(dllConsumer / "DllConsumer.nsproj",
              "<Project><PropertyGroup><Name>DllConsumer</Name><OutputType>Exe</"
              "OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
              "Runtime=\"../Producer/NativeMath.dll\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    // Put the consumer beside the runtime DLL so the Windows loader resolves the
    // exact product that LibraryReference validated.
    const auto dllExecutable = dllProducer / "DllConsumer.exe";
    const ExitCode dllResult =
        emitLibrary("--emit-project-oop-exe", dllConsumer / "DllConsumer.nsproj", dllExecutable, output, error, true);
    tests.expect(dllResult == ExitCode::Success,
                 "consumer links a DLL import library from LibraryReference "
                 "without --link-lib (" +
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

    const auto genericDllConsumer = dllRoot / "GenericConsumer";
    writeFile(genericDllConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        public static int Main()
        {
            Precompiled.Box<int> box;
            box.Value = Precompiled.GenericAlgorithms.Identity<int>(42);
            Precompiled.Box<long> fresh;
            fresh.Value = Precompiled.GenericAlgorithms.Identity<long>((long)0);
            int[] probes = new int[1];
            raii Precompiled.Counter original = new Precompiled.Counter(37);
            raii Precompiled.Counter transferred =
                Precompiled.GenericAlgorithms.Transfer<Precompiled.Counter>(move(original));
            if (Precompiled.GenericAlgorithms.Pick<int>(0) != 1) { return 1; }
            if (Precompiled.GenericAlgorithms.Pick<int>(probes) != 2) { return 2; }
            if (Precompiled.GenericAlgorithms.AddDefault<int>(0) != 2) { return 3; }
            if (transferred.Read() != 37) { return 4; }
            if (Precompiled.GenericAlgorithms.ClosureRoot<int>(42) != 42) { return 5; }
            if (Precompiled.ClosureOuter.NestedAlgorithms.NestedRoot<int>(42) != 42) { return 6; }
            return box.Read() + (int)fresh.Read() + Precompiled.Math.Add(0, 0);
        }
    }
}
)NS");
    writeFile(genericDllConsumer / "GenericConsumer.nsproj", "<Project><PropertyGroup><Name>GenericDllConsumer</Name>"
                                                             "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
                                                             "<Compile Include=\"**/*.ns\"/>"
                                                             "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
                                                             "Runtime=\"../Producer/NativeMath.dll\"/>"
                                                             "</ItemGroup></Project>");
    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const auto executable = dllProducer / (optimize ? "GenericDllO2.exe" : "GenericDllO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe", genericDllConsumer / "GenericConsumer.nsproj",
                                            executable, output, error, optimize);
        tests.expect(result == ExitCode::Success, std::string("DLL LibraryReference materializes a new generic "
                                                              "type and method at ") +
                                                      (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         std::string("cross-library DLL generic specializations execute to 42 at ") +
                             (optimize ? "O2" : "O0"));
#endif
    }

    const auto nominalDllConsumer = dllRoot / "NominalConsumer";
    writeFile(nominalDllConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        public static int Main()
        {
            Precompiled.Pair value;
            value.X = 9;
            value.Y = 10;
            Precompiled.Pair copy = Precompiled.Math.Echo(value);
            copy.Offset(1);
            Precompiled.Overlay overlay;
            overlay.Number = 1;
            Precompiled.Packet packet;
            packet.Header.Tag = 1;
            packet.Header.Value = 10;
            packet.Tail = 10;
            Precompiled.Packet packetCopy = Precompiled.Math.EchoPacket(packet);
            Precompiled.Mode mode = Precompiled.Math.EchoMode(Precompiled.Mode.Zero);
            return copy.Sum() + Precompiled.Math.ReadOverlay(overlay) +
                packetCopy.Header.Tag + packetCopy.Header.Value +
                (int)packetCopy.Tail + (int)mode;
        }
    }
}
)NS");
    writeFile(nominalDllConsumer / "NominalConsumer.nsproj",
              "<Project><PropertyGroup><Name>NominalDllConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
              "Runtime=\"../Producer/NativeMath.dll\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : { false, true })
    {
        output.clear();
        error.clear();
        const auto executable = dllProducer / (optimize ? "NominalDllO2.exe" : "NominalDllO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            nominalDllConsumer / "NominalConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     std::string("metadata-backed DLL consumer imports struct "
                                 "layout and enum identity at ") +
                         (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
        {
            tests.expect(runExecutable(executable) == 42,
                         std::string("DLL nominal ABI executes to 42 at ") + (optimize ? "O2" : "O0"));
        }
#endif
    }

    const auto callbackDllConsumer = dllRoot / "CallbackConsumer";
    writeFile(callbackDllConsumer / "Main.ns", R"NS(
namespace Consumer
{
    public class Program
    {
        private static int PlusTwo(int value) { return value + 2; }

        public static int Main()
        {
            Precompiled.Transform callback = Program.PlusTwo;
            return Precompiled.Callbacks.Invoke(callback, 40);
        }
    }
}
)NS");
    writeFile(callbackDllConsumer / "CallbackConsumer.nsproj",
              "<Project><PropertyGroup><Name>CallbackDllConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
              "Runtime=\"../Producer/NativeMath.dll\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : { false, true })
    {
        output.clear();
        error.clear();
        const auto executable = dllProducer / (optimize ? "CallbackDllO2.exe" : "CallbackDllO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            callbackDllConsumer / "CallbackConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     std::string("metadata-backed DLL consumer reconstructs an "
                                                              "extern C delegate callback at ") +
                         (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         std::string("DLL precompiled callback executes to 42 at ") +
                             (optimize ? "O2" : "O0"));
#endif
    }

    const auto managedDllConsumer = dllRoot / "ManagedDelegateConsumer";
    writeFile(managedDllConsumer / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        raii Precompiled.ManagedTransform direct = Precompiled.ManagedCallbacks.MakeStatic();
        raii Precompiled.ManagedTransform closure = Precompiled.ManagedCallbacks.MakeClosure(2);
        raii Precompiled.ManagedCallbacks target = new Precompiled.ManagedCallbacks(2);
        raii Precompiled.ManagedTransform instance =
            Precompiled.ManagedCallbacks.MakeInstance(target);
        return Precompiled.ManagedCallbacks.Invoke(direct, 10) +
            Precompiled.ManagedCallbacks.Invoke(closure, 10) +
            Precompiled.ManagedCallbacks.Invoke(instance, 8) + 8;
    }
}
)NS");
    writeFile(managedDllConsumer / "ManagedDelegateConsumer.nsproj",
              "<Project><PropertyGroup><Name>ManagedDelegateDllConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
              "Runtime=\"../Producer/NativeMath.dll\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const auto executable = dllProducer /
            (optimize ? "ManagedDelegateDllO2.exe" : "ManagedDelegateDllO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            managedDllConsumer / "ManagedDelegateConsumer.nsproj",
                                            executable, output, error, optimize);
        tests.expect(result == ExitCode::Success,
                     std::string("ordinary delegates and closures cross a DLL boundary at ") +
                         (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         std::string("DLL delegate code/context/destructor ABI executes to 42 at ") +
                             (optimize ? "O2" : "O0"));
#endif
    }

    const auto exceptionDllConsumer = dllRoot / "ExceptionConsumer";
    writeFile(exceptionDllConsumer / "Main.ns", R"NS(
using Precompiled;

public class Program
{
    public static int Main()
    {
        int finallyCount = 0;
        raii Problem problem = new Problem(40);
        try
        {
            CrossModuleExceptions.Raise(problem);
        }
        catch (Problem caught)
        {
            return caught.Code + finallyCount + 2;
        }
        finally
        {
            finallyCount += 1;
        }
        return 1;
    }
}
)NS");
    writeFile(exceptionDllConsumer / "ExceptionConsumer.nsproj",
              "<Project><PropertyGroup><Name>ExceptionDllConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
              "Runtime=\"../Producer/NativeMath.dll\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : {false, true})
    {
        output.clear();
        error.clear();
        const auto executable = dllProducer /
            (optimize ? "ExceptionDllO2.exe" : "ExceptionDllO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            exceptionDllConsumer / "ExceptionConsumer.nsproj",
                                            executable, output, error, optimize);
        tests.expect(result == ExitCode::Success,
                     std::string("typed exceptions, RTTI and finally cross a DLL boundary at ") +
                         (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
            tests.expect(runExecutable(executable) == 42,
                         std::string("DLL exception identity and cleanup execute to 42 at ") +
                             (optimize ? "O2" : "O0"));
#endif
    }

    const auto opaqueDllConsumer = dllRoot / "OpaqueClassConsumer";
    writeFile(opaqueDllConsumer / "Main.ns", R"NS(
namespace Consumer
{
    using Precompiled;

    public sealed class LocalCounter : CounterBase, IScore
    {
        public LocalCounter(int value) : base(value) { }
        public override int Score() { return Value + 2; }
    }

    public class Program
    {
        public static int Main()
        {
            raii Precompiled.Counter value = new Precompiled.Counter(40);
            int answer = value.Add(2);
            if (Precompiled.Objects.Read(value) != 42) { return 1; }
            raii Precompiled.BonusCounter polymorphic = new Precompiled.BonusCounter(40);
            borrowed Precompiled.CounterBase baseView = polymorphic;
            borrowed Precompiled.IScore interfaceView = polymorphic;
            if (baseView.Score() != 42) { return 2; }
            if (interfaceView.Score() != 42) { return 3; }
            if (Precompiled.GenericAlgorithms.ScoreOf<Precompiled.BonusCounter>(polymorphic) != 42)
            {
                return 4;
            }
            raii LocalCounter local = new LocalCounter(40);
            borrowed Precompiled.CounterBase localBase = local;
            borrowed Precompiled.IScore localInterface = local;
            if (local.Score() != 42) { return 6; }
            if (localBase.Score() != 42) { return 7; }
            if (localInterface.Score() != 42) { return 8; }
            Precompiled.Lifetime.Reset();
            {
                raii Precompiled.Tracked tracked = new Precompiled.Tracked();
            }
            if (Precompiled.Lifetime.Destroyed() != 1) { return 5; }
            return answer;
        }
    }
}
)NS");
    writeFile(opaqueDllConsumer / "OpaqueClassConsumer.nsproj",
              "<Project><PropertyGroup><Name>OpaqueClassDllConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
              "Runtime=\"../Producer/NativeMath.dll\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : { false, true })
    {
        output.clear();
        error.clear();
        const auto executable = dllProducer / (optimize ? "OpaqueClassDllO2.exe" : "OpaqueClassDllO0.exe");
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            opaqueDllConsumer / "OpaqueClassConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     std::string("metadata-backed DLL consumer constructs a "
                                                              "precompiled class and calls a "
                                 "concrete instance receiver at ") +
                         (optimize ? "O2 (" : "O0 (") + error + ")");
#ifdef _WIN32
        if (result == ExitCode::Success)
        {
            tests.expect(runExecutable(executable) == 42,
                         std::string("DLL precompiled construction and ownership execute to 42 at ") +
                             (optimize ? "O2" : "O0"));
        }
#endif
    }

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
    writeFile(automaticDllConsumer / "AutomaticConsumer.nsproj",
              "<Project><PropertyGroup><Name>AutomaticDllConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
              "Runtime=\"../Producer/NativeMath.dll\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto automaticDllExecutable = dllProducer / "AutomaticConsumer.exe";
    const ExitCode automaticDllResult = emitLibrary("--emit-project-oop-exe",
                                                    automaticDllConsumer / "AutomaticConsumer.nsproj",
                                                    automaticDllExecutable,
                                                    output,
                                                    error,
                                                    true);
    tests.expect(automaticDllResult == ExitCode::Success,
                 "metadata-backed DLL consumer resolves NativeSharp overloads "
                 "without an extern C redeclaration (" +
                     error + ")");
#ifdef _WIN32
    if (automaticDllResult == ExitCode::Success)
    {
        tests.expect(runExecutable(automaticDllExecutable) == 42,
                     "metadata-backed DLL overload calls use the import library "
                     "and execute to 42");
    }
#endif

    const auto compositeDllProducer = dllRoot / "CompositeProducer";
    writeFile(compositeDllProducer / "Main.ns", R"NS(
namespace Precompiled
{
    public class CompositeDll
    {
        public static int Answer()
        {
            Box<long> value;
            value.Value = GenericAlgorithms.Identity<long>((long)41);
            return (int)value.Read() + Math.Add(0, 1);
        }
    }
}
)NS");
    writeFile(compositeDllProducer / "CompositeDllProducer.nsproj",
              "<Project><PropertyGroup><Name>CompositeDllProducer</Name>"
              "<OutputType>SharedLibrary</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
              "Runtime=\"../Producer/NativeMath.dll\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    const auto compositeDll = compositeDllProducer / "CompositeNative.dll";
    const ExitCode compositeDllResult = emitLibrary("--emit-project-oop-dll",
                                                    compositeDllProducer / "CompositeDllProducer.nsproj",
                                                    compositeDll,
                                                    output,
                                                    error,
                                                    true);
    tests.expect(compositeDllResult == ExitCode::Success,
                 "DLL producer consumes a dynamic nominal dependency without "
                 "source sharing (" +
                     error + ")");
    tests.expect(std::filesystem::exists(compositeDllProducer / "NativeMath.dll"),
                 "DLL packaging places the explicitly imported producer runtime beside the dependent DLL");

    const auto compositeDllConsumer = dllRoot / "CompositeConsumer";
    writeFile(compositeDllConsumer / "Main.ns", R"NS(
public class Program
{
    public static int Main() { return Precompiled.CompositeDll.Answer(); }
}
)NS");
    writeFile(compositeDllConsumer / "CompositeDllConsumer.nsproj",
              "<Project><PropertyGroup><Name>CompositeDllConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../CompositeProducer/CompositeNative.lib\" "
              "Runtime=\"../CompositeProducer/CompositeNative.dll\"/>"
              "</ItemGroup></Project>");
    for (const bool optimize : { false, true })
    {
        output.clear();
        error.clear();
        const auto packageRoot = usesNsLinker()
                                     ? root / "package_outside_checkout"
                                     : root / std::filesystem::path(L"paquete_áé_运行");
        const auto packageDirectory = packageRoot / (optimize ? "O2" : "O0");
        std::filesystem::create_directories(packageDirectory);
        const auto executable = packageDirectory / "CompositeConsumer.exe";
        const ExitCode result = emitLibrary("--emit-project-oop-exe",
                                            compositeDllConsumer / "CompositeDllConsumer.nsproj",
                                            executable,
                                            output,
                                            error,
                                            optimize);
        tests.expect(result == ExitCode::Success,
                     std::string("consumer links only the top-level DLL of a "
                                 "nominal dependency chain at ") +
                         (optimize ? "O2 (" : "O0 (") + error + ")");
        PeExecutableVerifier compositeConsumerVerifier;
        tests.expect(result == ExitCode::Success && compositeConsumerVerifier.verify(executable) &&
                         contains(compositeConsumerVerifier.info().importedLibraries, "CompositeNative.dll") &&
                         !contains(compositeConsumerVerifier.info().importedLibraries, "NativeMath.dll"),
                     "the final PE imports only the top-level DLL while its private NativeMath dependency stays transitive");
        tests.expect(result == ExitCode::Success &&
                         std::filesystem::exists(packageDirectory / "CompositeNative.dll") &&
                         std::filesystem::exists(packageDirectory / "NativeMath.dll"),
                     std::string("runtime packaging follows the exact PE dependency closure into an out-of-checkout path at ") +
                         (optimize ? "O2" : "O0"));
#ifdef _WIN32
        if (result == ExitCode::Success)
        {
            tests.expect(runExecutable(executable) == 42,
                         std::string("transitive nominal DLL composition executes to 42 at ") +
                             (optimize ? "O2" : "O0"));
        }
#endif
    }

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
    tests.expect(
        emitLibrary("--emit-project-oop-dll", wrongProducer / "WrongProducer.nsproj", wrongDll, output, error, true) ==
            ExitCode::Success,
        "mismatched-runtime negative fixture builds its own valid DLL");

    const auto basenameProducer = dllRoot / "BasenameCollisionProducer";
    writeProject(basenameProducer, "BasenameCollisionProducer", "SharedLibrary", R"NS(
namespace Collision
{
    public class Service
    {
        public static export extern C int OtherAnswer() { return 42; }
    }
}
)NS");
    output.clear();
    error.clear();
    const auto collidingDll = basenameProducer / "NativeMath.dll";
    tests.expect(emitLibrary("--emit-project-oop-dll",
                             basenameProducer / "BasenameCollisionProducer.nsproj",
                             collidingDll,
                             output,
                             error,
                             true) == ExitCode::Success,
                 "basename-collision fixture emits a second valid NativeMath.dll with a distinct module contract");

    const auto basenameConsumer = dllRoot / "BasenameCollisionConsumer";
    writeFile(basenameConsumer / "Main.ns", R"NS(
public class Program
{
    public static int Main() { return Precompiled.Math.Add(20, 22); }
}
)NS");
    writeFile(basenameConsumer / "BasenameCollisionConsumer.nsproj",
              "<Project><PropertyGroup><Name>BasenameCollisionConsumer</Name>"
              "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
              "<Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
              "Runtime=\"../Producer/NativeMath.dll\"/>"
              "<LibraryReference Include=\"../BasenameCollisionProducer/NativeMath.lib\" "
              "Runtime=\"../BasenameCollisionProducer/NativeMath.dll\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary("--emit-project-oop-exe",
                             basenameConsumer / "BasenameCollisionConsumer.nsproj",
                             basenameConsumer / "BasenameCollisionConsumer.exe",
                             output,
                             error,
                             true) == ExitCode::CompilationError,
                 "two module contracts cannot claim distinct runtime files with the same DLL basename");
    tests.expect(error.find("require different DLLs named 'NativeMath.dll'") != std::string::npos,
                 "DLL basename collision is diagnosed before link and packaging");

    const auto wrongNativeArchiveConsumer = dllRoot / "WrongNativeArchiveConsumer";
    writeFile(wrongNativeArchiveConsumer / "Main.ns", R"NS(
public class Program
{
    public static int Main() { return Precompiled.Math.Blend(20); }
}
)NS");
    writeFile(wrongNativeArchiveConsumer / "WrongNativeArchiveConsumer.nsproj",
              "<Project><PropertyGroup><Name>WrongNativeArchiveConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../WrongProducer/Wrong.lib\" "
              "Metadata=\"../Producer/NativeMath.nsmeta\" "
              "Runtime=\"../WrongProducer/Wrong.dll\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary("--emit-project-oop-exe",
                             wrongNativeArchiveConsumer / "WrongNativeArchiveConsumer.nsproj",
                             wrongNativeArchiveConsumer / "WrongNativeArchiveConsumer.exe",
                             output,
                             error,
                             true) == ExitCode::CompilationError,
                 "a same-shaped C import library cannot impersonate a NativeSharp module");
    tests.expect(error.find("does not contain NativeSharp symbol") != std::string::npos,
                 "native archive/metadata mismatch reports the missing mangled "
                 "symbol before link");

    const auto mismatchConsumer = dllRoot / "MismatchConsumer";
    writeFile(mismatchConsumer / "Main.ns", consumerSource);
    writeFile(mismatchConsumer / "MismatchConsumer.nsproj",
              "<Project><PropertyGroup><Name>MismatchConsumer</"
              "Name><OutputType>Exe</OutputType>"
              "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/>"
              "<LibraryReference Include=\"../Producer/NativeMath.lib\" "
              "Metadata=\"../Producer/NativeMath.nsmeta\" "
              "Runtime=\"../WrongProducer/Wrong.dll\"/>"
              "</ItemGroup></Project>");
    output.clear();
    error.clear();
    tests.expect(emitLibrary("--emit-project-oop-exe",
                             mismatchConsumer / "MismatchConsumer.nsproj",
                             mismatchConsumer / "MismatchConsumer.exe",
                             output,
                             error,
                             true) == ExitCode::CompilationError,
                 "LibraryReference rejects a runtime DLL that does not match its "
                 "metadata");
    tests.expect(error.find("runtime DLL export table does not match metadata contract") != std::string::npos ||
                     error.find("linked PE does not import declared runtime DLL") != std::string::npos,
                 "same-shaped wrong DLL is rejected by its NativeSharp export set or "
                 "final PE import identity");
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
    tests.expect(emitLibrary("--emit-project-oop-lib",
                             directory / "WrongKind.nsproj",
                             directory / "WrongKind.lib",
                             output,
                             error,
                             false) == ExitCode::CompilationError,
                 "static-library emission rejects an Exe root project");
    tests.expect(error.find("requires root OutputType Library or StaticLibrary") != std::string::npos,
                 "wrong static-library project kind reports a precise Stage 46 "
                 "diagnostic");

    output.clear();
    error.clear();
    tests.expect(emitLibrary("--emit-project-oop-dll",
                             directory / "WrongKind.nsproj",
                             directory / "WrongKind.dll",
                             output,
                             error,
                             false) == ExitCode::CompilationError,
                 "DLL emission rejects an Exe root project");
    tests.expect(error.find("requires root OutputType SharedLibrary") != std::string::npos,
                 "wrong DLL project kind reports a precise Stage 46 diagnostic");
}
} // namespace

int main()
{
    TestContext tests;
    const std::filesystem::path root = nativesharp::tests::evidenceRoot("nativesharp_stage46_native_libraries");
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root, cleanupError);

    testProjectKinds(tests, root);
    testRuntimeSupportMode(tests);
    testStaticLibrary(tests, root);
    testDynamicLibrary(tests, root);
    testPrecompiledDllRegressionCorpus(tests, root);
    testDllP4ImportedSignaturesAndEvents(tests, root);
    testDllP4ImportedBaseContract(tests, root);
    testCompileOnlyMetadataReferenceDoesNotPromiseRuntime(tests, root);
    testDllP4ProtectedConsumerCallableContract(tests, root);
    testDllP4ProtectedVirtualAccessors(tests, root);
    testDllP4QualifiedBaseIdentity(tests, root);
    testDllP4FoundationAndImportedCleanup(tests, root);
    testDllP4PrivateStagingSurface(tests, root);
    testPrecompiledStructConstructorContract(tests, root);
    testPrecompiledNumericConstantContract(tests, root);
    testPrecompiledStringConstantContract(tests, root);
    testPrecompiledNewVirtualSlotContract(tests, root);
    testPrecompiledTypeSemanticFlags(tests, root);
    testPrecompiledOperatorsAndConversions(tests, root);
    testPrecompiledMutableStaticFieldContract(tests, root);
    testPrecompiledImplicitClassConstructionContract(tests, root);
    testPrecompiledNestedTypeContract(tests, root);
    testPrecompiledExternCImportContract(tests, root);
    testMetadataOnlyDllContract(tests, root);
    testSynthesizedSymbolHygiene(tests, root);
    testPrecompiledLibraryReferences(tests, root);
    testOutputKindGuards(tests, root);

    return tests.resultKeeping(root);
}
