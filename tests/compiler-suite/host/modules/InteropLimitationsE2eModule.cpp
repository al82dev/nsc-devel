// EN: Interop limitations found while porting the tutorials (A:\mejoras-y-limitaciones, L-xxx). Each case copies
//     one fixture of tests/compiler-suite/fixtures/interop-limitations into an isolated directory, instantiates its
//     Project.nsproj.in (@FIXTURE@ is the copy's directory), builds it with the suite's nsc and checks the
//     executable's exit code, or the exact diagnostic for the negative case.
// ES: Limitaciones de interop encontradas al portar los tutoriales (L-xxx). Cada caso copia un fixture de
//     tests/compiler-suite/fixtures/interop-limitations a un directorio aislado, instancia su Project.nsproj.in
//     (@FIXTURE@ es el directorio de la copia), lo compila con el nsc de la suite y comprueba el codigo de salida
//     del ejecutable, o el diagnostico exacto del caso negativo.
#include "ExternalProgramTestSupport.h"
#include "nsc_suite/TestRegistry.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef NSC_SUITE_REPOSITORY_ROOT
#error "NSC_SUITE_REPOSITORY_ROOT is required for interop limitation cases"
#endif
#ifndef NSC_SUITE_NSC_EXECUTABLE
#error "NSC_SUITE_NSC_EXECUTABLE is required for interop limitation cases"
#endif
#ifndef NSC_SUITE_WINDOWS_RUNTIME_LIBRARY
#error "NSC_SUITE_WINDOWS_RUNTIME_LIBRARY is required for interop limitation cases"
#endif
#ifndef NSC_SUITE_CLANG_ROOT
#error "NSC_SUITE_CLANG_ROOT is required for interop limitation cases"
#endif

namespace nsc::suite
{
namespace
{
namespace fs = std::filesystem;
using external_program_support::Result;

// Isolated directory removed with the case. / Directorio aislado que se elimina con el caso.
class IsolatedFixture final
{
public:
    explicit IsolatedFixture(std::string_view caseId)
    {
        path_ = fs::temp_directory_path() / "nsc-component-interop-limitations" /
            (std::string(caseId) + "-" + std::to_string(GetCurrentProcessId()) + "-" +
                std::to_string(GetTickCount64()));
    }

    IsolatedFixture(const IsolatedFixture&) = delete;
    IsolatedFixture& operator=(const IsolatedFixture&) = delete;

    ~IsolatedFixture()
    {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }

    [[nodiscard]] const fs::path& path() const noexcept { return path_; }

private:
    fs::path path_;
};

std::string readFile(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::string describe(std::string_view phase, const Result& result)
{
    return std::string(phase) + " started=" + (result.started ? "true" : "false") +
        " exit=" + std::to_string(result.exitCode) + " error=" + result.error + " output=" + result.output;
}

struct Built
{
    std::string failure;
    Result compilation;
    fs::path executable;
};

// Copies the fixture, writes Project.nsproj and runs `nsc build`. / Copia el fixture, escribe Project.nsproj y
// ejecuta `nsc build`.
Built build(const IsolatedFixture& isolated, std::string_view fixtureName)
{
    Built built;
    const fs::path fixture = fs::path(NSC_SUITE_REPOSITORY_ROOT) /
        "tests/compiler-suite/fixtures/interop-limitations" / fixtureName;
    std::error_code error;
    fs::create_directories(isolated.path().parent_path(), error);
    fs::copy(fixture, isolated.path(), fs::copy_options::recursive | fs::copy_options::overwrite_existing, error);
    if (error) { built.failure = "fixture copy failed: " + error.message(); return built; }
    std::string project = readFile(isolated.path() / "Project.nsproj.in");
    const std::string directory = isolated.path().generic_string();
    for (std::size_t at = project.find("@FIXTURE@"); at != std::string::npos; at = project.find("@FIXTURE@", at))
    {
        project.replace(at, 9, directory);
        at += directory.size();
    }
    const fs::path projectFile = isolated.path() / "Project.nsproj";
    {
        std::ofstream stream(projectFile, std::ios::binary | std::ios::trunc);
        stream << project;
    }
    const std::vector<std::wstring> arguments = {L"build", projectFile.wstring(), L"-O0", L"-g0",
        L"--linker", L"LLD", L"--cpp-clang-root=" + fs::path(NSC_SUITE_CLANG_ROOT).wstring()};
    const std::vector<std::pair<std::wstring, std::wstring>> environment = {
        {L"NSC_WINDOWS_RUNTIME_LIBRARY", fs::path(NSC_SUITE_WINDOWS_RUNTIME_LIBRARY).wstring()}};
    built.compilation = external_program_support::run(
        fs::path(NSC_SUITE_NSC_EXECUTABLE), arguments, {}, isolated.path(), environment);
    built.executable = isolated.path() / "bin/Debug/InteropLimitation.exe";
    return built;
}

// The fixture builds and its executable ends with `expectedExit`. / El fixture compila y su ejecutable termina con
// `expectedExit`.
void expectExit(TestContext& context, std::string_view caseId, std::string_view fixtureName, DWORD expectedExit)
{
    const IsolatedFixture isolated(caseId);
    const Built built = build(isolated, fixtureName);
    std::error_code error;
    const bool ready = built.failure.empty() && built.compilation.started && built.compilation.exitCode == 0 &&
        fs::is_regular_file(built.executable, error) && !error;
    context.expect(ready, std::string(caseId) + " fixture must build (" + built.failure + " " +
        describe("compile", built.compilation) + ")");
    if (!ready) return;
    const Result execution = external_program_support::run(built.executable, {}, {}, isolated.path());
    context.expect(execution.started && execution.exitCode == expectedExit && execution.error.empty(),
        std::string(caseId) + " executable must exit with " + std::to_string(expectedExit) + " (" +
            describe("execute", execution) + ")");
}

// The fixture is rejected with a diagnostic that contains `expected`. / El fixture se rechaza con un diagnostico
// que contiene `expected`.
void expectDiagnostic(TestContext& context, std::string_view caseId, std::string_view fixtureName,
    std::string_view expected)
{
    const IsolatedFixture isolated(caseId);
    const Built built = build(isolated, fixtureName);
    context.expect(built.failure.empty() && built.compilation.started && built.compilation.exitCode != 0 &&
            built.compilation.output.find(expected) != std::string::npos,
        std::string(caseId) + " fixture must be rejected with '" + std::string(expected) + "' (" + built.failure +
            " " + describe("compile", built.compilation) + ")");
}

void addCase(TestRegistry& registry, std::string id, std::vector<std::string> tags, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-INTEROP-051", "Host.Interop", "integration", "PR", std::move(tags),
        std::move(function)});
}
}

void registerInteropLimitationsE2eModule(TestRegistry& registry)
{
    addCase(registry, "TF-INTEROP-401", {"interop", "cpp", "inline", "l-020", "l-026", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-401", "inline-referenced", 42); });
    addCase(registry, "TF-INTEROP-402", {"interop", "cpp", "template", "l-020", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-402", "template-on-demand", 42); });
    addCase(registry, "TF-INTEROP-403", {"interop", "cpp", "reference", "l-020", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-403", "const-ref-place", 42); });
    addCase(registry, "TF-INTEROP-404", {"interop", "cpp", "template", "overload", "l-020", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-404", "partial-ordering", 42); });
    addCase(registry, "TF-INTEROP-405", {"interop", "cpp", "function-pointer", "l-019", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-405", "static-fnptr", 42); });
    addCase(registry, "TF-INTEROP-406", {"interop", "cpp", "sessions", "l-018", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-406", "shared-header", 42); });
    addCase(registry, "TF-INTEROP-407", {"interop", "c", "implementation", "l-016", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-407", "c-single-file", 42); });
    addCase(registry, "TF-INTEROP-408", {"interop", "c", "function-pointer", "l-012", "l-004", "l-015", "l-008", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-408", "c-fnptr-boundary", 33); });
    addCase(registry, "TF-INTEROP-409", {"interop", "c", "namespace-state", "l-022", "l-001", "l-002", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-409", "namespace-constants", 42); });
    addCase(registry, "TF-INTEROP-411", {"interop", "language", "array", "l-027", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-411", "array-count-constant", 42); });
    addCase(registry, "TF-INTEROP-412", {"interop", "language", "default-argument", "l-021", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-412", "default-named-constant", 42); });
    addCase(registry, "TF-INTEROP-413", {"interop", "c", "function-pointer", "cast", "l-006", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-413", "function-pointer-cast", 42); });
    addCase(registry, "TF-INTEROP-414", {"interop", "lexer", "conditional-compilation", "l-011", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-414", "conditional-compilation", 42); });
    addCase(registry, "TF-INTEROP-415", {"interop", "c", "string", "l-003", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-415", "string-to-c-string", 42); });
    addCase(registry, "TF-INTEROP-416", {"interop", "language", "namespace-state", "l-023", "e2e"},
        [](TestContext& context) { expectExit(context, "TF-INTEROP-416", "namespace-dynamic-initializer", 42); });
    addCase(registry, "TF-INTEROP-410", {"interop", "diagnostic", "l-009", "e2e"},
        [](TestContext& context) {
            expectDiagnostic(context, "TF-INTEROP-410", "using-diagnostic",
                "using directives of the file: NativeSharp.Core");
        });
}
}
