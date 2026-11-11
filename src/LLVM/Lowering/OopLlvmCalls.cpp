#include "nativesharp/Interop/CppAbiPlanQueries.h"
#include "../Internal/CppSysVRecordAbi.h"
#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/CppBoolAbi.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"
#include "../Internal/CppTlsDllAlias.h"
#include "../Internal/CppDllExportHelper.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

// Find an export in its own DLL when another DLL exports the same C symbol.
// Pin the module before returning its address, so it cannot be unloaded while
// generated code uses the TLS template. The loader handles concurrent loads.
LLVMValueRef cppDllTlsExportHelper(LLVMContextRef context, LLVMModuleRef module)
{
    constexpr const char* name = "__nsc_cpp_dll_tls_export";
    if (auto existing = LLVMGetNamedFunction(module, name)) return existing;
    const auto ptr = LLVMPointerTypeInContext(context, 0);
    const auto i16 = LLVMInt16TypeInContext(context);
    const auto i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef arguments[] = {ptr, ptr};
    const auto type = LLVMFunctionType(ptr, arguments, 2, 0);
    const auto helper = LLVMAddFunction(module, name, type);
    LLVMSetLinkage(helper, LLVMInternalLinkage);
    LLVMTypeRef moduleArgs[] = {i32, ptr, ptr};
    const auto moduleType = LLVMFunctionType(i32, moduleArgs, 3, 0);
    auto getModule = LLVMGetNamedFunction(module, "GetModuleHandleExA");
    if (!getModule) getModule = LLVMAddFunction(module, "GetModuleHandleExA", moduleType);
    LLVMSetDLLStorageClass(getModule, LLVMDLLImportStorageClass);
    auto getModuleW = LLVMGetNamedFunction(module, "GetModuleHandleExW");
    if (!getModuleW) getModuleW = LLVMAddFunction(module, "GetModuleHandleExW", moduleType);
    LLVMSetDLLStorageClass(getModuleW, LLVMDLLImportStorageClass);
    LLVMTypeRef pathArgs[] = {ptr, ptr, i32};
    const auto pathType = LLVMFunctionType(i32, pathArgs, 3, 0);
    auto getPath = LLVMGetNamedFunction(module, "GetModuleFileNameW");
    if (!getPath) getPath = LLVMAddFunction(module, "GetModuleFileNameW", pathType);
    LLVMSetDLLStorageClass(getPath, LLVMDLLImportStorageClass);
    LLVMTypeRef convertArgs[] = {i32, i32, ptr, i32, ptr, i32};
    const auto convertType = LLVMFunctionType(i32, convertArgs, 6, 0);
    auto convert = LLVMGetNamedFunction(module, "MultiByteToWideChar");
    if (!convert) convert = LLVMAddFunction(module, "MultiByteToWideChar", convertType);
    LLVMSetDLLStorageClass(convert, LLVMDLLImportStorageClass);
    LLVMTypeRef loadArgs[] = {ptr, ptr, i32};
    const auto loadType = LLVMFunctionType(ptr, loadArgs, 3, 0);
    auto load = LLVMGetNamedFunction(module, "LoadLibraryExW");
    if (!load) load = LLVMAddFunction(module, "LoadLibraryExW", loadType);
    LLVMSetDLLStorageClass(load, LLVMDLLImportStorageClass);
    const auto procType = LLVMFunctionType(ptr, arguments, 2, 0);
    auto proc = LLVMGetNamedFunction(module, "GetProcAddress");
    if (!proc) proc = LLVMAddFunction(module, "GetProcAddress", procType);
    LLVMSetDLLStorageClass(proc, LLVMDLLImportStorageClass);
    // EN: T08-D05: activation-context isolation of the homonymous transitive dependencies of an alias.
    // ES: T08-D05: aislamiento por contexto de activacion de las dependencias transitivas homonimas de un alias.
    const auto i64 = LLVMInt64TypeInContext(context);
    const auto declare = [&](const char* importName, LLVMTypeRef type)
    {
        auto function = LLVMGetNamedFunction(module, importName);
        if (!function) function = LLVMAddFunction(module, importName, type);
        LLVMSetDLLStorageClass(function, LLVMDLLImportStorageClass);
        return function;
    };
    LLVMTypeRef onePointer[] = {ptr};
    const auto attributesType = LLVMFunctionType(i32, onePointer, 1, 0);
    const auto getAttributes = declare("GetFileAttributesW", attributesType);
    const auto createContextType = LLVMFunctionType(ptr, onePointer, 1, 0);
    const auto createContext = declare("CreateActCtxW", createContextType);
    LLVMTypeRef activateArgs[] = {ptr, ptr};
    const auto activateType = LLVMFunctionType(i32, activateArgs, 2, 0);
    const auto activateContext = declare("ActivateActCtx", activateType);
    LLVMTypeRef deactivateArgs[] = {i32, i64};
    const auto deactivateType = LLVMFunctionType(i32, deactivateArgs, 2, 0);
    const auto deactivateContext = declare("DeactivateActCtx", deactivateType);
    const auto releaseType = LLVMFunctionType(LLVMVoidTypeInContext(context), onePointer, 1, 0);
    const auto releaseContext = declare("ReleaseActCtx", releaseType);
    const auto entry = LLVMAppendBasicBlockInContext(context, helper, "entry");
    const auto pathBlock = LLVMAppendBasicBlockInContext(context, helper, "path");
    const auto scanBlock = LLVMAppendBasicBlockInContext(context, helper, "scan");
    const auto characterBlock = LLVMAppendBasicBlockInContext(context, helper, "character");
    const auto convertBlock = LLVMAppendBasicBlockInContext(context, helper, "convert");
    const auto findBlock = LLVMAppendBasicBlockInContext(context, helper, "find");
    const auto loadBlock = LLVMAppendBasicBlockInContext(context, helper, "load");
    const auto plainBlock = LLVMAppendBasicBlockInContext(context, helper, "load.plain");
    const auto isolatedBlock = LLVMAppendBasicBlockInContext(context, helper, "load.isolated");
    const auto activatedBlock = LLVMAppendBasicBlockInContext(context, helper, "load.activated");
    const auto releaseFailBlock = LLVMAppendBasicBlockInContext(context, helper, "load.release.fail");
    const auto loadedBlock = LLVMAppendBasicBlockInContext(context, helper, "load.done");
    const auto pinBlock = LLVMAppendBasicBlockInContext(context, helper, "pin");
    const auto lookup = LLVMAppendBasicBlockInContext(context, helper, "lookup");
    const auto fail = LLVMAppendBasicBlockInContext(context, helper, "fail");
    const auto builder = LLVMCreateBuilderInContext(context);
    const auto zero = LLVMConstInt(i32, 0, 0);
    const auto one = LLVMConstInt(i32, 1, 0);
    const auto pin = LLVMConstInt(i32, 1, 0);
    const auto library = LLVMGetParam(helper, 0);
    const auto symbol = LLVMGetParam(helper, 1);
    LLVMPositionBuilderAtEnd(builder, entry);
    const auto slot = LLVMBuildAlloca(builder, ptr, "module.slot");
    const auto ownerSlot = LLVMBuildAlloca(builder, ptr, "owner.slot");
    const auto scanSlot = LLVMBuildAlloca(builder, i32, "scan.slot");
    const auto capacity = LLVMConstInt(i32, 32768, 0);
    const auto path = LLVMBuildArrayAlloca(builder, i16, capacity, "module.path");
    LLVMBuildStore(builder, LLVMConstNull(ptr), slot);
    LLVMValueRef ownerValues[] = {LLVMConstInt(i32, 6, 0), helper, ownerSlot};
    const auto ownerFound = LLVMBuildCall2(builder, moduleType, getModule, ownerValues, 3, "owner.found");
    LLVMBuildCondBr(builder, LLVMBuildICmp(builder, LLVMIntNE, ownerFound, zero, "owner.ok"), pathBlock, fail);
    LLVMPositionBuilderAtEnd(builder, pathBlock);
    const auto owner = LLVMBuildLoad2(builder, ptr, ownerSlot, "owner");
    LLVMValueRef pathValues[] = {owner, path, capacity};
    const auto length = LLVMBuildCall2(builder, pathType, getPath, pathValues, 3, "path.length");
    const auto nonempty = LLVMBuildICmp(builder, LLVMIntNE, length, zero, "path.nonempty");
    const auto fits = LLVMBuildICmp(builder, LLVMIntULT, length, capacity, "path.fits");
    LLVMBuildStore(builder, length, scanSlot);
    LLVMBuildCondBr(builder, LLVMBuildAnd(builder, nonempty, fits, "path.valid"), scanBlock, fail);
    LLVMPositionBuilderAtEnd(builder, scanBlock);
    const auto index = LLVMBuildLoad2(builder, i32, scanSlot, "scan.index");
    LLVMBuildCondBr(builder, LLVMBuildICmp(builder, LLVMIntNE, index, zero, "scan.valid"), characterBlock, fail);
    LLVMPositionBuilderAtEnd(builder, characterBlock);
    auto previous = LLVMBuildSub(builder, index, one, "scan.previous");
    LLVMBuildStore(builder, previous, scanSlot);
    const auto charPtr = LLVMBuildGEP2(builder, i16, path, &previous, 1, "scan.character.ptr");
    const auto character = LLVMBuildLoad2(builder, i16, charPtr, "scan.character");
    LLVMBuildCondBr(builder, LLVMBuildICmp(builder, LLVMIntEQ, character, LLVMConstInt(i16, '\\', 0), "scan.separator"), convertBlock, scanBlock);
    LLVMPositionBuilderAtEnd(builder, convertBlock);
    auto afterSeparator = LLVMBuildAdd(builder, previous, one, "directory.length");
    const auto suffix = LLVMBuildGEP2(builder, i16, path, &afterSeparator, 1, "path.suffix");
    const auto remaining = LLVMBuildSub(builder, capacity, afterSeparator, "path.remaining");
    LLVMValueRef convertValues[] = {LLVMConstInt(i32, 65001, 0), LLVMConstInt(i32, 8, 0),
                                    library, LLVMConstInt(i32, -1, 1), suffix, remaining};
    const auto converted = LLVMBuildCall2(builder, convertType, convert, convertValues, 6, "path.converted");
    LLVMBuildCondBr(builder, LLVMBuildICmp(builder, LLVMIntSGT, converted, zero, "convert.ok"), findBlock, fail);
    LLVMPositionBuilderAtEnd(builder, findBlock);
    LLVMValueRef moduleValues[] = {pin, path, slot};
    const auto found = LLVMBuildCall2(builder, moduleType, getModuleW, moduleValues, 3, "module.pinned");
    LLVMBuildCondBr(builder, LLVMBuildICmp(builder, LLVMIntNE, found, zero, "module.found"), lookup, loadBlock);
    // EN: T08-D05. When packaging isolated a homonymous transitive dependency of this alias, NSC published
    //     `<alias>.nsc-isolation.manifest`. The alias is then loaded with that activation context active, so
    //     its imports resolve to the private assembly copies instead of a module already loaded by name.
    // ES: T08-D05. Si el empaquetado aislo una dependencia transitiva homonima de este alias, NSC publico
    //     `<alias>.nsc-isolation.manifest`. El alias se carga con ese contexto de activacion activo, asi que
    //     sus importaciones se resuelven en las copias del ensamblado privado y no en un modulo ya cargado.
    LLVMPositionBuilderAtEnd(builder, loadBlock);
    constexpr std::u16string_view manifestSuffix = u".nsc-isolation.manifest";
    const auto pathEnd = LLVMBuildSub(builder, LLVMBuildAdd(builder, afterSeparator, converted, "path.end.raw"), one,
                                      "path.end");
    const auto suffixFits = LLVMBuildICmp(
        builder, LLVMIntULT,
        LLVMBuildAdd(builder, pathEnd, LLVMConstInt(i32, manifestSuffix.size() + 1, 0), "suffix.end"), capacity,
        "suffix.fits");
    const auto writeSuffix = [&](bool terminateOnly)
    {
        for (std::size_t offset = 0; offset <= manifestSuffix.size(); ++offset)
        {
            if (terminateOnly && offset != 0) break;
            const std::uint16_t value = terminateOnly || offset == manifestSuffix.size() ? 0 : manifestSuffix[offset];
            auto position = LLVMBuildAdd(builder, pathEnd, LLVMConstInt(i32, offset, 0), "suffix.index");
            const auto target = LLVMBuildGEP2(builder, i16, path, &position, 1, "suffix.ptr");
            LLVMBuildStore(builder, LLVMConstInt(i16, value, 0), target);
        }
    };
    const auto probeBlock = LLVMAppendBasicBlockInContext(context, helper, "load.probe");
    LLVMBuildCondBr(builder, suffixFits, probeBlock, plainBlock);
    LLVMPositionBuilderAtEnd(builder, probeBlock);
    writeSuffix(false);
    LLVMValueRef manifestValues[] = {path};
    const auto attributes = LLVMBuildCall2(builder, attributesType, getAttributes, manifestValues, 1, "manifest.attrs");
    writeSuffix(true);
    LLVMBuildCondBr(builder, LLVMBuildICmp(builder, LLVMIntNE, attributes, LLVMConstInt(i32, 0xffffffffULL, 0),
                                           "manifest.present"), isolatedBlock, plainBlock);

    LLVMPositionBuilderAtEnd(builder, plainBlock);
    LLVMValueRef loadValues[] = {path, LLVMConstNull(ptr), LLVMConstInt(i32, 8, 0)};
    const auto loaded = LLVMBuildCall2(builder, loadType, load, loadValues, 3, "module.loaded");
    LLVMBuildBr(builder, loadedBlock);

    // EN: ACTCTXW on x64 is 56 bytes; only cbSize (offset 0) and lpSource (offset 8) are set.
    // ES: ACTCTXW ocupa 56 bytes en x64; solo se fijan cbSize (desplazamiento 0) y lpSource (8).
    LLVMPositionBuilderAtEnd(builder, isolatedBlock);
    const auto contextType = LLVMArrayType2(i64, 7);
    const auto contextStorage = LLVMBuildAlloca(builder, contextType, "actctx");
    LLVMBuildStore(builder, LLVMConstNull(contextType), contextStorage);
    LLVMBuildStore(builder, LLVMConstInt(i32, 56, 0), contextStorage);
    auto sourceIndex = LLVMConstInt(i64, 1, 0);
    LLVMBuildStore(builder, path, LLVMBuildGEP2(builder, i64, contextStorage, &sourceIndex, 1, "actctx.source"));
    const auto cookie = LLVMBuildAlloca(builder, i64, "actctx.cookie");
    LLVMBuildStore(builder, LLVMConstInt(i64, 0, 0), cookie);
    writeSuffix(false);
    LLVMValueRef createValues[] = {contextStorage};
    const auto activation = LLVMBuildCall2(builder, createContextType, createContext, createValues, 1, "actctx.handle");
    writeSuffix(true);
    const auto activationBits = LLVMBuildPtrToInt(builder, activation, i64, "actctx.bits");
    const auto activateBlock = LLVMAppendBasicBlockInContext(context, helper, "load.activate");
    LLVMBuildCondBr(builder, LLVMBuildICmp(builder, LLVMIntNE, activationBits, LLVMConstInt(i64, ~0ULL, 0),
                                           "actctx.valid"), activateBlock, fail);
    LLVMPositionBuilderAtEnd(builder, activateBlock);
    LLVMValueRef activateValues[] = {activation, cookie};
    const auto activated = LLVMBuildCall2(builder, activateType, activateContext, activateValues, 2, "actctx.active");
    LLVMBuildCondBr(builder, LLVMBuildICmp(builder, LLVMIntNE, activated, zero, "actctx.activated"), activatedBlock,
                    releaseFailBlock);
    LLVMPositionBuilderAtEnd(builder, activatedBlock);
    const auto isolatedLoaded = LLVMBuildCall2(builder, loadType, load, loadValues, 3, "module.loaded.isolated");
    LLVMValueRef deactivateValues[] = {zero, LLVMBuildLoad2(builder, i64, cookie, "actctx.cookie.value")};
    LLVMBuildCall2(builder, deactivateType, deactivateContext, deactivateValues, 2, "");
    LLVMValueRef releaseValues[] = {activation};
    LLVMBuildCall2(builder, releaseType, releaseContext, releaseValues, 1, "");
    LLVMBuildBr(builder, loadedBlock);
    LLVMPositionBuilderAtEnd(builder, releaseFailBlock);
    LLVMBuildCall2(builder, releaseType, releaseContext, releaseValues, 1, "");
    LLVMBuildBr(builder, fail);

    LLVMPositionBuilderAtEnd(builder, loadedBlock);
    const auto loadedValue = LLVMBuildPhi(builder, ptr, "module.loaded.any");
    LLVMValueRef incomingValues[] = {loaded, isolatedLoaded};
    LLVMBasicBlockRef incomingBlocks[] = {plainBlock, activatedBlock};
    LLVMAddIncoming(loadedValue, incomingValues, incomingBlocks, 2);
    LLVMBuildCondBr(builder, LLVMBuildICmp(builder, LLVMIntNE, loadedValue, LLVMConstNull(ptr), "load.ok"), pinBlock,
                    fail);
    LLVMPositionBuilderAtEnd(builder, pinBlock);
    const auto pinned = LLVMBuildCall2(builder, moduleType, getModuleW, moduleValues, 3, "module.pin");
    LLVMBuildCondBr(builder, LLVMBuildICmp(builder, LLVMIntNE, pinned, zero, "pin.ok"), lookup, fail);
    LLVMPositionBuilderAtEnd(builder, lookup);
    const auto moduleValue = LLVMBuildLoad2(builder, ptr, slot, "module");
    LLVMValueRef procValues[] = {moduleValue, symbol};
    LLVMBuildRet(builder, LLVMBuildCall2(builder, procType, proc, procValues, 2, "export"));
    LLVMPositionBuilderAtEnd(builder, fail);
    LLVMBuildRet(builder, LLVMConstNull(ptr));
    LLVMDisposeBuilder(builder);
    return helper;
}

// Resolve an exported PE TLS template address to this thread's storage.
// The loader keeps the DLL loaded via its import; no reference count is changed.
static LLVMValueRef cppDllTlsAddressHelper(LLVMContextRef context, LLVMModuleRef module)
{
    constexpr const char* name = "__nsc_cpp_dll_tls_address";
    if (auto existing = LLVMGetNamedFunction(module, name)) return existing;
    const auto ptr = LLVMPointerTypeInContext(context, 0);
    const auto i8 = LLVMInt8TypeInContext(context);
    const auto i16 = LLVMInt16TypeInContext(context);
    const auto i32 = LLVMInt32TypeInContext(context);
    const auto i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef args[] = {ptr};
    const auto helperType = LLVMFunctionType(ptr, args, 1, 0);
    const auto helper = LLVMAddFunction(module, name, helperType);
    LLVMSetLinkage(helper, LLVMInternalLinkage);
    LLVMTypeRef moduleArgs[] = {i32, ptr, ptr};
    const auto moduleType = LLVMFunctionType(i32, moduleArgs, 3, 0);
    auto getModule = LLVMGetNamedFunction(module, "GetModuleHandleExA");
    if (!getModule) getModule = LLVMAddFunction(module, "GetModuleHandleExA", moduleType);
    LLVMSetDLLStorageClass(getModule, LLVMDLLImportStorageClass);

    const auto entry = LLVMAppendBasicBlockInContext(context, helper, "entry");
    const auto dos = LLVMAppendBasicBlockInContext(context, helper, "dos");
    const auto pe = LLVMAppendBasicBlockInContext(context, helper, "pe");
    const auto optional = LLVMAppendBasicBlockInContext(context, helper, "optional");
    const auto tls = LLVMAppendBasicBlockInContext(context, helper, "tls");
    const auto bounds = LLVMAppendBasicBlockInContext(context, helper, "bounds");
    const auto slotsBlock = LLVMAppendBasicBlockInContext(context, helper, "slots");
    const auto elementBlock = LLVMAppendBasicBlockInContext(context, helper, "element");
    const auto done = LLVMAppendBasicBlockInContext(context, helper, "done");
    const auto fail = LLVMAppendBasicBlockInContext(context, helper, "fail");
    const auto b = LLVMCreateBuilderInContext(context);
    const auto constant = [&](LLVMTypeRef type, unsigned long long n) { return LLVMConstInt(type, n, 0); };
    const auto offset = [&](LLVMValueRef base, LLVMValueRef index, const char* label) {
        return LLVMBuildGEP2(b, i8, base, &index, 1, label);
    };
    const auto at = [&](LLVMValueRef base, unsigned long long n, const char* label) {
        return offset(base, constant(i64, n), label);
    };
    const auto exported = LLVMGetParam(helper, 0);
    LLVMPositionBuilderAtEnd(b, entry);
    const auto moduleSlot = LLVMBuildAlloca(b, ptr, "module.slot");
    LLVMBuildStore(b, LLVMConstNull(ptr), moduleSlot);
    LLVMValueRef callArgs[] = {constant(i32, 6), exported, moduleSlot};
    const auto moduleResult = LLVMBuildCall2(b, moduleType, getModule, callArgs, 3, "module.result");
    LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntNE, moduleResult, constant(i32, 0), "module.ok"), dos, fail);

    LLVMPositionBuilderAtEnd(b, dos);
    const auto image = LLVMBuildLoad2(b, ptr, moduleSlot, "image");
    const auto mz = LLVMBuildLoad2(b, i16, image, "mz");
    LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, mz, constant(i16, 0x5a4d), "mz.ok"), pe, fail);

    LLVMPositionBuilderAtEnd(b, pe);
    const auto ntOffset = LLVMBuildLoad2(b, i32, at(image, 60, "nt.offset.ptr"), "nt.offset");
    const auto nt = offset(image, LLVMBuildZExt(b, ntOffset, i64, "nt.offset64"), "nt");
    const auto peMagic = LLVMBuildLoad2(b, i32, nt, "pe.magic");
    LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, peMagic, constant(i32, 0x4550), "pe.ok"), optional, fail);

    LLVMPositionBuilderAtEnd(b, optional);
    const auto magic = LLVMBuildLoad2(b, i16, at(nt, 24, "optional.magic.ptr"), "optional.magic");
    LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, magic, constant(i16, 0x20b), "optional.ok"), tls, fail);

    LLVMPositionBuilderAtEnd(b, tls);
    const auto tlsRva = LLVMBuildLoad2(b, i32, at(nt, 208, "tls.rva.ptr"), "tls.rva");
    LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntNE, tlsRva, constant(i32, 0), "tls.present"), bounds, fail);

    LLVMPositionBuilderAtEnd(b, bounds);
    const auto directory = offset(image, LLVMBuildZExt(b, tlsRva, i64, "tls.rva64"), "tls.directory");
    const auto start = LLVMBuildLoad2(b, ptr, directory, "tls.raw.start");
    const auto end = LLVMBuildLoad2(b, ptr, at(directory, 8, "tls.raw.end.ptr"), "tls.raw.end");
    const auto indexPtr = LLVMBuildLoad2(b, ptr, at(directory, 16, "tls.index.ptr"), "tls.index.address");
    const auto zeroFill = LLVMBuildLoad2(b, i32, at(directory, 32, "tls.zero.fill.ptr"), "tls.zero.fill");
    const auto endInteger = LLVMBuildPtrToInt(b, end, i64, "tls.raw.end.integer");
    const auto storageEnd = LLVMBuildAdd(b, endInteger, LLVMBuildZExt(b, zeroFill, i64, "tls.zero.fill64"), "tls.storage.end");
    const auto exportedInteger = LLVMBuildPtrToInt(b, exported, i64, "tls.export.integer");
    const auto indexValid = LLVMBuildICmp(b, LLVMIntNE, indexPtr, LLVMConstNull(ptr), "tls.index.valid");
    const auto aboveStart = LLVMBuildICmp(b, LLVMIntUGE, exported, start, "tls.above.start");
    const auto belowEnd = LLVMBuildICmp(b, LLVMIntULT, exportedInteger, storageEnd, "tls.below.end");
    const auto inRange = LLVMBuildAnd(b, aboveStart, belowEnd, "tls.in.range");
    LLVMBuildCondBr(b, LLVMBuildAnd(b, indexValid, inRange, "tls.valid"), slotsBlock, fail);

    LLVMPositionBuilderAtEnd(b, slotsBlock);
    const auto gsPtr = LLVMBuildIntToPtr(b, constant(i64, 0x58), LLVMPointerTypeInContext(context, 256), "teb.tls.ptr");
    const auto slotsInteger = LLVMBuildLoad2(b, i64, gsPtr, "teb.tls");
    LLVMSetVolatile(slotsInteger, 1);
    const auto slots = LLVMBuildIntToPtr(b, slotsInteger, ptr, "slots.ptr");
    LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntNE, slots, LLVMConstNull(ptr), "slots.valid"), elementBlock, fail);

    LLVMPositionBuilderAtEnd(b, elementBlock);
    const auto index = LLVMBuildLoad2(b, i32, indexPtr, "tls.index");
    auto index64 = LLVMBuildZExt(b, index, i64, "tls.index64");
    const auto slotAddr = LLVMBuildGEP2(b, ptr, slots, &index64, 1, "slot.address");
    const auto slot = LLVMBuildLoad2(b, ptr, slotAddr, "slot");
    LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntNE, slot, LLVMConstNull(ptr), "slot.valid"), done, fail);

    LLVMPositionBuilderAtEnd(b, done);
    const auto displacement = LLVMBuildSub(b, LLVMBuildPtrToInt(b, exported, i64, "export.integer"),
                                            LLVMBuildPtrToInt(b, start, i64, "start.integer"), "tls.offset");
    LLVMBuildRet(b, offset(slot, displacement, "tls.address"));
    LLVMPositionBuilderAtEnd(b, fail);
    LLVMBuildRet(b, LLVMConstNull(ptr));
    LLVMDisposeBuilder(b);
    return helper;
}

bool OopLlvmImplementation::callArgumentOrder(
        const frontend::ProjectOopMirNode* node,
        std::size_t sourceCount,
        std::vector<std::size_t>& parameterToSource,
        std::string& error)
{
        parameterToSource.resize(sourceCount);
        if (node->argumentParameterOrdinals.empty())
        {
            for (std::size_t index = 0; index < sourceCount; ++index) parameterToSource[index] = index;
            return true;
        }
        if (node->argumentParameterOrdinals.size() != sourceCount)
        {
            return fail(error, "named call argument map does not match the evaluated argument count");
        }
        std::vector<bool> assigned(sourceCount, false);
        for (std::size_t source = 0; source < sourceCount; ++source)
        {
            const std::size_t parameter = node->argumentParameterOrdinals[source];
            if (parameter >= sourceCount || assigned[parameter])
            {
                return fail(error, "named call argument map is not a parameter permutation");
            }
            assigned[parameter] = true;
            parameterToSource[parameter] = source;
        }
        return true;
    }

bool OopLlvmImplementation::applyMustTail(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef call,
        std::string& error)
{
        if (!node->mustTailCall) return true;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
            return fail(error, "T08-F06 must_tail cannot use the current indirect aggregate return ABI");
        if (call == nullptr || LLVMIsACallInst(call) == nullptr)
            return fail(error, "T08-F06 must_tail lowered to an invoke or non-call instruction");
        LLVMSetTailCallKind(call, LLVMTailCallKindMustTail);
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerNullConditionalCall(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.empty())
            return failValue(error, "null-conditional call is missing its receiver");
        LLVMValueRef receiver = lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr) return nullptr;
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef hasReceiver = LLVMBuildICmp(
            builder_, LLVMIntNE, receiver, LLVMConstNull(pointerType), "nullcall.hasreceiver");
        LLVMBasicBlockRef nullBlock = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef callBlock = appendBlock("nullcall.invoke");
        LLVMBasicBlockRef endBlock = appendBlock("nullcall.end");
        LLVMBuildCondBr(builder_, hasReceiver, callBlock, endBlock);

        enterBlock(callBlock);
        LLVMValueRef value = nullptr;
        switch (node->kind)
        {
            case frontend::ProjectOopMirNodeKind::DirectInstanceCall:
                value = lowerCall(node, error, receiver);
                break;
            case frontend::ProjectOopMirNodeKind::VirtualInstanceCall:
                value = lowerVirtualCall(node, error, receiver);
                break;
            case frontend::ProjectOopMirNodeKind::InterfaceInstanceCall:
                value = lowerInterfaceCall(node, error, receiver);
                break;
            default:
                return failValue(error, "null-conditional dispatch requires an instance call");
        }
        if (value == nullptr) return nullptr;
        LLVMBasicBlockRef callExit = LLVMGetInsertBlock(builder_);
        if (!terminated()) LLVMBuildBr(builder_, endBlock);

        enterBlock(endBlock);
        LLVMTypeRef resultType = llvmType(node->typeKey, error);
        // Semantic analysis admits only physical nullable carriers here; every such carrier uses
        // the opaque-pointer ABI.  Do not depend on LLVM's type-introspection C API merely to
        // repeat that already authoritative decision.
        if (resultType == nullptr) return nullptr;
        LLVMValueRef phi = LLVMBuildPhi(builder_, resultType, "nullcall");
        LLVMValueRef values[] = {LLVMConstNull(resultType), value};
        LLVMBasicBlockRef blocks[] = {nullBlock, callExit};
        LLVMAddIncoming(phi, values, blocks, 2);
        return phi;
    }

LLVMValueRef OopLlvmImplementation::lowerRangeSlice(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 3U || node->calleeName.empty() ||
            node->secondaryCalleeName.empty())
        {
            return failValue(error, "T01-F04 range slice requires receiver, two endpoints and Length/Slice targets");
        }
        const auto lengthTarget = functions_.find(node->secondaryCalleeName);
        if (lengthTarget == functions_.end())
        {
            return failValue(error, "T01-F04 Length target was not declared: " + node->secondaryCalleeName);
        }

        LLVMValueRef receiver = lowerExpression(node->children[0].get(), error);
        if (receiver == nullptr) return nullptr;
        const bool structReceiver = isStructValue(node->children[0]->typeKey);
        LLVMValueRef checked = receiver;
        if (!structReceiver)
        {
            checked = checkedReceiver(receiver, error);
            if (checked == nullptr) return nullptr;
        }

        LLVMValueRef lengthArguments[] = {checked};
        LLVMValueRef length = buildPotentiallyThrowingCall(
            lengthTarget->second.functionType,
            lengthTarget->second.function,
            lengthArguments,
            1U,
            "range.length",
            error);
        if (length == nullptr) return nullptr;

        LLVMValueRef writtenStart = lowerExpression(node->children[1].get(), error);
        LLVMValueRef writtenEnd = lowerExpression(node->children[2].get(), error);
        if (writtenStart == nullptr || writtenEnd == nullptr) return nullptr;

        constexpr std::uint64_t hasStartBit = 1U;
        constexpr std::uint64_t startFromEndBit = 2U;
        constexpr std::uint64_t hasEndBit = 4U;
        constexpr std::uint64_t endFromEndBit = 8U;
        constexpr std::uint64_t boundsProvenBit = 16U;
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMValueRef zero = LLVMConstInt(i32, 0U, 0);
        LLVMValueRef start = (node->integerValue & hasStartBit) != 0U
            ? writtenStart : zero;
        if ((node->integerValue & startFromEndBit) != 0U)
            start = LLVMBuildSub(builder_, length, start, "range.start.fromend");
        LLVMValueRef end = (node->integerValue & hasEndBit) != 0U
            ? writtenEnd : length;
        if ((node->integerValue & endFromEndBit) != 0U)
            end = LLVMBuildSub(builder_, length, end, "range.end.fromend");

        if ((node->integerValue & boundsProvenBit) == 0U)
        {
            LLVMValueRef lengthValid = LLVMBuildICmp(
                builder_, LLVMIntSGE, length, zero, "range.length.nonnegative");
            LLVMValueRef startValid = LLVMBuildICmp(
                builder_, LLVMIntSGE, start, zero, "range.start.nonnegative");
            LLVMValueRef ordered = LLVMBuildICmp(
                builder_, LLVMIntSLE, start, end, "range.ordered");
            LLVMValueRef endValid = LLVMBuildICmp(
                builder_, LLVMIntSLE, end, length, "range.end.inbounds");
            LLVMValueRef valid = LLVMBuildAnd(
                builder_, lengthValid, startValid, "range.valid.length-start");
            valid = LLVMBuildAnd(builder_, valid, ordered, "range.valid.order");
            valid = LLVMBuildAnd(builder_, valid, endValid, "range.valid.end");
            if (!emitGuard(valid, LlvmTrapReason::BoundsViolation, "range.slice", error))
                return nullptr;
        }

        LLVMValueRef count = LLVMBuildSub(builder_, end, start, "range.count");
        const std::vector<LLVMValueRef> sliceArguments = {start, count};
        return lowerCall(node, error, checked, &sliceArguments);
    }

LLVMValueRef OopLlvmImplementation::lowerCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef receiverOverride ,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end())
        {
            return failValue(error, "Macro A call target was not declared: " + node->calleeName);
        }
        if (node->children.empty())
        {
            return failValue(error, "Macro A instance call is missing its explicit receiver operand");
        }

        LLVMValueRef receiver = receiverOverride != nullptr
            ? receiverOverride
            : lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr)
        {
            return nullptr;
        }
        if (terminated()) return receiver;
        const bool structReceiver = node->children.front() != nullptr &&
            isStructValue(node->children.front()->typeKey);
        if (!structReceiver)
        {
            receiver = checkedReceiver(receiver, error);
            if (receiver == nullptr)
            {
                return nullptr;
            }
        }

        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->children.size() + 1);
        arguments.push_back(receiver);
        // Win64 aggregate ABI: an indirect struct return travels through an sret slot placed
        // right after the receiver; indirect struct arguments travel as pointers to per-call
        // copies. Mirrors the signature built in declareFunctions -- and clang's own output.
        LLVMValueRef sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
        }
        std::vector<LLVMValueRef> sourceArguments;
        sourceArguments.reserve(node->children.size() - 1);
        LLVMValueRef assignedValue = nullptr;
        LLVMValueRef terminatedValue = nullptr;
        if (!collectCallArguments(
                node, argumentOverrides, sourceArguments, assignedValue, terminatedValue, error))
        {
            return nullptr;
        }
        if (terminatedValue != nullptr) return terminatedValue;
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, sourceArguments.size(), parameterToSource, error)) return nullptr;
        for (const std::size_t source : parameterToSource) arguments.push_back(sourceArguments[source]);
        releaseTransferredArguments(node);
        LLVMValueRef call = buildPotentiallyThrowingCall(
            found->second.functionType, found->second.function,
            arguments.data(), static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "call", error);
        if (call == nullptr) return nullptr;
        if (!applyMustTail(node, call, error)) return nullptr;
        applyExternalBoolAbi(context_, found->second.mirFunction,
            [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
            { LLVMAddCallSiteAttribute(call, index, attribute); });
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                1U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        if (node->returnsAssignedValue)
        {
            if (assignedValue == nullptr)
                return failValue(error, "setter assignment call lost its converted value");
            return assignedValue;
        }
        return call;
    }

bool OopLlvmImplementation::collectCallArguments(
        const frontend::ProjectOopMirNode* node,
        const std::vector<LLVMValueRef>* argumentOverrides,
        std::vector<LLVMValueRef>& sourceArguments,
        LLVMValueRef& assignedValue,
        LLVMValueRef& terminatedValue,
        std::string& error)
{
        if (argumentOverrides != nullptr &&
            argumentOverrides->size() != node->children.size() - 1U)
        {
            failValue(error, "direct call argument override count does not match MIR operands");
            return false;
        }
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index - 1U]
                : lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return false;
            if (terminated())
            {
                terminatedValue = value;
                return true;
            }
            if (node->returnsAssignedValue && index + 1U == node->children.size())
                assignedValue = value;
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return false;
            sourceArguments.push_back(value);
        }
        return true;
    }

bool OopLlvmImplementation::ensureTypeInitialized(std::size_t typeOrdinal, std::string& error)
{
        const auto found = staticInitFunctions_.find(typeOrdinal);
        if (found == staticInitFunctions_.end())
        {
            return true;
        }
        if (currentFunction_ == nullptr)
        {
            return fail(error, "Stage 37 cannot emit a type-initialization guard without an active function");
        }
        if (buildPotentiallyThrowingCall(
                found->second.functionType, found->second.function,
                nullptr, 0, "", error) == nullptr && !error.empty())
        {
            return false;
        }
        return true;
    }

LLVMValueRef OopLlvmImplementation::staticFieldAddress(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node == nullptr || !node->memberOrdinal.has_value())
        {
            return failValue(error, "Stage 37 static field MIR is missing its member ordinal");
        }
        const auto globalFound = staticFieldGlobals_.find(*node->memberOrdinal);
        const auto infoFound = staticFieldInfo_.find(*node->memberOrdinal);
        if (infoFound == staticFieldInfo_.end() || infoFound->second == nullptr)
        {
            return failValue(error, "Stage 37 static field has no declared MIR information");
        }
        const frontend::ProjectOopMirStaticField& field = *infoFound->second;
        if (field.isImported && !field.isConst)
        {
            const auto accessorFound = staticFieldAccessorFunctions_.find(*node->memberOrdinal);
            if (accessorFound == staticFieldAccessorFunctions_.end())
                return failValue(error, "Stage 37 imported static field has no declared accessor");
            LLVMValueRef address = buildPotentiallyThrowingCall(
                accessorFound->second.functionType, accessorFound->second.function,
                nullptr, 0, "static.address", error);
            if (address == nullptr && !error.empty()) return nullptr;
            return address;
        }
        if (field.isCppExternal && field.cppThreadLocal && field.cppDllImport)
        {
            const auto ptr = LLVMPointerTypeInContext(context_, 0);
            LLVMTypeRef arguments[] = {ptr};
            const auto type = LLVMFunctionType(ptr, arguments, 1, 0);
            const auto helper = cppDllTlsAddressHelper(context_, module_);
            const auto collision = !field.cppTlsRuntimeIdentity.empty() &&
                std::any_of(mirModel_.staticFields().begin(), mirModel_.staticFields().end(),
                    [&](const auto& other) {
                        return other.isCppExternal && other.cppThreadLocal && other.cppDllImport &&
                            other.storageSymbol == field.storageSymbol &&
                            !other.cppTlsRuntimeIdentity.empty() &&
                            other.cppTlsRuntimeIdentity != field.cppTlsRuntimeIdentity;
                    });
            LLVMValueRef exported = nullptr;
            if (collision)
            {
                const auto separator = field.cppTlsRuntimeIdentity.rfind(':');
                if (separator == std::string::npos || separator == 0)
                    return failValue(error, "C TLS DLL identity has no runtime name");
                const bool sameNameCollision = std::any_of(
                    mirModel_.staticFields().begin(), mirModel_.staticFields().end(), [&](const auto& other) {
                        return other.isCppExternal && other.cppThreadLocal && other.cppDllImport &&
                            other.storageSymbol == field.storageSymbol &&
                            other.cppTlsRuntimeIdentity != field.cppTlsRuntimeIdentity &&
                            interop::cppTlsSameDllName(field.cppTlsRuntimeIdentity, other.cppTlsRuntimeIdentity);
                    });
                const auto libraryName = sameNameCollision
                    ? interop::cppTlsDllAlias(field.cppTlsRuntimeIdentity)
                    : field.cppTlsRuntimeIdentity.substr(0, separator);
                if (libraryName.empty()) return failValue(error, "C TLS DLL identity has an invalid digest");
                const auto library = cStringGlobal(libraryName, "tls_dll");
                const auto symbol = cStringGlobal(field.storageSymbol, "tls_symbol");
                if (!library || !symbol) return failValue(error, "C TLS DLL identity strings are unavailable");
                LLVMTypeRef exportArguments[] = {ptr, ptr};
                const auto exportType = LLVMFunctionType(ptr, exportArguments, 2, 0);
                LLVMValueRef exportValues[] = {library, symbol};
                exported = LLVMBuildCall2(builder_, exportType, cppDllTlsExportHelper(context_, module_),
                                          exportValues, 2, "cpp.dll.tls.export");
                exported = checkedReceiver(exported, error);
                if (!exported) return nullptr;
            }
            else
            {
                if (globalFound == staticFieldGlobals_.end())
                    return failValue(error, "C TLS DLL global was not declared");
                exported = globalFound->second;
            }
            LLVMValueRef values[] = {exported};
            const auto address = LLVMBuildCall2(builder_, type, helper, values, 1, "cpp.dll.tls.address");
            return checkedReceiver(address, error);
        }
        if (globalFound == staticFieldGlobals_.end())
            return failValue(error, "Stage 37 static field has no declared LLVM global");
        if (!field.isConst && !field.isCppExternal && !ensureTypeInitialized(field.declaringTypeOrdinal, error))
        {
            return nullptr;
        }
        return globalFound->second;
    }

// EN: Freeze 3.0 (plan item 118): right before the call instruction, the hidden owner of each argument
//     handed to an `owned` parameter is cleared, so the callee owns it and the full-expression cleanup (or
//     an unwind out of the callee) finds the slot empty. Until here the slot destroys it if anything throws.
// ES: Congelamiento 3.0 (elemento 118 del plan): justo antes de la instrucción de llamada se limpia el dueño
//     oculto de cada argumento entregado a un parámetro `owned`, así el llamado es el dueño y el cleanup de la
//     expresión completa (o un unwind desde el llamado) encuentra el slot vacío. Hasta aquí el slot lo destruye
//     si algo lanza.
void OopLlvmImplementation::releaseTransferredArguments(const frontend::ProjectOopMirNode* call)
{
        if (call == nullptr || terminated()) return;
        LLVMValueRef null = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
        for (const auto& child : call->children)
        {
            if (child == nullptr || child->kind != frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow ||
                !child->booleanValue || !child->localOrdinal.has_value())
                continue;
            const auto storage = localStorage_.find(*child->localOrdinal);
            if (storage != localStorage_.end() && storage->second != nullptr)
                LLVMBuildStore(builder_, null, storage->second);
        }
}

LLVMValueRef OopLlvmImplementation::lowerDirectClosureCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.empty() || node->children.size() != node->argumentCount + 1)
            return failValue(error, "generic closure call has invalid MIR children");
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end())
            return failValue(error, "generic closure target was not declared: " + node->calleeName);

        LLVMValueRef delegateCell = lowerExpression(node->children.front().get(), error);
        if (delegateCell == nullptr) return nullptr;
        delegateCell = checkedReceiver(delegateCell, error);
        if (delegateCell == nullptr) return nullptr;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef receiver = LLVMBuildLoad2(
            builder_, ptr, byteOffset(delegateCell, 8, "generic.closure.receiver.addr"),
            "generic.closure.receiver");
        receiver = checkedReceiver(receiver, error);
        if (receiver == nullptr) return nullptr;

        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->argumentCount + 2);
        arguments.push_back(receiver);
        LLVMValueRef sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(
                carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
        }
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return nullptr;
            if (terminated()) return value;
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return nullptr;
            arguments.push_back(value);
        }
        releaseTransferredArguments(node);
        LLVMValueRef call = buildPotentiallyThrowingCall(
            found->second.functionType, found->second.function,
            arguments.data(), static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr
                ? "" : "generic.closure.call",
            error);
        if (call == nullptr) return nullptr;
        if (!applyMustTail(node, call, error)) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                1U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        return call;
    }

LLVMValueRef OopLlvmImplementation::lowerStaticCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        const auto found = functions_.find(node->calleeName);
        if (found == functions_.end())
        {
            return failValue(error, "Stage 37 static call target was not declared: " + node->calleeName);
        }

        const auto* imported = found->second.mirFunction;
        if (imported && imported->cppSysVCall && imported->cppAbiPlan &&
            interop::abi_plan::isSysVSmallRecordsRoundTrip(*imported->cppAbiPlan, imported->cAbiName))
        {
            const auto count = imported->cppAbiPlan->logicalParameters.size();
            if (node->children.size() != count || node->referenceResult || node->returnsAssignedValue ||
                (argumentOverrides && argumentOverrides->size() != count))
                return failValue(error, "SysV multiple-record call has an unexpected argument contract");
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (!carrier) return nullptr;
            const unsigned alignment = structAlignmentFor(node->typeKey);
            if (alignment == 0) return failValue(error, "SysV record has no alignment");
            std::vector<std::size_t> parameterToSource;
            if (!callArgumentOrder(node, count, parameterToSource, error)) return nullptr;
            if (parameterToSource.size() != count) return failValue(error, "SysV record argument mapping is invalid");
            std::vector<std::size_t> sourceToParameter(count, count);
            for (std::size_t parameter = 0; parameter < count; ++parameter)
            {
                const auto source = parameterToSource[parameter];
                if (source >= count || sourceToParameter[source] != count)
                    return failValue(error, "SysV record argument mapping is invalid");
                sourceToParameter[source] = parameter;
            }
            std::vector<LLVMValueRef> sourceArguments;
            for (std::size_t index = 0; index < count; ++index)
            {
                LLVMValueRef value = argumentOverrides ? (*argumentOverrides)[index] :
                    lowerExpression(node->children[index].get(), error);
                if (!value) return nullptr;
                if (terminated()) return value;
                const auto& typeKey = node->children[index]->typeKey;
                if (typeKey == "builtin:int*" &&
                    imported->cppAbiPlan->physicalParameters[sourceToParameter[index]].type.kind == interop::abi_plan::TypeKind::Pointer)
                {
                    sourceArguments.push_back(value);
                    continue;
                }
                if ((typeKey == "builtin:float" &&
                    imported->cppAbiPlan->physicalParameters[sourceToParameter[index]].type.kind == interop::abi_plan::TypeKind::Float) ||
                    (typeKey == "builtin:double" &&
                    imported->cppAbiPlan->physicalParameters[sourceToParameter[index]].type.kind == interop::abi_plan::TypeKind::Double))
                {
                    sourceArguments.push_back(value);
                    continue;
                }
                if (typeKey == "builtin:int" || typeKey == "builtin:uint" || typeKey == "builtin:long" || typeKey == "builtin:ulong")
                {
                    const auto width = imported->cppAbiPlan->physicalParameters[sourceToParameter[index]].type.widthOrCount;
                    if (width != (typeKey == "builtin:int" || typeKey == "builtin:uint" ? 32U : 64U))
                        return failValue(error, "SysV scalar argument has an unexpected physical type");
                    sourceArguments.push_back(value);
                    continue;
                }
                LLVMTypeRef argumentCarrier = llvmType(typeKey, error);
                if (!argumentCarrier) return nullptr;
                const unsigned argumentAlignment = structAlignmentFor(typeKey);
                if (argumentAlignment == 0) return failValue(error, "SysV record argument has no alignment");
                LLVMTypeRef argumentInteger = LLVMIntTypeInContext(context_, static_cast<unsigned>(
                    imported->cppAbiPlan->physicalParameters[sourceToParameter[index]].type.widthOrCount));
                LLVMValueRef slot = entryBlockAlloca(argumentCarrier, argumentAlignment, "sysv.records.arg");
                LLVMValueRef store = LLVMBuildStore(builder_, value, slot);
                LLVMSetAlignment(store, argumentAlignment);
                LLVMValueRef piece = LLVMBuildLoad2(builder_, argumentInteger, slot, "sysv.records.piece");
                LLVMSetAlignment(piece, argumentAlignment);
                sourceArguments.push_back(piece);
            }
            std::vector<LLVMValueRef> arguments;
            arguments.reserve(count);
            for (const auto source : parameterToSource) arguments.push_back(sourceArguments[source]);
            releaseTransferredArguments(node);
            LLVMValueRef call = buildPotentiallyThrowingCall(found->second.functionType, found->second.function,
                arguments.data(), static_cast<unsigned>(arguments.size()), "sysv.records.call", error);
            if (!call) return nullptr;
            LLVMSetInstructionCallConv(call, LLVMX8664SysVCallConv);
            if (!applyMustTail(node, call, error)) return nullptr;
            LLVMValueRef resultSlot = entryBlockAlloca(carrier, alignment, "sysv.records.result");
            LLVMValueRef resultStore = LLVMBuildStore(builder_, call, resultSlot);
            LLVMSetAlignment(resultStore, alignment);
            LLVMValueRef result = LLVMBuildLoad2(builder_, carrier, resultSlot, "sysv.records.value");
            LLVMSetAlignment(result, alignment);
            return result;
        }
        if (imported && imported->cppSysVCall && imported->cppAbiPlan &&
            interop::abi_plan::isSysVIndirectRecordRoundTrip(*imported->cppAbiPlan, imported->cAbiName))
        {
            if (node->children.size() != 1 || node->referenceResult || node->returnsAssignedValue ||
                node->children[0]->typeKey != node->typeKey ||
                (argumentOverrides && argumentOverrides->size() != 1))
                return failValue(error, "SysV indirect record call has an unexpected argument contract");
            LLVMValueRef value = argumentOverrides ? (*argumentOverrides)[0] :
                lowerExpression(node->children[0].get(), error);
            if (!value) return nullptr;
            if (terminated()) return value;
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (!carrier) return nullptr;
            const unsigned alignment = structAlignmentFor(node->typeKey);
            if (alignment != imported->cppAbiPlan->logicalReturn.alignment)
                return failValue(error, "SysV indirect record has an unexpected alignment");
            LLVMValueRef resultSlot = entryBlockAlloca(carrier, alignment, "sysv.indirect.result");
            const unsigned argumentAlignment = static_cast<unsigned>(imported->cppAbiPlan->physicalParameters[1].alignment);
            LLVMValueRef argumentSlot = entryBlockAlloca(carrier, argumentAlignment, "sysv.indirect.argument");
            LLVMValueRef argumentStore = LLVMBuildStore(builder_, value, argumentSlot);
            LLVMSetAlignment(argumentStore, argumentAlignment);
            LLVMValueRef arguments[] = {resultSlot, argumentSlot};
            releaseTransferredArguments(node);
            LLVMValueRef call = buildPotentiallyThrowingCall(found->second.functionType, found->second.function,
                arguments, 2, "", error);
            if (!call) return nullptr;
            LLVMSetInstructionCallConv(call, LLVMX8664SysVCallConv);
            applySretAttributes([&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); }, 0, node->typeKey);
            applySysVRecordByValue(context_, carrier, argumentAlignment,
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); });
            if (!applyMustTail(node, call, error)) return nullptr;
            LLVMValueRef result = LLVMBuildLoad2(builder_, carrier, resultSlot, "sysv.indirect.value");
            LLVMSetAlignment(result, alignment);
            return result;
        }
        if (imported && imported->cppSysVCall && imported->cppAbiPlan &&
            (interop::abi_plan::isSysVTwoIntegerRecordWithScalars(*imported->cppAbiPlan, imported->cAbiName) ||
             interop::abi_plan::isSysVTwoIntegerRecordRoundTrip(*imported->cppAbiPlan, imported->cAbiName) ||
             interop::abi_plan::isSysVMixedRecordRoundTrip(*imported->cppAbiPlan, imported->cAbiName)))
        {
            const auto count = imported->cppAbiPlan->logicalParameters.size();
            if (node->children.size() != count || node->referenceResult || node->returnsAssignedValue ||
                (argumentOverrides && argumentOverrides->size() != count))
                return failValue(error, "SysV two-piece record call has an unexpected argument contract");
            std::vector<std::size_t> order;
            if (!callArgumentOrder(node, count, order, error)) return nullptr;
            if (order.size() != count) return failValue(error, "SysV two-piece argument mapping is invalid");
            std::vector<bool> seen(count, false);
            for (const auto index : order)
            {
                if (index >= count || seen[index]) return failValue(error, "SysV two-piece argument mapping is invalid");
                seen[index] = true;
            }
            if (node->children[order[0]]->typeKey != node->typeKey)
                return failValue(error, "SysV two-piece record argument type is invalid");
            std::vector<LLVMValueRef> sourceArguments;
            for (std::size_t i = 0; i < count; ++i)
            {
                LLVMValueRef value = argumentOverrides ? (*argumentOverrides)[i] : lowerExpression(node->children[i].get(), error);
                if (!value) return nullptr;
                if (terminated()) return value;
                sourceArguments.push_back(value);
            }
            LLVMValueRef value = sourceArguments[order[0]];
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (!carrier) return nullptr;
            const unsigned alignment = structAlignmentFor(node->typeKey);
            if (alignment == 0) return failValue(error, "SysV two-piece record has no alignment");
            const unsigned pieceAlignment = alignment < 8 ? alignment : 8;
            LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
            LLVMTypeRef firstType = imported->cppAbiPlan->physicalReturn.elements[0].kind == interop::abi_plan::TypeKind::Double
                ? LLVMDoubleTypeInContext(context_) :
                imported->cppAbiPlan->physicalReturn.elements[0].kind == interop::abi_plan::TypeKind::Float
                ? LLVMFloatTypeInContext(context_) :
                imported->cppAbiPlan->physicalReturn.elements[0].kind == interop::abi_plan::TypeKind::Vector
                ? LLVMVectorType(LLVMFloatTypeInContext(context_), 2) : i64;
            LLVMTypeRef secondType = imported->cppAbiPlan->physicalReturn.elements[1].kind == interop::abi_plan::TypeKind::Double
                ? LLVMDoubleTypeInContext(context_) :
                imported->cppAbiPlan->physicalReturn.elements[1].kind == interop::abi_plan::TypeKind::Float
                ? LLVMFloatTypeInContext(context_) :
                imported->cppAbiPlan->physicalReturn.elements[1].kind == interop::abi_plan::TypeKind::Vector
                ? LLVMVectorType(LLVMFloatTypeInContext(context_), 2) : LLVMIntTypeInContext(context_,
                    static_cast<unsigned>(imported->cppAbiPlan->physicalReturn.elements[1].widthOrCount));
            LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
            LLVMValueRef offset = LLVMConstInt(i64, 8, 0);
            const auto second = [&](LLVMValueRef slot, const char* name) {
                return LLVMBuildGEP2(builder_, i8, slot, &offset, 1, name);
            };
            LLVMValueRef argumentSlot = entryBlockAlloca(carrier, alignment, "sysv.pair.arg");
            LLVMValueRef argumentStore = LLVMBuildStore(builder_, value, argumentSlot);
            LLVMSetAlignment(argumentStore, alignment);
            std::vector<LLVMValueRef> arguments = {
                LLVMBuildLoad2(builder_, firstType, argumentSlot, "sysv.pair.first"),
                LLVMBuildLoad2(builder_, secondType, second(argumentSlot, "sysv.pair.arg.second"), "sysv.pair.second")};
            LLVMSetAlignment(arguments[0], pieceAlignment);
            LLVMSetAlignment(arguments[1], pieceAlignment);
            for (std::size_t i = 1; i < count; ++i)
            {
                if (imported->cppAbiPlan->logicalParameters[i].physicalCount == 2)
                {
                    const auto& argumentKey = node->children[order[i]]->typeKey;
                    LLVMTypeRef argumentCarrier = llvmType(argumentKey, error);
                    if (!argumentCarrier) return nullptr;
                    const unsigned argumentAlignment = structAlignmentFor(argumentKey);
                    if (argumentAlignment == 0) return failValue(error, "SysV two-piece argument has no alignment");
                    const unsigned argumentPieceAlignment = argumentAlignment < 8 ? argumentAlignment : 8;
                    const auto physicalIndex = imported->cppAbiPlan->logicalParameters[i].firstPhysical;
                    LLVMTypeRef argumentSecond = LLVMIntTypeInContext(context_, static_cast<unsigned>(
                        imported->cppAbiPlan->physicalParameters[physicalIndex + 1].type.widthOrCount));
                    LLVMValueRef slot = entryBlockAlloca(argumentCarrier, argumentAlignment, "sysv.pair.extra.arg");
                    LLVMValueRef store = LLVMBuildStore(builder_, sourceArguments[order[i]], slot);
                    LLVMSetAlignment(store, argumentAlignment);
                    LLVMValueRef firstPiece = LLVMBuildLoad2(builder_, firstType, slot, "sysv.pair.extra.first");
                    LLVMValueRef secondPiece = LLVMBuildLoad2(builder_, argumentSecond, second(slot, "sysv.pair.extra.second.addr"), "sysv.pair.extra.second");
                    LLVMSetAlignment(firstPiece, argumentPieceAlignment);
                    LLVMSetAlignment(secondPiece, argumentPieceAlignment);
                    arguments.push_back(firstPiece);
                    arguments.push_back(secondPiece);
                }
                else arguments.push_back(sourceArguments[order[i]]);
            }
            releaseTransferredArguments(node);
            LLVMValueRef call = buildPotentiallyThrowingCall(found->second.functionType, found->second.function,
                arguments.data(), static_cast<unsigned>(arguments.size()), "sysv.pair.call", error);
            if (!call) return nullptr;
            LLVMSetInstructionCallConv(call, LLVMX8664SysVCallConv);
            if (!applyMustTail(node, call, error)) return nullptr;
            LLVMValueRef resultSlot = entryBlockAlloca(carrier, alignment, "sysv.pair.result");
            LLVMValueRef firstStore = LLVMBuildStore(builder_, LLVMBuildExtractValue(builder_, call, 0, "sysv.pair.r0"),
                                                      resultSlot);
            LLVMValueRef secondStore = LLVMBuildStore(builder_, LLVMBuildExtractValue(builder_, call, 1, "sysv.pair.r1"),
                                                       second(resultSlot, "sysv.pair.result.second"));
            LLVMSetAlignment(firstStore, pieceAlignment);
            LLVMSetAlignment(secondStore, pieceAlignment);
            LLVMValueRef result = LLVMBuildLoad2(builder_, carrier, resultSlot, "sysv.pair.value");
            LLVMSetAlignment(result, alignment);
            return result;
        }
        if (imported && imported->cppSysVCall && imported->cppAbiPlan &&
            (interop::abi_plan::isSysVSmallRecordRoundTrip(*imported->cppAbiPlan, imported->cAbiName) ||
             interop::abi_plan::isSysVSingleVector2FRecordRoundTrip(*imported->cppAbiPlan, imported->cAbiName) ||
             interop::abi_plan::isSysVSingleFloatingRecordRoundTrip(*imported->cppAbiPlan, imported->cAbiName)))
        {
            if (node->children.size() != 1 || node->referenceResult || node->returnsAssignedValue ||
                node->children[0]->typeKey != node->typeKey ||
                (argumentOverrides && argumentOverrides->size() != 1))
                return failValue(error, "SysV small record call has an unexpected argument contract");
            LLVMValueRef value = argumentOverrides ? (*argumentOverrides)[0] :
                lowerExpression(node->children[0].get(), error);
            if (!value) return nullptr;
            if (terminated()) return value;
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (!carrier) return nullptr;
            const unsigned alignment = structAlignmentFor(node->typeKey);
            if (alignment == 0) return failValue(error, "SysV small record has no alignment");
            LLVMValueRef argumentSlot = entryBlockAlloca(carrier, alignment, "sysv.record.arg");
            LLVMValueRef argumentStore = LLVMBuildStore(builder_, value, argumentSlot);
            LLVMSetAlignment(argumentStore, alignment);
            LLVMTypeRef integer = interop::abi_plan::isSysVSingleVector2FRecordRoundTrip(*imported->cppAbiPlan, imported->cAbiName)
                ? LLVMVectorType(LLVMFloatTypeInContext(context_), 2) :
                imported->cppAbiPlan->physicalReturn.kind == interop::abi_plan::TypeKind::Float ? LLVMFloatTypeInContext(context_) :
                imported->cppAbiPlan->physicalReturn.kind == interop::abi_plan::TypeKind::Double ? LLVMDoubleTypeInContext(context_) : LLVMIntTypeInContext(context_,
                    static_cast<unsigned>(imported->cppAbiPlan->physicalReturn.widthOrCount));
            LLVMValueRef physicalArgument = LLVMBuildLoad2(builder_, integer, argumentSlot, "sysv.record.physical");
            LLVMSetAlignment(physicalArgument, alignment);
            releaseTransferredArguments(node);
            LLVMValueRef call = buildPotentiallyThrowingCall(found->second.functionType, found->second.function,
                &physicalArgument, 1, "sysv.record.call", error);
            if (!call) return nullptr;
            LLVMSetInstructionCallConv(call, LLVMX8664SysVCallConv);
            if (!applyMustTail(node, call, error)) return nullptr;
            LLVMValueRef resultSlot = entryBlockAlloca(carrier, alignment, "sysv.record.result");
            LLVMValueRef resultStore = LLVMBuildStore(builder_, call, resultSlot);
            LLVMSetAlignment(resultStore, alignment);
            LLVMValueRef result = LLVMBuildLoad2(builder_, carrier, resultSlot, "sysv.record.value");
            LLVMSetAlignment(result, alignment);
            return result;
        }

        std::vector<LLVMValueRef> arguments;
        arguments.reserve(node->children.size() + 1);
        // Win64 aggregate ABI -- see lowerCall; for a static callee the sret slot leads.
        LLVMValueRef sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return nullptr;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
        }
        std::vector<LLVMValueRef> sourceArguments;
        sourceArguments.reserve(node->children.size());
        // EN: T08-D22: the unwrapped values, for vector/HVA arguments of a `__vectorcall` C import.
        // ES: T08-D22: los valores sin envolver, para argumentos vector/HVA de una importacion C `__vectorcall`.
        std::vector<LLVMValueRef> rawVectorCallArguments;
        LLVMValueRef assignedValue = nullptr;
        if (argumentOverrides != nullptr &&
            argumentOverrides->size() != node->children.size())
        {
            return failValue(error, "static call argument override count does not match MIR operands");
        }
        for (std::size_t index = 0; index < node->children.size(); ++index)
        {
            const auto& child = node->children[index];
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index]
                : lowerExpression(child.get(), error);
            if (value == nullptr)
            {
                return nullptr;
            }
            if (terminated()) return value;
            if (node->returnsAssignedValue && index + 1U == node->children.size())
                assignedValue = value;
            rawVectorCallArguments.push_back(value);
            value = wrapIndirectArgument(value, child->typeKey, error);
            if (value == nullptr) return nullptr;
            sourceArguments.push_back(value);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, sourceArguments.size(), parameterToSource, error)) return nullptr;
        for (const std::size_t source : parameterToSource) arguments.push_back(sourceArguments[source]);
        releaseTransferredArguments(node);
        LLVMValueRef target = found->second.function;
        if (imported != nullptr && imported->isVariadic && imported->importRequiresRuntimeBinding &&
            imported->mangledName.starts_with("__nsc_cpp_tls_import_"))
        {
            LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
            LLVMValueRef library = cStringGlobal(imported->importDllName, "dll");
            LLVMValueRef symbol = cStringGlobal(imported->cAbiName, "sym");
            if (library == nullptr || symbol == nullptr)
                return failValue(error, "variadic TLS import lost its library or symbol identity");
            LLVMTypeRef parameterTypes[] = {ptr, ptr};
            LLVMTypeRef helperType = LLVMFunctionType(ptr, parameterTypes, 2, 0);
            LLVMValueRef helperArgs[] = {library, symbol};
            target = LLVMBuildCall2(builder_, helperType, cppDllTlsExportHelper(context_, module_),
                helperArgs, 2, "import.address");
            if (!emitGuard(LLVMBuildICmp(builder_, LLVMIntNE, target, LLVMConstNull(ptr), "import.bound"),
                           LlvmTrapReason::ImportBindingFailure, "import.binding", error)) return nullptr;
        }
        // EN: T08-D22: a `__vectorcall` C import is called through the extern-C function-pointer path with the
        //     vectorcall convention, which already maps Float32x4 and HVAs to the vectorcall ABI.
        // ES: T08-D22: una importacion C `__vectorcall` se llama por el camino de punteros a funcion C con la
        //     convencion vectorcall, que ya traduce Float32x4 y las HVA al ABI vectorcall.
        if (imported != nullptr && imported->cppVectorCall && !imported->isVariadic)
        {
            std::vector<LLVMValueRef> vectorArguments;
            std::vector<std::string> argumentTypeKeys;
            for (const std::size_t source : parameterToSource)
            {
                const std::string& typeKey = node->children[source]->typeKey;
                argumentTypeKeys.push_back(typeKey);
                // EN: vectors and HVAs travel unwrapped; every other aggregate keeps its per-call pointer copy.
                // ES: los vectores y las HVA viajan sin envolver; los demas agregados conservan su copia por puntero.
                vectorArguments.push_back(vectorCallAbiType(typeKey) != nullptr
                    ? rawVectorCallArguments.at(source)
                    : sourceArguments.at(source));
            }
            return emitExternCDelegateCall(target, vectorArguments, argumentTypeKeys, node->typeKey, "", true,
                                           error);
        }
        LLVMValueRef call = buildPotentiallyThrowingCall(
            found->second.functionType, target,
            arguments.empty() ? nullptr : arguments.data(),
            static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "static.call", error);
        if (call == nullptr) return nullptr;
        if (imported != nullptr && imported->cppSysVCall)
            LLVMSetInstructionCallConv(call, LLVMX8664SysVCallConv);
        if (!applyMustTail(node, call, error)) return nullptr;
        applySysVIntegerExtension(context_, imported,
            [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
            { LLVMAddCallSiteAttribute(call, index, attribute); });
        applyExternalBoolAbi(context_, found->second.mirFunction,
            [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
            { LLVMAddCallSiteAttribute(call, index, attribute); });
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                0U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        if (node->returnsAssignedValue)
        {
            if (assignedValue == nullptr)
                return failValue(error, "static setter assignment call lost its converted value");
            return assignedValue;
        }
        return call;
    }

LLVMValueRef OopLlvmImplementation::loadVirtualTarget(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error)
{
        if (!node->virtualSlot.has_value())
            return failValue(error, "T08-F05 virtual target is missing its vtable slot");
        if (descriptorType_ == nullptr)
            return failValue(error, "T08-F05 virtual target has no class-descriptor LLVM type");
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, receiver, "class.descriptor");
        LLVMValueRef vtableAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 1, "vtable.addr");
        LLVMValueRef vtable = LLVMBuildLoad2(builder_, ptr, vtableAddress, "vtable");
        LLVMValueRef slotIndex = LLVMConstInt(
            LLVMInt64TypeInContext(context_), *node->virtualSlot, 0);
        LLVMValueRef indices[] = {slotIndex};
        LLVMValueRef slotAddress = LLVMBuildGEP2(
            builder_, ptr, vtable, indices, 1, "vslot.addr");
        return LLVMBuildLoad2(builder_, ptr, slotAddress, "vtarget");
    }

bool OopLlvmImplementation::prepareDynamicCallArguments(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        const std::vector<LLVMValueRef>* argumentOverrides,
        std::vector<LLVMValueRef>& arguments,
        std::vector<LLVMTypeRef>& parameterTypes,
        LLVMValueRef& sretSlot,
        LLVMValueRef& assignedValue,
        std::string& error)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        arguments.reserve(node->children.size() + 1);
        parameterTypes.reserve(node->children.size() + 1);
        arguments.push_back(receiver);
        parameterTypes.push_back(ptr);
        sretSlot = nullptr;
        if (!node->referenceResult && isIndirectStructTypeKey(node->typeKey))
        {
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            if (carrier == nullptr) return false;
            sretSlot = entryBlockAlloca(carrier, structAlignmentFor(node->typeKey), "abi.sret.slot");
            arguments.push_back(sretSlot);
            parameterTypes.push_back(ptr);
        }
        std::vector<LLVMValueRef> sourceArguments;
        std::vector<LLVMTypeRef> sourceParameterTypes;
        sourceArguments.reserve(node->children.size() - 1);
        sourceParameterTypes.reserve(node->children.size() - 1);
        assignedValue = nullptr;
        for (std::size_t index = 1; index < node->children.size(); ++index)
        {
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index - 1]
                : lowerExpression(node->children[index].get(), error);
            LLVMTypeRef parameterType = abiParameterType(node->children[index]->typeKey, error);
            if (value == nullptr || parameterType == nullptr)
            {
                return false;
            }
            if (node->returnsAssignedValue && index + 1U == node->children.size())
                assignedValue = value;
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return false;
            sourceArguments.push_back(value);
            sourceParameterTypes.push_back(parameterType);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, sourceArguments.size(), parameterToSource, error)) return false;
        for (const std::size_t source : parameterToSource)
        {
            arguments.push_back(sourceArguments[source]);
            parameterTypes.push_back(sourceParameterTypes[source]);
        }
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerVirtualCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef receiverOverride ,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (!node->virtualSlot.has_value())
            return failValue(error, "Macro A virtual call is missing its vtable slot");
        if (node->children.empty())
            return failValue(error, "Macro A virtual call is missing its receiver operand");
        if (descriptorType_ == nullptr)
            return failValue(error, "Macro A virtual call has no class-descriptor LLVM type");
        LLVMValueRef receiver = receiverOverride != nullptr
            ? receiverOverride : lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr) return nullptr;
        receiver = checkedReceiver(receiver, error);
        if (receiver == nullptr) return nullptr;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef target = loadVirtualTarget(node, receiver, error);
        if (target == nullptr) return nullptr;
        std::vector<LLVMValueRef> arguments;
        std::vector<LLVMTypeRef> parameterTypes;
        // Win64 aggregate ABI -- the reconstructed callee type must match what
        // declareFunctions gave every override: sret slot after the receiver, pointer-typed
        // indirect struct parameters.
        LLVMValueRef sretSlot = nullptr;
        LLVMValueRef assignedValue = nullptr;
        if (!prepareDynamicCallArguments(
                node, receiver, argumentOverrides, arguments, parameterTypes,
                sretSlot, assignedValue, error)) return nullptr;
        LLVMTypeRef returnType = node->referenceResult
            ? ptr
            : sretSlot != nullptr
                ? LLVMVoidTypeInContext(context_)
                : llvmType(node->typeKey, error);
        if (returnType == nullptr)
        {
            return nullptr;
        }
        LLVMTypeRef virtualFunctionType = LLVMFunctionType(
            returnType,
            parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()),
            0);
        releaseTransferredArguments(node);
        LLVMValueRef call = buildPotentiallyThrowingCall(
            virtualFunctionType, target,
            arguments.data(), static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "vcall", error);
        if (call == nullptr) return nullptr;
        if (!applyMustTail(node, call, error)) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(call, index, attribute); },
                1U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        if (node->returnsAssignedValue)
        {
            if (assignedValue == nullptr)
                return failValue(error, "virtual setter assignment call lost its converted value");
            return assignedValue;
        }
        return call;
    }

LLVMValueRef OopLlvmImplementation::loadInterfaceTarget(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error)
{
        if (!node->interfaceSlot.has_value() || !node->targetTypeOrdinal.has_value())
            return failValue(error, "T08-F05 interface target is missing identity or method slot");
        if (descriptorType_ == nullptr || interfaceMapType_ == nullptr || interfaceMapEntryType_ == nullptr)
            return failValue(error, "T08-F05 interface target has no interface-map LLVM ABI types");

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, receiver, "interface.descriptor");
        LLVMValueRef mapAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 4, "interface.map.addr");
        LLVMValueRef map = LLVMBuildLoad2(builder_, ptr, mapAddress, "interface.map");

        LLVMBasicBlockRef mapBlock = appendBlock("interface.map.ok");
        LLVMBasicBlockRef searchBlock = appendBlock("interface.search");
        LLVMBasicBlockRef probeBlock = appendBlock("interface.probe");
        LLVMBasicBlockRef nextBlock = appendBlock("interface.next");
        LLVMBasicBlockRef foundBlock = appendBlock("interface.found");
        LLVMBasicBlockRef missingBlock = appendBlock("interface.missing");
        LLVMValueRef hasMap = LLVMBuildICmp(
            builder_, LLVMIntNE, map, LLVMConstNull(ptr), "interface.map.nonnull");
        LLVMBuildCondBr(builder_, hasMap, mapBlock, missingBlock);

        enterBlock(mapBlock);
        LLVMValueRef countAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 0, "interface.map.count.addr");
        LLVMValueRef count = LLVMBuildLoad2(builder_, i64, countAddress, "interface.map.count");
        LLVMValueRef entriesAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 1, "interface.map.entries.addr");
        LLVMValueRef entries = LLVMBuildLoad2(builder_, ptr, entriesAddress, "interface.map.entries");
        LLVMBuildBr(builder_, searchBlock);

        enterBlock(searchBlock);
        LLVMValueRef index = LLVMBuildPhi(builder_, i64, "interface.map.index");
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMValueRef initialValues[] = {zero};
        LLVMBasicBlockRef initialBlocks[] = {mapBlock};
        LLVMAddIncoming(index, initialValues, initialBlocks, 1);
        LLVMValueRef inRange = LLVMBuildICmp(builder_, LLVMIntULT, index, count, "interface.map.inrange");
        LLVMBuildCondBr(builder_, inRange, probeBlock, missingBlock);

        enterBlock(probeBlock);
        LLVMValueRef entryIndices[] = {index};
        LLVMValueRef entry = LLVMBuildGEP2(
            builder_, interfaceMapEntryType_, entries, entryIndices, 1, "interface.entry");
        LLVMValueRef idAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 0, "interface.entry.id.addr");
        LLVMValueRef id = LLVMBuildLoad2(builder_, i64, idAddress, "interface.entry.id");
        LLVMValueRef wantedId = LLVMConstInt(i64, node->immediate, 0);
        LLVMValueRef matches = LLVMBuildICmp(builder_, LLVMIntEQ, id, wantedId, "interface.entry.matches");
        LLVMBuildCondBr(builder_, matches, foundBlock, nextBlock);

        enterBlock(nextBlock);
        LLVMValueRef one = LLVMConstInt(i64, 1, 0);
        LLVMValueRef nextIndex = LLVMBuildAdd(builder_, index, one, "interface.map.next");
        LLVMBuildBr(builder_, searchBlock);
        LLVMValueRef nextValues[] = {nextIndex};
        LLVMBasicBlockRef nextBlocks[] = {nextBlock};
        LLVMAddIncoming(index, nextValues, nextBlocks, 1);

        enterBlock(missingBlock);
        emitInterfaceDispatchTrap();

        enterBlock(foundBlock);
        return loadInterfaceMethodSlot(node, entry, ptr, i64);
    }

void OopLlvmImplementation::emitInterfaceDispatchTrap()
{
        LLVMValueRef reason = LLVMConstInt(
            LLVMInt32TypeInContext(context_),
            static_cast<std::uint64_t>(LlvmTrapReason::InterfaceDispatchFailure), 0);
        LLVMValueRef trapArguments[] = {reason};
        buildNoUnwindCall(trapFunctionType_, trapFunction_, trapArguments, 1, "");
        LLVMBuildUnreachable(builder_);
    }

LLVMValueRef OopLlvmImplementation::loadInterfaceMethodSlot(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef entry,
        LLVMTypeRef ptr,
        LLVMTypeRef i64)
{
        LLVMValueRef tableAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 1, "interface.entry.table.addr");
        LLVMValueRef methodTable = LLVMBuildLoad2(builder_, ptr, tableAddress, "interface.method.table");
        LLVMValueRef slotIndex = LLVMConstInt(i64, *node->interfaceSlot, 0);
        LLVMValueRef slotIndices[] = {slotIndex};
        LLVMValueRef slotAddress = LLVMBuildGEP2(
            builder_, ptr, methodTable, slotIndices, 1, "interface.slot.addr");
        return LLVMBuildLoad2(builder_, ptr, slotAddress, "interface.target");
    }

LLVMValueRef OopLlvmImplementation::lowerInterfaceCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef receiverOverride ,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (!node->interfaceSlot.has_value() || !node->targetTypeOrdinal.has_value())
        {
            return failValue(error, "Stage 36 interface call is missing interface identity or method slot");
        }
        if (node->children.empty())
        {
            return failValue(error, "Stage 36 interface call is missing its receiver operand");
        }
        if (descriptorType_ == nullptr || interfaceMapType_ == nullptr || interfaceMapEntryType_ == nullptr)
        {
            return failValue(error, "Stage 36 interface call has no interface-map LLVM ABI types");
        }

        LLVMValueRef receiver = receiverOverride != nullptr
            ? receiverOverride
            : lowerExpression(node->children.front().get(), error);
        if (receiver == nullptr)
        {
            return nullptr;
        }
        receiver = checkedReceiver(receiver, error);
        if (receiver == nullptr)
        {
            return nullptr;
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef target = loadInterfaceTarget(node, receiver, error);
        if (target == nullptr) return nullptr;

        // Win64 aggregate ABI -- same reconstruction rule as lowerVirtualCall: the callee type
        // must match what declareFunctions gave every implementation.
        LLVMValueRef sretSlot = nullptr;
        std::vector<LLVMValueRef> arguments;
        std::vector<LLVMTypeRef> parameterTypes;
        LLVMValueRef assignedValue = nullptr;
        if (!prepareDynamicCallArguments(
                node, receiver, argumentOverrides, arguments, parameterTypes,
                sretSlot, assignedValue, error)) return nullptr;
        LLVMTypeRef returnType = node->referenceResult
            ? ptr
            : sretSlot != nullptr
                ? LLVMVoidTypeInContext(context_)
                : llvmType(node->typeKey, error);
        if (returnType == nullptr)
        {
            return nullptr;
        }
        LLVMTypeRef interfaceFunctionType = LLVMFunctionType(
            returnType,
            parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()),
            0);
        releaseTransferredArguments(node);
        LLVMValueRef interfaceCall = buildPotentiallyThrowingCall(
            interfaceFunctionType, target,
            arguments.data(), static_cast<unsigned>(arguments.size()),
            node->typeKey == "builtin:void" || sretSlot != nullptr ? "" : "icall", error);
        if (interfaceCall == nullptr) return nullptr;
        if (!applyMustTail(node, interfaceCall, error)) return nullptr;
        if (sretSlot != nullptr)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex attributeIndex, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(interfaceCall, attributeIndex, attribute); },
                1U, node->typeKey);
            LLVMTypeRef carrier = llvmType(node->typeKey, error);
            return LLVMBuildLoad2(builder_, carrier, sretSlot, "abi.sret.load");
        }
        if (node->returnsAssignedValue)
        {
            if (assignedValue == nullptr)
                return failValue(error, "interface setter assignment call lost its converted value");
            return assignedValue;
        }
        return interfaceCall;
    }

}
