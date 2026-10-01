#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::interop::LibraryMetadata;
using nativesharp::interop::LibraryMetadataCodec;
using nativesharp::interop::LibraryMetadataDefaultValueKind;
using nativesharp::interop::LibraryMetadataMember;
using nativesharp::interop::LibraryMetadataParameterPassKind;
using nativesharp::interop::LibraryMetadataType;
using nativesharp::interop::LibraryMetadataTypeKind;
using nativesharp::tests::TestContext;

bool environmentFlag(const char* name)
{
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) return false;
    const bool enabled = std::string_view(value) == "1";
    std::free(value);
    return enabled;
#else
    const char* value = std::getenv(name);
    return value != nullptr && std::string_view(value) == "1";
#endif
}

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

void writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::uint16_t readU16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

void writeU16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
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
                                                               "</OutputType></PropertyGroup><ItemGroup><Compile "
                                                               "Include=\"**/*.ns\" />"
            "</ItemGroup></Project>");
    writeFile(directory / "Api.ns", source);
}

ExitCode runDriver(
    std::span<const std::string_view> args,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode code = driver.run(args, out, err);
    output = out.str();
    error = err.str();
    return code;
}

ExitCode emitMetadata(
    const std::filesystem::path& project,
    const std::filesystem::path& destination,
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::string destinationText = destination.string();
    const std::array<std::string_view, 4> args = {
        "--emit-project-library-metadata", projectText, "-o", destinationText};
    return runDriver(args, output, error);
}

ExitCode verifyMetadata(
    const std::filesystem::path& source,
    std::string& output,
    std::string& error)
{
    const std::string sourceText = source.string();
    const std::array<std::string_view, 2> args = {"--verify-library-metadata", sourceText};
    return runDriver(args, output, error);
}

ExitCode verifyArtifacts(const std::filesystem::path& metadata, const std::filesystem::path& library,
                         const std::filesystem::path& runtime, std::string& output, std::string& error)
{
    const std::string metadataText = metadata.string();
    const std::string libraryText = library.string();
    if (runtime.empty())
    {
        const std::array<std::string_view, 3> args = {"--verify-library-artifacts", metadataText, libraryText};
        return runDriver(args, output, error);
    }
    const std::string runtimeText = runtime.string();
    const std::array<std::string_view, 4> args = {"--verify-library-artifacts", metadataText, libraryText, runtimeText};
    return runDriver(args, output, error);
}

ExitCode emitProduct(
    std::string_view action,
    const std::filesystem::path& project,
    const std::filesystem::path& destination,
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::string destinationText = destination.string();
    const std::array<std::string_view, 5> args = {"-O2", action, projectText, "-o", destinationText};
    return runDriver(args, output, error);
}

const LibraryMetadataType* findType(const LibraryMetadata& metadata, std::string_view suffix)
{
    const auto found = std::find_if(metadata.types.begin(), metadata.types.end(), [suffix](const auto& type) {
        return type.identityKey.size() >= suffix.size() && type.identityKey.ends_with(suffix);
    });
    return found == metadata.types.end() ? nullptr : &*found;
}

const char* richSource()
{
    return R"NS(
namespace Meta
{
    public interface IValue
    {
        int Get();
    }

    public class Base
    {
        public int Seed;
        public virtual int Get() { return Seed; }
    }

    public class Derived : Base, IValue
    {
        public int Extra;
        public override int Get() { return Seed + Extra; }
    }

    public struct Pair
    {
        public int X;
        public short Y;
    }

    public enum Mode
    {
        First = 1,
        Second = 2
    }

    // Enums CON tipo subyacente declarado. Los metadatos alimentan a los GENERADORES DE HOST (el
    // enlace P/Invoke de C# y el de C++/CLI), asi que un tamano fijo de 4 aqui se propaga a codigo
    // que otro compilador genera y que ya nadie relaciona con este fichero.
    public enum Narrow : byte
    {
        Low = 0,
        High = 200
    }

    public enum Wide : long
    {
        Zero = 0,
        Huge = 5000000000
    }

    extern C delegate int Transform(int value);
    public delegate int ManagedTransform(int value);

    public class Api
    {
        public const int Answer = 42;
        public const string Label = "NativeSharp";
        extern C from "kernel32.dll" { public uint GetCurrentThreadId(); }
        public static export extern C int Add(int left, int right) { return left + right; }
        public static export extern C Pair Echo(Pair value) { return value; }
        public static export extern C int Invoke(Transform callback, int value) { return callback(value); }
        public static int Internal(int value) { return value + Answer; }
        public static void Mutate(ref int value) { value += 1; }
        public static void Assign(out int value) { value = 42; }
        public static int Read(in int value) { return value; }
        public static int Defaults(
            int required,
            int offset = -7,
            double scale = -0.5,
            char marker = 'x',
            bool enabled = true,
            string label = "meta",
            int* pointer = null)
        {
            return required + offset;
        }
    }
}
)NS";
}

void testExplicitRoundTrip(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "RoundTrip";
    writeProject(directory, "MetaLib", "SharedLibrary", richSource());
    const auto metadataPath = directory / "generated" / "MetaLib.nsmeta";
    std::string output;
    std::string error;
    tests.expect(emitMetadata(directory / "MetaLib.nsproj", metadataPath, output, error) == ExitCode::Success,
        "Stage 48.2 emits .nsmeta explicitly without linking the binary product");
    tests.expect(error.empty() && std::filesystem::is_regular_file(metadataPath),
                 "explicit metadata generation creates parent directories and "
                 "keeps stderr empty");
    const std::string metadataFormat =
        std::to_string(nativesharp::interop::kMetadataFormatMajor) + "." +
        std::to_string(nativesharp::interop::kMetadataFormatMinor);
    tests.expect(output.find("format: NSMETA " + metadataFormat) != std::string::npos &&
            output.find("abi-fingerprint:") != std::string::npos,
        "explicit metadata emission reports format and ABI fingerprint");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    tests.expect(codec.read(metadataPath, metadata, error), "generated metadata round-trips through the Stage 48.2 reader");
    tests.expect(error.empty() && codec.verifyCurrentCompiler(metadata, error),
        "round-tripped metadata is compatible with the current compiler/target");
    tests.expect(metadata.producerVersion == nativesharp::kCompilerVersion && metadata.moduleName == "MetaLib",
        "metadata persists exact producer and module identity");
    tests.expect(metadata.targetTriple == "x86_64-pc-windows-msvc" && metadata.pointerSize == 8 && metadata.pointerAlignment == 8,
        "metadata freezes the NativeSharp 1.0.1 Windows x64 target contract");
    tests.expect(metadata.types.size() == 10, "all root-module named types are persisted as compiler metadata");
    tests.expect(metadata.exports.size() == 3, "three explicit Stage47 C ABI exports are persisted");
    tests.expect(LibraryMetadataCodec::fingerprintHex(metadata.abiFingerprint).size() == 64,
        "ABI fingerprint is a full 256-bit SHA-256 value");
    tests.expect(metadata.nativeAbiMajor == 1 && metadata.nativeAbiMinor == 0 &&
            metadata.runtimeAbiMajor == 1 && metadata.runtimeAbiMinor == 0 &&
            metadata.manglingAbiVersion == 1 && metadata.objectAbiVersion == 2 &&
            metadata.delegateAbiVersion == 1 && metadata.exceptionAbiVersion == 1 &&
            metadata.managedDelegateCellSize == 32,
                 "NSMETA 1.8 separates its file version from the frozen "
                 "NativeSharp/runtime ABI contract");

    const LibraryMetadataType* pair = findType(metadata, "::Meta.Pair");
    tests.expect(pair != nullptr && pair->kind == LibraryMetadataTypeKind::Struct,
        "struct type identity survives metadata serialization");
    tests.expect(pair != nullptr && pair->size == 8 && pair->alignment == 4 && pair->fields.size() == 2,
        "struct physical layout survives metadata serialization");
    tests.expect(pair != nullptr && pair->fields[0].name == "X" && pair->fields[0].offset == 0 &&
            pair->fields[1].name == "Y" && pair->fields[1].offset == 4,
                 "field names and exact offsets are preserved for compile-time "
                 "layout semantics");

    // EL TAMANO DEL ENUM EN LOS METADATOS. Hasta el 2026-09-03 se escribia 4/4
    // para todos, sin preguntar. Un `enum : byte` exportado hacia entonces que el
    // host generado reservara cuatro bytes donde el modulo pone uno -- el mismo
    // defecto de ABI que el tipo subyacente vino a cerrar, en la salida que menos
    // se mira.
    const LibraryMetadataType* mode = findType(metadata, "::Meta.Mode");
    tests.expect(mode != nullptr && mode->size == 4 && mode->alignment == 4,
        "an enum with no declared underlying type stays 4/4 in metadata");
    const LibraryMetadataType* narrow = findType(metadata, "::Meta.Narrow");
    tests.expect(narrow != nullptr && narrow->size == 1 && narrow->alignment == 1,
        "`enum Narrow : byte` is persisted as 1/1, not 4/4");
    // EL SIGNO, que el tamano NO lleva. `enum : byte` y `enum : sbyte` miden los
    // dos un byte: un generador de host que solo leyera el tamano no podria
    // distinguirlos, y elegir el signo contrario convertiria un 200 en un -56.
    tests.expect(narrow != nullptr && narrow->enumUnderlyingTypeKey == "builtin:byte",
                 "metadata records the enum's underlying type key, so size alone "
                 "need not carry the sign");
    tests.expect(mode != nullptr && mode->enumUnderlyingTypeKey == "builtin:int",
                 "an enum with no declared underlying type records builtin:int "
                 "explicitly");
    const LibraryMetadataType* wide = findType(metadata, "::Meta.Wide");
    tests.expect(wide != nullptr && wide->size == 8 && wide->alignment == 8,
        "`enum Wide : long` is persisted as 8/8");
    tests.expect(wide != nullptr && wide->enumUnderlyingTypeKey == "builtin:long",
        "`enum Wide : long` records builtin:long as its underlying type");
    tests.expect(pair != nullptr && pair->enumUnderlyingTypeKey.empty(),
        "a struct records no enum underlying type");

    const LibraryMetadataType* transform = findType(metadata, "::Meta.Transform");
    tests.expect(transform != nullptr && transform->kind == LibraryMetadataTypeKind::Delegate &&
            transform->delegateFlags == 1U && transform->delegateReturnTypeKey == "builtin:int" &&
            transform->delegateParameterTypeKeys == std::vector<std::string>{"builtin:int"},
                 "NSMETA 1.8 preserves the complete extern C delegate signature "
                 "introduced in 1.5");
    const LibraryMetadataType* managedTransform = findType(metadata, "::Meta.ManagedTransform");
    tests.expect(managedTransform != nullptr && managedTransform->kind == LibraryMetadataTypeKind::Delegate &&
            managedTransform->delegateFlags == 0U && managedTransform->referenceSize == 8 &&
            managedTransform->referenceAlignment == 8 &&
            managedTransform->delegateReturnTypeKey == "builtin:int",
                 "NSMETA 1.8 distinguishes a managed delegate cell from a raw "
                 "callback and freezes its pointer carrier");

    const LibraryMetadataType* derived = findType(metadata, "::Meta.Derived");
    tests.expect(derived != nullptr && derived->kind == LibraryMetadataTypeKind::Class &&
            derived->baseTypeKey.find("Meta.Base") != std::string::npos,
        "class base identity is persisted");
    tests.expect(derived != nullptr && !derived->interfaceTypeKeys.empty() &&
            derived->interfaceTypeKeys.front().find("Meta.IValue") != std::string::npos,
        "class interface contracts are persisted");
    tests.expect(derived != nullptr && derived->size != 0 && derived->referenceSize == 8,
                 "class object layout and reference ABI are both persisted "
                 "without exposing them as C structs");

    tests.expect(std::ranges::any_of(metadata.members, [](const auto& member) {
        return member.name == "Answer" && member.constantKind == 1 && member.constantBits == 42;
    }), "compile-time integral const values survive precompiled metadata");
    tests.expect(std::ranges::any_of(metadata.members, [](const auto& member) {
        return member.name == "Label" && member.constantKind == 6 && member.constantText == "NativeSharp";
    }), "NSMETA 1.9 preserves the UTF-8 payload of a compile-time string constant");
    tests.expect(std::ranges::any_of(metadata.members, [](const auto& member) {
        return member.name == "GetCurrentThreadId" && member.importDllName == "kernel32.dll" &&
            (member.flags & (1U << 20U)) != 0U;
    }), "NSMETA 1.10 preserves the owning DLL of a public imported C declaration");
    tests.expect(std::ranges::any_of(metadata.members, [](const auto& member) {
        return member.name == "Add" && !member.mangledName.empty() && member.cAbiName == "Add";
    }),
                 "metadata records both NativeSharp binary symbol identity and "
                 "stable C ABI name");
    const auto mutate = std::ranges::find_if(metadata.members, [](const auto& member) {
        return member.name == "Mutate";
    });
    const auto assign = std::ranges::find_if(metadata.members, [](const auto& member) {
        return member.name == "Assign";
    });
    const auto read = std::ranges::find_if(metadata.members, [](const auto& member) {
        return member.name == "Read";
    });
    tests.expect(mutate != metadata.members.end() && mutate->parameters.size() == 1 &&
            mutate->parameters[0].passKind == LibraryMetadataParameterPassKind::Ref &&
            assign != metadata.members.end() && assign->parameters.size() == 1 &&
            assign->parameters[0].passKind == LibraryMetadataParameterPassKind::Out &&
            read != metadata.members.end() && read->parameters.size() == 1 &&
            read->parameters[0].passKind == LibraryMetadataParameterPassKind::In,
                 "NSMETA preserves ref, out, and in independently from their pointer "
                 "carrier");
    const auto defaults = std::ranges::find_if(metadata.members, [](const auto& member) {
        return member.name == "Defaults";
    });
    tests.expect(defaults != metadata.members.end() && defaults->parameters.size() == 7,
                 "precompiled metadata preserves the complete optional-parameter "
                 "contract");
    if (defaults != metadata.members.end() && defaults->parameters.size() == 7)
    {
        const auto& parameters = defaults->parameters;
        tests.expect(parameters[0].defaultValueKind == LibraryMetadataDefaultValueKind::None,
            "required parameters remain distinguishable from optional parameters");
        tests.expect(parameters[1].defaultValueKind == LibraryMetadataDefaultValueKind::Integer &&
                parameters[1].defaultValueFlags == 1U && parameters[1].defaultValueBits == 7U,
            "negative integer defaults preserve sign and magnitude");
        tests.expect(parameters[2].defaultValueKind == LibraryMetadataDefaultValueKind::Floating &&
                std::bit_cast<double>(parameters[2].defaultValueBits) == -0.5,
            "floating defaults preserve their exact parsed IEEE-754 value");
        tests.expect(parameters[3].defaultValueKind == LibraryMetadataDefaultValueKind::Character &&
                parameters[3].defaultValueBits == static_cast<std::uint64_t>('x'),
            "character defaults survive the library boundary");
        tests.expect(parameters[4].defaultValueKind == LibraryMetadataDefaultValueKind::Boolean &&
                parameters[4].defaultValueBits == 1U,
            "boolean defaults survive the library boundary");
        tests.expect(parameters[5].defaultValueKind == LibraryMetadataDefaultValueKind::String &&
                parameters[5].defaultValueText == "meta",
                     "string defaults survive the library boundary without "
                     "retaining source spelling");
        tests.expect(parameters[6].defaultValueKind == LibraryMetadataDefaultValueKind::Null,
                     "null defaults survive the library boundary without an "
                     "invented payload");
    }
    tests.expect(std::ranges::any_of(metadata.dispatchTables, [](const auto& dispatch) {
        return dispatch.typeKey.find("Meta.Derived") != std::string::npos &&
            !dispatch.descriptorAccessorName.empty() && !dispatch.slotSignatures.empty();
    }),
                 "virtual dispatch metadata includes the producer-owned descriptor "
                 "accessor and its slots");
}

void testDeterminismAndAbiSensitivity(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Determinism";
    writeProject(directory, "Stable", "StaticLibrary", richSource());
    std::string output;
    std::string error;
    const auto first = directory / "first.nsmeta";
    const auto second = directory / "second.nsmeta";
    tests.expect(emitMetadata(directory / "Stable.nsproj", first, output, error) == ExitCode::Success,
        "first deterministic metadata emission succeeds");
    output.clear(); error.clear();
    tests.expect(emitMetadata(directory / "Stable.nsproj", second, output, error) == ExitCode::Success,
        "second deterministic metadata emission succeeds");
    tests.expect(readBytes(first) == readBytes(second), "same semantic input produces byte-for-byte identical .nsmeta");

    LibraryMetadataCodec codec;
    LibraryMetadata a;
    LibraryMetadata b;
    tests.expect(codec.read(first, a, error) && codec.read(second, b, error) && a.abiFingerprint == b.abiFingerprint,
        "same semantic input produces the same ABI fingerprint");

    const auto changed = root / "AbiChanged";
    writeProject(changed, "Stable", "StaticLibrary", R"NS(
public class Api
{
    public static export extern C long Add(long left, long right) { return left + right; }
}
)NS");
    const auto changedMetadata = changed / "changed.nsmeta";
    output.clear(); error.clear();
    tests.expect(emitMetadata(changed / "Stable.nsproj", changedMetadata, output, error) == ExitCode::Success,
        "ABI-changed comparison metadata emits successfully");
    LibraryMetadata c;
    tests.expect(codec.read(changedMetadata, c, error) && c.abiFingerprint != a.abiFingerprint,
                 "changing an exported ABI signature changes the metadata ABI "
                 "fingerprint");

    // Golden mínimo del ABI 1.0: fija el orden y ancho de todos los campos raíz
    // aunque no haya declaraciones. Si alguien reordena
    // mangling/object/delegate/exception o cambia un ancho sin versionarlo, esta
    // huella cambia y la revisión ve un diff concreto, no una sospecha.
    LibraryMetadata golden;
    golden.producerVersion = std::string(nativesharp::kCompilerVersion);
    golden.moduleName = "GoldenAbi10";
    golden.targetTriple = "x86_64-pc-windows-msvc";
    const auto goldenPath = directory / "golden-abi-1.0.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(golden, goldenPath, error),
        "the ABI 1.0 golden metadata model serializes");
    LibraryMetadata goldenDecoded;
    error.clear();
    tests.expect(codec.read(goldenPath, goldenDecoded, error),
        "the ABI 1.0 golden metadata model round-trips");
    const std::string goldenHex = LibraryMetadataCodec::fingerprintHex(goldenDecoded.abiFingerprint);
    tests.expect(goldenHex == "4968fe9c9b16c2a0bb8fe9c15a9dd45492f71ac055c6b59748abaffa582c0fa9",
        "ABI root contract with Object ABI 2 retains its reviewed golden fingerprint (" +
            goldenHex + ")");
}

void testDefaultValueContractSensitivity(TestContext& tests, const std::filesystem::path& root)
{
    const auto firstDirectory = root / "DefaultOne";
    const auto secondDirectory = root / "DefaultTwo";
    writeProject(firstDirectory, "DefaultContract", "StaticLibrary", R"NS(
public class Api
{
    public static int Pick(int value = 1) { return value; }
}
)NS");
    writeProject(secondDirectory, "DefaultContract", "StaticLibrary", R"NS(
public class Api
{
    public static int Pick(int value = 2) { return value; }
}
)NS");

    std::string output;
    std::string error;
    const auto firstPath = firstDirectory / "DefaultContract.nsmeta";
    const auto secondPath = secondDirectory / "DefaultContract.nsmeta";
    tests.expect(emitMetadata(
            firstDirectory / "DefaultContract.nsproj", firstPath, output, error) == ExitCode::Success,
        "the first optional-parameter contract emits metadata");
    output.clear();
    error.clear();
    tests.expect(emitMetadata(
            secondDirectory / "DefaultContract.nsproj", secondPath, output, error) == ExitCode::Success,
        "the changed optional-parameter contract emits metadata");

    LibraryMetadataCodec codec;
    LibraryMetadata first;
    LibraryMetadata second;
    error.clear();
    tests.expect(codec.read(firstPath, first, error) && codec.read(secondPath, second, error),
        "both default-value contracts round-trip through current NSMETA");
    tests.expect(first.abiFingerprint != second.abiFingerprint,
                 "changing only a parameter default changes the published "
                 "library contract fingerprint");
}

void testAutomaticProducts(TestContext& tests, const std::filesystem::path& root)
{
    const auto shared = root / "Shared";
    writeProject(shared, "SharedMeta", "SharedLibrary", richSource());
    std::string output;
    std::string error;
    tests.expect(emitProduct("--emit-project-oop-dll", shared / "SharedMeta.nsproj", shared / "SharedMeta.dll", output, error) == ExitCode::Success,
                 "SharedLibrary product now composes DLL, import LIB, generated "
                 "header and metadata");
    tests.expect(std::filesystem::is_regular_file(shared / "SharedMeta.dll") &&
            std::filesystem::is_regular_file(shared / "SharedMeta.lib") &&
            std::filesystem::is_regular_file(shared / "SharedMeta.h") &&
            std::filesystem::is_regular_file(shared / "SharedMeta.nsmeta"),
        "all four SharedLibrary artifacts exist after one product command");
    tests.expect(output.find("library-metadata:") != std::string::npos,
        "DLL product reports its metadata sidecar");

    output.clear();
    error.clear();
    tests.expect(verifyArtifacts(shared / "SharedMeta.nsmeta", shared / "SharedMeta.lib", shared / "SharedMeta.dll",
                                 output, error) == ExitCode::Success &&
                     output.find("NativeSharp library artifacts: valid") != std::string::npos &&
                     output.find("product-kind: DLL plus import archive") != std::string::npos &&
                     output.find("native-abi: 1.0") != std::string::npos &&
                     output.find("delegate-cell-size: 32") != std::string::npos,
                 "E6 standalone verifier certifies metadata, import archive, DLL "
                 "exports and frozen ABI together");

    const auto genericShared = root / "GenericShared";
    writeProject(genericShared, "GenericShared", "SharedLibrary", R"NS(
namespace GenericDll
{
    public struct Box<T>
    {
        private T value;
        public Box(T item) { value = item; }
        public T Value() { return value; }
    }

    public class Boxes
    {
        public static Box<int> Make() { return new Box<int>(42); }
    }
}
)NS");
    output.clear();
    error.clear();
    tests.expect(emitProduct("--emit-project-oop-dll", genericShared / "GenericShared.nsproj",
                             genericShared / "GenericShared.dll", output, error) == ExitCode::Success,
                 "a DLL with an exported concrete generic struct constructor is emitted (" + error + ")");
    LibraryMetadataCodec genericCodec;
    LibraryMetadata genericMetadata;
    error.clear();
    tests.expect(genericCodec.read(genericShared / "GenericShared.nsmeta", genericMetadata, error),
                 "the concrete generic DLL metadata reads successfully");
    tests.expect(std::ranges::any_of(genericMetadata.members, [](const auto& member) {
        return member.kind == 3U && member.accessibility == 3U &&
               member.mangledName.starts_with("__nsc_ctor_");
    }), "NSMETA records the public constructor of the concrete Box<int> specialization");
    output.clear();
    error.clear();
    tests.expect(verifyArtifacts(genericShared / "GenericShared.nsmeta", genericShared / "GenericShared.lib",
                                 genericShared / "GenericShared.dll", output, error) == ExitCode::Success,
                 "an exported concrete generic struct constructor remains synchronized "
                 "between the DLL export table and NSMETA verification");

    const auto statik = root / "Static";
    writeProject(statik, "StaticMeta", "StaticLibrary", R"NS(
public struct Vec2 { public float X; public float Y; }
public class Math { public static int Internal(int value) { return value; } }
)NS");
    output.clear(); error.clear();
    tests.expect(emitProduct("--emit-project-oop-lib", statik / "StaticMeta.nsproj", statik / "StaticMeta.lib", output, error) == ExitCode::Success,
                 "NativeSharp-only static libraries receive metadata even with "
                 "no C exports");
    tests.expect(std::filesystem::is_regular_file(statik / "StaticMeta.lib") &&
            std::filesystem::is_regular_file(statik / "StaticMeta.h") &&
            std::filesystem::is_regular_file(statik / "StaticMeta.nsmeta"),
                 "static library product composes archive, empty/usable host header and "
                 "NativeSharp metadata");

    output.clear();
    error.clear();
    tests.expect(verifyArtifacts(statik / "StaticMeta.nsmeta", statik / "StaticMeta.lib", {}, output, error) ==
                         ExitCode::Success &&
                     output.find("product-kind: static archive") != std::string::npos &&
                     output.find("required-symbols:") != std::string::npos,
                 "E6 standalone verifier certifies a NativeSharp-only static archive "
                 "without requiring a DLL");

    output.clear();
    error.clear();
    tests.expect(verifyArtifacts(shared / "SharedMeta.nsmeta", statik / "StaticMeta.lib", {}, output, error) ==
                         ExitCode::CompilationError &&
                     error.find("is missing") != std::string::npos &&
                     error.find("required by metadata module 'SharedMeta'") != std::string::npos,
                 "E6 rejects a valid archive paired with metadata from another "
                 "producer and names the missing symbols");

    output.clear();
    error.clear();
    tests.expect(verifyArtifacts(statik / "StaticMeta.nsmeta", statik / "StaticMeta.lib", shared / "SharedMeta.dll",
                                 output, error) == ExitCode::CompilationError &&
                     error.find("export table does not match metadata module 'StaticMeta'") != std::string::npos &&
                     error.find("missing") != std::string::npos && error.find("unexpected") != std::string::npos,
                 "E6 reports both sides of a DLL/metadata export mismatch instead of a "
                 "generic contract failure");

    std::vector<std::uint8_t> truncatedArchive = readBytes(shared / "SharedMeta.lib");
    truncatedArchive.resize(20);
    const auto truncatedArchivePath = shared / "Truncated.lib";
    writeBytes(truncatedArchivePath, truncatedArchive);
    output.clear();
    error.clear();
    tests.expect(verifyArtifacts(shared / "SharedMeta.nsmeta", truncatedArchivePath, shared / "SharedMeta.dll",
                                 output, error) == ExitCode::CompilationError &&
                     error.find("COFF archive") != std::string::npos && error.find("truncated") != std::string::npos,
                 "P6 rejects a physically truncated import archive before inspecting the DLL");

    std::vector<std::uint8_t> corruptIndex = readBytes(shared / "SharedMeta.lib");
    if (corruptIndex.size() >= 72)
    {
        corruptIndex[68] = 0xffU;
        corruptIndex[69] = 0xffU;
        corruptIndex[70] = 0xffU;
        corruptIndex[71] = 0xffU;
    }
    const auto corruptIndexPath = shared / "CorruptIndex.lib";
    writeBytes(corruptIndexPath, corruptIndex);
    output.clear();
    error.clear();
    tests.expect(verifyArtifacts(shared / "SharedMeta.nsmeta", corruptIndexPath, shared / "SharedMeta.dll",
                                 output, error) == ExitCode::CompilationError &&
                     error.find("COFF archive") != std::string::npos &&
                     (error.find("symbol-offset table") != std::string::npos ||
                      error.find("symbol-count safety limit") != std::string::npos),
                 "P6 rejects an archive whose linker-member index claims an unsafe or unreadable symbol table");

    const std::vector<std::uint8_t> validDll = readBytes(shared / "SharedMeta.dll");
    const std::size_t peOffset = static_cast<std::size_t>(readU32(validDll, 0x3c));

    std::vector<std::uint8_t> executableImage = validDll;
    const std::size_t characteristicsOffset = peOffset + 4U + 18U;
    writeU16(executableImage, characteristicsOffset,
             static_cast<std::uint16_t>(readU16(executableImage, characteristicsOffset) & ~0x2000U));
    const auto notDllPath = shared / "NotADll.dll";
    writeBytes(notDllPath, executableImage);
    output.clear();
    error.clear();
    tests.expect(verifyArtifacts(shared / "SharedMeta.nsmeta", shared / "SharedMeta.lib", notDllPath,
                                 output, error) == ExitCode::CompilationError &&
                     error.find("not marked as a DLL") != std::string::npos,
                 "P6 rejects a valid PE image whose physical characteristics do not identify a DLL");

    std::vector<std::uint8_t> wrongMachine = validDll;
    writeU16(wrongMachine, peOffset + 4U, 0x014cU);
    const auto wrongMachinePath = shared / "WrongMachine.dll";
    writeBytes(wrongMachinePath, wrongMachine);
    output.clear();
    error.clear();
    tests.expect(verifyArtifacts(shared / "SharedMeta.nsmeta", shared / "SharedMeta.lib", wrongMachinePath,
                                 output, error) == ExitCode::CompilationError &&
                     error.find("machine is not AMD64") != std::string::npos,
                 "P6 rejects a runtime DLL that advertises the wrong COFF machine before export comparison");
}

void testGenericCompilerMetadata(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Generics";
    writeProject(directory, "GenericMeta", "StaticLibrary", R"NS(
namespace GenericMeta
{
    public struct Box<T> { public T Value; }
    public class Algorithms
    {
        private static T Unused<T>(T value) { return value; }
        private static T IdentityCore<T>(T value) { return value; }
        public static T Identity<T>(T value) { return IdentityCore<T>(value); }
        public static int Run()
        {
            Box<int> box;
            box.Value = Identity<int>(42);
            return box.Value;
        }
    }
}
)NS");
    std::string output;
    std::string error;
    const auto metadataPath = directory / "GenericMeta.nsmeta";
    tests.expect(emitMetadata(directory / "GenericMeta.nsproj", metadataPath, output, error) == ExitCode::Success,
        "metadata can be emitted after generic AOT monomorphization");
    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    tests.expect(codec.read(metadataPath, metadata, error), "generic metadata reads successfully");
    tests.expect(metadata.genericTemplates.size() == 3,
                 "public generic roots and their private materialization dependency are persisted");
    tests.expect(std::ranges::all_of(metadata.genericTemplates, [](const auto& item) {
        return item.accessibility <= 3U && !item.sourceText.empty();
    }), "generic templates carry a valid materializable declaration slice");
    tests.expect(std::ranges::any_of(metadata.genericTemplates, [](const auto& item) {
        return item.name == "IdentityCore" && item.accessibility == 0U;
    }), "private generic helper retains private accessibility in NSMETA");
    tests.expect(std::ranges::none_of(metadata.genericTemplates, [](const auto& item) {
        return item.name == "Unused";
    }), "unreachable private generic implementation is not leaked into NSMETA");
    const auto identity = std::ranges::find_if(metadata.genericTemplates, [](const auto& item) {
        return item.name == "Identity";
    });
    tests.expect(identity != metadata.genericTemplates.end() && identity->dependencies.size() == 1U &&
                     identity->dependencies.front().find("IdentityCore") != std::string::npos,
                 "public generic root records its private closure edge");
    LibraryMetadata incompleteClosure = metadata;
    std::erase_if(incompleteClosure.genericTemplates, [](const auto& item) {
        return item.name == "IdentityCore";
    });
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompleteClosure, error) &&
                     error.find("reusable template closure is incomplete") != std::string::npos &&
                     error.find("IdentityCore") != std::string::npos &&
                     error.find("dependency chain") != std::string::npos,
                 "verifier rejects a missing private closure entry and reports its chain");
    tests.expect(metadata.genericSpecializations.size() == 3,
                 "concrete Box<int>, Identity<int>, and private IdentityCore<int> "
                 "specialization identities are persisted");
    tests.expect(std::ranges::any_of(metadata.genericSpecializations, [](const auto& item) {
        return item.specializationKey.find("Box`1<int>") != std::string::npos;
    }), "generic type specialization key survives metadata");
    tests.expect(std::ranges::any_of(metadata.genericSpecializations, [](const auto& item) {
        return item.specializationKey.find("Identity``1<int>") != std::string::npos;
    }), "generic method specialization key survives metadata");
}

void testPrecompiledClosedGenericSurface(TestContext& tests, const std::filesystem::path& root)
{
    const auto genericOwner = root / "ClosedGenericOwner";
    writeProject(genericOwner, "ClosedGenericOwner", "StaticLibrary", R"NS(
namespace ClosedGenericContracts
{
    public class Bucket<T>
    {
        private T value;
        public Bucket() { }
        public int Count() { return 0; }
    }
}

)NS");

    std::string output;
    std::string error;
    tests.expect(emitProduct("--emit-project-oop-lib", genericOwner / "ClosedGenericOwner.nsproj",
                             genericOwner / "ClosedGenericOwner.lib", output, error) == ExitCode::Success,
                 "closed-generic owner emits its reusable template library (" + error + ")");

    const auto producer = root / "ClosedGenericProducer";
    writeFile(producer / "ClosedGenericProducer.nsproj",
        "<Project><PropertyGroup><Name>ClosedGenericProducer</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (genericOwner / "ClosedGenericOwner.lib").generic_string() +
        "\" Metadata=\"" + (genericOwner / "ClosedGenericOwner.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(producer / "Producer.ns", R"NS(
using ClosedGenericContracts;

namespace ClosedGenericProducerApi
{
    public struct ProducerValue
    {
        public int Value;
    }

    public class ProducerBox
    {
        public owned Bucket<ProducerValue> Values;

        public ProducerBox()
        {
            Values = new Bucket<ProducerValue>();
        }

        public static int CountValues(Bucket<ProducerValue> values)
        {
            return values.Count();
        }
    }
}
)NS");

    output.clear();
    error.clear();
    tests.expect(emitProduct("--emit-project-oop-lib", producer / "ClosedGenericProducer.nsproj",
                             producer / "ClosedGenericProducer.lib", output, error) == ExitCode::Success,
                 "producer emits a library whose public field closes an external template (" + error + ")");

    LibraryMetadataCodec codec;
    LibraryMetadata producerMetadata;
    error.clear();
    tests.expect(codec.read(producer / "ClosedGenericProducer.nsmeta", producerMetadata, error),
                 "producer closed-generic metadata reads successfully (" + error + ")");
    const auto publishedBucket = std::ranges::find_if(
        producerMetadata.genericSpecializations,
        [](const auto& specialization) {
            return specialization.templateKey.find("ClosedGenericContracts.Bucket`1") != std::string::npos;
        });
    tests.expect(publishedBucket != producerMetadata.genericSpecializations.end(),
                 "NSMETA publishes the external closed specialization named by the producer API");
    tests.expect(publishedBucket != producerMetadata.genericSpecializations.end() &&
                     std::ranges::any_of(publishedBucket->typeArguments, [](const std::string& argument) {
                         return argument.find("type:ClosedGenericProducer::ClosedGenericProducerApi.ProducerValue") == 0U;
                     }),
                 "the specialization records the producer-owned argument by canonical module identity");

    LibraryMetadata incomplete = producerMetadata;
    std::erase_if(incomplete.genericSpecializations, [](const auto& specialization) {
        return specialization.templateKey.find("ClosedGenericContracts.Bucket`1") != std::string::npos;
    });
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incomplete, error) &&
                     error.find("refers to closed generic type") != std::string::npos &&
                     error.find("no generic specialization declares") != std::string::npos,
                 "metadata verifier rejects a referenced closed type whose declaration record is missing");

    const auto consumer = root / "ClosedGenericConsumer";
    writeFile(consumer / "ClosedGenericConsumer.nsproj",
        "<Project><PropertyGroup><Name>ClosedGenericConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (genericOwner / "ClosedGenericOwner.lib").generic_string() +
        "\" Metadata=\"" + (genericOwner / "ClosedGenericOwner.nsmeta").generic_string() + "\" />"
        "<LibraryReference Include=\"" + (producer / "ClosedGenericProducer.lib").generic_string() +
        "\" Metadata=\"" + (producer / "ClosedGenericProducer.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(consumer / "Consumer.ns", R"NS(
using ClosedGenericProducerApi;
using ClosedGenericContracts;

class Program
{
    public static int Main()
    {
        raii ProducerBox box = new ProducerBox();
        raii Bucket<ProducerValue> local = new Bucket<ProducerValue>();
        return ProducerBox.CountValues(box.Values) == 0 && box.Values.Count() == 0 &&
            local.Count() == 0 ? 0 : 1;
    }
}
)NS");

    const std::string consumerProject = (consumer / "ClosedGenericConsumer.nsproj").string();
    const std::array<std::string_view, 2> hirArgs = {"--check-project-oop-hir", consumerProject};
    output.clear();
    error.clear();
    tests.expect(runDriver(hirArgs, output, error) == ExitCode::Success,
                 "source-free consumer materializes the producer's closed generic API (" + error + ")");

    const std::string executable = (consumer / "ClosedGenericConsumer.exe").string();
    const std::array<std::string_view, 5> executableArgs = {
        "-O0", "--emit-project-oop-exe", consumerProject, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArgs, output, error) == ExitCode::Success;
    tests.expect(emitted && std::filesystem::is_regular_file(executable),
                 "source-free consumer emits an O0 executable through both precompiled libraries (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
                     "closed-generic precompiled consumer executes with exit code zero");
    }
}

void testPrecompiledCallableKeepsNominalParameterIdentity(
    TestContext& tests, const std::filesystem::path& root)
{
    const auto dependency = root / "NominalExternalKeys";
    writeProject(dependency, "NominalExternalKeys", "StaticLibrary", R"NS(
namespace NativeSharp.Glfw
{
    public enum Keys
    {
        External = 1
    }
}
)NS");

    std::string output;
    std::string error;
    tests.expect(emitProduct("--emit-project-oop-lib", dependency / "NominalExternalKeys.nsproj",
                             dependency / "NominalExternalKeys.lib", output, error) == ExitCode::Success,
                 "external Keys dependency emits a precompiled library (" + error + ")");

    const auto producer = root / "NominalProducer";
    writeFile(producer / "NominalProducer.nsproj",
        "<Project><PropertyGroup><Name>NominalProducer</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (dependency / "NominalExternalKeys.lib").generic_string() +
        "\" Metadata=\"" + (dependency / "NominalExternalKeys.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(producer / "Api.ns", R"NS(
public enum Keys
{
    Own = 7
}

public class InputKeyEventArgs
{
    public Keys Key { get; }

    public InputKeyEventArgs(Keys key)
    {
        Key = key;
    }
}

public class GlfwKeyBridge
{
    public static NativeSharp.Glfw.Keys Echo(NativeSharp.Glfw.Keys key)
    {
        return key;
    }
}
)NS");

    output.clear(); error.clear();
    tests.expect(emitProduct("--emit-project-oop-lib", producer / "NominalProducer.nsproj",
                             producer / "NominalProducer.lib", output, error) == ExitCode::Success,
                 "producer emits local and external Keys identities together (" + error + ")");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    error.clear();
    const bool read = codec.read(producer / "NominalProducer.nsmeta", metadata, error);
    tests.expect(read, "nominal producer NSMETA can be decoded (" + error + ")");
    if (!read) return;
    const auto constructor = std::ranges::find_if(metadata.members, [](const LibraryMetadataMember& member) {
        return member.declaringTypeKey == "type:NominalProducer::InputKeyEventArgs" &&
               member.signatureKey == "InputKeyEventArgs(type:NominalProducer::Keys)";
    });
    tests.expect(constructor != metadata.members.end() && !constructor->mangledName.empty(),
                 "public constructor publishes the producer-local enum identity and a physical symbol");

    const auto consumer = root / "NominalConsumer";
    writeFile(consumer / "NominalConsumer.nsproj",
        "<Project><PropertyGroup><Name>NominalConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (dependency / "NominalExternalKeys.lib").generic_string() +
        "\" Metadata=\"" + (dependency / "NominalExternalKeys.nsmeta").generic_string() + "\" />"
        "<LibraryReference Include=\"" + (producer / "NominalProducer.lib").generic_string() +
        "\" Metadata=\"" + (producer / "NominalProducer.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(consumer / "Program.ns", R"NS(
class Program
{
    public static int Main()
    {
        raii InputKeyEventArgs value = new InputKeyEventArgs(Keys.Own);
        NativeSharp.Glfw.Keys external = GlfwKeyBridge.Echo(NativeSharp.Glfw.Keys.External);
        return value.Key == Keys.Own && external == NativeSharp.Glfw.Keys.External ? 0 : 1;
    }
}
)NS");

    const std::string consumerProject = (consumer / "NominalConsumer.nsproj").string();
    const std::string executable = (consumer / "NominalConsumer.exe").string();
    const std::array<std::string_view, 5> args = {
        "-O0", "--emit-project-oop-exe", consumerProject, "-o", executable};
    output.clear(); error.clear();
    tests.expect(runDriver(args, output, error) == ExitCode::Success,
                 "consumer binds the constructor to producer-local Keys instead of the imported enum (" +
                     error + ")");
}

void testPrecompiledExternalTypeClosure(TestContext& tests, const std::filesystem::path& root)
{
    const auto provider = root / "ExternalTypeProvider";
    writeProject(provider, "ExternalTypeProvider", "StaticLibrary", R"NS(
namespace ExternalContracts
{
    public struct Mat4 { public float Value; }
    public struct Quat { public float Value; }
    public struct Vec2 { public float Value; }
    public struct Vec3 { public float Value; }
    public struct Vec4 { public float Value; }

    public struct EventArgs0 { public int Value; }
    public struct EventArgs1 { public int Value; }
    public struct EventArgs2 { public int Value; }
    public struct EventArgs3 { public int Value; }
    public struct EventArgs4 { public int Value; }
    public struct EventArgs5 { public int Value; }
    public struct EventArgs6 { public int Value; }
    public delegate void EventHandler<T>(T args);

    public class PublishedHandlers
    {
        public EventHandler<EventArgs0> H0;
        public EventHandler<EventArgs1> H1;
        public EventHandler<EventArgs2> H2;
        public EventHandler<EventArgs3> H3;
        public EventHandler<EventArgs4> H4;
        public EventHandler<EventArgs5> H5;
        public EventHandler<EventArgs6> H6;
    }
}
)NS");

    std::string output;
    std::string error;
    tests.expect(emitProduct("--emit-project-oop-lib", provider / "ExternalTypeProvider.nsproj",
                             provider / "ExternalTypeProvider.lib", output, error) == ExitCode::Success,
                 "external type provider emits math types and seven closed delegate types (" + error + ")");

    const auto facade = root / "ExternalTypeFacade";
    writeFile(facade / "ExternalTypeFacade.nsproj",
        "<Project><PropertyGroup><Name>ExternalTypeFacade</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (provider / "ExternalTypeProvider.lib").generic_string() +
        "\" Metadata=\"" + (provider / "ExternalTypeProvider.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(facade / "Facade.ns", R"NS(
using ExternalContracts;

public class ExternalFacade
{
    public static Mat4 M0(Mat4 value) { return value; }
    public static Quat M1(Quat value) { return value; }
    public static Vec2 M2(Vec2 value) { return value; }
    public static Vec3 M3(Vec3 value) { return value; }
    public static Vec4 M4(Vec4 value) { return value; }
    public static EventHandler<EventArgs0> H0(EventHandler<EventArgs0> value) { return value; }
    public static EventHandler<EventArgs1> H1(EventHandler<EventArgs1> value) { return value; }
    public static EventHandler<EventArgs2> H2(EventHandler<EventArgs2> value) { return value; }
    public static EventHandler<EventArgs3> H3(EventHandler<EventArgs3> value) { return value; }
    public static EventHandler<EventArgs4> H4(EventHandler<EventArgs4> value) { return value; }
    public static EventHandler<EventArgs5> H5(EventHandler<EventArgs5> value) { return value; }
    public static EventHandler<EventArgs6> H6(EventHandler<EventArgs6> value) { return value; }
}

public class ExternalEvents
{
    public event EventHandler<EventArgs0> Changed0;
    public event EventHandler<EventArgs1> Changed1;
    public event EventHandler<EventArgs2> Changed2;
    public event EventHandler<EventArgs3> Changed3;
    public event EventHandler<EventArgs4> Changed4;
    public event EventHandler<EventArgs5> Changed5;
    public event EventHandler<EventArgs6> Changed6;
}

public struct ExternalConversions
{
    public Mat4 M0;
    public Quat M1;
    public Vec2 M2;
    public Vec3 M3;
    public Vec4 M4;

    public static implicit operator Mat4(ExternalConversions value) { return value.M0; }
    public static implicit operator Quat(ExternalConversions value) { return value.M1; }
    public static implicit operator Vec2(ExternalConversions value) { return value.M2; }
    public static implicit operator Vec3(ExternalConversions value) { return value.M3; }
    public static implicit operator Vec4(ExternalConversions value) { return value.M4; }
}
)NS");

    output.clear(); error.clear();
    tests.expect(emitProduct("--emit-project-oop-lib", facade / "ExternalTypeFacade.nsproj",
                             facade / "ExternalTypeFacade.lib", output, error) == ExitCode::Success,
                 "facade emits methods, events, and conversions containing provider-owned nominal types (" +
                     error + ")");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    error.clear();
    const bool read = codec.read(facade / "ExternalTypeFacade.nsmeta", metadata, error);
    tests.expect(read, "external facade NSMETA can be decoded (" + error + ")");
    if (!read) return;
    const auto eventHandlerDependencies = std::ranges::count_if(
        metadata.externalTypeKeys, [](const std::string& typeKey) {
        return typeKey.find("ExternalTypeProvider::ExternalContracts.EventHandler__g1__") != std::string::npos;
    });
    const auto mathDependencies = std::ranges::count_if(
        metadata.externalTypeKeys, [](const std::string& typeKey) {
        return typeKey == "type:ExternalTypeProvider::ExternalContracts.Mat4" ||
               typeKey == "type:ExternalTypeProvider::ExternalContracts.Quat" ||
               typeKey == "type:ExternalTypeProvider::ExternalContracts.Vec2" ||
               typeKey == "type:ExternalTypeProvider::ExternalContracts.Vec3" ||
               typeKey == "type:ExternalTypeProvider::ExternalContracts.Vec4";
    });
    tests.expect(eventHandlerDependencies == 7,
        "all seven provider-owned EventHandler<T> specializations remain external nominal dependencies");
    tests.expect(mathDependencies == 5,
                 "Mat4, Quat, Vec2, Vec3, and Vec4 remain provider-owned nominal dependencies");
    tests.expect(std::ranges::none_of(metadata.genericSpecializations, [](const auto& specialization) {
        return specialization.templateKey.find("ExternalTypeProvider::ExternalContracts.EventHandler`1") !=
            std::string::npos;
    }), "facade does not republish closed delegate specializations already materialized by its provider");

    const auto consumer = root / "ExternalTypeConsumer";
    writeFile(consumer / "ExternalTypeConsumer.nsproj",
        "<Project><PropertyGroup><Name>ExternalTypeConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (provider / "ExternalTypeProvider.lib").generic_string() +
        "\" Metadata=\"" + (provider / "ExternalTypeProvider.nsmeta").generic_string() + "\" />"
        "<LibraryReference Include=\"" + (facade / "ExternalTypeFacade.lib").generic_string() +
        "\" Metadata=\"" + (facade / "ExternalTypeFacade.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(consumer / "Program.ns", R"NS(
class Program
{
    public static int Main() { return 0; }
}
)NS");
    const std::string consumerProject = (consumer / "ExternalTypeConsumer.nsproj").string();
    const std::array<std::string_view, 2> hirArgs = {"--check-project-oop-hir", consumerProject};
    output.clear(); error.clear();
    tests.expect(runDriver(hirArgs, output, error) == ExitCode::Success,
                  "combined LibraryReference set reconstructs qualified external event and conversion types (" +
                      error + ")");

    const auto incomplete = root / "ExternalTypeConsumerMissingProvider";
    writeFile(incomplete / "ExternalTypeConsumerMissingProvider.nsproj",
        "<Project><PropertyGroup><Name>ExternalTypeConsumerMissingProvider</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (facade / "ExternalTypeFacade.lib").generic_string() +
        "\" Metadata=\"" + (facade / "ExternalTypeFacade.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(incomplete / "Program.ns", "class Program { public static int Main() { return 0; } }");
    const std::string incompleteProject = (incomplete / "ExternalTypeConsumerMissingProvider.nsproj").string();
    const std::array<std::string_view, 2> incompleteArgs = {"--check-project-oop-hir", incompleteProject};
    output.clear(); error.clear();
    tests.expect(runDriver(incompleteArgs, output, error) == ExitCode::CompilationError &&
                     error.find("precompiled LibraryReference type closure is incomplete") != std::string::npos &&
                     error.find("ExternalContracts.Mat4") != std::string::npos &&
                     error.find("EventHandler__g1__") != std::string::npos,
                 "missing provider is rejected once with the accumulated external type closure");
}

void testLibraryMaterializesCompletePublicClosedGenericSurface(
    TestContext& tests, const std::filesystem::path& root)
{
    const auto producer = root / "ClosedGenericCallableProducer";
    writeProject(producer, "ClosedGenericCallableProducer", "StaticLibrary", R"NS(
public interface IPhysicsObject
{
    int Id();
}

public class ReferenceList<T>
{
    public ReferenceList()
    {
    }

    public void RemoveAt(int index)
    {
        Helper(index);
    }

    public int Count
    {
        get { return 0; }
    }

    public static int Kind()
    {
        return 7;
    }

    protected void Shift(int index)
    {
    }

    private void Helper(int index)
    {
    }
}

public class PhysicsLists
{
    public ReferenceList<IPhysicsObject> Items;
}
)NS");

    std::string output;
    std::string error;
    tests.expect(emitProduct("--emit-project-oop-lib", producer / "ClosedGenericCallableProducer.nsproj",
                             producer / "ClosedGenericCallableProducer.lib", output, error) == ExitCode::Success,
                 "library materializes uncalled methods of a public closed generic surface (" + error + ")");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    error.clear();
    const bool read = codec.read(producer / "ClosedGenericCallableProducer.nsmeta", metadata, error);
    tests.expect(read, "closed generic callable metadata can be decoded (" + error + ")");
    if (!read) return;
    const auto specializedType = std::ranges::find_if(metadata.types, [](const LibraryMetadataType& type) {
        return type.fullyQualifiedName.find("ReferenceList__g1__IPhysicsObject__h") != std::string::npos;
    });
    tests.expect(specializedType != metadata.types.end(),
                 "the producer publishes the closed ReferenceList<IPhysicsObject> nominal type");
    if (specializedType == metadata.types.end()) return;

    const std::string declaringTypeKey = "type:" + specializedType->identityKey;
    const std::array<std::string_view, 4> publicSurfaceNames = {
        "RemoveAt", "get_Count", "Kind", "Shift"};
    for (const std::string_view name : publicSurfaceNames)
    {
        const auto member = std::ranges::find_if(metadata.members, [&](const LibraryMetadataMember& candidate) {
            return candidate.declaringTypeKey == declaringTypeKey && candidate.name == name;
        });
        tests.expect(member != metadata.members.end() && !member->mangledName.empty(),
                     "closed generic public/protected callable '" + std::string(name) +
                         "' has a physical identity without producer-side use");
    }

    const auto consumer = root / "ClosedGenericCallableConsumer";
    writeFile(consumer / "ClosedGenericCallableConsumer.nsproj",
        "<Project><PropertyGroup><Name>ClosedGenericCallableConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" +
        (producer / "ClosedGenericCallableProducer.lib").generic_string() + "\" Metadata=\"" +
        (producer / "ClosedGenericCallableProducer.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(consumer / "Program.ns", R"NS(
class Program
{
    public static int Main()
    {
        raii ReferenceList<IPhysicsObject> items = new ReferenceList<IPhysicsObject>();
        items.RemoveAt(0);
        return items.Count == 0 && ReferenceList<IPhysicsObject>.Kind() == 7 ? 0 : 1;
    }
}
)NS");

    const std::string consumerProject = (consumer / "ClosedGenericCallableConsumer.nsproj").string();
    const std::string executable = (consumer / "ClosedGenericCallableConsumer.exe").string();
    const std::array<std::string_view, 5> args = {
        "-O0", "--emit-project-oop-exe", consumerProject, "-o", executable};
    output.clear(); error.clear();
    tests.expect(runDriver(args, output, error) == ExitCode::Success,
                 "consumer links uncalled producer methods from the closed generic public API (" + error + ")");
}

void testIntegrityAndCompatibility(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Integrity";
    writeProject(directory, "Integrity", "StaticLibrary", richSource());
    const auto validPath = directory / "valid.nsmeta";
    std::string output;
    std::string error;
    tests.expect(emitMetadata(directory / "Integrity.nsproj", validPath, output, error) == ExitCode::Success,
        "integrity fixture metadata emits successfully");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    tests.expect(codec.read(validPath, metadata, error), "valid integrity fixture passes payload and ABI checks");

    // DLL-P2: exercise every historically readable minor as an actual on-disk NSMETA file.
    // writeModel encodes only the fields promised by that minor, so the current reader must not
    // infer later capabilities while it still accepts the common contract.
    for (std::uint16_t minor = 0; minor <= nativesharp::interop::kMetadataFormatMinor; ++minor)
    {
        LibraryMetadata historical = metadata;
        historical.formatMinor = minor;
        const auto historicalPath = directory / ("historical-1." + std::to_string(minor) + ".nsmeta");
        error.clear();
        const bool wrote = codec.writeModel(historical, historicalPath, error);
        tests.expect(wrote,
                     "NSMETA 1." + std::to_string(minor) + " historical file writes (" + error + ")");
        LibraryMetadata decoded;
        error.clear();
        const bool read = wrote && codec.read(historicalPath, decoded, error);
        tests.expect(read && decoded.formatMajor == 1 && decoded.formatMinor == minor,
                     "current reader accepts actual NSMETA 1." + std::to_string(minor) +
                         " bytes without changing their version (" + error + ")");
        error.clear();
        tests.expect(read && codec.verifyCurrentCompiler(decoded, error),
                     "current compiler accepts the common ABI represented by NSMETA 1." +
                         std::to_string(minor) + " (" + error + ")");

        // The current format is exercised above through write/read/verify. Archived
        // bytes are required only for older readable minors; when the format moves
        // forward, today's current format automatically becomes historical.
        if (minor < nativesharp::interop::kMetadataFormatMinor)
        {
            const auto goldenPath = std::filesystem::path(NSC_SOURCE_DIR) / "tests" / "fixtures" /
                "nsmeta" / ("historical-1." + std::to_string(minor) + ".nsmeta");
            if (environmentFlag("NSC_P2_REFRESH_GOLDENS"))
            {
                std::filesystem::create_directories(goldenPath.parent_path());
                // Archived bytes are immutable by default. Filling a newly historical minor
                // must not silently rewrite the compatibility evidence for earlier formats.
                const auto copyOptions = environmentFlag("NSC_P2_OVERWRITE_GOLDENS")
                    ? std::filesystem::copy_options::overwrite_existing
                    : std::filesystem::copy_options::skip_existing;
                std::filesystem::copy_file(
                    historicalPath, goldenPath, copyOptions);
            }
            LibraryMetadata golden;
            error.clear();
            const bool goldenRead = codec.read(goldenPath, golden, error);
            tests.expect(goldenRead && golden.formatMajor == 1 && golden.formatMinor == minor,
                         "archived NSMETA 1." + std::to_string(minor) +
                             " golden remains readable (" + error + ")");
            error.clear();
            const bool goldenCompatible = goldenRead && codec.verifyCurrentCompiler(golden, error);
            if (minor >= 8 && minor < 15)
            {
                tests.expect(!goldenCompatible &&
                                 error.find("object ABI expected 2, found 1") != std::string::npos,
                             "archived NSMETA 1." + std::to_string(minor) +
                                 " golden is rejected at the explicit Object ABI 2 boundary (" + error + ")");
            }
            else if (golden.producerVersion.starts_with("0.9.0"))
            {
                // These immutable snapshots were emitted by the pre-release 0.9 producer. They
                // remain proof that every historical NSMETA minor is readable, but 2.0 must not
                // silently expand its certified producer window beyond released N-1 (1.0.1).
                tests.expect(!goldenCompatible &&
                                 error.find("outside the supported producer window") != std::string::npos,
                             "archived NSMETA 1." + std::to_string(minor) +
                                 " golden is readable but its obsolete producer is rejected (" + error + ")");
            }
            else
            {
                tests.expect(goldenCompatible,
                             "archived NSMETA 1." + std::to_string(minor) +
                                 " golden from the certified producer window remains compatible (" +
                                 error + ")");
            }
        }
    }

    std::vector<std::uint8_t> corrupted = readBytes(validPath);
    corrupted.back() ^= 0x5aU;
    const auto corruptedPath = directory / "corrupted.nsmeta";
    writeBytes(corruptedPath, corrupted);
    LibraryMetadata ignored;
    error.clear();
    tests.expect(!codec.read(corruptedPath, ignored, error) && error.find("checksum mismatch") != std::string::npos,
        "single-byte payload corruption is detected before metadata is accepted");

    std::vector<std::uint8_t> versionMismatch = readBytes(validPath);
    versionMismatch[8] = 2;
    versionMismatch[9] = 0;
    const auto versionPath = directory / "version.nsmeta";
    writeBytes(versionPath, versionMismatch);
    error.clear();
    tests.expect(!codec.read(versionPath, ignored, error) && error.find("unsupported") != std::string::npos,
        "unsupported metadata major version is rejected explicitly");

    std::vector<std::uint8_t> futureMinor = readBytes(validPath);
    futureMinor[10] = static_cast<std::uint8_t>(nativesharp::interop::kMetadataFormatMinor + 1U);
    futureMinor[11] = 0;
    const auto futureMinorPath = directory / "future-minor.nsmeta";
    writeBytes(futureMinorPath, futureMinor);
    error.clear();
    const std::string futureMinorDiagnostic = "unsupported NativeSharp metadata format 1." +
        std::to_string(nativesharp::interop::kMetadataFormatMinor + 1U);
    tests.expect(!codec.read(futureMinorPath, ignored, error) &&
                     error.find(futureMinorDiagnostic) != std::string::npos,
                 "a future NSMETA minor is rejected before payload decoding");

    std::vector<std::uint8_t> badMagic = readBytes(validPath);
    badMagic[0] = 'X';
    const auto magicPath = directory / "magic.nsmeta";
    writeBytes(magicPath, badMagic);
    error.clear();
    tests.expect(!codec.read(magicPath, ignored, error) && error.find("magic") != std::string::npos,
        "wrong NSMETA magic is rejected explicitly");

    const auto truncatedPath = directory / "truncated.nsmeta";
    writeBytes(truncatedPath, std::vector<std::uint8_t>(20, 0));
    error.clear();
    tests.expect(!codec.read(truncatedPath, ignored, error) && error.find("truncated") != std::string::npos,
        "truncated fixed header is rejected safely");

    LibraryMetadata incompatible = metadata;
    incompatible.producerVersion = "0.2.0-dev-old";
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) && error.find("producer version") != std::string::npos,
                 "compiler-version compatibility gate rejects metadata produced "
                 "by a different compiler release");
    incompatible = metadata;
    incompatible.producerVersion = std::string(nativesharp::kCompilerReleaseVersion) +
        "+build.20000101T000000Z.0000000000000000";
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(incompatible, error),
                 "SemVer build metadata identifies the producer without breaking "
                 "release compatibility");
    incompatible.producerVersion = std::string(nativesharp::kCompilerReleaseVersion) +
        "-dev+build.20000101T000000Z.0000000000000000";
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(incompatible, error),
                  "a prerelease suffix preserves the 3.0.0 compatibility line");
    incompatible = metadata;
    incompatible.pointerSize = 4;
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) && error.find("target ABI") != std::string::npos,
                 "target ABI compatibility gate rejects non-Windows-x64 pointer "
                 "contracts");
    incompatible = metadata;
    incompatible.delegateAbiVersion += 1;
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) && error.find("delegate ABI expected 1, found 2") != std::string::npos,
                 "a different managed-delegate representation is rejected before "
                 "compilation or link");
    incompatible = metadata;
    incompatible.exceptionAbiVersion += 1;
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) && error.find("exception ABI expected 1, found 2") != std::string::npos,
                 "a different exception transport/RTTI contract is rejected "
                 "before compilation or link");

    incompatible = metadata;
    incompatible.nativeAbiMajor = 2;
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("native ABI expected 1.0, found 2.0") != std::string::npos,
                 "E5 names the incompatible Native ABI version and both values");
    incompatible = metadata;
    incompatible.runtimeAbiMinor = 1;
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("runtime ABI expected 1.0, found 1.1") != std::string::npos,
                 "E5 names a newer runtime contract instead of collapsing all "
                 "ABI fields into one message");
    incompatible = metadata;
    incompatible.manglingAbiVersion = 2;
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("mangling ABI expected 1, found 2") != std::string::npos,
                 "E5 identifies the mangling contract independently");
    incompatible = metadata;
    incompatible.objectAbiVersion = 3;
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("object ABI expected 2, found 3") != std::string::npos,
                 "E5 identifies the object-layout contract independently");
    incompatible = metadata;
    incompatible.managedDelegateCellSize = 16;
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("managed delegate cell size expected 32 bytes, found 16 bytes") != std::string::npos,
                 "E5 reports the exact producer and consumer delegate-cell sizes");

    incompatible = metadata;
    incompatible.targetTriple = "x86_64-unknown-windows";
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("target triple expected 'x86_64-pc-windows-msvc'") != std::string::npos &&
                     error.find("found 'x86_64-unknown-windows'") != std::string::npos,
                 "E5 target diagnostics preserve expected and observed triples");
    incompatible = metadata;
    incompatible.moduleName.clear();
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("module identity is empty") != std::string::npos,
                 "E5 rejects self-consistent metadata without a canonical "
                 "producer identity");
    incompatible = metadata;
    incompatible.types.push_back(incompatible.types.front());
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("duplicate type identity") != std::string::npos,
                 "E5 rejects duplicate canonical type identities before source "
                 "materialization");
    incompatible = metadata;
    incompatible.exports.push_back(incompatible.exports.front());
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("duplicate C ABI export") != std::string::npos,
                 "E5 rejects duplicate host export identities before physical "
                 "artifact checks");
    incompatible = metadata;
    if (!incompatible.dispatchTables.empty())
        incompatible.dispatchTables.front().slotCalleeNames.push_back("extra-callee");
    error.clear();
    tests.expect(!incompatible.dispatchTables.empty() && !codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("slot signatures") != std::string::npos && error.find("callees") != std::string::npos,
                 "E5 rejects structurally inconsistent virtual-dispatch tables "
                 "with both slot counts");
    const LibraryMetadata inconsistentParallelTables = incompatible;

    incompatible = metadata;
    bool clearedSlotSignature = false;
    for (auto& dispatch : incompatible.dispatchTables)
    {
        if (dispatch.slotSignatures.empty()) continue;
        dispatch.slotSignatures.front().clear();
        clearedSlotSignature = true;
        break;
    }
    error.clear();
    tests.expect(clearedSlotSignature && !codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("empty signature") != std::string::npos && error.find("slot") != std::string::npos,
                 "DLL-P4 rejects a dispatch slot that cannot identify its semantic member");

    incompatible = metadata;
    const auto overrideMember = std::ranges::find_if(incompatible.members, [](const auto& member) {
        constexpr std::uint32_t kOverride = 1U << 4U;
        return (member.flags & kOverride) != 0U && !member.mangledName.empty();
    });
    if (overrideMember != incompatible.members.end()) overrideMember->mangledName += "_wrong";
    error.clear();
    tests.expect(overrideMember != incompatible.members.end() && !codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("concrete virtual member") != std::string::npos &&
                     error.find("dispatch slot") != std::string::npos,
                 "DLL-P4 rejects a concrete override whose NSMETA member and dispatch callee disagree");

    incompatible = metadata;
    const auto virtualMember = std::ranges::find_if(incompatible.members, [](const auto& member) {
        constexpr std::uint32_t kVirtual = 1U << 3U;
        constexpr std::uint32_t kOverride = 1U << 4U;
        return (member.flags & kVirtual) != 0U && (member.flags & kOverride) == 0U && !member.mangledName.empty();
    });
    if (virtualMember != incompatible.members.end())
    {
        constexpr std::uint32_t kAbstract = 1U << 5U;
        virtualMember->flags |= kAbstract;
        virtualMember->mangledName.clear();
    }
    error.clear();
    tests.expect(virtualMember != incompatible.members.end() && !codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("abstract member") != std::string::npos &&
                     error.find("unexpectedly owns dispatch callee") != std::string::npos,
                 "DLL-P4 rejects an abstract slot that incorrectly claims a physical callee");

    incompatible = metadata;
    bool changedInterfaceImplementation = false;
    for (auto& dispatch : incompatible.dispatchTables)
    {
        for (auto& interface : dispatch.interfaces)
        {
            for (std::size_t slot = 0; slot < interface.implementationSignatures.size(); ++slot)
            {
                if (interface.calleeNames[slot].empty()) continue;
                interface.implementationSignatures[slot] = "wrong-interface-implementation";
                changedInterfaceImplementation = true;
                break;
            }
            if (changedInterfaceImplementation) break;
        }
        if (changedInterfaceImplementation) break;
    }
    error.clear();
    tests.expect(changedInterfaceImplementation && !codec.verifyCurrentCompiler(incompatible, error) &&
                     error.find("interface dispatch slot") != std::string::npos &&
                     error.find("expected 'wrong-interface-implementation'") != std::string::npos,
                 "DLL-P4 rejects an interface-table callee paired with the wrong implementation signature");

    const auto inconsistentPath = directory / "checksum-valid-inconsistent.nsmeta";
    error.clear();
    tests.expect(!inconsistentParallelTables.dispatchTables.empty() &&
                     codec.writeModel(inconsistentParallelTables, inconsistentPath, error),
                 "P6 writes a checksum-valid negative NSMETA fixture with inconsistent parallel tables (" +
                     error + ")");
    LibraryMetadata inconsistentDecoded;
    error.clear();
    const bool inconsistentRead = codec.read(inconsistentPath, inconsistentDecoded, error);
    tests.expect(inconsistentRead,
                 "P6 checksum-valid inconsistent NSMETA passes byte-integrity and ABI fingerprint checks (" +
                     error + ")");
    error.clear();
    tests.expect(inconsistentRead && !codec.verifyCurrentCompiler(inconsistentDecoded, error) &&
                     error.find("slot signatures") != std::string::npos && error.find("callees") != std::string::npos,
                 "P6 rejects the decoded model for structural incoherence after checksum validation");

    output.clear(); error.clear();
    tests.expect(verifyMetadata(validPath, output, error) == ExitCode::Success &&
            output.find("NativeSharp library metadata: valid") != std::string::npos &&
                     output.find("format: NSMETA " +
                                     std::to_string(nativesharp::interop::kMetadataFormatMajor) + "." +
                                     std::to_string(nativesharp::interop::kMetadataFormatMinor)) !=
                         std::string::npos &&
                     output.find("native-abi: 1.0") != std::string::npos &&
                     output.find("runtime-abi: 1.0") != std::string::npos &&
                     output.find("object-abi: 2") != std::string::npos &&
                     output.find("delegate-cell-size: 32") != std::string::npos,
                 "driver verification prints the complete frozen ABI contract, "
                 "not only its fingerprint");
    output.clear(); error.clear();
    tests.expect(verifyMetadata(corruptedPath, output, error) == ExitCode::CompilationError &&
            error.find("checksum mismatch") != std::string::npos,
                 "driver verification command rejects corrupted metadata with a "
                 "useful diagnostic");
}

void testScopeAndFailureBehavior(TestContext& tests, const std::filesystem::path& root)
{
    const auto executable = root / "Executable";
    writeProject(executable, "App", "Exe", "public class Program { public static int Main() { return 0; } }");
    std::string output;
    std::string error;
    const auto metadataPath = executable / "App.nsmeta";
    tests.expect(emitMetadata(executable / "App.nsproj", metadataPath, output, error) == ExitCode::CompilationError,
        "explicit .nsmeta emission rejects executable projects");
    tests.expect(!std::filesystem::exists(metadataPath),
        "failed explicit metadata emission leaves no partial destination");

    const auto invalid = root / "Invalid";
    writeProject(invalid, "Bad", "StaticLibrary", "public class Bad { public int F() { return Missing; } }");
    output.clear(); error.clear();
    const auto invalidMetadata = invalid / "Bad.nsmeta";
    tests.expect(emitMetadata(invalid / "Bad.nsproj", invalidMetadata, output, error) == ExitCode::CompilationError,
        "metadata emission preserves normal semantic compilation failures");
    tests.expect(!std::filesystem::exists(invalidMetadata),
        "semantic failure cannot leave a stale or partial .nsmeta artifact");
}

void testInterfaceContractsUseDeclaringTypeKind(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "InterfaceContracts";
    writeProject(directory, "InterfaceContracts", "StaticLibrary", R"NS(
namespace Contracts.Types
{
    public class Payload
    {
    }
}

namespace Contracts.Api
{
    public interface IBase
    {
        Contracts.Types.Payload BaseValue();
    }

    public interface IIndexed
        : Contracts.Api.IBase
    {
        Contracts.Types.Payload Read(Contracts.Types.Payload value);
        Contracts.Types.Payload Value { get; set; }
        Contracts.Types.Payload this[int index] { get; }
    }
}
)NS");

    const auto project = directory / "InterfaceContracts.nsproj";
    const auto library = directory / "InterfaceContracts.lib";
    const auto metadataPath = directory / "InterfaceContracts.nsmeta";
    std::string output;
    std::string error;
    tests.expect(emitProduct("--emit-project-oop-lib", project, library, output, error) == ExitCode::Success,
                 "interface contract producer emits a static library (" + error + ")");
    output.clear(); error.clear();
    tests.expect(emitMetadata(project, metadataPath, output, error) == ExitCode::Success,
                 "interface contract producer emits NSMETA (" + error + ")");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    error.clear();
    const bool read = codec.read(metadataPath, metadata, error);
    tests.expect(read, "interface contract NSMETA can be decoded (" + error + ")");
    const LibraryMetadataType* interfaceType = findType(metadata, "::Contracts.Api.IIndexed");
    tests.expect(interfaceType != nullptr && interfaceType->kind == LibraryMetadataTypeKind::Interface,
                 "interface declaring kind is preserved independently of member flags");
    if (!read || interfaceType == nullptr) return;

    constexpr std::uint32_t kVirtual = 1U << 3U;
    constexpr std::uint32_t kAbstract = 1U << 5U;
    const std::string declaringTypeKey = "type:" + interfaceType->identityKey;
    std::size_t rewrittenMembers = 0;
    for (LibraryMetadataMember& member : metadata.members)
    {
        if (member.declaringTypeKey != declaringTypeKey) continue;
        member.flags = (member.flags & ~kAbstract) | kVirtual;
        ++rewrittenMembers;
    }
    tests.expect(rewrittenMembers == 4,
                 "method, property getter, and indexer accessors are present in the fixture");

    const auto rewrittenMetadata = directory / "InterfaceContractsWithoutAbstract.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(metadata, rewrittenMetadata, error),
                 "checksum-valid interface metadata without abstract member flags is written (" + error + ")");
    output.clear(); error.clear();
    tests.expect(verifyArtifacts(rewrittenMetadata, library, {}, output, error) == ExitCode::Success,
                 "public interface methods, properties, and indexers materialize from declaring type kind (" +
                     error + ")");
}

void testPublicFieldInsideInternalTypeIsNotRequired(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "InternalTypePublicField";
    writeProject(directory, "InternalTypePublicField", "StaticLibrary", R"NS(
namespace HiddenSurface
{
    internal class VertexSkinData
    {
        public static int Rigid;
    }
}

)NS");

    const auto project = directory / "InternalTypePublicField.nsproj";
    const auto library = directory / "InternalTypePublicField.lib";
    const auto metadataPath = directory / "InternalTypePublicField.nsmeta";
    std::string output;
    std::string error;
    tests.expect(emitProduct("--emit-project-oop-lib", project, library, output, error) == ExitCode::Success,
                 "internal producer emits its static library (" + error + ")");
    output.clear(); error.clear();
    tests.expect(emitMetadata(project, metadataPath, output, error) == ExitCode::Success,
                 "internal producer emits NSMETA (" + error + ")");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    error.clear();
    const bool read = codec.read(metadataPath, metadata, error);
    tests.expect(read, "internal producer NSMETA can be decoded (" + error + ")");
    const LibraryMetadataType* hiddenType = findType(metadata, "::HiddenSurface.VertexSkinData");
    tests.expect(read && hiddenType == nullptr,
                 "internal containers are omitted from the public NSMETA type surface");
    const bool publishesHiddenField = std::ranges::any_of(metadata.members, [](const LibraryMetadataMember& member) {
        return member.name == "Rigid" &&
            member.declaringTypeKey.ends_with("::HiddenSurface.VertexSkinData");
    });
    tests.expect(read && !publishesHiddenField,
                 "public fields of an internal container are omitted from public NSMETA");

    output.clear(); error.clear();
    tests.expect(verifyArtifacts(metadataPath, library, {}, output, error) == ExitCode::Success,
                 "artifact verification does not require public members of an internal container (" + error + ")");
}

void testPrivateStaticFieldIsNotPublished(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "PrivateStaticField";
    writeProject(directory, "PrivateStaticField", "SharedLibrary", R"NS(
namespace HiddenMemberSurface
{
    extern C delegate void NativeSetter(int value);

    public class Api
    {
        private static NativeSetter callback;

        public static void Set(int value)
        {
        }
    }
}
)NS");

    const auto project = directory / "PrivateStaticField.nsproj";
    const auto library = directory / "PrivateStaticField.lib";
    const auto runtime = directory / "PrivateStaticField.dll";
    const auto metadataPath = directory / "PrivateStaticField.nsmeta";
    std::string output;
    std::string error;
    tests.expect(emitProduct("--emit-project-oop-dll", project, runtime, output, error) == ExitCode::Success,
                 "producer with a private delegate-typed static field emits a DLL (" + error + ")");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    error.clear();
    const bool read = codec.read(metadataPath, metadata, error);
    tests.expect(read, "private static field NSMETA can be decoded (" + error + ")");
    const bool publishesPrivateField = std::ranges::any_of(
        metadata.members, [](const LibraryMetadataMember& member) {
            return member.name == "callback" &&
                member.declaringTypeKey.ends_with("::HiddenMemberSurface.Api");
        });
    tests.expect(read && !publishesPrivateField,
                 "private static implementation fields are omitted from public NSMETA members");
    const LibraryMetadataType* hiddenDelegate = findType(metadata, "::HiddenMemberSurface.NativeSetter");
    tests.expect(read && hiddenDelegate == nullptr,
                 "a type referenced only by a private static field is not promoted into public NSMETA");
    const bool publishesPublicMethod = std::ranges::any_of(
        metadata.members, [](const LibraryMetadataMember& member) {
            return member.name == "Set" &&
                member.declaringTypeKey.ends_with("::HiddenMemberSurface.Api");
        });
    tests.expect(read && publishesPublicMethod,
                 "filtering private fields preserves public members of the same container");

    output.clear(); error.clear();
    tests.expect(verifyArtifacts(metadataPath, library, runtime, output, error) == ExitCode::Success,
                 "artifacts verify without private static implementation members (" + error + ")");
}

void testPublicInterfaceSignaturesMatchDllExports(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "PublicInterfaceDllSurface";
    writeProject(directory, "PublicInterfaceDllSurface", "SharedLibrary", R"NS(
namespace PublicInterfaceSurface
{
    public interface IService
    {
        int Read();
    }

    public class Api
    {
        public static IService Echo(IService value)
        {
            return value;
        }
    }
}
)NS");

    const auto project = directory / "PublicInterfaceDllSurface.nsproj";
    const auto library = directory / "PublicInterfaceDllSurface.lib";
    const auto runtime = directory / "PublicInterfaceDllSurface.dll";
    const auto metadata = directory / "PublicInterfaceDllSurface.nsmeta";
    std::string output;
    std::string error;
    tests.expect(emitProduct("--emit-project-oop-dll", project, runtime, output, error) == ExitCode::Success,
                 "DLL with a public interface in parameter and return positions is emitted (" + error + ")");
    output.clear(); error.clear();
    tests.expect(verifyArtifacts(metadata, library, runtime, output, error) == ExitCode::Success,
                 "NSMETA publication and the DLL export table agree for public interface signatures (" +
                     error + ")");
}

void testPublicReferenceArraysMatchDllExports(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "PublicReferenceArrayDllSurface";
    writeProject(directory, "PublicReferenceArrayDllSurface", "SharedLibrary", R"NS(
namespace PublicReferenceArraySurface
{
    public interface IService
    {
        int Read();
    }

    public class Item
    {
    }

    public class Api
    {
        private IService[] services;
        private Item[] items;

        public IService[] Services { get { return services; } }
        public Item[] Items { get { return items; } }

        public static IService[] EchoServices(IService[] values)
        {
            return values;
        }

        public static Item[] EchoItems(Item[] values)
        {
            return values;
        }
    }
}
)NS");

    const auto project = directory / "PublicReferenceArrayDllSurface.nsproj";
    const auto library = directory / "PublicReferenceArrayDllSurface.lib";
    const auto runtime = directory / "PublicReferenceArrayDllSurface.dll";
    const auto metadata = directory / "PublicReferenceArrayDllSurface.nsmeta";
    std::string output;
    std::string error;
    tests.expect(emitProduct("--emit-project-oop-dll", project, runtime, output, error) == ExitCode::Success,
                 "DLL with public class and interface arrays in methods and properties is emitted (" +
                     error + ")");
    output.clear(); error.clear();
    tests.expect(verifyArtifacts(metadata, library, runtime, output, error) == ExitCode::Success,
                 "NSMETA publication and DLL exports agree for arrays of public reference types (" +
                     error + ")");
}

void testDelegateAggregateSignaturesPublishCompleteDllContract(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const auto producer = root / "DelegateAggregateProducer";
    writeProject(producer, "DelegateAggregateProducer", "SharedLibrary", R"NS(
namespace DelegateAggregateContracts
{
    internal struct CallbackPayload
    {
        public int Value;
    }

    public extern C delegate void CallbackA(int value);
    public extern C delegate void CallbackB(void* context);
    public extern C delegate void RecursiveCallback(RecursiveConfiguration* configuration);
    public extern C delegate void PayloadCallback(CallbackPayload* payload);

    public struct Configuration
    {
        public CallbackA First;
        public CallbackB Second;
        public void* UserData;
    }

    public struct RecursiveConfiguration
    {
        public RecursiveCallback Callback;
    }

    public class Api
    {
        public static void AcceptDelegate(CallbackA callback)
        {
        }

        public static void AcceptStructPointer(Configuration* configuration)
        {
        }

        public static Configuration ReturnStruct()
        {
            return default(Configuration);
        }

        public static Configuration* ReturnStructPointer()
        {
            return null;
        }

        public static Configuration Create(
            CallbackA first,
            CallbackB second,
            void* userData)
        {
            Configuration result = default(Configuration);
            result.First = first;
            result.Second = second;
            result.UserData = userData;
            return result;
        }

        public static void AcceptRecursive(RecursiveCallback callback)
        {
        }

        public static RecursiveConfiguration ReturnRecursive()
        {
            return default(RecursiveConfiguration);
        }

        public static void AcceptPayload(PayloadCallback callback)
        {
        }
    }
}
)NS");

    const auto project = producer / "DelegateAggregateProducer.nsproj";
    const auto library = producer / "DelegateAggregateProducer.lib";
    const auto runtime = producer / "DelegateAggregateProducer.dll";
    const auto metadataPath = producer / "DelegateAggregateProducer.nsmeta";
    std::string output;
    std::string error;
    const std::string producerProject = project.string();
    const std::string producerRuntime = runtime.string();
    const std::array<std::string_view, 5> producerArgs = {
        "-O0", "--emit-project-oop-dll", producerProject, "-o", producerRuntime};
    tests.expect(runDriver(producerArgs, output, error) == ExitCode::Success,
                 "delegate-aggregate producer emits DLL/LIB/NSMETA (" + error + ")");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    error.clear();
    const bool read = codec.read(metadataPath, metadata, error);
    tests.expect(read, "delegate-aggregate NSMETA decodes (" + error + ")");
    const LibraryMetadataType* callbackA =
        findType(metadata, "::DelegateAggregateContracts.CallbackA");
    const LibraryMetadataType* callbackB =
        findType(metadata, "::DelegateAggregateContracts.CallbackB");
    const LibraryMetadataType* configuration =
        findType(metadata, "::DelegateAggregateContracts.Configuration");
    const LibraryMetadataType* callbackPayload =
        findType(metadata, "::DelegateAggregateContracts.CallbackPayload");
    tests.expect(read && callbackA != nullptr && callbackB != nullptr &&
                     callbackA->kind == LibraryMetadataTypeKind::Delegate &&
                     callbackB->kind == LibraryMetadataTypeKind::Delegate &&
                     callbackA->delegateFlags == 1U && callbackB->delegateFlags == 1U,
                 "NSMETA preserves both extern C delegate identities and calling conventions");
    tests.expect(read && configuration != nullptr &&
                     configuration->kind == LibraryMetadataTypeKind::Struct &&
                     configuration->fields.size() == 3U &&
                     configuration->fields[0].typeKey.ends_with(
                         "::DelegateAggregateContracts.CallbackA") &&
                     configuration->fields[1].typeKey.ends_with(
                         "::DelegateAggregateContracts.CallbackB"),
                 "NSMETA preserves delegate fields in the public struct layout");
    tests.expect(read && callbackPayload != nullptr &&
                     callbackPayload->kind == LibraryMetadataTypeKind::Struct,
                 "delegate signature closure publishes its non-root payload type");
    const std::array<std::string_view, 8> expectedMethods = {
        "AcceptDelegate", "AcceptStructPointer", "ReturnStruct", "ReturnStructPointer", "Create",
        "AcceptRecursive", "ReturnRecursive", "AcceptPayload"};
    for (const std::string_view name : expectedMethods)
    {
        tests.expect(std::ranges::any_of(metadata.members, [&](const LibraryMetadataMember& member) {
                         return member.name == name &&
                             member.declaringTypeKey.ends_with(
                                 "::DelegateAggregateContracts.Api") &&
                             !member.mangledName.empty();
                     }),
                     "NSMETA publishes delegate-reachable method '" + std::string(name) + "'");
    }

    output.clear();
    error.clear();
    tests.expect(verifyArtifacts(metadataPath, library, runtime, output, error) == ExitCode::Success,
                 "delegate-aggregate DLL exports exactly match NSMETA (" + error + ")");

    const auto consumer = root / "DelegateAggregateConsumer";
    writeFile(consumer / "DelegateAggregateConsumer.nsproj",
        "<Project><PropertyGroup><Name>DelegateAggregateConsumer</Name>"
        "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"" + library.generic_string() + "\" Metadata=\"" +
        metadataPath.generic_string() + "\" Runtime=\"" + runtime.generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(consumer / "Program.ns", R"NS(
using DelegateAggregateContracts;

class Program
{
    public static void OnA(int value)
    {
    }

    public static void OnB(void* context)
    {
    }

    public static int Main()
    {
        CallbackA first = Program.OnA;
        CallbackB second = Program.OnB;
        RecursiveCallback recursive = Program.OnRecursive;
        Configuration configuration = Api.Create(first, second, null);
        Api.AcceptDelegate(first);
        Api.AcceptStructPointer(&configuration);
        Configuration returned = Api.ReturnStruct();
        Configuration* pointer = Api.ReturnStructPointer();
        Api.AcceptRecursive(recursive);
        RecursiveConfiguration recursiveConfiguration = Api.ReturnRecursive();
        return returned.UserData == null && pointer == null &&
            recursiveConfiguration.Callback == null ? 0 : 1;
    }

    public static void OnRecursive(RecursiveConfiguration* configuration)
    {
    }
}
)NS");

    const std::string consumerProject =
        (consumer / "DelegateAggregateConsumer.nsproj").string();
    const std::string executable = (consumer / "DelegateAggregateConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArgs = {
        "-O0", "--emit-project-oop-exe", consumerProject, "-o", executable};
    output.clear();
    error.clear();
    tests.expect(runDriver(consumerArgs, output, error) == ExitCode::Success,
                 "source-free LibraryReference consumer compiles every delegate-reachable method (" +
                     error + ")");
}
void testPrecompiledStoreGenericKeepsModuleQualifier(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const auto store = root / "ModuleQualifiedStore";
    writeFile(store / "ModuleQualifiedStore.nsproj",
        "<Project><PropertyGroup><Name>ModuleQualifiedStore</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(store / "Store.ns", R"NS(
namespace NativeSharp.Core
{
    public struct Span<T>
    {
        public T Value;
    }

    public struct ReadOnlySpan<T>
    {
        public T Value;
    }
}
)NS");

    const auto producer = root / "ModuleQualifiedProducer";
    writeFile(producer / "ModuleQualifiedProducer.nsproj",
        "<Project><PropertyGroup><Name>ModuleQualifiedProducer</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "<ProjectReference Include=\"" + (store / "ModuleQualifiedStore.nsproj").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(producer / "Producer.ns", R"NS(
namespace Foundation.Binary
{
    public class BinaryReader
    {
        public static byte Read(NativeSharp.Core.ReadOnlySpan<byte> source)
        {
            return source.Value;
        }

        public static void Write(NativeSharp.Core.Span<byte> destination, byte value)
        {
            destination.Value = value;
        }
    }
}
)NS");

    std::string output;
    std::string error;
    const std::string producerProject = (producer / "ModuleQualifiedProducer.nsproj").string();
    const std::string producerLibrary = (producer / "ModuleQualifiedProducer.lib").string();
    const std::array<std::string_view, 5> producerArgs = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    tests.expect(runDriver(producerArgs, output, error) == ExitCode::Success,
        "producer emits O0 metadata whose public surface uses two store-owned generic types (" +
            error + ")");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    error.clear();
    const bool read = codec.read(producer / "ModuleQualifiedProducer.nsmeta", metadata, error);
    tests.expect(read, "producer metadata with store-owned generic types can be decoded (" + error + ")");
    const bool keepsStoreTemplateIdentity = std::ranges::any_of(
        metadata.genericSpecializations, [](const auto& specialization) {
            return specialization.templateKey ==
                       "ModuleQualifiedStore::NativeSharp.Core.ReadOnlySpan`1" ||
                   specialization.templateKey ==
                       "ModuleQualifiedStore::NativeSharp.Core.Span`1";
        });
    tests.expect(read && keepsStoreTemplateIdentity,
        "NSMETA keeps the canonical store module on closed generic template identities");

    const auto consumer = root / "ModuleQualifiedConsumer";
    writeFile(consumer / "ModuleQualifiedConsumer.nsproj",
        "<Project><PropertyGroup><Name>ModuleQualifiedConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" />"
        "<ProjectReference Include=\"" + (store / "ModuleQualifiedStore.nsproj").generic_string() +
        "\" />"
        "<LibraryReference Include=\"" + (producer / "ModuleQualifiedProducer.lib").generic_string() +
        "\" Metadata=\"" + (producer / "ModuleQualifiedProducer.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(consumer / "Program.ns",
        "class Program { public static int Main() { return 0; } }");

    const std::string consumerProject = (consumer / "ModuleQualifiedConsumer.nsproj").string();
    const std::array<std::string_view, 2> consumerArgs = {
        "--check-project-oop-hir", consumerProject};
    output.clear();
    error.clear();
    tests.expect(runDriver(consumerArgs, output, error) == ExitCode::Success,
        "LibraryReference public source resolves store generics by their NSMETA module identity (" +
            error + ")");
}

void testTypedParameterPackRoundTrip(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const auto producer = root / "TypedParameterPackProducer";
    writeFile(producer / "TypedParameterPackProducer.nsproj",
        "<Project><PropertyGroup><Name>TypedParameterPackProducer</Name>"
        "<OutputType>StaticLibrary</OutputType><Language>3</Language></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Api.ns", R"NS(
namespace TypedPacks
{
    public borrowed struct ReadOnlySpan<T>
    {
        public T* start;
        public int count;
    }

    public static class Api
    {
        public static int Sum(params ReadOnlySpan<int> values)
        {
            int result = 0;
            int index = 0;
            while (index < values.count)
            {
                result = result + values.start[index];
                index = index + 1;
            }
            return result;
        }
    }
}
)NS");

    std::string output;
    std::string error;
    const std::string producerProject =
        (producer / "TypedParameterPackProducer.nsproj").string();
    const std::string producerLibrary =
        (producer / "TypedParameterPackProducer.lib").string();
    const std::array<std::string_view, 5> producerArgs = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    tests.expect(runDriver(producerArgs, output, error) == ExitCode::Success,
        "language 3 producer emits an O0 library with a typed parameter pack (" + error + ")");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    error.clear();
    const bool read = codec.read(
        producer / "TypedParameterPackProducer.nsmeta", metadata, error);
    const auto member = std::ranges::find_if(
        metadata.members, [](const LibraryMetadataMember& candidate) {
            return candidate.name == "Sum";
        });
    tests.expect(read && metadata.formatMinor >= 24U && metadata.languageEdition == 3U &&
                     member != metadata.members.end() && member->parameters.size() == 1U &&
                     member->parameters.front().isParameterPack,
        "NSMETA 1.24 preserves language edition and params on the public signature (" +
            error + ")");

    LibraryMetadata downlevel = metadata;
    downlevel.formatMinor = 23U;
    error.clear();
    tests.expect(
        !codec.writeModel(
            downlevel, producer / "TypedParameterPackProducer-1.23.nsmeta", error) &&
            error.find("before 1.24 cannot represent a params") != std::string::npos,
        "NSMETA 1.23 rejects rather than silently erases a typed parameter-pack contract (" +
            error + ")");

    const auto consumer = root / "TypedParameterPackConsumer";
    writeFile(consumer / "TypedParameterPackConsumer.nsproj",
        "<Project><PropertyGroup><Name>TypedParameterPackConsumer</Name>"
        "<Language>3</Language></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"" + producerLibrary + "\" Metadata=\"" +
        (producer / "TypedParameterPackProducer.nsmeta").generic_string() +
        "\" /></ItemGroup></Project>");
    writeFile(consumer / "Program.ns", R"NS(
using TypedPacks;
class Program
{
    public static int Main()
    {
        return Api.Sum(20, 22);
    }
}
)NS");

    const std::string consumerProject =
        (consumer / "TypedParameterPackConsumer.nsproj").string();
    const std::array<std::string_view, 2> consumerArgs = {
        "--check-project-oop-hir", consumerProject};
    output.clear();
    error.clear();
    tests.expect(runDriver(consumerArgs, output, error) == ExitCode::Success,
        "LibraryReference rehydrates params and accepts an expanded typed pack (" + error + ")");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage48_2_library_metadata_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testExplicitRoundTrip(tests, root);
    testDeterminismAndAbiSensitivity(tests, root);
    testDefaultValueContractSensitivity(tests, root);
    testAutomaticProducts(tests, root);
    testGenericCompilerMetadata(tests, root);
    testPrecompiledClosedGenericSurface(tests, root);
    testPrecompiledCallableKeepsNominalParameterIdentity(tests, root);
    testPrecompiledExternalTypeClosure(tests, root);
    testLibraryMaterializesCompletePublicClosedGenericSurface(tests, root);
    testIntegrityAndCompatibility(tests, root);
    testScopeAndFailureBehavior(tests, root);
    testInterfaceContractsUseDeclaringTypeKind(tests, root);
    testPublicFieldInsideInternalTypeIsNotRequired(tests, root);
    testPrivateStaticFieldIsNotPublished(tests, root);
    testPublicInterfaceSignaturesMatchDllExports(tests, root);
    testPrecompiledStoreGenericKeepsModuleQualifier(tests, root);
    testTypedParameterPackRoundTrip(tests, root);
    testPublicReferenceArraysMatchDllExports(tests, root);
    testDelegateAggregateSignaturesPublishCompleteDllContract(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
