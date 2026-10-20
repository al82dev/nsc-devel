#include "nativesharp/Linker/WindowsRuntimeImports.h"

#include "Internal/PlannedLibrary.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>

namespace nativesharp::linker
{
namespace
{
std::string upperAscii(std::string_view text)
{
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

template <std::size_t Size>
void addSymbols(
    std::vector<PlannedLibrary>& planned,
    std::map<std::string, std::size_t>& byUpperName,
    std::string_view dll,
    const std::array<std::string_view, Size>& symbols)
{
    const std::string upper = upperAscii(dll);
    const auto found = byUpperName.find(upper);
    std::size_t index;
    if (found == byUpperName.end())
    {
        index = planned.size();
        planned.push_back(PlannedLibrary{std::string(dll), {}});
        byUpperName.emplace(upper, index);
    }
    else
    {
        index = found->second;
    }
    for (const std::string_view symbol : symbols)
    {
        const std::string name(symbol);
        auto& list = planned[index].symbols;
        if (std::find(list.begin(), list.end(), name) == list.end())
        {
            list.push_back(name);
        }
    }
}

void addSymbols(
    std::vector<PlannedLibrary>& planned,
    std::map<std::string, std::size_t>& byUpperName,
    std::string_view dll,
    const std::vector<std::string>& symbols)
{
    const std::string upper = upperAscii(dll);
    const auto found = byUpperName.find(upper);
    std::size_t index;
    if (found == byUpperName.end())
    {
        index = planned.size();
        planned.push_back(PlannedLibrary{std::string(dll), {}});
        byUpperName.emplace(upper, index);
    }
    else
    {
        index = found->second;
    }
    for (const std::string& symbol : symbols)
    {
        auto& list = planned[index].symbols;
        if (std::find(list.begin(), list.end(), symbol) == list.end())
        {
            list.push_back(symbol);
        }
    }
}

auto kernel32FoundationSymbols()
{
    return std::to_array<std::string_view>({
        "ExitProcess",
        "GetProcessHeap",
        "HeapAlloc",
        "HeapReAlloc",
        "HeapFree",
        "GetLastError",
        "GetStdHandle",
        "WriteFile",
        "TlsAlloc",
        "TlsGetValue",
        "TlsSetValue",
        "RaiseException",
        // Stage 53.1 "Time" -- NativeClock's three raw wrappers (see
        // src/Runtime/WindowsRuntimeBuilder.cpp) import these directly from KERNEL32.dll.
        "QueryPerformanceCounter",
        "QueryPerformanceFrequency",
        "GetSystemTimeAsFileTime",
        // Stage 53.2 "Environment" -- Environment.TickCount64/ProcessorCount (see
        // src/Runtime/WindowsRuntimeBuilder.cpp) import these directly from KERNEL32.dll.
        "GetTickCount64",
        "GetActiveProcessorCount",
        // Stage 53.2b "Environment (variables)" -- Environment.GetEnvironmentVariable/
        // SetEnvironmentVariable (see src/Runtime/WindowsRuntimeBuilder.cpp) import these
        // directly from KERNEL32.dll.
        "GetEnvironmentVariableA",
        "SetEnvironmentVariableA",
        // Stage 53.3/T46 "Filesystem/Paths" -- existence checks use the W variant for UTF-8
        // paths; legacy mutation operations remain listed independently.
        "GetFileAttributesA",
        "GetFileAttributesW",
        "DeleteFileA",
        "CreateDirectoryA",
        // Stage 53.3b/T46 content operations. ReadAllText uses CreateFileW; other operations keep
        // their existing CreateFileA path until the complete SH48 filesystem conversion.
        "CreateFileA",
        "CreateFileW",
        "GetFileSizeEx",
        "ReadFile",
        "CloseHandle",
        "SetFilePointer",
        // Stage 53.4 "Filesystem/Paths (cierre)" -- Directory.Delete/Path.GetFullPath (see
        // src/Runtime/WindowsRuntimeBuilder.cpp) import these directly from KERNEL32.dll.
        "RemoveDirectoryA",
        "GetFullPathNameW",
        // Bloque 4, fase A -- Environment.CurrentDirectory. ESTA LISTA ES EL DECIMO SITIO QUE HAY
        // QUE TOCAR, y no estaba en la tabla del doc 100: declarar el import con LLVMAddFunction
        // en WindowsRuntimeBuilder NO basta. Esa declaracion solo dice "existe un simbolo con
        // este nombre"; quien le dice al enlazador de que DLL sacarlo es esta lista, que alimenta
        // la biblioteca de importacion.
        //
        // Sin esta linea el compilador construye e instala sin una queja, y despues TODOS los
        // programas fallan al enlazar con "undefined symbol: GetCurrentDirectoryA" -- porque el
        // objeto de runtime va dentro de cada ejecutable, asi que un import que falta no rompe la
        // funcion que lo usa: rompe el enlace de todo. Medido el 2026-09-02: 86 de 86 trabajos
        // con "build failed".
        "GetCurrentDirectoryA",
        // Bloque 4, fase C -- Process.Run. WaitForSingleObject y CloseHandle ya estaban (los
        // hilos de Stage 54.1 los importan), asi que solo faltan estos dos.
        "CreateProcessA",
        "GetExitCodeProcess",
        // Stage 53.5 "Command-line arguments" -- Environment.GetCommandLineArgCount/
        // GetCommandLineArgs (see src/Runtime/WindowsRuntimeBuilder.cpp) import these directly
        // from KERNEL32.dll. CommandLineToArgvW itself lives in SHELL32.dll, not here -- see the
        // separate shell32Library block below.
        "GetCommandLineW",
        "WideCharToMultiByte"});
}

auto kernel32PlatformSymbols()
{
    return std::to_array<std::string_view>({
        "MultiByteToWideChar",
        "LocalFree",
        // Stage 53.6 "Enumeracion de directorios" -- Directory.GetFileCount/GetFiles/
        // GetDirectoryCount/GetDirectories (see src/Runtime/WindowsRuntimeBuilder.cpp) import
        // these directly from KERNEL32.dll.
        "FindFirstFileW",
        "FindNextFileW",
        "FindClose",
        // Stage 54.1 "Threads" -- Thread's ThreadCreate/ThreadJoin/ThreadDetach (see
        // src/Runtime/WindowsRuntimeBuilder.cpp) import these directly from KERNEL32.dll.
        "CreateThread",
        "WaitForSingleObject",
        // Stage 54.3 "Mutex" -- Mutex's MutexCreate/MutexUnlock (see
        // src/Runtime/WindowsRuntimeBuilder.cpp) import these directly from KERNEL32.dll.
        // MutexLock/MutexClose reuse WaitForSingleObject/CloseHandle above, no new import needed.
        "CreateMutexA",
        "ReleaseMutex",
        // Stage 54.3 "Mutex" (fix) -- the type-initializer ("cctor") guard's same-thread-
        // recursion check (see ProjectOopLlvmLowerer.cpp's defineTypeInitializers) imports this
        // directly, independent of Mutex itself.
        "GetCurrentThreadId",
        // Stage 54.4 "Condition Variables" -- Monitor's Enter/Exit/Wait/Pulse/PulseAll (see
        // src/Runtime/WindowsRuntimeBuilder.cpp) import these directly from KERNEL32.dll.
        "AcquireSRWLockExclusive",
        "ReleaseSRWLockExclusive",
        "WakeConditionVariable",
        "WakeAllConditionVariable",
        "SleepConditionVariableSRW",
        // Stage 54.5 "Otras primitivas de sincronizacion" -- Semaphore's SemaphoreCreate/Release
        // and Event's EventCreate/Set/Reset (see src/Runtime/WindowsRuntimeBuilder.cpp) import
        // these directly from KERNEL32.dll. SemaphoreWait/EventWait reuse WaitForSingleObject
        // above (via the shared __nativesharp_wait_for_handle_timeout wrapper), and
        // SemaphoreClose/EventClose reuse CloseHandle above -- no new import needed for those.
        "CreateSemaphoreA",
        "ReleaseSemaphore",
        "CreateEventA",
        "SetEvent",
        "ResetEvent",
        // Stage 55.1 "Process" -- Process's five Raw getters/setter (see
        // src/Runtime/WindowsRuntimeBuilder.cpp) import these directly from KERNEL32.dll.
        // K32GetProcessMemoryInfo is the kernel32.dll-exported alias for psapi.lib's
        // GetProcessMemoryInfo (no separate psapi import library needed).
        "GetCurrentProcessId",
        "GetCurrentProcess",
        "K32GetProcessMemoryInfo",
        "GetProcessTimes",
        "GetPriorityClass",
        "SetPriorityClass",
        // Stage 55.2 "Hardware / System Info" -- SystemInfo's six Raw getters (see
        // src/Runtime/WindowsRuntimeBuilder.cpp) import these directly from KERNEL32.dll.
        "GetSystemInfo",
        "GlobalMemoryStatusEx",
        // Stage 55.3 "Diagnostics" -- Debug's IsDebuggerPresentRaw/CaptureStackFrameCountRaw/
        // OutputStringRaw (see src/Runtime/WindowsRuntimeBuilder.cpp) import these directly from
        // KERNEL32.dll. RtlCaptureStackBackTrace is the kernel32.dll export the Windows SDK's
        // CaptureStackBackTrace macro resolves to.
        "OutputDebugStringA",
        "IsDebuggerPresent",
        "RtlCaptureStackBackTrace",
        // Pre-Stage-56 "utilidades basicas" round (Alberto, 2026-08-29) -- Thread.SleepRaw and
        // File.CopyRaw/MoveRaw/Directory.MoveRaw (see src/Runtime/WindowsRuntimeBuilder.cpp)
        // import these directly from KERNEL32.dll. MoveFileA is shared by BOTH File.MoveRaw and
        // Directory.MoveRaw (one Win32 call, two NativeSharp-level operations).
        "Sleep",
        "CopyFileA",
        "MoveFileA"});
}

std::vector<PlannedLibrary> planWindowsImportLibraries(
    std::span<const ExternalImportModule> externalImports)
{
    std::vector<PlannedLibrary> planned;
    std::map<std::string, std::size_t> byUpperName;
    addSymbols(planned, byUpperName, "KERNEL32.dll", kernel32FoundationSymbols());
    addSymbols(planned, byUpperName, "KERNEL32.dll", kernel32PlatformSymbols());
    const auto kernelbaseSymbols = std::to_array<std::string_view>({"__chkstk"});
    // Stage 53.5 "Command-line arguments" -- CommandLineToArgvW is the one Win32 symbol the
    // runtime imports from SHELL32.dll rather than KERNEL32/KERNELBASE.
    const auto shell32Symbols = std::to_array<std::string_view>({"CommandLineToArgvW"});

    // Compiler-support imports from ucrtbase.dll, driven by what the IR actually references:
    // LLVM passes can materialize @llvm.memcpy/memset/memmove (LoopIdiomRecognize rewriting a
    // hand-rolled copy loop) or reference the SEH personality in either module, so the caller
    // hands in BOTH modules' IR text concatenated and the scan decides.
    //
    // LOS CUATRO NOMBRES DE ABAJO SE BUSCAN SIN COMILLAS, Y ESO NO ES UN DESCUIDO. LLVM
    // entrecomilla un nombre en cuanto lleva un byte que no sea alfanumerico, `-`, `.` o `_`;
    // estos cuatro son fijos y no llevan ninguno, asi que salen impresos tal cual.
    //
    // Un simbolo NUESTRO no: todo el que sale de una identidad lleva el `$3A$3A` del `::` del
    // modulo y va SIEMPRE entre comillas --ver symbolFragmentForIdentityKey en TypeIdentity.h--.
    // Si algun dia hay que decidir un import mirando uno de esos, la busqueda tiene que
    // construirse con `irTextSymbolReference`: escrita a mano encontraria cero coincidencias, y
    // aqui un cero no da un rojo, da una biblioteca de importacion a la que le falta una entrada.
    // These entries are the compiler-support closure of a final NativeSharp image. They
    // used to be selected by scanning only the two IR modules compiled in the current command.
    // That is insufficient once a precompiled static library participates: its SEH tables and
    // LLVM-generated memcpy/memset/memmove calls no longer have IR available to scan. Keeping
    // this tiny support set stable lets object-only archives compose transitively without
    // vendoring process import libraries into every .lib.
    const auto ucrtSymbols = std::to_array<std::string_view>(
        {"strlen", "__C_specific_handler", "memset", "memcpy", "memmove", "fmaf"});
    addSymbols(planned, byUpperName, "KERNELBASE.dll", kernelbaseSymbols);
    addSymbols(planned, byUpperName, "SHELL32.dll", shell32Symbols);
    addSymbols(planned, byUpperName, "ucrtbase.dll", ucrtSymbols);
    for (const ExternalImportModule& module : externalImports)
    {
        if (module.dllName.empty() || module.symbols.empty()) continue;
        addSymbols(planned, byUpperName, module.dllName, module.symbols);
    }
    return planned;
}

bool materializeWindowsImportLibraries(
    const LldLinker& linker,
    const std::filesystem::path& directory,
    const std::vector<PlannedLibrary>& planned,
    std::vector<std::filesystem::path>& libraries,
    std::string& error)
{
    for (const PlannedLibrary& library : planned)
    {
        std::filesystem::path stem = upperAscii(library.canonicalDll);
        stem.replace_extension();
        const std::filesystem::path output =
            directory / (stem.string() + ".import.lib");
        std::vector<std::string_view> views;
        views.reserve(library.symbols.size());
        for (const std::string& symbol : library.symbols) views.push_back(symbol);
        if (!linker.createImportLibrary(library.canonicalDll, views, output, error))
        {
            error = library.canonicalDll + " import-library generation failed: " + error;
            return false;
        }
        libraries.push_back(output);
    }
    return true;
}
}

bool createWindowsImportLibraries(
    const LldLinker& linker,
    const std::filesystem::path& directory,
    std::string_view combinedIrText,
    std::span<const ExternalImportModule> externalImports,
    std::vector<std::filesystem::path>& libraries,
    std::string& error)
{
    libraries.clear();
    (void)combinedIrText; // Kept in the API for compatibility; precompiled archives require a stable closure.
    // The canonical runtime tables. The KERNEL32 list's per-stage comments are the project's
    // import history and moved here verbatim from the (formerly duplicated) builder copies.
    //
    // EL TAMANO SE DEDUCE, Y ESO ES UNA CORRECCION, NO UN ADORNO. Estas tres tablas se
    // declaraban como `std::array<std::string_view, N>` con la N escrita a mano. Anadir un
    // simbolo y no tocar la N da "error: excess elements in struct initializer" apuntando al
    // ULTIMO elemento de la lista -- no al que acabas de meter -- asi que el error senala una
    // linea que no se ha cambiado en meses. Medido el 2026-09-02: las tres altas del Bloque 4
    // (GetCurrentDirectoryA, CreateProcessA, GetExitCodeProcess) dejaron la N en 62 con 65
    // entradas y tumbaron el build de LAS DOS PASADAS de all.bat. Con std::to_array la cuenta
    // la lleva el compilador y ese fallo ya no se puede escribir.
    //
    // Merge: runtime tables first, then the project's external imports. A DLL is matched
    // case-insensitively ("user32.dll" == "USER32.DLL"); symbols are deduplicated so a project
    // may freely re-declare something the runtime already imports (GetTickCount64, say) without
    // producing a duplicate .def entry. New DLLs keep the canonical spelling the project wrote.
    const std::vector<PlannedLibrary> planned = planWindowsImportLibraries(externalImports);
    return materializeWindowsImportLibraries(linker, directory, planned, libraries, error);
}
}
