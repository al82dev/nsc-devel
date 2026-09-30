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
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectIntegerConstant;
using nativesharp::frontend::ProjectTypeConversion;
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

// EL SELECTOR DE FIXTURES, y por que este ejecutable tiene que leer argv.
//
// CMake registra UNA prueba de ctest por fixture --`nsc.cc1.correctness.cc1-c30-contratos` y
// compania-- y le pasa la etiqueta como argumento. Si el ejecutable ignorara ese argumento,
// pasarian tres cosas, y las tres son peores que la lentitud que el reparto venia a arreglar:
//
//   1. CADA UNA DE LAS ~40 ENTRADAS CORRERIA LA SUITE ENTERA. El reparto no ahorraria nada;
//      multiplicaria el trabajo por cuarenta.
//   2. TODAS COMPARTIRIAN EL MISMO DIRECTORIO TEMPORAL, y cada una lo borra al arrancar. Una
//      instancia borra los .exe que otra esta enlazando: fallos erraticos que no corresponden a
//      ningun defecto del compilador. Un rojo que no se reproduce es peor que no medir.
//   3. UN ROJO SEGUIRIA SIN DECIR CUAL. Ese era el segundo motivo del reparto, y no el de la
//      velocidad.
//
// Por eso el directorio de salida lleva el selector en el nombre: dos entradas concurrentes NO
// pueden pisarse aunque el argumento se pierda por otro camino.
std::string gSelectedFixture;
bool gRestMode = false;
// ¿llego a coincidir el selector con alguna fixture? Si no, la corrida no midio NADA y tiene que
// decirlo: un selector con una errata daria "PASS: 0 checks", que ctest cuenta como verde. Una
// prueba que pasa sin comprobar nada es la unica forma de rojo que no se ve.
bool gSelectedFixtureMatched = false;

// ¿toca ejecutar esta fixture en esta invocacion? Sin argumento se ejecutan todas --que es como
// se corre a mano-- y en modo `--rest` ninguna, porque esa entrada mide lo que NO es fixture.
bool fixtureSelected(std::string_view label)
{
    if (gRestMode) return false;
    if (gSelectedFixture.empty()) return true;
    if (gSelectedFixture != label) return false;
    gSelectedFixtureMatched = true;
    return true;
}

// ¿toca ejecutar lo que no es fixture --los rechazos y las comprobaciones sobre el IR? Solo en
// `--rest`, o cuando no se pidio nada en concreto.
bool restSelected()
{
    return gRestMode || gSelectedFixture.empty();
}

void checkFixture(
    TestContext& tests,
    const std::filesystem::path& outputRoot,
    std::string_view relativeProject,
    std::string_view label,
    int expected)
{
    if (!fixtureSelected(label)) return;
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
    std::string_view expectedFragment,
    std::string_view languageEdition = "2")
{
    const auto directory = root / "reject" / std::string(label);
    writeFile(
        directory / "reject.nsproj",
        "<Project><PropertyGroup><Name>reject</Name><Language>" +
        std::string(languageEdition) + "</Language></PropertyGroup>"
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
// Recorta UNA funcion del texto de un modulo. Las aserciones sobre IR que cuentan ocurrencias
// tienen que contar dentro de la funcion que miden: el modulo trae la biblioteca de runtime
// entera, y una cuenta hecha sobre todo el texto mide a los vecinos -- y se rompe el dia que un
// vecino cambia, con un rojo que no habla del contrato que la prueba defiende.
std::string sliceFunction(const std::string& text, std::string_view nameFragment)
{
    std::size_t search = 0;
    while (true)
    {
        const std::size_t define = text.find("\ndefine ", search);
        if (define == std::string::npos) return {};
        const std::size_t lineEnd = text.find('\n', define + 1);
        if (lineEnd == std::string::npos) return {};
        const std::string header = text.substr(define + 1, lineEnd - define - 1);
        // El final de una funcion en el texto de LLVM es una llave en la columna cero.
        const std::size_t close = text.find("\n}", lineEnd);
        if (close == std::string::npos) return {};
        if (header.find(nameFragment) != std::string::npos)
        {
            return text.substr(define + 1, close - define + 1);
        }
        search = close + 1;
    }
}

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

int main(int argc, char** argv)
{
    // argv[1] es `--rest` o una etiqueta de fixture. Sin argumento se corre todo, que es lo que
    // hace falta al invocar el ejecutable a mano.
    if (argc > 1 && argv[1] != nullptr)
    {
        const std::string argument = argv[1];
        if (argument == "--rest")
        {
            gRestMode = true;
        }
        else
        {
            // LA FORMA SE VALIDA ANTES DE TOCAR EL DISCO, y ese orden es el punto.
            //
            // El selector acaba dentro de una ruta a la que se le hace `remove_all`. Sin validarlo
            // primero, un argumento como `../../algo` sale de %TEMP% y BORRA otra cosa -- y la
            // comprobacion de que el selector nombra una fixture registrada, que existe mas abajo,
            // llegaria demasiado tarde: el borrado ya habria ocurrido.
            //
            // Una etiqueta es `cc1-` y despues minusculas, digitos y guiones. Nada mas: ni puntos,
            // ni separadores, ni mayusculas.
            const bool wellFormed =
                argument.size() > 4 && argument.compare(0, 4, "cc1-") == 0 &&
                std::all_of(argument.begin() + 4, argument.end(), [](unsigned char c) {
                    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
                });
            if (!wellFormed)
            {
                std::cerr << "FAILED: '" << argument
                          << "' is not a fixture label (expected cc1-<lowercase-digits-dashes>)\n";
                return 1;
            }
            gSelectedFixture = argument;
        }
    }

    TestContext tests;
    // EL DIRECTORIO LLEVA EL SELECTOR EN EL NOMBRE. Es lo que evita que dos entradas de ctest en
    // paralelo se borren los ficheros la una a la otra: cada una tiene el suyo, y el `remove_all`
    // de abajo solo toca el propio.
    const std::string slot = gRestMode ? std::string("rest")
        : (gSelectedFixture.empty() ? std::string("all") : gSelectedFixture);
    const auto root = std::filesystem::temp_directory_path() / ("nsc-cc1-" + slot);
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

    // IDs 121 y 122 -- la aritmetica de constantes enteras. La fixture prueba dos cosas a la vez:
    // que los campos `const` de anchos mezclados COMPILAN (un inicializador que no se pliega no es
    // constante y el archivo no compilaria) y que el valor plegado es el correcto en ejecucion.
    checkFixture(
        tests, root, "tests/cc1/c17_constant_promotion/c17_constant_promotion.nsproj",
        "cc1-c17-constant-promotion", 42);

    // ID 123 -- endurecimiento de `stackalloc`. LA MITAD POSITIVA: que el endurecimiento no tenga
    // falsos positivos. Las reservas imposibles no caben en una fixture que tiene que compilar
    // entera; se miden con checkRejected mas abajo.
    checkFixture(
        tests, root, "tests/cc1/c18_stackalloc_hardening/c18_stackalloc_hardening.nsproj",
        "cc1-c18-stackalloc-hardening", 42);

    // ID 89 -- `sbyte`. Enfrenta a `sbyte` con `byte` en cada operacion donde el SIGNO cambia el
    // resultado y no solo su lectura: extension, division, modulo, shift a la derecha,
    // comparacion y el cast que estrecha.
    checkFixture(tests, root, "tests/cc1/c19_sbyte/c19_sbyte.nsproj", "cc1-c19-sbyte", 42);

    // ID 82 -- asignacion compuesta sobre propiedad e indexador. La fixture no comprueba solo el
    // valor final: CUENTA las llamadas al getter y al setter, porque un desazucarado que evaluara
    // el destino dos veces daria el mismo valor en casi todos los casos y solo se delataria en el
    // conteo. La mitad NEGATIVA --receptores e indices que no se pueden reevaluar-- esta abajo,
    // con checkRejected: una fixture tiene que compilar entera.
    checkFixture(
        tests, root, "tests/cc1/c20_property_compound/c20_property_compound.nsproj",
        "cc1-c20-property-compound", 42);

    // SHR-024 -- parsing numerico. El PRIMER item del Bloque 2, y el que lo bloquea entero: sin
    // esto no hay lexer. La fixture no comprueba "que parsea": comprueba la POSICION --varios
    // numeros leidos de una sola cadena sin cortar subcadenas, que es lo que hace un lexer--, los
    // limites EXACTOS por los dos lados, la asimetria del minimo con signo, y que lo invalido se
    // rechaza. Ver mkdown\90_shr_024_parsing_numerico.md.
    checkFixture(
        tests, root, "tests/cc1/c21_number_parsing/c21_number_parsing.nsproj",
        "cc1-c21-number-parsing", 42);

    // SHR-022 + SHR-023 -- consola con stderr, y formateo. LA FIXTURE DE ACEPTACION DEL BLOQUE 2:
    // lee 0x1F, 42UL y 3.14f de UNA cadena y escribe un diagnostico formateado por stderr, que es
    // el criterio del bloque palabra por palabra. Mide ademas cuatro cosas que no se ven mirando
    // la salida: que stdout y stderr son canales distintos (la mascara de dos bits llega entera
    // hasta el runtime), que `Write` NO salta de linea, que `String.ToString(v, base)` y
    // `Number.TryParseUInt64Radix` son inversas exactas para las catorce bases, y que
    // `ToString(v, 16)` (signo+magnitud) y `ToHexString(v)` (patron de bits) NO son lo mismo.
    //
    // Clava tambien el desbordamiento que la medicion contra glibc encontro: 1e-300 exige escalar
    // por 10^310, que no cabe en un double; con un solo factor el producto es infinito y el
    // numero mas pequeno del rango se imprimia como 9.22e-299.
    // Ver mkdown\91_shr_022_consola_dos_canales.md y mkdown\92_shr_023_formateo_medido.md.
    checkFixture(
        tests, root, "tests/cc1/c22_console_and_formatting/c22_console_and_formatting.nsproj",
        "cc1-c22-console-and-formatting", 42);

    // SHR-010 + SHR-011/012 + SHR-017 -- LA FIXTURE DE ACEPTACION DEL BLOQUE 3, y su primera
    // mitad: "el lexer guarda lexemas como vistas sin copiar". La segunda mitad del criterio --
    // "y el compilador rechaza devolver una vista de un local"-- NO cabe aqui, porque un programa
    // rechazado no produce ejecutable ni codigo de salida; vive en
    // tests/unit/Stage58ViewLifetimesTests.cpp, que exige el diagnostico exacto de cada forma
    // prohibida.
    //
    // Lo que mide y no se ve en la salida: que las vistas NO copian -- cada lexema se compara
    // contra String.CharacterPointer(fuente, desplazamiento), asi que una copia caeria fuera del
    // buffer original y la fixture lo veria; que escribir por un Span<T> escribe el arreglo de
    // verdad; que CopyToOverlapping va hacia atras cuando destino y origen se solapan; que
    // Optional<int> distingue "no hay" de un -1 que SI esta en los datos; y que los indexadores
    // comprueban el rango con una excepcion catchable.
    // Ver mkdown\94_bloque3_vistas_y_tiempo_de_vida.md.
    checkFixture(
        tests, root, "tests/cc1/c23_views_and_lifetimes/c23_views_and_lifetimes.nsproj",
        "cc1-c23-views-and-lifetimes", 42);

    // Bloque 4, fase A -- E/S binaria de archivos. Las cinco operaciones nuevas se enfrentan ENTRE
    // SI: se escribe, se pregunta el tamano, se lee de las dos formas, y las tres respuestas
    // tienen que coincidir. Cubre ademas los tres casos que separan esta familia del resto --
    // que GetSize devuelve -1 y no 0 cuando el archivo no esta (para poder distinguirlo de uno
    // VACIO, que tambien se prueba), que ReadBytes con un buffer corto lee el principio y lo dice
    // en vez de fallar, y que WriteAllBytes recorta el contador a la longitud real del arreglo.
    // Ver mkdown\100_bloque4_fase_a_io_binaria.md.
    checkFixture(
        tests, root, "tests/cc1/c24_binary_file_io/c24_binary_file_io.nsproj",
        "cc1-c24-binary-file-io", 42);

    // Bloque 4, fase B -- ByteBuffer, BinaryWriter y BinaryReader, IDA Y VUELTA. Se construye una
    // cabecera con la forma de la de COFF, se vuelca a disco con las operaciones de la fase A, se
    // relee y se exige que TODOS los campos coincidan. La propiedad medida es que escribir y leer
    // son inversas: una prueba contra bytes literales pasaria igual si los dos estuvieran mal del
    // mismo modo. Cubre ademas el maximo de 64 bits y un negativo --donde el signo se rompe--, que
    // leer de mas devuelve None SIN mover la posicion, y que ByteBuffer no pierde nada al crecer.
    // Ver mkdown\101_bloque4_fases_b_y_c.md.
    checkFixture(
        tests, root, "tests/cc1/c25_binary_roundtrip/c25_binary_roundtrip.nsproj",
        "cc1-c25-binary-roundtrip", 42);

    // Bloque 4 -- EL CRITERIO DE CIERRE DEL BLOQUE, escrito en el doc 69 antes de empezarlo: "un
    // programa NativeSharp encuentra fuentes, las lee, escribe un binario y llama al enlazador".
    // c24 y c25 miden las fases por separado; este mide que COMPONEN: prepara un arbol de
    // fuentes, las descubre con Directory.GetFiles, las lee, emite un objeto binario con
    // ByteBuffer/BinaryWriter, lo relee con File.ReadBytes/BinaryReader campo por campo, y
    // lanza dos procesos hijos con Process.Run -- uno es el propio ejecutable (codigo de salida
    // y salida de texto comprobados) y el otro es el enlazador de verdad.
    //
    // OJO CON LO QUE MIDE ESTA PRUEBA Y LO QUE NO. Aqui solo se COMPILA Y ENLAZA: checkFixture
    // ejecuta bajo Wine, que en Windows no esta, asi que la ejecucion se salta. Quien de verdad
    // corre c26 --y por tanto quien mide Process.Run-- es run_cases.py, en los 90 trabajos.
    // Ver mkdown\105_bloque4_cierre_driver_minimo.md.
    checkFixture(
        tests, root, "tests/cc1/c26_driver_minimo/c26_driver_minimo.nsproj",
        "cc1-c26-driver-minimo", 42);

    // Bloque 5, fase A -- Algorithm (SHR-052): las NUEVE funciones que `src/` usa de verdad, no
    // las dieciocho de <algorithm>. Cada una con sus casos de borde (rango vacio, un elemento,
    // todos iguales, ya ordenado, orden inverso), y el heapsort comprobado ELEMENTO A ELEMENTO en
    // cinco formas de entrada -- un "esta ordenado" pasaria igual con un sort que perdiera o
    // duplicara elementos. Cubre ademas que List<T>.Sort ya delega aqui en vez de tener su propia
    // insertion sort. Ver mkdown\109_bloque5_algoritmos_texto_memoria.md.
    checkFixture(
        tests, root, "tests/cc1/c27_algoritmos/c27_algoritmos.nsproj",
        "cc1-c27-algoritmos", 42);

    // Bloque 5, fases B y C -- StringBuilder (SHR-019) y TextWriter (SHR-020). De StringBuilder,
    // que al duplicar la capacidad no se pierda nada y que ToOwnedString devuelva el tamano EXACTO
    // y no la capacidad. De TextWriter, la propiedad que justifica su diseno: el MISMO productor
    // de texto vuelca a memoria y a fichero, y las dos salidas se comparan byte a byte.
    checkFixture(
        tests, root, "tests/cc1/c28_texto/c28_texto.nsproj",
        "cc1-c28-texto", 42);

    // Bloque 5, fase D -- ArenaAllocator (SHR-039). La comprobacion central no es que encadene
    // bloques: es que los punteros REPARTIDOS ANTES de encadenar siguen siendo validos despues.
    // Se guardan cuarenta, se fuerzan varios bloques nuevos, y se releen todos al final. Si la
    // arena realojara y copiara --como hacen List<T>, ByteBuffer o StringBuilder-- se veria ahi y
    // en ningun otro sitio.
    checkFixture(
        tests, root, "tests/cc1/c29_arena/c29_arena.nsproj",
        "cc1-c29-arena", 42);

    // Fase M1 -- LO QUE LOS CONTRATOS AFIRMAN, medido. Los docs 110 (overflow) y 111 (memoria)
    // escriben lo que el lenguaje promete; esta fixture comprueba que el compilador lo cumple, y
    // sobre todo que lo cumple IGUAL en -O0 y en -O2.
    //
    // Su paso 6 es el que ninguna otra prueba del arbol hacia: la MISMA expresion evaluada por el
    // plegado de constantes en compilacion y por el binario en ejecucion, ocho veces. Si las dos
    // vias discrepan hay dos juegos de reglas de aritmetica -- el fallo que la unificacion de
    // include/nativesharp/Frontend/BuiltinIntegerTypes.h existe para impedir.
    checkFixture(
        tests, root, "tests/cc1/c30_contratos/c30_contratos.nsproj",
        "cc1-c30-contratos", 42);

    // CString -- `string` a cadena C terminada en NUL. La pieza que faltaba para que un programa
    // NativeSharp pueda llamar a la API `A` de Win32 sin escribir la conversion a mano, y la
    // escribio a mano el primer programa real del proyecto (doc 112 §C.3).
    //
    // Su comprobacion util es la que le pasa la cadena a `lstrlenA` de kernel32: quien decide si
    // el NUL esta bien puesto es EL SISTEMA OPERATIVO, no este programa contandose los bytes.
    checkFixture(
        tests, root, "tests/cc1/c31_cadena_c/c31_cadena_c.nsproj",
        "cc1-c31-cadena-c", 42);

    // LA DIVISION POR CERO. No devuelve 42 y no puede: es una TRAMPA, y el runtime sale con
    // ExitProcess(128 + razon). DivisionByZero es la razon 1, asi que el exito es SALIR CON 129.
    // Es el caso negativo que el doc 110 §2.5 dejo anotado y que ninguna otra fixture podia
    // cubrir, porque todas las demas certifican devolviendo 42.
    checkFixture(
        tests, root, "tests/cc1/c32_division_por_cero/c32_division_por_cero.nsproj",
        "cc1-c32-division-por-cero", 129);

    // Thread.Join como punto acquire (doc 111 §2.3). Un modelo de memoria NO se verifica con
    // fixtures --lo dice el propio documento-- pero si el contrato es falso esto falla A VECES, y
    // "a veces" ya es informacion que hoy no existe. Ocho hilos, dieciseis ranuras cada uno,
    // treinta rondas.
    checkFixture(
        tests, root, "tests/cc1/c33_hilos_join/c33_hilos_join.nsproj",
        "cc1-c33-hilos-join", 42);

    // ID 129 -- `ToString()` de un enum da el NOMBRE del enumerador. Es del compilador y no de
    // String.ns porque los nombres NO sobreviven a la compilacion: un enum es un entero con
    // nombres en el codigo fuente y nada mas en el binario.
    //
    // Mide el alias (dos enumeradores con el mismo valor: gana el primero declarado), el valor
    // fuera de rango (devuelve el nombre del TIPO, no una cadena vacia) y, sobre todo, un valor
    // que el compilador NO puede plegar -- ahi la cadena de condicionales se recorre de verdad.
    checkFixture(
        tests, root, "tests/cc1/c34_enum_nombre/c34_enum_nombre.nsproj",
        "cc1-c34-enum-nombre", 42);

    // ID 132 -- dos metodos genericos con el mismo nombre y distinta lista de parametros. La
    // identidad de una plantilla generica era nombre + aridad de TIPOS, asi que `Cuenta<T>(T[])` y
    // `Cuenta<T>(T[], int, int)` eran el mismo simbolo y chocaban. El rodeo era renombrar, y esta
    // escrito en dos sitios de la biblioteca (Spans.OfRange, Algorithm.SortWith).
    //
    // El consumidor de verdad esta al final de la fixture: Algorithm.Sort con y sin comparador.
    checkFixture(
        tests, root, "tests/cc1/c35_sobrecarga_generica/c35_sobrecarga_generica.nsproj",
        "cc1-c35-sobrecarga-generica", 42);

    // Utf8 -- la otra mitad de la decision de que `string` son BYTES (mkdown/114, ID 125). Sin
    // esto, "bytes opacos" seria un callejon sin salida: no habria forma de contar caracteres,
    // recorrerlos ni saber si una cadena que viene de fuera esta bien formada.
    //
    // Las secuencias esperadas salen del codificador UTF-8 de Python --un tercero-- y no de mi
    // cabeza. Y la mitad que mas protege es la de lo INVALIDO: una sobrelarga C0 AF decodifica a
    // '/' en un decodificador flojo, y por ahi se han saltado comprobaciones de rutas durante
    // veinte anos.
    checkFixture(
        tests, root, "tests/cc1/c36_utf8/c36_utf8.nsproj",
        "cc1-c36-utf8", 42);

    // `size(N)` -- el contrato de layout, la mitad positiva. Once structs cuyos tamaños salieron
    // de compilar las EQUIVALENTES EN C con clang y preguntarle sizeof/_Alignof: ninguna
    // verificación cuenta si el modelo contra el que verifico lo construí yo, y aquí el modelo es
    // un compilador de C, que es contra quien estas structs tienen que coincidir de verdad.
    //
    // La fixture mide las DOS mitades: que el modelo de layout coincida con C (eso lo comprueba
    // el propio `size(N)` al compilar) y que lo que el backend materializa sea ese mismo modelo
    // (eso lo comprueban `sizeof` y la lectura por punteros en ejecución).
    checkFixture(
        tests, root, "tests/cc1/c37_contrato_de_layout/c37_contrato_de_layout.nsproj",
        "cc1-c37-contrato-de-layout", 42);

    // `enum E : byte` -- el TIPO SUBYACENTE de un enum. Hasta el 2026-09-03 todo enum media cuatro
    // bytes y se trataba como con signo, en cuatro sitios que decian lo mismo por separado, asi
    // que traducir un header de C con `enum : uint8_t` dentro de una struct daba un layout
    // SILENCIOSAMENTE equivocado. Eso no es falta de expresividad: es una respuesta falsa.
    //
    // La fixture mide las cuatro respuestas a la vez --tamano, tipo LLVM, signo de las extensiones
    // y signo de las constantes de `case`-- con valores elegidos para que discrepar se note: 200
    // en un `byte` extendido con signo daria -56, y -3 en un `sbyte` extendido sin signo daria
    // 253. Los tamanos de las structs los verifica `size(N)` contra lo que clang da para las
    // equivalentes en C.
    checkFixture(
        tests, root, "tests/cc1/c38_enum_ancho/c38_enum_ancho.nsproj",
        "cc1-c38-enum-ancho", 42);

    // `ToString()` sobre enums estrechos -- LA FRONTERA DE LA REGRESION b9.
    //
    // El 2026-09-03, c38 devolvio 42 a -O0 y 39 a -O2 al pedir el nombre de un `enum : sbyte` con
    // valor NEGATIVO. La corrida de triage de las 18:29 demostro despues que basta un receptor
    // CONSTANTE: no hace falta ninguna funcion de por medio, y queda descartada la ABI del retorno
    // de ocho bits. b9 lo fija promoviendo el patron estrecho a i64 antes del switch de EnumName;
    // ese caso esta aislado en tests/bug_triage/b9_enum_estrecho_o2 y NO se repite aqui.
    //
    // Esta fixture mide sus vecinos inmediatos --los otros tres anchos, el mismo `sbyte` con
    // valores positivos, y el receptor constante, por local, por funcion y no plegable-- que hoy
    // responden bien. Va ordenada por capas igual que b9: el valor, despues el contenido de la
    // cadena byte a byte, y al final el operador de comparacion.
    checkFixture(
        tests, root, "tests/cc1/c39_enum_nombre_o2/c39_enum_nombre_o2.nsproj",
        "cc1-c39-enum-nombre-o2", 42);

    // NSC-LIMIT-005. LA FIXTURE DIFERENCIAL. c38, c39 y b9 comprueban que el resultado es EL
    // CORRECTO; esta comprueba que LAS DOS VIAS COINCIDEN -- la constante, que el optimizador
    // pliega, y la opaca, que no puede plegar -- para los cuatro anchos estrechos con los valores
    // que delatan el signo.
    //
    // El defecto de -O2 no era un valor equivocado en todas partes: era que el optimizador decidia
    // distinto que el codigo sin optimizar. Comparar las dos vias lo detecta caiga la que caiga, y
    // DENTRO de una sola corrida: no depende de que alguien compare la pasada de -O0 con la de
    // -O2.
    //
    // MEDIDO que muerde: con `zext` cambiado a `sext` en `lowerEnumName`, el IR a -O2 pasa de
    // llamar a `MismosBytes` con el mismo global en los dos argumentos a llamarlo con globales
    // distintos en las tres comprobaciones de enums CON SIGNO -- codigo 20, 22 o 26 en vez de 42 --
    // y deja intactas las de los enums sin signo.
    checkFixture(
        tests, root, "tests/cc1/c47_diferencial_o0_o2/c47_diferencial_o0_o2.nsproj",
        "cc1-c47-diferencial-o0-o2", 42);

    // NSC-LIMIT-011. El `where T : class, IComponent` se declara en una biblioteca, mientras
    // `Payload` existe solamente en el proyecto consumidor. Tambien fuerza una especializacion
    // anidada Reader<T> dentro de Envelope<T>, para que el origen se propague y no se vuelva a
    // derivar desde el namespace de la biblioteca.
    checkFixture(
        tests, root, "tests/cc1/c48_constraints_cross_project/c48_constraints_cross_project.nsproj",
        "cc1-c48-constraints-cross-project", 42);

    // NSC-LIMIT-021. Fresh owned strings are materialized as hidden full-expression owners when
    // lent to an explicit borrowed parameter or to the audited non-retaining Core surface. Two
    // simultaneous temporaries pin reverse cleanup; String.Copy and Console pin both lowering
    // paths that originally forced Saru to introduce named `raii` workarounds.
    checkFixture(
        tests, root, "tests/cc1/c49_temporary_string_borrow/c49_temporary_string_borrow.nsproj",
        "cc1-c49-temporary-string-borrow", 42);

    // NSC-LIMIT-017. Count inference, trailing comma, left-to-right single evaluation and value
    // conversion/storage for primitive and struct elements.
    checkFixture(
        tests, root, "tests/cc1/c50_inferred_array_initializer/c50_inferred_array_initializer.nsproj",
        "cc1-c50-inferred-array-initializer", 42);

    // NSC-LIMIT-020 + C#-compatible named arguments. Literal defaults fill omitted parameters,
    // names select declaration ordinals, and evaluation remains left-to-right even when ABI
    // parameter order differs. The fixture covers static calls and constructors at O0 and O2.
    checkFixture(
        tests, root, "tests/cc1/c51_default_parameters/c51_default_parameters.nsproj",
        "cc1-c51-default-parameters", 42);

    // NSC-LIMIT-018. Unused members of a closed generic type do not impose operations on T;
    // List<Pair>.Add/indexer compile although Pair deliberately has no natural ordering.
    checkFixture(
        tests, root, "tests/cc1/c52_lazy_generic_members/c52_lazy_generic_members.nsproj",
        "cc1-c52-lazy-generic-members", 42);

    // 1.0.1 -- `const string` is a real compile-time constant, not a hidden cctor assignment.
    // The fixture reads a literal, an alias and the empty string at O0 and O2. This is the source
    // shape emitted by Silk.NET's three StringConstants files.
    checkFixture(
        tests, root, "tests/cc1/c53_const_string/c53_const_string.nsproj",
        "cc1-c53-const-string", 42);

    // NSC-SARU-002/003. A get-only auto-property is initialized through its generated backing
    // field only inside the declaring constructor, and literal zero converts to an enum for
    // equality/inequality. These are the exact GamePadTriggers/GamePadDPad source forms.
    checkFixture(
        tests, root,
        "tests/cc1/c54_constructor_get_only_enum_zero/c54_constructor_get_only_enum_zero.nsproj",
        "cc1-c54-constructor-get-only-enum-zero", 42);

    // NativeSharp 3.0 T01-F01/F02/F05. Named calls retain textual evaluation order; typed params
    // materialize zero/one/many elements in stack-only ReadOnlySpan storage; generalized struct
    // initializers map names deterministically and evaluate one same-type spread exactly once.
    checkFixture(
        tests, root,
        "tests/cc1/c55_ns3_calls_initializers/c55_ns3_calls_initializers.nsproj",
        "cc1-c55-ns3-calls-initializers", 42);

    // NativeSharp 3.0 T01-F03/F04/F06/F07. Null coalescing and null-coalescing assignment lower
    // their primary/address once and keep the fallback lazy; `with` evaluates its source once
    // before textual field updates; ^ indexes from the already-read length; ?. evaluates one
    // receiver; and fully-static interpolation folds into the ordinary literal path.
    checkFixture(
        tests, root,
        "tests/cc1/c56_ns3_null_with/c56_ns3_null_with.nsproj",
        "cc1-c56-ns3-null-with", 42);

    // NativeSharp 3.0 T02-F01/F02/F03. Structural tuple names do not affect identity; nested
    // layouts use the canonical Win64 direct/sret classification; literals convert element by
    // element; assignment deconstruction snapshots the source before aliasing targets; and
    // `var (a, _)` creates inferred locals without copying or re-evaluating the source.
    checkFixture(
        tests, root,
        "tests/cc1/c57_ns3_tuples/c57_ns3_tuples.nsproj",
        "cc1-c57-ns3-tuples", 42);

    // NativeSharp 3.0 T02-F04/F05/F06. Records retain struct layout and expose only explicitly
    // derived operations; primary parameters desugar to named physical fields plus an ordinary
    // constructor; readonly records preserve readonly fields; aggregate spread evaluates its
    // source exactly once before applying explicit replacements.
    checkFixture(
        tests, root,
        "tests/cc1/c58_ns3_records/c58_ns3_records.nsproj",
        "cc1-c58-ns3-records", 42);

    // NativeSharp 3.0 T03-F01/F02/F03 safe vertical. Tagged variants use a minimum-width tag
    // plus one in-place union payload; generic, unit, tuple and alignment-heavy alternatives
    // execute at O0. Statement match snapshots the governing value once, scopes payload bindings,
    // supports nesting and requires a final wildcard until metadata-domain analysis is connected.
    checkFixture(
        tests, root,
        "tests/cc1/c59_ns3_variants/c59_ns3_variants.nsproj",
        "cc1-c59-ns3-variants", 42);

    // NativeSharp 3.0 T04-F01/F04/F07 productive slice. Exact expected types infer generic calls
    // in typed initializers, returns and assignments (including null); inferred arguments still
    // traverse declarative constraints; the same materializer publishes deterministic budget and
    // cache statistics. Resource-limit negatives are kept as directed probes, not this fixture.
    checkFixture(
        tests, root,
        "tests/cc1/c60_ns3_generic_solver/c60_ns3_generic_solver.nsproj",
        "cc1-c60-ns3-generic-solver", 42);

    // NativeSharp 3.0 T04-F02/F03: const values reach layout and method bodies; a trailing type
    // pack accepts empty/multiple lists, reports its length, and forwards with `Ts...`.
    checkFixture(
        tests, root,
        "tests/cc1/c61_ns3_const_generics_packs/c61_ns3_const_generics_packs.nsproj",
        "cc1-c61-ns3-const-generics-packs", 42);

    // NativeSharp 3.0 T05-F01: trait declarations reuse the interface ABI; a constrained generic
    // call is statically specialized while trait-typed views retain dynamic dispatch.
    checkFixture(
        tests, root,
        "tests/cc1/c62_ns3_traits_dispatch/c62_ns3_traits_dispatch.nsproj",
        "cc1-c62-ns3-traits-dispatch", 42);

    // NativeSharp 3.0 T06-F01..F05. A partial move keeps the containing owner alive, empties
    // exactly one owned field and transfers that value to a new owner; scope cleanup must destroy
    // both payloads exactly once. Borrow-conflict variants remain directed negative probes.
    checkFixture(
        tests, root,
        "tests/cc1/c63_ns3_move_borrow_drop/c63_ns3_move_borrow_drop.nsproj",
        "cc1-c63-ns3-move-borrow-drop", 42);

    // NativeSharp 3.0 T06-F06 vertical. `pin raii` owners remain borrowable but cannot be moved
    // or replaced; lexical cleanup still invokes their destructor exactly once.
    checkFixture(
        tests, root,
        "tests/cc1/c64_ns3_pin_drop_boundary/c64_ns3_pin_drop_boundary.nsproj",
        "cc1-c64-ns3-pin-drop-boundary", 42);

    // NativeSharp 3.0 T07-F04/F06. Contextual panic() has type never and lowers to the stable
    // abort-profile trap. The fixture takes the non-panicking branch; a directed true-path probe
    // outside this aggregate runner checks exit code 142 so the baseline never aborts its host.
    checkFixture(
        tests, root,
        "tests/cc1/c65_ns3_panic_never/c65_ns3_panic_never.nsproj",
        "cc1-c65-ns3-panic-never", 42);

    // NativeSharp 3.0 T08-F02/F05. The same executable crosses all three callable-value shapes:
    // a zero-environment extern-C lambda, a managed shared-cell closure, and a bound instance
    // method. Directed MIR probes additionally verify the now-public closure cost/provenance.
    checkFixture(
        tests, root,
        "tests/cc1/c66_ns3_callable_values/c66_ns3_callable_values.nsproj",
        "cc1-c66-ns3-callable-values", 42);

    // NativeSharp 3.0 E017 carrier bridge. Optional<T> coalescing calls the public
    // HasValue/UncheckedValue protocol, evaluates the carrier once and keeps fallback lazy.
    checkFixture(
        tests, root,
        "tests/cc1/c67_ns3_optional_coalescing/c67_ns3_optional_coalescing.nsproj",
        "cc1-c67-ns3-optional-coalescing", 42);

    // NSC-LIMIT-001/002. Dos modulos publican un tipo `Color`; el consumidor llama miembros
    // estaticos por nombre calificado, alias de namespace y alias de tipo. Tambien lee un campo,
    // una propiedad y el miembro `C` de un enum, de modo que la prueba cruza hasta MIR, LLVM,
    // enlace y ejecucion sin depender de una resolucion accidental por nombre corto.
    checkFixture(
        tests, root,
        "tests/cc1/c40_receptor_estatico_calificado/c40_receptor_estatico_calificado.nsproj",
        "cc1-c40-receptor-estatico-calificado", 42);

    // NSC-LIMIT-008. `f.Actual.Leer()` con `Actual` un getter que devuelve un struct por valor:
    // antes se rechazaba con «properties are semantic accessor calls and are not addressable».
    // La fixture cuenta las llamadas al getter DENTRO del accesor, porque bajar el receptor dos
    // veces daria el valor correcto y llamaria dos veces -- un defecto que comprobar el valor no
    // ve. Cubre struct pequenio, struct por sret, metodo mutante sobre el temporal, y un campo
    // struct corriente que no debe recibir temporal alguno.
    checkFixture(
        tests, root,
        "tests/cc1/c41_propiedad_receptor_struct/c41_propiedad_receptor_struct.nsproj",
        "cc1-c41-propiedad-receptor-struct", 42);

    // NSC-LIMIT-010. `new Caja(move(duenio))` con un constructor de parametro `owned`. Cubre la
    // transferencia directa, el reenvio de un parametro `owned` a otro constructor, y la
    // delegacion `: this(move(...))`. Los negativos --copia sin move, move a parametro sin
    // calificador-- no caben en una fixture, que tiene que compilar entera.
    checkFixture(
        tests, root,
        "tests/cc1/c42_move_constructor_owned/c42_move_constructor_owned.nsproj",
        "cc1-c42-move-constructor-owned", 42);

    // NSC-LIMIT-004. `==` y `!=` sobre dos valores del mismo struct, que antes llegaban a
    // `LLVMBuildICmp` como agregado y el verificador rechazaba el modulo entero. Cubre padding,
    // anidamiento, arreglo fijo, un byte, 32 bytes, coma flotante y un generico materializado.
    // Los rechazos --orden, tipos distintos, union-- viven en la suite de negativos.
    checkFixture(
        tests, root,
        "tests/cc1/c43_igualdad_de_structs/c43_igualdad_de_structs.nsproj",
        "cc1-c43-igualdad-de-structs", 42);

    // NSC-LIMIT-007. `where T : class, IComponente`. La fixture mide la mitad POSITIVA: las CUATRO
    // posiciones gramaticales que se admiten --clase con lista de bases delante, struct, metodo y
    // delegado--, el cuerpo generico llamando a un miembro de la interfaz del constraint, la
    // derivacion, la IDENTIDAD (`where T : Base` lo satisface `Base` misma) y una interfaz que
    // extiende otra. La quinta posicion, una FUNCION GLOBAL generica, se rechaza --no se
    // materializa, de modo que su clausula no la comprobaria nadie-- y ese rechazo vive abajo,
    // con los demas negativos, que tampoco caben aqui: una fixture tiene que compilar entera.
    checkFixture(
        tests, root,
        "tests/cc1/c44_constraints_genericos/c44_constraints_genericos.nsproj",
        "cc1-c44-constraints-genericos", 42);

    // NSC-LIMIT-009. Dos identidades que se distinguen --`c45.Caja+Pieza` y `c45.Caja.Pieza`--
    // llevadas hasta HIR, LLVM, enlace y ejecucion. Las pruebas de tipos anidados llegaban al
    // modelo de nombres y al de miembros, donde las identidades ya eran distintas; el defecto
    // vivia una fase mas abajo, en los cuatro saneadores que aplastaban `+` y `.` al mismo `_`.
    checkFixture(
        tests, root,
        "tests/cc1/c45_tipos_anidados/c45_tipos_anidados.nsproj",
        "cc1-c45-tipos-anidados", 42);

    // NSC-LIMIT-007. Que plantillas genericas alcanza una llamada. Programas VALIDOS que el
    // compilador rechazaba: dos `C.F<T>` en namespaces distintos --que no compilaban NINGUNA de
    // las dos--, una sobrecarga de dos parametros que se materializaba para una llamada de uno y
    // cuyo cuerpo sustituido rechazaba el programa --con y sin `where`, porque son dos huecos--,
    // y un receptor ABREVIADO que alcanzaba a la clase homonima del otro namespace.
    //
    // Y un caso que NO es una regresion sino cobertura, marcado como tal en la fixture: dos
    // candidatas de la MISMA cuenta de argumentos, una con una clausula que el argumento
    // incumple. Es el unico sitio del arbol que recorre el APLAZAMIENTO de esa clausula.
    checkFixture(
        tests, root,
        "tests/cc1/c46_sobrecargas_genericas/c46_sobrecargas_genericas.nsproj",
        "cc1-c46-sobrecargas-genericas", 42);


    // A PARTIR DE AQUI NADA ES UNA FIXTURE: son rechazos y comprobaciones sobre el IR. Los corre
    // la entrada `--rest`, y nadie mas -- una entrada por fixture que ademas arrastrara todo esto
    // lo repetiria cuarenta veces.
    if (!restSelected())
    {
        tests.expect(
            gSelectedFixture.empty() || gSelectedFixtureMatched,
            "selector '" + gSelectedFixture + "' names a registered fixture");
        return tests.result();
    }

    // T01 deliberately rejects shapes for which 3.0 does not yet have a sound carrier/protocol.
    // These checks prevent a later parser/backend change from silently turning the rejection into
    // an allocating array/string slice, an Optional<T> ABI guess, or an owning aggregate copy.
    checkRejected(
        tests, root, "ns3-t01-runtime-interpolation-needs-handler",
        R"(namespace reject { public class Program { public static int Main() { int value = 42; string text = $"value={value}"; return 0; } } })",
        "runtime holes require an explicit formatting handler", "3");
    checkRejected(
        tests, root, "ns3-t01-value-null-conditional-needs-optional",
        R"(namespace reject { public class Box { public int Value; } public class Program { public static int Main() { Box box = null; int value = box?.Value; return value; } } })",
        "Optional<T> carrier", "3");
    checkRejected(
        tests, root, "ns3-t01-heap-array-range-needs-view",
        R"(namespace reject { public class Program { public static int Main() { int[] source = new int[4]; int[] result = source[1..^1]; delete[] source; return 0; } } })",
        "Span<T>/ReadOnlySpan<T> Length/Slice view", "3");
    checkRejected(
        tests, root, "ns3-t01-owning-with-needs-clone",
        R"(namespace reject { public struct Box { public owned string Text; } public class Program { public static int Main() { Box source = { Text = "a" }; Box copy = source with { Text = "b" }; return 0; } } })",
        "explicit clone/copy strategy", "3");

    // T06-F06: pin is an edition-3 ownership commitment. The owner remains borrowable and its
    // ordinary lexical cleanup stays active, but neither ownership transfer nor replacement may
    // relocate it after construction.
    checkRejected(
        tests, root, "ns3-t06-pin-requires-edition3",
        R"(class Payload { } class Program { public static int Main() { pin raii Payload stable = new Payload(); return 0; } })",
        "pin raii locals requires <Language>3</Language>", "2");
    checkRejected(
        tests, root, "ns3-t06-pinned-owner-cannot-move",
        R"(class Payload { } class Program { public static int Main() { pin raii Payload stable = new Payload(); owned Payload moved = move(stable); return 0; } })",
        "cannot move pinned owner 'stable'", "3");
    checkRejected(
        tests, root, "ns3-t06-pinned-owner-cannot-be-replaced",
        R"(class Payload { } class Program { public static int Main() { pin raii Payload stable = new Payload(); owned Payload replacement = new Payload(); stable = move(replacement); return 0; } })",
        "cannot replace pinned owner 'stable'", "3");
    checkRejected(
        tests, root, "ns3-t06-pinned-projection-cannot-move",
        R"(class Payload { } class Holder { public owned Payload Slot; public Holder(owned Payload value) { Slot = move(value); } } class Program { public static int Main() { pin raii Holder stable = new Holder(new Payload()); owned Payload moved = move(stable.Slot); return 0; } })",
        "cannot partially move from pinned owner 'stable'", "3");

    // =====================================================================================
    // NSC-LIMIT-010 y NSC-LIMIT-004 -- LOS RECHAZOS.
    //
    // Ninguno cabe en una fixture: una fixture tiene que compilar entera, y estos programas no
    // deben compilar. Sin ellos, las fixtures de arriba solo demuestran que lo legal pasa, y una
    // regla que acepta todo tambien lo hace.
    // =====================================================================================

    // T01-F05. Generalized initializers must not partially construct an owner-bearing value and
    // then leak an earlier field when a later expression fails. Value-type drop flags arrive with
    // T06; until then the product contract is an explicit, stable rejection rather than silent
    // resource loss.
    checkRejected(
        tests, root, "cc1-t01-f05-nontrivial-generalized-initializer",
        R"NS(
class Resource { }
struct OwnerAggregate { public owned Resource Value; }
class Program
{
    public static int Main()
    {
        OwnerAggregate invalid = { Value = null };
        return 0;
    }
}
)NS",
        "requires a trivially destructible target",
        "3");

    // Copiar un owner a un parametro `owned` sin `move`. La resolucion de constructores no miraba
    // propiedad en absoluto, asi que esto COMPILABA antes de NSC-LIMIT-010 -- y producia dos
    // owners del mismo objeto.
    checkRejected(
        tests, root, "cc1-limit010-constructor-copia-sin-move",
        R"NS(
namespace limit010copia
{
    class Carga { public int valor; public Carga(int v) { valor = v; } }
    class Caja
    {
        private owned Carga dentro;
        public Caja(owned Carga entrante) { dentro = move(entrante); }
    }
    class P
    {
        public static int Main()
        {
            owned Carga c = new Carga(7);
            owned Caja b = new Caja(c);
            return 42;
        }
    }
}
)NS",
        "requires an ownership transfer");

    // `move` hacia un parametro SIN calificador: el contrato se perderia en silencio y nadie
    // liberaria el objeto.
    checkRejected(
        tests, root, "cc1-limit010-move-a-parametro-sin-calificador",
        R"NS(
namespace limit010sincalif
{
    class Carga { public int valor; public Carga(int v) { valor = v; } }
    class Caja
    {
        private Carga dentro;
        public Caja(Carga entrante) { dentro = entrante; }
    }
    class P
    {
        public static int Main()
        {
            owned Carga c = new Carga(7);
            Caja b = new Caja(move(c));
            return 42;
        }
    }
}
)NS",
        "cannot receive an ownership transfer");

    // USO DESPUES DE MOVER. Es lo que la fixture c42 NO puede medir --tendria que no compilar--
    // y sin esto «la fuente quedo moved-from» seria una afirmacion sin prueba.
    checkRejected(
        tests, root, "cc1-limit010-uso-tras-mover",
        R"NS(
namespace limit010usotras
{
    class Carga { public int valor; public Carga(int v) { valor = v; } }
    class Caja
    {
        private owned Carga dentro;
        public Caja(owned Carga entrante) { dentro = move(entrante); }
    }
    class P
    {
        public static int Main()
        {
            owned Carga c = new Carga(7);
            owned Caja b = new Caja(move(c));
            return c.valor;
        }
    }
}
)NS",
        "use of moved-from");

    // `this(...)` sin `move`, teniendo el parametro `owned` en la mano. El camino del
    // inicializador calculaba la objecion y luego emitia el mensaje generico; ahora dice cual.
    checkRejected(
        tests, root, "cc1-limit010-inicializador-sin-move",
        R"NS(
namespace limit010init
{
    class Carga { public int valor; public Carga(int v) { valor = v; } }
    class Caja
    {
        private owned Carga dentro;
        public Caja(owned Carga entrante) { dentro = move(entrante); }
        public Caja(owned Carga entrante, int extra) : this(entrante) { }
    }
    class P
    {
        public static int Main() { return 42; }
    }
}
)NS",
        "requires an ownership transfer");

    // ORDENAR structs. Un struct no tiene orden natural y adivinarlo daria una respuesta estable
    // y arbitraria, que es la peor clase.
    checkRejected(
        tests, root, "cc1-limit004-orden-entre-structs",
        R"NS(
namespace limit004orden
{
    struct Punto { public int x; public int y; }
    class P
    {
        public static int Main()
        {
            Punto a; a.x = 1; a.y = 2;
            Punto b; b.x = 1; b.y = 2;
            if (a < b) { return 1; }
            return 42;
        }
    }
}
)NS",
        "supports only '==' and '!='");

    // Dos structs DISTINTOS con la misma forma. Comparar contra otro tipo significaria boxing,
    // que este lenguaje no hace.
    checkRejected(
        tests, root, "cc1-limit004-structs-de-tipos-distintos",
        R"NS(
namespace limit004tipos
{
    struct Punto { public int x; public int y; }
    struct Otro { public int x; public int y; }
    class P
    {
        public static int Main()
        {
            Punto a; a.x = 1; a.y = 2;
            Otro o; o.x = 1; o.y = 2;
            if (a == o) { return 1; }
            return 42;
        }
    }
}
)NS",
        "compares only against its own type");

    // UNION. Solo un miembro esta vivo y nada en el valor dice cual: la misma razon que ya
    // rechaza `union.Equals(...)`, repetida para que no entre por la otra puerta.
    checkRejected(
        tests, root, "cc1-limit004-union",
        R"NS(
namespace limit004union
{
    union Mezcla { public int entero; public float flotante; }
    class P
    {
        public static int Main()
        {
            Mezcla m1; m1.entero = 1;
            Mezcla m2; m2.entero = 1;
            if (m1 == m2) { return 1; }
            return 42;
        }
    }
}
)NS",
        "only one member of a union is live");

    // `base(...)` sin `move`. Comparte la implementacion con `this(...)` y NO comparte todo: la
    // accesibilidad del candidato se calcula distinto para cada uno (`ExplicitThis` la da por
    // buena, `base` la comprueba contra la clase derivada). Compartir codigo no es lo mismo que
    // estar probado, y sin este caso una divergencia entre los dos pasaba sin verse.
    checkRejected(
        tests, root, "cc1-limit010-base-sin-move",
        R"NS(
namespace limit010base
{
    class Carga { public int valor; public Carga(int v) { valor = v; } }
    class CajaBase
    {
        private owned Carga guardada;
        public CajaBase(owned Carga entrante) { guardada = move(entrante); }
    }
    class CajaDerivada : CajaBase
    {
        public CajaDerivada(owned Carga entrante) : base(entrante) { }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "requires an ownership transfer");

    // =====================================================================================
    // NSC-LIMIT-004 -- EL PADDING, MEDIDO EN EL IR Y NO AFIRMADO EN UN COMENTARIO.
    //
    // POR QUE HACE FALTA, aunque la fixture c43 ya compare un struct con relleno y salga verde.
    // El relleno NO ES DIRECCIONABLE desde NativeSharp: no hay forma de escribir dos valores que
    // difieran solo en los bytes de relleno. Asi que una implementacion incorrecta que comparara
    // los dieciseis bytes del carrier --un memcmp, o un `icmp` sobre el agregado-- daria la MISMA
    // respuesta en esa fixture y pasaria igual.
    //
    // Lo unico que distingue una implementacion de la otra es la forma del IR. `Padded` declara
    // dos campos: un `byte` en el offset 0 y un `long` en el 8. Nueve bytes de contenido en
    // dieciseis de tamanio.
    // =====================================================================================
    {
        const std::string text = lowerToLlvmText(
            tests, root, "cc1-limit004-padding-en-el-ir",
            R"NS(
namespace limit004pad
{
    struct Padded { public byte bandera; public long ancho; }
    struct Tres { public byte a; public byte b; public long c; }
    struct Lleno { public long x; public long y; }
    class P
    {
        public static bool Iguales(Padded a, Padded b) { return a == b; }
        public static bool TresIguales(Tres a, Tres b) { return a == b; }
        public static bool LlenoIguales(Lleno a, Lleno b) { return a == b; }
    }
}
)NS");

        // SE RECORTA A LA FUNCION, y no es cosmetico: `lowerToLlvmText` devuelve el MODULO
        // ENTERO, biblioteca de runtime incluida, y ahi hay decenas de structs de dieciseis
        // bytes. Una cuenta sobre el modulo mediria a los vecinos y se rompria el dia que
        // alguien anadiera una comparacion de agregados al runtime -- un rojo que no habla de
        // este contrato. Recortada, cada cuenta de abajo es exacta.
        const std::string cuerpo = sliceFunction(text, "limit004pad$2EP__Iguales");
        // ESTA GUARDA NO ES UNA FORMALIDAD. Si `sliceFunction` no encuentra la funcion --un
        // nombre mal escrito basta-- `cuerpo` queda vacio, los conteos dan cero y fallan... pero
        // las aserciones de AUSENCIA (`find(...) == npos`) pasan solas sobre una cadena vacia.
        // Un verde falso entre los rojos, que es peor que un rojo limpio. Medido: la primera
        // version de esta prueba buscaba `padir_P__Iguales`, el nombre de un programa de scratch,
        // en un modulo cuyo namespace es `limit004pad`.
        //
        // Y LA GUARDA VOLVIO A SALVARLA. La segunda version corrigio el namespace pero dejo el
        // separador del saneador VIEJO --`limit004pad_P`--, que desde NSC-LIMIT-009 es
        // `limit004pad$2EP`: `.` ya no se aplasta a `_`. El recorte devolvia vacio otra vez, y lo
        // que lo dijo fue esta linea y no las cuentas de abajo, que sobre una cadena vacia habrian
        // dado sus ausencias por buenas.
        tests.expect(!cuerpo.empty(), "cc1-limit004-padding: encuentra la funcion en el modulo");
        if (cuerpo.empty()) return tests.result();

        // DOS hojas, ni una mas. Con tres, el relleno estaria entrando en la respuesta; con una,
        // habria un campo sin comparar.
        tests.expect(
            countOccurrences(cuerpo, "getelementptr i8, ptr %struct.root.operand, i64 ") == 2,
            "cc1-limit004-padding: compara exactamente dos hojas");
        // Y EN LOS OFFSETS DECLARADOS. El 0 es `bandera`, el 8 es `ancho`. Los bytes 1..7 y 9..15
        // son relleno: no hay ni un GEP que los alcance.
        tests.expect(
            countOccurrences(cuerpo, "getelementptr i8, ptr %struct.root.operand, i64 0") == 1,
            "cc1-limit004-padding: lee el offset 0");
        tests.expect(
            countOccurrences(cuerpo, "getelementptr i8, ptr %struct.root.operand, i64 8") == 1,
            "cc1-limit004-padding: lee el offset 8");
        // Una comparacion por hoja, sobre los bits de la hoja -- no sobre el carrier.
        tests.expect(
            countOccurrences(cuerpo, "= icmp eq i64 %struct.eq.left") == 2,
            "cc1-limit004-padding: una comparacion por hoja");
        // Y NADA del agregado entero. Estas dos formas son las que un `icmp` sobre el carrier o un
        // memcmp dejarian en el IR, y son exactamente las que la fixture c43 NO puede descartar:
        // con el mismo contenido declarado en los dos valores, las dos dan la misma respuesta.
        tests.expect(
            cuerpo.find("icmp eq [") == std::string::npos &&
                cuerpo.find("icmp ne [") == std::string::npos,
            "cc1-limit004-padding: ningun icmp sobre el agregado");
        tests.expect(
            cuerpo.find("memcmp") == std::string::npos,
            "cc1-limit004-padding: ningun memcmp del carrier");
        // El carrier se carga DOS veces y solo para copiar cada operando a su ranura. Si apareciera
        // una tercera, seria una lectura del agregado que no es una copia.
        tests.expect(
            countOccurrences(cuerpo, "load [16 x i8]") == 2,
            "cc1-limit004-padding: el carrier solo se carga para copiar los dos operandos");

        // LA PRUEBA DE QUE LO ANTERIOR MIDE ALGO, y no solo describe lo que salio.
        //
        // Los tres structs miden LOS MISMOS DIECISEIS BYTES y declaran distinto numero de campos.
        // Una implementacion que comparara el carrier daria la misma forma para los tres -- el
        // conteo de hojas seria constante. Que siga a los CAMPOS DECLARADOS y no al tamanio es la
        // unica diferencia observable entre las dos implementaciones, y es lo que se afirma aqui:
        //
        //     Padded  1 + 8, siete de relleno  -> 2 hojas, offsets 0 y 8
        //     Tres    1 + 1 + 8, seis          -> 3 hojas, offsets 0, 1 y 8
        //     Lleno   8 + 8, cero              -> 2 hojas, offsets 0 y 8
        //
        // `Padded` y `Lleno` dan la misma cuenta a proposito: lo que separa a los tres no es el
        // relleno sino cuantos campos hay, que es exactamente el contrato.
        const std::string tres = sliceFunction(text, "limit004pad$2EP__TresIguales");
        const std::string lleno = sliceFunction(text, "limit004pad$2EP__LlenoIguales");
        tests.expect(
            !tres.empty() && !lleno.empty(),
            "cc1-limit004-padding: encuentra las dos funciones de control");
        tests.expect(
            countOccurrences(tres, "getelementptr i8, ptr %struct.root.operand, i64 ") == 3,
            "cc1-limit004-padding: tres campos en dieciseis bytes dan TRES hojas");
        tests.expect(
            countOccurrences(tres, "getelementptr i8, ptr %struct.root.operand, i64 1") == 1,
            "cc1-limit004-padding: la segunda hoja de Tres esta en el offset 1");
        tests.expect(
            countOccurrences(lleno, "getelementptr i8, ptr %struct.root.operand, i64 ") == 2,
            "cc1-limit004-padding: dos campos sin relleno dan DOS hojas");
    }


    // =====================================================================================
    // NSC-LIMIT-007 -- invariantes de especializacion, cada uno por su mecanismo.
    // =====================================================================================
    //
    // Las tres comprobaciones de aqui reemplazan a una anterior que NO probaba lo que decia: usaba
    // `F<Pieza>` y `F<Valor>` --dos especializaciones DISTINTAS-- y sobrecargas de uno y dos
    // parametros, que el filtro separa antes de formar grupo. O sea, no pasaba por la
    // especializacion cacheada ni por el codigo de diferido, y habria seguido verde con el rango
    // viejo.
    //
    // Aqui las dos llamadas cierran el MISMO `<Valor>` y las dos sobrecargas toman UN argumento,
    // de modo que forman grupo, se cachean, y la segunda llamada reutiliza la especializacion que
    // creo la primera.
    {
        const std::string source = R"NS(
namespace limit007cache
{
    interface IComponente { int Peso(); }
    class Valor { public int x; }
    class P
    {
        private static int F<T>(string s) { return 0; }
        private static int F<T>(int n) where T : IComponente { return n; }
        public static int Main()
        {
            if (F<Valor>("hola") != 0) { return 1; }
            return F<Valor>(7);
        }
    }
}
)NS";
        const auto directory = root / "reject" / "cc1-limit007-sitio-cacheado";
        writeFile(
            directory / "reject.nsproj",
            "<Project><PropertyGroup><Name>reject</Name></PropertyGroup>"
            "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
        writeFile(directory / "Main.ns", source);

        ProjectCompilation compilation;
        std::string error;
        compilation.compileProjectMethodBodies(directory / "reject.nsproj", error);
        tests.expect(compilation.diagnostics().hasErrors(),
            "cc1-limit007-cache: the second call is rejected");

        const std::size_t primera = source.find("F<Valor>(\"hola\")");
        const std::size_t segunda = source.find("F<Valor>(7)");
        tests.expect(primera != std::string::npos && segunda != std::string::npos &&
                primera < segunda,
            "cc1-limit007-cache: the fixture really has two calls that close the same T");

        bool segundaSenalada = false;
        bool primeraSenalada = false;
        for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        {
            if (diagnostic.message().find("requires an implementation of") == std::string::npos)
                continue;
            if (!diagnostic.range().has_value()) continue;
            (diagnostic.range()->begin().offset() >= segunda ? segundaSenalada : primeraSenalada) =
                true;
        }
        // El rango que la especializacion guarda es el de la PRIMERA instanciacion, porque se
        // cachea por clave y no se actualiza. Que el diagnostico apunte a la segunda es lo que
        // demuestra que el sitio manda sobre el guardado.
        //
        // Y QUE ESTE PROGRAMA PASA DE VERDAD POR AHI ESTA MEDIDO, no supuesto: sustituyendo
        // `callRange` por el rango guardado en `reportSpecializationConstraintViolations`, el
        // unico diagnostico pasa del desplazamiento 367 --la segunda llamada-- al 311 --la
        // primera--, y las dos comprobaciones de abajo se ponen rojas. La version anterior de
        // esta prueba usaba `F<Pieza>` y `F<Valor>` --especializaciones DISTINTAS, cada una con
        // su propio rango-- y sobrecargas de uno y dos parametros, que el filtro de cuenta separa
        // antes de formar grupo: con esa misma sustitucion se habria quedado VERDE.
        tests.expect(segundaSenalada,
            "cc1-limit007-cache: the cached specialization reports the CURRENT call site");
        tests.expect(!primeraSenalada,
            "cc1-limit007-cache: and never the earlier call, whose overload resolves fine");
    }

    // UNA ESPECIALIZACION INVALIDA NO ANALIZA SU CUERPO, tenga grupo o no lo tenga.
    //
    // Se mide por AUSENCIA, que es lo unico que lo distingue: el error de constraint aparecia ya
    // antes: lo que sobraba era un SEGUNDO error, salido del cuerpo sustituido. Una comprobacion
    // que solo buscara el primer mensaje habria estado verde con el defecto puesto.
    //
    // Los dos programas se pasaron por el compilador ANTERIOR y los dos emitian ese segundo
    // mensaje: el de `Solo<int>` decia ademas «member access requires a class/interface
    // reference [...] receiver has type 'builtin:int'», y el de `Caja<Valor>` anadia
    // «[...] has no compatible member named 'MetodoInexistente'». Con este, uno cada uno.
    {
        struct Caso final
        {
            const char* label;
            const char* source;
            const char* constraintMessage;
            const char* bodyMessage;
        };
        const Caso casos[] = {
            // Sin grupo: una sola plantilla, asi que su diagnostico se emite de inmediato y nunca
            // entraba en el mapa de diferidos -- que era el unico sitio que se consultaba.
            {"cc1-limit007-invalida-sin-grupo",
             R"NS(
namespace limit007sola
{
    interface IComponente { int Peso(); }
    class P
    {
        private static int Solo<T>(T x) where T : IComponente { return x.Peso(); }
        public static int Main() { return Solo<int>(1); }
    }
}
)NS",
             "requires 'IComponente'",
             "member access requires a class/interface reference"},
            // De TIPO: la invalidez esta en el nodo de `Caja<Valor>`, y sus metodos son hijos
            // suyos, asi que preguntar solo por el nodo del metodo no la veia.
            {"cc1-limit007-invalida-de-tipo",
             R"NS(
namespace limit007tipo
{
    struct Valor { public int x; }
    class Caja<T> where T : class
    {
        public int Medir(T v) { return v.MetodoInexistente(); }
    }
    class P { public static int Main() { Caja<Valor> c = new Caja<Valor>(); return 42; } }
}
)NS",
             "requires a reference type",
             "has no compatible member named"},
            // LA TERCERA INVALIDEZ: la PLANTILLA esta mal declarada, y eso no depende de ningun
            // argumento. `int` no puede ser constraint, de modo que la segunda pasada SALTA esa
            // clausula --no hay nada que comprobar contra el argumento--, `pending` queda vacio y
            // la especializacion no entraba en `invalidSpecializations_`. Su cuerpo se analizaba y
            // el usuario recibia, encima del error suyo, un `MetodoInexistente` derivado.
            //
            // Se marca ahora desde `invalidTemplateOrdinals_`, que se llena de las violaciones de
            // DECLARACION de la primera pasada.
            {"cc1-limit007-plantilla-mal-declarada",
             R"NS(
namespace limit007mala
{
    class Mala<T> where T : int
    {
        public int Medir(T x) { return x.MetodoInexistente(); }
    }
    class P { public static int Main() { Mala<int> m = new Mala<int>(); return 42; } }
}
)NS",
             "cannot constrain 'T'",
             "MetodoInexistente"},
        };

        for (const Caso& caso : casos)
        {
            const auto directory = root / "reject" / caso.label;
            writeFile(
                directory / "reject.nsproj",
                "<Project><PropertyGroup><Name>reject</Name></PropertyGroup>"
                "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
            writeFile(directory / "Main.ns", caso.source);

            ProjectCompilation compilation;
            std::string error;
            compilation.compileProjectMethodBodies(directory / "reject.nsproj", error);

            bool dijoElConstraint = false;
            bool dijoElCuerpo = false;
            for (const auto& diagnostic : compilation.diagnostics().diagnostics())
            {
                if (diagnostic.message().find(caso.constraintMessage) != std::string::npos)
                    dijoElConstraint = true;
                if (diagnostic.message().find(caso.bodyMessage) != std::string::npos)
                    dijoElCuerpo = true;
            }
            tests.expect(dijoElConstraint,
                std::string(caso.label) + ": the constraint violation is reported");
            tests.expect(!dijoElCuerpo,
                std::string(caso.label) +
                    ": and the substituted body is NOT analysed, so it adds no second error");
            // Y NI UN ERROR MAS, sea cual sea su texto. Buscar la ausencia de UN mensaje concreto
            // deja pasar cualquier otro diagnostico derivado que aparezca manana; contar los
            // errores dice lo que de verdad se quiere decir: el programa tiene UN defecto y el
            // compilador cuenta UNO.
            std::size_t errores = 0;
            for (const auto& diagnostic : compilation.diagnostics().diagnostics())
            {
                if (diagnostic.severity() == nativesharp::diagnostics::DiagnosticSeverity::Error)
                    ++errores;
            }
            tests.expect(errores == 1,
                std::string(caso.label) + ": exactly one error, not a cascade");
        }
    }

    // NSC-LIMIT-007 -- LA ESCALA DE NOMBRES, medida sobre las ESPECIALIZACIONES y no sobre el
    // codigo de salida.
    //
    // c46 codigos 17-20 comprueban que cada llamada devuelve lo suyo. Eso es la mitad: un programa
    // puede devolver lo correcto y HABER CLONADO IGUALMENTE la plantilla ajena, y entonces la
    // fixture esta midiendo otra cosa. Lo que cierra el invariante --«una llamada nunca materializa
    // una plantilla que su receptor no puede nombrar»-- es que esas plantillas no aparezcan en el
    // modelo generico, y eso solo se ve aqui.
    //
    // Las cuatro ajenas tienen ademas cuerpos que no compilan con `T = int`, asi que un clon las
    // delataria tambien por el otro lado. Las dos medidas juntas, no una.
    {
        const std::filesystem::path projectPath = std::filesystem::path(NSC_SOURCE_DIR) /
            "tests/cc1/c46_sobrecargas_genericas/c46_sobrecargas_genericas.nsproj";
        ProjectCompilation compilation;
        std::string error;
        const bool built = compilation.compileProjectMethodBodies(projectPath, error);
        tests.expect(built && compilation.genericModel() != nullptr,
            "cc1-limit007-escala: c46 compiles up to method bodies");
        if (built && compilation.genericModel() != nullptr)
        {
            static constexpr std::string_view kAjenas[] = {
                "c46ajeno.C.F",
                "c46anidado.Helper.F",
                "c46intruso.Caja.F",
                "c46intruso.Caja.G"};
            for (const std::string_view ajena : kAjenas)
            {
                bool clonada = false;
                for (const auto& record : compilation.genericModel()->specializations())
                {
                    if (record.templateKey.find(ajena) != std::string::npos) clonada = true;
                }
                tests.expect(!clonada,
                    std::string("cc1-limit007-escala: '") + std::string(ajena) +
                        "' is never materialised -- the call names a different owner");
            }
            // Y EL CONTROL POSITIVO: las que SI se nombran estan. Sin esto, una fase generica que
            // no materializara nada pasaria las cuatro comprobaciones de arriba.
            static constexpr std::string_view kPropias[] = {
                "c46hijo.C.F",
                "c46anidado.Marco+Helper.F",
                "c46lejos.dentro.Caja.F",
                "c46lejos.dentro.Caja.G"};
            for (const std::string_view propia : kPropias)
            {
                bool clonada = false;
                for (const auto& record : compilation.genericModel()->specializations())
                {
                    if (record.templateKey.find(propia) != std::string::npos) clonada = true;
                }
                tests.expect(clonada,
                    std::string("cc1-limit007-escala: '") + std::string(propia) +
                        "' IS materialised -- it is the owner the call names");
            }
        }
    }

    // =====================================================================================
    // NSC-LIMIT-009 -- que los dos simbolos existan, medido en el IR.
    // =====================================================================================
    //
    // La fixture c45 prueba el COMPORTAMIENTO: si los simbolos colapsaran, uno de los dos cuerpos
    // ganaria y sus codigos de retorno lo dirian. Esto prueba la CAUSA, que es otra cosa: que el
    // modulo contiene dos funciones con nombres distintos, y que el que corresponde al anidado
    // lleva la marca que ningun identificador de usuario puede producir.
    //
    // Hacen falta las dos. Un comportamiento correcto por casualidad --porque el enlazador eligio
    // el cuerpo bueno-- pasaria la fixture; y dos nombres distintos que no se ejecutan no prueban
    // que el programa haga lo correcto.
    {
        const std::string text = lowerToLlvmText(
            tests, root, "cc1-limit009-simbolos-anidados",
            R"NS(
namespace limit009sym.Caja
{
    public class Pieza { public int Peso() { return 20; } }
}
namespace limit009sym
{
    public class Caja
    {
        public class Pieza { public int Peso() { return 21; } }
        public int Interno() { Pieza p = new Pieza(); return p.Peso(); }
    }
    class P
    {
        public static int Main()
        {
            Caja c = new Caja();
            limit009sym.Caja.Pieza n = new limit009sym.Caja.Pieza();
            return c.Interno() + n.Peso();
        }
    }
}
)NS");

        // Alfanumerico y `_` se copian; todo lo demas se escribe `$` + dos digitos hex. `+` es
        // `$2B` y `.` es `$2E`, de modo que los dos nombres son distintos POR CONSTRUCCION y no
        // por una excepcion. El saneador anterior aplastaba los dos a `_`.
        // lowerToLlvmText crea siempre un proyecto llamado ir. TypeIdentity::key incluye el
        // modulo (ir::...) y el codificador convierte esos dos puntos a $3A$3A.
        const std::string anidadoTipo = "ir$3A$3Alimit009sym$2ECaja$2BPieza";
        const std::string deNamespaceTipo = "ir$3A$3Alimit009sym$2ECaja$2EPieza";
        const std::string anidado = anidadoTipo + "__Peso";
        const std::string deNamespace = deNamespaceTipo + "__Peso";
        tests.expect(text.find(anidado) != std::string::npos,
            "cc1-limit009-simbolos: el metodo del tipo ANIDADO lleva '$' en su simbolo");
        tests.expect(text.find(deNamespace) != std::string::npos,
            "cc1-limit009-simbolos: el metodo del tipo de NAMESPACE conserva su simbolo de siempre");
        tests.expect(anidado != deNamespace,
            "cc1-limit009-simbolos: los dos simbolos son distintos");
        // Y los descriptores de clase, que es la otra mitad del choque: dos tipos que comparten
        // descriptor comparten identidad en tiempo de ejecucion -- `is`, casts y destruccion.
        tests.expect(
            text.find("__nsc_classdesc_" + anidadoTipo) != std::string::npos &&
                text.find("__nsc_classdesc_" + deNamespaceTipo) != std::string::npos,
            "cc1-limit009-simbolos: cada tipo tiene su propio descriptor de clase");
        tests.expect(
            text.find("__nsc_destroy_" + anidadoTipo) != std::string::npos &&
                text.find("__nsc_destroy_" + deNamespaceTipo) != std::string::npos,
            "cc1-limit009-simbolos: cada tipo tiene su propia entrada de destruccion");

        // Y EL PAR QUE LA PRIMERA CORRECCION NO SEPARABA: un namespace `A` con una clase `B`
        // frente a una clase llamada `A_B`. Los dos daban `..._A_B` cuando el saneador aplastaba
        // el punto; ahora el punto es `$2E` y el subrayado sigue siendo un subrayado.
        const std::string ir2 = lowerToLlvmText(
            tests, root, "cc1-limit009-simbolos-subrayado",
            R"NS(
namespace limit009u
{
    public class A_B { public int Valor() { return 10; } }
    class P
    {
        public static int Main()
        {
            A_B directo = new A_B();
            limit009u.A.B anidado = new limit009u.A.B();
            return directo.Valor() + anidado.Valor();
        }
    }
}
namespace limit009u.A
{
    public class B { public int Valor() { return 20; } }
}
)NS");
        tests.expect(
            ir2.find("__nsc_method_ir$3A$3Alimit009u$2EA_B__Valor") != std::string::npos,
            "cc1-limit009-simbolos: el subrayado de un identificador sigue siendo un subrayado");
        tests.expect(
            ir2.find("__nsc_method_ir$3A$3Alimit009u$2EA$2EB__Valor") != std::string::npos,
            "cc1-limit009-simbolos: el punto de un namespace NO es un subrayado");
    }

    // NSC-LIMIT-018, lado negativo. La pereza elimina requisitos de miembros NO usados; no
    // convierte un cuerpo invalido en valido cuando el programa realmente lo selecciona.
    checkRejected(
        tests, root, "cc1-limit018-used-generic-member-still-validates",
        R"NS(
namespace limit018used
{
    struct Pair { public int X; }
    class Box<T>
    {
        public int Good(T value) { return 42; }
        public bool NeedsOrder(T left, T right) { return left < right; }
    }
    class P
    {
        public static int Main()
        {
            Box<Pair> box = new Box<Pair>();
            Pair a; Pair b;
            return box.NeedsOrder(a, b) ? 1 : 0;
        }
    }
}
)NS",
        "there is no ordering over its fields");

    checkRejected(
        tests, root, "cc1-limit020-required-after-optional",
        R"NS(
namespace limit020order
{
    class P
    {
        static int F(int first = 1, int second) { return first + second; }
        public static int Main() { return 42; }
    }
}
)NS",
        "cannot follow a parameter with a default value");

    checkRejected(
        tests, root, "cc1-limit020-default-must-be-constant",
        R"NS(
namespace limit020constant
{
    class P
    {
        static int Value() { return 1; }
        static int F(int value = Value()) { return value; }
        public static int Main() { return 42; }
    }
}
)NS",
        "default parameter value must be a literal constant");

    checkRejected(
        tests, root, "cc1-limit020-default-type",
        R"NS(
namespace limit020type
{
    class P
    {
        static int F(int value = "wrong") { return value; }
        public static int Main() { return 42; }
    }
}
)NS",
        "default value cannot convert to parameter type 'builtin:int'");

    // =====================================================================================
    // NSC-LIMIT-007 -- los constraints que NO se satisfacen, y las clausulas mal escritas.
    // =====================================================================================
    //
    // Los cinco primeros son SEMANTICOS: la clausula esta bien escrita y el argumento no la
    // cumple. Salen de ProjectGenericConstraintModel, que corre entre las interfaces y los
    // cuerpos, y por eso el mensaje puede nombrar el argumento -- antes de esa fase no existe
    // quien responda «este tipo implementa esa interfaz».
    //
    // Los cinco ultimos son SINTACTICOS y salen del parser. Van aqui y no en las pruebas de
    // parsing porque lo que se mide es el mensaje: cada uno de estos errores tenia antes una
    // sola respuesta --«generic constraints are deferred beyond Stage 44»-- y la entrega
    // consiste precisamente en que ahora digan cual de los cinco defectos es.

    checkRejected(
        tests, root, "cc1-limit007-class-con-struct",
        R"NS(
namespace limit007clase
{
    struct Valor { public int x; }
    class Caja<T> where T : class
    {
        public int Medir(T pieza) { return 0; }
    }
    class P { public static int Main() { Caja<Valor> c = new Caja<Valor>(); return 42; } }
}
)NS",
        "requires a reference type ('where T : class'), but the argument 'Valor' is not a class, "
        "interface, delegate, string or dynamic array");

    // El argumento no es un tipo declarado desde el punto de vista de la herencia --es un struct--
    // y aun asi el mensaje tiene que hablar de la INTERFAZ, no repetir el de `class`.
    checkRejected(
        tests, root, "cc1-limit007-interfaz-con-struct",
        R"NS(
namespace limit007ifazvalor
{
    interface IComponente { int Peso(); }
    struct Valor { public int x; }
    class Caja<T> where T : IComponente
    {
        public int Medir(T pieza) { return 0; }
    }
    class P { public static int Main() { Caja<Valor> c = new Caja<Valor>(); return 42; } }
}
)NS",
        "requires an implementation of 'IComponente', but the argument 'Valor' does not implement it");

    // Una CLASE que no implementa la interfaz. Distinto del anterior: aqui el argumento si esta
    // en la tabla de tipos, asi que se consulta el modelo de interfaces de verdad.
    checkRejected(
        tests, root, "cc1-limit007-interfaz-no-implementada",
        R"NS(
namespace limit007ifazclase
{
    interface IComponente { int Peso(); }
    class Suelta { public int y; }
    class Caja<T> where T : IComponente
    {
        public int Medir(T pieza) { return 0; }
    }
    class P { public static int Main() { Caja<Suelta> c = new Caja<Suelta>(); return 42; } }
}
)NS",
        "requires an implementation of 'IComponente', but the argument 'Suelta' does not implement it");

    // Clase base. La fixture positiva mide que la identidad basta; aqui se mide que no derivar
    // no basta, que es la otra mitad de la misma regla.
    checkRejected(
        tests, root, "cc1-limit007-base-no-derivada",
        R"NS(
namespace limit007base
{
    class Base { public int Marca() { return 3; } }
    class Suelta { public int y; }
    class Derivada<T> where T : Base
    {
        public int Marcar(T valor) { return 0; }
    }
    class P { public static int Main() { Derivada<Suelta> d = new Derivada<Suelta>(); return 42; } }
}
)NS",
        "requires a type derived from 'Base', but the argument 'Suelta' does not derive from it");

    // Un constraint que no nombra ni clase ni interfaz es un error DE LA DECLARACION, y por eso
    // este mensaje no menciona ningun argumento: el sitio de instanciacion no tiene nada que
    // corregir, lo tiene quien escribio el `where`. Notese que `Caja` NO se instancia en este
    // programa: la comprobacion corre sobre la PLANTILLA, y esa es la mitad que faltaba -- una
    // biblioteca con la clausula mal escrita y sin un solo uso salia verde.
    checkRejected(
        tests, root, "cc1-limit007-constraint-que-no-es-tipo-referencia",
        R"NS(
namespace limit007enum
{
    enum Color { Rojo, Verde }
    class Caja<T> where T : Color
    {
        public int Medir(T pieza) { return 0; }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "'Color' cannot constrain 'T': a generic constraint names a base class or an interface");

    // A PARTIR DE AQUI, EL PARSER. Restringir un parametro que la declaracion no declara.
    checkRejected(
        tests, root, "cc1-limit007-parametro-inexistente",
        R"NS(
namespace limit007param
{
    class Caja<T> where U : class
    {
        public int Medir(T pieza) { return 0; }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "'U' is not a generic type parameter of this declaration");

    // Dos clausulas para el mismo parametro. C# tampoco lo permite, y aceptarlo aqui obligaria a
    // decidir si se fusionan o si gana la ultima -- una decision que nadie necesita.
    checkRejected(
        tests, root, "cc1-limit007-clausula-duplicada",
        R"NS(
namespace limit007dupclausula
{
    interface IComponente { int Peso(); }
    class Caja<T> where T : class where T : IComponente
    {
        public int Medir(T pieza) { return 0; }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "duplicate 'where' clause for generic type parameter 'T'");

    // `class` fuera de la primera posicion. Es una regla de orden, no de contenido: la clausula
    // de abajo dice exactamente lo mismo que `where T : class, IComponente`, y aun asi se
    // rechaza, porque aceptar las dos formas convierte el orden en algo que hay que recordar.
    checkRejected(
        tests, root, "cc1-limit007-class-fuera-de-la-primera-posicion",
        R"NS(
namespace limit007ordenclass
{
    interface IComponente { int Peso(); }
    class Caja<T> where T : IComponente, class
    {
        public int Medir(T pieza) { return 0; }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "'class' must be the first constraint of its 'where' clause");

    // El mismo tipo dos veces. Es inofensivo --la validacion daria el mismo veredicto-- y se
    // rechaza igualmente: casi siempre es el sintoma de una fusion mal hecha.
    checkRejected(
        tests, root, "cc1-limit007-constraint-duplicado",
        R"NS(
namespace limit007dupconstraint
{
    interface IComponente { int Peso(); }
    class Caja<T> where T : IComponente, IComponente
    {
        public int Medir(T pieza) { return 0; }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "duplicate constraint 'IComponente' for 'T'");

    // Los constraints de C# que este compilador NO implementa. El mensaje los nombra uno a uno
    // en vez de decir «no soportado», porque quien escribe `where T : struct` necesita saber que
    // el compilador entiende la palabra y ha decidido no aceptarla todavia, no que la clausula
    // entera es ilegible.
    checkRejected(
        tests, root, "cc1-limit007-constraint-no-implementado",
        R"NS(
namespace limit007struct
{
    class Caja<T> where T : struct
    {
        public int Medir(T pieza) { return 0; }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "the 'struct' generic constraint is not supported yet; this compiler implements 'class', a base class and interfaces");

    // NSC-LIMIT-007, SEGUNDA VUELTA. Los cinco de abajo salen de una revision que encontro que la
    // primera entrega prometia mas de lo que comprobaba.

    // Una FUNCION GLOBAL generica acepta la clausula gramaticalmente y despues nadie la
    // materializa --`isGenericMethodTemplateNode` reconoce MethodDeclaration y nada mas-- de modo
    // que el `where` quedaba escrito y sin comprobar. Un constraint que no se comprueba es peor
    // que no tenerlo: el autor lo lee como una garantia.
    checkRejected(
        tests, root, "cc1-limit007-where-en-funcion-global",
        R"NS(
namespace limit007funcion
{
    interface IComponente { int Peso(); }
    int Medir<T>(T pieza) where T : class, IComponente { return 0; }
    class P { public static int Main() { return 42; } }
}
)NS",
        "a generic constraint on a global function is not supported yet");

    // `where T : U` con `U` otro parametro de la misma declaracion. Se rechaza en el parser para
    // que la resolucion de nombres no acabe diciendo «unknown type name 'U'», que manda al lector
    // a buscar un tipo que nunca hubo.
    checkRejected(
        tests, root, "cc1-limit007-constraint-es-parametro-generico",
        R"NS(
namespace limit007paramconstraint
{
    class Caja<T, U> where T : U
    {
        public int Medir(T pieza) { return 0; }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "is a generic type parameter of this declaration and cannot be used as a constraint");

    // El MISMO tipo nombrado dos veces bajo dos escrituras. El parser compara texto --es lo unico
    // que puede hacer sin resolver-- y las dos escrituras se le escapan; la comprobacion
    // semantica compara identidades canonicas, que es donde el alias deja de esconder nada.
    checkRejected(
        tests, root, "cc1-limit007-constraint-duplicado-por-alias",
        R"NS(
using IAlias = limit007alias.IComponente;
namespace limit007alias
{
    interface IComponente { int Peso(); }
    class Caja<T> where T : IComponente, IAlias
    {
        public int Medir(T pieza) { return 0; }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "name the same type");

    // Un PUNTERO no es un tipo referencia. Es el control de la fixture: alli `class` lo satisfacen
    // un delegado, `string` y un arreglo, y sin este caso «se acepta casi todo» seria
    // indistinguible de «se acepta todo».
    checkRejected(
        tests, root, "cc1-limit007-class-con-puntero",
        R"NS(
namespace limit007puntero
{
    struct Valor { public int x; }
    class Caja<T> where T : class
    {
        public int Medir() { return 0; }
    }
    class P { public static int Main() { Caja<Valor*> c = new Caja<Valor*>(); return 42; } }
}
)NS",
        "is not a class, interface, delegate, string or dynamic array");

    // Un ARREGLO FIJO tampoco. `T[]` es una referencia --objeto con cabecera, longitud y vida
    // propia-- y `T[N]` es almacenamiento inline, tantos bytes seguidos dentro de quien lo
    // declara. Este compilador ya los separa asi; meter los dos bajo `class` prometia sobre
    // `int[4]` lo que solo es cierto de `int[]`. El positivo de `int[]` vive en la fixture c44.
    checkRejected(
        tests, root, "cc1-limit007-class-con-arreglo-fijo",
        R"NS(
namespace limit007arreglofijo
{
    class Caja<T> where T : class
    {
        public int Uno() { return 1; }
    }
    class P { public static int Main() { Caja<int[4]> c = new Caja<int[4]>(); return 42; } }
}
)NS",
        "is not a class, interface, delegate, string or dynamic array");

    // NSC-LIMIT-007, TERCERA VUELTA. Una sobrecarga que NO acepta la llamada no puede salvar al
    // grupo: `F<T>(T x, int extra)` no atiende `F<int>(1)`, asi que la unica candidata aplicable
    // es la que exige `class` -- y la incumple. La version anterior de la regla de grupo miraba
    // solo el nombre y la aridad generica, y este programa salia verde.
    checkRejected(
        tests, root, "cc1-limit007-sobrecarga-no-aplicable-no-salva",
        R"NS(
namespace limit007aplicable
{
    class P
    {
        private static int F<T>(T x) where T : class { return 0; }
        private static int F<T>(T x, int extra) { return extra; }
        public static int Main() { return F<int>(1); }
    }
}
)NS",
        "requires a reference type");

    // LA REGLA DE CONSTRAINT PRIMARIO, en sus tres formas. Ninguna se rechazaba: el parser admite
    // cualquier combinacion --no sabe cual de los nombres es una clase-- y la validacion solo
    // miraba que cada uno fuera clase o interfaz, nunca cuantos ni en que orden.
    checkRejected(
        tests, root, "cc1-limit007-class-y-clase-base",
        R"NS(
namespace limit007primario
{
    class Base { public int Marca() { return 3; } }
    class Caja<T> where T : class, Base
    {
        public int Medir(T v) { return 0; }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "are both primary constraints");

    checkRejected(
        tests, root, "cc1-limit007-dos-clases-base",
        R"NS(
namespace limit007dosbases
{
    class Base1 { public int A() { return 1; } }
    class Base2 { public int B() { return 2; } }
    class Caja<T> where T : Base1, Base2
    {
        public int Medir(T v) { return 0; }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "cannot be constrained by two base classes");

    checkRejected(
        tests, root, "cc1-limit007-clase-base-detras-de-interfaz",
        R"NS(
namespace limit007orden
{
    interface IComponente { int Peso(); }
    class Base { public int Marca() { return 3; } }
    class Caja<T> where T : IComponente, Base
    {
        public int Medir(T v) { return 0; }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "must be the first constraint of the 'where' clause");

    // NSC-LIMIT-009. `protected` sobre un anidado, visto desde un tipo que NO deriva del
    // contenedor. Su gemelo positivo --la derivada, y el propio contenedor-- vive en la fixture
    // c45, codigos 14 y 15.
    checkRejected(
        tests, root, "cc1-limit009-anidado-protected-desde-fuera",
        R"NS(
namespace limit009protegido
{
    class Base { protected class Reservada { public int Codigo() { return 40; } } }
    class Ajena
    {
        public int Usar() { Base.Reservada r = new Base.Reservada(); return r.Codigo(); }
    }
    class P { public static int Main() { return 42; } }
}
)NS",
        "is protected and is not accessible here");

    // NSC-LIMIT-007, CUARTA VUELTA. El caso que la regla por CUENTA DE ARGUMENTOS no separaba:
    // dos sobrecargas que toman lo mismo --uno-- y difieren en el TIPO. `F<T>(T[] x)` no tiene
    // constraint y no puede atender `F<int>(1)`; la unica que puede lo incumple. La decision ya
    // no la toma el modelo de constraints, que no conoce los tipos: la toma la resolucion de
    // sobrecargas, y por eso este programa se rechaza.
    checkRejected(
        tests, root, "cc1-limit007-sobrecarga-mismo-conteo-tipos-distintos",
        R"NS(
namespace limit007tipos
{
    class P
    {
        private static int F<T>(T x) where T : class { return 1; }
        private static int F<T>(T[] x) { return 2; }
        public static int Main() { return F<int>(1); }
    }
}
)NS",
        "requires a reference type");

    // Y SU CONTROL POSITIVO, sin el cual «se rechaza» sería indistinguible de «se rechaza
    // siempre»: con un argumento de tipo arreglo la llamada resuelve a la sobrecarga sin
    // constraint, y el programa es válido.
    checkAccepted(
        tests, root, "cc1-limit007-sobrecarga-arreglo-se-selecciona",
        R"NS(
namespace limit007tiposok
{
    class P
    {
        private static int F<T>(T x) where T : class { return 1; }
        private static int F<T>(T[] x) { return 2; }
        public static int Main()
        {
            int[] values = new int[1];
            if (F<int>(values) != 2) { return 1; }
            return 42;
        }
    }
}
)NS");

    // NSC-LIMIT-009. El anidado protegido alcanzado SIN escribir un tipo: el receptor de
    // `Base.Reservada.Codigo()` es una expresion, no un TypeSyntax, de modo que producia un
    // ExpressionTypeNameBinding -- que la comprobacion diferida no recorria. Un tipo ajeno
    // llegaba al anidado protegido con solo llamar a un miembro estatico.
    checkRejected(
        tests, root, "cc1-limit009-protected-por-receptor-estatico",
        R"NS(
namespace limit009receptor
{
    class Base { protected class Reservada { public static int Codigo() { return 40; } } }
    class Ajena { public int Usar() { return Base.Reservada.Codigo(); } }
    class P { public static int Main() { return 42; } }
}
)NS",
        "is protected and is not accessible here");

    checkAccepted(
        tests, root, "cc1-limit009-protected-receptor-estatico-desde-derivada",
        R"NS(
namespace limit009receptorok
{
    class Base { protected class Reservada { public static int Codigo() { return 42; } } }
    class Derivada : Base { public int Usar() { return Base.Reservada.Codigo(); } }
    class P { public static int Main() { Derivada d = new Derivada(); return d.Usar(); } }
}
)NS");

    // Y el positivo que la revision pedia: dos plantillas con el mismo nombre y la misma aridad se
    // materializan las dos, y que UNA incumpla no dice nada del programa -- la llamada resuelve a
    // la otra. Antes se rechazaba por la candidata que nadie eligio.
    checkAccepted(
        tests, root, "cc1-limit007-sobrecarga-descartada-no-es-error",
        R"NS(
namespace limit007sobrecarga
{
    interface IComponente { int Peso(); }
    interface IPesado : IComponente { int Extra(); }
    class Pieza : IComponente { public int Peso() { return 7; } }
    class P
    {
        private static int Elegir<T>(T unica) where T : IComponente { return unica.Peso(); }
        private static int Elegir<T>(T primera, T segunda) where T : IPesado { return 2; }
        public static int Main()
        {
            Pieza p = new Pieza();
            if (Elegir<Pieza>(p) != 7) { return 1; }
            return 42;
        }
    }
}
)NS");

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

    // =====================================================================================
    // `size(N)` -- el contrato de layout de una struct, para bindings.
    //
    // La mitad positiva vive en tests/cc1/c37_contrato_de_layout, con once structs cuyos tamaños
    // salieron de compilar las EQUIVALENTES EN C con clang. Aquí van los tres rechazos, que son
    // los que demuestran que el modificador SIRVE para algo: uno que no comprueba nada nunca falla.
    // =====================================================================================

    // EL CASO QUE JUSTIFICA LA FUNCIÓN. `byte` + `int` miden 8 por el relleno, no 5. Un binding
    // escrito a ojo pone 5, y sin esto compilaría y leería basura desde la primera llamada.
    checkRejected(
        tests, root, "cc1-size-mismatch",
        R"NS(
namespace cc1sizebad
{
    public size(5) struct R
    {
        public byte Bandera;
        public int Valor;
    }

    class P
    {
        public static int Main() { return 42; }
    }
}
)NS",
        "declares size(5) but its fields lay out to 8 bytes");

    // `size` es un contrato de LAYOUT, y una clase no tiene layout de valor: es un objeto de
    // montículo con su propia cabecera. Aceptarlo lo convertiría en una decoración que nadie
    // honra, que es justo lo que `pack`/`align` ya rechazan por el mismo motivo.
    checkRejected(
        tests, root, "cc1-size-on-class",
        R"NS(
namespace cc1sizeclass
{
    // El nombre NO puede ser `C`: es palabra reservada (`extern C`), y el parser rechazaria la
    // declaracion por el nombre ANTES de llegar a comprobar el modificador de layout. La prueba
    // seguiria viendo un rechazo --y pasando por rechazo-- pero midiendo otra cosa.
    public size(8) class Caja
    {
        public int Valor;
    }

    class P
    {
        public static int Main() { return 42; }
    }
}
)NS",
        "layout modifiers apply only to struct and union declarations");

    // `size(0)` casi siempre es un generador que no supo leer el tamaño y escribió su valor por
    // omisión. Una struct de cero bytes no existe en el modelo de objetos de Windows x64.
    checkRejected(
        tests, root, "cc1-size-zero",
        R"NS(
namespace cc1sizezero
{
    public size(0) struct S
    {
        public int Valor;
    }

    class P
    {
        public static int Main() { return 42; }
    }
}
)NS",
        "size requires a positive byte count");

    // =====================================================================================
    // El tipo subyacente de un enum -- los cuatro rechazos.
    //
    // La mitad positiva vive en tests/cc1/c38_enum_ancho. Estos cuatro son los que demuestran que
    // declarar el tipo subyacente SIRVE: si el compilador aceptara un valor que no cabe, lo
    // truncaria al materializarlo y el programa veria un numero que nadie escribio.
    // =====================================================================================

    // 300 no cabe en un byte. Truncado seria 44, y nada en el programa lo diria.
    checkRejected(
        tests, root, "cc1-enum-underlying-overflow",
        R"NS(
namespace cc1enumover
{
    public enum E : byte { A = 300 }

    class P
    {
        public static int Main() { return (int)E.A; }
    }
}
)NS",
        "does not fit in the underlying type 'builtin:byte'");

    // EL AUTO-INCREMENTO TAMBIEN SE SALE, y este es el caso que nadie ve: 255 cabe, y `B` no lleva
    // ningun numero escrito que mirar. Comprobar solo los valores explicitos dejaria pasar
    // justamente esto.
    checkRejected(
        tests, root, "cc1-enum-underlying-overflow-implicit",
        R"NS(
namespace cc1enumimpl
{
    public enum E : byte { A = 255, B }

    class P
    {
        public static int Main() { return (int)E.B; }
    }
}
)NS",
        "enum value for 'B' does not fit in the underlying type 'builtin:byte'");

    // NO HAY SUCESOR DE 2^64-1. Un enumerador sin inicializador detras del maximo del tipo
    // subyacente no tiene ningun valor que tomar, y darle uno seria inventarlo.
    //
    // La primera version del manejo de extremos SATURABA aqui: devolvia el mismo valor, `Siguiente`
    // valia lo mismo que `Max`, y compilaba. Dos nombres indistinguibles y un valor que nadie
    // escribio -- una respuesta falsa, que es peor que el rechazo que faltaba.
    checkRejected(
        tests, root, "cc1-enum-underlying-no-successor",
        R"NS(
namespace cc1enumnosucc
{
    public enum E : ulong { Max = 18446744073709551615, Siguiente }

    class P
    {
        public static int Main() { return 0; }
    }
}
)NS",
        "there is no next one to count to");

    // El tipo subyacente tiene que ser un entero incorporado. Ni `bool`, ni coma flotante, ni una
    // clase: el ancho y el signo de un enum son parte del ABI y no hay respuesta para esos.
    checkRejected(
        tests, root, "cc1-enum-underlying-not-integer",
        R"NS(
namespace cc1enumnotint
{
    public enum E : string { A = 0 }

    class P
    {
        public static int Main() { return (int)E.A; }
    }
}
)NS",
        "underlying type of an enum must be a built-in integer type");

    // `char` ESTA EN LA TABLA GENERAL DE ENTEROS Y AUN ASI NO VALE. En NativeSharp `char` es un
    // byte de TEXTO (ocho bits, sin signo, CC1-100) y esta en la tabla porque aritmeticamente se
    // comporta como uno; pero el tipo subyacente de un enum es una decision de ABI, y "el ancho de
    // un caracter" no responde a eso. `byte` dice lo mismo sin ambiguedad, y C# tampoco lo admite.
    // Esta prueba existe porque el diagnostico ENUMERA los tipos permitidos: si `char` se colara,
    // el mensaje estaria mintiendo sobre su propia lista.
    checkRejected(
        tests, root, "cc1-enum-underlying-char",
        R"NS(
namespace cc1enumchar
{
    public enum E : char { A = 0 }

    class P
    {
        public static int Main() { return (int)E.A; }
    }
}
)NS",
        "underlying type of an enum must be a built-in integer type");

    // Y SE MIRA LA FORMA, NO EL NOMBRE. `int*` tiene "int" de nombre base: una comprobacion escrita
    // sobre el nombre lo aceptaria y el enum acabaria midiendo cuatro bytes en vez de ocho.
    checkRejected(
        tests, root, "cc1-enum-underlying-pointer",
        R"NS(
namespace cc1enumptr
{
    public enum E : int* { A = 0 }

    class P
    {
        public static int Main() { return (int)E.A; }
    }
}
)NS",
        "underlying type of an enum must be a built-in integer type");

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

    // ID 126, DESDE EL LENGUAJE. La aritmetica sobre tipos estrechos ocurre en `int` -- promocion
    // integral, como en C y en C++ -- y guardarla de vuelta en el tipo estrecho pide el cast
    // escrito. Es lo mismo que hace C#, y lo que la biblioteca de este proyecto ya escribia a mano
    // desde el principio: `return (char)((int)value + 32);` en String.ns.
    checkAccepted(
        tests, root, "cc1-narrow-arithmetic-promotes-to-int",
        R"NS(
namespace cc1promo
{
    class P
    {
        public static int Main()
        {
            short s = 10;
            int wide = s + 3;
            short t = (short)(s + 3);
            byte b = 5;
            byte c = (byte)(b + 1);
            short back = (short)(s - 3);
            return wide + (int)t + (int)c + (int)back;
        }
    }
}
)NS");

    // Y LA MITAD NEGATIVA, que es donde estaba el defecto: sin el cast NO compila. Antes del ID 126
    // esta linea compilaba y el backend emitia un `add i8` que ENVUELVE -- 200+100 daba 44, sin un
    // solo diagnostico. Es la misma clase de perdida silenciosa que el ID 81 ya rechazaba en
    // `byte b; int n; b += n;`, aceptada aqui por una regla distinta en el mismo lenguaje.
    checkRejected(
        tests, root, "cc1-narrow-sum-needs-a-written-cast",
        R"NS(
namespace cc1narrowsum
{
    class P
    {
        public static int Main()
        {
            byte a = 200;
            byte b = 100;
            byte wraps = a + b;
            return (int)wraps;
        }
    }
}
)NS",
        "cannot initialize local 'wraps'");

    // DOS ENTEROS NUNCA DAN `double`. `long + ulong` no tiene tipo comun -- no hay ensanchamiento
    // long->ulong -- y ahora se dice. Hasta el ID 126 caia al bucle de preferencia, cuya primera
    // parada es `double`, y el programa compilaba computando en punto flotante: el plegado de
    // constantes se rendia y por encima de 2^53 el resultado dejaba de ser la suma.
    checkRejected(
        tests, root, "cc1-signed-and-unsigned-64-have-no-common-type",
        R"NS(
namespace cc1mix64
{
    class P
    {
        public static int Main()
        {
            long signedWide = 1;
            ulong unsignedWide = 2;
            long total = signedWide + (long)unsignedWide;
            ulong bad = signedWide + unsignedWide;
            return (int)total + (int)bad;
        }
    }
}
)NS",
        "compatible numeric operands");

    // LO QUE NO SE PUEDE ROMPER AL ARREGLAR LO ANTERIOR. `nuint` y `nint` son el ancho del puntero
    // y su aritmetica tiene que seguir siendo suya: colapsarlos a `ulong`/`long` haria que
    // `nuint total = size * 2;` dejara de compilar, porque la vuelta `ulong -> nuint` no ensancha.
    // C++ contesta lo mismo -- `size_t + int` es `size_t`, no `unsigned long`.
    checkAccepted(
        tests, root, "cc1-native-width-arithmetic-stays-native",
        R"NS(
namespace cc1native
{
    class P
    {
        public static int Main()
        {
            nuint size = sizeof(int);
            nuint total = size * 2;
            nuint plusOne = total + 1;
            nint offset = 3;
            nint stepped = offset + 1;
            if (total != 8) { return 1; }
            if (plusOne != 9) { return 2; }
            if (stepped != 4) { return 3; }
            return 0;
        }
    }
}
)NS");

    // IDs 80/83 -- el limite de `->`, con SU mensaje.
    //
    // `nsc.project-member-model` comprueba que esto se rechaza y con que DiagnosticId; aqui se
    // comprueba QUE DICE. Los dos hacen falta: al desazucarar `->` en el parser, la comprobacion
    // vieja quedo muerta y `this->Value` paso a compilar -- solo se vio porque una prueba medía
    // esa fase. Este test mide la otra mitad, el texto, que es lo que la persona lee.
    checkRejected(
        tests, root, "cc1-arrow-on-class-reference",
        R"NS(
namespace cc1arrowthis
{
    class Box
    {
        int Value;
        public int Run() { return this->Value; }
        public static int Main() { return 0; }
    }
}
)NS",
        "'this' is a class reference; use '.' rather than '->' for member lookup");

    // Y la forma general: `->` sobre algo que no es un puntero nombra `->`, no un `*` que la
    // persona nunca escribio.
    checkRejected(
        tests, root, "cc1-arrow-on-non-pointer",
        R"NS(
namespace cc1arrownonptr
{
    public struct Cell { public int Value; }
    class P
    {
        public static int Main()
        {
            Cell c;
            c.Value = 1;
            return c->Value;
        }
    }
}
)NS",
        "left side of '->' must be a non-void pointer");

    // =====================================================================================
    // ID 123 -- las reservas IMPOSIBLES. La mitad que no cabe en una fixture positiva.
    //
    // Cada una comprueba que falla Y POR QUE falla. Solo "falla" pasaria tambien con una errata
    // en la propia prueba, que es como un test negativo deja de medir algo sin que nadie se
    // entere.
    // =====================================================================================

    // Contador constante negativo. Se puede comprobar desde que el ID 119 cerro la constancia de
    // los literales negativos: antes, `-1` ni siquiera llegaba al analisis como constante.
    checkRejected(
        tests, root, "cc1-stackalloc-negative-count",
        R"NS(
namespace cc1sanegative
{
    class P
    {
        public static int Main()
        {
            int* buffer = stackalloc int[-1];
            return (int)buffer[0];
        }
    }
}
)NS",
        "stackalloc element count is negative");

    // La misma forma escrita como constante plegada, para que la comprobacion no dependa de que
    // el contador sea un literal suelto. Se apoya en el ID 121: `2 - 5` se pliega.
    checkRejected(
        tests, root, "cc1-stackalloc-negative-folded",
        R"NS(
namespace cc1sanegfold
{
    class P
    {
        public static int Main()
        {
            byte* buffer = stackalloc byte[2 - 5];
            return (int)buffer[0];
        }
    }
}
)NS",
        "stackalloc element count is negative");

    // La reserva que no cabe. Un gigabyte en una pila cuya reserva por omision es un mega.
    checkRejected(
        tests, root, "cc1-stackalloc-too-large",
        R"NS(
namespace cc1satoobig
{
    class P
    {
        public static int Main()
        {
            byte* buffer = stackalloc byte[1073741824];
            return (int)buffer[0];
        }
    }
}
)NS",
        "does not fit on the stack");

    // El caso que la multiplicacion ingenua deja pasar: `1 << 62` elementos de 4 bytes. El
    // producto envuelve a 0 en 64 bits, asi que `elements * elementSize > limite` seria FALSO y
    // la reserva pasaria. La comprobacion divide el limite entre el tamano justamente para que
    // este caso no exista.
    checkRejected(
        tests, root, "cc1-stackalloc-count-overflows-product",
        R"NS(
namespace cc1saoverflow
{
    class P
    {
        public static int Main()
        {
            int* buffer = stackalloc int[4611686018427387904L];
            return buffer[0];
        }
    }
}
)NS",
        "does not fit on the stack");

    // Y la otra mitad: una reserva grande PERO POSIBLE sigue compilando. Si el limite se
    // escribiera demasiado estrecho, esto dejaria de compilar y el compilador estaria prohibiendo
    // codigo correcto. 65536 bytes son un dieciseisavo del limite.
    checkAccepted(
        tests, root, "cc1-stackalloc-large-but-legal",
        R"NS(
namespace cc1salegal
{
    class P
    {
        public static int Main()
        {
            byte* buffer = stackalloc byte[65536];
            buffer[0] = 1;
            return (int)buffer[0];
        }
    }
}
)NS");

    // =====================================================================================
    // ID 89 -- `sbyte`. Los limites del tipo, medidos por los dos lados.
    // =====================================================================================

    // 200 no cabe en un entero de 8 bits CON signo. Si `constantFits` hubiera copiado la rama de
    // `byte`, esto compilaria y el valor seria -56 en silencio.
    checkRejected(
        tests, root, "cc1-sbyte-constant-out-of-range",
        R"NS(
namespace cc1sbyterange
{
    class P
    {
        public static int Main()
        {
            sbyte s = 200;
            return (int)s;
        }
    }
}
)NS",
        "cannot initialize local 's'");

    // Y por abajo: -129 tampoco. El minimo es -128.
    checkRejected(
        tests, root, "cc1-sbyte-constant-below-minimum",
        R"NS(
namespace cc1sbytelow
{
    class P
    {
        public static int Main()
        {
            sbyte s = -129;
            return (int)s;
        }
    }
}
)NS",
        "cannot initialize local 's'");

    // `sbyte` NO ensancha a un destino sin signo: sus valores negativos no caben ahi. Si la tabla
    // de ensanchamientos lo hubiera incluido, `uint u = s;` compilaria y envolveria en silencio.
    checkRejected(
        tests, root, "cc1-sbyte-does-not-widen-to-unsigned",
        R"NS(
namespace cc1sbyteunsigned
{
    class P
    {
        public static int Main()
        {
            sbyte s = -1;
            uint u = s;
            return (int)u;
        }
    }
}
)NS",
        "cannot initialize local 'u'");

    // LA RAZON DE SER DEL TIPO: declarar `signed char` en una API de C. Si `sbyte` estuviera en
    // todas las tablas de ancho y de signo pero no en la lista de tipos que cruzan la ABI de C,
    // el tipo no serviria para lo unico que se pidio.
    checkAccepted(
        tests, root, "cc1-sbyte-crosses-the-c-abi",
        R"NS(
namespace cc1sbyteabi
{
    class P
    {
        public static export extern C sbyte Clamp(sbyte value)
        {
            if (value < -100) { return -100; }
            if (value > 100) { return 100; }
            return value;
        }

        public static int Main()
        {
            if ((int)Clamp(-128) != -100) { return 1; }
            if ((int)Clamp(7) != 7) { return 2; }
            return 0;
        }
    }
}
)NS");

    // =====================================================================================
    // ID 82 -- asignacion compuesta sobre propiedad e indexador: la MITAD NEGATIVA.
    //
    // La fixture c20 mide que la forma funciona y que el getter, el setter y el lado derecho
    // corren una vez cada uno. Lo que no cabe en una fixture --que tiene que compilar entera-- es
    // lo que el compilador tiene que RECHAZAR. Y es donde esta el riesgo real: aceptar de mas
    // aqui no da un error, da un programa que evalua algo dos veces en silencio.
    // =====================================================================================

    // El receptor es una LLAMADA. `Get().Value += 1` tendria que llamar a Get() una vez y usar el
    // mismo objeto para el getter y para el setter; bajarlo dos veces lo llamaria dos veces, y con
    // un Get() que tenga efectos --o que devuelva un objeto distinto cada vez-- el incremento se
    // perderia. Es exactamente la propiedad que la fixture c14 mide para `buffer[Probe()] += 1`.
    checkRejected(
        tests, root, "cc1-compound-property-receiver-is-a-call",
        R"NS(
namespace cc1compoundrecv
{
    class Box
    {
        private int slot;
        public Box() { slot = 0; }
        public int Value { get { return slot; } set { slot = value; } }
    }

    class P
    {
        static Box shared;
        static Box Get() { return shared; }
        public static int Main()
        {
            Get().Value += 1;
            return 0;
        }
    }
}
)NS",
        "assign this one to a local first");

    // El INDICE es una llamada. La misma regla por el otro lado: `s[Probe()] += 1` bajaria Probe()
    // una vez para el getter y otra para el setter.
    checkRejected(
        tests, root, "cc1-compound-indexer-index-is-a-call",
        R"NS(
namespace cc1compoundidx
{
    class Slots
    {
        private int a;
        public Slots() { a = 0; }
        public int this[int index] { get { return a; } set { a = value; } }
    }

    class P
    {
        static int Probe() { return 0; }
        public static int Main()
        {
            raii Slots s = new Slots();
            s[Probe()] += 1;
            return 0;
        }
    }
}
)NS",
        "assign this one to a local first");

    // El receptor es un CAMPO. Parece inofensivo y no lo es: el getter corre entre las dos
    // lecturas del campo y si alcanza los campos del objeto, asi que podria reescribirlo. C#
    // evalua el receptor una vez y usa el valor viejo; aceptarlo aqui seria divergir en silencio.
    checkRejected(
        tests, root, "cc1-compound-property-receiver-is-a-field",
        R"NS(
namespace cc1compoundfield
{
    class Box
    {
        private int slot;
        public Box() { slot = 0; }
        public int Value { get { return slot; } set { slot = value; } }
    }

    class Outer
    {
        public Box inner;
        public Outer() { inner = new Box(); }
        public void Bump() { inner.Value += 1; }
    }

    class P
    {
        public static int Main() { return 0; }
    }
}
)NS",
        "assign this one to a local first");

    // Y la regla ARITMETICA, que es la misma que la de un destino con direccion y no una segunda:
    // el lado derecho tiene que convertir implicitamente al tipo del destino. `byte` con un `int`
    // suelto por la derecha se rechaza y pide el cast escrito. Es la divergencia deliberada
    // respecto de C# que escribio el ID 81; una propiedad no puede ser su excepcion.
    checkRejected(
        tests, root, "cc1-compound-property-narrowing-rhs",
        R"NS(
namespace cc1compoundnarrow
{
    class Box
    {
        private byte slot;
        public Box() { slot = 0; }
        public byte Narrow { get { return slot; } set { slot = value; } }
    }

    class P
    {
        public static int Main()
        {
            raii Box b = new Box();
            int wide = 5;
            b.Narrow += wide;
            return 0;
        }
    }
}
)NS",
        "write the read-modify-write with an explicit cast");

    // =====================================================================================
    // SHR-005 -- el residuo de "owned destination", y era el ARREGLO FIJO.
    //
    // Las dos admisiones --fuente y destino-- decidian con `isNativeArray`, que compara el sufijo
    // "[]". Un arreglo fijo termina en "n]", asi que no era ninguna de las dos. Las destrucciones
    // se cuentan en Stage50_11OwnershipExecutionTests, que es como este proyecto verifica
    // ownership; aqui se mide solo que las dos formas COMPILAN, que es lo que no hacian.
    // =====================================================================================
    checkAccepted(
        tests, root, "cc1-fixed-array-slot-is-an-ownership-destination",
        R"NS(
namespace NativeSharp.Core { public class Exception { public string Message; } }

namespace cc1fixedowned
{
    class Resource
    {
        private int tag;
        public Resource(int initial) { tag = initial; }
        public int Tag() { return tag; }
    }

    class P
    {
        public static int Main()
        {
            raii Resource first = new Resource(7);
            Resource[2] slots;
            slots[0] = null;
            slots[1] = null;
            slots[0] = move(first);
            raii Resource taken = move(slots[0]);
            return taken.Tag() - 7;
        }
    }
}
)NS");

    // =====================================================================================
    // IDs 121 y 122 -- la aritmetica de constantes enteras.
    //
    // Esta seccion mide `commonNumericType` LLAMANDOLA, que es como se midio el ID 120 y como se
    // descubrio que su primera version rompia `(-1) + 1L`. Una fixture de ejecucion no puede
    // aislar esto: dice si el programa da 42, no que tipo eligio el frontend por el camino.
    // =====================================================================================
    {
        const auto magnitude = [](std::uint64_t value, bool isUnsigned, bool isNegative)
        {
            return ProjectIntegerConstant{value, isUnsigned, isNegative};
        };
        const auto common = [](std::string_view left,
                               std::string_view right,
                               std::optional<ProjectIntegerConstant> leftConstant,
                               std::optional<ProjectIntegerConstant> rightConstant)
        {
            const std::optional<std::string> result = ProjectTypeConversion::commonNumericType(
                left, right, leftConstant, rightConstant);
            return result.has_value() ? *result : std::string("<none>");
        };

        // =================================================================================
        // ID 126 -- LA TABLA ENTERA. Los 91 pares de los trece tipos numericos, sin constantes.
        //
        // POR QUE LA TABLA COMPLETA Y NO UNA MUESTRA. Antes del ID 126 esta funcion no tenia una
        // especificacion: tenia tres reglas generales --cortocircuito de tipos iguales, "si solo
        // una conversion existe gana ese tipo", y un bucle de preferencia-- aplicadas en un orden
        // que nadie eligio para los enteros. El resultado era un mapa con TRES clases de respuesta
        // equivocada, y ninguna se veia mirando el codigo: hubo que evaluar los 91 pares para que
        // aparecieran. Doc 89.
        //
        // Ahora la tabla ES la especificacion. Cualquier cambio en las tablas de ancho, de signo o
        // de ensanchamiento que mueva una sola de estas respuestas falla aqui, con el par escrito.
        //
        // LO QUE FIJA, en tres afirmaciones:
        //
        //   * los cinco tipos MENORES QUE INT promueven a `int` -- promocion integral de C y C++,
        //     y ningun `add i8` que envuelva en silencio;
        //   * DOS ENTEROS NUNCA DAN `double` -- los diez pares que mezclan 64 con signo y 64 sin
        //     signo no tienen tipo comun, y eso es lo que se dice;
        //   * `nint`/`nuint` conservan su nombre cuando el otro operando llega a ellos, y se caen
        //     a `long`/`ulong` cuando no. `uint + nint` sigue dando `long`.
        //
        // Donde C++ y C# no coinciden mandan las respuestas de C#, y es un solo sitio: mezclar con
        // signo y sin signo del MISMO rango. C++ colapsa al sin signo (`int + uint` es `uint`, y
        // por eso `-1 < 1u` es false); aqui da `long`.
        // =================================================================================
        struct CommonTypeCase
        {
            std::string_view left;
            std::string_view right;
            std::string_view expected;
        };
        constexpr std::array<CommonTypeCase, 91> kCommonTypeTable = {{
        {"builtin:sbyte", "builtin:sbyte", "builtin:int"},
        {"builtin:sbyte", "builtin:byte", "builtin:int"},
        {"builtin:sbyte", "builtin:char", "builtin:int"},
        {"builtin:sbyte", "builtin:short", "builtin:int"},
        {"builtin:sbyte", "builtin:ushort", "builtin:int"},
        {"builtin:sbyte", "builtin:int", "builtin:int"},
        {"builtin:sbyte", "builtin:uint", "builtin:long"},
        {"builtin:sbyte", "builtin:long", "builtin:long"},
        {"builtin:sbyte", "builtin:ulong", "<none>"},
        {"builtin:sbyte", "builtin:nint", "builtin:nint"},
        {"builtin:sbyte", "builtin:nuint", "<none>"},
        {"builtin:sbyte", "builtin:float", "builtin:float"},
        {"builtin:sbyte", "builtin:double", "builtin:double"},
        {"builtin:byte", "builtin:byte", "builtin:int"},
        {"builtin:byte", "builtin:char", "builtin:int"},
        {"builtin:byte", "builtin:short", "builtin:int"},
        {"builtin:byte", "builtin:ushort", "builtin:int"},
        {"builtin:byte", "builtin:int", "builtin:int"},
        {"builtin:byte", "builtin:uint", "builtin:uint"},
        {"builtin:byte", "builtin:long", "builtin:long"},
        {"builtin:byte", "builtin:ulong", "builtin:ulong"},
        {"builtin:byte", "builtin:nint", "builtin:nint"},
        {"builtin:byte", "builtin:nuint", "builtin:nuint"},
        {"builtin:byte", "builtin:float", "builtin:float"},
        {"builtin:byte", "builtin:double", "builtin:double"},
        {"builtin:char", "builtin:char", "builtin:int"},
        {"builtin:char", "builtin:short", "builtin:int"},
        {"builtin:char", "builtin:ushort", "builtin:int"},
        {"builtin:char", "builtin:int", "builtin:int"},
        {"builtin:char", "builtin:uint", "builtin:uint"},
        {"builtin:char", "builtin:long", "builtin:long"},
        {"builtin:char", "builtin:ulong", "builtin:ulong"},
        {"builtin:char", "builtin:nint", "builtin:nint"},
        {"builtin:char", "builtin:nuint", "builtin:nuint"},
        {"builtin:char", "builtin:float", "builtin:float"},
        {"builtin:char", "builtin:double", "builtin:double"},
        {"builtin:short", "builtin:short", "builtin:int"},
        {"builtin:short", "builtin:ushort", "builtin:int"},
        {"builtin:short", "builtin:int", "builtin:int"},
        {"builtin:short", "builtin:uint", "builtin:long"},
        {"builtin:short", "builtin:long", "builtin:long"},
        {"builtin:short", "builtin:ulong", "<none>"},
        {"builtin:short", "builtin:nint", "builtin:nint"},
        {"builtin:short", "builtin:nuint", "<none>"},
        {"builtin:short", "builtin:float", "builtin:float"},
        {"builtin:short", "builtin:double", "builtin:double"},
        {"builtin:ushort", "builtin:ushort", "builtin:int"},
        {"builtin:ushort", "builtin:int", "builtin:int"},
        {"builtin:ushort", "builtin:uint", "builtin:uint"},
        {"builtin:ushort", "builtin:long", "builtin:long"},
        {"builtin:ushort", "builtin:ulong", "builtin:ulong"},
        {"builtin:ushort", "builtin:nint", "builtin:nint"},
        {"builtin:ushort", "builtin:nuint", "builtin:nuint"},
        {"builtin:ushort", "builtin:float", "builtin:float"},
        {"builtin:ushort", "builtin:double", "builtin:double"},
        {"builtin:int", "builtin:int", "builtin:int"},
        {"builtin:int", "builtin:uint", "builtin:long"},
        {"builtin:int", "builtin:long", "builtin:long"},
        {"builtin:int", "builtin:ulong", "<none>"},
        {"builtin:int", "builtin:nint", "builtin:nint"},
        {"builtin:int", "builtin:nuint", "<none>"},
        {"builtin:int", "builtin:float", "builtin:float"},
        {"builtin:int", "builtin:double", "builtin:double"},
        {"builtin:uint", "builtin:uint", "builtin:uint"},
        {"builtin:uint", "builtin:long", "builtin:long"},
        {"builtin:uint", "builtin:ulong", "builtin:ulong"},
        {"builtin:uint", "builtin:nint", "builtin:long"},
        {"builtin:uint", "builtin:nuint", "builtin:nuint"},
        {"builtin:uint", "builtin:float", "builtin:float"},
        {"builtin:uint", "builtin:double", "builtin:double"},
        {"builtin:long", "builtin:long", "builtin:long"},
        {"builtin:long", "builtin:ulong", "<none>"},
        {"builtin:long", "builtin:nint", "builtin:long"},
        {"builtin:long", "builtin:nuint", "<none>"},
        {"builtin:long", "builtin:float", "builtin:float"},
        {"builtin:long", "builtin:double", "builtin:double"},
        {"builtin:ulong", "builtin:ulong", "builtin:ulong"},
        {"builtin:ulong", "builtin:nint", "<none>"},
        {"builtin:ulong", "builtin:nuint", "builtin:ulong"},
        {"builtin:ulong", "builtin:float", "builtin:float"},
        {"builtin:ulong", "builtin:double", "builtin:double"},
        {"builtin:nint", "builtin:nint", "builtin:nint"},
        {"builtin:nint", "builtin:nuint", "<none>"},
        {"builtin:nint", "builtin:float", "builtin:float"},
        {"builtin:nint", "builtin:double", "builtin:double"},
        {"builtin:nuint", "builtin:nuint", "builtin:nuint"},
        {"builtin:nuint", "builtin:float", "builtin:float"},
        {"builtin:nuint", "builtin:double", "builtin:double"},
        {"builtin:float", "builtin:float", "builtin:float"},
        {"builtin:float", "builtin:double", "builtin:double"},
        {"builtin:double", "builtin:double", "builtin:double"},
        }};
        for (const CommonTypeCase& entry : kCommonTypeTable)
        {
            const std::string got = common(entry.left, entry.right, std::nullopt, std::nullopt);
            tests.expect(
                got == entry.expected,
                "126 table: " + std::string(entry.left) + " + " + std::string(entry.right) +
                    " -> " + std::string(entry.expected) + " (got " + got + ")");
            // La funcion es SIMETRICA y tiene que seguir siendolo: `a + b` y `b + a` no pueden
            // diferir, y con tres reglas ordenadas por conversiones unidireccionales eso no es
            // gratis -- hay que comprobarlo.
            const std::string swapped = common(entry.right, entry.left, std::nullopt, std::nullopt);
            tests.expect(
                swapped == entry.expected,
                "126 table (simetria): " + std::string(entry.right) + " + " + std::string(entry.left) +
                    " -> " + std::string(entry.expected) + " (got " + swapped + ")");
        }

        // LA CONSTANTE `int` SE ADAPTA AL OTRO OPERANDO, PERO SOLO HACIA ARRIBA.
        //
        // Estas seis lineas existen porque la primera version del ID 126 rompio `lib/`. Al retirar
        // la regla del ID 120 se fue con ella la adaptacion HACIA TIPOS ANCHOS, que no es la misma
        // cosa: `uint hash; hash = hash * 16777619;` pasaba a evaluarse en `long` --que es la
        // respuesta correcta para dos operandos de verdad, porque ni `uint` ni `int` contienen al
        // otro-- y `long` no vuelve a `uint`. String.ns:980 dejo de compilar y con el, TODA la
        // biblioteca y las 52 fixtures que la referencian.
        //
        // Lo encontro la corrida. El barrido previo de `lib/` busco tipos ESTRECHOS y `uint` no es
        // uno: la medicion estaba mal hecha, no la conclusion.
        tests.expect(
            common("builtin:uint", "builtin:int", std::nullopt, magnitude(16777619, false, false)) ==
                "builtin:uint",
            "126: a fitting int constant adapts UP to a uint operand -- String.ns:980");
        tests.expect(
            common("builtin:ulong", "builtin:int", std::nullopt, magnitude(10, false, false)) ==
                "builtin:ulong",
            "126: and to a ulong operand -- `remaining % 10` in integer formatting");
        tests.expect(
            common("builtin:nuint", "builtin:int", std::nullopt, magnitude(2, false, false)) ==
                "builtin:nuint",
            "126: and to nuint -- `nuint total = size * 2`");
        // Y NO hacia abajo, que es justo lo que el ID 126 cierra: hacia un tipo estrecho la
        // adaptacion es lo que hacia que la operacion se computara a 8 o 16 bits y envolviera.
        tests.expect(
            common("builtin:byte", "builtin:int", std::nullopt, magnitude(1, false, false)) ==
                "builtin:int",
            "126: but NOT down to a narrow operand -- that is the wrap this closes");
        // Un negativo no cabe en un destino sin signo, asi que la regla no dispara y manda la
        // promocion: ni se adapta ni se envuelve en silencio.
        tests.expect(
            common("builtin:uint", "builtin:int", std::nullopt, magnitude(1, false, true)) ==
                "builtin:long",
            "126: a NEGATIVE constant does not adapt to uint -- both widen to long instead");

        // 121, el defecto directo. Antes: builtin:double, y el evaluador entero se rendia.
        tests.expect(
            common("builtin:int", "builtin:long", magnitude(1, false, false),
                   magnitude(1, false, false)) == "builtin:long",
            "121: int and long with constants give long, not double");

        // La parte sin signo. `long` contra `ulong` no tiene tipo comun en C#; con constantes que
        // caben, aqui la conversion existe en los dos sentidos y el tipo util es ulong.
        tests.expect(
            common("builtin:long", "builtin:ulong", magnitude(3, false, false),
                   magnitude(5, true, false)) == "builtin:ulong",
            "121: long and ulong constants give ulong");

        // Y sin constantes NO se inventa: no hay ensanchamiento long->ulong, la guarda falla, y
        // ahora eso es lo que se dice -- NO HAY TIPO COMUN. Hasta el ID 126 esto caia al bucle de
        // preferencia y contestaba `double`: dos enteros de 64 bits computados en punto flotante,
        // con el plegado de constantes rindiendose y el resultado dejando de ser la suma por
        // encima de 2^53. Es el mismo par que C# prohibe.
        tests.expect(
            common("builtin:long", "builtin:ulong", std::nullopt, std::nullopt) == "<none>",
            "126: long and ulong without constants have NO common type -- never double");

        // Sin constantes de por medio: la promocion entera de C# para los pares que la necesitan.
        tests.expect(
            common("builtin:int", "builtin:uint", std::nullopt, std::nullopt) == "builtin:long",
            "121: int and uint give long -- neither side loses range");
        tests.expect(
            common("builtin:uint", "builtin:byte", std::nullopt, std::nullopt) == "builtin:uint",
            "121: uint and byte give uint");
        tests.expect(
            common("builtin:ushort", "builtin:short", std::nullopt, std::nullopt) == "builtin:int",
            "121: two narrow types of different signedness give int");

        // EL ID 120, RETIRADO POR EL ID 126. Su regla hacia que una constante literal adoptara el
        // tipo estrecho del otro operando, de modo que `short s; short t = s + 3;` compilaba. Era
        // una divergencia deliberada de C# --alli tampoco compila-- y mantenerla dejaba `b + b`
        // como error y `b + 1` truncando en silencio, que es la unica combinacion peor que no
        // arreglar nada. Ahora los dos dan `int` y el cast se escribe.
        tests.expect(
            common("builtin:short", "builtin:int", std::nullopt, magnitude(3, false, false)) ==
                "builtin:int",
            "126: a fitting int constant no longer drags the operation down to short");
        tests.expect(
            common("builtin:byte", "builtin:int", std::nullopt, magnitude(1, false, false)) ==
                "builtin:int",
            "126: nor down to byte");

        // Un literal escrito ANCHO no se estrecha: la otra mitad de la razon por la que la regla
        // del 120 se escribio estrecha a proposito.
        tests.expect(
            common("builtin:int", "builtin:long", magnitude(1, false, true),
                   magnitude(1, false, false)) == "builtin:long",
            "121: a negative int constant against 1L still gives long, not int");

        // 122 MEDIDO POR SU EFECTO. Con la representacion vieja el evaluador entregaba `bits` --
        // complemento a dos con el signo ya extendido -- en el campo documentado como magnitud.
        // Las dos llamadas son el MISMO -3 escrito de las dos maneras, y tienen que dar respuestas
        // distintas o el 122 no estaba cerrado.
        //
        // SE MIDE EN `classifyImplicit` Y NO EN `commonNumericType`, y el sitio cambio con el ID
        // 126: al retirarse la regla del 120, los dos pares dan ahora `int` y el par dejaba de
        // discriminar. La pregunta de fondo --si -3 cabe en un `short`-- vive en el clasificador,
        // que es donde `constantFits` decide, asi que este es ademas el sitio correcto.
        tests.expect(
            ProjectTypeConversion::classifyImplicit(
                "builtin:int", "builtin:short", magnitude(3, false, true)).exists(),
            "122: -3 as magnitude+flag fits in a short");
        tests.expect(
            !ProjectTypeConversion::classifyImplicit(
                "builtin:int", "builtin:short",
                magnitude(0xFFFFFFFFFFFFFFFDULL, false, false)).exists(),
            "122: the same value as raw two's complement bits does NOT -- that was the defect");

        // =================================================================================
        // ID 89 -- `sbyte` en el clasificador de conversiones, medido llamandolo.
        //
        // Un tipo nuevo tiene que entrar en TODAS las reglas que nombran a su familia, no solo en
        // las tablas de ancho y signo. La primera version de este cambio pasaba la fixture de
        // ejecucion y fallaba AQUI, en el caso (sbyte, const 3): `sbyte` no estaba en la lista de
        // destinos estrechos de la regla del ID 120 y el tipo comun salia `int`.
        // =================================================================================

        // La constante que cabe ya NO arrastra la operacion al tipo estrecho (ID 126): `sbyte` se
        // comporta igual que `short` y `byte`, que es lo que esta linea sigue midiendo.
        tests.expect(
            common("builtin:sbyte", "builtin:int", std::nullopt, magnitude(3, false, false)) ==
                "builtin:int",
            "126: an sbyte operand promotes to int, as short and byte do");

        // Sin constante de por medio, dos estrechos dan int -- tambien si uno tiene signo.
        tests.expect(
            common("builtin:sbyte", "builtin:byte", std::nullopt, std::nullopt) == "builtin:int",
            "89: sbyte and byte give int");
        tests.expect(
            common("builtin:sbyte", "builtin:uint", std::nullopt, std::nullopt) == "builtin:long",
            "89: sbyte and uint give long -- neither side loses range");

        // Los limites del tipo. -128 cabe y -129 no; 127 cabe y 128 no.
        const auto fitsInSByte = [&magnitude](std::uint64_t value, bool isNegative)
        {
            return ProjectTypeConversion::classifyImplicit(
                       "builtin:int", "builtin:sbyte", magnitude(value, false, isNegative))
                .exists();
        };
        tests.expect(fitsInSByte(127, false), "89: 127 fits in sbyte");
        tests.expect(!fitsInSByte(128, false), "89: 128 does NOT fit in sbyte");
        tests.expect(fitsInSByte(128, true), "89: -128 fits in sbyte -- the minimum is one more");
        tests.expect(!fitsInSByte(129, true), "89: -129 does NOT fit in sbyte");

        // Ensancha SOLO a destinos con signo. Meterlo en los sin signo haria que `uint u = s;`
        // compilara y envolviera en silencio.
        const auto widens = [](std::string_view from, std::string_view to)
        {
            return ProjectTypeConversion::classifyImplicit(from, to).exists();
        };
        tests.expect(
            widens("builtin:sbyte", "builtin:short") && widens("builtin:sbyte", "builtin:int") &&
                widens("builtin:sbyte", "builtin:long") &&
                widens("builtin:sbyte", "builtin:double"),
            "89: sbyte widens to the signed types and to floating point");
        tests.expect(
            !widens("builtin:sbyte", "builtin:uint") && !widens("builtin:sbyte", "builtin:byte") &&
                !widens("builtin:byte", "builtin:sbyte"),
            "89: sbyte does NOT widen to unsigned, and byte does not widen to sbyte");

        // Los pares con punto flotante siguen siendo del bucle de preferencia, que es lo suyo.
        tests.expect(
            common("builtin:int", "builtin:double", std::nullopt, std::nullopt) == "builtin:double",
            "121: an integral against double is still double");
        tests.expect(
            common("builtin:float", "builtin:long", std::nullopt, std::nullopt) == "builtin:float",
            "121: float against long is still float");
    }

    // ID 121 medido en el IR -- LA SEGUNDA MITAD DE LA MEDICION.
    //
    // La fixture de ejecucion dice que el valor es 42. No dice si el compilador lo CALCULO o lo
    // PLEGO: un `add` en tiempo de ejecucion tambien daria 42. Lo que separa las dos cosas es el
    // IR, y un campo `const` plegado se materializa como una GLOBAL CONSTANTE con su valor dentro.
    //
    // LA PRIMERA VERSION DE ESTE TEST CONTABA `add` EN TODO EL MODULO, Y DABA UN FALSO POSITIVO.
    // Encontro uno y fallo. Medido sobre el IR que el propio test imprimio, ese `add` era
    //
    //     %interface_map.next = add i64 %interface_map.index, 1
    //
    // el bucle del mapa de interfaces que el backend emite en TODO modulo, tenga o no aritmetica
    // el programa. Contar instrucciones sueltas por el modulo entero mide el runtime, no el
    // plegado. Se mide la propiedad directamente: la global existe y lleva el valor plegado.
    {
        const std::string text = lowerToLlvmText(
            tests, root, "cc1-constant-promotion-folds",
            R"NS(
namespace cc1promoir
{
    public class K
    {
        public const long Mixed = 1 + 1L;
        public const long Chained = Mixed + 40L;
        // La fixture c17 salio con codigo 3 en la corrida del 2026-09-02, que es su
        // comprobacion de NegativeWide. Estas dos constantes negativas se miden AQUI, en el
        // IR, porque el IR imprime el VALOR y una fixture de ejecucion solo imprime el
        // numero de la comprobacion que fallo.
        public const long NegativeWide = (-1) + 1L;
        public const long NegativeSum = (0 - 5) + 10L;
        public const int DivisionWrap = (-2147483647 - 1) / -1;
        public const int RemainderWrap = (-2147483647 - 1) % -1;
        public const long DivisionWrap64 = (-9223372036854775807L - 1L) / -1L;
        public const long RemainderWrap64 = (-9223372036854775807L - 1L) % -1L;
    }

    class P
    {
        public static long Value()
        {
            return K.Chained;
        }

        public static int Main()
        {
            return 0;
        }
    }
}
)NS");
        // Busca una linea que contenga las dos subcadenas. El nombre de la global lleva un indice
        // que asigna el backend, asi que se ancla por el nombre del campo y por el valor, no por
        // el simbolo completo.
        const auto foldedTo = [&text](std::string_view field, std::string_view value)
        {
            std::size_t cursor = 0;
            while (cursor < text.size())
            {
                std::size_t end = text.find('\n', cursor);
                if (end == std::string::npos) end = text.size();
                const std::string line = text.substr(cursor, end - cursor);
                if (line.find(field) != std::string::npos && line.find(value) != std::string::npos)
                {
                    return true;
                }
                cursor = end + 1;
            }
            return false;
        };

        tests.expect(
            foldedTo("Mixed", "constant i64 2"),
            "121: `1 + 1L` folds to the constant 2 in the IR, not to a runtime add");
        tests.expect(
            foldedTo("Chained", "constant i64 42"),
            "121: a const built on another folded const folds too -- 2 + 40 = 42");

        // 122, EL LADO NEGATIVO, y es lo que la fixture c17 no pudo decir. Si estas dos fallan,
        // el mensaje imprime el modulo entero y con el el valor que el compilador SI calculo,
        // que es el dato que hace falta para saber donde se pierde el signo.
        //
        // EL MENSAJE LLEVA EL IR ENTERO A PROPOSITO. La primera version de estas dos aserciones no
        // lo llevaba, fallo, y el log no dijo QUE valor habia calculado el compilador -- sin ese
        // numero no se puede distinguir "el signo se perdio al ensanchar" (4294967296) de "el
        // signo se ignoro" (2). Una asercion que solo dice que fallo obliga a otra corrida entera.
        tests.expect(
            foldedTo("NegativeWide", "constant i64 0"),
            "127: `(-1) + 1L` folds to 0 -- the sign survives the widening to long\n" + text);
        tests.expect(
            foldedTo("NegativeSum", "constant i64 5"),
            "127: `(0 - 5) + 10L` folds to 5 -- a negative magnitude plus a wider literal\n" +
                text);
        tests.expect(
            foldedTo("DivisionWrap", "constant i32 -2147483648"),
            "227: const int MinValue / -1 follows runtime wrap semantics\n" + text);
        tests.expect(
            foldedTo("RemainderWrap", "constant i32 0"),
            "227: const int MinValue % -1 is zero\n" + text);
        tests.expect(
            foldedTo("DivisionWrap64", "constant i64 -9223372036854775808"),
            "227: const long MinValue / -1 follows runtime wrap semantics\n" + text);
        tests.expect(
            foldedTo("RemainderWrap64", "constant i64 0"),
            "227: const long MinValue % -1 is zero\n" + text);
    }

    return tests.result();
}
