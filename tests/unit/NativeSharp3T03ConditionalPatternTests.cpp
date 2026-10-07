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
    std::string_view source,
    std::string_view language = "3")
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>" + std::string(language) + "</Language></PropertyGroup>"
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
    std::string& diagnostic,
    std::string_view language = "3")
{
    writeProject(directory, "RejectedConditionalPattern", source, language);
    const std::string project = (directory / "RejectedConditionalPattern.nsproj").string();
    const std::string executable = (directory / "RejectedConditionalPattern.exe").string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", project, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    return result != ExitCode::Success && diagnostic.find(expected) != std::string::npos;
}

std::size_t countOccurrences(std::string_view text, std::string_view needle)
{
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string_view::npos)
    {
        ++count;
        position += needle.size();
    }
    return count;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t03_conditional_patterns";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path direct = root / "direct";
    writeProject(direct, "ConditionalPatterns", R"NS(
namespace ConditionalPatterns
{
    public variant Maybe { Some(int), None }

    public class Program
    {
        private static int Calls;

        private static Maybe Once(int value)
        {
            Calls = Calls + 1;
            return Maybe.Some(value);
        }

        private static Maybe At(int index)
        {
            if (index < 3) return Maybe.Some(index + 1);
            return Maybe.None();
        }

        private static int Unwrap(Maybe value)
        {
            let Some(var payload) = value else { return -100; };
            return payload;
        }

        private static int ViaIfLet(Maybe value)
        {
            if (let Some(var payload) = value) return payload;
            return 0;
        }

        private static int ViaMatch(Maybe value)
        {
            match (value)
            {
                Some(var payload) => { return payload; },
                None => { return 0; }
            }
            return 0;
        }

        public static int Main()
        {
            int score = 0;
            if (let Some(var first) = Once(20))
                score = score + first;
            else
                return 1;
            if (Calls != 1) return 2;

            if (let None = Maybe.None())
                score = score + 0;
            else
                return 3;

            int index = 0;
            while (let Some(var current) = At(index))
            {
                score = score + current;
                index = index + 1;
            }
            if (index != 3) return 4;

            score = score + Unwrap(Maybe.Some(15));
            if (let Some(var outer) = Maybe.Some(1))
            {
                if (let Some(var inner) = Maybe.Some(outer)) score = score + inner;
            }
            if (ViaIfLet(Maybe.Some(7)) != ViaMatch(Maybe.Some(7))) return 5;
            if (ViaIfLet(Maybe.None()) != ViaMatch(Maybe.None())) return 6;
            int aliased = 40;
            if (let ref alias = aliased) alias = alias + 2;
            if (aliased != 42) return 7;
            return score;
        }
    }
}
)NS");
    const std::string project = (direct / "ConditionalPatterns.nsproj").string();
    const std::string executable = (direct / "ConditionalPatterns.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", project, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitResult = runDriver(emitArguments, output, error);
    tests.expect(
        emitResult == ExitCode::Success,
        "if let, while let, nested bindings, and let else compile at O0 (" +
            error + output + ")");
    if (emitResult == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "conditional patterns preserve binding scopes and evaluate each source once");
    }

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", project, "-O0"};
    output.clear();
    error.clear();
    const ExitCode mirResult = runDriver(mirArguments, output, error);
    tests.expect(
        mirResult == ExitCode::Success &&
            output.find("ViaIfLet") != std::string::npos &&
            output.find("ViaMatch") != std::string::npos &&
            countOccurrences(output, "IsSome") >= 2 &&
            countOccurrences(output, "__nsc_variant_extract_Some") >= 2,
        "if let and match reach O0 MIR through the same case-test and payload-extraction "
        "primitives (" + error + ")");

    std::string diagnostic;
    tests.expect(
        rejected(root / "scope", R"NS(
variant Maybe { Some(int), None }
class Program { public static int Main() {
    if (let Some(var value) = Maybe.Some(42)) { int inside = value; }
    return value;
} }
)NS", "unknown identifier 'value'", diagnostic),
        "if-let bindings do not escape the success branch (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "nondivergent", R"NS(
variant Maybe { Some(int), None }
class Program { public static int Main() {
    let Some(var value) = Maybe.None() else { int fallback = 0; };
    return value;
} }
)NS", "else body of a let-else statement must diverge", diagnostic),
        "a non-divergent let-else continuation is rejected causally (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        !rejected(root / "edition", R"NS(
variant Maybe { Some(int), None }
class Program { public static int Main() {
    if (let Some(var value) = Maybe.Some(42)) return value;
    return 0;
} }
)NS", "requires <Language>3</Language>", diagnostic, "2"),
        "conditional pattern syntax is independent of historical edition input (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
