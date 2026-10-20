#pragma once

#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace nativesharp::frontend
{
class ProjectNameLookup;
class ProjectMemberModel;
class ProjectOopMirModel;
}

namespace nativesharp::source
{
class SourceManager;
}

namespace nativesharp::llvm_backend
{
class ProjectOopLlvmLowerer;

// Item 399: the only public ingress into project LLVM lowering.  Construction through the named
// factories makes the three supported backend modes explicit and keeps partition/debug options
// from leaking into the lowering implementation as loosely coupled argument lists.
class ProjectOopLlvmLoweringRequest final
{
public:
    static ProjectOopLlvmLoweringRequest withoutDebug(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectOopMirModel& mirModel);

    static ProjectOopLlvmLoweringRequest withDebug(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel& memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager& sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::span<const DebugPrefixMap> debugPrefixMaps = {},
        std::span<const std::uint8_t> reachableDefinitions = {},
        std::span<const std::uint8_t> staticFieldDeclarations = {});

    static ProjectOopLlvmLoweringRequest partition(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel& memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager& sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::span<const std::size_t> functionOrdinals,
        bool ownsModuleGlobals,
        std::string_view moduleName,
        std::span<const DebugPrefixMap> debugPrefixMaps = {},
        std::span<const std::size_t> declarationOrdinals = {},
        std::span<const std::uint8_t> reachableDefinitions = {},
        std::span<const std::uint8_t> staticFieldDeclarations = {});

private:
    friend class ProjectOopLlvmLowerer;
    const frontend::ProjectNameLookup* nameLookup_ = nullptr;
    const frontend::ProjectMemberModel* memberModel_ = nullptr;
    const frontend::ProjectOopMirModel* mirModel_ = nullptr;
    const source::SourceManager* sourceManager_ = nullptr;
    DebugInformationKind debugInformation_ = DebugInformationKind::None;
    LlvmOptimizationLevel optimizationLevel_ = LlvmOptimizationLevel::O0;
    std::span<const DebugPrefixMap> debugPrefixMaps_;
    std::span<const std::size_t> functionOrdinals_;
    std::span<const std::size_t> declarationOrdinals_;
    std::span<const std::uint8_t> reachableDefinitions_;
    std::span<const std::uint8_t> staticFieldDeclarations_;
    bool partitioned_ = false;
    bool ownsModuleGlobals_ = true;
    std::string_view moduleName_ = "NativeSharp.Stage34.OOP";
};
}
