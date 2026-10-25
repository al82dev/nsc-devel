#include "Internal/DriverInternal.h"

#include "nativesharp/Compilation/ImplicitProjectRegistry.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/LLVM/ProjectOopLlvmLoweringRequest.h"
#include "nativesharp/Source/SourceFile.h"

#include <algorithm>
#include <cctype>
#include <system_error>

// EN: Freeze 3.0 C-05. The file actions of the driver (`nsc <file.ns>`, `--check`, the file dumps
//     and the file code generation) used to run the 1.x single-file pipeline, which accepts a
//     different language than the 3.x project pipeline. A lone `.ns` now compiles as its implicit
//     project: an in-memory `.nsproj` beside the file that compiles only that file and references
//     NativeSharp.Core. Each file action maps to the project action that answers the same
//     question; the dumps of 1.x stages with no 3.x counterpart are rejected by name.
// ES: Congelamiento 3.0 C-05. Las acciones de archivo del driver (`nsc <file.ns>`, `--check`, los
//     dumps de archivo y la generacion de codigo de archivo) corrian el pipeline 1.x de un solo
//     archivo, que acepta un lenguaje distinto al del pipeline 3.x de proyectos. Un `.ns` suelto
//     ahora compila como su proyecto implicito: un `.nsproj` en memoria junto al archivo, que
//     compila solo ese archivo y referencia NativeSharp.Core. Cada accion de archivo corresponde a
//     la accion de proyecto que responde la misma pregunta; los dumps de etapas 1.x sin
//     equivalente 3.x se rechazan por nombre.
namespace nativesharp::driver
{
namespace
{
std::filesystem::path compilerExecutablePath()
{
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return {};
        if (length < buffer.size())
        {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2U);
    }
}

// EN: An installation keeps `bin/nsc.exe` and `lib/NativeSharp.Core/`; a build tree keeps
//     `build-*/bin/nsc.exe` under the repository root. The nearest ancestor that holds the Core
//     project wins.
// ES: Una instalacion tiene `bin/nsc.exe` y `lib/NativeSharp.Core/`; un arbol de compilacion tiene
//     `build-*/bin/nsc.exe` bajo la raiz del repositorio. Gana el ancestro mas cercano que contiene
//     el proyecto Core.
std::optional<std::filesystem::path> findCoreProject()
{
    const std::filesystem::path executable = compilerExecutablePath();
    for (std::filesystem::path directory = executable.parent_path(); !directory.empty();
         directory = directory.parent_path())
    {
        const std::filesystem::path candidate = directory / "lib" / "NativeSharp.Core" / "NativeSharp.Core.nsproj";
        std::error_code filesystemError;
        if (std::filesystem::is_regular_file(candidate, filesystemError)) return candidate.lexically_normal();
        if (directory == directory.root_path()) break;
    }
    return std::nullopt;
}

std::string xmlEscaped(std::string_view text)
{
    std::string escaped;
    for (const char character : text)
    {
        switch (character)
        {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&apos;"; break;
            default: escaped += character; break;
        }
    }
    return escaped;
}

// EN: The module takes the file stem, made a valid identifier (`my-tool` becomes `my_tool`).
// ES: El modulo toma el nombre del archivo, vuelto identificador valido (`my-tool` es `my_tool`).
std::string implicitModuleName(const std::filesystem::path& source)
{
    std::string name = source.stem().string();
    for (char& character : name)
    {
        const unsigned char value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '_') character = '_';
    }
    if (name.empty() || std::isdigit(static_cast<unsigned char>(name.front()))) name.insert(0, "_");
    return name;
}

// EN: The project action that answers a file action, or empty when there is none.
// ES: La accion de proyecto que responde una accion de archivo, o vacio si no hay.
std::string_view projectActionFor(std::string_view action) noexcept
{
    if (action == "--check" || action == "--verify-llvm")
        return "--verify-project-oop-llvm";
    if (action == "--dump-symbols") return "--dump-project-names";
    if (action == "--dump-sema") return "--dump-project-method-bodies";
    if (action == "--dump-hir") return "--dump-project-oop-hir";
    if (action == "--verify-hir") return "--check-project-oop-hir";
    if (action == "--dump-mir") return "--dump-project-oop-mir";
    if (action == "--verify-mir") return "--check-project-oop-mir";
    if (action == "--dump-llvm") return "--dump-project-oop-llvm";
    if (action == "--dump-llvm-opt") return "--dump-project-oop-llvm-opt";
    if (action == "--verify-llvm-opt") return "--verify-project-oop-llvm-opt";
    return {};
}

bool sameFile(const std::filesystem::path& left, const std::filesystem::path& right)
{
    std::error_code filesystemError;
    const bool equivalent = std::filesystem::equivalent(left, right, filesystemError);
    return !filesystemError && equivalent;
}

// EN: `--dump-source`, `--dump-tokens` and `--dump-ast` read the syntax unit of the file inside
//     its implicit project: the same lexer and parser (and comptime expansion) a build uses.
// ES: `--dump-source`, `--dump-tokens` y `--dump-ast` leen la unidad sintactica del archivo dentro
//     de su proyecto implicito: el mismo lexer y parser (y expansion comptime) que usa un build.
ExitCode dumpSourceFileSyntax(std::string_view action, std::string_view path, std::ostream& output,
                              std::ostream& error)
{
    std::string loadError;
    const std::optional<std::filesystem::path> project = registerImplicitProject(path, false, loadError);
    if (!project.has_value())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    std::optional<compilation::CompilationInputManifest> manifest =
        loadCompilationInputManifest(*project, loadError);
    if (!manifest.has_value())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    frontend::ProjectCompilation compilation;
    configureProjectExecution(compilation);
    if (!configurePrecompiledModules(*manifest, compilation, loadError))
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    const bool success = compilation.compileProjectSyntax(*project, loadError);
    if (!loadError.empty())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    const std::filesystem::path source = pathFromUtf8(path);
    const frontend::ProjectSyntaxUnit* unit = nullptr;
    for (const std::unique_ptr<frontend::ProjectSyntaxUnit>& candidate : compilation.units())
    {
        const source::SourceFile* file = compilation.sourceManager().getFile(candidate->fileId());
        if (file != nullptr && sameFile(file->path(), source)) unit = candidate.get();
    }
    if (unit != nullptr && action == "--dump-source")
    {
        const source::SourceFile* file = compilation.sourceManager().getFile(unit->fileId());
        output << "file: " << file->displayPath() << '\n'
               << "bytes: " << file->size() << '\n'
               << "lines: " << file->lineCount() << '\n';
        for (std::uint32_t line = 1; line <= file->lineCount(); ++line)
            output << line << ": " << file->lineText(line) << '\n';
    }
    else if (unit != nullptr && action == "--dump-tokens")
    {
        lexer::TokenDumper dumper;
        dumper.dump(unit->tokens(), output);
    }
    else if (unit != nullptr && action == "--dump-ast" && unit->syntaxTree() != nullptr)
    {
        ast::AstDumper dumper;
        dumper.dump(unit->syntaxTree(), output);
    }
    if (compilation.diagnostics().hasErrors())
    {
        diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
        renderer.renderAll(compilation.diagnostics(), error);
    }
    return success && unit != nullptr ? ExitCode::Success : ExitCode::CompilationError;
}

bool isRemovedLegacyAction(std::string_view action) noexcept
{
    return action == "--dump-cfg" || action == "--verify-cfg" || action == "--dump-dominance" ||
        action == "--verify-dominance" || action == "--dump-ssa" || action == "--verify-ssa" ||
        action == "--dump-opt" || action == "--verify-opt" || action == "--dump-pass-pipeline";
}
}

std::optional<std::filesystem::path> registerImplicitProject(
    std::string_view sourcePath, bool executable, std::string& error)
{
    std::error_code filesystemError;
    const std::filesystem::path source = std::filesystem::absolute(pathFromUtf8(sourcePath), filesystemError);
    if (filesystemError || !std::filesystem::is_regular_file(source, filesystemError))
    {
        error = "source file does not exist or is not a regular file: '" + std::string(sourcePath) + "'";
        return std::nullopt;
    }
    if (source.extension() != ".ns")
    {
        error = "a NativeSharp source file must use the .ns extension: '" + std::string(sourcePath) + "'";
        return std::nullopt;
    }
    const std::optional<std::filesystem::path> core = findCoreProject();
    if (!core.has_value())
    {
        error = "the implicit project of a source file references NativeSharp.Core, but no "
                "lib/NativeSharp.Core/NativeSharp.Core.nsproj was found above the compiler";
        return std::nullopt;
    }
    const std::filesystem::path project = source.parent_path() /
        (source.stem().string() + (executable ? ".nsc-exe.nsproj" : ".nsc-lib.nsproj"));
    std::string text = "<Project>\n  <PropertyGroup>\n    <Name>" + implicitModuleName(source) +
        "</Name>\n    <OutputType>" + (executable ? "Exe" : "Library") +
        "</OutputType>\n  </PropertyGroup>\n  <ItemGroup>\n    <Compile Include=\"" +
        xmlEscaped(source.filename().generic_string()) + "\" />\n    <ProjectReference Include=\"" +
        xmlEscaped(core->generic_string()) + "\" />\n  </ItemGroup>\n</Project>\n";
    compilation::ImplicitProjectRegistry::add(project, std::move(text));
    return project;
}

bool lowerSourceFileModule(
    std::string_view sourcePath, bool executable, std::unique_ptr<llvm_backend::LlvmModule>& llvmModule,
    std::ostream& error)
{
    std::string loadError;
    const std::optional<std::filesystem::path> project =
        registerImplicitProject(sourcePath, executable, loadError);
    std::optional<compilation::CompilationInputManifest> manifest;
    if (project.has_value()) manifest = loadCompilationInputManifest(*project, loadError);
    if (!manifest.has_value())
    {
        error << "nsc: error: " << loadError << '\n';
        return false;
    }
    frontend::ProjectCompilation compilation;
    configureProjectExecution(compilation);
    if (!configurePrecompiledModules(*manifest, compilation, loadError))
    {
        error << "nsc: error: " << loadError << '\n';
        return false;
    }
    const bool success = compilation.compileProjectOopMir(*project, loadError);
    if (!loadError.empty()) error << "nsc: error: " << loadError << '\n';
    if (compilation.diagnostics().hasErrors())
    {
        diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
        renderer.renderAll(compilation.diagnostics(), error);
    }
    if (!success || compilation.nameLookup() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: code generation requires a successful compilation\n";
        return false;
    }
    // EN: A program needs exactly one entry point, as the executable emission does.
    // ES: Un programa necesita exactamente un punto de entrada, como la emision del ejecutable.
    bool ambiguousEntryPoint = false;
    if (executable &&
        (compilation.namespaceFunctionModel() == nullptr ||
         compilation.oopMirModel()->executableEntryPoint(*compilation.namespaceFunctionModel(),
                                                         ambiguousEntryPoint) == nullptr ||
         ambiguousEntryPoint))
    {
        error << "nsc: error: " << (ambiguousEntryPoint
            ? "OOP executable has multiple int Main() entry points\n"
            : "OOP executable requires exactly one int Main() function or static method with no parameters\n");
        return false;
    }
    llvm_backend::ProjectOopLlvmLowerer lowerer;
    llvmModule = lowerer.lower(llvm_backend::ProjectOopLlvmLoweringRequest::withoutDebug(
        *compilation.nameLookup(), *compilation.oopMirModel()));
    if (llvmModule == nullptr)
    {
        error << "nsc: error: " << lowerer.error() << '\n';
        return false;
    }
    std::string verificationError;
    if (!llvmModule->verify(verificationError))
    {
        error << "nsc: internal error: LLVM verification failed: " << verificationError << '\n';
        return false;
    }
    return true;
}

ExitCode Driver::runSourceFile(std::string_view action, std::string_view path,
                               llvm_backend::LlvmOptimizationLevel optimizationLevel, std::ostream& output,
                               std::ostream& error)
{
    if (action == "--dump-source" || action == "--dump-tokens" || action == "--dump-ast")
        return dumpSourceFileSyntax(action, path, output, error);
    if (action == "--program")
    {
        std::unique_ptr<llvm_backend::LlvmModule> program;
        if (!lowerSourceFileModule(path, true, program, error)) return ExitCode::CompilationError;
        output << "program check: ok\n";
        return ExitCode::Success;
    }
    if (isRemovedLegacyAction(action))
    {
        error << "nsc: error: '" << action << "' dumped a stage of the NativeSharp 1.x single-file "
                 "pipeline, which the 3.x driver no longer runs; a source file compiles as its implicit "
                 "project, whose stages are shown by --dump-hir, --dump-mir, --dump-llvm and "
                 "--dump-llvm-opt\n";
        return ExitCode::CommandLineError;
    }
    const std::string_view projectAction = projectActionFor(action);
    if (projectAction.empty())
    {
        error << "nsc: error: unknown source-file action '" << action << "'\n";
        return ExitCode::CommandLineError;
    }
    std::string projectError;
    const std::optional<std::filesystem::path> project = registerImplicitProject(path, false, projectError);
    if (!project.has_value())
    {
        error << "nsc: error: " << projectError << '\n';
        return ExitCode::CompilationError;
    }
    const std::string projectText = project->string();
    return runProject(projectAction, projectText, output, error, optimizationLevel);
}
}
