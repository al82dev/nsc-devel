// Stage 57 -- contrato del lenguaje: expresion condicional `?:` y `do...while`.
//
// Las dos formas que esta ronda agrego no existian en el lenguaje, y en el caso de `?:` tampoco
// existia el token `?`. Los dos fixtures que esta suite compila no verifican "que compile": cada
// uno esta escrito alrededor de la propiedad que una implementacion equivocada rompe primero.
//
//   * l1_conditional -- LAZINESS. Solo se evalua la rama tomada. Es la propiedad que hace segura
//     la forma de guardia (`p != null ? p.Value : 0`), y es la que mas facil se pierde: bajar el
//     operador a un `select` de LLVM evalua LAS DOS ramas, compila, pasa el verifier y esta mal.
//     El fixture lo mide con un contador, no lo supone. Ademas fija la asociatividad a la
//     derecha y el tipo del resultado cuando las ramas difieren.
//   * l2_dowhile -- que el CUERPO CORRE ANTES DE LA PRIMERA PRUEBA, que es la unica razon por la
//     que el bucle existe, y que `continue` salta a la CONDICION y no al principio del cuerpo,
//     como especifican C y C#. Equivocar eso convierte un bucle que termina en uno que no.
//
// Como en el resto del arbol: aqui se verifica que COMPILAN a -O0 y -O2; la ejecucion real la
// corre run_windows_checklist.bat en Windows, donde no hay Wine.

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
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
        "WINEDEBUG=-all timeout 25 " + wine + " " + exePath.string() + " >/dev/null 2>&1";
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

    return tests.result();
}
