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
bool resolveBaseDescriptor(
    LLVMTypeRef ptr,
    const frontend::ProjectOopMirDispatchTable& table,
    const std::unordered_set<std::size_t>& runtimeDescriptorAccessorTypes,
    const std::unordered_map<std::size_t, LLVMValueRef>& descriptorGlobals,
    LLVMValueRef& baseDescriptor,
    std::string& error)
{
    baseDescriptor = LLVMConstNull(ptr);
    if (!table.baseTypeOrdinal.has_value() ||
        runtimeDescriptorAccessorTypes.contains(table.typeOrdinal))
        return true;
    const auto baseFound = descriptorGlobals.find(*table.baseTypeOrdinal);
    if (baseFound != descriptorGlobals.end())
    {
        baseDescriptor = baseFound->second;
        return true;
    }
    error = "Macro A descriptor cannot resolve its local base descriptor";
    return false;
}

bool resolveDestructionFunctions(
    const frontend::ProjectOopMirDispatchTable& table,
    const std::function<LLVMValueRef(std::size_t)>& destructionFunctionLookup,
    const std::function<LLVMValueRef(std::size_t)>& objectDestructionFunctionLookup,
    LLVMValueRef& destructionFunction,
    LLVMValueRef& objectDestructionFunction,
    std::string& error)
{
    destructionFunction = destructionFunctionLookup(table.typeOrdinal);
    if (destructionFunction == nullptr)
    {
        error = "Stage 35 descriptor cannot resolve its destruction entry";
        return false;
    }
    objectDestructionFunction = objectDestructionFunctionLookup(table.typeOrdinal);
    if (objectDestructionFunction != nullptr) return true;
    error = "item 319 descriptor cannot resolve its object-only destruction entry";
    return false;
}
}

// NSC305-L03. Creates the dispatch metadata carrier types (moved out of the lowerer's
// emitDispatchMetadata as a pure move).
bool createDispatchTypes(
    LLVMContextRef context,
    LLVMTypeRef& interfaceMapEntryType,
    LLVMTypeRef& interfaceMapType,
    LLVMTypeRef& descriptorType,
    LLVMTypeRef& descriptorAccessorFunctionType,
    std::string& error)
{
    LLVMTypeRef ptr = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef interfaceEntryFields[] = {i64, ptr};
    interfaceMapEntryType = LLVMStructCreateNamed(context, "nativesharp.interface_map_entry");
    if (interfaceMapEntryType == nullptr)
    {
        error = "Stage 36 failed to create interface-map entry type";
        return false;
    }
    LLVMStructSetBody(interfaceMapEntryType, interfaceEntryFields, 2, 0);
    LLVMTypeRef interfaceMapFields[] = {i64, ptr};
    interfaceMapType = LLVMStructCreateNamed(context, "nativesharp.interface_map");
    if (interfaceMapType == nullptr)
    {
        error = "Stage 36 failed to create interface-map header type";
        return false;
    }
    LLVMStructSetBody(interfaceMapType, interfaceMapFields, 2, 0);
    LLVMTypeRef descriptorFields[] = {ptr, ptr, i64, i64, ptr, ptr, ptr, ptr};
    descriptorType = LLVMStructCreateNamed(context, "nativesharp.class_descriptor");
    if (descriptorType == nullptr)
    {
        error = "Macro A failed to create NativeSharp class-descriptor type";
        return false;
    }
    LLVMStructSetBody(descriptorType, descriptorFields, 8, 0);
    descriptorAccessorFunctionType = LLVMFunctionType(ptr, nullptr, 0, 0);
    if (descriptorAccessorFunctionType == nullptr)
    {
        error = "1.0-E1 failed to create class-descriptor accessor type";
        return false;
    }
    return true;
}

// NSC305-L03. Computes the transitive set of types whose descriptors are completed through runtime
// accessors (moved out of emitDispatchMetadata as a pure move).
void computeRuntimeDescriptorAccessorTypes(
    const frontend::ProjectOopMirModel& mirModel,
    std::unordered_set<std::size_t>& runtimeDescriptorAccessorTypes)
{
    for (const frontend::ProjectOopMirDispatchTable& table : mirModel.dispatchTables())
    {
        if (table.isImported) runtimeDescriptorAccessorTypes.insert(table.typeOrdinal);
    }
    bool addedRuntimeDescriptor = true;
    while (addedRuntimeDescriptor)
    {
        addedRuntimeDescriptor = false;
        for (const frontend::ProjectOopMirDispatchTable& table : mirModel.dispatchTables())
        {
            if (table.isImported || !table.baseTypeOrdinal.has_value() ||
                !runtimeDescriptorAccessorTypes.contains(*table.baseTypeOrdinal))
            {
                continue;
            }
            addedRuntimeDescriptor = runtimeDescriptorAccessorTypes.insert(table.typeOrdinal).second ||
                addedRuntimeDescriptor;
        }
    }
}

// NSC305-L03. Applies the weak_odr linkage + COMDAT to a public ODR entity (moved out of the
// lowerer's applyOdrDefinition as a pure move).
void applyOdrLinkage(LLVMModuleRef module, LLVMValueRef value)
{
    if (value == nullptr) return;
    LLVMSetLinkage(value, LLVMWeakODRLinkage);
    std::size_t physicalNameLength = 0;
    const char* physicalNameData = LLVMGetValueName2(value, &physicalNameLength);
    if (physicalNameData == nullptr || physicalNameLength == 0) return;
    const std::string physicalName(physicalNameData, physicalNameLength);
    LLVMComdatRef comdat = LLVMGetOrInsertComdat(module, physicalName.c_str());
    LLVMSetComdatSelectionKind(comdat, LLVMAnyComdatSelectionKind);
    LLVMSetComdat(value, comdat);
}

// NSC305-L03. Emits a dispatch table's vtable global (moved out of emitDispatchMetadata as a pure
// move). The function lookup is a callback so the private FunctionInfo map stays in the lowerer.
bool emitVtableGlobal(
    LLVMModuleRef module,
    LLVMTypeRef ptr,
    const frontend::ProjectOopMirDispatchTable& table,
    const std::function<LLVMValueRef(std::string_view)>& functionLookup,
    std::unordered_map<std::size_t, LLVMValueRef>& vtableGlobals,
    std::string& error)
{
    LLVMValueRef vtableGlobal = nullptr;
    if (!table.slotCalleeNames.empty())
    {
        LLVMTypeRef arrayType = LLVMArrayType2(ptr, table.slotCalleeNames.size());
        vtableGlobal = LLVMAddGlobal(module, arrayType, table.vtableName.c_str());
        if (vtableGlobal == nullptr)
        {
            error = "Macro A failed to declare vtable global " + table.vtableName;
            return false;
        }
        std::vector<LLVMValueRef> entries;
        entries.reserve(table.slotCalleeNames.size());
        for (const std::string& callee : table.slotCalleeNames)
        {
            if (callee.empty())
            {
                entries.push_back(LLVMConstNull(ptr));
                continue;
            }
            LLVMValueRef target = functionLookup(callee);
            if (target == nullptr)
            {
                error = "Stage 35 vtable target was not declared: " + callee;
                return false;
            }
            entries.push_back(target);
        }
        LLVMValueRef initializer = LLVMConstArray2(
            ptr, entries.data(), static_cast<std::uint64_t>(entries.size()));
        LLVMSetInitializer(vtableGlobal, initializer);
        LLVMSetGlobalConstant(vtableGlobal, 1);
        if (!table.genericSpecializationKey.empty())
            applyOdrLinkage(module, vtableGlobal);
        vtableGlobals[table.typeOrdinal] = vtableGlobal;
    }
    return true;
}

bool emitInterfaceMethodTable(
    LLVMModuleRef module,
    LLVMTypeRef ptr,
    LLVMTypeRef i64,
    LLVMTypeRef interfaceMapEntryType,
    const frontend::ProjectOopMirDispatchTable& table,
    const frontend::ProjectOopMirDispatchTable::InterfaceTable& interfaceTable,
    const std::function<LLVMValueRef(std::string_view)>& functionLookup,
    std::vector<LLVMValueRef>& interfaceMapEntries,
    std::string& error)
{
    LLVMTypeRef methodArrayType = LLVMArrayType2(ptr, interfaceTable.calleeNames.size());
    LLVMValueRef methodTable = LLVMAddGlobal(
        module, methodArrayType, interfaceTable.methodTableName.c_str());
    if (methodTable == nullptr)
    {
        error = "Stage 36 failed to declare interface method table " +
            interfaceTable.methodTableName;
        return false;
    }
    std::vector<LLVMValueRef> methods;
    methods.reserve(interfaceTable.calleeNames.size());
    for (const std::string& callee : interfaceTable.calleeNames)
    {
        if (callee.empty())
        {
            methods.push_back(LLVMConstNull(ptr));
            continue;
        }
        LLVMValueRef target = functionLookup(callee);
        if (target == nullptr)
        {
            error = "Stage 36 interface implementation was not declared: " + callee;
            return false;
        }
        methods.push_back(target);
    }
    LLVMValueRef methodInitializer = LLVMConstArray2(
        ptr, methods.data(), static_cast<std::uint64_t>(methods.size()));
    LLVMSetInitializer(methodTable, methodInitializer);
    LLVMSetGlobalConstant(methodTable, 1);
    if (!table.genericSpecializationKey.empty())
        applyOdrLinkage(module, methodTable);

    LLVMValueRef entryFields[] = {
        LLVMConstInt(i64, interfaceTable.stableInterfaceTypeId, 0),
        methodTable
    };
    interfaceMapEntries.push_back(
        LLVMConstNamedStruct(interfaceMapEntryType, entryFields, 2));
    return true;
}

// NSC305-L03. Emits a dispatch table's interface-map globals (moved out of emitDispatchMetadata as a
// pure move). The function lookup is a callback so the private FunctionInfo map stays in the lowerer.
bool emitInterfaceMapGlobals(
    LLVMModuleRef module,
    LLVMTypeRef ptr,
    LLVMTypeRef i64,
    LLVMTypeRef interfaceMapEntryType,
    LLVMTypeRef interfaceMapType,
    const frontend::ProjectOopMirDispatchTable& table,
    const std::function<LLVMValueRef(std::string_view)>& functionLookup,
    std::unordered_map<std::size_t, LLVMValueRef>& interfaceMapGlobals,
    std::string& error)
{
    std::vector<LLVMValueRef> interfaceMapEntries;
    for (const frontend::ProjectOopMirDispatchTable::InterfaceTable& interfaceTable :
         table.interfaceTables)
    {
        if (!emitInterfaceMethodTable(
                module, ptr, i64, interfaceMapEntryType, table,
                interfaceTable, functionLookup, interfaceMapEntries, error))
            return false;
    }
    if (!interfaceMapEntries.empty())
    {
        LLVMTypeRef entriesArrayType = LLVMArrayType2(
            interfaceMapEntryType, interfaceMapEntries.size());
        const std::string entriesName = table.interfaceMapName + "__entries";
        LLVMValueRef entriesGlobal = LLVMAddGlobal(
            module, entriesArrayType, entriesName.c_str());
        if (entriesGlobal == nullptr)
        {
            error = "Stage 36 failed to declare interface-map entries";
            return false;
        }
        LLVMValueRef entriesInitializer = LLVMConstArray2(
            interfaceMapEntryType, interfaceMapEntries.data(),
            static_cast<std::uint64_t>(interfaceMapEntries.size()));
        LLVMSetInitializer(entriesGlobal, entriesInitializer);
        LLVMSetGlobalConstant(entriesGlobal, 1);
        if (!table.genericSpecializationKey.empty())
            applyOdrLinkage(module, entriesGlobal);

        LLVMValueRef mapGlobal = LLVMAddGlobal(
            module, interfaceMapType, table.interfaceMapName.c_str());
        if (mapGlobal == nullptr)
        {
            error = "Stage 36 failed to declare interface-map header";
            return false;
        }
        LLVMValueRef mapFields[] = {
            LLVMConstInt(i64, interfaceMapEntries.size(), 0),
            entriesGlobal
        };
        LLVMSetInitializer(mapGlobal, LLVMConstNamedStruct(interfaceMapType, mapFields, 2));
        LLVMSetGlobalConstant(mapGlobal, 1);
        if (!table.genericSpecializationKey.empty())
            applyOdrLinkage(module, mapGlobal);
        interfaceMapGlobals[table.typeOrdinal] = mapGlobal;
    }
    return true;
}

// NSC305-L03. Materializes (or returns the cached) type-name constant (moved out of the lowerer's
// internTypeNameConstant as a pure move).
LLVMValueRef materializeTypeNameConstant(
    LLVMModuleRef module,
    LLVMContextRef context,
    std::unordered_map<std::string, LLVMValueRef>& typeNameGlobals,
    const std::string& globalName,
    const std::string& text)
{
    const auto found = typeNameGlobals.find(globalName);
    if (found != typeNameGlobals.end()) return found->second;
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef i8 = LLVMInt8TypeInContext(context);
    LLVMTypeRef bytesType = LLVMArrayType2(i8, text.size() + 1U);
    LLVMTypeRef literalType = LLVMStructCreateNamed(context, (globalName + "_type").c_str());
    LLVMTypeRef fields[] = {i64, bytesType};
    LLVMStructSetBody(literalType, fields, 2, 0);
    LLVMValueRef values[] = {
        LLVMConstInt(i64, static_cast<std::uint64_t>(text.size()), 0),
        LLVMConstStringInContext2(context, text.data(), text.size(), 0)};
    LLVMValueRef initializer = LLVMConstNamedStruct(literalType, values, 2);
    LLVMValueRef global = LLVMAddGlobal(module, literalType, globalName.c_str());
    if (global == nullptr || initializer == nullptr) return nullptr;
    LLVMSetInitializer(global, initializer);
    LLVMSetGlobalConstant(global, 1);
    typeNameGlobals.emplace(globalName, global);
    return global;
}

// NSC305-L03. Emits a class descriptor's initializer (moved out of emitDispatchMetadata as a pure
// move). The destruction/type-name lookups are callbacks so the private maps stay in the lowerer.
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
    std::string& error)
{
    const auto descriptorFound = descriptorGlobals.find(table.typeOrdinal);
    if (descriptorFound == descriptorGlobals.end())
    {
        error = "Macro A lost a class descriptor during initialization";
        return false;
    }
    LLVMValueRef baseDescriptor = nullptr;
    if (!resolveBaseDescriptor(
            ptr, table, runtimeDescriptorAccessorTypes, descriptorGlobals,
            baseDescriptor, error)) return false;
    LLVMValueRef vtable = LLVMConstNull(ptr);
    const auto vtableFound = vtableGlobals.find(table.typeOrdinal);
    if (vtableFound != vtableGlobals.end()) vtable = vtableFound->second;

    LLVMValueRef destructionFunction = nullptr;
    LLVMValueRef objectDestructionFunction = nullptr;
    if (!resolveDestructionFunctions(
            table, destructionFunctionLookup, objectDestructionFunctionLookup,
            destructionFunction, objectDestructionFunction, error)) return false;
    LLVMValueRef interfaceMap = LLVMConstNull(ptr);
    const auto interfaceMapFound = interfaceMapGlobals.find(table.typeOrdinal);
    if (interfaceMapFound != interfaceMapGlobals.end())
        interfaceMap = interfaceMapFound->second;

    LLVMValueRef typeName = typeNameLookup(table.typeNameGlobalName, table.typeDisplayName);
    if (typeName == nullptr)
    {
        error = "C8 descriptor cannot materialize its type name constant";
        return false;
    }
    if (!table.genericSpecializationKey.empty())
        applyOdrLinkage(module, typeName);

    LLVMValueRef values[] = {
        baseDescriptor,
        vtable,
        LLVMConstInt(i64, table.slotMemberOrdinals.size(), 0),
        LLVMConstInt(i64, table.stableTypeId, 0),
        interfaceMap,
        destructionFunction,
        typeName,
        objectDestructionFunction
    };
    LLVMValueRef initializer = LLVMConstNamedStruct(descriptorType, values, 8);
    LLVMSetInitializer(descriptorFound->second, initializer);
    LLVMSetGlobalConstant(
        descriptorFound->second,
        runtimeDescriptorAccessorTypes.contains(table.typeOrdinal) ? 0 : 1);
    return true;
}

// NSC305-L03. Emits a descriptor accessor's body (moved out of emitDispatchMetadata as a pure
// move). The base accessor lookup is a callback so the private FunctionInfo map stays in the lowerer.
bool emitDescriptorAccessorBody(
    LLVMContextRef context,
    LLVMTypeRef descriptorType,
    LLVMTypeRef ptr,
    LLVMValueRef accessor,
    LLVMValueRef descriptor,
    const frontend::ProjectOopMirDispatchTable& table,
    bool runtimeDescriptor,
    const std::function<bool(std::size_t, LLVMTypeRef&, LLVMValueRef&)>& baseAccessorLookup,
    std::string& error)
{
    LLVMBasicBlockRef accessorEntry = LLVMAppendBasicBlockInContext(context, accessor, "entry");
    LLVMBuilderRef accessorBuilder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(accessorBuilder, accessorEntry);
    if (runtimeDescriptor)
    {
        if (!table.baseTypeOrdinal.has_value())
        {
            LLVMDisposeBuilder(accessorBuilder);
            error = "1.0-E1 runtime descriptor has no base type";
            return false;
        }
        LLVMTypeRef baseFunctionType = nullptr;
        LLVMValueRef baseFunction = nullptr;
        if (!baseAccessorLookup(*table.baseTypeOrdinal, baseFunctionType, baseFunction))
        {
            LLVMDisposeBuilder(accessorBuilder);
            error = "1.0-E1 runtime descriptor cannot resolve its base accessor";
            return false;
        }
        LLVMValueRef resolvedBase = LLVMBuildCall2(
            accessorBuilder, baseFunctionType, baseFunction, nullptr, 0, "base.classdesc");
        LLVMValueRef baseField = LLVMBuildStructGEP2(
            accessorBuilder, descriptorType, descriptor, 0, "base.classdesc.addr");
        LLVMBuildAtomicCmpXchg(
            accessorBuilder, baseField, LLVMConstNull(ptr), resolvedBase,
            LLVMAtomicOrderingSequentiallyConsistent,
            LLVMAtomicOrderingSequentiallyConsistent, 0);
    }
    LLVMBuildRet(accessorBuilder, descriptor);
    LLVMDisposeBuilder(accessorBuilder);
    return true;
}

}
