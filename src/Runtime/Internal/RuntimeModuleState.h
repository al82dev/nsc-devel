#pragma once

#include "../../LLVM/LlvmCApi.h"

namespace nativesharp::runtime::internal
{
using namespace llvm_api;

struct RuntimeModuleState final
{
    LLVMContextRef context = nullptr;
    LLVMModuleRef module = nullptr;

    LLVMTypeRef voidType = nullptr;
    LLVMTypeRef i16Type = nullptr;
    LLVMTypeRef i32Type = nullptr;
    LLVMTypeRef i64Type = nullptr;
    LLVMTypeRef pointerType = nullptr;

    LLVMTypeRef exitProcessType = nullptr;
    LLVMValueRef exitProcess = nullptr;
    LLVMTypeRef getProcessHeapType = nullptr;
    LLVMValueRef getProcessHeap = nullptr;
    LLVMTypeRef heapAllocType = nullptr;
    LLVMValueRef heapAlloc = nullptr;
    LLVMTypeRef heapReAllocType = nullptr;
    LLVMValueRef heapReAlloc = nullptr;
    LLVMTypeRef heapFreeType = nullptr;
    LLVMValueRef heapFree = nullptr;
    LLVMTypeRef getStdHandleType = nullptr;
    LLVMValueRef getStdHandle = nullptr;
    LLVMTypeRef getLastErrorType = nullptr;
    LLVMValueRef getLastError = nullptr;
    LLVMTypeRef writeFileType = nullptr;
    LLVMValueRef writeFile = nullptr;

    LLVMTypeRef queryPerformanceCounterType = nullptr;
    LLVMValueRef queryPerformanceCounter = nullptr;
    LLVMTypeRef queryPerformanceFrequencyType = nullptr;
    LLVMValueRef queryPerformanceFrequency = nullptr;
    LLVMTypeRef getSystemTimeAsFileTimeType = nullptr;
    LLVMValueRef getSystemTimeAsFileTime = nullptr;
    LLVMTypeRef getTickCount64Type = nullptr;
    LLVMValueRef getTickCount64 = nullptr;
    LLVMTypeRef getActiveProcessorCountType = nullptr;
    LLVMValueRef getActiveProcessorCount = nullptr;
    LLVMTypeRef getEnvironmentVariableAType = nullptr;
    LLVMValueRef getEnvironmentVariableA = nullptr;
    LLVMTypeRef setEnvironmentVariableAType = nullptr;
    LLVMValueRef setEnvironmentVariableA = nullptr;

    LLVMTypeRef getFileAttributesAType = nullptr;
    LLVMValueRef getFileAttributesA = nullptr;
    LLVMTypeRef getFileAttributesWType = nullptr;
    LLVMValueRef getFileAttributesW = nullptr;
    LLVMTypeRef deleteFileAType = nullptr;
    LLVMValueRef deleteFileA = nullptr;
    LLVMTypeRef createDirectoryAType = nullptr;
    LLVMValueRef createDirectoryA = nullptr;
    LLVMTypeRef createFileAType = nullptr;
    LLVMValueRef createFileA = nullptr;
    LLVMTypeRef createFileWType = nullptr;
    LLVMValueRef createFileW = nullptr;
    LLVMTypeRef getFileSizeExType = nullptr;
    LLVMValueRef getFileSizeEx = nullptr;
    LLVMTypeRef readFileType = nullptr;
    LLVMValueRef readFile = nullptr;
    LLVMTypeRef closeHandleType = nullptr;
    LLVMValueRef closeHandle = nullptr;
    LLVMTypeRef setFilePointerType = nullptr;
    LLVMValueRef setFilePointer = nullptr;
    LLVMTypeRef removeDirectoryAType = nullptr;
    LLVMValueRef removeDirectoryA = nullptr;
    LLVMTypeRef getFullPathNameWType = nullptr;
    LLVMValueRef getFullPathNameW = nullptr;
    LLVMTypeRef getCurrentDirectoryAType = nullptr;
    LLVMValueRef getCurrentDirectoryA = nullptr;
    LLVMTypeRef createProcessAType = nullptr;
    LLVMValueRef createProcessA = nullptr;
    LLVMTypeRef getExitCodeProcessType = nullptr;
    LLVMValueRef getExitCodeProcess = nullptr;

    LLVMTypeRef getCommandLineWType = nullptr;
    LLVMValueRef getCommandLineW = nullptr;
    LLVMTypeRef commandLineToArgvWType = nullptr;
    LLVMValueRef commandLineToArgvW = nullptr;
    LLVMTypeRef wideCharToMultiByteType = nullptr;
    LLVMValueRef wideCharToMultiByte = nullptr;
    LLVMTypeRef multiByteToWideCharType = nullptr;
    LLVMValueRef multiByteToWideChar = nullptr;
    LLVMTypeRef localFreeType = nullptr;
    LLVMValueRef localFree = nullptr;
    LLVMTypeRef findFirstFileWType = nullptr;
    LLVMValueRef findFirstFileW = nullptr;
    LLVMTypeRef findNextFileWType = nullptr;
    LLVMValueRef findNextFileW = nullptr;
    LLVMTypeRef findCloseType = nullptr;
    LLVMValueRef findClose = nullptr;

    LLVMTypeRef createThreadType = nullptr;
    LLVMValueRef createThread = nullptr;
    LLVMTypeRef waitForSingleObjectType = nullptr;
    LLVMValueRef waitForSingleObject = nullptr;
    LLVMTypeRef createMutexAType = nullptr;
    LLVMValueRef createMutexA = nullptr;
    LLVMTypeRef releaseMutexType = nullptr;
    LLVMValueRef releaseMutex = nullptr;
    LLVMTypeRef getCurrentThreadIdType = nullptr;
    LLVMValueRef getCurrentThreadId = nullptr;
    LLVMTypeRef acquireSRWLockExclusiveType = nullptr;
    LLVMValueRef acquireSRWLockExclusive = nullptr;
    LLVMTypeRef releaseSRWLockExclusiveType = nullptr;
    LLVMValueRef releaseSRWLockExclusive = nullptr;
    LLVMTypeRef wakeConditionVariableType = nullptr;
    LLVMValueRef wakeConditionVariable = nullptr;
    LLVMTypeRef wakeAllConditionVariableType = nullptr;
    LLVMValueRef wakeAllConditionVariable = nullptr;
    LLVMTypeRef sleepConditionVariableSRWType = nullptr;
    LLVMValueRef sleepConditionVariableSRW = nullptr;
    LLVMTypeRef createSemaphoreAType = nullptr;
    LLVMValueRef createSemaphoreA = nullptr;
    LLVMTypeRef releaseSemaphoreType = nullptr;
    LLVMValueRef releaseSemaphore = nullptr;
    LLVMTypeRef createEventAType = nullptr;
    LLVMValueRef createEventA = nullptr;
    LLVMTypeRef setEventType = nullptr;
    LLVMValueRef setEvent = nullptr;
    LLVMTypeRef resetEventType = nullptr;
    LLVMValueRef resetEvent = nullptr;

    LLVMTypeRef getCurrentProcessIdType = nullptr;
    LLVMValueRef getCurrentProcessId = nullptr;
    LLVMTypeRef getCurrentProcessType = nullptr;
    LLVMValueRef getCurrentProcess = nullptr;
    LLVMTypeRef getProcessMemoryInfoType = nullptr;
    LLVMValueRef getProcessMemoryInfo = nullptr;
    LLVMTypeRef getProcessTimesType = nullptr;
    LLVMValueRef getProcessTimes = nullptr;
    LLVMTypeRef getPriorityClassType = nullptr;
    LLVMValueRef getPriorityClass = nullptr;
    LLVMTypeRef setPriorityClassType = nullptr;
    LLVMValueRef setPriorityClass = nullptr;
    LLVMTypeRef getSystemInfoType = nullptr;
    LLVMValueRef getSystemInfo = nullptr;
    LLVMTypeRef globalMemoryStatusExType = nullptr;
    LLVMValueRef globalMemoryStatusEx = nullptr;
    LLVMTypeRef outputDebugStringAType = nullptr;
    LLVMValueRef outputDebugStringA = nullptr;
    LLVMTypeRef isDebuggerPresentType = nullptr;
    LLVMValueRef isDebuggerPresent = nullptr;
    LLVMTypeRef rtlCaptureStackBackTraceType = nullptr;
    LLVMValueRef rtlCaptureStackBackTrace = nullptr;
    LLVMTypeRef sleepType = nullptr;
    LLVMValueRef sleep = nullptr;
    LLVMTypeRef copyFileAType = nullptr;
    LLVMValueRef copyFileA = nullptr;
    LLVMTypeRef moveFileAType = nullptr;
    LLVMValueRef moveFileA = nullptr;

    LLVMTypeRef tlsAllocType = nullptr;
    LLVMValueRef tlsAlloc = nullptr;
    LLVMTypeRef tlsGetValueType = nullptr;
    LLVMValueRef tlsGetValue = nullptr;
    LLVMTypeRef tlsSetValueType = nullptr;
    LLVMValueRef tlsSetValue = nullptr;
    LLVMTypeRef raiseExceptionType = nullptr;
    LLVMValueRef raiseException = nullptr;
    LLVMValueRef exceptionTlsIndex = nullptr;
    LLVMValueRef ehInit = nullptr;
    LLVMTypeRef ehInitType = nullptr;
    LLVMValueRef threadTrampoline = nullptr;
    bool builtEhInit = false;
};
}
