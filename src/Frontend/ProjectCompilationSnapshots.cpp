#include "nativesharp/Frontend/ProjectCompilation.h"

#include "Internal/ProjectPhaseTimer.h"
#include "Snapshot/PrecompiledSnapshotValidation.h"

#include "nativesharp/Compilation/PrecompiledSemanticSnapshotCodec.h"
#include "nativesharp/Frontend/ArrayReferenceLatePathProof.h"

#include <string>
#include <algorithm>
#include <cctype>
#include <iterator>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
using TemplateParents = std::unordered_map<const ast::AstNode*, const ast::AstNode*>;

TemplateParents templateParents(const ast::AstNode* root)
{
    TemplateParents parents;
    if (!root) return parents;
    std::vector<const ast::AstNode*> pending{root};
    parents.emplace(root, nullptr);
    while (!pending.empty())
    {
        const auto* node = pending.back();
        pending.pop_back();
        const auto add = [&](const ast::AstNode* child) {
            if (child && parents.emplace(child, node).second) pending.push_back(child);
        };
        add(node->first());
        add(node->second());
        add(node->third());
        for (const auto* child : node->children()) add(child);
    }
    return parents;
}

bool earlierBlockLocal(const ast::AstNode* statement, std::string_view name)
{
    if (!statement) return false;
    if (statement->kind() == ast::AstNodeKind::VariableDeclarationStatement)
        return statement->text() == name;
    if (statement->kind() == ast::AstNodeKind::DeconstructionDeclarationStatement)
    {
        std::vector<const ast::AstNode*> pending(statement->children().begin(), statement->children().end());
        while (!pending.empty())
        {
            const auto* binding = pending.back();
            pending.pop_back();
            if (!binding) continue;
            if (binding->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                if (binding->text() != "_" && binding->text() == name) return true;
            }
            else if (binding->kind() == ast::AstNodeKind::TupleExpression)
            {
                pending.insert(pending.end(), binding->children().begin(), binding->children().end());
            }
        }
        return false;
    }
    if (statement->kind() != ast::AstNodeKind::BlockStatement ||
        statement->auxiliaryText() != "declaration-list") return false;
    for (const auto* declaration : statement->children())
        if (declaration && declaration->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
            declaration->text() == name) return true;
    return false;
}

// A candidate is removed only when an earlier declaration in an enclosing
// block proves that this unqualified use denotes a local. Other forms remain
// candidates until their own binding rules can be established.
bool provenLocal(const TemplateParents& parents, const CppFunctionDemand& demand)
{
    const auto* expression = demand.syntax;
    if (!expression) return false;
    if (demand.directCall)
    {
        if (expression->kind() != ast::AstNodeKind::CallExpression) return false;
        expression = expression->first();
    }
    if (!expression || expression->kind() != ast::AstNodeKind::IdentifierExpression ||
        expression->text() != demand.writtenName) return false;

    const auto* child = demand.syntax;
    auto current = parents.find(child);
    if (current == parents.end()) return false;
    while (current->second)
    {
        const auto* parent = current->second;
        if (parent->kind() == ast::AstNodeKind::BlockStatement)
        {
            const auto& statements = parent->children();
            const auto position = std::find(statements.begin(), statements.end(), child);
            if (position != statements.end())
                for (auto it = statements.begin(); it != position; ++it)
                    if (earlierBlockLocal(*it, demand.writtenName)) return true;
        }
        else if (parent->kind() == ast::AstNodeKind::ForStatement && child != parent->first())
        {
            // The loop variable is in scope in the condition, increment and
            // body, but not before it has been declared in the initializer.
            if (earlierBlockLocal(parent->first(), demand.writtenName)) return true;
        }
        else if (parent->kind() == ast::AstNodeKind::DeconstructionDeclarationStatement &&
                 child != parent->first())
        {
            // Pattern identifiers declare locals or discards; only the initializer is an expression.
            return true;
        }
        else if (parent->kind() == ast::AstNodeKind::CatchClause &&
                 (child == parent->first() || child == parent->second()) &&
                 parent->text() == demand.writtenName)
        {
            // The exception variable is visible in both the handler and its filter.
            return true;
        }
        else if ((parent->kind() == ast::AstNodeKind::MethodDeclaration ||
                  parent->kind() == ast::AstNodeKind::FunctionDeclaration ||
                  parent->kind() == ast::AstNodeKind::LambdaExpression) &&
                 child == parent->first())
        {
            // Parameters are children; only the callable or lambda body is their scope.
            for (const auto* parameter : parent->children())
                if (parameter && parameter->kind() == ast::AstNodeKind::ParameterDeclaration &&
                    parameter->text() == demand.writtenName) return true;
        }
        child = parent;
        current = parents.find(parent);
        if (current == parents.end()) break;
    }
    return false;
}
}

std::span<const ProjectCompilation::CppTemplateDemand> ProjectCompilation::cppTemplateDemands() const noexcept
{
    return cppTemplateDemands_;
}

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
    if (module.surfacePruned)
    {
        reason = "pruned";
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
bool ProjectCompilation::buildMemberModel(std::string& error)
{
    memberModel_ = std::make_unique<ProjectMemberModel>(lifetime_->indexes());
    if (experimentalCppLifetimeMethodProvider_)
    {
        std::vector<CppImportedLifetimeMethod> methods;
        if (!experimentalCppLifetimeMethodProvider_(*projectGraph_, cppProjectSessions_, *nameLookup_, methods, error) ||
            !memberModel_->setExperimentalCppLifetimeMethods(std::move(methods), error)) return false;
    }
    if (experimentalCppInstanceMethodProvider_)
    {
        std::vector<CppImportedInstanceMethod> methods;
        if (!experimentalCppInstanceMethodProvider_(*projectGraph_, cppProjectSessions_, *nameLookup_, methods, error) ||
            !memberModel_->setExperimentalCppInstanceMethods(std::move(methods), error)) return false;
    }
    if (experimentalCppInstanceFieldProvider_)
    {
        std::vector<CppImportedInstanceField> fields;
        if (!experimentalCppInstanceFieldProvider_(*projectGraph_, cppProjectSessions_, *nameLookup_, fields, error) ||
            !memberModel_->setExperimentalCppInstanceFields(std::move(fields), error)) return false;
    }
    if (experimentalCppStaticMethodProvider_)
    {
        std::vector<CppImportedStaticMethod> methods;
        if (!experimentalCppStaticMethodProvider_(*projectGraph_, cppProjectSessions_, *nameLookup_, methods, error) ||
            !memberModel_->setExperimentalCppStaticMethods(std::move(methods), error)) return false;
    }
    if (experimentalCppStaticFieldProvider_)
    {
        std::vector<CppImportedStaticField> fields;
        if (!experimentalCppStaticFieldProvider_(*projectGraph_, cppProjectSessions_, *nameLookup_, fields, error) ||
            !memberModel_->setExperimentalCppStaticFields(std::move(fields), error)) return false;
    }
    if (!experimentalCppLifetimeMethodProvider_ && !experimentalCppInstanceMethodProvider_ && !experimentalCppInstanceFieldProvider_ && !experimentalCppStaticMethodProvider_ && !experimentalCppStaticFieldProvider_)
        memberModel_->setPrecompiledSnapshots(precompiledSnapshots_);
    if (!experimentalCppLifetimeMethodProvider_ && !experimentalCppInstanceMethodProvider_ && !experimentalCppInstanceFieldProvider_ && !experimentalCppStaticMethodProvider_ && !experimentalCppStaticFieldProvider_ && precompiledSnapshotCapture_.has_value())
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
bool ProjectCompilation::buildNamespaceFunctionModel(std::string& error)
{
    namespaceFunctionModel_ = std::make_unique<ProjectNamespaceFunctionModel>();
    cppTemplateDemands_.clear();
    namespaceDiagnosticsBegin_ = diagnostics_.diagnostics().size();
    std::vector<NamespaceStateSymbol> importedStates = experimentalCppImportedStates_;
    if (cppFunctionProvider_ || cppGlobalProvider_)
    {
        ProjectNamespaceFunctionModel observer;
        observer.setCaptureCppFunctionDemands(true);
        observer.setPrecompiledSnapshots(precompiledSnapshots_);
        if (precompiledSnapshotCapture_.has_value())
            observer.setPrecompiledSnapshotCaptureFile(
                units_[precompiledSnapshotCapture_->unitOrdinal]->fileId());
        diagnostics::DiagnosticEngine observerDiagnostics;
        if (!observer.build(*nameLookup_, *memberModel_, units_, observerDiagnostics, genericModel_.get()))
        {
            namespaceFunctionModel_->setPrecompiledSnapshots(precompiledSnapshots_);
            if (precompiledSnapshotCapture_.has_value())
                namespaceFunctionModel_->setPrecompiledSnapshotCaptureFile(
                    units_[precompiledSnapshotCapture_->unitOrdinal]->fileId());
            namespaceFunctionModel_->build(*nameLookup_, *memberModel_, units_, diagnostics_, genericModel_.get());
            return false;
        }
        const auto observedDemands = observer.cppFunctionDemands();
        std::vector<CppFunctionDemand> effectiveDemands(observedDemands.begin(), observedDemands.end());
        if (!cppProjectSessions_.empty() && genericModel_)
        {
            std::unordered_multimap<const ast::AstNode*, const GenericTemplateRecord*> owners;
            std::unordered_map<const GenericTemplateRecord*, TemplateParents> parentIndexes;
            for (const auto& record : genericModel_->templates())
            {
                parentIndexes.emplace(&record, templateParents(record.syntax));
                for (const auto& candidate : record.foreignReferenceCandidates)
                    if (candidate.syntax && !candidate.typeSyntax)
                        owners.emplace(candidate.syntax, &record);
            }
            effectiveDemands.erase(std::remove_if(effectiveDemands.begin(), effectiveDemands.end(),
                [&](const CppFunctionDemand& demand) {
                    const auto [first, last] = owners.equal_range(demand.syntax);
                    bool hasOwner = false;
                    for (auto owner = first; owner != last; ++owner)
                    {
                        if (owner->second->projectOrdinal != demand.projectOrdinal) continue;
                        hasOwner = true;
                        if (!provenLocal(parentIndexes.at(owner->second), demand)) return false;
                    }
                    return hasOwner;
                }), effectiveDemands.end());
            for (const auto& demand : effectiveDemands)
            {
                const auto [first, last] = owners.equal_range(demand.syntax);
                for (auto owner = first; owner != last; ++owner)
                    if (owner->second->projectOrdinal == demand.projectOrdinal)
                        cppTemplateDemands_.push_back({owner->second->canonicalKey, demand});
                // EN: T10-016. An operator expression is not a foreign name of the template, so the template
                //     whose body contains it owns its operator demand.
                // ES: T10-016. Una expresion de operador no es un nombre externo de la plantilla; la plantilla
                //     cuyo cuerpo la contiene es duena de su demanda de operador.
                const auto& name = demand.lookupName;
                if (first != last || !demand.syntax || name.size() <= 8 || !name.starts_with("operator") ||
                    std::isalnum(static_cast<unsigned char>(name[8])) || name[8] == '_') continue;
                for (const auto& record : genericModel_->templates())
                    if (record.projectOrdinal == demand.projectOrdinal &&
                        parentIndexes.at(&record).contains(demand.syntax))
                        cppTemplateDemands_.push_back({record.canonicalKey, demand});
            }
        }
        // EN: T13-005. A direct call whose lookup reaches a C++ function template of a visible project, before
        //     its template arguments are deduced.
        // ES: T13-005. Una llamada directa cuya busqueda alcanza una plantilla de funcion C++ de un proyecto
        //     visible, antes de deducir sus argumentos de plantilla.
        if ((cppTemplateDeductionRound_ < kCppTemplateDeductionRounds &&
             (!cppFunctionTemplates_.empty() || !cppFunctionMacros_.empty())) ||
            !cppUnimportedMacros_.empty())
        {
            std::unordered_map<const ast::AstNode*, CppTemplateCallSite> sites;
            for (const auto& demand : effectiveDemands)
            {
                if (!demand.directCall || !demand.syntax || demand.lookupName.empty()) continue;
                // T14-003: a macro is a name of the global scope of the translation unit. / Una macro es global.
                bool macro = false;
                if (std::find(demand.lookupNamespaces.begin(), demand.lookupNamespaces.end(), "") !=
                    demand.lookupNamespaces.end())
                    for (const auto project : demand.visibleProjects)
                        if (const auto macros = cppUnimportedMacros_.find(project); macros != cppUnimportedMacros_.end())
                            if (const auto found = macros->second.find(demand.lookupName); found != macros->second.end())
                            {
                                sites.emplace(demand.syntax, CppTemplateCallSite{project, {demand.lookupName}, found->second});
                                macro = true;
                                break;
                            }
                // EN: T19-003. Before the expansion, a call to a C function-like macro records its argument types like
                //     a call to a function template; afterwards it calls the wrappers imported under the macro's name.
                // ES: T19-003. Antes de la expansion, una llamada a una macro de funcion C registra los tipos de sus
                //     argumentos como una llamada a una plantilla; despues llama a los envoltorios con el nombre de la
                //     macro.
                if (!macro && std::find(demand.lookupNamespaces.begin(), demand.lookupNamespaces.end(), "") !=
                    demand.lookupNamespaces.end())
                    for (const auto project : demand.visibleProjects)
                        if (const auto macros = cppFunctionMacros_.find(project);
                            macros != cppFunctionMacros_.end() && macros->second.contains(demand.lookupName))
                        {
                            if (cppTemplateCallPending(demand.syntax))
                            {
                                CppTemplateCallSite site{project, {demand.lookupName}, {}, true, {}};
                                if (const auto tokens = cppFunctionMacroTokens_.find(project);
                                    tokens != cppFunctionMacroTokens_.end())
                                    if (const auto found = tokens->second.find(demand.lookupName);
                                        found != tokens->second.end())
                                        site.tokenParameters = found->second;
                                sites.emplace(demand.syntax, std::move(site));
                            }
                            macro = true;
                            break;
                        }
                if (macro || !cppTemplateCallPending(demand.syntax)) continue;
                for (const auto project : demand.visibleProjects)
                {
                    const auto templates = cppFunctionTemplates_.find(project);
                    if (templates == cppFunctionTemplates_.end()) continue;
                    CppTemplateCallSite site;
                    site.projectOrdinal = project;
                    for (const auto& ns : demand.lookupNamespaces)
                    {
                        std::string qualified;
                        for (const char c : ns) qualified += c == '.' ? std::string("::") : std::string(1, c);
                        if (!qualified.empty()) qualified += "::";
                        qualified += demand.lookupName;
                        if (templates->second.contains(qualified) &&
                            std::find(site.qualifiedNames.begin(), site.qualifiedNames.end(), qualified) ==
                                site.qualifiedNames.end())
                            site.qualifiedNames.push_back(std::move(qualified));
                    }
                    if (!site.qualifiedNames.empty()) { sites.emplace(demand.syntax, std::move(site)); break; }
                }
            }
            namespaceFunctionModel_->setCppTemplateCallSites(std::move(sites));
        }
        // T19-040: the calls that call their own macro wrapper. / Las llamadas que llaman su propio envoltorio.
        namespaceFunctionModel_->setCppMacroCallWrappers(cppMacroCallWrappers_);
        if (cppFunctionProvider_)
        {
        std::vector<NamespaceFunctionSymbol> functions;
        std::vector<interop::CppDeferredFunctionCandidate> deferred;
        if (!cppFunctionProvider_(*projectGraph_, cppProjectSessions_, units_,
                                  effectiveDemands, functions, deferred, error))
            return false;
        for (const auto& candidate : deferred)
        {
            const auto& demands = effectiveDemands;
            const auto demand = std::find_if(demands.begin(), demands.end(), [&](const auto& item) {
                if (item.syntax != candidate.syntax || item.projectOrdinal != candidate.consumerProject ||
                    item.directCall != candidate.directCall ||
                    std::find(item.visibleProjects.begin(), item.visibleProjects.end(), candidate.declarationProject) ==
                        item.visibleProjects.end()) return false;
                // T10-016: an operator demand is looked up in the namespaces associated with its operands,
                // which the C++ provider adds; the declaration only has to be that operator.
                const auto& name = item.lookupName;
                if (name.size() > 8 && name.starts_with("operator") &&
                    !std::isalnum(static_cast<unsigned char>(name[8])) && name[8] != '_')
                    return candidate.declaration.name == name;
                for (const auto& ns : item.lookupNamespaces)
                {
                    std::string qualified;
                    for (const char c : ns) qualified += c == '.' ? "::" : std::string(1, c);
                    if (!qualified.empty()) qualified += "::";
                    qualified += item.lookupName;
                    if (qualified == candidate.declaration.qualifiedName) return true;
                }
                return false;
            });
            const auto validReference = [&](std::size_t project, const std::filesystem::path& file,
                                            std::size_t reference) {
                if (project >= projectGraph_->projects().size()) return false;
                std::error_code ownerError;
                const auto& owner = projectGraph_->projects()[project].projectFile();
                if (!std::filesystem::equivalent(file, owner, ownerError) || ownerError) return false;
                for (const auto& group : cppProjectSessions_)
                {
                    std::error_code groupError;
                    if (std::filesystem::equivalent(group.projectFile, owner, groupError) && !groupError &&
                        reference < group.sessions.size()) return true;
                }
                return false;
            };
            bool validOrigins = validReference(candidate.declarationProject, candidate.declarationProjectFile,
                                                candidate.referenceIndex);
            for (const auto& origin : candidate.conflictingOrigins)
            {
                if (origin.declarationProject != candidate.declarationProject ||
                    origin.declaration.qualifiedName.empty() || origin.declaration.canonicalFunctionType.empty() ||
                    !validReference(origin.declarationProject, origin.declarationProjectFile, origin.referenceIndex))
                    validOrigins = false;
            }
            if (demand == demands.end() || !validOrigins)
            {
                error = "deferred C/C++ function does not belong to a visible demand";
                return false;
            }
        }
        if (!namespaceFunctionModel_->setCppImportedFunctions(std::move(functions), error)) return false;
        if (!namespaceFunctionModel_->setCppDeferredFunctions(std::move(deferred), error)) return false;
        }
        else if (!cppImportedFunctions_.empty() &&
                 !namespaceFunctionModel_->setCppImportedFunctions(cppImportedFunctions_, error)) return false;
        if (cppGlobalProvider_)
        {
            std::vector<NamespaceStateSymbol> globals;
            if (!cppGlobalProvider_(*projectGraph_, cppProjectSessions_, effectiveDemands,
                                    globals, error)) return false;
            importedStates.insert(importedStates.end(),
                                  std::make_move_iterator(globals.begin()), std::make_move_iterator(globals.end()));
        }
    }
    else if (!cppImportedFunctions_.empty())
    {
        if (!namespaceFunctionModel_->setCppImportedFunctions(cppImportedFunctions_, error))
            return false;
    }
    if (!namespaceFunctionModel_->setCppImportedStates(std::move(importedStates), error)) return false;
    namespaceFunctionModel_->setPrecompiledSnapshots(precompiledSnapshots_);
    if (precompiledSnapshotCapture_.has_value())
    {
        namespaceFunctionModel_->setPrecompiledSnapshotCaptureFile(
            units_[precompiledSnapshotCapture_->unitOrdinal]->fileId());
    }
    if (!namespaceFunctionModel_->build(*nameLookup_, *memberModel_, units_, diagnostics_, genericModel_.get()))
        return false;
    namespaceDiagnosticsEnd_ = diagnostics_.diagnostics().size();
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
    // EN: X3-04. A class that implements an abstract C++ interface declares, for each pure virtual method, one public
    //     instance method of that name with the NativeSharp signature of the imported C++ method.
    // ES: X3-04. Una clase que implementa una interfaz C++ abstracta declara, por cada metodo virtual puro, un metodo
    //     publico de instancia con ese nombre y la firma NativeSharp del metodo C++ importado.
    bool implemented = true;
    if (memberModel_ != nullptr)
        for (const auto& field : memberModel_->members())
        {
            if (!field.cppInterfaceTypeOrdinal || field.name != "__nsc_cpp_interface_vptr") continue;
            const auto* interface = nameLookup_->cppImportedType(*field.cppInterfaceTypeOrdinal);
            const auto& owner = nameLookup_->namedTypes()[field.declaringTypeOrdinal];
            const auto range = owner.parts.empty() || owner.parts.front().syntax == nullptr
                ? SourceRange{} : owner.parts.front().syntax->range();
            const auto report = [&](const std::string& message) {
                diagnostics_.report(diagnostics::DiagnosticId::InvalidBaseType, diagnostics::DiagnosticSeverity::Error,
                    range, "class '" + owner.identity.key() + "' implements the C++ interface '" +
                        interface->identity.fullyQualifiedName() + "': " + message);
                implemented = false;
            };
            // X3-01: without a default constructor the C++ object is built by `base(args)` only. / Sin constructor
            //     por defecto, el objeto C++ solo lo construye `base(args)`.
            if (!interface->cppInterfaceDefaultConstructible)
            {
                bool constructors = false;
                for (const auto ordinal : memberModel_->membersOfType(field.declaringTypeOrdinal))
                {
                    const auto& member = memberModel_->members()[ordinal];
                    if (member.kind != ClassMemberKind::Constructor || member.isStatic) continue;
                    constructors = true;
                    const auto* initializer = member.syntax != nullptr ? member.syntax->second() : nullptr;
                    if (initializer == nullptr || initializer->kind() != ast::AstNodeKind::BaseConstructorInitializer)
                        report("its constructor '" + member.signatureKey + "' must call base(...): the C++ class has no "
                               "accessible default constructor");
                }
                if (!constructors)
                    report("it must declare a constructor that calls base(...): the C++ class has no accessible default "
                           "constructor");
            }
            for (const auto& name : interface->cppInterfaceMethods)
            {
                const ClassMemberSymbol* cpp = nullptr;
                for (const auto ordinal : memberModel_->findMembers(*field.cppInterfaceTypeOrdinal, name))
                    if (ordinal < memberModel_->members().size() &&
                        memberModel_->members()[ordinal].kind == ClassMemberKind::Method &&
                        !memberModel_->members()[ordinal].isStatic) cpp = &memberModel_->members()[ordinal];
                const bool required = std::find(interface->cppInterfacePureMethods.begin(),
                    interface->cppInterfacePureMethods.end(), name) != interface->cppInterfacePureMethods.end();
                if (cpp == nullptr && !required && memberModel_->findMembers(field.declaringTypeOrdinal, name).empty())
                    continue;
                if (cpp == nullptr) { report("its method '" + name + "' has no NativeSharp signature"); continue; }
                const ClassMemberSymbol* own = nullptr;
                std::size_t count = 0;
                for (const auto ordinal : memberModel_->findMembers(field.declaringTypeOrdinal, name))
                    if (ordinal < memberModel_->members().size() &&
                        memberModel_->members()[ordinal].kind == ClassMemberKind::Method)
                    { own = &memberModel_->members()[ordinal]; ++count; }
                // A virtual method with a C++ body may be left to it. / Un virtual con cuerpo C++ puede quedarse asi.
                if (count == 0 && std::find(interface->cppInterfacePureMethods.begin(),
                        interface->cppInterfacePureMethods.end(), name) == interface->cppInterfacePureMethods.end())
                    continue;
                const bool same = own != nullptr && count == 1 && !own->isStatic &&
                    own->accessibility == ClassMemberAccessibility::Public &&
                    own->declaredTypeKey == cpp->declaredTypeKey && own->parameters.size() == cpp->parameters.size() &&
                    std::equal(own->parameters.begin(), own->parameters.end(), cpp->parameters.begin(),
                        [](const auto& left, const auto& right) {
                            return left.typeKey == right.typeKey && left.passKind == right.passKind;
                        });
                if (!same)
                    report("it must declare one public instance method '" + name + "' with the signature '" +
                        cpp->signatureKey + "' returning '" + cpp->declaredTypeKey + "'");
            }
        }
    return built && implemented;
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
    constructionModel_->setVirtualDispatchModel(virtualDispatchModel_.get());
    constructionModel_->setPrecompiledSnapshots(precompiledSnapshots_);
    std::vector<bool> guardedConstructors(nameLookup_->namedTypes().size(), false);
    std::vector<bool> guardedDestructors(nameLookup_->namedTypes().size(), false);
    for (std::size_t moduleIndex = 0; moduleIndex < precompiledModules_.size(); ++moduleIndex)
    {
        if (moduleIndex >= precompiledApiUnits_.size() || !precompiledApiUnits_[moduleIndex].has_value()) continue;
        const std::size_t unitOrdinal = *precompiledApiUnits_[moduleIndex];
        if (unitOrdinal >= units_.size() || units_[unitOrdinal] == nullptr) continue;
        const FileId file = units_[unitOrdinal]->fileId();
        for (const auto& record : precompiledModules_[moduleIndex].guardedReceiverContracts)
        {
            const auto ordinal = nameLookup_->typeOrdinalForIdentity(record.identity);
            if (!ordinal.has_value() || *ordinal >= nameLookup_->namedTypes().size()) continue;
            const auto& type = nameLookup_->namedTypes()[*ordinal];
            if (!type.isPrecompiled || type.kind != NamedTypeDeclarationKind::Class || type.parts.empty()) continue;
            bool sameUnit = true;
            for (const auto& part : type.parts)
                if (!part.isPrecompiled || part.fileId != file) { sameUnit = false; break; }
            if (!sameUnit) continue;
            if (record.kind == ReceiverContractKind::Constructor) guardedConstructors[*ordinal] = true;
            if (record.kind == ReceiverContractKind::Destructor) guardedDestructors[*ordinal] = true;
        }
    }
    constructionModel_->setGuardedReceiverContracts(std::move(guardedConstructors), std::move(guardedDestructors));
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
    if (!built || diagnostics_.hasErrors()) return false;
    reportArrayReferenceLatePaths(*this, methodBodyModel_->arrayReferenceReplayObservations(), diagnostics_);
    return !diagnostics_.hasErrors();
}

// EN: Builds the object layout model with the accepted snapshots (plan 3.12) and, in the reference
//     compilation of a producer, with the FileId of the captured API unit.
// ES: Construye el modelo de layout de objetos con los snapshots aceptados (plan 3.12) y, en la
//     compilación de referencia de un productor, con el FileId de la unidad de API capturada.
bool ProjectCompilation::buildObjectLayoutModel()
{
    objectLayoutModel_ = std::make_unique<ProjectObjectLayoutModel>(lifetime_->indexes());
    objectLayoutModel_->setPrecompiledSnapshots(precompiledSnapshots_);
    if (precompiledSnapshotCapture_.has_value())
        objectLayoutModel_->setPrecompiledSnapshotCaptureFile(precompiledSnapshotCapture_->fileId);
    bool built = objectLayoutModel_->build(
        *nameLookup_, *memberModel_, *inheritanceModel_, *constructionModel_, diagnostics_);
    // EN: T13-007. A NativeSharp struct mirrored for a C++ template has, in NativeSharp, the size, alignment and
    //     field offsets Clang gave its mirror; otherwise the C++ code would read it with another layout.
    // ES: T13-007. Un struct NativeSharp reflejado para una plantilla C++ tiene, en NativeSharp, el tamano, la
    //     alineacion y los offsets de campo que Clang dio a su espejo; si no, el codigo C++ lo leeria con otro.
    for (const auto& [key, mirror] : cppNativeMirrorLayouts_)
    {
        const auto ordinal = built ? nameLookup_->typeOrdinalForIdentity(key) : std::nullopt;
        const auto* layout = ordinal ? objectLayoutModel_->structLayoutForType(*ordinal) : nullptr;
        if (!built || (layout && layout->size == mirror.sizeBytes && layout->alignment == mirror.alignmentBytes &&
            layout->fields.size() == mirror.fieldOffsets.size() &&
            std::equal(layout->fields.begin(), layout->fields.end(), mirror.fieldOffsets.begin(),
                [](const auto& field, std::uint64_t offset) { return field.offset == offset; })))
            continue;
        diagnostics_.report(diagnostics::DiagnosticId::InvalidType, diagnostics::DiagnosticSeverity::Error,
            SourceRange{}, "NativeSharp struct '" + key + "' does not have the layout of its C++ template argument "
            "mirror (" + std::to_string(mirror.sizeBytes) + " bytes, alignment " +
            std::to_string(mirror.alignmentBytes) + ")");
        built = false;
    }
    if (reportSink_ && !precompiledSnapshots_.empty())
    {
        reportSink_("precompiled-snapshot-install: layout-structs=" +
            std::to_string(objectLayoutModel_->installedSnapshotStructCount()) + " layout-classes=" +
            std::to_string(objectLayoutModel_->installedSnapshotClassCount()));
    }
    return built;
}
}
