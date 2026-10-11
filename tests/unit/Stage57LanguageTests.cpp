// Stage 57 -- contrato del lenguaje: `?:`, `do...while` y `switch`/`case`.
//
// Ninguna de las tres formas existia en el lenguaje, y en el caso de `?:` tampoco existia el token
// `?` ni, en el de `switch`, las keywords. Los fixtures que esta suite compila no verifican "que
// compile": cada uno esta escrito alrededor de la propiedad que una implementacion equivocada
// rompe primero.
//
//   * l1_conditional -- LAZINESS. Solo se evalua la rama tomada. Es la propiedad que hace segura
//     la forma de guardia (`p != null ? p.Value : 0`), y es la que mas facil se pierde: bajar el
//     operador a un `select` de LLVM evalua LAS DOS ramas, compila, pasa el verifier y esta mal.
//     El fixture lo mide con un contador, no lo supone. Ademas fija la asociatividad a la
//     derecha y el tipo del resultado cuando las ramas difieren.
//   * l2_dowhile -- que el CUERPO CORRE ANTES DE LA PRIMERA PRUEBA, que es la unica razon por la
//     que el bucle existe, y que `continue` salta a la CONDICION y no al principio del cuerpo,
//     como especifican C y C#. Equivocar eso convierte un bucle que termina en uno que no.
//   * l3_switch -- QUE NO HAY FALL-THROUGH: una seccion que llega a su final sale del switch, no
//     cae en la siguiente. Es la decision que separa este `switch` del de C, y la que un lector
//     que viene de C va a suponer al reves. Mas el par que una implementacion ingenua invierte sin
//     que nada falle en compilacion: `break` dentro de un switch dentro de un bucle sale DEL
//     SWITCH, `continue` dentro de un switch dentro de un bucle continua EL BUCLE.
//
// Como en el resto del arbol: aqui se verifica que COMPILAN a -O0 y -O2; la ejecucion real la
// corre run_windows_checklist.bat en Windows, donde no hay Wine.
//
// La segunda mitad de esta suite es el CONTRATO NEGATIVO del switch: que rechace lo que tiene que
// rechazar, y con que mensaje. Vale tanto como la mitad positiva -- un compilador que acepta dos
// `case` con el mismo valor no da un error, da codigo muerto que parece codigo vivo (y en LLVM,
// comportamiento indefinido), y uno que acepta una etiqueta no constante ya no tiene tabla de
// saltos que construir.

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
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
    // On Windows there is nothing to emulate, and probing with `command -v` -- a POSIX shell
    // builtin -- makes cmd print "The system cannot find the path specified" once per probe.
    // Thirty-four lines of that in the ctest log is noise that hides real output.
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
        "WINEDEBUG=-all " + wine + " " + exePath.string() + " >/dev/null 2>&1";
    const int raw = std::system(command.c_str());
#ifdef WIFEXITED
    return WIFEXITED(raw) ? WEXITSTATUS(raw) : (raw & 0xFF);
#else
    return raw & 0xFF;
#endif
}

// `expected` < 0 means "build and link only, never execute" -- the two-libraries case links on
// purpose against DLLs that do not exist.
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

        if (expected < 0)
        {
            tests.expect(true, scenario + ": build and link only, not executed by contract");
            continue;
        }

        const std::string wine = wineCommand();
        if (wine.empty())
        {
            // Not a silent pass: the execution half is run by run_bug_triage.bat on Windows.
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

// The negative half: compile a tiny program that must NOT compile, and check both that it failed
// and that it failed FOR THE STATED REASON. Checking only "it failed" would pass for a typo in the
// fixture, which is how a negative test quietly stops testing anything.
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
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nsc-stage57-language";
    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    std::filesystem::create_directories(root);

    checkFixture(
        tests, root, "tests/language/l1_conditional/l1_conditional.nsproj", "l1-conditional", 42);
    checkFixture(
        tests, root, "tests/language/l2_dowhile/l2_dowhile.nsproj", "l2-dowhile", 42);
    checkFixture(
        tests, root, "tests/language/l3_switch/l3_switch.nsproj", "l3-switch", 42);

    // ------------------------------------------------------------------------------------
    // Contrato negativo del switch.
    // ------------------------------------------------------------------------------------

    checkRejected(
        tests,
        root,
        "governing-type",
        R"NS(
class P
{
    public static int Main()
    {
        double d = 1.0;
        switch (d) { case 1: return 1; }
        return 0;
    }
}
)NS",
        "must have an integral or enum type");

    checkRejected(
        tests,
        root,
        "duplicate-label",
        R"NS(
class P
{
    public static int Main()
    {
        int v = 1;
        switch (v)
        {
            case 2:
                return 1;
            case 2:
                return 2;
        }
        return 0;
    }
}
)NS",
        "duplicate case label");

    // The same duplicate written the way it actually happens: two spellings of one value.
    checkRejected(
        tests,
        root,
        "duplicate-label-through-const",
        R"NS(
class K
{
    public const int Two = 2;
}

class P
{
    public static int Main()
    {
        int v = 1;
        switch (v)
        {
            case 2:
                return 1;
            case K.Two:
                return 2;
        }
        return 0;
    }
}
)NS",
        "duplicate case label");

    checkRejected(
        tests,
        root,
        "two-defaults",
        R"NS(
class P
{
    public static int Main()
    {
        int v = 1;
        switch (v)
        {
            default:
                return 1;
            case 2:
                return 2;
            default:
                return 3;
        }
        return 0;
    }
}
)NS",
        "only one default label");

    checkRejected(
        tests,
        root,
        "non-constant-label",
        R"NS(
class P
{
    public static int Main()
    {
        int v = 1;
        int k = 2;
        switch (v)
        {
            case k:
                return 1;
        }
        return 0;
    }
}
)NS",
        "must be a compile-time integer constant");

    // `break` is legal in a switch, so `continue` must not be borrowed from the same frame: with
    // no loop around it there is nothing to continue.
    checkRejected(
        tests,
        root,
        "continue-without-loop",
        R"NS(
class P
{
    public static int Main()
    {
        int v = 1;
        switch (v)
        {
            case 1:
                continue;
        }
        return 0;
    }
}
)NS",
        "continue is only valid within a loop");

    // The string-literal/native-string join is one existing implicit conversion, not permission
    // to invent a common type for unrelated conditional branches.
    checkRejected(
        tests,
        root,
        "conditional-incompatible-branches",
        R"NS(
class P
{
    public static int Main()
    {
        bool chooseText = true;
        string text = chooseText ? "native" : 42;
        return text.Length;
    }
}
)NS",
        "branches of a conditional expression have incompatible types");

    return tests.result();
}
