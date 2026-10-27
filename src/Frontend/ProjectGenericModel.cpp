#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "Internal/CloneDepthGuard.h"
#include "Cache/CachePayloadReader.h"
#include "Internal/EnumConstResolver.h"
#include "Internal/SpecializationWaveTimer.h"
#include "Internal/InterfaceFingerprint.h"
#include "Internal/ImplementationFingerprint.h"
#include "Internal/LocatedNode.h"
#include "Internal/MirFunctionKey.h"
#include "Internal/MirCacheRecord.h"
#include "Internal/ProjectGenericDependencyFrame.h"
#include "Internal/ProjectGenericBodyFrame.h"
#include "Internal/ProjectGenericAliasFrame.h"
#include "Internal/ProjectGenericChildWork.h"
#include "Internal/ProjectGenericCollectFrame.h"
#include "Internal/ProjectGenericProcessFrame.h"
#include "Internal/ProjectGenericReservedDemand.h"
#include "Internal/TypeWork.h"
#include "nativesharp/Frontend/ProjectOpaqueResultModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "Generics/GenericModelSupport.h"
#include "Generics/Support/TemplateClassification.h"
#include "Generics/Support/TemplateSpelling.h"
#include "Generics/Support/ConstGenericArguments.h"
#include "Generics/Support/GenericPatterns.h"
#include "Generics/Cache/TemplateCatalogCodec.h"
namespace nativesharp::frontend
{
using internal::appendOwner;
using internal::computeTemplateSourceFingerprint;
using internal::constArgumentExpression;
using internal::constGenericParameterTypeKey;
using internal::decodeConstGenericArgument;
using internal::deserializeAstNodeTree;
using internal::enclosingTypeSpellings;
using internal::genericArityAccepts;
using internal::genericAritySpelling;
using internal::genericTemplateAccessibility;
using internal::genericTypeTemplateKind;
using internal::isGenericMethodTemplateNode;
using internal::isGenericTypeTemplateNode;
using internal::isNamedTypeDeclaration;
using internal::isStructuralGenericSurface;
using internal::methodSpecializationFamilyKey;
using internal::normalizeConstExpressionArgument;
using internal::qualifiedName;
using internal::receiverSourceSpelling;
using internal::restoreCatalogEntry;
using internal::serializeAstNodeTree;
using internal::serializeCatalogEntry;
using internal::serializeMirCacheRecord;
using internal::splitDotted;
using internal::stableHashSuffix;
using internal::templateCacheKey;
using internal::templateSourceSpelling;
using internal::trailingPackIndex;
using internal::valueParameterCount;
using internal::CachePayloadReader;
using internal::EnumConstResolver;
using internal::MirCacheRecord;
using internal::SpecializationWaveTimer;
#if 0 // INSTRUMENTATION CODE
using internal::genericNodeDetail;
using internal::genericTypeDetail;
using internal::genericUnitDetail;
using internal::GenericWorkProgress;
#endif // INSTRUMENTATION CODE
ProjectGenericModel::GenericLookupScope::ValueBinding::ValueBinding(
                std::string bindingName,
                const ast::TypeSyntax* bindingType,
                std::shared_ptr<ValueBinding> bindingParent)
                : name(std::move(bindingName)), type(bindingType), parent(std::move(bindingParent))
            {
            }

ProjectGenericModel::GenericLookupScope::ValueBinding::~ValueBinding() noexcept
            {
                // A persistent scope can contain thousands of fields.  Letting shared_ptr destroy
                // the unique tail recursively consumes one native stack frame per binding.  Peel
                // that tail here so each node is destroyed with an empty parent and teardown stays
                // iterative; stop as soon as an ancestor is still shared by another scope.
                while (parent && parent.use_count() == 1)
                {
                    std::shared_ptr<ValueBinding> ancestor = std::move(parent);
                    parent = std::move(ancestor->parent);
                    ancestor->parent.reset();
                }
            }

void ProjectGenericModel::GenericLookupScope::addValueType(std::string name, const ast::TypeSyntax* type)
        {
            valueTypes = std::make_shared<ValueBinding>(
                std::move(name), type, std::move(valueTypes));
        }

const ast::TypeSyntax* ProjectGenericModel::GenericLookupScope::findValueType(std::string_view name) const noexcept
        {
            for (const ValueBinding* binding = valueTypes.get(); binding != nullptr;
                 binding = binding->parent.get())
            {
                if (binding->name == name) return binding->type;
            }
            return nullptr;
        }

void ProjectGenericModel::SpecializationContext::save(const ProjectGenericModel& model)
        {
            projectOrdinal = model.currentInstantiationProjectOrdinal_;
            namespaceName = model.currentInstantiationNamespace_;
            range = model.currentInstantiationRange_;
            overloadGroup = model.currentOverloadGroup_;
            packSubstitutions = model.activePackSubstitutions_;
            packError = model.packMaterializationError_;
            packErrorRange = model.packMaterializationErrorRange_;
            associatedProjections = model.activeAssociatedProjections_;
            associatedError = model.associatedProjectionError_;
            associatedErrorRange = model.associatedProjectionErrorRange_;
        }

void ProjectGenericModel::SpecializationContext::restore(ProjectGenericModel& model) const
        {
            model.currentInstantiationProjectOrdinal_ = projectOrdinal;
            model.currentInstantiationNamespace_ = namespaceName;
            model.currentInstantiationRange_ = range;
            model.currentOverloadGroup_ = overloadGroup;
            model.activePackSubstitutions_ = packSubstitutions;
            model.packMaterializationError_ = packError;
            model.packMaterializationErrorRange_ = packErrorRange;
            model.activeAssociatedProjections_ = associatedProjections;
            model.associatedProjectionError_ = associatedError;
            model.associatedProjectionErrorRange_ = associatedErrorRange;
        }

std::optional<std::size_t> ProjectGenericModel::SpecializationPlanner::request(const std::string& key) const
        {
            const auto found = byKey.find(key);
            return found == byKey.end() ? std::optional<std::size_t>{} : found->second;
        }

void ProjectGenericModel::SpecializationPlanner::commit(const std::string& key, std::size_t ordinal)
        {
            byKey.emplace(key, ordinal);
        }

void ProjectGenericModel::SpecializationPlanner::rollback(const std::string& key)
        {
            byKey.erase(key);
        }
void ProjectGenericModel::expandAliasesForBuild(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // Item 276. Generic aliases are transparent source spellings, so erase them before collecting
    // templates or dependency edges.  Doing it later would let `Alias<int>` acquire a bogus
    // specialization identity and would leave aliases inside template bodies until after cloning.
    const GenericTelemetry::Scope phase(telemetry_, "generics.alias-expansion");
    for (std::size_t unitIndex = 0; unitIndex < units.size(); ++unitIndex)
    {
        const std::unique_ptr<ProjectSyntaxUnit>& unitPtr = units[unitIndex];
        if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
#if 0 // INSTRUMENTATION CODE
        const GenericWorkProgress progress(
            telemetry_, "generics.alias-expansion.unit", genericUnitDetail(*unitPtr),
            unitIndex + 1U, units.size());
#endif // INSTRUMENTATION CODE
        expandAliasesInNode(
            *unitPtr, unitPtr->syntaxTree(), "", GenericLookupScope{}, diagnostics);
    }
}

bool ProjectGenericModel::mergeTraitImplementationsForBuild(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const GenericTelemetry::Scope phase(telemetry_, "generics.trait-merge");
    if (!mergeTraitImplementations(units, diagnostics))
    {
        built_ = true;
        return false;
    }
    return true;
}

bool ProjectGenericModel::buildOpaqueResultsForBuild(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const GenericTelemetry::Scope phase(telemetry_, "generics.opaque-results");
    if (!ProjectOpaqueResultModel{}.build(units, diagnostics))
    {
        built_ = true;
        return false;
    }
    return true;
}

void ProjectGenericModel::indexOrdinaryCallableReturnTypes(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units)
{
    ordinaryCallableReturnTypes_.clear();
    const GenericTelemetry::Scope phase(telemetry_, "generics.ordinary-return-index");
    // NSC305-G06. Iterative walk (was a self-recursive lambda over the AST).
    std::vector<ast::AstNode*> ordinaryReturnStack;
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
        if (unit != nullptr && unit->syntaxTree() != nullptr)
            ordinaryReturnStack.push_back(unit->syntaxTree());
    while (!ordinaryReturnStack.empty())
    {
        ast::AstNode* node = ordinaryReturnStack.back();
        ordinaryReturnStack.pop_back();
        if (node == nullptr) continue;
        if ((node->kind() == ast::AstNodeKind::MethodDeclaration ||
             node->kind() == ast::AstNodeKind::FunctionDeclaration) &&
            node->genericParameters().empty() && node->typeSyntax() != nullptr)
        {
            const auto [entry, inserted] = ordinaryCallableReturnTypes_.emplace(
                node->text(), node->typeSyntax());
            if (!inserted && entry->second != nullptr &&
                typeSpelling(entry->second) != typeSpelling(node->typeSyntax()))
                entry->second = nullptr;
        }
        // EN: The children are read in place: the frames below only record the pointers and nothing
        //     here edits the child list of `node`.
        // ES: Los hijos se leen en su lugar: los frames de abajo solo guardan los punteros y aquí
        //     nada modifica la lista de hijos de `node`.
        const std::vector<ast::AstNode*>& children = node->children();
        for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
            ordinaryReturnStack.push_back(*iterator);
        if (node->third() != nullptr) ordinaryReturnStack.push_back(node->third());
        if (node->second() != nullptr) ordinaryReturnStack.push_back(node->second());
        if (node->first() != nullptr) ordinaryReturnStack.push_back(node->first());
    }
}

bool ProjectGenericModel::collectAssociatedItemsForBuild(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const GenericTelemetry::Scope phase(telemetry_, "generics.associated-items");
    if (!collectAssociatedItems(units, diagnostics))
    {
        built_ = true;
        return false;
    }
    return true;
}

void ProjectGenericModel::collectTemplatesForBuild(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const GenericTelemetry::Scope phase(telemetry_, "generics.template-collection");
    for (std::size_t unitIndex = 0; unitIndex < units.size(); ++unitIndex)
    {
        const std::unique_ptr<ProjectSyntaxUnit>& unitPtr = units[unitIndex];
        if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
#if 0 // INSTRUMENTATION CODE
        const GenericWorkProgress progress(
            telemetry_, "generics.template-collection.unit", genericUnitDetail(*unitPtr),
            unitIndex + 1U, units.size());
#endif // INSTRUMENTATION CODE
        ProjectSyntaxUnit& unit = *unitPtr;
        collectTemplates(unit, unit.syntaxTree(), nullptr, "", "", false, diagnostics);
    }
}

void ProjectGenericModel::disambiguateTemplateCanonicalKeys()
{
    // ID 132. LA DESAMBIGUACION SE APLICA SOLO DONDE HAY COLISION, y esto no es una optimizacion:
    // es lo que evita cambiar una cadena que YA ES PUBLICA.
    //
    // La primera version de este arreglo metia la cuenta de parametros en la clave SIEMPRE, y esa
    // clave sale del compilador por tres sitios: el volcado de HIR, el de MIR y los METADATOS DE
    // BIBLIOTECA que lee un host. Cuatro pruebas se pusieron rojas y tenian razon: cambiar el
    // formato de una identidad publica por un problema interno rompe a quien la lea.
    //
    // Asi que la clave de un metodo generico se queda EXACTAMENTE como estaba mientras sea unica.
    // Solo cuando dos plantillas comparten `nombre``aridad` se les anade `#<parametros de valor>`
    // a las dos, que es el unico caso donde antes habia un error y por tanto el unico donde no
    // puede haber nada escrito que dependa del formato anterior.
    const GenericTelemetry::Scope phase(telemetry_, "generics.template-index");
    std::unordered_map<std::string, std::size_t> baseCounts;
    for (const TemplateState& state : templateStates_)
    {
        if (state.record.kind != GenericTemplateKind::Method) continue;
        ++baseCounts[state.record.canonicalKey];
    }
    // EL DESEMPATE ES LA POSICION DE DECLARACION, no la cuenta de parametros.
    //
    // La cuenta separaba `F<T>(T[])` de `F<T>(T[], int, int)`, pero NO separa `F<T>(T)` de
    // `F<T>(T[])` -- las dos toman uno. Y si dos plantillas comparten clave, comparten la
    // clave de especializacion, asi que la segunda daria en la cache de la primera y una de
    // las dos sobrecargas DESAPARECERIA en silencio. Ese es exactamente el fallo que este
    // proyecto persigue: no un rojo, sino un verde que mide otra cosa.
    //
    // El indice de declaracion es unico por construccion y determinista (orden de fuente).
    std::size_t index = 0;
    for (TemplateState& state : templateStates_)
    {
        const std::size_t position = index++;
        if (state.record.kind != GenericTemplateKind::Method) continue;
        const auto found = baseCounts.find(state.record.canonicalKey);
        if (found == baseCounts.end() || found->second < 2) continue;
        state.record.canonicalKey += "#" + std::to_string(position);
    }

    // A closed member from a controlled family is an implementation artifact, not an
    // additional public overload. Mark every participant (including the unmodified fallback)
    // so NSMETA can publish the open templates without trying to merge equal closed language
    // signatures.
    std::unordered_set<std::string> controlledFamilies;
    for (const TemplateState& state : templateStates_)
        if (state.record.kind == GenericTemplateKind::Method &&
            state.record.isControlledSpecialization)
            controlledFamilies.insert(state.record.specializationFamilyKey);
    for (TemplateState& state : templateStates_)
        state.record.isControlledSpecializationFamily =
            state.record.kind == GenericTemplateKind::Method &&
            controlledFamilies.contains(state.record.specializationFamilyKey);
}

void ProjectGenericModel::buildTemplateIndexes()
{
    // NSC305-G01. Build the O(1) syntax and canonical-key indexes now that template collection and
    // canonical-key disambiguation are final. `templateStates_` is stable from here on, so the
    // stored element pointers remain valid. `emplace` keeps the first state for a duplicated key,
    // matching the previous linear scan's "first match" semantics (duplicate type keys are
    // diagnosed just below, so their winner is irrelevant to a successful build).
    for (TemplateState& state : templateStates_)
    {
        stateBySyntax_.emplace(state.syntax, &state);
        stateByCanonicalKey_.emplace(state.record.canonicalKey, &state);
        if (state.record.kind != GenericTemplateKind::Method)
        {
            state.qualifiedTypeSpelling =
                qualifiedName(state.record.namespaceName, state.record.qualifiedTypeName);
            typesByModuleQualified_[state.record.moduleName + "::" + state.qualifiedTypeSpelling]
                .push_back(&state);
            typesByVisibleName_[state.record.name].push_back(&state);
        }
        else
        {
            methodsByName_[state.record.name].push_back(&state);
        }
    }
}

void ProjectGenericModel::discoverTemplateDependenciesForBuild(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units)
{
    // Record the declaration-level dependency graph before any template is cloned. A producer
    // can then publish exactly the fixed-point closure of its public roots, and the NSMETA
    // verifier can prove that a same-module edge was not silently dropped.
    const GenericTelemetry::Scope phase(telemetry_, "generics.dependency-discovery");
    for (std::size_t unitIndex = 0; unitIndex < units.size(); ++unitIndex)
    {
        const std::unique_ptr<ProjectSyntaxUnit>& unitPtr = units[unitIndex];
        if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
#if 0 // INSTRUMENTATION CODE
        const GenericWorkProgress progress(
            telemetry_, "generics.dependency-discovery.unit", genericUnitDetail(*unitPtr),
            unitIndex + 1U, units.size());
#endif // INSTRUMENTATION CODE
        discoverTemplateDependencies(
            *unitPtr, unitPtr->syntaxTree(), "", "", GenericLookupScope{});
    }
    for (TemplateState& state : templateStates_)
    {
        std::sort(state.record.dependencies.begin(), state.record.dependencies.end());
        state.record.dependencies.erase(
            std::unique(state.record.dependencies.begin(), state.record.dependencies.end()),
            state.record.dependencies.end());
        std::sort(
            state.record.unresolvedDependencies.begin(),
            state.record.unresolvedDependencies.end());
        state.record.unresolvedDependencies.erase(
            std::unique(
                state.record.unresolvedDependencies.begin(),
                state.record.unresolvedDependencies.end()),
            state.record.unresolvedDependencies.end());
        std::sort(
            state.record.externalTypeReferences.begin(),
            state.record.externalTypeReferences.end(),
            [](const auto& left, const auto& right) {
                if (left.range.begin().offset() != right.range.begin().offset())
                    return left.range.begin().offset() < right.range.begin().offset();
                return left.typeKey < right.typeKey;
            });
        state.record.externalTypeReferences.erase(
            std::unique(
                state.record.externalTypeReferences.begin(),
                state.record.externalTypeReferences.end(),
                [](const auto& left, const auto& right) {
                    return left.range.begin() == right.range.begin() &&
                        left.range.end() == right.range.end() &&
                        left.typeKey == right.typeKey;
                }),
            state.record.externalTypeReferences.end());
    }
}

void ProjectGenericModel::publishTemplateRecordsForBuild(
    diagnostics::DiagnosticEngine& diagnostics)
{
    // LA DETECCION DE DUPLICADOS ES SOLO PARA TIPOS, desde el 2026-09-03.
    //
    // Dos plantillas de METODO con el mismo nombre y la misma aridad generica YA NO son un error:
    // son sobrecargas, y se materializan todas con un nombre compartido para que la resolucion
    // ordinaria elija por tipos (ver processGenericCall). Si despues de sustituir acaban con la
    // MISMA lista de parametros --un duplicado de verdad-- lo detecta la deteccion de metodos
    // duplicados del modelo de miembros, que es donde se comparan listas de parametros.
    //
    // Para clases, structs y delegados no cambia nada: dos con el mismo nombre y aridad siguen
    // siendo un choque, porque ahi no hay lista de parametros que los distinga.
    const GenericTelemetry::Scope phase(telemetry_, "generics.template-index");
    std::unordered_set<std::string> templateKeys;
    for (const TemplateState& state : templateStates_)
    {
        if (state.record.kind == GenericTemplateKind::Method) continue;
        if (!templateKeys.insert(state.record.canonicalKey).second)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                state.syntax != nullptr ? state.syntax->range() : SourceRange{},
                // Este bucle salta los METODOS --la linea de arriba-- asi que aqui solo llegan
                // TIPOS genericos, y el mensaje tiene que hablar de tipos. Decia «dos metodos
                // genericos», que era el texto del caso vecino: un tipo generico no tiene
                // parametros por los que desambiguar, de modo que dos con el mismo nombre, el
                // mismo namespace y la misma aridad son sencillamente el mismo tipo dos veces.
                "duplicate generic type identity '" + state.record.canonicalKey +
                    "'; two generic types in the same namespace cannot share a name and an "
                    "arity -- rename one, or declare it 'partial' at namespace scope");
        }
    }

    templateRecords_.reserve(templateStates_.size());
    for (const TemplateState& state : templateStates_) templateRecords_.push_back(state.record);
}

void ProjectGenericModel::seedImportedSpecializationsForBuild(
    std::span<const PrecompiledGenericSpecialization> precompiledSpecializations)
{
    // Seed only identities whose public template was actually hydrated. An old metadata file may
    // list historical specializations without carrying a reusable template body; such a record
    // must never make an unavailable generic appear usable.
    const GenericTelemetry::Scope phase(telemetry_, "generics.imported-specialization-seed");
    for (const PrecompiledGenericSpecialization& imported : precompiledSpecializations)
    {
        if (!imported.hasConcreteDeclaration) continue;
        const auto templateStateIt = stateByCanonicalKey_.find(imported.templateKey);
        if (templateStateIt == stateByCanonicalKey_.end() ||
            specializationPlanner_.byKey.contains(imported.specializationKey))
        {
            continue;
        }
        const TemplateState* templateState = templateStateIt->second;
        const std::size_t ordinal = specializations_.size();
        specializationPlanner_.commit(imported.specializationKey, ordinal);
        specializations_.push_back(GenericSpecializationRecord{
            imported.kind,
            templateState->syntax,
            nullptr,
            imported.templateKey,
            imported.specializationKey,
            imported.specializedName,
            imported.typeArguments,
            ordinal,
            templateState->ordinal,
            nullptr,
            SourceRange{},
            0,
            InstantiationContext{}});
        specializations_.back().state = SpecializationState::Ready;
    }
}

void ProjectGenericModel::discoverRootDemandsForBuild(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const GenericTelemetry::Scope phase(telemetry_, "generics.root-demand-discovery");
    for (std::size_t unitIndex = 0; unitIndex < units.size(); ++unitIndex)
    {
        const std::unique_ptr<ProjectSyntaxUnit>& unitPtr = units[unitIndex];
        if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
#if 0 // INSTRUMENTATION CODE
        const GenericWorkProgress progress(
            telemetry_, "generics.root-demand-discovery.unit", genericUnitDetail(*unitPtr),
            unitIndex + 1U, units.size());
#endif // INSTRUMENTATION CODE
        ProjectSyntaxUnit& unit = *unitPtr;
        processNode(unit, unit.syntaxTree(), "", "", GenericLookupScope{}, false, diagnostics);
    }
}

bool ProjectGenericModel::materializePendingDemandsForBuild(
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NSC305-G08. Apply the body-type demands first (they materialize directly and may collect
    // further type demands), then drain the type-demand waves (the two-pass).
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.body-type-demands");
        if (!materializeBodyTypeSpecializationDemands(diagnostics))
        {
            built_ = true;
            return false;
        }
    }

    // NSC305-G08. Drain the collected type specialization demands in waves.
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.type-demand-waves");
        if (!executeTypeDemandWaves(diagnostics))
        {
            built_ = true;
            return false;
        }
    }
    return true;
}

void ProjectGenericModel::finalizeBuildState(const diagnostics::DiagnosticEngine& diagnostics)
{
    telemetry_.maximumLogicalDepth = instantiationBudget_.maximumObservedDepth();
    // NSC305-C01. Persist the collected template records (the C1 metadata) when the cache is enabled.
    // restoreTemplateRecords is not called here: it only counted hits after the build, and those
    // counts went into the instantiation-cache hit statistics. Nothing is stored from a
    // compilation that reported errors.
    if (!diagnostics.hasErrors()) storeTemplateRecords();
    indexes_->rebuildGenerics(templateRecords_, specializations_);
    built_ = true;
}

bool ProjectGenericModel::build(
    const compilation::ProjectGraph& graph,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    std::span<const PrecompiledGenericSpecialization> precompiledSpecializations,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (built_) return !diagnostics.hasErrors();
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.visibility");
        buildVisibility(graph);
    }

    expandAliasesForBuild(units, diagnostics);
    if (diagnostics.hasErrors())
    {
        built_ = true;
        return false;
    }

    if (!mergeTraitImplementationsForBuild(units, diagnostics))
    {
        return false;
    }
    if (!buildOpaqueResultsForBuild(units, diagnostics))
    {
        return false;
    }
    indexOrdinaryCallableReturnTypes(units);
    if (!collectAssociatedItemsForBuild(units, diagnostics))
    {
        return false;
    }
    collectTemplatesForBuild(units, diagnostics);

    disambiguateTemplateCanonicalKeys();

    buildTemplateIndexes();

    discoverTemplateDependenciesForBuild(units);

    publishTemplateRecordsForBuild(diagnostics);

    seedImportedSpecializationsForBuild(precompiledSpecializations);

    discoverRootDemandsForBuild(units, diagnostics);

    if (!materializePendingDemandsForBuild(diagnostics))
    {
        return false;
    }
    finalizeBuildState(diagnostics);
    return !diagnostics.hasErrors();
}

std::span<const GenericTemplateRecord> ProjectGenericModel::templates() const noexcept
{
    return templateRecords_;
}

const GenericTemplateRecord* ProjectGenericModel::templateForCanonicalKey(
    std::string_view canonicalKey) const noexcept
{
    if (indexes_ != nullptr)
    {
        if (const GenericTemplateRecord* indexed =
                indexes_->genericTemplateForCanonicalKey(canonicalKey))
        {
            return indexed;
        }
    }
    const auto found = std::ranges::find_if(
        templateRecords_,
        [&](const GenericTemplateRecord& record)
        {
            return record.canonicalKey == canonicalKey;
        });
    return found == templateRecords_.end() ? nullptr : &*found;
}

std::span<const GenericSpecializationRecord> ProjectGenericModel::specializations() const noexcept
{
    return specializations_;
}

const GenericSpecializationRecord* ProjectGenericModel::specializationForSyntax(
    const ast::AstNode* syntax) const noexcept
{
    if (built_)
    {
        return indexes_->genericSpecializationForSyntax(syntax);
    }
    const auto found = specializationBySyntax_.find(syntax);
    return found != specializationBySyntax_.end() && found->second < specializations_.size()
        ? &specializations_[found->second] : nullptr;
}

const GenericSpecializationRecord* ProjectGenericModel::specializationForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (typeKey.starts_with("type:")) typeKey.remove_prefix(5U);
    const std::size_t separator = typeKey.find_last_of(".+:");
    const std::string_view terminal = separator == std::string_view::npos
        ? typeKey : typeKey.substr(separator + 1U);
    if (built_)
    {
        return indexes_->genericSpecializationForName(terminal);
    }
    return nullptr;
}

std::string ProjectGenericModel::closedTypeKeyForTemplate(
    std::string_view templateName,
    std::string_view elementTypeKey) const
{
    const GenericTemplateRecord* templateRecord = nullptr;
    if (built_)
    {
        const std::span<const std::size_t> ordinals =
            indexes_->genericTemplateOrdinalsForName(templateName);
        if (!ordinals.empty())
        {
            templateRecord = indexes_->genericTemplateForOrdinal(ordinals.front());
        }
    }
    else
    {
        for (const GenericTemplateRecord& record : templates())
        {
            if (record.name == templateName)
            {
                templateRecord = &record;
                break;
            }
        }
    }
    if (templateRecord == nullptr) return {};

    // Keep the terminal spelling only for the readable generated symbol. The hash must use the
    // same declaration-qualified identity as ordinary generic specialization; otherwise two
    // modules or namespaces that both declare `Item` synthesize the same Span<Item> type key.
    std::string spelling = std::string(elementTypeKey);
    const bool builtin = spelling.starts_with("builtin:");
    if (builtin) spelling.erase(0, 8U);
    if (spelling.starts_with("type:")) spelling.erase(0, 5U);
    const std::size_t separator = spelling.find_last_of(".+:");
    if (separator != std::string::npos) spelling.erase(0, separator + 1U);
    std::string identity;
    if (builtin)
    {
        identity = spelling;
    }
    else if (const GenericSpecializationRecord* specialization =
                 specializationForTypeKey(elementTypeKey);
             specialization != nullptr)
    {
        identity = "generic:" + specialization->specializationKey;
    }
    else
    {
        identity = std::string(elementTypeKey);
    }

    const std::string key = templateRecord->canonicalKey + "<" + identity + ">";
    const std::string specializedName = templateRecord->name + "__g1__" + sanitize(spelling) +
        "__h" + stableHashSuffix(key);

    const std::string_view canonicalKey = templateRecord->canonicalKey;
    const std::size_t moduleSeparator = canonicalKey.find("::");
    if (moduleSeparator == std::string_view::npos) return {};
    const std::size_t terminal = canonicalKey.find_last_of(".+");
    const std::size_t terminalBegin = terminal == std::string_view::npos ||
            terminal < moduleSeparator + 2U
        ? moduleSeparator + 2U
        : terminal + 1U;
    if (canonicalKey.find('`', terminalBegin) == std::string_view::npos) return {};
    return "type:" + std::string(canonicalKey.substr(0, terminalBegin)) + specializedName;
}

const std::string* ProjectGenericModel::resolvedTypeIdentity(
    const ast::TypeSyntax* syntax) const noexcept
{
    return lookupResolvedTypeIdentity(syntax);
}

const std::string* ProjectGenericModel::lookupResolvedTypeIdentity(
    const ast::TypeSyntax* syntax) const noexcept
{
    if (workerResolvedTypeIdentities_ != nullptr)
    {
        const auto local = workerResolvedTypeIdentities_->find(syntax);
        if (local != workerResolvedTypeIdentities_->end()) return &local->second;
    }
    const auto found = resolvedTypeIdentityBySyntax_.find(syntax);
    return found == resolvedTypeIdentityBySyntax_.end() ? nullptr : &found->second;
}

const ProjectGenericModel::DeclaredTypeOrigin* ProjectGenericModel::lookupResolvedTypeOrigin(
    const ast::TypeSyntax* syntax) const noexcept
{
    if (workerResolvedTypeOrigins_ != nullptr)
    {
        const auto local = workerResolvedTypeOrigins_->find(syntax);
        if (local != workerResolvedTypeOrigins_->end()) return &local->second;
    }
    const auto found = resolvedTypeOriginBySyntax_.find(syntax);
    return found == resolvedTypeOriginBySyntax_.end() ? nullptr : &found->second;
}

std::string* ProjectGenericModel::mutableResolvedTypeIdentity(
    const ast::TypeSyntax* syntax)
{
    if (workerResolvedTypeIdentities_ != nullptr)
    {
        const auto local = workerResolvedTypeIdentities_->find(syntax);
        if (local != workerResolvedTypeIdentities_->end()) return &local->second;
        const auto shared = resolvedTypeIdentityBySyntax_.find(syntax);
        if (shared == resolvedTypeIdentityBySyntax_.end()) return nullptr;
        const auto [inserted, unused] =
            workerResolvedTypeIdentities_->emplace(syntax, shared->second);
        (void)unused;
        return &inserted->second;
    }
    const auto found = resolvedTypeIdentityBySyntax_.find(syntax);
    return found == resolvedTypeIdentityBySyntax_.end() ? nullptr : &found->second;
}

void ProjectGenericModel::recordResolvedTypeIdentity(
    const ast::TypeSyntax* syntax,
    std::string identity)
{
    if (workerResolvedTypeIdentities_ != nullptr)
        (*workerResolvedTypeIdentities_)[syntax] = std::move(identity);
    else
        resolvedTypeIdentityBySyntax_[syntax] = std::move(identity);
}

void ProjectGenericModel::recordResolvedTypeOrigin(
    const ast::TypeSyntax* syntax,
    DeclaredTypeOrigin origin)
{
    if (workerResolvedTypeOrigins_ != nullptr)
        (*workerResolvedTypeOrigins_)[syntax] = std::move(origin);
    else
        resolvedTypeOriginBySyntax_[syntax] = std::move(origin);
}

GenericInstantiationReport ProjectGenericModel::instantiationReport() const noexcept
{
    return GenericInstantiationReport{
        instantiationBudget_.count(),
        instantiationCacheHitCount_,
        instantiationBudget_.maximumObservedDepth(),
        instantiationBudget_.maximumDepth(),
        instantiationBudget_.maximumCount()};
}

GenericTelemetry& ProjectGenericModel::telemetry() noexcept
{
    return telemetry_;
}

const GenericTelemetry& ProjectGenericModel::telemetry() const noexcept
{
    return telemetry_;
}

bool ProjectGenericModel::isBuilt() const noexcept
{
    return built_;
}

void ProjectGenericModel::setTemplateCacheRoot(std::filesystem::path root)
{
    templateCache_.setRoot(std::move(root));
}

void ProjectGenericModel::setParallelismWorkers(std::size_t workers) noexcept
{
    parallelismWorkers_ = workers == 0U ? 1U : workers;
}
}
