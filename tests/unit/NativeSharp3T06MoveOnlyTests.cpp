#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/LibraryMetadata.h"

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
    int edition = 3,
    std::string_view outputType = "Exe",
    std::string_view extraItems = {})
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(
        project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>" + std::to_string(edition) + "</Language><OutputType>" +
        std::string(outputType) + "</OutputType></PropertyGroup><ItemGroup>" +
        "<Compile Include=\"**/*.ns\" />" + std::string(extraItems) +
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
    int edition = 3)
{
    ProjectCompilation compilation;
    std::string error;
    std::string projectName(name);
    for (char& character : projectName)
        if (character == '-') character = '_';
    const bool compiled = compilation.compileProjectOopMir(
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t06_move_only";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    const std::string positiveSource = R"NS(
public copy struct Pair
{
    public int left;
    public int right;
    public Pair(int a, int b) { left = a; right = b; }
}
public struct Token
{
    public int value;
    public Token(int source) { value = source; }
}
public class Program
{
    public static Token Relay(Token value) { return move(value); }
    public static int Sum(Pair value) { return value.left + value.right; }
    public static int Main()
    {
        Pair pair = new Pair(20, 20);
        Pair copied = pair;
        Token first = new Token(Sum(copied));
        Token second = Relay(move(first));
        Token result = move(second);
        second = new Token(2);
        return result.value + second.value;
    }
}
)NS";
    const std::filesystem::path positive =
        writeProject(root / "positive", "MoveOnlyPositive", positiveSource);
    std::string diagnostic;
    tests.expect(compileAndRun(positive, 42, diagnostic),
        "copy values copy while move-only locals/parameters/returns execute through O0 and NSLink (" +
            diagnostic + ")");

    std::string observed;
    tests.expect(rejectsWith(root, "local-copy", R"NS(
public struct Token { public int value; public Token(int source) { value = source; } }
public class Program { public static int Main() { Token a = new Token(1); Token b = a; return b.value; } }
)NS", "cannot be copied into local", observed),
        "an addressable move-only local cannot be copied into another local (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "use-after-move", R"NS(
public struct Token { public int value; public Token(int source) { value = source; } }
public class Program { public static int Main() { Token a = new Token(1); Token b = move(a); return a.value; } }
)NS", "use of moved-from move-only local", observed),
        "use-after-move is rejected for ordinary edition-3 value locals (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "parameter-copy", R"NS(
public struct Token { public int value; public Token(int source) { value = source; } }
public class Program {
    public static int Read(Token value) { return value.value; }
    public static int Main() { Token token = new Token(42); return Read(token); }
}
)NS", "move-only argument cannot be copied", observed),
        "a by-value parameter consumes a named move-only argument explicitly (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "namespace-parameter-copy", R"NS(
public struct Token { public int value; public Token(int source) { value = source; } }
namespace Values { public int Read(Token value) { return value.value; } }
public class Program { public static int Main() { Token token = new Token(42); return Values.Read(token); } }
)NS", "move-only argument cannot be copied", observed),
        "free functions enforce the same by-value move contract as methods (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "constructor-parameter-copy", R"NS(
public struct Token { public int value; public Token(int source) { value = source; } }
public struct Box { public int value; public Box(Token token) { value = token.value; } }
public class Program { public static int Main() { Token token = new Token(42); Box box = new Box(token); return box.value; } }
)NS", "move-only value cannot be copied into constructor parameter", observed),
        "constructors enforce explicit movement of named move-only arguments (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "return-copy", R"NS(
public struct Token { public int value; public Token(int source) { value = source; } }
public class Program { public static Token Make() { Token token = new Token(42); return token; } public static int Main() { return 0; } }
)NS", "requires return move(value)", observed),
        "returning a named move-only local requires an explicit move (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "branch-move", R"NS(
public struct Token { public int value; public Token(int source) { value = source; } }
public class Program { public static int Main() { Token token = new Token(42); if (token.value == 42) { Token other = move(token); } return token.value; } }
)NS", "possibly moved", observed),
        "branch joins preserve possibly-moved state (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "loop-move", R"NS(
public struct Token { public int value; public Token(int source) { value = source; } }
public class Program { public static int Main() { Token token = new Token(42); bool again = true; while (again) { Token other = move(token); again = false; } return token.value; } }
)NS", "possibly moved", observed),
        "loop joins preserve possibly-moved state (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "invalid-copy-field", R"NS(
public struct Token { public int value; }
public copy struct Invalid { public Token token; }
public class Program { public static int Main() { return 0; } }
)NS", "contains non-Copy field", observed),
        "copy declarations fail closed when a field is move-only (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "fixed-array-copy", R"NS(
public struct Token { public int value; public Token(int source) { value = source; } }
public class Program { public static int Main() { Token[2] a = { new Token(1), new Token(2) }; Token[2] b = a; return b[0].value; } }
)NS", "cannot be copied into local", observed),
        "fixed arrays inherit the element Copy capability (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "generic-copy", R"NS(
public struct Token { public int value; public Token(int source) { value = source; } }
public class Program {
    public static int Accept<T>(T value) where T : copy { return 42; }
    public static int Main() { return Accept<Token>(new Token(1)); }
}
)NS", "requires copyable storage", observed),
        "where T : copy rejects a move-only edition-3 value even when passed as a fresh temporary (" +
            observed + ")");

    const std::filesystem::path legacy = writeProject(root / "legacy", "LegacyCopy", R"NS(
public struct Pair { public int left; public int right; }
public class Program { public static int Main() { Pair a; a.left = 20; a.right = 22; Pair b = a; return b.left + b.right; } }
)NS", 2);
    ProjectCompilation legacyCompilation;
    std::string legacyError;
    const bool legacyCompiled = legacyCompilation.compileProjectOopMir(legacy, legacyError);
    tests.expect(legacyCompiled && !legacyCompilation.diagnostics().hasErrors(),
        "edition 2 preserves structural implicit-copy compatibility (" + legacyError +
            diagnosticsText(legacyCompilation) + ")");

    const std::filesystem::path producerDirectory = root / "cross-module" / "Producer";
    const std::filesystem::path producer = writeProject(
        producerDirectory, "CopyApi", R"NS(
namespace CopyApi
{
    public copy struct Point
    {
        public int x;
        public int y;
        public Point(int left, int right) { x = left; y = right; }
    }
}
)NS", 3, "StaticLibrary");
    const std::string producerProject = producer.string();
    const std::string library = (producerDirectory / "CopyApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProject, "-O0", "-o", library};
    std::string producerOutput;
    std::string producerError;
    const ExitCode producerResult = runDriver(
        producerArguments, producerOutput, producerError);
    nativesharp::interop::LibraryMetadata metadata;
    std::string readError;
    const bool readMetadata = producerResult == ExitCode::Success &&
        nativesharp::interop::LibraryMetadataCodec{}.read(
            producerDirectory / "CopyApi.nsmeta", metadata, readError);
    bool publishedCopy = false;
    for (const auto& type : metadata.types)
        if (type.fullyQualifiedName == "CopyApi.Point" &&
            (type.flags & nativesharp::interop::kMetadataTypeCopy) != 0U)
            publishedCopy = true;
    tests.expect(
        readMetadata &&
            metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor && publishedCopy,
        "the current NSMETA format preserves the 1.33 Copy capability (" + producerError +
            producerOutput + readError + ")");

    const std::filesystem::path consumer = writeProject(
        root / "cross-module" / "Consumer", "CopyConsumer", R"NS(
using CopyApi;
public class Program
{
    public static int Main()
    {
        Point original = new Point(20, 22);
        Point copied = original;
        return copied.x + copied.y;
    }
}
)NS", 3, "Exe", "<LibraryReference Include=\"../Producer/CopyApi.lib\" />");
    diagnostic.clear();
    tests.expect(producerResult == ExitCode::Success && compileAndRun(consumer, 42, diagnostic),
        "a consumer reconstructs and executes Copy semantics across a LibraryReference (" +
            diagnostic + ")");

    nativesharp::interop::LibraryMetadata downlevel;
    downlevel.formatMajor = nativesharp::interop::kMetadataFormatMajor;
    downlevel.formatMinor = 32;
    nativesharp::interop::LibraryMetadataType copyType;
    copyType.fullyQualifiedName = "Demo.Point";
    copyType.kind = nativesharp::interop::LibraryMetadataTypeKind::Struct;
    copyType.flags = nativesharp::interop::kMetadataTypeCopy;
    downlevel.types.push_back(copyType);
    std::string metadataError;
    tests.expect(!nativesharp::interop::LibraryMetadataCodec{}.writeModel(
            downlevel, root / "copy-v1.32.nsmeta", metadataError) &&
            metadataError.find("before 1.33") != std::string::npos,
        "NSMETA 1.32 cannot silently erase the Copy capability");

    return tests.resultKeeping(root);
}
