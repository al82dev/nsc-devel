#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::driver::Driver;
using nativesharp::ExitCode;
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
    std::string_view outputType,
    std::string_view source,
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>" + std::string(outputType) +
        "</OutputType></PropertyGroup><ItemGroup>" + std::string(items) +
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
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

bool rejected(
    const std::filesystem::path& directory,
    std::string_view source,
    std::string_view expected,
    std::string& diagnostic)
{
    const auto project = writeProject(directory, "RejectedConstraint", "Exe", source);
    const std::string projectText = project.string();
    const std::array<std::string_view, 3> arguments = {
        "--check-project-oop-hir", projectText, "-O0"};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result == ExitCode::Success)
    {
        diagnostic += "constraint was accepted";
    }
    const bool accepted = result != ExitCode::Success &&
        diagnostic.find(expected) != std::string::npos;
    if (!accepted)
    {
        std::cerr << diagnostic << '\n';
    }
    return accepted;
}

bool compileAndRun(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    int expected,
    std::string& diagnostic,
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    const auto project = writeProject(directory, name, "Exe", source, items);
    const std::string projectText = project.string();
    const std::string executableText = (directory / (std::string(name) + ".exe")).string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result != ExitCode::Success)
    {
        if (diagnostic.empty())
        {
            diagnostic = "compiler exit code " + std::to_string(static_cast<int>(result)) +
                " without a diagnostic";
        }
        std::cerr << diagnostic << '\n';
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed != expected)
    {
        diagnostic += "process exit code " + std::to_string(observed) +
            ", expected " + std::to_string(expected);
        std::cerr << diagnostic << '\n';
        return false;
    }
    return true;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t04_declarative_constraints";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    tests.expect(
        compileAndRun(root / "capabilities", "Capabilities", R"NS(
namespace Capabilities
{
    public interface ITagged { public int Tag(); }
    public struct Pod
    {
        public int Value;
    }
    public class Resource : ITagged
    {
        public Resource() { }
        public int Tag() { return 2; }
    }
    public class Program
    {
        private static int Value<T>(T value)
            where T : unmanaged, copy, move, new(), notnull
        {
            return 40;
        }
        private static int Reference<T>(T value)
            where T : class, lifetime, copy, move, new(), ITagged
        {
            return 2;
        }
        public static int Main()
        {
            Pod pod; pod.Value = 1;
            Resource resource = new Resource();
            return Value(pod) + Reference(resource);
        }
    }
}
)NS", 42, diagnostic),
        "trait and declarative capability constraints execute through O0/NSLink (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        compileAndRun(root / "equality", "Equality", R"NS(
namespace Equality
{
    public class Program
    {
        private static int Same<T, U>(T left, U right) where T == U { return 38; }
        private static int Four<const uint N>() where N == 4 { return (int)N; }
        public static int Main() { return Same<int, int>(1, 2) + Four<4>(); }
    }
}
)NS", 42, diagnostic),
        "type and const equality constraints share normalized specialization identities (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        compileAndRun(root / "cycle", "EqualityCycle", R"NS(
namespace EqualityCycle
{
    public class Program
    {
        private static int Same<T, U>(T left, U right) where T == U where U == T { return 42; }
        public static int Main() { return Same<int, int>(1, 2); }
    }
}
)NS", 42, diagnostic),
        "a cyclic equality graph terminates and accepts a consistent specialization (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        compileAndRun(root / "overload", "ConstraintOverload", R"NS(
namespace ConstraintOverload
{
    public class Program
    {
        private static int Select<T>(T value) where T : struct { return 1; }
        private static int Select<T>(T[] values) { return 42; }
        public static int Main()
        {
            int[] values = new int[1];
            return Select<int>(values);
        }
    }
}
)NS", 42, diagnostic),
        "an inapplicable constrained overload does not poison the selected candidate (" +
            diagnostic + ")");

    const auto canonicalProject = writeProject(
        root / "canonical", "CanonicalConstraints", "Exe", R"NS(
class First<T> where T : copy, move { }
class Second<T> where T : move, copy { }
class Program { public static int Main() { return 42; } }
)NS");
    nativesharp::frontend::ProjectCompilation canonicalCompilation;
    std::string canonicalError;
    const bool canonicalCompiled = canonicalCompilation.compileProjectGenericConstraints(
        canonicalProject, canonicalError);
    const auto* canonicalModel = canonicalCompilation.genericConstraintModel();
    tests.expect(
        canonicalCompiled && canonicalModel != nullptr &&
            canonicalModel->normalizedConstraintIdentity(0) ==
                canonicalModel->normalizedConstraintIdentity(1) &&
            !canonicalModel->normalizedConstraintIdentity(0).empty(),
        "constraint source order does not change the cache-facing canonical identity (" +
            canonicalError + ")");

    struct Rejection final { const char* name; const char* source; const char* expected; };
    const std::array<Rejection, 8> rejections = {{
        {"value-type", R"NS(
class Ref { }
class Program { static int Need<T>() where T : struct { return 0; }
public static int Main() { return Need<Ref>(); } }
)NS", "requires a value type"},
        {"constructor", R"NS(
class Ref { private Ref(int value) { } }
class Program { static int Need<T>() where T : new() { return 0; }
public static int Main() { return Need<Ref>(); } }
)NS", "requires a public parameterless or implicit default constructor"},
        {"unmanaged", R"NS(
struct Managed { public string Text; }
class Program { static int Need<T>() where T : unmanaged { return 0; }
public static int Main() { return Need<Managed>(); } }
)NS", "requires recursively unmanaged storage"},
        {"lifetime", R"NS(
class Program { static int Need<T>() where T : lifetime { return 0; }
public static int Main() { return Need<int>(); } }
)NS", "requires a type with an explicit tracked lifetime"},
        {"type-equality", R"NS(
class Program { static int Same<T, U>() where T == U { return 0; }
public static int Main() { return Same<int, long>(); } }
)NS", "requires equality with"},
        {"const-equality", R"NS(
class Program { static int Four<const int N>() where N == 4 { return 0; }
public static int Main() { return Four<5>(); } }
)NS", "requires equality with"},
        {"contradiction", R"NS(
class Box<T> where T : class, struct { }
class Program { public static int Main() { return 0; } }
)NS", "cannot require both class and struct"},
        {"redundant", R"NS(
class Box<T> where T : copy, copy { }
class Program { public static int Main() { return 0; } }
)NS", "duplicate 'copy' constraint"}
    }};
    for (const Rejection& rejection : rejections)
    {
        diagnostic.clear();
        tests.expect(
            rejected(root / rejection.name, rejection.source, rejection.expected, diagnostic),
            std::string(rejection.name) + " reports its causal obligation (" + diagnostic + ")");
    }

    const std::filesystem::path producer = root / "modules" / "Producer";
    const auto producerProject = writeProject(producer, "ConstraintApi", "StaticLibrary", R"NS(
namespace ConstraintApi
{
    public class Api
    {
        public static int Measure<T>(T value) where T : unmanaged, copy { return 42; }
    }
}
)NS");
    const std::string producerProjectText = producerProject.string();
    const std::string libraryText = (producer / "ConstraintApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", libraryText};
    std::string output;
    std::string error;
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    diagnostic = error + output;
    const bool moduleRoundTrip = producerResult == ExitCode::Success && compileAndRun(
        root / "modules" / "Consumer", "ConstraintConsumer", R"NS(
using ConstraintApi;
class Program { public static int Main() { return Api.Measure<int>(1); } }
)NS", 42, diagnostic,
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/ConstraintApi.lib\" />");
    tests.expect(
        moduleRoundTrip,
        "source and precompiled generic constraints have equivalent behavior (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
