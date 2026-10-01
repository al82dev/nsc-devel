#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

bool hasDiagnostic(const ProjectCompilation& compilation, std::string_view text)
{
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic) { return diagnostic.message().find(text) != std::string::npos; });
}

std::string diagnosticText(const ProjectCompilation& compilation)
{
    std::string result;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (!result.empty()) result += " | ";
        result += diagnostic.message();
    }
    return result;
}

void expectRejected(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view diagnostic)
{
    const std::filesystem::path directory = root / std::string(name);
    writeFile(directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>StaticLibrary</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    ProjectCompilation compilation;
    std::string error;
    const bool accepted = compilation.compileProjectMethodBodies(
        directory / (std::string(name) + ".nsproj"), error);
    tests.expect(!accepted && hasDiagnostic(compilation, diagnostic),
        std::string(name) + " is rejected for the intended lifetime reason (" + error +
        "; diagnostics: " + diagnosticText(compilation) + ")");
}

constexpr std::string_view kBorrowedPrelude = R"NS(
public class Resource
{
    public int Value;
    public Resource(int value) { Value = value; }
}

public borrowed struct BorrowedReference<T>
{
    private T target;
    public BorrowedReference(T value) { target = value; }
    public T Get() { return target; }
}
)NS";
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t31_array_borrow";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path producer = root / "producer";
    writeFile(producer / "JaggedProducer.nsproj",
        "<Project><PropertyGroup><Name>JaggedProducer</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(producer / "Api.ns", R"NS(
namespace JaggedApi
{
    public class Arrays
    {
        public static int Sum(int[][] values)
        {
            return values[0][0] + values[0][1];
        }
    }
}
)NS");
    const std::string producerProject = (producer / "JaggedProducer.nsproj").string();
    const std::string producerLibrary = (producer / "JaggedProducer.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    std::string output;
    std::string error;
    ProjectCompilation producerProbe;
    std::string producerProbeError;
    const bool producerSemantics = producerProbe.compileProjectMethodBodies(
        producer / "JaggedProducer.nsproj", producerProbeError);
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    tests.expect(producerResult == ExitCode::Success,
        "T31 producer emits jagged-array NSMETA and library at O0 (exit=" +
        std::to_string(static_cast<int>(producerResult)) + "; semantic=" +
        (producerSemantics ? "ok" : "failed") + "; " + producerProbeError + "; diagnostics: " +
        diagnosticText(producerProbe) + "; " + error + output + ")");

    nativesharp::interop::LibraryMetadata metadata;
    nativesharp::interop::LibraryMetadataCodec codec;
    error.clear();
    const bool metadataRead = codec.read(producer / "JaggedProducer.nsmeta", metadata, error);
    const auto sum = std::ranges::find_if(metadata.members, [](const auto& member) {
        return member.name == "Sum";
    });
    tests.expect(metadataRead && sum != metadata.members.end() && sum->parameters.size() == 1 &&
            sum->parameters[0].typeKey == "builtin:int[][]",
        "NSMETA preserves the exact nested native-array parameter key across modules (" + error + ")");

    const std::filesystem::path consumer = root / "consumer";
    writeFile(consumer / "JaggedConsumer.nsproj",
        "<Project><PropertyGroup><Name>JaggedConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (producer / "JaggedProducer.lib").generic_string() +
        "\" Metadata=\"" + (producer / "JaggedProducer.nsmeta").generic_string() +
        "\"/></ItemGroup></Project>");
    writeFile(consumer / "BorrowedReference.ns",
        readFile(std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/BorrowedReference.ns"));
    writeFile(consumer / "Span.ns",
        readFile(std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/Span.ns"));
    writeFile(consumer / "Support.ns", R"NS(
namespace NativeSharp.Core
{
    public class Exception { }
    public class ArgumentOutOfRangeException : Exception
    {
        public ArgumentOutOfRangeException() { }
    }
}
)NS");
    writeFile(consumer / "Main.ns", R"NS(
using JaggedApi;
using NativeSharp.Core;

public class Payload
{
    public static int Drops = 0;
    public int Value;
    public Payload(int value) { Value = value; }
    ~Payload() { Drops = Drops + 1; }
}

public class Program
{
    public static int Main()
    {
        int[4] inlineValues;
        if (inlineValues.Length != 4) { return 1; }
        Span<int> inlineView = Spans.OfRaw<int>(&inlineValues[0], inlineValues.Length);
        inlineView[0] = 42;
        if (inlineValues[0] != 42) { return 9; }

        int[][] rows = new int[2][];
        rows[0] = new int[] { 20, 22 };
        rows[1] = new int[] { };
        if (Arrays.Sum(rows) != 42) { return 2; }
        delete[] rows[0];
        delete[] rows;

        int[][] alternate = new int[][1];
        alternate[0] = new int[] { 42 };
        if (alternate[0][0] != 42) { return 3; }
        delete[] alternate;

        int[][] inferred = new int[][] { new int[] { 42 } };
        if (inferred.Length != 1 || inferred[0][0] != 42) { return 4; }
        delete[] inferred;

        Payload[] owners = new Payload[] { new Payload(1), new Payload(2) };
        delete[] owners;
        if (Payload.Drops != 2) { return 5; }

        {
            owned Payload owner = new Payload(40);
            {
                BorrowedReference<Payload> reference = Borrows.Of<Payload>(owner);
                if (reference.Get().Value != 40) { return 6; }
            }
        }
        if (Payload.Drops != 3) { return 7; }

        int[] flat = new int[] { 20, 22 };
        {
            Span<int> view = Spans.Of<int>(flat);
            ReadOnlySpan<int> readOnly = view.AsReadOnly();
            if (view.Length() != 2 || readOnly[0] + readOnly[1] != 42) { return 8; }
        }
        delete[] flat;
        return 0;
    }
}
)NS");

    const std::string consumerProject = (consumer / "JaggedConsumer.nsproj").string();
    const std::string executable = (consumer / "JaggedConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArguments = {
        "-O0", "--emit-project-oop-exe", consumerProject, "-o", executable};
    output.clear();
    error.clear();
    const bool consumerEmitted = runDriver(consumerArguments, output, error) == ExitCode::Success;
    tests.expect(consumerEmitted,
        "T31 consumer compiles and links jagged arrays, views and owning initializers at O0 (" + error + output + ")");
    if (consumerEmitted)
    {
        const std::string command = "cmd.exe /d /s /c \"\"" + executable + "\"\"";
        tests.expect(std::system(command.c_str()) == 0,
            "jagged destruction, slot clearing, owning initializer cleanup and borrows execute exactly once");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", consumerProject, "-O0"};
    output.clear();
    error.clear();
    const bool hirDumped = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hirDumped && output.find("type=builtin:int[][]") != std::string::npos &&
            output.find("array-allocation") != std::string::npos &&
            output.find("integer-constant type=builtin:int value=4") != std::string::npos,
        "HIR publishes nested array allocation and folds fixed Length without hidden storage (" + error + ")");

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", consumerProject, "-O0"};
    output.clear();
    error.clear();
    const bool mirDumped = runDriver(mirArguments, output, error) == ExitCode::Success;
    tests.expect(mirDumped && output.find("type=builtin:int[][]") != std::string::npos &&
            output.find("array-allocate") != std::string::npos &&
            output.find("deallocate") != std::string::npos,
        "MIR exposes the real jagged representation and explicit destruction cost (" + error + ")");

    const std::string debugObject = (consumer / "JaggedConsumer.debug.obj").string();
    const std::array<std::string_view, 6> debugArguments = {
        "-g", "-O0", "--emit-project-oop-obj", consumerProject, "-o", debugObject};
    output.clear();
    error.clear();
    const bool debugEmitted = runDriver(debugArguments, output, error) == ExitCode::Success;
    const std::string debugBytes = debugEmitted ? readFile(debugObject) : std::string{};
    tests.expect(debugEmitted &&
            debugBytes.find("builtin:int[][]") != std::string::npos &&
            debugBytes.find("builtin:int[]") != std::string::npos,
        "CodeView object preserves both jagged header type names (" + error + output + ")");

    expectRejected(tests, root, "BorrowEscape", std::string(kBorrowedPrelude) + R"NS(
public class Case
{
    public static BorrowedReference<Resource> Leak()
    {
        owned Resource owner = new Resource(1);
        return new BorrowedReference<Resource>(owner);
    }
}
)NS", "borrows from local 'owner' and cannot be returned");

    expectRejected(tests, root, "BorrowOwnerMove", std::string(kBorrowedPrelude) + R"NS(
public class Case
{
    public static int Run()
    {
        owned Resource owner = new Resource(1);
        BorrowedReference<Resource> view = new BorrowedReference<Resource>(owner);
        owned Resource moved = move(owner);
        return view.Get().Value;
    }
}
)NS", "cannot move owner 'owner' while a borrowed local is alive");

    expectRejected(tests, root, "BorrowRetention", std::string(kBorrowedPrelude) + R"NS(
public class Case
{
    public BorrowedReference<Resource> retained;
}
)NS", "cannot have view type");

    expectRejected(tests, root, "ReadOnlyMutation", R"NS(
public borrowed struct ReadOnlyArrayView
{
    private int* data;
    public ReadOnlyArrayView(int* value) { data = value; }
    public int this[int index] { get { return data[index]; } }
}
public class Case
{
    public static void Mutate(int[] values)
    {
        ReadOnlyArrayView view = new ReadOnlyArrayView(&values[0]);
        view[0] = 42;
    }
}
)NS", "has no accessible setter");

    const std::string borrowedSource = readFile(
        std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/BorrowedReference.ns");
    tests.expect(borrowedSource.find("public borrowed struct BorrowedReference<T>") != std::string::npos &&
            borrowedSource.find("~BorrowedReference") == std::string::npos &&
            borrowedSource.find("delete target") == std::string::npos,
        "NativeSharp.Core publishes a one-reference non-owning relationship that never destroys its target");

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
