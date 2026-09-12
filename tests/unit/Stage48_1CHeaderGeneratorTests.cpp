#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::kCompilerVersion;
using nativesharp::driver::Driver;
using nativesharp::linker::LldLinker;
using nativesharp::linker::PeExecutableVerifier;
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
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
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
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::string outputText = outputPath.string();
    const std::array<std::string_view, 5> args = {
        "-O2", action, projectText, "-o", outputText};
    return runDriver(args, output, error);
}

ExitCode emitHeader(
    const std::filesystem::path& project,
    const std::filesystem::path& outputPath,
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::string outputText = outputPath.string();
    const std::array<std::string_view, 4> args = {
        "--emit-project-host-header", projectText, "-o", outputText};
    return runDriver(args, output, error);
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

const char* completeHostApiSource()
{
    return R"NS(
namespace Stage48_1.Host
{
    extern C delegate int Transform(int value);

    public enum Mode
    {
        First = 1,
        Second = 2
    }

    // Enums CON tipo subyacente declarado. Existen aqui porque la cara de C del contrato es este
    // fichero: si el typedef generado dijera `int32_t` para un enum de un byte, el lado de C leeria
    // cuatro bytes de un campo de uno.
    public enum Narrow : byte
    {
        Low = 0,
        High = 200
    }

    public enum Wide : long
    {
        Zero = 0,
        Huge = 5000000000
    }

    public struct Outer
    {
        public Inner Child;
        public int Tail;
    }

    public struct Inner
    {
        public short Small;
        public ushort UnsignedSmall;
    }

    public struct Node
    {
        public Node* Next;
        public int Value;
    }

    public class Counter
    {
        public int Value;
        public Counter(int value) { Value = value; }
    }

    public interface IMarker
    {
    }

    public class Api
    {
        public static export extern C int Add(int left, int right) { return left + right; }
        public static export extern C bool EchoBool(bool value) { return value; }
        public static export extern C byte EchoByte(byte value) { return value; }
        public static export extern C char EchoChar(char value) { return value; }
        public static export extern C short EchoShort(short value) { return value; }
        public static export extern C ushort EchoUShort(ushort value) { return value; }
        public static export extern C uint EchoUInt(uint value) { return value; }
        public static export extern C long EchoLong(long value) { return value; }
        public static export extern C ulong EchoULong(ulong value) { return value; }
        public static export extern C nint EchoNInt(nint value) { return value; }
        public static export extern C nuint EchoNUInt(nuint value) { return value; }
        public static export extern C float EchoFloat(float value) { return value; }
        public static export extern C double EchoDouble(double value) { return value; }
        public static export extern C void* EchoPointer(void* value) { return value; }
        public static export extern C Mode EchoMode(Mode value) { return value; }
        public static export extern C Narrow EchoNarrow(Narrow value) { return value; }
        public static export extern C Wide EchoWide(Wide value) { return value; }
        public static export extern C Inner EchoInner(Inner value) { return value; }
        public static export extern C Outer EchoOuter(Outer value) { return value; }
        public static export extern C Node* EchoNodePointer(Node* value) { return value; }
        public static export extern C Counter EchoCounter(Counter value) { return value; }
        public static export extern C IMarker EchoMarker(IMarker value) { return value; }
        public static export extern C int Invoke(Transform callback, int value) { return callback(value); }
        public static int InternalOnly(int value) { return value + 1; }
    }
}
)NS";
}

void testAutomaticDllHeader(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "AutomaticDll";
    writeProject(directory, "HostApi", "SharedLibrary", completeHostApiSource());

    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            directory / "HostApi.nsproj",
            directory / "HostApi.dll",
            output,
            error) == ExitCode::Success,
        "Stage 48.1 emits a DLL and its generated C/C++ header as one product operation");
    tests.expect(error.empty(), "valid Stage 48.1 DLL header generation keeps stderr empty");
    tests.expect(std::filesystem::is_regular_file(directory / "HostApi.h"),
        "DLL product automatically includes HostApi.h");
    tests.expect(output.find("host-header:") != std::string::npos,
        "DLL emission reports the generated host-header path");
    tests.expect(std::filesystem::is_regular_file(directory / "HostApi.nsmeta"),
        "Stage 48.2 composes metadata with the Stage 48.1 DLL/header product");

    const std::string header = readFile(directory / "HostApi.h");
    tests.expect(header.find(std::string{"Generated by nsc "} + std::string{kCompilerVersion}) != std::string::npos,
        "generated header records the current compiler identity while preserving Stage 48.1 syntax");
    tests.expect(header.find("#ifndef NATIVESHARP_HOSTAPI_HOST_H") != std::string::npos,
        "generated header has a deterministic project-specific include guard");
    tests.expect(header.find("#include <stddef.h>") != std::string::npos &&
            header.find("#include <stdint.h>") != std::string::npos,
        "generated header is self-contained for standard fixed-width ABI types");
    tests.expect(header.find("#error \"This NativeSharp 1.0.1 host contract targets Windows x64 only.\"") != std::string::npos,
        "generated header states the Stage 48.1 Windows x64 platform contract");
    tests.expect(header.find("extern \"C\"") != std::string::npos,
        "generated header is directly consumable by C++ without name mangling");
    tests.expect(header.find("#define NS_API __declspec(dllimport)") != std::string::npos,
        "SharedLibrary header defaults to dllimport for host consumers");
    tests.expect(header.find("No hidden marshaling, retention or GC") != std::string::npos,
        "generated header documents the unmanaged ownership boundary");

    tests.expect(header.find("typedef struct NS_Counter NS_Counter;") != std::string::npos &&
            header.find("NS_Counter* NS_CALL EchoCounter(NS_Counter* value);") != std::string::npos,
        "classes become opaque pointer-sized C handles");
    tests.expect(header.find("typedef struct NS_IMarker NS_IMarker;") != std::string::npos &&
            header.find("NS_IMarker* NS_CALL EchoMarker(NS_IMarker* value);") != std::string::npos,
        "interfaces become opaque pointer-sized C handles");
    tests.expect(header.find("typedef int32_t NS_Mode;") != std::string::npos,
        "an enum with no declared underlying type keeps the int32 host carrier");
    // EL TYPEDEF LLEVA EL TIPO SUBYACENTE. Es la cara de C del contrato del enum: un `enum : byte`
    // declarado aqui como `int32_t` haria que el consumidor de C leyera cuatro bytes de un campo de
    // uno y desplazara todo lo que fuera detras dentro de una struct -- el mismo defecto que el
    // tipo subyacente vino a cerrar, pero en la direccion que el propio compilador genera.
    tests.expect(header.find("typedef uint8_t NS_Narrow;") != std::string::npos,
        "`enum Narrow : byte` becomes uint8_t in the generated C header, not int32_t");
    tests.expect(header.find("typedef int64_t NS_Wide;") != std::string::npos,
        "`enum Wide : long` becomes int64_t in the generated C header");
    tests.expect(header.find("typedef int32_t (NS_CALL *NS_Transform)(int32_t value);") != std::string::npos,
        "extern C delegates become native C function-pointer typedefs");
    tests.expect(header.find("NS_API int32_t NS_CALL Add(int32_t left, int32_t right);") != std::string::npos,
        "export extern C methods become exact unmangled C prototypes");
    tests.expect(header.find("InternalOnly") == std::string::npos,
        "ordinary NativeSharp methods stay outside the generated host surface");

    tests.expect(header.find("uint8_t NS_CALL EchoBool(uint8_t value)") != std::string::npos &&
            header.find("uint8_t NS_CALL EchoByte(uint8_t value)") != std::string::npos &&
            header.find("uint8_t NS_CALL EchoChar(uint8_t value)") != std::string::npos,
        "bool, byte and char use explicit one-byte ABI carriers");
    tests.expect(header.find("int16_t NS_CALL EchoShort(int16_t value)") != std::string::npos &&
            header.find("uint16_t NS_CALL EchoUShort(uint16_t value)") != std::string::npos &&
            header.find("uint32_t NS_CALL EchoUInt(uint32_t value)") != std::string::npos,
        "16/32-bit integral host mappings are explicit");
    tests.expect(header.find("int64_t NS_CALL EchoLong(int64_t value)") != std::string::npos &&
            header.find("uint64_t NS_CALL EchoULong(uint64_t value)") != std::string::npos &&
            header.find("intptr_t NS_CALL EchoNInt(intptr_t value)") != std::string::npos &&
            header.find("uintptr_t NS_CALL EchoNUInt(uintptr_t value)") != std::string::npos,
        "64-bit and pointer-sized integer host mappings are explicit");
    tests.expect(header.find("float NS_CALL EchoFloat(float value)") != std::string::npos &&
            header.find("double NS_CALL EchoDouble(double value)") != std::string::npos &&
            header.find("void* NS_CALL EchoPointer(void* value)") != std::string::npos,
        "floating-point and raw pointer mappings are native C types");

    const std::size_t innerDefinition = header.find("struct NS_Inner\n{");
    const std::size_t outerDefinition = header.find("struct NS_Outer\n{");
    tests.expect(innerDefinition != std::string::npos && outerDefinition != std::string::npos &&
            innerDefinition < outerDefinition,
        "by-value struct dependencies are topologically ordered even when source declarations are reversed");
    tests.expect(header.find("typedef struct NS_Node NS_Node;") != std::string::npos &&
            header.find("NS_Node* Next;") != std::string::npos,
        "self-referential pointer structs use C forward declarations correctly");
    tests.expect(header.find("NS_STATIC_ASSERT(sizeof(NS_Inner) == 4") != std::string::npos &&
            header.find("NS_STATIC_ASSERT(NS_ALIGNOF(NS_Inner) == 2") != std::string::npos &&
            header.find("offsetof(NS_Inner, UnsignedSmall) == 2") != std::string::npos,
        "POD struct size, alignment and field offsets are guarded at C/C++ compile time");
}

void testExplicitDeterministicHeader(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "Determinism";
    writeProject(directory, "Determinism", "SharedLibrary", completeHostApiSource());

    std::string output;
    std::string error;
    const std::filesystem::path first = directory / "generated" / "first.h";
    const std::filesystem::path second = directory / "generated" / "second.h";
    tests.expect(emitHeader(directory / "Determinism.nsproj", first, output, error) == ExitCode::Success,
        "Stage 48.1 can emit a host header explicitly without linking a binary");
    tests.expect(error.empty() && std::filesystem::is_regular_file(first),
        "explicit header command creates missing parent directories and keeps stderr empty");
    output.clear();
    error.clear();
    tests.expect(emitHeader(directory / "Determinism.nsproj", second, output, error) == ExitCode::Success,
        "Stage 48.1 can regenerate the same host contract independently");
    tests.expect(readFile(first) == readFile(second),
        "C header generation is byte-for-byte deterministic");
}

void testStaticLibraryHeader(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "StaticLibrary";
    writeProject(directory, "StaticApi", "StaticLibrary", completeHostApiSource());

    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-lib",
            directory / "StaticApi.nsproj",
            directory / "StaticApi.lib",
            output,
            error) == ExitCode::Success,
        "Stage 48.1 automatically emits a host header beside a static NativeSharp library");
    const std::string header = readFile(directory / "StaticApi.h");
    tests.expect(header.find("#define NS_API\n") != std::string::npos &&
            header.find("__declspec(dllimport)") == std::string::npos,
        "StaticLibrary host headers contain no DLL import decoration");
    tests.expect(std::filesystem::is_regular_file(directory / "StaticApi.nsmeta"),
        "Stage 48.2 composes metadata with the Stage 48.1 static-library/header product");

    const std::filesystem::path emptyDirectory = root / "StaticNoExports";
    writeProject(emptyDirectory, "InternalMath", "StaticLibrary", R"NS(
public class Math
{
    public static int Add(int left, int right) { return left + right; }
}
)NS");
    output.clear();
    error.clear();
    tests.expect(emitProduct(
            "--emit-project-oop-lib",
            emptyDirectory / "InternalMath.nsproj",
            emptyDirectory / "InternalMath.lib",
            output,
            error) == ExitCode::Success,
        "NativeSharp-only static libraries remain legal in Stage 48.1");
    tests.expect(readFile(emptyDirectory / "InternalMath.h").find("no export extern C host functions") != std::string::npos,
        "a static library without host exports gets an explicit empty host header");
}

void testGeneratedHeaderCompilesAndLinks(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "NativeHosts";
    writeProject(directory, "NativeHosts", "SharedLibrary", R"NS(
namespace Stage48_1.Hosts
{
    extern C delegate int Callback(int value);
    public struct Pair { public int X; public int Y; }
    public class Api
    {
        public static export extern C int Add(int a, int b) { return a + b; }
        public static export extern C Pair Echo(Pair value) { return value; }
        public static export extern C int Invoke(Callback cb, int value) { return cb(value); }
    }
}
)NS");

    std::string output;
    std::string error;
    tests.expect(emitProduct(
            "--emit-project-oop-dll",
            directory / "NativeHosts.nsproj",
            directory / "NativeHosts.dll",
            output,
            error) == ExitCode::Success,
        "generated-header host certification builds the NativeSharp DLL");

    writeFile(directory / "host.c", R"C(
#include "NativeHosts.h"
static int32_t Twice(int32_t value) { return value * 2; }
__declspec(dllexport) int Stage48_1CHostProbe(void)
{
    NS_Pair pair = {20, 22};
    NS_Pair echoed = Echo(pair);
    return Add(20, 22) == 42 && echoed.X == 20 && echoed.Y == 22 && Invoke(Twice, 21) == 42 ? 0 : 1;
}
)C");
    writeFile(directory / "host.cpp", R"CPP(
#include "NativeHosts.h"
static int32_t Triple(int32_t value) { return value * 3; }
extern "C" __declspec(dllexport) int Stage48_1CppHostProbe(void)
{
    NS_Pair pair{30, 12};
    const NS_Pair echoed = Echo(pair);
    return Add(30, 12) == 42 && echoed.X == 30 && echoed.Y == 12 && Invoke(Triple, 14) == 42 ? 0 : 1;
}
)CPP");

    tests.expect(runCommand(
            "clang --target=x86_64-pc-windows-msvc -fms-extensions -fdeclspec -std=c11 -c -I" +
                quote(directory) + " " + quote(directory / "host.c") + " -o " + quote(directory / "host_c.obj"),
            directory / "clang_c.log"),
        "Clang compiles a Windows C11 host using only the generated header");
    tests.expect(runCommand(
            "clang++ --target=x86_64-pc-windows-msvc -fms-extensions -fdeclspec -std=c++20 -fno-exceptions -fno-rtti -c -I" +
                quote(directory) + " " + quote(directory / "host.cpp") + " -o " + quote(directory / "host_cpp.obj"),
            directory / "clang_cpp.log"),
        "Clang compiles a Windows C++20 host using only the generated header");

    std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
    tests.expect(linker != nullptr, "Stage 48.1 host certification discovers lld-link");
    if (linker == nullptr) return;
    const std::string linkerPath = quote(linker->executablePath());
    tests.expect(runCommand(
            linkerPath + " /dll /noentry /nodefaultlib /machine:x64 /out:" +
                quote(directory / "CHost.dll") + " /export:Stage48_1CHostProbe " +
                quote(directory / "host_c.obj") + " " + quote(directory / "NativeHosts.lib"),
            directory / "link_c.log"),
        "C host links against the NativeSharp import library using generated declarations");
    tests.expect(runCommand(
            linkerPath + " /dll /noentry /nodefaultlib /machine:x64 /out:" +
                quote(directory / "CppHost.dll") + " /export:Stage48_1CppHostProbe " +
                quote(directory / "host_cpp.obj") + " " + quote(directory / "NativeHosts.lib"),
            directory / "link_cpp.log"),
        "C++ host links against the same NativeSharp C ABI import library");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verifyDynamicLibrary(directory / "CHost.dll") && verifier.info().machine == 0x8664,
        "generated-header C host is a valid AMD64 PE32+ DLL");
    tests.expect(verifier.verifyDynamicLibrary(directory / "CppHost.dll") && verifier.info().machine == 0x8664,
        "generated-header C++ host is a valid AMD64 PE32+ DLL");
}

void testTypeNameCollisions(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "Collisions";
    writeProject(directory, "Collisions", "StaticLibrary", R"NS(
namespace First { public struct Pair { public int X; } }
namespace Second { public struct Pair { public int Y; } }
public class Api
{
    public static export extern C First.Pair EchoFirst(First.Pair value) { return value; }
    public static export extern C Second.Pair EchoSecond(Second.Pair value) { return value; }
}
)NS");
    std::string output;
    std::string error;
    tests.expect(emitHeader(directory / "Collisions.nsproj", directory / "Collisions.h", output, error) == ExitCode::Success,
        "Stage 48.1 generates collision-free C names for same-named NativeSharp types in different namespaces");
    const std::string header = readFile(directory / "Collisions.h");
    // NSC-LIMIT-009. La forma calificada es ahora una codificacion REVERSIBLE de la identidad:
    // alfanumerico tal cual, `_` como `__`, y cualquier otro byte como `_HH`. `::` es `_3A_3A` y
    // `.` es `_2E`. Antes se aplastaba todo a `_`, lo que hacia indistinguibles dos identidades
    // legales y dejaba el desempate --y por tanto el header-- a merced del orden de declaracion.
    tests.expect(header.find("NS_Collisions_3A_3AFirst_2EPair") != std::string::npos &&
            header.find("NS_Collisions_3A_3ASecond_2EPair") != std::string::npos,
        "colliding simple type names are deterministically qualified in generated C identifiers");
}

// NSC-LIMIT-009. EL PAR QUE EL SANEADOR ANTERIOR APLASTABA AL MISMO NOMBRE.
//
// La primera version de esta prueba usaba `Ns.Inner` y `Ns_Inner`, y NO probaba nada: sus nombres
// simples son distintos --`Inner` y `Ns_Inner`--, asi que los dos tomaban la forma corta y el
// codificador completo no llegaba a llamarse. El saneador viejo habria pasado esa prueba.
//
// Estos dos se llaman los DOS `Pair`, asi que ninguno puede usar la forma corta y los dos pasan
// por la codificacion. Y sus identidades son exactamente el par que colapsaba:
//
//     Col::A.B+Pair    ->  Col__A_B_Pair     (aplastando todo a `_`)
//     Col::A_B.Pair    ->  Col__A_B_Pair     el mismo
//
// El primero ejercita ademas el `+` real de los tipos anidados, no un separador inventado.
void testQualifiedNamesAreInjective(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "Injective";
    writeProject(directory, "Injective", "StaticLibrary", R"NS(
namespace A { public class B { public struct Pair { public int X; } } }
namespace A_B { public struct Pair { public int Y; } }
public class Api
{
    public static export extern C A.B.Pair EchoNested(A.B.Pair value) { return value; }
    public static export extern C A_B.Pair EchoFlat(A_B.Pair value) { return value; }
}
)NS");
    std::string output;
    std::string error;
    tests.expect(
        emitHeader(directory / "Injective.nsproj", directory / "Injective.h", output, error) ==
            ExitCode::Success,
        "a nested type and a namespace type whose sanitized names once collided both emit");
    const std::string header = readFile(directory / "Injective.h");
    // `+` es `_2B`, `.` es `_2E` y el `_` escrito por el usuario es `__`. Las tres reglas juntas
    // son lo que separa este par; con cualquiera de las tres a medias volveria a colapsar.
    tests.expect(header.find("NS_Injective_3A_3AA_2EB_2BPair") != std::string::npos,
        "the nested Pair encodes its '+' distinctly");
    tests.expect(header.find("NS_Injective_3A_3AA__B_2EPair") != std::string::npos,
        "the namespace Pair encodes its underscore distinctly");
    // Y NINGUNO lleva sufijo de desempate: no hubo choque que desempatar, que es la diferencia
    // entre una codificacion inyectiva y un parche sobre las consecuencias.
    tests.expect(header.find("Pair_2") == std::string::npos,
        "no tie-break suffix: the two identities never asked for the same C name");
}

void testHeaderGenerationGuards(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path executable = root / "Executable";
    writeProject(executable, "App", "Exe", "class Program { public static int Main() { return 0; } }");
    std::string output;
    std::string error;
    tests.expect(emitHeader(executable / "App.nsproj", executable / "App.h", output, error) == ExitCode::CompilationError,
        "Stage 48.1 explicit host-header generation rejects executable projects");
    tests.expect(error.find("require OutputType Library") != std::string::npos,
        "executable-project diagnostic states the library requirement");

    const std::filesystem::path unsupported = root / "UnsupportedNestedField";
    writeProject(unsupported, "Unsupported", "StaticLibrary", R"NS(
public struct Bad { public string Text; }
public class Api { public static export extern C Bad Echo(Bad value) { return value; } }
)NS");
    output.clear();
    error.clear();
    tests.expect(emitHeader(unsupported / "Unsupported.nsproj", unsupported / "Unsupported.h", output, error) == ExitCode::CompilationError,
        "Stage 48.1 rejects a host struct whose nested field cannot be represented in C");
    tests.expect(error.find("cannot express NativeSharp ABI type 'builtin:string'") != std::string::npos,
        "unsupported nested host-field diagnostic names the exact semantic ABI type");
    tests.expect(!std::filesystem::exists(unsupported / "Unsupported.h"),
        "failed explicit header generation does not leave a partial header");

    output.clear();
    error.clear();
    tests.expect(emitProduct(
            "--emit-project-oop-lib",
            unsupported / "Unsupported.nsproj",
            unsupported / "Unsupported.lib",
            output,
            error) == ExitCode::CompilationError,
        "automatic Stage 48.1 sidecar failure makes the static-library product fail atomically");
    tests.expect(error.find("static-library C header generation failed") != std::string::npos,
        "automatic sidecar failure reports that the binary failed because its host contract could not be generated");
    tests.expect(!std::filesystem::exists(unsupported / "Unsupported.lib") &&
            !std::filesystem::exists(unsupported / "Unsupported.h"),
        "failed automatic header generation removes the just-built binary and leaves no stale header");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage48_1_c_header_generator";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root, cleanupError);

    testAutomaticDllHeader(tests, root);
    testExplicitDeterministicHeader(tests, root);
    testStaticLibraryHeader(tests, root);
    testGeneratedHeaderCompilesAndLinks(tests, root);
    testTypeNameCollisions(tests, root);
    testQualifiedNamesAreInjective(tests, root);
    testHeaderGenerationGuards(tests, root);

    return tests.resultKeeping(root);
}
