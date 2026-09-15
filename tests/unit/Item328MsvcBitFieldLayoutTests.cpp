#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include "../support/TestContext.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

void writeProject(const std::filesystem::path& root, std::string_view name, std::string_view source)
{
    writeFile(root / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>StaticLibrary</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", source);
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode code = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return code;
}

nativesharp::target::StructFieldRequest bitField(
    std::uint64_t size,
    std::uint64_t alignment,
    std::uint64_t width,
    bool unnamed = false)
{
    return {{size, alignment}, std::nullopt, std::nullopt, false, width, unnamed};
}

nativesharp::target::StructFieldRequest ordinary(std::uint64_t size, std::uint64_t alignment)
{
    return {{size, alignment}, std::nullopt, std::nullopt, false, std::nullopt, false};
}

void expectRejected(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view field,
    std::string_view diagnostic)
{
    const auto projectRoot = root / std::string(name);
    writeProject(projectRoot, name,
        "public struct Bits { " + std::string(field) + " public byte Tail; }");
    const std::string project = (projectRoot / (std::string(name) + ".nsproj")).string();
    const std::array<std::string_view, 2> arguments = {"--dump-project-layouts", project};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success &&
                     error.find(diagnostic) != std::string::npos,
        std::string(name) + " rejects invalid bit-field layout (" + error + ")");
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::target::StructFieldRequest;
    using nativesharp::target::StructLayoutOptions;
    using nativesharp::target::TargetDataLayout;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item328_msvc_bitfield_layout_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const TargetDataLayout target = TargetDataLayout::WindowsX64();
    const std::array sameType = {
        bitField(4, 4, 1), bitField(4, 4, 3), bitField(4, 4, 28), bitField(4, 4, 1),
        ordinary(1, 1)};
    const auto same = target.layoutStruct(sameType, {});
    tests.expect(same.has_value() && same->size == 12 && same->alignment == 4 &&
                     same->fields[0].offset == 0 && *same->fields[0].bitOffset == 0 &&
                     *same->fields[1].bitOffset == 1 && *same->fields[2].bitOffset == 4 &&
                     same->fields[3].offset == 4 && *same->fields[3].bitOffset == 0 &&
                     same->fields[4].offset == 8,
        "MSVC fills and then reopens same-sized allocation units");

    const std::array changedType = {
        bitField(1, 1, 4), bitField(1, 1, 4), bitField(2, 2, 3), bitField(4, 4, 5),
        ordinary(1, 1)};
    const auto changed = target.layoutStruct(changedType, {});
    tests.expect(changed.has_value() && changed->size == 12 && changed->alignment == 4 &&
                     changed->fields[0].offset == 0 && changed->fields[1].offset == 0 &&
                     changed->fields[2].offset == 2 && changed->fields[3].offset == 4 &&
                     changed->fields[4].offset == 8,
        "changing allocation-unit size closes and aligns the previous unit");

    const std::array zeroWidth = {
        bitField(1, 1, 1), bitField(4, 4, 0, true),
        ordinary(1, 1)};
    const auto zero = target.layoutStruct(zeroWidth, {});
    tests.expect(zero.has_value() && zero->size == 8 && zero->alignment == 4 &&
                     zero->fields[1].offset == 4 && zero->fields[2].offset == 4,
        "a zero-width declaration flushes a live unit and aligns the next field");

    const std::array leadingZero = {
        bitField(4, 4, 0, true),
        ordinary(1, 1)};
    const auto leading = target.layoutStruct(leadingZero, {});
    tests.expect(leading.has_value() && leading->size == 1 && leading->alignment == 1 &&
                     leading->fields[1].offset == 0,
        "a leading zero-width declaration has no MSVC layout effect");

    const std::array packedFields = {
        bitField(4, 4, 3), bitField(4, 4, 5),
        ordinary(1, 1)};
    StructLayoutOptions packedOptions;
    packedOptions.pack = 1;
    const auto packed = target.layoutStruct(packedFields, packedOptions);
    tests.expect(packed.has_value() && packed->size == 5 && packed->alignment == 1 &&
                     packed->fields[0].size == 4 && packed->fields[2].offset == 4,
        "pack caps a bit-field unit's alignment without shrinking its storage");

    const std::array unionFields = {
        bitField(4, 4, 3), bitField(2, 2, 7),
        ordinary(1, 1)};
    StructLayoutOptions unionOptions;
    unionOptions.isUnion = true;
    const auto unionLayout = target.layoutStruct(unionFields, unionOptions);
    tests.expect(unionLayout.has_value() && unionLayout->size == 4 && unionLayout->alignment == 1 &&
                     unionLayout->fields[0].offset == 0 && unionLayout->fields[1].offset == 0 &&
                     *unionLayout->fields[0].bitOffset == 0 && *unionLayout->fields[1].bitOffset == 0,
        "MSVC union bit-fields affect extent but not union alignment");

    const auto projectRoot = root / "project";
    writeProject(projectRoot, "BitFieldLayoutItem328", R"NS(
public struct SameType
{
    public uint A : 1;
    public int B : 3;
    public uint C : 28;
    public uint D : 1;
    public byte Tail;
}
public struct ZeroWidth
{
    public byte A : 1;
    public uint : 0;
    public byte Tail;
}
public pack(1) struct Packed
{
    public uint A : 3;
    public uint B : 5;
    public byte Tail;
}
public union UnionBits
{
    public uint A : 3;
    public ushort B : 7;
    public byte Tail;
}
public class ClassBits
{
    public uint A : 3;
    public int B : 5;
    public byte Tail;
}
public class Api
{
    public static export extern C SameType EchoSame(SameType value) { return value; }
    public static export extern C ZeroWidth EchoZero(ZeroWidth value) { return value; }
    public static export extern C Packed EchoPacked(Packed value) { return value; }
    public static export extern C UnionBits EchoUnion(UnionBits value) { return value; }
}
)NS");
    const std::string project = (projectRoot / "BitFieldLayoutItem328.nsproj").string();
    const std::array<std::string_view, 2> layoutArguments = {"--dump-project-layouts", project};
    std::string output;
    std::string error;
    const bool layoutAccepted = runDriver(layoutArguments, output, error) == ExitCode::Success;
    tests.expect(layoutAccepted, "project bit-fields receive physical MSVC layout (" + error + ")");
    if (layoutAccepted)
    {
        tests.expect(output.find("struct type:BitFieldLayoutItem328::SameType size=12 align=4") !=
                         std::string::npos &&
                         output.find("name=B type=builtin:int offset=0 size=4 align=4 bit-offset=1 bit-width=3") !=
                             std::string::npos &&
                         output.find("name=D type=builtin:uint offset=4 size=4 align=4 bit-offset=0 bit-width=1") !=
                             std::string::npos,
            "signed and unsigned fields of equal width share the measured MSVC unit");
        tests.expect(output.find("struct type:BitFieldLayoutItem328::ZeroWidth size=8 align=4") !=
                         std::string::npos &&
                         output.find("offset=4 size=0 align=4 bit-offset=0 bit-width=0") !=
                             std::string::npos,
            "the project model publishes zero-width alignment barriers");
        tests.expect(output.find("struct type:BitFieldLayoutItem328::Packed size=5 align=1") !=
                         std::string::npos &&
                         output.find("struct type:BitFieldLayoutItem328::UnionBits size=4 align=1") !=
                             std::string::npos,
            "pack and union results agree with the MSVC oracle");
        tests.expect(output.find("class type:BitFieldLayoutItem328::ClassBits size=16 align=8 header=8") !=
                         std::string::npos &&
                         output.find("name=A type=builtin:uint offset=8 size=4 align=4 bit-offset=0 bit-width=3") !=
                             std::string::npos &&
                         output.find("name=B type=builtin:int offset=8 size=4 align=4 bit-offset=3 bit-width=5") !=
                             std::string::npos &&
                         output.find("name=Tail type=builtin:byte offset=12 size=1 align=1") !=
                             std::string::npos,
            "class bit-fields share the canonical MSVC layout after the object header");
    }

    expectRejected(tests, root, "FloatBase", "public float Value : 1;",
        "a bit-field base type must be an integer, bool, or enum");
    expectRejected(tests, root, "NamedZero", "public uint Value : 0;",
        "a named bit-field cannot have width zero");
    expectRejected(tests, root, "TooWide", "public byte Value : 9;",
        "bit-field width exceeds its storage type");
    expectRejected(tests, root, "Negative", "public int Value : -1;",
        "bit-field width must be a nonnegative integral constant expression");

    writeFile(projectRoot / "BitFieldLayoutItem328.nsproj",
        "<Project><PropertyGroup><Name>BitFieldLayoutItem328</Name>"
        "<OutputType>StaticLibrary</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    const std::string producerLibrary = (projectRoot / "BitFieldLayoutItem328.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", project, "-o", producerLibrary};
    output.clear();
    error.clear();
    const bool producerEmitted = runDriver(producerArguments, output, error) == ExitCode::Success;
    tests.expect(producerEmitted,
        "a bit-field ABI producer emits its library, header, and NSMETA 1.18 (" + error + ")");

    std::ifstream headerStream(projectRoot / "BitFieldLayoutItem328.h", std::ios::binary);
    const std::string headerText(
        (std::istreambuf_iterator<char>(headerStream)), std::istreambuf_iterator<char>());
    tests.expect(headerStream.good() &&
                     headerText.find("uint32_t A : 1;") != std::string::npos &&
                     headerText.find("int32_t B : 3;") != std::string::npos &&
                     headerText.find("uint32_t : 0;") != std::string::npos &&
                     headerText.find("offsetof(SameType, A)") == std::string::npos,
        "the C/MSVC header spells named and unnamed bit-fields without illegal offsetof checks");

    nativesharp::interop::LibraryMetadata metadata;
    nativesharp::interop::LibraryMetadataCodec codec;
    std::string metadataError;
    const auto metadataPath = projectRoot / "BitFieldLayoutItem328.nsmeta";
    const bool metadataRead = producerEmitted && codec.read(metadataPath, metadata, metadataError);
    bool foundNamedCoordinates = false;
    bool foundUnnamedBarrier = false;
    if (metadataRead)
    {
        for (const auto& type : metadata.types)
        {
            for (const auto& field : type.fields)
            {
                foundNamedCoordinates = foundNamedCoordinates ||
                    (field.name == "B" &&
                     (field.flags & nativesharp::interop::kMetadataFieldBitField) != 0U &&
                     field.offset == 0 && field.bitOffset == 1 && field.bitWidth == 3);
                foundUnnamedBarrier = foundUnnamedBarrier ||
                    ((field.flags & nativesharp::interop::kMetadataFieldUnnamedBitField) != 0U &&
                     field.bitWidth == 0);
            }
        }
    }
    tests.expect(metadataRead &&
                     metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor &&
                     foundNamedCoordinates && foundUnnamedBarrier,
        "NSMETA 1.18 preserves exact named and unnamed bit coordinates (" + metadataError + ")");

    nativesharp::interop::LibraryMetadata downlevel = metadata;
    downlevel.formatMinor = 17U;
    metadataError.clear();
    tests.expect(metadataRead &&
                     !codec.writeModel(downlevel, projectRoot / "unsupported-1.17.nsmeta", metadataError) &&
                     metadataError.find("before 1.18 cannot represent bit-field layout") !=
                         std::string::npos,
        "NSMETA refuses a lossy pre-1.18 bit-field downlevel (" + metadataError + ")");

    const auto consumerRoot = root / "consumer";
    writeFile(consumerRoot / "BitFieldConsumer.nsproj",
        "<Project><PropertyGroup><Name>BitFieldConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + producerLibrary + "\" Metadata=\"" +
        metadataPath.string() + "\" /></ItemGroup></Project>");
    writeFile(consumerRoot / "Main.ns", R"NS(
public class Program
{
    public static int Main() { return sizeof(SameType) == 12 ? 0 : 1; }
}
)NS");
    const std::string consumerProject = (consumerRoot / "BitFieldConsumer.nsproj").string();
    const std::array<std::string_view, 2> consumerArguments = {
        "--dump-project-layouts", consumerProject};
    output.clear();
    error.clear();
    const bool consumerAccepted = runDriver(consumerArguments, output, error) == ExitCode::Success;
    tests.expect(consumerAccepted &&
                     output.find("SameType size=12 align=4") != std::string::npos &&
                     output.find("bit-offset=1 bit-width=3") != std::string::npos,
        "LibraryReference reconstructs the producer's bit-field layout (" + error + ")");

    return tests.resultKeeping(root);
}
