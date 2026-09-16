#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "nativesharp/Linker/WindowsRuntimeImports.h"
#include "nativesharp/Linker/WindowsRuntimeLibrary.h"

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

#ifndef NSC_TEST_CLANG_EXECUTABLE
#error "CMake must provide the configured Clang C host compiler"
#endif

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::linker::LldLinker;
using nativesharp::linker::PeExecutableVerifier;
using nativesharp::linker::configuredWindowsRuntimeLibrary;
using nativesharp::linker::createWindowsImportLibraries;
using nativesharp::linker::verifyWindowsRuntimeLibrary;
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
        directory / "Stage48_3HostApi.nsproj",
        "<Project><PropertyGroup><Name>Stage48_3HostApi</Name><OutputType>" +
            std::string(outputType) +
            "</OutputType></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
            "</ItemGroup></Project>");
    writeFile(directory / "Library.ns", source);
}

const char* apiSource()
{
    return R"NS(
namespace NativeSharp.Stage48_3
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

    public struct FixedBlock
    {
        public int[4] Values;
    }

    public class Counter
    {
        public int Value;
        public Counter(int value) { Value = value; }
    }

    public class HostApi
    {
        private static IntCallback StoredCallback;

        public static export extern C int NS48_Add(int left, int right) { return left + right; }
        public static export extern C int* NS48_EchoIntPointer(int* values) { return values; }
        public static export extern C int NS48_InvokeBuffer(BufferCallback callback, int* values, int count)
        {
            return callback(values, count);
        }
        public static export extern C void* NS48_EchoPointer(void* value) { return value; }
        public static export extern C Pair NS48_EchoPair(Pair value) { return value; }
        public static export extern C HostMode NS48_EchoMode(HostMode value) { return value; }
        public static export extern C LayoutProbe NS48_EchoLayout(LayoutProbe value) { return value; }
        public static export extern C FixedBlock NS48_EchoFixed(FixedBlock value) { return value; }
        public static export extern C void NS48_WriteAnswer(int* output) { output[0] = 42; }
        public static export extern C void NS48_WritePair(Pair* output, int left, int right)
        {
            output[0].Left = left;
            output[0].Right = right;
        }
        public static export extern C int NS48_Utf8Checksum(byte* data, int count)
        {
            if (count != 3) { return 0; }
            return data[0] + data[1] + data[2];
        }
        public static export extern C int NS48_Invoke(IntCallback callback, int value) { return callback(value); }
        public static export extern C void NS48_SetCallback(IntCallback callback) { StoredCallback = callback; }
        public static export extern C int NS48_CallCallback(int value) { return StoredCallback(value); }
        public static export extern C Counter NS48_Counter_Create(int value) { return new Counter(value); }
        public static export extern C int NS48_Counter_Get(Counter counter) { return counter.Value; }
        public static export extern C void NS48_Counter_Add(Counter counter, int delta)
        {
            counter.Value = counter.Value + delta;
        }
        public static export extern C void NS48_Counter_Destroy(Counter counter) { delete counter; }
    }
}
)NS";
}

const char* cHostSource()
{
    return R"C(
#include "Stage48_3HostApi.h"

static int32_t Twice(int32_t value)
{
    return value * 2;
}

static int32_t SumBuffer(int32_t* values, int32_t count)
{
    int32_t sum = 0;
    int32_t i;
    for (i = 0; i < count; ++i)
    {
        sum += values[i];
    }
    return sum;
}

int main(void)
{
    int32_t values[4] = {1, 2, 3, 4};
    NS_Pair pair = {20, 22};
    NS_LayoutProbe layout = {1, 42};
    NS_FixedBlock fixed = {{10, 11, 12, 9}};
    uint8_t utf8[3] = {78, 83, 67};
    int32_t output = 0;
    NS_Counter* counter;
    int marker = 7;

    if (NS48_Add(20, 22) != 42) return 1;
    if (NS48_EchoIntPointer(values) != values) return 2;
    if (NS48_InvokeBuffer(SumBuffer, values, 4) != 10) return 3;
    if (NS48_EchoPointer(&marker) != &marker) return 4;
    pair = NS48_EchoPair(pair);
    if (pair.Left != 20 || pair.Right != 22) return 5;
    if (NS48_EchoMode((NS_HostMode)2) != (NS_HostMode)2) return 6;
    layout = NS48_EchoLayout(layout);
    if (layout.Tag != 1 || layout.Value != 42) return 7;
    fixed = NS48_EchoFixed(fixed);
    if (fixed.Values[0] + fixed.Values[1] + fixed.Values[2] + fixed.Values[3] != 42) return 12;
    NS48_WriteAnswer(&output);
    if (output != 42) return 13;
    NS48_WritePair(&pair, 21, 21);
    if (pair.Left + pair.Right != 42) return 14;
    if (NS48_Utf8Checksum(utf8, 3) != 228) return 15;
    if (NS48_Invoke(Twice, 21) != 42) return 8;
    NS48_SetCallback(Twice);
    if (NS48_CallCallback(21) != 42) return 9;

    counter = NS48_Counter_Create(40);
    if (counter == NULL) return 10;
    NS48_Counter_Add(counter, 2);
    if (NS48_Counter_Get(counter) != 42)
    {
        NS48_Counter_Destroy(counter);
        return 11;
    }
    NS48_Counter_Destroy(counter);
    return 0;
}
)C";
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

// Materializa la superficie de imports del runtime para enlazar un host contra la biblioteca
// ESTATICA. Desde el item 401 la biblioteca de usuario no incrusta el runtime: el host enlaza
// explicitamente NativeSharp.Runtime.Win32.lib y necesita la lista entera de imports Win32 de ese
// runtime. (En la variante DLL no hace falta: la DLL ya resolvio su soporte al enlazarse.)
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

std::string clangCCommand(
    const std::filesystem::path& includeDirectory,
    const std::filesystem::path& source,
    const std::filesystem::path& object)
{
    return quote(NSC_TEST_CLANG_EXECUTABLE) +
        " --target=x86_64-pc-windows-msvc -fms-extensions -fdeclspec "
        "-ffreestanding -fno-stack-protector -std=c11 -Wall -Wextra -Werror -Wpedantic -c -I" +
        quote(includeDirectory) + " " + quote(source) + " -o " + quote(object);
}

void testGeneratedContract(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path shared = root / "Contract" / "Shared";
    writeProject(shared, "SharedLibrary", apiSource());

    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            shared / "Stage48_3HostApi.nsproj",
            shared / "Stage48_3HostApi.dll",
            output,
            error) == ExitCode::Success,
        "Stage 48.3 builds the C-host DLL product");
    tests.expect(error.empty(), "valid Stage 48.3 C-host product keeps stderr empty");
    tests.expect(std::filesystem::is_regular_file(shared / "Stage48_3HostApi.h") &&
            std::filesystem::is_regular_file(shared / "Stage48_3HostApi.lib") &&
            std::filesystem::is_regular_file(shared / "Stage48_3HostApi.nsmeta"),
        "C host product carries generated header, import library and NativeSharp metadata sidecars");

    const std::string header = readFile(shared / "Stage48_3HostApi.h");
    tests.expect(header.find("typedef int32_t (NS_CALL *NS_BufferCallback)(int32_t* values, int32_t count);") !=
            std::string::npos,
        "generated C contract expresses pointer+count buffer callbacks without marshaling");
    tests.expect(header.find("NS_API int32_t* NS_CALL NS48_EchoIntPointer(int32_t* values);") !=
            std::string::npos,
        "generated C contract exposes typed raw pointers directly");
    tests.expect(header.find("typedef struct NS_Counter NS_Counter;") != std::string::npos &&
            header.find("NS_API NS_Counter* NS_CALL NS48_Counter_Create") != std::string::npos &&
            header.find("NS_API void NS_CALL NS48_Counter_Destroy(NS_Counter* counter)") != std::string::npos,
        "C ownership pattern uses an opaque handle plus explicit create/destroy API");
    tests.expect(header.find("NS_STATIC_ASSERT(sizeof(NS_LayoutProbe) == 8") != std::string::npos &&
            header.find("offsetof(NS_LayoutProbe, Value) == 4") != std::string::npos,
        "C host header carries executable layout guards for ABI-visible POD structs");
    tests.expect(header.find("int32_t Values[4]") != std::string::npos &&
            header.find("NS_API void NS_CALL NS48_WriteAnswer(int32_t* output)") != std::string::npos &&
            header.find("NS_API int32_t NS_CALL NS48_Utf8Checksum(uint8_t* data, int32_t count)") !=
                std::string::npos,
        "generated C contract expresses fixed arrays, output pointers, and UTF-8 pointer/count buffers");
    tests.expect(header.find("No hidden marshaling, retention or GC occurs at this boundary") != std::string::npos,
        "generated C contract states the zero-hidden-marshaling boundary rule");
}

void testSharedLibraryCHost(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "SharedHost";
    writeProject(directory, "SharedLibrary", apiSource());
    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            directory / "Stage48_3HostApi.nsproj",
            directory / "Stage48_3HostApi.dll",
            output,
            error) == ExitCode::Success,
        "Stage 48.3 emits the NativeSharp DLL used by the C host");

    writeFile(directory / "host.c", cHostSource());
    tests.expect(runCommand(
            clangCCommand(directory, directory / "host.c", directory / "host.obj"),
            directory / "clang_host.log"),
        "Clang C11 compiles a strict Windows x64 host using only the generated header");

    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    tests.expect(linker != nullptr, "Stage 48.3 discovers lld-link for the C host certification");
    if (linker == nullptr) return;

    tests.expect(runCommand(
            quote(linker->executablePath()) +
                " /entry:main /subsystem:console /nodefaultlib /machine:x64 /out:" +
                quote(directory / "host_shared.exe") + " " + quote(directory / "host.obj") + " " +
                quote(directory / "Stage48_3HostApi.lib"),
            directory / "link_host.log"),
        "C11 host links into a native executable through the NativeSharp DLL import library");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verify(directory / "host_shared.exe"),
        "shared-library C host is a structurally valid PE32+ executable");
    if (!verifier.verify(directory / "host_shared.exe")) return;
    const auto& info = verifier.info();
    tests.expect(info.machine == 0x8664 && info.optionalHeaderMagic == 0x020b && info.entryPointRva != 0,
        "shared C host is AMD64 PE32+ with a concrete entry point");
    tests.expect(info.subsystem == 3 && info.clrDirectoryRva == 0,
        "shared C host is a native Windows console image with no CLR header");
    tests.expect(contains(info.importedLibraries, "Stage48_3HostApi.dll"),
        "shared C host imports the NativeSharp DLL by product name");

    const std::array<std::string_view, 18> expectedSymbols = {
        "NS48_Add", "NS48_EchoIntPointer", "NS48_InvokeBuffer", "NS48_EchoPointer",
        "NS48_EchoPair", "NS48_EchoMode", "NS48_EchoLayout", "NS48_Invoke",
        "NS48_SetCallback", "NS48_CallCallback", "NS48_Counter_Create", "NS48_Counter_Get",
        "NS48_Counter_Add", "NS48_Counter_Destroy", "NS48_EchoFixed", "NS48_WriteAnswer",
        "NS48_WritePair", "NS48_Utf8Checksum"};
    for (const std::string_view symbol : expectedSymbols)
    {
        tests.expect(importsSymbol(info, "Stage48_3HostApi.dll", symbol),
            std::string("shared C host imports exact C ABI symbol '") + std::string(symbol) + "'");
    }
}

void testStaticLibraryCHost(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "StaticHost";
    writeProject(directory, "StaticLibrary", apiSource());
    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-lib",
            directory / "Stage48_3HostApi.nsproj",
            directory / "Stage48_3HostApi.lib",
            output,
            error) == ExitCode::Success,
        "Stage 48.3 emits the NativeSharp static library used by the C host");

    const std::string header = readFile(directory / "Stage48_3HostApi.h");
    tests.expect(header.find("#define NS_API\n") != std::string::npos &&
            header.find("__declspec(dllimport)") == std::string::npos,
        "static C host contract has no DLL import decoration");

    writeFile(directory / "host.c", cHostSource());
    tests.expect(runCommand(
            clangCCommand(directory, directory / "host.c", directory / "host.obj"),
            directory / "clang_host.log"),
        "Clang C11 compiles the same host source against the generated static-library header");

    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    tests.expect(linker != nullptr, "Stage 48.3 discovers lld-link for static C host certification");
    if (linker == nullptr) return;

    std::vector<std::filesystem::path> imports;
    tests.expect(createRuntimeImports(*linker, directory, imports, error),
        "Stage 48.3 materializes the Windows runtime import surface for static host linkage");
    if (imports.size() != 4) return;

    const std::filesystem::path runtimeLibrary = configuredWindowsRuntimeLibrary();
    const bool runtimeValid = verifyWindowsRuntimeLibrary(runtimeLibrary, error);
    tests.expect(runtimeValid,
        "static C host consumes the verified independent Windows runtime library");
    if (!runtimeValid) return;
    const std::string dependencies = " " + quote(directory / "Stage48_3HostApi.lib") + " " +
        quote(runtimeLibrary) + " " +
        quote(imports[0]) + " " + quote(imports[1]) + " " + quote(imports[2]) + " " + quote(imports[3]);
    tests.expect(runCommand(
            quote(linker->executablePath()) +
                " /entry:main /subsystem:console /nodefaultlib /machine:x64 /out:" +
                quote(directory / "host_static.exe") + " " + quote(directory / "host.obj") + dependencies,
            directory / "link_host.log"),
        "C11 host links directly against the NativeSharp static library and runtime imports");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verify(directory / "host_static.exe"),
        "static-library C host is a structurally valid PE32+ executable");
    if (!verifier.verify(directory / "host_static.exe")) return;
    const auto& info = verifier.info();
    tests.expect(info.machine == 0x8664 && info.optionalHeaderMagic == 0x020b && info.entryPointRva != 0,
        "static C host is AMD64 PE32+ with a concrete entry point");
    tests.expect(info.subsystem == 3 && info.clrDirectoryRva == 0,
        "static C host remains a native Windows console image with no CLR header");
    tests.expect(!contains(info.importedLibraries, "Stage48_3HostApi.dll"),
        "static C host has no dependency on the NativeSharp DLL product");
    tests.expect(contains(info.importedLibraries, "KERNEL32.dll") &&
            contains(info.importedLibraries, "KERNELBASE.dll"),
        "static C host imports only the native Windows runtime dependencies it actually needs");
    tests.expect(info.exceptionDirectoryRva != 0,
        "static C host retains NativeSharp Win64 unwind metadata after direct library linkage");
}

void testCAbiMisuseGuards(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "Misuse";
    writeProject(directory, "SharedLibrary", apiSource());
    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            directory / "Stage48_3HostApi.nsproj",
            directory / "Stage48_3HostApi.dll",
            output,
            error) == ExitCode::Success,
        "Stage 48.3 creates the generated contract used by negative C ABI tests");

    writeFile(directory / "bad_pack.c", R"C(
#pragma pack(push, 1)
#include "Stage48_3HostApi.h"
#pragma pack(pop)
int main(void) { return 0; }
)C");
    tests.expect(!runCommand(
            clangCCommand(directory, directory / "bad_pack.c", directory / "bad_pack.obj"),
            directory / "bad_pack.log"),
        "generated POD assertions reject a C host that changes ABI packing");
    const std::string packLog = readFile(directory / "bad_pack.log");
    tests.expect(packLog.find("static assertion failed") != std::string::npos ||
            packLog.find("Static_assert") != std::string::npos,
        "packing misuse fails specifically through the generated compile-time layout contract");

    writeFile(directory / "bad_opaque.c", R"C(
#include "Stage48_3HostApi.h"
int bad(NS_Counter* counter) { return counter->Value; }
)C");
    tests.expect(!runCommand(
            clangCCommand(directory, directory / "bad_opaque.c", directory / "bad_opaque.obj"),
            directory / "bad_opaque.log"),
        "C host cannot dereference a NativeSharp opaque class handle");
    const std::string opaqueLog = readFile(directory / "bad_opaque.log");
    tests.expect(opaqueLog.find("incomplete") != std::string::npos,
        "opaque-handle misuse is rejected because the generated C type is intentionally incomplete");

    writeFile(directory / "bad_callback.c", R"C(
#include "Stage48_3HostApi.h"
static int32_t Wrong(int32_t* value) { return value == 0 ? 0 : *value; }
void bad(void) { NS48_SetCallback(Wrong); }
)C");
    tests.expect(!runCommand(
            clangCCommand(directory, directory / "bad_callback.c", directory / "bad_callback.obj"),
            directory / "bad_callback.log"),
        "strict C11 host build rejects a callback whose function-pointer signature differs from the generated typedef");
    const std::string callbackLog = readFile(directory / "bad_callback.log");
    tests.expect(callbackLog.find("incompatible function pointer") != std::string::npos ||
            callbackLog.find("incompatible-pointer-types") != std::string::npos,
        "callback misuse diagnostic exposes the exact C function-pointer mismatch");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_stage48_3_c_host_integration");
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root, cleanupError);

    testGeneratedContract(tests, root);
    testSharedLibraryCHost(tests, root);
    testStaticLibraryCHost(tests, root);
    testCAbiMisuseGuards(tests, root);

    return tests.resultKeeping(root);
}
