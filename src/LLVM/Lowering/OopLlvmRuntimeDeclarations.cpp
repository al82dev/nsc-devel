#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/LlvmInstrumentationSwitches.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool OopLlvmImplementation::declareRuntime(std::string& error)
{
        if (!declareRuntimeMemory(error)) return false;
        declareRuntimeConsoleTimeEnvironment();
        declareRuntimeFilesystem();
        declareRuntimeDirectoryAndThreads();
        declareRuntimeAtomics();
        declareRuntimeMutexAndMonitor();
        declareRuntimeSynchronization();
        declareRuntimeProcessSystemDiagnostics();
        declareRuntimeExceptionsAndIntrinsics();
        return runtimeDeclarationsValid(error);
}

bool OopLlvmImplementation::declareRuntimeMemory(std::string& error)
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        if (!ensureRuntimeDebugTrace(error)) return false;
        LLVMTypeRef trapParams[] = {i32};
        trapFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), trapParams, 1, 0);
        trapFunction_ = LLVMAddFunction(module_, kTrapFunctionName, trapFunctionType_);
        // `nounwind` is both TRUE (the trap terminates the process; it never raises) and
        // load-bearing for exception correctness. Without it, LLVM's inliner rewrites every trap
        // CALL that ends up inside a `try` into an INVOKE, which extends that try's protected
        // instruction range past the trap. The Windows personality treats the byte immediately
        // after a protected range as still inside it (scope-table LabelEnd is "last instruction
        // + 1"), so a catch handler laid out right after the range is seen as INSIDE the enclosing
        // scope -- and an enclosing `finally` or cleanup is then silently skipped. Keeping the trap
        // a plain call leaves its bytes OUTSIDE the range, which is exactly what pushes the handler
        // past LabelEnd. See lowerExceptionalTransfer's comment and
        // BUGS_ENCONTRADOS/bug_finally_saltado_por_layout_de_handler.md for the measurements,
        // including the clang reproduction of the same hazard.
        applyFunctionAttribute(trapFunction_, "nounwind");
        applyFunctionAttribute(trapFunction_, "noreturn");

        LLVMTypeRef allocParams[] = {i64};
        allocationFunctionType_ = LLVMFunctionType(ptr, allocParams, 1, 0);
        LLVMValueRef runtimeAllocationFunction = LLVMAddFunction(
            module_, kAllocationFunctionName, allocationFunctionType_);
        allocationFunction_ = runtimeAllocationFunction;

        LLVMTypeRef freeParams[] = {ptr};
        deallocationFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), freeParams, 1, 0);
        LLVMValueRef runtimeDeallocationFunction = LLVMAddFunction(
            module_, kDeallocationFunctionName, deallocationFunctionType_);
        deallocationFunction_ = runtimeDeallocationFunction;

        if (!declareRuntimeAllocatorDebug(
                runtimeAllocationFunction, runtimeDeallocationFunction, error)) return false;
        // NS304-T13-F07: stable allocator-deallocation ABI. Custom allocator objects preserve a
        // function with this shape in a per-object descriptor sidecar; the public object header
        // remains the historical single descriptor pointer.
        {
            LLVMTypeRef thunkParams[] = {ptr, ptr};
            allocatorFreeThunkType_ = LLVMFunctionType(
                LLVMVoidTypeInContext(context_), thunkParams, 2, 0);
        }
        return true;
}

bool OopLlvmImplementation::declareRuntimeAllocatorDebug(
        LLVMValueRef runtimeAllocationFunction,
        LLVMValueRef runtimeDeallocationFunction,
        std::string& error)
{
        if constexpr (!kRuntimeDebugTraceEnabled) return true;
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        if (debugInformation_ == DebugInformationKind::Full)
        {
            constexpr std::string_view allocationRecordText =
                "[NSC RUNTIME DEBUG INSTRUMENTATION CODE] event=ALLOCATE|allocator=runtime\r\n";
            constexpr std::string_view deallocationRecordText =
                "[NSC RUNTIME DEBUG INSTRUMENTATION CODE] event=FREE-BEGIN|allocator=runtime\r\n";
            LLVMValueRef allocationRecord = internStringLiteral(
                allocationRecordText,
                error);
            LLVMValueRef deallocationRecord = internStringLiteral(
                deallocationRecordText,
                error);
            if (allocationRecord == nullptr || deallocationRecord == nullptr)
                return fail(error, "INSTRUMENTATION CODE could not materialize allocator records");

            LLVMValueRef allocationWrapper = LLVMAddFunction(
                module_, "__nsc_runtime_debug_alloc", allocationFunctionType_);
            LLVMSetLinkage(allocationWrapper, LLVMInternalLinkage);
            applyFunctionAttribute(allocationWrapper, "nounwind");
            LLVMBasicBlockRef allocationEntry = LLVMAppendBasicBlockInContext(
                context_, allocationWrapper, "entry");
            LLVMPositionBuilderAtEnd(builder_, allocationEntry);
            LLVMValueRef allocationArguments[] = {LLVMGetParam(allocationWrapper, 0)};
            LLVMValueRef allocated = LLVMBuildCall2(
                builder_, allocationFunctionType_, runtimeAllocationFunction,
                allocationArguments, 1, "memory");
            LLVMValueRef allocationTraceArguments[] = {
                LLVMConstInt(i32, 2U, 0),
                allocationRecord,
                LLVMConstInt(i32, allocationRecordText.size(), 0)};
            LLVMBuildCall2(
                builder_, runtimeDebugTraceFunctionType_, runtimeDebugTraceFunction_,
                allocationTraceArguments, 3, "");
            LLVMBuildRet(builder_, allocated);
            allocationFunction_ = allocationWrapper;

            LLVMValueRef deallocationWrapper = LLVMAddFunction(
                module_, "__nsc_runtime_debug_free", deallocationFunctionType_);
            LLVMSetLinkage(deallocationWrapper, LLVMInternalLinkage);
            applyFunctionAttribute(deallocationWrapper, "nounwind");
            LLVMBasicBlockRef deallocationEntry = LLVMAppendBasicBlockInContext(
                context_, deallocationWrapper, "entry");
            LLVMPositionBuilderAtEnd(builder_, deallocationEntry);
            LLVMValueRef deallocationTraceArguments[] = {
                LLVMConstInt(i32, 2U, 0),
                deallocationRecord,
                LLVMConstInt(i32, deallocationRecordText.size(), 0)};
            LLVMBuildCall2(
                builder_, runtimeDebugTraceFunctionType_, runtimeDebugTraceFunction_,
                deallocationTraceArguments, 3, "");
            LLVMValueRef deallocationArguments[] = {LLVMGetParam(deallocationWrapper, 0)};
            LLVMBuildCall2(
                builder_, deallocationFunctionType_, runtimeDeallocationFunction,
                deallocationArguments, 1, "");
            LLVMBuildRetVoid(builder_);
            deallocationFunction_ = deallocationWrapper;
        }
        return true;
}

void OopLlvmImplementation::declareRuntimeConsoleTimeEnvironment()
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef consoleParams[] = {ptr, LLVMInt32TypeInContext(context_)};
        consoleWriteLineFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), consoleParams, 2, 0);
        consoleWriteLineFunction_ =
            LLVMAddFunction(module_, kConsoleWriteLineFunctionName, consoleWriteLineFunctionType_);
        LLVMTypeRef consoleReadLineParams[] = {ptr};
        consoleReadLineFunctionType_ = LLVMFunctionType(ptr, consoleReadLineParams, 1, 0);
        consoleReadLineFunction_ = LLVMAddFunction(
            module_, kConsoleReadLineFunctionName, consoleReadLineFunctionType_);

        // Stage 53.1 "Time" -- all three take zero parameters and return i64 (builtin:long).
        // Defined for real in src/Runtime/WindowsRuntimeBuilder.cpp; declared here, exactly
        // like every other __nativesharp_* runtime hook, so the object this lowerer emits has
        // an external reference the linker resolves against the runtime module at link time.
        clockQueryCounterFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        clockQueryCounterFunction_ = LLVMAddFunction(
            module_, kClockQueryCounterFunctionName, clockQueryCounterFunctionType_);
        clockQueryFrequencyFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        clockQueryFrequencyFunction_ = LLVMAddFunction(
            module_, kClockQueryFrequencyFunctionName, clockQueryFrequencyFunctionType_);
        clockQuerySystemFileTimeFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        clockQuerySystemFileTimeFunction_ = LLVMAddFunction(
            module_, kClockQuerySystemFileTimeFunctionName, clockQuerySystemFileTimeFunctionType_);

        // Stage 53.2 "Environment" -- TickCount64/ProcessorCount take zero parameters, same as
        // the NativeClock trio above (TickCount64 returns i64, ProcessorCount returns i32 --
        // GetActiveProcessorCount is a real 32-bit DWORD in Win32). Exit takes one i32
        // parameter and returns void, the same shape as consoleWriteLineFunctionType_ above
        // just with an i32 argument instead of a pointer.
        environmentTickCount64FunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        environmentTickCount64Function_ = LLVMAddFunction(
            module_, kEnvironmentTickCount64FunctionName, environmentTickCount64FunctionType_);
        environmentProcessorCountFunctionType_ = LLVMFunctionType(i32, nullptr, 0, 0);
        environmentProcessorCountFunction_ = LLVMAddFunction(
            module_, kEnvironmentProcessorCountFunctionName, environmentProcessorCountFunctionType_);
        LLVMTypeRef environmentExitParams[] = {i32};
        environmentExitFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), environmentExitParams, 1, 0);
        environmentExitFunction_ = LLVMAddFunction(
            module_, kEnvironmentExitFunctionName, environmentExitFunctionType_);

        // Stage 53.2b "Environment (variables)". GetEnvironmentVariable takes one NativeSharp
        // string pointer and returns a freshly allocated one (ptr -> ptr, same shape as any
        // other reference-typed core operation); SetEnvironmentVariable takes two string
        // pointers and returns void.
        LLVMTypeRef environmentGetVariableParams[] = {ptr};
        environmentGetVariableFunctionType_ = LLVMFunctionType(ptr, environmentGetVariableParams, 1, 0);
        environmentGetVariableFunction_ = LLVMAddFunction(
            module_, kEnvironmentGetVariableFunctionName, environmentGetVariableFunctionType_);
        LLVMTypeRef environmentSetVariableParams[] = {ptr, ptr};
        environmentSetVariableFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), environmentSetVariableParams, 2, 0);
        environmentSetVariableFunction_ = LLVMAddFunction(
            module_, kEnvironmentSetVariableFunctionName, environmentSetVariableFunctionType_);
}

void OopLlvmImplementation::declareRuntimeFilesystem()
{
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        // Stage 53.3 "Filesystem/Paths". Exists takes one NativeSharp string pointer (a path)
        // and returns i1 (builtin:bool) directly -- no allocation, unlike GetEnvironmentVariable,
        // so the return type is i1 here rather than ptr. Delete/CreateDirectory take one string
        // pointer and return void, same shape as SetEnvironmentVariable minus one parameter.
        LLVMTypeRef boolType = LLVMInt1TypeInContext(context_);
        LLVMTypeRef fileExistsParams[] = {ptr};
        fileExistsFunctionType_ = LLVMFunctionType(boolType, fileExistsParams, 1, 0);
        fileExistsFunction_ = LLVMAddFunction(module_, kFileExistsFunctionName, fileExistsFunctionType_);
        LLVMTypeRef fileDeleteParams[] = {ptr};
        fileDeleteFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), fileDeleteParams, 1, 0);
        fileDeleteFunction_ = LLVMAddFunction(module_, kFileDeleteFunctionName, fileDeleteFunctionType_);
        LLVMTypeRef directoryExistsParams[] = {ptr};
        directoryExistsFunctionType_ = LLVMFunctionType(boolType, directoryExistsParams, 1, 0);
        directoryExistsFunction_ = LLVMAddFunction(
            module_, kDirectoryExistsFunctionName, directoryExistsFunctionType_);
        LLVMTypeRef directoryCreateParams[] = {ptr};
        directoryCreateFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), directoryCreateParams, 1, 0);
        directoryCreateFunction_ = LLVMAddFunction(
            module_, kDirectoryCreateFunctionName, directoryCreateFunctionType_);

        // Stage 53.3b "Filesystem/Paths (contenido de archivo)". ReadAllText takes one string
        // pointer (a path) and returns a freshly allocated one -- same ptr -> ptr shape as
        // GetEnvironmentVariable. WriteAllText/AppendAllText take two string pointers (path,
        // content) and return void -- same shape as SetEnvironmentVariable.
        LLVMTypeRef fileReadAllTextParams[] = {ptr};
        fileReadAllTextFunctionType_ = LLVMFunctionType(ptr, fileReadAllTextParams, 1, 0);
        fileReadAllTextFunction_ = LLVMAddFunction(
            module_, kFileReadAllTextFunctionName, fileReadAllTextFunctionType_);

        // Bloque 4, fase A -- File.GetSize. Un puntero a cadena entra, un i64 sale. Es la primera
        // operacion nucleo de esta familia que devuelve 64 bits: las demas devuelven i1, i32, void
        // o un puntero. Ver MethodCoreOperationKind::FileGetSize para por que no puede ser i32.
        LLVMTypeRef fileGetSizeParams[] = {ptr};
        fileGetSizeFunctionType_ = LLVMFunctionType(
            LLVMInt64TypeInContext(context_), fileGetSizeParams, 1, 0);
        fileGetSizeFunction_ = LLVMAddFunction(
            module_, kFileGetSizeFunctionName, fileGetSizeFunctionType_);

        // Bloque 4, fase A -- E/S binaria. Un `byte[]` es un puntero al MISMO layout que una
        // cadena, { i64 longitud; datos }, asi que en el IR los tres son punteros y la firma no
        // distingue "arreglo de bytes" de "cadena": lo que las separa es la clave de tipo del
        // frontend, no el tipo LLVM.
        LLVMTypeRef fileReadAllBytesParams[] = {ptr};
        fileReadAllBytesFunctionType_ = LLVMFunctionType(ptr, fileReadAllBytesParams, 1, 0);
        fileReadAllBytesFunction_ = LLVMAddFunction(
            module_, kFileReadAllBytesFunctionName, fileReadAllBytesFunctionType_);
        LLVMTypeRef fileReadBytesParams[] = {ptr, ptr};
        fileReadBytesFunctionType_ = LLVMFunctionType(i32, fileReadBytesParams, 2, 0);
        fileReadBytesFunction_ = LLVMAddFunction(
            module_, kFileReadBytesFunctionName, fileReadBytesFunctionType_);
        LLVMTypeRef fileWriteAllBytesParams[] = {ptr, ptr, i32};
        fileWriteAllBytesFunctionType_ = LLVMFunctionType(
            LLVMInt1TypeInContext(context_), fileWriteAllBytesParams, 3, 0);
        fileWriteAllBytesFunction_ = LLVMAddFunction(
            module_, kFileWriteAllBytesFunctionName, fileWriteAllBytesFunctionType_);
        environmentCurrentDirectoryFunctionType_ = LLVMFunctionType(ptr, nullptr, 0, 0);
        environmentCurrentDirectoryFunction_ = LLVMAddFunction(
            module_, kEnvironmentCurrentDirectoryFunctionName, environmentCurrentDirectoryFunctionType_);

        declareRuntimeFilesystemCompletion();
}

void OopLlvmImplementation::declareRuntimeFilesystemCompletion()
{
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        // Bloque 4, fase C -- Process.Run. Tres punteros a cadena entran (ejecutable, linea de
        // comandos, ruta de salida), un i32 sale: el codigo de salida del hijo, o -1.
        LLVMTypeRef processRunParams[] = {ptr, ptr, ptr};
        processRunFunctionType_ = LLVMFunctionType(i32, processRunParams, 3, 0);
        processRunFunction_ = LLVMAddFunction(
            module_, kProcessRunFunctionName, processRunFunctionType_);
        LLVMTypeRef fileWriteAllTextParams[] = {ptr, ptr};
        fileWriteAllTextFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), fileWriteAllTextParams, 2, 0);
        fileWriteAllTextFunction_ = LLVMAddFunction(
            module_, kFileWriteAllTextFunctionName, fileWriteAllTextFunctionType_);
        LLVMTypeRef fileAppendAllTextParams[] = {ptr, ptr};
        if (fileAppendAllTextFunction_ == nullptr)
        {
            fileAppendAllTextFunctionType_ = LLVMFunctionType(
                LLVMVoidTypeInContext(context_), fileAppendAllTextParams, 2, 0);
            fileAppendAllTextFunction_ = LLVMAddFunction(
                module_, kFileAppendAllTextFunctionName, fileAppendAllTextFunctionType_);
        }

        // Stage 53.4 "Filesystem/Paths (cierre)". DirectoryDelete: one string pointer (a path),
        // void return -- same shape as FileDelete/DirectoryCreate above. PathGetFullPath: one
        // string pointer in, a freshly allocated one out -- same ptr -> ptr shape as
        // GetEnvironmentVariable/ReadAllText above.
        LLVMTypeRef directoryDeleteParams[] = {ptr};
        directoryDeleteFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), directoryDeleteParams, 1, 0);
        directoryDeleteFunction_ = LLVMAddFunction(
            module_, kDirectoryDeleteFunctionName, directoryDeleteFunctionType_);
        LLVMTypeRef pathGetFullPathParams[] = {ptr};
        pathGetFullPathFunctionType_ = LLVMFunctionType(ptr, pathGetFullPathParams, 1, 0);
        pathGetFullPathFunction_ = LLVMAddFunction(
            module_, kPathGetFullPathFunctionName, pathGetFullPathFunctionType_);
}

void OopLlvmImplementation::declareRuntimeDirectoryAndThreads()
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        // Stage 53.5 "Command-line arguments". GetCommandLineArgCount takes zero parameters
        // and returns i32 (builtin:int) directly -- same zero-argument shape as
        // environmentProcessorCountFunctionType_ above, just a different wrapped Win32 call.
        // GetCommandLineArgs takes one pointer (the caller-provided builtin:string[] buffer,
        // the "buffer provided by caller" pattern -- see Environment.ns) and also returns i32
        // directly (how many elements were actually written), NOT ptr -> ptr like
        // GetEnvironmentVariable/PathGetFullPath: this is the first core operation whose single
        // pointer argument is an array rather than a string, and whose result is a meaningful
        // integer rather than a freshly allocated NativeSharp object.
        environmentGetCommandLineArgCountFunctionType_ = LLVMFunctionType(i32, nullptr, 0, 0);
        environmentGetCommandLineArgCountFunction_ = LLVMAddFunction(
            module_, kEnvironmentGetCommandLineArgCountFunctionName,
            environmentGetCommandLineArgCountFunctionType_);
        LLVMTypeRef environmentGetCommandLineArgsParams[] = {ptr};
        environmentGetCommandLineArgsFunctionType_ = LLVMFunctionType(
            i32, environmentGetCommandLineArgsParams, 1, 0);
        environmentGetCommandLineArgsFunction_ = LLVMAddFunction(
            module_, kEnvironmentGetCommandLineArgsFunctionName,
            environmentGetCommandLineArgsFunctionType_);

        // Stage 53.6 "Enumeracion de directorios". GetFileCount/GetDirectoryCount: one string
        // pointer (a path) in, i32 out -- same ptr -> i32 shape as
        // environmentGetCommandLineArgCountFunctionType_ but with a real argument this time.
        // GetFiles/GetDirectories: two pointers in (the path, then the caller-provided
        // builtin:string[] buffer -- same buffer-in-second-position idea
        // environmentGetCommandLineArgsFunctionType_ established, just with a path ahead of it),
        // i32 out (how many elements were actually written).
        LLVMTypeRef directoryGetCountParams[] = {ptr};
        directoryGetFileCountFunctionType_ = LLVMFunctionType(i32, directoryGetCountParams, 1, 0);
        directoryGetFileCountFunction_ = LLVMAddFunction(
            module_, kDirectoryGetFileCountFunctionName, directoryGetFileCountFunctionType_);
        directoryGetDirectoryCountFunctionType_ = LLVMFunctionType(i32, directoryGetCountParams, 1, 0);
        directoryGetDirectoryCountFunction_ = LLVMAddFunction(
            module_, kDirectoryGetDirectoryCountFunctionName, directoryGetDirectoryCountFunctionType_);
        LLVMTypeRef directoryGetEntriesParams[] = {ptr, ptr};
        directoryGetFilesFunctionType_ = LLVMFunctionType(i32, directoryGetEntriesParams, 2, 0);
        directoryGetFilesFunction_ = LLVMAddFunction(
            module_, kDirectoryGetFilesFunctionName, directoryGetFilesFunctionType_);
        directoryGetDirectoriesFunctionType_ = LLVMFunctionType(i32, directoryGetEntriesParams, 2, 0);
        directoryGetDirectoriesFunction_ = LLVMAddFunction(
            module_, kDirectoryGetDirectoriesFunctionName, directoryGetDirectoriesFunctionType_);

        // Stage 54.1 "Threads". ThreadCreate: one pointer in (the ThreadStart delegate's 16-byte
        // heap cell -- a delegate value is ALWAYS represented as `ptr` at this LLVM level,
        // regardless of what it points to at the NativeSharp type level, exactly like every other
        // reference-typed operand this lowerer already handles), i64 out (the Win32 HANDLE,
        // reinterpreted as nint) -- a real, meaningful result, so NO "dummy i1" wrapping, same
        // discipline as directoryGetFileCountFunctionType_/directoryGetDirectoriesFunctionType_
        // above. ThreadJoin/ThreadDetach: one i64 in (the HANDLE the caller already holds as
        // nint), void out -- same "runtime function genuinely returns void, the LOWERING CASE
        // BLOCK synthesizes its own i1 dummy value in IR after the call" pattern
        // directoryDeleteFunctionType_/fileDeleteFunctionType_ above already use (see the
        // CoreThreadJoin/CoreThreadDetach case blocks further down for where that i1 actually
        // comes from -- it is NOT part of this function's own return type).
        LLVMTypeRef threadCreateParams[] = {ptr};
        threadCreateFunctionType_ = LLVMFunctionType(i64, threadCreateParams, 1, 0);
        threadCreateFunction_ = LLVMAddFunction(
            module_, kThreadCreateFunctionName, threadCreateFunctionType_);
        LLVMTypeRef threadJoinOrDetachParams[] = {i64};
        threadJoinFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), threadJoinOrDetachParams, 1, 0);
        threadJoinFunction_ = LLVMAddFunction(module_, kThreadJoinFunctionName, threadJoinFunctionType_);
        threadDetachFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), threadJoinOrDetachParams, 1, 0);
        threadDetachFunction_ = LLVMAddFunction(module_, kThreadDetachFunctionName, threadDetachFunctionType_);
}

void OopLlvmImplementation::declareRuntimeAtomics()
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        // Stage 54.2 "Atomics". The location parameter is always `ptr` at this LLVM level --
        // NativeSharp's raw-pointer machinery (see ProjectMethodBodyModel.cpp's Interlocked
        // recognition block: the first parameter's typeKey is builtin:int*/builtin:long*) already
        // lowers ANY `*`-suffixed typeKey to plain LLVM `ptr` via isPointerLikeType(), so there is
        // nothing special to do here beyond picking i32 vs i64 for the value/result type.
        // Increment/Decrement: one ptr in, i32/i64 out (the NEW value -- Win32's
        // InterlockedIncrement/Decrement already return the new value directly, no adjustment
        // needed in the runtime wrapper). Add: ptr + i32/i64 in, i32/i64 out (also the NEW value,
        // but Win32's InterlockedExchangeAdd returns the OLD value -- the runtime wrapper computes
        // old + value itself, see WindowsRuntimeBuilder.cpp). Exchange: ptr + i32/i64 in, i32/i64
        // out (the OLD value, Win32's InterlockedExchange result used directly).
        // CompareExchange: ptr + i32/i64 + i32/i64 in (location, value, comparand -- same order as
        // Win32's InterlockedCompareExchange(Destination, Exchange, Comparand) and C#'s
        // Interlocked.CompareExchange(location, value, comparand)), i32/i64 out (the OLD value,
        // regardless of whether the comparison matched, Win32's result used directly). None of
        // these ten need the "dummy i1" pattern -- every one returns a real, meaningful value.
        LLVMTypeRef interlockedLocationOnlyInt32Params[] = {ptr};
        atomicLoadInt32FunctionType_ = LLVMFunctionType(i32, interlockedLocationOnlyInt32Params, 1, 0);
        atomicLoadInt32Function_ = LLVMAddFunction(
            module_, kAtomicLoadInt32FunctionName, atomicLoadInt32FunctionType_);
        interlockedIncrementInt32FunctionType_ = LLVMFunctionType(i32, interlockedLocationOnlyInt32Params, 1, 0);
        interlockedIncrementInt32Function_ = LLVMAddFunction(
            module_, kInterlockedIncrementInt32FunctionName, interlockedIncrementInt32FunctionType_);
        interlockedDecrementInt32FunctionType_ = LLVMFunctionType(i32, interlockedLocationOnlyInt32Params, 1, 0);
        interlockedDecrementInt32Function_ = LLVMAddFunction(
            module_, kInterlockedDecrementInt32FunctionName, interlockedDecrementInt32FunctionType_);
        LLVMTypeRef interlockedLocationOnlyInt64Params[] = {ptr};
        atomicLoadInt64FunctionType_ = LLVMFunctionType(i64, interlockedLocationOnlyInt64Params, 1, 0);
        atomicLoadInt64Function_ = LLVMAddFunction(
            module_, kAtomicLoadInt64FunctionName, atomicLoadInt64FunctionType_);
        interlockedIncrementInt64FunctionType_ = LLVMFunctionType(i64, interlockedLocationOnlyInt64Params, 1, 0);
        interlockedIncrementInt64Function_ = LLVMAddFunction(
            module_, kInterlockedIncrementInt64FunctionName, interlockedIncrementInt64FunctionType_);
        interlockedDecrementInt64FunctionType_ = LLVMFunctionType(i64, interlockedLocationOnlyInt64Params, 1, 0);
        interlockedDecrementInt64Function_ = LLVMAddFunction(
            module_, kInterlockedDecrementInt64FunctionName, interlockedDecrementInt64FunctionType_);
        LLVMTypeRef interlockedLocationValueInt32Params[] = {ptr, i32};
        atomicStoreInt32FunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), interlockedLocationValueInt32Params, 2, 0);
        atomicStoreInt32Function_ = LLVMAddFunction(
            module_, kAtomicStoreInt32FunctionName, atomicStoreInt32FunctionType_);
        interlockedAddInt32FunctionType_ = LLVMFunctionType(i32, interlockedLocationValueInt32Params, 2, 0);
        interlockedAddInt32Function_ = LLVMAddFunction(
            module_, kInterlockedAddInt32FunctionName, interlockedAddInt32FunctionType_);
        interlockedExchangeInt32FunctionType_ = LLVMFunctionType(i32, interlockedLocationValueInt32Params, 2, 0);
        interlockedExchangeInt32Function_ = LLVMAddFunction(
            module_, kInterlockedExchangeInt32FunctionName, interlockedExchangeInt32FunctionType_);
        LLVMTypeRef interlockedLocationValueInt64Params[] = {ptr, i64};
        atomicStoreInt64FunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), interlockedLocationValueInt64Params, 2, 0);
        atomicStoreInt64Function_ = LLVMAddFunction(
            module_, kAtomicStoreInt64FunctionName, atomicStoreInt64FunctionType_);
        interlockedAddInt64FunctionType_ = LLVMFunctionType(i64, interlockedLocationValueInt64Params, 2, 0);
        interlockedAddInt64Function_ = LLVMAddFunction(
            module_, kInterlockedAddInt64FunctionName, interlockedAddInt64FunctionType_);
        interlockedExchangeInt64FunctionType_ = LLVMFunctionType(i64, interlockedLocationValueInt64Params, 2, 0);
        interlockedExchangeInt64Function_ = LLVMAddFunction(
            module_, kInterlockedExchangeInt64FunctionName, interlockedExchangeInt64FunctionType_);
        LLVMTypeRef interlockedCompareExchangeInt32Params[] = {ptr, i32, i32};
        interlockedCompareExchangeInt32FunctionType_ = LLVMFunctionType(
            i32, interlockedCompareExchangeInt32Params, 3, 0);
        interlockedCompareExchangeInt32Function_ = LLVMAddFunction(
            module_, kInterlockedCompareExchangeInt32FunctionName, interlockedCompareExchangeInt32FunctionType_);
        LLVMTypeRef interlockedCompareExchangeInt64Params[] = {ptr, i64, i64};
        interlockedCompareExchangeInt64FunctionType_ = LLVMFunctionType(
            i64, interlockedCompareExchangeInt64Params, 3, 0);
        interlockedCompareExchangeInt64Function_ = LLVMAddFunction(
            module_, kInterlockedCompareExchangeInt64FunctionName, interlockedCompareExchangeInt64FunctionType_);
}

void OopLlvmImplementation::declareRuntimeMutexAndMonitor()
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        // Stage 54.3 "Mutex". Exact same shape as ThreadCreate/ThreadJoin/ThreadDetach above:
        // MutexCreate takes NO parameters (the first zero-parameter runtime hook in the project),
        // i64 out (the HANDLE, as nint) -- a real, meaningful result, no dummy-i1 wrapping.
        // MutexLock/MutexUnlock/MutexClose: one i64 in (the HANDLE), void out -- same "dummy i1
        // synthesized by the lowering case block" pattern as ThreadJoin/ThreadDetach.
        mutexCreateFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        mutexCreateFunction_ = LLVMAddFunction(module_, kMutexCreateFunctionName, mutexCreateFunctionType_);
        LLVMTypeRef oneStringParam[] = {ptr};
        mutexCreateNamedFunctionType_ = LLVMFunctionType(i64, oneStringParam, 1, 0);
        mutexCreateNamedFunction_ = LLVMAddFunction(
            module_, kMutexCreateNamedFunctionName, mutexCreateNamedFunctionType_);
        LLVMTypeRef mutexHandleParams[] = {i64};
        mutexLockFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), mutexHandleParams, 1, 0);
        mutexLockFunction_ = LLVMAddFunction(module_, kMutexLockFunctionName, mutexLockFunctionType_);
        mutexUnlockFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), mutexHandleParams, 1, 0);
        mutexUnlockFunction_ = LLVMAddFunction(module_, kMutexUnlockFunctionName, mutexUnlockFunctionType_);
        mutexCloseFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), mutexHandleParams, 1, 0);
        mutexCloseFunction_ = LLVMAddFunction(module_, kMutexCloseFunctionName, mutexCloseFunctionType_);
        getCurrentThreadIdFunctionType_ = LLVMFunctionType(i32, nullptr, 0, 0);
        getCurrentThreadIdFunction_ = LLVMAddFunction(
            module_, kGetCurrentThreadIdFunctionName, getCurrentThreadIdFunctionType_);

        // Stage 54.4 "Condition Variables". Enter/Exit/Pulse/PulseAll: one pointer in (the
        // FieldAddress-computed lock_/cv_ address), void out. Wait: two pointers + one i32 in
        // (cv_ address, lock_ address, millisecondsTimeout), a REAL builtin:bool (i1) out -- no
        // dummy wrapping needed, same discipline as threadCreateFunctionType_ above.
        LLVMTypeRef monitorOnePointerParams[] = {ptr};
        monitorEnterFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), monitorOnePointerParams, 1, 0);
        monitorEnterFunction_ = LLVMAddFunction(module_, kMonitorEnterFunctionName, monitorEnterFunctionType_);
        monitorExitFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), monitorOnePointerParams, 1, 0);
        monitorExitFunction_ = LLVMAddFunction(module_, kMonitorExitFunctionName, monitorExitFunctionType_);
        monitorPulseFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), monitorOnePointerParams, 1, 0);
        monitorPulseFunction_ = LLVMAddFunction(module_, kMonitorPulseFunctionName, monitorPulseFunctionType_);
        monitorPulseAllFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), monitorOnePointerParams, 1, 0);
        monitorPulseAllFunction_ = LLVMAddFunction(
            module_, kMonitorPulseAllFunctionName, monitorPulseAllFunctionType_);
        LLVMTypeRef monitorWaitParams[] = {ptr, ptr, i32};
        monitorWaitFunctionType_ = LLVMFunctionType(
            LLVMInt1TypeInContext(context_), monitorWaitParams, 3, 0);
        monitorWaitFunction_ = LLVMAddFunction(module_, kMonitorWaitFunctionName, monitorWaitFunctionType_);
}

void OopLlvmImplementation::declareRuntimeSynchronization()
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        // Stage 54.5 "Otras primitivas de sincronizacion". SemaphoreCreate: two i32 in
        // (initialCount, maximumCount), i64 out (a real HANDLE, no dummy wrapping). The shared
        // waitForHandleTimeoutFunction_ both SemaphoreWait and EventWait call: one i64 in (the
        // HANDLE), one i32 in (millisecondsTimeout), a real builtin:bool (i1) out.
        // SemaphoreRelease/SemaphoreClose/EventSet/EventReset/EventClose: one i64 in, void out
        // (dummy-i1 pattern in the LOWERING CASE BLOCK, same as CoreMutexLock/Unlock/Close).
        // EventCreate: two i1 in (manualReset, initialState), i64 out (a real HANDLE).
        LLVMTypeRef semaphoreCreateParams[] = {i32, i32};
        semaphoreCreateFunctionType_ = LLVMFunctionType(i64, semaphoreCreateParams, 2, 0);
        semaphoreCreateFunction_ = LLVMAddFunction(
            module_, kSemaphoreCreateFunctionName, semaphoreCreateFunctionType_);
        LLVMTypeRef semaphoreCreateNamedParams[] = {i32, i32, ptr};
        semaphoreCreateNamedFunctionType_ = LLVMFunctionType(i64, semaphoreCreateNamedParams, 3, 0);
        semaphoreCreateNamedFunction_ = LLVMAddFunction(
            module_, kSemaphoreCreateNamedFunctionName, semaphoreCreateNamedFunctionType_);
        LLVMTypeRef waitForHandleTimeoutParams[] = {i64, i32};
        waitForHandleTimeoutFunctionType_ = LLVMFunctionType(
            LLVMInt1TypeInContext(context_), waitForHandleTimeoutParams, 2, 0);
        waitForHandleTimeoutFunction_ = LLVMAddFunction(
            module_, kWaitForHandleTimeoutFunctionName, waitForHandleTimeoutFunctionType_);
        LLVMTypeRef oneHandleParams[] = {i64};
        semaphoreReleaseFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), oneHandleParams, 1, 0);
        semaphoreReleaseFunction_ = LLVMAddFunction(
            module_, kSemaphoreReleaseFunctionName, semaphoreReleaseFunctionType_);
        semaphoreCloseFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), oneHandleParams, 1, 0);
        semaphoreCloseFunction_ = LLVMAddFunction(
            module_, kSemaphoreCloseFunctionName, semaphoreCloseFunctionType_);
        LLVMTypeRef eventCreateParams[] = {LLVMInt1TypeInContext(context_), LLVMInt1TypeInContext(context_)};
        eventCreateFunctionType_ = LLVMFunctionType(i64, eventCreateParams, 2, 0);
        eventCreateFunction_ = LLVMAddFunction(module_, kEventCreateFunctionName, eventCreateFunctionType_);
        LLVMTypeRef eventCreateNamedParams[] = {
            LLVMInt1TypeInContext(context_), LLVMInt1TypeInContext(context_), ptr};
        eventCreateNamedFunctionType_ = LLVMFunctionType(i64, eventCreateNamedParams, 3, 0);
        eventCreateNamedFunction_ = LLVMAddFunction(
            module_, kEventCreateNamedFunctionName, eventCreateNamedFunctionType_);
        eventSetFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), oneHandleParams, 1, 0);
        eventSetFunction_ = LLVMAddFunction(module_, kEventSetFunctionName, eventSetFunctionType_);
        eventResetFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), oneHandleParams, 1, 0);
        eventResetFunction_ = LLVMAddFunction(module_, kEventResetFunctionName, eventResetFunctionType_);
        eventCloseFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), oneHandleParams, 1, 0);
        eventCloseFunction_ = LLVMAddFunction(module_, kEventCloseFunctionName, eventCloseFunctionType_);
}

void OopLlvmImplementation::declareRuntimeProcessSystemDiagnostics()
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        // Stage 55.1 "Process". Five zero-argument builtin:long imports (the CURRENT process
        // HANDLE is obtained inside the runtime wrapper itself, never passed in from here).
        processGetCurrentProcessIdFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        processGetCurrentProcessIdFunction_ = LLVMAddFunction(
            module_, kProcessGetCurrentProcessIdFunctionName, processGetCurrentProcessIdFunctionType_);
        processGetWorkingSetSizeFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        processGetWorkingSetSizeFunction_ = LLVMAddFunction(
            module_, kProcessGetWorkingSetSizeFunctionName, processGetWorkingSetSizeFunctionType_);
        processGetKernelTimeFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        processGetKernelTimeFunction_ = LLVMAddFunction(
            module_, kProcessGetKernelTimeFunctionName, processGetKernelTimeFunctionType_);
        processGetUserTimeFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        processGetUserTimeFunction_ = LLVMAddFunction(
            module_, kProcessGetUserTimeFunctionName, processGetUserTimeFunctionType_);
        processGetPriorityClassFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        processGetPriorityClassFunction_ = LLVMAddFunction(
            module_, kProcessGetPriorityClassFunctionName, processGetPriorityClassFunctionType_);
        // SetPriorityClass: one builtin:int argument (the Win32 DWORD priority class constant),
        // void return -- dummy-i1 pattern at the call site, same as every other void core op.
        LLVMTypeRef setPriorityClassParams[] = {i32};
        processSetPriorityClassFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), setPriorityClassParams, 1, 0);
        processSetPriorityClassFunction_ = LLVMAddFunction(
            module_, kProcessSetPriorityClassFunctionName, processSetPriorityClassFunctionType_);

        declareRuntimeSystemDiagnostics();
}

void OopLlvmImplementation::declareRuntimeSystemDiagnostics()
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        // Stage 55.2 "Hardware / System Info". All six zero-argument, builtin:long.
        systemInfoGetPageSizeFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetPageSizeFunction_ = LLVMAddFunction(
            module_, kSystemInfoGetPageSizeFunctionName, systemInfoGetPageSizeFunctionType_);
        systemInfoGetProcessorArchitectureFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetProcessorArchitectureFunction_ = LLVMAddFunction(
            module_, kSystemInfoGetProcessorArchitectureFunctionName, systemInfoGetProcessorArchitectureFunctionType_);
        systemInfoGetAllocationGranularityFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetAllocationGranularityFunction_ = LLVMAddFunction(
            module_, kSystemInfoGetAllocationGranularityFunctionName, systemInfoGetAllocationGranularityFunctionType_);
        systemInfoGetTotalPhysicalMemoryBytesFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetTotalPhysicalMemoryBytesFunction_ = LLVMAddFunction(
            module_,
            kSystemInfoGetTotalPhysicalMemoryBytesFunctionName,
            systemInfoGetTotalPhysicalMemoryBytesFunctionType_);
        systemInfoGetAvailablePhysicalMemoryBytesFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetAvailablePhysicalMemoryBytesFunction_ = LLVMAddFunction(
            module_,
            kSystemInfoGetAvailablePhysicalMemoryBytesFunctionName,
            systemInfoGetAvailablePhysicalMemoryBytesFunctionType_);
        systemInfoGetMemoryLoadPercentFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        systemInfoGetMemoryLoadPercentFunction_ = LLVMAddFunction(
            module_, kSystemInfoGetMemoryLoadPercentFunctionName, systemInfoGetMemoryLoadPercentFunctionType_);

        // Stage 55.3 "Diagnostics". Is/CaptureCount: zero-argument, builtin:long. OutputString:
        // one builtin:string pointer argument (marshaled the same way EnvironmentSetVariable's
        // arguments are, via buildNullTerminatedCopy on the runtime side), void return.
        debugIsDebuggerPresentFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        debugIsDebuggerPresentFunction_ = LLVMAddFunction(
            module_, kDebugIsDebuggerPresentFunctionName, debugIsDebuggerPresentFunctionType_);
        debugCaptureStackFrameCountFunctionType_ = LLVMFunctionType(i64, nullptr, 0, 0);
        debugCaptureStackFrameCountFunction_ = LLVMAddFunction(
            module_, kDebugCaptureStackFrameCountFunctionName, debugCaptureStackFrameCountFunctionType_);
        LLVMTypeRef debugOutputStringParams[] = {ptr};
        debugOutputStringFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), debugOutputStringParams, 1, 0);
        debugOutputStringFunction_ = LLVMAddFunction(
            module_, kDebugOutputStringFunctionName, debugOutputStringFunctionType_);

        // Pre-Stage-56 "utilidades basicas" round. ThreadSleep: one builtin:int argument, void
        // return (dummy-i1 pattern at the call site, same as ThreadJoin/ThreadDetach). FileCopy:
        // two string pointers + one i1 (overwrite) in, a real i1 out. The shared MoveFile hook
        // both CoreFileMove and CoreDirectoryMove call: two string pointers in, a real i1 out.
        LLVMTypeRef threadSleepParams[] = {i32};
        threadSleepFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), threadSleepParams, 1, 0);
        threadSleepFunction_ = LLVMAddFunction(module_, kThreadSleepFunctionName, threadSleepFunctionType_);
        LLVMTypeRef fileCopyParams[] = {ptr, ptr, LLVMInt1TypeInContext(context_)};
        fileCopyFunctionType_ = LLVMFunctionType(
            LLVMInt1TypeInContext(context_), fileCopyParams, 3, 0);
        fileCopyFunction_ = LLVMAddFunction(module_, kFileCopyFunctionName, fileCopyFunctionType_);
        LLVMTypeRef moveFileParams[] = {ptr, ptr};
        moveFileFunctionType_ = LLVMFunctionType(
            LLVMInt1TypeInContext(context_), moveFileParams, 2, 0);
        moveFileFunction_ = LLVMAddFunction(module_, kMoveFileFunctionName, moveFileFunctionType_);
}

void OopLlvmImplementation::declareRuntimeExceptionsAndIntrinsics()
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef throwParams[] = {ptr};
        nativeThrowFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), throwParams, 1, 0);
        nativeThrowFunction_ = LLVMAddFunction(module_, kNativeThrowFunctionName, nativeThrowFunctionType_);

        LLVMTypeRef raiseExceptionParams[] = {i32, i32, i32, ptr};
        raiseExceptionFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), raiseExceptionParams, 4, 0);
        raiseExceptionFunction_ = LLVMAddFunction(
            module_, kRaiseExceptionFunctionName, raiseExceptionFunctionType_);

        exceptionCurrentFunctionType_ = LLVMFunctionType(ptr, nullptr, 0, 0);
        exceptionCurrentFunction_ =
            LLVMAddFunction(module_, kExceptionCurrentFunctionName, exceptionCurrentFunctionType_);

        exceptionClearFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), nullptr, 0, 0);
        exceptionClearFunction_ = LLVMAddFunction(module_, kExceptionClearFunctionName, exceptionClearFunctionType_);

        LLVMTypeRef filterParams[] = {ptr, ptr};
        ehFilterFunctionType_ = LLVMFunctionType(i32, filterParams, 2, 0);
        ehFilterFunction_ = LLVMAddFunction(module_, kEhFilterFunctionName, ehFilterFunctionType_);
        panicEhFilterFunctionType_ = LLVMFunctionType(i32, filterParams, 2, 0);
        panicEhFilterFunction_ = LLVMAddFunction(
            module_, kPanicEhFilterFunctionName, panicEhFilterFunctionType_);

        sehPersonalityFunctionType_ = LLVMFunctionType(i32, nullptr, 0, 1);
        sehPersonalityFunction_ = LLVMAddFunction(module_, kSehPersonalityFunctionName, sehPersonalityFunctionType_);

        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMTypeRef i1 = LLVMInt1TypeInContext(context_);
        LLVMTypeRef memsetParams[] = {ptr, i8, i64, i1};
        memsetFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), memsetParams, 4, 0);
        memsetFunction_ = LLVMAddFunction(module_, kMemsetIntrinsicName, memsetFunctionType_);
        LLVMTypeRef memcpyParams[] = {ptr, ptr, i64, i1};
        memcpyFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), memcpyParams, 4, 0);
        memcpyFunction_ = LLVMAddFunction(module_, kMemcpyIntrinsicName, memcpyFunctionType_);
        LLVMTypeRef assumeParams[] = {i1};
        assumeFunctionType_ = LLVMFunctionType(LLVMVoidTypeInContext(context_), assumeParams, 1, 0);
        assumeFunction_ = LLVMAddFunction(module_, kAssumeIntrinsicName, assumeFunctionType_);
}

bool OopLlvmImplementation::runtimeDeclarationsValid(std::string& error) const
{
        if (trapFunction_ == nullptr || allocationFunction_ == nullptr ||
            deallocationFunction_ == nullptr || consoleWriteLineFunction_ == nullptr ||
            clockQueryCounterFunction_ == nullptr || clockQueryFrequencyFunction_ == nullptr ||
            clockQuerySystemFileTimeFunction_ == nullptr ||
            environmentTickCount64Function_ == nullptr || environmentProcessorCountFunction_ == nullptr ||
            environmentExitFunction_ == nullptr ||
            environmentGetVariableFunction_ == nullptr || environmentSetVariableFunction_ == nullptr ||
            fileExistsFunction_ == nullptr || fileDeleteFunction_ == nullptr ||
            directoryExistsFunction_ == nullptr || directoryCreateFunction_ == nullptr ||
            fileReadAllTextFunction_ == nullptr || fileWriteAllTextFunction_ == nullptr ||
            fileAppendAllTextFunction_ == nullptr || fileGetSizeFunction_ == nullptr ||
            fileReadAllBytesFunction_ == nullptr || fileReadBytesFunction_ == nullptr ||
            fileWriteAllBytesFunction_ == nullptr ||
            environmentCurrentDirectoryFunction_ == nullptr ||
            processRunFunction_ == nullptr ||
            directoryDeleteFunction_ == nullptr || pathGetFullPathFunction_ == nullptr ||
            environmentGetCommandLineArgCountFunction_ == nullptr ||
            environmentGetCommandLineArgsFunction_ == nullptr ||
            directoryGetFileCountFunction_ == nullptr || directoryGetFilesFunction_ == nullptr ||
            directoryGetDirectoryCountFunction_ == nullptr || directoryGetDirectoriesFunction_ == nullptr ||
            threadCreateFunction_ == nullptr || threadJoinFunction_ == nullptr ||
            threadDetachFunction_ == nullptr ||
            atomicLoadInt32Function_ == nullptr || atomicStoreInt32Function_ == nullptr ||
            atomicLoadInt64Function_ == nullptr || atomicStoreInt64Function_ == nullptr ||
            interlockedIncrementInt32Function_ == nullptr || interlockedDecrementInt32Function_ == nullptr ||
            interlockedAddInt32Function_ == nullptr || interlockedExchangeInt32Function_ == nullptr ||
            interlockedCompareExchangeInt32Function_ == nullptr ||
            interlockedIncrementInt64Function_ == nullptr || interlockedDecrementInt64Function_ == nullptr ||
            interlockedAddInt64Function_ == nullptr || interlockedExchangeInt64Function_ == nullptr ||
            interlockedCompareExchangeInt64Function_ == nullptr ||
            mutexCreateFunction_ == nullptr || mutexLockFunction_ == nullptr ||
            mutexUnlockFunction_ == nullptr || mutexCloseFunction_ == nullptr ||
            getCurrentThreadIdFunction_ == nullptr ||
            monitorEnterFunction_ == nullptr || monitorExitFunction_ == nullptr ||
            monitorPulseFunction_ == nullptr || monitorPulseAllFunction_ == nullptr ||
            monitorWaitFunction_ == nullptr ||
            semaphoreCreateFunction_ == nullptr || waitForHandleTimeoutFunction_ == nullptr ||
            semaphoreReleaseFunction_ == nullptr || semaphoreCloseFunction_ == nullptr ||
            eventCreateFunction_ == nullptr || eventSetFunction_ == nullptr ||
            eventResetFunction_ == nullptr || eventCloseFunction_ == nullptr ||
            nativeThrowFunction_ == nullptr || raiseExceptionFunction_ == nullptr ||
            exceptionCurrentFunction_ == nullptr ||
            exceptionClearFunction_ == nullptr ||
            ehFilterFunction_ == nullptr || panicEhFilterFunction_ == nullptr ||
            sehPersonalityFunction_ == nullptr ||
            memsetFunction_ == nullptr || memcpyFunction_ == nullptr || assumeFunction_ == nullptr)
        {
            error = "Stage 45.10 failed to declare OOP runtime/EH hooks";
            return false;
        }
        return true;
}

}
