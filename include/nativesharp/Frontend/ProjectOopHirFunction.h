#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"

#include "nativesharp/Frontend/ProjectOopHirNode.h"
#include "nativesharp/Frontend/ProjectOopYieldItemMode.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>


namespace nativesharp::frontend
{
struct ProjectOopHirFunction final
{
    std::size_t memberOrdinal = 0;
    std::optional<std::size_t> namespaceFunctionOrdinal{};
    std::size_t declaringTypeOrdinal = 0;
    bool constructor = false;
    bool destructor = false;
    bool isStatic = false;
    bool referenceReturn = false;
    bool isNoHeap = false;
    sema::EffectSet effects;
    // NS304-T05-F03: poll function synthesized from an async function's body. Shares the async
    // function's memberOrdinal (like a lambda shares its owner); mangledName carries `$poll`.
    bool isAsyncPoll = false;
    // Taken from the closed declared iterator result before splitting the poll.
    ProjectOopYieldItemMode asyncYieldItemMode = ProjectOopYieldItemMode::Unknown;
    InliningPolicy inliningPolicy = InliningPolicy::Automatic;
    bool isExportedC = false;
    // Interop Foundation: a function declared in `extern C from "<dll>" { ... }`. It has NO body
    // anywhere in the project -- mangledName is the C symbol verbatim, the backend emits only a
    // declaration (with the same Win64 ABI mapping every definition gets), and (importDllName,
    // mangledName) flows to the linker so the import library materializes.
    bool isImportedC = false;
    bool cppDllImport = false;
    bool isVariadic = false;
    // A public NativeSharp method materialized from .nsmeta. Its source declaration participates
    // in ordinary overload resolution, but its body and symbol live in the referenced library.
    bool isImportedNative = false;
    bool isCppExternal = false;
    // EN: T08-D22: an imported C function declared `__vectorcall`; declaration and calls use that convention.
    // ES: T08-D22: una funcion C importada declarada `__vectorcall`; declaracion y llamadas usan esa convencion.
    bool cppVectorCall = false;
    bool cppRegCall = false;
    bool cppPreserveMost = false;
    bool cppPreserveAll = false;
    bool cppPreserveNone = false;
    // Imported C function using the x86-64 System V calling convention.
    bool cppSysVCall = false;
    // X2-05: a C++ factory function; its calls construct in place through its factory constructor, so it is never
    //     declared itself. / Una fabrica C++; sus llamadas construyen en sitio por su constructor de fabrica.
    bool cppFactoryConstruction = false;
    // X2-05: a NativeSharp function returning a non-trivial C++ record constructs it in the caller's storage (sret).
    //     / Una funcion NativeSharp que devuelve un record C++ no trivial lo construye en el almacenamiento del llamador.
    bool cppConstructedResult = false;
    // X2-05: the parameters (by-value records) whose temporary the callee destroys; the caller clears their drop
    //     flags just before the call. / Los parametros cuyo temporal destruye el llamado.
    std::vector<std::size_t> cppCalleeDestroyedParameters;
    std::vector<std::uint8_t> cppObjectSizeModes;
    // EN: C3-02. The N of each `_BitInt(N)` parameter, then of the result (0 for any other type); empty when none.
    // ES: C3-02. El N de cada parametro `_BitInt(N)` y luego del resultado (0 para otro tipo); vacio si no hay.
    std::vector<std::uint8_t> cppBitIntWidths;
    bool cppSysVRecordArguments = false;
    bool cppSysVDirectRecordReturn = false;
    std::shared_ptr<const interop::abi_plan::Plan> cppAbiPlan;
    std::shared_ptr<const interop::abi_plan::VariadicCallPlan> cppVariadicBasePlan;
    bool isTransportedInlineBody = false;
    // Producer-owned callable synthesized for a source class that declares no constructor.
    // It freezes base chaining plus instance field initializers behind the ordinary ctor ABI.
    bool isSyntheticImplicitConstructor = false;
    std::string importDllName;
    // C1.2 "import identity": this import's C symbol name is claimed by more than one library, so
    // it cannot be resolved statically by name -- mangledName is a unique internal name and the
    // backend emits a body that binds through LoadLibrary/GetProcAddress at first call.
    bool importRequiresRuntimeBinding = false;
    std::string cAbiName;
    bool isLambda = false;
    std::optional<std::size_t> lambdaOrdinal;
    bool hasClosureEnvironment = false;
    std::vector<std::string> captureTypeKeys;
    std::vector<std::string> captureNames;
    // Every current closure slot owns a pointer to a shared heap cell. Preserve provenance so
    // MIR dumps, cache identities and debugger consumers can distinguish direct and inherited
    // captures without reconstructing the method-body model.
    std::vector<MethodLambdaCaptureSourceKind> captureSourceKinds;
    std::vector<language::CaptureMode> captureModes;
    std::vector<bool> captureOwnsValues;
    language::CallableTrait callableTrait = language::CallableTrait::Fn;
    std::uint64_t closureLayoutSize = 0;
    std::uint64_t closureLayoutAlignment = 1;
    std::vector<std::size_t> capturedLocalOrdinals;
    // NS304-T06-F04: generator parameters captured into the frame (original callable's parameter
    // type keys, in original order); the constructor allocates the frame and stores them.
    std::vector<std::string> generatorParameterTypeKeys;
    bool isGeneratorConstructor = false;
    std::vector<std::size_t> capturedParameterOrdinals;
    bool capturesThisCell = false;
    std::string genericSpecializationKey;
    std::string mangledName;
    std::string debugDisplayName;
    std::string returnTypeKey;
    // Empty for non-throwing callables. The current Windows x64 profile uses native EH ABI 1;
    // this key is the statically declared payload bound checked by callers and metadata users.
    std::string errorTypeKey;
    // NS304-T05-F08: async frame storage selected by [FrameStorage(...)].
    language::FrameStorage frameStorage = language::FrameStorage::CallerStack;
    sema::OwnershipKind returnOwnership = sema::OwnershipKind::None;
    std::vector<std::string> parameterNames;
    std::vector<std::string> parameterTypeKeys;
    std::vector<ParameterPassKind> parameterPassKinds{};
    std::vector<sema::OwnershipKind> parameterOwnerships;
    std::vector<std::size_t> localOrdinals;
    std::unique_ptr<ProjectOopHirNode> body;
};
}
