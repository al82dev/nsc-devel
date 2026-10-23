#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../Internal/CloneDepthGuard.h"
#include "../Cache/CachePayloadReader.h"
#include "../Internal/EnumConstResolver.h"
#include "../Internal/SpecializationWaveTimer.h"
#include "../Internal/InterfaceFingerprint.h"
#include "../Internal/ImplementationFingerprint.h"
#include "../Internal/LocatedNode.h"
#include "../Internal/MirFunctionKey.h"
#include "../Internal/MirCacheRecord.h"
#include "../Internal/ProjectGenericDependencyFrame.h"
#include "../Internal/ProjectGenericBodyFrame.h"
#include "../Internal/ProjectGenericAliasFrame.h"
#include "../Internal/ProjectGenericChildWork.h"
#include "../Internal/ProjectGenericCollectFrame.h"
#include "../Internal/ProjectGenericProcessFrame.h"
#include "../Internal/ProjectGenericReservedDemand.h"
#include "../Internal/TypeWork.h"
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
#include "GenericModelSupport.h"
#include "Support/TemplateClassification.h"
#include "Support/TemplateSpelling.h"
#include "Support/ConstGenericArguments.h"
#include "Support/GenericPatterns.h"
#include "Cache/TemplateCatalogCodec.h"
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
ProjectGenericModel::TypeResolution ProjectGenericModel::resolveTypeTemplate(
    const ProjectSyntaxUnit& unit,
    std::string_view namespaceName,
    std::string_view writtenName,
    std::size_t arity)
{
    const auto sinFiltro = std::numeric_limits<std::size_t>::max();
    const std::size_t moduleSeparator = writtenName.find("::");
    if (moduleSeparator != std::string_view::npos)
    {
        const std::string_view requestedModule = writtenName.substr(0, moduleSeparator);
        const std::string_view requestedType = writtenName.substr(moduleSeparator + 2U);
        if (requestedModule.empty() || requestedType.empty())
        {
            telemetry_.noteResolutionQuery(0);
            return TypeResolution{TypeResolutionKind::Missing, nullptr};
        }

        // writtenName is exactly requestedModule + "::" + requestedType (the two parts are its
        // text before and after the first "::"), so it is looked up as it is, without a copy.
        const auto bucket = typesByModuleQualified_.find(writtenName);
        if (bucket == typesByModuleQualified_.end())
        {
            telemetry_.noteResolutionQuery(0);
            return TypeResolution{TypeResolutionKind::Missing, nullptr};
        }
        telemetry_.noteResolutionQuery(bucket->second.size());
        TemplateState* exact = nullptr;
        for (TemplateState* state : bucket->second)
        {
            if (arity != sinFiltro && !genericArityAccepts(state->record, arity)) continue;
            if (exact != nullptr) return TypeResolution{TypeResolutionKind::Ambiguous, nullptr};
            exact = state;
        }
        return TypeResolution{
            exact != nullptr ? TypeResolutionKind::Unique : TypeResolutionKind::Missing, exact};
    }

    // NSC305-G02. Bucket by terminal name; the original scan tested every catalog entry for
    // visibility, arity and candidate spelling. The bucket keeps source order, so the first-match
    // and ambiguity semantics are unchanged.
    // The namespace-qualified spelling is only compared for a visible candidate of an
    // undotted name; it is built at that first comparison instead of for every query.
    std::optional<std::string> qualified;
    const std::string_view terminalName = [&]() -> std::string_view
    {
        const std::size_t dot = writtenName.rfind('.');
        return dot == std::string_view::npos ? writtenName : writtenName.substr(dot + 1U);
    }();
    const auto bucket = typesByVisibleName_.find(terminalName);
    if (bucket == typesByVisibleName_.end())
    {
        telemetry_.noteResolutionQuery(0);
        return TypeResolution{TypeResolutionKind::Missing, nullptr};
    }
    telemetry_.noteResolutionQuery(bucket->second.size());
    TemplateState* fallback = nullptr;
    for (TemplateState* state : bucket->second)
    {
        if (!templateVisible(unit.projectOrdinal(), state->record)) continue;
        if (arity != sinFiltro && !genericArityAccepts(state->record, arity)) continue;
        const std::string candidate =
            qualifiedName(state->record.namespaceName, state->record.qualifiedTypeName);
        if (writtenName.find('.') != std::string_view::npos)
        {
            if (candidate == writtenName)
            {
                if (fallback != nullptr) return TypeResolution{TypeResolutionKind::Ambiguous, nullptr};
                fallback = state;
            }
            continue;
        }
        if (!qualified.has_value()) qualified = qualifiedName(namespaceName, writtenName);
        if (candidate == *qualified) return TypeResolution{TypeResolutionKind::Unique, state};
        if (state->record.name == writtenName)
        {
            if (fallback != nullptr) return TypeResolution{TypeResolutionKind::Ambiguous, nullptr};
            fallback = state;
        }
    }
    return TypeResolution{
        fallback != nullptr ? TypeResolutionKind::Unique : TypeResolutionKind::Missing, fallback};
}

// NSC-LIMIT-007. QUE PLANTILLAS NOMBRA UNA LLAMADA -- con la escala del modelo de nombres, no
// con una propia.
//
// La version anterior tenia tres escalones inventados aqui: exacto, relativo al namespace del que
// llama, y sufijo de segmentos. Eso no es lo que hace este compilador. Dentro de `A.B`, el nombre
// `C` significa `A.B.C` si existe, si no `A.C`, si no lo que traiga un `using`, si no el global --y
// antes que todo eso, un tipo contenedor del sitio: dentro de `Outer`, `Helper` es `Outer.Helper`--.
// Con los tres escalones inventados, `A.C` y `Z.C` eran los DOS sufijos de `.C` y empataban, de
// modo que una llamada perfectamente legal se declaraba ambigua o clonaba la plantilla ajena --con
// su cuerpo, que habla de SU `T`--.
//
// Ahora la escala viene de `qualifiedNameLadder`, que es la MISMA funcion que recorre
// ProjectNameLookup. Lo que cambia entre las dos fases es que se sondea: alli la tabla de tipos,
// aqui la de plantillas. Ver QualifiedNameLadder.h.
std::vector<ProjectGenericModel::TemplateState*> ProjectGenericModel::templatesNamedBy(
    const ProjectSyntaxUnit& unit,
    const QualifiedNameSite& site,
    std::string_view writtenOwner,
    std::string_view methodName,
    std::size_t arity,
    std::size_t argumentCount,
    const std::vector<QualifiedNameRung>* ladder)
{
    const auto bucket = methodsByName_.find(std::string(methodName));
    if (bucket == methodsByName_.end())
    {
        telemetry_.noteResolutionQuery(0);
        return {};
    }
    telemetry_.noteResolutionQuery(bucket->second.size());
    const auto sinFiltro = std::numeric_limits<std::size_t>::max();
    const auto aplicable = [&](const TemplateState& state)
    {
        if (!templateVisible(unit.projectOrdinal(), state.record)) return false;
        if (arity != sinFiltro && !genericArityAccepts(state.record, arity)) return false;
        if (argumentCount != sinFiltro && valueParameterCount(state.syntax) != argumentCount)
            return false;
        return true;
    };

    // EL PRIMER ESCALON QUE PRODUCE ALGO DECIDE, y los de abajo ya no se miran. Es la regla de
    // sombreado: que exista un `Z.C` en otro namespace no puede cambiar lo que `C` significa aqui.
    std::vector<QualifiedNameRung> ownLadder;
    if (ladder == nullptr)
        ownLadder = qualifiedNameLadder(site, writtenOwner);
    const std::vector<QualifiedNameRung>& effectiveLadder = ladder != nullptr ? *ladder : ownLadder;
    for (const QualifiedNameRung& rung : effectiveLadder)
    {
        std::vector<TemplateState*> found;
        for (TemplateState* state : bucket->second)
        {
            if (!aplicable(*state)) continue;
            const std::string owner =
                templateSourceSpelling(state->record.namespaceName, state->record.ownerName);
            for (const std::string& candidate : rung.spellings)
            {
                if (owner == candidate)
                {
                    found.push_back(state);
                    break;
                }
            }
        }
        if (!found.empty()) return found;
    }
    // NS304-T08-F06: a global-namespace template has an empty source owner. The ladder cannot
    // produce an empty rung, so match those templates explicitly.
    if (writtenOwner.empty())
    {
        std::vector<TemplateState*> found;
        for (TemplateState* state : bucket->second)
        {
            if (!aplicable(*state)) continue;
            if (templateSourceSpelling(state->record.namespaceName, state->record.ownerName).empty())
                found.push_back(state);
        }
        if (!found.empty()) return found;
    }
    return {};
}

std::vector<ProjectGenericModel::TemplateState*> ProjectGenericModel::collectMethodTemplatesFromNamed(
    const std::vector<TemplateState*>& named) const
{
    // UN SOLO DUENO. Un escalon puede empatar --dos `using` que traen dos `Caja`, por ejemplo--.
    // Materializar las dos significa clonar y analizar un cuerpo que este codigo no nombra, y ese
    // cuerpo habla de SU `T`. Si el empate cruza duenos la llamada es realmente ambigua y no se
    // materializa nada: `resolveMethodTemplate` da entonces su diagnostico, que es su sitio.
    std::string owner;
    std::string ownerModule;
    for (const TemplateState* state : named)
    {
        const std::string candidate =
            templateSourceSpelling(state->record.namespaceName, state->record.ownerName);
        if (owner.empty())
        {
            owner = candidate;
            ownerModule = state->record.moduleName;
        }
        else if (owner != candidate || ownerModule != state->record.moduleName) return {};
    }
    return named;
}

std::vector<ProjectGenericModel::TemplateState*> ProjectGenericModel::collectMethodTemplates(
    const ProjectSyntaxUnit& unit,
    const QualifiedNameSite& site,
    std::string_view writtenOwner,
    std::string_view methodName,
    std::size_t arity,
    std::size_t argumentCount)
{
    return collectMethodTemplatesFromNamed(
        templatesNamedBy(unit, site, writtenOwner, methodName, arity, argumentCount));
}

ProjectGenericModel::TemplateState* ProjectGenericModel::resolveMethodTemplate(
    const ProjectSyntaxUnit& unit,
    const QualifiedNameSite& site,
    std::string_view writtenOwner,
    std::string_view methodName,
    std::size_t arity,
    std::size_t argumentCount,
    const std::vector<TemplateState*>* named,
    const std::vector<QualifiedNameRung>* ladder)
{
    const auto sinFiltro = std::numeric_limits<std::size_t>::max();

    // CON DUENO: la misma funcion que usa `collectMethodTemplates`, no una copia de sus reglas.
    // Primero exigiendo la cuenta de argumentos y despues sin ella, que es el aflojamiento que
    // este metodo ya tenia. NSC305-G03: el primer escalon puede venir ya calculado (`named`) y la
    // ladder puede venir compartida (`ladder`) para no reconstruirla en cada intento.
    for (const std::size_t cuenta : {argumentCount, sinFiltro})
    {
        const std::vector<TemplateState*> result =
            (cuenta == argumentCount && named != nullptr)
            ? *named
            : templatesNamedBy(unit, site, writtenOwner, methodName, arity, cuenta, ladder);
        if (result.size() == 1) return result.front();
        // Mas de una en el escalon ganador y del mismo nombre: ambigua de verdad. Aflojar la
        // cuenta de argumentos no la desambigua, asi que se sigue al siguiente intento solo
        // cuando no hubo NINGUNA.
        if (!result.empty()) break;
        if (cuenta == sinFiltro) break;
    }

    // SIN DUENO -- y solo aqui, cuando la escala no encontro ninguno.
    //
    // Es lo unico que esta fase tiene para sospechar que el receptor escrito no era un tipo sino un
    // VALOR: `objeto.F<int>()` produce la escritura `objeto`, que no nombra ninguna plantilla
    // porque no nombra ningun tipo. Tomar este camino cuando el dueno SI se resolvio seria
    // deshacer el filtro que se acaba de aplicar, y era justamente lo que hacia antes.
    const auto coincideSinDueno = [&](const TemplateState& state, bool exigirArgumentos)
    {
        if (!templateVisible(unit.projectOrdinal(), state.record)) return false;
        if (arity != sinFiltro && !genericArityAccepts(state.record, arity)) return false;
        if (exigirArgumentos && argumentCount != sinFiltro &&
            valueParameterCount(state.syntax) != argumentCount)
            return false;
        return true;
    };
    const auto unico = [&](bool exigirArgumentos) -> TemplateState*
    {
        const auto bucket = methodsByName_.find(std::string(methodName));
        if (bucket == methodsByName_.end()) return nullptr;
        TemplateState* result = nullptr;
        for (TemplateState* state : bucket->second)
        {
            if (!coincideSinDueno(*state, exigirArgumentos)) continue;
            if (result != nullptr) return nullptr;
            result = state;
        }
        return result;
    };
    if (TemplateState* result = unico(true); result != nullptr) return result;
    return unico(false);
}

// NSC305-G07. Shared steps of the ensure*Specialization pair; see the header.
std::string ProjectGenericModel::computeSpecializationKey(
    const GenericTemplateRecord& record,
    std::span<ast::TypeSyntax* const> arguments,
    std::vector<std::string>& spellings,
    std::vector<std::string>& identities) const
{
    spellings.reserve(arguments.size());
    identities.reserve(arguments.size());
    for (const ast::TypeSyntax* argument : arguments)
    {
        spellings.push_back(typeSpelling(argument));
        identities.push_back(typeSpecializationIdentity(argument));
    }
    std::ostringstream keyBuilder;
    keyBuilder << record.canonicalKey << '<';
    for (std::size_t index = 0; index < spellings.size(); ++index)
    {
        if (index != 0) keyBuilder << ',';
        keyBuilder << identities[index];
    }
    keyBuilder << '>';
    return keyBuilder.str();
}

void ProjectGenericModel::prepareSpecializationSubstitutions(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions)
{
    const std::size_t fixedParameterCount = trailingPackIndex(state.record).value_or(
        state.record.parameters.size());
    for (std::size_t index = 0; index < fixedParameterCount; ++index)
        substitutions.emplace(state.record.parameters[index], arguments[index]);
    activePackSubstitutions_.clear();
    if (const auto pack = trailingPackIndex(state.record); pack.has_value())
    {
        auto& values = activePackSubstitutions_[state.record.parameters[*pack]];
        for (std::size_t index = *pack; index < arguments.size(); ++index)
            values.push_back(arguments[index]);
    }
}

// NSC305-G08. Build a QualifiedNameSite once, owning the vectors its spans point into.
ProjectGenericModel::QualifiedNameSiteContext ProjectGenericModel::makeQualifiedNameSite(
    std::string_view namespaceName,
    std::string_view ownerName,
    const GenericLookupScope& scope) const
{
    QualifiedNameSiteContext context;
    context.namespaceSegments = splitDotted(namespaceName);
    context.enclosingTypeNames = enclosingTypeSpellings(namespaceName, ownerName);
    context.site.namespaceSegments = context.namespaceSegments;
    context.site.enclosingTypeNames = context.enclosingTypeNames;
    context.site.aliases = &scope.aliases.read();
    context.site.importedNamespaces = scope.importedNamespaces.read();
    return context;
}

// NSC305-G08. Resolve a generic call site once: named candidates at the winning rung, the
// shared-overload set, and the unique fallback (exact arity, then relaxed arity).
ProjectGenericModel::GenericCallResolution ProjectGenericModel::resolveGenericCall(
    const ProjectSyntaxUnit& unit,
    const QualifiedNameSite& site,
    std::string_view requestedOwner,
    std::string_view methodName,
    std::size_t arity,
    std::size_t argumentCount)
{
    // NSC305-G03. Build the name ladder once; every arity/argument-count variation reuses it.
    const std::vector<QualifiedNameRung> ladder = qualifiedNameLadder(site, requestedOwner);
    GenericCallResolution resolution;
    resolution.named = templatesNamedBy(
        unit, site, requestedOwner, methodName, arity, argumentCount, &ladder);
    resolution.shared = collectMethodTemplatesFromNamed(resolution.named);
    if (resolution.shared.size() <= 1U)
    {
        resolution.unique = resolveMethodTemplate(
            unit, site, requestedOwner, methodName, arity, argumentCount, nullptr, &ladder);
        if (resolution.unique == nullptr)
            resolution.unique = resolveMethodTemplate(
                unit, site, requestedOwner, methodName,
                std::numeric_limits<std::size_t>::max(), argumentCount, nullptr, &ladder);
    }
    return resolution;
}
}
