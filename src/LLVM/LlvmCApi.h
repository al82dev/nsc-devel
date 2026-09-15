#pragma once

#include <cstddef>
#include <cstdint>

namespace nativesharp::llvm_api
{
extern "C"
{
struct LLVMOpaqueContext;
struct LLVMOpaqueModule;
struct LLVMOpaqueType;
struct LLVMOpaqueValue;
struct LLVMOpaqueBasicBlock;
struct LLVMOpaqueBuilder;
struct LLVMOpaqueTarget;
struct LLVMOpaqueTargetMachine;
struct LLVMOpaqueTargetData;
struct LLVMOpaqueError;
struct LLVMOpaquePassBuilderOptions;
struct LLVMOpaqueOperandBundle;
struct LLVMOpaqueMetadata;
struct LLVMOpaqueDIBuilder;
struct LLVMOpaqueDbgRecord;
struct LLVMComdat;

using LLVMContextRef = LLVMOpaqueContext*;
using LLVMModuleRef = LLVMOpaqueModule*;
using LLVMTypeRef = LLVMOpaqueType*;
using LLVMValueRef = LLVMOpaqueValue*;
using LLVMBasicBlockRef = LLVMOpaqueBasicBlock*;
using LLVMBuilderRef = LLVMOpaqueBuilder*;
using LLVMTargetRef = LLVMOpaqueTarget*;
using LLVMTargetMachineRef = LLVMOpaqueTargetMachine*;
using LLVMTargetDataRef = LLVMOpaqueTargetData*;
using LLVMErrorRef = LLVMOpaqueError*;
using LLVMPassBuilderOptionsRef = LLVMOpaquePassBuilderOptions*;
using LLVMOperandBundleRef = LLVMOpaqueOperandBundle*;
using LLVMMetadataRef = LLVMOpaqueMetadata*;
using LLVMDIBuilderRef = LLVMOpaqueDIBuilder*;
using LLVMDbgRecordRef = LLVMOpaqueDbgRecord*;
using LLVMComdatRef = LLVMComdat*;
using LLVMBool = int;

// llvm-c/Core.h LLVMCallConv::LLVMX86VectorCallCallConv.
inline constexpr unsigned LLVMX86VectorCallCallConv = 80;

enum LLVMModuleFlagBehavior
{
    LLVMModuleFlagBehaviorError = 0,
    LLVMModuleFlagBehaviorWarning = 1,
    LLVMModuleFlagBehaviorRequire = 2,
    LLVMModuleFlagBehaviorOverride = 3,
    LLVMModuleFlagBehaviorAppend = 4,
    LLVMModuleFlagBehaviorAppendUnique = 5,
};

enum LLVMDWARFSourceLanguage
{
    // Exact ordinal from llvm-c/DebugInfo.h in LLVM 23.
    LLVMDWARFSourceLanguageC_sharp = 48,
};

enum LLVMDWARFEmissionKind
{
    LLVMDWARFEmissionNone = 0,
    LLVMDWARFEmissionFull = 1,
    LLVMDWARFEmissionLineTablesOnly = 2,
};

enum LLVMChecksumKind
{
    CSK_MD5,
    CSK_SHA1,
    CSK_SHA256
};

enum LLVMDIFlags
{
    LLVMDIFlagZero = 0,
    LLVMDIFlagPrivate = 1,
    LLVMDIFlagProtected = 2,
    LLVMDIFlagPublic = 3,
    LLVMDIFlagArtificial = 1 << 6,
    LLVMDIFlagObjectPointer = 1 << 10,
    LLVMDIFlagThunk = 1 << 25,
};

constexpr unsigned LLVMDWARFTagClassType = 0x02;
constexpr unsigned LLVMDWARFTagStructureType = 0x13;

using LLVMDWARFTypeEncoding = unsigned;
constexpr LLVMDWARFTypeEncoding LLVMDWARFTypeEncodingAddress = 0x01;
constexpr LLVMDWARFTypeEncoding LLVMDWARFTypeEncodingBoolean = 0x02;
constexpr LLVMDWARFTypeEncoding LLVMDWARFTypeEncodingFloat = 0x04;
constexpr LLVMDWARFTypeEncoding LLVMDWARFTypeEncodingSigned = 0x05;
constexpr LLVMDWARFTypeEncoding LLVMDWARFTypeEncodingSignedChar = 0x06;
constexpr LLVMDWARFTypeEncoding LLVMDWARFTypeEncodingUnsigned = 0x07;
constexpr LLVMDWARFTypeEncoding LLVMDWARFTypeEncodingUnsignedChar = 0x08;

// Values are fixed by llvm-c/Core.h and llvm-c/Analysis.h.
enum LLVMIntPredicate
{
    LLVMIntEQ = 32,
    LLVMIntNE = 33,
    LLVMIntUGT = 34,
    LLVMIntUGE = 35,
    LLVMIntULT = 36,
    LLVMIntULE = 37,
    LLVMIntSGT = 38,
    LLVMIntSGE = 39,
    LLVMIntSLT = 40,
    LLVMIntSLE = 41
};

enum LLVMRealPredicate
{
    LLVMRealPredicateFalse = 0,
    LLVMRealOEQ = 1,
    LLVMRealOGT = 2,
    LLVMRealOGE = 3,
    LLVMRealOLT = 4,
    LLVMRealOLE = 5,
    LLVMRealONE = 6,
    LLVMRealORD = 7,
    LLVMRealUNO = 8,
    LLVMRealUEQ = 9,
    LLVMRealUGT = 10,
    LLVMRealUGE = 11,
    LLVMRealULT = 12,
    LLVMRealULE = 13,
    LLVMRealUNE = 14,
    LLVMRealPredicateTrue = 15
};

// Stage 54.2 "Atomics" -- values fixed by llvm-c/Core.h, same "hand-maintained subset, exact
// upstream numeric values" discipline as LLVMIntPredicate/LLVMRealPredicate above. Only the
// members this compiler actually emits are listed (LLVMAtomicRMWBinOpXchg for Interlocked.
// Exchange, LLVMAtomicRMWBinOpAdd for Interlocked.Increment/Decrement/Add -- Decrement is
// implemented as Add with a negated operand, no separate Sub variant needed) -- omitting the
// rest (And/Or/Xor/Max/Min/floating-point variants/UIncWrap/UUSubCond/etc.) does not change
// their real enumerator values, since C enums are numbered in declaration order starting at 0
// and every earlier member is still present here in the same order.
enum LLVMAtomicRMWBinOp
{
    LLVMAtomicRMWBinOpXchg = 0,
    LLVMAtomicRMWBinOpAdd = 1
};

// Stage 54.2 "Atomics" -- values fixed by llvm-c/Core.h. Only SequentiallyConsistent is used
// (the strongest, simplest-to-reason-about ordering -- C#'s System.Threading.Interlocked
// documented contract is itself full sequential consistency, so this is not a place to economize
// with a weaker ordering), but every earlier enumerator is listed in its real declared order so
// the numeric value fixed below (7) is correct.
enum LLVMAtomicOrdering
{
    LLVMAtomicOrderingNotAtomic = 0,
    LLVMAtomicOrderingUnordered = 1,
    LLVMAtomicOrderingMonotonic = 2,
    LLVMAtomicOrderingAcquire = 4,
    LLVMAtomicOrderingRelease = 5,
    LLVMAtomicOrderingAcquireRelease = 6,
    LLVMAtomicOrderingSequentiallyConsistent = 7
};

enum LLVMCodeGenOptLevel
{
    LLVMCodeGenLevelNone = 0,
    LLVMCodeGenLevelLess = 1,
    LLVMCodeGenLevelDefault = 2,
    LLVMCodeGenLevelAggressive = 3
};

enum LLVMRelocMode
{
    LLVMRelocDefault = 0,
    LLVMRelocStatic = 1,
    LLVMRelocPIC = 2,
    LLVMRelocDynamicNoPic = 3,
    LLVMRelocROPI = 4,
    LLVMRelocRWPI = 5,
    LLVMRelocROPI_RWPI = 6
};

enum LLVMCodeModel
{
    LLVMCodeModelDefault = 0,
    LLVMCodeModelJITDefault = 1,
    LLVMCodeModelTiny = 2,
    LLVMCodeModelSmall = 3,
    LLVMCodeModelKernel = 4,
    LLVMCodeModelMedium = 5,
    LLVMCodeModelLarge = 6
};

enum LLVMCodeGenFileType
{
    LLVMAssemblyFile = 0,
    LLVMObjectFile = 1
};

enum LLVMVerifierFailureAction
{
    LLVMAbortProcessAction = 0,
    LLVMPrintMessageAction = 1,
    LLVMReturnStatusAction = 2
};

// Exact values from llvm-c/Core.h. NativeSharp only needs external (the default), internal and
// private today; spelling the numeric values explicitly avoids recreating unrelated linkage modes
// merely to keep their upstream ordinals.
enum LLVMLinkage
{
    LLVMExternalLinkage = 0,
    LLVMAvailableExternallyLinkage = 1,
    LLVMLinkOnceAnyLinkage = 2,
    LLVMLinkOnceODRLinkage = 3,
    LLVMLinkOnceODRAutoHideLinkage = 4,
    LLVMWeakAnyLinkage = 5,
    LLVMWeakODRLinkage = 6,
    LLVMAppendingLinkage = 7,
    LLVMInternalLinkage = 8,
    LLVMPrivateLinkage = 9
};

enum LLVMComdatSelectionKind
{
    LLVMAnyComdatSelectionKind = 0,
    LLVMExactMatchComdatSelectionKind = 1,
    LLVMLargestComdatSelectionKind = 2,
    LLVMNoDeduplicateComdatSelectionKind = 3,
    LLVMSameSizeComdatSelectionKind = 4
};

enum LLVMInlineAsmDialect
{
    LLVMInlineAsmDialectATT = 0,
    LLVMInlineAsmDialectIntel = 1
};

LLVMComdatRef LLVMGetOrInsertComdat(LLVMModuleRef module, const char* name);
void LLVMSetComdat(LLVMValueRef value, LLVMComdatRef comdat);
void LLVMSetComdatSelectionKind(LLVMComdatRef comdat, LLVMComdatSelectionKind kind);

LLVMContextRef LLVMContextCreate();
void LLVMContextDispose(LLVMContextRef context);
LLVMModuleRef LLVMModuleCreateWithNameInContext(const char* moduleId, LLVMContextRef context);
void LLVMDisposeModule(LLVMModuleRef module);
void LLVMSetTarget(LLVMModuleRef module, const char* triple);
void LLVMSetDataLayout(LLVMModuleRef module, const char* dataLayout);
void LLVMAddModuleFlag(
    LLVMModuleRef module,
    LLVMModuleFlagBehavior behavior,
    const char* key,
    std::size_t keyLength,
    LLVMMetadataRef value);
char* LLVMPrintModuleToString(LLVMModuleRef module);
void LLVMDisposeMessage(char* message);
// NSC-LIMIT-005. El triple y el data layout SEGUN EL MODULO, no segun el TargetMachine: entre uno
// y otro corre `configureModuleForTarget`, y lo que hay que registrar es lo que la API C tenia
// delante cuando decidio. Las dos cadenas pertenecen al modulo: no se liberan.
const char* LLVMGetTarget(LLVMModuleRef module);
const char* LLVMGetDataLayoutStr(LLVMModuleRef module);
// La version de la biblioteca CARGADA, que no tiene por que ser contra la que se compilo.
void LLVMGetVersion(unsigned* major, unsigned* minor, unsigned* patch);
// NSC-LIMIT-005 -- LA SONDA QUE COMPRUEBA POR DONDE HABLA LLVM.
//
// `LLVMDumpType` escribe por el `errs()` de LA BIBLIOTECA. Es la unica forma de saber si la salida
// de error de LLVM llega a nuestra captura: una marca escrita por NOSOTROS solo demuestra que
// nuestro propio descriptor esta desviado, y en Windows la DLL de LLVM puede tener otro. Se elige
// un TIPO y no un modulo porque imprime tres bytes --«i32»-- y no tiene ningun efecto.
LLVMContextRef LLVMGetModuleContext(LLVMModuleRef module);
LLVMMetadataRef LLVMValueAsMetadata(LLVMValueRef value);
void LLVMDumpType(LLVMTypeRef type);
LLVMBool LLVMVerifyModule(
    LLVMModuleRef module,
    LLVMVerifierFailureAction action,
    char** outMessage);

unsigned LLVMDebugMetadataVersion();
LLVMDIBuilderRef LLVMCreateDIBuilder(LLVMModuleRef module);
void LLVMDisposeDIBuilder(LLVMDIBuilderRef builder);
void LLVMDIBuilderFinalize(LLVMDIBuilderRef builder);
LLVMMetadataRef LLVMDIBuilderCreateCompileUnit(
    LLVMDIBuilderRef builder,
    LLVMDWARFSourceLanguage language,
    LLVMMetadataRef file,
    const char* producer,
    std::size_t producerLength,
    LLVMBool optimized,
    const char* flags,
    std::size_t flagsLength,
    unsigned runtimeVersion,
    const char* splitName,
    std::size_t splitNameLength,
    LLVMDWARFEmissionKind kind,
    unsigned dwoId,
    LLVMBool splitDebugInlining,
    LLVMBool debugInfoForProfiling,
    const char* sysRoot,
    std::size_t sysRootLength,
    const char* sdk,
    std::size_t sdkLength);
LLVMMetadataRef LLVMDIBuilderCreateFile(
    LLVMDIBuilderRef builder,
    const char* filename,
    std::size_t filenameLength,
    const char* directory,
    std::size_t directoryLength);
LLVMMetadataRef LLVMDIBuilderCreateFileWithChecksum(
    LLVMDIBuilderRef builder,
    const char* filename,
    std::size_t filenameLength,
    const char* directory,
    std::size_t directoryLength,
    LLVMChecksumKind checksumKind,
    const char* checksum,
    std::size_t checksumLength,
    const char* source,
    std::size_t sourceLength);
LLVMMetadataRef LLVMDIBuilderCreateSubroutineType(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef file,
    LLVMMetadataRef* parameterTypes,
    unsigned parameterTypeCount,
    LLVMDIFlags flags);
LLVMMetadataRef LLVMDIBuilderCreateFunction(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef scope,
    const char* name,
    std::size_t nameLength,
    const char* linkageName,
    std::size_t linkageNameLength,
    LLVMMetadataRef file,
    unsigned line,
    LLVMMetadataRef type,
    LLVMBool localToUnit,
    LLVMBool definition,
    unsigned scopeLine,
    LLVMDIFlags flags,
    LLVMBool optimized);
LLVMMetadataRef LLVMDIBuilderCreateLexicalBlockFile(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef scope,
    LLVMMetadataRef file,
    unsigned discriminator);
LLVMMetadataRef LLVMDIBuilderCreateLexicalBlock(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef scope,
    LLVMMetadataRef file,
    unsigned line,
    unsigned column);
LLVMMetadataRef LLVMDIBuilderCreateDebugLocation(
    LLVMContextRef context,
    unsigned line,
    unsigned column,
    LLVMMetadataRef scope,
    LLVMMetadataRef inlinedAt);
LLVMMetadataRef LLVMDIBuilderCreateUnspecifiedType(
    LLVMDIBuilderRef builder,
    const char* name,
    std::size_t nameLength);
LLVMMetadataRef LLVMDIBuilderCreateBasicType(
    LLVMDIBuilderRef builder,
    const char* name,
    std::size_t nameLength,
    std::uint64_t sizeInBits,
    LLVMDWARFTypeEncoding encoding,
    LLVMDIFlags flags);
LLVMMetadataRef LLVMDIBuilderCreatePointerType(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef pointeeType,
    std::uint64_t sizeInBits,
    std::uint32_t alignInBits,
    unsigned addressSpace,
    const char* name,
    std::size_t nameLength);
LLVMMetadataRef LLVMDIBuilderCreateEnumerator(
    LLVMDIBuilderRef builder,
    const char* name,
    std::size_t nameLength,
    std::int64_t value,
    LLVMBool isUnsigned);
LLVMMetadataRef LLVMDIBuilderCreateEnumerationType(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef scope,
    const char* name,
    std::size_t nameLength,
    LLVMMetadataRef file,
    unsigned line,
    std::uint64_t sizeInBits,
    std::uint32_t alignInBits,
    LLVMMetadataRef* elements,
    unsigned elementCount,
    LLVMMetadataRef classType);
LLVMMetadataRef LLVMDIBuilderCreateUnionType(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef scope,
    const char* name,
    std::size_t nameLength,
    LLVMMetadataRef file,
    unsigned line,
    std::uint64_t sizeInBits,
    std::uint32_t alignInBits,
    LLVMDIFlags flags,
    LLVMMetadataRef* elements,
    unsigned elementCount,
    unsigned runtimeLanguage,
    const char* uniqueId,
    std::size_t uniqueIdLength);
LLVMMetadataRef LLVMDIBuilderGetOrCreateSubrange(
    LLVMDIBuilderRef builder,
    std::int64_t lowerBound,
    std::int64_t count);
LLVMMetadataRef LLVMDIBuilderCreateArrayType(
    LLVMDIBuilderRef builder,
    std::uint64_t sizeInBits,
    std::uint32_t alignInBits,
    LLVMMetadataRef elementType,
    LLVMMetadataRef* subscripts,
    unsigned subscriptCount);
LLVMMetadataRef LLVMDIBuilderCreateClassType(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef scope,
    const char* name,
    std::size_t nameLength,
    LLVMMetadataRef file,
    unsigned line,
    std::uint64_t sizeInBits,
    std::uint32_t alignInBits,
    std::uint64_t offsetInBits,
    LLVMDIFlags flags,
    LLVMMetadataRef derivedFrom,
    LLVMMetadataRef* elements,
    unsigned elementCount,
    LLVMMetadataRef vtableHolder,
    LLVMMetadataRef templateParameters,
    const char* uniqueId,
    std::size_t uniqueIdLength);
LLVMMetadataRef LLVMDIBuilderCreateInheritance(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef type,
    LLVMMetadataRef baseType,
    std::uint64_t baseOffset,
    std::uint32_t virtualBasePointerOffset,
    LLVMDIFlags flags);
LLVMMetadataRef LLVMDIBuilderCreateStructType(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef scope,
    const char* name,
    std::size_t nameLength,
    LLVMMetadataRef file,
    unsigned line,
    std::uint64_t sizeInBits,
    std::uint32_t alignInBits,
    LLVMDIFlags flags,
    LLVMMetadataRef derivedFrom,
    LLVMMetadataRef* elements,
    unsigned elementCount,
    unsigned runtimeLanguage,
    LLVMMetadataRef vtableHolder,
    const char* uniqueId,
    std::size_t uniqueIdLength);
LLVMMetadataRef LLVMDIBuilderCreateMemberType(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef scope,
    const char* name,
    std::size_t nameLength,
    LLVMMetadataRef file,
    unsigned line,
    std::uint64_t sizeInBits,
    std::uint32_t alignInBits,
    std::uint64_t offsetInBits,
    LLVMDIFlags flags,
    LLVMMetadataRef type);
LLVMMetadataRef LLVMDIBuilderCreateReplaceableCompositeType(
    LLVMDIBuilderRef builder,
    unsigned tag,
    const char* name,
    std::size_t nameLength,
    LLVMMetadataRef scope,
    LLVMMetadataRef file,
    unsigned line,
    unsigned runtimeLanguage,
    std::uint64_t sizeInBits,
    std::uint32_t alignInBits,
    LLVMDIFlags flags,
    const char* uniqueId,
    std::size_t uniqueIdLength);
LLVMMetadataRef LLVMDIBuilderCreateExpression(
    LLVMDIBuilderRef builder,
    std::uint64_t* addressOperations,
    std::size_t addressOperationCount);
LLVMMetadataRef LLVMDIBuilderCreateAutoVariable(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef scope,
    const char* name,
    std::size_t nameLength,
    LLVMMetadataRef file,
    unsigned line,
    LLVMMetadataRef type,
    LLVMBool alwaysPreserve,
    LLVMDIFlags flags,
    std::uint32_t alignInBits);
LLVMMetadataRef LLVMDIBuilderCreateParameterVariable(
    LLVMDIBuilderRef builder,
    LLVMMetadataRef scope,
    const char* name,
    std::size_t nameLength,
    unsigned argumentNumber,
    LLVMMetadataRef file,
    unsigned line,
    LLVMMetadataRef type,
    LLVMBool alwaysPreserve,
    LLVMDIFlags flags);
LLVMDbgRecordRef LLVMDIBuilderInsertDeclareRecordAtEnd(
    LLVMDIBuilderRef builder,
    LLVMValueRef storage,
    LLVMMetadataRef variable,
    LLVMMetadataRef expression,
    LLVMMetadataRef location,
    LLVMBasicBlockRef block);
LLVMDbgRecordRef LLVMDIBuilderInsertDbgValueRecordAtEnd(
    LLVMDIBuilderRef builder,
    LLVMValueRef value,
    LLVMMetadataRef variable,
    LLVMMetadataRef expression,
    LLVMMetadataRef location,
    LLVMBasicBlockRef block);
void LLVMMetadataReplaceAllUsesWith(
    LLVMMetadataRef temporaryTarget,
    LLVMMetadataRef replacement);
void LLVMSetSubprogram(LLVMValueRef function, LLVMMetadataRef subprogram);
void LLVMSetIsNewDbgInfoFormat(LLVMModuleRef module, LLVMBool useNewFormat);
LLVMMetadataRef LLVMGetCurrentDebugLocation2(LLVMBuilderRef builder);
void LLVMSetCurrentDebugLocation2(LLVMBuilderRef builder, LLVMMetadataRef location);

void LLVMInitializeX86TargetInfo();
void LLVMInitializeX86Target();
void LLVMInitializeX86TargetMC();
void LLVMInitializeX86AsmParser();
void LLVMInitializeX86AsmPrinter();
void LLVMInitializeAArch64TargetInfo();
void LLVMInitializeAArch64Target();
void LLVMInitializeAArch64TargetMC();
void LLVMInitializeAArch64AsmParser();
void LLVMInitializeAArch64AsmPrinter();
LLVMBool LLVMGetTargetFromTriple(const char* triple, LLVMTargetRef* target, char** errorMessage);
LLVMTargetMachineRef LLVMCreateTargetMachine(
    LLVMTargetRef target,
    const char* triple,
    const char* cpu,
    const char* features,
    LLVMCodeGenOptLevel level,
    LLVMRelocMode reloc,
    LLVMCodeModel codeModel);
void LLVMDisposeTargetMachine(LLVMTargetMachineRef targetMachine);
LLVMTargetDataRef LLVMCreateTargetDataLayout(LLVMTargetMachineRef targetMachine);
char* LLVMCopyStringRepOfTargetData(LLVMTargetDataRef targetData);
void LLVMDisposeTargetData(LLVMTargetDataRef targetData);
void LLVMSetModuleDataLayout(LLVMModuleRef module, LLVMTargetDataRef targetData);
LLVMBool LLVMTargetMachineEmitToFile(
    LLVMTargetMachineRef targetMachine,
    LLVMModuleRef module,
    char* filename,
    LLVMCodeGenFileType fileType,
    char** errorMessage);

LLVMPassBuilderOptionsRef LLVMCreatePassBuilderOptions();
void LLVMPassBuilderOptionsSetVerifyEach(LLVMPassBuilderOptionsRef options, LLVMBool verifyEach);
// NSC-LIMIT-005. El registro de pases del new pass manager, y el unico camino a «que se estaba
// ejecutando» que no exige la API C++ de LLVM.
//
// LA API C NO ACEPTA UN SINK: LLVM lo escribe por la salida de error del PROCESO --`llvm::dbgs()`--
// y no hay entrada C para redirigirlo. Por eso LlvmOptimizer captura el descriptor 2 alrededor de
// la llamada en vez de pedirle a LLVM que le entregue el texto.
void LLVMPassBuilderOptionsSetDebugLogging(
    LLVMPassBuilderOptionsRef options, LLVMBool debugLogging);
void LLVMDisposePassBuilderOptions(LLVMPassBuilderOptionsRef options);
LLVMErrorRef LLVMRunPasses(
    LLVMModuleRef module,
    const char* passes,
    LLVMTargetMachineRef targetMachine,
    LLVMPassBuilderOptionsRef options);
// Bug B-001 (EH+RAII reuse-after-catch crash, see BUGS_ENCONTRADOS/
// bug_try_catch_reuso_objeto_tras_catch.md and 30_..._eh_raii_mem2reg_fix.md) fix support --
// LLVMRunPassesOnFunction runs a textual pass pipeline against ONE function instead of the
// whole module, letting LlvmOptimizer::optimize scope the "mem2reg" legalization pass to only
// the functions that actually declare a Windows SEH personality (LLVMHasPersonalityFn), instead
// of promoting every -O0 local in the whole program -- see that function's own header comment
// for why a module-wide mem2reg-at-O0 would be the wrong fix.
LLVMErrorRef LLVMRunPassesOnFunction(
    LLVMValueRef function,
    const char* passes,
    LLVMTargetMachineRef targetMachine,
    LLVMPassBuilderOptionsRef options);
char* LLVMGetErrorMessage(LLVMErrorRef error);
void LLVMDisposeErrorMessage(char* errorMessage);
LLVMValueRef LLVMGetFirstFunction(LLVMModuleRef module);
LLVMValueRef LLVMGetNextFunction(LLVMValueRef function);
LLVMBool LLVMIsDeclaration(LLVMValueRef global);
LLVMBool LLVMHasPersonalityFn(LLVMValueRef function);

LLVMTypeRef LLVMVoidTypeInContext(LLVMContextRef context);
LLVMTypeRef LLVMInt1TypeInContext(LLVMContextRef context);
LLVMTypeRef LLVMInt8TypeInContext(LLVMContextRef context);
LLVMTypeRef LLVMInt16TypeInContext(LLVMContextRef context);
LLVMTypeRef LLVMInt32TypeInContext(LLVMContextRef context);
LLVMTypeRef LLVMInt64TypeInContext(LLVMContextRef context);
LLVMTypeRef LLVMHalfTypeInContext(LLVMContextRef context);
LLVMTypeRef LLVMFloatTypeInContext(LLVMContextRef context);
LLVMTypeRef LLVMDoubleTypeInContext(LLVMContextRef context);
LLVMTypeRef LLVMVectorType(LLVMTypeRef elementType, unsigned elementCount);
LLVMTypeRef LLVMPointerTypeInContext(LLVMContextRef context, unsigned addressSpace);
LLVMTypeRef LLVMArrayType2(LLVMTypeRef elementType, std::uint64_t elementCount);
LLVMTypeRef LLVMStructCreateNamed(LLVMContextRef context, const char* name);
LLVMTypeRef LLVMStructTypeInContext(
    LLVMContextRef context,
    LLVMTypeRef* elementTypes,
    unsigned elementCount,
    LLVMBool packed);
void LLVMStructSetBody(
    LLVMTypeRef structType,
    LLVMTypeRef* elementTypes,
    unsigned elementCount,
    LLVMBool packed);
LLVMTypeRef LLVMFunctionType(
    LLVMTypeRef returnType,
    LLVMTypeRef* parameterTypes,
    unsigned parameterCount,
    LLVMBool variadic);

LLVMValueRef LLVMConstInt(LLVMTypeRef integerType, unsigned long long value, LLVMBool signExtend);
LLVMValueRef LLVMConstReal(LLVMTypeRef realType, double value);
LLVMValueRef LLVMConstNull(LLVMTypeRef type);
LLVMValueRef LLVMConstArray2(
    LLVMTypeRef elementType,
    LLVMValueRef* constantValues,
    std::uint64_t length);
LLVMValueRef LLVMConstNamedStruct(
    LLVMTypeRef structType,
    LLVMValueRef* constantValues,
    unsigned count);
LLVMValueRef LLVMConstStringInContext2(
    LLVMContextRef context,
    const char* value,
    std::size_t length,
    LLVMBool dontNullTerminate);
LLVMValueRef LLVMConstInBoundsGEP2(
    LLVMTypeRef type,
    LLVMValueRef constantValue,
    LLVMValueRef* constantIndices,
    unsigned indexCount);
LLVMValueRef LLVMGetUndef(LLVMTypeRef type);
LLVMValueRef LLVMConstVector(LLVMValueRef* scalarConstantValues, unsigned size);

LLVMValueRef LLVMAddGlobal(LLVMModuleRef module, LLVMTypeRef type, const char* name);
void LLVMSetInitializer(LLVMValueRef global, LLVMValueRef constantValue);
void LLVMSetGlobalConstant(LLVMValueRef global, LLVMBool isConstant);
LLVMValueRef LLVMAddFunction(LLVMModuleRef module, const char* name, LLVMTypeRef functionType);
LLVMValueRef LLVMGetNamedFunction(LLVMModuleRef module, const char* name);
void LLVMSetFunctionCallConv(LLVMValueRef function, unsigned callingConvention);
void LLVMSetLinkage(LLVMValueRef global, LLVMLinkage linkage);
LLVMValueRef LLVMGetParam(LLVMValueRef function, unsigned index);
void LLVMSetValueName2(LLVMValueRef value, const char* name, std::size_t nameLength);
const char* LLVMGetValueName2(LLVMValueRef value, std::size_t* nameLength);
void LLVMSetPersonalityFn(LLVMValueRef function, LLVMValueRef personality);

// Function-attribute API (llvm-c/Core.h) -- used to mark every emitted function `uwtable`, the
// way every real front end (clang included) does for any Windows x64 target. See
// BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md: without it, a function with more
// than one exception-handling region (more than one catchswitch/cleanuppad reachable from a
// shared, outer RAII cleanup) can crash at runtime even along a path that touches neither region,
// because the backend is not told it must always materialize a complete unwind table for the
// whole function.
struct LLVMOpaqueAttributeRef;
using LLVMAttributeRef = LLVMOpaqueAttributeRef*;
using LLVMAttributeIndex = unsigned;
unsigned LLVMGetEnumAttributeKindForName(const char* name, std::size_t sLen);
LLVMAttributeRef LLVMCreateEnumAttribute(LLVMContextRef context, unsigned kindId, std::uint64_t value);
void LLVMAddAttributeAtIndex(LLVMValueRef function, LLVMAttributeIndex index, LLVMAttributeRef attribute);

// Stage 56-prep (Win64 aggregate ABI): indirect struct returns are expressed exactly the way
// clang expresses them for x86_64-pc-windows-msvc -- a leading pointer parameter carrying
// `sret(<type>) noalias align <n>`. `sret` is a TYPE attribute (it names the pointee), so the
// enum-only constructor above cannot build it; this is llvm-c/Core.h's dedicated constructor.
// LLVMAddCallSiteAttribute is the call-site twin of LLVMAddAttributeAtIndex: the ABI contract
// must be stated on BOTH the definition and every call, matching what clang emits.
LLVMAttributeRef LLVMCreateTypeAttribute(LLVMContextRef context, unsigned kindId, LLVMTypeRef typeRef);
void LLVMAddCallSiteAttribute(LLVMValueRef call, LLVMAttributeIndex index, LLVMAttributeRef attribute);

// Win64 aggregate ABI support machinery:
//  - LLVMSetAlignment: an alloca of the `[N x i8]` struct carrier defaults to align 1, but every
//    member access through it does naturally-aligned typed loads/stores -- the slot itself must
//    carry the struct layout's real alignment.
//  - entry-block navigation: caller-side argument copies and sret result slots are materialized
//    as allocas HOISTED into the function's entry block (the way clang does), because an alloca
//    emitted at the call site would re-allocate on every loop iteration and grow the stack
//    without bound.
void LLVMSetAlignment(LLVMValueRef value, unsigned bytes);
// Item 308. Marks a load/store as an observable volatile access. This affects optimization only;
// atomic ordering and synchronization remain separate APIs and contracts.
void LLVMSetVolatile(LLVMValueRef memoryAccessInstruction, LLVMBool isVolatile);

enum LLVMFastMathFlags
{
    LLVMFastMathNone = 0,
    LLVMFastMathAllowReassoc = 1 << 0,
    LLVMFastMathNoNaNs = 1 << 1,
    LLVMFastMathNoInfs = 1 << 2,
    LLVMFastMathNoSignedZeros = 1 << 3,
    LLVMFastMathAllowReciprocal = 1 << 4,
    LLVMFastMathAllowContract = 1 << 5,
    LLVMFastMathApproxFunc = 1 << 6,
    LLVMFastMathFast = LLVMFastMathAllowReassoc | LLVMFastMathNoNaNs | LLVMFastMathNoInfs |
        LLVMFastMathNoSignedZeros | LLVMFastMathAllowReciprocal | LLVMFastMathAllowContract |
        LLVMFastMathApproxFunc
};
void LLVMSetFastMathFlags(LLVMValueRef floatingPointInstruction, LLVMFastMathFlags flags);

LLVMBasicBlockRef LLVMGetEntryBasicBlock(LLVMValueRef function);
LLVMValueRef LLVMGetFirstInstruction(LLVMBasicBlockRef block);
void LLVMPositionBuilderBefore(LLVMBuilderRef builder, LLVMValueRef instruction);

LLVMBasicBlockRef LLVMAppendBasicBlockInContext(
    LLVMContextRef context,
    LLVMValueRef function,
    const char* name);
LLVMBuilderRef LLVMCreateBuilderInContext(LLVMContextRef context);
void LLVMDisposeBuilder(LLVMBuilderRef builder);
void LLVMPositionBuilderAtEnd(LLVMBuilderRef builder, LLVMBasicBlockRef block);
LLVMBasicBlockRef LLVMGetInsertBlock(LLVMBuilderRef builder);
LLVMValueRef LLVMGetBasicBlockTerminator(LLVMBasicBlockRef block);

LLVMValueRef LLVMBuildAlloca(
    LLVMBuilderRef builder,
    LLVMTypeRef type,
    const char* name);
LLVMValueRef LLVMBuildArrayAlloca(
    LLVMBuilderRef builder,
    LLVMTypeRef type,
    LLVMValueRef count,
    const char* name);
LLVMValueRef LLVMBuildLoad2(
    LLVMBuilderRef builder,
    LLVMTypeRef type,
    LLVMValueRef pointer,
    const char* name);
LLVMValueRef LLVMBuildStore(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMValueRef pointer);

LLVMValueRef LLVMBuildNeg(LLVMBuilderRef builder, LLVMValueRef value, const char* name);
LLVMValueRef LLVMBuildFNeg(LLVMBuilderRef builder, LLVMValueRef value, const char* name);
LLVMValueRef LLVMBuildNot(LLVMBuilderRef builder, LLVMValueRef value, const char* name);
LLVMValueRef LLVMBuildAdd(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildSub(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildMul(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildSDiv(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildUDiv(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildSRem(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildURem(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildAnd(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildOr(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildXor(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildShl(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildLShr(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildAShr(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildFAdd(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildFSub(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildFMul(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildFDiv(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildFRem(
    LLVMBuilderRef builder,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);

LLVMValueRef LLVMBuildICmp(
    LLVMBuilderRef builder,
    LLVMIntPredicate predicate,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildFCmp(
    LLVMBuilderRef builder,
    LLVMRealPredicate predicate,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);

// SHR-022. Ampliacion deliberada de la frontera enumerada con LLVM: un simbolo mas, para elegir
// entre dos constantes sin abrir un bloque basico. La alternativa era aritmetica sobre el modo
// (`-11 - ((mode >> 1) & 1)`), que no habria anadido simbolos pero esconde la intencion.
LLVMValueRef LLVMBuildSelect(
    LLVMBuilderRef builder,
    LLVMValueRef condition,
    LLVMValueRef whenTrue,
    LLVMValueRef whenFalse,
    const char* name);

LLVMValueRef LLVMBuildTrunc(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildSExt(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildZExt(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildShuffleVector(
    LLVMBuilderRef builder,
    LLVMValueRef first,
    LLVMValueRef second,
    LLVMValueRef mask,
    const char* name);
LLVMValueRef LLVMBuildBitCast(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildFPTrunc(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildFPExt(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildFPToSI(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildFPToUI(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildSIToFP(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildUIToFP(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildPtrToInt(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildIntToPtr(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);
LLVMValueRef LLVMBuildPointerCast(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMTypeRef destinationType,
    const char* name);

LLVMValueRef LLVMBuildGEP2(
    LLVMBuilderRef builder,
    LLVMTypeRef elementType,
    LLVMValueRef pointer,
    LLVMValueRef* indices,
    unsigned indexCount,
    const char* name);
LLVMValueRef LLVMBuildStructGEP2(
    LLVMBuilderRef builder,
    LLVMTypeRef structType,
    LLVMValueRef pointer,
    unsigned fieldIndex,
    const char* name);
LLVMValueRef LLVMBuildPtrDiff2(
    LLVMBuilderRef builder,
    LLVMTypeRef elementType,
    LLVMValueRef left,
    LLVMValueRef right,
    const char* name);
LLVMValueRef LLVMBuildExtractValue(
    LLVMBuilderRef builder,
    LLVMValueRef aggregate,
    unsigned index,
    const char* name);

// Stage 54.2 "Atomics". LLVMBuildAtomicRMW emits a real hardware atomic read-modify-write
// instruction (lowers to `lock xadd`/`lock xchg` on x86-64) -- used for Interlocked.Increment/
// Decrement/Add/Exchange. LLVMBuildAtomicCmpXchg emits a real hardware compare-and-swap (lowers
// to `lock cmpxchg`) -- used for Interlocked.CompareExchange; its result is a `{T, i1}` aggregate
// (the loaded old value, plus whether the swap happened), so the runtime wrapper extracts index 0
// via LLVMBuildExtractValue above to get the old-value result C#'s CompareExchange contract wants
// (NativeSharp's Interlocked.CompareExchange never needs the success flag -- callers compare the
// returned old value against their own comparand exactly the way C#'s does).
LLVMValueRef LLVMBuildAtomicRMW(
    LLVMBuilderRef builder,
    LLVMAtomicRMWBinOp op,
    LLVMValueRef pointer,
    LLVMValueRef value,
    LLVMAtomicOrdering ordering,
    LLVMBool singleThread);
LLVMValueRef LLVMBuildAtomicCmpXchg(
    LLVMBuilderRef builder,
    LLVMValueRef pointer,
    LLVMValueRef compare,
    LLVMValueRef newValue,
    LLVMAtomicOrdering successOrdering,
    LLVMAtomicOrdering failureOrdering,
    LLVMBool singleThread);
// Item 307. `singleThread == 0` is a hardware/thread fence; `singleThread != 0` emits LLVM
// `syncscope("singlethread")`, which constrains compiler motion without imposing inter-thread
// hardware ordering.
LLVMValueRef LLVMBuildFence(
    LLVMBuilderRef builder,
    LLVMAtomicOrdering ordering,
    LLVMBool singleThread,
    const char* name);
// Stage 54.3 "Mutex" -- LLVMSetOrdering upgrades an ordinary (non-atomic) LLVMBuildLoad2/
// LLVMBuildStore instruction into an atomic one with the given memory ordering, WITHOUT using
// LLVMBuildAtomicRMW/LLVMBuildAtomicCmpXchg (there is no "plain atomic load"/"plain atomic store"
// builder in the LLVM C API -- an ordinary load/store instruction IS the atomic load/store, once
// its ordering is set to something other than LLVMAtomicOrderingNotAtomic). Used by
// ProjectOopLlvmLowerer.cpp's type-initializer ("cctor") guard, fixing a genuine data race: the
// guard used to be a plain non-atomic load-compare-branch-store, letting two threads that reach a
// class's very first static-field access at the same time BOTH observe "not yet initialized" and
// BOTH run the initializer concurrently. See defineTypeInitializers's own comment for the full
// double-checked-locking fix this enables.
void LLVMSetOrdering(LLVMValueRef memoryAccessInstruction, LLVMAtomicOrdering ordering);


LLVMValueRef LLVMBuildMalloc(
    LLVMBuilderRef builder,
    LLVMTypeRef type,
    const char* name);
LLVMValueRef LLVMBuildArrayMalloc(
    LLVMBuilderRef builder,
    LLVMTypeRef type,
    LLVMValueRef count,
    const char* name);
LLVMValueRef LLVMBuildFree(LLVMBuilderRef builder, LLVMValueRef pointer);
LLVMValueRef LLVMBuildGlobalStringPtr(
    LLVMBuilderRef builder,
    const char* value,
    const char* name);

LLVMValueRef LLVMBuildCall2(
    LLVMBuilderRef builder,
    LLVMTypeRef functionType,
    LLVMValueRef function,
    LLVMValueRef* arguments,
    unsigned argumentCount,
    const char* name);
void LLVMSetInstructionCallConv(LLVMValueRef instruction, unsigned callingConvention);
LLVMValueRef LLVMGetInlineAsm(
    LLVMTypeRef functionType,
    char* assembly,
    std::size_t assemblyLength,
    char* constraints,
    std::size_t constraintsLength,
    LLVMBool hasSideEffects,
    LLVMBool alignStack,
    LLVMInlineAsmDialect dialect,
    LLVMBool canThrow);
LLVMOperandBundleRef LLVMCreateOperandBundle(
    const char* tag,
    std::size_t tagLength,
    LLVMValueRef* arguments,
    unsigned argumentCount);
void LLVMDisposeOperandBundle(LLVMOperandBundleRef bundle);
LLVMValueRef LLVMBuildCallWithOperandBundles(
    LLVMBuilderRef builder,
    LLVMTypeRef functionType,
    LLVMValueRef function,
    LLVMValueRef* arguments,
    unsigned argumentCount,
    LLVMOperandBundleRef* bundles,
    unsigned bundleCount,
    const char* name);
LLVMValueRef LLVMBuildInvoke2(
    LLVMBuilderRef builder,
    LLVMTypeRef functionType,
    LLVMValueRef function,
    LLVMValueRef* arguments,
    unsigned argumentCount,
    LLVMBasicBlockRef normalBlock,
    LLVMBasicBlockRef unwindBlock,
    const char* name);
LLVMValueRef LLVMBuildInvokeWithOperandBundles(
    LLVMBuilderRef builder,
    LLVMTypeRef functionType,
    LLVMValueRef function,
    LLVMValueRef* arguments,
    unsigned argumentCount,
    LLVMBasicBlockRef normalBlock,
    LLVMBasicBlockRef unwindBlock,
    LLVMOperandBundleRef* bundles,
    unsigned bundleCount,
    const char* name);
LLVMValueRef LLVMBuildCatchSwitch(
    LLVMBuilderRef builder,
    LLVMValueRef parentPad,
    LLVMBasicBlockRef unwindBlock,
    unsigned handlerCount,
    const char* name);
void LLVMAddHandler(LLVMValueRef catchSwitch, LLVMBasicBlockRef handlerBlock);
LLVMValueRef LLVMBuildCatchPad(
    LLVMBuilderRef builder,
    LLVMValueRef catchSwitch,
    LLVMValueRef* arguments,
    unsigned argumentCount,
    const char* name);
LLVMValueRef LLVMBuildCatchRet(
    LLVMBuilderRef builder,
    LLVMValueRef catchPad,
    LLVMBasicBlockRef targetBlock);
LLVMValueRef LLVMBuildCleanupPad(
    LLVMBuilderRef builder,
    LLVMValueRef parentPad,
    LLVMValueRef* arguments,
    unsigned argumentCount,
    const char* name);
LLVMValueRef LLVMBuildCleanupRet(
    LLVMBuilderRef builder,
    LLVMValueRef cleanupPad,
    LLVMBasicBlockRef unwindBlock);
LLVMValueRef LLVMBuildPhi(LLVMBuilderRef builder, LLVMTypeRef type, const char* name);
void LLVMAddIncoming(
    LLVMValueRef phi,
    LLVMValueRef* incomingValues,
    LLVMBasicBlockRef* incomingBlocks,
    unsigned incomingCount);
LLVMValueRef LLVMBuildBr(LLVMBuilderRef builder, LLVMBasicBlockRef target);
LLVMValueRef LLVMBuildCondBr(
    LLVMBuilderRef builder,
    LLVMValueRef condition,
    LLVMBasicBlockRef trueBlock,
    LLVMBasicBlockRef falseBlock);
// `switch`. `caseCount` is only a CAPACITY HINT for LLVM's operand reservation -- the cases are
// whatever LLVMAddCase adds afterwards, so a wrong hint costs a reallocation, never correctness.
// `defaultBlock` is mandatory: a switch with no `default` section passes the block that follows the
// switch, which is exactly "an unmatched value skips the statement".
LLVMValueRef LLVMBuildSwitch(
    LLVMBuilderRef builder,
    LLVMValueRef value,
    LLVMBasicBlockRef defaultBlock,
    unsigned caseCount);
void LLVMAddCase(LLVMValueRef switchInstruction, LLVMValueRef onValue, LLVMBasicBlockRef target);
LLVMValueRef LLVMBuildRet(LLVMBuilderRef builder, LLVMValueRef value);
LLVMValueRef LLVMBuildRetVoid(LLVMBuilderRef builder);
LLVMValueRef LLVMBuildUnreachable(LLVMBuilderRef builder);
}
}
