#pragma once

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/ProjectLoadOptions.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectConstructionModelDumper.h"
#include "nativesharp/Frontend/ProjectDestructionModelDumper.h"
#include "nativesharp/Frontend/ProjectGenericModelDumper.h"
#include "nativesharp/Frontend/ProjectInheritanceModelDumper.h"
#include "nativesharp/Frontend/ProjectInterfaceModelDumper.h"
#include "nativesharp/Frontend/ProjectMemberModelDumper.h"
#include "nativesharp/Frontend/ProjectMethodBodyModelDumper.h"
#include "nativesharp/Frontend/ProjectNameLookupDumper.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModelDumper.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModelDumper.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// EN: Shared by the precompiled semantic snapshot tests (plan items 4.1 and 4.2). A fixture of two
//     libraries (FixtureBase and Fixture, which references it) covering namespace functions and
//     states, interfaces, a class hierarchy across both libraries with virtual methods and a
//     destructor, and structs; a consumer of both; and the model dumps compared with snapshots on
//     and off.
// ES: Compartido por las pruebas del snapshot semántico precompilado (puntos 4.1 y 4.2 del plan).
//     Un fixture de dos bibliotecas (FixtureBase y Fixture, que la referencia) que cubre funciones y
//     estados de namespace, interfaces, una jerarquía de clases entre ambas bibliotecas con métodos
//     virtuales y destructor, y structs; un consumidor de ambas; y los volcados de modelos que se
//     comparan con snapshots encendidos y apagados.

namespace nativesharp::tests::snapshot
{
using ModelDumps = std::map<std::string, std::string>;

constexpr std::string_view kBaseSource = R"NS(namespace FixtureBase
{
    public struct Pair
    {
        public int First;
        public int Second;
    }

    public class Shape
    {
        public Pair Origin;

        public virtual int Corners()
        {
            return 0;
        }
    }

    public int Twice(int value)
    {
        return value * 2;
    }
}
)NS";

constexpr std::string_view kFixtureSource = R"NS(namespace Fixture
{
    public const int Bias = 5;
    public int Counter = 7;
    public const double Scale = 2.5;

    public int Add(int a, int b)
    {
        return a + b + Bias;
    }

    public double Add(double a, double b)
    {
        return a + b * Scale;
    }

    public int Next()
    {
        Counter = Counter + 1;
        return Counter;
    }

    public interface IArea
    {
        double Area();
    }

    public interface IVolume : IArea
    {
        double Volume();
    }

    public struct Box
    {
        public FixtureBase.Pair Low;
        public FixtureBase.Pair High;

        public int Width()
        {
            return High.First - Low.First;
        }
    }

    public class Square : FixtureBase.Shape, IArea
    {
        public double Side;
        public Box Bounds;

        public double Area()
        {
            return Side * Side;
        }

        public override int Corners()
        {
            return 4;
        }
    }

    public class Cube : Square, IVolume
    {
        public double Volume()
        {
            return Side * Side * Side;
        }

        public override int Corners()
        {
            return 8;
        }

        ~Cube()
        {
        }
    }
}
)NS";

constexpr std::string_view kConsumerSource = R"NS(namespace FixtureConsumer
{
    public static class Use
    {
        public static int One()
        {
            return Fixture.Add(1, 2) + Fixture.Next() + Fixture.Bias + FixtureBase.Twice(3);
        }
    }
}
)NS";

inline void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

// EN: Writes `<directory>/<name>.nsproj` (a library with one source file and the given library
//     references, given as `name` of a sibling folder) and returns its path.
// ES: Escribe `<directory>/<name>.nsproj` (una biblioteca con un archivo fuente y las referencias de
//     biblioteca dadas, como `name` de una carpeta hermana) y regresa su ruta.
inline std::filesystem::path writeLibraryProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    const std::vector<std::string>& references)
{
    std::string items = "<Compile Include=\"Main.ns\" />";
    for (const std::string& reference : references)
        items += "<LibraryReference Include=\"../" + reference + "/" + reference + ".lib\" />";
    writeFile(directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>Library</OutputType><Language>3</Language></PropertyGroup><ItemGroup>" + items +
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
}

// EN: Emits `project` as `<project folder>/<name>.lib` with the driver, which publishes its
//     `.nsmeta` and `.nsproducts` (and the snapshot) beside it. The driver output goes to `output`.
// ES: Emite `project` como `<carpeta del proyecto>/<name>.lib` con el driver, que publica junto a
//     ella su `.nsmeta` y `.nsproducts` (y el snapshot). La salida del driver va a `output`.
inline bool emitLibrary(const std::filesystem::path& project, std::string& output)
{
    const std::string projectText = project.string();
    const std::string libraryText = project.parent_path().string() + "/" + project.stem().string() + ".lib";
    const std::array<std::string_view, 6> arguments = {
        "--emit-project-oop-lib", projectText, "-O0", "--time", "-o", libraryText};
    driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const bool emitted = driver.run(arguments, out, err) == ExitCode::Success;
    output = out.str() + err.str();
    return emitted;
}

// EN: Writes and emits FixtureBase and Fixture under `root` and writes the consumer project;
//     returns the consumer project path, or nothing when an emission fails.
// ES: Escribe y emite FixtureBase y Fixture bajo `root` y escribe el proyecto consumidor; regresa la
//     ruta del proyecto consumidor, o nada si falla una emisión.
inline std::optional<std::filesystem::path> buildFixture(const std::filesystem::path& root, std::string& output)
{
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path base = writeLibraryProject(root / "FixtureBase", "FixtureBase", kBaseSource, {});
    const std::filesystem::path fixture =
        writeLibraryProject(root / "Fixture", "Fixture", kFixtureSource, {"FixtureBase"});
    std::string baseOutput;
    std::string fixtureOutput;
    const bool emitted = emitLibrary(base, baseOutput) && emitLibrary(fixture, fixtureOutput);
    output = baseOutput + fixtureOutput;
    if (!emitted) return std::nullopt;
    return writeLibraryProject(
        root / "FixtureConsumer", "FixtureConsumer", kConsumerSource, {"FixtureBase", "Fixture"});
}

// EN: The dump of every model the driver can print, keyed by model name.
// ES: El volcado de cada modelo que el driver puede imprimir, indexado por nombre de modelo.
inline ModelDumps dumpModels(const frontend::ProjectCompilation& compilation)
{
    ModelDumps dumps;
    const frontend::ProjectNameLookup& names = *compilation.nameLookup();
    const frontend::ProjectMemberModel& members = *compilation.memberModel();
    std::ostringstream text;
    dumps["generics"] = frontend::ProjectGenericModelDumper::dump(*compilation.genericModel());
    frontend::ProjectNameLookupDumper().dump(names, text);
    dumps["names"] = std::exchange(text, std::ostringstream{}).str();
    frontend::ProjectMemberModelDumper().dump(members, names, text);
    dumps["members"] = std::exchange(text, std::ostringstream{}).str();
    frontend::ProjectInheritanceModelDumper().dump(*compilation.inheritanceModel(), names, text);
    dumps["inheritance"] = std::exchange(text, std::ostringstream{}).str();
    frontend::ProjectInterfaceModelDumper::dump(*compilation.interfaceModel(), text);
    dumps["interfaces"] = std::exchange(text, std::ostringstream{}).str();
    frontend::ProjectVirtualDispatchModelDumper().dump(*compilation.virtualDispatchModel(), members, names, text);
    dumps["virtual-dispatch"] = std::exchange(text, std::ostringstream{}).str();
    frontend::ProjectMethodBodyModelDumper().dump(*compilation.methodBodyModel(), members, names, text);
    dumps["method-bodies"] = std::exchange(text, std::ostringstream{}).str();
    frontend::ProjectDestructionModelDumper::dump(*compilation.destructionModel(), names, members, text);
    dumps["destruction"] = std::exchange(text, std::ostringstream{}).str();
    frontend::ProjectConstructionModelDumper().dump(*compilation.constructionModel(), members, names, text);
    dumps["constructions"] = std::exchange(text, std::ostringstream{}).str();
    frontend::ProjectObjectLayoutModelDumper().dump(*compilation.objectLayoutModel(), members, names, text);
    dumps["layouts"] = std::exchange(text, std::ostringstream{}).str();
    dumps["oop-hir"] = frontend::ProjectOopHirModelDumper::dump(*compilation.oopHirModel());
    dumps["oop-mir"] = frontend::ProjectOopMirModelDumper::dump(*compilation.oopMirModel());
    return dumps;
}

// EN: Loads, prepares and compiles `project` to MIR with snapshots on or off; empty on failure.
//     The `--time` snapshot report lines are appended to `report`.
// ES: Carga, prepara y compila `project` hasta MIR con snapshots encendidos o apagados; vacío si
//     falla. Las líneas del reporte de snapshots de `--time` se agregan a `report`.
inline std::optional<ModelDumps> compileAndDump(
    const std::filesystem::path& project,
    bool useSnapshots,
    std::vector<std::string>& report,
    std::string& error)
{
    compilation::ProjectLoadOptions options;
    options.usePrecompiledSnapshots = useSnapshots;
    const compilation::ProjectGraphLoader loader(1, {}, options);
    std::optional<compilation::ProjectGraph> graph = loader.load(project, error);
    if (!graph.has_value()) return std::nullopt;
    frontend::ProjectCompilation compilation;
    compilation.setProjectLoadOptions(options);
    compilation.setReportSink([&report](std::string_view line) { report.emplace_back(line); });
    if (!compilation::preparePrecompiledProject(std::move(*graph), options, compilation, error) ||
        !compilation.compileProjectOopMir(project, error))
    {
        return std::nullopt;
    }
    return dumpModels(compilation);
}

// EN: The first line where two dumps differ, for the failure message.
// ES: La primera línea donde difieren dos volcados, para el mensaje de falla.
inline std::string firstDifference(const std::string& left, const std::string& right)
{
    std::istringstream leftLines(left);
    std::istringstream rightLines(right);
    std::string leftLine;
    std::string rightLine;
    for (std::size_t line = 1;; ++line)
    {
        const bool hasLeft = static_cast<bool>(std::getline(leftLines, leftLine));
        const bool hasRight = static_cast<bool>(std::getline(rightLines, rightLine));
        if (!hasLeft && !hasRight) return "no difference";
        if (!hasLeft || !hasRight || leftLine != rightLine)
            return "line " + std::to_string(line) + ": off='" + leftLine + "' on='" + rightLine + "'";
    }
}

// EN: True when some report line contains `text`.
// ES: True si alguna línea del reporte contiene `text`.
inline bool reportContains(const std::vector<std::string>& report, std::string_view text)
{
    for (const std::string& line : report)
    {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}
}
