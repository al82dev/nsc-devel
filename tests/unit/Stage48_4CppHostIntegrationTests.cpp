#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "nativesharp/Linker/WindowsRuntimeImports.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
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

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream text;
    text << stream.rdbuf();
    return text.str();
}

std::string quote(const std::filesystem::path& path)
{
    return "\"" + path.string() + "\"";
}

bool runCommand(std::string command, const std::filesystem::path& logPath)
{
    command += " >" + quote(logPath) + " 2>&1";
#ifdef _WIN32
    // See the matching comment in Stage47HostCAbiTests.cpp -- cmd.exe /c misparses a
    // command line with more than two quote characters, wrapping the whole thing in one
    // more outer pair of quotes is the documented workaround.
    command = "\"" + command + "\"";
#endif
    return std::system(command.c_str()) == 0;
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
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::string outputText = outputPath.string();
    const std::array<std::string_view, 5> args = {
        "-O2", action, projectText, "-o", outputText};
    return runDriver(args, output, error);
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view outputType,
    std::string_view source)
{
    writeFile(
        directory / "Stage48_4HostApi.nsproj",
        "<Project><PropertyGroup><Name>Stage48_4HostApi</Name><OutputType>" +
            std::string(outputType) +
            "</OutputType></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
            "</ItemGroup></Project>");
    writeFile(directory / "Library.ns", source);
}

const char* apiSource()
{
    return R"NS(
namespace NativeSharp.Stage48_4
{
    extern C delegate int IntCallback(int value);
    extern C delegate int BufferCallback(int* values, int count);

    public enum HostMode
    {
        First = 1,
        Second = 2
    }

    public struct Pair
    {
        public int Left;
        public int Right;
    }

    public struct LayoutProbe
    {
        public byte Tag;
        public int Value;
    }

    public class Counter
    {
        public int Value;
        public Counter(int value) { Value = value; }
    }

    public class HostApi
    {
        private static IntCallback StoredCallback;

        public static export extern C int NS484_Add(int left, int right) { return left + right; }
        public static export extern C int* NS484_EchoIntPointer(int* values) { return values; }
        public static export extern C int NS484_InvokeBuffer(BufferCallback callback, int* values, int count)
        {
            return callback(values, count);
        }
        public static export extern C void* NS484_EchoPointer(void* value) { return value; }
        public static export extern C Pair NS484_EchoPair(Pair value) { return value; }
        public static export extern C HostMode NS484_EchoMode(HostMode value) { return value; }
        public static export extern C LayoutProbe NS484_EchoLayout(LayoutProbe value) { return value; }
        public static export extern C int NS484_Invoke(IntCallback callback, int value) { return callback(value); }
        public static export extern C void NS484_SetCallback(IntCallback callback) { StoredCallback = callback; }
        public static export extern C int NS484_CallCallback(int value) { return StoredCallback(value); }
        public static export extern C Counter NS484_Counter_Create(int value) { return new Counter(value); }
        public static export extern C int NS484_Counter_Get(Counter counter) { return counter.Value; }
        public static export extern C void NS484_Counter_Add(Counter counter, int delta)
        {
            counter.Value = counter.Value + delta;
        }
        public static export extern C void NS484_Counter_Destroy(Counter counter) { delete counter; }
    }
}
)NS";
}

const char* cppHostSource()
{
    return R"CPP(
#include "Stage48_4HostApi.h"

static int32_t Twice(int32_t value)
{
    return value * 2;
}

static int32_t SumBuffer(int32_t* values, int32_t count)
{
    int32_t sum = 0;
    for (int32_t i = 0; i < count; ++i)
    {
        sum += values[i];
    }
    return sum;
}

class CounterHandle final
{
public:
    explicit CounterHandle(int32_t value)
        : handle_(NS484_Counter_Create(value))
    {
    }

    ~CounterHandle()
    {
        if (handle_ != nullptr) NS484_Counter_Destroy(handle_);
    }

    CounterHandle(const CounterHandle&) = delete;
    CounterHandle& operator=(const CounterHandle&) = delete;

    bool valid() const { return handle_ != nullptr; }
    void add(int32_t delta) { NS484_Counter_Add(handle_, delta); }
    int32_t value() const { return NS484_Counter_Get(handle_); }

private:
    NS_Counter* handle_;
};

int main()
{
    int32_t values[4] = {1, 2, 3, 4};
    NS_Pair pair{20, 22};
    NS_LayoutProbe layout{1, 42};
    int marker = 7;

    if (NS484_Add(20, 22) != 42) return 1;
    if (NS484_EchoIntPointer(values) != values) return 2;
    if (NS484_InvokeBuffer(SumBuffer, values, 4) != 10) return 3;
    if (NS484_EchoPointer(&marker) != &marker) return 4;
    pair = NS484_EchoPair(pair);
    if (pair.Left != 20 || pair.Right != 22) return 5;
    if (NS484_EchoMode(static_cast<NS_HostMode>(2)) != static_cast<NS_HostMode>(2)) return 6;
    layout = NS484_EchoLayout(layout);
    if (layout.Tag != 1 || layout.Value != 42) return 7;
    if (NS484_Invoke(Twice, 21) != 42) return 8;
    NS484_SetCallback(Twice);
    if (NS484_CallCallback(21) != 42) return 9;

    CounterHandle counter(40);
    if (!counter.valid()) return 10;
    counter.add(2);
    if (counter.value() != 42) return 11;
    return 0;
}
)CPP";
}

bool contains(const std::vector<std::string>& values, std::string_view expected)
{
    for (const std::string& value : values)
    {
        if (value == expected) return true;
    }
    return false;
}

bool importsSymbol(
    const nativesharp::linker::PeExecutableInfo& info,
    std::string_view library,
    std::string_view symbol)
{
    for (const auto& imported : info.importedSymbols)
    {
        if (imported.library == library && imported.name == symbol) return true;
    }
    return false;
}

bool hasDecoratedNativeSharpImport(const nativesharp::linker::PeExecutableInfo& info)
{
    for (const auto& imported : info.importedSymbols)
    {
        if (imported.library != "Stage48_4HostApi.dll") continue;
        if (!imported.name.empty() && (imported.name.front() == '?' || imported.name.front() == '_'))
        {
            return true;
        }
    }
    return false;
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

std::string clangCppCommand(
    const std::filesystem::path& includeDirectory,
    const std::filesystem::path& source,
    const std::filesystem::path& object)
{
    return "clang++ --target=x86_64-pc-windows-msvc -fms-extensions -fdeclspec "
        "-ffreestanding -fno-stack-protector -fno-exceptions -fno-rtti "
        "-std=c++20 -Wall -Wextra -Werror -Wpedantic -c -I" +
        quote(includeDirectory) + " " + quote(source) + " -o " + quote(object);
}

void testGeneratedCppContract(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "Contract";
    writeProject(directory, "SharedLibrary", apiSource());
    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            directory / "Stage48_4HostApi.nsproj",
            directory / "Stage48_4HostApi.dll",
            output,
            error) == ExitCode::Success,
        "Stage 48.4 builds the C++-host DLL product");
    tests.expect(error.empty(), "valid Stage 48.4 C++ host product keeps stderr empty");

    const std::string header = readFile(directory / "Stage48_4HostApi.h");
    tests.expect(header.find("#ifdef __cplusplus") != std::string::npos &&
            header.find("extern \"C\"") != std::string::npos,
        "generated host header automatically switches exported declarations to C linkage under C++");
    tests.expect(header.find("#define NS_STATIC_ASSERT(condition, message) static_assert") != std::string::npos,
        "generated ABI layout guards use native C++ static_assert in C++ mode");
    tests.expect(header.find("typedef struct NS_Counter NS_Counter;") != std::string::npos,
        "NativeSharp classes stay opaque C handles rather than becoming C++ classes");
    tests.expect(header.find("NS_API int32_t NS_CALL NS484_Add") != std::string::npos &&
            header.find("NS_API NS_Counter* NS_CALL NS484_Counter_Create") != std::string::npos,
        "C++ host sees the exact Stage47 C ABI function surface");
    tests.expect(header.find("class NS_Counter") == std::string::npos &&
            header.find("virtual ") == std::string::npos,
        "generated C++ consumption contract exposes no NativeSharp or C++ vtable surface");
}

void testSharedLibraryCppHost(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "SharedHost";
    writeProject(directory, "SharedLibrary", apiSource());
    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            directory / "Stage48_4HostApi.nsproj",
            directory / "Stage48_4HostApi.dll",
            output,
            error) == ExitCode::Success,
        "Stage 48.4 emits the NativeSharp DLL consumed by C++20");

    writeFile(directory / "host.cpp", cppHostSource());
    tests.expect(runCommand(
            clangCppCommand(directory, directory / "host.cpp", directory / "host.obj"),
            directory / "clang_cpp_host.log"),
        "Clang C++20 compiles a strict Windows x64 host from only the generated header");

    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    tests.expect(linker != nullptr, "Stage 48.4 discovers lld-link for C++ host certification");
    if (linker == nullptr) return;

    tests.expect(runCommand(
            quote(linker->executablePath()) +
                " /entry:main /subsystem:console /nodefaultlib /machine:x64 /out:" +
                quote(directory / "host_cpp_shared.exe") + " " + quote(directory / "host.obj") + " " +
                quote(directory / "Stage48_4HostApi.lib"),
            directory / "link_cpp_host.log"),
        "C++20 host links through the NativeSharp DLL import library without a C++ ABI bridge");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verify(directory / "host_cpp_shared.exe"),
        "shared C++ host is a structurally valid PE32+ executable");
    if (!verifier.verify(directory / "host_cpp_shared.exe")) return;
    const auto& info = verifier.info();
    tests.expect(info.machine == 0x8664 && info.optionalHeaderMagic == 0x020b && info.entryPointRva != 0,
        "shared C++ host is AMD64 PE32+ with a concrete entry point");
    tests.expect(info.subsystem == 3 && info.clrDirectoryRva == 0,
        "shared C++ host is a native Windows console image with no CLR header");
    tests.expect(contains(info.importedLibraries, "Stage48_4HostApi.dll"),
        "shared C++ host imports the NativeSharp DLL by product name");

    const std::array<std::string_view, 14> expectedSymbols = {
        "NS484_Add", "NS484_EchoIntPointer", "NS484_InvokeBuffer", "NS484_EchoPointer",
        "NS484_EchoPair", "NS484_EchoMode", "NS484_EchoLayout", "NS484_Invoke",
        "NS484_SetCallback", "NS484_CallCallback", "NS484_Counter_Create", "NS484_Counter_Get",
        "NS484_Counter_Add", "NS484_Counter_Destroy"};
    for (const std::string_view symbol : expectedSymbols)
    {
        tests.expect(importsSymbol(info, "Stage48_4HostApi.dll", symbol),
            std::string("C++ host imports unmangled C ABI symbol '") + std::string(symbol) + "'");
    }
    tests.expect(!hasDecoratedNativeSharpImport(info),
        "C++ host PE import table contains no MSVC C++-decorated NativeSharp symbol");
}

void testStaticLibraryCppHost(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "StaticHost";
    writeProject(directory, "StaticLibrary", apiSource());
    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-lib",
            directory / "Stage48_4HostApi.nsproj",
            directory / "Stage48_4HostApi.lib",
            output,
            error) == ExitCode::Success,
        "Stage 48.4 emits the NativeSharp static library consumed by C++20");

    const std::string header = readFile(directory / "Stage48_4HostApi.h");
    tests.expect(header.find("#define NS_API\n") != std::string::npos &&
            header.find("__declspec(dllimport)") == std::string::npos,
        "static C++ host contract contains no DLL import decoration");

    writeFile(directory / "host.cpp", cppHostSource());
    tests.expect(runCommand(
            clangCppCommand(directory, directory / "host.cpp", directory / "host.obj"),
            directory / "clang_cpp_host.log"),
        "Clang C++20 compiles the same host source against the static-library header");

    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    tests.expect(linker != nullptr, "Stage 48.4 discovers lld-link for static C++ host certification");
    if (linker == nullptr) return;

    std::vector<std::filesystem::path> imports;
    tests.expect(createRuntimeImports(*linker, directory, imports, error),
        "Stage 48.4 materializes only NativeSharp runtime imports needed by static C++ linkage");
    if (imports.size() != 4) return;

    const std::string dependencies = " " + quote(directory / "Stage48_4HostApi.lib") + " " +
        quote(imports[0]) + " " + quote(imports[1]) + " " + quote(imports[2]) + " " + quote(imports[3]);
    tests.expect(runCommand(
            quote(linker->executablePath()) +
                " /entry:main /subsystem:console /nodefaultlib /machine:x64 /out:" +
                quote(directory / "host_cpp_static.exe") + " " + quote(directory / "host.obj") + dependencies,
            directory / "link_cpp_host.log"),
        "C++20 host links directly against NativeSharp static library without C++ runtime dependencies");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verify(directory / "host_cpp_static.exe"),
        "static C++ host is a structurally valid PE32+ executable");
    if (!verifier.verify(directory / "host_cpp_static.exe")) return;
    const auto& info = verifier.info();
    tests.expect(info.machine == 0x8664 && info.optionalHeaderMagic == 0x020b && info.entryPointRva != 0,
        "static C++ host is AMD64 PE32+ with a concrete entry point");
    tests.expect(info.subsystem == 3 && info.clrDirectoryRva == 0,
        "static C++ host remains native and has no CLR header");
    tests.expect(!contains(info.importedLibraries, "Stage48_4HostApi.dll"),
        "static C++ host has no dependency on a NativeSharp DLL");
    tests.expect(contains(info.importedLibraries, "KERNEL32.dll") &&
            contains(info.importedLibraries, "KERNELBASE.dll"),
        "static C++ host imports the expected Windows runtime surface only");
    tests.expect(info.exceptionDirectoryRva != 0,
        "static C++ host retains NativeSharp Win64 unwind metadata");
}

void testCppLinkageAndTypeGuards(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "Guards";
    writeProject(directory, "SharedLibrary", apiSource());
    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            directory / "Stage48_4HostApi.nsproj",
            directory / "Stage48_4HostApi.dll",
            output,
            error) == ExitCode::Success,
        "Stage 48.4 creates the contract used by C++ negative tests");

    writeFile(directory / "bad_opaque.cpp", R"CPP(
#include "Stage48_4HostApi.h"
int bad(NS_Counter* counter) { return counter->Value; }
)CPP");
    tests.expect(!runCommand(
            clangCppCommand(directory, directory / "bad_opaque.cpp", directory / "bad_opaque.obj"),
            directory / "bad_opaque.log"),
        "C++ cannot dereference a NativeSharp opaque class handle");
    tests.expect(readFile(directory / "bad_opaque.log").find("incomplete") != std::string::npos,
        "opaque C++ misuse fails because NativeSharp object layout is intentionally hidden");

    writeFile(directory / "bad_callback.cpp", R"CPP(
#include "Stage48_4HostApi.h"
static int32_t Wrong(int32_t* value) { return value == nullptr ? 0 : *value; }
void bad() { NS484_SetCallback(Wrong); }
)CPP");
    tests.expect(!runCommand(
            clangCppCommand(directory, directory / "bad_callback.cpp", directory / "bad_callback.obj"),
            directory / "bad_callback.log"),
        "strict C++20 build rejects callback signature mismatch at compile time");
    const std::string callbackLog = readFile(directory / "bad_callback.log");
    tests.expect(callbackLog.find("incompatible") != std::string::npos ||
            callbackLog.find("no viable conversion") != std::string::npos ||
            callbackLog.find("no matching function") != std::string::npos ||
            callbackLog.find("no known conversion") != std::string::npos,
        "callback misuse diagnostic identifies the C function-pointer incompatibility");

    writeFile(directory / "bad_mangled.cpp", R"CPP(
#include <stdint.h>
int32_t NS484_Add(int32_t left, int32_t right);
int main() { return NS484_Add(20, 22) == 42 ? 0 : 1; }
)CPP");
    tests.expect(runCommand(
            clangCppCommand(directory, directory / "bad_mangled.cpp", directory / "bad_mangled.obj"),
            directory / "bad_mangled_compile.log"),
        "control C++ source without generated header compiles with ordinary C++ linkage");

    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    tests.expect(linker != nullptr, "Stage 48.4 discovers lld-link for C++ linkage negative test");
    if (linker == nullptr) return;
    tests.expect(!runCommand(
            quote(linker->executablePath()) +
                " /entry:main /subsystem:console /nodefaultlib /machine:x64 /out:" +
                quote(directory / "bad_mangled.exe") + " " + quote(directory / "bad_mangled.obj") + " " +
                quote(directory / "Stage48_4HostApi.lib"),
            directory / "bad_mangled_link.log"),
        "omitting generated extern-C declarations makes C++-mangled call fail to link against NativeSharp C ABI");
    const std::string mangledLog = readFile(directory / "bad_mangled_link.log");

    // ESTA COMPROBACION EXIGIA EL TEXTO DE lld-link, Y ESO ERA EL DEFECTO.
    //
    // Pedia que el log contuviera la cadena literal "undefined symbol". Eso es COMO LO DICE
    // lld-link, no lo que tiene que pasar. Medido el 2026-09-02: con NSLink el enlace falla igual
    // --la comprobacion de arriba, que exige el fallo, pasa-- y esta caia solo porque el otro
    // enlazador redacta su diagnostico con otras palabras. Una prueba asi da rojo con cualquier
    // enlazador que no sea aquel contra el que se escribio, y llama defecto suyo a una diferencia
    // de vocabulario.
    //
    // LO QUE SE MIDE AHORA es la propiedad de verdad, y son dos hechos independientes:
    //   1. el enlace FALLA sin las declaraciones extern "C"   -- la comprobacion de arriba;
    //   2. el diagnostico NOMBRA el simbolo que no resolvio    -- esta.
    // El nombre del simbolo en el log es la evidencia de que la causa es el decorado de nombres,
    // y cualquier enlazador que se pueda usar en serio lo nombra. La redaccion no es contrato.
    tests.expect(mangledLog.find("NS484_Add") != std::string::npos,
        "negative linkage test exposes C++ name mangling as the reason the C ABI symbol cannot resolve");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_stage48_4_cpp_host_integration");
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root, cleanupError);

    testGeneratedCppContract(tests, root);
    testSharedLibraryCppHost(tests, root);
    testStaticLibraryCppHost(tests, root);
    testCppLinkageAndTypeGuards(tests, root);

    return tests.resultKeeping(root);
}
