#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
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
    writeProject(directory, "Rejected", source);
    const std::string project = (directory / "Rejected.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--check-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    return result != ExitCode::Success && diagnostic.find(expected) != std::string::npos;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t02_tuple_literals";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    writeProject(root / "positive", "TupleLiterals", R"NS(
namespace TupleLiterals
{
    public class Base { }
    public class Derived : Base { }

    public class Program
    {
        private static int trace = 0;

        private static int Mark(int value)
        {
            trace = trace * 10 + value;
            return value;
        }

        private static int ConsumeExact((int, int) value) { return 42; }
        private static int ConsumeWide((long, double) value) { return 42; }
        private static int ConsumeReferences((Base, Base) value) { return 42; }
        private static int Pick((long, long) value) { return 42; }
        private static int Pick((double, double) value) { return 1; }

        private static (long, double) ConvertedReturn()
        {
            return (Mark(4), Mark(2));
        }

        public static int Main()
        {
            trace = 0;
            var inferred = (Mark(4), Mark(2));
            if (trace != 42 || ConsumeExact(inferred) != 42) { return 1; }

            trace = 0;
            if (ConsumeWide((Mark(4), Mark(2))) != 42 || trace != 42) { return 2; }

            trace = 0;
            (long, double) converted = ConvertedReturn();
            if (trace != 42 || ConsumeWide(converted) != 42) { return 3; }

            raii Derived first = new Derived();
            raii Derived second = new Derived();
            if (ConsumeReferences((first, second)) != 42) { return 4; }

            if (Pick((20, 22)) != 42) { return 5; }
            return 42;
        }
    }
}
)NS");

    const std::string project = (root / "positive" / "TupleLiterals.nsproj").string();
    const std::string executable = (root / "positive" / "TupleLiterals.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", project, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 compiles inferred, widened, reference-upcast, temporary and overloaded tuples (" +
            error + output + ")");
    if (emitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "tuple literals evaluate once left-to-right and choose the best element-wise overload");
    }

    std::string diagnostic;
    tests.expect(
        rejected(root / "element_error", R"NS(
class Program
{
    public static int Main()
    {
        (long, long) values = (1, "not an integer");
        return 0;
    }
}
)NS", "cannot convert tuple element 2", diagnostic),
        "an invalid tuple conversion identifies the failing element (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "arity_error", R"NS(
class Program
{
    public static int Main()
    {
        (long, long) values = (1, 2, 3);
        return 0;
    }
}
)NS", "tuple conversion requires equal arity", diagnostic),
        "an invalid tuple conversion reports source and target arity (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
