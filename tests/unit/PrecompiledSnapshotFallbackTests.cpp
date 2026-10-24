#include "nativesharp/Compilation/PrecompiledProductSidecar.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarCodec.h"
#include "nativesharp/Compilation/PrecompiledSemanticSnapshot.h"
#include "nativesharp/Compilation/PrecompiledSemanticSnapshotCodec.h"

#include "../support/PrecompiledSnapshotFixture.h"
#include "../support/TestContext.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

// EN: Plan item 4.2 (fallback). With the snapshot of the Fixture library truncated, with a
//     different dependency digest, with a different node-kind digest and rewritten as a v1 sidecar
//     without snapshot, the consumer must report the fallback reason for that library and compile
//     to exactly the same models as with snapshots off.
// ES: Punto 4.2 del plan (respaldo). Con el snapshot de la biblioteca Fixture truncado, con un digest
//     de dependencia distinto, con un digest de clases de nodo distinto y reescrito como sidecar v1
//     sin snapshot, el consumidor debe reportar la razón de respaldo de esa biblioteca y compilar
//     exactamente los mismos modelos que con snapshots apagados.

namespace
{
using namespace nativesharp;

std::string readBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void writeBytes(const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << bytes;
}

std::span<const std::uint8_t> byteView(const std::string& bytes)
{
    return {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
}

// EN: Rewrites the Fixture sidecar from the original bytes with the snapshot changed by `mutate`.
// ES: Reescribe el sidecar de Fixture desde los bytes originales con el snapshot cambiado por
//     `mutate`.
template <typename Mutate>
bool rewriteSidecar(
    const std::filesystem::path& metadataPath,
    const std::string& originalSidecar,
    Mutate mutate,
    std::string& error)
{
    const std::string metadata = readBytes(metadataPath);
    compilation::PrecompiledProductSidecar sidecar;
    if (!compilation::PrecompiledProductSidecarCodec::decode(originalSidecar, byteView(metadata), sidecar, error))
        return false;
    mutate(sidecar);
    return compilation::PrecompiledProductSidecarCodec::write(metadataPath, sidecar, byteView(metadata), error);
}

// EN: Decodes the snapshot of `sidecar`, applies `mutate` and encodes it back.
// ES: Decodifica el snapshot de `sidecar`, aplica `mutate` y lo vuelve a codificar.
template <typename Mutate>
void mutateSnapshot(compilation::PrecompiledProductSidecar& sidecar, Mutate mutate)
{
    compilation::PrecompiledSemanticSnapshot snapshot;
    std::string error;
    if (!compilation::PrecompiledSemanticSnapshotCodec::decode(sidecar.publicApiSnapshot, snapshot, error)) return;
    mutate(snapshot);
    sidecar.publicApiSnapshot = compilation::PrecompiledSemanticSnapshotCodec::encode(snapshot);
}

// EN: Compiles the consumer with snapshots on and checks the fallback reason and the models.
// ES: Compila el consumidor con snapshots encendidos y comprueba la razón de respaldo y los modelos.
void expectFallback(
    tests::TestContext& tests,
    const std::filesystem::path& consumer,
    const tests::snapshot::ModelDumps& off,
    std::string_view label,
    std::string_view reason)
{
    std::vector<std::string> report;
    std::string error;
    const std::optional<tests::snapshot::ModelDumps> on =
        tests::snapshot::compileAndDump(consumer, true, report, error);
    tests.expect(on.has_value(), std::string(label) + ": the consumer compiles: " + error);
    tests.expect(tests::snapshot::reportContains(report, reason) &&
            tests::snapshot::reportContains(report, "precompiled-snapshot: loaded=1 "),
        std::string(label) + ": the Fixture snapshot falls back with '" + std::string(reason) + "'");
    if (!on.has_value()) return;
    for (const auto& [model, dump] : off)
    {
        const std::string& other = on->at(model);
        tests.expect(dump == other, std::string(label) + ": model '" + model + "' equals snapshots off" +
            (dump == other ? std::string{} : " (" + tests::snapshot::firstDifference(dump, other) + ")"));
    }
}
}

int main()
{
    tests::TestContext tests;
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());
    const std::filesystem::path root = std::filesystem::path(NSC_BINARY_DIR) / "precompiled_snapshot_fallback";
    std::string output;
    const std::optional<std::filesystem::path> consumer = tests::snapshot::buildFixture(root, output);
    tests.expect(consumer.has_value(), "the fixture libraries emit: " + output);
    if (!consumer.has_value()) return tests.result();
    std::vector<std::string> offReport;
    std::string offError;
    const std::optional<tests::snapshot::ModelDumps> off =
        tests::snapshot::compileAndDump(*consumer, false, offReport, offError);
    tests.expect(off.has_value(), "the consumer compiles with snapshots off: " + offError);
    if (!off.has_value()) return tests.result();

    const std::filesystem::path metadataPath = root / "Fixture" / "Fixture.nsmeta";
    const std::filesystem::path sidecarPath =
        compilation::PrecompiledProductSidecarCodec::pathForMetadata(metadataPath);
    const std::string original = readBytes(sidecarPath);
    std::string error;
    tests.expect(rewriteSidecar(metadataPath, original, [](compilation::PrecompiledProductSidecar& sidecar)
        { sidecar.publicApiSnapshot.pop_back(); }, error), "truncated snapshot is written: " + error);
    expectFallback(tests, *consumer, *off, "truncated snapshot", "=decode");

    tests.expect(rewriteSidecar(metadataPath, original, [](compilation::PrecompiledProductSidecar& sidecar)
        {
            mutateSnapshot(sidecar, [](compilation::PrecompiledSemanticSnapshot& snapshot)
                {
                    for (compilation::PrecompiledSnapshotDependency& dependency : snapshot.dependencies)
                        dependency.metadataDigest = std::string(64, '0');
                });
        }, error), "snapshot with another dependency digest is written: " + error);
    expectFallback(tests, *consumer, *off, "dependency digest", "=dependency-digest:");

    tests.expect(rewriteSidecar(metadataPath, original, [](compilation::PrecompiledProductSidecar& sidecar)
        {
            mutateSnapshot(sidecar, [](compilation::PrecompiledSemanticSnapshot& snapshot)
                { snapshot.nodeKindDigest = std::string(64, '0'); });
        }, error), "snapshot with another node-kind digest is written: " + error);
    expectFallback(tests, *consumer, *off, "node kinds", "=node-kinds");

    tests.expect(rewriteSidecar(metadataPath, original, [](compilation::PrecompiledProductSidecar& sidecar)
        { sidecar.publicApiSnapshot.clear(); }, error), "v1 sidecar is written: " + error);
    expectFallback(tests, *consumer, *off, "v1 sidecar", "=absent");
    writeBytes(sidecarPath, original);
    return tests.result();
}
