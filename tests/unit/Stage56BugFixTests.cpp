// Stage 56 -- red de regresion de los ocho bugs reportados contra el compilador.
//
// POR QUE EXISTE. `tests/bug_triage/` nacio como un ARNES DE MEDICION: ocho reproductores
// minimos escritos para responder "cuales de los bugs que las instancias consumidoras
// reportaron siguen vivos", porque varios se habian reportado contra compiladores anteriores
// a C1, C6, C7 y C8 y no estaba dicho cuales seguian en pie. La respuesta fue siete vivos y
// uno ya muerto, y los siete se arreglaron (ver `68_triage_y_cierre_de_bugs_reportados.md`).
//
// Un arnes de medicion no es una red. Mientras estos ocho casos vivieran solo en un `.bat`
// que hay que acordarse de correr, nada impedia que cualquiera de ellos volviera sin que
// nadie se enterara -- que es exactamente como llegaron a acumularse en primer lugar. Esta
// suite los pone bajo `ctest`.
//
// QUE VERIFICA Y QUE NO, dicho explicitamente para que nadie lea de mas en un verde:
//
//   * Aqui: que los ocho COMPILAN, a -O0 y a -O2. Seis de los ocho eran fallos ruidosos
//     -- error del verifier, error interno, error de tipos, fallo de enlace -- asi que para
//     esos seis compilar ES la regresion, y esta suite los cubre entera.
//   * Bajo Wine, si esta disponible: ademas se EJECUTAN y se compara el codigo de salida.
//     En la maquina Windows de desarrollo no hay Wine, asi que ahi este paso degrada a skip
//     explicito, igual que en el resto de las suites de este arbol.
//   * La mitad de ejecucion real la corre `run_bug_triage.bat`, que `run_windows_checklist.bat`
//     invoca en su modo completo. Eso es lo que cubre a los DOS bugs silenciosos -- `T self =
//     this;` devolviendo basura y el plan de asignacion equivocado -- que compilan de todas
//     formas y solo se distinguen por el valor que producen.
//
// Cada fixture devuelve 42 cuando el compilador esta sano, corre primero sus controles con
// codigos de salida distintos, y lleva el reporte original en la cabecera de su Main.ns.

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

bool commandAvailable(std::string_view command)
{
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
}

std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
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
    const auto root = std::filesystem::temp_directory_path() / "nsc-stage56-bugfix";
    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    std::filesystem::create_directories(root);

    // b1 -- `T local = this;` in a struct method stored the POINTER instead of copying the
    // struct. Silent: it compiled, passed the verifier, and returned garbage. Fixed in the HIR
    // by loading through `this` in a value context.
    checkFixture(tests, root, "tests/bug_triage/b1_this_copy/b1_this_copy.nsproj", "b1-this-copy", 42);

    // b2 -- `x == null` inside a user-defined `operator ==` bound to that same operator and
    // recursed until the stack overflowed. The null literal no longer binds user operators.
    checkFixture(
        tests, root, "tests/bug_triage/b2_operator_null/b2_operator_null.nsproj", "b2-operator-null", 42);

    // b3 -- a constructor could not receive a struct larger than 8 bytes by value: the call site
    // passed the aggregate while the signature declared `ptr`. The constructor was the one call
    // shape that never went through the Win64 indirect-aggregate mapping.
    checkFixture(
        tests, root, "tests/bug_triage/b3_ctor_aggregate/b3_ctor_aggregate.nsproj", "b3-ctor-aggregate", 42);

    // b4 -- the same HIR line as b1, seen from the other side: `Get(this)` passed the pointer
    // where the callee expected the aggregate.
    checkFixture(
        tests, root, "tests/bug_triage/b4_this_by_value/b4_this_by_value.nsproj", "b4-this-by-value", 42);

    // b5 -- the allocation-plan vector was indexed by construction ordinal, but it only gets an
    // entry for constructions that allocate. One skipped struct construction shifted every later
    // plan: loud when it ran off the end, SILENT when it stayed in range and handed back somebody
    // else's plan.
    checkFixture(
        tests,
        root,
        "tests/bug_triage/b5_ctor_owned_field_array/b5_ctor_owned_field_array.nsproj",
        "b5-ctor-owned-field-array",
        42);

    // b6 -- an `extern C delegate` could not carry an accessibility modifier at all, so it could
    // only ever be internal and could never cross a ProjectReference. One missing branch in the
    // parser, not the module boundary.
    checkFixture(
        tests,
        root,
        "tests/bug_triage/b6_delegate_projectref/app/b6_delegate_projectref.nsproj",
        "b6-delegate-projectref",
        42);

    // b7 -- the same C symbol declared in two classes used to reach the linker as `symbol.1`.
    // Already closed by C1's import identity triple; kept so it stays closed.
    checkFixture(
        tests,
        root,
        "tests/bug_triage/b7_duplicate_extern_c/b7_duplicate_extern_c.nsproj",
        "b7-duplicate-extern-c",
        42);

    // b8 -- a shift's result type was computed as the common type of both operands, which for
    // `1u << 28` had no integral answer and fell through to `double`.
    checkFixture(
        tests, root, "tests/bug_triage/b8_shift_folding/b8_shift_folding.nsproj", "b8-shift-folding", 42);

    return tests.result();
}
