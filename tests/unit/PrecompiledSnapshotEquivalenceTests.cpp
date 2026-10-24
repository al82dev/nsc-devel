#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/ProjectLoadOptions.h"
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

#include "../support/TestContext.h"

#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

// EN: Plan item 4.1 (component equivalence). Compiles one consumer project to MIR twice, with
//     usePrecompiledSnapshots off and on, and requires identical dumps of every frontend model.
//     Run with a consumer .nsproj as the first argument; the snapshot report lines go to stdout.
// ES: Punto 4.1 del plan (equivalencia de componentes). Compila un proyecto consumidor hasta MIR dos
//     veces, con usePrecompiledSnapshots apagado y encendido, y exige volcados idénticos de cada
//     modelo del frontend. Se ejecuta con un .nsproj consumidor como primer argumento; las líneas
//     del reporte de snapshots salen por stdout.

namespace
{
using namespace nativesharp;
using ModelDumps = std::map<std::string, std::string>;

// EN: The dump of every model the driver can print, keyed by model name.
// ES: El volcado de cada modelo que el driver puede imprimir, indexado por nombre de modelo.
ModelDumps dumpModels(const frontend::ProjectCompilation& compilation)
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
// ES: Carga, prepara y compila `project` hasta MIR con snapshots encendidos o apagados; vacío si
//     falla.
std::optional<ModelDumps> compileAndDump(
    const std::filesystem::path& project,
    bool useSnapshots,
    std::string& error)
{
    compilation::ProjectLoadOptions options;
    options.usePrecompiledSnapshots = useSnapshots;
    const compilation::ProjectGraphLoader loader(1, {}, options);
    std::optional<compilation::ProjectGraph> graph = loader.load(project, error);
    if (!graph.has_value()) return std::nullopt;
    frontend::ProjectCompilation compilation;
    compilation.setProjectLoadOptions(options);
    compilation.setReportSink([useSnapshots](std::string_view line)
        { std::cout << (useSnapshots ? "[on] " : "[off] ") << line << '\n'; });
    if (!compilation::preparePrecompiledProject(std::move(*graph), options, compilation, error) ||
        !compilation.compileProjectOopMir(project, error))
    {
        return std::nullopt;
    }
    return dumpModels(compilation);
}

// EN: The first line where two dumps differ, for the failure message.
// ES: La primera línea donde difieren dos volcados, para el mensaje de falla.
std::string firstDifference(const std::string& left, const std::string& right)
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
}

int main(int argc, char** argv)
{
    tests::TestContext tests;
    if (argc < 2)
    {
        std::cout << "usage: nsc_precompiled_snapshot_equivalence_tests <consumer.nsproj>\n";
        return tests.result();
    }
    const std::filesystem::path project(argv[1]);
    std::string offError;
    std::string onError;
    const std::optional<ModelDumps> off = compileAndDump(project, false, offError);
    const std::optional<ModelDumps> on = compileAndDump(project, true, onError);
    tests.expect(off.has_value(), "the consumer compiles with snapshots off: " + offError);
    tests.expect(on.has_value(), "the consumer compiles with snapshots on: " + onError);
    if (!off.has_value() || !on.has_value()) return tests.result();
    for (const auto& [model, dump] : *off)
    {
        const std::string& other = on->at(model);
        tests.expect(dump == other, "model '" + model + "' is identical with snapshots on and off" +
            (dump == other ? std::string{} : " (" + firstDifference(dump, other) + ")"));
    }
    return tests.result();
}
