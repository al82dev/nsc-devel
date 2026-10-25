#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

std::string compile(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    nativesharp::ExitCode& code,
    std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 6> arguments = {
        "-O2", "--incremental-link", "--emit-project-oop-exe",
        projectText, "-o", executableText};
    nativesharp::driver::Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    code = driver.run(arguments, output, error);
    diagnostic = error.str();
    return output.str();
}

std::string readBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(stream), {}};
}

/// EN: Builds `project` at O0 with full debug information into `directory/Edit.exe`, either with
///     the artifact cache at `cache` or with the cache disabled (`cache` empty). Returns the driver
///     output; `code` and `diagnostic` receive the exit code and the error stream.
/// ES: Compila `project` en O0 con depuracion completa en `directory/Edit.exe`, con el cache de
///     artefactos en `cache` o con el cache apagado (`cache` vacio). Devuelve la salida del driver;
///     `code` y `diagnostic` reciben el codigo de salida y el flujo de error.
std::string compileDebug(
    const std::filesystem::path& project,
    const std::filesystem::path& directory,
    const std::filesystem::path& cache,
    nativesharp::ExitCode& code,
    std::string& diagnostic)
{
    std::filesystem::create_directories(directory);
    const std::string projectText = project.string();
    const std::string executableText = (directory / "Edit.exe").string();
    const std::string cacheOption =
        cache.empty() ? "--no-artifact-cache" : "--artifact-cache=" + cache.string();
    const std::array<std::string_view, 7> arguments = {
        "-O0", "-g", cacheOption, "--emit-project-oop-exe", projectText, "-o", executableText};
    nativesharp::driver::Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    code = driver.run(arguments, output, error);
    diagnostic = error.str();
    return output.str();
}

std::uint64_t reportedCount(std::string_view output, std::string_view prefix)
{
    const std::size_t begin = output.find(prefix);
    if (begin == std::string_view::npos) return 0;
    return std::strtoull(std::string(output.substr(begin + prefix.size(), 20)).c_str(), nullptr, 10);
}

/// EN: Optimization part 2, items 21, 23 and 24. Builds `before` with a warm artifact cache,
///     edits one body of `editedFile` into `after` (`fixedFile` keeps `fixedText`), rebuilds with
///     the same cache and compares the executable and PDB with a clean build of `after`. The warm
///     rebuild must reuse cached code units, produce the same bytes and run the edited program.
/// ES: Puntos 21, 23 y 24 de la parte 2. Compila `before` con un cache caliente, edita un cuerpo
///     de `editedFile` a `after` (`fixedFile` conserva `fixedText`), recompila con el mismo cache y
///     compara ejecutable y PDB con un build limpio de `after`. La recompilacion caliente debe
///     reutilizar unidades del cache, dar los mismos bytes y ejecutar el programa editado.
void testEditedBodyMatchesCleanBuild(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view fixedFile,
    std::string_view fixedText,
    std::string_view editedFile,
    std::string_view before,
    std::string_view after,
    int expectedExit)
{
    const std::filesystem::path project = root / "Edit.nsproj";
    writeFile(project,
        "<Project><PropertyGroup><Name>Edit</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / fixedFile, fixedText);
    writeFile(root / editedFile, before);
    nativesharp::ExitCode code{};
    std::string error;
    compileDebug(project, root / "warm", root / "cache", code, error);
    tests.expect(code == nativesharp::ExitCode::Success,
        std::string(name) + ": the first build fills the artifact cache: " + error);
    writeFile(root / editedFile, after);
    const std::string warm = compileDebug(project, root / "warm", root / "cache", code, error);
    tests.expect(code == nativesharp::ExitCode::Success &&
            reportedCount(warm, "artifact-cache-hits: ") > 0,
        std::string(name) + ": the rebuild after the edit reuses cached code units: " + error);
    compileDebug(project, root / "clean", {}, code, error);
    tests.expect(code == nativesharp::ExitCode::Success,
        std::string(name) + ": the clean build of the edited sources succeeds: " + error);
    const std::string warmExecutable = readBytes(root / "warm" / "Edit.exe");
    const std::string warmPdb = readBytes(root / "warm" / "Edit.pdb");
    tests.expect(!warmExecutable.empty() && warmExecutable == readBytes(root / "clean" / "Edit.exe"),
        std::string(name) + ": the reused build links the same executable as the clean build");
    tests.expect(!warmPdb.empty() && warmPdb == readBytes(root / "clean" / "Edit.pdb"),
        std::string(name) + ": the reused build writes the same PDB as the clean build");
    const std::string command = "\"" + (root / "warm" / "Edit.exe").string() + "\"";
    tests.expect(std::system(command.c_str()) == expectedExit,
        std::string(name) + ": the reused build runs the edited body");
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item392_incremental_link_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path project = root / "Incremental.nsproj";
    const std::filesystem::path source = root / "Program.ns";
    const std::filesystem::path executable = root / "Incremental.exe";
    writeFile(project,
        "<Project><PropertyGroup><Name>Incremental</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(source, "class Program { public static int Main() { return 41; } }");

    nativesharp::ExitCode code{};
    std::string error;
    const std::string cold = compile(project, executable, code, error);
    tests.expect(code == nativesharp::ExitCode::Success &&
            cold.find("incremental-link: miss") != std::string::npos,
        "first incremental link produces and records the PE: " + error);
    tests.expect(std::filesystem::is_regular_file(executable.string() + ".nslinc"),
        "incremental link publishes its versioned sidecar state");

    const std::string warm = compile(project, executable, code, error);
    tests.expect(code == nativesharp::ExitCode::Success &&
            warm.find("incremental-link: hit") != std::string::npos,
        "unchanged inputs reuse the verified linked PE: " + error);

    writeFile(source, "class Program { public static int Main() { return 42; } }");
    const std::string changed = compile(project, executable, code, error);
    tests.expect(code == nativesharp::ExitCode::Success &&
            changed.find("incremental-link: miss") != std::string::npos,
        "source/object change invalidates incremental link state: " + error);
    if (code == nativesharp::ExitCode::Success)
    {
        const std::string command = "\"" + executable.string() + "\"";
        tests.expect(std::system(command.c_str()) == 42,
            "relinked executable contains the changed program");
    }

    // A body edit in one method of a multi-method program (items 21 and 24).
    // Una edicion de cuerpo en un metodo de un programa con varios metodos (puntos 21 y 24).
    std::string helpers = "class Helpers\n{\n";
    for (int index = 0; index < 24; ++index)
    {
        helpers += "    public static noinline int H" + std::to_string(index) + "(int value) { int local = value + " +
            std::to_string(index) + "; return local * 2; }\n";
    }
    helpers += "}\n";
    testEditedBodyMatchesCleanBuild(tests, root / "body-edit", "body edit", "Library.ns", helpers, "Program.ns",
        "class Program { public static int Main() { int first = Helpers.H1(3); return first; } }\n",
        "class Program { public static int Main() { int first = Helpers.H1(3); int second = Helpers.H2(1);"
        " return first + second; } }\n",
        14);
    // A generic body edit must reach the specialization the program uses (item 23).
    // Una edicion en un cuerpo generico debe llegar a la especializacion que usa el programa (punto 23).
    testEditedBodyMatchesCleanBuild(tests, root / "generic-edit", "generic body edit", "Library.ns", helpers,
        "Program.ns",
        "class Tools { public static int Weight<T>(T value) { return 3; } }\n"
        "class Program { public static int Main() { return Tools.Weight(1) + Helpers.H0(1); } }\n",
        "class Tools { public static int Weight<T>(T value) { return 5; } }\n"
        "class Program { public static int Main() { return Tools.Weight(1) + Helpers.H0(1); } }\n",
        7);
    return tests.resultKeeping(root);
}
