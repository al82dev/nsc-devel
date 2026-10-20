#include "nativesharp/Linker/WindowsLibraryBuilder.h"
#include "Internal/ScopedTemporaryDirectory.h"

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
std::unique_ptr<ScopedTemporaryDirectory> createScopedTemporaryDirectory(std::string& error)
{
    return ScopedTemporaryDirectory::create(
        "nativesharp-stage46-", "Stage 46", {}, error);
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

bool createLibraryTargetMachine(
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::unique_ptr<llvm_backend::LlvmTargetMachine>& targetMachine,
    std::string& error)
{
    std::string targetError;
    targetMachine = llvm_backend::LlvmTargetMachine::createWindowsX64(
        optimizationLevel, targetError);
    if (targetMachine != nullptr) return true;
    error = "could not create the Windows x64 TargetMachine: " + targetError;
    return false;
}

bool buildLibraryRuntimeSupport(
    runtime::WindowsRuntimeBuilder& runtimeBuilder,
    const std::filesystem::path& runtimeLibrary,
    std::string_view failureLabel,
    bool& independentRuntime,
    std::unique_ptr<llvm_backend::LlvmModule>& runtimeModule,
    std::string& error)
{
    independentRuntime = !runtimeLibrary.empty();
    if (independentRuntime && !verifyWindowsRuntimeLibrary(runtimeLibrary, error)) return false;
    if (!independentRuntime) runtimeModule = runtimeBuilder.buildSupport(error);
    if (independentRuntime || runtimeModule != nullptr) return true;
    error = std::string(failureLabel) + error;
    return false;
}

bool optimizeLibraryModules(
    llvm_backend::LlvmModule& programModule,
    llvm_backend::LlvmModule* runtimeModule,
    const llvm_backend::LlvmTargetMachine& targetMachine,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::string& error)
{
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation programObservation;
    std::string targetError;
    if (!optimizer.optimize(
            programModule, targetMachine, optimizationLevel, &programObservation, targetError))
    {
        error = "program LLVM optimization failed: " + targetError;
        return false;
    }
    // NSC-LIMIT-005: observada, igual que la de arriba.
    llvm_backend::LlvmOptimizationObservation runtimeObservation;
    if (runtimeModule == nullptr || optimizer.optimize(
            *runtimeModule, targetMachine, optimizationLevel, &runtimeObservation, targetError))
        return true;
    error = "runtime LLVM optimization failed: " + targetError;
    return false;
}

bool emitLibraryObjects(
    llvm_backend::LlvmModule& programModule,
    llvm_backend::LlvmModule* runtimeModule,
    const llvm_backend::LlvmTargetMachine& targetMachine,
    std::unique_ptr<ScopedTemporaryDirectory>& temporaryDirectory,
    std::filesystem::path& programObject,
    std::filesystem::path& runtimeObject,
    std::string& error)
{
    temporaryDirectory = createScopedTemporaryDirectory(error);
    if (temporaryDirectory == nullptr) return false;
    programObject = temporaryDirectory->path() / "program.obj";
    runtimeObject = temporaryDirectory->path() / "runtime.obj";
    std::string targetError;
    if (!targetMachine.emit(
            programModule, programObject, llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error = "program object emission failed: " + targetError;
        return false;
    }
    if (!verifyObject(programObject, "program", error)) return false;
    if (runtimeModule != nullptr && !targetMachine.emit(
            *runtimeModule, runtimeObject,
            llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error = "runtime object emission failed: " + targetError;
        return false;
    }
    return runtimeModule == nullptr || verifyObject(runtimeObject, "runtime", error);
}

bool createAndVerifyStaticLibrary(
    std::span<const std::filesystem::path> archiveInputs,
    const std::filesystem::path& outputPath,
    CoffArchiveInfo& archiveInfo,
    std::string& error)
{
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
    archiveInfo = verifier.info();
    return true;
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
    if (!validateOutputParent(outputPath, "static library", error)) return false;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine;
    if (!createLibraryTargetMachine(optimizationLevel, targetMachine, error)) return false;
    bool independentRuntime = false;
    runtime::WindowsRuntimeBuilder runtimeBuilder;
    std::unique_ptr<llvm_backend::LlvmModule> runtimeModule;
    if (!buildLibraryRuntimeSupport(
            runtimeBuilder, runtimeLibrary_,
            "could not construct NativeSharp static-library runtime support: ",
            independentRuntime, runtimeModule, error)) return false;
    // NSC-LIMIT-005. La observacion va SIEMPRE, no bajo bandera. Un fallo del optimizador que no
    // dice contra que LLVM, con que pipeline ni por donde iba obliga a REPRODUCIRLO para poder
    // diagnosticarlo, y eso es exactamente lo que no se pudo hacer con `c38_enum_ancho`: cuando el
    // sintoma es un codigo de salida distinto a -O2, la corrida que fallo ya termino. Ver
    // LlvmOptimizer.h y LlvmOptimizationObservation.h.
    //
    // COSTE MEDIDO: ~3% del tiempo de compilacion completo a -O2 y 0% a -O0, y el IR resultante es
    // byte a byte el mismo con y sin observar --hay una prueba que lo compara en los tres
    // niveles--. Se paga en cada construccion a cambio de que el PRIMER fallo sea legible.
    if (!optimizeLibraryModules(
            programModule, runtimeModule.get(), *targetMachine, optimizationLevel, error))
        return false;
    std::unique_ptr<ScopedTemporaryDirectory> temporaryDirectory;
    std::filesystem::path programObject;
    std::filesystem::path runtimeObject;
    if (!emitLibraryObjects(
            programModule, runtimeModule.get(), *targetMachine, temporaryDirectory,
            programObject, runtimeObject, error)) return false;
    // A static archive owns this project's object code, not a flattened copy of every
    // LibraryReference archive. NSMETA preserves the dependency closure and the final EXE/DLL
    // link supplies each referenced library exactly once. Vendoring `extraLinkLibraries_` here
    // duplicated both NativeSharp definitions and DLL import members: Saru.NativeSharp.lib, for
    // example, carried NativeSharp.Foundation's __imp_/public symbols and NSLink then received the
    // same Foundation import library again from the metadata closure.
    std::vector<std::filesystem::path> archiveInputs = {programObject};
    if (runtimeModule != nullptr) archiveInputs.push_back(runtimeObject);
    return createAndVerifyStaticLibrary(archiveInputs, outputPath, archiveInfo_, error);
}

namespace
{
std::string combinedDynamicLibraryIrText(
    const llvm_backend::LlvmModule& programModule,
    const llvm_backend::LlvmModule* runtimeModule)
{
    // Stage 53.2b bug fix: scan BOTH modules that get linked into the DLL, not just the
    // program module -- see the identical fix (and its full rationale) in
    // WindowsExecutableBuilder.cpp. Without the runtime module's own text in this scan, a
    // runtime helper whose copy loop gets idiom-recognized into an @llvm.memcpy intrinsic call
    // (as Environment.GetEnvironmentVariable/SetEnvironmentVariable's does, at any optimization
    // level above O0) links with "undefined symbol: memcpy" for every DLL, not just ones that
    // use Environment.
    return programModule.text() +
        (runtimeModule != nullptr ? runtimeModule->text() : std::string());
}

bool discoverDynamicLibraryImports(
    const std::filesystem::path& temporaryDirectory,
    std::string_view programText,
    std::span<const ExternalImportModule> externalImports,
    std::span<const std::filesystem::path> extraLinkLibraries,
    const std::filesystem::path& runtimeLibrary,
    bool independentRuntime,
    std::unique_ptr<LldLinker>& linker,
    std::vector<std::filesystem::path>& importLibraries,
    std::string& error)
{
    linker = LldLinker::discover(error);
    if (linker == nullptr) return false;
    if (!createRuntimeImportLibraries(
            *linker, temporaryDirectory, programText,
            externalImports, importLibraries, error)) return false;
    importLibraries.insert(
        importLibraries.end(), extraLinkLibraries.begin(), extraLinkLibraries.end());
    if (independentRuntime) importLibraries.push_back(runtimeLibrary);
    return true;
}

bool assembleDynamicLibraryObjects(
    runtime::WindowsRuntimeBuilder& runtimeBuilder,
    llvm_backend::LlvmTargetMachine& targetMachine,
    const std::filesystem::path& temporaryDirectory,
    const std::filesystem::path& programObject,
    const std::filesystem::path& runtimeObject,
    const llvm_backend::LlvmModule* runtimeModule,
    bool independentRuntime,
    std::vector<std::filesystem::path>& objects,
    std::string& error)
{
    objects.push_back(programObject);
    if (runtimeModule != nullptr) objects.push_back(runtimeObject);
    if (!independentRuntime) return true;
    const std::filesystem::path anchorObject = temporaryDirectory / "dll-anchor.obj";
    if (!emitDynamicLibraryAnchor(runtimeBuilder, targetMachine, anchorObject, error)) return false;
    objects.push_back(anchorObject);
    return true;
}

std::filesystem::path dynamicLibraryPdbPath(
    const std::filesystem::path& outputPath,
    llvm_backend::DebugInformationKind debugInformation)
{
    if (debugInformation == llvm_backend::DebugInformationKind::None) return {};
    std::filesystem::path pdbPath = outputPath;
    pdbPath.replace_extension(".pdb");
    return pdbPath;
}

bool verifyDynamicLibraryWithNestedExports(
    const std::filesystem::path& outputPath,
    const std::filesystem::path& importLibraryPath,
    std::span<const std::string> exportSymbols,
    PeExecutableInfo& dynamicLibraryInfo,
    std::string& error)
{
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
        if (found) continue;
        std::error_code cleanupError;
        std::filesystem::remove(outputPath, cleanupError);
        std::filesystem::remove(importLibraryPath, cleanupError);
        error = "linked DLL is missing requested export '" + exportSymbol + "'";
        return false;
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
    dynamicLibraryInfo = verifier.info();
    return true;
}
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
        return false;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine;
    if (!createLibraryTargetMachine(optimizationLevel, targetMachine, error)) return false;
    bool independentRuntime = false;
    runtime::WindowsRuntimeBuilder runtimeBuilder;
    std::unique_ptr<llvm_backend::LlvmModule> runtimeModule;
    if (!buildLibraryRuntimeSupport(
            runtimeBuilder, runtimeLibrary_,
            "could not construct NativeSharp DLL runtime support: ",
            independentRuntime, runtimeModule, error)) return false;
    // NSC-LIMIT-005. La observacion va SIEMPRE, no bajo bandera. Un fallo del optimizador que no
    // dice contra que LLVM, con que pipeline ni por donde iba obliga a REPRODUCIRLO para poder
    // diagnosticarlo, y eso es exactamente lo que no se pudo hacer con `c38_enum_ancho`: cuando el
    // sintoma es un codigo de salida distinto a -O2, la corrida que fallo ya termino. Ver
    // LlvmOptimizer.h y LlvmOptimizationObservation.h.
    //
    // COSTE MEDIDO: ~3% del tiempo de compilacion completo a -O2 y 0% a -O0, y el IR resultante es
    // byte a byte el mismo con y sin observar --hay una prueba que lo compara en los tres
    // niveles--. Se paga en cada construccion a cambio de que el PRIMER fallo sea legible.
    // NSC-LIMIT-005: observada, igual que la de arriba.
    if (!optimizeLibraryModules(
            programModule, runtimeModule.get(), *targetMachine, optimizationLevel, error))
        return false;
    std::unique_ptr<ScopedTemporaryDirectory> temporaryDirectory;
    std::filesystem::path programObject;
    std::filesystem::path runtimeObject;
    if (!emitLibraryObjects(
            programModule, runtimeModule.get(), *targetMachine, temporaryDirectory,
            programObject, runtimeObject, error)) return false;
    std::vector<std::filesystem::path> importLibraries;
    const std::string combinedProgramText =
        combinedDynamicLibraryIrText(programModule, runtimeModule.get());
    std::unique_ptr<LldLinker> linker;
    if (!discoverDynamicLibraryImports(
            temporaryDirectory->path(), combinedProgramText, externalImports_,
            extraLinkLibraries_, runtimeLibrary_, independentRuntime,
            linker, importLibraries, error)) return false;
    std::vector<std::filesystem::path> objects;
    if (!assembleDynamicLibraryObjects(
            runtimeBuilder, *targetMachine, temporaryDirectory->path(),
            programObject, runtimeObject, runtimeModule.get(), independentRuntime,
            objects, error)) return false;
    const std::filesystem::path pdbPath = dynamicLibraryPdbPath(outputPath, debugInformation);
    if (!linker->linkDynamicLibrary(
            objects, importLibraries, exportSymbols, outputPath,
            importLibraryPath, pdbPath, error)) return false;
    return verifyDynamicLibraryWithNestedExports(
        outputPath, importLibraryPath, exportSymbols, dynamicLibraryInfo_, error);
}

namespace
{
bool validatePartitionedStaticLibraryRequest(
    std::span<const std::filesystem::path> programObjects,
    bool programObjectsVerified,
    const std::filesystem::path& outputPath,
    std::string& error)
{
    if (programObjects.empty())
    {
        error = "partitioned static library has no program objects";
        return false;
    }
    if (!validateOutputParent(outputPath, "static library", error)) return false;
    for (const std::filesystem::path& object : programObjects)
    {
        if (!programObjectsVerified &&
            !verifyObject(object, "program code unit", error)) return false;
    }
    return true;
}

bool restoreStaticArchive(
    const llvm_backend::CompilationArtifactCache* cache,
    bool cacheEnabled,
    std::string_view codeGenerationPolicy,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::span<const std::filesystem::path> inputs,
    const std::filesystem::path& outputPath,
    CoffArchiveInfo& archiveInfo,
    bool& cacheAttempted,
    bool& reusedArchive,
    std::string& error)
{
    if (!cacheEnabled) return false;
    std::string identity = cachedFileSetIdentity(
        "NativeSharp.StaticArchive.v1", inputs, error);
    if (identity.empty()) return false;
    const std::string key = cache->inputObjectKey(
        identity, "windows-x64-msvc-static-archive", codeGenerationPolicy,
        optimizationLevel, llvm_backend::DebugInformationKind::None);
    cacheAttempted = true;
    bool hit = false;
    if (!cache->restoreObject(key, outputPath, hit, error) || !hit) return false;
    CoffArchiveVerifier verifier;
    if (!verifier.verify(outputPath))
    {
        std::error_code ignored;
        std::filesystem::remove(outputPath, ignored);
        error.clear();
        return false;
    }
    archiveInfo = verifier.info();
    reusedArchive = true;
    return true;
}

bool storeStaticArchive(
    const llvm_backend::CompilationArtifactCache* cache,
    bool cacheEnabled,
    std::string_view codeGenerationPolicy,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::span<const std::filesystem::path> inputs,
    const std::filesystem::path& outputPath,
    std::string& error)
{
    if (!cacheEnabled) return true;
    std::string identity = cachedFileSetIdentity(
        "NativeSharp.StaticArchive.v1", inputs, error);
    if (identity.empty()) return false;
    const std::string key = cache->inputObjectKey(
        identity, "windows-x64-msvc-static-archive", codeGenerationPolicy,
        optimizationLevel, llvm_backend::DebugInformationKind::None);
    return cache->storeObject(key, outputPath, error);
}

bool buildRuntimeObjectForStaticArchive(
    const llvm_backend::CompilationArtifactCache* cache,
    bool cacheEnabled,
    std::string_view codeGenerationPolicy,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    const std::filesystem::path& runtimeObject,
    bool& cacheAttempted,
    bool& reusedRuntimeObject,
    std::string& error)
{
    const std::string runtimeKey = cacheEnabled
        ? cache->inputObjectKey(
            "NativeSharp.WindowsRuntimeSupport.v1", "windows-x64-msvc-runtime",
            codeGenerationPolicy, optimizationLevel, llvm_backend::DebugInformationKind::None)
        : std::string();
    bool runtimeHit = false;
    if (cacheEnabled)
    {
        cacheAttempted = true;
        if (!cache->restoreObject(runtimeKey, runtimeObject, runtimeHit, error)) return false;
        if (runtimeHit && !verifyObject(runtimeObject, "cached runtime", error))
        {
            runtimeHit = false;
            error.clear();
        }
        else if (runtimeHit)
        {
            reusedRuntimeObject = true;
        }
    }
    if (runtimeHit) return true;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine;
    if (!createLibraryTargetMachine(optimizationLevel, targetMachine, error)) return false;
    runtime::WindowsRuntimeBuilder runtimeBuilder;
    std::unique_ptr<llvm_backend::LlvmModule> runtimeModule = runtimeBuilder.buildSupport(error);
    if (runtimeModule == nullptr)
    {
        error = "could not construct NativeSharp static-library runtime support: " + error;
        return false;
    }
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation runtimeObservation;
    std::string targetError;
    if (!optimizer.optimize(
            *runtimeModule, *targetMachine, optimizationLevel, &runtimeObservation, targetError))
    {
        error = "runtime LLVM optimization failed: " + targetError;
        return false;
    }
    if (!targetMachine->emit(
            *runtimeModule, runtimeObject,
            llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error = "runtime object emission failed: " + targetError;
        return false;
    }
    if (!verifyObject(runtimeObject, "runtime", error)) return false;
    return !cacheEnabled || cache->storeObject(runtimeKey, runtimeObject, error);
}

bool buildAndCacheStaticArchive(
    const llvm_backend::CompilationArtifactCache* cache,
    bool cacheEnabled,
    std::string_view codeGenerationPolicy,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::span<const std::filesystem::path> archiveInputs,
    const std::filesystem::path& outputPath,
    CoffArchiveInfo& archiveInfo,
    bool& cacheAttempted,
    bool& reusedArchive,
    std::string& error)
{
    if (restoreStaticArchive(
            cache, cacheEnabled, codeGenerationPolicy, optimizationLevel,
            archiveInputs, outputPath, archiveInfo,
            cacheAttempted, reusedArchive, error)) return true;
    if (!error.empty()) return false;
    if (!createAndVerifyStaticLibrary(archiveInputs, outputPath, archiveInfo, error)) return false;
    return storeStaticArchive(
        cache, cacheEnabled, codeGenerationPolicy,
        optimizationLevel, archiveInputs, outputPath, error);
}
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
    if (!validatePartitionedStaticLibraryRequest(
            programObjects, programObjectsVerified_, outputPath, error)) return false;
    const llvm_backend::CompilationArtifactCache* const cache =
        llvm_backend::activeCompilationArtifactCache();
    const bool cacheEnabled = cache != nullptr && cache->enabled();
    const std::string codeGenerationPolicy =
        llvm_backend::codeGenerationPolicyIdentity(
            llvm_backend::LinkTimeOptimizationMode::None);
    if (!runtimeLibrary_.empty())
    {
        if (!verifyWindowsRuntimeLibrary(runtimeLibrary_, error)) return false;
        return buildAndCacheStaticArchive(
            cache, cacheEnabled, codeGenerationPolicy, optimizationLevel,
            programObjects, outputPath, archiveInfo_, staticArchiveCacheAttempted_,
            reusedStaticArchive_, error);
    }
    std::unique_ptr<ScopedTemporaryDirectory> temporaryDirectory = createScopedTemporaryDirectory(error);
    if (temporaryDirectory == nullptr) return false;
    const std::filesystem::path runtimeObject = temporaryDirectory->path() / "runtime.obj";
    if (!buildRuntimeObjectForStaticArchive(
            cache, cacheEnabled, codeGenerationPolicy, optimizationLevel, runtimeObject,
            runtimeObjectCacheAttempted_, reusedRuntimeObject_, error)) return false;
    std::vector<std::filesystem::path> archiveInputs(programObjects.begin(), programObjects.end());
    archiveInputs.push_back(runtimeObject);
    return buildAndCacheStaticArchive(
        cache, cacheEnabled, codeGenerationPolicy, optimizationLevel,
        archiveInputs, outputPath, archiveInfo_, staticArchiveCacheAttempted_,
        reusedStaticArchive_, error);
}

namespace
{
bool validatePartitionedDynamicLibraryRequest(
    std::span<const std::filesystem::path> programObjects,
    bool programObjectsVerified,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& importLibraryPath,
    std::string& error)
{
    if (programObjects.empty())
    {
        error = "partitioned DLL has no program objects";
        return false;
    }
    if (!validateOutputParent(outputPath, "DLL", error) ||
        !validateOutputParent(importLibraryPath, "DLL import library", error)) return false;
    for (const std::filesystem::path& object : programObjects)
    {
        if (!programObjectsVerified &&
            !verifyObject(object, "program code unit", error)) return false;
    }
    return true;
}

bool optimizeDynamicRuntime(
    llvm_backend::LlvmModule* runtimeModule,
    const llvm_backend::LlvmTargetMachine& targetMachine,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::string& error)
{
    if (runtimeModule == nullptr) return true;
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation runtimeObservation;
    std::string targetError;
    if (optimizer.optimize(
            *runtimeModule, targetMachine, optimizationLevel, &runtimeObservation, targetError))
        return true;
    error = "runtime LLVM optimization failed: " + targetError;
    return false;
}

bool emitDynamicRuntimeObject(
    llvm_backend::LlvmModule* runtimeModule,
    const llvm_backend::LlvmTargetMachine& targetMachine,
    std::unique_ptr<ScopedTemporaryDirectory>& temporaryDirectory,
    std::filesystem::path& runtimeObject,
    std::string& error)
{
    temporaryDirectory = createScopedTemporaryDirectory(error);
    if (temporaryDirectory == nullptr) return false;
    runtimeObject = temporaryDirectory->path() / "runtime.obj";
    if (runtimeModule == nullptr) return true;
    std::string targetError;
    if (!targetMachine.emit(
            *runtimeModule, runtimeObject,
            llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error = "runtime object emission failed: " + targetError;
        return false;
    }
    return verifyObject(runtimeObject, "runtime", error);
}

bool verifyDynamicLibraryWithFindExports(
    const std::filesystem::path& outputPath,
    const std::filesystem::path& importLibraryPath,
    std::span<const std::string> exportSymbols,
    PeExecutableInfo& dynamicLibraryInfo,
    std::string& error)
{
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
    dynamicLibraryInfo = verifier.info();
    return true;
}
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
    if (!validatePartitionedDynamicLibraryRequest(
            programObjects, programObjectsVerified_, outputPath, importLibraryPath, error))
        return false;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine;
    if (!createLibraryTargetMachine(optimizationLevel, targetMachine, error)) return false;
    bool independentRuntime = false;
    runtime::WindowsRuntimeBuilder runtimeBuilder;
    std::unique_ptr<llvm_backend::LlvmModule> runtimeModule;
    if (!buildLibraryRuntimeSupport(
            runtimeBuilder, runtimeLibrary_, "could not construct NativeSharp DLL runtime support: ",
            independentRuntime, runtimeModule, error)) return false;
    if (!optimizeDynamicRuntime(
            runtimeModule.get(), *targetMachine, optimizationLevel, error)) return false;
    std::unique_ptr<ScopedTemporaryDirectory> temporaryDirectory;
    std::filesystem::path runtimeObject;
    if (!emitDynamicRuntimeObject(
            runtimeModule.get(), *targetMachine, temporaryDirectory, runtimeObject, error))
        return false;
    std::unique_ptr<LldLinker> linker;
    std::vector<std::filesystem::path> importLibraries;
    // Runtime/compiler-support imports are a stable closure and project imports are explicit;
    // createRuntimeImportLibraries keeps its text parameter only for API compatibility.
    if (!discoverDynamicLibraryImports(
            temporaryDirectory->path(), {}, externalImports_, extraLinkLibraries_,
            runtimeLibrary_, independentRuntime, linker, importLibraries, error)) return false;
    std::vector<std::filesystem::path> objects(programObjects.begin(), programObjects.end());
    if (runtimeModule != nullptr) objects.push_back(runtimeObject);
    if (independentRuntime)
    {
        const std::filesystem::path anchorObject = temporaryDirectory->path() / "dll-anchor.obj";
        if (!emitDynamicLibraryAnchor(runtimeBuilder, *targetMachine, anchorObject, error))
            return false;
        objects.push_back(anchorObject);
    }
    const std::filesystem::path pdbPath = dynamicLibraryPdbPath(outputPath, debugInformation);
    if (!linker->linkDynamicLibrary(objects, importLibraries, exportSymbols, outputPath,
            importLibraryPath, pdbPath, error)) return false;
    return verifyDynamicLibraryWithFindExports(
        outputPath, importLibraryPath, exportSymbols, dynamicLibraryInfo_, error);
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
