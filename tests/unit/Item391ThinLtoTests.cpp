#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/ThinLtoLibrarySidecar.h"
#include "nativesharp/LLVM/ThinLtoLibrarySidecarCodec.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
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

std::string valueAfter(std::string_view output, std::string_view prefix)
{
    const std::size_t begin = output.find(prefix);
    if (begin == std::string_view::npos) return {};
    const std::size_t valueBegin = begin + prefix.size();
    const std::size_t end = output.find('\n', valueBegin);
    return std::string(output.substr(valueBegin, end - valueBegin));
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    tests.expect(nativesharp::llvm_backend::linkTimeOptimizationModeName(
            nativesharp::llvm_backend::LinkTimeOptimizationMode::Thin) == "thin",
        "ThinLTO has a stable diagnostic identity");

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item391_thinlto_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Thin.nsproj",
        "<Project><PropertyGroup><Name>Thin</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Program.ns", R"NS(
class Program
{
    static noinline int A(int x) { return x + 1; }
    static noinline int B(int x) { return A(x) + 1; }
    static noinline int C(int x) { return B(x) + 1; }
    static noinline int D(int x) { return C(x) + 1; }
    static noinline int E(int x) { return D(x) + 1; }
    static noinline int F(int x) { return E(x) + 1; }
    static noinline int G(int x) { return F(x) + 1; }
    public static int Main() { return G(35); }
}
)NS");

    const std::string project = (root / "Thin.nsproj").string();
    std::string firstIndex;
    for (int run = 0; run < 2; ++run)
    {
        const std::string executable = (root / ("Thin" + std::to_string(run) + ".exe")).string();
        const std::array<std::string_view, 7> arguments = {
            "-O2", "-j4", "-flto=thin", "--emit-project-oop-exe", project, "-o", executable};
        nativesharp::driver::Driver driver;
        std::ostringstream output;
        std::ostringstream error;
        const nativesharp::ExitCode result = driver.run(arguments, output, error);
        tests.expect(result == nativesharp::ExitCode::Success,
            "ThinLTO project emits successfully: " + error.str());
        tests.expect(error.str().find("nsc: warning: -flto=thin") == std::string::npos,
            "ThinLTO at -O2 runs real backends and does not warn");
        const std::string text = output.str();
        const std::string index = valueAfter(text, "thinlto-index: ");
        tests.expect(text.find("link-time-optimization: thin") != std::string::npos &&
                text.find("thinlto-summaries: 1") == std::string::npos &&
                text.find("thinlto-imports: ") != std::string::npos &&
                index.size() == 64,
            "ThinLTO builds multiple indexed summaries");
        if (run == 0) firstIndex = index;
        else tests.expect(index == firstIndex,
            "ThinLTO global index is reproducible across parallel builds");
        if (result == nativesharp::ExitCode::Success)
        {
            const std::string command = "\"" + executable + "\"";
            tests.expect(std::system(command.c_str()) == 42,
                "ThinLTO executable preserves native program behavior");
        }
    }
    {
        // A final ThinLTO link keeps visible only what native objects need and drops what nothing
        // references, so it is not larger than the partitioned build (item 50). Un enlace final
        // ThinLTO deja visible solo lo que necesitan los objetos nativos y tira lo que nada referencia,
        // asi que no es mas grande que el build particionado (punto 50).
        const std::string plain = (root / "Plain.exe").string();
        const std::array<std::string_view, 6> arguments = {
            "-O2", "-j4", "--emit-project-oop-exe", project, "-o", plain};
        nativesharp::driver::Driver driver;
        std::ostringstream output;
        std::ostringstream error;
        const bool built = driver.run(arguments, output, error) == nativesharp::ExitCode::Success;
        std::error_code sizeError;
        const std::uintmax_t plainSize = std::filesystem::file_size(plain, sizeError);
        const std::uintmax_t thinSize = std::filesystem::file_size(root / "Thin0.exe", sizeError);
        tests.expect(built && !sizeError && thinSize <= plainSize,
            "a ThinLTO executable is not larger than the partitioned one (" + std::to_string(thinSize) + " <= " +
                std::to_string(plainSize) + "): " + error.str());
    }
    {
        // The second build with the same artifact cache restores every ThinLTO backend (item 45).
        // El segundo build con el mismo cache de artefactos restaura cada backend ThinLTO (punto 45).
        const std::string cache = "--artifact-cache=" + (root / "cache").string();
        std::string firstObjectIndex;
        for (int run = 0; run < 2; ++run)
        {
            const std::string executable = (root / ("Cached" + std::to_string(run) + ".exe")).string();
            const std::array<std::string_view, 8> arguments = {
                "-O2", "-j4", "-flto=thin", cache, "--emit-project-oop-exe", project, "-o", executable};
            nativesharp::driver::Driver driver;
            std::ostringstream output;
            std::ostringstream error;
            const nativesharp::ExitCode result = driver.run(arguments, output, error);
            const std::string text = output.str();
            const std::string cached = valueAfter(text, "thinlto-cached-backends: ");
            const std::string summaries = valueAfter(text, "thinlto-summaries: ");
            tests.expect(result == nativesharp::ExitCode::Success &&
                    (run == 0 ? cached == "0" : (!cached.empty() && cached == summaries)),
                run == 0 ? "the first cached ThinLTO build runs every backend: " + error.str()
                         : "the second cached ThinLTO build restores every backend: " + error.str());
            const std::string index = valueAfter(text, "thinlto-index: ");
            if (run == 0) firstObjectIndex = index;
            else tests.expect(index == firstObjectIndex && index.size() == 64,
                "restored ThinLTO backends produce the same objects");
            if (result == nativesharp::ExitCode::Success)
            {
                const std::string command = "\"" + executable + "\"";
                tests.expect(std::system(command.c_str()) == 42,
                    "an executable linked from cached ThinLTO backends preserves program behavior");
            }
        }
    }
    {
        // Two identical -O2 -g ThinLTO builds write the same executable and PDB: the backends record
        // no object path in CodeView. Dos builds ThinLTO -O2 -g identicos escriben el mismo ejecutable
        // y PDB: los backends no guardan ruta de objeto en CodeView.
        std::string executables[2];
        std::string pdbs[2];
        for (int run = 0; run < 2; ++run)
        {
            const std::filesystem::path directory = root / ("debug" + std::to_string(run));
            std::filesystem::create_directories(directory);
            const std::string executable = (directory / "Thin.exe").string();
            const std::array<std::string_view, 9> arguments = {"-O2", "-g", "-j4", "-flto=thin",
                "--no-artifact-cache", "--emit-project-oop-exe", project, "-o", executable};
            nativesharp::driver::Driver driver;
            std::ostringstream output;
            std::ostringstream error;
            tests.expect(driver.run(arguments, output, error) == nativesharp::ExitCode::Success,
                "a -O2 -g ThinLTO build succeeds: " + error.str());
            std::ifstream executableStream(directory / "Thin.exe", std::ios::binary);
            executables[run].assign(std::istreambuf_iterator<char>(executableStream), {});
            std::ifstream pdbStream(directory / "Thin.pdb", std::ios::binary);
            pdbs[run].assign(std::istreambuf_iterator<char>(pdbStream), {});
        }
        tests.expect(!executables[0].empty() && executables[0] == executables[1] &&
                !pdbs[0].empty() && pdbs[0] == pdbs[1],
            "identical -O2 -g ThinLTO builds write the same executable and PDB");
    }
    {
        // A static library built with ThinLTO publishes its sidecar, and a ThinLTO consumer at the same
        // options imports from its units; another level keeps the native library (item 48). Una
        // biblioteca estatica construida con ThinLTO publica su sidecar y un consumidor ThinLTO con las
        // mismas opciones importa de sus unidades; otro nivel conserva la biblioteca nativa (punto 48).
        const auto build = [](std::span<const std::string_view> arguments, std::string& output) {
            nativesharp::driver::Driver driver;
            std::ostringstream out;
            std::ostringstream err;
            const nativesharp::ExitCode result = driver.run(arguments, out, err);
            output = out.str() + err.str();
            return result == nativesharp::ExitCode::Success;
        };
        const auto readBytes = [](const std::filesystem::path& path) {
            std::ifstream stream(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(stream), {});
        };
        for (const std::string_view variant : {std::string_view("Producer"), std::string_view("ProducerDebug")})
        {
            const std::filesystem::path producer = root / variant;
            writeFile(producer / "ThinProducer.nsproj",
                "<Project><PropertyGroup><Name>ThinProducer</Name><OutputType>StaticLibrary</OutputType>"
                "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
            writeFile(producer / "Api.ns", R"NS(
namespace ThinLibrary
{
    public static class Steps
    {
        public static int Add(int value) { return value + 1; }
        public static int Twice(int value) { return Add(Add(value)); }
    }
}
)NS");
            const std::filesystem::path consumer = root / ("Consumer" + std::string(variant));
            writeFile(consumer / "ThinConsumer.nsproj",
                "<Project><PropertyGroup><Name>ThinConsumer</Name></PropertyGroup>"
                "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
                "<LibraryReference Include=\"" + (producer / "ThinProducer.lib").generic_string() +
                "\" Metadata=\"" + (producer / "ThinProducer.nsmeta").generic_string() +
                "\" /></ItemGroup></Project>");
            writeFile(consumer / "Program.ns", R"NS(
using ThinLibrary;

class Program
{
    public static int Main() { return Steps.Twice(40); }
}
)NS");
        }
        std::string text;
        const std::string library = (root / "Producer" / "ThinProducer.lib").string();
        const std::string producerProject = (root / "Producer" / "ThinProducer.nsproj").string();
        const std::array<std::string_view, 6> producerArguments = {
            "-O2", "-flto=thin", "--emit-project-oop-lib", producerProject, "-o", library};
        tests.expect(build(producerArguments, text) &&
                text.find("thinlto-library-sidecar: ") != std::string::npos &&
                std::filesystem::is_regular_file(root / "Producer" / "ThinProducer.nsthinlto"),
            "a ThinLTO static library publishes its bitcode sidecar: " + text);
        const std::string consumerProject = (root / "ConsumerProducer" / "ThinConsumer.nsproj").string();
        const std::string consumed = (root / "ConsumerProducer" / "Consumed.exe").string();
        const std::array<std::string_view, 6> consumedArguments = {
            "-O2", "-flto=thin", "--emit-project-oop-exe", consumerProject, "-o", consumed};
        const bool consumedBuilt = build(consumedArguments, text);
        const std::string imports = valueAfter(text, "thinlto-imports: ");
        tests.expect(consumedBuilt && text.find("thinlto-libraries: 1") != std::string::npos &&
                text.find("thinlto-library-fallback: ") == std::string::npos &&
                !imports.empty() && imports != "0",
            "a ThinLTO consumer at the same options imports from the library sidecar: " + text);
        if (consumedBuilt)
        {
            const std::string command = "\"" + consumed + "\"";
            tests.expect(std::system(command.c_str()) == 42,
                "an executable that consumed a library sidecar preserves program behavior");
        }
        const std::string fallback = (root / "ConsumerProducer" / "Fallback.exe").string();
        const std::array<std::string_view, 6> fallbackArguments = {
            "-O3", "-flto=thin", "--emit-project-oop-exe", consumerProject, "-o", fallback};
        const bool fallbackBuilt = build(fallbackArguments, text);
        tests.expect(fallbackBuilt && text.find("thinlto-libraries: 0") != std::string::npos &&
                text.find("thinlto-library-fallback: ThinProducer.lib: built with another LLVM version, target or "
                          "options") != std::string::npos,
            "a ThinLTO consumer at another level keeps the native library and says why: " + text);
        if (fallbackBuilt)
        {
            const std::string command = "\"" + fallback + "\"";
            tests.expect(std::system(command.c_str()) == 42,
                "an executable linked with the native library fallback preserves program behavior");
        }

        // Consuming a -g sidecar stays reproducible: the replacement archive path depends only on its
        // objects. Consumir un sidecar -g sigue siendo reproducible: la ruta del archivo de reemplazo
        // solo depende de sus objetos.
        const std::string debugLibrary = (root / "ProducerDebug" / "ThinProducer.lib").string();
        const std::string debugProducerProject = (root / "ProducerDebug" / "ThinProducer.nsproj").string();
        const std::array<std::string_view, 7> debugProducerArguments = {
            "-O2", "-g", "-flto=thin", "--emit-project-oop-lib", debugProducerProject, "-o", debugLibrary};
        tests.expect(build(debugProducerArguments, text),
            "a -O2 -g ThinLTO static library builds: " + text);
        const std::string debugConsumerProject = (root / "ConsumerProducerDebug" / "ThinConsumer.nsproj").string();
        std::string executables[2];
        std::string pdbs[2];
        for (int run = 0; run < 2; ++run)
        {
            const std::filesystem::path directory = root / "ConsumerProducerDebug" / ("out" + std::to_string(run));
            std::filesystem::create_directories(directory);
            const std::string executable = (directory / "Consumed.exe").string();
            const std::array<std::string_view, 8> arguments = {"-O2", "-g", "-flto=thin", "--no-artifact-cache",
                "--emit-project-oop-exe", debugConsumerProject, "-o", executable};
            tests.expect(build(arguments, text) && text.find("thinlto-libraries: 1") != std::string::npos,
                "a -O2 -g ThinLTO consumer imports from the -g library sidecar: " + text);
            executables[run] = readBytes(directory / "Consumed.exe");
            pdbs[run] = readBytes(directory / "Consumed.pdb");
        }
        tests.expect(!executables[0].empty() && executables[0] == executables[1] &&
                !pdbs[0].empty() && pdbs[0] == pdbs[1],
            "identical -O2 -g builds that consume a library sidecar write the same executable and PDB");
    }
    {
        // A sidecar binds the metadata of the libraries its library was built against (item 53):
        // after a dependency changes, the dependent library links natively and says why. A v1 sidecar
        // is rejected. Un sidecar liga la metadata de las bibliotecas contra las que se construyo su
        // biblioteca (punto 53): tras cambiar una dependencia, la dependiente se enlaza nativa y dice
        // por que. Un sidecar v1 se rechaza.
        const auto build = [](std::span<const std::string_view> arguments, std::string& output) {
            nativesharp::driver::Driver driver;
            std::ostringstream out;
            std::ostringstream err;
            const nativesharp::ExitCode result = driver.run(arguments, out, err);
            output = out.str() + err.str();
            return result == nativesharp::ExitCode::Success;
        };
        const std::filesystem::path chain = root / "Chain";
        const auto library = [&](std::string_view name, std::string_view source, std::string_view references) {
            writeFile(chain / name / (std::string(name) + ".nsproj"),
                "<Project><PropertyGroup><Name>" + std::string(name) + "</Name><OutputType>StaticLibrary"
                "</OutputType></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup>" +
                std::string(references) + "</Project>");
            writeFile(chain / name / "Api.ns", source);
        };
        const auto reference = [&](std::string_view name) {
            const std::filesystem::path directory = chain / name;
            return "<LibraryReference Include=\"" + (directory / (std::string(name) + ".lib")).generic_string() +
                "\" Metadata=\"" + (directory / (std::string(name) + ".nsmeta")).generic_string() + "\" />";
        };
        const auto emitLibrary = [&](std::string_view name, std::string& text) {
            const std::string project = (chain / name / (std::string(name) + ".nsproj")).string();
            const std::string output = (chain / name / (std::string(name) + ".lib")).string();
            const std::array<std::string_view, 6> arguments = {
                "-O2", "-flto=thin", "--emit-project-oop-lib", project, "-o", output};
            return build(arguments, text);
        };
        library("ThinBase", "namespace ThinBase { public static class Seed { "
            "public static int Value() { return 20; } } }\n", "");
        library("ThinMid", "namespace ThinMid { public static class Steps { "
            "public static int Twice() { return ThinBase.Seed.Value() * 2; } } }\n",
            "<ItemGroup>" + reference("ThinBase") + "</ItemGroup>");
        writeFile(chain / "App" / "App.nsproj",
            "<Project><PropertyGroup><Name>App</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
            "</ItemGroup><ItemGroup>" + reference("ThinMid") + reference("ThinBase") + "</ItemGroup></Project>");
        writeFile(chain / "App" / "Program.ns",
            "class Program { public static int Main() { return ThinMid.Steps.Twice() + 2; } }\n");
        const std::string appProject = (chain / "App" / "App.nsproj").string();
        const std::string appExecutable = (chain / "App" / "App.exe").string();
        const std::array<std::string_view, 6> appArguments = {
            "-O2", "-flto=thin", "--emit-project-oop-exe", appProject, "-o", appExecutable};
        std::string text;
        tests.expect(emitLibrary("ThinBase", text) && emitLibrary("ThinMid", text),
            "the ThinLTO dependency chain builds: " + text);
        tests.expect(build(appArguments, text) && text.find("thinlto-libraries: 2") != std::string::npos,
            "a consumer imports from a library and from its dependency: " + text);
        // A real dependency change already fails the build (the binary closure ABI fingerprint), so
        // the sidecar records a dependency digest that no longer matches. Un cambio real de
        // dependencia ya falla el build (huella ABI del cierre binario), asi que el sidecar registra un
        // digest de dependencia que ya no coincide.
        const std::filesystem::path midSidecar = chain / "ThinMid" / "ThinMid.nsthinlto";
        nativesharp::llvm_backend::ThinLtoLibrarySidecar stale;
        std::string codecError;
        const bool readStale = nativesharp::llvm_backend::ThinLtoLibrarySidecarCodec::read(
            midSidecar, stale, codecError);
        const std::size_t separator = stale.dependencies.rfind(' ');
        tests.expect(readStale && stale.dependencies.starts_with("thinbase.nsmeta ") && separator != std::string::npos,
            "the sidecar of a library lists the metadata it was built against: " + codecError);
        if (readStale && separator != std::string::npos)
        {
            stale.dependencies = stale.dependencies.substr(0, separator + 1) + std::string(64, '0') + "\n";
            tests.expect(nativesharp::llvm_backend::ThinLtoLibrarySidecarCodec::write(midSidecar, stale, codecError),
                "the stale sidecar is written: " + codecError);
        }
        const bool rebuilt = build(appArguments, text);
        tests.expect(rebuilt && text.find("thinlto-libraries: 1") != std::string::npos &&
                text.find("thinlto-library-fallback: ThinMid.lib: dependency thinbase.nsmeta differs") !=
                    std::string::npos,
            "a library whose dependency differs links natively and says why: " + text);
        if (rebuilt)
        {
            const std::string command = "\"" + appExecutable + "\"";
            tests.expect(std::system(command.c_str()) == 42,
                "the executable with a native fallback for a changed dependency preserves program behavior");
        }

        // A sidecar in the v1 format links natively. Un sidecar en formato v1 se enlaza nativo.
        std::string version1("NSTHINLT");
        version1.append("\x01\x00\x00\x00", 4);
        const nativesharp::basic::Sha256Digest digest = nativesharp::basic::sha256(std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(version1.data()), version1.size()));
        version1.append(reinterpret_cast<const char*>(digest.data()), digest.size());
        writeFile(chain / "ThinMid" / "ThinMid.nsthinlto", version1);
        nativesharp::llvm_backend::ThinLtoLibrarySidecar sidecar;
        std::string error;
        tests.expect(!nativesharp::llvm_backend::ThinLtoLibrarySidecarCodec::read(
                         chain / "ThinMid" / "ThinMid.nsthinlto", sidecar, error) &&
                error.find("version") != std::string::npos,
            "a v1 ThinLTO sidecar is rejected by its version: " + error);
    }
    {
        // At -O0 thin keeps the ordinary partitioned build and says so (optimization part 2, item 44).
        // En -O0 thin conserva el build particionado normal y lo dice (parte 2, punto 44).
        const std::string executable = (root / "ThinO0.exe").string();
        const std::array<std::string_view, 7> arguments = {
            "-O0", "-j4", "-flto=thin", "--emit-project-oop-exe", project, "-o", executable};
        nativesharp::driver::Driver driver;
        std::ostringstream output;
        std::ostringstream error;
        const nativesharp::ExitCode result = driver.run(arguments, output, error);
        tests.expect(result == nativesharp::ExitCode::Success &&
                error.str().find("nsc: warning: -flto=thin at -O0") != std::string::npos,
            "ThinLTO at -O0 warns that it builds the ordinary partitioned code: " + error.str());
    }
    return tests.resultKeeping(root);
}
