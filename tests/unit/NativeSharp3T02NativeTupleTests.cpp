#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Language/AggregateSemantics.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

std::string tupleType(std::size_t arity, std::string_view element = "int")
{
    std::string result = "(";
    for (std::size_t index = 0; index < arity; ++index)
    {
        if (index != 0) result += ", ";
        result += element;
    }
    if (arity == 1) result += ',';
    result += ')';
    return result;
}

std::string tupleLiteral(std::size_t arity)
{
    std::string result = "(";
    for (std::size_t index = 0; index < arity; ++index)
    {
        if (index != 0) result += ", ";
        result += std::to_string(index + 1);
    }
    if (arity == 1) result += ',';
    result += ')';
    return result;
}

std::string positiveSource()
{
    std::ostringstream source;
    source << "namespace NativeTupleTests\n{\n";
    source << "    public struct Box<T> { public T Value; }\n";
    source << "    public class Program\n    {\n";
    for (std::size_t arity = 0; arity <= 16; ++arity)
    {
        source << "        private static " << tupleType(arity) << " Make" << arity
               << "() { return " << tupleLiteral(arity) << "; }\n";
        source << "        private static int Take" << arity << "(" << tupleType(arity)
               << " value) { return " << arity << "; }\n";
    }
    source << "        private static " << tupleType(32, "long")
           << " MakeLarge() { return " << tupleLiteral(32) << "; }\n";
    source << "        private static int TakeLarge(" << tupleType(32, "long")
           << " value) { return 32; }\n";
    source << R"NS(
        private static (int, (int, long)) Nested()
        {
            return (1, (2, 3));
        }

        private static int Named((int x, long y) value) { return 42; }
        private static int Generic(Box<(int, int)> value) { return 42; }

        public static int Main()
        {
)NS";
    for (std::size_t arity = 0; arity <= 16; ++arity)
    {
        source << "            if (Take" << arity << "(Make" << arity << "()) != "
               << arity << ") { return 1; }\n";
    }
    source << R"NS(
            if (TakeLarge(MakeLarge()) != 32) { return 2; }
            (int a, long b) renamed = (20, 22);
            if (Named(renamed) != 42) { return 3; }
            (int, (int, long)) nested = Nested();
            Box<(int, int)> box = { (20, 22) };
            if (Generic(box) != 42) { return 4; }
            return 42;
        }
    }
}
)NS";
    return source.str();
}
}

int main()
{
    TestContext tests;
    using namespace nativesharp;

    // Every supported arity has an injective, round-trippable structural identity.  A large
    // tuple exercises the length-prefix grammar beyond the ABI's direct-register sizes.
    for (const std::size_t arity : std::array<std::size_t, 18>{
             0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 64})
    {
        std::vector<std::string> elements;
        elements.reserve(arity);
        for (std::size_t index = 0; index < arity; ++index)
            elements.push_back(index % 2 == 0 ? "builtin:int" : "builtin:long");
        const std::string key = frontend::ProjectTypeConversion::tupleTypeKey(elements);
        const std::optional<std::vector<std::string>> decoded =
            frontend::ProjectTypeConversion::tupleElementTypeKeys(key);
        tests.expect(decoded.has_value() && *decoded == elements,
            "tuple identity round-trips arity " + std::to_string(arity));
    }

    const std::array<language::TupleElement, 2> named = {{{"builtin:int", "x"}, {"builtin:long", "y"}}};
    const std::array<language::TupleElement, 2> renamed = {{{"builtin:int", "left"}, {"builtin:long", "right"}}};
    tests.expect(
        language::tupleIdentity(named) == language::tupleIdentity(renamed) &&
            !language::tupleNamesAffectIdentity(),
        "tuple element names do not affect identity or ABI");

    const std::array<language::AggregateFieldLayout, 1> one = {{{4, 4}}};
    const std::array<language::AggregateFieldLayout, 3> nested = {{{4, 4}, {8, 8}, {4, 4}}};
    const auto oneLayout = language::computeWindowsX64AggregateLayout(one);
    const auto nestedLayout = language::computeWindowsX64AggregateLayout(nested);
    tests.expect(
        oneLayout.has_value() && oneLayout->size == 4 && oneLayout->alignment == 4 &&
            nestedLayout.has_value() && nestedLayout->offsets == std::vector<std::size_t>({0, 8, 16}) &&
            nestedLayout->size == 24 && nestedLayout->alignment == 8,
        "single and nested tuple fields follow deterministic Windows x64 aggregate layout");
    tests.expect(
        language::classifyWindowsX64AggregateReturn(1) ==
                language::WindowsX64AggregateReturn::DirectInteger &&
            language::classifyWindowsX64AggregateReturn(4) ==
                language::WindowsX64AggregateReturn::DirectInteger &&
            language::classifyWindowsX64AggregateReturn(24) ==
                language::WindowsX64AggregateReturn::IndirectSRet,
        "unit/single carriers return directly and large carriers use sret");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t02_native_tuples";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    writeProject(root / "positive", "NativeTuples", positiveSource());

    const std::string project = (root / "positive" / "NativeTuples.nsproj").string();
    const std::string executable = (root / "positive" / "NativeTuples.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", project, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 compiles tuple arities 0..16, a large tuple, nesting, names and generics (" +
            error + output + ")");
    if (emitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "native tuple carriers execute through direct and indirect NSLink ABI paths");
    }

    writeProject(root / "move_only", "MoveOnlyTuple", R"NS(
struct Owner { public owned string Text; }
class Program
{
    public static int Main()
    {
        Owner owner = { "owned" };
        var (value, number) = (owner, 42);
        return number;
    }
}
)NS");
    const std::string moveProject = (root / "move_only" / "MoveOnlyTuple.nsproj").string();
    const std::array<std::string_view, 3> checkArguments = {
        "--check-project-oop-hir", moveProject, "-O0"};
    output.clear();
    error.clear();
    const ExitCode moveOnly = runDriver(checkArguments, output, error);
    tests.expect(
        moveOnly != ExitCode::Success &&
            (error + output).find("drop flags") != std::string::npos,
        "move-only tuple deconstruction fails before partial-drop lowering is required (" +
            error + output + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
