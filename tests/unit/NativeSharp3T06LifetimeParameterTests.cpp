#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/LibraryMetadata.h"

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
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    std::string_view edition = "3",
    std::string_view outputType = "Exe",
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>" + std::string(edition) +
        "</Language><OutputType>" + std::string(outputType) +
        "</OutputType></PropertyGroup><ItemGroup>" + std::string(items) +
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
}

std::string diagnosticsText(const ProjectCompilation& compilation)
{
    std::string result;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (!result.empty()) result += " | ";
        result += diagnostic.message();
    }
    return result;
}

bool rejectsWith(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view expected,
    std::string& observed,
    std::string_view edition = "3")
{
    ProjectCompilation compilation;
    std::string error;
    std::string projectName(name);
    for (char& character : projectName)
        if (character == '-') character = '_';
    const bool compiled = compilation.compileProjectMethodBodies(
        writeProject(root / std::string(name), projectName, source, edition), error);
    observed = error + diagnosticsText(compilation);
    return (!compiled || compilation.diagnostics().hasErrors()) &&
        observed.find(expected) != std::string::npos;
}

ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

bool compileAndRun(
    const std::filesystem::path& project,
    int expected,
    std::string& diagnostic)
{
    const std::filesystem::path executable = project.parent_path() /
        (project.stem().string() + ".exe");
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result != ExitCode::Success) return false;
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    return false;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t06_lifetime_parameters";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    const std::filesystem::path positive = writeProject(
        root / "positive", "LifetimePositive", R"NS(
public class Holder
{
    public int Value;
    public Holder(int value) { Value = value; }
    public ref[this] int Get() { return ref Value; }
}

public class Program
{
    private static ref[a] int Select<lifetime a>(ref[a] int left, ref[a] int right, bool takeLeft)
    {
        if (takeLeft) { return ref left; }
        return ref right;
    }

    private static ref[shorter] int Narrow<lifetime longer, lifetime shorter>(ref[longer] int value)
        where longer : shorter
    {
        return ref value;
    }

    public static int Main()
    {
        int first = 20;
        int second = 21;
        {
            ref int selected = ref Select(ref first, ref second, false);
            selected = selected + 1;
        }
        int narrowed = Narrow(ref second);
        Holder holder = new Holder(narrowed);
        ref int field = ref holder.Get();
        field = field + 20;
        return field;
    }
}
)NS");
    std::string diagnostic;
    tests.expect(compileAndRun(positive, 42, diagnostic),
        "named, inferred, outlives and receiver lifetimes execute at O0 through NSLink (" +
            diagnostic + ")");

    const std::string projectText = positive.string();
    const std::array<std::string_view, 3> dumpArguments = {
        "--dump-project-members", projectText, "-O0"};
    std::string output;
    std::string error;
    tests.expect(
        runDriver(dumpArguments, output, error) == ExitCode::Success &&
            output.find("return-lifetime=a") != std::string::npos &&
            output.find("outlives=longer:shorter") != std::string::npos &&
            output.find("return-lifetime=this") != std::string::npos &&
            output.find("lifetime=longer") != std::string::npos,
        "member dump publishes erased lifetime parameters, bindings and outlives edges (" +
            error + output + ")");

    ProjectCompilation surface;
    std::string surfaceError;
    const bool surfaceCompiled = surface.compileProjectOopMir(
        writeProject(root / "surface", "LifetimeSurface", R"NS(
public borrowed struct View<lifetime a>
{
    public borrowed[a] string Source;
}
public delegate ref[a] int Selector<lifetime a>(ref[a] int value);
public trait ISelect
{
    public ref[a] int Select<lifetime a>(ref[a] int value);
}
public class SelectorHost { }
impl ISelect for SelectorHost
{
    public ref[b] int Select<lifetime b>(ref[b] int value) { return ref value; }
}
public class Program { public static int Main() { return 0; } }
)NS"), surfaceError);
    tests.expect(surfaceCompiled && !surface.diagnostics().hasErrors(),
        "borrowed structs, erased delegate binders and alpha-equivalent trait contracts compile (" +
            surfaceError + diagnosticsText(surface) + ")");

    const std::filesystem::path producer = root / "modules" / "Producer";
    const std::filesystem::path producerProject = writeProject(
        producer, "LifetimeApi", R"NS(
namespace LifetimeApi
{
    public borrowed struct View<lifetime a>
    {
        public borrowed[a] string Source;
    }

    public delegate ref[a] int Selector<lifetime a>(ref[a] int value);

    public class BorrowApi
    {
        public static ref[shorter] int Narrow<lifetime longer, lifetime shorter>(
            ref[longer] int value) where longer : shorter
        {
            return ref value;
        }
    }
}
)NS", "3", "StaticLibrary");
    const std::filesystem::path producerLibrary = producer / "LifetimeApi.lib";
    const std::string producerProjectText = producerProject.string();
    const std::string producerLibraryText = producerLibrary.string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", producerLibraryText};
    output.clear();
    error.clear();
    const ExitCode producerResult = runDriver(producerArguments, output, error);

    nativesharp::interop::LibraryMetadata metadata;
    nativesharp::interop::LibraryMetadataCodec codec;
    std::string metadataError;
    const bool metadataRead = producerResult == ExitCode::Success &&
        codec.read(producer / "LifetimeApi.nsmeta", metadata, metadataError);
    const auto view = std::ranges::find_if(
        metadata.types,
        [](const nativesharp::interop::LibraryMetadataType& type)
        {
            return type.fullyQualifiedName == "LifetimeApi.View";
        });
    const auto selector = std::ranges::find_if(
        metadata.types,
        [](const nativesharp::interop::LibraryMetadataType& type)
        {
            return type.fullyQualifiedName == "LifetimeApi.Selector";
        });
    const auto narrow = std::ranges::find_if(
        metadata.members,
        [](const nativesharp::interop::LibraryMetadataMember& member)
        {
            return member.name == "Narrow";
        });
    tests.expect(
        metadataRead && metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor &&
            view != metadata.types.end() && view->lifetimeParameters == std::vector<std::string>{"a"} &&
            view->fields.size() == 1U && view->fields[0].lifetimeName == "a" &&
            selector != metadata.types.end() && selector->delegateReferenceReturn &&
            selector->delegateReturnLifetimeName == "a" &&
            selector->delegateParameterLifetimeNames == std::vector<std::string>{"a"} &&
            narrow != metadata.members.end() && narrow->returnLifetimeName == "shorter" &&
            narrow->lifetimeParameters == std::vector<std::string>({"longer", "shorter"}) &&
            narrow->lifetimeOutlivesConstraints ==
                std::vector<std::pair<std::string, std::string>>({{"longer", "shorter"}}) &&
            narrow->parameters.size() == 1U &&
            narrow->parameters[0].lifetimeName == "longer",
        "current NSMETA preserves the 1.34 type, field, delegate and callable lifetime contracts (" +
            error + output + metadataError + ")");

    nativesharp::interop::LibraryMetadata downlevel = metadata;
    downlevel.formatMinor = 33;
    std::string downlevelError;
    tests.expect(
        metadataRead && !codec.writeModel(
            downlevel, root / "lifetime-cannot-be-1.33.nsmeta", downlevelError) &&
            downlevelError.find("before 1.34") != std::string::npos,
        "a lifetime contract cannot be mislabeled as NSMETA 1.33 (" + downlevelError + ")");

    const std::string_view consumerItems =
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/LifetimeApi.lib\" />";
    const std::filesystem::path consumerProject = writeProject(
        root / "modules" / "Consumer", "LifetimeConsumer", R"NS(
using LifetimeApi;
public class Program
{
    public static int Main()
    {
        int value = 41;
        {
            ref int narrowed = ref BorrowApi.Narrow(ref value);
            narrowed = narrowed + 1;
        }
        return value;
    }
}
)NS", "3", "Exe", consumerItems);
    diagnostic.clear();
    tests.expect(
        producerResult == ExitCode::Success && compileAndRun(consumerProject, 42, diagnostic),
        "a separate LibraryReference consumer reconstructs and executes the lifetime contract (" +
            error + output + diagnostic + ")");

    std::string observed;
    tests.expect(rejectsWith(root, "ambiguous-elision", R"NS(
public class Program
{
    private static ref int Pick(ref int left, ref int right) { return ref left; }
    public static int Main() { return 0; }
}
)NS", "has multiple input lifetimes", observed),
        "multiple elided input lifetimes require an explicit output binding (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "ambiguous-delegate", R"NS(
public delegate ref int Picker(ref int left, ref int right);
public class Program { public static int Main() { return 0; } }
)NS", "ref-returning delegate 'Picker' has multiple input lifetimes", observed),
        "delegates use the same fail-closed elision rule as methods (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "delegate-out-lifetime", R"NS(
public delegate void Filler<lifetime a>(out[a] int value);
public class Program { public static int Main() { return 0; } }
)NS", "out delegate parameter 'value' initializes storage", observed),
        "delegate out parameters cannot claim an input lifetime (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "undeclared", R"NS(
public class Program
{
    private static ref[missing] int Pick<lifetime a>(ref[a] int value) { return ref value; }
    public static int Main() { return 0; }
}
)NS", "names undeclared lifetime 'missing'", observed),
        "undeclared output lifetime names are rejected at the declaration (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "wrong-return", R"NS(
public class Program
{
    private static ref[a] int Pick<lifetime a, lifetime b>(ref[a] int left, ref[b] int right)
    { return ref right; }
    public static int Main() { return 0; }
}
)NS", "returned storage has lifetime 'b'", observed),
        "a body cannot return storage tied to an unrelated lifetime (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "missing-outlives", R"NS(
public class Program
{
    private static ref[shorter] int Narrow<lifetime longer, lifetime shorter>(ref[longer] int value)
    { return ref value; }
    public static int Main() { return 0; }
}
)NS", "has no ref/in parameter whose lifetime outlives it", observed),
        "a named result requires a declared source or an outlives proof (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "out-annotation", R"NS(
public class Program
{
    private static void Fill<lifetime a>(out[a] int value) { value = 42; }
    public static int Main() { int value; Fill(out value); return value; }
}
)NS", "cannot carry an input lifetime", observed),
        "out parameters cannot pretend to borrow pre-existing input storage (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "cycle", R"NS(
public class Program
{
    private static ref[a] int Bad<lifetime a, lifetime b>(ref[a] int value)
        where a : b where b : a
    { return ref value; }
    public static int Main() { return 0; }
}
)NS", "lifetime constraints contain a cycle", observed),
        "cyclic outlives declarations are rejected deterministically (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "type-cycle", R"NS(
public borrowed struct BadView<lifetime a, lifetime b> where a : b where b : a
{
    public borrowed[a] string Source;
}
public class Program { public static int Main() { return 0; } }
)NS", "lifetime constraints contain a cycle", observed),
        "borrowed type outlives graphs reject cycles even without a ref-returning method (" +
            observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "void-cycle", R"NS(
public class Program
{
    private static void Bad<lifetime a, lifetime b>(ref[a] int value)
        where a : b where b : a
    { value = value + 1; }
    public static int Main() { return 0; }
}
)NS", "lifetime constraints contain a cycle", observed),
        "non-ref callables validate their outlives graph instead of ignoring it (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "local-escape", R"NS(
public class Program
{
    private static ref[a] int Bad<lifetime a>(ref[a] int input)
    {
        int local = 42;
        return ref local;
    }
    public static int Main() { int value = 0; return Bad(ref value); }
}
)NS", "cannot expose storage owned by the current stack frame", observed),
        "a named result cannot launder a local borrow into its caller (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "closure-escape", R"NS(
public delegate int Getter();
public class Program
{
    private static int Read<lifetime a>(ref[a] int value)
    {
        Getter getter = () => value;
        return getter();
    }
    public static int Main() { int value = 42; return Read(ref value); }
}
)NS", "cannot be captured by a lambda because the delegate may outlive the referenced storage", observed),
        "a closure cannot erase a named borrowed parameter's lifetime (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "edition2", R"NS(
public class Program
{
    private static ref[a] int Pick<lifetime a>(ref[a] int value) { return ref value; }
    public static int Main() { return 0; }
}
)NS", "requires <Language>3</Language>", observed, "2"),
        "named lifetime syntax is edition-gated (" + observed + ")");

    return tests.resultKeeping(root);
}
