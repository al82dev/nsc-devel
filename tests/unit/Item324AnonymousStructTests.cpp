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
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item324_anonymous_struct_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto valid = root / "valid";
    writeProject(valid, "AnonymousStructItem324", R"NS(
public struct Packet
{
    public byte Tag;
    public struct
    {
        public short Low;
        public int High;
    } Payload;
    public byte Tail;
}

public class Program
{
    public static int Main()
    {
        Packet packet;
        packet.Tag = 1;
        packet.Payload.Low = 2;
        packet.Payload.High = 39;
        packet.Tail = 0;
        return packet.Tag + packet.Payload.Low + packet.Payload.High + packet.Tail == 42 ? 0 : 1;
    }
}
)NS");

    const std::string project = (valid / "AnonymousStructItem324.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 2> memberArguments = {"--dump-project-members", project};
    const bool members = runDriver(memberArguments, output, error) == ExitCode::Success;
    tests.expect(members, "anonymous struct reaches the project member model (" + error + ")");
    if (members)
    {
        tests.expect(output.find("AnonymousStructItem324::Packet+__nsc_anonymous_struct_") !=
                         std::string::npos,
            "the anonymous body receives a deterministic nested type identity");
        tests.expect(output.find("field Payload type=type:AnonymousStructItem324::Packet+__nsc_anonymous_struct_") !=
                         std::string::npos,
            "the containing struct receives one ordinary field of the synthesized value type");
        tests.expect(output.find("field Low type=builtin:short") != std::string::npos &&
                         output.find("field High type=builtin:int") != std::string::npos,
            "members inside the anonymous body belong to the synthesized struct");
    }

    const std::array<std::string_view, 2> layoutArguments = {"--dump-project-layouts", project};
    output.clear();
    error.clear();
    const bool layouts = runDriver(layoutArguments, output, error) == ExitCode::Success;
    tests.expect(layouts, "anonymous struct computes Windows x64 layouts (" + error + ")");
    if (layouts)
    {
        tests.expect(output.find("Packet+__nsc_anonymous_struct_") != std::string::npos &&
                         output.find("size=8 align=4 representation=inline-value") != std::string::npos,
            "the synthesized struct has the natural short/int layout");
        tests.expect(output.find("struct type:AnonymousStructItem324::Packet size=16 align=4") !=
                         std::string::npos,
            "the carrier participates inline in its containing struct layout");
        tests.expect(output.find("name=Payload type=type:AnonymousStructItem324::Packet+__nsc_anonymous_struct_") !=
                         std::string::npos && output.find("offset=4 size=8 align=4") != std::string::npos,
            "the named carrier retains its field identity, offset, size and alignment");
    }

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    output.clear();
    error.clear();
    const bool llvm = runDriver(llvmArguments, output, error) == ExitCode::Success;
    tests.expect(llvm, "anonymous struct field access lowers to verifier-clean O0 LLVM (" + error + ")");
    if (llvm)
    {
        tests.expect(output.find("getelementptr i8, ptr") != std::string::npos,
            "nested anonymous-field access uses ordinary byte-offset address lowering");
        tests.expect(output.find("%local0.addr = alloca [16 x i8], align 4") != std::string::npos,
            "LLVM materializes the complete containing value with its anonymous inline payload");
    }

    const std::string executable = (valid / "AnonymousStruct.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "anonymous struct fixture links at O0 (" + error + ")");
    if (emitted)
    {
        tests.expect(std::system(("\"" + executable + "\"").c_str()) == 0,
            "nested anonymous-struct field stores and loads execute with result 42");
    }

    const auto nested = root / "nested";
    writeProject(nested, "NestedAnonymousItem324", R"NS(
struct Envelope
{
    public struct
    {
        public struct { public int Value; } Inner;
        public short Tail;
    } Payload;
}
class Program
{
    public static int Main()
    {
        Envelope value;
        value.Payload.Inner.Value = 42;
        value.Payload.Tail = 0;
        return value.Payload.Inner.Value == 42 ? 0 : 1;
    }
}
)NS");
    const std::string nestedProject = (nested / "NestedAnonymousItem324.nsproj").string();
    const std::array<std::string_view, 3> nestedArguments = {
        "--dump-project-oop-llvm", nestedProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(runDriver(nestedArguments, output, error) == ExitCode::Success,
        "anonymous structs nest recursively without flattening member ownership (" + error + ")");

    const auto packed = root / "packed";
    writeProject(packed, "PackedAnonymousItem324", R"NS(
struct Wire
{
    public byte Lead;
    public at(4) pack(1) struct { public short Code; public int Count; } Payload;
    public byte Tail;
}
)NS");
    const std::string packedProject = (packed / "PackedAnonymousItem324.nsproj").string();
    const std::array<std::string_view, 2> packedArguments = {
        "--dump-project-layouts", packedProject};
    output.clear();
    error.clear();
    const bool packedOk = runDriver(packedArguments, output, error) == ExitCode::Success;
    tests.expect(packedOk, "layout modifiers apply to the anonymous body (" + error + ")");
    if (packedOk)
    {
        tests.expect(output.find("size=6 align=1 representation=inline-value") != std::string::npos &&
                         output.find("struct type:PackedAnonymousItem324::Wire size=11 align=1") !=
                             std::string::npos,
            "pack(1) fixes both anonymous-body and containing wire layout");
        tests.expect(output.find("name=Payload type=type:PackedAnonymousItem324::Wire+__nsc_anonymous_struct_") !=
                         std::string::npos && output.find("offset=4 size=6 align=1") != std::string::npos,
            "at(4) applies to the carrier field while pack(1) remains on the anonymous body");
    }

    const auto partial = root / "partial-files";
    writeProject(partial, "PartialAnonymousItem324", "");
    writeFile(partial / "A.ns", R"NS(
public partial class Host
{
    public struct { public int A; } Left;
}
)NS");
    writeFile(partial / "B.ns", R"NS(
public partial class Host
{
    public struct { public int B; } Right;
}
)NS");
    const std::string partialProject = (partial / "PartialAnonymousItem324.nsproj").string();
    const std::array<std::string_view, 2> partialArguments = {
        "--dump-project-layouts", partialProject};
    output.clear();
    error.clear();
    const bool partialOk = runDriver(partialArguments, output, error) == ExitCode::Success;
    tests.expect(partialOk, "same-offset anonymous structs in separate partial files do not collide (" + error + ")");
    if (partialOk)
    {
        const std::size_t first = output.find("Host+__nsc_anonymous_struct_f");
        const std::size_t second = first == std::string::npos
            ? std::string::npos
            : output.find("Host+__nsc_anonymous_struct_f", first + 1);
        tests.expect(first != std::string::npos && second != std::string::npos,
            "file identity plus source offset gives each partial-file body a distinct nested type");
    }

    return tests.resultKeeping(root);
}
