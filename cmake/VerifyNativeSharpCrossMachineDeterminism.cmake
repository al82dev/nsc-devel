if(NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR OR
   NOT DEFINED SH41_OUTPUT_DIR OR NOT DEFINED LINKER_EXECUTABLE OR NOT DEFINED NSLINK_EXECUTABLE)
    message(FATAL_ERROR "SH44 requires Python, roots, SH41 products, LLD and NSLink")
endif()

get_filename_component(nslink_bin "${NSLINK_EXECUTABLE}" DIRECTORY)
get_filename_component(prefix "${nslink_bin}" DIRECTORY)
get_filename_component(llvm_bin "${LINKER_EXECUTABLE}" DIRECTORY)
get_filename_component(llvm_root "${llvm_bin}" DIRECTORY)
if(DEFINED RUNTIME_LIBRARY AND NOT RUNTIME_LIBRARY STREQUAL "")
    set(runtime_library "${RUNTIME_LIBRARY}")
else()
    set(runtime_library "${prefix}/lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib")
endif()
if(DEFINED LLVM_LIBRARY AND NOT LLVM_LIBRARY STREQUAL "")
    set(llvm_library "${LLVM_LIBRARY}")
else()
    set(llvm_library "${llvm_root}/lib/LLVM-C.lib")
endif()
foreach(required IN ITEMS PYTHON_EXECUTABLE LINKER_EXECUTABLE NSLINK_EXECUTABLE runtime_library llvm_library)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "SH44 missing ${required}: ${${required}}")
    endif()
endforeach()

set(input_dir "${OUTPUT_DIR}-inputs")
file(REMOVE_RECURSE "${OUTPUT_DIR}" "${input_dir}")
file(MAKE_DIRECTORY "${input_dir}")

set(candidate_arguments)
foreach(profile IN ITEMS O0 O1 O2 O3)
    string(TOLOWER "${profile}" lower)
    set(candidate "${SH41_OUTPUT_DIR}/nsc2-${lower}-lld.exe")
    if(NOT EXISTS "${candidate}")
        message(FATAL_ERROR "SH44 missing SH41 nsc2 image: ${candidate}")
    endif()
    list(APPEND candidate_arguments "--nsc2-${lower}" "${candidate}")
endforeach()

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
    set(path "${input_dir}/${filename}")
    execute_process(COMMAND "${NSLINK_EXECUTABLE}" --link --make-implib "${definition}" "${path}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SH44 import library ${filename} failed: ${out}${err}")
    endif()
    list(APPEND import_arguments --import-library "${path}")
endforeach()

set(peer_arguments)
if(DEFINED PEER_ATTESTATION AND NOT PEER_ATTESTATION STREQUAL "")
    list(APPEND peer_arguments --peer-attestation "${PEER_ATTESTATION}")
elseif(DEFINED ENV{NSC_SH44_PEER_ATTESTATION} AND NOT "$ENV{NSC_SH44_PEER_ATTESTATION}" STREQUAL "")
    list(APPEND peer_arguments --peer-attestation "$ENV{NSC_SH44_PEER_ATTESTATION}")
endif()
if(DEFINED REQUIRE_PEER AND REQUIRE_PEER)
    list(APPEND peer_arguments --require-peer)
endif()

execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B
    "${SOURCE_ROOT}/selfhost/bootstrap/verify_cross_machine_determinism.py"
    --contract "${SOURCE_ROOT}/selfhost/contracts/cross-machine-determinism.contract.json"
    --source-root "${SOURCE_ROOT}" ${candidate_arguments}
    --linker "${LINKER_EXECUTABLE}" --nslink "${NSLINK_EXECUTABLE}"
    --runtime-library "${runtime_library}" --llvm-library "${llvm_library}"
    ${import_arguments} --output-dir "${OUTPUT_DIR}" --report "${OUTPUT_DIR}/sh44-report.json"
    --attestation "${OUTPUT_DIR}/machine-attestation.json" ${peer_arguments}
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SH44 verifier failed: ${out}${err}")
endif()
message(STATUS "${out}")
