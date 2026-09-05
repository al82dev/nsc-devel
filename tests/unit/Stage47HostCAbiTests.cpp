#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Linker/CoffArchiveVerifier.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "nativesharp/Linker/WindowsRuntimeImports.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::linker::CoffArchiveVerifier;
using nativesharp::linker::LldLinker;
using nativesharp::linker::PeExecutableVerifier;
using nativesharp::linker::createWindowsImportLibraries;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view outputType,
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name><OutputType>" + std::string(outputType) +
            "</OutputType></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
            "</ItemGroup></Project>");
    writeFile(directory / "Api.ns", source);
}

ExitCode runDriver(
    std::span<const std::string_view> args,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    output = out.str();
    error = err.str();
    return result;
}

ExitCode emitProduct(
    std::string_view action,
    const std::filesystem::path& project,
    const std::filesystem::path& outputPath,
    bool optimize,
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::string outputText = outputPath.string();
    if (optimize)
    {
        const std::array<std::string_view, 5> args = {
            "-O2", action, projectText, "-o", outputText};
        return runDriver(args, output, error);
    }
    const std::array<std::string_view, 4> args = {
        action, projectText, "-o", outputText};
    return runDriver(args, output, error);
}

ExitCode dumpLlvm(
    const std::filesystem::path& project,
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::array<std::string_view, 2> args = {
        "--dump-project-oop-llvm", projectText};
    return runDriver(args, output, error);
}

bool contains(const std::span<const std::string> values, std::string_view expected)
{
    for (const std::string& value : values)
    {
        if (value == expected) return true;
    }
    return false;
}

std::string quote(const std::filesystem::path& path)
{
    return "\"" + path.string() + "\"";
}

bool runCommand(std::string command, const std::filesystem::path& logPath)
{
    command += " >" + quote(logPath) + " 2>&1";
#ifdef _WIN32
    // Windows-only cmd.exe quirk (std::system runs "cmd.exe /c <command>"): when the
    // command line starts with a quoted executable path AND contains more than two
    // quote characters in total (true here -- the executable plus every quoted path
    // argument), cmd.exe abandons its simple "strip the two outer quotes" fast path and
    // falls back to a much cruder legacy parse that can misinterpret the remaining
    // quoted arguments entirely, surfacing as a bogus "The filename, directory name, or
    // volume label syntax is incorrect." error -- never seen on Linux/macOS, where
    // std::system runs "/bin/sh -c <command>" and this quoting rule does not exist. The
    // documented workaround is to wrap the WHOLE command in one more outer pair of
    // quotes: cmd.exe /c then strips exactly that outer pair before parsing the
    // (now-correctly-quoted) command underneath.
    command = "\"" + command + "\"";
#endif
    return std::system(command.c_str()) == 0;
}

const char* hostApiSource()
{
    return R"NS(
namespace Stage47.Host
{
    extern C delegate int IntCallback(int value);

    public enum Mode
    {
        First = 1,
        Second = 2
    }

    public struct Pair
    {
        public int X;
        public int Y;
    }

    public class Counter
    {
        public int Value;
        public Counter(int value) { Value = value; }
    }

    public class Api
    {
        private static IntCallback StoredCallback;

        public static export extern C int Add(int left, int right) { return left + right; }
        public static export extern C int Invoke(IntCallback callback, int value) { return callback(value); }
        public static export extern C void SetCallback(IntCallback callback) { StoredCallback = callback; }
        public static export extern C int CallCallback(int value) { return StoredCallback(value); }
        public static export extern C void* EchoPointer(void* value) { return value; }
        public static export extern C Pair EchoPair(Pair value) { return value; }
        public static export extern C Mode EchoMode(Mode value) { return value; }
        public static export extern C Counter CounterCreate(int value) { return new Counter(value); }
        public static export extern C int CounterGet(Counter counter) { return counter.Value; }
        public static export extern C void CounterDestroy(Counter counter) { delete counter; }

        public static int InternalOnly(int value) { return value + 1; }
        private static int Hidden(int value) { return value + 2; }
    }
}
)NS";
}

void testStableExports(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "StableExports";
    writeProject(directory, "StableExports", "SharedLibrary", hostApiSource());

    std::string output;
    std::string error;
    const std::filesystem::path dll = directory / "HostApi.dll";
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            directory / "StableExports.nsproj",
            dll,
            true,
            output,
            error) == ExitCode::Success,
        "Stage 47 emits a SharedLibrary containing explicitly selected C ABI exports");
    tests.expect(error.empty(), "valid Stage 47 host ABI generation keeps stderr empty");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verifyDynamicLibrary(dll),
        "Stage 47 C ABI DLL is a structurally valid PE32+ dynamic library");
    if (!verifier.verifyDynamicLibrary(dll)) return;

    const auto& info = verifier.info();
    const std::array<std::string_view, 10> expected = {
        "Add", "Invoke", "SetCallback", "CallCallback", "EchoPointer", "EchoPair", "EchoMode",
        "CounterCreate", "CounterGet", "CounterDestroy"};
    for (const std::string_view symbol : expected)
    {
        tests.expect(contains(info.exportedSymbols, symbol),
            std::string("stable C ABI export table contains exact symbol '") + std::string(symbol) + "'");
    }
    tests.expect(info.exportedSymbols.size() == expected.size(),
        "Stage 47 export selection contains only explicit export extern C methods");
    tests.expect(!contains(info.exportedSymbols, "InternalOnly") &&
            !contains(info.exportedSymbols, "Hidden"),
        "public/private NativeSharp methods are not exported without export extern C");
    tests.expect(info.machine == 0x8664 && info.optionalHeaderMagic == 0x020b,
        "Stage 47 host ABI DLL remains AMD64 PE32+");
    tests.expect(info.clrDirectoryRva == 0,
        "Stage 47 host ABI DLL remains completely native with no CLR header");
    tests.expect(info.exportDirectoryRva != 0 && info.exceptionDirectoryRva != 0,
        "Stage 47 DLL contains both PE exports and Win64 unwind metadata");

    CoffArchiveVerifier importVerifier;
    tests.expect(importVerifier.verify(directory / "HostApi.lib"),
        "Stage 47 DLL generation produces a valid matching import library");

    const std::filesystem::path dllO0 = directory / "HostApiO0.dll";
    output.clear();
    error.clear();
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            directory / "StableExports.nsproj",
            dllO0,
            false,
            output,
            error) == ExitCode::Success,
        "Stage 47 stable C ABI DLL also closes at O0");
    tests.expect(verifier.verifyDynamicLibrary(dllO0) &&
            contains(verifier.info().exportedSymbols, "Invoke") &&
            verifier.info().exportedSymbols.size() == expected.size(),
        "O0 and O2 preserve the same stable C export surface");
}

void testLlvmThunks(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "LlvmThunks";
    writeProject(directory, "LlvmThunks", "SharedLibrary", hostApiSource());

    std::string llvm;
    std::string error;
    tests.expect(dumpLlvm(directory / "LlvmThunks.nsproj", llvm, error) == ExitCode::Success,
        "Stage 47 LLVM lowering accepts a complete host ABI surface");
    tests.expect(error.empty(), "Stage 47 LLVM ABI dump keeps stderr empty");
    tests.expect(llvm.find("define i32 @Add(i32") != std::string::npos,
        "LLVM contains an unmangled C ABI thunk named Add");
    tests.expect(llvm.find("define i32 @Invoke(ptr") != std::string::npos,
        "LLVM C ABI callback thunk exposes a raw function pointer carrier");
    tests.expect(llvm.find("cabi.callback.0.cell") == std::string::npos &&
            llvm.find("cabi.callback.result") != std::string::npos,
        "extern C callback remains a raw native function pointer and is invoked directly");
    tests.expect(llvm.find("@__nsc_static_0_StoredCallback") != std::string::npos &&
            llvm.find("define void @SetCallback(ptr") != std::string::npos &&
            llvm.find("define i32 @CallCallback(i32") != std::string::npos,
        "extern C callbacks can persist in NativeSharp storage without a temporary delegate-cell lifetime");
    // NSC-LIMIT-009. LA BUSQUEDA LLEVA `@\"` Y NO `@`, y no es un adorno: todo simbolo que sale
    // de una identidad lleva el `$3A$3A` del `::` del modulo, y LLVM entrecomilla cualquier
    // nombre con un byte que no sea alfanumerico, `-`, `.` o `_`. Lo impreso es
    // `@\"__nsc_...\"`. Escrito a mano sin la comilla encuentra CERO, y en una asercion de
    // ausencia eso seria un verde falso. Ver TypeIdentity.h e `irTextSymbolReference`.
    tests.expect(llvm.find("@\"__nsc_method_") != std::string::npos,
        "internal NativeSharp methods remain mangled behind the stable C ABI thunks");
    tests.expect(llvm.find("define ptr @CounterCreate(i32") != std::string::npos,
        "class references cross the host boundary as opaque pointer-sized handles");
    tests.expect(llvm.find("define i64 @EchoPair(i64") != std::string::npos,
        "8-byte plain struct uses the Windows x64 integer carrier at the C ABI boundary");
}

void testStaticLibraryStableSymbols(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "StaticLibrary";
    writeProject(directory, "StaticLibrary", "StaticLibrary", hostApiSource());

    std::string output;
    std::string error;
    const std::filesystem::path library = directory / "HostApi.lib";
    tests.expect(emitProduct(
            "--emit-project-oop-lib",
            directory / "StaticLibrary.nsproj",
            library,
            true,
            output,
            error) == ExitCode::Success,
        "Stage 47 emits stable C ABI wrappers into a NativeSharp static library");
    CoffArchiveVerifier verifier;
    tests.expect(verifier.verify(library),
        "Stage 47 static host library remains a valid COFF archive");
}

void testSemanticGuards(TestContext& tests, const std::filesystem::path& root)
{
    struct InvalidCase
    {
        const char* name;
        const char* source;
        const char* expected;
    };

    const std::array<InvalidCase, 10> cases = {{
        {"OnlyExport", R"NS(class Api { public static export int Bad(int x) { return x; } })NS",
            "paired modifier sequence 'export extern C'"},
        {"NotPublic", R"NS(class Api { private static export extern C int Bad(int x) { return x; } })NS",
            "ordinary public static concrete method"},
        {"NotStatic", R"NS(class Api { public export extern C int Bad(int x) { return x; } })NS",
            "ordinary public static concrete method"},
        {"Generic", R"NS(class Api { public static export extern C T Bad<T>(T x) { return x; } })NS",
            "does not expose generic methods"},
        {"String", R"NS(class Api { public static export extern C string Bad(string x) { return x; } })NS",
            "cannot export return type"},
        {"DynamicArray", R"NS(class Api { public static export extern C int[] Bad(int[] x) { return x; } })NS",
            "cannot export return type"},
        {"ManagedDelegate", R"NS(delegate int Callback(int x); class Api { public static export extern C int Bad(Callback cb, int x) { return cb(x); } })NS",
            "cannot export parameter type"},
        // C1.1 "function-pointer round trip" flipped this case: an EXTERN C delegate return is a
        // raw C function pointer and is now legal (it is what makes a `wglGetProcAddress`-shaped
        // declaration expressible with its real signature -- see Stage56C1InteropCompletionTests).
        // What stays rejected is the ORDINARY delegate, which is a 16-byte code+receiver heap
        // cell with no C shape at all, and that is the case pinned here now.
        {"ManagedDelegateReturn", R"NS(delegate int Callback(int x); class Api { public static export extern C Callback Bad() { return null; } })NS",
            "does not return delegate values"},
        {"Duplicate", R"NS(class A { public static export extern C int Ping(int x) { return x; } } class B { public static export extern C int Ping(int x) { return x + 1; } })NS",
            "duplicate Stage 47 C ABI export name 'Ping'"},
        {"RuntimeCollision", R"NS(class Api { public static export extern C int ExitProcess(int x) { return x; } })NS",
            "collides with a reserved NativeSharp/runtime/toolchain symbol"}
    }};

    for (const InvalidCase& invalid : cases)
    {
        const std::filesystem::path directory = root / "Invalid" / invalid.name;
        writeProject(directory, invalid.name, "SharedLibrary", invalid.source);
        std::string output;
        std::string error;
        const ExitCode result = emitProduct(
            "--emit-project-oop-dll",
            directory / (std::string(invalid.name) + ".nsproj"),
            directory / (std::string(invalid.name) + ".dll"),
            false,
            output,
            error);
        tests.expect(result == ExitCode::CompilationError,
            std::string("Stage 47 rejects invalid host ABI case ") + invalid.name);
        tests.expect(error.find(invalid.expected) != std::string::npos,
            std::string("Stage 47 reports precise diagnostic for ") + invalid.name);
    }

    const std::filesystem::path noExports = root / "Invalid" / "NoExports";
    writeProject(noExports, "NoExports", "SharedLibrary", R"NS(
class Api { public static int Internal(int x) { return x; } }
)NS");
    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            noExports / "NoExports.nsproj",
            noExports / "NoExports.dll",
            false,
            output,
            error) == ExitCode::CompilationError,
        "Stage 47 SharedLibrary requires an explicit stable C ABI export surface");
    tests.expect(error.find("requires at least one 'public static export extern C' method") != std::string::npos,
        "DLL-without-exports diagnostic names the required Stage 47 contract");
}

void testParserMemberRestrictions(TestContext& tests, const std::filesystem::path& root)
{
    const std::array<std::string_view, 4> invalidMembers = {
        "class Api { public static export extern C int operator +(Api a, Api b) { return 0; } }",
        "class Api { public static export extern C implicit operator int(Api a) { return 0; } }",
        "delegate void D(); class Api { public static export extern C event D Changed; }",
        "class Api { public export extern C int this[int i] { get { return i; } } }"};

    for (std::size_t index = 0; index < invalidMembers.size(); ++index)
    {
        const std::filesystem::path directory = root / "InvalidMember" / std::to_string(index);
        writeProject(directory, "InvalidMember", "SharedLibrary", invalidMembers[index]);
        std::string output;
        std::string error;
        const ExitCode result = emitProduct(
            "--emit-project-oop-dll",
            directory / "InvalidMember.nsproj",
            directory / "InvalidMember.dll",
            false,
            output,
            error);
        tests.expect(result == ExitCode::CompilationError,
            "export extern C is rejected on a non-ordinary class member");
        tests.expect(error.find("valid only on ordinary static methods in Stage 47") != std::string::npos,
            "parser explains that C ABI exports are restricted to ordinary static methods");
    }
}

// Materializa la superficie de imports del runtime para enlazar un host contra la biblioteca
// ESTATICA. En el enlace estatico el objeto de runtime entra dentro del ejecutable del host, asi
// que el host necesita la lista entera de simbolos Win32 del runtime, no solo los que su propio
// codigo llama. (En la variante DLL no hace falta: el runtime va dentro de la DLL.)
//
// LLAMA A LA FUNCION DEL COMPILADOR, y eso es la correccion del 2026-09-02. Hasta hoy este helper
// tenia su PROPIA COPIA de las cuatro tablas de simbolos --las 62 entradas de KERNEL32
// incluidas--, pegada a mano, y la misma copia estaba repetida en los tres ficheros de prueba de
// host (Stage47, Stage48_3, Stage48_4).
//
// WindowsRuntimeImports.h se describe a si mismo como "THE single source of truth", y su
// comentario de HISTORY cuenta que la consolidacion elimino las copias duplicadas. Elimino DOS DE
// CINCO --las de WindowsExecutableBuilder.cpp y WindowsLibraryBuilder.cpp-- y dejo estas tres,
// que nadie volvio a mirar porque estaban en tests/.
//
// LO QUE ESO COSTO, medido: el Bloque 4 anadio GetCurrentDirectoryA, CreateProcessA y
// GetExitCodeProcess a la tabla del compilador. Estas copias se quedaron en 62 entradas, el
// enlace del host estatico no pudo resolver esos tres simbolos, y las tres pruebas se pusieron
// rojas por una lista copiada -- no por el codigo que miden. Una tabla duplicada no verifica
// nada: solo se queda vieja en silencio y despues acusa al cambio equivocado.
//
// NO SE RELAJA NINGUNA COMPROBACION AL HACER ESTO. Salen las mismas cuatro bibliotecas de
// importacion, en el mismo orden (KERNEL32, KERNELBASE, SHELL32, ucrtbase), y detras quedan
// exactamente los mismos expect() de antes. Lo unico que cambia es de donde sale la lista de
// simbolos: de la tabla real del compilador en vez de una copia.
bool createRuntimeImports(
    const LldLinker& linker,
    const std::filesystem::path& directory,
    std::vector<std::filesystem::path>& imports,
    std::string& error)
{
    // Los cuatro simbolos condicionales de ucrtbase los decide un escaneo del texto del IR
    // (@__C_specific_handler y los tres @llvm.mem*), porque las pasadas de LLVM pueden
    // materializarlos en cualquiera de los dos modulos. Aqui no hay un IR concreto que escanear
    // --se esta preparando el enlace de un host ajeno-- asi que se le pasa un texto que los
    // nombra a los cuatro, que es exactamente el conjunto que la copia anterior pedia a mano.
    //
    // Pedir de mas no cuesta: una biblioteca de importacion es un archivo PEREZOSO y el enlazador
    // solo saca de ella los miembros que alguien referencia. Pedir de menos es justo el fallo que
    // esto viene a arreglar.
    const std::string_view allCompilerSupportSymbols =
        "@__C_specific_handler @llvm.memset @llvm.memcpy @llvm.memmove";
    return createWindowsImportLibraries(
        linker, directory, allCompilerSupportSymbols, {}, imports, error);
}

void testNativeHosts(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "NativeHosts";
    const std::filesystem::path dynamicDirectory = directory / "Dynamic";
    const std::filesystem::path staticDirectory = directory / "Static";
    writeProject(dynamicDirectory, "DynamicApi", "SharedLibrary", hostApiSource());
    writeProject(staticDirectory, "StaticApi", "StaticLibrary", hostApiSource());

    std::string output;
    std::string error;
    const std::filesystem::path dynamicDll = dynamicDirectory / "DynamicApi.dll";
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            dynamicDirectory / "DynamicApi.nsproj",
            dynamicDll,
            true,
            output,
            error) == ExitCode::Success,
        "native-host certification builds the NativeSharp DLL under test");

    const std::filesystem::path staticLib = staticDirectory / "StaticApi.lib";
    output.clear();
    error.clear();
    tests.expect(emitProduct(
            "--emit-project-oop-lib",
            staticDirectory / "StaticApi.nsproj",
            staticLib,
            true,
            output,
            error) == ExitCode::Success,
        "native-host certification builds the NativeSharp static library under test");

    writeFile(directory / "host.c", R"C(
__declspec(dllimport) int Add(int, int);
__declspec(dllimport) int Invoke(int (*)(int), int);
__declspec(dllimport) void SetCallback(int (*)(int));
__declspec(dllimport) int CallCallback(int);
__declspec(dllimport) void* EchoPointer(void*);
typedef struct Pair { int X; int Y; } Pair;
__declspec(dllimport) Pair EchoPair(Pair);
__declspec(dllimport) int EchoMode(int);
__declspec(dllimport) void* CounterCreate(int);
__declspec(dllimport) int CounterGet(void*);
__declspec(dllimport) void CounterDestroy(void*);
static int Twice(int value) { return value * 2; }
__declspec(dllexport) int HostProbe(void)
{
    int marker = 7;
    Pair pair = { 20, 22 };
    Pair echoed = EchoPair(pair);
    SetCallback(Twice);
    void* counter = CounterCreate(42);
    int result = Add(20, 22) == 42 && Invoke(Twice, 21) == 42 && CallCallback(21) == 42 &&
        EchoPointer(&marker) == &marker && EchoMode(2) == 2 &&
        echoed.X == 20 && echoed.Y == 22 && CounterGet(counter) == 42;
    CounterDestroy(counter);
    return result ? 0 : 1;
}
)C");
    writeFile(directory / "host.cpp", R"CPP(
extern "C" __declspec(dllimport) int Add(int, int);
extern "C" __declspec(dllimport) int Invoke(int (*)(int), int);
extern "C" __declspec(dllimport) void SetCallback(int (*)(int));
extern "C" __declspec(dllimport) int CallCallback(int);
struct Pair { int X; int Y; };
extern "C" __declspec(dllimport) Pair EchoPair(Pair);
extern "C" __declspec(dllimport) int EchoMode(int);
extern "C" __declspec(dllimport) void* EchoPointer(void*);
static int Triple(int value) { return value * 3; }
extern "C" __declspec(dllexport) int CppHostProbe(void)
{
    int marker = 9;
    Pair pair{ 30, 12 };
    Pair echoed = EchoPair(pair);
    SetCallback(Triple);
    return Add(30, 12) == 42 && Invoke(Triple, 14) == 42 && CallCallback(14) == 42 && EchoPointer(&marker) == &marker &&
        EchoMode(1) == 1 && echoed.X == 30 && echoed.Y == 12 ? 0 : 1;
}
)CPP");

    tests.expect(runCommand(
            "clang --target=x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -c " +
                quote(directory / "host.c") + " -o " + quote(directory / "host_c.obj"),
            directory / "clang_c.log"),
        "Clang compiles a freestanding Windows C host against the Stage 47 declarations");
    tests.expect(runCommand(
            "clang++ --target=x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -fno-exceptions -fno-rtti -c " +
                quote(directory / "host.cpp") + " -o " + quote(directory / "host_cpp.obj"),
            directory / "clang_cpp.log"),
        "Clang compiles a freestanding Windows C++ host using extern C declarations");

    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    tests.expect(linker != nullptr, "Stage 47 native-host certification discovers lld-link");
    if (linker == nullptr) return;

    const std::string linkerPath = quote(linker->executablePath());
    tests.expect(runCommand(
            linkerPath + " /dll /noentry /nodefaultlib /machine:x64 /out:" +
                quote(directory / "CHost.dll") + " /export:HostProbe " +
                quote(directory / "host_c.obj") + " " + quote(dynamicDirectory / "DynamicApi.lib"),
            directory / "link_c_dynamic.log"),
        "C host links successfully against the NativeSharp DLL import library");
    tests.expect(runCommand(
            linkerPath + " /dll /noentry /nodefaultlib /machine:x64 /out:" +
                quote(directory / "CppHost.dll") + " /export:CppHostProbe " +
                quote(directory / "host_cpp.obj") + " " + quote(dynamicDirectory / "DynamicApi.lib"),
            directory / "link_cpp_dynamic.log"),
        "C++ host links successfully against the NativeSharp DLL import library");

    PeExecutableVerifier peVerifier;
    tests.expect(peVerifier.verifyDynamicLibrary(directory / "CHost.dll") &&
            peVerifier.info().machine == 0x8664,
        "linked C host is a valid AMD64 PE32+ DLL");
    tests.expect(peVerifier.verifyDynamicLibrary(directory / "CppHost.dll") &&
            peVerifier.info().machine == 0x8664,
        "linked C++ host is a valid AMD64 PE32+ DLL");

    std::vector<std::filesystem::path> runtimeImports;
    tests.expect(createRuntimeImports(*linker, directory, runtimeImports, error),
        "Stage 47 test creates the Windows runtime import libraries needed by static consumers");
    if (runtimeImports.size() != 4) return;

    std::string cStatic = std::string(
        "int Add(int,int); int Invoke(int (*)(int),int); void* EchoPointer(void*); ") +
        "static int Twice(int x){return x*2;} "
        "__declspec(dllexport) int HostProbe(void){int m=1; return Add(20,22)==42 && Invoke(Twice,21)==42 && EchoPointer(&m)==&m ? 0 : 1;}";
    writeFile(directory / "host_static.c", cStatic);
    std::string cppStatic = std::string(
        "extern \"C\" int Add(int,int); extern \"C\" int Invoke(int (*)(int),int); extern \"C\" void* EchoPointer(void*); ") +
        "static int Triple(int x){return x*3;} "
        "extern \"C\" __declspec(dllexport) int CppHostProbe(void){int m=1; return Add(30,12)==42 && Invoke(Triple,14)==42 && EchoPointer(&m)==&m ? 0 : 1;}";
    writeFile(directory / "host_static.cpp", cppStatic);

    tests.expect(runCommand(
            "clang --target=x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -c " +
                quote(directory / "host_static.c") + " -o " + quote(directory / "host_static_c.obj"),
            directory / "clang_static_c.log"),
        "Clang compiles a C host for direct static NativeSharp linkage");
    tests.expect(runCommand(
            "clang++ --target=x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -fno-exceptions -fno-rtti -c " +
                quote(directory / "host_static.cpp") + " -o " + quote(directory / "host_static_cpp.obj"),
            directory / "clang_static_cpp.log"),
        "Clang compiles a C++ host for direct static NativeSharp linkage");

    const std::string staticDependencies = " " + quote(staticLib) + " " + quote(runtimeImports[0]) +
        " " + quote(runtimeImports[1]) + " " + quote(runtimeImports[2]) + " " + quote(runtimeImports[3]);
    tests.expect(runCommand(
            linkerPath + " /dll /noentry /nodefaultlib /machine:x64 /out:" +
                quote(directory / "CStaticHost.dll") + " /export:HostProbe " +
                quote(directory / "host_static_c.obj") + staticDependencies,
            directory / "link_c_static.log"),
        "C host links directly against a NativeSharp static library");
    tests.expect(runCommand(
            linkerPath + " /dll /noentry /nodefaultlib /machine:x64 /out:" +
                quote(directory / "CppStaticHost.dll") + " /export:CppHostProbe " +
                quote(directory / "host_static_cpp.obj") + staticDependencies,
            directory / "link_cpp_static.log"),
        "C++ host links directly against a NativeSharp static library");
    tests.expect(peVerifier.verifyDynamicLibrary(directory / "CStaticHost.dll"),
        "C + NativeSharp static-library link produces a valid PE32+ host image");
    tests.expect(peVerifier.verifyDynamicLibrary(directory / "CppStaticHost.dll"),
        "C++ + NativeSharp static-library link produces a valid PE32+ host image");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_stage47_host_c_abi");
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root, cleanupError);

    testStableExports(tests, root);
    testLlvmThunks(tests, root);
    testStaticLibraryStableSymbols(tests, root);
    testSemanticGuards(tests, root);
    testParserMemberRestrictions(tests, root);
    testNativeHosts(tests, root);

    return tests.resultKeeping(root);
}
