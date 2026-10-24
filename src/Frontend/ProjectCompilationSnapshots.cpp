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

std::span<const PrecompiledSyntaxModule> ProjectCompilation::precompiledModules() const noexcept
{
    return precompiledModules_;
}

void ProjectCompilation::setPrecompiledSnapshotCaptureModule(std::string moduleName)
{
    precompiledSnapshotCaptureModule_ = std::move(moduleName);
}

const PrecompiledSnapshotUnit* ProjectCompilation::precompiledSnapshotCapture() const noexcept
{
    return precompiledSnapshotCapture_.has_value() ? &*precompiledSnapshotCapture_ : nullptr;
}

const compilation::ProjectLoadOptions& ProjectCompilation::projectLoadOptions() const noexcept
{
    return projectLoadOptions_;
}

std::size_t ProjectCompilation::compilationJobs() const noexcept
{
    return compilationJobs_;
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
    precompiledSnapshotCapture_.reset();
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
        if (!precompiledSnapshotCaptureModule_.empty() && module.moduleName == precompiledSnapshotCaptureModule_)
        {
            PrecompiledSnapshotUnit& capture = precompiledSnapshotCapture_.emplace();
            capture.unitOrdinal = *precompiledApiUnits_[moduleIndex];
            capture.fileId = units_[capture.unitOrdinal]->fileId();
            capture.moduleName = module.moduleName;
            capture.index.build(units_[capture.unitOrdinal]->syntaxTree());
        }
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
    accepted.fileId = units_[unitOrdinal]->fileId();
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

// EN: Builds the member model with the accepted snapshots (plan 3.3) and, in the reference
//     compilation of a producer, with the FileId of the captured API unit.
// ES: Construye el modelo de miembros con los snapshots aceptados (plan 3.3) y, en la compilación
//     de referencia de un productor, con el FileId de la unidad de API capturada.
bool ProjectCompilation::buildMemberModel()
{
    memberModel_ = std::make_unique<ProjectMemberModel>(lifetime_->indexes());
    memberModel_->setPrecompiledSnapshots(precompiledSnapshots_);
    if (precompiledSnapshotCapture_.has_value())
        memberModel_->setPrecompiledSnapshotCaptureFile(units_[precompiledSnapshotCapture_->unitOrdinal]->fileId());
    if (!memberModel_->build(*nameLookup_, units_, diagnostics_, genericModel_.get())) return false;
    if (reportSink_ && !precompiledSnapshots_.empty())
    {
        reportSink_("precompiled-snapshot-install: members=" +
            std::to_string(memberModel_->installedSnapshotMemberCount()));
    }
    return true;
}

// EN: Builds the namespace function model with the accepted snapshots (plan 3.4) and, in the
//     reference compilation of a producer, with the FileId of the captured API unit.
// ES: Construye el modelo de funciones de namespace con los snapshots aceptados (plan 3.4) y, en la
//     compilación de referencia de un productor, con el FileId de la unidad de API capturada.
bool ProjectCompilation::buildNamespaceFunctionModel()
{
    namespaceFunctionModel_ = std::make_unique<ProjectNamespaceFunctionModel>();
    namespaceFunctionModel_->setPrecompiledSnapshots(precompiledSnapshots_);
    if (precompiledSnapshotCapture_.has_value())
    {
        namespaceFunctionModel_->setPrecompiledSnapshotCaptureFile(
            units_[precompiledSnapshotCapture_->unitOrdinal]->fileId());
    }
    if (!namespaceFunctionModel_->build(*nameLookup_, *memberModel_, units_, diagnostics_, genericModel_.get()))
        return false;
    if (reportSink_ && !precompiledSnapshots_.empty())
    {
        reportSink_("precompiled-snapshot-install: namespace-functions=" +
            std::to_string(namespaceFunctionModel_->installedSnapshotFunctionCount()) + " namespace-states=" +
            std::to_string(namespaceFunctionModel_->installedSnapshotStateCount()));
    }
    return true;
}

// EN: Builds the inheritance model with the accepted snapshots (plan 3.6) and, in the reference
//     compilation of a producer, with the FileId of the captured API unit.
// ES: Construye el modelo de herencia con los snapshots aceptados (plan 3.6) y, en la compilación
//     de referencia de un productor, con el FileId de la unidad de API capturada.
bool ProjectCompilation::buildInheritanceModel()
{
    inheritanceModel_ = std::make_unique<ProjectInheritanceModel>();
    inheritanceModel_->setPrecompiledSnapshots(precompiledSnapshots_);
    if (precompiledSnapshotCapture_.has_value())
        inheritanceModel_->setPrecompiledSnapshotCaptureFile(precompiledSnapshotCapture_->fileId);
    const bool built = inheritanceModel_->build(*nameLookup_, diagnostics_);
    if (reportSink_ && !precompiledSnapshots_.empty())
    {
        reportSink_("precompiled-snapshot-install: classes=" +
            std::to_string(inheritanceModel_->installedSnapshotClassCount()));
    }
    return built;
}

// EN: Builds the interface model with the accepted snapshots (plan 3.7) and, in the reference
//     compilation of a producer, with the FileId of the captured API unit.
// ES: Construye el modelo de interfaces con los snapshots aceptados (plan 3.7) y, en la compilación
//     de referencia de un productor, con el FileId de la unidad de API capturada.
bool ProjectCompilation::buildInterfaceModel()
{
    interfaceModel_ = std::make_unique<ProjectInterfaceModel>();
    interfaceModel_->setPrecompiledSnapshots(precompiledSnapshots_);
    if (precompiledSnapshotCapture_.has_value())
        interfaceModel_->setPrecompiledSnapshotCaptureFile(precompiledSnapshotCapture_->fileId);
    const bool built = interfaceModel_->build(*nameLookup_, *memberModel_, *inheritanceModel_, diagnostics_);
    if (reportSink_ && !precompiledSnapshots_.empty())
    {
        reportSink_("precompiled-snapshot-install: interfaces=" +
            std::to_string(interfaceModel_->installedSnapshotInterfaceCount()) + " implementing-types=" +
            std::to_string(interfaceModel_->installedSnapshotImplementingTypeCount()));
    }
    return built;
}

// EN: Builds the virtual dispatch model with the accepted snapshots (plan 3.9) and, in the
//     reference compilation of a producer, with the FileId of the captured API unit.
// ES: Construye el modelo de despacho virtual con los snapshots aceptados (plan 3.9) y, en la
//     compilación de referencia de un productor, con el FileId de la unidad de API capturada.
bool ProjectCompilation::buildVirtualDispatchModel()
{
    virtualDispatchModel_ = std::make_unique<ProjectVirtualDispatchModel>(lifetime_->indexes());
    virtualDispatchModel_->setPrecompiledSnapshots(precompiledSnapshots_);
    if (precompiledSnapshotCapture_.has_value())
        virtualDispatchModel_->setPrecompiledSnapshotCaptureFile(precompiledSnapshotCapture_->fileId);
    const bool built = virtualDispatchModel_->build(*nameLookup_, *memberModel_, *inheritanceModel_, diagnostics_);
    if (reportSink_ && !precompiledSnapshots_.empty())
    {
        reportSink_("precompiled-snapshot-install: dispatch-classes=" +
            std::to_string(virtualDispatchModel_->installedSnapshotClassCount()));
    }
    return built;
}

// EN: Builds the destruction model with the accepted snapshots (plan 3.10) and, in the reference
//     compilation of a producer, with the FileId of the captured API unit.
// ES: Construye el modelo de destrucción con los snapshots aceptados (plan 3.10) y, en la
//     compilación de referencia de un productor, con el FileId de la unidad de API capturada.
bool ProjectCompilation::buildDestructionModel()
{
    destructionModel_ = std::make_unique<ProjectDestructionModel>();
    destructionModel_->setPrecompiledSnapshots(precompiledSnapshots_);
    if (precompiledSnapshotCapture_.has_value())
        destructionModel_->setPrecompiledSnapshotCaptureFile(precompiledSnapshotCapture_->fileId);
    const bool built = destructionModel_->build(*nameLookup_, *memberModel_, *inheritanceModel_, diagnostics_);
    if (reportSink_ && !precompiledSnapshots_.empty())
    {
        reportSink_("precompiled-snapshot-install: destruction-classes=" +
            std::to_string(destructionModel_->installedSnapshotClassCount()));
    }
    return built;
}

// EN: Builds the construction model with the accepted snapshots (plan 3.11) and, in the reference
//     compilation of a producer, with the FileId of the captured API unit.
// ES: Construye el modelo de construcción con los snapshots aceptados (plan 3.11) y, en la
//     compilación de referencia de un productor, con el FileId de la unidad de API capturada.
bool ProjectCompilation::buildConstructionModel()
{
    constructionModel_ = std::make_unique<ProjectConstructionModel>();
    constructionModel_->setPrecompiledSnapshots(precompiledSnapshots_);
    if (precompiledSnapshotCapture_.has_value())
        constructionModel_->setPrecompiledSnapshotCaptureFile(precompiledSnapshotCapture_->fileId);
    const bool built = constructionModel_->build(
        *nameLookup_, *memberModel_, *inheritanceModel_, *interfaceModel_, *methodBodyModel_,
        *namespaceFunctionModel_, units_, diagnostics_);
    if (reportSink_ && !precompiledSnapshots_.empty())
    {
        reportSink_("precompiled-snapshot-install: construction-classes=" +
            std::to_string(constructionModel_->installedSnapshotClassCount()));
    }
    return built;
}
}
