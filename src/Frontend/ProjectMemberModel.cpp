#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "Internal/EnumInitializerResolver.h"
#include "Internal/AnonymousMemberPromotionVisitState.h"
#include "Internal/EnumValue.h"
#include "Internal/EnumSuccessor.h"

#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
ProjectMemberModel::ProjectMemberModel() noexcept
    : indexes_(&ownedIndexes_)
{
}

ProjectMemberModel::ProjectMemberModel(ProjectCompilationIndexes& indexes) noexcept
    : indexes_(&indexes)
{
}

std::size_t ProjectMemberModel::TransparentStringHash::operator()(std::string_view value) const noexcept
        {
            return std::hash<std::string_view>{}(value);
        }

std::size_t ProjectMemberModel::TransparentStringHash::operator()(const std::string& value) const noexcept
        {
            return (*this)(std::string_view(value));
        }

bool ProjectMemberModel::build(
    const ProjectNameLookup& nameLookup,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics,
    const ProjectGenericModel* genericModel)
{
    if (built_)
    {
        return !diagnostics.hasErrors();
    }

    initializeBuildState(nameLookup, genericModel);

    collectDeclaredTypeMembers(nameLookup, diagnostics);
    collectExternalTraitImplMembers(nameLookup, diagnostics);
    // Method-body shards share this model. Complete the ownership closure while build() is still
    // single-threaded so every later query is a read-only lookup instead of a lazy cache write.
    buildTransitiveOwnershipCache();

    validateCopyTypes(nameLookup, diagnostics);

    validateRawUnions(nameLookup, diagnostics);
    collectAnonymousMemberPromotions(nameLookup, diagnostics);
    buildMembersDeclaredByTypeIndex();
    buildMemberNameIndex();
    indexes_->rebuildMembers(members_);

    validateCAbiExportNames(diagnostics);
    validateImportedCIdentities(diagnostics);
    scanMemberBodies(diagnostics);
    scanCompilationUnits(units, diagnostics);
    built_ = true;
    return !diagnostics.hasErrors();
}


std::span<const ClassMemberSymbol> ProjectMemberModel::members() const noexcept
{
    return members_;
}

std::span<const ModuleInitializationRecord> ProjectMemberModel::moduleInitializers() const noexcept
{
    return moduleInitializers_;
}

std::span<const MemberBodyBinding> ProjectMemberModel::bodyBindings() const noexcept
{
    return bodyBindings_;
}

std::span<const std::size_t> ProjectMemberModel::membersOfType(std::size_t typeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    if (typeOrdinal >= membersByType_.size())
    {
        return empty;
    }
    return membersByType_[typeOrdinal];
}

std::span<const std::size_t> ProjectMemberModel::membersDeclaredByType(
    std::size_t typeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    return membersDeclaredByTypeIndexBuilt_ && typeOrdinal < membersDeclaredByType_.size()
        ? std::span<const std::size_t>(membersDeclaredByType_[typeOrdinal])
        : std::span<const std::size_t>(empty);
}

bool ProjectMemberModel::hasMembersDeclaredByTypeIndex() const noexcept
{
    return membersDeclaredByTypeIndexBuilt_;
}

std::span<const std::size_t> ProjectMemberModel::constructorsOfType(
    std::size_t typeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    if (typeOrdinal >= constructorsByType_.size())
    {
        return empty;
    }
    return constructorsByType_[typeOrdinal];
}

std::optional<std::size_t> ProjectMemberModel::typeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    return typeOrdinalForKey(typeKey);
}

bool ProjectMemberModel::isBuilt() const noexcept
{
    return built_;
}

}
