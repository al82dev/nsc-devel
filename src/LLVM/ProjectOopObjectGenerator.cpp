#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"
#include "nativesharp/LLVM/ProjectOopFunctionReachability.h"

#include "nativesharp/LLVM/FloatingPointMode.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/LLVM/CodegenIdentity.h"

#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopCodePartition.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/ProjectOopMirCacheIdentity.h"

#include "Internal/ColdLlvmPreambleTelemetry.h"
#include "Internal/ColdLlvmTrace.h"
#include "Internal/ColdLlvmTraceScope.h"
#include "Internal/CodeUnitProgress.h"
#include "Internal/PhaseClock.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <optional>
#include <system_error>
#include <utility>

namespace nativesharp::llvm_backend
{
namespace
{
std::optional<std::filesystem::path> createTemporaryDirectory(std::string& error)
{
    std::error_code filesystemError;
    const std::filesystem::path root = std::filesystem::temp_directory_path(filesystemError);
    if (filesystemError)
    {
        error = "could not resolve the system temporary directory for partitioned code generation: " +
            filesystemError.message();
        return std::nullopt;
    }
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (std::uint32_t attempt = 0; attempt < 100; ++attempt)
    {
        const std::filesystem::path candidate = root /
            ("nativesharp-codegen-" + std::to_string(stamp) + "-" + std::to_string(attempt));
        filesystemError.clear();
        if (std::filesystem::create_directory(candidate, filesystemError)) return candidate;
        if (filesystemError && filesystemError != std::errc::file_exists)
        {
            error = "could not create partitioned code-generation directory: " +
                filesystemError.message();
            return std::nullopt;
        }
    }
    error = "could not allocate a unique partitioned code-generation directory";
    return std::nullopt;
}





void appendNodeSourceRanges(
    const frontend::ProjectOopMirNode* root,
    std::vector<SourceRange>& ranges)
{
    std::vector<const frontend::ProjectOopMirNode*> work;
    work.push_back(root);
    while (!work.empty())
    {
        const frontend::ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        if (node->syntax != nullptr) ranges.push_back(node->syntax->range());
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

std::vector<SourceRange> collectLineTableSourceRanges(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel)
{
    std::vector<SourceRange> ranges;
    for (const frontend::NamedTypeDeclaration& type : nameLookup.namedTypes())
        ranges.push_back(type.range);
    for (const frontend::ClassMemberSymbol& member : memberModel.members())
        ranges.push_back(member.range);
    for (const frontend::ProjectOopMirStaticField& field : mirModel.staticFields())
        ranges.push_back(field.sourceRange);
    for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel.fieldInitializers())
        appendNodeSourceRanges(initializer.value.get(), ranges);
    for (const frontend::ProjectOopMirFunction& function : mirModel.functions())
    {
        appendNodeSourceRanges(function.body.get(), ranges);
        for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
            appendNodeSourceRanges(cleanup.action, ranges);
        for (const frontend::ProjectOopMirNonlocalExit& exit : function.exceptionFlow.nonlocalExits)
            appendNodeSourceRanges(exit.node, ranges);
    }
    return ranges;
}

std::string combineLineTableCacheIdentity(
    std::string mirIdentity,
    std::string_view sourceIdentity)
{
    std::string combined = "NativeSharp.ProjectOopLineTableObjectIdentity.v1\n";
    combined += "mir-size " + std::to_string(mirIdentity.size()) + "\n";
    combined += mirIdentity;
    combined += "source-size " + std::to_string(sourceIdentity.size()) + "\n";
    combined += sourceIdentity;
    return combined;
}

std::vector<std::uint8_t> prepareCodeUnitTelemetry(
    std::size_t ordinal,
    bool usePartitions,
    const frontend::ProjectOopMirModel& mirModel,
    const ProjectOopCodePartition& partition,
    std::span<const std::size_t> coordinatorFunctionDeclarations,
    const ProjectOopFunctionReachabilityAnalysis& functionAnalysis,
    CodeUnitTelemetry& telemetry,
    std::string& moduleName)
{
    moduleName = "NativeSharp.OOP";
    if (usePartitions)
        moduleName = ordinal == 0
            ? "NativeSharp.OOP.globals"
            : "NativeSharp.OOP." + partition.units()[ordinal - 1].stableKey;
    telemetry.name = moduleName;
    if (!usePartitions)
    {
        telemetry.definitionCount = mirModel.functions().size();
        telemetry.declarationCount = mirModel.functions().size();
    }
    else if (ordinal == 0)
    {
        telemetry.declarationCount = coordinatorFunctionDeclarations.size();
    }
    else
    {
        const ProjectOopCodeUnit& unit = partition.units()[ordinal - 1];
        telemetry.definitionCount = unit.functionOrdinals.size();
        telemetry.declarationCount = unit.declarationOrdinals.size();
    }
    const std::span<const std::size_t> definitions = !usePartitions || ordinal == 0
        ? std::span<const std::size_t>{}
        : std::span<const std::size_t>(partition.units()[ordinal - 1].functionOrdinals);
    const PhaseClock staticFieldMaskClock;
    std::vector<std::uint8_t> selectedStaticFields =
        functionAnalysis.staticFieldDeclarationMask(
            definitions, usePartitions, usePartitions && ordinal == 0);
    telemetry.staticFieldMaskMicros = staticFieldMaskClock.elapsedMicros();
    telemetry.staticFieldCount = static_cast<std::size_t>(std::count(
        selectedStaticFields.begin(), selectedStaticFields.end(), std::uint8_t{1U}));
    return selectedStaticFields;
}

bool restorePreLlvmObject(
    const CompilationArtifactCache* cache,
    bool usePreLlvmCache,
    const std::string& cacheKey,
    const std::filesystem::path& objectFile,
    CodeUnitTelemetry& telemetry,
    std::uint8_t& cacheHit,
    std::uint8_t& cacheMiss,
    std::string& moduleError,
    const std::function<void(const basic::CompilationEvent&)>& progressSink,
    const std::string& moduleName,
    bool& complete)
{
    complete = false;
    if (!usePreLlvmCache) return true;
    bool hit = false;
    CacheMissReason reason = CacheMissReason::Cold;
    const PhaseClock lookup;
    if (!cache->restoreObject(cacheKey, objectFile, hit, moduleError, &reason)) return false;
    telemetry.lookupMicros += lookup.elapsedMicros();
    telemetry.missReason = reason;
    if (hit)
    {
        const PhaseClock verification;
        CoffObjectVerifier verifier;
        if (verifier.verify(objectFile))
        {
            telemetry.verificationMicros += verification.elapsedMicros();
            cacheHit = 1;
            if (progressSink)
                progressSink({basic::CompilationEventKind::CacheHit,
                    "LLVM", moduleName, 0, 0, {}});
            complete = true;
            return true;
        }
        telemetry.verificationMicros += verification.elapsedMicros();
    }
    cacheMiss = 1;
    if (progressSink)
        progressSink({basic::CompilationEventKind::CacheMiss,
            "LLVM", moduleName, 0, 0, {}});
    return true;
}

std::unique_ptr<LlvmModule> lowerCodeUnit(
    std::size_t ordinal,
    bool usePartitions,
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::uint8_t> reachableDefinitions,
    std::span<const std::uint8_t> selectedStaticFields,
    std::span<const std::size_t> coordinatorFunctionDeclarations,
    const ProjectOopCodePartition& partition,
    std::string& moduleError,
    CodeUnitTelemetry& telemetry)
{
    ProjectOopLlvmLowerer lowerer;
    const PhaseClock lowering;
    std::unique_ptr<LlvmModule> module;
    if (!usePartitions)
        module = lowerer.lower(ProjectOopLlvmLoweringRequest::withDebug(
            nameLookup, memberModel, mirModel, sourceManager, debugInformation,
            optimizationLevel, debugPrefixMaps, reachableDefinitions, selectedStaticFields));
    else if (ordinal == 0)
        module = lowerer.lower(ProjectOopLlvmLoweringRequest::partition(
            nameLookup, memberModel, mirModel, sourceManager, debugInformation,
            optimizationLevel, {}, true, "NativeSharp.OOP.globals", debugPrefixMaps,
            coordinatorFunctionDeclarations, reachableDefinitions, selectedStaticFields));
    else
    {
        const ProjectOopCodeUnit& unit = partition.units()[ordinal - 1];
        module = lowerer.lower(ProjectOopLlvmLoweringRequest::partition(
            nameLookup, memberModel, mirModel, sourceManager, debugInformation,
            optimizationLevel, unit.functionOrdinals, false,
            "NativeSharp.OOP." + unit.stableKey, debugPrefixMaps,
            unit.declarationOrdinals, reachableDefinitions, selectedStaticFields));
    }
    telemetry.loweringMicros += lowering.elapsedMicros();
    if (module == nullptr || !lowerer.error().empty())
    {
        moduleError = "lowering failed";
        if (!lowerer.error().empty()) moduleError += ": " + lowerer.error();
        return nullptr;
    }
    return module;
}

}

/// EN: Runs the LLVM optimization pipeline on a lowered module already configured for its target.
///     Parameters: `module`, the code unit (mutated in place); `targetMachine`, the machine that
///     configured it; `optimizationLevel`, O0..O3; `requiresWholeModuleRequiredInlining`, the
///     mandatory-inlining obligation; `moduleError` receives the failure text; `telemetry`
///     accumulates `optimizationMicros`.
///     Returns true on success. Thread safety: one call per module; modules are not shared.
///     This is the former second half of `prepareCodeUnitTarget`, split out unchanged so the
///     pre-optimization IR can be fingerprinted between target configuration and optimization.
/// ES: Ejecuta el pipeline de optimizacion de LLVM sobre un modulo ya configurado para su target.
///     Parametros: `module`, la unidad (se modifica); `targetMachine`, la maquina que la configuro;
///     `optimizationLevel`, O0..O3; `requiresWholeModuleRequiredInlining`, la obligacion de inlining
///     obligatorio; `moduleError` recibe el texto de falla; `telemetry` acumula
///     `optimizationMicros`.
///     Retorno: true si tiene exito. Hilos: una llamada por modulo; los modulos no se comparten.
///     Es la antigua segunda mitad de `prepareCodeUnitTarget`, separada sin cambios para poder tomar
///     la huella del IR previo a la optimizacion entre la configuracion del target y la optimizacion.
bool optimizeConfiguredCodeUnit(
    LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    LlvmOptimizationLevel optimizationLevel,
    bool requiresWholeModuleRequiredInlining,
    std::string& moduleError,
    CodeUnitTelemetry& telemetry)
{
    LlvmOptimizer optimizer;
    const PhaseClock optimization;
    if (!optimizer.optimizePrepared(
            module, targetMachine, optimizationLevel,
            requiresWholeModuleRequiredInlining, nullptr, moduleError))
    {
        telemetry.optimizationMicros += optimization.elapsedMicros();
        moduleError = moduleError.empty()
            ? "optimization failed"
            : "optimization failed: " + moduleError;
        return false;
    }
    telemetry.optimizationMicros += optimization.elapsedMicros();
    return true;
}

namespace
{

/// EN: Creates the Windows x64 target machine for a code unit and configures the module for it.
///     Parameters: `module`, the lowered unit (triple and data layout are set in place);
///     `optimizationLevel`, the requested level; `moduleError`, the failure text; `telemetry`
///     accumulates `targetMicros`.
///     Returns the configured machine, or nullptr with `moduleError` set. This is the former first
///     half of `prepareCodeUnitTarget`, unchanged.
/// ES: Crea la maquina de target Windows x64 de una unidad y configura el modulo para ella.
///     Parametros: `module`, la unidad bajada (se fijan triple y data layout); `optimizationLevel`,
///     el nivel pedido; `moduleError`, el texto de falla; `telemetry` acumula `targetMicros`.
///     Retorno: la maquina configurada, o nullptr con `moduleError` fijado. Es la antigua primera
///     mitad de `prepareCodeUnitTarget`, sin cambios.
std::unique_ptr<LlvmTargetMachine> configureCodeUnitTarget(
    LlvmModule& module,
    LlvmOptimizationLevel optimizationLevel,
    std::string& moduleError,
    CodeUnitTelemetry& telemetry)
{
    const PhaseClock targetSetup;
    std::unique_ptr<LlvmTargetMachine> targetMachine =
        LlvmTargetMachine::createWindowsX64(optimizationLevel, moduleError);
    const bool configured = targetMachine != nullptr &&
        targetMachine->configureModuleForTarget(module, moduleError);
    telemetry.targetMicros += targetSetup.elapsedMicros();
    if (!configured)
    {
        moduleError = moduleError.empty()
            ? "target configuration failed"
            : "target configuration failed: " + moduleError;
        return nullptr;
    }
    return targetMachine;
}

/// EN: Content key of the object produced from a pre-optimization LLVM module.
///     Parameters: `cache`, an enabled artifact cache; `module`, the lowered unit after target
///     configuration and before optimization; `targetMachine`, its machine; the remaining values
///     are every other input of optimization and emission.
///     Returns a SHA-256 key over: a domain tag (so it never collides with the post-optimization
///     keys of `restorePostLlvmObject`, whose text is optimized IR), the mandatory-inlining flag,
///     the CPU name and feature string, and the full textual IR; `objectKey` adds the compiler
///     codegen identity, LLVM version, triple, data layout, policy, level, pipeline and debug kind.
///     Soundness: optimization and emission are deterministic functions of exactly these inputs,
///     so two units with equal keys produce byte-identical objects (strict equivalence).
///     Complexity: O(size of the IR) to print and hash. No side effects; thread safe.
/// ES: Clave de contenido del objeto producido desde un modulo LLVM previo a la optimizacion.
///     Parametros: `cache`, un cache de artefactos habilitado; `module`, la unidad bajada despues
///     de configurar el target y antes de optimizar; `targetMachine`, su maquina; los demas valores
///     son el resto de entradas de la optimizacion y la emision.
///     Retorno: clave SHA-256 sobre: una etiqueta de dominio (para no chocar con las claves
///     posteriores a la optimizacion de `restorePostLlvmObject`, cuyo texto es IR optimizado), la
///     bandera de inlining obligatorio, el nombre de CPU y sus features, y el IR textual completo;
///     `objectKey` agrega la identidad de codegen del compilador, la version de LLVM, el triple, el
///     data layout, la politica, el nivel, el pipeline y el tipo de depuracion.
///     Solidez: optimizacion y emision son funciones deterministas de exactamente estas entradas,
///     asi que dos unidades con la misma clave producen objetos identicos byte a byte.
///     Complejidad: O(tamano del IR) para imprimir y hashear. Sin efectos laterales.
std::string preOptimizationObjectKey(
    const CompilationArtifactCache& cache,
    const LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    const std::string& codeGenerationPolicy,
    LlvmOptimizationLevel optimizationLevel,
    DebugInformationKind debugInformation,
    bool requiresWholeModuleRequiredInlining)
{
    std::string material = "NativeSharp.PreOptimizationIrObject.v1\n";
    material += requiresWholeModuleRequiredInlining ? "required-inlining 1\n" : "required-inlining 0\n";
    material += "cpu " + targetMachine.cpuName() + "\n";
    material += "features " + targetMachine.featureString() + "\n";
    material += module.text();
    return cache.objectKey(
        material, targetMachine.triple(), targetMachine.dataLayout(),
        codeGenerationPolicy, optimizationLevel, debugInformation);
}

/// EN: Second-level reuse: restores the object of a unit whose MIR identity missed but whose
///     pre-optimization IR equals one already compiled.
///     Parameters: `cache` may be null; `usePreLlvmCache` gates the feature exactly like the MIR
///     cache (off for full debug information and ThinLTO); `irKey` receives the key so the caller
///     can also store the freshly emitted object under it; `complete` becomes true on a hit, in
///     which case `cacheHit` is 1, `cacheMiss` is 0 and optimization and emission are skipped.
///     Returns false only on a cache I/O error (`moduleError` set). Telemetry: the print and hash
///     time is added to `fingerprintMicros`, the restore to `lookupMicros`, the COFF check to
///     `verificationMicros`; `irReuse` records off/miss/hit.
///     Why: the MIR identity includes whole-model tables and global ordinals, so adding a member
///     anywhere misses every unit even when its LLVM IR is unchanged (HECHO, audit 2026-10-22).
/// ES: Reuso de segundo nivel: restaura el objeto de una unidad cuya identidad MIR fallo pero cuyo
///     IR previo a la optimizacion es igual a uno ya compilado.
///     Parametros: `cache` puede ser null; `usePreLlvmCache` habilita la funcion igual que el cache
///     MIR (apagado con depuracion completa y ThinLTO); `irKey` recibe la clave para que el
///     llamador guarde tambien con ella el objeto recien emitido; `complete` queda en true en un
///     acierto, con `cacheHit` en 1, `cacheMiss` en 0 y sin optimizar ni emitir.
///     Retorno: false solo ante un error de E/S del cache (`moduleError` fijado). Telemetria: la
///     impresion y el hash se suman a `fingerprintMicros`, la restauracion a `lookupMicros`, la
///     verificacion COFF a `verificationMicros`; `irReuse` registra off/miss/hit.
///     Motivo: la identidad MIR incluye tablas de todo el modelo y ordinales globales, por lo que
///     agregar un miembro en cualquier lugar falla todas las unidades aunque su IR no cambie
///     (HECHO, auditoria 2026-10-22).
bool restorePreOptimizationObject(
    const CompilationArtifactCache* cache,
    bool usePreLlvmCache,
    const LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    const std::string& codeGenerationPolicy,
    LlvmOptimizationLevel optimizationLevel,
    DebugInformationKind debugInformation,
    bool requiresWholeModuleRequiredInlining,
    const std::filesystem::path& objectFile,
    std::string& irKey,
    CodeUnitTelemetry& telemetry,
    std::uint8_t& cacheHit,
    std::uint8_t& cacheMiss,
    std::string& moduleError,
    bool& complete)
{
    complete = false;
    if (cache == nullptr || !cache->enabled() || !usePreLlvmCache) return true;
    const PhaseClock fingerprint;
    irKey = preOptimizationObjectKey(
        *cache, module, targetMachine, codeGenerationPolicy, optimizationLevel,
        debugInformation, requiresWholeModuleRequiredInlining);
    telemetry.fingerprintMicros += fingerprint.elapsedMicros();
    telemetry.irReuse = CodeUnitIrReuse::Miss;
    bool hit = false;
    const PhaseClock lookup;
    if (!cache->restoreObject(irKey, objectFile, hit, moduleError)) return false;
    telemetry.lookupMicros += lookup.elapsedMicros();
    if (!hit) return true;
    const PhaseClock verification;
    CoffObjectVerifier verifier;
    const bool verified = verifier.verify(objectFile);
    telemetry.verificationMicros += verification.elapsedMicros();
    if (!verified) return true;
    telemetry.irReuse = CodeUnitIrReuse::Hit;
    cacheHit = 1;
    cacheMiss = 0;
    complete = true;
    return true;
}

bool restorePostLlvmObject(
    const CompilationArtifactCache* cache,
    bool usePreLlvmCache,
    const LlvmTargetMachine& targetMachine,
    const std::string& codeGenerationPolicy,
    LlvmOptimizationLevel optimizationLevel,
    DebugInformationKind debugInformation,
    const std::filesystem::path& objectFile,
    const std::string& moduleText,
    std::string& cacheKey,
    CodeUnitTelemetry& telemetry,
    std::uint8_t& cacheHit,
    std::uint8_t& cacheMiss,
    std::string& moduleError,
    bool& complete)
{
    complete = false;
    if (cache == nullptr || !cache->enabled() || usePreLlvmCache) return true;
    const PhaseClock fingerprint;
    cacheKey = cache->objectKey(
        moduleText, targetMachine.triple(), targetMachine.dataLayout(),
        codeGenerationPolicy, optimizationLevel, debugInformation);
    telemetry.fingerprintMicros += fingerprint.elapsedMicros();
    bool hit = false;
    CacheMissReason reason = CacheMissReason::Cold;
    const PhaseClock lookup;
    if (!cache->restoreObject(cacheKey, objectFile, hit, moduleError, &reason)) return false;
    telemetry.lookupMicros += lookup.elapsedMicros();
    telemetry.missReason = reason;
    if (hit)
    {
        const PhaseClock verification;
        CoffObjectVerifier verifier;
        if (verifier.verify(objectFile))
        {
            telemetry.verificationMicros += verification.elapsedMicros();
            cacheHit = 1;
            complete = true;
            return true;
        }
        telemetry.verificationMicros += verification.elapsedMicros();
    }
    cacheMiss = 1;
    return true;
}

}

void emitAndStoreCodeUnit(
    LlvmModule& module,
    LlvmTargetMachine& targetMachine,
    const CompilationArtifactCache* cache,
    const std::string& cacheKey,
    const std::filesystem::path& objectFile,
    CodeUnitTelemetry& telemetry,
    std::string& moduleError)
{
    const PhaseClock emission;
    if (!targetMachine.emitPrepared(
            module, objectFile, LlvmCodeGenFileType::Object, moduleError))
    {
        telemetry.emissionMicros += emission.elapsedMicros();
        return;
    }
    telemetry.emissionMicros += emission.elapsedMicros();
    CoffObjectVerifier verifier;
    const PhaseClock verification;
    if (!verifier.verify(objectFile)) moduleError = verifier.error();
    telemetry.verificationMicros += verification.elapsedMicros();
    if (!moduleError.empty() || cache == nullptr || !cache->enabled()) return;
    const PhaseClock store;
    cache->storeObject(cacheKey, objectFile, moduleError);
    telemetry.storeMicros += store.elapsedMicros();
}

namespace
{

/// EN: Publishes a freshly emitted object under its pre-optimization IR key as well.
///     Parameters: `cache` may be null; `irKey` is empty when the IR reuse was not attempted;
///     `objectFile`, the verified object just written by `emitAndStoreCodeUnit`; `telemetry`
///     accumulates `storeMicros`; `moduleError` is left untouched when it already holds an error
///     and otherwise receives a store failure.
///     Effects: one cache publication (atomic in `CompilationArtifactCache::storeObject`).
///     Why: a later build whose MIR identity differs but whose IR is equal then restores this
///     object instead of optimizing and emitting again.
/// ES: Publica el objeto recien emitido tambien con su clave de IR previo a la optimizacion.
///     Parametros: `cache` puede ser null; `irKey` vacio si no se intento el reuso por IR;
///     `objectFile`, el objeto verificado que acaba de escribir `emitAndStoreCodeUnit`;
///     `telemetry` acumula `storeMicros`; `moduleError` no se toca si ya tiene un error y en otro
///     caso recibe una falla de guardado.
///     Efectos: una publicacion en el cache (atomica en `CompilationArtifactCache::storeObject`).
///     Motivo: un build posterior con identidad MIR distinta pero IR igual restaura este objeto
///     en lugar de optimizar y emitir otra vez.
void storePreOptimizationObject(
    const CompilationArtifactCache* cache,
    const std::string& irKey,
    const std::filesystem::path& objectFile,
    CodeUnitTelemetry& telemetry,
    std::string& moduleError)
{
    if (cache == nullptr || !cache->enabled() || irKey.empty() || !moduleError.empty()) return;
    const PhaseClock store;
    cache->storeObject(irKey, objectFile, moduleError);
    telemetry.storeMicros += store.elapsedMicros();
}

void processObjectCodeUnit(
    std::size_t ordinal,
    bool usePartitions,
    bool usePreLlvmCache,
    bool preserveModuleText,
    bool requiresWholeModuleRequiredInlining,
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::uint8_t> reachableDefinitions,
    std::span<const std::size_t> coordinatorFunctionDeclarations,
    const ProjectOopCodePartition& partition,
    const ProjectOopFunctionReachabilityAnalysis& functionAnalysis,
    const CompilationArtifactCache* cache,
    const std::string& codeGenerationPolicy,
    const std::filesystem::path& objectFile,
    std::string& moduleText,
    std::string& moduleError,
    std::string cacheKey,
    std::uint8_t& cacheHit,
    std::uint8_t& cacheMiss,
    CodeUnitTelemetry& telemetry,
    ColdLlvmTrace& coldLlvmTrace,
    const std::function<void(const basic::CompilationEvent&)>& progressSink,
    std::atomic_size_t& completedModules,
    std::mutex& moduleProgressMutex,
    std::size_t moduleCount)
{
    std::string moduleName;
    const std::vector<std::uint8_t> selectedStaticFields = prepareCodeUnitTelemetry(
        ordinal, usePartitions, mirModel, partition, coordinatorFunctionDeclarations,
        functionAnalysis, telemetry, moduleName);
    const ColdLlvmTraceScope coldTraceScope(
        coldLlvmTrace, telemetry, cacheHit, cacheMiss, moduleError);
    if (cache == nullptr || !cache->enabled()) telemetry.missReason = CacheMissReason::Disabled;
    const CodeUnitProgress progress(
        progressSink, completedModules, moduleProgressMutex, moduleCount, moduleName);
    bool complete = false;
    if (!restorePreLlvmObject(
            cache, usePreLlvmCache, cacheKey, objectFile, telemetry,
            cacheHit, cacheMiss, moduleError, progressSink, moduleName, complete) || complete) return;
    std::unique_ptr<LlvmModule> module = lowerCodeUnit(
        ordinal, usePartitions, nameLookup, memberModel, mirModel, sourceManager,
        debugInformation, optimizationLevel, debugPrefixMaps, reachableDefinitions,
        selectedStaticFields, coordinatorFunctionDeclarations, partition, moduleError, telemetry);
    if (module == nullptr) return;
    std::unique_ptr<LlvmTargetMachine> targetMachine = configureCodeUnitTarget(
        *module, optimizationLevel, moduleError, telemetry);
    if (targetMachine == nullptr) return;
    std::string irKey;
    if (!restorePreOptimizationObject(
            cache, usePreLlvmCache, *module, *targetMachine, codeGenerationPolicy,
            optimizationLevel, debugInformation, requiresWholeModuleRequiredInlining, objectFile,
            irKey, telemetry, cacheHit, cacheMiss, moduleError, complete) || complete) return;
    if (!optimizeConfiguredCodeUnit(
            *module, *targetMachine, optimizationLevel, requiresWholeModuleRequiredInlining,
            moduleError, telemetry)) return;
    if (preserveModuleText) moduleText = module->text();
    if (!restorePostLlvmObject(
            cache, usePreLlvmCache, *targetMachine, codeGenerationPolicy,
            optimizationLevel, debugInformation, objectFile, moduleText, cacheKey,
            telemetry, cacheHit, cacheMiss, moduleError, complete) || complete) return;
    emitAndStoreCodeUnit(
        *module, *targetMachine, cache, cacheKey, objectFile, telemetry, moduleError);
    storePreOptimizationObject(cache, irKey, objectFile, telemetry, moduleError);
}

/// EN: Builds the code partition of one generation with the DeterministicIncremental policy, for
///     every cache and LTO mode. The partition is a pure function of the MIR and `partitionCount`:
///     no plan is restored from or stored in the artifact cache. A restored CacheAwareBalanced
///     plan made the objects of an incremental build depend on earlier builds and differ from a
///     clean build of the same sources (audit 2026-10-22, H3), and its key covered the whole
///     function set, so adding one function redistributed every unit (H7).
///     Preconditions: `mirModel` is built and `partition` is unused. Postconditions: on success
///     `partition` is built and `telemetry.partitionBuildMicros` is set. Returns false with
///     `error` when the partition cannot be built. Not thread safe for one `partition`.
/// ES: Construye la particion de codigo de una generacion con la politica
///     DeterministicIncremental, en todo modo de cache y de LTO. La particion es funcion pura del
///     MIR y de `partitionCount`: no se restaura ni se guarda ningun plan en la cache de
///     artefactos. Un plan CacheAwareBalanced restaurado hacia que los objetos de un build
///     incremental dependieran de builds anteriores y difirieran del build limpio de las mismas
///     fuentes (auditoria 2026-10-22, H3), y su clave cubria todo el conjunto de funciones, asi
///     que agregar una funcion redistribuia todas las unidades (H7).
///     Precondiciones: `mirModel` esta construido y `partition` no se ha usado. Postcondiciones:
///     si tiene exito, `partition` queda construida y `telemetry.partitionBuildMicros` asignado.
///     Devuelve false con `error` si la particion no se puede construir. No es seguro entre
///     hilos para una misma `partition`.
bool buildObjectPartition(
    const frontend::ProjectOopMirModel& mirModel,
    std::size_t partitionCount,
    std::span<const std::uint8_t> reachableDefinitions,
    ProjectOopCodePartition& partition,
    ColdLlvmPreambleTelemetry& telemetry,
    std::string& error)
{
    const PhaseClock partitionBuildClock;
    const bool built = partition.build(
        mirModel, partitionCount, error, ProjectOopCodePartitionPolicy::DeterministicIncremental,
        std::string_view{}, reachableDefinitions);
    telemetry.partitionBuildMicros = partitionBuildClock.elapsedMicros();
    return built;
}

bool requiresWholeModuleInlining(
    const frontend::ProjectOopMirModel& mirModel,
    std::span<const std::uint8_t> reachableDefinitions)
{
    for (std::size_t ordinal = 0; ordinal < mirModel.functions().size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = mirModel.functions()[ordinal];
        if (function.inliningPolicy == frontend::InliningPolicy::Required &&
            (!function.isTransportedInlineBody ||
             (ordinal < reachableDefinitions.size() && reachableDefinitions[ordinal] != 0U)))
            return true;
    }
    return false;
}

std::vector<std::size_t> collectCacheFunctionOrdinals(
    const frontend::ProjectOopMirModel& mirModel,
    std::span<const std::uint8_t> reachableDefinitions)
{
    std::vector<std::size_t> ordinals;
    ordinals.reserve(mirModel.functions().size());
    for (std::size_t ordinal = 0; ordinal < mirModel.functions().size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = mirModel.functions()[ordinal];
        if (function.isTransportedInlineBody &&
            (ordinal >= reachableDefinitions.size() || reachableDefinitions[ordinal] == 0U))
            continue;
        ordinals.push_back(ordinal);
    }
    return ordinals;
}

void composeModuleCacheKeys(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::uint8_t> reachableDefinitions,
    std::span<const std::size_t> coordinatorFunctionDeclarations,
    const ProjectOopCodePartition& partition,
    bool usePartitions,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    const CompilationArtifactCache& cache,
    const std::string& codeGenerationPolicy,
    std::vector<std::string>& cacheKeys,
    std::vector<CodeUnitTelemetry>& codeUnitTelemetry,
    ColdLlvmPreambleTelemetry& telemetry)
{
    const PhaseClock identityIndexClock;
    const ProjectOopMirCacheIdentityIndex identityIndex(mirModel);
    telemetry.identityIndexBuildMicros = identityIndexClock.elapsedMicros();
    std::string lineTableSourceIdentity;
    if (debugInformation == DebugInformationKind::LineTablesOnly)
    {
        const PhaseClock lineTableIdentityClock;
        const std::vector<SourceRange> sourceRanges =
            collectLineTableSourceRanges(nameLookup, memberModel, mirModel);
        lineTableSourceIdentity = projectOopLineTableSourceIdentity(
            sourceManager, debugPrefixMaps, sourceRanges);
        telemetry.lineTableIdentityMicros = lineTableIdentityClock.elapsedMicros();
    }
    const std::vector<std::size_t> allFunctions =
        collectCacheFunctionOrdinals(mirModel, reachableDefinitions);
    const PhaseClock moduleKeyComposeClock;
    for (std::size_t ordinal = 0; ordinal < cacheKeys.size(); ++ordinal)
    {
        std::span<const std::size_t> definitions;
        std::span<const std::size_t> declarations;
        if (!usePartitions)
        {
            definitions = allFunctions;
            declarations = allFunctions;
        }
        else if (ordinal == 0)
        {
            declarations = coordinatorFunctionDeclarations;
        }
        else
        {
            const ProjectOopCodeUnit& unit = partition.units()[ordinal - 1];
            definitions = unit.functionOrdinals;
            declarations = unit.declarationOrdinals;
        }
        const PhaseClock fingerprint;
        std::string identity = identityIndex.identity(definitions, declarations);
        if (debugInformation == DebugInformationKind::LineTablesOnly)
            identity = combineLineTableCacheIdentity(
                std::move(identity), lineTableSourceIdentity);
        cacheKeys[ordinal] = cache.inputObjectKey(
            identity, "windows-x64-msvc", codeGenerationPolicy,
            optimizationLevel, debugInformation);
        codeUnitTelemetry[ordinal].fingerprintMicros += fingerprint.elapsedMicros();
    }
    telemetry.moduleKeyComposeMicros = moduleKeyComposeClock.elapsedMicros();
}

bool executeCodeUnits(
    std::size_t,
    std::size_t moduleCount,
    bool usePartitions,
    const FloatingPointState& floatingPointState,
    const std::function<void(std::size_t)>& processModule,
    const std::vector<std::string>& moduleErrors,
    std::string& error,
    driver::CompilationWorkerPool* workerPool)
{
    if (!usePartitions)
    {
        processModule(0);
        return true;
    }
    std::atomic_bool moduleFailure = false;
    for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
    {
        workerPool->submit([&, ordinal](std::size_t) {
            if (moduleFailure.load(std::memory_order_acquire)) return;
            FloatingPointModeScope floatingPointScope;
            floatingPointScope.setState(floatingPointState);
            processModule(ordinal);
            if (!moduleErrors[ordinal].empty())
            {
                moduleFailure.store(true, std::memory_order_release);
                workerPool->requestCancellation();
            }
        });
    }
    try
    {
        workerPool->wait();
    }
    catch (const std::exception& exception)
    {
        error = "LLVM worker failed: " + std::string(exception.what());
        return false;
    }
    catch (...)
    {
        error = "LLVM worker failed with a non-standard exception";
        return false;
    }
    return true;
}

bool summarizeCodeUnits(
    const std::vector<std::string>& moduleErrors,
    const std::vector<std::uint8_t>& cacheHits,
    const std::vector<std::uint8_t>& cacheMisses,
    std::size_t& cacheHitCount,
    std::size_t& cacheMissCount,
    std::string& error)
{
    for (std::size_t ordinal = 0; ordinal < moduleErrors.size(); ++ordinal)
    {
        if (!moduleErrors[ordinal].empty())
        {
            error = "LLVM code unit #" + std::to_string(ordinal) +
                " failed: " + moduleErrors[ordinal];
            return false;
        }
        cacheHitCount += cacheHits[ordinal];
        cacheMissCount += cacheMisses[ordinal];
    }
    return true;
}

std::string combineModuleTexts(const std::vector<std::string>& moduleTexts)
{
    std::string combined;
    for (const std::string& moduleText : moduleTexts)
    {
        combined += moduleText;
        combined += '\n';
    }
    return combined;
}

std::string thinLtoIndexDigest(const std::vector<std::string>& moduleTexts)
{
    // The index is independent of temporary paths, scheduling and cache state. Each optimized
    // partition contributes in stable ordinal order, matching ThinLTO's summary-before-backend
    // contract while native object generation remains embarrassingly parallel.
    std::string index = "NativeSharp.ThinLTO.Index.v1\n";
    for (std::size_t ordinal = 0; ordinal < moduleTexts.size(); ++ordinal)
    {
        const auto bytes = std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(moduleTexts[ordinal].data()),
            moduleTexts[ordinal].size());
        index += std::to_string(ordinal) + ":" +
            basic::sha256Hex(basic::sha256(bytes)) + "\n";
    }
    const auto bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(index.data()), index.size());
    return basic::sha256Hex(basic::sha256(bytes));
}

}

void ProjectOopObjectGenerator::setProgressSink(
    std::function<void(const basic::CompilationEvent&)> sink)
{
    progressSink_ = std::move(sink);
}

ProjectOopObjectBundle::ProjectOopObjectBundle(
    std::filesystem::path temporaryDirectory) noexcept
    : temporaryDirectory_(std::move(temporaryDirectory))
{
}

ProjectOopObjectBundle::~ProjectOopObjectBundle()
{
    if (temporaryDirectory_.empty()) return;
    std::error_code error;
    std::filesystem::remove_all(temporaryDirectory_, error);
}

std::span<const std::filesystem::path> ProjectOopObjectBundle::objectFiles() const noexcept
{
    return objectFiles_;
}

const std::string& ProjectOopObjectBundle::combinedLlvmText() const noexcept
{
    return combinedLlvmText_;
}

std::size_t ProjectOopObjectBundle::codeUnitCount() const noexcept
{
    return codeUnitCount_;
}

bool ProjectOopObjectBundle::usedParallelOptimization() const noexcept
{
    return usedParallelOptimization_;
}

std::size_t ProjectOopObjectBundle::cacheHitCount() const noexcept
{
    return cacheHitCount_;
}

std::size_t ProjectOopObjectBundle::cacheMissCount() const noexcept
{
    return cacheMissCount_;
}

std::size_t ProjectOopObjectBundle::thinLtoSummaryCount() const noexcept
{
    return thinLtoSummaryCount_;
}

const std::string& ProjectOopObjectBundle::thinLtoIndexDigest() const noexcept
{
    return thinLtoIndexDigest_;
}

std::span<const CodeUnitTelemetry> ProjectOopObjectBundle::codeUnitTelemetry() const noexcept
{
    return codeUnitTelemetry_;
}

bool ProjectOopObjectGenerator::finishCodeUnitBundle(
    ProjectOopObjectBundle& result,
    bool preserveModuleText,
    LinkTimeOptimizationMode linkTimeOptimization,
    std::vector<std::string>& moduleTexts,
    const std::vector<std::string>& moduleErrors,
    const std::vector<std::uint8_t>& cacheHits,
    const std::vector<std::uint8_t>& cacheMisses,
    std::vector<CodeUnitTelemetry>& codeUnitTelemetry,
    std::string& error) const
{
    if (!summarizeCodeUnits(
            moduleErrors, cacheHits, cacheMisses,
            result.cacheHitCount_, result.cacheMissCount_, error)) return false;
    result.codeUnitTelemetry_ = std::move(codeUnitTelemetry);
    if (preserveModuleText) result.combinedLlvmText_ = combineModuleTexts(moduleTexts);
    if (linkTimeOptimization == LinkTimeOptimizationMode::Thin)
    {
        result.thinLtoSummaryCount_ = moduleTexts.size();
        result.thinLtoIndexDigest_ = thinLtoIndexDigest(moduleTexts);
    }
    return true;
}

bool ProjectOopObjectGenerator::generateCodeUnits(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::size_t workerCount,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    LinkTimeOptimizationMode linkTimeOptimization,
    const CompilationArtifactCache* cache,
    const ProjectOopFunctionReachabilityAnalysis& functionAnalysis,
    std::span<const std::uint8_t> reachableDefinitions,
    std::span<const std::size_t> coordinatorFunctionDeclarations,
    ProjectOopCodePartition& partition,
    bool requiresWholeModuleRequiredInlining,
    bool usePartitions,
    ProjectOopObjectBundle& result,
    ColdLlvmPreambleTelemetry& preambleTelemetry,
    std::string& error) const
{
    driver::CompilationWorkerPool workerPool(workerCount);
    return generateCodeUnits(
        nameLookup, memberModel, mirModel, sourceManager, debugInformation,
        optimizationLevel, workerCount, debugPrefixMaps, linkTimeOptimization,
        cache, functionAnalysis, reachableDefinitions, coordinatorFunctionDeclarations,
        partition, requiresWholeModuleRequiredInlining, usePartitions, result,
        preambleTelemetry, error, &workerPool);
}

bool ProjectOopObjectGenerator::generateCodeUnits(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::size_t workerCount,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    LinkTimeOptimizationMode linkTimeOptimization,
    const CompilationArtifactCache* cache,
    const ProjectOopFunctionReachabilityAnalysis& functionAnalysis,
    std::span<const std::uint8_t> reachableDefinitions,
    std::span<const std::size_t> coordinatorFunctionDeclarations,
    ProjectOopCodePartition& partition,
    bool requiresWholeModuleRequiredInlining,
    bool usePartitions,
    ProjectOopObjectBundle& result,
    ColdLlvmPreambleTelemetry& preambleTelemetry,
    std::string& error,
    driver::CompilationWorkerPool* workerPool) const
{
    const std::size_t moduleCount = result.objectFiles_.size();
    std::vector<std::string> moduleTexts(moduleCount);
    std::vector<std::string> moduleErrors(moduleCount);
    std::vector<std::uint8_t> cacheHits(moduleCount, 0);
    std::vector<std::uint8_t> cacheMisses(moduleCount, 0);
    std::vector<CodeUnitTelemetry> codeUnitTelemetry(moduleCount);
    const FloatingPointState workerFloatingPointState = captureFloatingPointState();
    const std::string codeGenerationPolicy = codeGenerationPolicyIdentity(linkTimeOptimization);
    const bool usePreLlvmCache = cache != nullptr && cache->enabled() &&
        debugInformation != DebugInformationKind::Full &&
        linkTimeOptimization != LinkTimeOptimizationMode::Thin;
    ColdLlvmTrace coldLlvmTrace(
        kCodegenCacheIdentity, functionAnalysis.projectFingerprint(), debugInformation,
        optimizationLevel, moduleCount, usePartitions, usePreLlvmCache,
        cache != nullptr ? cache->lastPruneTelemetry() : CachePruneTelemetry{},
        preambleTelemetry);
    std::vector<std::string> cacheKeys(moduleCount);
    if (usePreLlvmCache)
        composeModuleCacheKeys(
            nameLookup, memberModel, mirModel, sourceManager, debugPrefixMaps,
            reachableDefinitions, coordinatorFunctionDeclarations, partition, usePartitions,
            debugInformation, optimizationLevel, *cache, codeGenerationPolicy,
            cacheKeys, codeUnitTelemetry, preambleTelemetry);
    // Textual IR used to be serialized for every code unit even though the Windows import
    // planner no longer consumes it.  Keep it only for the two features whose keys/summaries
    // are intentionally based on IR.  Ordinary EXE/DLL/LIB production already fingerprints
    // the emitted objects when incremental linking is enabled.
    const bool preserveModuleText =
        (cache != nullptr && cache->enabled() && !usePreLlvmCache) ||
        linkTimeOptimization == LinkTimeOptimizationMode::Thin;
    // Process one code unit end to end. The LLVM module and target machine are destroyed as soon
    // as its object is emitted instead of retaining every partition through three global waves.
    std::atomic_size_t completedModules = 0;
    std::mutex moduleProgressMutex;
    const auto processModule = [&](std::size_t ordinal) {
        processObjectCodeUnit(
            ordinal, usePartitions, usePreLlvmCache, preserveModuleText,
            requiresWholeModuleRequiredInlining, nameLookup, memberModel, mirModel,
            sourceManager, debugInformation, optimizationLevel, debugPrefixMaps,
            reachableDefinitions, coordinatorFunctionDeclarations, partition,
            functionAnalysis, cache, codeGenerationPolicy, result.objectFiles_[ordinal],
            moduleTexts[ordinal], moduleErrors[ordinal], cacheKeys[ordinal],
            cacheHits[ordinal], cacheMisses[ordinal], codeUnitTelemetry[ordinal],
            coldLlvmTrace, progressSink_, completedModules, moduleProgressMutex, moduleCount);
    };
    const PhaseClock workerWallClock;
    if (!executeCodeUnits(
            workerCount, moduleCount, usePartitions, workerFloatingPointState,
            processModule, moduleErrors, error, workerPool)) return false;
    preambleTelemetry.workerWallMicros = workerWallClock.elapsedMicros();
    for (const CodeUnitTelemetry& telemetry : codeUnitTelemetry)
        preambleTelemetry.staticFieldMaskMicros += telemetry.staticFieldMaskMicros;
    return finishCodeUnitBundle(
        result, preserveModuleText, linkTimeOptimization, moduleTexts, moduleErrors,
        cacheHits, cacheMisses, codeUnitTelemetry, error);
}

std::unique_ptr<ProjectOopObjectBundle> ProjectOopObjectGenerator::generate(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::size_t workerCount,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::string& error) const
{
    driver::CompilationWorkerPool workerPool(workerCount);
    return generate(
        nameLookup, memberModel, mirModel, sourceManager, debugInformation,
        optimizationLevel, workerCount, debugPrefixMaps, error, &workerPool);
}

std::unique_ptr<ProjectOopObjectBundle> ProjectOopObjectGenerator::generate(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::size_t workerCount,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::string& error,
    driver::CompilationWorkerPool* workerPool) const
{
    return generate(nameLookup, memberModel, mirModel, sourceManager, debugInformation,
        optimizationLevel, workerCount, debugPrefixMaps,
        LinkTimeOptimizationMode::None, error, workerPool);
}

std::unique_ptr<ProjectOopObjectBundle> ProjectOopObjectGenerator::generate(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::size_t workerCount,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    LinkTimeOptimizationMode linkTimeOptimization,
    std::string& error) const
{
    driver::CompilationWorkerPool workerPool(workerCount);
    return generate(
        nameLookup, memberModel, mirModel, sourceManager, debugInformation,
        optimizationLevel, workerCount, debugPrefixMaps,
        linkTimeOptimization, error, &workerPool);
}

std::unique_ptr<ProjectOopObjectBundle> ProjectOopObjectGenerator::generate(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::size_t workerCount,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    LinkTimeOptimizationMode linkTimeOptimization,
    std::string& error,
    driver::CompilationWorkerPool* workerPool) const
{
    error.clear();
    workerCount = std::max<std::size_t>(1, workerCount);
    // NSC305-L06D. At O0, using the requested worker count as the code-unit count
    // over-partitions cold builds: beyond eight units the duplicated LLVM setup,
    // emission, verification and cache publication exceed the scheduling gain.
    // Keep the requested worker pool, but cap only the number of O0 partitions.
    constexpr std::size_t maxColdO0CodeUnits = 8;
    const std::size_t partitionCount = optimizationLevel == LlvmOptimizationLevel::O0
        ? std::min(workerCount, maxColdO0CodeUnits)
        : workerCount;
    const CompilationArtifactCache* const cache = activeCompilationArtifactCache();
    ColdLlvmPreambleTelemetry preambleTelemetry;
    const PhaseClock reachabilityClock;
    const ProjectOopFunctionReachabilityAnalysis functionAnalysis(mirModel);
    preambleTelemetry.reachabilityMicros = reachabilityClock.elapsedMicros();
    const std::span<const std::uint8_t> reachableDefinitions =
        functionAnalysis.reachableDefinitions();

    ProjectOopCodePartition partition;
    if (!buildObjectPartition(
            mirModel, partitionCount, reachableDefinitions, partition, preambleTelemetry, error))
        return nullptr;
    const PhaseClock coordinatorClosureClock;
    const std::vector<std::size_t> coordinatorFunctionDeclarations =
        functionAnalysis.coordinatorFunctionDeclarations();
    preambleTelemetry.coordinatorClosureMicros = coordinatorClosureClock.elapsedMicros();
    // LLVM can only inline a call while caller and callee coexist in the same module. Until
    // code-unit summary import is available, splitting a full-debug optimized build silently
    // removes its CodeView/PDB inline scopes. More importantly, splitting any build containing a
    // Required callable can leave a forceinline call unexpanded -- including O0, where the
    // mandatory inliner otherwise works correctly. Keep both profiles monolithic. Projects with
    // no forceinline obligation retain their existing partitioned O0 and lightweight-debug paths.
    const bool requiresWholeModuleDebugInlining =
        debugInformation == DebugInformationKind::Full &&
        optimizationLevel != LlvmOptimizationLevel::O0;
    const bool requiresWholeModuleRequiredInlining =
        requiresWholeModuleInlining(mirModel, reachableDefinitions);
    // Full LTO deliberately keeps the complete program in one LLVM module. This makes
    // inter-unit inlining, global DCE and constant propagation happen before native object
    // emission instead of asking the PE linker to recover information already discarded.
    const bool usePartitions = linkTimeOptimization != LinkTimeOptimizationMode::Full &&
        workerCount > 1 && partition.units().size() > 1 &&
        !requiresWholeModuleDebugInlining && !requiresWholeModuleRequiredInlining;
    const std::size_t moduleCount = usePartitions ? partition.units().size() + 1 : 1;
    const bool parallelOptimization = usePartitions && optimizationLevel != LlvmOptimizationLevel::O0;
    const std::optional<std::filesystem::path> temporaryDirectory =
        createTemporaryDirectory(error);
    if (!temporaryDirectory.has_value()) return nullptr;
    auto result = std::unique_ptr<ProjectOopObjectBundle>(
        new ProjectOopObjectBundle(*temporaryDirectory));
    result->codeUnitCount_ = usePartitions ? partition.units().size() : 1;
    result->usedParallelOptimization_ = parallelOptimization;
    result->objectFiles_.reserve(moduleCount);
    for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
        result->objectFiles_.push_back(
            *temporaryDirectory / ("program-" + std::to_string(ordinal) + ".obj"));

    if (!generateCodeUnits(
            nameLookup, memberModel, mirModel, sourceManager, debugInformation,
            optimizationLevel, workerCount, debugPrefixMaps, linkTimeOptimization, cache,
            functionAnalysis, reachableDefinitions, coordinatorFunctionDeclarations, partition,
            requiresWholeModuleRequiredInlining, usePartitions, *result, preambleTelemetry, error,
            workerPool)) return nullptr;
    return result;
}
}
