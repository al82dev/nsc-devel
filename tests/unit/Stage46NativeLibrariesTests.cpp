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
    const std::string projectText = project.string();
    const std::string outputText = outputPath.string();
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
    tests.expect(info.exportedSymbols.size() == 5 &&
                     std::ranges::any_of(info.exportedSymbols, [](const std::string& symbol)
                                         { return symbol.starts_with("__nsc_get_classdesc_"); }),
                 "DLL exports each public method plus the public class "
                 "descriptor required "
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
        public static T Identity<T>(T value) { return value; }
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
            Precompiled.Packet packet;
            packet.Header.Tag = 1;
            packet.Header.Value = 10;
            packet.Tail = 11;
            Precompiled.Packet packetCopy = Precompiled.Math.EchoPacket(packet);
            Precompiled.Mode mode = Precompiled.Math.EchoMode(Precompiled.Mode.Zero);
            return copy.Sum() + packetCopy.Header.Tag + packetCopy.Header.Value +
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
        return Precompiled.GenericAlgorithms.Secret<int>(42);
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
                 "consumer cannot instantiate a private generic method omitted from "
                 "NSMETA");
    tests.expect(error.find("does not name a unique visible") != std::string::npos,
                 "unpublished generic method fails during generic selection, "
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
            Precompiled.Packet packet;
            packet.Header.Tag = 1;
            packet.Header.Value = 10;
            packet.Tail = 11;
            Precompiled.Packet packetCopy = Precompiled.Math.EchoPacket(packet);
            Precompiled.Mode mode = Precompiled.Math.EchoMode(Precompiled.Mode.Zero);
            return copy.Sum() + packetCopy.Header.Tag + packetCopy.Header.Value +
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
              "<LibraryReference "
              "Include=\"../../PrecompiledStatic/Producer/NativeMath.lib\"/>"
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
                 "DLL producer consumes a static nominal dependency without "
                 "source sharing (" +
                     error + ")");

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
        const auto executable =
            compositeDllProducer / (optimize ? "CompositeConsumerO2.exe" : "CompositeConsumerO0.exe");
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
                     "the final PE imports the top-level DLL, while its NativeMath "
                     "dependency stays static");
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
    testPrecompiledLibraryReferences(tests, root);
    testOutputKindGuards(tests, root);

    return tests.resultKeeping(root);
}
