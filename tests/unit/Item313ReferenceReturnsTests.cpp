#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/HistoricalMetadataProjection.h"
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

void writeProject(const std::filesystem::path& root, std::string_view program)
{
    writeFile(root / "ReferenceReturns.nsproj",
        "<Project><PropertyGroup><Name>ReferenceReturnsItem313</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", program);
}

void expectReject(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view program,
    std::string_view diagnostic)
{
    const auto caseRoot = root / std::string(name);
    writeProject(caseRoot, program);
    const std::string project = (caseRoot / "ReferenceReturns.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
        std::string(name) + " is rejected");
    tests.expect(error.find(diagnostic) != std::string::npos,
        std::string(name) + " reports the reference-return contract (" + error + ")");
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item313_reference_returns_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto validRoot = root / "valid";
    writeProject(validRoot, R"NS(
public interface IHolder
{
    ref int Slot();
}

public class Holder : IHolder
{
    public int Value;

    public virtual ref int Slot()
    {
        return ref Value;
    }
}

public class DerivedHolder : Holder
{
    public int DerivedValue;

    public override ref int Slot()
    {
        return ref DerivedValue;
    }
}

public class Program
{
    private static int Global;

    private static ref int Forward(ref int value)
    {
        return ref value;
    }

    private static ref int GlobalSlot()
    {
        return ref Global;
    }

    public static int Main()
    {
        int value = 10;
        Forward(ref value) = 20;
        if (value != 20) { return 1; }

        ref int alias = ref Forward(ref value);
        alias += 2;
        if (Forward(ref value) != 22) { return 2; }

        GlobalSlot() = 30;
        if (Global != 30) { return 3; }

        Holder holder = new Holder();
        holder.Value = 40;
        holder.Slot() = 42;
        if (holder.Value != 42) { return 4; }

        Holder virtualHolder = new DerivedHolder();
        virtualHolder.Slot() = 50;
        if (((DerivedHolder)virtualHolder).DerivedValue != 50) { return 5; }

        IHolder interfaceHolder = virtualHolder;
        interfaceHolder.Slot() = 60;
        return ((DerivedHolder)virtualHolder).DerivedValue == 60 ? 0 : 6;
    }
}
)NS");

    const std::string project = (validRoot / "ReferenceReturns.nsproj").string();
    std::string output;
    std::string error;

    const std::array<std::string_view, 3> bodiesArguments = {
        "--dump-project-method-bodies", project, "-O0"};
    const bool bodies = runDriver(bodiesArguments, output, error) == ExitCode::Success;
    tests.expect(bodies, "reference returns pass semantic analysis (" + error + ")");
    if (bodies)
    {
        tests.expect(output.find("name=alias type=builtin:int ref=true") != std::string::npos,
            "a ref-return call can initialize a reference local");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", project, "-O0"};
    output.clear();
    error.clear();
    const bool hir = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hir, "reference returns lower to O0 HIR (" + error + ")");
    if (hir)
    {
        tests.expect(output.find("ref-return=true") != std::string::npos &&
                output.find("reference-result=true") != std::string::npos,
            "HIR preserves function and call reference identity");
        tests.expect(output.find("pointer-load type=builtin:int") != std::string::npos,
            "value use of a ref-return call loads through the returned address");
    }

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", project, "-O0"};
    output.clear();
    error.clear();
    const bool mir = runDriver(mirArguments, output, error) == ExitCode::Success;
    tests.expect(mir, "reference returns lower to O0 MIR (" + error + ")");
    if (mir)
    {
        tests.expect(output.find("ref-return=true") != std::string::npos &&
                output.find("reference-result=true") != std::string::npos,
            "MIR transports the reference-return ABI facts");
    }

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    output.clear();
    error.clear();
    const bool llvm = runDriver(llvmArguments, output, error) == ExitCode::Success;
    tests.expect(llvm, "reference returns emit valid O0 LLVM (" + error + ")");
    if (llvm)
    {
        writeFile(validRoot / "ReferenceReturns.ll", output);
        tests.expect(output.find("define ptr") != std::string::npos &&
                output.find("%pointer.load = load i32, ptr") != std::string::npos,
            "LLVM returns an address and loads it only in value context");
    }

    const std::string executable = (validRoot / "ReferenceReturns.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "reference-return fixture links at O0 (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "ref-return calls mutate parameters, fields, and virtual/interface targets");
    }

    expectReject(tests, root, "stack_local", R"NS(
public class Program
{
    private static ref int Bad() { int value = 1; return ref value; }
    public static int Main() { return 0; }
}
)NS", "cannot expose storage owned by the current stack frame");

    expectReject(tests, root, "literal", R"NS(
public class Program
{
    private static ref int Bad() { return ref 1; }
    public static int Main() { return 0; }
}
)NS", "requires a mutable addressable storage location");

    expectReject(tests, root, "mismatch", R"NS(
public class Program
{
    private static long Value;
    private static ref int Bad() { return ref Value; }
    public static int Main() { return 0; }
}
)NS", "requires exact type identity");

    expectReject(tests, root, "missing_return_ref", R"NS(
public class Program
{
    private static int Value;
    private static ref int Bad() { return Value; }
    public static int Main() { return 0; }
}
)NS", "must return an alias with 'return ref expression;'");

    expectReject(tests, root, "ordinary_return_ref", R"NS(
public class Program
{
    private static int Value;
    private static int Bad() { return ref Value; }
    public static int Main() { return 0; }
}
)NS", "return ref is valid only inside a ref-returning method");

    expectReject(tests, root, "value_parameter", R"NS(
public class Program
{
    private static ref int Bad(int value) { return ref value; }
    public static int Main() { return 0; }
}
)NS", "cannot expose a by-value or readonly parameter");

    expectReject(tests, root, "override_mode_mismatch", R"NS(
public class Base
{
    protected int Value;
    public virtual ref int Slot() { return ref Value; }
}
public class Derived : Base
{
    public override int Slot() { return 1; }
}
public class Program { public static int Main() { return 0; } }
)NS", "preserve the exact return type and ref-return mode");

    const auto producer = root / "producer";
    writeFile(producer / "RefProducer.nsproj",
        "<Project><PropertyGroup><Name>RefProducer</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Slots.ns", R"NS(
namespace CrossRef
{
    public class Slots
    {
        public static int Value;
        public static ref int Current() { return ref Value; }
    }
}
)NS");
    const std::string producerProject = (producer / "RefProducer.nsproj").string();
    const std::string producerLibrary = (producer / "RefProducer.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    output.clear();
    error.clear();
    tests.expect(runDriver(producerArguments, output, error) == ExitCode::Success,
        "public ref-return producer emits a static library and NSMETA 1.15 (" + error + ")");

    nativesharp::interop::LibraryMetadataCodec codec;
    nativesharp::interop::LibraryMetadata metadata;
    std::string metadataError;
    const auto metadataPath = producer / "RefProducer.nsmeta";
    const bool metadataRead = codec.read(metadataPath, metadata, metadataError);
    bool metadataReferenceReturn = false;
    if (metadataRead)
    {
        for (const auto& member : metadata.members)
        {
            if (member.name == "Current" &&
                (member.flags & nativesharp::interop::kMetadataMemberReferenceReturn) != 0U)
                metadataReferenceReturn = true;
        }
    }
    tests.expect(metadataRead &&
                     metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor &&
                     metadataReferenceReturn,
        "current NSMETA preserves the public ref-return bit (" + metadataError + ")");
    if (metadataRead)
    {
        metadata.formatMinor = 14U;
        nativesharp::tests::projectHistoricalFixtureMetadata(metadata, 14U);
        const auto downlevelPath = producer / "RefProducer-1.14.nsmeta";
        tests.expect(codec.writeModel(metadata, downlevelPath, metadataError),
            "the historical NSMETA 1.14 form can still be emitted (" + metadataError + ")");
        nativesharp::interop::LibraryMetadata downlevel;
        const bool downlevelRead = codec.read(downlevelPath, downlevel, metadataError);
        bool retainedReferenceReturn = false;
        if (downlevelRead)
        {
            for (const auto& member : downlevel.members)
                retainedReferenceReturn = retainedReferenceReturn ||
                    (member.flags & nativesharp::interop::kMetadataMemberReferenceReturn) != 0U;
        }
        tests.expect(downlevelRead && !retainedReferenceReturn,
            "NSMETA 1.14 output strips the 1.15 ref-return bit rather than lying about ABI");
    }

    const auto consumer = root / "consumer";
    writeFile(consumer / "RefConsumer.nsproj",
        "<Project><PropertyGroup><Name>RefConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (producer / "RefProducer.lib").generic_string() +
        "\" Metadata=\"" + metadataPath.generic_string() + "\" /></ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using CrossRef;
public class Program
{
    public static int Main()
    {
        Slots.Current() = 77;
        return Slots.Value == 77 ? 0 : 1;
    }
}
)NS");
    const std::string consumerProject = (consumer / "RefConsumer.nsproj").string();
    const std::string consumerExecutable = (consumer / "RefConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArguments = {
        "-O0", "--emit-project-oop-exe", consumerProject, "-o", consumerExecutable};
    output.clear();
    error.clear();
    const bool consumerEmitted = runDriver(consumerArguments, output, error) == ExitCode::Success;
    tests.expect(consumerEmitted,
        "LibraryReference reconstructs and links the ref-return signature (" + error + ")");
    if (consumerEmitted)
    {
        const std::string command = "\"" + consumerExecutable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "a consumer mutates producer storage through a precompiled ref-return call");
    }

    return tests.resultKeeping(root);
}
