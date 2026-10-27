#include "Internal/DriverInternal.h"

#include "nativesharp/Compilation/ImplicitProjectRegistry.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/LLVM/LlvmModuleInspection.h"
#include "nativesharp/LLVM/ProjectOopLlvmLoweringRequest.h"
#include "nativesharp/Source/SourceFile.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <system_error>

// EN: Freeze 3.0 C-05. The file actions of the driver (`nsc <file.ns>`, `--check`, the file dumps
//     and the file code generation) used to run the 1.x single-file pipeline, which accepts a
//     different language than the 3.x project pipeline. A lone `.ns` now compiles as its implicit
//     project: an in-memory `.nsproj` beside the file that compiles only that file and references
//     NativeSharp.Core. Each file action maps to the project action that answers the same
//     question. DEC-05: the CFG, dominance, SSA, optimization and pass-pipeline actions inspect the
//     LLVM that the 3.x pipeline generates for that project (LlvmModuleInspector).
// ES: Congelamiento 3.0 C-05. Las acciones de archivo del driver (`nsc <file.ns>`, `--check`, los
//     dumps de archivo y la generacion de codigo de archivo) corrian el pipeline 1.x de un solo
//     archivo, que acepta un lenguaje distinto al del pipeline 3.x de proyectos. Un `.ns` suelto
//     ahora compila como su proyecto implicito: un `.nsproj` en memoria junto al archivo, que
//     compila solo ese archivo y referencia NativeSharp.Core. Cada accion de archivo corresponde a
//     la accion de proyecto que responde la misma pregunta. DEC-05: las acciones de CFG, dominancia,
//     SSA, optimizacion y pass-pipeline inspeccionan el LLVM que el pipeline 3.x genera para ese
//     proyecto (LlvmModuleInspector).
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

// EN: Freeze 3.0 DEC-05. The CFG, dominance and SSA actions of a source file read the unoptimized
//     LLVM of its implicit project; -O does not change them. The module is lowered for the
//     inspection only and is never used to generate code.
// ES: Congelamiento 3.0 DEC-05. Las acciones de CFG, dominancia y SSA de un archivo fuente leen el
//     LLVM sin optimizar de su proyecto implicito; -O no las cambia. El modulo se baja solo para la
//     inspeccion y nunca se usa para generar codigo.
bool isLlvmGraphInspection(std::string_view action) noexcept
{
    return action == "--dump-cfg" || action == "--verify-cfg" || action == "--dump-dominance" ||
        action == "--verify-dominance" || action == "--dump-ssa" || action == "--verify-ssa";
}

ExitCode inspectSourceFileLlvm(std::string_view action, std::string_view path, std::ostream& output,
                               std::ostream& error)
{
    std::unique_ptr<llvm_backend::LlvmModule> module;
    if (!lowerSourceFileModule(path, false, module, error)) return ExitCode::CompilationError;
    using Inspector = llvm_backend::LlvmModuleInspector;
    std::string inspectionError;
    bool inspected = false;
    if (action == "--dump-cfg") inspected = Inspector::dumpControlFlow(*module, output, inspectionError);
    else if (action == "--verify-cfg") inspected = Inspector::verifyControlFlow(*module, output, inspectionError);
    else if (action == "--dump-dominance") inspected = Inspector::dumpDominance(*module, output, inspectionError);
    else if (action == "--verify-dominance")
        inspected = Inspector::verifyDominance(*module, output, inspectionError);
    else if (action == "--dump-ssa") inspected = Inspector::dumpSsa(*module, output, inspectionError);
    else inspected = Inspector::verifySsa(*module, output, inspectionError);
    if (inspected) return ExitCode::Success;
    error << "nsc: internal error: " << action << ": " << inspectionError << '\n';
    return ExitCode::CompilationError;
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
    if (!success || compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
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
    // EN: Freeze 3.0 (plan item 114): the module is lowered with the member model, without debug information, as
    //     the emission lowers it. The value drops of C-11 need that model; without it a destructible struct
    //     failed with "value drop has no address/member model".
    // ES: Congelamiento 3.0 (elemento 114 del plan): el módulo se baja con el modelo de miembros, sin información
    //     de depuración, como lo baja la emisión. Los drops de valor de C-11 necesitan ese modelo; sin él un
    //     struct con destructor fallaba con «value drop has no address/member model».
    llvm_backend::ProjectOopLlvmLowerer lowerer;
    llvmModule = lowerer.lower(llvm_backend::ProjectOopLlvmLoweringRequest::withDebug(
        *compilation.nameLookup(), *compilation.memberModel(), *compilation.oopMirModel(),
        compilation.sourceManager(), llvm_backend::DebugInformationKind::None,
        llvm_backend::LlvmOptimizationLevel::O0));
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
    if (isLlvmGraphInspection(action)) return inspectSourceFileLlvm(action, path, output, error);
    if (action == "--dump-pass-pipeline")
    {
        // EN: DEC-05: the pipeline that optimizes this file at the requested level.
        // ES: DEC-05: el pipeline que optimiza este archivo en el nivel pedido.
        std::unique_ptr<llvm_backend::LlvmModule> module;
        if (!lowerSourceFileModule(path, false, module, error)) return ExitCode::CompilationError;
        output << "pass.pipeline " << llvm_backend::kLlvmPassPipelineFormat << '\n';
        printLlvmPipeline(optimizationLevel, output);
        return ExitCode::Success;
    }
    // EN: DEC-05: --dump-opt and --verify-opt are the optimized LLVM of the implicit project, the
    //     same module --dump-llvm-opt and --verify-llvm-opt produce, under a versioned header.
    // ES: DEC-05: --dump-opt y --verify-opt son el LLVM optimizado del proyecto implicito, el mismo
    //     modulo que producen --dump-llvm-opt y --verify-llvm-opt, con un encabezado versionado.
    const bool optimizedInspection = action == "--dump-opt" || action == "--verify-opt";
    const std::string_view projectAction = optimizedInspection
        ? (action == "--dump-opt" ? std::string_view("--dump-project-oop-llvm-opt")
                                  : std::string_view("--verify-project-oop-llvm-opt"))
        : projectActionFor(action);
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
    if (!optimizedInspection) return runProject(projectAction, projectText, output, error, optimizationLevel);
    std::ostringstream optimized;
    const ExitCode code = runProject(projectAction, projectText, optimized, error, optimizationLevel);
    if (code != ExitCode::Success) return code;
    if (action == "--dump-opt")
        output << "; optimized.ssa.module " << llvm_backend::kLlvmOptimizedFormat << ' '
               << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n';
    else
        output << "Optimization verification: ok\n"
               << "format: " << llvm_backend::kLlvmOptimizedFormat << '\n';
    output << optimized.str();
    return ExitCode::Success;
}
}
