#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

#include "nativesharp/Runtime/RuntimeAbiContract.h"

#include "../LLVM/LlvmCApi.h"

#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>

namespace nativesharp::runtime
{
namespace
{
#include <cstdio>
#include <cstdlib>

using namespace llvm_api;

// EL TOPE ERA 8 Y DEVOLVIA nullptr EN SILENCIO. Bloque 4, fase C: `CreateProcessA` tiene DIEZ
// parametros, asi que su tipo salia nulo, `LLVMAddFunction` lo recibia, y el compilador se caia
// con un SIGSEGV dentro de `llvm::PointerType::get` -- una pila que no menciona ni esta funcion ni
// el tope ni el nombre del import. El limite estaba puesto porque hasta entonces ninguna firma de
// Win32 usada aqui pasaba de ocho.
//
// SE SUBE A 16 Y SE DEJA DE DEVOLVER nullptr. Un tope superado ahora ABORTA con un mensaje, porque
// devolver nulo aqui no es un error que el llamador pueda manejar: nadie comprueba el resultado
// --son cincuenta declaraciones de imports seguidas-- y el sintoma aparece a treinta marcos de
// distancia. Entre un fallo ruidoso en el sitio y un puntero nulo que viaja, el ruidoso.
LLVMTypeRef functionType(
    LLVMTypeRef returnType,
    std::initializer_list<LLVMTypeRef> parameters)
{
    std::array<LLVMTypeRef, 16> storage{};
    if (parameters.size() > storage.size())
    {
        std::fprintf(stderr,
            "nsc: internal error: functionType() recibio %zu parametros y el tope es %zu. "
            "Sube el tamano de `storage`.\n",
            parameters.size(), storage.size());
        std::abort();
    }

    unsigned index = 0;
    for (LLVMTypeRef parameter : parameters)
    {
        storage[index] = parameter;
        ++index;
    }
    return LLVMFunctionType(returnType, storage.data(), index, 0);
}

bool buildAllocate(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getProcessHeap,
    LLVMTypeRef getProcessHeapType,
    LLVMValueRef heapAlloc,
    LLVMTypeRef heapAllocType)
{
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef type = functionType(pointerType, {i64Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_alloc", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef heap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "heap");
    LLVMValueRef arguments[3] = {
        heap,
        LLVMConstInt(i32Type, 0, 0),
        LLVMGetParam(function, 0)};
    LLVMValueRef memory = LLVMBuildCall2(
        builder,
        heapAllocType,
        heapAlloc,
        arguments,
        3,
        "memory");
    LLVMBuildRet(builder, memory);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildFree(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getProcessHeap,
    LLVMTypeRef getProcessHeapType,
    LLVMValueRef heapFree,
    LLVMTypeRef heapFreeType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef type = functionType(voidType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_free", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef release = LLVMAppendBasicBlockInContext(context, function, "release");
    LLVMBasicBlockRef done = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pointer = LLVMGetParam(function, 0);
    LLVMValueRef isNull = LLVMBuildICmp(
        builder,
        LLVMIntEQ,
        pointer,
        LLVMConstNull(pointerType),
        "is_null");
    LLVMBuildCondBr(builder, isNull, done, release);

    LLVMPositionBuilderAtEnd(builder, release);
    LLVMValueRef heap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "heap");
    LLVMValueRef arguments[3] = {
        heap,
        LLVMConstInt(i32Type, 0, 0),
        pointer};
    LLVMBuildCall2(builder, heapFreeType, heapFree, arguments, 3, "released");
    LLVMBuildBr(builder, done);

    LLVMPositionBuilderAtEnd(builder, done);
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildTrap(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef exitProcess,
    LLVMTypeRef exitProcessType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef type = functionType(voidType, {i32Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_trap", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef exitCode = LLVMBuildAdd(
        builder,
        LLVMGetParam(function, 0),
        LLVMConstInt(i32Type, 128, 0),
        "exit_code");
    LLVMValueRef arguments[1] = {exitCode};
    LLVMBuildCall2(builder, exitProcessType, exitProcess, arguments, 1, "");
    LLVMBuildUnreachable(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildStart(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef exitProcess,
    LLVMTypeRef exitProcessType,
    LLVMValueRef ehInit,
    LLVMTypeRef ehInitType,
    std::string_view mainSymbol)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef mainType = functionType(i32Type, {});
    LLVMValueRef mainFunction = LLVMAddFunction(module, std::string(mainSymbol).c_str(), mainType);
    LLVMTypeRef startType = functionType(voidType, {});
    LLVMValueRef start = LLVMAddFunction(module, "__nativesharp_start", startType);
    if (mainFunction == nullptr || start == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, start, "entry");
    LLVMBasicBlockRef runMain = LLVMAppendBasicBlockInContext(context, start, "run_main");
    LLVMBasicBlockRef initFailed = LLVMAppendBasicBlockInContext(context, start, "eh_init_failed");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef initialized = LLVMBuildCall2(builder, ehInitType, ehInit, nullptr, 0, "eh_initialized");
    LLVMValueRef initOk = LLVMBuildICmp(
        builder,
        LLVMIntNE,
        initialized,
        LLVMConstInt(i32Type, 0, 0),
        "eh_init_ok");
    LLVMBuildCondBr(builder, initOk, runMain, initFailed);

    LLVMPositionBuilderAtEnd(builder, runMain);
    LLVMValueRef result = LLVMBuildCall2(builder, mainType, mainFunction, nullptr, 0, "result");
    LLVMValueRef arguments[1] = {result};
    LLVMBuildCall2(builder, exitProcessType, exitProcess, arguments, 1, "");
    LLVMBuildUnreachable(builder);

    LLVMPositionBuilderAtEnd(builder, initFailed);
    LLVMValueRef failureArguments[1] = {LLVMConstInt(i32Type, 253, 0)};
    LLVMBuildCall2(builder, exitProcessType, exitProcess, failureArguments, 1, "");
    LLVMBuildUnreachable(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// SHR-022 -- LA UNICA SALIDA DE CONSOLA, parametrizada.
//
// Antes era `__nativesharp_console_writeline(ptr)`: salida estandar siempre, salto de linea
// siempre. Console tenia UN metodo y un compilador sin stderr no puede emitir diagnosticos.
//
// Ahora toma una mascara de dos bits y cubre las cuatro combinaciones:
//
//     bit 0 = anadir "\r\n" al final
//     bit 1 = escribir por ERROR estandar (-12) en vez de por salida estandar (-11)
//
// POR QUE UNA FUNCION Y NO CUATRO. Las cuatro hacen exactamente lo mismo salvo el numero que le
// pasan a GetStdHandle y si escriben dos bytes mas. Cuatro copias de la misma llamada a WriteFile
// es la duplicacion que este arbol lleva una decada de commits quitando; y ademas la mascara la
// fija el NOMBRE DEL METODO en compilacion, asi que llega aqui como constante y el optimizador
// pliega las dos comparaciones sin dejar rama.
//
// EL HANDLE SE PIDE UNA SOLA VEZ. La version anterior llamaba a GetStdHandle dos veces --una para
// el texto y otra para el salto de linea-- y eran dos llamadas al sistema por cada WriteLine.
bool buildConsoleWrite(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getStdHandle,
    LLVMTypeRef getStdHandleType,
    LLVMValueRef writeFile,
    LLVMTypeRef writeFileType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType, i32Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_console_write", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveString = LLVMAppendBasicBlockInContext(context, function, "have_string");
    LLVMBasicBlockRef newlineCheck = LLVMAppendBasicBlockInContext(context, function, "newline_check");
    LLVMBasicBlockRef writeNewline = LLVMAppendBasicBlockInContext(context, function, "write_newline");
    LLVMBasicBlockRef done = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef text = LLVMGetParam(function, 0);
    LLVMValueRef mode = LLVMGetParam(function, 1);

    // STD_OUTPUT_HANDLE = -11 (0xfffffff5), STD_ERROR_HANDLE = -12 (0xfffffff4).
    LLVMValueRef errorBit = LLVMBuildAnd(builder, mode, LLVMConstInt(i32Type, 2, 0), "error_bit");
    LLVMValueRef useError = LLVMBuildICmp(
        builder, LLVMIntNE, errorBit, LLVMConstInt(i32Type, 0, 0), "use_stderr");
    LLVMValueRef handleCode = LLVMBuildSelect(
        builder,
        useError,
        LLVMConstInt(i32Type, 0xfffffff4ULL, 0),
        LLVMConstInt(i32Type, 0xfffffff5ULL, 0),
        "std_handle_code");
    LLVMValueRef handleArguments[] = {handleCode};
    LLVMValueRef handle = LLVMBuildCall2(
        builder, getStdHandleType, getStdHandle, handleArguments, 1, "std_handle");

    // Un `string` nulo no es un error: escribe la linea vacia, igual que antes.
    LLVMValueRef isNull = LLVMBuildICmp(
        builder, LLVMIntEQ, text, LLVMConstNull(pointerType), "text_is_null");
    LLVMBuildCondBr(builder, isNull, newlineCheck, haveString);

    // El string nativo es longitud i64 en el offset 0 y los bytes justo detras, en el 8. WriteFile
    // toma un contador de bytes explicito, asi que no hace falta terminador nulo ni copia.
    LLVMPositionBuilderAtEnd(builder, haveString);
    LLVMValueRef length64 = LLVMBuildLoad2(builder, i64Type, text, "length64");
    LLVMValueRef length32 = LLVMBuildTrunc(builder, length64, i32Type, "length32");
    LLVMValueRef dataOffset = LLVMConstInt(i64Type, 8, 0);
    LLVMValueRef dataIndices[] = {dataOffset};
    LLVMValueRef data = LLVMBuildGEP2(builder, i8Type, text, dataIndices, 1, "string_data");
    LLVMValueRef writtenSlot = LLVMBuildAlloca(builder, i32Type, "written_slot");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0, 0), writtenSlot);
    LLVMValueRef writeArguments[5] = {
        handle,
        data,
        length32,
        writtenSlot,
        LLVMConstNull(pointerType)};
    LLVMBuildCall2(builder, writeFileType, writeFile, writeArguments, 5, "");
    LLVMBuildBr(builder, newlineCheck);

    LLVMPositionBuilderAtEnd(builder, newlineCheck);
    LLVMValueRef newlineBit = LLVMBuildAnd(builder, mode, LLVMConstInt(i32Type, 1, 0), "newline_bit");
    LLVMValueRef wantNewline = LLVMBuildICmp(
        builder, LLVMIntNE, newlineBit, LLVMConstInt(i32Type, 0, 0), "want_newline");
    LLVMBuildCondBr(builder, wantNewline, writeNewline, done);

    LLVMPositionBuilderAtEnd(builder, writeNewline);
    LLVMValueRef newline = LLVMBuildGlobalStringPtr(builder, "\r\n", "core_newline");
    LLVMValueRef newlineWrittenSlot = LLVMBuildAlloca(builder, i32Type, "newline_written_slot");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0, 0), newlineWrittenSlot);
    LLVMValueRef newlineArguments[5] = {
        handle,
        newline,
        LLVMConstInt(i32Type, 2, 0),
        newlineWrittenSlot,
        LLVMConstNull(pointerType)};
    LLVMBuildCall2(builder, writeFileType, writeFile, newlineArguments, 5, "");
    LLVMBuildBr(builder, done);

    LLVMPositionBuilderAtEnd(builder, done);
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 53.1 "Time" -- three tiny wrappers, all sharing one shape: allocate 8 bytes on the
// stack, hand Win32 a pointer to them ("an out-parameter"), then load back a plain i64 and
// return it. This is the SAME "out-parameter via alloca" idiom QueryPerformanceCounter,
// QueryPerformanceFrequency and GetSystemTimeAsFileTime all use in the real Win32 API (none of
// the three returns its result directly in RAX -- each one WRITES it through a pointer the
// caller supplies). A single local helper avoids writing the same six lines three times.
LLVMValueRef buildOutParamI64Call(
    LLVMContextRef context,
    LLVMBuilderRef builder,
    LLVMValueRef win32Function,
    LLVMTypeRef win32FunctionType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMValueRef slot = LLVMBuildAlloca(builder, i64Type, "out_param_slot");
    LLVMValueRef arguments[1] = {slot};
    // The three Win32 functions this backs differ in whether they return a BOOL success flag
    // (QueryPerformanceCounter/QueryPerformanceFrequency) or nothing at all
    // (GetSystemTimeAsFileTime) -- LLVMBuildCall2 does not require using a call's return value,
    // so the same call expression is valid either way; the possible BOOL is simply discarded.
    // This is deliberate, not an oversight: Microsoft's own documentation states
    // QueryPerformanceCounter/QueryPerformanceFrequency "will always succeed and will thus never
    // return zero" on Windows XP or later, which is the only Windows this compiler targets (see
    // the matching comment on NativeClock.ns's QueryCounterRaw/QueryFrequencyRaw). Checking a
    // failure branch that is documented to be unreachable on every supported target would add
    // dead code without adding real safety.
    LLVMBuildCall2(builder, win32FunctionType, win32Function, arguments, 1, "");
    return LLVMBuildLoad2(builder, i64Type, slot, "out_param_value");
}

bool buildClockQueryCounter(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef queryPerformanceCounter,
    LLVMTypeRef queryPerformanceCounterType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_clock_query_counter", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef result = buildOutParamI64Call(
        context, builder, queryPerformanceCounter, queryPerformanceCounterType);
    LLVMBuildRet(builder, result);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildClockQueryFrequency(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef queryPerformanceFrequency,
    LLVMTypeRef queryPerformanceFrequencyType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_clock_query_frequency", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef result = buildOutParamI64Call(
        context, builder, queryPerformanceFrequency, queryPerformanceFrequencyType);
    LLVMBuildRet(builder, result);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildClockSystemFileTime(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getSystemTimeAsFileTime,
    LLVMTypeRef getSystemTimeAsFileTimeType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_clock_query_system_filetime", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    // GetSystemTimeAsFileTime fills a FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; } --
    // two consecutive 32-bit words, low word first, with no padding between them (8 bytes
    // total). Reading those same 8 bytes back as one little-endian i64 (which is exactly what
    // x86-64 is) gives precisely dwLowDateTime in the low 32 bits and dwHighDateTime in the high
    // 32 bits -- the correct combined 64-bit FILETIME value, with no bit-shifting/OR-ing code
    // needed: buildOutParamI64Call's plain "alloca i64, call, load i64" already does the right
    // thing by construction.
    LLVMValueRef result = buildOutParamI64Call(
        context, builder, getSystemTimeAsFileTime, getSystemTimeAsFileTimeType);
    LLVMBuildRet(builder, result);
    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 53.2 "Environment" -- three tiny wrappers, none sharing buildOutParamI64Call's shape
// this time: GetTickCount64 and GetActiveProcessorCount both return their result directly (no
// out-parameter indirection, unlike the Stage 53.1 trio), and Exit never returns at all.
bool buildEnvironmentTickCount64(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getTickCount64,
    LLVMTypeRef getTickCount64Type)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_environment_tick_count64", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    // GetTickCount64 returns its ULONGLONG result directly in RAX -- a plain call+return, no
    // alloca/out-parameter needed here.
    LLVMValueRef result = LLVMBuildCall2(
        builder, getTickCount64Type, getTickCount64, nullptr, 0, "tick_count64");
    LLVMBuildRet(builder, result);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildEnvironmentProcessorCount(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getActiveProcessorCount,
    LLVMTypeRef getActiveProcessorCountType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i16Type = LLVMInt16TypeInContext(context);
    LLVMTypeRef type = functionType(i32Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_environment_processor_count", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    // GetActiveProcessorCount(WORD GroupNumber) -- passing the constant ALL_PROCESSOR_GROUPS
    // (0xFFFF) here, hardcoded, rather than exposing a "which processor group" parameter to
    // NativeSharp: Windows processor groups only matter above 64 logical processors on one
    // machine, which is out of scope for Stage 53.2 (it just answers "how many processors does
    // this machine have, in total").
    LLVMValueRef allProcessorGroups = LLVMConstInt(i16Type, 0xFFFF, 0);
    LLVMValueRef arguments[1] = {allProcessorGroups};
    LLVMValueRef result = LLVMBuildCall2(
        builder, getActiveProcessorCountType, getActiveProcessorCount, arguments, 1, "processor_count");
    LLVMBuildRet(builder, result);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildEnvironmentExit(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef exitProcess,
    LLVMTypeRef exitProcessType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef type = functionType(voidType, {i32Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_environment_exit", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    // Exactly buildTrap's shape (above in this same file) minus the "+128" abnormal-exit-code
    // offset: this is a NORMAL, user-requested exit, so the caller's exit code is passed to
    // ExitProcess completely unmodified. ExitProcess never returns control to the caller, so
    // the block ends in an LLVM "unreachable" terminator rather than a ret -- there is no
    // meaningful value to return past a call that terminates the whole process.
    LLVMValueRef arguments[1] = {LLVMGetParam(function, 0)};
    LLVMBuildCall2(builder, exitProcessType, exitProcess, arguments, 1, "");
    LLVMBuildUnreachable(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 53.2b "Environment (variables)" -- GetEnvironmentVariableA/SetEnvironmentVariableA
// (the ANSI, byte-oriented Win32 variants -- NativeSharp's own `char` is a single byte, see
// integralBitWidth() in ProjectMethodBodyModel.cpp, so ANSI is the honest match; there is no
// UTF-16 anywhere in this compiler yet) both take null-terminated C strings, but a
// NativeSharp.Core.String is NEVER null-terminated -- its layout is exactly
// { i64 length; i8 data[length] }, nothing more, nothing padded on the end. Every wrapper below
// needs a scratch null-terminated copy of a NativeSharp string's bytes before it can call
// either Win32 function, so that copy is factored into this one shared routine instead of
// being written out twice.
//
// This builds the copy at the CURRENT builder position and leaves the builder positioned right
// after the copy completes -- the caller keeps emitting from there, exactly like calling an
// inlined helper function would read at the C++ level, except this one emits LLVM IR blocks
// instead of running immediately. The loop itself follows the exact same
// alloca-index-slot/load/compare/branch shape buildPuts (below) already uses to find a C
// string's length -- same idiom, just copying bytes forward instead of just counting them.
LLVMValueRef buildNullTerminatedCopy(
    LLVMContextRef context,
    LLVMBuilderRef builder,
    LLVMValueRef function,
    LLVMValueRef sourceString)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);

    LLVMBasicBlockRef loopBlock = LLVMAppendBasicBlockInContext(context, function, "copy_loop");
    LLVMBasicBlockRef bodyBlock = LLVMAppendBasicBlockInContext(context, function, "copy_body");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "copy_done");

    // sourceString's length lives in the first 8 bytes (no GEP needed, same as buildPuts/
    // buildConsoleWrite reading a string's length below); its bytes start right after that,
    // at a flat +8 byte offset (dataOffset for a 1-byte-aligned element -- see
    // 53_1_nativesharp_time.md section on NativeSharp's array/string physical layout).
    LLVMValueRef length = LLVMBuildLoad2(builder, i64Type, sourceString, "source_length");
    LLVMValueRef sourceDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef sourceData = LLVMBuildGEP2(builder, i8Type, sourceString, sourceDataIndex, 1, "source_data");
    // A dynamic (non-constant-size) stack allocation -- LLVMBuildArrayAlloca accepts a runtime
    // SSA value for the element count, unlike a plain LLVMBuildAlloca. The scratch buffer only
    // needs to live for the rest of THIS runtime function call (it is read once, by the Win32
    // call the caller makes right after this returns), so the stack is the right place for it:
    // no heap allocation, no need to free it.
    LLVMValueRef bufferSize = LLVMBuildAdd(builder, length, LLVMConstInt(i64Type, 1, 0), "buffer_size");
    LLVMValueRef buffer = LLVMBuildArrayAlloca(builder, i8Type, bufferSize, "ansi_buffer");
    LLVMValueRef indexSlot = LLVMBuildAlloca(builder, i64Type, "copy_index_slot");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), indexSlot);
    LLVMBuildBr(builder, loopBlock);

    LLVMPositionBuilderAtEnd(builder, loopBlock);
    LLVMValueRef index = LLVMBuildLoad2(builder, i64Type, indexSlot, "copy_index");
    LLVMValueRef doneCopying = LLVMBuildICmp(builder, LLVMIntUGE, index, length, "copy_done_cond");
    LLVMBuildCondBr(builder, doneCopying, doneBlock, bodyBlock);

    LLVMPositionBuilderAtEnd(builder, bodyBlock);
    LLVMValueRef srcIndex[1] = {index};
    LLVMValueRef srcPtr = LLVMBuildGEP2(builder, i8Type, sourceData, srcIndex, 1, "copy_src_ptr");
    LLVMValueRef character = LLVMBuildLoad2(builder, i8Type, srcPtr, "copy_char");
    LLVMValueRef dstIndex[1] = {index};
    LLVMValueRef dstPtr = LLVMBuildGEP2(builder, i8Type, buffer, dstIndex, 1, "copy_dst_ptr");
    LLVMBuildStore(builder, character, dstPtr);
    LLVMValueRef nextIndex = LLVMBuildAdd(builder, index, LLVMConstInt(i64Type, 1, 0), "copy_next_index");
    LLVMBuildStore(builder, nextIndex, indexSlot);
    LLVMBuildBr(builder, loopBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMValueRef terminatorIndex[1] = {length};
    LLVMValueRef terminatorPtr = LLVMBuildGEP2(builder, i8Type, buffer, terminatorIndex, 1, "copy_terminator_ptr");
    LLVMBuildStore(builder, LLVMConstInt(i8Type, 0, 0), terminatorPtr);
    // Builder is left positioned at doneBlock -- every byte is copied and the terminator is
    // written, so the caller can start using `buffer` as a plain LPCSTR immediately.
    return buffer;
}

// NativeSharp strings are UTF-8. Windows filesystem APIs use UTF-16, so path-bearing runtime
// hooks must cross that boundary explicitly instead of sending UTF-8 bytes to an ANSI (*A) API.
// The returned scratch buffer lives in the caller's stack frame and is always null terminated.
LLVMValueRef buildUtf16NullTerminatedCopy(
    LLVMContextRef context,
    LLVMBuilderRef builder,
    LLVMValueRef sourceString,
    LLVMValueRef multiByteToWideChar,
    LLVMTypeRef multiByteToWideCharType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i16Type = LLVMInt16TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);

    LLVMValueRef length64 = LLVMBuildLoad2(builder, i64Type, sourceString, "utf8_length");
    LLVMValueRef length32 = LLVMBuildTrunc(builder, length64, i32Type, "utf8_length32");
    LLVMValueRef dataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef data = LLVMBuildGEP2(builder, i8Type, sourceString, dataIndex, 1, "utf8_data");
    LLVMValueRef sizeArguments[6] = {
        LLVMConstInt(i32Type, 65001, 0), // CP_UTF8
        LLVMConstInt(i32Type, 0x8, 0), // MB_ERR_INVALID_CHARS
        data,
        length32,
        LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 0, 0)};
    LLVMValueRef required = LLVMBuildCall2(
        builder, multiByteToWideCharType, multiByteToWideChar, sizeArguments, 6, "utf16_required");
    LLVMValueRef required64 = LLVMBuildZExt(builder, required, i64Type, "utf16_required64");
    LLVMValueRef capacity = LLVMBuildAdd(
        builder, required64, LLVMConstInt(i64Type, 1, 0), "utf16_capacity");
    LLVMValueRef buffer = LLVMBuildArrayAlloca(builder, i16Type, capacity, "utf16_buffer");
    LLVMValueRef fillArguments[6] = {
        LLVMConstInt(i32Type, 65001, 0),
        LLVMConstInt(i32Type, 0x8, 0),
        data,
        length32,
        buffer,
        required};
    LLVMBuildCall2(
        builder, multiByteToWideCharType, multiByteToWideChar, fillArguments, 6, "utf16_written");
    LLVMValueRef terminatorIndex[1] = {required64};
    LLVMValueRef terminator = LLVMBuildGEP2(
        builder, i16Type, buffer, terminatorIndex, 1, "utf16_terminator");
    LLVMBuildStore(builder, LLVMConstInt(i16Type, 0, 0), terminator);
    return buffer;
}

bool buildEnvironmentGetVariable(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getProcessHeap,
    LLVMTypeRef getProcessHeapType,
    LLVMValueRef heapAlloc,
    LLVMTypeRef heapAllocType,
    LLVMValueRef getEnvironmentVariableA,
    LLVMTypeRef getEnvironmentVariableAType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(pointerType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_environment_get_variable", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef emptyResult = LLVMAppendBasicBlockInContext(context, function, "empty_result");
    LLVMBasicBlockRef allocateResult = LLVMAppendBasicBlockInContext(context, function, "allocate_result");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef nameString = LLVMGetParam(function, 0);
    LLVMValueRef nameBuffer = buildNullTerminatedCopy(context, builder, function, nameString);

    // First call: lpBuffer=NULL, nSize=0 -- Win32's documented idiom for "tell me how big a
    // buffer I need", used instead of guessing a size and possibly having to retry. The return
    // value is the required size INCLUDING the null terminator Win32 always writes; 0 means the
    // variable does not exist (or, indistinguishably, that it errored -- see the comment on
    // emptyResult below for why Stage 53.2b treats both the same way on purpose).
    LLVMValueRef sizeArguments[3] = {nameBuffer, LLVMConstNull(pointerType), LLVMConstInt(i32Type, 0, 0)};
    LLVMValueRef requiredSize = LLVMBuildCall2(
        builder, getEnvironmentVariableAType, getEnvironmentVariableA, sizeArguments, 3, "required_size");
    LLVMValueRef notFound = LLVMBuildICmp(
        builder, LLVMIntEQ, requiredSize, LLVMConstInt(i32Type, 0, 0), "not_found");
    LLVMBuildCondBr(builder, notFound, emptyResult, allocateResult);

    // Stage 53.2b deliberately does NOT distinguish "variable does not exist" from "Win32
    // returned an error" -- both collapse to an empty NativeSharp string. A future round could
    // add a bool-returning TryGetEnvironmentVariable if that distinction turns out to matter for
    // a real caller; for this round, "" is the same deliberate simplification already applied
    // to null-vs-missing elsewhere in this project (see Environment.ns's header comment).
    LLVMPositionBuilderAtEnd(builder, emptyResult);
    LLVMValueRef emptyHeap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "empty_heap");
    LLVMValueRef emptyAllocArguments[3] = {
        emptyHeap, LLVMConstInt(i32Type, 0, 0), LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef emptyString = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, emptyAllocArguments, 3, "empty_string");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), emptyString);
    LLVMBuildRet(builder, emptyString);

    LLVMPositionBuilderAtEnd(builder, allocateResult);
    // Allocate exactly 8 (the { i64 length; ... } header) + requiredSize bytes: requiredSize
    // already includes room for Win32's null terminator, which becomes one harmless trailing
    // byte beyond what the NativeSharp `length` field below claims -- nothing in this compiler
    // ever reads past `length`, so that extra byte is simply unused padding, not a bug.
    LLVMValueRef requiredSize64 = LLVMBuildZExt(builder, requiredSize, i64Type, "required_size64");
    LLVMValueRef totalSize = LLVMBuildAdd(builder, requiredSize64, LLVMConstInt(i64Type, 8, 0), "total_size");
    LLVMValueRef heap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "heap");
    LLVMValueRef allocArguments[3] = {heap, LLVMConstInt(i32Type, 0, 0), totalSize};
    LLVMValueRef resultString = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, allocArguments, 3, "result_string");
    LLVMValueRef charCount = LLVMBuildSub(builder, requiredSize64, LLVMConstInt(i64Type, 1, 0), "char_count");
    LLVMBuildStore(builder, charCount, resultString);
    LLVMValueRef resultDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef resultData = LLVMBuildGEP2(builder, i8Type, resultString, resultDataIndex, 1, "result_data");
    // Second call: this time with a real buffer sized exactly to what the first call reported.
    // Between the two calls another process (or, once Stage 54 threading exists, another
    // thread) could in principle change the same environment variable -- Stage 53.2b does not
    // guard against that race, matching this compiler's current single-threaded scope; it is
    // the same category of "not applicable yet" as every other race-condition concern noted as
    // out of scope until Stage 54 exists.
    LLVMValueRef fillArguments[3] = {nameBuffer, resultData, requiredSize};
    LLVMBuildCall2(
        builder, getEnvironmentVariableAType, getEnvironmentVariableA, fillArguments, 3, "fill_result");
    LLVMBuildRet(builder, resultString);

    LLVMDisposeBuilder(builder);
    return true;
}

bool buildEnvironmentSetVariable(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef setEnvironmentVariableA,
    LLVMTypeRef setEnvironmentVariableAType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType, pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_environment_set_variable", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef nameString = LLVMGetParam(function, 0);
    LLVMValueRef valueString = LLVMGetParam(function, 1);
    // Two independent null-terminated copies -- one per NativeSharp string argument. Calling
    // the shared helper twice inside the same function is fine: LLVM auto-uniquifies the block
    // names ("copy_loop" then "copy_loop.1", etc.), there is no collision.
    LLVMValueRef nameBuffer = buildNullTerminatedCopy(context, builder, function, nameString);
    LLVMValueRef valueBuffer = buildNullTerminatedCopy(context, builder, function, valueString);

    LLVMValueRef arguments[2] = {nameBuffer, valueBuffer};
    LLVMBuildCall2(builder, setEnvironmentVariableAType, setEnvironmentVariableA, arguments, 2, "");
    // SetEnvironmentVariableA's BOOL return is discarded -- same documented rationale as
    // buildOutParamI64Call in Stage 53.1: the only realistic failure mode here (the process
    // environment block running out of memory) is not something Stage 53.2b's scope surfaces as
    // a NativeSharp-visible error. A future round could add a bool-returning
    // TrySetEnvironmentVariable if that turns out to matter for a real caller.
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 53.3 "Filesystem/Paths" -- GetFileAttributesA/DeleteFileA/CreateDirectoryA, all ANSI
// (see the comment on buildNullTerminatedCopy above for why). Every one of the four functions
// below starts by turning its one NativeSharp string argument (a path) into a null-terminated
// ANSI copy via that same shared helper -- exactly the pattern Stage 53.2b established for
// Environment's two wrappers, just with one string argument instead of one or two.
//
// File.Exists/Directory.Exists both go through GetFileAttributesA, a single call that returns
// one DWORD of bit flags -- no struct marshaling, no second call, much simpler than
// GetEnvironmentVariableA's two-call "ask the size, then fill it" dance. Two Win32 constants
// decide the answer:
//   INVALID_FILE_ATTRIBUTES (0xFFFFFFFF) -- the path does not exist (or GetFileAttributesA
//     failed for some other reason; Stage 53.3 does not distinguish those two cases, the same
//     deliberate simplification Stage 53.2b already applied to a missing environment variable).
//   FILE_ATTRIBUTE_DIRECTORY (0x10) -- set when the path IS a directory. File.Exists must
//     answer false for a path that is a directory (matching real .NET's System.IO.File.Exists);
//     Directory.Exists checks the exact same bit the other way around.
bool buildFileExists(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getFileAttributesW,
    LLVMTypeRef getFileAttributesWType,
    LLVMValueRef multiByteToWideChar,
    LLVMTypeRef multiByteToWideCharType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i1Type = LLVMInt1TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i1Type, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_file_exists", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef pathBuffer = buildUtf16NullTerminatedCopy(
        context, builder, pathString, multiByteToWideChar, multiByteToWideCharType);
    LLVMValueRef arguments[1] = {pathBuffer};
    LLVMValueRef attributes = LLVMBuildCall2(
        builder, getFileAttributesWType, getFileAttributesW, arguments, 1, "attributes");

    LLVMValueRef isInvalid = LLVMBuildICmp(
        builder, LLVMIntEQ, attributes, LLVMConstInt(i32Type, 0xFFFFFFFFu, 0), "is_invalid");
    LLVMValueRef directoryBit = LLVMBuildAnd(
        builder, attributes, LLVMConstInt(i32Type, 0x10, 0), "directory_bit");
    LLVMValueRef isDirectory = LLVMBuildICmp(
        builder, LLVMIntNE, directoryBit, LLVMConstInt(i32Type, 0, 0), "is_directory");
    // exists == NOT(invalid OR is-a-directory) -- a plain file exists only when the path
    // resolved to something real AND that something is not a directory.
    LLVMValueRef notFile = LLVMBuildOr(builder, isInvalid, isDirectory, "not_file");
    LLVMValueRef exists = LLVMBuildNot(builder, notFile, "file_exists");
    LLVMBuildRet(builder, exists);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildDirectoryExists(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getFileAttributesW,
    LLVMTypeRef getFileAttributesWType,
    LLVMValueRef multiByteToWideChar,
    LLVMTypeRef multiByteToWideCharType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i1Type = LLVMInt1TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i1Type, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_directory_exists", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef pathBuffer = buildUtf16NullTerminatedCopy(
        context, builder, pathString, multiByteToWideChar, multiByteToWideCharType);
    LLVMValueRef arguments[1] = {pathBuffer};
    LLVMValueRef attributes = LLVMBuildCall2(
        builder, getFileAttributesWType, getFileAttributesW, arguments, 1, "attributes");

    LLVMValueRef isInvalid = LLVMBuildICmp(
        builder, LLVMIntEQ, attributes, LLVMConstInt(i32Type, 0xFFFFFFFFu, 0), "is_invalid");
    LLVMValueRef directoryBit = LLVMBuildAnd(
        builder, attributes, LLVMConstInt(i32Type, 0x10, 0), "directory_bit");
    LLVMValueRef isDirectory = LLVMBuildICmp(
        builder, LLVMIntNE, directoryBit, LLVMConstInt(i32Type, 0, 0), "is_directory");
    // exists == (NOT invalid) AND is-a-directory -- the mirror image of buildFileExists above.
    LLVMValueRef notInvalid = LLVMBuildNot(builder, isInvalid, "not_invalid");
    LLVMValueRef exists = LLVMBuildAnd(builder, notInvalid, isDirectory, "directory_exists");
    LLVMBuildRet(builder, exists);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildFileDelete(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef deleteFileA,
    LLVMTypeRef deleteFileAType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_file_delete", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef pathBuffer = buildNullTerminatedCopy(context, builder, function, pathString);
    LLVMValueRef arguments[1] = {pathBuffer};
    LLVMBuildCall2(builder, deleteFileAType, deleteFileA, arguments, 1, "");
    // DeleteFileA's BOOL return is discarded -- same rationale as SetEnvironmentVariableA above:
    // deleting a path that does not exist, or one this process lacks permission for, is not
    // something Stage 53.3's scope surfaces as a NativeSharp-visible error yet. A future round
    // could add a bool-returning TryDelete if that turns out to matter for a real caller.
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildDirectoryCreate(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createDirectoryA,
    LLVMTypeRef createDirectoryAType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_directory_create", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef pathBuffer = buildNullTerminatedCopy(context, builder, function, pathString);
    // CreateDirectoryA(lpPathName, lpSecurityAttributes) -- NULL for the second argument means
    // "use default security attributes", the same thing every other allocation in this runtime
    // already does implicitly (HeapAlloc's process heap has no per-object security choice to
    // make in the first place).
    LLVMValueRef arguments[2] = {pathBuffer, LLVMConstNull(pointerType)};
    LLVMBuildCall2(builder, createDirectoryAType, createDirectoryA, arguments, 2, "");
    // BOOL return discarded -- same rationale as buildFileDelete above (the parent directory not
    // existing, or the directory already existing, are not surfaced as NativeSharp-visible
    // errors in this round; a future TryCreateDirectory could add that).
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 53.4 "Filesystem/Paths (cierre)" -- RemoveDirectoryA. Calco letra por letra de
// buildFileDelete arriba (mismo shape: un string, void, BOOL descartado), solo cambia la
// funcion Win32 real que se llama. Ver el comentario de cabecera de buildFileDelete para la
// justificacion completa de por que se descarta el BOOL de retorno.
bool buildDirectoryDelete(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef removeDirectoryA,
    LLVMTypeRef removeDirectoryAType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_directory_delete", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef pathBuffer = buildNullTerminatedCopy(context, builder, function, pathString);
    LLVMValueRef arguments[1] = {pathBuffer};
    LLVMBuildCall2(builder, removeDirectoryAType, removeDirectoryA, arguments, 1, "");
    // BOOL return discarded -- same rationale as buildFileDelete/buildDirectoryCreate above.
    // Nota real de Win32 (no una simplificacion de este compilador): RemoveDirectoryA solo borra
    // directorios VACIOS -- un directorio con contenido simplemente no se borra, sin que haya
    // forma de distinguir ese caso de "no existia" bajo esta simplificacion.
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 53.3b "Filesystem/Paths (contenido de archivo)" -- the piece Stage 53.3's first round
// deliberately deferred (see File.ns): CreateFileA + GetFileSizeEx + ReadFile/WriteFile +
// CloseHandle, a genuinely longer Win32 sequence than the single-call operations Stage 53 has
// used so far (all of them still ANSI, same reason as ever -- see buildNullTerminatedCopy).
//
// A detail worth calling out explicitly: unlike the PATH argument (which every one of these
// three functions still turns into a null-terminated copy via buildNullTerminatedCopy, because
// CreateFileA needs an LPCSTR), the string CONTENT that WriteAllText/AppendAllText write does
// NOT need that treatment. WriteFile takes an explicit byte buffer plus an explicit byte count
// (no null terminator anywhere in its contract) -- so a NativeSharp string's own data pointer
// (its bytes start at a flat +8 byte offset, see buildNullTerminatedCopy's comment) can be
// handed to WriteFile completely as-is. Only path arguments need the null-terminated detour;
// content arguments never do.
//
// All three check for INVALID_HANDLE_VALUE the same way: CreateFileA returns HANDLE, a pointer
// type in the C API, but INVALID_HANDLE_VALUE is defined as (HANDLE)-1 -- every bit set. That is
// awkward to compare directly as a pointer constant across LLVM versions/opaque-pointer modes, so
// each function converts the returned handle to i64 with LLVMBuildPtrToInt first and compares
// against the all-ones 64-bit constant, which is unambiguous.
bool buildFileReadAllText(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getProcessHeap,
    LLVMTypeRef getProcessHeapType,
    LLVMValueRef heapAlloc,
    LLVMTypeRef heapAllocType,
    LLVMValueRef createFileW,
    LLVMTypeRef createFileWType,
    LLVMValueRef multiByteToWideChar,
    LLVMTypeRef multiByteToWideCharType,
    LLVMValueRef getFileSizeEx,
    LLVMTypeRef getFileSizeExType,
    LLVMValueRef readFile,
    LLVMTypeRef readFileType,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(pointerType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_file_read_all_text", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef emptyResult = LLVMAppendBasicBlockInContext(context, function, "empty_result");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef pathBuffer = buildUtf16NullTerminatedCopy(
        context, builder, pathString, multiByteToWideChar, multiByteToWideCharType);
    // GENERIC_READ=0x80000000, FILE_SHARE_READ=0x1 (other readers may open the same file
    // concurrently -- there is no writer in this picture yet), lpSecurityAttributes=NULL,
    // OPEN_EXISTING=3 (fail if the file does not exist -- ReadAllText should never CREATE a
    // file), FILE_ATTRIBUTE_NORMAL=0x80, hTemplateFile=NULL.
    LLVMValueRef createArguments[7] = {
        pathBuffer,
        LLVMConstInt(i32Type, 0x80000000u, 0),
        LLVMConstInt(i32Type, 0x1, 0),
        LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 3, 0),
        LLVMConstInt(i32Type, 0x80, 0),
        LLVMConstNull(pointerType)};
    LLVMValueRef handle = LLVMBuildCall2(builder, createFileWType, createFileW, createArguments, 7, "handle");
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMValueRef invalidHandle = LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0);
    LLVMValueRef isInvalid = LLVMBuildICmp(builder, LLVMIntEQ, handleAsInt, invalidHandle, "is_invalid");
    LLVMBuildCondBr(builder, isInvalid, emptyResult, haveHandle);

    // File does not exist (or CreateFileA failed for any other reason) -- Stage 53.3b does not
    // distinguish those cases, the same deliberate simplification already applied to a missing
    // environment variable (53.2b) and to File.Exists/Directory.Exists (53.3): collapse every
    // failure into an empty NativeSharp string, rather than throwing an exception (this compiler
    // has exceptions since Stage 45, but wiring Win32 error codes into NativeSharp exceptions is
    // its own piece of design work, deliberately out of scope here).
    LLVMPositionBuilderAtEnd(builder, emptyResult);
    LLVMValueRef emptyHeap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "empty_heap");
    LLVMValueRef emptyAllocArguments[3] = {
        emptyHeap, LLVMConstInt(i32Type, 0, 0), LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef emptyString = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, emptyAllocArguments, 3, "empty_string");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), emptyString);
    LLVMBuildRet(builder, emptyString);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    // sizeSlot starts at 0 -- if GetFileSizeEx itself fails (its BOOL return is not checked,
    // same "not surfaced as a NativeSharp-visible error yet" rationale as every other discarded
    // BOOL in this file), the slot keeps its safe default instead of holding uninitialized
    // garbage, so the worst case degrades to "read zero bytes", never undefined behavior.
    LLVMValueRef sizeSlot = LLVMBuildAlloca(builder, i64Type, "size_slot");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), sizeSlot);
    LLVMValueRef sizeArguments[2] = {handle, sizeSlot};
    LLVMBuildCall2(builder, getFileSizeExType, getFileSizeEx, sizeArguments, 2, "");
    LLVMValueRef size = LLVMBuildLoad2(builder, i64Type, sizeSlot, "size");

    LLVMValueRef heap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "heap");
    LLVMValueRef totalSize = LLVMBuildAdd(builder, size, LLVMConstInt(i64Type, 8, 0), "total_size");
    LLVMValueRef allocArguments[3] = {heap, LLVMConstInt(i32Type, 0, 0), totalSize};
    LLVMValueRef resultString = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, allocArguments, 3, "result_string");
    LLVMBuildStore(builder, size, resultString);
    LLVMValueRef resultDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef resultData = LLVMBuildGEP2(builder, i8Type, resultString, resultDataIndex, 1, "result_data");

    // ReadFile's nNumberOfBytesToRead is a 32-bit DWORD, but a NativeSharp string's length is a
    // 64-bit i64 -- this truncation means a file bigger than ~4 GiB would read only its first
    // ~4 GiB. Deliberately out of scope for this round (a multi-chunk read loop would be needed
    // to handle that correctly): the target domain for this compiler (see
    // claude/03_posicionamiento.md) makes a multi-gigabyte TEXT file an unrealistic input for
    // ReadAllText specifically.
    LLVMValueRef sizeI32 = LLVMBuildTrunc(builder, size, i32Type, "size_i32");
    LLVMValueRef bytesReadSlot = LLVMBuildAlloca(builder, i32Type, "bytes_read_slot");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0, 0), bytesReadSlot);
    LLVMValueRef readArguments[5] = {
        handle, resultData, sizeI32, bytesReadSlot, LLVMConstNull(pointerType)};
    LLVMBuildCall2(builder, readFileType, readFile, readArguments, 5, "");
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    LLVMBuildRet(builder, resultString);

    LLVMDisposeBuilder(builder);
    return true;
}

// Bloque 4, fase A -- SHR-030. `File.GetSize(path)`: cuanto ocupa un archivo, sin leerlo.
//
// POR QUE EXISTE Y NO SE DEDUCE DE ReadAllBytes. El driver necesita el tamano ANTES de decidir
// como leer: para reservar el buffer exacto, para saltarse un archivo vacio, o para no intentar
// meter en memoria algo que no cabe. Deducirlo leyendo el archivo entero es justo lo contrario de
// lo que se quiere.
//
// DEVUELVE -1 CUANDO NO SE PUDO. Y esa es la unica diferencia de criterio con el resto de esta
// familia. `File.ReadAllText` colapsa cualquier fallo en la cadena vacia y `File.Exists` en false,
// porque ahi el valor de fallo es indistinguible de un resultado legitimo y da igual. Aqui no:
// **0 es el tamano de un archivo vacio**, que existe y es un caso normal. Devolver 0 para "no se
// pudo abrir" haria imposible distinguir un archivo vacio de uno que no esta, y esa distincion es
// justo la que el driver necesita. -1 no colisiona con ningun tamano posible.
//
// POR ESO SI SE MIRA EL BOOL de GetFileSizeEx, a diferencia de buildFileReadAllText, que lo
// descarta con su motivo escrito. Alli el peor caso al ignorarlo es leer cero bytes; aqui el
// resultado ES el tamano, y devolver un cero silencioso seria devolver un dato falso.
bool buildFileGetSize(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createFileA,
    LLVMTypeRef createFileAType,
    LLVMValueRef getFileSizeEx,
    LLVMTypeRef getFileSizeExType,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i64Type, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_file_get_size", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef failed = LLVMAppendBasicBlockInContext(context, function, "failed");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef sizeFailed = LLVMAppendBasicBlockInContext(context, function, "size_failed");
    LLVMBasicBlockRef sizeOk = LLVMAppendBasicBlockInContext(context, function, "size_ok");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef pathBuffer = buildNullTerminatedCopy(context, builder, function, pathString);
    // Los mismos siete argumentos que buildFileReadAllText, y por los mismos motivos:
    // GENERIC_READ=0x80000000, FILE_SHARE_READ=0x1, sin atributos de seguridad, OPEN_EXISTING=3
    // (preguntar el tamano no puede CREAR el archivo), FILE_ATTRIBUTE_NORMAL=0x80, sin plantilla.
    LLVMValueRef createArguments[7] = {
        pathBuffer,
        LLVMConstInt(i32Type, 0x80000000u, 0),
        LLVMConstInt(i32Type, 0x1, 0),
        LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 3, 0),
        LLVMConstInt(i32Type, 0x80, 0),
        LLVMConstNull(pointerType)};
    LLVMValueRef handle = LLVMBuildCall2(builder, createFileAType, createFileA, createArguments, 7, "handle");
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMValueRef invalidHandle = LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0);
    LLVMValueRef isInvalid = LLVMBuildICmp(builder, LLVMIntEQ, handleAsInt, invalidHandle, "is_invalid");
    LLVMBuildCondBr(builder, isInvalid, failed, haveHandle);

    // No se pudo abrir: no existe, no hay permiso, o esta bloqueado. -1, sin distinguir cual --
    // la misma simplificacion deliberada del resto de la familia (los codigos de error de Win32
    // no estan cableados a excepciones de NativeSharp, y eso es su propio trabajo de diseno).
    LLVMPositionBuilderAtEnd(builder, failed);
    LLVMBuildRet(builder, LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0));

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef sizeSlot = LLVMBuildAlloca(builder, i64Type, "size_slot");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), sizeSlot);
    LLVMValueRef sizeArguments[2] = {handle, sizeSlot};
    LLVMValueRef sizeOkFlag = LLVMBuildCall2(
        builder, getFileSizeExType, getFileSizeEx, sizeArguments, 2, "size_ok_flag");
    // El handle se cierra en LOS DOS caminos, y por eso se cierra aqui, antes de la bifurcacion:
    // un `return` temprano que se deje el handle abierto es una fuga que no da la cara hasta que
    // el proceso lleva horas corriendo.
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    LLVMValueRef sizeFailedFlag = LLVMBuildICmp(
        builder, LLVMIntEQ, sizeOkFlag, LLVMConstInt(i32Type, 0, 0), "size_failed_flag");
    LLVMBuildCondBr(builder, sizeFailedFlag, sizeFailed, sizeOk);

    LLVMPositionBuilderAtEnd(builder, sizeFailed);
    LLVMBuildRet(builder, LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0));

    LLVMPositionBuilderAtEnd(builder, sizeOk);
    LLVMValueRef size = LLVMBuildLoad2(builder, i64Type, sizeSlot, "size");
    LLVMBuildRet(builder, size);

    LLVMDisposeBuilder(builder);
    return true;
}

// Bloque 4, fase A -- SHR-028. `File.ReadAllBytes(path)`.
//
// ES EL MISMO CODIGO QUE buildFileReadAllText, Y NO ES PEREZA. Un `byte[]` de NativeSharp y un
// `string` de NativeSharp tienen el MISMO layout en memoria: { i64 longitud; los datos justo
// detras }. Se comprobo mirando el IR de `new byte[8]`, que pide 16 bytes -- 8 de cabecera y 8 de
// datos. Leer un archivo entero a memoria produce exactamente esa forma, se llame como se llame.
//
// POR QUE ENTONCES DOS HOOKS Y NO UNO. Porque el que los distingue es el FRONTEND, por la clave de
// tipo del valor devuelto (`builtin:byte[]` contra `builtin:string`), y esa distincion tiene que
// sobrevivir hasta el IR para que el codigo de arriba sepa que tiene. Compartir el simbolo
// obligaria a que la bajada eligiera el tipo de salida por su cuenta, que es justo la clase de
// decision que no debe estar ahi. El coste son sesenta lineas de IR duplicadas en el runtime; el
// beneficio es que el tipo lo decide quien lo sabe.
//
// LA DIFERENCIA REAL CON ReadAllText es el truncamiento a 32 bits de ReadFile: alli el comentario
// dice que un fichero de TEXTO de mas de 4 GiB es irreal en este dominio, y es cierto. Aqui un
// .obj de esa talla tampoco es realista, pero el argumento ya no es el mismo, asi que queda dicho
// aparte: si algun dia hace falta, la solucion es un bucle de lectura por trozos, no un cambio de
// tipo.
bool buildFileReadAllBytes(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getProcessHeap,
    LLVMTypeRef getProcessHeapType,
    LLVMValueRef heapAlloc,
    LLVMTypeRef heapAllocType,
    LLVMValueRef createFileA,
    LLVMTypeRef createFileAType,
    LLVMValueRef getFileSizeEx,
    LLVMTypeRef getFileSizeExType,
    LLVMValueRef readFile,
    LLVMTypeRef readFileType,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(pointerType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_file_read_all_bytes", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef emptyResult = LLVMAppendBasicBlockInContext(context, function, "empty_result");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef pathBuffer = buildNullTerminatedCopy(context, builder, function, pathString);
    LLVMValueRef createArguments[7] = {
        pathBuffer,
        LLVMConstInt(i32Type, 0x80000000u, 0),
        LLVMConstInt(i32Type, 0x1, 0),
        LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 3, 0),
        LLVMConstInt(i32Type, 0x80, 0),
        LLVMConstNull(pointerType)};
    LLVMValueRef handle = LLVMBuildCall2(builder, createFileAType, createFileA, createArguments, 7, "handle");
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMValueRef invalidHandle = LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0);
    LLVMValueRef isInvalid = LLVMBuildICmp(builder, LLVMIntEQ, handleAsInt, invalidHandle, "is_invalid");
    LLVMBuildCondBr(builder, isInvalid, emptyResult, haveHandle);

    // No se pudo abrir: un arreglo VACIO, no nulo. Devolver nulo obligaria a cada llamador a
    // comprobarlo antes de leer `.Length`, y el resto de esta familia ya establecio que un fallo
    // se colapsa en el valor vacio. Quien necesite distinguir "vacio" de "no existe" tiene
    // File.Exists y File.GetSize, que para eso devuelve -1.
    LLVMPositionBuilderAtEnd(builder, emptyResult);
    LLVMValueRef emptyHeap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "empty_heap");
    LLVMValueRef emptyAllocArguments[3] = {
        emptyHeap, LLVMConstInt(i32Type, 0, 0), LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef emptyArray = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, emptyAllocArguments, 3, "empty_array");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), emptyArray);
    LLVMBuildRet(builder, emptyArray);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef sizeSlot = LLVMBuildAlloca(builder, i64Type, "size_slot");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), sizeSlot);
    LLVMValueRef sizeArguments[2] = {handle, sizeSlot};
    LLVMBuildCall2(builder, getFileSizeExType, getFileSizeEx, sizeArguments, 2, "");
    LLVMValueRef size = LLVMBuildLoad2(builder, i64Type, sizeSlot, "size");

    LLVMValueRef heap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "heap");
    LLVMValueRef totalSize = LLVMBuildAdd(builder, size, LLVMConstInt(i64Type, 8, 0), "total_size");
    LLVMValueRef allocArguments[3] = {heap, LLVMConstInt(i32Type, 0, 0), totalSize};
    LLVMValueRef resultArray = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, allocArguments, 3, "result_array");
    LLVMBuildStore(builder, size, resultArray);
    LLVMValueRef resultDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef resultData = LLVMBuildGEP2(builder, i8Type, resultArray, resultDataIndex, 1, "result_data");

    // Mismo truncamiento a DWORD que ReadAllText, y con la misma consecuencia: por encima de
    // ~4 GiB se leeria solo el principio. Ver la cabecera de esta funcion.
    LLVMValueRef sizeI32 = LLVMBuildTrunc(builder, size, i32Type, "size_i32");
    LLVMValueRef bytesReadSlot = LLVMBuildAlloca(builder, i32Type, "bytes_read_slot");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0, 0), bytesReadSlot);
    LLVMValueRef readArguments[5] = {
        handle, resultData, sizeI32, bytesReadSlot, LLVMConstNull(pointerType)};
    LLVMBuildCall2(builder, readFileType, readFile, readArguments, 5, "");
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    LLVMBuildRet(builder, resultArray);

    LLVMDisposeBuilder(builder);
    return true;
}

// Bloque 4, fase A -- SHR-028, la variante que NO asigna. `File.ReadBytes(path, destino)`.
//
// POR QUE EXISTE SI YA HAY ReadAllBytes. Porque el driver lee muchos archivos y ReadAllBytes pide
// memoria en cada uno. Con un buffer reutilizado --el patron que Directory.GetFiles establecio--
// se lee un .obj tras otro sin una sola asignacion. Es exactamente lo que el Bloque 3 persigue.
//
// DEVUELVE CUANTOS BYTES SE LEYERON, y lee como mucho lo que quepa: si el archivo es mas grande
// que el destino, se lee el principio y se devuelve la longitud del destino. NO es un error ni se
// trunca en silencio sin decirlo -- el llamador compara lo devuelto con File.GetSize y sabe si se
// quedo corto. La alternativa --fallar-- obligaria a dos llamadas siempre (tamano y lectura) para
// el caso normal en que el buffer sobra.
//
// Devuelve 0 si no se pudo abrir. Aqui 0 no es ambiguo: leer cero bytes de un archivo vacio y no
// poder abrirlo dejan el destino igual, y ninguno de los dos es un caso que el llamador trate
// distinto. Quien necesite la diferencia tiene File.GetSize.
bool buildFileReadBytes(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createFileA,
    LLVMTypeRef createFileAType,
    LLVMValueRef getFileSizeEx,
    LLVMTypeRef getFileSizeExType,
    LLVMValueRef readFile,
    LLVMTypeRef readFileType,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i32Type, {pointerType, pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_file_read_bytes", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef failed = LLVMAppendBasicBlockInContext(context, function, "failed");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef destination = LLVMGetParam(function, 1);
    LLVMValueRef pathBuffer = buildNullTerminatedCopy(context, builder, function, pathString);
    LLVMValueRef createArguments[7] = {
        pathBuffer,
        LLVMConstInt(i32Type, 0x80000000u, 0),
        LLVMConstInt(i32Type, 0x1, 0),
        LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 3, 0),
        LLVMConstInt(i32Type, 0x80, 0),
        LLVMConstNull(pointerType)};
    LLVMValueRef handle = LLVMBuildCall2(builder, createFileAType, createFileA, createArguments, 7, "handle");
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMValueRef invalidHandle = LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0);
    LLVMValueRef isInvalid = LLVMBuildICmp(builder, LLVMIntEQ, handleAsInt, invalidHandle, "is_invalid");
    LLVMBuildCondBr(builder, isInvalid, failed, haveHandle);

    LLVMPositionBuilderAtEnd(builder, failed);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 0, 0));

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    // El tamano del archivo y la capacidad del destino; se lee el menor de los dos.
    LLVMValueRef sizeSlot = LLVMBuildAlloca(builder, i64Type, "size_slot");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), sizeSlot);
    LLVMValueRef sizeArguments[2] = {handle, sizeSlot};
    LLVMBuildCall2(builder, getFileSizeExType, getFileSizeEx, sizeArguments, 2, "");
    LLVMValueRef fileSize = LLVMBuildLoad2(builder, i64Type, sizeSlot, "file_size");
    LLVMValueRef capacity = LLVMBuildLoad2(builder, i64Type, destination, "capacity");
    LLVMValueRef fits = LLVMBuildICmp(builder, LLVMIntULE, fileSize, capacity, "fits");
    LLVMValueRef toRead = LLVMBuildSelect(builder, fits, fileSize, capacity, "to_read");

    LLVMValueRef destinationDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef destinationData = LLVMBuildGEP2(
        builder, i8Type, destination, destinationDataIndex, 1, "destination_data");
    LLVMValueRef toReadI32 = LLVMBuildTrunc(builder, toRead, i32Type, "to_read_i32");
    LLVMValueRef bytesReadSlot = LLVMBuildAlloca(builder, i32Type, "bytes_read_slot");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0, 0), bytesReadSlot);
    LLVMValueRef readArguments[5] = {
        handle, destinationData, toReadI32, bytesReadSlot, LLVMConstNull(pointerType)};
    LLVMBuildCall2(builder, readFileType, readFile, readArguments, 5, "");
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    // Se devuelve lo que ReadFile dice que leyo, no lo que se le pidio: son distintos si el
    // archivo se acorto entre GetFileSizeEx y ReadFile, y el dato honesto es el segundo.
    LLVMValueRef bytesRead = LLVMBuildLoad2(builder, i32Type, bytesReadSlot, "bytes_read");
    LLVMBuildRet(builder, bytesRead);

    LLVMDisposeBuilder(builder);
    return true;
}

// Bloque 4, fase A -- SHR-029. `File.WriteAllBytes(path, datos, cuantos)`.
//
// CREATE_ALWAYS, igual que WriteAllText: el archivo termina conteniendo SOLO lo que se escribe
// ahora. Si ya existia, se trunca a cero antes.
//
// DEVUELVE bool, y aqui SI se mira el BOOL de WriteFile, a diferencia de WriteAllText que lo
// descarta. El motivo es el mismo que en File.GetSize: escribir un .obj a medias y seguir como si
// nada produce un enlace que falla mucho despues, con un error que no menciona la escritura. Un
// disco lleno es un caso real y barato de detectar aqui.
//
// EL CONTADOR ES EXPLICITO y no la longitud del arreglo -- ver MethodCoreOperationKind. Se recorta
// a la capacidad real del arreglo por si el llamador se pasa: escribir mas alla del final seria
// leer memoria ajena y meterla en un archivo.
bool buildFileWriteAllBytes(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createFileA,
    LLVMTypeRef createFileAType,
    LLVMValueRef writeFile,
    LLVMTypeRef writeFileType,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef i1Type = LLVMInt1TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i1Type, {pointerType, pointerType, i32Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_file_write_all_bytes", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef failed = LLVMAppendBasicBlockInContext(context, function, "failed");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef data = LLVMGetParam(function, 1);
    LLVMValueRef requestedCount = LLVMGetParam(function, 2);
    LLVMValueRef pathBuffer = buildNullTerminatedCopy(context, builder, function, pathString);
    // GENERIC_WRITE=0x40000000, sin comparticion, CREATE_ALWAYS=2, FILE_ATTRIBUTE_NORMAL=0x80.
    LLVMValueRef createArguments[7] = {
        pathBuffer,
        LLVMConstInt(i32Type, 0x40000000u, 0),
        LLVMConstInt(i32Type, 0, 0),
        LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 2, 0),
        LLVMConstInt(i32Type, 0x80, 0),
        LLVMConstNull(pointerType)};
    LLVMValueRef handle = LLVMBuildCall2(builder, createFileAType, createFileA, createArguments, 7, "handle");
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMValueRef invalidHandle = LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0);
    LLVMValueRef isInvalid = LLVMBuildICmp(builder, LLVMIntEQ, handleAsInt, invalidHandle, "is_invalid");
    LLVMBuildCondBr(builder, isInvalid, failed, haveHandle);

    LLVMPositionBuilderAtEnd(builder, failed);
    LLVMBuildRet(builder, LLVMConstInt(i1Type, 0, 0));

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    // El contador pedido se recorta a la longitud real del arreglo. Un llamador que pide escribir
    // mas de lo que tiene esta pidiendo que se lea memoria que no es suya.
    LLVMValueRef requested64 = LLVMBuildSExt(builder, requestedCount, i64Type, "requested64");
    LLVMValueRef length = LLVMBuildLoad2(builder, i64Type, data, "length");
    LLVMValueRef negative = LLVMBuildICmp(
        builder, LLVMIntSLT, requested64, LLVMConstInt(i64Type, 0, 0), "negative");
    LLVMValueRef nonNegative = LLVMBuildSelect(
        builder, negative, LLVMConstInt(i64Type, 0, 0), requested64, "non_negative");
    LLVMValueRef fits = LLVMBuildICmp(builder, LLVMIntULE, nonNegative, length, "fits");
    LLVMValueRef toWrite = LLVMBuildSelect(builder, fits, nonNegative, length, "to_write");

    LLVMValueRef dataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef dataStart = LLVMBuildGEP2(builder, i8Type, data, dataIndex, 1, "data_start");
    LLVMValueRef toWriteI32 = LLVMBuildTrunc(builder, toWrite, i32Type, "to_write_i32");
    LLVMValueRef writtenSlot = LLVMBuildAlloca(builder, i32Type, "written_slot");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0, 0), writtenSlot);
    LLVMValueRef writeArguments[5] = {
        handle, dataStart, toWriteI32, writtenSlot, LLVMConstNull(pointerType)};
    LLVMValueRef writeOk = LLVMBuildCall2(
        builder, writeFileType, writeFile, writeArguments, 5, "write_ok");
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    // Verdadero solo si WriteFile dijo que si Y escribio todo lo que se le pidio. Un disco que se
    // llena a mitad devuelve TRUE con menos bytes escritos, y eso es un fallo.
    LLVMValueRef written = LLVMBuildLoad2(builder, i32Type, writtenSlot, "written");
    LLVMValueRef apiOk = LLVMBuildICmp(
        builder, LLVMIntNE, writeOk, LLVMConstInt(i32Type, 0, 0), "api_ok");
    LLVMValueRef complete = LLVMBuildICmp(builder, LLVMIntEQ, written, toWriteI32, "complete");
    LLVMValueRef succeeded = LLVMBuildAnd(builder, apiOk, complete, "succeeded");
    LLVMBuildRet(builder, succeeded);

    LLVMDisposeBuilder(builder);
    return true;
}

// Bloque 4, fase A. `Environment.CurrentDirectory()` -- cierra `current_path`, la ultima de las
// siete operaciones de filesystem que el doc 81 midio como usadas de verdad por el arbol de nsc.
//
// Mismo idioma de dos llamadas que buildPathGetFullPath: la primera con buffer nulo devuelve el
// tamano necesario INCLUYENDO el terminador; la segunda lo llena. La diferencia es que
// GetCurrentDirectoryA tiene dos parametros y no cuatro, y que el orden es (tamano, buffer).
bool buildEnvironmentCurrentDirectory(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getProcessHeap,
    LLVMTypeRef getProcessHeapType,
    LLVMValueRef heapAlloc,
    LLVMTypeRef heapAllocType,
    LLVMValueRef getCurrentDirectoryA,
    LLVMTypeRef getCurrentDirectoryAType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(pointerType, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_environment_current_directory", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef emptyResult = LLVMAppendBasicBlockInContext(context, function, "empty_result");
    LLVMBasicBlockRef allocateResult = LLVMAppendBasicBlockInContext(context, function, "allocate_result");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef sizeArguments[2] = {LLVMConstInt(i32Type, 0, 0), LLVMConstNull(pointerType)};
    LLVMValueRef requiredSize = LLVMBuildCall2(
        builder, getCurrentDirectoryAType, getCurrentDirectoryA, sizeArguments, 2, "required_size");
    LLVMValueRef failed = LLVMBuildICmp(
        builder, LLVMIntEQ, requiredSize, LLVMConstInt(i32Type, 0, 0), "failed");
    LLVMBuildCondBr(builder, failed, emptyResult, allocateResult);

    LLVMPositionBuilderAtEnd(builder, emptyResult);
    LLVMValueRef emptyHeap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "empty_heap");
    LLVMValueRef emptyAllocArguments[3] = {
        emptyHeap, LLVMConstInt(i32Type, 0, 0), LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef emptyString = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, emptyAllocArguments, 3, "empty_string");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), emptyString);
    LLVMBuildRet(builder, emptyString);

    LLVMPositionBuilderAtEnd(builder, allocateResult);
    LLVMValueRef requiredSize64 = LLVMBuildZExt(builder, requiredSize, i64Type, "required_size64");
    LLVMValueRef totalSize = LLVMBuildAdd(builder, requiredSize64, LLVMConstInt(i64Type, 8, 0), "total_size");
    LLVMValueRef heap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "heap");
    LLVMValueRef allocArguments[3] = {heap, LLVMConstInt(i32Type, 0, 0), totalSize};
    LLVMValueRef resultString = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, allocArguments, 3, "result_string");
    LLVMValueRef resultDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef resultData = LLVMBuildGEP2(builder, i8Type, resultString, resultDataIndex, 1, "result_data");
    LLVMValueRef fillArguments[2] = {requiredSize, resultData};
    // La SEGUNDA llamada devuelve los caracteres copiados SIN el terminador, que es exactamente
    // la longitud que quiere una cadena de NativeSharp. Se guarda esa, no `requiredSize - 1`: si
    // el directorio cambiara entre las dos llamadas, el dato honesto es el de la segunda.
    LLVMValueRef copied = LLVMBuildCall2(
        builder, getCurrentDirectoryAType, getCurrentDirectoryA, fillArguments, 2, "copied");
    LLVMValueRef copied64 = LLVMBuildZExt(builder, copied, i64Type, "copied64");
    LLVMBuildStore(builder, copied64, resultString);
    LLVMBuildRet(builder, resultString);

    LLVMDisposeBuilder(builder);
    return true;
}

// Bloque 4, fase C. Pone a cero un bloque de pila, en trozos de 8 bytes.
//
// POR QUE NO memset. Porque este runtime tiene un bug ya documentado con memset y su alias (ver
// BUGS_ENCONTRADOS_bug_memset_se_llama_por_su_alias.md), y porque aqui no hace falta: los tres
// bloques que se limpian son de 24, 104 y 24 bytes, o sea tres, trece y tres almacenamientos.
// Emitir un bucle o una llamada para eso seria mas codigo del que se ahorra.
//
// `bytes` tiene que ser multiplo de 8; las tres estructuras de Win32 que se usan aqui lo son en
// x64 por su propio alineamiento.
void buildZeroFillStack(
    LLVMContextRef context,
    LLVMBuilderRef builder,
    LLVMValueRef base,
    unsigned long long bytes)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    for (unsigned long long offset = 0; offset + 8 <= bytes; offset += 8)
    {
        LLVMValueRef index[1] = {LLVMConstInt(i64Type, offset, 0)};
        LLVMValueRef slot = LLVMBuildGEP2(builder, i8Type, base, index, 1, "zero_slot");
        LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), slot);
    }
}

// Bloque 4, fase C -- SHR-032. `Process.Run(ejecutable, lineaDeComandos, rutaDeSalida)`.
//
// ES EL BLOQUEO DURO DEL AUTO-HOSPEDADO. Sin crear procesos hijos, un `nsc` escrito en NativeSharp
// no puede lanzar el enlazador, y por tanto no puede producir un ejecutable. `Process.ns` existia
// desde Stage 55.1, pero su cabecera decia que crear procesos estaba "deliberadamente fuera de
// alcance"; sus cinco operaciones hablan solo del proceso ACTUAL.
//
// QUE HACE, EN ORDEN:
//   1. abre `rutaDeSalida` para escritura, con el handle marcado como HEREDABLE;
//   2. lanza el proceso con stdout Y stderr apuntando a ese handle;
//   3. espera a que termine, sin limite de tiempo;
//   4. lee su codigo de salida y lo devuelve.
//
// Es exactamente lo que `LldLinker.cpp` hace hoy en C++ para invocar a lld-link, y esa es la
// prueba de que el conjunto es el correcto: no se ha inventado una API, se ha portado la que el
// compilador ya usa.
//
// POR QUE stdout Y stderr AL MISMO FICHERO. Porque un enlazador escribe sus diagnosticos por
// stderr y su ruido por stdout, y quien lee el resultado quiere los dos INTERCALADOS en el orden
// en que ocurrieron. Dos ficheros obligarian a reconstruir ese orden, que no se puede.
//
// POR QUE NO HAY TIEMPO LIMITE. Un enlazador que no termina es un fallo del enlazador, y matarlo a
// los N segundos convertiria ese fallo en "el codigo de salida fue raro" -- que es peor, porque se
// parece a un fallo del programa que se compilaba. WaitForSingleObject con INFINITE deja el
// problema donde esta.
//
// DEVUELVE -1 SI NO SE PUDO LANZAR, y ese valor NO puede confundirse con un codigo de salida: los
// codigos de salida de Windows son DWORD sin signo, y aqui se devuelven como i32, asi que un
// proceso que devolviera 0xFFFFFFFF daria -1 tambien. Es la unica ambiguedad, se acepta a
// sabiendas: ningun enlazador ni compilador de esta familia devuelve ese valor, y la alternativa
// --un out-parameter de exito-- convertiria la firma mas usada del driver en dos argumentos por un
// caso que no ocurre.
//
// UNA RUTA DE SALIDA VACIA NO ES UN CASO ESPECIAL: CreateFileA falla sobre "" y se cae por el
// mismo camino de error, devolviendo -1. No hace falta comprobarlo antes.
bool buildProcessRun(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createFileA,
    LLVMTypeRef createFileAType,
    LLVMValueRef createProcessA,
    LLVMTypeRef createProcessAType,
    LLVMValueRef waitForSingleObject,
    LLVMTypeRef waitForSingleObjectType,
    LLVMValueRef getExitCodeProcess,
    LLVMTypeRef getExitCodeProcessType,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i32Type, {pointerType, pointerType, pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_process_run", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef noOutput = LLVMAppendBasicBlockInContext(context, function, "no_output");
    LLVMBasicBlockRef haveOutput = LLVMAppendBasicBlockInContext(context, function, "have_output");
    LLVMBasicBlockRef spawnFailed = LLVMAppendBasicBlockInContext(context, function, "spawn_failed");
    LLVMBasicBlockRef spawned = LLVMAppendBasicBlockInContext(context, function, "spawned");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef executableString = LLVMGetParam(function, 0);
    LLVMValueRef commandLineString = LLVMGetParam(function, 1);
    LLVMValueRef outputPathString = LLVMGetParam(function, 2);

    LLVMValueRef executableBuffer = buildNullTerminatedCopy(context, builder, function, executableString);
    // La linea de comandos tiene que estar en memoria ESCRIBIBLE: CreateProcessA puede modificarla
    // en sitio. `buildNullTerminatedCopy` da un alloca, asi que ya lo esta -- pero conviene decirlo,
    // porque pasar un literal de solo lectura ahi es un fallo clasico que en Windows se manifiesta
    // como una violacion de acceso dentro de kernel32.
    LLVMValueRef commandLineBuffer = buildNullTerminatedCopy(context, builder, function, commandLineString);
    LLVMValueRef outputPathBuffer = buildNullTerminatedCopy(context, builder, function, outputPathString);

    // SECURITY_ATTRIBUTES { DWORD nLength; LPVOID lpSecurityDescriptor; BOOL bInheritHandle; }
    // 24 bytes en x64. bInheritHandle=TRUE es lo que hace que el handle del fichero sobreviva al
    // CreateProcess: sin eso el hijo recibe un handle invalido y no escribe nada.
    LLVMValueRef securityAttributes = LLVMBuildArrayAlloca(
        builder, i8Type, LLVMConstInt(i64Type, 24, 0), "security_attributes");
    buildZeroFillStack(context, builder, securityAttributes, 24);
    LLVMValueRef saLengthIndex[1] = {LLVMConstInt(i64Type, 0, 0)};
    LLVMValueRef saLength = LLVMBuildGEP2(builder, i8Type, securityAttributes, saLengthIndex, 1, "sa_length");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 24, 0), saLength);
    LLVMValueRef saInheritIndex[1] = {LLVMConstInt(i64Type, 16, 0)};
    LLVMValueRef saInherit = LLVMBuildGEP2(builder, i8Type, securityAttributes, saInheritIndex, 1, "sa_inherit");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 1, 0), saInherit);

    // GENERIC_WRITE=0x40000000, FILE_SHARE_READ=0x1 (para poder leer el fichero mientras el hijo
    // escribe), CREATE_ALWAYS=2, FILE_ATTRIBUTE_NORMAL=0x80.
    LLVMValueRef createFileArguments[7] = {
        outputPathBuffer,
        LLVMConstInt(i32Type, 0x40000000u, 0),
        LLVMConstInt(i32Type, 0x1, 0),
        securityAttributes,
        LLVMConstInt(i32Type, 2, 0),
        LLVMConstInt(i32Type, 0x80, 0),
        LLVMConstNull(pointerType)};
    LLVMValueRef outputHandle = LLVMBuildCall2(
        builder, createFileAType, createFileA, createFileArguments, 7, "output_handle");
    LLVMValueRef outputAsInt = LLVMBuildPtrToInt(builder, outputHandle, i64Type, "output_as_int");
    LLVMValueRef invalidHandle = LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0);
    LLVMValueRef outputInvalid = LLVMBuildICmp(
        builder, LLVMIntEQ, outputAsInt, invalidHandle, "output_invalid");
    LLVMBuildCondBr(builder, outputInvalid, noOutput, haveOutput);

    // No se pudo abrir el fichero de salida -- incluida la ruta vacia, que cae aqui sola.
    LLVMPositionBuilderAtEnd(builder, noOutput);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, static_cast<unsigned long long>(-1), 0));

    LLVMPositionBuilderAtEnd(builder, haveOutput);
    // STARTUPINFOA: 104 bytes en x64. Solo tres campos importan aqui --cb, dwFlags y los tres
    // handles-- y el resto tiene que ser CERO, de ahi el memset: un dwFlags con basura hace que
    // CreateProcess interprete campos que nadie relleno.
    //
    //   cb            0     dwFlags      60     hStdInput   80
    //   hStdOutput   88     hStdError    96
    //
    // STARTF_USESTDHANDLES = 0x100. Sin ese bit, los tres handles se ignoran.
    LLVMValueRef startupInfo = LLVMBuildArrayAlloca(
        builder, i8Type, LLVMConstInt(i64Type, 104, 0), "startup_info");
    buildZeroFillStack(context, builder, startupInfo, 104);
    LLVMValueRef cbIndex[1] = {LLVMConstInt(i64Type, 0, 0)};
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 104, 0),
        LLVMBuildGEP2(builder, i8Type, startupInfo, cbIndex, 1, "si_cb"));
    LLVMValueRef flagsIndex[1] = {LLVMConstInt(i64Type, 60, 0)};
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0x100, 0),
        LLVMBuildGEP2(builder, i8Type, startupInfo, flagsIndex, 1, "si_flags"));
    LLVMValueRef stdOutIndex[1] = {LLVMConstInt(i64Type, 88, 0)};
    LLVMBuildStore(builder, outputHandle,
        LLVMBuildGEP2(builder, i8Type, startupInfo, stdOutIndex, 1, "si_stdout"));
    LLVMValueRef stdErrIndex[1] = {LLVMConstInt(i64Type, 96, 0)};
    LLVMBuildStore(builder, outputHandle,
        LLVMBuildGEP2(builder, i8Type, startupInfo, stdErrIndex, 1, "si_stderr"));

    // PROCESS_INFORMATION { HANDLE hProcess; HANDLE hThread; DWORD pid; DWORD tid; } -- 24 bytes.
    LLVMValueRef processInformation = LLVMBuildArrayAlloca(
        builder, i8Type, LLVMConstInt(i64Type, 24, 0), "process_information");
    buildZeroFillStack(context, builder, processInformation, 24);

    // bInheritHandles=TRUE es obligatorio para que el handle de salida llegue al hijo; sin el, los
    // campos hStdOutput/hStdError apuntan a un handle que el hijo no tiene.
    LLVMValueRef createProcessArguments[10] = {
        executableBuffer,
        commandLineBuffer,
        LLVMConstNull(pointerType),
        LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 1, 0),
        LLVMConstInt(i32Type, 0, 0),
        LLVMConstNull(pointerType),
        LLVMConstNull(pointerType),
        startupInfo,
        processInformation};
    LLVMValueRef spawnOk = LLVMBuildCall2(
        builder, createProcessAType, createProcessA, createProcessArguments, 10, "spawn_ok");
    // El handle del fichero se cierra AQUI, en el padre, pase lo que pase: el hijo ya tiene su
    // copia. Dejarlo abierto mantendria el fichero bloqueado despues de que el hijo terminara, y
    // quien quiere leer el resultado justo despues se encontraria con un fichero en uso.
    LLVMValueRef closeOutputArguments[1] = {outputHandle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeOutputArguments, 1, "");
    LLVMValueRef spawnFailedFlag = LLVMBuildICmp(
        builder, LLVMIntEQ, spawnOk, LLVMConstInt(i32Type, 0, 0), "spawn_failed_flag");
    LLVMBuildCondBr(builder, spawnFailedFlag, spawnFailed, spawned);

    LLVMPositionBuilderAtEnd(builder, spawnFailed);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, static_cast<unsigned long long>(-1), 0));

    LLVMPositionBuilderAtEnd(builder, spawned);
    LLVMValueRef processHandleIndex[1] = {LLVMConstInt(i64Type, 0, 0)};
    LLVMValueRef processHandleSlot = LLVMBuildGEP2(
        builder, i8Type, processInformation, processHandleIndex, 1, "process_handle_slot");
    LLVMValueRef processHandle = LLVMBuildLoad2(
        builder, pointerType, processHandleSlot, "process_handle");
    LLVMValueRef threadHandleIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef threadHandleSlot = LLVMBuildGEP2(
        builder, i8Type, processInformation, threadHandleIndex, 1, "thread_handle_slot");
    LLVMValueRef threadHandle = LLVMBuildLoad2(
        builder, pointerType, threadHandleSlot, "thread_handle");

    // INFINITE = 0xFFFFFFFF. Ver la cabecera: un enlazador colgado es un fallo del enlazador, y un
    // tiempo limite lo disfrazaria de codigo de salida raro.
    LLVMValueRef waitArguments[2] = {processHandle, LLVMConstInt(i32Type, 0xFFFFFFFFu, 0)};
    LLVMBuildCall2(builder, waitForSingleObjectType, waitForSingleObject, waitArguments, 2, "");

    LLVMValueRef exitCodeSlot = LLVMBuildAlloca(builder, i32Type, "exit_code_slot");
    // Por omision -1: si GetExitCodeProcess fallara, el resultado sigue siendo "no se pudo", que es
    // la verdad, en vez de un cero que diria "todo bien".
    LLVMBuildStore(builder, LLVMConstInt(i32Type, static_cast<unsigned long long>(-1), 0), exitCodeSlot);
    LLVMValueRef exitCodeArguments[2] = {processHandle, exitCodeSlot};
    LLVMBuildCall2(builder, getExitCodeProcessType, getExitCodeProcess, exitCodeArguments, 2, "");
    LLVMValueRef exitCode = LLVMBuildLoad2(builder, i32Type, exitCodeSlot, "exit_code");

    // LOS DOS handles del hijo. Olvidar el del hilo es la fuga clasica de CreateProcess: el
    // proceso termina, su handle se cierra, y el del hilo primario se queda vivo para siempre.
    LLVMValueRef closeThreadArguments[1] = {threadHandle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeThreadArguments, 1, "");
    LLVMValueRef closeProcessArguments[1] = {processHandle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeProcessArguments, 1, "");
    LLVMBuildRet(builder, exitCode);

    LLVMDisposeBuilder(builder);
    return true;
}

bool buildFileWriteAllText(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createFileA,
    LLVMTypeRef createFileAType,
    LLVMValueRef writeFile,
    LLVMTypeRef writeFileType,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType, pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_file_write_all_text", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef done = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef contentString = LLVMGetParam(function, 1);
    LLVMValueRef pathBuffer = buildNullTerminatedCopy(context, builder, function, pathString);
    // GENERIC_WRITE=0x40000000, dwShareMode=0 (exclusive access -- no other process should be
    // reading or writing this file WHILE this call replaces its content), lpSecurityAttributes=
    // NULL, CREATE_ALWAYS=2 (create a new file, or truncate an existing one to zero length first
    // -- exactly what "WriteAllText" means: the file ends up containing ONLY the new content,
    // nothing left over from a previous longer file), FILE_ATTRIBUTE_NORMAL=0x80,
    // hTemplateFile=NULL.
    LLVMValueRef createArguments[7] = {
        pathBuffer,
        LLVMConstInt(i32Type, 0x40000000u, 0),
        LLVMConstInt(i32Type, 0, 0),
        LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 2, 0),
        LLVMConstInt(i32Type, 0x80, 0),
        LLVMConstNull(pointerType)};
    LLVMValueRef handle = LLVMBuildCall2(builder, createFileAType, createFileA, createArguments, 7, "handle");
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMValueRef invalidHandle = LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0);
    LLVMValueRef isInvalid = LLVMBuildICmp(builder, LLVMIntEQ, handleAsInt, invalidHandle, "is_invalid");
    // CreateFileA failing (e.g. the parent directory does not exist, or a permissions problem)
    // is not surfaced as a NativeSharp-visible error in this round -- same "silently do nothing"
    // rationale as File.Delete/Directory.CreateDirectory in 53.3 when their own single Win32
    // call fails.
    LLVMBuildCondBr(builder, isInvalid, done, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    // The string CONTENT never goes through buildNullTerminatedCopy -- see this function group's
    // header comment for why: WriteFile takes an explicit byte count, not a null-terminated
    // buffer, so a NativeSharp string's own data pointer (flat +8 byte offset, no copy needed)
    // is exactly what WriteFile wants already.
    LLVMValueRef contentLength = LLVMBuildLoad2(builder, i64Type, contentString, "content_length");
    LLVMValueRef contentDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef contentData = LLVMBuildGEP2(
        builder, i8Type, contentString, contentDataIndex, 1, "content_data");
    // Same ~4 GiB truncation caveat as buildFileReadAllText's sizeI32 -- see that comment.
    LLVMValueRef contentLengthI32 = LLVMBuildTrunc(builder, contentLength, i32Type, "content_length_i32");
    LLVMValueRef bytesWrittenSlot = LLVMBuildAlloca(builder, i32Type, "bytes_written_slot");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0, 0), bytesWrittenSlot);
    LLVMValueRef writeArguments[5] = {
        handle, contentData, contentLengthI32, bytesWrittenSlot, LLVMConstNull(pointerType)};
    LLVMBuildCall2(builder, writeFileType, writeFile, writeArguments, 5, "");
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    LLVMBuildBr(builder, done);

    LLVMPositionBuilderAtEnd(builder, done);
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildFileAppendAllText(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createFileA,
    LLVMTypeRef createFileAType,
    LLVMValueRef setFilePointer,
    LLVMTypeRef setFilePointerType,
    LLVMValueRef writeFile,
    LLVMTypeRef writeFileType,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType, pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_file_append_all_text", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef done = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef contentString = LLVMGetParam(function, 1);
    LLVMValueRef pathBuffer = buildNullTerminatedCopy(context, builder, function, pathString);
    // Same as buildFileWriteAllText's CreateFileA call, except OPEN_ALWAYS=4 instead of
    // CREATE_ALWAYS=2 -- open the file if it exists (keeping its current content, which is the
    // entire point of "append"), or create it fresh if it does not exist yet (AppendAllText on a
    // missing file behaves like WriteAllText, exactly like System.IO.File.AppendAllText in real
    // .NET).
    LLVMValueRef createArguments[7] = {
        pathBuffer,
        LLVMConstInt(i32Type, 0x40000000u, 0),
        LLVMConstInt(i32Type, 0, 0),
        LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 4, 0),
        LLVMConstInt(i32Type, 0x80, 0),
        LLVMConstNull(pointerType)};
    LLVMValueRef handle = LLVMBuildCall2(builder, createFileAType, createFileA, createArguments, 7, "handle");
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMValueRef invalidHandle = LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0);
    LLVMValueRef isInvalid = LLVMBuildICmp(builder, LLVMIntEQ, handleAsInt, invalidHandle, "is_invalid");
    LLVMBuildCondBr(builder, isInvalid, done, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    // The one step buildFileWriteAllText does NOT need: seek the file pointer to the END of the
    // file before writing -- otherwise OPEN_ALWAYS leaves the file position at the START (byte
    // 0), and the write below would OVERWRITE the existing content instead of appending after
    // it. SetFilePointer(handle, 0, NULL, FILE_END=2): move 0 bytes relative to FILE_END, i.e.
    // land exactly at end-of-file. lpDistanceToMoveHigh=NULL because this compiler's files are
    // never expected to exceed 2^31 bytes from a single SetFilePointer call (the same practical
    // scope limit already noted for ReadFile/WriteFile's 32-bit byte counts).
    LLVMValueRef seekArguments[4] = {
        handle, LLVMConstInt(i32Type, 0, 0), LLVMConstNull(pointerType), LLVMConstInt(i32Type, 2, 0)};
    LLVMBuildCall2(builder, setFilePointerType, setFilePointer, seekArguments, 4, "");

    LLVMValueRef contentLength = LLVMBuildLoad2(builder, i64Type, contentString, "content_length");
    LLVMValueRef contentDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef contentData = LLVMBuildGEP2(
        builder, i8Type, contentString, contentDataIndex, 1, "content_data");
    LLVMValueRef contentLengthI32 = LLVMBuildTrunc(builder, contentLength, i32Type, "content_length_i32");
    LLVMValueRef bytesWrittenSlot = LLVMBuildAlloca(builder, i32Type, "bytes_written_slot");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0, 0), bytesWrittenSlot);
    LLVMValueRef writeArguments[5] = {
        handle, contentData, contentLengthI32, bytesWrittenSlot, LLVMConstNull(pointerType)};
    LLVMBuildCall2(builder, writeFileType, writeFile, writeArguments, 5, "");
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    LLVMBuildBr(builder, done);

    LLVMPositionBuilderAtEnd(builder, done);
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 53.4/T46 "Filesystem/Paths (cierre)" -- GetFullPathNameW. Calco del mismo patron de dos
// llamadas "pedir tamaño, despues llenar el buffer" que buildEnvironmentGetVariable estableció en
// 53.2b (ver el comentario de cabecera de esa función para el detalle completo del patrón) -- la
// única diferencia real es la firma real de GetFullPathNameW, que tiene CUATRO parámetros en vez
// de tres:
//
//   DWORD GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer,
//                           LPWSTR *lpFilePart);
//
// lpFilePart (un puntero de salida opcional al componente "nombre de archivo" dentro del buffer
// ya resuelto) se pasa siempre NULL -- este compilador no lo necesita, Path.GetFileName ya
// resuelve esa misma pregunta con manipulación de texto pura (ver Path.ns).
//
// El valor de retorno tiene la MISMA forma de dos casos que GetEnvironmentVariableA: si el buffer
// pasado es demasiado chico (el caso de la primera llamada, con lpBuffer=NULL/nBufferLength=0),
// devuelve el tamaño que hace falta INCLUYENDO el terminador nulo -- exactamente lo que
// buildEnvironmentGetVariable ya asume más abajo. 0 significa fallo (path con caracteres
// inválidos, por ejemplo) -- igual que "variable no encontrada" en 53.2b, Stage 53.4 no distingue
// ese motivo de ningún otro, misma simplificación deliberada de siempre.
//
// Un detalle que vale la pena aclarar porque NO es obvio -- y que la intuición inicial al diseñar
// esta ronda tuvo AL REVÉS: un path "" (vacío) SÍ es un error para GetFullPathNameW bajo Wine
// (devuelve 0, la rama emptyResult de abajo), a diferencia de "." (que sí resuelve al directorio
// de trabajo actual sin problema). "" NO es un sinónimo válido de "." para esta función -- ambos
// se ven parecidos ("ningún path que agregar") pero Win32 los trata distinto. Verificado bajo
// Wine con un probe aislado ANTES de empaquetar esta ronda (ver
// `54_stage53_4_filesystem_paths_cierre_implementacion_paso_a_paso.md` para el detalle completo
// de cómo se detectó la suposición incorrecta original).
bool buildPathGetFullPath(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getProcessHeap,
    LLVMTypeRef getProcessHeapType,
    LLVMValueRef heapAlloc,
    LLVMTypeRef heapAllocType,
    LLVMValueRef getFullPathNameW,
    LLVMTypeRef getFullPathNameWType,
    LLVMValueRef multiByteToWideChar,
    LLVMTypeRef multiByteToWideCharType,
    LLVMValueRef wideCharToMultiByte,
    LLVMTypeRef wideCharToMultiByteType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i16Type = LLVMInt16TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(pointerType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_path_get_full_path", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef emptyResult = LLVMAppendBasicBlockInContext(context, function, "empty_result");
    LLVMBasicBlockRef allocateResult = LLVMAppendBasicBlockInContext(context, function, "allocate_result");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef pathBuffer = buildUtf16NullTerminatedCopy(
        context, builder, pathString, multiByteToWideChar, multiByteToWideCharType);

    // First call: lpBuffer=NULL, nBufferLength=0, lpFilePart=NULL -- same "ask the required size
    // first" idiom as buildEnvironmentGetVariable's first call, just with GetFullPathNameW's
    // extra (always-NULL, in this compiler) fourth parameter.
    LLVMValueRef sizeArguments[4] = {
        pathBuffer, LLVMConstInt(i32Type, 0, 0), LLVMConstNull(pointerType), LLVMConstNull(pointerType)};
    LLVMValueRef requiredSize = LLVMBuildCall2(
        builder, getFullPathNameWType, getFullPathNameW, sizeArguments, 4, "required_size");
    LLVMValueRef failed = LLVMBuildICmp(
        builder, LLVMIntEQ, requiredSize, LLVMConstInt(i32Type, 0, 0), "failed");
    LLVMBuildCondBr(builder, failed, emptyResult, allocateResult);

    // Same deliberate simplification as buildEnvironmentGetVariable's emptyResult block: any
    // failure (not "the path does not exist" -- GetFullPathNameW never checks that, it is pure
    // lexical resolution -- but things like invalid characters) collapses to an empty NativeSharp
    // string, not an exception.
    LLVMPositionBuilderAtEnd(builder, emptyResult);
    LLVMValueRef emptyHeap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "empty_heap");
    LLVMValueRef emptyAllocArguments[3] = {
        emptyHeap, LLVMConstInt(i32Type, 0, 0), LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef emptyString = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, emptyAllocArguments, 3, "empty_string");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), emptyString);
    LLVMBuildRet(builder, emptyString);

    LLVMPositionBuilderAtEnd(builder, allocateResult);
    // GetFullPathNameW reports UTF-16 code units, including the terminator when the first call's
    // buffer is too small. Resolve into a wide scratch buffer first; NativeSharp strings remain
    // UTF-8 and are allocated only after the path has been canonicalized.
    LLVMValueRef requiredSize64 = LLVMBuildZExt(builder, requiredSize, i64Type, "required_size64");
    LLVMValueRef wideResult = LLVMBuildArrayAlloca(
        builder, i16Type, requiredSize64, "wide_result");
    // Second call: this time with a real buffer sized exactly to what the first call reported.
    // Its own return value (the length actually copied, excluding the null terminator) is
    // ignored -- same as buildEnvironmentGetVariable's fillArguments call -- because charCount
    // above already derives the same number from requiredSize.
    LLVMValueRef fillArguments[4] = {pathBuffer, requiredSize, wideResult, LLVMConstNull(pointerType)};
    LLVMBuildCall2(
        builder, getFullPathNameWType, getFullPathNameW, fillArguments, 4, "fill_result");

    LLVMValueRef utf8SizeArguments[8] = {
        LLVMConstInt(i32Type, 65001, 0), LLVMConstInt(i32Type, 0x80, 0), wideResult,
        LLVMConstInt(i32Type, 0xFFFFFFFFu, 0), LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 0, 0), LLVMConstNull(pointerType), LLVMConstNull(pointerType)};
    LLVMValueRef utf8Required = LLVMBuildCall2(
        builder, wideCharToMultiByteType, wideCharToMultiByte,
        utf8SizeArguments, 8, "utf8_required");
    LLVMValueRef utf8Required64 = LLVMBuildZExt(
        builder, utf8Required, i64Type, "utf8_required64");
    LLVMValueRef totalSize = LLVMBuildAdd(
        builder, utf8Required64, LLVMConstInt(i64Type, 8, 0), "total_size");
    LLVMValueRef heap = LLVMBuildCall2(
        builder, getProcessHeapType, getProcessHeap, nullptr, 0, "heap");
    LLVMValueRef allocArguments[3] = {
        heap, LLVMConstInt(i32Type, 0, 0), totalSize};
    LLVMValueRef resultString = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, allocArguments, 3, "result_string");
    LLVMValueRef charCount = LLVMBuildSub(
        builder, utf8Required64, LLVMConstInt(i64Type, 1, 0), "char_count");
    LLVMBuildStore(builder, charCount, resultString);
    LLVMValueRef resultDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef resultData = LLVMBuildGEP2(
        builder, i8Type, resultString, resultDataIndex, 1, "result_data");
    LLVMValueRef utf8FillArguments[8] = {
        LLVMConstInt(i32Type, 65001, 0), LLVMConstInt(i32Type, 0x80, 0), wideResult,
        LLVMConstInt(i32Type, 0xFFFFFFFFu, 0), resultData, utf8Required,
        LLVMConstNull(pointerType), LLVMConstNull(pointerType)};
    LLVMBuildCall2(
        builder, wideCharToMultiByteType, wideCharToMultiByte,
        utf8FillArguments, 8, "utf8_written");
    LLVMBuildRet(builder, resultString);

    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 53.5 "Command-line arguments" -- GetCommandLineW + CommandLineToArgvW + WideCharToMultiByte
// + LocalFree. Windows has no ANSI CommandLineToArgvA; the official parser only exists in its
// UTF-16 form. T46 also uses UTF-16 for compiler filesystem reads, but command parsing remains a
// separate boundary. Reimplementing its quoting/escaping rules by hand was considered and
// rejected -- see Environment.ns's header comment and
// `55_stage53_5_command_line_args.md` for the full design discussion Alberto and Claude worked
// through -- because CommandLineToArgvW already IS the correct, battle-tested implementation of
// those exact rules, and a hand-rolled reimplementation would only reintroduce the same class of
// risk that produced the real cmd.exe quoting bug documented in
// `15_fix_cmdexe_quoting_tests_host_abi_windows.md`. So this round crosses the ANSI boundary on
// purpose, and converts every UTF-16 result to NativeSharp's canonical UTF-8 representation via
// WideCharToMultiByte(CP_UTF8) before it reaches NativeSharp code.
//
// GetCommandLineArgCount is the simpler of the two: call GetCommandLineW to get the raw UTF-16
// command line Windows built for this process, hand it to CommandLineToArgvW (which parses it
// into an argv-style array AND writes the element count into an out-parameter), read that count,
// free the array CommandLineToArgvW allocated (LocalFree -- CommandLineToArgvW's documented
// allocator, NOT this runtime's own HeapAlloc/HeapFree pair), and return the count. A null return
// from CommandLineToArgvW (its documented failure signal) collapses to 0 -- same deliberate
// "any failure becomes the empty/zero case" simplification as every prior Stage 53 wrapper.
bool buildEnvironmentGetCommandLineArgCount(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getCommandLineW,
    LLVMTypeRef getCommandLineWType,
    LLVMValueRef commandLineToArgvW,
    LLVMTypeRef commandLineToArgvWType,
    LLVMValueRef localFree,
    LLVMTypeRef localFreeType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i32Type, {});
    LLVMValueRef function = LLVMAddFunction(
        module, "__nativesharp_environment_get_command_line_arg_count", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef failResult = LLVMAppendBasicBlockInContext(context, function, "fail_result");
    LLVMBasicBlockRef okResult = LLVMAppendBasicBlockInContext(context, function, "ok_result");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef commandLine = LLVMBuildCall2(
        builder, getCommandLineWType, getCommandLineW, nullptr, 0, "command_line");
    LLVMValueRef argCountSlot = LLVMBuildAlloca(builder, i32Type, "arg_count_slot");
    LLVMValueRef argvArguments[2] = {commandLine, argCountSlot};
    LLVMValueRef argv = LLVMBuildCall2(
        builder, commandLineToArgvWType, commandLineToArgvW, argvArguments, 2, "argv");
    LLVMValueRef argvIsNull = LLVMBuildICmp(
        builder, LLVMIntEQ, argv, LLVMConstNull(pointerType), "argv_is_null");
    LLVMBuildCondBr(builder, argvIsNull, failResult, okResult);

    LLVMPositionBuilderAtEnd(builder, failResult);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 0, 0));

    LLVMPositionBuilderAtEnd(builder, okResult);
    LLVMValueRef argCount = LLVMBuildLoad2(builder, i32Type, argCountSlot, "arg_count");
    LLVMValueRef freeArguments[1] = {argv};
    LLVMBuildCall2(builder, localFreeType, localFree, freeArguments, 1, "");
    LLVMBuildRet(builder, argCount);

    LLVMDisposeBuilder(builder);
    return true;
}

// GetCommandLineArgs: the caller (see Environment.ns) has already called GetCommandLineArgCount
// and allocated `new string[n]`, so `destination` here is that caller-provided buffer -- the
// exact same "buffer provided by caller" workaround NativeSharp.Core.String.Split established for
// the confirmed "hueco de owned string[]" architectural gap (this compiler cannot construct a
// fresh owned string[] whose elements are filled across multiple steps). This function fills
// `destination`'s existing slots and returns how many it actually wrote, mirroring Split's own
// contract.
//
// Per-argument shape: argv[i] (from CommandLineToArgvW) is an LPWSTR -- a UTF-16, null-terminated
// string. It is converted to UTF-8 with the exact same two-call "ask the size, then fill the
// buffer" idiom buildEnvironmentGetVariable/buildPathGetFullPath already established for other
// Win32 APIs, just applied to WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ..., cchWideChar=-1 for
// "auto-detect the null terminator", ...) instead of GetEnvironmentVariableA/GetFullPathNameW.
//
// writeCount = min(argc, destination.Length) -- if the real command line has more arguments than
// the caller's buffer can hold, the extra ones are silently dropped (no exception, no partial
// write into the last slot) -- this is documented explicitly in Environment.ns's doc comment as a
// real behavioral difference from String.Split (which truncates the LAST element instead).
//
// No PHI nodes anywhere below -- alloca-based index/result slots and branch-to-merge blocks, the
// same style buildNullTerminatedCopy's copy loop and every other loop in this runtime already use.
bool buildEnvironmentGetCommandLineArgs(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getProcessHeap,
    LLVMTypeRef getProcessHeapType,
    LLVMValueRef heapAlloc,
    LLVMTypeRef heapAllocType,
    LLVMValueRef getCommandLineW,
    LLVMTypeRef getCommandLineWType,
    LLVMValueRef commandLineToArgvW,
    LLVMTypeRef commandLineToArgvWType,
    LLVMValueRef wideCharToMultiByte,
    LLVMTypeRef wideCharToMultiByteType,
    LLVMValueRef localFree,
    LLVMTypeRef localFreeType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i32Type, {pointerType});
    LLVMValueRef function = LLVMAddFunction(
        module, "__nativesharp_environment_get_command_line_args", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef failResult = LLVMAppendBasicBlockInContext(context, function, "fail_result");
    LLVMBasicBlockRef okEntry = LLVMAppendBasicBlockInContext(context, function, "ok_entry");
    LLVMBasicBlockRef useArgCount = LLVMAppendBasicBlockInContext(context, function, "use_arg_count");
    LLVMBasicBlockRef useDestLength = LLVMAppendBasicBlockInContext(context, function, "use_dest_length");
    LLVMBasicBlockRef loopCheck = LLVMAppendBasicBlockInContext(context, function, "loop_check");
    LLVMBasicBlockRef loopBody = LLVMAppendBasicBlockInContext(context, function, "loop_body");
    LLVMBasicBlockRef elementEmpty = LLVMAppendBasicBlockInContext(context, function, "element_empty");
    LLVMBasicBlockRef elementAlloc = LLVMAppendBasicBlockInContext(context, function, "element_alloc");
    LLVMBasicBlockRef elementMerge = LLVMAppendBasicBlockInContext(context, function, "element_merge");
    LLVMBasicBlockRef loopIncrement = LLVMAppendBasicBlockInContext(context, function, "loop_increment");
    LLVMBasicBlockRef loopDone = LLVMAppendBasicBlockInContext(context, function, "loop_done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef destination = LLVMGetParam(function, 0);
    LLVMValueRef commandLine = LLVMBuildCall2(
        builder, getCommandLineWType, getCommandLineW, nullptr, 0, "command_line");
    LLVMValueRef argCountSlot = LLVMBuildAlloca(builder, i32Type, "arg_count_slot");
    LLVMValueRef argvArguments[2] = {commandLine, argCountSlot};
    LLVMValueRef argv = LLVMBuildCall2(
        builder, commandLineToArgvWType, commandLineToArgvW, argvArguments, 2, "argv");
    LLVMValueRef argvIsNull = LLVMBuildICmp(
        builder, LLVMIntEQ, argv, LLVMConstNull(pointerType), "argv_is_null");
    LLVMBuildCondBr(builder, argvIsNull, failResult, okEntry);

    LLVMPositionBuilderAtEnd(builder, failResult);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 0, 0));

    // writeCount = min(argc, destination.Length) -- computed via a branch-to-merge (no Select in
    // this LLVM C API wrapper), same alloca-based idiom as everything else in this function. All
    // three allocas (and indexSlot's zero-initialization) sit here in okEntry, BEFORE the
    // fits-branch's terminator -- useArgCount/useDestLength each only ever add ONE instruction
    // (a store) before their own terminator, since a basic block's terminator must stay the last
    // instruction in that block; loopCheck picks writeCount/index back up via a load, right
    // after the two paths merge into it.
    LLVMPositionBuilderAtEnd(builder, okEntry);
    LLVMValueRef argCount32 = LLVMBuildLoad2(builder, i32Type, argCountSlot, "arg_count");
    LLVMValueRef argCount64 = LLVMBuildZExt(builder, argCount32, i64Type, "arg_count64");
    LLVMValueRef destLength = LLVMBuildLoad2(builder, i64Type, destination, "dest_length");
    LLVMValueRef argCountFits = LLVMBuildICmp(
        builder, LLVMIntULT, argCount64, destLength, "arg_count_fits");
    LLVMValueRef writeCountSlot = LLVMBuildAlloca(builder, i64Type, "write_count_slot");
    LLVMValueRef indexSlot = LLVMBuildAlloca(builder, i64Type, "index_slot");
    LLVMValueRef elementResultSlot = LLVMBuildAlloca(builder, pointerType, "element_result_slot");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), indexSlot);
    LLVMBuildCondBr(builder, argCountFits, useArgCount, useDestLength);

    LLVMPositionBuilderAtEnd(builder, useArgCount);
    LLVMBuildStore(builder, argCount64, writeCountSlot);
    LLVMBuildBr(builder, loopCheck);

    LLVMPositionBuilderAtEnd(builder, useDestLength);
    LLVMBuildStore(builder, destLength, writeCountSlot);
    LLVMBuildBr(builder, loopCheck);

    LLVMPositionBuilderAtEnd(builder, loopCheck);
    LLVMValueRef writeCount = LLVMBuildLoad2(builder, i64Type, writeCountSlot, "write_count");
    LLVMValueRef index = LLVMBuildLoad2(builder, i64Type, indexSlot, "index");
    LLVMValueRef indexDone = LLVMBuildICmp(builder, LLVMIntUGE, index, writeCount, "index_done");
    LLVMBuildCondBr(builder, indexDone, loopDone, loopBody);

    // argv (from CommandLineToArgvW) is a plain C array of LPWSTR -- no NativeSharp { i64 length;
    // ... } header, elements start at byte offset 0, element i at offset i*8 (8-byte pointers).
    LLVMPositionBuilderAtEnd(builder, loopBody);
    LLVMValueRef argvElementByteOffset = LLVMBuildMul(
        builder, index, LLVMConstInt(i64Type, 8, 0), "argv_element_byte_offset");
    LLVMValueRef argvElementByteOffsetIndex[1] = {argvElementByteOffset};
    LLVMValueRef argvElementAddress = LLVMBuildGEP2(
        builder, i8Type, argv, argvElementByteOffsetIndex, 1, "argv_element_address");
    LLVMValueRef wideArg = LLVMBuildLoad2(builder, pointerType, argvElementAddress, "wide_arg");

    // First WideCharToMultiByte call: lpMultiByteStr=NULL, cbMultiByte=0 -- same "ask the
    // required size" idiom as GetEnvironmentVariableA/GetFullPathNameW's first call above.
    // cchWideChar=-1 (0xFFFFFFFF) means "the string is null-terminated, figure out its length
    // yourself" -- the required size this returns INCLUDES room for the null terminator.
    LLVMValueRef sizeArguments[8] = {
        LLVMConstInt(i32Type, 65001, 0), // CP_UTF8
        LLVMConstInt(i32Type, 0x80, 0), // WC_ERR_INVALID_CHARS
        wideArg,
        LLVMConstInt(i32Type, 0xFFFFFFFFu, 0), // cchWideChar = -1
        LLVMConstNull(pointerType), // lpMultiByteStr
        LLVMConstInt(i32Type, 0, 0), // cbMultiByte
        LLVMConstNull(pointerType), // lpDefaultChar
        LLVMConstNull(pointerType)}; // lpUsedDefaultChar
    LLVMValueRef requiredSize = LLVMBuildCall2(
        builder, wideCharToMultiByteType, wideCharToMultiByte, sizeArguments, 8, "required_size");
    LLVMValueRef elementFailed = LLVMBuildICmp(
        builder, LLVMIntEQ, requiredSize, LLVMConstInt(i32Type, 0, 0), "element_failed");
    LLVMBuildCondBr(builder, elementFailed, elementEmpty, elementAlloc);

    // Same deliberate "any failure collapses to an empty NativeSharp string" simplification as
    // every prior Stage 53 wrapper's emptyResult block.
    LLVMPositionBuilderAtEnd(builder, elementEmpty);
    LLVMValueRef emptyHeap = LLVMBuildCall2(
        builder, getProcessHeapType, getProcessHeap, nullptr, 0, "empty_heap");
    LLVMValueRef emptyAllocArguments[3] = {
        emptyHeap, LLVMConstInt(i32Type, 0, 0), LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef emptyElement = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, emptyAllocArguments, 3, "empty_element");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), emptyElement);
    LLVMBuildStore(builder, emptyElement, elementResultSlot);
    LLVMBuildBr(builder, elementMerge);

    LLVMPositionBuilderAtEnd(builder, elementAlloc);
    LLVMValueRef requiredSize64 = LLVMBuildZExt(builder, requiredSize, i64Type, "required_size64");
    LLVMValueRef totalSize = LLVMBuildAdd(builder, requiredSize64, LLVMConstInt(i64Type, 8, 0), "total_size");
    LLVMValueRef heap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "heap");
    LLVMValueRef allocArguments[3] = {heap, LLVMConstInt(i32Type, 0, 0), totalSize};
    LLVMValueRef elementString = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, allocArguments, 3, "element_string");
    LLVMValueRef charCount = LLVMBuildSub(builder, requiredSize64, LLVMConstInt(i64Type, 1, 0), "char_count");
    LLVMBuildStore(builder, charCount, elementString);
    LLVMValueRef elementDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef elementData = LLVMBuildGEP2(builder, i8Type, elementString, elementDataIndex, 1, "element_data");
    // Second call: a real buffer this time, sized exactly to what the first call reported. Its
    // own return value (bytes written) is ignored -- same as every prior two-call wrapper here --
    // because charCount above already derives the same number from requiredSize.
    LLVMValueRef fillArguments[8] = {
        LLVMConstInt(i32Type, 65001, 0),
        LLVMConstInt(i32Type, 0x80, 0),
        wideArg,
        LLVMConstInt(i32Type, 0xFFFFFFFFu, 0),
        elementData,
        requiredSize,
        LLVMConstNull(pointerType),
        LLVMConstNull(pointerType)};
    LLVMBuildCall2(builder, wideCharToMultiByteType, wideCharToMultiByte, fillArguments, 8, "");
    LLVMBuildStore(builder, elementString, elementResultSlot);
    LLVMBuildBr(builder, elementMerge);

    LLVMPositionBuilderAtEnd(builder, elementMerge);
    LLVMValueRef elementResult = LLVMBuildLoad2(builder, pointerType, elementResultSlot, "element_result");
    // destination's physical layout is the same NativeSharp array layout as every other
    // string[]/array in this compiler: { i64 length; element0; element1; ... }, dataOffset=8 for
    // 8-byte pointer-sized elements (see lowerArrayElementAddress/lowerArrayAllocate in
    // ProjectOopLlvmLowerer.cpp) -- element i lives at byte offset 8 + i*8, same GEP-with-i8Type
    // idiom the MoveArrayElement MIR case already uses to store a pointer-typed value there.
    LLVMValueRef destElementByteOffset = LLVMBuildAdd(
        builder, argvElementByteOffset, LLVMConstInt(i64Type, 8, 0), "dest_element_byte_offset");
    LLVMValueRef destElementByteOffsetIndex[1] = {destElementByteOffset};
    LLVMValueRef destElementAddress = LLVMBuildGEP2(
        builder, i8Type, destination, destElementByteOffsetIndex, 1, "dest_element_address");
    LLVMBuildStore(builder, elementResult, destElementAddress);
    LLVMBuildBr(builder, loopIncrement);

    LLVMPositionBuilderAtEnd(builder, loopIncrement);
    LLVMValueRef nextIndex = LLVMBuildAdd(builder, index, LLVMConstInt(i64Type, 1, 0), "next_index");
    LLVMBuildStore(builder, nextIndex, indexSlot);
    LLVMBuildBr(builder, loopCheck);

    LLVMPositionBuilderAtEnd(builder, loopDone);
    LLVMValueRef freeArguments[1] = {argv};
    LLVMBuildCall2(builder, localFreeType, localFree, freeArguments, 1, "");
    LLVMValueRef writeCount32 = LLVMBuildTrunc(builder, writeCount, i32Type, "write_count32");
    LLVMBuildRet(builder, writeCount32);

    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 53.6/T46 directory enumeration uses FindFirstFileW/FindNextFileW/FindClose so paths and
// discovered names survive independently of the active Windows code page. See Directory.ns for
// the full two-pass design (a single pair of C++ helpers parameterized by `wantDirectories`,
// de dos pasadas, un solo par de funciones C++ compartidas parametrizadas por un booleano
// which decides at code-generation time -- not runtime --
// que rama del filtro por atributo emitir, exactamente igual a como buildFileExists/
// buildDirectoryExists share the same GetFileAttributesW constants but invert
// la comparacion).
//
// Layout real de WIN32_FIND_DATAW (struct de Win32, NO decision de este compilador) que este
// runtime asume al leerla campo por campo via GEP con offsets constantes, en vez de declarar el
// struct completo en la capa LLVM C API (que no tiene soporte de structs con nombre en este
// wrapper -- ver LlvmCApi.h):
//   offset  0: DWORD dwFileAttributes            (4 bytes -- el bit FILE_ATTRIBUTE_DIRECTORY,
//                                                  0x10, es el mismo que File.Exists/
//                                                  Directory.Exists ya usan)
//   offset  4: FILETIME ftCreationTime            (8 bytes, sin usar por esta ronda)
//   offset 12: FILETIME ftLastAccessTime          (8 bytes, sin usar)
//   offset 20: FILETIME ftLastWriteTime           (8 bytes, sin usar)
//   offset 28: DWORD nFileSizeHigh                (4 bytes, sin usar)
//   offset 32: DWORD nFileSizeLow                 (4 bytes, sin usar)
//   offset 36: DWORD dwReserved0                  (4 bytes, sin usar)
//   offset 40: DWORD dwReserved1                  (4 bytes, sin usar)
//   offset 44: WCHAR cFileName[260]               (el nombre, null-terminated UTF-16 -- lo unico
//                                                  que GetFiles/GetDirectories realmente leen)
//   offset 564: WCHAR cAlternateFileName[14]       (nombre corto 8.3, sin usar)
//   sizeof == 592 (redondeado a multiplo de 4 -- el struct entero es DWORD-aligned)
// Cada funcion de esta ronda reserva 592 bytes en el stack para este struct via
// LLVMBuildArrayAlloca -- el mismo idiom de scratch-en-el-stack que buildNullTerminatedCopy usa
// para su buffer temporal, ya que el struct solo necesita vivir durante la llamada.

// Construye "path" + "\*" + terminador nulo UTF-16 -- el patron que FindFirstFileW/
// FindNextFileW requieren para enumerar TODO el contenido de un directorio (un patron sin
// comodin, o el path a secas, buscaria solo una entrada exacta). No maneja el caso de un path
// que ya termina en '\\' de forma especial (produciria "C:\foo\\*", que Win32 tolera en la
// practica) -- misma disciplina de simplificacion deliberada de cada ronda anterior. Deja el
// builder posicionado justo despues de que el patron termina de construirse, mismo contrato que
// buildNullTerminatedCopy.
LLVMValueRef buildDirectorySearchPattern(
    LLVMContextRef context,
    LLVMBuilderRef builder,
    LLVMValueRef sourceString,
    LLVMValueRef multiByteToWideChar,
    LLVMTypeRef multiByteToWideCharType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i16Type = LLVMInt16TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);

    LLVMValueRef length64 = LLVMBuildLoad2(builder, i64Type, sourceString, "pattern_source_length");
    LLVMValueRef length32 = LLVMBuildTrunc(builder, length64, i32Type, "pattern_source_length32");
    LLVMValueRef sourceDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef sourceData = LLVMBuildGEP2(
        builder, i8Type, sourceString, sourceDataIndex, 1, "pattern_source_data");
    LLVMValueRef sizeArguments[6] = {
        LLVMConstInt(i32Type, 65001, 0), LLVMConstInt(i32Type, 0x8, 0), sourceData, length32,
        LLVMConstNull(pointerType), LLVMConstInt(i32Type, 0, 0)};
    LLVMValueRef required = LLVMBuildCall2(
        builder, multiByteToWideCharType, multiByteToWideChar, sizeArguments, 6, "pattern_required");
    LLVMValueRef required64 = LLVMBuildZExt(builder, required, i64Type, "pattern_required64");
    LLVMValueRef capacity = LLVMBuildAdd(
        builder, required64, LLVMConstInt(i64Type, 3, 0), "pattern_capacity");
    LLVMValueRef buffer = LLVMBuildArrayAlloca(builder, i16Type, capacity, "pattern_buffer");
    LLVMValueRef fillArguments[6] = {
        LLVMConstInt(i32Type, 65001, 0), LLVMConstInt(i32Type, 0x8, 0), sourceData, length32,
        buffer, required};
    LLVMBuildCall2(
        builder, multiByteToWideCharType, multiByteToWideChar, fillArguments, 6, "pattern_written");
    LLVMValueRef backslashIndex[1] = {required64};
    LLVMValueRef backslashPtr = LLVMBuildGEP2(builder, i16Type, buffer, backslashIndex, 1, "pattern_backslash_ptr");
    LLVMBuildStore(builder, LLVMConstInt(i16Type, '\\', 0), backslashPtr);
    LLVMValueRef starOffset = LLVMBuildAdd(builder, required64, LLVMConstInt(i64Type, 1, 0), "pattern_star_offset");
    LLVMValueRef starIndex[1] = {starOffset};
    LLVMValueRef starPtr = LLVMBuildGEP2(builder, i16Type, buffer, starIndex, 1, "pattern_star_ptr");
    LLVMBuildStore(builder, LLVMConstInt(i16Type, '*', 0), starPtr);
    LLVMValueRef terminatorOffset = LLVMBuildAdd(
        builder, required64, LLVMConstInt(i64Type, 2, 0), "pattern_terminator_offset");
    LLVMValueRef terminatorIndex[1] = {terminatorOffset};
    LLVMValueRef terminatorPtr = LLVMBuildGEP2(
        builder, i16Type, buffer, terminatorIndex, 1, "pattern_terminator_ptr");
    LLVMBuildStore(builder, LLVMConstInt(i16Type, 0, 0), terminatorPtr);
    return buffer;
}

// Convierte WIN32_FIND_DATAW.cFileName de UTF-16 a la representacion UTF-8 de
// NativeSharp.Core.String. WideCharToMultiByte incluye el terminador en `required`; el objeto
// guarda `required - 1` como longitud y reserva esos bytes mas el encabezado de ocho bytes.
LLVMValueRef buildWideCStringToNativeSharpString(
    LLVMContextRef context,
    LLVMBuilderRef builder,
    LLVMValueRef function,
    LLVMValueRef getProcessHeap,
    LLVMTypeRef getProcessHeapType,
    LLVMValueRef heapAlloc,
    LLVMTypeRef heapAllocType,
    LLVMValueRef wideCharToMultiByte,
    LLVMTypeRef wideCharToMultiByteType,
    LLVMValueRef cString)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    (void)function;

    LLVMValueRef sizeArguments[8] = {
        LLVMConstInt(i32Type, 65001, 0), LLVMConstInt(i32Type, 0x80, 0), cString,
        LLVMConstInt(i32Type, 0xFFFFFFFFu, 0), LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 0, 0), LLVMConstNull(pointerType), LLVMConstNull(pointerType)};
    LLVMValueRef required = LLVMBuildCall2(
        builder, wideCharToMultiByteType, wideCharToMultiByte, sizeArguments, 8, "cstr_required");
    LLVMValueRef required64 = LLVMBuildZExt(builder, required, i64Type, "cstr_required64");
    LLVMValueRef length = LLVMBuildSub(
        builder, required64, LLVMConstInt(i64Type, 1, 0), "cstr_length");
    LLVMValueRef heap = LLVMBuildCall2(builder, getProcessHeapType, getProcessHeap, nullptr, 0, "cstr_heap");
    LLVMValueRef totalSize = LLVMBuildAdd(
        builder, required64, LLVMConstInt(i64Type, 8, 0), "cstr_total_size");
    LLVMValueRef allocArguments[3] = {heap, LLVMConstInt(i32Type, 0, 0), totalSize};
    LLVMValueRef resultString = LLVMBuildCall2(
        builder, heapAllocType, heapAlloc, allocArguments, 3, "cstr_result_string");
    LLVMBuildStore(builder, length, resultString);
    LLVMValueRef resultDataIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef resultData = LLVMBuildGEP2(
        builder, i8Type, resultString, resultDataIndex, 1, "cstr_result_data");
    LLVMValueRef fillArguments[8] = {
        LLVMConstInt(i32Type, 65001, 0), LLVMConstInt(i32Type, 0x80, 0), cString,
        LLVMConstInt(i32Type, 0xFFFFFFFFu, 0), resultData, required,
        LLVMConstNull(pointerType), LLVMConstNull(pointerType)};
    LLVMBuildCall2(
        builder, wideCharToMultiByteType, wideCharToMultiByte, fillArguments, 8, "cstr_written");
    return resultString;
}

// Emite, dentro del bloque actual (`considerEntry`), la prueba de atributo compartida por las
// cuatro funciones de esta ronda: isDirectory = (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) !=
// 0 (el mismo bit 0x10 que buildFileExists/buildDirectoryExists ya usan), despues matchesKind =
// wantDirectories ? isDirectory : !isDirectory. `wantDirectories` es un booleano de C++ (decide
// en tiempo de generacion de codigo que rama emitir), NO un valor de runtime -- ver el comentario
// de cabecera de este grupo de funciones.
LLVMValueRef buildDirectoryEntryMatchesKind(
    LLVMBuilderRef builder,
    LLVMTypeRef i32Type,
    LLVMValueRef findData,
    bool wantDirectories)
{
    LLVMValueRef attributes = LLVMBuildLoad2(builder, i32Type, findData, "attributes");
    LLVMValueRef directoryBit = LLVMBuildAnd(
        builder, attributes, LLVMConstInt(i32Type, 0x10, 0), "directory_bit");
    LLVMValueRef isDirectory = LLVMBuildICmp(
        builder, LLVMIntNE, directoryBit, LLVMConstInt(i32Type, 0, 0), "is_directory");
    if (wantDirectories)
    {
        return isDirectory;
    }
    return LLVMBuildNot(builder, isDirectory, "is_file");
}

// Emite, dentro del bloque actual, la prueba de "es la entrada pseudo '.' o '..'" -- solo
// llamada cuando wantDirectories es true (GetFiles nunca la necesita: los archivos nunca se
// llaman "." o ".."). cFileName vive en el offset 44 de WIN32_FIND_DATAW (ver el comentario de
// cabecera de este grupo de funciones); comparar los primeros tres wchar_t distingue ambas
// entradas de cualquier nombre real mas largo.
LLVMValueRef buildDirectoryEntryIsPseudo(
    LLVMBuilderRef builder,
    LLVMTypeRef i64Type,
    LLVMTypeRef i8Type,
    LLVMTypeRef i16Type,
    LLVMValueRef findData)
{
    LLVMValueRef nameIndex0[1] = {LLVMConstInt(i64Type, 44, 0)};
    LLVMValueRef namePtr0 = LLVMBuildGEP2(builder, i8Type, findData, nameIndex0, 1, "name_char0_ptr");
    LLVMValueRef wcharIndex1[1] = {LLVMConstInt(i64Type, 1, 0)};
    LLVMValueRef wcharIndex2[1] = {LLVMConstInt(i64Type, 2, 0)};
    LLVMValueRef namePtr1 = LLVMBuildGEP2(builder, i16Type, namePtr0, wcharIndex1, 1, "name_char1_ptr");
    LLVMValueRef namePtr2 = LLVMBuildGEP2(builder, i16Type, namePtr0, wcharIndex2, 1, "name_char2_ptr");
    LLVMValueRef char0 = LLVMBuildLoad2(builder, i16Type, namePtr0, "name_char0");
    LLVMValueRef char1 = LLVMBuildLoad2(builder, i16Type, namePtr1, "name_char1");
    LLVMValueRef char2 = LLVMBuildLoad2(builder, i16Type, namePtr2, "name_char2");

    LLVMValueRef char0IsDot = LLVMBuildICmp(
        builder, LLVMIntEQ, char0, LLVMConstInt(i16Type, '.', 0), "char0_is_dot");
    LLVMValueRef char1IsNull = LLVMBuildICmp(
        builder, LLVMIntEQ, char1, LLVMConstInt(i16Type, 0, 0), "char1_is_null");
    LLVMValueRef isDot = LLVMBuildAnd(builder, char0IsDot, char1IsNull, "is_dot");

    LLVMValueRef char1IsDot = LLVMBuildICmp(
        builder, LLVMIntEQ, char1, LLVMConstInt(i16Type, '.', 0), "char1_is_dot");
    LLVMValueRef char2IsNull = LLVMBuildICmp(
        builder, LLVMIntEQ, char2, LLVMConstInt(i16Type, 0, 0), "char2_is_null");
    LLVMValueRef dotDotPrefix = LLVMBuildAnd(builder, char0IsDot, char1IsDot, "dotdot_prefix");
    LLVMValueRef isDotDot = LLVMBuildAnd(builder, dotDotPrefix, char2IsNull, "is_dotdot");

    return LLVMBuildOr(builder, isDot, isDotDot, "is_pseudo_entry");
}

// GetFileCount/GetDirectoryCount: primera pasada, solo cuenta -- ver el comentario de cabecera de
// Directory.ns. Un solo recorrido FindFirstFileW/FindNextFileW/FindClose sobre "path\*", filtrado
// por el bit FILE_ATTRIBUTE_DIRECTORY segun `wantDirectories` (y, cuando wantDirectories es true,
// excluyendo ademas "." y ".." -- ver buildDirectoryEntryIsPseudo). Directorio inexistente/
// inaccesible (FindFirstFileW devuelve INVALID_HANDLE_VALUE) colapsa a 0, misma simplificacion
// deliberada de siempre.
bool buildDirectoryGetEntryCount(
    LLVMContextRef context,
    LLVMModuleRef module,
    const char* functionName,
    bool wantDirectories,
    LLVMValueRef findFirstFileW,
    LLVMTypeRef findFirstFileWType,
    LLVMValueRef findNextFileW,
    LLVMTypeRef findNextFileWType,
    LLVMValueRef findClose,
    LLVMTypeRef findCloseType,
    LLVMValueRef multiByteToWideChar,
    LLVMTypeRef multiByteToWideCharType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i16Type = LLVMInt16TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i32Type, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, functionName, type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef failResult = LLVMAppendBasicBlockInContext(context, function, "fail_result");
    LLVMBasicBlockRef considerEntry = LLVMAppendBasicBlockInContext(context, function, "consider_entry");
    LLVMBasicBlockRef checkPseudo = wantDirectories
        ? LLVMAppendBasicBlockInContext(context, function, "check_pseudo")
        : nullptr;
    LLVMBasicBlockRef countEntry = LLVMAppendBasicBlockInContext(context, function, "count_entry");
    LLVMBasicBlockRef advance = LLVMAppendBasicBlockInContext(context, function, "advance");
    LLVMBasicBlockRef loopDone = LLVMAppendBasicBlockInContext(context, function, "loop_done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef searchPattern = buildDirectorySearchPattern(
        context, builder, pathString, multiByteToWideChar, multiByteToWideCharType);
    LLVMValueRef findData = LLVMBuildArrayAlloca(
        builder, i8Type, LLVMConstInt(i64Type, 592, 0), "find_data");
    LLVMValueRef countSlot = LLVMBuildAlloca(builder, i32Type, "count_slot");
    LLVMValueRef handleSlot = LLVMBuildAlloca(builder, pointerType, "handle_slot");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0, 0), countSlot);
    LLVMValueRef firstArguments[2] = {searchPattern, findData};
    LLVMValueRef handle = LLVMBuildCall2(
        builder, findFirstFileWType, findFirstFileW, firstArguments, 2, "handle");
    LLVMBuildStore(builder, handle, handleSlot);
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMValueRef invalidHandle = LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0);
    LLVMValueRef isInvalid = LLVMBuildICmp(builder, LLVMIntEQ, handleAsInt, invalidHandle, "is_invalid");
    LLVMBuildCondBr(builder, isInvalid, failResult, considerEntry);

    LLVMPositionBuilderAtEnd(builder, failResult);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 0, 0));

    LLVMPositionBuilderAtEnd(builder, considerEntry);
    LLVMValueRef matchesKind = buildDirectoryEntryMatchesKind(builder, i32Type, findData, wantDirectories);
    if (wantDirectories)
    {
        LLVMBuildCondBr(builder, matchesKind, checkPseudo, advance);
        LLVMPositionBuilderAtEnd(builder, checkPseudo);
        LLVMValueRef isPseudo = buildDirectoryEntryIsPseudo(
            builder, i64Type, i8Type, i16Type, findData);
        LLVMBuildCondBr(builder, isPseudo, advance, countEntry);
    }
    else
    {
        LLVMBuildCondBr(builder, matchesKind, countEntry, advance);
    }

    LLVMPositionBuilderAtEnd(builder, countEntry);
    LLVMValueRef currentCount = LLVMBuildLoad2(builder, i32Type, countSlot, "current_count");
    LLVMValueRef nextCount = LLVMBuildAdd(builder, currentCount, LLVMConstInt(i32Type, 1, 0), "next_count");
    LLVMBuildStore(builder, nextCount, countSlot);
    LLVMBuildBr(builder, advance);

    LLVMPositionBuilderAtEnd(builder, advance);
    LLVMValueRef currentHandle = LLVMBuildLoad2(builder, pointerType, handleSlot, "current_handle");
    LLVMValueRef nextArguments[2] = {currentHandle, findData};
    LLVMValueRef hasNext = LLVMBuildCall2(
        builder, findNextFileWType, findNextFileW, nextArguments, 2, "has_next");
    LLVMValueRef hasNextBool = LLVMBuildICmp(
        builder, LLVMIntNE, hasNext, LLVMConstInt(i32Type, 0, 0), "has_next_bool");
    LLVMBuildCondBr(builder, hasNextBool, considerEntry, loopDone);

    LLVMPositionBuilderAtEnd(builder, loopDone);
    LLVMValueRef finalHandle = LLVMBuildLoad2(builder, pointerType, handleSlot, "final_handle");
    LLVMValueRef closeArguments[1] = {finalHandle};
    LLVMBuildCall2(builder, findCloseType, findClose, closeArguments, 1, "");
    LLVMValueRef finalCount = LLVMBuildLoad2(builder, i32Type, countSlot, "final_count");
    LLVMBuildRet(builder, finalCount);

    LLVMDisposeBuilder(builder);
    return true;
}

// GetFiles/GetDirectories: segunda pasada, llena `destination` (el buffer provisto por el
// caller) con los NOMBRES que hacen match, y devuelve cuantos escribio de verdad. Mismo recorrido
// FindFirstFileW/FindNextFileW/FindClose y mismo filtro que buildDirectoryGetEntryCount arriba,
// pero en vez de incrementar un contador, cada match se convierte a NativeSharp string (via
// buildWideCStringToNativeSharpString) y se escribe en destination[writeIndex] -- exactamente la
// misma direccion "destination's layout es { i64 length; element0; ... }, elemento i en el byte
// offset 8 + i*8" que buildEnvironmentGetCommandLineArgs ya establecio. Si destination ya esta
// lleno (writeIndex == destination.Length) el loop corta temprano hacia loopDone en vez de seguir
// enumerando -- ya no hay donde escribir mas, y las entradas que sobran se descartan en silencio
// (ver el comentario de cabecera de Directory.ns), sin excepcion.
bool buildDirectoryGetEntries(
    LLVMContextRef context,
    LLVMModuleRef module,
    const char* functionName,
    bool wantDirectories,
    LLVMValueRef getProcessHeap,
    LLVMTypeRef getProcessHeapType,
    LLVMValueRef heapAlloc,
    LLVMTypeRef heapAllocType,
    LLVMValueRef findFirstFileW,
    LLVMTypeRef findFirstFileWType,
    LLVMValueRef findNextFileW,
    LLVMTypeRef findNextFileWType,
    LLVMValueRef findClose,
    LLVMTypeRef findCloseType,
    LLVMValueRef multiByteToWideChar,
    LLVMTypeRef multiByteToWideCharType,
    LLVMValueRef wideCharToMultiByte,
    LLVMTypeRef wideCharToMultiByteType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i16Type = LLVMInt16TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i32Type, {pointerType, pointerType});
    LLVMValueRef function = LLVMAddFunction(module, functionName, type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef failResult = LLVMAppendBasicBlockInContext(context, function, "fail_result");
    LLVMBasicBlockRef considerEntry = LLVMAppendBasicBlockInContext(context, function, "consider_entry");
    LLVMBasicBlockRef checkPseudo = wantDirectories
        ? LLVMAppendBasicBlockInContext(context, function, "check_pseudo")
        : nullptr;
    LLVMBasicBlockRef spaceCheck = LLVMAppendBasicBlockInContext(context, function, "space_check");
    LLVMBasicBlockRef writeEntry = LLVMAppendBasicBlockInContext(context, function, "write_entry");
    LLVMBasicBlockRef advance = LLVMAppendBasicBlockInContext(context, function, "advance");
    LLVMBasicBlockRef loopDone = LLVMAppendBasicBlockInContext(context, function, "loop_done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef pathString = LLVMGetParam(function, 0);
    LLVMValueRef destination = LLVMGetParam(function, 1);
    LLVMValueRef searchPattern = buildDirectorySearchPattern(
        context, builder, pathString, multiByteToWideChar, multiByteToWideCharType);
    LLVMValueRef findData = LLVMBuildArrayAlloca(
        builder, i8Type, LLVMConstInt(i64Type, 592, 0), "find_data");
    LLVMValueRef destLength = LLVMBuildLoad2(builder, i64Type, destination, "dest_length");
    LLVMValueRef writeIndexSlot = LLVMBuildAlloca(builder, i64Type, "write_index_slot");
    LLVMValueRef handleSlot = LLVMBuildAlloca(builder, pointerType, "handle_slot");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), writeIndexSlot);
    LLVMValueRef firstArguments[2] = {searchPattern, findData};
    LLVMValueRef handle = LLVMBuildCall2(
        builder, findFirstFileWType, findFirstFileW, firstArguments, 2, "handle");
    LLVMBuildStore(builder, handle, handleSlot);
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMValueRef invalidHandle = LLVMConstInt(i64Type, static_cast<unsigned long long>(-1), 0);
    LLVMValueRef isInvalid = LLVMBuildICmp(builder, LLVMIntEQ, handleAsInt, invalidHandle, "is_invalid");
    LLVMBuildCondBr(builder, isInvalid, failResult, considerEntry);

    LLVMPositionBuilderAtEnd(builder, failResult);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 0, 0));

    LLVMPositionBuilderAtEnd(builder, considerEntry);
    LLVMValueRef matchesKind = buildDirectoryEntryMatchesKind(builder, i32Type, findData, wantDirectories);
    if (wantDirectories)
    {
        LLVMBuildCondBr(builder, matchesKind, checkPseudo, advance);
        LLVMPositionBuilderAtEnd(builder, checkPseudo);
        LLVMValueRef isPseudo = buildDirectoryEntryIsPseudo(
            builder, i64Type, i8Type, i16Type, findData);
        LLVMBuildCondBr(builder, isPseudo, advance, spaceCheck);
    }
    else
    {
        LLVMBuildCondBr(builder, matchesKind, spaceCheck, advance);
    }

    LLVMPositionBuilderAtEnd(builder, spaceCheck);
    LLVMValueRef writeIndexForSpaceCheck = LLVMBuildLoad2(builder, i64Type, writeIndexSlot, "write_index_space_check");
    LLVMValueRef hasSpace = LLVMBuildICmp(
        builder, LLVMIntULT, writeIndexForSpaceCheck, destLength, "has_space");
    // Sin espacio: cortar directamente a loopDone en vez de a advance -- seguir enumerando no
    // serviria de nada, ya no hay donde escribir mas (ver el comentario de cabecera de esta
    // funcion).
    LLVMBuildCondBr(builder, hasSpace, writeEntry, loopDone);

    LLVMPositionBuilderAtEnd(builder, writeEntry);
    LLVMValueRef nameIndex[1] = {LLVMConstInt(i64Type, 44, 0)};
    LLVMValueRef namePtr = LLVMBuildGEP2(builder, i8Type, findData, nameIndex, 1, "name_ptr");
    LLVMValueRef elementString = buildWideCStringToNativeSharpString(
        context, builder, function, getProcessHeap, getProcessHeapType, heapAlloc, heapAllocType,
        wideCharToMultiByte, wideCharToMultiByteType, namePtr);
    LLVMValueRef writeIndex = LLVMBuildLoad2(builder, i64Type, writeIndexSlot, "write_index");
    LLVMValueRef destElementByteOffset = LLVMBuildAdd(
        builder,
        LLVMBuildMul(builder, writeIndex, LLVMConstInt(i64Type, 8, 0), "dest_element_scaled_offset"),
        LLVMConstInt(i64Type, 8, 0),
        "dest_element_byte_offset");
    LLVMValueRef destElementByteOffsetIndex[1] = {destElementByteOffset};
    LLVMValueRef destElementAddress = LLVMBuildGEP2(
        builder, i8Type, destination, destElementByteOffsetIndex, 1, "dest_element_address");
    LLVMBuildStore(builder, elementString, destElementAddress);
    LLVMValueRef nextWriteIndex = LLVMBuildAdd(builder, writeIndex, LLVMConstInt(i64Type, 1, 0), "next_write_index");
    LLVMBuildStore(builder, nextWriteIndex, writeIndexSlot);
    LLVMBuildBr(builder, advance);

    LLVMPositionBuilderAtEnd(builder, advance);
    LLVMValueRef currentHandle = LLVMBuildLoad2(builder, pointerType, handleSlot, "current_handle");
    LLVMValueRef nextArguments[2] = {currentHandle, findData};
    LLVMValueRef hasNext = LLVMBuildCall2(
        builder, findNextFileWType, findNextFileW, nextArguments, 2, "has_next");
    LLVMValueRef hasNextBool = LLVMBuildICmp(
        builder, LLVMIntNE, hasNext, LLVMConstInt(i32Type, 0, 0), "has_next_bool");
    LLVMBuildCondBr(builder, hasNextBool, considerEntry, loopDone);

    LLVMPositionBuilderAtEnd(builder, loopDone);
    LLVMValueRef finalHandle = LLVMBuildLoad2(builder, pointerType, handleSlot, "final_handle");
    LLVMValueRef closeArguments[1] = {finalHandle};
    LLVMBuildCall2(builder, findCloseType, findClose, closeArguments, 1, "");
    LLVMValueRef finalWriteIndex = LLVMBuildLoad2(builder, i64Type, writeIndexSlot, "final_write_index");
    LLVMValueRef finalWriteIndex32 = LLVMBuildTrunc(builder, finalWriteIndex, i32Type, "final_write_index32");
    LLVMBuildRet(builder, finalWriteIndex32);

    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 54.1 "Threads" -- Thread's four runtime-side pieces. Unlike every Stage 53 round, this
// is the FIRST core operation whose Win32 resource handle (the HANDLE CreateThread returns) must
// persist ACROSS SEPARATE NativeSharp method calls (ThreadCreate returns it, then a LATER,
// separate call to ThreadJoin or ThreadDetach consumes it) rather than being fully created,
// used, and closed within the span of a single runtime wrapper call -- every prior Win32 wrapper
// in this file (File.ReadAllText, Directory.GetFiles, etc.) opened a handle and closed it before
// returning. Thread.ns stores the HANDLE across that gap as a `nint` field (`handle_`), and it
// crosses the LLVM/runtime boundary as a plain `i64` -- see ProjectMethodBodyModel.h's
// MethodCoreOperationKind::ThreadCreate/ThreadJoin/ThreadDetach comment for the full frontend
// side of this design, and Thread.ns's header comment for the complete NativeSharp-visible API.
//
// buildThreadTrampolineFunction builds `__nativesharp_thread_trampoline`, the function
// CreateThread itself calls on the new OS thread. Its signature MUST match Win32's
// `LPTHREAD_START_ROUTINE` exactly: `DWORD WINAPI ThreadProc(LPVOID lpParameter)`, i.e.
// `i32(ptr)` in this LLVM C API. It is built FIRST (before buildThreadCreate, which needs its
// LLVMValueRef to bake into CreateThread's lpStartAddress argument), and it is INTERNAL ONLY: no
// linker symbol, never exported, never imported, never NativeSharp-callable -- its address is
// captured directly in-module by buildThreadCreate below.
LLVMValueRef buildThreadTrampolineFunction(LLVMContextRef context, LLVMModuleRef module)
{
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef type = functionType(i32Type, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_thread_trampoline", type);
    if (function == nullptr)
    {
        return nullptr;
    }
    // No LLVMSetLinkage call here (this hand-maintained LlvmCApi.h wrapper does not declare
    // LLVMSetLinkage/LLVMLinkage at all, and it is not worth widening that surface for a purely
    // cosmetic change): the function keeps LLVMAddFunction's default external linkage, which is
    // harmless -- nothing outside this module ever looks it up BY NAME either way. Win32 reaches
    // it purely through the raw function-pointer VALUE CreateThread was handed (baked in
    // directly by buildThreadCreate below, in the SAME module, at LLVM-IR-construction time), not
    // through any name-based symbol lookup -- the same reason the linker symbol lists in
    // WindowsExecutableBuilder.cpp/WindowsLibraryBuilder.cpp do NOT need an entry for this
    // function: those lists are for Win32 functions this runtime IMPORTS by name, and this one is
    // neither imported nor ever looked up by name by anything.

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef instanceBlock = LLVMAppendBasicBlockInContext(context, function, "instance");
    LLVMBasicBlockRef staticBlock = LLVMAppendBasicBlockInContext(context, function, "static");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    // lpParameter IS the ThreadStart delegate's 16-byte heap cell (see buildThreadCreate below --
    // CreateThread's 4th argument is exactly the pointer ThreadCreate's own single operand
    // lowered to). The cell layout (`ptr code @ byte 0; ptr receiver @ byte 8`) is fixed by
    // ProjectOopLlvmLowerer.cpp's allocateDelegateCell: this trampoline performs the EXACT same
    // two loads and the exact same "branch on receiver != null" dispatch emitDelegateCall
    // performs for every OTHER delegate call in this compiler -- see that function's comment in
    // ProjectOopLlvmLowerer.cpp for the full rationale of that representation.
    LLVMValueRef cell = LLVMGetParam(function, 0);
    LLVMValueRef code = LLVMBuildLoad2(builder, pointerType, cell, "code");
    LLVMValueRef receiverAddressIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef receiverAddress = LLVMBuildGEP2(
        builder, i8Type, cell, receiverAddressIndex, 1, "receiver_addr");
    LLVMValueRef receiver = LLVMBuildLoad2(builder, pointerType, receiverAddress, "receiver");
    LLVMValueRef hasReceiver = LLVMBuildICmp(
        builder, LLVMIntNE, receiver, LLVMConstNull(pointerType), "has_receiver");
    LLVMBuildCondBr(builder, hasReceiver, instanceBlock, staticBlock);

    // Static shape: `code` is a plain `static void SomeMethod()` (or a captureless lambda) --
    // called with zero arguments, matching ThreadStart's own zero-argument signature. Same
    // "static" branch shape emitDelegateCall's own staticBlock uses for every other zero-
    // argument delegate call in this compiler.
    LLVMPositionBuilderAtEnd(builder, staticBlock);
    LLVMTypeRef staticCallType = functionType(voidType, {});
    // Deliberate, documented scope limitation (see the Stage 54.1 didactic .md): this is a
    // PLAIN, non-exception-aware LLVMBuildCall2 -- NOT the invoke/funclet machinery
    // buildPotentiallyThrowingCall uses for delegate calls that happen on the main thread's own
    // EH-aware call graph. An unhandled NativeSharp exception escaping ThreadStart here finds no
    // SEH handler anywhere up THIS thread's stack; Windows' default unhandled-exception behavior
    // then terminates the entire PROCESS, exactly as it would for any unhandled SEH exception on
    // any thread of any ordinary Win32/C++ program. This cannot be safely exercised by the
    // packaged example (it would kill the test process), so it is documented here and verified
    // only by reasoning about Windows' own documented behavior, not by a runnable test.
    LLVMBuildCall2(builder, staticCallType, code, nullptr, 0, "");
    LLVMBuildBr(builder, doneBlock);

    // Instance/closure shape: `receiver` is either a real instance's `this` or a lambda's heap-
    // allocated closure environment -- indistinguishable at this level, exactly as
    // emitDelegateCall's own comment already documents for every other delegate call in this
    // compiler. Either way it is passed as the callable's first (and, for zero-argument
    // ThreadStart, only) argument.
    LLVMPositionBuilderAtEnd(builder, instanceBlock);
    LLVMTypeRef instanceCallType = functionType(voidType, {pointerType});
    LLVMValueRef instanceArguments[1] = {receiver};
    LLVMBuildCall2(builder, instanceCallType, code, instanceArguments, 1, "");
    LLVMBuildBr(builder, doneBlock);

    // ThreadProc must return a DWORD -- NativeSharp's ThreadStart is `void`, so there is no real
    // exit status to report; Windows itself does not surface this value anywhere NativeSharp code
    // can observe it (GetExitCodeThread is not part of Stage 54.1's scope), so 0 is a harmless
    // constant, never inspected.
    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 0, 0));

    LLVMDisposeBuilder(builder);
    return function;
}

// buildThreadCreate: `__nativesharp_thread_create`, i64(ptr). Calls
// CreateThread(NULL, 0, trampoline, delegateCell, 0, NULL) and returns the resulting HANDLE
// reinterpreted as i64 (nint) via LLVMBuildPtrToInt directly -- unlike CreateFileA/FindFirstFileW
// above (whose failure sentinel is INVALID_HANDLE_VALUE, all-ones), Win32 defines CreateThread's
// failure return as NULL (all-ZEROS), so a failed CreateThread already becomes exactly the
// NativeSharp-visible `nint` value 0 with no extra comparison needed at all -- the SAME "any
// Win32 failure collapses to a safe/inert value" discipline every prior Stage 53 round has used,
// just free this time because 0 already IS the natural PtrToInt of a NULL HANDLE.
bool buildThreadCreate(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createThread,
    LLVMTypeRef createThreadType,
    LLVMValueRef trampolineFunction)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i64Type, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_thread_create", type);
    if (function == nullptr || trampolineFunction == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef delegateCell = LLVMGetParam(function, 0);
    // CreateThread(lpThreadAttributes=NULL (default security), dwStackSize=0 (default 1 MiB
    // reserved stack -- Stage 54.1 exposes no way to customize this), lpStartAddress=trampoline
    // (the function built just above), lpParameter=delegateCell (handed to the trampoline
    // untouched), dwCreationFlags=0 (start running immediately -- no "create suspended" API in
    // this round), lpThreadId=NULL (Thread does not expose the raw OS thread ID, only the
    // HANDLE)).
    LLVMValueRef arguments[6] = {
        LLVMConstNull(pointerType),
        LLVMConstInt(i64Type, 0, 0),
        trampolineFunction,
        delegateCell,
        LLVMConstInt(i32Type, 0, 0),
        LLVMConstNull(pointerType)};
    LLVMValueRef handle = LLVMBuildCall2(builder, createThreadType, createThread, arguments, 6, "handle");
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMBuildRet(builder, handleAsInt);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildThreadJoin: `__nativesharp_thread_join`, void(i64). A HANDLE value of 0 (the
// "CreateThread failed" case from buildThreadCreate above) is a silent no-op: neither
// WaitForSingleObject nor CloseHandle is meaningful on a HANDLE that was never a real thread
// handle to begin with, and Thread.Join()/~Thread() must never crash or hang just because an
// earlier ThreadCreate happened to fail -- the same "any Win32 failure collapses to a safe/inert
// value" discipline as buildThreadCreate. Otherwise: WaitForSingleObject(handle, INFINITE) blocks
// the CALLING thread until the target thread actually finishes running, and ONLY THEN
// CloseHandle(handle) releases the kernel object -- that order is load-bearing, not stylistic:
// closing the handle first (while the thread might still be running) would let an unrelated
// future CreateThread/CreateFileA/etc. call reuse the same numeric HANDLE value for a completely
// different kernel object while this thread is still executing, a use-after-close hazard.
bool buildThreadJoin(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef waitForSingleObject,
    LLVMTypeRef waitForSingleObjectType,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {i64Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_thread_join", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef handleAsInt = LLVMGetParam(function, 0);
    LLVMValueRef isNullHandle = LLVMBuildICmp(
        builder, LLVMIntEQ, handleAsInt, LLVMConstInt(i64Type, 0, 0), "is_null_handle");
    LLVMBuildCondBr(builder, isNullHandle, doneBlock, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef handle = LLVMBuildIntToPtr(builder, handleAsInt, pointerType, "handle");
    // INFINITE = 0xFFFFFFFF -- Stage 54.1 only provides the unconditional "wait as long as it
    // takes" shape (C++ std::thread::join()'s equivalent), no timed-wait overload yet.
    LLVMValueRef waitArguments[2] = {handle, LLVMConstInt(i32Type, 0xFFFFFFFFu, 0)};
    LLVMBuildCall2(builder, waitForSingleObjectType, waitForSingleObject, waitArguments, 2, "");
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    LLVMBuildBr(builder, doneBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildThreadDetach: `__nativesharp_thread_detach`, void(i64). Same null-handle guard as
// buildThreadJoin above (a HANDLE of 0 is a silent no-op). Otherwise: just CloseHandle(handle) --
// no WaitForSingleObject. This is a REAL Win32 fact, not a simplification this compiler is
// choosing: Windows has no distinct pthread-style "detach" syscall. A thread keeps running after
// its HANDLE is closed, because the handle only names the kernel object that tracks the thread's
// lifetime bookkeeping (its exit code, whether anything is still waiting on it) -- it is not what
// keeps the thread's code executing. Closing it merely means NativeSharp gives up the ability to
// ever Join() or otherwise observe that thread again; the thread itself runs to completion (or
// forever) on its own.
bool buildThreadDetach(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {i64Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_thread_detach", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef handleAsInt = LLVMGetParam(function, 0);
    LLVMValueRef isNullHandle = LLVMBuildICmp(
        builder, LLVMIntEQ, handleAsInt, LLVMConstInt(i64Type, 0, 0), "is_null_handle");
    LLVMBuildCondBr(builder, isNullHandle, doneBlock, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef handle = LLVMBuildIntToPtr(builder, handleAsInt, pointerType, "handle");
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    LLVMBuildBr(builder, doneBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 54.3 "Mutex". buildMutexCreate: `__nativesharp_mutex_create`, i64() -- the FIRST
// zero-parameter runtime hook in this entire project. Calls CreateMutexA(NULL, FALSE, NULL): NULL
// security attributes (default), bInitialOwner=FALSE (the mutex starts UNOWNED -- Mutex.ns never
// exposes a way to create one already locked), NULL name (an ANONYMOUS/unnamed mutex -- this round
// has no need for cross-process named mutexes, only intra-process mutual exclusion between
// NativeSharp threads). Same "any Win32 failure collapses to a safe/inert value" discipline as
// buildThreadCreate: Win32 defines CreateMutexA's failure return as NULL, which LLVMBuildPtrToInt
// already turns into exactly the NativeSharp-visible `nint` value 0 with no extra comparison
// needed -- MutexLock/MutexUnlock/MutexClose below all treat a HANDLE of 0 as a silent no-op,
// exactly like ThreadJoin/ThreadDetach already do for a failed ThreadCreate.
bool buildMutexCreate(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createMutexA,
    LLVMTypeRef createMutexAType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_mutex_create", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef arguments[3] = {
        LLVMConstNull(pointerType),
        LLVMConstInt(i32Type, 0, 0),
        LLVMConstNull(pointerType)};
    LLVMValueRef handle = LLVMBuildCall2(builder, createMutexAType, createMutexA, arguments, 3, "handle");
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMBuildRet(builder, handleAsInt);

    LLVMDisposeBuilder(builder);
    return true;
}

// Item 370. Named mutexes use the same kernel object as MutexCreate, but pass a stable ANSI
// object name so independently started processes can rendezvous on it.
bool buildMutexCreateNamed(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createMutexA,
    LLVMTypeRef createMutexAType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i64Type, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_mutex_create_named", type);
    if (function == nullptr) return false;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef nameBuffer = buildNullTerminatedCopy(
        context, builder, function, LLVMGetParam(function, 0));
    LLVMValueRef arguments[3] = {
        LLVMConstNull(pointerType), LLVMConstInt(i32Type, 0, 0), nameBuffer};
    LLVMValueRef handle = LLVMBuildCall2(
        builder, createMutexAType, createMutexA, arguments, 3, "handle");
    LLVMBuildRet(builder, LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int"));
    LLVMDisposeBuilder(builder);
    return true;
}

// buildMutexLock: `__nativesharp_mutex_lock`, void(i64). A HANDLE value of 0 (the "CreateMutexA
// failed" case from buildMutexCreate above) is a silent no-op -- WaitForSingleObject on a HANDLE
// that was never a real mutex to begin with is meaningless, and Mutex.Lock() must never crash just
// because an earlier MutexCreate happened to fail. Otherwise: WaitForSingleObject(handle, INFINITE)
// -- the EXACT same runtime hook Stage 54.1's buildThreadJoin already declares and uses (see
// buildInternal's wiring below: this round reuses that function/type pair directly instead of
// declaring a second copy). A real kernel Mutex (unlike a bare CRITICAL_SECTION) is RECURSIVELY
// acquirable by the thread that already owns it -- calling Lock() twice from the same thread
// succeeds both times (Win32 increments an internal recursion count), and each acquisition needs a
// matching Unlock() to fully release it. This is a genuine Win32 behavior this compiler relies on,
// not a simplification: Stage 54.3's packaged example exercises this directly (see Main.ns Section
// 2).
bool buildMutexLock(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef waitForSingleObject,
    LLVMTypeRef waitForSingleObjectType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {i64Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_mutex_lock", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef handleAsInt = LLVMGetParam(function, 0);
    LLVMValueRef isNullHandle = LLVMBuildICmp(
        builder, LLVMIntEQ, handleAsInt, LLVMConstInt(i64Type, 0, 0), "is_null_handle");
    LLVMBuildCondBr(builder, isNullHandle, doneBlock, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef handle = LLVMBuildIntToPtr(builder, handleAsInt, pointerType, "handle");
    // INFINITE = 0xFFFFFFFF -- Stage 54.3 only provides the unconditional "block until acquired"
    // shape, no timed-lock overload yet, same limitation Stage 54.1's Join() already documents.
    LLVMValueRef waitArguments[2] = {handle, LLVMConstInt(i32Type, 0xFFFFFFFFu, 0)};
    LLVMBuildCall2(builder, waitForSingleObjectType, waitForSingleObject, waitArguments, 2, "");
    LLVMBuildBr(builder, doneBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildMutexUnlock: `__nativesharp_mutex_unlock`, void(i64). Same null-handle guard as
// buildMutexLock above. Otherwise: ReleaseMutex(handle) -- its BOOL return is deliberately
// DISCARDED, same "unchecked failure" discipline as CloseHandle/DeleteFileA/etc. elsewhere in this
// runtime. This is significant for one specific corner case Stage 54.3's rigor rule requires
// covering: calling Unlock() on a mutex the CALLING THREAD never actually locked (a
// "never-acquired release") makes ReleaseMutex fail (it sets the last error to
// ERROR_NOT_OWNER and returns FALSE) -- but Win32 does NOT crash or raise any structured exception
// for this, it is an ordinary recoverable failure. Discarding the return value here means
// Mutex.Unlock() on an unowned mutex is a safe, silent no-op at the NativeSharp level too -- see
// Main.ns Section 3 for the packaged verification of this exact case.
bool buildMutexUnlock(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef releaseMutex,
    LLVMTypeRef releaseMutexType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {i64Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_mutex_unlock", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef handleAsInt = LLVMGetParam(function, 0);
    LLVMValueRef isNullHandle = LLVMBuildICmp(
        builder, LLVMIntEQ, handleAsInt, LLVMConstInt(i64Type, 0, 0), "is_null_handle");
    LLVMBuildCondBr(builder, isNullHandle, doneBlock, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef handle = LLVMBuildIntToPtr(builder, handleAsInt, pointerType, "handle");
    LLVMValueRef releaseArguments[1] = {handle};
    LLVMBuildCall2(builder, releaseMutexType, releaseMutex, releaseArguments, 1, "");
    LLVMBuildBr(builder, doneBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildMutexClose: `__nativesharp_mutex_close`, void(i64). Same null-handle guard as the two
// functions above; otherwise just CloseHandle(handle) -- the SAME runtime hook Stage 53.3b's
// buildFileReadAllText/etc. already declare (reused directly, see buildInternal's wiring below,
// same "reuse the existing CloseHandle declaration" discipline buildThreadJoin/buildThreadDetach
// already established for Thread's HANDLE). Mutex has no RAII-driven "must run" guarantee the way
// Thread's auto-Join() does -- a Mutex is typically a long-lived STATIC field shared across many
// threads for the whole life of the program, not a short-lived `raii`-scoped local the way each
// Thread in Stage 54.1's examples is, so this destructor path mostly matters for the
// `raii`-scoped-local usage Main.ns Section 4 exercises, not for the shared-counter sections.
bool buildMutexClose(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {i64Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_mutex_close", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef handleAsInt = LLVMGetParam(function, 0);
    LLVMValueRef isNullHandle = LLVMBuildICmp(
        builder, LLVMIntEQ, handleAsInt, LLVMConstInt(i64Type, 0, 0), "is_null_handle");
    LLVMBuildCondBr(builder, isNullHandle, doneBlock, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef handle = LLVMBuildIntToPtr(builder, handleAsInt, pointerType, "handle");
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    LLVMBuildBr(builder, doneBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 54.3 "Mutex" (fix). buildGetCurrentThreadId: `__nativesharp_get_current_thread_id`,
// i32() -- a trivial pass-through wrapper over Win32's GetCurrentThreadId(). NOT a NativeSharp-
// visible core operation (no NativeSharp source syntax reaches this -- there is no
// `Thread.CurrentId` or similar exposed anywhere in this round): it exists purely as an internal
// implementation detail the COMPILER's own generated code uses, specifically the type-initializer
// ("cctor") guard in ProjectOopLlvmLowerer.cpp's defineTypeInitializers, to distinguish "a
// DIFFERENT thread is currently running this type's static field initializers, I must wait" from
// "I am recursively re-entering my OWN type's initializer (e.g. a self-referencing static field
// initializer like `static int B = Foo.A + 1;`), I must proceed without waiting on myself" -- see
// that function's own header comment for the full guard design this thread-id hook enables.
bool buildGetCurrentThreadId(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getCurrentThreadId,
    LLVMTypeRef getCurrentThreadIdType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef type = functionType(i32Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_get_current_thread_id", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef tid = LLVMBuildCall2(builder, getCurrentThreadIdType, getCurrentThreadId, nullptr, 0, "tid");
    LLVMBuildRet(builder, tid);

    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 54.4 "Condition Variables". buildMonitorEnter/buildMonitorExit/buildMonitorPulse/
// buildMonitorPulseAll: four trivial single-pointer-argument wrappers over
// AcquireSRWLockExclusive/ReleaseSRWLockExclusive/WakeConditionVariable/WakeAllConditionVariable.
// Unlike every Mutex wrapper above, NONE of these four needs a null-handle guard: the pointer
// argument is always the address of a REAL field inside a REAL, already-null-checked receiver
// object (ProjectOopLlvmLowerer.cpp's checkedReceiver already traps on a null Monitor before this
// wrapper is ever called) -- there is no equivalent of "CreateMutexA returned NULL" failure mode
// here at all, since Monitor's two backing fields (`lock_`, `cv_`) need no creation syscall (see
// MethodCoreOperationKind::MonitorEnter's comment in ProjectMethodBodyModel.h for why: both are
// safely zero-initialized by construction, no Initialize* call needed).
bool buildMonitorEnter(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef acquireSRWLockExclusive,
    LLVMTypeRef acquireSRWLockExclusiveType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_monitor_enter", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef lockPointer = LLVMGetParam(function, 0);
    LLVMValueRef arguments[1] = {lockPointer};
    LLVMBuildCall2(builder, acquireSRWLockExclusiveType, acquireSRWLockExclusive, arguments, 1, "");
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildMonitorExit: `__nativesharp_monitor_exit`, void(ptr) over ReleaseSRWLockExclusive. Same
// shape as buildMonitorEnter above -- see its header comment for why no null-handle guard exists.
// NOTE, documented here rather than repeated at every call site: calling Exit() without a matching
// prior Enter() on the SAME thread is undefined behavior at the Win32 level (SRWLOCK has no
// "unowned release" recoverable-failure return the way a kernel Mutex's ReleaseMutex does) -- this
// is a real usage contract, not a NativeSharp simplification, documented in Monitor.ns.
bool buildMonitorExit(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef releaseSRWLockExclusive,
    LLVMTypeRef releaseSRWLockExclusiveType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_monitor_exit", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef lockPointer = LLVMGetParam(function, 0);
    LLVMValueRef arguments[1] = {lockPointer};
    LLVMBuildCall2(builder, releaseSRWLockExclusiveType, releaseSRWLockExclusive, arguments, 1, "");
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildMonitorPulse: `__nativesharp_monitor_pulse`, void(ptr) over WakeConditionVariable -- wakes
// ONE thread blocked in Wait() on this condition variable (if any are blocked at all; waking a
// condition variable nobody is waiting on is a well-defined, harmless no-op per Win32).
bool buildMonitorPulse(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef wakeConditionVariable,
    LLVMTypeRef wakeConditionVariableType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_monitor_pulse", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef cvPointer = LLVMGetParam(function, 0);
    LLVMValueRef arguments[1] = {cvPointer};
    LLVMBuildCall2(builder, wakeConditionVariableType, wakeConditionVariable, arguments, 1, "");
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildMonitorPulseAll: `__nativesharp_monitor_pulse_all`, void(ptr) over
// WakeAllConditionVariable -- wakes EVERY thread currently blocked in Wait() on this condition
// variable. Same no-blocked-waiters-is-a-no-op guarantee as buildMonitorPulse above.
bool buildMonitorPulseAll(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef wakeAllConditionVariable,
    LLVMTypeRef wakeAllConditionVariableType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_monitor_pulse_all", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef cvPointer = LLVMGetParam(function, 0);
    LLVMValueRef arguments[1] = {cvPointer};
    LLVMBuildCall2(builder, wakeAllConditionVariableType, wakeAllConditionVariable, arguments, 1, "");
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildMonitorWait: `__nativesharp_monitor_wait`, i1(ptr cv, ptr lock, i32 dwMilliseconds) over
// SleepConditionVariableSRW(PCONDITION_VARIABLE, PSRWLOCK, DWORD, ULONG Flags) -- Flags is always
// 0 (this round exposes no way to request the SRWLOCK be re-acquired in SHARED mode after waking,
// only the EXCLUSIVE mode Enter()/Exit() already use throughout). Win32's BOOL result (TRUE if
// woken -- by Pulse()/PulseAll(), OR spuriously per Win32's own documented contract, see
// Monitor.ns; FALSE only on real failure, which in practice means the timeout expired) is
// forwarded directly as NativeSharp's builtin:bool -- no extra logic needed, unlike
// buildWaitForHandleTimeout below, which needs its OWN null-handle guard (Monitor's receiver is
// already guaranteed non-null upstream, see buildMonitorEnter's header comment; Semaphore/Event's
// HANDLE can legitimately be 0 if their own Create call failed).
bool buildMonitorWait(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef sleepConditionVariableSRW,
    LLVMTypeRef sleepConditionVariableSRWType)
{
    LLVMTypeRef i1Type = LLVMInt1TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i1Type, {pointerType, pointerType, i32Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_monitor_wait", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef cvPointer = LLVMGetParam(function, 0);
    LLVMValueRef lockPointer = LLVMGetParam(function, 1);
    LLVMValueRef milliseconds = LLVMGetParam(function, 2);
    LLVMValueRef flags = LLVMConstInt(i32Type, 0, 0);
    LLVMValueRef arguments[4] = {cvPointer, lockPointer, milliseconds, flags};
    LLVMValueRef woken = LLVMBuildCall2(
        builder, sleepConditionVariableSRWType, sleepConditionVariableSRW, arguments, 4, "woken");
    LLVMValueRef wokenBool = LLVMBuildICmp(
        builder, LLVMIntNE, woken, LLVMConstInt(i32Type, 0, 0), "woken_bool");
    LLVMBuildRet(builder, wokenBool);

    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 54.5 "Otras primitivas de sincronizacion". buildWaitForHandleTimeout:
// `__nativesharp_wait_for_handle_timeout`, i1(i64 handleAsInt, i32 dwMilliseconds) -- a SHARED
// runtime hook both Semaphore.Wait and Event.Wait's LLVM lowering call directly (see
// ProjectOopLlvmLowerer.cpp's CoreSemaphoreWait/CoreEventWait cases), the first genuinely shared
// wrapper function two DIFFERENT MethodCoreOperationKind values ever call in this compiler.
// Justified because the underlying operation is IDENTICAL for both: WaitForSingleObject(handle,
// ms), true iff the result is WAIT_OBJECT_0 (0) -- a timeout (WAIT_TIMEOUT, 0x102) or any other
// failure (WAIT_FAILED, 0xFFFFFFFF) both collapse to false, matching this round's "timeouts must
// be observable, not swallowed into a crash or a hang" rigor requirement. Same null-handle guard
// discipline as buildMutexLock (a HANDLE of 0 means an earlier CreateSemaphoreA/CreateEventA
// failed) -- but unlike buildMutexLock (which just no-ops, since Lock() returns void), this
// wrapper must still produce a well-defined `false` in that case: a HANDLE that never existed was
// obviously never "acquired".
bool buildWaitForHandleTimeout(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef waitForSingleObject,
    LLVMTypeRef waitForSingleObjectType)
{
    LLVMTypeRef i1Type = LLVMInt1TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i1Type, {i64Type, i32Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_wait_for_handle_timeout", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef handleAsInt = LLVMGetParam(function, 0);
    LLVMValueRef milliseconds = LLVMGetParam(function, 1);
    LLVMValueRef resultSlot = LLVMBuildAlloca(builder, i1Type, "acquired");
    LLVMBuildStore(builder, LLVMConstInt(i1Type, 0, 0), resultSlot);
    LLVMValueRef isNullHandle = LLVMBuildICmp(
        builder, LLVMIntEQ, handleAsInt, LLVMConstInt(i64Type, 0, 0), "is_null_handle");
    LLVMBuildCondBr(builder, isNullHandle, doneBlock, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef handle = LLVMBuildIntToPtr(builder, handleAsInt, pointerType, "handle");
    LLVMValueRef waitArguments[2] = {handle, milliseconds};
    LLVMValueRef waitResult = LLVMBuildCall2(
        builder, waitForSingleObjectType, waitForSingleObject, waitArguments, 2, "wait_result");
    LLVMValueRef acquired = LLVMBuildICmp(
        builder, LLVMIntEQ, waitResult, LLVMConstInt(i32Type, 0, 0), "acquired");
    LLVMBuildStore(builder, acquired, resultSlot);
    LLVMBuildBr(builder, doneBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMValueRef finalResult = LLVMBuildLoad2(builder, i1Type, resultSlot, "final_result");
    LLVMBuildRet(builder, finalResult);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildSemaphoreCreate: `__nativesharp_semaphore_create`, i64(i32 initialCount, i32 maximumCount)
// over CreateSemaphoreA(NULL, lInitialCount, lMaximumCount, NULL) -- NULL security attributes and
// NULL name (an anonymous/intra-process-only semaphore), same "no cross-process named kernel
// objects needed this round" choice buildMutexCreate already made. NativeSharp's `int` and Win32's
// LONG are both 32-bit signed -- both parameters pass straight through with no conversion. Same
// "any Win32 failure collapses to a safe/inert HANDLE of 0" discipline as buildMutexCreate/
// buildThreadCreate -- Semaphore.ns documents what a 0 handle means for its Wait/Release/Close.
bool buildSemaphoreCreate(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createSemaphoreA,
    LLVMTypeRef createSemaphoreAType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i64Type, {i32Type, i32Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_semaphore_create", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef initialCount = LLVMGetParam(function, 0);
    LLVMValueRef maximumCount = LLVMGetParam(function, 1);
    LLVMValueRef arguments[4] = {
        LLVMConstNull(pointerType),
        initialCount,
        maximumCount,
        LLVMConstNull(pointerType)};
    LLVMValueRef handle = LLVMBuildCall2(builder, createSemaphoreAType, createSemaphoreA, arguments, 4, "handle");
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMBuildRet(builder, handleAsInt);

    LLVMDisposeBuilder(builder);
    return true;
}

// Item 370. A named semaphore is visible to every process in the same Windows object namespace.
bool buildSemaphoreCreateNamed(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createSemaphoreA,
    LLVMTypeRef createSemaphoreAType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i64Type, {i32Type, i32Type, pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_semaphore_create_named", type);
    if (function == nullptr) return false;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef nameBuffer = buildNullTerminatedCopy(
        context, builder, function, LLVMGetParam(function, 2));
    LLVMValueRef arguments[4] = {
        LLVMConstNull(pointerType), LLVMGetParam(function, 0), LLVMGetParam(function, 1), nameBuffer};
    LLVMValueRef handle = LLVMBuildCall2(
        builder, createSemaphoreAType, createSemaphoreA, arguments, 4, "handle");
    LLVMBuildRet(builder, LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int"));
    LLVMDisposeBuilder(builder);
    return true;
}

// buildSemaphoreRelease: `__nativesharp_semaphore_release`, void(i64) over ReleaseSemaphore(handle,
// 1, NULL) -- always releases exactly ONE count (Semaphore.ns documents why a variable-count
// Release(int) overload is deliberately out of scope this round: it is a strict superset that adds
// no new compiler mechanism, just API surface, left for a future round if Alberto asks for it).
// lpPreviousCount is NULL (discarded) -- same "unchecked BOOL failure" discipline as
// buildMutexUnlock: releasing past the semaphore's maximum count fails safely (ReleaseSemaphore
// returns FALSE, the count is left unchanged) rather than corrupting state or crashing. Same
// null-handle guard as buildMutexUnlock/buildMutexClose.
bool buildSemaphoreRelease(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef releaseSemaphore,
    LLVMTypeRef releaseSemaphoreType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {i64Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_semaphore_release", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef handleAsInt = LLVMGetParam(function, 0);
    LLVMValueRef isNullHandle = LLVMBuildICmp(
        builder, LLVMIntEQ, handleAsInt, LLVMConstInt(i64Type, 0, 0), "is_null_handle");
    LLVMBuildCondBr(builder, isNullHandle, doneBlock, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef handle = LLVMBuildIntToPtr(builder, handleAsInt, pointerType, "handle");
    LLVMValueRef releaseArguments[3] = {
        handle, LLVMConstInt(i32Type, 1, 0), LLVMConstNull(pointerType)};
    LLVMBuildCall2(builder, releaseSemaphoreType, releaseSemaphore, releaseArguments, 3, "");
    LLVMBuildBr(builder, doneBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildSemaphoreClose: `__nativesharp_semaphore_close`, void(i64) -- CloseHandle(handle), reusing
// the SAME `closeHandle`/`closeHandleType` declaration Stage 53.3b/54.3 already established (no
// new import). Same null-handle guard as buildMutexClose.
bool buildSemaphoreClose(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {i64Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_semaphore_close", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef handleAsInt = LLVMGetParam(function, 0);
    LLVMValueRef isNullHandle = LLVMBuildICmp(
        builder, LLVMIntEQ, handleAsInt, LLVMConstInt(i64Type, 0, 0), "is_null_handle");
    LLVMBuildCondBr(builder, isNullHandle, doneBlock, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef handle = LLVMBuildIntToPtr(builder, handleAsInt, pointerType, "handle");
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    LLVMBuildBr(builder, doneBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildEventCreate: `__nativesharp_event_create`, i64(i1 manualReset, i1 initialState) over
// CreateEventA(NULL, bManualReset, bInitialState, NULL) -- NULL security attributes and NULL name
// (anonymous, same choice as Mutex/Semaphore above). NativeSharp's builtin:bool lowers to LLVM i1
// everywhere in this compiler (see buildFileExists's i1 return, Stage 51) -- both parameters are
// zero-extended to Win32's 32-bit BOOL before the real call. manualReset=true means the event
// stays signaled until Reset() is called explicitly (every waiter wakes); manualReset=false means
// Win32 automatically resets it back to non-signaled the instant ONE waiter is released (only ONE
// waiter wakes per Set() -- see Event.ns for the full contract and Stage54_5SyncPrimitivesTests.cpp
// for a packaged test distinguishing the two). Same "any Win32 failure collapses to a safe/inert
// HANDLE of 0" discipline as every other Create* wrapper in this file.
bool buildEventCreate(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createEventA,
    LLVMTypeRef createEventAType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i1Type = LLVMInt1TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i64Type, {i1Type, i1Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_event_create", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef manualResetI1 = LLVMGetParam(function, 0);
    LLVMValueRef initialStateI1 = LLVMGetParam(function, 1);
    LLVMValueRef manualResetI32 = LLVMBuildZExt(builder, manualResetI1, i32Type, "manual_reset_i32");
    LLVMValueRef initialStateI32 = LLVMBuildZExt(builder, initialStateI1, i32Type, "initial_state_i32");
    LLVMValueRef arguments[4] = {
        LLVMConstNull(pointerType),
        manualResetI32,
        initialStateI32,
        LLVMConstNull(pointerType)};
    LLVMValueRef handle = LLVMBuildCall2(builder, createEventAType, createEventA, arguments, 4, "handle");
    LLVMValueRef handleAsInt = LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int");
    LLVMBuildRet(builder, handleAsInt);

    LLVMDisposeBuilder(builder);
    return true;
}

// Item 370. Named events preserve Win32's first-creator semantics for reset mode and state.
bool buildEventCreateNamed(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef createEventA,
    LLVMTypeRef createEventAType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i1Type = LLVMInt1TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i64Type, {i1Type, i1Type, pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_event_create_named", type);
    if (function == nullptr) return false;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef nameBuffer = buildNullTerminatedCopy(
        context, builder, function, LLVMGetParam(function, 2));
    LLVMValueRef manualReset = LLVMBuildZExt(
        builder, LLVMGetParam(function, 0), i32Type, "manual_reset_i32");
    LLVMValueRef initialState = LLVMBuildZExt(
        builder, LLVMGetParam(function, 1), i32Type, "initial_state_i32");
    LLVMValueRef arguments[4] = {
        LLVMConstNull(pointerType), manualReset, initialState, nameBuffer};
    LLVMValueRef handle = LLVMBuildCall2(
        builder, createEventAType, createEventA, arguments, 4, "handle");
    LLVMBuildRet(builder, LLVMBuildPtrToInt(builder, handle, i64Type, "handle_as_int"));
    LLVMDisposeBuilder(builder);
    return true;
}

// buildEventSet: `__nativesharp_event_set`, void(i64) over SetEvent(handle) -- signals the event.
// BOOL result discarded (same "unchecked failure" discipline as ReleaseMutex/ReleaseSemaphore
// above). Same null-handle guard as buildMutexUnlock.
bool buildEventSet(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef setEvent,
    LLVMTypeRef setEventType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {i64Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_event_set", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef handleAsInt = LLVMGetParam(function, 0);
    LLVMValueRef isNullHandle = LLVMBuildICmp(
        builder, LLVMIntEQ, handleAsInt, LLVMConstInt(i64Type, 0, 0), "is_null_handle");
    LLVMBuildCondBr(builder, isNullHandle, doneBlock, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef handle = LLVMBuildIntToPtr(builder, handleAsInt, pointerType, "handle");
    LLVMValueRef setArguments[1] = {handle};
    LLVMBuildCall2(builder, setEventType, setEvent, setArguments, 1, "");
    LLVMBuildBr(builder, doneBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildEventReset: `__nativesharp_event_reset`, void(i64) over ResetEvent(handle) -- returns a
// manual-reset event to non-signaled explicitly. Meaningless (but harmless -- Win32 defines it as
// a safe no-op) on an auto-reset event, since Win32 already resets those automatically on release;
// Event.ns documents this. Same null-handle guard and discarded-BOOL discipline as buildEventSet.
bool buildEventReset(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef resetEvent,
    LLVMTypeRef resetEventType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {i64Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_event_reset", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef handleAsInt = LLVMGetParam(function, 0);
    LLVMValueRef isNullHandle = LLVMBuildICmp(
        builder, LLVMIntEQ, handleAsInt, LLVMConstInt(i64Type, 0, 0), "is_null_handle");
    LLVMBuildCondBr(builder, isNullHandle, doneBlock, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef handle = LLVMBuildIntToPtr(builder, handleAsInt, pointerType, "handle");
    LLVMValueRef resetArguments[1] = {handle};
    LLVMBuildCall2(builder, resetEventType, resetEvent, resetArguments, 1, "");
    LLVMBuildBr(builder, doneBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// buildEventClose: `__nativesharp_event_close`, void(i64) -- CloseHandle(handle), reusing the same
// shared `closeHandle`/`closeHandleType` declaration every other Close wrapper in this file uses.
// Same null-handle guard.
bool buildEventClose(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef closeHandle,
    LLVMTypeRef closeHandleType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {i64Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_event_close", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef haveHandle = LLVMAppendBasicBlockInContext(context, function, "have_handle");
    LLVMBasicBlockRef doneBlock = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef handleAsInt = LLVMGetParam(function, 0);
    LLVMValueRef isNullHandle = LLVMBuildICmp(
        builder, LLVMIntEQ, handleAsInt, LLVMConstInt(i64Type, 0, 0), "is_null_handle");
    LLVMBuildCondBr(builder, isNullHandle, doneBlock, haveHandle);

    LLVMPositionBuilderAtEnd(builder, haveHandle);
    LLVMValueRef handle = LLVMBuildIntToPtr(builder, handleAsInt, pointerType, "handle");
    LLVMValueRef closeArguments[1] = {handle};
    LLVMBuildCall2(builder, closeHandleType, closeHandle, closeArguments, 1, "");
    LLVMBuildBr(builder, doneBlock);

    LLVMPositionBuilderAtEnd(builder, doneBlock);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 55.1 "Process". All five getters obtain the CURRENT process HANDLE internally via
// GetCurrentProcess() -- NativeSharp never sees a process HANDLE the way it does for Mutex/
// Semaphore/Event's Win32 kernel objects, since there is exactly one process this runtime ever
// addresses. GetCurrentProcess() always returns the same pseudo-handle ((HANDLE)-1) by Win32
// contract, so calling it fresh inside every wrapper that needs it (rather than caching it in a
// global) costs nothing and keeps every wrapper trivially self-contained.
bool buildProcessGetCurrentProcessId(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getCurrentProcessId,
    LLVMTypeRef getCurrentProcessIdType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_process_get_current_process_id", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef pid = LLVMBuildCall2(builder, getCurrentProcessIdType, getCurrentProcessId, nullptr, 0, "pid");
    LLVMValueRef pid64 = LLVMBuildZExt(builder, pid, i64Type, "pid64");
    LLVMBuildRet(builder, pid64);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildProcessGetWorkingSetSize(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getCurrentProcess,
    LLVMTypeRef getCurrentProcessType,
    LLVMValueRef getProcessMemoryInfo,
    LLVMTypeRef getProcessMemoryInfoType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_process_get_working_set_size", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    // PROCESS_MEMORY_COUNTERS is 72 bytes on x64: DWORD cb; DWORD PageFaultCount; then eight
    // SIZE_T fields, WorkingSetSize being the second one (byte offset 16). cb must be set to the
    // struct's own size before the call -- K32GetProcessMemoryInfo uses it to bound-check.
    LLVMValueRef counters = LLVMBuildArrayAlloca(
        builder, i8Type, LLVMConstInt(i64Type, 72, 0), "process_memory_counters");
    LLVMValueRef cbIndex[1] = {LLVMConstInt(i64Type, 0, 0)};
    LLVMValueRef cbPointer = LLVMBuildGEP2(builder, i8Type, counters, cbIndex, 1, "cb_ptr");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 72, 0), cbPointer);
    LLVMValueRef handle = LLVMBuildCall2(builder, getCurrentProcessType, getCurrentProcess, nullptr, 0, "handle");
    LLVMValueRef getInfoArguments[3] = {handle, counters, LLVMConstInt(i32Type, 72, 0)};
    LLVMBuildCall2(builder, getProcessMemoryInfoType, getProcessMemoryInfo, getInfoArguments, 3, "");
    LLVMValueRef workingSetIndex[1] = {LLVMConstInt(i64Type, 16, 0)};
    LLVMValueRef workingSetPointer = LLVMBuildGEP2(builder, i8Type, counters, workingSetIndex, 1, "working_set_ptr");
    LLVMValueRef workingSetSize = LLVMBuildLoad2(builder, i64Type, workingSetPointer, "working_set_size");
    LLVMBuildRet(builder, workingSetSize);
    LLVMDisposeBuilder(builder);
    return true;
}

// GetProcessTimes requires four separate FILETIME* out-parameters (creation/exit/kernel/user),
// each 8 bytes -- Win32 does not allow any of the four to be omitted, so both wrappers below
// allocate all four (even though each one only reads one slot back) rather than sharing state
// across two independent runtime functions.
bool buildProcessGetKernelTime(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getCurrentProcess,
    LLVMTypeRef getCurrentProcessType,
    LLVMValueRef getProcessTimes,
    LLVMTypeRef getProcessTimesType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_process_get_kernel_time", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef creationTime = LLVMBuildAlloca(builder, i64Type, "creation_time");
    LLVMValueRef exitTime = LLVMBuildAlloca(builder, i64Type, "exit_time");
    LLVMValueRef kernelTime = LLVMBuildAlloca(builder, i64Type, "kernel_time");
    LLVMValueRef userTime = LLVMBuildAlloca(builder, i64Type, "user_time");
    LLVMValueRef handle = LLVMBuildCall2(builder, getCurrentProcessType, getCurrentProcess, nullptr, 0, "handle");
    LLVMValueRef arguments[5] = {handle, creationTime, exitTime, kernelTime, userTime};
    LLVMBuildCall2(builder, getProcessTimesType, getProcessTimes, arguments, 5, "");
    LLVMValueRef result = LLVMBuildLoad2(builder, i64Type, kernelTime, "kernel_time_value");
    LLVMBuildRet(builder, result);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildProcessGetUserTime(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getCurrentProcess,
    LLVMTypeRef getCurrentProcessType,
    LLVMValueRef getProcessTimes,
    LLVMTypeRef getProcessTimesType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_process_get_user_time", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef creationTime = LLVMBuildAlloca(builder, i64Type, "creation_time");
    LLVMValueRef exitTime = LLVMBuildAlloca(builder, i64Type, "exit_time");
    LLVMValueRef kernelTime = LLVMBuildAlloca(builder, i64Type, "kernel_time");
    LLVMValueRef userTime = LLVMBuildAlloca(builder, i64Type, "user_time");
    LLVMValueRef handle = LLVMBuildCall2(builder, getCurrentProcessType, getCurrentProcess, nullptr, 0, "handle");
    LLVMValueRef arguments[5] = {handle, creationTime, exitTime, kernelTime, userTime};
    LLVMBuildCall2(builder, getProcessTimesType, getProcessTimes, arguments, 5, "");
    LLVMValueRef result = LLVMBuildLoad2(builder, i64Type, userTime, "user_time_value");
    LLVMBuildRet(builder, result);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildProcessGetPriorityClass(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getCurrentProcess,
    LLVMTypeRef getCurrentProcessType,
    LLVMValueRef getPriorityClass,
    LLVMTypeRef getPriorityClassType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_process_get_priority_class", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef handle = LLVMBuildCall2(builder, getCurrentProcessType, getCurrentProcess, nullptr, 0, "handle");
    LLVMValueRef handleArguments[1] = {handle};
    LLVMValueRef priorityClass = LLVMBuildCall2(
        builder, getPriorityClassType, getPriorityClass, handleArguments, 1, "priority_class");
    LLVMValueRef priorityClass64 = LLVMBuildZExt(builder, priorityClass, i64Type, "priority_class64");
    LLVMBuildRet(builder, priorityClass64);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildProcessSetPriorityClass(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getCurrentProcess,
    LLVMTypeRef getCurrentProcessType,
    LLVMValueRef setPriorityClass,
    LLVMTypeRef setPriorityClassType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef type = functionType(voidType, {i32Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_process_set_priority_class", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef handle = LLVMBuildCall2(builder, getCurrentProcessType, getCurrentProcess, nullptr, 0, "handle");
    LLVMValueRef priorityClass = LLVMGetParam(function, 0);
    LLVMValueRef arguments[2] = {handle, priorityClass};
    LLVMBuildCall2(builder, setPriorityClassType, setPriorityClass, arguments, 2, "");
    // SetPriorityClass's BOOL return is discarded -- same documented rationale as
    // SetEnvironmentVariableA above: an invalid priority-class constant or insufficient privilege
    // is not something Stage 55.1's scope surfaces as a NativeSharp-visible error yet.
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 55.2 "Hardware / System Info". GetSystemInfo fills a 48-byte SYSTEM_INFO struct with no
// return value; each getter below calls it fresh -- SYSTEM_INFO never changes at runtime on a
// given machine, so there is no correctness cost to re-querying it every time, and it keeps every
// wrapper independent (same trade-off buildProcessGetKernelTime/UserTime make above).
bool buildSystemInfoGetPageSize(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getSystemInfo,
    LLVMTypeRef getSystemInfoType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_system_info_get_page_size", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef info = LLVMBuildArrayAlloca(builder, i8Type, LLVMConstInt(i64Type, 48, 0), "system_info");
    LLVMValueRef arguments[1] = {info};
    LLVMBuildCall2(builder, getSystemInfoType, getSystemInfo, arguments, 1, "");
    // dwPageSize is a DWORD at byte offset 4 of SYSTEM_INFO.
    LLVMValueRef pageSizeIndex[1] = {LLVMConstInt(i64Type, 4, 0)};
    LLVMValueRef pageSizePointer = LLVMBuildGEP2(builder, i8Type, info, pageSizeIndex, 1, "page_size_ptr");
    LLVMValueRef pageSize = LLVMBuildLoad2(builder, i32Type, pageSizePointer, "page_size");
    LLVMValueRef pageSize64 = LLVMBuildZExt(builder, pageSize, i64Type, "page_size64");
    LLVMBuildRet(builder, pageSize64);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildSystemInfoGetProcessorArchitecture(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getSystemInfo,
    LLVMTypeRef getSystemInfoType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i16Type = LLVMInt16TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(
        module, "__nativesharp_system_info_get_processor_architecture", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef info = LLVMBuildArrayAlloca(builder, i8Type, LLVMConstInt(i64Type, 48, 0), "system_info");
    LLVMValueRef arguments[1] = {info};
    LLVMBuildCall2(builder, getSystemInfoType, getSystemInfo, arguments, 1, "");
    // wProcessorArchitecture is the first WORD of SYSTEM_INFO's union, byte offset 0 -- no GEP
    // needed, same as buildNullTerminatedCopy reading a NativeSharp string's length.
    LLVMValueRef architecture = LLVMBuildLoad2(builder, i16Type, info, "processor_architecture");
    LLVMValueRef architecture64 = LLVMBuildZExt(builder, architecture, i64Type, "processor_architecture64");
    LLVMBuildRet(builder, architecture64);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildSystemInfoGetAllocationGranularity(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getSystemInfo,
    LLVMTypeRef getSystemInfoType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(
        module, "__nativesharp_system_info_get_allocation_granularity", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef info = LLVMBuildArrayAlloca(builder, i8Type, LLVMConstInt(i64Type, 48, 0), "system_info");
    LLVMValueRef arguments[1] = {info};
    LLVMBuildCall2(builder, getSystemInfoType, getSystemInfo, arguments, 1, "");
    // dwAllocationGranularity is a DWORD at byte offset 40 of SYSTEM_INFO.
    LLVMValueRef granularityIndex[1] = {LLVMConstInt(i64Type, 40, 0)};
    LLVMValueRef granularityPointer = LLVMBuildGEP2(
        builder, i8Type, info, granularityIndex, 1, "allocation_granularity_ptr");
    LLVMValueRef granularity = LLVMBuildLoad2(builder, i32Type, granularityPointer, "allocation_granularity");
    LLVMValueRef granularity64 = LLVMBuildZExt(builder, granularity, i64Type, "allocation_granularity64");
    LLVMBuildRet(builder, granularity64);
    LLVMDisposeBuilder(builder);
    return true;
}

// GlobalMemoryStatusEx requires dwLength to be set to sizeof(MEMORYSTATUSEX) (64 on x64) BEFORE
// the call -- the one Win32 struct-out-param call in this entire runtime that needs a field
// written before it is filled, not just read after.
bool buildSystemInfoGetTotalPhysicalMemoryBytes(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef globalMemoryStatusEx,
    LLVMTypeRef globalMemoryStatusExType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(
        module, "__nativesharp_system_info_get_total_physical_memory_bytes", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef status = LLVMBuildArrayAlloca(builder, i8Type, LLVMConstInt(i64Type, 64, 0), "memory_status");
    LLVMValueRef lengthIndex[1] = {LLVMConstInt(i64Type, 0, 0)};
    LLVMValueRef lengthPointer = LLVMBuildGEP2(builder, i8Type, status, lengthIndex, 1, "length_ptr");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 64, 0), lengthPointer);
    LLVMValueRef arguments[1] = {status};
    LLVMBuildCall2(builder, globalMemoryStatusExType, globalMemoryStatusEx, arguments, 1, "");
    LLVMValueRef totalIndex[1] = {LLVMConstInt(i64Type, 8, 0)};
    LLVMValueRef totalPointer = LLVMBuildGEP2(builder, i8Type, status, totalIndex, 1, "total_phys_ptr");
    LLVMValueRef total = LLVMBuildLoad2(builder, i64Type, totalPointer, "total_phys");
    LLVMBuildRet(builder, total);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildSystemInfoGetAvailablePhysicalMemoryBytes(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef globalMemoryStatusEx,
    LLVMTypeRef globalMemoryStatusExType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(
        module, "__nativesharp_system_info_get_available_physical_memory_bytes", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef status = LLVMBuildArrayAlloca(builder, i8Type, LLVMConstInt(i64Type, 64, 0), "memory_status");
    LLVMValueRef lengthIndex[1] = {LLVMConstInt(i64Type, 0, 0)};
    LLVMValueRef lengthPointer = LLVMBuildGEP2(builder, i8Type, status, lengthIndex, 1, "length_ptr");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 64, 0), lengthPointer);
    LLVMValueRef arguments[1] = {status};
    LLVMBuildCall2(builder, globalMemoryStatusExType, globalMemoryStatusEx, arguments, 1, "");
    LLVMValueRef availIndex[1] = {LLVMConstInt(i64Type, 16, 0)};
    LLVMValueRef availPointer = LLVMBuildGEP2(builder, i8Type, status, availIndex, 1, "avail_phys_ptr");
    LLVMValueRef avail = LLVMBuildLoad2(builder, i64Type, availPointer, "avail_phys");
    LLVMBuildRet(builder, avail);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildSystemInfoGetMemoryLoadPercent(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef globalMemoryStatusEx,
    LLVMTypeRef globalMemoryStatusExType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(
        module, "__nativesharp_system_info_get_memory_load_percent", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef status = LLVMBuildArrayAlloca(builder, i8Type, LLVMConstInt(i64Type, 64, 0), "memory_status");
    LLVMValueRef lengthIndex[1] = {LLVMConstInt(i64Type, 0, 0)};
    LLVMValueRef lengthPointer = LLVMBuildGEP2(builder, i8Type, status, lengthIndex, 1, "length_ptr");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 64, 0), lengthPointer);
    LLVMValueRef arguments[1] = {status};
    LLVMBuildCall2(builder, globalMemoryStatusExType, globalMemoryStatusEx, arguments, 1, "");
    LLVMValueRef loadIndex[1] = {LLVMConstInt(i64Type, 4, 0)};
    LLVMValueRef loadPointer = LLVMBuildGEP2(builder, i8Type, status, loadIndex, 1, "memory_load_ptr");
    LLVMValueRef load = LLVMBuildLoad2(builder, i32Type, loadPointer, "memory_load");
    LLVMValueRef load64 = LLVMBuildZExt(builder, load, i64Type, "memory_load64");
    LLVMBuildRet(builder, load64);
    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 55.3 "Diagnostics".
bool buildDebugIsDebuggerPresent(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef isDebuggerPresent,
    LLVMTypeRef isDebuggerPresentType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_debug_is_debugger_present", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef present = LLVMBuildCall2(builder, isDebuggerPresentType, isDebuggerPresent, nullptr, 0, "present");
    LLVMValueRef present64 = LLVMBuildZExt(builder, present, i64Type, "present64");
    LLVMBuildRet(builder, present64);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildDebugCaptureStackFrameCount(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef rtlCaptureStackBackTrace,
    LLVMTypeRef rtlCaptureStackBackTraceType)
{
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i64Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_debug_capture_stack_frame_count", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    // Stage 55.3 deliberately surfaces only the FRAME COUNT, not the frame addresses themselves
    // -- returning the raw addresses would need an array-marshaling mechanism this compiler does
    // not have yet (see Debug.ns's header comment). The 62-pointer scratch buffer below (496
    // bytes on x64) is discarded the moment this call returns; RtlCaptureStackBackTrace still
    // performs a REAL stack walk, this just does not surface where it walked to.
    LLVMValueRef backTrace = LLVMBuildArrayAlloca(builder, pointerType, LLVMConstInt(i64Type, 62, 0), "back_trace");
    LLVMValueRef arguments[4] = {
        LLVMConstInt(i32Type, 0, 0), LLVMConstInt(i32Type, 62, 0), backTrace, LLVMConstNull(pointerType)};
    LLVMValueRef frameCount = LLVMBuildCall2(
        builder, rtlCaptureStackBackTraceType, rtlCaptureStackBackTrace, arguments, 4, "frame_count");
    LLVMValueRef frameCount64 = LLVMBuildZExt(builder, frameCount, i64Type, "frame_count64");
    LLVMBuildRet(builder, frameCount64);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildDebugOutputString(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef outputDebugStringA,
    LLVMTypeRef outputDebugStringAType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_debug_output_string", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef messageString = LLVMGetParam(function, 0);
    // Same marshaling EnvironmentSetVariable (53.2b) established: a NativeSharp string is never
    // null-terminated, so a scratch ANSI copy is built before calling the Win32 function.
    LLVMValueRef messageBuffer = buildNullTerminatedCopy(context, builder, function, messageString);
    LLVMValueRef arguments[1] = {messageBuffer};
    LLVMBuildCall2(builder, outputDebugStringAType, outputDebugStringA, arguments, 1, "");
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

// Pre-Stage-56 "utilidades basicas" round (Alberto, 2026-08-29). Three small additions requested
// before starting Stage 56 Win32 Windowing.
//
// buildThreadSleep: `__nativesharp_thread_sleep`, void(i32 milliseconds) over Win32's
// VOID Sleep(DWORD dwMilliseconds) -- Sleep genuinely returns nothing at the Win32 level (unlike
// almost every other function this runtime wraps, there is no BOOL to discard here), so the
// NativeSharp argument passes straight through with no conversion and the wrapper is a plain
// call-then-return-void, same shape as buildMonitorPulse's single-pointer-in/void-out body just
// with an i32 instead of a pointer.
bool buildThreadSleep(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef sleep,
    LLVMTypeRef sleepType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef type = functionType(voidType, {i32Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_thread_sleep", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef arguments[1] = {LLVMGetParam(function, 0)};
    LLVMBuildCall2(builder, sleepType, sleep, arguments, 1, "");
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

// buildFileCopy: `__nativesharp_file_copy`, i1(ptr source, ptr dest, i1 overwrite) over Win32's
// BOOL CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists). Two
// independent null-terminated copies, exactly buildEnvironmentSetVariable's (53.2b) two-string
// precedent. The one piece of real translation this wrapper does: NativeSharp's `overwrite`
// parameter and Win32's `bFailIfExists` are the SAME BOOLEAN WITH OPPOSITE POLARITY --
// overwrite=true must become bFailIfExists=FALSE (and vice versa) -- so the i1 argument is
// inverted (XOR against an all-ones i1, the standard LLVM boolean-not idiom) before being
// zero-extended to Win32's 32-bit BOOL, the same zext-to-i32 idiom buildEventCreate (54.5) uses
// for its two i1 parameters. CopyFileA's real BOOL result is compared against zero and returned
// as a genuine builtin:bool (no dummy wrapping) -- a caller can tell success from failure, unlike
// buildFileDelete/buildDirectoryCreate's discarded-BOOL discipline for the older, void-returning
// File/Directory operations.
bool buildFileCopy(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef copyFileA,
    LLVMTypeRef copyFileAType)
{
    LLVMTypeRef i1Type = LLVMInt1TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i1Type, {pointerType, pointerType, i1Type});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_file_copy", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef sourceString = LLVMGetParam(function, 0);
    LLVMValueRef destString = LLVMGetParam(function, 1);
    LLVMValueRef overwriteI1 = LLVMGetParam(function, 2);
    LLVMValueRef sourceBuffer = buildNullTerminatedCopy(context, builder, function, sourceString);
    LLVMValueRef destBuffer = buildNullTerminatedCopy(context, builder, function, destString);
    // overwrite -> bFailIfExists is a polarity flip: bFailIfExists = NOT(overwrite).
    LLVMValueRef failIfExistsI1 = LLVMBuildNot(builder, overwriteI1, "fail_if_exists");
    LLVMValueRef failIfExistsI32 = LLVMBuildZExt(builder, failIfExistsI1, i32Type, "fail_if_exists_i32");
    LLVMValueRef arguments[3] = {sourceBuffer, destBuffer, failIfExistsI32};
    LLVMValueRef copyResult = LLVMBuildCall2(builder, copyFileAType, copyFileA, arguments, 3, "copy_result");
    LLVMValueRef succeeded = LLVMBuildICmp(
        builder, LLVMIntNE, copyResult, LLVMConstInt(i32Type, 0, 0), "succeeded");
    LLVMBuildRet(builder, succeeded);
    LLVMDisposeBuilder(builder);
    return true;
}

// buildMoveFile: `__nativesharp_move_file`, i1(ptr source, ptr dest) over Win32's BOOL
// MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName). Shared by BOTH File.MoveRaw and
// Directory.MoveRaw at the LLVM-lowering layer (see the two-case-label CoreFileMove/
// CoreDirectoryMove block in ProjectOopLlvmLowerer.cpp) -- MoveFileA itself works identically for
// a file path or a directory path, so one wrapper serves both NativeSharp-level operations, the
// same "one Win32 call, two MethodCoreOperationKind values" precedent buildWaitForHandleTimeout
// (54.5) established for Semaphore.Wait/Event.Wait. Two independent null-terminated copies, same
// shape as buildFileCopy above (minus the third boolean argument). Real BOOL result surfaced as a
// genuine builtin:bool, same "succeeded" icmp idiom as buildFileCopy.
bool buildMoveFile(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef moveFileA,
    LLVMTypeRef moveFileAType)
{
    LLVMTypeRef i1Type = LLVMInt1TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i1Type, {pointerType, pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_move_file", type);
    if (function == nullptr)
    {
        return false;
    }
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef sourceString = LLVMGetParam(function, 0);
    LLVMValueRef destString = LLVMGetParam(function, 1);
    LLVMValueRef sourceBuffer = buildNullTerminatedCopy(context, builder, function, sourceString);
    LLVMValueRef destBuffer = buildNullTerminatedCopy(context, builder, function, destString);
    LLVMValueRef arguments[2] = {sourceBuffer, destBuffer};
    LLVMValueRef moveResult = LLVMBuildCall2(builder, moveFileAType, moveFileA, arguments, 2, "move_result");
    LLVMValueRef succeeded = LLVMBuildICmp(
        builder, LLVMIntNE, moveResult, LLVMConstInt(i32Type, 0, 0), "succeeded");
    LLVMBuildRet(builder, succeeded);
    LLVMDisposeBuilder(builder);
    return true;
}

// Stage 54.2/Entrega 226 "Atomics". All seven builder families below are parameterized by `bitWidth` (32 or
// 64) to share code between the Int32/Int64 variants, the same `wantDirectories`-style shared-
// helper discipline Stage 53.6's buildDirectoryEntryMatchesKind already established above. Every
// every runtime function built from these families uses LLVM atomic load/store/RMW/CAS operations
// directly. The original ten RMW functions use LLVMBuildAtomicRMW/
// LLVMBuildAtomicCmpXchg directly -- REAL hardware atomic instructions (`lock xadd`/`lock xchg`/
// `lock cmpxchg` on x86-64), NOT a call into any Win32 DLL function.
//
// This is a deliberate pivot from the ORIGINAL Stage 54.2 design, which planned to wrap
// kernel32.dll's InterlockedIncrement/InterlockedExchangeAdd/InterlockedExchange/
// InterlockedCompareExchange exports (and their *64 counterparts), following the exact same "wrap
// one real Win32 function" pattern every earlier core operation in this compiler uses. During
// implementation this was verified against the real Windows headers (mingw-w64's
// psdk_inc/intrin-impl.h, which mirrors the Microsoft SDK's winnt.h on this point): the 32-bit
// InterlockedIncrement/InterlockedDecrement/InterlockedExchangeAdd/InterlockedExchange/
// InterlockedCompareExchange ARE real kernel32.dll exports (a legacy compatibility path dating
// back to before compilers had atomic intrinsics, still present today for old/dynamically-loading
// callers) -- but the 64-bit InterlockedIncrement64/Decrement64/ExchangeAdd64/Exchange64/
// CompareExchange64 family is NOT exported by kernel32.dll at all. On x64, Windows headers define
// every one of those as an inline call straight to a COMPILER intrinsic
// (_InterlockedIncrement64, etc.), which the compiler expands directly into the same
// lock-prefixed machine instruction LLVMBuildAtomicRMW/LLVMBuildAtomicCmpXchg emit here -- there
// was never a DLL-exported fallback for 64-bit atomics, because x64 shipped with hardware 64-bit
// CMPXCHG/XADD support from day one, unlike x86 which needed the older kernel32 exports before
// compilers grew intrinsics of their own.
//
// Rather than have the Int32 half of this feature call through kernel32.dll while the Int64 half
// physically cannot, both widths now use the exact same mechanism the compiler intrinsics
// themselves use under the hood: real LLVM atomic IR instructions, emitted directly in this
// runtime module, with zero Win32 API dependency. This is, if anything, MORE faithful to how a
// real Windows program behaves than the originally planned approach would have been -- a real x64
// build of C#'s Interlocked.Increment (or a hand-written MSVC program calling InterlockedIncrement)
// never actually calls into kernel32.dll either; it inlines the lock-prefixed instruction exactly
// like this does. Consequence for the rest of this round: no new Win32 imports are needed, so
// kernel32Symbols in the five linker-symbol locations stays untouched for Stage 54.2 (a genuine
// difference from every prior round, documented here so it is not mistaken for an oversight).
//
// All five ordering choices below use LLVMAtomicOrderingSequentiallyConsistent, the strongest and
// simplest-to-reason-about ordering LLVM offers -- deliberately NOT economizing with a weaker
// ordering (Acquire/Release/Monotonic), because C#'s System.Threading.Interlocked family is
// itself documented as providing full sequential consistency, and this compiler has no mechanism
// yet for a NativeSharp author to request anything weaker.
bool buildAtomicLoad(
    LLVMContextRef context,
    LLVMModuleRef module,
    const char* functionName,
    unsigned bitWidth)
{
    LLVMTypeRef valueType = bitWidth == 64
        ? LLVMInt64TypeInContext(context)
        : LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(valueType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, functionName, type);
    if (function == nullptr) return false;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef value = LLVMBuildLoad2(
        builder, valueType, LLVMGetParam(function, 0), "atomic.load.value");
    LLVMSetOrdering(value, LLVMAtomicOrderingSequentiallyConsistent);
    LLVMSetAlignment(value, bitWidth / 8U);
    LLVMBuildRet(builder, value);
    LLVMDisposeBuilder(builder);
    return true;
}

// Item 354: LLVM legalizes scalar binary16 conversions to these compiler-rt entry points on
// baseline x64 targets. NativeSharp links without a CRT, so provide the two helpers in its own
// runtime using integer IEEE-754 field manipulation. This also works on machines without F16C.
bool buildHalfCompilerRtSupport(LLVMContextRef context, LLVMModuleRef module)
{
    LLVMTypeRef i1 = LLVMInt1TypeInContext(context);
    LLVMTypeRef i16 = LLVMInt16TypeInContext(context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef f16 = LLVMHalfTypeInContext(context);
    LLVMTypeRef f32 = LLVMFloatTypeInContext(context);
    auto c32 = [i32](std::uint64_t value) { return LLVMConstInt(i32, value, 0); };

    LLVMTypeRef truncType = functionType(f16, {f32});
    LLVMValueRef truncFunction = LLVMAddFunction(module, "__truncsfhf2", truncType);
    if (truncFunction == nullptr) return false;
    LLVMBasicBlockRef truncEntry = LLVMAppendBasicBlockInContext(context, truncFunction, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, truncEntry);
    LLVMValueRef bits = LLVMBuildBitCast(builder, LLVMGetParam(truncFunction, 0), i32, "bits");
    LLVMValueRef sign = LLVMBuildAnd(builder,
        LLVMBuildLShr(builder, bits, c32(16), "sign.shift"), c32(0x8000), "sign");
    LLVMValueRef exponent = LLVMBuildAnd(builder,
        LLVMBuildLShr(builder, bits, c32(23), "exp.shift"), c32(0xff), "exp");
    LLVMValueRef mantissa = LLVMBuildAnd(builder, bits, c32(0x7fffff), "mantissa");
    LLVMValueRef halfExponent = LLVMBuildSub(builder, exponent, c32(112), "half.exp");
    LLVMValueRef retained = LLVMBuildAnd(builder,
        LLVMBuildLShr(builder, mantissa, c32(13), "retained.shift"), c32(1), "retained.bit");
    LLVMValueRef roundedMantissa = LLVMBuildLShr(builder,
        LLVMBuildAdd(builder, mantissa,
            LLVMBuildAdd(builder, c32(0xfff), retained, "round.bias"), "rounded.source"),
        c32(13), "rounded.mantissa");
    LLVMValueRef normal = LLVMBuildOr(builder, sign,
        LLVMBuildAdd(builder, LLVMBuildShl(builder, halfExponent, c32(10), "normal.exp"),
            roundedMantissa, "normal.payload"), "normal");
    LLVMValueRef shift = LLVMBuildSub(builder, c32(126), exponent, "subnormal.shift");
    LLVMValueRef hiddenMantissa = LLVMBuildOr(builder, mantissa, c32(0x800000), "hidden.mantissa");
    LLVMValueRef subnormalRound = LLVMBuildShl(builder, c32(1),
        LLVMBuildSub(builder, shift, c32(1), "subnormal.round.shift"), "subnormal.round");
    LLVMValueRef subnormalPayload = LLVMBuildLShr(builder,
        LLVMBuildAdd(builder, hiddenMantissa, subnormalRound, "subnormal.rounded"), shift, "subnormal.payload");
    LLVMValueRef subnormal = LLVMBuildOr(builder, sign, subnormalPayload, "subnormal");
    LLVMValueRef finiteLow = LLVMBuildSelect(builder,
        LLVMBuildICmp(builder, LLVMIntULT, exponent, c32(103), "underflows"), sign, subnormal, "finite.low");
    LLVMValueRef finite = LLVMBuildSelect(builder,
        LLVMBuildICmp(builder, LLVMIntULT, exponent, c32(113), "is.subnormal"), finiteLow, normal, "finite");
    LLVMValueRef infinity = LLVMBuildOr(builder, sign, c32(0x7c00), "infinity");
    LLVMValueRef overflowed = LLVMBuildSelect(builder,
        LLVMBuildICmp(builder, LLVMIntUGE, exponent, c32(143), "overflows"), infinity, finite, "overflowed");
    LLVMValueRef nanPayload = LLVMBuildOr(builder,
        LLVMBuildLShr(builder, mantissa, c32(13), "nan.payload.shift"), c32(0x200), "nan.payload");
    LLVMValueRef special = LLVMBuildOr(builder, infinity, nanPayload, "special");
    LLVMValueRef result32 = LLVMBuildSelect(builder,
        LLVMBuildICmp(builder, LLVMIntEQ, exponent, c32(255), "is.special"), special, overflowed, "half.bits.i32");
    LLVMValueRef result16 = LLVMBuildTrunc(builder, result32, i16, "half.bits");
    LLVMBuildRet(builder, LLVMBuildBitCast(builder, result16, f16, "half.value"));
    LLVMDisposeBuilder(builder);

    LLVMTypeRef extendType = functionType(f32, {f16});
    LLVMValueRef extendFunction = LLVMAddFunction(module, "__extendhfsf2", extendType);
    if (extendFunction == nullptr) return false;
    LLVMBasicBlockRef extendEntry = LLVMAppendBasicBlockInContext(context, extendFunction, "entry");
    builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, extendEntry);
    LLVMValueRef halfBits16 = LLVMBuildBitCast(builder, LLVMGetParam(extendFunction, 0), i16, "half.bits");
    LLVMValueRef halfBits = LLVMBuildZExt(builder, halfBits16, i32, "half.bits.i32");
    LLVMValueRef wideSign = LLVMBuildShl(builder,
        LLVMBuildAnd(builder, halfBits, c32(0x8000), "half.sign"), c32(16), "float.sign");
    LLVMValueRef halfExp = LLVMBuildAnd(builder,
        LLVMBuildLShr(builder, halfBits, c32(10), "half.exp.shift"), c32(0x1f), "half.exp");
    LLVMValueRef halfMantissa = LLVMBuildAnd(builder, halfBits, c32(0x3ff), "half.mantissa");
    LLVMValueRef normalBits = LLVMBuildOr(builder, wideSign,
        LLVMBuildOr(builder,
            LLVMBuildShl(builder, LLVMBuildAdd(builder, halfExp, c32(112), "float.exp"), c32(23), "float.exp.bits"),
            LLVMBuildShl(builder, halfMantissa, c32(13), "float.mantissa"), "float.payload"), "normal.bits");
    LLVMValueRef specialBits = LLVMBuildOr(builder, wideSign,
        LLVMBuildOr(builder, c32(0x7f800000),
            LLVMBuildShl(builder, halfMantissa, c32(13), "special.mantissa"), "special.payload"), "special.bits");
    LLVMTypeRef ctlzType = functionType(i32, {i32, i1});
    LLVMValueRef ctlz = LLVMAddFunction(module, "llvm.ctlz.i32", ctlzType);
    LLVMValueRef ctlzArgs[] = {halfMantissa, LLVMConstInt(i1, 0, 0)};
    LLVMValueRef leading = LLVMBuildCall2(builder, ctlzType, ctlz, ctlzArgs, 2, "leading.zeros");
    LLVMValueRef normalizeShift = LLVMBuildSub(builder, leading, c32(21), "normalize.shift");
    LLVMValueRef normalizedMantissa = LLVMBuildAnd(builder,
        LLVMBuildShl(builder, halfMantissa, normalizeShift, "normalized.source"), c32(0x3ff), "normalized.mantissa");
    LLVMValueRef subnormalBits = LLVMBuildOr(builder, wideSign,
        LLVMBuildOr(builder,
            LLVMBuildShl(builder, LLVMBuildSub(builder, c32(113), normalizeShift, "subnormal.exp"), c32(23), "subnormal.exp.bits"),
            LLVMBuildShl(builder, normalizedMantissa, c32(13), "subnormal.mantissa"), "subnormal.float.payload"),
        "subnormal.float.bits");
    LLVMValueRef zeroOrSubnormal = LLVMBuildSelect(builder,
        LLVMBuildICmp(builder, LLVMIntEQ, halfMantissa, c32(0), "is.zero"), wideSign, subnormalBits, "zero.or.subnormal");
    LLVMValueRef finiteBits = LLVMBuildSelect(builder,
        LLVMBuildICmp(builder, LLVMIntEQ, halfExp, c32(0), "exp.zero"), zeroOrSubnormal, normalBits, "finite.bits");
    LLVMValueRef floatBits = LLVMBuildSelect(builder,
        LLVMBuildICmp(builder, LLVMIntEQ, halfExp, c32(31), "exp.special"), specialBits, finiteBits, "float.bits");
    LLVMBuildRet(builder, LLVMBuildBitCast(builder, floatBits, f32, "float.value"));
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildAtomicStore(
    LLVMContextRef context,
    LLVMModuleRef module,
    const char* functionName,
    unsigned bitWidth)
{
    LLVMTypeRef valueType = bitWidth == 64
        ? LLVMInt64TypeInContext(context)
        : LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(LLVMVoidTypeInContext(context), {pointerType, valueType});
    LLVMValueRef function = LLVMAddFunction(module, functionName, type);
    if (function == nullptr) return false;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef store = LLVMBuildStore(
        builder, LLVMGetParam(function, 1), LLVMGetParam(function, 0));
    LLVMSetOrdering(store, LLVMAtomicOrderingSequentiallyConsistent);
    LLVMSetAlignment(store, bitWidth / 8U);
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    return true;
}

bool buildInterlockedIncrementOrDecrement(
    LLVMContextRef context,
    LLVMModuleRef module,
    const char* functionName,
    unsigned bitWidth,
    bool isIncrement)
{
    LLVMTypeRef valueType = bitWidth == 64
        ? LLVMInt64TypeInContext(context)
        : LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(valueType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, functionName, type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef location = LLVMGetParam(function, 0);
    // +1 for Increment, all-ones (two's-complement -1) for Decrement. LLVMAtomicRMWBinOpAdd with
    // a negative operand IS a subtract -- there is no separate "Sub" case needed here, which is
    // why LlvmCApi.h's LLVMAtomicRMWBinOp subset only lists Xchg and Add (see its own comment).
    LLVMValueRef delta = isIncrement
        ? LLVMConstInt(valueType, 1, 0)
        : LLVMConstInt(valueType, static_cast<unsigned long long>(-1), 1);
    LLVMValueRef oldValue = LLVMBuildAtomicRMW(
        builder, LLVMAtomicRMWBinOpAdd, location, delta,
        LLVMAtomicOrderingSequentiallyConsistent, 0);
    // A raw atomicrmw instruction only ever hands back the OLD value (that is what the hardware
    // instruction itself produces) -- but Win32's InterlockedIncrement/Decrement, and C#'s
    // Interlocked.Increment/Decrement, are documented to return the NEW value (the value AFTER
    // the operation). So the new value is computed here explicitly as old + delta.
    LLVMValueRef newValue = LLVMBuildAdd(builder, oldValue, delta, "interlocked.new_value");
    LLVMBuildRet(builder, newValue);

    LLVMDisposeBuilder(builder);
    return true;
}

bool buildInterlockedAdd(
    LLVMContextRef context,
    LLVMModuleRef module,
    const char* functionName,
    unsigned bitWidth)
{
    LLVMTypeRef valueType = bitWidth == 64
        ? LLVMInt64TypeInContext(context)
        : LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(valueType, {pointerType, valueType});
    LLVMValueRef function = LLVMAddFunction(module, functionName, type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef location = LLVMGetParam(function, 0);
    LLVMValueRef value = LLVMGetParam(function, 1);
    LLVMValueRef oldValue = LLVMBuildAtomicRMW(
        builder, LLVMAtomicRMWBinOpAdd, location, value,
        LLVMAtomicOrderingSequentiallyConsistent, 0);
    // Same story as Increment/Decrement above: atomicrmw hands back the OLD value, but C#'s
    // Interlocked.Add contract requires the NEW value -- computed here as old + value. This is
    // the exact same adjustment the originally-planned Win32 InterlockedExchangeAdd wrapper would
    // have needed too (Win32's ExchangeAdd also only returns the old value); the pivot to raw
    // atomic IR did not remove this step, it only moved where the "old" value comes from.
    LLVMValueRef newValue = LLVMBuildAdd(builder, oldValue, value, "interlocked.add.new_value");
    LLVMBuildRet(builder, newValue);

    LLVMDisposeBuilder(builder);
    return true;
}

bool buildInterlockedExchange(
    LLVMContextRef context,
    LLVMModuleRef module,
    const char* functionName,
    unsigned bitWidth)
{
    LLVMTypeRef valueType = bitWidth == 64
        ? LLVMInt64TypeInContext(context)
        : LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(valueType, {pointerType, valueType});
    LLVMValueRef function = LLVMAddFunction(module, functionName, type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef location = LLVMGetParam(function, 0);
    LLVMValueRef value = LLVMGetParam(function, 1);
    LLVMValueRef oldValue = LLVMBuildAtomicRMW(
        builder, LLVMAtomicRMWBinOpXchg, location, value,
        LLVMAtomicOrderingSequentiallyConsistent, 0);
    // Unlike Increment/Decrement/Add, Exchange's C# contract wants the OLD value directly -- the
    // atomicrmw instruction's own natural result -- so no adjustment is needed here at all.
    LLVMBuildRet(builder, oldValue);

    LLVMDisposeBuilder(builder);
    return true;
}

bool buildInterlockedCompareExchange(
    LLVMContextRef context,
    LLVMModuleRef module,
    const char* functionName,
    unsigned bitWidth)
{
    LLVMTypeRef valueType = bitWidth == 64
        ? LLVMInt64TypeInContext(context)
        : LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(valueType, {pointerType, valueType, valueType});
    LLVMValueRef function = LLVMAddFunction(module, functionName, type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    // Parameter order matches Win32's InterlockedCompareExchange(Destination, Exchange,
    // Comparand) and C#'s Interlocked.CompareExchange(location, value, comparand) -- but
    // LLVMBuildAtomicCmpXchg's own parameter order is (pointer, compare, new), so `comparand` and
    // `value` swap positions going into the call below.
    LLVMValueRef location = LLVMGetParam(function, 0);
    LLVMValueRef value = LLVMGetParam(function, 1);
    LLVMValueRef comparand = LLVMGetParam(function, 2);
    LLVMValueRef resultPair = LLVMBuildAtomicCmpXchg(
        builder, location, comparand, value,
        LLVMAtomicOrderingSequentiallyConsistent, LLVMAtomicOrderingSequentiallyConsistent, 0);
    // LLVMBuildAtomicCmpXchg's result is a `{ valueType, i1 }` aggregate -- the value that was
    // actually AT the location right before the swap attempt, plus whether the swap succeeded.
    // C#'s Interlocked.CompareExchange never exposes that success flag to the caller (callers
    // compare the returned old value against their own comparand themselves, exactly as C#'s
    // does), so only index 0 is extracted here.
    LLVMValueRef oldValue = LLVMBuildExtractValue(builder, resultPair, 0, "interlocked.cmpxchg.old_value");
    LLVMBuildRet(builder, oldValue);

    LLVMDisposeBuilder(builder);
    return true;
}

bool buildPuts(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef getStdHandle,
    LLVMTypeRef getStdHandleType,
    LLVMValueRef writeFile,
    LLVMTypeRef writeFileType)
{
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef putsType = functionType(i32Type, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "puts", putsType);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef measure = LLVMAppendBasicBlockInContext(context, function, "measure");
    LLVMBasicBlockRef advance = LLVMAppendBasicBlockInContext(context, function, "advance");
    LLVMBasicBlockRef measured = LLVMAppendBasicBlockInContext(context, function, "measured");
    LLVMBasicBlockRef writeText = LLVMAppendBasicBlockInContext(context, function, "write_text");
    LLVMBasicBlockRef writeNewline = LLVMAppendBasicBlockInContext(context, function, "write_newline");
    LLVMBasicBlockRef success = LLVMAppendBasicBlockInContext(context, function, "success");
    LLVMBasicBlockRef failure = LLVMAppendBasicBlockInContext(context, function, "failure");

    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef indexSlot = LLVMBuildAlloca(builder, i64Type, "index_slot");
    LLVMValueRef writtenSlot = LLVMBuildAlloca(builder, i32Type, "written_slot");
    LLVMBuildStore(builder, LLVMConstInt(i64Type, 0, 0), indexSlot);

    LLVMValueRef text = LLVMGetParam(function, 0);
    LLVMValueRef textIsNull = LLVMBuildICmp(
        builder,
        LLVMIntEQ,
        text,
        LLVMConstNull(pointerType),
        "text_is_null");
    LLVMBuildCondBr(builder, textIsNull, failure, measure);

    LLVMPositionBuilderAtEnd(builder, measure);
    LLVMValueRef index = LLVMBuildLoad2(builder, i64Type, indexSlot, "index");
    LLVMValueRef indices[1] = {index};
    LLVMValueRef characterPointer = LLVMBuildGEP2(
        builder,
        i8Type,
        text,
        indices,
        1,
        "character_ptr");
    LLVMValueRef character = LLVMBuildLoad2(builder, i8Type, characterPointer, "character");
    LLVMValueRef terminator = LLVMBuildICmp(
        builder,
        LLVMIntEQ,
        character,
        LLVMConstInt(i8Type, 0, 0),
        "terminator");
    LLVMBuildCondBr(builder, terminator, measured, advance);

    LLVMPositionBuilderAtEnd(builder, advance);
    LLVMValueRef nextIndex = LLVMBuildAdd(
        builder,
        index,
        LLVMConstInt(i64Type, 1, 0),
        "next_index");
    LLVMBuildStore(builder, nextIndex, indexSlot);
    LLVMBuildBr(builder, measure);

    LLVMPositionBuilderAtEnd(builder, measured);
    LLVMValueRef length64 = LLVMBuildLoad2(builder, i64Type, indexSlot, "length64");
    LLVMValueRef tooLarge = LLVMBuildICmp(
        builder,
        LLVMIntUGT,
        length64,
        LLVMConstInt(i64Type, 0xffffffffULL, 0),
        "too_large");
    LLVMBuildCondBr(builder, tooLarge, failure, writeText);

    LLVMPositionBuilderAtEnd(builder, writeText);
    LLVMValueRef stdOutputCode = LLVMConstInt(i32Type, 0xfffffff5ULL, 0);
    LLVMValueRef handleArguments[1] = {stdOutputCode};
    LLVMValueRef handle = LLVMBuildCall2(
        builder,
        getStdHandleType,
        getStdHandle,
        handleArguments,
        1,
        "stdout_handle");
    LLVMValueRef length32 = LLVMBuildTrunc(builder, length64, i32Type, "length32");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0, 0), writtenSlot);
    LLVMValueRef writeArguments[5] = {
        handle,
        text,
        length32,
        writtenSlot,
        LLVMConstNull(pointerType)};
    LLVMValueRef writeResult = LLVMBuildCall2(
        builder,
        writeFileType,
        writeFile,
        writeArguments,
        5,
        "write_result");
    LLVMValueRef written = LLVMBuildLoad2(builder, i32Type, writtenSlot, "written");
    LLVMValueRef writeSucceeded = LLVMBuildICmp(
        builder,
        LLVMIntNE,
        writeResult,
        LLVMConstInt(i32Type, 0, 0),
        "write_succeeded");
    LLVMValueRef fullWrite = LLVMBuildICmp(builder, LLVMIntEQ, written, length32, "full_write");
    LLVMValueRef textOk = LLVMBuildAnd(builder, writeSucceeded, fullWrite, "text_ok");
    LLVMBuildCondBr(builder, textOk, writeNewline, failure);

    LLVMPositionBuilderAtEnd(builder, writeNewline);
    LLVMValueRef newline = LLVMBuildGlobalStringPtr(builder, "\r\n", "newline");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 0, 0), writtenSlot);
    LLVMValueRef newlineArguments[5] = {
        handle,
        newline,
        LLVMConstInt(i32Type, 2, 0),
        writtenSlot,
        LLVMConstNull(pointerType)};
    LLVMValueRef newlineResult = LLVMBuildCall2(
        builder,
        writeFileType,
        writeFile,
        newlineArguments,
        5,
        "newline_result");
    LLVMValueRef newlineWritten = LLVMBuildLoad2(builder, i32Type, writtenSlot, "newline_written");
    LLVMValueRef newlineSucceeded = LLVMBuildICmp(
        builder,
        LLVMIntNE,
        newlineResult,
        LLVMConstInt(i32Type, 0, 0),
        "newline_succeeded");
    LLVMValueRef fullNewline = LLVMBuildICmp(
        builder,
        LLVMIntEQ,
        newlineWritten,
        LLVMConstInt(i32Type, 2, 0),
        "full_newline");
    LLVMValueRef newlineOk = LLVMBuildAnd(builder, newlineSucceeded, fullNewline, "newline_ok");
    LLVMBuildCondBr(builder, newlineOk, success, failure);

    LLVMPositionBuilderAtEnd(builder, success);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 0, 0));

    LLVMPositionBuilderAtEnd(builder, failure);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 0xffffffffULL, 0));

    LLVMDisposeBuilder(builder);
    return true;
}

constexpr std::uint32_t kNativeSharpExceptionCode = 0xE04E5301U;
constexpr std::uint32_t kTlsOutOfIndexes = 0xffffffffU;

bool buildEhInit(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef tlsIndex,
    LLVMValueRef tlsAlloc,
    LLVMTypeRef tlsAllocType,
    LLVMValueRef& functionOut)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef type = functionType(i32Type, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_eh_init", type);
    functionOut = function;
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef allocate = LLVMAppendBasicBlockInContext(context, function, "allocate");
    LLVMBasicBlockRef allocated = LLVMAppendBasicBlockInContext(context, function, "allocated");
    LLVMBasicBlockRef success = LLVMAppendBasicBlockInContext(context, function, "success");
    LLVMBasicBlockRef failure = LLVMAppendBasicBlockInContext(context, function, "failure");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef index = LLVMBuildLoad2(builder, i32Type, tlsIndex, "tls_index");
    LLVMValueRef uninitialized = LLVMBuildICmp(
        builder,
        LLVMIntEQ,
        index,
        LLVMConstInt(i32Type, kTlsOutOfIndexes, 0),
        "tls_uninitialized");
    LLVMBuildCondBr(builder, uninitialized, allocate, success);

    LLVMPositionBuilderAtEnd(builder, allocate);
    LLVMValueRef newIndex = LLVMBuildCall2(builder, tlsAllocType, tlsAlloc, nullptr, 0, "new_tls_index");
    LLVMValueRef allocationFailed = LLVMBuildICmp(
        builder,
        LLVMIntEQ,
        newIndex,
        LLVMConstInt(i32Type, kTlsOutOfIndexes, 0),
        "tls_allocation_failed");
    LLVMBuildCondBr(builder, allocationFailed, failure, allocated);

    LLVMPositionBuilderAtEnd(builder, allocated);
    LLVMBuildStore(builder, newIndex, tlsIndex);
    LLVMBuildBr(builder, success);

    LLVMPositionBuilderAtEnd(builder, success);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 1, 0));

    LLVMPositionBuilderAtEnd(builder, failure);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 0, 0));

    LLVMDisposeBuilder(builder);
    return true;
}

bool buildExceptionCurrent(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef tlsIndex,
    LLVMValueRef tlsGetValue,
    LLVMTypeRef tlsGetValueType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(pointerType, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_exception_current", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef loadValue = LLVMAppendBasicBlockInContext(context, function, "load_value");
    LLVMBasicBlockRef noValue = LLVMAppendBasicBlockInContext(context, function, "no_value");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef index = LLVMBuildLoad2(builder, i32Type, tlsIndex, "tls_index");
    LLVMValueRef uninitialized = LLVMBuildICmp(
        builder,
        LLVMIntEQ,
        index,
        LLVMConstInt(i32Type, kTlsOutOfIndexes, 0),
        "tls_uninitialized");
    LLVMBuildCondBr(builder, uninitialized, noValue, loadValue);

    LLVMPositionBuilderAtEnd(builder, loadValue);
    LLVMValueRef arguments[1] = {index};
    LLVMValueRef current = LLVMBuildCall2(
        builder,
        tlsGetValueType,
        tlsGetValue,
        arguments,
        1,
        "current_exception");
    LLVMBuildRet(builder, current);

    LLVMPositionBuilderAtEnd(builder, noValue);
    LLVMBuildRet(builder, LLVMConstNull(pointerType));

    LLVMDisposeBuilder(builder);
    return true;
}

bool buildExceptionClear(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef tlsIndex,
    LLVMValueRef tlsSetValue,
    LLVMTypeRef tlsSetValueType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_exception_clear", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef clear = LLVMAppendBasicBlockInContext(context, function, "clear");
    LLVMBasicBlockRef done = LLVMAppendBasicBlockInContext(context, function, "done");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef index = LLVMBuildLoad2(builder, i32Type, tlsIndex, "tls_index");
    LLVMValueRef uninitialized = LLVMBuildICmp(
        builder,
        LLVMIntEQ,
        index,
        LLVMConstInt(i32Type, kTlsOutOfIndexes, 0),
        "tls_uninitialized");
    LLVMBuildCondBr(builder, uninitialized, done, clear);

    LLVMPositionBuilderAtEnd(builder, clear);
    LLVMValueRef arguments[2] = {index, LLVMConstNull(pointerType)};
    LLVMBuildCall2(builder, tlsSetValueType, tlsSetValue, arguments, 2, "cleared");
    LLVMBuildBr(builder, done);

    LLVMPositionBuilderAtEnd(builder, done);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

bool buildThrow(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef tlsIndex,
    LLVMValueRef ehInit,
    LLVMTypeRef ehInitType,
    LLVMValueRef tlsSetValue,
    LLVMTypeRef tlsSetValueType,
    LLVMValueRef raiseException,
    LLVMTypeRef raiseExceptionType,
    LLVMValueRef exitProcess,
    LLVMTypeRef exitProcessType)
{
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(voidType, {pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_throw", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef publish = LLVMAppendBasicBlockInContext(context, function, "publish");
    LLVMBasicBlockRef raise = LLVMAppendBasicBlockInContext(context, function, "raise");
    LLVMBasicBlockRef failed = LLVMAppendBasicBlockInContext(context, function, "tls_failed");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef initialized = LLVMBuildCall2(builder, ehInitType, ehInit, nullptr, 0, "eh_initialized");
    LLVMValueRef initOk = LLVMBuildICmp(
        builder,
        LLVMIntNE,
        initialized,
        LLVMConstInt(i32Type, 0, 0),
        "eh_init_ok");
    LLVMBuildCondBr(builder, initOk, publish, failed);

    LLVMPositionBuilderAtEnd(builder, publish);
    LLVMValueRef index = LLVMBuildLoad2(builder, i32Type, tlsIndex, "tls_index");
    LLVMValueRef exception = LLVMGetParam(function, 0);
    LLVMValueRef tlsArguments[2] = {index, exception};
    LLVMValueRef published = LLVMBuildCall2(
        builder, tlsSetValueType, tlsSetValue, tlsArguments, 2, "exception_published");
    LLVMValueRef publishOk = LLVMBuildICmp(
        builder,
        LLVMIntNE,
        published,
        LLVMConstInt(i32Type, 0, 0),
        "exception_publish_ok");
    LLVMBuildCondBr(builder, publishOk, raise, failed);

    LLVMPositionBuilderAtEnd(builder, raise);
    LLVMValueRef parameterSlot = LLVMBuildAlloca(builder, i64Type, "exception_parameter");
    LLVMValueRef exceptionAddress = LLVMBuildPtrToInt(builder, exception, i64Type, "exception_address");
    LLVMBuildStore(builder, exceptionAddress, parameterSlot);
    LLVMValueRef raiseArguments[4] = {
        LLVMConstInt(i32Type, kNativeSharpExceptionCode, 0),
        LLVMConstInt(i32Type, 1, 0),
        LLVMConstInt(i32Type, 1, 0),
        parameterSlot};
    LLVMBuildCall2(builder, raiseExceptionType, raiseException, raiseArguments, 4, "");
    LLVMBuildUnreachable(builder);

    LLVMPositionBuilderAtEnd(builder, failed);
    LLVMValueRef failureArguments[1] = {LLVMConstInt(i32Type, 253, 0)};
    LLVMBuildCall2(builder, exitProcessType, exitProcess, failureArguments, 1, "");
    LLVMBuildUnreachable(builder);

    LLVMDisposeBuilder(builder);
    return true;
}

bool buildEhFilter(
    LLVMContextRef context,
    LLVMModuleRef module,
    LLVMValueRef tlsIndex,
    LLVMValueRef ehInit,
    LLVMTypeRef ehInitType,
    LLVMValueRef tlsSetValue,
    LLVMTypeRef tlsSetValueType)
{
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef type = functionType(i32Type, {pointerType, pointerType});
    LLVMValueRef function = LLVMAddFunction(module, "__nativesharp_eh_filter", type);
    if (function == nullptr)
    {
        return false;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef loadRecord = LLVMAppendBasicBlockInContext(context, function, "load_record");
    LLVMBasicBlockRef checkCode = LLVMAppendBasicBlockInContext(context, function, "check_code");
    LLVMBasicBlockRef checkPayload = LLVMAppendBasicBlockInContext(context, function, "check_payload");
    LLVMBasicBlockRef publish = LLVMAppendBasicBlockInContext(context, function, "publish_exception");
    LLVMBasicBlockRef reject = LLVMAppendBasicBlockInContext(context, function, "reject");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef exceptionPointers = LLVMGetParam(function, 0);
    LLVMValueRef pointersNull = LLVMBuildICmp(
        builder,
        LLVMIntEQ,
        exceptionPointers,
        LLVMConstNull(pointerType),
        "exception_pointers_null");
    LLVMBuildCondBr(builder, pointersNull, reject, loadRecord);

    LLVMPositionBuilderAtEnd(builder, loadRecord);
    LLVMValueRef exceptionRecord = LLVMBuildLoad2(
        builder,
        pointerType,
        exceptionPointers,
        "exception_record");
    LLVMValueRef recordNull = LLVMBuildICmp(
        builder,
        LLVMIntEQ,
        exceptionRecord,
        LLVMConstNull(pointerType),
        "exception_record_null");
    LLVMBuildCondBr(builder, recordNull, reject, checkCode);

    LLVMPositionBuilderAtEnd(builder, checkCode);
    LLVMValueRef code = LLVMBuildLoad2(builder, i32Type, exceptionRecord, "exception_code");
    LLVMValueRef nativeSharp = LLVMBuildICmp(
        builder,
        LLVMIntEQ,
        code,
        LLVMConstInt(i32Type, kNativeSharpExceptionCode, 0),
        "is_nativesharp_exception");
    LLVMBuildCondBr(builder, nativeSharp, checkPayload, reject);

    // EXCEPTION_RECORD on Win64 stores NumberParameters at byte 24 and the first ULONG_PTR at
    // byte 32. __nativesharp_throw already places the object address there. Importing it here is
    // the crucial module-boundary step: a DLL and its consumer have distinct runtime globals/TLS
    // indices, so consulting only the thrower's slot made a valid exception invisible to the
    // consumer's catch even though Windows transported the pointer in the SEH record.
    LLVMPositionBuilderAtEnd(builder, checkPayload);
    LLVMValueRef parameterCountIndex = LLVMConstInt(i64Type, 24, 0);
    LLVMValueRef parameterCountAddress = LLVMBuildGEP2(
        builder, i8Type, exceptionRecord, &parameterCountIndex, 1, "exception_parameter_count_addr");
    LLVMValueRef parameterCount = LLVMBuildLoad2(
        builder, i32Type, parameterCountAddress, "exception_parameter_count");
    LLVMValueRef hasPayload = LLVMBuildICmp(
        builder, LLVMIntUGE, parameterCount, LLVMConstInt(i32Type, 1, 0),
        "exception_has_object_payload");
    LLVMBuildCondBr(builder, hasPayload, publish, reject);

    LLVMPositionBuilderAtEnd(builder, publish);
    LLVMValueRef initialized = LLVMBuildCall2(builder, ehInitType, ehInit, nullptr, 0, "filter_eh_init");
    LLVMValueRef initOk = LLVMBuildICmp(
        builder, LLVMIntNE, initialized, LLVMConstInt(i32Type, 0, 0), "filter_eh_init_ok");
    LLVMBasicBlockRef storePayload = LLVMAppendBasicBlockInContext(context, function, "store_payload");
    LLVMBuildCondBr(builder, initOk, storePayload, reject);

    LLVMPositionBuilderAtEnd(builder, storePayload);
    LLVMValueRef payloadIndex = LLVMConstInt(i64Type, 32, 0);
    LLVMValueRef payloadAddress = LLVMBuildGEP2(
        builder, i8Type, exceptionRecord, &payloadIndex, 1, "exception_payload_addr");
    LLVMValueRef payloadBits = LLVMBuildLoad2(builder, i64Type, payloadAddress, "exception_payload_bits");
    LLVMValueRef payloadObject = LLVMBuildIntToPtr(builder, payloadBits, pointerType, "exception_payload_object");
    LLVMValueRef index = LLVMBuildLoad2(builder, i32Type, tlsIndex, "filter_tls_index");
    LLVMValueRef tlsArguments[2] = {index, payloadObject};
    LLVMValueRef stored = LLVMBuildCall2(
        builder, tlsSetValueType, tlsSetValue, tlsArguments, 2, "filter_exception_published");
    LLVMValueRef storeOk = LLVMBuildICmp(
        builder, LLVMIntNE, stored, LLVMConstInt(i32Type, 0, 0), "filter_publish_ok");
    LLVMValueRef result = LLVMBuildZExt(builder, storeOk, i32Type, "filter_result");
    LLVMBuildRet(builder, result);

    LLVMPositionBuilderAtEnd(builder, reject);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 0, 0));

    LLVMDisposeBuilder(builder);
    return true;
}
}

std::unique_ptr<llvm_backend::LlvmModule> WindowsRuntimeBuilder::build(std::string& error) const
{
    return buildInternal("Main", true, error);
}

std::unique_ptr<llvm_backend::LlvmModule> WindowsRuntimeBuilder::build(
    std::string_view mainSymbol,
    std::string& error) const
{
    return buildInternal(mainSymbol, true, error);
}

std::unique_ptr<llvm_backend::LlvmModule> WindowsRuntimeBuilder::buildSupport(std::string& error) const
{
    return buildInternal({}, false, error);
}

std::unique_ptr<llvm_backend::LlvmModule> WindowsRuntimeBuilder::buildEntry(
    std::string_view mainSymbol,
    std::string& error) const
{
    error.clear();
    if (mainSymbol.empty())
    {
        error = "NativeSharp Windows runtime entry symbol is empty";
        return nullptr;
    }
    LLVMContextRef context = LLVMContextCreate();
    if (context == nullptr)
    {
        error = "LLVMContextCreate failed for the NativeSharp Windows runtime entry";
        return nullptr;
    }
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("nativesharp.windows.entry", context);
    if (module == nullptr)
    {
        LLVMContextDispose(context);
        error = "LLVMModuleCreateWithNameInContext failed for the NativeSharp Windows runtime entry";
        return nullptr;
    }
    const std::string triple(target::TargetInfo::WindowsX64().llvmTriple());
    LLVMSetTarget(module, triple.c_str());
    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef exitProcessType = functionType(voidType, {i32Type});
    LLVMValueRef exitProcess = LLVMAddFunction(module, "ExitProcess", exitProcessType);
    LLVMTypeRef ehInitType = functionType(i32Type, {});
    LLVMValueRef ehInit = LLVMAddFunction(module, "__nativesharp_eh_init", ehInitType);
    if (exitProcess == nullptr || ehInit == nullptr ||
        !buildStart(context, module, exitProcess, exitProcessType, ehInit, ehInitType, mainSymbol))
    {
        LLVMDisposeModule(module);
        LLVMContextDispose(context);
        error = "failed to construct the NativeSharp Windows runtime entry";
        return nullptr;
    }
    std::unique_ptr<llvm_backend::LlvmModule> result(
        new llvm_backend::LlvmModule(context, module));
    std::string verificationError;
    if (!result->verify(verificationError))
    {
        error = "NativeSharp Windows runtime entry LLVM verification failed: " + verificationError;
        return nullptr;
    }
    if (!verifyRuntimeAbiContract(result->text(), RuntimeAbiImageKind::EntryPoint, verificationError))
    {
        error = "NativeSharp Windows runtime entry ABI contract verification failed: " + verificationError;
        return nullptr;
    }
    return result;
}

std::unique_ptr<llvm_backend::LlvmModule> WindowsRuntimeBuilder::buildDynamicLibraryAnchor(
    std::string& error) const
{
    error.clear();
    LLVMContextRef context = LLVMContextCreate();
    if (context == nullptr)
    {
        error = "LLVMContextCreate failed for the NativeSharp Windows DLL anchor";
        return nullptr;
    }
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext(
        "nativesharp.windows.dll.anchor", context);
    if (module == nullptr)
    {
        LLVMContextDispose(context);
        error = "LLVMModuleCreateWithNameInContext failed for the NativeSharp Windows DLL anchor";
        return nullptr;
    }
    const std::string triple(target::TargetInfo::WindowsX64().llvmTriple());
    LLVMSetTarget(module, triple.c_str());
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMValueRef anchor = LLVMAddGlobal(module, i8Type, "__nsc_dll_anchor");
    LLVMSetInitializer(anchor, LLVMConstInt(i8Type, 0, 0));
    LLVMSetGlobalConstant(anchor, 1);

    std::unique_ptr<llvm_backend::LlvmModule> result(
        new llvm_backend::LlvmModule(context, module));
    std::string verificationError;
    if (!result->verify(verificationError))
    {
        error = "NativeSharp Windows DLL anchor LLVM verification failed: " + verificationError;
        return nullptr;
    }
    return result;
}

std::unique_ptr<llvm_backend::LlvmModule> WindowsRuntimeBuilder::buildInternal(
    std::string_view mainSymbol,
    bool includeStart,
    std::string& error) const
{
    error.clear();
    if (includeStart && mainSymbol.empty())
    {
        error = "NativeSharp Windows runtime entry symbol is empty";
        return nullptr;
    }

    LLVMContextRef context = LLVMContextCreate();
    if (context == nullptr)
    {
        error = "LLVMContextCreate failed for the NativeSharp Windows runtime";
        return nullptr;
    }

    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("nativesharp.windows.runtime", context);
    if (module == nullptr)
    {
        LLVMContextDispose(context);
        error = "LLVMModuleCreateWithNameInContext failed for the NativeSharp Windows runtime";
        return nullptr;
    }

    const std::string triple(target::TargetInfo::WindowsX64().llvmTriple());
    LLVMSetTarget(module, triple.c_str());

    LLVMTypeRef voidType = LLVMVoidTypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef pointerType = LLVMPointerTypeInContext(context, 0);

    LLVMTypeRef exitProcessType = functionType(voidType, {i32Type});
    LLVMValueRef exitProcess = LLVMAddFunction(module, "ExitProcess", exitProcessType);
    LLVMTypeRef getProcessHeapType = functionType(pointerType, {});
    LLVMValueRef getProcessHeap = LLVMAddFunction(module, "GetProcessHeap", getProcessHeapType);
    LLVMTypeRef heapAllocType = functionType(pointerType, {pointerType, i32Type, i64Type});
    LLVMValueRef heapAlloc = LLVMAddFunction(module, "HeapAlloc", heapAllocType);
    LLVMTypeRef heapFreeType = functionType(i32Type, {pointerType, i32Type, pointerType});
    LLVMValueRef heapFree = LLVMAddFunction(module, "HeapFree", heapFreeType);
    LLVMTypeRef getStdHandleType = functionType(pointerType, {i32Type});
    LLVMValueRef getStdHandle = LLVMAddFunction(module, "GetStdHandle", getStdHandleType);
    LLVMTypeRef writeFileType = functionType(
        i32Type,
        {pointerType, pointerType, i32Type, pointerType, pointerType});
    LLVMValueRef writeFile = LLVMAddFunction(module, "WriteFile", writeFileType);

    // Stage 53.1 "Time". All three take one pointer (an "out parameter" in Win32 terms) --
    // QueryPerformanceCounter/QueryPerformanceFrequency additionally return a BOOL success flag
    // (kept in their declared type for ABI correctness even though buildOutParamI64Call never
    // reads it -- see the comment there for why that is deliberate); GetSystemTimeAsFileTime
    // returns nothing.
    LLVMTypeRef queryPerformanceCounterType = functionType(i32Type, {pointerType});
    LLVMValueRef queryPerformanceCounter = LLVMAddFunction(
        module, "QueryPerformanceCounter", queryPerformanceCounterType);
    LLVMTypeRef queryPerformanceFrequencyType = functionType(i32Type, {pointerType});
    LLVMValueRef queryPerformanceFrequency = LLVMAddFunction(
        module, "QueryPerformanceFrequency", queryPerformanceFrequencyType);
    LLVMTypeRef getSystemTimeAsFileTimeType = functionType(voidType, {pointerType});
    LLVMValueRef getSystemTimeAsFileTime = LLVMAddFunction(
        module, "GetSystemTimeAsFileTime", getSystemTimeAsFileTimeType);

    // Stage 53.2 "Environment". GetTickCount64 and GetActiveProcessorCount both return their
    // result directly (no out-parameter pointer, unlike the Stage 53.1 trio above) -- Exit
    // reuses the already-declared ExitProcess/exitProcessType from above, nothing new to
    // declare for it.
    LLVMTypeRef i16Type = LLVMInt16TypeInContext(context);
    LLVMTypeRef getTickCount64Type = functionType(i64Type, {});
    LLVMValueRef getTickCount64 = LLVMAddFunction(module, "GetTickCount64", getTickCount64Type);
    LLVMTypeRef getActiveProcessorCountType = functionType(i32Type, {i16Type});
    LLVMValueRef getActiveProcessorCount = LLVMAddFunction(
        module, "GetActiveProcessorCount", getActiveProcessorCountType);

    // Stage 53.2b "Environment (variables)". Both are the ANSI (*A) Win32 variants -- see the
    // comment on buildNullTerminatedCopy above for why ANSI, not the *W (UTF-16) ones.
    LLVMTypeRef getEnvironmentVariableAType = functionType(i32Type, {pointerType, pointerType, i32Type});
    LLVMValueRef getEnvironmentVariableA = LLVMAddFunction(
        module, "GetEnvironmentVariableA", getEnvironmentVariableAType);
    LLVMTypeRef setEnvironmentVariableAType = functionType(i32Type, {pointerType, pointerType});
    LLVMValueRef setEnvironmentVariableA = LLVMAddFunction(
        module, "SetEnvironmentVariableA", setEnvironmentVariableAType);

    // Stage 53.3 "Filesystem/Paths". The legacy mutation hooks remain ANSI. Existence checks use
    // GetFileAttributesW after an explicit UTF-8 -> UTF-16 conversion so compiler source paths
    // are not constrained by the active Windows code page.
    LLVMTypeRef getFileAttributesAType = functionType(i32Type, {pointerType});
    LLVMValueRef getFileAttributesA = LLVMAddFunction(
        module, "GetFileAttributesA", getFileAttributesAType);
    LLVMTypeRef getFileAttributesWType = functionType(i32Type, {pointerType});
    LLVMValueRef getFileAttributesW = LLVMAddFunction(
        module, "GetFileAttributesW", getFileAttributesWType);
    LLVMTypeRef deleteFileAType = functionType(i32Type, {pointerType});
    LLVMValueRef deleteFileA = LLVMAddFunction(module, "DeleteFileA", deleteFileAType);
    LLVMTypeRef createDirectoryAType = functionType(i32Type, {pointerType, pointerType});
    LLVMValueRef createDirectoryA = LLVMAddFunction(
        module, "CreateDirectoryA", createDirectoryAType);

    // Stage 53.3b "Filesystem/Paths (contenido de archivo)". CreateFileA/CreateFileW return HANDLEs
    // pointer in the C API, even though it is opaque); GetFileSizeEx/ReadFile/CloseHandle/
    // SetFilePointer round out the sequence buildFileReadAllText/buildFileWriteAllText/
    // buildFileAppendAllText need. WriteFile itself needs no new declaration -- it is already
    // declared above (`writeFile`/`writeFileType`) for Console.WriteLine, and its real Win32
    // signature is exactly what these three new wrappers need too.
    LLVMTypeRef createFileAType = functionType(
        pointerType, {pointerType, i32Type, i32Type, pointerType, i32Type, i32Type, pointerType});
    LLVMValueRef createFileA = LLVMAddFunction(module, "CreateFileA", createFileAType);
    LLVMTypeRef createFileWType = functionType(
        pointerType, {pointerType, i32Type, i32Type, pointerType, i32Type, i32Type, pointerType});
    LLVMValueRef createFileW = LLVMAddFunction(module, "CreateFileW", createFileWType);
    LLVMTypeRef getFileSizeExType = functionType(i32Type, {pointerType, pointerType});
    LLVMValueRef getFileSizeEx = LLVMAddFunction(module, "GetFileSizeEx", getFileSizeExType);
    LLVMTypeRef readFileType = functionType(
        i32Type, {pointerType, pointerType, i32Type, pointerType, pointerType});
    LLVMValueRef readFile = LLVMAddFunction(module, "ReadFile", readFileType);
    LLVMTypeRef closeHandleType = functionType(i32Type, {pointerType});
    LLVMValueRef closeHandle = LLVMAddFunction(module, "CloseHandle", closeHandleType);
    LLVMTypeRef setFilePointerType = functionType(i32Type, {pointerType, i32Type, pointerType, i32Type});
    LLVMValueRef setFilePointer = LLVMAddFunction(module, "SetFilePointer", setFilePointerType);

    // Stage 53.4 "Filesystem/Paths (cierre)". RemoveDirectoryA: BOOL, un LPCSTR, discarded by
    // buildDirectoryDelete, misma forma que DeleteFileA arriba. GetFullPathNameW: DWORD, cuatro
    // parametros (ver el comentario de cabecera de buildPathGetFullPath para el detalle
    // completo). Ambas ANSI (*A), misma razon de siempre.
    LLVMTypeRef removeDirectoryAType = functionType(i32Type, {pointerType});
    LLVMValueRef removeDirectoryA = LLVMAddFunction(module, "RemoveDirectoryA", removeDirectoryAType);
    LLVMTypeRef getFullPathNameWType = functionType(
        i32Type, {pointerType, i32Type, pointerType, pointerType});
    LLVMValueRef getFullPathNameW = LLVMAddFunction(module, "GetFullPathNameW", getFullPathNameWType);

    // Bloque 4, fase A. GetCurrentDirectoryA es el UNICO import de Win32 que esta fase anade: las
    // otras tres operaciones (ReadAllBytes/ReadBytes/WriteAllBytes) reutilizan CreateFileA,
    // GetFileSizeEx, ReadFile, WriteFile y CloseHandle, que ya estaban aqui para las variantes de
    // texto. Su forma es la del "pregunta el tamano primero" de GetFullPathNameW, con un
    // parametro menos:
    //
    //   DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer);
    LLVMTypeRef getCurrentDirectoryAType = functionType(i32Type, {i32Type, pointerType});
    LLVMValueRef getCurrentDirectoryA = LLVMAddFunction(module, "GetCurrentDirectoryA", getCurrentDirectoryAType);

    // Bloque 4, fase C -- Process.Run. CreateProcessA tiene DIEZ parametros; GetExitCodeProcess,
    // dos. WaitForSingleObject y CloseHandle ya estaban (los hilos de Stage 54.1 los importan).
    //
    //   BOOL CreateProcessA(LPCSTR app, LPSTR cmd, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
    //                       BOOL inherit, DWORD flags, LPVOID env, LPCSTR cwd,
    //                       LPSTARTUPINFOA, LPPROCESS_INFORMATION);
    LLVMTypeRef createProcessAType = functionType(i32Type, {
        pointerType, pointerType, pointerType, pointerType, i32Type,
        i32Type, pointerType, pointerType, pointerType, pointerType});
    LLVMValueRef createProcessA = LLVMAddFunction(module, "CreateProcessA", createProcessAType);
    LLVMTypeRef getExitCodeProcessType = functionType(i32Type, {pointerType, pointerType});
    LLVMValueRef getExitCodeProcess = LLVMAddFunction(module, "GetExitCodeProcess", getExitCodeProcessType);

    // Stage 53.5/T46 command-line arguments. GetCommandLineW/CommandLineToArgvW/WideCharToMultiByte/
    // LocalFree -- see buildEnvironmentGetCommandLineArgs's header comment for why this round
    // deliberately uses UTF-16 Win32 calls and preserves all arguments as UTF-8.
    // CommandLineToArgvW lives in SHELL32.dll (not kernel32.dll like every other Win32 function
    // this runtime calls) -- its import comes from a separate import library, wired up alongside
    // KERNELBASE.dll's below; declaring it here is unaffected by which DLL actually exports it,
    // exactly like every other extern function declaration in this module.
    LLVMTypeRef getCommandLineWType = functionType(pointerType, {});
    LLVMValueRef getCommandLineW = LLVMAddFunction(module, "GetCommandLineW", getCommandLineWType);
    LLVMTypeRef commandLineToArgvWType = functionType(pointerType, {pointerType, pointerType});
    LLVMValueRef commandLineToArgvW = LLVMAddFunction(
        module, "CommandLineToArgvW", commandLineToArgvWType);
    LLVMTypeRef wideCharToMultiByteType = functionType(
        i32Type,
        {i32Type, i32Type, pointerType, i32Type, pointerType, i32Type, pointerType, pointerType});
    LLVMValueRef wideCharToMultiByte = LLVMAddFunction(
        module, "WideCharToMultiByte", wideCharToMultiByteType);
    LLVMTypeRef multiByteToWideCharType = functionType(
        i32Type, {i32Type, i32Type, pointerType, i32Type, pointerType, i32Type});
    LLVMValueRef multiByteToWideChar = LLVMAddFunction(
        module, "MultiByteToWideChar", multiByteToWideCharType);
    LLVMTypeRef localFreeType = functionType(pointerType, {pointerType});
    LLVMValueRef localFree = LLVMAddFunction(module, "LocalFree", localFreeType);

    // Stage 53.6/T46 directory enumeration. FindFirstFileW/FindNextFileW preserve the compiler's
    // UTF-8 paths through an explicit UTF-16 Windows boundary. FindFirstFileW returns a HANDLE
    // (a search handle, not a file handle) checked for INVALID_HANDLE_VALUE; its second parameter
    // is an out-pointer to a 592-byte WIN32_FIND_DATAW struct (see buildDirectorySearchPattern/
    // buildDirectoryGetEntryCount below for the exact layout this runtime relies on).
    // FindNextFileW reuses the same search handle and struct pointer, returning BOOL (0 means
    // "no more entries", the loop's real termination condition -- NOT an error the way a
    // discarded BOOL elsewhere in this file usually means "ignored"). FindClose releases the
    // search handle -- a different release function from CloseHandle above, because a search
    // handle is not a file handle.
    LLVMTypeRef findFirstFileWType = functionType(pointerType, {pointerType, pointerType});
    LLVMValueRef findFirstFileW = LLVMAddFunction(module, "FindFirstFileW", findFirstFileWType);
    LLVMTypeRef findNextFileWType = functionType(i32Type, {pointerType, pointerType});
    LLVMValueRef findNextFileW = LLVMAddFunction(module, "FindNextFileW", findNextFileWType);
    LLVMTypeRef findCloseType = functionType(i32Type, {pointerType});
    LLVMValueRef findClose = LLVMAddFunction(module, "FindClose", findCloseType);

    // Stage 54.1 "Threads". CreateThread: HANDLE CreateThread(LPSECURITY_ATTRIBUTES,
    // SIZE_T dwStackSize, LPTHREAD_START_ROUTINE, LPVOID lpParameter, DWORD dwCreationFlags,
    // LPDWORD lpThreadId) -- SIZE_T and the two LP*/HANDLE-shaped parameters/result are pointer-
    // sized (i64/ptr on this target), dwCreationFlags is a plain DWORD (i32). See
    // buildThreadCreate below for how each argument is filled in. WaitForSingleObject: DWORD
    // WaitForSingleObject(HANDLE, DWORD dwMilliseconds) -- the first genuinely new
    // synchronization primitive this runtime declares (everything up through Stage 53 was
    // single-threaded by construction, so nothing needed to wait on anything).
    LLVMTypeRef createThreadType = functionType(
        pointerType, {pointerType, i64Type, pointerType, pointerType, i32Type, pointerType});
    LLVMValueRef createThread = LLVMAddFunction(module, "CreateThread", createThreadType);
    LLVMTypeRef waitForSingleObjectType = functionType(i32Type, {pointerType, i32Type});
    LLVMValueRef waitForSingleObject = LLVMAddFunction(
        module, "WaitForSingleObject", waitForSingleObjectType);

    // Stage 54.3 "Mutex". CreateMutexA: HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES
    // lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName) -- ANSI (*A), same reason as every
    // other Win32 string-taking call this runtime makes (see buildNullTerminatedCopy's comment),
    // though buildMutexCreate below always passes NULL for lpName (an anonymous/unnamed mutex),
    // so no actual ANSI string ever crosses this boundary. ReleaseMutex: BOOL
    // ReleaseMutex(HANDLE hMutex) -- releases ONE level of ownership a thread previously acquired
    // via WaitForSingleObject (which buildMutexLock below reuses directly -- Mutex needs no
    // separate "wait" declaration of its own, Stage 54.1's waitForSingleObject/
    // waitForSingleObjectType above already cover it). WaitForSingleObject/CloseHandle themselves
    // need no new declaration here -- see buildMutexLock/buildMutexClose's own header comments for
    // why both are reused as-is from Stage 54.1/53.3b.
    LLVMTypeRef createMutexAType = functionType(pointerType, {pointerType, i32Type, pointerType});
    LLVMValueRef createMutexA = LLVMAddFunction(module, "CreateMutexA", createMutexAType);
    LLVMTypeRef releaseMutexType = functionType(i32Type, {pointerType});
    LLVMValueRef releaseMutex = LLVMAddFunction(module, "ReleaseMutex", releaseMutexType);

    // Stage 54.3 "Mutex" (fix). GetCurrentThreadId: DWORD GetCurrentThreadId(void) -- see
    // buildGetCurrentThreadId's own header comment above for why this compiler needs it
    // internally (the type-initializer guard's same-thread-recursion check), independent of
    // Mutex itself.
    LLVMTypeRef getCurrentThreadIdType = functionType(i32Type, {});
    LLVMValueRef getCurrentThreadId = LLVMAddFunction(
        module, "GetCurrentThreadId", getCurrentThreadIdType);

    // Stage 54.4 "Condition Variables". AcquireSRWLockExclusive/ReleaseSRWLockExclusive: VOID
    // AcquireSRWLockExclusive(PSRWLOCK)/ReleaseSRWLockExclusive(PSRWLOCK) -- a bare pointer in,
    // VOID out, no ANSI/wide string variant to choose between (unlike almost every other Win32
    // call this runtime makes). WakeConditionVariable/WakeAllConditionVariable: VOID
    // WakeConditionVariable(PCONDITION_VARIABLE)/WakeAllConditionVariable(PCONDITION_VARIABLE) --
    // same one-pointer-in/void-out shape. SleepConditionVariableSRW: BOOL
    // SleepConditionVariableSRW(PCONDITION_VARIABLE, PSRWLOCK, DWORD dwMilliseconds, ULONG Flags)
    // -- the one function of this group that returns a real value (see buildMonitorWait above).
    // All five are kernel32.dll exports since Windows Vista (NOT declared anywhere in this runtime
    // before this round) -- see kernel32Symbols in the linker files/host-integration tests for the
    // synchronized five-location listing this round adds them to.
    LLVMTypeRef acquireSRWLockExclusiveType = functionType(voidType, {pointerType});
    LLVMValueRef acquireSRWLockExclusive = LLVMAddFunction(
        module, "AcquireSRWLockExclusive", acquireSRWLockExclusiveType);
    LLVMTypeRef releaseSRWLockExclusiveType = functionType(voidType, {pointerType});
    LLVMValueRef releaseSRWLockExclusive = LLVMAddFunction(
        module, "ReleaseSRWLockExclusive", releaseSRWLockExclusiveType);
    LLVMTypeRef wakeConditionVariableType = functionType(voidType, {pointerType});
    LLVMValueRef wakeConditionVariable = LLVMAddFunction(
        module, "WakeConditionVariable", wakeConditionVariableType);
    LLVMTypeRef wakeAllConditionVariableType = functionType(voidType, {pointerType});
    LLVMValueRef wakeAllConditionVariable = LLVMAddFunction(
        module, "WakeAllConditionVariable", wakeAllConditionVariableType);
    LLVMTypeRef sleepConditionVariableSRWType = functionType(
        i32Type, {pointerType, pointerType, i32Type, i32Type});
    LLVMValueRef sleepConditionVariableSRW = LLVMAddFunction(
        module, "SleepConditionVariableSRW", sleepConditionVariableSRWType);

    // Stage 54.5 "Otras primitivas de sincronizacion". CreateSemaphoreA: HANDLE
    // CreateSemaphoreA(LPSECURITY_ATTRIBUTES, LONG lInitialCount, LONG lMaximumCount, LPCSTR
    // lpName) -- ANSI (*A), same reason as every other Win32 string-taking call this runtime makes
    // (see buildNullTerminatedCopy's comment), though buildSemaphoreCreate below always passes
    // NULL for lpName. ReleaseSemaphore: BOOL ReleaseSemaphore(HANDLE, LONG lReleaseCount, LPLONG
    // lpPreviousCount). CreateEventA: HANDLE CreateEventA(LPSECURITY_ATTRIBUTES, BOOL
    // bManualReset, BOOL bInitialState, LPCSTR lpName) -- same NULL-name anonymous-object choice.
    // SetEvent/ResetEvent: BOOL SetEvent(HANDLE)/ResetEvent(HANDLE). WaitForSingleObject/
    // CloseHandle themselves need no new declaration here -- buildWaitForHandleTimeout/
    // buildSemaphoreClose/buildEventClose above all reuse the Stage 54.1/53.3b declarations
    // directly, same discipline as buildMutexLock/buildMutexClose.
    LLVMTypeRef createSemaphoreAType = functionType(
        pointerType, {pointerType, i32Type, i32Type, pointerType});
    LLVMValueRef createSemaphoreA = LLVMAddFunction(
        module, "CreateSemaphoreA", createSemaphoreAType);
    LLVMTypeRef releaseSemaphoreType = functionType(i32Type, {pointerType, i32Type, pointerType});
    LLVMValueRef releaseSemaphore = LLVMAddFunction(module, "ReleaseSemaphore", releaseSemaphoreType);
    LLVMTypeRef createEventAType = functionType(
        pointerType, {pointerType, i32Type, i32Type, pointerType});
    LLVMValueRef createEventA = LLVMAddFunction(module, "CreateEventA", createEventAType);
    LLVMTypeRef setEventType = functionType(i32Type, {pointerType});
    LLVMValueRef setEvent = LLVMAddFunction(module, "SetEvent", setEventType);
    LLVMTypeRef resetEventType = functionType(i32Type, {pointerType});
    LLVMValueRef resetEvent = LLVMAddFunction(module, "ResetEvent", resetEventType);

    // Stage 55.1 "Process". GetCurrentProcessId/GetCurrentProcess: DWORD
    // GetCurrentProcessId(void) / HANDLE GetCurrentProcess(void) -- GetCurrentProcess always
    // returns the same pseudo-handle by Win32 contract, called fresh inside every wrapper that
    // needs it rather than cached, see buildProcessGetCurrentProcessId's header comment.
    // K32GetProcessMemoryInfo: BOOL K32GetProcessMemoryInfo(HANDLE, PPROCESS_MEMORY_COUNTERS,
    // DWORD cb) -- this is the kernel32.dll-exported alias Microsoft added circa Vista precisely
    // so callers do not need psapi.lib/Psapi.dll (see buildProcessGetWorkingSetSize). GetProcessTimes:
    // BOOL GetProcessTimes(HANDLE, LPFILETIME creation, LPFILETIME exit, LPFILETIME kernel,
    // LPFILETIME user). GetPriorityClass/SetPriorityClass: DWORD GetPriorityClass(HANDLE) / BOOL
    // SetPriorityClass(HANDLE, DWORD).
    LLVMTypeRef getCurrentProcessIdType = functionType(i32Type, {});
    LLVMValueRef getCurrentProcessId = LLVMAddFunction(module, "GetCurrentProcessId", getCurrentProcessIdType);
    LLVMTypeRef getCurrentProcessType = functionType(pointerType, {});
    LLVMValueRef getCurrentProcess = LLVMAddFunction(module, "GetCurrentProcess", getCurrentProcessType);
    LLVMTypeRef getProcessMemoryInfoType = functionType(i32Type, {pointerType, pointerType, i32Type});
    LLVMValueRef getProcessMemoryInfo = LLVMAddFunction(
        module, "K32GetProcessMemoryInfo", getProcessMemoryInfoType);
    LLVMTypeRef getProcessTimesType = functionType(
        i32Type, {pointerType, pointerType, pointerType, pointerType, pointerType});
    LLVMValueRef getProcessTimes = LLVMAddFunction(module, "GetProcessTimes", getProcessTimesType);
    LLVMTypeRef getPriorityClassType = functionType(i32Type, {pointerType});
    LLVMValueRef getPriorityClass = LLVMAddFunction(module, "GetPriorityClass", getPriorityClassType);
    LLVMTypeRef setPriorityClassType = functionType(i32Type, {pointerType, i32Type});
    LLVMValueRef setPriorityClass = LLVMAddFunction(module, "SetPriorityClass", setPriorityClassType);

    // Stage 55.2 "Hardware / System Info". GetSystemInfo: VOID GetSystemInfo(LPSYSTEM_INFO) --
    // no return value, unlike almost every other Win32 call this runtime makes.
    // GlobalMemoryStatusEx: BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX) -- the one struct-out-
    // param call in this runtime that requires a field (dwLength) written BEFORE the call, not
    // just read after (see buildSystemInfoGetTotalPhysicalMemoryBytes's header comment).
    LLVMTypeRef getSystemInfoType = functionType(voidType, {pointerType});
    LLVMValueRef getSystemInfo = LLVMAddFunction(module, "GetSystemInfo", getSystemInfoType);
    LLVMTypeRef globalMemoryStatusExType = functionType(i32Type, {pointerType});
    LLVMValueRef globalMemoryStatusEx = LLVMAddFunction(
        module, "GlobalMemoryStatusEx", globalMemoryStatusExType);

    // Stage 55.3 "Diagnostics". OutputDebugStringA: VOID OutputDebugStringA(LPCSTR) -- ANSI,
    // same reason as every other Win32 string-taking call this runtime makes (see
    // buildNullTerminatedCopy's comment). IsDebuggerPresent: BOOL IsDebuggerPresent(void), no
    // arguments. RtlCaptureStackBackTrace: USHORT RtlCaptureStackBackTrace(DWORD FramesToSkip,
    // DWORD FramesToCapture, PVOID *BackTrace, PDWORD BackTraceHash) -- the Windows SDK's
    // CaptureStackBackTrace macro resolves to this exact kernel32.dll export; its USHORT return
    // is declared i16 here for ABI correctness, then zero-extended to builtin:long by the wrapper
    // (see buildDebugCaptureStackFrameCount).
    LLVMTypeRef outputDebugStringAType = functionType(voidType, {pointerType});
    LLVMValueRef outputDebugStringA = LLVMAddFunction(
        module, "OutputDebugStringA", outputDebugStringAType);
    LLVMTypeRef isDebuggerPresentType = functionType(i32Type, {});
    LLVMValueRef isDebuggerPresent = LLVMAddFunction(module, "IsDebuggerPresent", isDebuggerPresentType);
    LLVMTypeRef rtlCaptureStackBackTraceType = functionType(
        i16Type, {i32Type, i32Type, pointerType, pointerType});
    LLVMValueRef rtlCaptureStackBackTrace = LLVMAddFunction(
        module, "RtlCaptureStackBackTrace", rtlCaptureStackBackTraceType);

    // Pre-Stage-56 "utilidades basicas" round. Sleep: VOID Sleep(DWORD dwMilliseconds) -- no
    // return value, same shape as GetSystemInfo above. CopyFileA: BOOL CopyFileA(LPCSTR
    // lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists) -- ANSI, same reason as every
    // other Win32 string-taking call this runtime makes (see buildNullTerminatedCopy's comment).
    // MoveFileA: BOOL MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName) -- shared by
    // File.MoveRaw and Directory.MoveRaw (see buildMoveFile's own header comment above).
    LLVMTypeRef sleepType = functionType(voidType, {i32Type});
    LLVMValueRef sleep = LLVMAddFunction(module, "Sleep", sleepType);
    LLVMTypeRef copyFileAType = functionType(i32Type, {pointerType, pointerType, i32Type});
    LLVMValueRef copyFileA = LLVMAddFunction(module, "CopyFileA", copyFileAType);
    LLVMTypeRef moveFileAType = functionType(i32Type, {pointerType, pointerType});
    LLVMValueRef moveFileA = LLVMAddFunction(module, "MoveFileA", moveFileAType);

    LLVMTypeRef tlsAllocType = functionType(i32Type, {});
    LLVMValueRef tlsAlloc = LLVMAddFunction(module, "TlsAlloc", tlsAllocType);
    LLVMTypeRef tlsGetValueType = functionType(pointerType, {i32Type});
    LLVMValueRef tlsGetValue = LLVMAddFunction(module, "TlsGetValue", tlsGetValueType);
    LLVMTypeRef tlsSetValueType = functionType(i32Type, {i32Type, pointerType});
    LLVMValueRef tlsSetValue = LLVMAddFunction(module, "TlsSetValue", tlsSetValueType);
    LLVMTypeRef raiseExceptionType = functionType(voidType, {i32Type, i32Type, i32Type, pointerType});
    LLVMValueRef raiseException = LLVMAddFunction(module, "RaiseException", raiseExceptionType);

    LLVMValueRef fltused = LLVMAddGlobal(module, i32Type, "_fltused");
    LLVMSetInitializer(fltused, LLVMConstInt(i32Type, 0, 0));
    LLVMValueRef exceptionTlsIndex = LLVMAddGlobal(module, i32Type, "__nativesharp_exception_tls_index");
    LLVMSetInitializer(exceptionTlsIndex, LLVMConstInt(i32Type, kTlsOutOfIndexes, 0));

    LLVMValueRef ehInit = nullptr;
    LLVMTypeRef ehInitType = functionType(i32Type, {});
    const bool builtEhInit = buildEhInit(
        context,
        module,
        exceptionTlsIndex,
        tlsAlloc,
        tlsAllocType,
        ehInit);

    // Stage 54.1 "Threads". The trampoline must exist BEFORE buildThreadCreate runs, since
    // buildThreadCreate bakes the trampoline's LLVMValueRef directly into CreateThread's
    // lpStartAddress argument -- see buildThreadTrampolineFunction's own header comment for why
    // this ordering is required and why the trampoline itself needs no linker symbol.
    LLVMValueRef threadTrampoline = buildThreadTrampolineFunction(context, module);

    const bool built =
        exitProcess != nullptr &&
        getProcessHeap != nullptr &&
        heapAlloc != nullptr &&
        heapFree != nullptr &&
        getStdHandle != nullptr &&
        writeFile != nullptr &&
        queryPerformanceCounter != nullptr &&
        queryPerformanceFrequency != nullptr &&
        getSystemTimeAsFileTime != nullptr &&
        getTickCount64 != nullptr &&
        getActiveProcessorCount != nullptr &&
        getEnvironmentVariableA != nullptr &&
        setEnvironmentVariableA != nullptr &&
        getFileAttributesA != nullptr &&
        getFileAttributesW != nullptr &&
        deleteFileA != nullptr &&
        createDirectoryA != nullptr &&
        createFileA != nullptr &&
        createFileW != nullptr &&
        getFileSizeEx != nullptr &&
        readFile != nullptr &&
        closeHandle != nullptr &&
        setFilePointer != nullptr &&
        removeDirectoryA != nullptr &&
        getFullPathNameW != nullptr &&
        getCommandLineW != nullptr &&
        commandLineToArgvW != nullptr &&
        wideCharToMultiByte != nullptr &&
        multiByteToWideChar != nullptr &&
        localFree != nullptr &&
        findFirstFileW != nullptr &&
        findNextFileW != nullptr &&
        findClose != nullptr &&
        createThread != nullptr &&
        waitForSingleObject != nullptr &&
        createMutexA != nullptr &&
        releaseMutex != nullptr &&
        getCurrentThreadId != nullptr &&
        acquireSRWLockExclusive != nullptr &&
        releaseSRWLockExclusive != nullptr &&
        wakeConditionVariable != nullptr &&
        wakeAllConditionVariable != nullptr &&
        sleepConditionVariableSRW != nullptr &&
        createSemaphoreA != nullptr &&
        releaseSemaphore != nullptr &&
        createEventA != nullptr &&
        setEvent != nullptr &&
        resetEvent != nullptr &&
        getCurrentProcessId != nullptr &&
        getCurrentProcess != nullptr &&
        getProcessMemoryInfo != nullptr &&
        getProcessTimes != nullptr &&
        getPriorityClass != nullptr &&
        setPriorityClass != nullptr &&
        getSystemInfo != nullptr &&
        globalMemoryStatusEx != nullptr &&
        outputDebugStringA != nullptr &&
        isDebuggerPresent != nullptr &&
        rtlCaptureStackBackTrace != nullptr &&
        sleep != nullptr &&
        copyFileA != nullptr &&
        moveFileA != nullptr &&
        threadTrampoline != nullptr &&
        tlsAlloc != nullptr &&
        tlsGetValue != nullptr &&
        tlsSetValue != nullptr &&
        raiseException != nullptr &&
        exceptionTlsIndex != nullptr &&
        builtEhInit &&
        ehInit != nullptr &&
        buildExceptionCurrent(context, module, exceptionTlsIndex, tlsGetValue, tlsGetValueType) &&
        buildExceptionClear(context, module, exceptionTlsIndex, tlsSetValue, tlsSetValueType) &&
        buildThrow(
            context,
            module,
            exceptionTlsIndex,
            ehInit,
            ehInitType,
            tlsSetValue,
            tlsSetValueType,
            raiseException,
            raiseExceptionType,
            exitProcess,
            exitProcessType) &&
        buildEhFilter(
            context, module, exceptionTlsIndex, ehInit, ehInitType, tlsSetValue, tlsSetValueType) &&
        buildAllocate(context, module, getProcessHeap, getProcessHeapType, heapAlloc, heapAllocType) &&
        buildFree(context, module, getProcessHeap, getProcessHeapType, heapFree, heapFreeType) &&
        buildTrap(context, module, exitProcess, exitProcessType) &&
        buildHalfCompilerRtSupport(context, module) &&
        (!includeStart || buildStart(
            context, module, exitProcess, exitProcessType, ehInit, ehInitType, mainSymbol)) &&
        buildConsoleWrite(context, module, getStdHandle, getStdHandleType, writeFile, writeFileType) &&
        buildPuts(context, module, getStdHandle, getStdHandleType, writeFile, writeFileType) &&
        buildClockQueryCounter(context, module, queryPerformanceCounter, queryPerformanceCounterType) &&
        buildClockQueryFrequency(context, module, queryPerformanceFrequency, queryPerformanceFrequencyType) &&
        buildClockSystemFileTime(context, module, getSystemTimeAsFileTime, getSystemTimeAsFileTimeType) &&
        buildEnvironmentTickCount64(context, module, getTickCount64, getTickCount64Type) &&
        buildEnvironmentProcessorCount(context, module, getActiveProcessorCount, getActiveProcessorCountType) &&
        buildEnvironmentExit(context, module, exitProcess, exitProcessType) &&
        buildEnvironmentGetVariable(
            context, module, getProcessHeap, getProcessHeapType, heapAlloc, heapAllocType,
            getEnvironmentVariableA, getEnvironmentVariableAType) &&
        buildEnvironmentSetVariable(context, module, setEnvironmentVariableA, setEnvironmentVariableAType) &&
        buildFileExists(
            context, module, getFileAttributesW, getFileAttributesWType,
            multiByteToWideChar, multiByteToWideCharType) &&
        buildDirectoryExists(
            context, module, getFileAttributesW, getFileAttributesWType,
            multiByteToWideChar, multiByteToWideCharType) &&
        buildFileDelete(context, module, deleteFileA, deleteFileAType) &&
        buildDirectoryCreate(context, module, createDirectoryA, createDirectoryAType) &&
        buildFileReadAllText(
            context, module, getProcessHeap, getProcessHeapType, heapAlloc, heapAllocType,
            createFileW, createFileWType, multiByteToWideChar, multiByteToWideCharType,
            getFileSizeEx, getFileSizeExType,
            readFile, readFileType, closeHandle, closeHandleType) &&
        buildFileGetSize(
            context, module, createFileA, createFileAType, getFileSizeEx, getFileSizeExType,
            closeHandle, closeHandleType) &&
        buildFileReadAllBytes(
            context, module, getProcessHeap, getProcessHeapType, heapAlloc, heapAllocType,
            createFileA, createFileAType, getFileSizeEx, getFileSizeExType,
            readFile, readFileType, closeHandle, closeHandleType) &&
        buildFileReadBytes(
            context, module, createFileA, createFileAType, getFileSizeEx, getFileSizeExType,
            readFile, readFileType, closeHandle, closeHandleType) &&
        buildFileWriteAllBytes(
            context, module, createFileA, createFileAType, writeFile, writeFileType,
            closeHandle, closeHandleType) &&
        buildEnvironmentCurrentDirectory(
            context, module, getProcessHeap, getProcessHeapType, heapAlloc, heapAllocType,
            getCurrentDirectoryA, getCurrentDirectoryAType) &&
        buildProcessRun(
            context, module, createFileA, createFileAType, createProcessA, createProcessAType,
            waitForSingleObject, waitForSingleObjectType,
            getExitCodeProcess, getExitCodeProcessType, closeHandle, closeHandleType) &&
        buildFileWriteAllText(
            context, module, createFileA, createFileAType, writeFile, writeFileType,
            closeHandle, closeHandleType) &&
        buildFileAppendAllText(
            context, module, createFileA, createFileAType, setFilePointer, setFilePointerType,
            writeFile, writeFileType, closeHandle, closeHandleType) &&
        buildDirectoryDelete(context, module, removeDirectoryA, removeDirectoryAType) &&
        buildPathGetFullPath(
            context, module, getProcessHeap, getProcessHeapType, heapAlloc, heapAllocType,
            getFullPathNameW, getFullPathNameWType,
            multiByteToWideChar, multiByteToWideCharType,
            wideCharToMultiByte, wideCharToMultiByteType) &&
        buildEnvironmentGetCommandLineArgCount(
            context, module, getCommandLineW, getCommandLineWType,
            commandLineToArgvW, commandLineToArgvWType, localFree, localFreeType) &&
        buildEnvironmentGetCommandLineArgs(
            context, module, getProcessHeap, getProcessHeapType, heapAlloc, heapAllocType,
            getCommandLineW, getCommandLineWType, commandLineToArgvW, commandLineToArgvWType,
            wideCharToMultiByte, wideCharToMultiByteType, localFree, localFreeType) &&
        buildDirectoryGetEntryCount(
            context, module, "__nativesharp_directory_get_file_count", false,
            findFirstFileW, findFirstFileWType, findNextFileW, findNextFileWType,
            findClose, findCloseType, multiByteToWideChar, multiByteToWideCharType) &&
        buildDirectoryGetEntryCount(
            context, module, "__nativesharp_directory_get_directory_count", true,
            findFirstFileW, findFirstFileWType, findNextFileW, findNextFileWType,
            findClose, findCloseType, multiByteToWideChar, multiByteToWideCharType) &&
        buildDirectoryGetEntries(
            context, module, "__nativesharp_directory_get_files", false,
            getProcessHeap, getProcessHeapType, heapAlloc, heapAllocType,
            findFirstFileW, findFirstFileWType, findNextFileW, findNextFileWType,
            findClose, findCloseType, multiByteToWideChar, multiByteToWideCharType,
            wideCharToMultiByte, wideCharToMultiByteType) &&
        buildDirectoryGetEntries(
            context, module, "__nativesharp_directory_get_directories", true,
            getProcessHeap, getProcessHeapType, heapAlloc, heapAllocType,
            findFirstFileW, findFirstFileWType, findNextFileW, findNextFileWType,
            findClose, findCloseType, multiByteToWideChar, multiByteToWideCharType,
            wideCharToMultiByte, wideCharToMultiByteType) &&
        buildThreadCreate(
            context, module, createThread, createThreadType, threadTrampoline) &&
        buildThreadJoin(
            context, module, waitForSingleObject, waitForSingleObjectType,
            closeHandle, closeHandleType) &&
        buildThreadDetach(context, module, closeHandle, closeHandleType) &&
        // Stage 54.2 "Atomics". None of these ten take any Win32 import as an argument -- see
        // buildInterlockedIncrementOrDecrement/buildInterlockedAdd/buildInterlockedExchange/
        // buildInterlockedCompareExchange's own header comment above for why: they emit real
        // atomic IR instructions directly, with no kernel32.dll dependency at all.
        buildAtomicLoad(context, module, "__nativesharp_atomic_load_int32", 32) &&
        buildAtomicStore(context, module, "__nativesharp_atomic_store_int32", 32) &&
        buildAtomicLoad(context, module, "__nativesharp_atomic_load_int64", 64) &&
        buildAtomicStore(context, module, "__nativesharp_atomic_store_int64", 64) &&
        buildInterlockedIncrementOrDecrement(
            context, module, "__nativesharp_interlocked_increment_int32", 32, true) &&
        buildInterlockedIncrementOrDecrement(
            context, module, "__nativesharp_interlocked_decrement_int32", 32, false) &&
        buildInterlockedIncrementOrDecrement(
            context, module, "__nativesharp_interlocked_increment_int64", 64, true) &&
        buildInterlockedIncrementOrDecrement(
            context, module, "__nativesharp_interlocked_decrement_int64", 64, false) &&
        buildInterlockedAdd(context, module, "__nativesharp_interlocked_add_int32", 32) &&
        buildInterlockedAdd(context, module, "__nativesharp_interlocked_add_int64", 64) &&
        buildInterlockedExchange(context, module, "__nativesharp_interlocked_exchange_int32", 32) &&
        buildInterlockedExchange(context, module, "__nativesharp_interlocked_exchange_int64", 64) &&
        buildInterlockedCompareExchange(
            context, module, "__nativesharp_interlocked_compare_exchange_int32", 32) &&
        buildInterlockedCompareExchange(
            context, module, "__nativesharp_interlocked_compare_exchange_int64", 64) &&
        // Stage 54.3 "Mutex". buildMutexLock reuses waitForSingleObject/waitForSingleObjectType
        // directly (Stage 54.1's declaration above) -- Mutex needs no separate WaitForSingleObject
        // import. buildMutexClose reuses closeHandle/closeHandleType directly (Stage 53.3b's
        // declaration above) -- same reason.
        buildMutexCreate(context, module, createMutexA, createMutexAType) &&
        buildMutexCreateNamed(context, module, createMutexA, createMutexAType) &&
        buildMutexLock(context, module, waitForSingleObject, waitForSingleObjectType) &&
        buildMutexUnlock(context, module, releaseMutex, releaseMutexType) &&
        buildMutexClose(context, module, closeHandle, closeHandleType) &&
        buildGetCurrentThreadId(context, module, getCurrentThreadId, getCurrentThreadIdType) &&
        // Stage 54.4 "Condition Variables".
        buildMonitorEnter(context, module, acquireSRWLockExclusive, acquireSRWLockExclusiveType) &&
        buildMonitorExit(context, module, releaseSRWLockExclusive, releaseSRWLockExclusiveType) &&
        buildMonitorPulse(context, module, wakeConditionVariable, wakeConditionVariableType) &&
        buildMonitorPulseAll(
            context, module, wakeAllConditionVariable, wakeAllConditionVariableType) &&
        buildMonitorWait(
            context, module, sleepConditionVariableSRW, sleepConditionVariableSRWType) &&
        // Stage 54.5 "Otras primitivas de sincronizacion". buildWaitForHandleTimeout reuses
        // waitForSingleObject/waitForSingleObjectType directly (Stage 54.1's declaration) --
        // Semaphore/Event need no separate WaitForSingleObject import. buildSemaphoreClose/
        // buildEventClose reuse closeHandle/closeHandleType directly (Stage 53.3b's declaration).
        buildWaitForHandleTimeout(context, module, waitForSingleObject, waitForSingleObjectType) &&
        buildSemaphoreCreate(context, module, createSemaphoreA, createSemaphoreAType) &&
        buildSemaphoreCreateNamed(context, module, createSemaphoreA, createSemaphoreAType) &&
        buildSemaphoreRelease(context, module, releaseSemaphore, releaseSemaphoreType) &&
        buildSemaphoreClose(context, module, closeHandle, closeHandleType) &&
        buildEventCreate(context, module, createEventA, createEventAType) &&
        buildEventCreateNamed(context, module, createEventA, createEventAType) &&
        buildEventSet(context, module, setEvent, setEventType) &&
        buildEventReset(context, module, resetEvent, resetEventType) &&
        buildEventClose(context, module, closeHandle, closeHandleType) &&
        // Stage 55.1 "Process".
        buildProcessGetCurrentProcessId(context, module, getCurrentProcessId, getCurrentProcessIdType) &&
        buildProcessGetWorkingSetSize(
            context, module, getCurrentProcess, getCurrentProcessType,
            getProcessMemoryInfo, getProcessMemoryInfoType) &&
        buildProcessGetKernelTime(
            context, module, getCurrentProcess, getCurrentProcessType,
            getProcessTimes, getProcessTimesType) &&
        buildProcessGetUserTime(
            context, module, getCurrentProcess, getCurrentProcessType,
            getProcessTimes, getProcessTimesType) &&
        buildProcessGetPriorityClass(
            context, module, getCurrentProcess, getCurrentProcessType,
            getPriorityClass, getPriorityClassType) &&
        buildProcessSetPriorityClass(
            context, module, getCurrentProcess, getCurrentProcessType,
            setPriorityClass, setPriorityClassType) &&
        // Stage 55.2 "Hardware / System Info".
        buildSystemInfoGetPageSize(context, module, getSystemInfo, getSystemInfoType) &&
        buildSystemInfoGetProcessorArchitecture(context, module, getSystemInfo, getSystemInfoType) &&
        buildSystemInfoGetAllocationGranularity(context, module, getSystemInfo, getSystemInfoType) &&
        buildSystemInfoGetTotalPhysicalMemoryBytes(
            context, module, globalMemoryStatusEx, globalMemoryStatusExType) &&
        buildSystemInfoGetAvailablePhysicalMemoryBytes(
            context, module, globalMemoryStatusEx, globalMemoryStatusExType) &&
        buildSystemInfoGetMemoryLoadPercent(
            context, module, globalMemoryStatusEx, globalMemoryStatusExType) &&
        // Stage 55.3 "Diagnostics".
        buildDebugIsDebuggerPresent(context, module, isDebuggerPresent, isDebuggerPresentType) &&
        buildDebugCaptureStackFrameCount(
            context, module, rtlCaptureStackBackTrace, rtlCaptureStackBackTraceType) &&
        buildDebugOutputString(context, module, outputDebugStringA, outputDebugStringAType) &&
        // Pre-Stage-56 "utilidades basicas" round.
        buildThreadSleep(context, module, sleep, sleepType) &&
        buildFileCopy(context, module, copyFileA, copyFileAType) &&
        buildMoveFile(context, module, moveFileA, moveFileAType);
    if (!built)
    {
        LLVMDisposeModule(module);
        LLVMContextDispose(context);
        error = "failed to construct one or more NativeSharp Windows runtime functions";
        return nullptr;
    }

    std::unique_ptr<llvm_backend::LlvmModule> result(
        new llvm_backend::LlvmModule(context, module));
    std::string verificationError;
    if (!result->verify(verificationError))
    {
        error = "NativeSharp Windows runtime LLVM verification failed: " + verificationError;
        return nullptr;
    }
    std::string contractError;
    const RuntimeAbiImageKind imageKind = includeStart
        ? RuntimeAbiImageKind::Executable
        : RuntimeAbiImageKind::SupportLibrary;
    if (!verifyRuntimeAbiContract(result->text(), imageKind, contractError))
    {
        error = "NativeSharp Windows runtime ABI contract verification failed: " + contractError;
        return nullptr;
    }
    return result;
}
}
