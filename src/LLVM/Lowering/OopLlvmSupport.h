#pragma once

#include "../LlvmCApi.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/FloatingPointMode.h"
#include "nativesharp/LLVM/FloatingPointModeScope.h"
#include "nativesharp/LLVM/LlvmModuleMetadata.h"
#include "nativesharp/LLVM/LlvmRuntimeNames.h"
#include "nativesharp/LLVM/LlvmTrapReason.h"
#include "nativesharp/LLVM/LlvmTypeClassification.h"
#include "nativesharp/LLVM/LlvmTypeLayout.h"
#include "nativesharp/LLVM/ProjectOopFunctionReachability.h"
#include "nativesharp/Source/SourceManager.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

std::optional<LLVMAtomicOrdering> orderedAtomicLoadOrdering(std::uint64_t encoded);

std::optional<LLVMAtomicOrdering> orderedAtomicStoreOrdering(std::uint64_t encoded);

std::optional<LLVMAtomicOrdering> orderedAtomicRmwOrdering(std::uint64_t encoded);

LLVMValueRef buildOrderedAtomicRmw(
    LLVMBuilderRef builder, LLVMAtomicRMWBinOp operation, LLVMValueRef location,
    LLVMValueRef operand, LLVMAtomicOrdering ordering, unsigned alignment,
    bool returnNewValue, const char* resultName);

LLVMValueRef buildOrderedAtomicCompareExchange(
    LLVMBuilderRef builder, LLVMValueRef location, LLVMValueRef value,
    LLVMValueRef comparand, LLVMAtomicOrdering successOrder,
    LLVMAtomicOrdering failureOrder, unsigned alignment, const char* resultName);

bool isFloating(std::string_view type) noexcept;

LLVMTypeRef mapBuiltinLlvmType(LLVMContextRef context, std::string_view typeKey) noexcept;

LLVMTypeRef mapPointerLikeLlvmType(
    LLVMContextRef context,
    const frontend::ProjectNameLookup& nameLookup,
    std::string_view typeKey) noexcept;

LLVMTypeRef mapNonBuiltinLlvmType(
    LLVMContextRef context,
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectOopMirModel& mirModel,
    std::string_view typeKey,
    std::string& error);

LLVMTypeRef mapLlvmType(
    LLVMContextRef context,
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectOopMirModel& mirModel,
    std::string_view typeKey,
    std::string& error);

void configureDebugModuleFlags(LLVMModuleRef module, LLVMContextRef context);

LLVMMetadataRef createDebugFile(
    LLVMDIBuilderRef debugBuilder,
    const source::SourceManager* sourceManager,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::string> sourceChecksums,
    std::unordered_map<std::uint32_t, LLVMMetadataRef>& debugFiles,
    std::unordered_map<std::uint32_t, unsigned>& debugCodeViewFileNumbers,
    SourceLocation location);

unsigned debugCodeViewFileNumberFor(
    LLVMDIBuilderRef debugBuilder,
    const source::SourceManager* sourceManager,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::string> sourceChecksums,
    std::unordered_map<std::uint32_t, LLVMMetadataRef>& debugFiles,
    std::unordered_map<std::uint32_t, unsigned>& debugCodeViewFileNumbers,
    SourceLocation location);

bool emitGlobalDebugInfo(
    DebugInformationKind debugInformation,
    LLVMDIBuilderRef debugBuilder,
    LLVMMetadataRef debugCompileUnit,
    LLVMContextRef context,
    const source::SourceManager* sourceManager,
    const std::function<LLVMMetadataRef(SourceLocation)>& debugFileLookup,
    const std::function<LLVMMetadataRef(std::string_view, std::string_view)>& debugTypeLookup,
    const frontend::ProjectOopMirStaticField& field,
    LLVMValueRef global,
    std::string& error);

bool emitThisDebugInfo(
    DebugInformationKind debugInformation,
    LLVMDIBuilderRef debugBuilder,
    LLVMMetadataRef currentDebugSubprogram,
    LLVMValueRef thisValue,
    LLVMContextRef context,
    LLVMBuilderRef builder,
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel* memberModel,
    const source::SourceManager* sourceManager,
    const std::function<LLVMMetadataRef(SourceLocation)>& debugFileLookup,
    const std::function<LLVMMetadataRef(std::string_view)>& debugTypeLookup,
    const std::function<bool(std::string_view)>& isStructValueLookup,
    const std::function<LLVMTypeRef(std::string_view, std::string&)>& llvmTypeLookup,
    const frontend::ProjectOopMirFunction& function,
    std::string& error);

bool emitCatchParameterDebugInfo(
    DebugInformationKind debugInformation,
    LLVMDIBuilderRef debugBuilder,
    LLVMContextRef context,
    LLVMBuilderRef builder,
    LLVMMetadataRef currentDebugSubprogram,
    LLVMMetadataRef currentDebugScope,
    const source::SourceManager* sourceManager,
    std::unordered_set<std::size_t>& debugDeclaredLocals,
    const std::function<LLVMMetadataRef(SourceLocation)>& debugFileLookup,
    const std::function<LLVMMetadataRef(std::string_view, std::string_view)>& debugTypeLookup,
    const frontend::ProjectOopMirNode* node,
    LLVMValueRef storage,
    std::string& error);

bool emitParameterDebugInfo(
    DebugInformationKind debugInformation,
    LLVMDIBuilderRef debugBuilder,
    LLVMContextRef context,
    LLVMBuilderRef builder,
    LLVMMetadataRef currentDebugSubprogram,
    const frontend::ProjectMemberModel* memberModel,
    const source::SourceManager* sourceManager,
    const std::function<LLVMMetadataRef(SourceLocation)>& debugFileLookup,
    const std::function<LLVMMetadataRef(std::string_view, frontend::ParameterPassKind,
                                        std::string_view)>& debugParameterTypeLookup,
    const std::function<LLVMMetadataRef(std::string_view, std::string_view)>&
        debugNamedValueTypeLookup,
    const std::function<LLVMMetadataRef(std::string_view, sema::OwnershipKind, bool,
                                        SourceRange)>& debugOwnershipTypeLookup,
    const frontend::ProjectOopMirFunction& function,
    std::size_t index,
    LLVMValueRef storage,
    std::string& error);

bool emitLocalDebugInfo(
    DebugInformationKind debugInformation,
    LLVMDIBuilderRef debugBuilder,
    LLVMContextRef context,
    LLVMBuilderRef builder,
    LLVMMetadataRef currentDebugSubprogram,
    LLVMMetadataRef currentDebugScope,
    const source::SourceManager* sourceManager,
    std::unordered_set<std::size_t>& debugDeclaredLocals,
    const std::function<LLVMMetadataRef(SourceLocation)>& debugFileLookup,
    const std::function<LLVMMetadataRef(std::string_view, std::size_t)>&
        debugVariableLengthArrayTypeLookup,
    const std::function<LLVMMetadataRef(std::string_view)>& debugPlacementTypeLookup,
    const std::function<LLVMMetadataRef(std::string_view, std::string_view)>&
        debugNamedValueTypeLookup,
    const std::function<LLVMMetadataRef(std::string_view, sema::OwnershipKind, bool,
                                        SourceRange)>& debugOwnershipTypeLookup,
    const frontend::ProjectOopMirNode* node,
    LLVMValueRef storage,
    std::string& error);

bool createDispatchTypes(
    LLVMContextRef context,
    LLVMTypeRef& interfaceMapEntryType,
    LLVMTypeRef& interfaceMapType,
    LLVMTypeRef& descriptorType,
    LLVMTypeRef& descriptorAccessorFunctionType,
    std::string& error);

void computeRuntimeDescriptorAccessorTypes(
    const frontend::ProjectOopMirModel& mirModel,
    std::unordered_set<std::size_t>& runtimeDescriptorAccessorTypes);

void applyOdrLinkage(LLVMModuleRef module, LLVMValueRef value);

bool emitVtableGlobal(
    LLVMModuleRef module,
    LLVMTypeRef ptr,
    const frontend::ProjectOopMirDispatchTable& table,
    const std::function<LLVMValueRef(std::string_view)>& functionLookup,
    std::unordered_map<std::size_t, LLVMValueRef>& vtableGlobals,
    std::string& error);

bool emitInterfaceMapGlobals(
    LLVMModuleRef module,
    LLVMTypeRef ptr,
    LLVMTypeRef i64,
    LLVMTypeRef interfaceMapEntryType,
    LLVMTypeRef interfaceMapType,
    const frontend::ProjectOopMirDispatchTable& table,
    const std::function<LLVMValueRef(std::string_view)>& functionLookup,
    std::unordered_map<std::size_t, LLVMValueRef>& interfaceMapGlobals,
    std::string& error);

LLVMValueRef materializeTypeNameConstant(
    LLVMModuleRef module,
    LLVMContextRef context,
    std::unordered_map<std::string, LLVMValueRef>& typeNameGlobals,
    const std::string& globalName,
    const std::string& text);

bool emitDescriptorInitializer(
    LLVMModuleRef module,
    LLVMTypeRef ptr,
    LLVMTypeRef i64,
    LLVMTypeRef descriptorType,
    const frontend::ProjectOopMirDispatchTable& table,
    const std::unordered_set<std::size_t>& runtimeDescriptorAccessorTypes,
    const std::unordered_map<std::size_t, LLVMValueRef>& descriptorGlobals,
    const std::unordered_map<std::size_t, LLVMValueRef>& vtableGlobals,
    const std::function<LLVMValueRef(std::size_t)>& destructionFunctionLookup,
    const std::function<LLVMValueRef(std::size_t)>& objectDestructionFunctionLookup,
    const std::unordered_map<std::size_t, LLVMValueRef>& interfaceMapGlobals,
    const std::function<LLVMValueRef(const std::string&, const std::string&)>& typeNameLookup,
    std::string& error);

bool emitDescriptorAccessorBody(
    LLVMContextRef context,
    LLVMTypeRef descriptorType,
    LLVMTypeRef ptr,
    LLVMValueRef accessor,
    LLVMValueRef descriptor,
    const frontend::ProjectOopMirDispatchTable& table,
    bool runtimeDescriptor,
    const std::function<bool(std::size_t, LLVMTypeRef&, LLVMValueRef&)>& baseAccessorLookup,
    std::string& error);

bool decimalCoefficientAndScale(double value, std::uint64_t& coefficient, int& scale) noexcept;

LLVMValueRef applyFloatingPointMode(LLVMValueRef instruction, bool contractable = false) noexcept;

FloatingPointState sourceFloatingPointState(
    ast::FloatingPointModeSyntaxKind mode,
    FloatingPointState inherited) noexcept;

bool isSignedInteger(std::string_view type) noexcept;

unsigned integerWidth(std::string_view type) noexcept;

bool isComparison(lexer::TokenKind kind) noexcept;

LLVMIntPredicate integerPredicate(lexer::TokenKind kind, bool signedOperands);

LLVMRealPredicate floatingPredicate(lexer::TokenKind kind);
}
