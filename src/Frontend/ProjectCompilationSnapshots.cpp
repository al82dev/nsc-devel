#include "nativesharp/Frontend/ProjectCompilation.h"

#include "Internal/ProjectPhaseTimer.h"
#include "Snapshot/PrecompiledSnapshotValidation.h"

#include "nativesharp/Compilation/PrecompiledSemanticSnapshotCodec.h"

#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
void ProjectCompilation::setReportSink(std::function<void(std::string_view)> sink)
{
    reportSink_ = std::move(sink);
}

std::span<const PrecompiledSnapshotUnit> ProjectCompilation::precompiledSnapshots() const noexcept
{
    return precompiledSnapshots_;
}

// EN: Plan item 1.7 (subphase `precompiled-snapshot`). For each precompiled module with a restored
//     public API unit: decode its snapshot, check it against the loaded dependencies and the
//     restored tree, and keep it when it passes. A module without a usable snapshot takes today's
//     path, without a diagnostic. With --time the report line gives the loaded and fallback counts
//     and the reason of every fallback.
// ES: Punto 1.7 del plan (subfase `precompiled-snapshot`). Para cada módulo precompilado con una
//     unidad de API pública restaurada: decodifica su snapshot, lo comprueba contra las
//     dependencias cargadas y el árbol restaurado, y lo conserva si pasa. Un módulo sin snapshot
//     utilizable va por la ruta de hoy, sin diagnóstico. Con --time la línea de reporte da las
//     cuentas de cargados y de respaldos y la razón de cada respaldo.
void ProjectCompilation::loadPrecompiledSnapshots()
{
    const internal::ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "precompiled-snapshot");
    precompiledSnapshots_.clear();
    std::vector<compilation::PrecompiledSnapshotDependency> loadedDependencies;
    loadedDependencies.reserve(precompiledModules_.size());
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
        loadedDependencies.push_back({module.moduleName, module.metadataDigest});
    std::size_t fallbacks = 0;
    std::string fallbackReasons;
    for (std::size_t moduleIndex = 0; moduleIndex < precompiledModules_.size(); ++moduleIndex)
    {
        if (moduleIndex >= precompiledApiUnits_.size() || !precompiledApiUnits_[moduleIndex].has_value())
            continue;
        const PrecompiledSyntaxModule& module = precompiledModules_[moduleIndex];
        std::string reason;
        if (loadPrecompiledSnapshot(module, *precompiledApiUnits_[moduleIndex], loadedDependencies, reason))
            continue;
        ++fallbacks;
        fallbackReasons += " " + module.moduleName + "=" + reason;
    }
    if (reportSink_)
    {
        reportSink_("precompiled-snapshot: loaded=" + std::to_string(precompiledSnapshots_.size()) +
            " fallback=" + std::to_string(fallbacks) + fallbackReasons);
    }
}

bool ProjectCompilation::loadPrecompiledSnapshot(
    const PrecompiledSyntaxModule& module,
    std::size_t unitOrdinal,
    std::span<const compilation::PrecompiledSnapshotDependency> loadedDependencies,
    std::string& reason)
{
    if (!projectLoadOptions_.usePrecompiledSnapshots)
    {
        reason = "disabled";
        return false;
    }
    if (module.publicApiSnapshot.empty())
    {
        reason = "absent";
        return false;
    }
    PrecompiledSnapshotUnit accepted;
    accepted.unitOrdinal = unitOrdinal;
    accepted.moduleName = module.moduleName;
    std::string decodeError;
    if (!compilation::PrecompiledSemanticSnapshotCodec::decode(
            module.publicApiSnapshot, accepted.snapshot, decodeError))
    {
        reason = "decode";
        return false;
    }
    accepted.index.build(units_[unitOrdinal]->syntaxTree());
    if (!validatePrecompiledSnapshot(accepted.snapshot, loadedDependencies, accepted.index, reason))
        return false;
    precompiledSnapshots_.push_back(std::move(accepted));
    return true;
}
}
