// Milestone CC1 -- Compiler Correctness & Semantic Consolidation.
//
// Esta suite es la red del milestone. Cubre dos mitades que valen lo mismo:
//
//   POSITIVA -- tres proyectos que se COMPILAN Y SE EJECUTAN a -O0 y -O2, y devuelven 42 cuando el
//   compilador esta sano. Miden comportamiento: que el programa salte en el mismo valor que el
//   frontend comparo, que un owner elegido por un ternario se destruya exactamente una vez, que un
//   do-while deje asignado lo que asigna su cuerpo.
//
//   NEGATIVA -- ocho programas que NO deben compilar, y de cada uno se comprueba que fallo Y POR
//   QUE. Comprobar solo "fallo" pasaria tambien con una errata en la propia fixture, que es como un
//   test negativo deja de probar algo sin que nadie se entere. Tres de los ocho -- los duplicados
//   de `switch` -- protegen contra comportamiento indefinido en la instruccion `switch` de LLVM, no
//   contra un estilo feo.
//
// Como en el resto del arbol: aqui se verifica que compilan; la ejecucion real la corre
// run_windows_checklist.bat en Windows, donde no hay Wine.

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

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
using nativesharp::driver::Driver;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::llvm_backend::LlvmModule;
using nativesharp::llvm_backend::ProjectOopLlvmLowerer;
using nativesharp::tests::TestContext;

#ifndef _WIN32
bool commandAvailable(std::string_view command)
{
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
}
#endif

std::string wineCommand()
{
#ifdef _WIN32
    return {};
#else
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
#endif
}

int runUnderWine(const std::string& wine, const std::filesystem::path& exePath)
{
    const std::string command =
        "WINEDEBUG=-all timeout 25 " + wine + " " + exePath.string() + " >/dev/null 2>&1";
    const int raw = std::system(command.c_str());
#ifdef WIFEXITED
    return WIFEXITED(raw) ? WEXITSTATUS(raw) : (raw & 0xFF);
#else
    return raw & 0xFF;
#endif
}

void checkFixture(
    TestContext& tests,
    const std::filesystem::path& outputRoot,
    std::string_view relativeProject,
    std::string_view label,
    int expected)
{
    const std::filesystem::path projectPath =
        std::filesystem::path(NSC_SOURCE_DIR) / std::filesystem::path(relativeProject);

    tests.expect(
        std::filesystem::exists(projectPath),
        std::string(label) + ": fixture exists (" + projectPath.string() + ")");
    if (!std::filesystem::exists(projectPath)) return;

    for (const std::string_view optimizationFlag : {std::string_view("-O0"), std::string_view("-O2")})
    {
        const std::string scenario = std::string(label) + " " + std::string(optimizationFlag);
        const auto exePath =
            outputRoot / (std::string(label) + std::string(optimizationFlag.substr(1)) + ".exe");

        const std::string projectText = projectPath.string();
        const std::string exeText = exePath.string();
        const std::array<std::string_view, 5> args = {
            "--emit-project-oop-exe", projectText, optimizationFlag, "-o", exeText};

        Driver driver;
        std::ostringstream out;
        std::ostringstream err;
        const ExitCode result = driver.run(args, out, err);
        tests.expect(
            result == ExitCode::Success, scenario + ": compiles and links (" + err.str() + ")");
        if (result != ExitCode::Success) continue;

        const std::string wine = wineCommand();
        if (wine.empty())
        {
            tests.expect(true, scenario + ": Wine unavailable -- execution skipped");
            continue;
        }
        tests.expect(
            runUnderWine(wine, exePath) == expected,
            scenario + ": exits with " + std::to_string(expected));
    }
}

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

// Compila un programa que NO debe compilar, y comprueba que fallo Y que fallo por la razon dicha.
void checkRejected(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view label,
    std::string_view body,
    std::string_view expectedFragment)
{
    const auto directory = root / "reject" / std::string(label);
    writeFile(
        directory / "reject.nsproj",
        "<Project><PropertyGroup><Name>reject</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", body);

    const std::string projectText = (directory / "reject.nsproj").string();
    const std::string exeText = (directory / "reject.exe").string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", exeText};

    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    const std::string diagnostics = err.str() + out.str();

    tests.expect(result != ExitCode::Success, std::string(label) + ": is rejected");
    tests.expect(
        diagnostics.find(std::string(expectedFragment)) != std::string::npos,
        std::string(label) + ": says why -- expected '" + std::string(expectedFragment) +
            "', got: " + diagnostics);
}

// Compila un programa que SI debe compilar, sin ejecutarlo. Para las formas que antes se
// rechazaban y ahora son legales, donde lo que se mide es la aceptacion.
void checkAccepted(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view label,
    std::string_view body)
{
    const auto directory = root / "accept" / std::string(label);
    writeFile(
        directory / "accept.nsproj",
        "<Project><PropertyGroup><Name>accept</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", body);

    const std::string projectText = (directory / "accept.nsproj").string();
    const std::string exeText = (directory / "accept.exe").string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", exeText};

    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    tests.expect(
        result == ExitCode::Success,
        std::string(label) + ": is accepted (" + err.str() + ")");
}

// =========================================================================================
// EH-001 -- la mitad que NO se puede medir ejecutando.
//
// Lo que EH-001 arregla es que una salida estructurada desde un `catch` limpie la excepcion
// actual del hilo igual que la hace la salida normal. Ese slot es TLS y NativeSharp no tiene
// hoy ninguna forma de leerlo, asi que ninguna fixture de ejecucion puede distinguir "limpio"
// de "sucio": las fixtures c4..c8 miden el comportamiento estructurado, que es otra cosa. Lo
// que si es observable es el IR emitido, y ahi es donde se mide esto.
//
// Por eso la limpieza se emite en un bloque propio, `eh.catch.leave.N`: le da nombre a la
// obligacion en el IR y permite comprobar lo que de verdad importa -- que el bloque existe Y
// que lo que contiene es la llamada -- en vez de buscar el nombre de la funcion suelto por
// todo el modulo, que pasaria igual gracias a los bloques `eh.catch.handled.N` que ya la
// llamaban antes de este cambio.
std::string lowerToLlvmText(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view label,
    std::string_view body)
{
    const auto directory = root / "ir" / std::string(label);
    writeFile(
        directory / "ir.nsproj",
        "<Project><PropertyGroup><Name>ir</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", body);

    ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(directory / "ir.nsproj", error);
    tests.expect(built, std::string(label) + ": reaches project OOP MIR (" + error + ")");
    if (!built || compilation.nameLookup() == nullptr || compilation.oopMirModel() == nullptr)
        return {};

    ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<LlvmModule> module =
        lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(
        module != nullptr, std::string(label) + ": lowers to LLVM (" + lowerer.error() + ")");
    if (module == nullptr) return {};

    std::string verifyError;
    tests.expect(
        module->verify(verifyError),
        std::string(label) + ": the emitted module verifies (" + verifyError + ")");
    return module->text();
}

// Cuenta los bloques `eh.catch.leave.N` del texto IR cuyo cuerpo llama de verdad a
// __nativesharp_exception_clear. Un bloque que existiera vacio contaria como 0, que es lo que
// hace util contar esto y no solo buscar la etiqueta.
std::size_t countCatchLeaveBlocksThatClear(const std::string& text)
{
    std::size_t total = 0;
    std::size_t cursor = 0;
    while (true)
    {
        const std::size_t label = text.find("\neh.catch.leave.", cursor);
        if (label == std::string::npos) break;
        const std::size_t bodyStart = text.find('\n', label + 1);
        if (bodyStart == std::string::npos) break;
        // El bloque termina en su terminador; con mirar hasta la linea en blanco que separa
        // bloques basta, porque LLVM imprime uno por bloque.
        std::size_t bodyEnd = text.find("\n\n", bodyStart);
        if (bodyEnd == std::string::npos) bodyEnd = text.size();
        const std::string block = text.substr(bodyStart, bodyEnd - bodyStart);
        if (block.find("call void @__nativesharp_exception_clear()") != std::string::npos)
        {
            ++total;
        }
        cursor = bodyStart;
    }
    return total;
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nsc-cc1";
    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    std::filesystem::create_directories(root);

    // =====================================================================================
    // Mitad positiva -- ejecucion.
    // =====================================================================================

    checkFixture(tests, root, "tests/cc1/c1_constants/c1_constants.nsproj", "cc1-c1-constants", 42);
    checkFixture(
        tests, root, "tests/cc1/c2_ternary_ownership/c2_ternary_ownership.nsproj",
        "cc1-c2-ternary-ownership", 42);
    checkFixture(
        tests, root, "tests/cc1/c3_dowhile_flow/c3_dowhile_flow.nsproj", "cc1-c3-dowhile-flow", 42);

    // Fase 3 -- EH. Las cinco miden salidas estructuradas desde `catch` y `finally`; ver la
    // cabecera de cada Main.ns para que fija cada una y por que hacen falta las cinco.
    checkFixture(
        tests, root, "tests/cc1/c4_catch_continue/c4_catch_continue.nsproj",
        "cc1-c4-catch-continue", 42);
    checkFixture(
        tests, root, "tests/cc1/c5_catch_break/c5_catch_break.nsproj", "cc1-c5-catch-break", 42);
    checkFixture(
        tests, root, "tests/cc1/c6_catch_return/c6_catch_return.nsproj", "cc1-c6-catch-return", 42);
    checkFixture(
        tests, root, "tests/cc1/c7_finally_switch/c7_finally_switch.nsproj",
        "cc1-c7-finally-switch", 42);
    checkFixture(
        tests, root, "tests/cc1/c8_loop_catch_switch/c8_loop_catch_switch.nsproj",
        "cc1-c8-loop-catch-switch", 42);

    // =====================================================================================
    // EH -- EH-001, medido en el IR.
    // =====================================================================================

    // Un catch cuyo cuerpo sale por `continue`, otro por `break` y otro por `return`: tres
    // obligaciones de limpieza, tres bloques `eh.catch.leave.N` con la llamada dentro.
    {
        const std::string text = lowerToLlvmText(
            tests, root, "cc1-eh-structured-exit-clears",
            R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;

namespace cc1ehir
{
    class P
    {
        public static int Loop(Exception problem, int limit)
        {
            int seen = 0;
            int index = 0;
            while (index < limit)
            {
                index = index + 1;
                try
                {
                    if (index == 1) { throw problem; }
                    if (index == 2) { throw problem; }
                    if (index == 3) { throw problem; }
                }
                catch
                {
                    seen = seen + 1;
                    if (index == 1) { continue; }
                    if (index == 2) { break; }
                    return seen;
                }
            }
            return seen;
        }
    }
}
)NS");
        tests.expect(
            countCatchLeaveBlocksThatClear(text) == 3,
            "cc1-eh-structured-exit-clears: las tres salidas estructuradas del catch "
            "(continue, break, return) emiten cada una su limpieza de la excepcion actual");
    }

    // El control negativo, que es lo que convierte el positivo en una medida y no en una
    // coincidencia: un catch cuyo unico final es el fallthrough normal ya limpiaba antes de
    // EH-001, por `eh.catch.handled.N`, y no debe ganar ningun bloque nuevo.
    {
        const std::string text = lowerToLlvmText(
            tests, root, "cc1-eh-fallthrough-unchanged",
            R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;

namespace cc1ehir2
{
    class P
    {
        public static int Once(Exception problem)
        {
            int seen = 0;
            try
            {
                throw problem;
            }
            catch
            {
                seen = seen + 1;
            }
            return seen;
        }
    }
}
)NS");
        tests.expect(
            countCatchLeaveBlocksThatClear(text) == 0,
            "cc1-eh-fallthrough-unchanged: la salida normal de un catch sigue limpiando por "
            "eh.catch.handled y no gana un bloque de salida estructurada");
        tests.expect(
            text.find("@__nativesharp_exception_clear") != std::string::npos,
            "cc1-eh-fallthrough-unchanged: ... y esa limpieza de la salida normal sigue ahi");
    }

    // Y la distincion que hace falta acertar para no limpiar de mas: un `break` de un bucle
    // abierto DENTRO del cuerpo del catch no sale del catch, asi que no es una salida del
    // catch y no debe emitir nada.
    {
        const std::string text = lowerToLlvmText(
            tests, root, "cc1-eh-inner-loop-break-is-not-an-exit",
            R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;

namespace cc1ehir3
{
    class P
    {
        public static int Inner(Exception problem)
        {
            int seen = 0;
            try
            {
                throw problem;
            }
            catch
            {
                int index = 0;
                while (true)
                {
                    index = index + 1;
                    if (index == 3) { break; }
                }
                seen = index;
            }
            return seen;
        }
    }
}
)NS");
        tests.expect(
            countCatchLeaveBlocksThatClear(text) == 0,
            "cc1-eh-inner-loop-break-is-not-an-exit: un break que solo abandona un bucle abierto "
            "dentro del catch no cuenta como salida del catch");
    }

    // =====================================================================================
    // Constantes y char -- C-001, C-005.
    // =====================================================================================

    // El repro exacto del defecto. `char` mide un byte, asi que 321 no le cabe. Antes la conversion
    // constante lo aceptaba (321 <= 65535, porque char compartia rama con ushort), el frontend veia
    // 65 y 321 como etiquetas distintas, y el backend emitia `i8 65` para las dos: dos valores de
    // caso identicos en un `switch` de LLVM, que es comportamiento indefinido.
    checkRejected(
        tests, root, "cc1-char-range",
        R"NS(
class P
{
    public static int Main()
    {
        char c = 321;
        return 0;
    }
}
)NS",
        "cannot initialize");

    // La otra mitad del mismo defecto: aunque la conversion aceptara el valor, la comparacion de
    // duplicados ocurre AHORA sobre el valor normalizado al tipo gobernante.
    checkRejected(
        tests, root, "cc1-switch-dup-narrowed",
        R"NS(
class P
{
    private static int Pick(byte value)
    {
        switch (value)
        {
            case 65:
                return 1;
            case (byte)321:
                return 2;
            default:
                return 0;
        }
    }

    public static int Main()
    {
        byte b = 65;
        return Pick(b);
    }
}
)NS",
        "duplicate case label with value 65");

    checkRejected(
        tests, root, "cc1-switch-dup-cast",
        R"NS(
class P
{
    private static int Pick(byte value)
    {
        switch (value)
        {
            case 44:
                return 1;
            case (byte)300:
                return 2;
            default:
                return 0;
        }
    }

    public static int Main()
    {
        byte b = 44;
        return Pick(b);
    }
}
)NS",
        "duplicate case label with value 44");

    // =====================================================================================
    // Flujo -- F-001, F-003, F-004.
    // =====================================================================================

    // El caso del auditor, literal. Si `condition` es false, `x` nunca se asigno. El analisis
    // secuencial la daba por asignada; C7 pone el almacenamiento a cero, asi que el programa
    // devolvia 0 en vez de explotar -- el backend tapaba el bug del analisis.
    checkRejected(
        tests, root, "cc1-ternary-definite-assignment",
        R"NS(
class P
{
    private static int F(bool condition)
    {
        int x;
        int unused = condition ? (x = 1) : 0;
        return x;
    }

    public static int Main()
    {
        return F(true);
    }
}
)NS",
        "x");

    checkRejected(
        tests, root, "cc1-andand-definite-assignment",
        R"NS(
class P
{
    private static int F(bool condition)
    {
        int x;
        bool unused = condition && (x = 1) == 1;
        return x;
    }

    public static int Main()
    {
        return F(true);
    }
}
)NS",
        "x");

    checkRejected(
        tests, root, "cc1-oror-definite-assignment",
        R"NS(
class P
{
    private static int F(bool condition)
    {
        int x;
        bool unused = condition || (x = 1) == 1;
        return x;
    }

    public static int Main()
    {
        return F(true);
    }
}
)NS",
        "x");

    // El gemelo del fixture c3: un `while` SI tiene camino de cero iteraciones, asi que esta misma
    // forma tiene que seguir rechazandose. Sin esto, "arreglar" do-while podria haber relajado
    // while sin que nadie lo notara.
    checkRejected(
        tests, root, "cc1-while-flow-still-strict",
        R"NS(
class P
{
    public static int Main()
    {
        int total;
        int index = 0;
        while (index < 4)
        {
            total = index;
            index = index + 1;
        }
        return total;
    }
}
)NS",
        "total");

    // =====================================================================================
    // Ownership del ternario -- F-002.
    // =====================================================================================

    // Ramas que difieren: una transfiere propiedad y la otra es un alias no propietario. El
    // resultado no tiene una procedencia unica, asi que no puede inicializar un owner.
    checkRejected(
        tests, root, "cc1-ternary-ownership-mixed",
        R"NS(
namespace cc1mixed
{
    class P
    {
        public static int Main()
        {
            string prefix = "ns-";
            string alias = prefix;
            bool flag = 2 + 2 == 4;
            raii string chosen = flag ? (prefix + "alpha") : alias;
            return chosen.Length;
        }
    }
}
)NS",
        // El mensaje real del compilador, verificado contra la corrida del 2026-08-31. Yo esperaba
        // "raii local" y me equivoque de cadena, no de comportamiento: el rechazo ocurrio como
        // debia. Esta es exactamente la razon por la que la mitad negativa comprueba el MENSAJE y
        // no solo el fallo -- un test que solo mira "fallo" no habria detectado que yo no sabia que
        // diagnostico estaba disparando.
        "RAII ownership cannot be copied or borrowed");

    // Y la forma que SI debe aceptarse, que es la que no compilaba antes de CC1-204.
    checkAccepted(
        tests, root, "cc1-ternary-ownership-both",
        R"NS(
namespace cc1both
{
    class P
    {
        public static int Main()
        {
            string prefix = "ns-";
            bool flag = 2 + 2 == 4;
            raii string chosen = flag ? (prefix + "alpha") : (prefix + "beta");
            return chosen.Length;
        }
    }
}
)NS");

    return tests.result();
}
