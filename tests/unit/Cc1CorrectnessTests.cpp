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
// IDs 79/81. Cuenta apariciones de una subcadena. Se usa para medir en el IR la propiedad que
// ninguna fixture de ejecucion puede aislar del todo: que el destino de una asignacion compuesta
// se evalua UNA sola vez. Si `buffer[Probe()] += 1` se hubiera desazucarado a
// `buffer[Probe()] = buffer[Probe()] + 1`, en el IR habria DOS llamadas a Probe y no una.
std::size_t countOccurrences(const std::string& text, std::string_view needle)
{
    std::size_t total = 0;
    std::size_t cursor = 0;
    while ((cursor = text.find(needle, cursor)) != std::string::npos)
    {
        ++total;
        cursor += needle.size();
    }
    return total;
}

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

    // Fase 4A -- `sizeof`. Primera fixture que EJECUTA un sizeof en este compilador: antes
    // parseaba, tipaba mal y moria en el `default` del HIR.
    checkFixture(tests, root, "tests/cc1/c9_sizeof/c9_sizeof.nsproj", "cc1-c9-sizeof", 42);

    // Fases 4B, 4C y 4D. Las tres eran la misma forma -- mitad delantera presente, feature
    // ausente -- y las tres atraviesan ahora las cuatro capas.
    checkFixture(
        tests, root, "tests/cc1/c10_stackalloc/c10_stackalloc.nsproj", "cc1-c10-stackalloc", 42);
    checkFixture(tests, root, "tests/cc1/c11_raw_new/c11_raw_new.nsproj", "cc1-c11-raw-new", 42);
    checkFixture(
        tests, root, "tests/cc1/c12_pointer_arith/c12_pointer_arith.nsproj",
        "cc1-c12-pointer-arith", 42);

    // IDs 79, 81, 80 y 83 -- las cuatro formas que quedaban con la mitad delantera del pipeline
    // presente y la trasera ausente. c13 y c14 son la MISMA correccion medida por los dos lados
    // (`x++` y `x += 1` mueren en el mismo sitio del HIR y por el mismo motivo); c15 mide el
    // acceso por `->`, cerrado en el parser convirtiendolo en `(*p).x`.
    checkFixture(tests, root, "tests/cc1/c13_increment/c13_increment.nsproj", "cc1-c13-increment", 42);
    checkFixture(
        tests, root, "tests/cc1/c14_compound_assign/c14_compound_assign.nsproj",
        "cc1-c14-compound-assign", 42);
    checkFixture(
        tests, root, "tests/cc1/c15_arrow_member/c15_arrow_member.nsproj",
        "cc1-c15-arrow-member", 42);

    // ID 78 (P1) -- un `enum` medía 4 según layoutNamedType y 8 según valueLayoutForTypeKey. La
    // fixture no compara contra un número escrito por mí: enfrenta los cuatro consumidores del
    // tamaño (sizeof, offset de campo, paso de puntero, paso de arreglo) entre sí.
    checkFixture(
        tests, root, "tests/cc1/c16_enum_layout/c16_enum_layout.nsproj", "cc1-c16-enum-layout", 42);

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

    // =====================================================================================
    // sizeof -- M-004.
    // =====================================================================================

    // EL REPRO EXACTO del hallazgo. `sizeof` mide el TIPO de su operando; el operando no se
    // ejecuta. Antes el modelo semantico lo analizaba como codigo normal, asi que la asignacion
    // dentro del sizeof marcaba `x` como asignada y el `return x` pasaba -- leyendo una variable
    // a la que nunca se le escribio nada. Ahora el estado de flujo se restaura despues de analizar
    // el operando, asi que `x` sigue sin asignar y el programa se rechaza.
    //
    // Esta es la mitad de ANALISIS del hallazgo; la mitad de EJECUCION (que los efectos escritos
    // dentro del operando no ocurran) la mide la fixture c9_sizeof.
    checkRejected(
        tests, root, "cc1-sizeof-unevaluated",
        R"NS(
class P
{
    public static int Main()
    {
        int x;
        nuint n = sizeof(x = 1);
        return x;
    }
}
)NS",
        // El fragmento nombra LA PROPIEDAD, no una letra. La primera version esperaba solo "x",
        // y con eso el test paso por la razon equivocada: el programa se rechazaba, si, pero por
        // el tipo del inicializador (`int n = sizeof(...)` cuando sizeof da nuint), y "x" aparece
        // tambien en ese mensaje. Un negativo cuyo fragmento casa con cualquier error no esta
        // midiendo nada. Es el mismo fallo que cc1-ternary-ownership-mixed en la fase 3.
        "use of unassigned local variable 'x'");

    // El mismo operando, pero sobre una variable YA asignada: aqui no hay nada que rechazar y lo
    // que importa es que el programa siga siendo valido. Sin este par, "restaurar el estado de
    // flujo" podria haberse implementado como "rechazar todo sizeof con una asignacion dentro".
    checkAccepted(
        tests, root, "cc1-sizeof-unevaluated-assigned",
        R"NS(
class P
{
    public static int Main()
    {
        int x = 0;
        nuint n = sizeof(x = 1);
        // `return n` seria nuint -> int, que es justo la conversion que NO existe y la que hizo
        // fallar la primera version de este caso. Se compara y se devuelve el int.
        if (n != 4) { return 1; }
        return x;
    }
}
)NS");

    // `void` no tiene tamano. Antes esto pasaba el analisis semantico sin una palabra -- el caso
    // ni siquiera miraba su typeSyntax() -- y moria despues en el HIR con "unsupported Stage 34
    // OOP expression kind", que es un mensaje interno, no un diagnostico.
    checkRejected(
        tests, root, "cc1-sizeof-void",
        R"NS(
class P
{
    public static int Main()
    {
        nuint n = sizeof(void);
        return 0;
    }
}
)NS",
        "sizeof(void) has no size");

    // Un tipo que no existe. Mismo caso: antes tipaba como ulong y seguia.
    checkRejected(
        tests, root, "cc1-sizeof-unknown-type",
        R"NS(
class P
{
    public static int Main()
    {
        nuint n = sizeof(NoSuchType);
        return 0;
    }
}
)NS",
        "NoSuchType");

    // =====================================================================================
    // stackalloc y raw new -- M-002, M-005.
    // =====================================================================================

    // Un elemento con un campo `owned` tiene un ciclo de vida que alguien debe cerrar, y ni un
    // buffer de pila ni un bloque crudo del monton lo cierran: no hay quien llame al destructor al
    // salir del ambito. Se prohibe en vez de inventar una semantica de destruccion para el nivel
    // crudo.
    checkRejected(
        tests, root, "cc1-stackalloc-owned-element",
        R"NS(
namespace cc1own
{
    public struct Holder
    {
        public owned string Text;
    }

    class P
    {
        public static int Main()
        {
            Holder* slots = stackalloc Holder[2];
            return 0;
        }
    }
}
)NS",
        "owns a lifetime that raw storage cannot discharge");

    // El mismo elemento, por el monton crudo. Misma razon, distinta puerta -- sin este par, la
    // regla podria haberse escrito para una sola de las dos formas.
    checkRejected(
        tests, root, "cc1-raw-new-owned-element",
        R"NS(
namespace cc1own2
{
    public struct Holder
    {
        public owned string Text;
    }

    class P
    {
        public static int Main()
        {
            Holder* slot = new Holder;
            return 0;
        }
    }
}
)NS",
        "owns a lifetime that raw storage cannot discharge");

    // El contador. La forma dinamica de `new T[n]` ya lo exigia; esta rama no lo hacia, asi que
    // `stackalloc int[unaCadena]` tipaba sin decir una palabra.
    checkRejected(
        tests, root, "cc1-stackalloc-nonintegral-count",
        R"NS(
class P
{
    public static int Main()
    {
        string n = "four";
        int* buffer = stackalloc int[n];
        return 0;
    }
}
)NS",
        "stackalloc requires an integral element count");

    // Sin tamano no hay reserva posible.
    checkRejected(
        tests, root, "cc1-stackalloc-void-element",
        R"NS(
class P
{
    public static int Main()
    {
        void* buffer = stackalloc void[4];
        return 0;
    }
}
)NS",
        "stackalloc requires an element type with a size");

    // =====================================================================================
    // Aritmetica de punteros -- M-006.
    // =====================================================================================

    // Un paso de tamano cero no es un paso.
    checkRejected(
        tests, root, "cc1-pointer-arith-void",
        R"NS(
class P
{
    private static int Walk(void* start)
    {
        void* next = start + 1;
        return 0;
    }

    public static int Main()
    {
        return 0;
    }
}
)NS",
        "'void*' has no element size");

    // Sumar dos direcciones no significa nada. Se rechaza explicitamente en vez de dejar que caiga
    // por el mensaje generico de operandos numericos, que habria sido confuso.
    checkRejected(
        tests, root, "cc1-pointer-arith-add-two-pointers",
        R"NS(
class P
{
    private static int Bad(int* left, int* right)
    {
        int* sum = left + right;
        return 0;
    }

    public static int Main()
    {
        return 0;
    }
}
)NS",
        "two pointers cannot be added");

    // Restarle una direccion a un entero tampoco.
    checkRejected(
        tests, root, "cc1-pointer-arith-integer-minus-pointer",
        R"NS(
class P
{
    private static int Bad(int* pointer)
    {
        int* result = 1 - pointer;
        return 0;
    }

    public static int Main()
    {
        return 0;
    }
}
)NS",
        "an integer minus a pointer is not defined");

    // Y la diferencia entre punteros de tipos distintos: no hay un paso con el que dividir.
    checkRejected(
        tests, root, "cc1-pointer-arith-mixed-difference",
        R"NS(
class P
{
    private static int Bad(int* left, double* right)
    {
        nint distance = left - right;
        return 0;
    }

    public static int Main()
    {
        return 0;
    }
}
)NS",
        "same pointer type");

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

    // =====================================================================================
    // IDs 79 y 81 -- la propiedad que ninguna fixture de ejecucion aisla del todo, medida en el IR.
    //
    // `buffer[Probe()] += 1` tiene que llamar a Probe UNA vez. La fixture c14 lo comprueba con un
    // contador en tiempo de ejecucion, que es la prueba de comportamiento; esto lo comprueba en el
    // IR, que es la prueba estructural: en el modulo emitido hay UNA sola llamada, no dos. Las dos
    // miden lo mismo desde lados distintos, y la del IR es la que sigue valiendo si algun dia el
    // optimizador aprende a fusionar dos llamadas identicas.
    // =====================================================================================
    {
        const std::string text = lowerToLlvmText(
            tests, root, "cc1-compound-single-evaluation",
            R"NS(
namespace cc1single
{
    class P
    {
        static int calls = 0;
        public static int Probe() { calls = calls + 1; return 2; }
        public static int Main()
        {
            int* buffer = stackalloc int[4];
            buffer[Probe()] += 1;
            return buffer[2];
        }
    }
}
)NS");
        tests.expect(
            countOccurrences(text, "call i32 @") >= 1,
            "cc1-compound-single-evaluation: el IR contiene la llamada al indice");
        tests.expect(
            countOccurrences(text, "Probe") == 2,
            "cc1-compound-single-evaluation: Probe aparece exactamente dos veces en el modulo "
            "(su definicion y UNA llamada) -- dos llamadas significarian que el destino se "
            "evaluo dos veces, que es el defecto que este nodo existe para evitar");
    }

    {
        const std::string text = lowerToLlvmText(
            tests, root, "cc1-increment-single-evaluation",
            R"NS(
namespace cc1singleinc
{
    class P
    {
        static int calls = 0;
        public static int Probe() { calls = calls + 1; return 2; }
        public static int Main()
        {
            int* buffer = stackalloc int[4];
            buffer[Probe()]++;
            return buffer[2];
        }
    }
}
)NS");
        tests.expect(
            countOccurrences(text, "Probe") == 2,
            "cc1-increment-single-evaluation: `x++` evalua su destino una sola vez, igual que "
            "`x += 1` -- son el mismo nodo");
    }

    // =====================================================================================
    // IDs 79, 80, 81, 83 -- lo que ANTES se rechazaba y ahora es legal.
    //
    // Estas cuatro son las que habrian pasado en verde antes del cambio si solo se comprobara
    // "falla": no fallaban por una errata, fallaban porque la forma no existia. Ahora miden lo
    // contrario, que es aceptacion.
    // =====================================================================================
    checkAccepted(
        tests, root, "cc1-increment-accepted",
        R"NS(
namespace cc1inc
{
    class P
    {
        public static int Main()
        {
            int i = 0;
            for (int k = 0; k < 3; k++) { i++; }
            --i;
            return i;
        }
    }
}
)NS");

    checkAccepted(
        tests, root, "cc1-compound-accepted",
        R"NS(
namespace cc1cmp
{
    class P
    {
        public static int Main()
        {
            int i = 8;
            i += 2; i -= 1; i *= 3; i /= 9; i %= 2; i &= 1; i |= 4; i ^= 1; i <<= 2; i >>= 1;
            return i;
        }
    }
}
)NS");

    checkAccepted(
        tests, root, "cc1-arrow-field-accepted",
        R"NS(
namespace cc1arrowfield
{
    public struct Cell { public int Value; }
    class P
    {
        public static int Main()
        {
            Cell c;
            Cell* p = &c;
            p->Value = 42;
            return p->Value;
        }
    }
}
)NS");

    checkAccepted(
        tests, root, "cc1-arrow-call-accepted",
        R"NS(
namespace cc1arrowcall
{
    public struct Cell { public int Value; public int Twice() { return Value * 2; } }
    class P
    {
        public static int Main()
        {
            Cell c;
            Cell* p = &c;
            p->Value = 21;
            return p->Twice();
        }
    }
}
)NS");

    // =====================================================================================
    // IDs 79 y 81 -- los limites, con su mensaje.
    //
    // Un test negativo que solo exija "falla" pasaria tambien con una errata en la propia fixture.
    // Cada uno de estos exige EL MENSAJE, y cada mensaje nombra la propiedad que se viola.
    // =====================================================================================
    checkRejected(
        tests, root, "cc1-increment-non-lvalue",
        R"NS(
namespace cc1incbad
{
    class P
    {
        public static int Main()
        {
            int i = 1;
            int j = (i + 1)++;
            return j;
        }
    }
}
)NS",
        "increment/decrement requires a modifiable numeric or non-void pointer operand");

    checkRejected(
        tests, root, "cc1-increment-void-pointer",
        R"NS(
namespace cc1incvoid
{
    class P
    {
        public static int Main()
        {
            int cell = 0;
            void* p = &cell;
            p++;
            return 0;
        }
    }
}
)NS",
        "increment/decrement requires a modifiable numeric or non-void pointer operand");

    // Un puntero avanza y retrocede; multiplicarlo no significa nada. El mensaje lo dice.
    checkRejected(
        tests, root, "cc1-compound-pointer-bad-operator",
        R"NS(
namespace cc1cmpptrbad
{
    class P
    {
        public static int Main()
        {
            int* buffer = stackalloc int[4];
            buffer *= 2;
            return 0;
        }
    }
}
)NS",
        "a non-void pointer supports only '+=' and '-=' with an integral right-hand side");

    // DIVERGENCIA DELIBERADA DE C#, y por eso tiene test propio: C# acepta esto metiendo un cast
    // implicito de vuelta que TRUNCA. Aqui se rechaza y se pide el cast escrito.
    checkRejected(
        tests, root, "cc1-compound-narrowing-rejected",
        R"NS(
namespace cc1cmpnarrow
{
    class P
    {
        public static int Main()
        {
            int i = 1;
            double d = 2.5;
            i += d;
            return i;
        }
    }
}
)NS",
        "compound assignment cannot implicitly convert its right-hand side from");

    // Y la otra mitad de esa divergencia: la conversion CONSTANTE sigue valiendo, asi que la
    // forma normal no pide ceremonia. Si el cierre anterior se hubiera escrito demasiado
    // estrecho, esto dejaria de compilar.
    checkAccepted(
        tests, root, "cc1-compound-constant-narrowing-accepted",
        R"NS(
namespace cc1cmpconst
{
    class P
    {
        public static int Main()
        {
            byte b = 40;
            b += 2;
            short s = -1;
            s -= 1;
            return (int)b;
        }
    }
}
)NS");

    // =====================================================================================
    // IDs 119 y 120 -- los encontró la corrida de las fixtures de arriba, y ninguno es del
    // cambio que las trajo.
    //
    // 119: unario `+` propagaba la constancia del operando y unario `-` no, así que
    //      `short s = 3;` compilaba y `short s = -3;` no -- el mismo literal, con signo.
    // 120: MEDIDO llamando a commonNumericType directamente -- `short` con la constante `3` daba
    //      **double**, porque con una constante que cabe las conversiones existen en las dos
    //      direcciones y se caía al bucle de preferencia, ordenado de más ancho a más estrecho.
    // =====================================================================================
    checkAccepted(
        tests, root, "cc1-negative-constant-narrowing",
        R"NS(
namespace cc1neg
{
    class P
    {
        public static int Main()
        {
            short s = -3;
            short low = -32768;
            int big = -2147483648;
            long huge = -9007199254740993;
            s = -1;
            return (int)s + (int)low - (int)low + big - big + (int)(huge - huge);
        }
    }
}
)NS");

    // La otra mitad: un negativo NO cabe en un destino sin signo. Si el cierre se hubiera escrito
    // demasiado ancho, esto compilaría y truncaría en silencio.
    checkRejected(
        tests, root, "cc1-negative-constant-unsigned-target",
        R"NS(
namespace cc1negbad
{
    class P
    {
        public static int Main()
        {
            byte b = -1;
            return (int)b;
        }
    }
}
)NS",
        "cannot initialize local 'b'");

    checkRejected(
        tests, root, "cc1-negative-constant-out-of-range",
        R"NS(
namespace cc1negrange
{
    class P
    {
        public static int Main()
        {
            short s = -32769;
            return (int)s;
        }
    }
}
)NS",
        "cannot initialize local 's'");

    // ID 120 medido desde el lenguaje: si `short + 3` siguiera dando `double`, esto no compilaría,
    // porque no hay conversión implícita de vuelta de double a short.
    checkAccepted(
        tests, root, "cc1-constant-adapts-to-operand",
        R"NS(
namespace cc1promo
{
    class P
    {
        public static int Main()
        {
            short s = 10;
            short t = s + 3;
            byte b = 5;
            byte c = b + 1;
            short back = s - 3;
            return (int)t + (int)c + (int)back;
        }
    }
}
)NS");

    return tests.result();
}
