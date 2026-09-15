#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
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

void expectSyntaxRejected(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view declaration,
    std::string_view diagnostic)
{
    const auto source = root / (std::string(name) + ".ns");
    writeFile(source, "struct Bits { " + std::string(declaration) + " }");
    const std::string sourcePath = source.string();
    const std::array<std::string_view, 2> arguments = {"--dump-ast", sourcePath};
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
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item327_bitfield_syntax_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto valid = root / "valid";
    writeProject(valid, "BitFieldSyntaxItem327", R"NS(
public struct Control
{
    public uint Enabled : 1;
    public volatile uint Mode : (1 + 2);
    public uint : 4;
    public uint : 0;
    public int Tail;
}
)NS");
    const std::string project = (valid / "BitFieldSyntaxItem327.nsproj").string();
    const std::string source = (valid / "Main.ns").string();
    std::string output;
    std::string error;

    const std::array<std::string_view, 2> syntaxArguments = {"--dump-ast", source};
    const bool syntaxAccepted = runDriver(syntaxArguments, output, error) == ExitCode::Success;
    tests.expect(syntaxAccepted, "named and unnamed bit-field grammar parses (" + error + ")");
    if (syntaxAccepted)
    {
        tests.expect(output.find("FieldDeclaration \"Enabled\" aux=\"bitfield\"") !=
                         std::string::npos,
            "a named bit-field retains an explicit AST marker");
        tests.expect(output.find("second:") != std::string::npos,
            "bit width expressions occupy a separate AST edge from field initializers");
        tests.expect(output.find("__nsc_unnamed_bitfield_f") != std::string::npos &&
                         output.find("bitfield-unnamed") != std::string::npos,
            "unnamed and zero-width forms receive deterministic internal identities");
    }

    const std::array<std::string_view, 2> memberArguments = {"--dump-project-members", project};
    output.clear();
    error.clear();
    const bool membersAccepted = runDriver(memberArguments, output, error) == ExitCode::Success;
    tests.expect(membersAccepted, "project member collection preserves bit-field syntax (" + error + ")");
    if (membersAccepted)
    {
        tests.expect(output.find("field Enabled type=builtin:uint") != std::string::npos &&
                         output.find("bit-field=true unnamed=false width-syntax=present") !=
                             std::string::npos,
            "named widths survive into the project member model");
        tests.expect(output.find("bit-field=true unnamed=true width-syntax=present") !=
                         std::string::npos,
            "unnamed widths survive without becoming a source-visible member");
        tests.expect(output.find("field Tail type=builtin:int") != std::string::npos &&
                         output.find("field Tail type=builtin:int ownership=none") !=
                             std::string::npos,
            "ordinary fields following bit-fields retain their existing syntax");
    }

    const std::array<std::string_view, 2> layoutArguments = {"--dump-project-layouts", project};
    output.clear();
    error.clear();
    tests.expect(runDriver(layoutArguments, output, error) != ExitCode::Success &&
                     error.find("bit-field physical layout belongs to item 328") != std::string::npos,
        "physical placement fails at the explicit item-328 boundary (" + error + ")");

    expectSyntaxRejected(tests, root, "Static", "public static uint Value : 1;",
        "a bit-field accepts accessibility and optional volatile only");
    expectSyntaxRejected(tests, root, "Readonly", "public readonly uint Value : 1;",
        "a bit-field accepts accessibility and optional volatile only");
    expectSyntaxRejected(tests, root, "UnnamedAlign", "public align(4) uint : 0;",
        "an unnamed bit-field accepts accessibility and optional volatile only");
    expectSyntaxRejected(tests, root, "Initializer", "public uint Value : 3 = 1;",
        "bit-field width cannot contain an assignment or initializer");

    return tests.resultKeeping(root);
}
