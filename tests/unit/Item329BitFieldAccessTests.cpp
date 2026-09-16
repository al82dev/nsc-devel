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

void writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view outputType = {})
{
    writeFile(root / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) + "</Name>" +
        (outputType.empty() ? "" : "<OutputType>" + std::string(outputType) + "</OutputType>") +
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "</ItemGroup></Project>");
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
    writeProject(projectRoot, name, source, "StaticLibrary");
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
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item329_bitfield_access_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto runtime = root / "runtime";
    writeProject(runtime, "BitFieldAccessItem329", R"NS(
public enum Small : byte { Zero = 0, Five = 5 }
public enum SignedSmall : sbyte { MinusOne = -1, Zero = 0 }

public struct Bits
{
    public uint A : 3;
    public uint B : 5;
    public int Signed : 4;
    public byte Tiny : 3;
    public bool Flag : 1;
    public Small Mode : 3;
    public SignedSmall SignedMode : 3;
}

public struct WideBits
{
    public ulong Low : 63;
    public ulong High : 1;
    public long Full : 64;
}

public pack(1) struct PackedBits
{
    public byte Lead;
    public uint Value : 5;
}

public union UnionBits
{
    public uint Raw;
    public uint Low : 3;
}

public struct PromotedBits
{
    public struct
    {
        public uint Low : 3;
        public uint High : 5;
    };
}

public struct UnnamedPadding
{
    public uint Left : 3;
    public uint : 2;
    public uint Right : 3;
}

public class Box
{
    public uint Value : 6;
    public int Signed : 5;
}

public class Program
{
    private static int Calls;

    private static Bits* Touch(Bits* value)
    {
        Calls++;
        return value;
    }

    public static int Main()
    {
        Bits bits = {};
        bits.B = 17u;
        uint assigned = (Touch(&bits)->A = 13u);
        if (assigned != 5u || bits.A != 5u || bits.B != 17u || Calls != 1) { return 1; }

        uint compound = (Touch(&bits)->A += 3u);
        if (compound != 0u || bits.A != 0u || bits.B != 17u || Calls != 2) { return 2; }
        uint old = Touch(&bits)->A++;
        uint current = ++Touch(&bits)->A;
        if (old != 0u || current != 2u || bits.A != 2u || Calls != 4) { return 3; }

        bits.Signed = 7;
        int wrapped = ++bits.Signed;
        if (wrapped != -8 || bits.Signed != -8) { return 4; }
        bits.Signed = -1;
        if (bits.Signed != -1) { return 5; }
        bits.Tiny = 7;
        bits.Flag = true;
        bits.Mode = Small.Five;
        bits.SignedMode = SignedSmall.MinusOne;
        if (bits.Tiny != 7 || !bits.Flag || bits.Mode != Small.Five ||
            bits.SignedMode != SignedSmall.MinusOne) { return 6; }

        Bits initialized = { 3u, 9u, -2, 6, true, Small.Five, SignedSmall.MinusOne };
        if (initialized.A != 3u || initialized.B != 9u || initialized.Signed != -2 ||
            initialized.Tiny != 6 || !initialized.Flag || initialized.Mode != Small.Five ||
            initialized.SignedMode != SignedSmall.MinusOne)
        { return 11; }
        UnnamedPadding padded = { 5u, 6u };
        if (padded.Left != 5u || padded.Right != 6u) { return 12; }

        WideBits wide = {};
        wide.Low = 9UL;
        wide.High = 1UL;
        wide.Full = -1L;
        if (wide.Low != 9UL || wide.High != 1UL || wide.Full != -1L) { return 13; }

        PackedBits packed = {};
        packed.Lead = 42;
        packed.Value = 31u;
        if (packed.Lead != 42 || packed.Value != 31u) { return 7; }

        UnionBits overlay = {};
        overlay.Raw = 0u;
        overlay.Low = 5u;
        if (overlay.Raw != 5u || overlay.Low != 5u) { return 8; }

        PromotedBits promoted = {};
        promoted.High = 19u;
        promoted.Low = 6u;
        if (promoted.Low != 6u || promoted.High != 19u) { return 9; }

        Box box = new Box();
        box.Value = 41u;
        box.Signed = -7;
        if (box.Value != 41u || box.Signed != -7) { return 10; }
        return 0;
    }
}
)NS");

    const std::string project = (runtime / "BitFieldAccessItem329.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", project, "-O0"};
    const bool mir = runDriver(mirArguments, output, error) == ExitCode::Success;
    tests.expect(mir, "bit-field reads and writes reach OOP MIR (" + error + ")");
    tests.expect(mir && output.find("schema=1.8") != std::string::npos &&
                     output.find("bit-field-load type=builtin:uint") != std::string::npos &&
                     output.find("bit-field-address type=builtin:uint") != std::string::npos &&
                     output.find("storage=4 bit-offset=0 bit-width=3") != std::string::npos,
        "MIR 1.7 carries logical bit-field lvalues and exact allocation-unit coordinates");

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    output.clear();
    error.clear();
    const bool llvm = runDriver(llvmArguments, output, error) == ExitCode::Success;
    tests.expect(llvm, "bit-field MIR lowers to verifier-clean LLVM (" + error + ")");
    tests.expect(llvm && output.find("bitfield.storage.load") != std::string::npos &&
                     output.find("bitfield.storage.merge") != std::string::npos &&
                     output.find("bitfield.sign.extend") != std::string::npos &&
                     output.find("bitfield.bool") != std::string::npos,
        "LLVM uses masked unit loads, read-modify-write stores, signed extension and bool normalization");
    tests.expect(llvm && output.find("load volatile") == std::string::npos &&
                     output.find("store volatile") == std::string::npos,
        "item 329 does not silently assign item-330 volatile semantics");

    for (const std::string_view optimization : {std::string_view{"-O0"}, std::string_view{"-O2"}})
    {
        const std::string suffix = optimization == "-O0" ? "O0" : "O2";
        const std::string executable = (runtime / ("BitFieldAccess-" + suffix + ".exe")).string();
        const std::array<std::string_view, 5> arguments = {
            optimization, "--emit-project-oop-exe", project, "-o", executable};
        output.clear();
        error.clear();
        const bool emitted = runDriver(arguments, output, error) == ExitCode::Success;
        tests.expect(emitted, "bit-field fixture links at " + suffix + " (" + error + ")");
        if (emitted)
        {
            tests.expect(std::system(("\"" + executable + "\"").c_str()) == 0,
                "bit-field reads, writes, truncation and single evaluation execute at " + suffix);
        }
    }

    expectRejected(tests, root, "AddressOfBitField", R"NS(
public struct Bits { public uint Value : 3; }
public class Api { public static uint* Address(Bits* value) { return &value->Value; } }
)NS", "bit fields have no address");

    const auto addressableArray = root / "addressable-array-element";
    writeProject(addressableArray, "AddressableArrayElement", R"NS(
public class Program
{
    public static int Main()
    {
        byte[] values = new byte[1];
        values[0] = 42;
        byte* first = &values[0];
        int result = first[0] == 42 ? 0 : 1;
        delete[] values;
        return result;
    }
}
)NS");
    const std::string addressableArrayProject =
        (addressableArray / "AddressableArrayElement.nsproj").string();
    const std::array<std::string_view, 2> addressableArrayArguments = {
        "--check-project-oop-hir", addressableArrayProject};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(addressableArrayArguments, output, error) == ExitCode::Success,
        "native array elements remain addressable after the bit-field guard (" + error + ")");

    const auto producer = root / "producer";
    writeProject(producer, "BitFieldProducer", R"NS(
namespace Shared
{
    public struct Packet
    {
        public uint Code : 6;
        public int Delta : 5;
    }
    public class Api
    {
        public static export extern C int Read(Packet value) { return (int)value.Code + value.Delta; }
    }
}
)NS", "StaticLibrary");
    const std::string producerProject = (producer / "BitFieldProducer.nsproj").string();
    const std::string producerLibrary = (producer / "BitFieldProducer.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    output.clear();
    error.clear();
    const bool producerEmitted = runDriver(producerArguments, output, error) == ExitCode::Success;
    tests.expect(producerEmitted,
        "bit-field producer emits its library and NSMETA 1.18 (" + error + ")");

    const auto consumer = root / "consumer";
    writeFile(consumer / "BitFieldConsumer.nsproj",
        "<Project><PropertyGroup><Name>BitFieldConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + producerLibrary + "\" Metadata=\"" +
        (producer / "BitFieldProducer.nsmeta").string() + "\" /></ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using Shared;
public class Program
{
    public static int Main()
    {
        Packet packet = {};
        packet.Code = 63u;
        packet.Delta = -9;
        return packet.Code == 63u && packet.Delta == -9 ? 0 : 1;
    }
}
)NS");
    const std::string consumerProject = (consumer / "BitFieldConsumer.nsproj").string();
    const std::string consumerExecutable = (consumer / "BitFieldConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArguments = {
        "-O0", "--emit-project-oop-exe", consumerProject, "-o", consumerExecutable};
    output.clear();
    error.clear();
    const bool consumerEmitted = producerEmitted &&
        runDriver(consumerArguments, output, error) == ExitCode::Success;
    tests.expect(consumerEmitted,
        "LibraryReference reconstructs executable bit-field access (" + error + ")");
    if (consumerEmitted)
    {
        tests.expect(std::system(("\"" + consumerExecutable + "\"").c_str()) == 0,
            "cross-module bit-field coordinates execute correctly");
    }

    return tests.resultKeeping(root);
}
