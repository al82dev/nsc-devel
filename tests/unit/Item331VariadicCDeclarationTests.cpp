#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

void writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view outputType = {})
{
    writeFile(root / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) + "</Name>" +
        (outputType.empty() ? "" : "<OutputType>" + std::string(outputType) + "</OutputType>") +
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
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

void expectRejected(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view diagnostic,
    bool project)
{
    std::string input;
    std::array<std::string_view, 2> arguments;
    if (project)
    {
        const auto projectRoot = root / std::string(name);
        writeProject(projectRoot, name, source);
        input = (projectRoot / (std::string(name) + ".nsproj")).string();
        arguments = {"--dump-project-members", input};
    }
    else
    {
        const auto sourcePath = root / (std::string(name) + ".ns");
        writeFile(sourcePath, source);
        input = sourcePath.string();
        arguments = {"--dump-ast", input};
    }
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
    const auto root = std::filesystem::path(NSC_BINARY_DIR) /
        "item331_variadic_c_declaration_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    {
        nativesharp::diagnostics::DiagnosticEngine diagnostics;
        nativesharp::source::SourceManager sources;
        const nativesharp::FileId id = sources.addBuffer("ellipsis.ns", ". ... ..");
        nativesharp::lexer::Lexer lexer(*sources.getFile(id), diagnostics);
        const std::vector<nativesharp::lexer::Token> tokens = lexer.lexAll();
        tests.expect(!diagnostics.hasErrors() && tokens.size() == 5U &&
                         tokens[0].kind() == nativesharp::lexer::TokenKind::Dot &&
                         tokens[1].kind() == nativesharp::lexer::TokenKind::Ellipsis &&
                         tokens[2].kind() == nativesharp::lexer::TokenKind::Dot &&
                         tokens[3].kind() == nativesharp::lexer::TokenKind::Dot,
            "ellipsis uses maximal munch without changing one- or two-dot tokenization");
    }

    const auto syntaxPath = root / "syntax.ns";
    writeFile(syntaxPath, "extern C { int Printf(char* format, ...); }");
    const std::string syntax = syntaxPath.string();
    const std::array<std::string_view, 2> syntaxArguments = {"--dump-ast", syntax};
    std::string output;
    std::string error;
    const bool syntaxAccepted = runDriver(syntaxArguments, output, error) == ExitCode::Success;
    tests.expect(syntaxAccepted, "terminal C ellipsis parses (" + error + ")");
    tests.expect(syntaxAccepted &&
                     output.find("FunctionDeclaration \"Printf\"") != std::string::npos &&
                     output.find("variadic=true") != std::string::npos,
        "AST preserves variadic independently from fixed parameters");

    const auto producer = root / "producer";
    writeProject(producer, "VariadicProducer", R"NS(
public class Native
{
    extern C from "ucrtbase.dll"
    {
        public int Printf(char* format, ...);
    }
}
)NS", "StaticLibrary");
    const std::string producerProject = (producer / "VariadicProducer.nsproj").string();

    const std::array<std::string_view, 2> memberArguments = {
        "--dump-project-members", producerProject};
    output.clear();
    error.clear();
    const bool membersAccepted = runDriver(memberArguments, output, error) == ExitCode::Success;
    tests.expect(membersAccepted, "variadic imported C member is accepted (" + error + ")");
    tests.expect(membersAccepted && output.find("signature=Printf(builtin:char*,...)") !=
                         std::string::npos &&
                     output.find("variadic=true") != std::string::npos,
        "member model makes ellipsis part of callable identity");

    const std::array<std::string_view, 2> hirArguments = {
        "--dump-project-oop-hir", producerProject};
    output.clear();
    error.clear();
    const bool hirAccepted = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hirAccepted && output.find("Printf") != std::string::npos &&
                     output.find(" variadic") != std::string::npos,
        "HIR preserves a body-less variadic import declaration (" + error + ")");

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", producerProject, "-O0"};
    output.clear();
    error.clear();
    const bool llvmAccepted = runDriver(llvmArguments, output, error) == ExitCode::Success;
    tests.expect(llvmAccepted && output.find("declare i32 @Printf(ptr, ...)") != std::string::npos,
        "LLVM declaration retains fixed parameters followed by varargs (" + error + ")");

    const std::string producerLibrary = (producer / "VariadicProducer.lib").string();
    const std::array<std::string_view, 5> emitArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    output.clear();
    error.clear();
    const bool emitted = runDriver(emitArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "variadic declaration producer emits NSMETA 1.19 (" + error + ")");

    nativesharp::interop::LibraryMetadata metadata;
    nativesharp::interop::LibraryMetadataCodec codec;
    std::string metadataError;
    const auto metadataPath = producer / "VariadicProducer.nsmeta";
    const bool metadataRead = emitted && codec.read(metadataPath, metadata, metadataError);
    bool foundVariadic = false;
    if (metadataRead)
    {
        for (const auto& member : metadata.members)
        {
            foundVariadic = foundVariadic ||
                (member.name == "Printf" &&
                 (member.flags & nativesharp::interop::kMetadataMemberVariadic) != 0U);
        }
    }
    tests.expect(metadataRead && metadata.formatMinor == 19U && foundVariadic,
        "NSMETA 1.19 preserves the variadic member flag (" + metadataError + ")");

    nativesharp::interop::LibraryMetadata downlevel = metadata;
    downlevel.formatMinor = 18U;
    metadataError.clear();
    tests.expect(metadataRead &&
                     !codec.writeModel(downlevel, producer / "unsupported-1.18.nsmeta", metadataError) &&
                     metadataError.find("before 1.19 cannot represent a variadic C declaration") !=
                         std::string::npos,
        "NSMETA refuses a lossy pre-1.19 variadic downlevel (" + metadataError + ")");

    const auto consumer = root / "consumer";
    writeFile(consumer / "VariadicConsumer.nsproj",
        "<Project><PropertyGroup><Name>VariadicConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + producerLibrary + "\" Metadata=\"" +
        metadataPath.string() + "\" /></ItemGroup></Project>");
    writeFile(consumer / "Main.ns", "public class Program { public static int Main() { return 0; } }");
    const std::string consumerProject = (consumer / "VariadicConsumer.nsproj").string();
    const std::array<std::string_view, 2> consumerArguments = {
        "--dump-project-members", consumerProject};
    output.clear();
    error.clear();
    const bool consumerAccepted = runDriver(consumerArguments, output, error) == ExitCode::Success;
    tests.expect(consumerAccepted && output.find("signature=Printf(builtin:char*,...)") !=
                         std::string::npos &&
                     output.find("variadic=true") != std::string::npos,
        "LibraryReference reconstructs the variadic C declaration (" + error + ")");

    expectRejected(tests, root, "NoFixed", "extern C { int Bad(...); }",
        "requires at least one fixed parameter", false);
    expectRejected(tests, root, "NotTerminal", "extern C { int Bad(int first, ..., int last); }",
        "expected ')'", false);
    expectRejected(tests, root, "ConflictingImport", R"NS(
public class Native
{
    extern C from "same.dll" { public int Fn(int value, ...); }
    extern C from "same.dll" { public int Fn(int value); }
}
)NS", "conflicting declarations of imported C symbol", true);

    const auto callBoundary = root / "call-boundary";
    writeProject(callBoundary, "VariadicCallBoundary", R"NS(
public class Native
{
    extern C from "ucrtbase.dll" { public int Printf(char* format, ...); }
}
public class Program
{
    public static int Main() { return Native.Printf(null); }
}
)NS");
    const std::string callProject = (callBoundary / "VariadicCallBoundary.nsproj").string();
    const std::array<std::string_view, 2> callArguments = {
        "--dump-project-oop-hir", callProject};
    output.clear();
    error.clear();
    tests.expect(runDriver(callArguments, output, error) == ExitCode::Success &&
                     output.find("static-call type=builtin:int") != std::string::npos,
        "item 332 consumes the variadic declaration established by item 331 (" + error + ")");

    return tests.resultKeeping(root);
}
