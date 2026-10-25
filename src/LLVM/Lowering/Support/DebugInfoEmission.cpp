#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/DebugMetadataVersion.h"

#include "../../LlvmCApi.h"
#include "../../SignedDivisionCorrection.h"

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

#include "../OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

namespace
{
bool insertDebugStorage(
    LLVMDIBuilderRef debugBuilder,
    LLVMBuilderRef builder,
    LLVMValueRef storage,
    LLVMMetadataRef variable,
    LLVMMetadataRef expression,
    LLVMMetadataRef location,
    std::string_view createFailure,
    std::string_view emitFailure,
    std::string_view name,
    std::string& error)
{
    if (variable == nullptr || expression == nullptr || location == nullptr)
    {
        error = std::string(createFailure) + std::string(name) + "'";
        return false;
    }
    if (LLVMDIBuilderInsertDeclareRecordAtEnd(
            debugBuilder, storage, variable, expression, location,
            LLVMGetInsertBlock(builder)) != nullptr)
        return true;
    error = std::string(emitFailure) + std::string(name) + "'";
    return false;
}
}

// NSC305-L03. Adds the module-level debug flags (moved out of initializeDebugInformation as a pure
// move).
void configureDebugModuleFlags(LLVMModuleRef module, LLVMContextRef context)
{
    LLVMSetIsNewDbgInfoFormat(module, 1);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMMetadataRef debugVersion = LLVMValueAsMetadata(
        LLVMConstInt(i32, LLVMDebugMetadataVersion(), 0));
    LLVMMetadataRef codeView = LLVMValueAsMetadata(LLVMConstInt(i32, 1, 0));
    LLVMMetadataRef nativeSharpDebugMajor = LLVMValueAsMetadata(
        LLVMConstInt(i32, kDebugMetadataMajor, 0));
    LLVMMetadataRef nativeSharpDebugMinor = LLVMValueAsMetadata(
        LLVMConstInt(i32, kDebugMetadataMinor, 0));
    LLVMAddModuleFlag(
        module, LLVMModuleFlagBehaviorWarning, "Debug Info Version", 18, debugVersion);
    LLVMAddModuleFlag(module, LLVMModuleFlagBehaviorWarning, "CodeView", 8, codeView);
    LLVMAddModuleFlag(
        module, LLVMModuleFlagBehaviorError,
        "NativeSharp Debug Metadata Major", 32, nativeSharpDebugMajor);
    LLVMAddModuleFlag(
        module, LLVMModuleFlagBehaviorWarning,
        "NativeSharp Debug Metadata Minor", 32, nativeSharpDebugMinor);
}

// NSC305-L03. Creates (or returns a cached) DIFile for a source location (moved out of the lowerer's
// debugFile as a pure move).
LLVMMetadataRef createDebugFile(
    LLVMDIBuilderRef debugBuilder,
    const source::SourceManager* sourceManager,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::string> sourceChecksums,
    std::unordered_map<std::uint32_t, LLVMMetadataRef>& debugFiles,
    std::unordered_map<std::uint32_t, unsigned>& debugCodeViewFileNumbers,
    SourceLocation location)
{
    if (debugBuilder == nullptr || sourceManager == nullptr || !location.isValid())
        return nullptr;
    const std::uint32_t fileKey = location.fileId().value();
    const auto existing = debugFiles.find(fileKey);
    if (existing != debugFiles.end()) return existing->second;
    const source::SourceFile* sourceFile = sourceManager->getFile(location.fileId());
    if (sourceFile == nullptr) return nullptr;
    const std::string normalized = remapDebugSourcePath(sourceFile->path(), debugPrefixMaps);
    const std::size_t separator = normalized.find_last_of('/');
    const std::string filename = separator == std::string::npos
        ? normalized
        : normalized.substr(separator + 1);
    const std::string directory = separator == std::string::npos
        ? std::string()
        : normalized.substr(0, separator == 0 ? 1 : separator);
    // A checksum computed once per compilation for all code units is used when present; it is
    // the same sha256Hex(sha256(buffer)) computed here otherwise.
    std::string checksum;
    if (fileKey < sourceChecksums.size() && !sourceChecksums[fileKey].empty())
        checksum = sourceChecksums[fileKey];
    else
    {
        const std::string_view sourceBytes = sourceFile->buffer();
        const auto bytes = std::span(
            reinterpret_cast<const std::uint8_t*>(sourceBytes.data()), sourceBytes.size());
        checksum = basic::sha256Hex(basic::sha256(bytes));
    }
    LLVMMetadataRef file = LLVMDIBuilderCreateFileWithChecksum(
        debugBuilder, filename.data(), filename.size(), directory.data(), directory.size(),
        CSK_SHA256, checksum.data(), checksum.size(), nullptr, 0);
    if (file != nullptr)
    {
        debugFiles.emplace(fileKey, file);
        debugCodeViewFileNumbers.emplace(
            fileKey, static_cast<unsigned>(debugCodeViewFileNumbers.size() + 1));
    }
    return file;
}

// NSC305-L03. Resolves the CodeView file number for a location (moved out of the lowerer).
unsigned debugCodeViewFileNumberFor(
    LLVMDIBuilderRef debugBuilder,
    const source::SourceManager* sourceManager,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::string> sourceChecksums,
    std::unordered_map<std::uint32_t, LLVMMetadataRef>& debugFiles,
    std::unordered_map<std::uint32_t, unsigned>& debugCodeViewFileNumbers,
    SourceLocation location)
{
    if (debugBuilder == nullptr || !location.isValid()) return 0;
    const std::uint32_t fileKey = location.fileId().value();
    if (debugFiles.find(fileKey) == debugFiles.end() &&
        createDebugFile(debugBuilder, sourceManager, debugPrefixMaps, sourceChecksums, debugFiles,
                        debugCodeViewFileNumbers, location) == nullptr)
        return 0;
    const auto found = debugCodeViewFileNumbers.find(fileKey);
    return found == debugCodeViewFileNumbers.end() ? 0 : found->second;
}

// NSC305-L03. Emits the debug metadata for a static field (moved out of the lowerer's
// emitDebugGlobal as a pure move).
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
    std::string& error)
{
    if (debugInformation != DebugInformationKind::Full || debugBuilder == nullptr ||
        debugCompileUnit == nullptr || global == nullptr || field.isImported ||
        !field.sourceRange.isValid())
        return true;
    LLVMMetadataRef file = debugFileLookup(field.sourceRange.begin());
    const std::string& displayName =
        field.debugDisplayName.empty() ? field.name : field.debugDisplayName;
    LLVMMetadataRef type = debugTypeLookup(field.typeKey, displayName);
    LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(debugBuilder, nullptr, 0);
    if (file == nullptr || type == nullptr || expression == nullptr) return true;
    const source::SourcePosition position =
        sourceManager->getPosition(field.sourceRange.begin());
    LLVMMetadataRef globalExpression = LLVMDIBuilderCreateGlobalVariableExpression(
        debugBuilder, debugCompileUnit, displayName.data(), displayName.size(),
        field.storageSymbol.data(), field.storageSymbol.size(),
        file, position.line, type, 0, expression, nullptr, 0);
    if (globalExpression == nullptr)
    {
        error = "failed to create debug metadata for global '" + displayName + "'";
        return false;
    }
    const unsigned debugKind = LLVMGetMDKindIDInContext(context, "dbg", 3);
    LLVMGlobalSetMetadata(global, debugKind, globalExpression);
    return true;
}

// NSC305-L03. Emits the debug metadata for the `this` receiver (moved out of the lowerer's
// emitDebugThis as a pure move).
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
    std::string& error)
{
    if (debugInformation != DebugInformationKind::Full || function.isStatic ||
        function.isLambda || currentDebugSubprogram == nullptr)
        return true;
    if (thisValue == nullptr || function.declaringTypeOrdinal >= nameLookup.namedTypes().size())
    {
        error = "debug receiver is missing its LLVM value or declaring type";
        return false;
    }
    if (memberModel == nullptr || function.memberOrdinal >= memberModel->members().size())
        return true;
    const frontend::ClassMemberSymbol& member = memberModel->members()[function.memberOrdinal];
    if (!member.range.isValid()) return true;
    const std::string thisTypeKey =
        "type:" + nameLookup.namedTypes()[function.declaringTypeOrdinal].identity.key();
    LLVMMetadataRef type = debugTypeLookup(thisTypeKey);
    LLVMMetadataRef file = debugFileLookup(member.range.begin());
    if (type == nullptr || file == nullptr) return true;
    const source::SourcePosition position = sourceManager->getPosition(member.range.begin());

    LLVMValueRef storage = thisValue;
    if (!isStructValueLookup(thisTypeKey))
    {
        LLVMTypeRef llvmThisType = llvmTypeLookup(thisTypeKey, error);
        if (llvmThisType == nullptr) return false;
        storage = LLVMBuildAlloca(builder, llvmThisType, "this.addr");
        LLVMBuildStore(builder, thisValue, storage);
    }
    const auto flags = static_cast<LLVMDIFlags>(LLVMDIFlagArtificial | LLVMDIFlagObjectPointer);
    static constexpr std::string_view thisName = "this";
    LLVMMetadataRef variable = LLVMDIBuilderCreateParameterVariable(
        debugBuilder, currentDebugSubprogram, thisName.data(), thisName.size(), 0,
        file, position.line, type, 1, flags);
    LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(debugBuilder, nullptr, 0);
    LLVMMetadataRef location = LLVMDIBuilderCreateDebugLocation(
        context, position.line, position.column, currentDebugSubprogram, nullptr);
    if (variable == nullptr || expression == nullptr || location == nullptr)
    {
        error = "failed to create debug metadata for 'this'";
        return false;
    }
    if (LLVMDIBuilderInsertDeclareRecordAtEnd(
            debugBuilder, storage, variable, expression, location,
            LLVMGetInsertBlock(builder)) == nullptr)
    {
        error = "failed to emit debug storage for 'this'";
        return false;
    }
    return true;
}

// NSC305-L03. Emits the debug metadata for a catch parameter (moved out of the lowerer's
// emitDebugCatchParameter as a pure move).
//
// ES: La variable del `catch` es un local del bloque `catch`, no un parámetro del método: el cuerpo
// del `catch` es código ordinario de la función padre (no un funclet con argumentos). Se describe
// como variable automática del ámbito léxico del `catch`. Como parámetro, dos `try` con variable
// de `catch` en el mismo método recibían el mismo número de argumento y el verificador de LLVM
// rechazaba el módulo con `-g`; además CodeView la marcaba `param` y el depurador la mostraba entre
// los argumentos.
// EN: The `catch` variable is a local of the `catch` block, not a parameter of the method: the
// `catch` body is ordinary code of the parent function (not a funclet with arguments). It is
// described as an automatic variable of the `catch` lexical scope. As a parameter, two `try`
// statements with a `catch` variable in the same method received the same argument number and the
// LLVM verifier rejected the module under `-g`; CodeView also flagged it `param` and the debugger
// listed it among the arguments.
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
    std::string& error)
{
    if (debugInformation != DebugInformationKind::Full || currentDebugSubprogram == nullptr ||
        node == nullptr || storage == nullptr || node->stringValue.empty() ||
        !node->localOrdinal.has_value())
        return true;
    if (!debugDeclaredLocals.insert(*node->localOrdinal).second) return true;
    if (node->syntax == nullptr || !node->syntax->range().isValid()) return true;

    const SourceRange range = node->syntax->range();
    LLVMMetadataRef file = debugFileLookup(range.begin());
    LLVMMetadataRef type = debugTypeLookup(node->elementTypeKey, node->stringValue);
    LLVMMetadataRef scope =
        currentDebugScope != nullptr ? currentDebugScope : currentDebugSubprogram;
    if (file == nullptr || type == nullptr || scope == nullptr) return true;
    const source::SourcePosition position = sourceManager->getPosition(range.begin());
    LLVMMetadataRef variable = LLVMDIBuilderCreateAutoVariable(
        debugBuilder, scope, node->stringValue.data(), node->stringValue.size(),
        file, position.line, type, 1, LLVMDIFlagZero, 0);
    LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(debugBuilder, nullptr, 0);
    LLVMMetadataRef location = LLVMDIBuilderCreateDebugLocation(
        context, position.line, position.column, scope, nullptr);
    if (variable == nullptr || expression == nullptr || location == nullptr)
    {
        error = "failed to create debug metadata for catch parameter '" +
            node->stringValue + "'";
        return false;
    }
    if (LLVMDIBuilderInsertDeclareRecordAtEnd(
            debugBuilder, storage, variable, expression, location,
            LLVMGetInsertBlock(builder)) == nullptr)
    {
        error = "failed to emit debug storage for catch parameter '" +
            node->stringValue + "'";
        return false;
    }
    return true;
}

// NSC305-L03. Emits the debug metadata for an ordinary parameter (moved out of the lowerer's
// emitDebugParameter as a pure move).
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
    std::string& error)
{
    if (debugInformation != DebugInformationKind::Full || currentDebugSubprogram == nullptr)
        return true;
    if (storage == nullptr || index >= function.parameterTypeKeys.size())
    {
        error = "debug parameter is missing its LLVM storage or type";
        return false;
    }
    SourceRange range;
    std::string name = index < function.parameterNames.size()
        ? function.parameterNames[index]
        : "p" + std::to_string(index);
    if (!function.isLambda && memberModel != nullptr &&
        function.memberOrdinal < memberModel->members().size())
    {
        const frontend::ClassMemberSymbol& member =
            memberModel->members()[function.memberOrdinal];
        range = index < member.parameters.size() ? member.parameters[index].range : member.range;
    }
    else if (function.body != nullptr && function.body->syntax != nullptr)
    {
        range = function.body->syntax->range();
    }
    if (!range.isValid()) return true;
    LLVMMetadataRef file = debugFileLookup(range.begin());
    const frontend::ParameterPassKind passKind = index < function.parameterPassKinds.size()
        ? function.parameterPassKinds[index]
        : frontend::ParameterPassKind::Value;
    const sema::OwnershipKind ownership = index < function.parameterOwnerships.size()
        ? function.parameterOwnerships[index]
        : sema::OwnershipKind::None;
    LLVMMetadataRef type = passKind != frontend::ParameterPassKind::Value
        ? debugParameterTypeLookup(function.parameterTypeKeys[index], passKind, name)
        : ownership == sema::OwnershipKind::None
            ? debugNamedValueTypeLookup(function.parameterTypeKeys[index], name)
            : debugOwnershipTypeLookup(
                function.parameterTypeKeys[index], ownership, false, range);
    if (file == nullptr || type == nullptr) return true;
    const source::SourcePosition position = sourceManager->getPosition(range.begin());
    LLVMMetadataRef variable = LLVMDIBuilderCreateParameterVariable(
        debugBuilder, currentDebugSubprogram, name.data(), name.size(),
        static_cast<unsigned>(index + 1), file, position.line, type, 1, LLVMDIFlagZero);
    LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(debugBuilder, nullptr, 0);
    LLVMMetadataRef location = LLVMDIBuilderCreateDebugLocation(
        context, position.line, position.column, currentDebugSubprogram, nullptr);
    return insertDebugStorage(
        debugBuilder, builder, storage, variable, expression, location,
        "failed to create debug metadata for parameter '",
        "failed to emit debug storage for parameter '", name, error);
}

// NSC305-L03. Emits the debug metadata for a local (moved out of the lowerer's emitDebugLocal as a
// pure move).
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
    std::string& error)
{
    if (debugInformation != DebugInformationKind::Full || currentDebugSubprogram == nullptr ||
        node == nullptr || storage == nullptr || node->stringValue.empty() ||
        !node->localOrdinal.has_value())
        return true;
    if (!debugDeclaredLocals.insert(*node->localOrdinal).second) return true;
    if (node->syntax == nullptr || !node->syntax->range().isValid()) return true;
    const SourceRange range = node->syntax->range();
    LLVMMetadataRef file = debugFileLookup(range.begin());
    const std::string referenceTypeKey =
        node->referenceLocal ? node->typeKey + "*" : std::string{};
    const std::string_view typeKey = node->referenceLocal
        ? std::string_view(referenceTypeKey)
        : (node->kind == frontend::ProjectOopMirNodeKind::UsingRegion ||
           node->kind == frontend::ProjectOopMirNodeKind::CatchRegion
            ? std::string_view(node->elementTypeKey)
            : std::string_view(node->typeKey));
    const sema::OwnershipKind debugOwnership = node->ownership != sema::OwnershipKind::None
        ? node->ownership
        : node->objectLifetimeOwnership;
    LLVMMetadataRef type = node->variableLengthArray
        ? debugVariableLengthArrayTypeLookup(
            node->elementTypeKey, node->variableLengthArrayRank)
        : node->placementConstructed
            ? debugPlacementTypeLookup(typeKey)
            : debugOwnership == sema::OwnershipKind::None && !node->automaticDestruction
                ? debugNamedValueTypeLookup(typeKey, node->stringValue)
                : debugOwnershipTypeLookup(
                    typeKey, debugOwnership, node->automaticDestruction, range);
    if (file == nullptr || type == nullptr) return true;
    const source::SourcePosition position = sourceManager->getPosition(range.begin());
    LLVMMetadataRef scope =
        currentDebugScope != nullptr ? currentDebugScope : currentDebugSubprogram;
    const std::string_view debugName = node->debugDisplayName.empty()
        ? std::string_view(node->stringValue)
        : std::string_view(node->debugDisplayName);
    LLVMMetadataRef variable = LLVMDIBuilderCreateAutoVariable(
        debugBuilder, scope, debugName.data(), debugName.size(), file, position.line, type, 1,
        LLVMDIFlagZero, 0);
    LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(debugBuilder, nullptr, 0);
    LLVMMetadataRef location = LLVMDIBuilderCreateDebugLocation(
        context, position.line, position.column, scope, nullptr);
    return insertDebugStorage(
        debugBuilder, builder, storage, variable, expression, location,
        "failed to create debug metadata for local '",
        "failed to emit debug storage for local '", debugName, error);
}

}
