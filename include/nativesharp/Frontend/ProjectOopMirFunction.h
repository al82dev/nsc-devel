#pragma once

#include "nativesharp/Frontend/ProjectOopMirNode.h"
#include "nativesharp/Frontend/ProjectOopMirFunctionAbi.h"
#include "nativesharp/Frontend/ProjectOopMirCleanupRegion.h"
#include "nativesharp/Frontend/ProjectOopMirExceptionFlow.h"

#include "nativesharp/Language/ErrorSemantics.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"
#include "nativesharp/Frontend/TemplateParseCache.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace nativesharp::frontend
{
struct ProjectOopMirFunction final
{
    std::size_t memberOrdinal = 0;
    std::optional<std::size_t> namespaceFunctionOrdinal;
    std::size_t declaringTypeOrdinal = 0;
    bool constructor = false;
    bool destructor = false;
    bool isStatic = false;
    bool referenceReturn = false;
    bool isNoHeap = false;
    sema::EffectSet effects;
    InliningPolicy inliningPolicy = InliningPolicy::Automatic;
    bool isExportedC = false;
    bool isImportedC = false;
    bool isVariadic = false;
    bool isImportedNative = false;
    bool isTransportedInlineBody = false;
    bool isSyntheticImplicitConstructor = false;
    std::string importDllName;
    // C1.2 "import identity" -- see ProjectOopHirFunction for the full note.
    bool importRequiresRuntimeBinding = false;
    std::string cAbiName;
    bool isLambda = false;
    std::optional<std::size_t> lambdaOrdinal;
    bool hasClosureEnvironment = false;
    std::vector<std::string> captureTypeKeys;
    std::vector<std::string> captureNames;
    std::vector<MethodLambdaCaptureSourceKind> captureSourceKinds;
    std::vector<language::CaptureMode> captureModes;
    std::vector<bool> captureOwnsValues;
    language::CallableTrait callableTrait = language::CallableTrait::Fn;
    std::uint64_t closureLayoutSize = 0;
    std::uint64_t closureLayoutAlignment = 1;
    std::vector<std::size_t> capturedLocalOrdinals;
    std::vector<std::size_t> capturedParameterOrdinals;
    bool capturesThisCell = false;
    std::string genericSpecializationKey;
    std::string mangledName;
    std::string debugDisplayName;
    std::string returnTypeKey;
    std::string errorTypeKey;
    // NS304-T05-F08: async frame storage selected by [FrameStorage(...)].
    language::FrameStorage frameStorage = language::FrameStorage::CallerStack;
    sema::OwnershipKind returnOwnership = sema::OwnershipKind::None;
    std::uint64_t asyncFrameSize = 0; // poll functions only: bytes needed to hold awaited futures
    // NS304-T05-F04: locals that must survive a suspension, with their byte offset and type key.
    std::vector<std::tuple<std::size_t, std::uint64_t, std::string>> asyncFrameLocals;
    // NS304-T06-F04: generator parameters that must survive a yield (captured into the frame),
    // with their byte offset and type key. parameterOrdinal refers to the ORIGINAL callable.
    std::vector<std::tuple<std::size_t, std::uint64_t, std::string>> asyncFrameParameters;
    // NS304-T06-F04: true for the generator CONSTRUCTOR (the original callable) — it must allocate
    // the frame and store the parameters so $moveNext/$poll can read them.
    bool isGeneratorConstructor = false;
    // NS304-T06-F04: original callable's parameter type keys (for $moveNext/$poll frame slots).
    std::vector<std::string> generatorParameterTypeKeys;
    std::vector<std::string> parameterNames;
    std::vector<std::string> parameterTypeKeys;
    std::vector<ParameterPassKind> parameterPassKinds;
    std::vector<sema::OwnershipKind> parameterOwnerships;
    ProjectOopMirFunctionAbi abi;
    std::vector<std::size_t> localOrdinals;
    std::vector<ProjectOopMirCleanupRegion> cleanupRegions;
    ProjectOopMirExceptionFlow exceptionFlow;
    std::unique_ptr<ProjectOopMirNode> body;
};
}
