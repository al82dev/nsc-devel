#include "../support/PrecompiledSnapshotFixture.h"
#include "../support/TestContext.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

// EN: Plan item 4.1 (component equivalence). Compiles one consumer project to MIR twice, with
//     usePrecompiledSnapshots off and on, and requires identical dumps of every frontend model.
//     Without arguments (CTest `nsc.precompiled-snapshot-equivalence`) it first emits its own
//     fixture of two libraries, which publish their snapshots, and also requires that the consumer
//     loads both snapshots and installs entries of every model. With a consumer .nsproj as the first
//     argument it checks that project instead; the snapshot report lines go to stdout.
// ES: Punto 4.1 del plan (equivalencia de componentes). Compila un proyecto consumidor hasta MIR dos
//     veces, con usePrecompiledSnapshots apagado y encendido, y exige volcados idénticos de cada
//     modelo del frontend. Sin argumentos (CTest `nsc.precompiled-snapshot-equivalence`) primero
//     emite su propio fixture de dos bibliotecas, que publican sus snapshots, y además exige que el
//     consumidor cargue ambos snapshots e instale entradas de cada modelo. Con un .nsproj
//     consumidor como primer argumento comprueba ese proyecto; las líneas del reporte de snapshots
//     salen por stdout.

namespace
{
using namespace nativesharp;

void printReport(const std::vector<std::string>& report, std::string_view prefix)
{
    for (const std::string& line : report) std::cout << prefix << line << '\n';
}

// EN: Emits the fixture under the build folder and returns its consumer project.
// ES: Emite el fixture bajo la carpeta de compilación y regresa su proyecto consumidor.
std::optional<std::filesystem::path> prepareFixture(tests::TestContext& tests)
{
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());
    std::string output;
    const std::optional<std::filesystem::path> consumer = tests::snapshot::buildFixture(
        std::filesystem::path(NSC_BINARY_DIR) / "precompiled_snapshot_equivalence", output);
    tests.expect(consumer.has_value(), "the fixture libraries emit: " + output);
    tests.expect(output.find("precompiled-snapshot-reference: written") != std::string::npos,
        "the fixture libraries publish their snapshots: " + output);
    return consumer;
}

// EN: With the fixture, the consumer must load both snapshots and install entries of every model.
// ES: Con el fixture, el consumidor debe cargar ambos snapshots e instalar entradas de cada modelo.
void expectFixtureInstalls(tests::TestContext& tests, const std::vector<std::string>& report)
{
    tests.expect(tests::snapshot::reportContains(report, "precompiled-snapshot: loaded=2 "),
        "the consumer loads the snapshots of both fixture libraries");
    for (const std::string_view empty : {"name-declarations=0", "members=0", "namespace-functions=0",
             "namespace-states=0", "classes=0", "interfaces=0", "implementing-types=0", "dispatch-classes=0",
             "destruction-classes=0", "layout-structs=0", "layout-classes=0"})
    {
        tests.expect(!tests::snapshot::reportContains(report, std::string("install: ") + std::string(empty)) &&
                !tests::snapshot::reportContains(report, " " + std::string(empty)),
            "the consumer installs fixture entries (" + std::string(empty) + " not reported)");
    }
}
}

int main(int argc, char** argv)
{
    tests::TestContext tests;
    const bool fixture = argc < 2;
    const std::optional<std::filesystem::path> project =
        fixture ? prepareFixture(tests) : std::optional<std::filesystem::path>(argv[1]);
    if (!project.has_value()) return tests.result();
    std::vector<std::string> offReport;
    std::vector<std::string> onReport;
    std::string offError;
    std::string onError;
    const std::optional<tests::snapshot::ModelDumps> off =
        tests::snapshot::compileAndDump(*project, false, offReport, offError);
    const std::optional<tests::snapshot::ModelDumps> on =
        tests::snapshot::compileAndDump(*project, true, onReport, onError);
    printReport(offReport, "[off] ");
    printReport(onReport, "[on] ");
    tests.expect(off.has_value(), "the consumer compiles with snapshots off: " + offError);
    tests.expect(on.has_value(), "the consumer compiles with snapshots on: " + onError);
    if (!off.has_value() || !on.has_value()) return tests.result();
    for (const auto& [model, dump] : *off)
    {
        const std::string& other = on->at(model);
        tests.expect(dump == other, "model '" + model + "' is identical with snapshots on and off" +
            (dump == other ? std::string{} : " (" + tests::snapshot::firstDifference(dump, other) + ")"));
    }
    if (fixture) expectFixtureInstalls(tests, onReport);
    return tests.result();
}
