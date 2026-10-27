#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/DebugMetadataVersion.h"

#include "LlvmCApi.h"
#include "SignedDivisionCorrection.h"
#include "Internal/OopLlvmImplementation.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/BuiltinIntegerTypes.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopFunctionReachability.h"
#include "nativesharp/LLVM/LlvmDebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmModuleMetadata.h"
#include "nativesharp/LLVM/LlvmRuntimeNames.h"
#include "nativesharp/LLVM/LlvmTypeClassification.h"
#include "nativesharp/LLVM/LlvmTypeLayout.h"
#include "nativesharp/LLVM/FloatingPointMode.h"
#include "nativesharp/LLVM/LlvmVersion.h"
#include "nativesharp/LLVM/LlvmTrapReason.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/Language3Surface.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Source/SourceManager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;



ProjectOopLlvmLoweringRequest ProjectOopLlvmLoweringRequest::withoutDebug(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectOopMirModel& mirModel)
{
    ProjectOopLlvmLoweringRequest request;
    request.nameLookup_ = &nameLookup;
    request.mirModel_ = &mirModel;
    return request;
}

ProjectOopLlvmLoweringRequest ProjectOopLlvmLoweringRequest::withDebug(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::uint8_t> reachableDefinitions,
    std::span<const std::uint8_t> staticFieldDeclarations)
{
    ProjectOopLlvmLoweringRequest request = withoutDebug(nameLookup, mirModel);
    request.memberModel_ = &memberModel;
    request.sourceManager_ = &sourceManager;
    request.debugInformation_ = debugInformation;
    request.optimizationLevel_ = optimizationLevel;
    request.debugPrefixMaps_ = debugPrefixMaps;
    request.reachableDefinitions_ = reachableDefinitions;
    request.staticFieldDeclarations_ = staticFieldDeclarations;
    return request;
}

ProjectOopLlvmLoweringRequest ProjectOopLlvmLoweringRequest::partition(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::span<const std::size_t> functionOrdinals,
    bool ownsModuleGlobals,
    std::string_view moduleName,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::size_t> declarationOrdinals,
    std::span<const std::uint8_t> reachableDefinitions,
    std::span<const std::uint8_t> staticFieldDeclarations)
{
    ProjectOopLlvmLoweringRequest request = withDebug(
        nameLookup, memberModel, mirModel, sourceManager, debugInformation,
        optimizationLevel, debugPrefixMaps, reachableDefinitions,
        staticFieldDeclarations);
    request.partitioned_ = true;
    request.functionOrdinals_ = functionOrdinals;
    request.declarationOrdinals_ = declarationOrdinals;
    request.reachableDefinitions_ = reachableDefinitions;
    request.staticFieldDeclarations_ = staticFieldDeclarations;
    request.ownsModuleGlobals_ = ownsModuleGlobals;
    request.moduleName_ = moduleName;
    return request;
}

void ProjectOopLlvmLoweringRequest::setSourceChecksums(
    std::span<const std::string> sourceChecksums) noexcept
{
    sourceChecksums_ = sourceChecksums;
}

std::unique_ptr<LlvmModule> ProjectOopLlvmLowerer::lower(
    const ProjectOopLlvmLoweringRequest& request)
{
    error_.clear();
    if (!verifyLoadedLlvmVersion(error_)) return nullptr;
    if (request.nameLookup_ == nullptr || request.mirModel_ == nullptr ||
        !request.nameLookup_->isBuilt() || !request.mirModel_->isBuilt())
    {
        error_ = "encapsulated LLVM backend requires built project names and physical MIR";
        return nullptr;
    }
    if ((request.memberModel_ == nullptr) != (request.sourceManager_ == nullptr) ||
        (request.memberModel_ != nullptr && !request.memberModel_->isBuilt()))
    {
        error_ = "encapsulated LLVM backend received an incomplete debug context";
        return nullptr;
    }
    if (request.debugInformation_ != DebugInformationKind::None &&
        (request.memberModel_ == nullptr || request.sourceManager_ == nullptr))
    {
        error_ = "encapsulated LLVM backend requires a debug context for debug information";
        return nullptr;
    }
    if (request.partitioned_ && request.moduleName_.empty())
    {
        error_ = "encapsulated LLVM backend partition requires a stable module name";
        return nullptr;
    }
    OopLlvmImplementation implementation(
        *request.nameLookup_, request.memberModel_, *request.mirModel_, request.sourceManager_,
        request.debugInformation_, request.optimizationLevel_, request.debugPrefixMaps_,
        request.partitioned_, request.ownsModuleGlobals_, request.functionOrdinals_,
        request.declarationOrdinals_, request.reachableDefinitions_,
        request.staticFieldDeclarations_,
        request.moduleName_);
    implementation.setSourceChecksums(request.sourceChecksums_);
    if (!implementation.lower(error_)) return nullptr;
    return std::unique_ptr<LlvmModule>(
        new LlvmModule(implementation.releaseContext(), implementation.releaseModule()));
}

std::unique_ptr<LlvmModule> ProjectOopLlvmLowerer::lower(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectOopMirModel& mirModel)
{
    return lower(ProjectOopLlvmLoweringRequest::withoutDebug(nameLookup, mirModel));
}

std::unique_ptr<LlvmModule> ProjectOopLlvmLowerer::lower(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::span<const DebugPrefixMap> debugPrefixMaps)
{
    return lower(ProjectOopLlvmLoweringRequest::withDebug(
        nameLookup, memberModel, mirModel, sourceManager, debugInformation,
        optimizationLevel, debugPrefixMaps));
}

std::unique_ptr<LlvmModule> ProjectOopLlvmLowerer::lowerPartition(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::span<const std::size_t> functionOrdinals,
    bool ownsModuleGlobals,
    std::string_view moduleName,
    std::span<const DebugPrefixMap> debugPrefixMaps)
{
    return lower(ProjectOopLlvmLoweringRequest::partition(
        nameLookup, memberModel, mirModel, sourceManager, debugInformation,
        optimizationLevel, functionOrdinals, ownsModuleGlobals, moduleName, debugPrefixMaps));
}

const std::string& ProjectOopLlvmLowerer::error() const noexcept
{
    return error_;
}
}
