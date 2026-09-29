#include "nativesharp/Linker/WindowsLibraryBuilder.h"

#include "nativesharp/Linker/WindowsRuntimeImports.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/WindowsRuntimeLibrary.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

namespace nativesharp::linker
{
namespace
{
class TemporaryDirectory final
{
public:
    explicit TemporaryDirectory(std::filesystem::path path) noexcept
        : path_(std::move(path))
    {
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

std::unique_ptr<TemporaryDirectory> createTemporaryDirectory(std::string& error)
{
    error.clear();
    std::error_code filesystemError;
    const std::filesystem::path root = std::filesystem::temp_directory_path(filesystemError);
    if (filesystemError)
    {
        error = "could not resolve the system temporary directory: " + filesystemError.message();
        return nullptr;
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (std::uint32_t attempt = 0; attempt < 100; ++attempt)
    {
        const std::filesystem::path candidate = root /
            ("nativesharp-stage46-" + std::to_string(stamp) + "-" + std::to_string(attempt));
        filesystemError.clear();
        if (std::filesystem::create_directory(candidate, filesystemError))
        {
            return std::make_unique<TemporaryDirectory>(candidate);
        }
        if (filesystemError && filesystemError != std::errc::file_exists)
        {
            error = "could not create a Stage 46 temporary directory: " + filesystemError.message();
            return nullptr;
        }
    }

    error = "could not allocate a unique Stage 46 temporary directory";
    return nullptr;
}

bool validateOutputParent(const std::filesystem::path& path, std::string_view label, std::string& error)
{
    if (path.empty())
    {
        error = std::string(label) + " output path is empty";
        return false;
    }
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty() && !std::filesystem::is_directory(parent))
    {
        error = std::string(label) + " output directory does not exist: " + parent.string();
        return false;
    }
    return true;
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

bool emitDynamicLibraryAnchor(
    runtime::WindowsRuntimeBuilder& runtimeBuilder,
    llvm_backend::LlvmTargetMachine& targetMachine,
    const std::filesystem::path& path,
    std::string& error)
{
    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmModule> module =
        runtimeBuilder.buildDynamicLibraryAnchor(targetError);
    if (module == nullptr)
    {
        error = "could not construct NativeSharp DLL anchor: " + targetError;
        return false;
    }
    if (!targetMachine.emit(
            *module, path, llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error = "DLL anchor object emission failed: " + targetError;
        return false;
    }
    return verifyObject(path, "DLL anchor", error);
}

bool createRuntimeImportLibraries(
    const LldLinker& linker,
    const std::filesystem::path& directory,
    std::string_view programText,
    std::span<const ExternalImportModule> externalImports,
    std::vector<std::filesystem::path>& libraries,
    std::string& error)
{
    // Delegates to the ONE shared import table (WindowsRuntimeImports.h). The four symbol
    // arrays that used to be duplicated here verbatim from WindowsExecutableBuilder.cpp are
    // gone on purpose -- see that header's HISTORY note about the five-locations sync rule
    // this component retired.
    return createWindowsImportLibraries(linker, directory, programText, externalImports, libraries, error);
}

std::string cachedFileSetIdentity(
    std::string_view domain,
    std::span<const std::filesystem::path> files,
    std::string& error)
{
    std::string identity(domain);
    identity += "\n";
    for (const std::filesystem::path& file : files)
    {
        basic::Sha256Digest digest{};
        std::uintmax_t size = 0;
        if (!basic::sha256File(file, digest, size, error)) return {};
        identity += std::to_string(size) + ":" + basic::sha256Hex(digest) + "\n";
    }
    return identity;
}
}

bool WindowsLibraryBuilder::buildStatic(
    llvm_backend::LlvmModule& programModule,
    const std::filesystem::path& outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::string& error)
{
    archiveInfo_ = {};
    error.clear();
    if (!validateOutputParent(outputPath, "static library", error))
    {
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
    std::unique_ptr<llvm_backend::LlvmModule> runtimeModule;
    if (!independentRuntime) runtimeModule = runtimeBuilder.buildSupport(error);
    if (!independentRuntime && runtimeModule == nullptr)
    {
        error = "could not construct NativeSharp static-library runtime support: " + error;
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
    if (!optimizer.optimize(
            programModule, *targetMachine, optimizationLevel, &programObservation, targetError))
    {
        error = "program LLVM optimization failed: " + targetError;
        return false;
    }
    // NSC-LIMIT-005: observada, igual que la de arriba.
    llvm_backend::LlvmOptimizationObservation runtimeObservation;
    if (runtimeModule != nullptr && !optimizer.optimize(
            *runtimeModule, *targetMachine, optimizationLevel, &runtimeObservation, targetError))
    {
        error = "runtime LLVM optimization failed: " + targetError;
        return false;
    }

    std::unique_ptr<TemporaryDirectory> temporaryDirectory = createTemporaryDirectory(error);
    if (temporaryDirectory == nullptr)
    {
        return false;
    }

    const std::filesystem::path programObject = temporaryDirectory->path() / "program.obj";
    const std::filesystem::path runtimeObject = temporaryDirectory->path() / "runtime.obj";
    if (!targetMachine->emit(
            programModule,
            programObject,
            llvm_backend::LlvmCodeGenFileType::Object,
            targetError))
    {
        error = "program object emission failed: " + targetError;
        return false;
    }
    if (!verifyObject(programObject, "program", error))
    {
        return false;
    }
    if (runtimeModule != nullptr && !targetMachine->emit(
            *runtimeModule, runtimeObject,
            llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error = "runtime object emission failed: " + targetError;
        return false;
    }
    if (runtimeModule != nullptr && !verifyObject(runtimeObject, "runtime", error))
    {
        return false;
    }
    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    if (linker == nullptr)
    {
        return false;
    }
    // A static archive owns this project's object code, not a flattened copy of every
    // LibraryReference archive. NSMETA preserves the dependency closure and the final EXE/DLL
    // link supplies each referenced library exactly once. Vendoring `extraLinkLibraries_` here
    // duplicated both NativeSharp definitions and DLL import members: Saru.NativeSharp.lib, for
    // example, carried NativeSharp.Foundation's __imp_/public symbols and NSLink then received the
    // same Foundation import library again from the metadata closure.
    std::vector<std::filesystem::path> archiveInputs = {programObject};
    if (runtimeModule != nullptr) archiveInputs.push_back(runtimeObject);
    if (!linker->createStaticLibrary(archiveInputs, outputPath, error))
    {
        return false;
    }

    CoffArchiveVerifier verifier;
    if (!verifier.verify(outputPath))
    {
        std::error_code cleanupError;
        std::filesystem::remove(outputPath, cleanupError);
        error = "static library verification failed: " + verifier.error();
        return false;
    }
    archiveInfo_ = verifier.info();
    return true;
}

bool WindowsLibraryBuilder::buildDynamic(
    llvm_backend::LlvmModule& programModule,
    std::span<const std::string> exportSymbols,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& importLibraryPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    llvm_backend::DebugInformationKind debugInformation,
    std::string& error)
{
    dynamicLibraryInfo_ = {};
    error.clear();
    if (!validateOutputParent(outputPath, "DLL", error) ||
        !validateOutputParent(importLibraryPath, "DLL import library", error))
    {
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
    std::unique_ptr<llvm_backend::LlvmModule> runtimeModule;
    if (!independentRuntime) runtimeModule = runtimeBuilder.buildSupport(error);
    if (!independentRuntime && runtimeModule == nullptr)
    {
        error = "could not construct NativeSharp DLL runtime support: " + error;
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
    if (!optimizer.optimize(
            programModule, *targetMachine, optimizationLevel, &programObservation, targetError))
    {
        error = "program LLVM optimization failed: " + targetError;
        return false;
    }
    // NSC-LIMIT-005: observada, igual que la de arriba.
    llvm_backend::LlvmOptimizationObservation runtimeObservation;
    if (runtimeModule != nullptr && !optimizer.optimize(
            *runtimeModule, *targetMachine, optimizationLevel, &runtimeObservation, targetError))
    {
        error = "runtime LLVM optimization failed: " + targetError;
        return false;
    }

    std::unique_ptr<TemporaryDirectory> temporaryDirectory = createTemporaryDirectory(error);
    if (temporaryDirectory == nullptr)
    {
        return false;
    }

    const std::filesystem::path programObject = temporaryDirectory->path() / "program.obj";
    const std::filesystem::path runtimeObject = temporaryDirectory->path() / "runtime.obj";
    if (!targetMachine->emit(
            programModule,
            programObject,
            llvm_backend::LlvmCodeGenFileType::Object,
            targetError))
    {
        error = "program object emission failed: " + targetError;
        return false;
    }
    if (!verifyObject(programObject, "program", error))
    {
        return false;
    }
    if (runtimeModule != nullptr && !targetMachine->emit(
            *runtimeModule, runtimeObject,
            llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error = "runtime object emission failed: " + targetError;
        return false;
    }
    if (runtimeModule != nullptr && !verifyObject(runtimeObject, "runtime", error))
    {
        return false;
    }

    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    if (linker == nullptr)
    {
        return false;
    }
    std::vector<std::filesystem::path> importLibraries;
    // Stage 53.2b bug fix: scan BOTH modules that get linked into the DLL, not just the
    // program module -- see the identical fix (and its full rationale) in
    // WindowsExecutableBuilder.cpp. Without the runtime module's own text in this scan, a
    // runtime helper whose copy loop gets idiom-recognized into an @llvm.memcpy intrinsic call
    // (as Environment.GetEnvironmentVariable/SetEnvironmentVariable's does, at any optimization
    // level above O0) links with "undefined symbol: memcpy" for every DLL, not just ones that
    // use Environment.
    const std::string combinedProgramText = programModule.text() +
        (runtimeModule != nullptr ? runtimeModule->text() : std::string());
    if (!createRuntimeImportLibraries(
            *linker,
            temporaryDirectory->path(),
            combinedProgramText,
            externalImports_,
            importLibraries,
            error))
    {
        return false;
    }
    for (const std::filesystem::path& extra : extraLinkLibraries_)
    {
        importLibraries.push_back(extra);
    }
    if (independentRuntime) importLibraries.push_back(runtimeLibrary_);

    const std::filesystem::path anchorObject = temporaryDirectory->path() / "dll-anchor.obj";
    std::vector<std::filesystem::path> objects = {programObject};
    if (runtimeModule != nullptr) objects.push_back(runtimeObject);
    if (independentRuntime)
    {
        if (!emitDynamicLibraryAnchor(runtimeBuilder, *targetMachine, anchorObject, error))
            return false;
        objects.push_back(anchorObject);
    }
    std::filesystem::path pdbPath;
    if (debugInformation != llvm_backend::DebugInformationKind::None)
    {
        pdbPath = outputPath;
        pdbPath.replace_extension(".pdb");
    }
    if (!linker->linkDynamicLibrary(
            objects,
            importLibraries,
            exportSymbols,
            outputPath,
            importLibraryPath,
            pdbPath,
            error))
    {
        return false;
    }

    PeExecutableVerifier verifier;
    if (!verifier.verifyDynamicLibrary(outputPath))
    {
        std::error_code cleanupError;
        std::filesystem::remove(outputPath, cleanupError);
        std::filesystem::remove(importLibraryPath, cleanupError);
        error = "linked DLL verification failed: " + verifier.error();
        return false;
    }

    for (const std::string& exportSymbol : exportSymbols)
    {
        bool found = false;
        for (const std::string& actual : verifier.info().exportedSymbols)
        {
            if (actual == exportSymbol)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            std::error_code cleanupError;
            std::filesystem::remove(outputPath, cleanupError);
            std::filesystem::remove(importLibraryPath, cleanupError);
            error = "linked DLL is missing requested export '" + exportSymbol + "'";
            return false;
        }
    }

    CoffArchiveVerifier importVerifier;
    if (!importVerifier.verify(importLibraryPath))
    {
        std::error_code cleanupError;
        std::filesystem::remove(outputPath, cleanupError);
        std::filesystem::remove(importLibraryPath, cleanupError);
        error = "DLL import-library verification failed: " + importVerifier.error();
        return false;
    }

    dynamicLibraryInfo_ = verifier.info();
    return true;
}

bool WindowsLibraryBuilder::buildStaticObjects(
    std::span<const std::filesystem::path> programObjects,
    const std::filesystem::path& outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::string& error)
{
    archiveInfo_ = {};
    staticArchiveCacheAttempted_ = false;
    reusedStaticArchive_ = false;
    runtimeObjectCacheAttempted_ = false;
    reusedRuntimeObject_ = false;
    error.clear();
    if (programObjects.empty())
    {
        error = "partitioned static library has no program objects";
        return false;
    }
    if (!validateOutputParent(outputPath, "static library", error)) return false;
    for (const std::filesystem::path& object : programObjects)
    {
        if (!programObjectsVerified_ &&
            !verifyObject(object, "program code unit", error)) return false;
    }

    const llvm_backend::CompilationArtifactCache* const cache =
        llvm_backend::activeCompilationArtifactCache();
    const bool cacheEnabled = cache != nullptr && cache->enabled();
    const std::string codeGenerationPolicy =
        llvm_backend::codeGenerationPolicyIdentity(
            llvm_backend::LinkTimeOptimizationMode::None);
    const auto restoreArchive = [&](std::span<const std::filesystem::path> inputs) {
        if (!cacheEnabled) return false;
        std::string identity = cachedFileSetIdentity(
            "NativeSharp.StaticArchive.v1", inputs, error);
        if (identity.empty()) return false;
        const std::string key = cache->inputObjectKey(identity,
            "windows-x64-msvc-static-archive", codeGenerationPolicy,
            optimizationLevel, llvm_backend::DebugInformationKind::None);
        staticArchiveCacheAttempted_ = true;
        bool hit = false;
        if (!cache->restoreObject(key, outputPath, hit, error)) return false;
        if (!hit) return false;
        CoffArchiveVerifier verifier;
        if (!verifier.verify(outputPath))
        {
            std::error_code ignored;
            std::filesystem::remove(outputPath, ignored);
            error.clear();
            return false;
        }
        archiveInfo_ = verifier.info();
        reusedStaticArchive_ = true;
        return true;
    };
    const auto storeArchive = [&](std::span<const std::filesystem::path> inputs) {
        if (!cacheEnabled) return true;
        std::string identity = cachedFileSetIdentity(
            "NativeSharp.StaticArchive.v1", inputs, error);
        if (identity.empty()) return false;
        const std::string key = cache->inputObjectKey(identity,
            "windows-x64-msvc-static-archive", codeGenerationPolicy,
            optimizationLevel, llvm_backend::DebugInformationKind::None);
        return cache->storeObject(key, outputPath, error);
    };

    if (!runtimeLibrary_.empty())
    {
        if (!verifyWindowsRuntimeLibrary(runtimeLibrary_, error)) return false;
        if (restoreArchive(programObjects)) return true;
        if (!error.empty()) return false;
        std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
        if (linker == nullptr) return false;
        if (!linker->createStaticLibrary(programObjects, outputPath, error)) return false;
        CoffArchiveVerifier verifier;
        if (!verifier.verify(outputPath))
        {
            std::error_code cleanupError;
            std::filesystem::remove(outputPath, cleanupError);
            error = "static library verification failed: " + verifier.error();
            return false;
        }
        archiveInfo_ = verifier.info();
        return storeArchive(programObjects);
    }

    std::unique_ptr<TemporaryDirectory> temporaryDirectory = createTemporaryDirectory(error);
    if (temporaryDirectory == nullptr) return false;
    const std::filesystem::path runtimeObject = temporaryDirectory->path() / "runtime.obj";
    const std::string runtimeKey = cacheEnabled
        ? cache->inputObjectKey("NativeSharp.WindowsRuntimeSupport.v1",
            "windows-x64-msvc-runtime", codeGenerationPolicy,
            optimizationLevel, llvm_backend::DebugInformationKind::None)
        : std::string();
    bool runtimeHit = false;
    if (cacheEnabled)
    {
        runtimeObjectCacheAttempted_ = true;
        if (!cache->restoreObject(runtimeKey, runtimeObject, runtimeHit, error)) return false;
        if (runtimeHit)
        {
            if (!verifyObject(runtimeObject, "cached runtime", error))
            {
                runtimeHit = false;
                error.clear();
            }
            else
            {
                reusedRuntimeObject_ = true;
            }
        }
    }
    if (!runtimeHit)
    {
        std::string targetError;
        std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
            llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
        if (targetMachine == nullptr)
        {
            error = "could not create the Windows x64 TargetMachine: " + targetError;
            return false;
        }
        runtime::WindowsRuntimeBuilder runtimeBuilder;
        std::unique_ptr<llvm_backend::LlvmModule> runtimeModule =
            runtimeBuilder.buildSupport(targetError);
        if (runtimeModule == nullptr)
        {
            error = "could not construct NativeSharp static-library runtime support: " + targetError;
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
        if (!targetMachine->emit(*runtimeModule, runtimeObject,
                llvm_backend::LlvmCodeGenFileType::Object, targetError))
        {
            error = "runtime object emission failed: " + targetError;
            return false;
        }
        if (!verifyObject(runtimeObject, "runtime", error)) return false;
        if (cacheEnabled && !cache->storeObject(runtimeKey, runtimeObject, error)) return false;
    }

    std::vector<std::filesystem::path> archiveInputs(programObjects.begin(), programObjects.end());
    archiveInputs.push_back(runtimeObject);
    if (restoreArchive(archiveInputs)) return true;
    if (!error.empty()) return false;
    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    if (linker == nullptr) return false;
    if (!linker->createStaticLibrary(archiveInputs, outputPath, error)) return false;

    CoffArchiveVerifier verifier;
    if (!verifier.verify(outputPath))
    {
        std::error_code cleanupError;
        std::filesystem::remove(outputPath, cleanupError);
        error = "static library verification failed: " + verifier.error();
        return false;
    }
    archiveInfo_ = verifier.info();
    return storeArchive(archiveInputs);
}

bool WindowsLibraryBuilder::buildDynamicObjects(
    std::span<const std::filesystem::path> programObjects,
    std::string_view programLlvmText,
    std::span<const std::string> exportSymbols,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& importLibraryPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    llvm_backend::DebugInformationKind debugInformation,
    std::string& error)
{
    dynamicLibraryInfo_ = {};
    error.clear();
    (void)programLlvmText; // Retained for source compatibility; import planning no longer scans IR.
    if (programObjects.empty())
    {
        error = "partitioned DLL has no program objects";
        return false;
    }
    if (!validateOutputParent(outputPath, "DLL", error) ||
        !validateOutputParent(importLibraryPath, "DLL import library", error))
    {
        return false;
    }
    for (const std::filesystem::path& object : programObjects)
    {
        if (!programObjectsVerified_ &&
            !verifyObject(object, "program code unit", error)) return false;
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
    std::unique_ptr<llvm_backend::LlvmModule> runtimeModule;
    if (!independentRuntime) runtimeModule = runtimeBuilder.buildSupport(targetError);
    if (!independentRuntime && runtimeModule == nullptr)
    {
        error = "could not construct NativeSharp DLL runtime support: " + targetError;
        return false;
    }
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation runtimeObservation;
    if (runtimeModule != nullptr && !optimizer.optimize(*runtimeModule, *targetMachine, optimizationLevel,
            &runtimeObservation, targetError))
    {
        error = "runtime LLVM optimization failed: " + targetError;
        return false;
    }

    std::unique_ptr<TemporaryDirectory> temporaryDirectory = createTemporaryDirectory(error);
    if (temporaryDirectory == nullptr) return false;
    const std::filesystem::path runtimeObject = temporaryDirectory->path() / "runtime.obj";
    if (runtimeModule != nullptr && !targetMachine->emit(*runtimeModule, runtimeObject,
            llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error = "runtime object emission failed: " + targetError;
        return false;
    }
    if (runtimeModule != nullptr && !verifyObject(runtimeObject, "runtime", error)) return false;
    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    if (linker == nullptr) return false;
    std::vector<std::filesystem::path> importLibraries;
    // Runtime/compiler-support imports are a stable closure and project imports are explicit;
    // createRuntimeImportLibraries keeps its text parameter only for API compatibility.
    if (!createRuntimeImportLibraries(*linker, temporaryDirectory->path(), {},
            externalImports_, importLibraries, error))
    {
        return false;
    }
    importLibraries.insert(
        importLibraries.end(), extraLinkLibraries_.begin(), extraLinkLibraries_.end());
    if (independentRuntime) importLibraries.push_back(runtimeLibrary_);
    const std::filesystem::path anchorObject = temporaryDirectory->path() / "dll-anchor.obj";
    std::vector<std::filesystem::path> objects(programObjects.begin(), programObjects.end());
    if (runtimeModule != nullptr) objects.push_back(runtimeObject);
    if (independentRuntime)
    {
        if (!emitDynamicLibraryAnchor(runtimeBuilder, *targetMachine, anchorObject, error))
            return false;
        objects.push_back(anchorObject);
    }
    std::filesystem::path pdbPath;
    if (debugInformation != llvm_backend::DebugInformationKind::None)
    {
        pdbPath = outputPath;
        pdbPath.replace_extension(".pdb");
    }
    if (!linker->linkDynamicLibrary(objects, importLibraries, exportSymbols, outputPath,
            importLibraryPath, pdbPath, error))
    {
        return false;
    }

    PeExecutableVerifier verifier;
    if (!verifier.verifyDynamicLibrary(outputPath))
    {
        std::error_code cleanupError;
        std::filesystem::remove(outputPath, cleanupError);
        std::filesystem::remove(importLibraryPath, cleanupError);
        error = "linked DLL verification failed: " + verifier.error();
        return false;
    }
    for (const std::string& exportSymbol : exportSymbols)
    {
        if (std::find(verifier.info().exportedSymbols.begin(),
                verifier.info().exportedSymbols.end(), exportSymbol) ==
            verifier.info().exportedSymbols.end())
        {
            std::error_code cleanupError;
            std::filesystem::remove(outputPath, cleanupError);
            std::filesystem::remove(importLibraryPath, cleanupError);
            error = "linked DLL is missing requested export '" + exportSymbol + "'";
            return false;
        }
    }
    CoffArchiveVerifier importVerifier;
    if (!importVerifier.verify(importLibraryPath))
    {
        std::error_code cleanupError;
        std::filesystem::remove(outputPath, cleanupError);
        std::filesystem::remove(importLibraryPath, cleanupError);
        error = "DLL import-library verification failed: " + importVerifier.error();
        return false;
    }
    dynamicLibraryInfo_ = verifier.info();
    return true;
}

void WindowsLibraryBuilder::setExternalImports(std::vector<ExternalImportModule> externalImports)
{
    externalImports_ = std::move(externalImports);
}

void WindowsLibraryBuilder::setExtraLinkLibraries(std::vector<std::filesystem::path> extraLibraries)
{
    extraLinkLibraries_ = std::move(extraLibraries);
}

void WindowsLibraryBuilder::setRuntimeLibrary(std::filesystem::path runtimeLibrary)
{
    runtimeLibrary_ = std::move(runtimeLibrary);
}

void WindowsLibraryBuilder::setProgramObjectsVerified(bool verified) noexcept
{
    programObjectsVerified_ = verified;
}

const CoffArchiveInfo& WindowsLibraryBuilder::archiveInfo() const noexcept
{
    return archiveInfo_;
}

const PeExecutableInfo& WindowsLibraryBuilder::dynamicLibraryInfo() const noexcept
{
    return dynamicLibraryInfo_;
}

bool WindowsLibraryBuilder::staticArchiveCacheAttempted() const noexcept
{
    return staticArchiveCacheAttempted_;
}

bool WindowsLibraryBuilder::reusedStaticArchive() const noexcept
{
    return reusedStaticArchive_;
}

bool WindowsLibraryBuilder::runtimeObjectCacheAttempted() const noexcept
{
    return runtimeObjectCacheAttempted_;
}

bool WindowsLibraryBuilder::reusedRuntimeObject() const noexcept
{
    return reusedRuntimeObject_;
}
}
