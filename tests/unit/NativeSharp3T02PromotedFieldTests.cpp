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

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
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
    writeProject(directory, "RejectedPromotedField", source);
    const std::string project = (directory / "RejectedPromotedField.nsproj").string();
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t02_promoted_fields";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    writeProject(root / "positive", "PromotedFields", R"NS(
namespace PromotedFields
{
    public struct Ordered(public int First, public int Second, public readonly long Third = 20)
    {
        public long Sum() { return First + Second + Third; }
    }

    public struct PrivateByDefault(int Hidden)
    {
        public int Read() { return Hidden; }
    }

    public record struct PublicByDefault(int Value)
    {
    }

    public struct Box<T>(public T Value)
    {
    }

    public struct Empty()
    {
        public int Answer() { return 42; }
    }

    public class Program
    {
        private static int trace;

        private static int Mark(int marker, int value)
        {
            trace = trace * 10 + marker;
            return value;
        }

        public static int Main()
        {
            trace = 0;
            Ordered ordered = new Ordered(Mark(1, 2), Mark(2, 20));
            if (trace != 12 || ordered.Sum() != 42) { return 1; }
            if (ordered.First != 2 || ordered.Second != 20 || ordered.Third != 20) { return 2; }

            PrivateByDefault hidden = new PrivateByDefault(42);
            if (hidden.Read() != 42) { return 3; }
            PublicByDefault visible = new PublicByDefault(42);
            if (visible.Value != 42) { return 4; }
            Box<int> generic = new Box<int>(42);
            if (generic.Value != 42) { return 5; }
            Empty empty = new Empty();
            return empty.Answer();
        }
    }
}
)NS");

    const std::string positiveProject =
        (root / "positive" / "PromotedFields.nsproj").string();
    const std::string executable = (root / "positive" / "PromotedFields.exe").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", positiveProject, "-O0", "-o", executable};
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 emits ordered/defaulted/readonly/generic promoted fields through NSLink (" +
            error + output + ")");
    if (emitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "primary constructor arguments run left-to-right and initialize every field once");
    }

    const std::array<std::string_view, 2> layoutArguments = {
        "--dump-project-layouts", positiveProject};
    output.clear();
    error.clear();
    const bool layoutDumped =
        runDriver(layoutArguments, output, error) == ExitCode::Success;
    tests.expect(
        layoutDumped &&
            output.find("Ordered size=16 align=8 representation=inline-value") != std::string::npos &&
            output.find("name=First type=builtin:int offset=0 size=4 align=4") != std::string::npos &&
            output.find("name=Second type=builtin:int offset=4 size=4 align=4") != std::string::npos &&
            output.find("name=Third type=builtin:long offset=8 size=8 align=8") != std::string::npos,
        "promoted fields retain textual order and canonical Windows x64 layout (" +
            error + output + ")");

    const std::string debugObject =
        (root / "positive" / "PromotedFields.debug.obj").string();
    const std::array<std::string_view, 6> debugArguments = {
        "-g", "-O0", "--emit-project-oop-obj", positiveProject, "-o", debugObject};
    output.clear();
    error.clear();
    const bool debugEmitted = runDriver(debugArguments, output, error) == ExitCode::Success;
    const std::string debugBytes = debugEmitted ? readFile(debugObject) : std::string{};
    tests.expect(
        debugEmitted && debugBytes.find("Ordered") != std::string::npos &&
            debugBytes.find("First") != std::string::npos &&
            debugBytes.find("Second") != std::string::npos &&
            debugBytes.find("Third") != std::string::npos,
        "CodeView preserves the source type, field, and primary parameter names (" +
            error + output + ")");

    std::string diagnostic;
    tests.expect(
        rejected(root / "duplicate", R"NS(
struct Duplicate(public int Value, public int Value) { }
)NS", "duplicate promoted field 'Value'", diagnostic),
        "duplicate promoted parameter names are rejected at their declaration (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "shadow", R"NS(
struct Shadow(public int Value) { public int Value; }
)NS", "member name 'Value' conflicts with an existing member", diagnostic),
        "a written member cannot shadow a promoted field (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "default_order", R"NS(
struct DefaultOrder(public int First = 1, public int Second) { }
)NS", "required parameter 'Second' cannot follow a parameter with a default value", diagnostic),
        "a required primary parameter cannot follow a defaulted one (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "protected", R"NS(
struct ProtectedField(protected int Value) { }
)NS", "cannot be protected because structs are not inheritable", diagnostic),
        "promoted struct fields reject an inheritance-only accessibility (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "private_access", R"NS(
struct Hidden(int Value) { }
class Program { public static int Read(Hidden value) { return value.Value; } }
)NS", "not accessible", diagnostic),
        "plain struct promoted fields are private by default (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "readonly_write", R"NS(
struct Immutable(public readonly int Value)
{
    public void Rewrite() { Value = 42; }
}
)NS", "left side of assignment is not assignable", diagnostic),
        "a promoted readonly field is writable only by its synthesized constructor (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "ownership", R"NS(
struct Owner(public owned string Value) { }
)NS", "partial-drop contract", diagnostic),
        "promoted ownership remains explicit and deferred instead of creating a hidden capture (" +
            diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
