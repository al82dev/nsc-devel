#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

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
        "<Project><PropertyGroup><Name>" + std::string(name) + "</Name></PropertyGroup>"
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

void expectRejected(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view diagnostic)
{
    const auto projectRoot = root / std::string(name);
    writeProject(projectRoot, name, source);
    const std::string project = (projectRoot / (std::string(name) + ".nsproj")).string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
        std::string(name) + " is rejected");
    tests.expect(error.find(diagnostic) != std::string::npos,
        std::string(name) + " reports '" + std::string(diagnostic) + "' (" + error + ")");
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) /
        "item326_anonymous_member_promotion_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto valid = root / "valid";
    writeProject(valid, "AnonymousPromotionItem326", R"NS(
public struct Packet
{
    public byte Tag;
    public struct
    {
        public short Low;
        public int High;
    };
    public byte Tail;
    public union
    {
        public int Bits;
        public float FloatBits;
        public byte[4] Bytes;
    };
}

public class Program
{
    public static int Main()
    {
        Packet packet;
        packet.Tag = 1;
        packet.Low = 2;
        packet.High = 35;
        packet.Tail = 4;
        packet.Bits = 42;
        if (packet.Bytes[0] != 42) { return 1; }
        if (offsetof(Packet, Low) != 4 || offsetof(Packet, High) != 8) { return 2; }
        if (offsetof(Packet, Bits) != 16 || offsetof(Packet, Bytes) != 16) { return 3; }
        return packet.Tag + packet.Low + packet.High + packet.Tail == 42 ? 0 : 4;
    }
}
)NS");

    const std::string project = (valid / "AnonymousPromotionItem326.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 2> memberArguments = {"--dump-project-members", project};
    const bool members = runDriver(memberArguments, output, error) == ExitCode::Success;
    tests.expect(members, "promoted fields reach the project member model (" + error + ")");
    if (members)
    {
        tests.expect(output.find("promoted-member[") != std::string::npos &&
                         output.find(" field Low type=builtin:short via=") != std::string::npos &&
                         output.find(" field High type=builtin:int via=") != std::string::npos,
            "anonymous-struct members are semantic aliases on the containing type");
        tests.expect(output.find(" field Bits type=builtin:int via=") != std::string::npos &&
                         output.find(" field Bytes type=builtin:byte[4] via=") != std::string::npos,
            "anonymous-union alternatives are semantic aliases on the containing type");
        tests.expect(output.find("field __nsc_anonymous_member_f") != std::string::npos,
            "physical carrier fields remain explicit in the internal member model");
    }

    const std::array<std::string_view, 2> layoutArguments = {"--dump-project-layouts", project};
    output.clear();
    error.clear();
    const bool layouts = runDriver(layoutArguments, output, error) == ExitCode::Success;
    tests.expect(layouts, "promoted fields preserve canonical physical layout (" + error + ")");
    if (layouts)
    {
        tests.expect(output.find("struct type:AnonymousPromotionItem326::Packet size=20 align=4") !=
                         std::string::npos,
            "the containing struct retains anonymous-struct padding and anonymous-union extent");
        tests.expect(output.find("name=__nsc_anonymous_member_f") != std::string::npos &&
                         output.find("offset=4 size=8 align=4") != std::string::npos &&
                         output.find("offset=16 size=4 align=4") != std::string::npos,
            "only hidden carriers occupy physical storage in the outer layout");
        tests.expect(output.find("name=Low type=builtin:short offset=4") == std::string::npos &&
                         output.find("name=Bits type=builtin:int offset=16") == std::string::npos,
            "promoted aliases are not duplicated as physical fields");
    }

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", project, "-O0"};
    output.clear();
    error.clear();
    const bool mir = runDriver(mirArguments, output, error) == ExitCode::Success;
    tests.expect(mir, "promoted accesses lower to verifier-clean O0 MIR (" + error + ")");
    if (mir)
    {
        tests.expect(output.find("field-address type=builtin:short member=") != std::string::npos &&
                         output.find("offset=4 align=2") != std::string::npos &&
                         output.find("offset=8 align=4") != std::string::npos,
            "struct promotions use carrier-plus-source offsets");
        tests.expect(output.find("field-address type=builtin:byte[4] member=") != std::string::npos &&
                         output.find("offset=16 align=4") != std::string::npos &&
                         output.find("offset=16 align=1") != std::string::npos,
            "union promotions share their carrier offset");
    }

    const std::string executable = (valid / "AnonymousPromotion.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "promoted-member fixture links at O0 (" + error + ")");
    if (emitted)
    {
        tests.expect(std::system(("\"" + executable + "\"").c_str()) == 0,
            "promoted struct and union reads, writes and offsetof execute correctly");
    }

    const auto nested = root / "nested";
    writeProject(nested, "NestedPromotionItem326", R"NS(
public struct Deep
{
    public struct
    {
        public byte A;
        public union
        {
            public int B;
            public byte[4] BBytes;
        };
        public short C;
    };
}
public class Program
{
    public static int Main()
    {
        Deep value;
        value.A = 1;
        value.B = 42;
        value.C = 3;
        return value.BBytes[0] == 42 && offsetof(Deep, A) == 0 &&
            offsetof(Deep, B) == 4 && offsetof(Deep, C) == 8 ? 0 : 1;
    }
}
)NS");
    const std::string nestedProject = (nested / "NestedPromotionItem326.nsproj").string();
    const std::string nestedExecutable = (nested / "NestedPromotion.exe").string();
    const std::array<std::string_view, 5> nestedArguments = {
        "-O0", "--emit-project-oop-exe", nestedProject, "-o", nestedExecutable};
    output.clear();
    error.clear();
    const bool nestedEmitted = runDriver(nestedArguments, output, error) == ExitCode::Success;
    tests.expect(nestedEmitted, "recursive promotion paths lower and link (" + error + ")");
    if (nestedEmitted)
    {
        tests.expect(std::system(("\"" + nestedExecutable + "\"").c_str()) == 0,
            "recursive carrier paths execute with offsets 0, 4 and 8");
    }

    const auto packed = root / "packed";
    writeProject(packed, "PackedPromotionItem326", R"NS(
public struct Wire
{
    public byte Lead;
    public pack(1) struct { public short Code; public int Count; };
    public byte Tail;
}
public class Program
{
    public static int Main()
    {
        Wire value;
        value.Code = 2;
        value.Count = 40;
        return offsetof(Wire, Code) == 1 && offsetof(Wire, Count) == 3 &&
            offsetof(Wire, Tail) == 7 && value.Code + value.Count == 42 ? 0 : 1;
    }
}
)NS");
    const std::string packedProject = (packed / "PackedPromotionItem326.nsproj").string();
    const std::array<std::string_view, 2> packedLayoutArguments = {
        "--dump-project-layouts", packedProject};
    output.clear();
    error.clear();
    const bool packedLayout = runDriver(packedLayoutArguments, output, error) == ExitCode::Success;
    tests.expect(packedLayout, "promoted fields preserve anonymous pack boundaries (" + error + ")");
    if (packedLayout)
    {
        tests.expect(output.find("struct type:PackedPromotionItem326::Wire size=8 align=1") !=
                         std::string::npos && output.find("offset=1 size=6 align=1") !=
                         std::string::npos,
            "the packed anonymous struct remains one physical six-byte carrier");
    }
    const std::string packedExecutable = (packed / "PackedPromotion.exe").string();
    const std::array<std::string_view, 5> packedExecutableArguments = {
        "-O0", "--emit-project-oop-exe", packedProject, "-o", packedExecutable};
    output.clear();
    error.clear();
    const bool packedEmitted = runDriver(packedExecutableArguments, output, error) == ExitCode::Success;
    tests.expect(packedEmitted, "packed promotion fixture links at O0 (" + error + ")");
    if (packedEmitted)
    {
        tests.expect(std::system(("\"" + packedExecutable + "\"").c_str()) == 0,
            "packed promoted offsets 1 and 3 execute without physical flattening");
    }

    const auto producer = root / "producer";
    writeFile(producer / "PromotionProducer.nsproj",
        "<Project><PropertyGroup><Name>PromotionProducer</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Api.ns", R"NS(
namespace CrossPromotion
{
    public struct Packet
    {
        public byte Tag;
        public struct
        {
            public int Low;
            public union
            {
                public int Bits;
                public byte[4] Bytes;
            };
        };
    }
    public class Api
    {
        public static export extern C int Read(Packet value) { return value.Tag + value.Low + value.Bits; }
    }
}
)NS");
    const std::string producerProject = (producer / "PromotionProducer.nsproj").string();
    const std::string producerLibrary = (producer / "PromotionProducer.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    output.clear();
    error.clear();
    const bool producerEmitted = runDriver(producerArguments, output, error) == ExitCode::Success;
    tests.expect(producerEmitted,
        "anonymous promotion producer emits its static library and NSMETA 1.17 (" + error + ")");
    std::ifstream headerStream(producer / "PromotionProducer.h", std::ios::binary);
    const std::string headerText(
        (std::istreambuf_iterator<char>(headerStream)), std::istreambuf_iterator<char>());
    tests.expect(headerStream.good() &&
                     headerText.find("int32_t Low;") != std::string::npos &&
                     headerText.find("int32_t Bits;") != std::string::npos &&
                     headerText.find("uint8_t Bytes[4];") != std::string::npos &&
                     headerText.find("__nsc_anonymous_member_f") == std::string::npos &&
                     headerText.find("__nsc_anonymous_struct_f") == std::string::npos &&
                     headerText.find("__nsc_anonymous_union_f") == std::string::npos,
        "the generated C header exposes promoted anonymous fields without hidden carrier names");

    nativesharp::interop::LibraryMetadata metadata;
    nativesharp::interop::LibraryMetadataCodec metadataCodec;
    std::string metadataError;
    const auto metadataPath = producer / "PromotionProducer.nsmeta";
    const bool metadataRead = producerEmitted && metadataCodec.read(metadataPath, metadata, metadataError);
    bool carrierFlag = false;
    if (metadataRead)
    {
        for (const auto& metadataType : metadata.types)
            for (const auto& field : metadataType.fields)
                carrierFlag = carrierFlag ||
                    (field.flags & nativesharp::interop::kMetadataFieldAnonymousCarrier) != 0U;
    }
    tests.expect(metadataRead &&
                     metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor && carrierFlag,
        "NSMETA 1.17 preserves the anonymous physical-carrier marker (" + metadataError + ")");

    const auto historicalMetadataPath = root / "historical-1.16.nsmeta";
    nativesharp::interop::LibraryMetadata historicalMetadata = metadata;
    historicalMetadata.formatMinor = 16U;
    metadataError.clear();
    const bool historicalWritten = metadataRead &&
        metadataCodec.writeModel(historicalMetadata, historicalMetadataPath, metadataError);
    nativesharp::interop::LibraryMetadata historicalDecoded;
    const bool historicalRead = historicalWritten &&
        metadataCodec.read(historicalMetadataPath, historicalDecoded, metadataError);
    bool historicalCarrierFlag = false;
    if (historicalRead)
    {
        for (const auto& metadataType : historicalDecoded.types)
            for (const auto& field : metadataType.fields)
                historicalCarrierFlag = historicalCarrierFlag ||
                    (field.flags & nativesharp::interop::kMetadataFieldAnonymousCarrier) != 0U;
    }
    tests.expect(historicalRead && historicalDecoded.formatMinor == 16U && !historicalCarrierFlag,
        "NSMETA 1.16 downlevel output removes the item-326 marker (" + metadataError + ")");

    const auto consumer = root / "consumer";
    writeFile(consumer / "PromotionConsumer.nsproj",
        "<Project><PropertyGroup><Name>PromotionConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + producerLibrary + "\" Metadata=\"" +
        metadataPath.string() + "\" /></ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using CrossPromotion;
public class Program
{
    public static int Main()
    {
        Packet value;
        value.Tag = 1;
        value.Low = 20;
        value.Bits = 21;
        if (value.Bytes[0] != 21) { return 1; }
        if (offsetof(Packet, Tag) != 0 || offsetof(Packet, Low) != 4 ||
            offsetof(Packet, Bits) != 8) { return 2; }
        return Api.Read(value) == 42 ? 0 : 3;
    }
}
)NS");
    const std::string consumerProject = (consumer / "PromotionConsumer.nsproj").string();
    const std::string consumerExecutable = (consumer / "PromotionConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArguments = {
        "-O0", "--emit-project-oop-exe", consumerProject, "-o", consumerExecutable};
    output.clear();
    error.clear();
    const bool consumerEmitted = runDriver(consumerArguments, output, error) == ExitCode::Success;
    tests.expect(consumerEmitted,
        "LibraryReference reconstructs recursive promoted aliases and physical offsets (" + error + ")");
    if (consumerEmitted)
    {
        tests.expect(std::system(("\"" + consumerExecutable + "\"").c_str()) == 0,
            "cross-module promoted fields execute through the producer ABI");
    }

    expectRejected(tests, root, "DirectConflict", R"NS(
struct Bad
{
    public int Value;
    public struct { public int Value; };
}
)NS", "promoted anonymous member name 'Value' conflicts");

    expectRejected(tests, root, "PromotionConflict", R"NS(
struct Bad
{
    public struct { public int Value; };
    public union { public int Value; public float Other; };
}
)NS", "promoted anonymous member name 'Value' conflicts");

    expectRejected(tests, root, "NonFieldPromotion", R"NS(
struct Bad
{
    public struct { public int Read() { return 42; } };
}
)NS", "can promote instance fields only");

    expectRejected(tests, root, "PrivatePromotion", R"NS(
public struct Secret
{
    private struct { public int Hidden; };
}
class Program
{
    public static int Main() { Secret value; return value.Hidden; }
}
)NS", "member 'Hidden' is not accessible");

    expectRejected(tests, root, "NamedCarrierDoesNotPromote", R"NS(
public struct Named
{
    public struct { public int Value; } Carrier;
}
class Program
{
    public static int Main() { Named value; return value.Value; }
}
)NS", "has no compatible member named 'Value'");

    expectRejected(tests, root, "StaticUnnamedAggregate", R"NS(
struct Bad
{
    public static struct { public int Value; };
}
)NS", "accepts accessibility, at, pack, align and size only");

    return tests.resultKeeping(root);
}
