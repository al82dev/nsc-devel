if(NOT DEFINED NSC_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR
   NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR
   NOT DEFINED LINKER_EXECUTABLE OR NOT DEFINED NSLINK_EXECUTABLE)
    message(FATAL_ERROR "SH41 requires nsc0, Python, source/output roots, LLD and NSLink")
endif()

foreach(required IN ITEMS NSC_EXECUTABLE PYTHON_EXECUTABLE LINKER_EXECUTABLE NSLINK_EXECUTABLE)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "SH41 missing ${required}: ${${required}}")
    endif()
endforeach()

get_filename_component(nslink_bin "${NSLINK_EXECUTABLE}" DIRECTORY)
get_filename_component(prefix "${nslink_bin}" DIRECTORY)
get_filename_component(llvm_bin "${LINKER_EXECUTABLE}" DIRECTORY)
get_filename_component(llvm_root "${llvm_bin}" DIRECTORY)
set(runtime_library "${prefix}/lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib")
set(llvm_library "${llvm_root}/lib/LLVM-C.lib")
if(NOT EXISTS "${runtime_library}" OR NOT EXISTS "${llvm_library}")
    message(FATAL_ERROR "SH41 missing runtime or LLVM-C library: ${runtime_library}; ${llvm_library}")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(nsc1 "${OUTPUT_DIR}/nsc1.exe")
execute_process(
    COMMAND "${NSC_EXECUTABLE}" --emit-project-oop-exe
        "${SOURCE_ROOT}/selfhost/nsc/NativeSharp.Compiler.nsproj" -O0 -g0 -o "${nsc1}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH41 nsc1 preparation failed: ${out}${err}")
endif()

set(kernel32_symbols
    "ExitProcess,GetProcessHeap,HeapAlloc,HeapFree,GetStdHandle,WriteFile,TlsAlloc,TlsGetValue,TlsSetValue,RaiseException,QueryPerformanceCounter,QueryPerformanceFrequency,GetSystemTimeAsFileTime,GetTickCount64,GetActiveProcessorCount,GetEnvironmentVariableA,SetEnvironmentVariableA,GetFileAttributesA,GetFileAttributesW,DeleteFileA,CreateDirectoryA,CreateFileA,CreateFileW,GetFileSizeEx,ReadFile,CloseHandle,SetFilePointer,RemoveDirectoryA,GetFullPathNameA,GetCurrentDirectoryA,CreateProcessA,GetExitCodeProcess,GetCommandLineW,WideCharToMultiByte,MultiByteToWideChar,LocalFree,FindFirstFileW,FindNextFileW,FindClose,CreateThread,WaitForSingleObject,CreateMutexA,ReleaseMutex,GetCurrentThreadId,AcquireSRWLockExclusive,ReleaseSRWLockExclusive,WakeConditionVariable,WakeAllConditionVariable,SleepConditionVariableSRW,CreateSemaphoreA,ReleaseSemaphore,CreateEventA,SetEvent,ResetEvent,GetCurrentProcessId,GetCurrentProcess,K32GetProcessMemoryInfo,GetProcessTimes,GetPriorityClass,SetPriorityClass,GetSystemInfo,GlobalMemoryStatusEx,OutputDebugStringA,IsDebuggerPresent,RtlCaptureStackBackTrace,Sleep,CopyFileA,MoveFileA")
set(import_specs
    "KERNEL32.dll:${kernel32_symbols}|kernel32.lib"
    "KERNELBASE.dll:__chkstk|kernelbase.lib"
    "SHELL32.dll:CommandLineToArgvW|shell32.lib"
    "ucrtbase.dll:strlen,__C_specific_handler,memset,memcpy,memmove,memcmp,fmaf|ucrtbase.lib")
set(import_arguments)
foreach(spec IN LISTS import_specs)
    string(REPLACE "|" ";" fields "${spec}")
    list(GET fields 0 definition)
    list(GET fields 1 filename)
    set(path "${OUTPUT_DIR}/${filename}")
    execute_process(COMMAND "${NSLINK_EXECUTABLE}" --link --make-implib "${definition}" "${path}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH41 import library ${filename} failed: ${out}${err}")
    endif()
    list(APPEND import_arguments --import-library "${path}")
endforeach()

set(report "${OUTPUT_DIR}/sh41-report.json")
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -B "${SOURCE_ROOT}/selfhost/bootstrap/verify_self_hosted_generation.py"
        --contract "${SOURCE_ROOT}/selfhost/contracts/self-hosted-generation.contract.json"
        --source-root "${SOURCE_ROOT}"
        --nsc0 "${NSC_EXECUTABLE}"
        --nsc1 "${nsc1}"
        --linker "${LINKER_EXECUTABLE}"
        --nslink "${NSLINK_EXECUTABLE}"
        --runtime-library "${runtime_library}"
        --llvm-library "${llvm_library}"
        ${import_arguments}
        --output-dir "${OUTPUT_DIR}"
        --report "${report}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH41 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
