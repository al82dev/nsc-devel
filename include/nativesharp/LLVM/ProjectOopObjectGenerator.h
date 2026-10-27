#pragma once

#include "nativesharp/Basic/CompilationEvent.h"
#include "nativesharp/LLVM/CodeUnitTelemetry.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/ProjectOopObjectBundle.h"
#include "nativesharp/LLVM/ThinLtoLibraryUnits.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
class ProjectMemberModel;
class ProjectNameLookup;
class ProjectOopMirModel;
}

namespace nativesharp::source
{
class SourceManager;
}

namespace nativesharp::driver
{
class CompilationWorkerPool;
}

namespace nativesharp::llvm_backend
{
struct ColdLlvmPreambleTelemetry;
class ProjectOopCodePartition;
class ProjectOopFunctionReachabilityAnalysis;

class ProjectOopObjectGenerator final
{
public:
    void setProgressSink(std::function<void(const basic::CompilationEvent&)> sink);
    // EN: Keeps the optimized IR of every ThinLTO code unit in the bundle (tests and dumps).
    // ES: Conserva en el paquete el IR optimizado de cada unidad ThinLTO (pruebas y volcados).
    void setThinLtoFinalModuleTextCapture(bool capture) noexcept;
    // EN: Keeps the summary bitcode of every ThinLTO unit and its compatibility identity in the
    //     bundle, for the sidecar of a static library (optimization part 2, item 48).
    // ES: Conserva en el paquete el bitcode con resumen de cada unidad ThinLTO y su identidad de
    //     compatibilidad, para el sidecar de una biblioteca estatica (parte 2, punto 48).
    void setThinLtoUnitBitcodeCapture(bool capture) noexcept;
    // EN: Units of accepted library sidecars that the ThinLTO backends index, import from and write
    //     as objects after the project's own units (item 48). Ignored when ThinLTO does not run.
    // ES: Unidades de sidecars de biblioteca aceptados que los backends ThinLTO indexan, de las que
    //     importan y que escriben como objetos despues de las unidades del proyecto (punto 48). Se
    //     ignoran cuando ThinLTO no corre.
    void setThinLtoLibraryUnits(std::vector<ThinLtoLibraryUnits> libraries);
    // EN: For an executable or a DLL (item 50): the symbols that native objects of the link need from
    //     the program. With a value, every other definition may be internalized and every function
    //     gets its own section; without one (a static library) every definition stays visible.
    // ES: Para un ejecutable o una DLL (punto 50): los simbolos que los objetos nativos del enlace
    //     necesitan del programa. Con valor, toda otra definicion puede internalizarse y cada funcion
    //     recibe su propia seccion; sin valor (una biblioteca estatica) toda definicion queda visible.
    void setThinLtoPreservedSymbols(std::optional<std::vector<std::string>> symbols);
    // EN: Appends the undefined symbols of the COFF object or archive `file` to `symbols`; false with
    //     `error` when it cannot be read or the host has no ThinLTO.
    // ES: Agrega a `symbols` los simbolos indefinidos del objeto o archivo COFF `file`; false con
    //     `error` si no se puede leer o el host no tiene ThinLTO.
    static bool collectUndefinedSymbols(
        const std::filesystem::path& file,
        std::vector<std::string>& symbols,
        std::string& error);

    // EN: Whether a generation with `mode` at `level` runs real ThinLTO backends: thin, -O1 to -O3
    //     and a host with llvm::lto.
    // ES: Si una generacion con `mode` en `level` ejecuta backends ThinLTO reales: thin, -O1 a -O3 y
    //     un host con llvm::lto.
    static bool thinLtoRuns(LinkTimeOptimizationMode mode, LlvmOptimizationLevel level) noexcept;
    // EN: The compatibility identity of the ThinLTO units this compiler writes at `level` with
    //     `debugInformation` (NSC-DISENO-SIDECAR-THINLTO-BIBLIOTECAS.md, section 5). Returns false
    //     with `error` when the target machine cannot be created.
    // ES: La identidad de compatibilidad de las unidades ThinLTO que este compilador escribe en
    //     `level` con `debugInformation` (seccion 5 del diseno). Devuelve false con `error` cuando no
    //     se puede crear la maquina destino.
    static bool thinLtoCompatibility(
        LlvmOptimizationLevel level,
        DebugInformationKind debugInformation,
        std::string& identity,
        std::string& error);

    std::unique_ptr<ProjectOopObjectBundle> generate(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel& memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager& sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::size_t workerCount,
        std::span<const DebugPrefixMap> debugPrefixMaps,
        std::string& error) const;
    std::unique_ptr<ProjectOopObjectBundle> generate(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel& memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager& sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::size_t workerCount,
        std::span<const DebugPrefixMap> debugPrefixMaps,
        std::string& error,
        driver::CompilationWorkerPool* workerPool) const;

    std::unique_ptr<ProjectOopObjectBundle> generate(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel& memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager& sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::size_t workerCount,
        std::span<const DebugPrefixMap> debugPrefixMaps,
        LinkTimeOptimizationMode linkTimeOptimization,
        std::string& error) const;
    std::unique_ptr<ProjectOopObjectBundle> generate(
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
        driver::CompilationWorkerPool* workerPool) const;

private:
    bool finishCodeUnitBundle(
        ProjectOopObjectBundle& result,
        bool preserveModuleText,
        LinkTimeOptimizationMode linkTimeOptimization,
        std::vector<std::string>& moduleTexts,
        const std::vector<std::string>& moduleErrors,
        const std::vector<std::uint8_t>& cacheHits,
        const std::vector<std::uint8_t>& cacheMisses,
        std::vector<CodeUnitTelemetry>& codeUnitTelemetry,
        std::string& error) const;
    bool generateCodeUnits(
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
        std::string& error) const;
    bool generateCodeUnits(
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
        driver::CompilationWorkerPool* workerPool) const;

    std::function<void(const basic::CompilationEvent&)> progressSink_;
    bool captureThinLtoFinalModuleTexts_ = false;
    bool captureThinLtoUnitBitcode_ = false;
    std::vector<ThinLtoLibraryUnits> thinLtoLibraries_;
    std::optional<std::vector<std::string>> thinLtoPreservedSymbols_;
};
}
