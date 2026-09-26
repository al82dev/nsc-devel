#include "nativesharp/Linker/WindowsExecutableBuilder.h"

#include "nativesharp/Linker/WindowsRuntimeImports.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/WindowsRuntimeLibrary.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

#include <array>
#include <chrono>
#include <cstdint>
// <cstdlib> por getenv/_dupenv_s y free -- ver linkTemporaryRootOverride mas abajo.
#include <cstdlib>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace nativesharp::linker
{
namespace
{
// NSL-ACT-001. `NSC_KEEP_LINK_TEMPS` -- la peticion 001 de quien desarrolla NSLink, y lo que
// hace falta para atenderla.
//
// EL PROBLEMA, dicho por ellos y comprobado aqui. Un enlace de ejecutable produce TRES entradas y
// las tres viven en este directorio temporal:
//
//     program.obj    el objeto del programa
//     runtime.obj    el runtime, construido A PARTIR DEL SIMBOLO DE ENTRADA de ESE programa
//     *.lib          las bibliotecas de importacion de ESA invocacion
//
// El destructor de abajo borra las tres al terminar. `corpus/llvm/` de NSLink tiene tres objetos
// de NativeSharp y NINGUNO trae su runtime, asi que sus pruebas no pueden enlazar un ejecutable
// completo por su cuenta: les faltan nueve simbolos (__nativesharp_alloc, memcpy, _fltused...) y
// la unica prueba de que un ejecutable NativeSharp enlaza vive FUERA de su arbol.
//
// QUE LA VARIABLE SEA LA RAIZ Y NO UN BOOLEANO, y esto no es un detalle. Con un booleano habria
// que averiguar DONDE quedo el directorio --el nombre lleva un sello de reloj-- y con dieciseis
// trabajos en paralelo eso es adivinar. Con la variable como raiz, quien llama decide el sitio y
// sabe exactamente donde mirar. El sello sigue puesto para que dos enlaces del mismo trabajo no
// se pisen.
//
// EL EMPAREJAMIENTO ES LO QUE SE ESTABA PIDIENDO. Un `runtime.obj` de un caso y un
// `program.obj` de otro no tienen por que enlazar juntos: el runtime se construye con
// `build(mainSymbol, ...)`. Guardar el directorio entero conserva los tres artefactos de UNA
// invocacion, que es justo lo que la peticion pedia y lo que un fichero suelto no puede dar.
//
// APAGADA POR OMISION. Sin la variable, esto es byte por byte lo de siempre: %TEMP% y borrar.
std::string linkTemporaryRootOverride()
{
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "NSC_KEEP_LINK_TEMPS") != 0 || value == nullptr)
    {
        return {};
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv("NSC_KEEP_LINK_TEMPS");
    return value != nullptr ? std::string(value) : std::string();
#endif
}

class TemporaryDirectory final
{
public:
    TemporaryDirectory(std::filesystem::path path, bool keep) noexcept
        : path_(std::move(path)),
          keep_(keep)
    {
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory()
    {
        if (keep_)
        {
            return;
        }
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
    bool keep_ = false;
};

std::unique_ptr<TemporaryDirectory> createTemporaryDirectory(std::string& error)
{
    error.clear();
    std::error_code filesystemError;

    // Con la variable puesta, la raiz la elige quien llama y el directorio SOBREVIVE. Se crea el
    // arbol si no existe: pedir que exista de antemano solo mueve el fallo de sitio.
    const std::string keepRoot = linkTemporaryRootOverride();
    const bool keep = !keepRoot.empty();
    std::filesystem::path root;
    if (keep)
    {
        root = std::filesystem::path(keepRoot);
        std::filesystem::create_directories(root, filesystemError);
        if (filesystemError)
        {
            error = "could not create the NSC_KEEP_LINK_TEMPS directory '" + keepRoot +
                "': " + filesystemError.message();
            return nullptr;
        }
        filesystemError.clear();
    }
    else
    {
        root = std::filesystem::temp_directory_path(filesystemError);
        if (filesystemError)
        {
            error = "could not resolve the system temporary directory: " + filesystemError.message();
            return nullptr;
        }
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (std::uint32_t attempt = 0; attempt < 100; ++attempt)
    {
        const std::filesystem::path candidate = root /
            ("nativesharp-stage20-" + std::to_string(stamp) + "-" + std::to_string(attempt));
        filesystemError.clear();
        if (std::filesystem::create_directory(candidate, filesystemError))
        {
            return std::make_unique<TemporaryDirectory>(candidate, keep);
        }
        if (filesystemError && filesystemError != std::errc::file_exists)
        {
            error = "could not create a Stage 20 temporary directory: " + filesystemError.message();
            return nullptr;
        }
    }

    error = "could not allocate a unique Stage 20 temporary directory";
    return nullptr;
}

bool verifyObject(const std::filesystem::path& path, std::string_view label, std::string& error)
{
    llvm_backend::CoffObjectVerifier verifier;
    if (!verifier.verify(path))
    {
        error = std::string(label) + " COFF verification failed: " + verifier.error();
        return false;
    }
    return true;
}

std::filesystem::path incrementalStatePath(const std::filesystem::path& outputPath)
{
    std::filesystem::path state = outputPath;
    state += ".nslinc";
    return state;
}

bool appendFileIdentity(
    const std::filesystem::path& path, std::string& keyMaterial, std::string& error)
{
    basic::Sha256Digest digest{};
    std::uintmax_t size = 0;
    if (!basic::sha256File(path, digest, size, error)) return false;
    keyMaterial += path.filename().generic_string() + ":" + std::to_string(size) + ":" +
        basic::sha256Hex(digest) + "\n";
    return true;
}

std::string incrementalLinkKey(
    std::span<const std::filesystem::path> programObjects,
    std::string_view programLlvmText,
    std::string_view runtimeLlvmText,
    std::span<const std::filesystem::path> extraLibraries,
    std::span<const ExternalImportModule> externalImports,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    llvm_backend::DebugInformationKind debugInformation,
    WindowsExecutableSubsystem subsystem,
    std::string_view mainSymbol,
    std::string& error)
{
    std::string material = "NativeSharp.IncrementalLink.v1\n";
    material += std::to_string(static_cast<unsigned>(optimizationLevel)) + ":" +
        std::to_string(static_cast<unsigned>(debugInformation)) + ":" +
        std::to_string(static_cast<unsigned>(subsystem)) + ":" + std::string(mainSymbol) + "\n";
    material.append(programLlvmText);
    material.push_back('\n');
    material.append(runtimeLlvmText);
    material.push_back('\n');
    for (const auto& module : externalImports)
    {
        material += module.dllName + "\n";
        for (const std::string& symbol : module.symbols) material += symbol + "\n";
    }
    for (const std::filesystem::path& object : programObjects)
        if (!appendFileIdentity(object, material, error)) return {};
    for (const std::filesystem::path& library : extraLibraries)
        if (!appendFileIdentity(library, material, error)) return {};
    const auto bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size());
    return basic::sha256Hex(basic::sha256(bytes));
}

std::string readIncrementalState(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::string value;
    if (file) std::getline(file, value);
    return value;
}

bool writeIncrementalState(
    const std::filesystem::path& path, std::string_view key, std::string& error)
{
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            error = "could not write incremental link state: " + temporary.string();
            return false;
        }
        file << key << '\n';
        if (!file)
        {
            error = "could not finish incremental link state: " + temporary.string();
            return false;
        }
    }
    std::error_code filesystemError;
    std::filesystem::remove(path, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(temporary, path, filesystemError);
    if (filesystemError)
    {
        error = "could not publish incremental link state: " + filesystemError.message();
        return false;
    }
    return true;
}
}

bool WindowsExecutableBuilder::build(
    llvm_backend::LlvmModule& programModule,
    const std::filesystem::path& outputPath,
    std::string& error)
{
    return build(programModule, outputPath, llvm_backend::LlvmOptimizationLevel::O0, error);
}

bool WindowsExecutableBuilder::build(
    llvm_backend::LlvmModule& programModule,
    const std::filesystem::path& outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::string& error)
{
    return build(programModule, outputPath, optimizationLevel, "Main", error);
}

bool WindowsExecutableBuilder::build(
    llvm_backend::LlvmModule& programModule,
    const std::filesystem::path& outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::string_view mainSymbol,
    std::string& error)
{
    info_ = {};
    error.clear();
    if (outputPath.empty())
    {
        error = "Windows executable output path is empty";
        return false;
    }
    if (mainSymbol.empty())
    {
        error = "Windows executable entry symbol is empty";
        return false;
    }

    const std::filesystem::path parent = outputPath.parent_path();
    if (!parent.empty() && !std::filesystem::is_directory(parent))
    {
        error = "Windows executable output directory does not exist: " + parent.string();
        return false;
    }

    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
        llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
    if (targetMachine == nullptr)
    {
        error = "could not create the Windows x64 TargetMachine: " + targetError;
        return false;
    }

    const bool independentRuntime = !runtimeLibrary_.empty();
    if (independentRuntime && !verifyWindowsRuntimeLibrary(runtimeLibrary_, error))
    {
        return false;
    }
    runtime::WindowsRuntimeBuilder runtimeBuilder;
    std::string runtimeError;
    std::unique_ptr<llvm_backend::LlvmModule> runtimeModule = independentRuntime
        ? runtimeBuilder.buildEntry(mainSymbol, runtimeError)
        : runtimeBuilder.build(mainSymbol, runtimeError);
    if (runtimeModule == nullptr)
    {
        error = "could not construct the NativeSharp runtime: " + runtimeError;
        return false;
    }

    // NSC-LIMIT-005. La observacion va SIEMPRE, no bajo bandera. Un fallo del optimizador que no
    // dice contra que LLVM, con que pipeline ni por donde iba obliga a REPRODUCIRLO para poder
    // diagnosticarlo, y eso es exactamente lo que no se pudo hacer con `c38_enum_ancho`: cuando el
    // sintoma es un codigo de salida distinto a -O2, la corrida que fallo ya termino. Ver
    // LlvmOptimizer.h y LlvmOptimizationObservation.h.
    //
    // COSTE MEDIDO: ~3% del tiempo de compilacion completo a -O2 y 0% a -O0, y el IR resultante es
    // byte a byte el mismo con y sin observar --hay una prueba que lo compara en los tres
    // niveles--. Se paga en cada construccion a cambio de que el PRIMER fallo sea legible.
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation programObservation;
    std::string optimizationError;
    if (!optimizer.optimize(
            programModule, *targetMachine, optimizationLevel, &programObservation,
            optimizationError))
    {
        error = "program LLVM optimization failed: " + optimizationError;
        return false;
    }
    optimizationError.clear();
    // NSC-LIMIT-005: observada, igual que la de arriba.
    llvm_backend::LlvmOptimizationObservation runtimeObservation;
    if (!optimizer.optimize(
            *runtimeModule, *targetMachine, optimizationLevel, &runtimeObservation,
            optimizationError))
    {
        error = "runtime LLVM optimization failed: " + optimizationError;
        return false;
    }

    std::string temporaryError;
    std::unique_ptr<TemporaryDirectory> temporaryDirectory =
        createTemporaryDirectory(temporaryError);
    if (temporaryDirectory == nullptr)
    {
        error = temporaryError;
        return false;
    }

    const std::filesystem::path programObject = temporaryDirectory->path() / "program.obj";
    const std::filesystem::path runtimeObject = temporaryDirectory->path() / "runtime.obj";
    std::string emissionError;
    if (!targetMachine->emit(
            programModule,
            programObject,
            llvm_backend::LlvmCodeGenFileType::Object,
            emissionError))
    {
        error = "program object emission failed: " + emissionError;
        return false;
    }
    if (!verifyObject(programObject, "program", error))
    {
        return false;
    }

    emissionError.clear();
    if (!targetMachine->emit(
            *runtimeModule,
            runtimeObject,
            llvm_backend::LlvmCodeGenFileType::Object,
            emissionError))
    {
        error = "runtime object emission failed: " + emissionError;
        return false;
    }
    if (!verifyObject(runtimeObject, "runtime", error))
    {
        return false;
    }

    // Stage 53.2b bug fix: this scan MUST cover both modules that actually get linked into the
    // final executable, not just the user's program module. Before this fix, only programText
    // was scanned -- which missed the case (introduced by Environment.GetEnvironmentVariable/
    // SetEnvironmentVariable's hand-rolled byte-copy loop in WindowsRuntimeBuilder.cpp) where
    // LLVM's own LoopIdiomRecognize optimization pass rewrites a plain copy loop into a call to
    // the @llvm.memcpy intrinsic *inside the runtime module*, at any optimization level above
    // O0. That intrinsic call lowers to a real "memcpy" symbol reference in runtime.obj, and
    // without this fix nothing ever told the linker to import it -- "undefined symbol: memcpy"
    // at link time, for EVERY executable (not just ones that call GetEnvironmentVariable),
    // because every executable links against the same runtime module. Concatenating both
    // modules' IR text before scanning is the minimal, honest fix: it makes no assumption about
    // which module a future runtime helper's copy/fill loop might get idiom-recognized in.
    const std::string programText = programModule.text() + runtimeModule->text();

    std::string linkerError;
    std::unique_ptr<LldLinker> linker = LldLinker::discover(linkerError);
    if (linker == nullptr)
    {
        error = linkerError;
        return false;
    }

    // Import libraries -- runtime surface plus the project's external `extern C from` imports
    // -- come from the ONE shared table (see WindowsRuntimeImports.h for the history of why a
    // single source of truth replaced the duplicated arrays that used to live right here).
    std::vector<std::filesystem::path> importLibraries;
    if (!createWindowsImportLibraries(
            *linker, temporaryDirectory->path(), programText, externalImports_, importLibraries, error))
    {
        return false;
    }
    for (const std::filesystem::path& extra : extraLinkLibraries_)
    {
        importLibraries.push_back(extra);
    }
    if (independentRuntime)
    {
        importLibraries.push_back(runtimeLibrary_);
    }

    const std::array<std::filesystem::path, 2> objectFiles = {programObject, runtimeObject};
    std::filesystem::path pdbPath;
    if (debugInformation_ != llvm_backend::DebugInformationKind::None)
    {
        pdbPath = outputPath;
        pdbPath.replace_extension(".pdb");
    }
    if (!linker->linkExecutable(
            objectFiles, importLibraries, outputPath, subsystem_, pdbPath, linkerError))
    {
        error = linkerError;
        return false;
    }

    PeExecutableVerifier verifier;
    if (!verifier.verify(outputPath))
    {
        std::error_code cleanupError;
        std::filesystem::remove(outputPath, cleanupError);
        if (!pdbPath.empty()) std::filesystem::remove(pdbPath, cleanupError);
        error = "linked PE verification failed: " + verifier.error();
        return false;
    }

    info_ = verifier.info();
    return true;
}

bool WindowsExecutableBuilder::buildObjects(
    std::span<const std::filesystem::path> programObjects,
    std::string_view programLlvmText,
    const std::filesystem::path& outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::string_view mainSymbol,
    std::string& error)
{
    info_ = {};
    reusedIncrementalLink_ = false;
    error.clear();
    if (programObjects.empty())
    {
        error = "partitioned executable has no program objects";
        return false;
    }
    if (outputPath.empty() || mainSymbol.empty())
    {
        error = "partitioned executable output path or entry symbol is empty";
        return false;
    }
    const std::filesystem::path parent = outputPath.parent_path();
    if (!parent.empty() && !std::filesystem::is_directory(parent))
    {
        error = "Windows executable output directory does not exist: " + parent.string();
        return false;
    }
    for (std::size_t ordinal = 0; ordinal < programObjects.size(); ++ordinal)
    {
        if (!programObjectsVerified_ &&
            !verifyObject(programObjects[ordinal], "program code unit", error)) return false;
    }

    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
        llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
    if (targetMachine == nullptr)
    {
        error = "could not create the Windows x64 TargetMachine: " + targetError;
        return false;
    }
    const bool independentRuntime = !runtimeLibrary_.empty();
    if (independentRuntime && !verifyWindowsRuntimeLibrary(runtimeLibrary_, error))
    {
        return false;
    }
    runtime::WindowsRuntimeBuilder runtimeBuilder;
    std::unique_ptr<llvm_backend::LlvmModule> runtimeModule =
        independentRuntime
            ? runtimeBuilder.buildEntry(mainSymbol, targetError)
            : runtimeBuilder.build(mainSymbol, targetError);
    if (runtimeModule == nullptr)
    {
        error = "could not construct the NativeSharp runtime: " + targetError;
        return false;
    }
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation runtimeObservation;
    if (!optimizer.optimize(*runtimeModule, *targetMachine, optimizationLevel,
            &runtimeObservation, targetError))
    {
        error = "runtime LLVM optimization failed: " + targetError;
        return false;
    }

    std::string incrementalKey;
    const std::filesystem::path statePath = incrementalStatePath(outputPath);
    if (incrementalLink_)
    {
        const std::string runtimeText = runtimeModule->text();
        std::vector<std::filesystem::path> runtimeAwareLibraries = extraLinkLibraries_;
        if (independentRuntime) runtimeAwareLibraries.push_back(runtimeLibrary_);
        incrementalKey = incrementalLinkKey(programObjects, programLlvmText, runtimeText,
            runtimeAwareLibraries, externalImports_, optimizationLevel, debugInformation_,
            subsystem_, mainSymbol, error);
        if (incrementalKey.empty()) return false;
        std::filesystem::path pdbPath = outputPath;
        pdbPath.replace_extension(".pdb");
        const bool debugCompanionExists =
            debugInformation_ == llvm_backend::DebugInformationKind::None ||
            std::filesystem::is_regular_file(pdbPath);
        if (debugCompanionExists && readIncrementalState(statePath) == incrementalKey)
        {
            PeExecutableVerifier verifier;
            if (verifier.verify(outputPath))
            {
                info_ = verifier.info();
                reusedIncrementalLink_ = true;
                return true;
            }
        }
    }

    std::unique_ptr<TemporaryDirectory> temporaryDirectory = createTemporaryDirectory(targetError);
    if (temporaryDirectory == nullptr)
    {
        error = targetError;
        return false;
    }
    const std::filesystem::path runtimeObject = temporaryDirectory->path() / "runtime.obj";
    if (!targetMachine->emit(*runtimeModule, runtimeObject,
            llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error = "runtime object emission failed: " + targetError;
        return false;
    }
    if (!verifyObject(runtimeObject, "runtime", error)) return false;

    std::unique_ptr<LldLinker> linker = LldLinker::discover(targetError);
    if (linker == nullptr)
    {
        error = targetError;
        return false;
    }
    std::vector<std::filesystem::path> importLibraries;
    // The import closure is canonical and external imports are explicit. The retained IR-text
    // parameter is intentionally ignored by createWindowsImportLibraries, so serializing both
    // optimized modules here only adds latency and peak memory.
    if (!createWindowsImportLibraries(*linker, temporaryDirectory->path(), {},
            externalImports_, importLibraries, error))
    {
        return false;
    }
    importLibraries.insert(
        importLibraries.end(), extraLinkLibraries_.begin(), extraLinkLibraries_.end());
    if (independentRuntime) importLibraries.push_back(runtimeLibrary_);

    std::vector<std::filesystem::path> objectFiles(programObjects.begin(), programObjects.end());
    objectFiles.push_back(runtimeObject);
    std::filesystem::path pdbPath;
    if (debugInformation_ != llvm_backend::DebugInformationKind::None)
    {
        pdbPath = outputPath;
        pdbPath.replace_extension(".pdb");
    }
    if (!linker->linkExecutable(
            objectFiles, importLibraries, outputPath, subsystem_, pdbPath, targetError))
    {
        error = targetError;
        return false;
    }
    PeExecutableVerifier verifier;
    if (!verifier.verify(outputPath))
    {
        std::error_code cleanupError;
        std::filesystem::remove(outputPath, cleanupError);
        if (!pdbPath.empty()) std::filesystem::remove(pdbPath, cleanupError);
        error = "linked PE verification failed: " + verifier.error();
        return false;
    }
    info_ = verifier.info();
    if (incrementalLink_ && !writeIncrementalState(statePath, incrementalKey, error)) return false;
    return true;
}

void WindowsExecutableBuilder::setExternalImports(std::vector<ExternalImportModule> externalImports)
{
    externalImports_ = std::move(externalImports);
}

void WindowsExecutableBuilder::setExtraLinkLibraries(std::vector<std::filesystem::path> extraLibraries)
{
    extraLinkLibraries_ = std::move(extraLibraries);
}

void WindowsExecutableBuilder::setRuntimeLibrary(std::filesystem::path runtimeLibrary)
{
    runtimeLibrary_ = std::move(runtimeLibrary);
}

void WindowsExecutableBuilder::setDebugInformation(
    llvm_backend::DebugInformationKind debugInformation) noexcept
{
    debugInformation_ = debugInformation;
}

void WindowsExecutableBuilder::setIncrementalLink(bool enabled) noexcept
{
    incrementalLink_ = enabled;
}

void WindowsExecutableBuilder::setProgramObjectsVerified(bool verified) noexcept
{
    programObjectsVerified_ = verified;
}

bool WindowsExecutableBuilder::reusedIncrementalLink() const noexcept
{
    return reusedIncrementalLink_;
}

void WindowsExecutableBuilder::setSubsystem(WindowsExecutableSubsystem subsystem) noexcept
{
    subsystem_ = subsystem;
}

const PeExecutableInfo& WindowsExecutableBuilder::info() const noexcept
{
    return info_;
}
}
