#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Language/Language3Surface.h"
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
#include <variant>

namespace
{
void writeText(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

bool hasDeferredDiagnostic(std::string_view source, std::string_view feature)
{
    nativesharp::frontend::FrontendCompilation compilation(
        nativesharp::language::LanguageEdition::V3);
    if (compilation.compileBuffer("t49-negative.ns", std::string(source), false)) return false;
    for (const nativesharp::diagnostics::Diagnostic& diagnostic :
         compilation.diagnostics().diagnostics())
    {
        if (diagnostic.id() ==
                nativesharp::diagnostics::DiagnosticId::NS3049DeferredLanguageSurface &&
            diagnostic.message().find(feature) != std::string::npos)
            return true;
    }
    return false;
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t49_language_surface";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);

    const std::string manifest = nativesharp::language::language3SurfaceManifest();
    tests.expect(manifest.starts_with("NSLANGSURFACE 3.0\n") &&
            manifest.find("target=x86_64-pc-windows-msvc") != std::string::npos &&
            manifest.find("raw-string-literals=implemented") != std::string::npos &&
            manifest.find("record-structs=implemented") != std::string::npos &&
            manifest.find("primary-constructors=implemented") != std::string::npos &&
            manifest.find("extension-methods=implemented") != std::string::npos &&
            manifest.find("trait-operators=implemented") != std::string::npos,
        "the frozen 3.0 manifest publishes the productive language surface");
    constexpr std::array deferred = {
        "async-await=deferred-4.0", "async-streams=deferred-4.0",
        "yield-generators=deferred-4.0", "default-interface-implementations=deferred-4.0",
        "list-patterns=deferred-4.0", "static-constructors=deferred-4.0",
        "decimal=deferred-4.0",
        "condition-declarations=deferred-4.0", "static-locals=deferred-4.0",
        "long-double=rejected-3.0"};
    for (const std::string_view entry : deferred)
    {
        tests.expect(manifest.find(entry) != std::string::npos,
            "the manifest makes a non-3.0 feature terminal: " + std::string(entry));
    }

    const std::array<std::string_view, 1> manifestArguments = {"--print-language-3-surface"};
    std::string output;
    std::string error;
    tests.expect(runDriver(manifestArguments, output, error) == ExitCode::Success &&
            output == manifest && error.empty(),
        "the compiler prints the exact versioned language-surface manifest");

    tests.expect(hasDeferredDiagnostic(
        "class C { public async int Work() { return 1; } }", "async-await"),
        "async declarations fail with NS3049 and a state-machine rationale");
    tests.expect(hasDeferredDiagnostic(
        "int Work() { return await Ready(); }", "async-await"),
        "await expressions fail with NS3049 and a state-machine rationale");
    tests.expect(hasDeferredDiagnostic(
        "int Work() { yield return 1; }", "yield-generators"),
        "yield statements fail with NS3049 and a generator-frame rationale");
    tests.expect(hasDeferredDiagnostic(
        "interface I { int Work() { return 1; } }", "default-interface-implementations"),
        "default interface methods fail with NS3049 and a dispatch rationale");
    tests.expect(hasDeferredDiagnostic(
        "int Work(int[] value) { switch (0) { case [1]: return 1; default: return 0; } }",
        "list-patterns"),
        "list patterns fail with NS3049 and an exhaustiveness rationale");
    tests.expect(hasDeferredDiagnostic(
        "class C { static C() {} }", "static-constructors"),
        "static constructors fail with NS3049 and an initialization rationale");
    tests.expect(hasDeferredDiagnostic(
        "decimal Work() { return 0; }", "decimal"),
        "decimal fails with NS3049 and an ABI rationale");
    tests.expect(!hasDeferredDiagnostic(
        "extension Text { int Size() { return 0; } }", "extension-methods"),
        "extension blocks are part of the productive NativeSharp 3 language surface");
    tests.expect(hasDeferredDiagnostic(
        "long double Work() { return 0.0; }", "long-double"),
        "long double fails with NS3049 and the Windows x64 ABI rationale");
    tests.expect(hasDeferredDiagnostic(
        "int Work() { if (int value = 1) return value; return 0; }",
        "condition-declarations"),
        "condition declarations fail with NS3049 and a flow rationale");
    tests.expect(!hasDeferredDiagnostic(
        "bool Work(double magnitude, double factor) { return magnitude * factor >= 1.0; }",
        "condition-declarations"),
        "multiplication in a condition is not mistaken for a condition declaration");
    tests.expect(hasDeferredDiagnostic(
        "int Work() { static int value = 1; return value; }", "static-locals"),
        "static locals fail with NS3049 and a lifetime rationale");

    nativesharp::frontend::FrontendCompilation edition2(
        nativesharp::language::LanguageEdition::V2);
    edition2.compileBuffer("raw-v2.ns", "string Value() { return \"\"\"raw\"\"\"; }", false);
    bool rawRequires3 = false;
    for (const nativesharp::diagnostics::Diagnostic& diagnostic : edition2.diagnostics().diagnostics())
    {
        rawRequires3 = rawRequires3 ||
            diagnostic.id() ==
                nativesharp::diagnostics::DiagnosticId::NS3002FeatureRequiresLanguageEdition3;
    }
    tests.expect(rawRequires3, "raw string literals require the explicit Language 3 edition");

    nativesharp::source::SourceManager rawSources;
    const nativesharp::FileId rawFile = rawSources.addBuffer(
        "raw-values.ns", "\"\"\"A \\ path\"\"\" \"\"\"first\nsecond\"\"\"");
    nativesharp::diagnostics::DiagnosticEngine rawDiagnostics;
    nativesharp::lexer::Lexer rawLexer(*rawSources.getFile(rawFile), rawDiagnostics);
    const std::vector<nativesharp::lexer::Token> rawTokens = rawLexer.lexAll();
    tests.expect(!rawDiagnostics.hasErrors() && rawTokens.size() == 3U &&
            rawTokens[0].kind() == nativesharp::lexer::TokenKind::StringLiteral &&
            std::get<std::string>(rawTokens[0].value()) == "A \\ path" &&
            rawTokens[1].kind() == nativesharp::lexer::TokenKind::StringLiteral &&
            std::get<std::string>(rawTokens[1].value()) == "first\nsecond",
        "the raw lexer preserves backslashes and physical newlines exactly");

    const std::filesystem::path project = root / "raw";
    writeText(project / "Raw.nsproj",
        "<Project><PropertyGroup><Name>T49Raw</Name><Language>3</Language>"
        "</PropertyGroup><ItemGroup><Compile Include=\"Main.ns\"/></ItemGroup></Project>");
    writeText(project / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        string single = """A "quote" and \ slash""";
        if (single != "A \"quote\" and \\ slash") return 1;
        string multiline = """first
second""";
        if (multiline != "first\nsecond") return 2;
        string quotes = """"He said """ now"""";
        if (quotes != "He said \"\"\" now") return 3;
        return 0;
    }
}
)NS");
    const std::string projectText = (project / "Raw.nsproj").string();
    const std::string objectText = (project / "Raw.obj").string();
    const std::array<std::string_view, 5> compileArguments = {
        "-O0", "--emit-project-oop-obj", projectText, "-o", objectText};
    output.clear();
    error.clear();
    const bool compiled = runDriver(compileArguments, output, error) == ExitCode::Success;
    tests.expect(compiled && std::filesystem::exists(objectText) &&
            std::filesystem::file_size(objectText) > 0U,
        "raw strings preserve quotes, backslashes and newlines through O0 LLVM/COFF (" +
            error + ")");

    return tests.resultKeeping(root);
}
