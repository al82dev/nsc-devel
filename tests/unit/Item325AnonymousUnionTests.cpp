#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item325_anonymous_union_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto valid = root / "valid";
    writeProject(valid, "AnonymousUnionItem325", R"NS(
public struct Packet
{
    public byte Tag;
    public union
    {
        public int AsInt;
        public float AsFloat;
        public byte[4] Bytes;
    } Payload;
    public byte Tail;
}

public class Program
{
    public static int Main()
    {
        Packet packet;
        packet.Tag = 1;
        packet.Payload.AsInt = 42;
        packet.Tail = 0;
        return packet.Payload.AsInt == 42 && packet.Payload.Bytes[0] == 42 ? 0 : 1;
    }
}
)NS");

    const std::string project = (valid / "AnonymousUnionItem325.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 2> memberArguments = {"--dump-project-members", project};
    const bool members = runDriver(memberArguments, output, error) == ExitCode::Success;
    tests.expect(members, "anonymous union reaches the project member model (" + error + ")");
    if (members)
    {
        tests.expect(output.find("AnonymousUnionItem325::Packet+__nsc_anonymous_union_f") !=
                         std::string::npos,
            "the anonymous union receives a deterministic nested type identity");
        tests.expect(output.find("field Payload type=type:AnonymousUnionItem325::Packet+__nsc_anonymous_union_f") !=
                         std::string::npos,
            "the containing struct receives an ordinary union carrier field");
        tests.expect(output.find("field AsInt type=builtin:int") != std::string::npos &&
                         output.find("field AsFloat type=builtin:float") != std::string::npos &&
                         output.find("field Bytes type=builtin:byte[4]") != std::string::npos,
            "members inside the anonymous union belong to its synthesized type");
    }

    const std::array<std::string_view, 2> layoutArguments = {"--dump-project-layouts", project};
    output.clear();
    error.clear();
    const bool layouts = runDriver(layoutArguments, output, error) == ExitCode::Success;
    tests.expect(layouts, "anonymous union computes Windows x64 layouts (" + error + ")");
    if (layouts)
    {
        tests.expect(output.find("Packet+__nsc_anonymous_union_f") != std::string::npos &&
                         output.find("size=4 align=4 representation=inline-value") != std::string::npos,
            "the synthesized union overlaps int, float and byte-array alternatives");
        tests.expect(output.find("name=AsInt type=builtin:int offset=0") != std::string::npos &&
                         output.find("name=AsFloat type=builtin:float offset=0") != std::string::npos &&
                         output.find("name=Bytes type=builtin:byte[4] offset=0") !=
                             std::string::npos,
            "every anonymous-union alternative starts at offset zero");
        tests.expect(output.find("struct type:AnonymousUnionItem325::Packet size=12 align=4") !=
                         std::string::npos,
            "the union carrier participates inline in the containing struct layout");
        tests.expect(output.find("name=Payload type=type:AnonymousUnionItem325::Packet+__nsc_anonymous_union_f") !=
                         std::string::npos && output.find("offset=4 size=4 align=4") != std::string::npos,
            "the carrier retains its own field identity, offset, size and alignment");
    }

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    output.clear();
    error.clear();
    const bool llvm = runDriver(llvmArguments, output, error) == ExitCode::Success;
    tests.expect(llvm, "anonymous union field access lowers to verifier-clean O0 LLVM (" + error + ")");
    if (llvm)
    {
        tests.expect(output.find("getelementptr i8, ptr") != std::string::npos,
            "anonymous-union alternatives use ordinary byte-offset address lowering");
        tests.expect(output.find("%local0.addr = alloca [12 x i8], align 4") != std::string::npos,
            "LLVM materializes the complete containing value with its inline union payload");
    }

    const std::string executable = (valid / "AnonymousUnion.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "anonymous union fixture links at O0 (" + error + ")");
    if (emitted)
    {
        tests.expect(std::system(("\"" + executable + "\"").c_str()) == 0,
            "overlapping anonymous-union stores and loads execute with result 42");
    }

    const auto nested = root / "nested";
    writeProject(nested, "NestedAnonymousUnionItem325", R"NS(
struct Envelope
{
    public struct
    {
        public union { public int Value; public byte[4] Bytes; } Bits;
        public short Tail;
    } Payload;
}
class Program
{
    public static int Main()
    {
        Envelope value;
        value.Payload.Bits.Value = 42;
        value.Payload.Tail = 0;
        return value.Payload.Bits.Bytes[0] == 42 ? 0 : 1;
    }
}
)NS");
    const std::string nestedProject = (nested / "NestedAnonymousUnionItem325.nsproj").string();
    const std::array<std::string_view, 3> nestedArguments = {
        "--dump-project-oop-llvm", nestedProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(runDriver(nestedArguments, output, error) == ExitCode::Success,
        "anonymous unions nest inside anonymous structs without flattening ownership (" + error + ")");

    const auto packed = root / "packed";
    writeProject(packed, "PackedAnonymousUnionItem325", R"NS(
struct Wire
{
    public byte Lead;
    public at(8) pack(1) union { public short Code; public int Count; } Payload;
    public byte Tail;
}
)NS");
    const std::string packedProject = (packed / "PackedAnonymousUnionItem325.nsproj").string();
    const std::array<std::string_view, 2> packedArguments = {
        "--dump-project-layouts", packedProject};
    output.clear();
    error.clear();
    const bool packedOk = runDriver(packedArguments, output, error) == ExitCode::Success;
    tests.expect(packedOk, "layout modifiers apply to the anonymous union body (" + error + ")");
    if (packedOk)
    {
        tests.expect(output.find("size=4 align=1 representation=inline-value") != std::string::npos &&
                         output.find("struct type:PackedAnonymousUnionItem325::Wire size=13 align=1") !=
                             std::string::npos,
            "pack(1) fixes the anonymous union and containing wire layout");
        tests.expect(output.find("name=Payload type=type:PackedAnonymousUnionItem325::Wire+__nsc_anonymous_union_f") !=
                         std::string::npos && output.find("offset=8 size=4 align=1") != std::string::npos,
            "at(8) applies to the carrier while pack(1) remains on the anonymous union");
    }

    const auto partial = root / "partial-files";
    writeProject(partial, "PartialAnonymousUnionItem325", "");
    writeFile(partial / "A.ns", R"NS(
public partial class Host
{
    public union { public int A; public float AF; } Left;
}
)NS");
    writeFile(partial / "B.ns", R"NS(
public partial class Host
{
    public union { public int B; public float BF; } Right;
}
)NS");
    const std::string partialProject = (partial / "PartialAnonymousUnionItem325.nsproj").string();
    const std::array<std::string_view, 2> partialArguments = {
        "--dump-project-layouts", partialProject};
    output.clear();
    error.clear();
    const bool partialOk = runDriver(partialArguments, output, error) == ExitCode::Success;
    tests.expect(partialOk, "same-offset anonymous unions in separate partial files do not collide (" + error + ")");
    if (partialOk)
    {
        const std::size_t first = output.find("Host+__nsc_anonymous_union_f");
        const std::size_t second = first == std::string::npos
            ? std::string::npos
            : output.find("Host+__nsc_anonymous_union_f", first + 1);
        tests.expect(first != std::string::npos && second != std::string::npos,
            "file identity plus source offset gives each anonymous union a distinct nested type");
    }

    return tests.resultKeeping(root);
}
