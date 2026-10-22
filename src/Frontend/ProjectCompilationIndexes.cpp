#include "nativesharp/Frontend/ProjectCompilationIndexes.h"

#include "nativesharp/Frontend/ClassMemberSymbol.h"
#include "nativesharp/Frontend/ClassObjectLayout.h"
#include "nativesharp/Frontend/ClassVirtualTable.h"
#include "nativesharp/Frontend/GenericSpecializationRecord.h"
#include "nativesharp/Frontend/GenericTemplateRecord.h"
#include "nativesharp/Frontend/MethodCleanupRegion.h"
#include "nativesharp/Frontend/MethodLambdaBinding.h"
#include "nativesharp/Frontend/MethodLambdaValueBinding.h"
#include "nativesharp/Frontend/MethodLocalSymbol.h"
#include "nativesharp/Frontend/NamedTypeDeclaration.h"
#include "nativesharp/Frontend/ObjectFieldLayout.h"
#include "nativesharp/Frontend/ProjectOopMirFunction.h"
#include "nativesharp/Frontend/ProjectOopMirClassLayout.h"
#include "nativesharp/Frontend/ProjectOopMirStructLayout.h"
#include "nativesharp/Frontend/StructValueLayout.h"
#include "nativesharp/Frontend/VirtualMethodInfo.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{
std::size_t ProjectCompilationIndexes::TransparentStringHash::operator()(
    std::string_view value) const noexcept
{
    return std::hash<std::string_view>{}(value);
}

std::size_t ProjectCompilationIndexes::TransparentStringHash::operator()(
    const std::string& value) const noexcept
{
    return (*this)(std::string_view(value));
}

void ProjectCompilationIndexes::clear() noexcept
{
    typesByOrdinal_.clear();
    typeOrdinalByIdentity_.clear();
    typeOrdinalsByName_.clear();
    membersByOrdinal_.clear();
    memberOrdinalsByName_.clear();
    memberOrdinalsBySignature_.clear();
    virtualMethodsByMember_.clear();
    virtualTablesByType_.clear();
    classLayoutsByType_.clear();
    structLayoutsByType_.clear();
    fieldLayoutsByMember_.clear();
    genericTemplatesByOrdinal_.clear();
    genericTemplateByCanonicalKey_.clear();
    genericTemplateOrdinalsByName_.clear();
    genericSpecializationsByOrdinal_.clear();
    genericSpecializationBySyntax_.clear();
    genericSpecializationByKey_.clear();
    genericSpecializationByName_.clear();
    bodyTypeDemandKeys_.clear();
    genericTypeDemandKeys_.clear();
    clearHirBindings();
}

void ProjectCompilationIndexes::rebuildTypes(std::span<const NamedTypeDeclaration> types)
{
    typesByOrdinal_.clear();
    typeOrdinalByIdentity_.clear();
    typeOrdinalsByName_.clear();
    membersByOrdinal_.clear();
    memberOrdinalsByName_.clear();
    memberOrdinalsBySignature_.clear();
    virtualMethodsByMember_.clear();
    virtualTablesByType_.clear();
    classLayoutsByType_.clear();
    structLayoutsByType_.clear();
    fieldLayoutsByMember_.clear();
    clearHirBindings();
    typesByOrdinal_.reserve(types.size());
    typeOrdinalByIdentity_.reserve(types.size());
    typeOrdinalsByName_.reserve(types.size() * 2U);
    for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
    {
        const NamedTypeDeclaration& type = types[ordinal];
        typesByOrdinal_.push_back(&type);
        typeOrdinalByIdentity_.try_emplace(
            std::string(type.identity.keyView()), ordinal);
        typeOrdinalsByName_[type.identity.name()].push_back(ordinal);
        const std::string_view qualifiedName = type.identity.fullyQualifiedNameView();
        if (qualifiedName != type.identity.name())
        {
            typeOrdinalsByName_[std::string(qualifiedName)].push_back(ordinal);
        }
    }
}

void ProjectCompilationIndexes::rebuildGenerics(
    std::span<const GenericTemplateRecord> templates,
    std::span<const GenericSpecializationRecord> specializations)
{
    genericTemplatesByOrdinal_.clear();
    genericTemplateByCanonicalKey_.clear();
    genericTemplateOrdinalsByName_.clear();
    genericSpecializationsByOrdinal_.clear();
    genericSpecializationBySyntax_.clear();
    genericSpecializationByKey_.clear();
    genericSpecializationByName_.clear();
    genericTemplatesByOrdinal_.reserve(templates.size());
    genericTemplateByCanonicalKey_.reserve(templates.size());
    genericTemplateOrdinalsByName_.reserve(templates.size());
    for (std::size_t ordinal = 0; ordinal < templates.size(); ++ordinal)
    {
        const GenericTemplateRecord& record = templates[ordinal];
        genericTemplatesByOrdinal_.push_back(&record);
        genericTemplateByCanonicalKey_.try_emplace(record.canonicalKey, &record);
        genericTemplateOrdinalsByName_[record.name].push_back(ordinal);
    }
    genericSpecializationsByOrdinal_.reserve(specializations.size());
    genericSpecializationBySyntax_.reserve(specializations.size());
    genericSpecializationByKey_.reserve(specializations.size());
    genericSpecializationByName_.reserve(specializations.size());
    for (const GenericSpecializationRecord& record : specializations)
    {
        genericSpecializationsByOrdinal_.push_back(&record);
        if (record.specializedSyntax != nullptr)
        {
            genericSpecializationBySyntax_.try_emplace(record.specializedSyntax, &record);
        }
        genericSpecializationByKey_.try_emplace(record.specializationKey, &record);
        genericSpecializationByName_.try_emplace(record.specializedName, &record);
    }
    clearBodyTypeDemands();
    clearGenericTypeDemands();
}

bool ProjectCompilationIndexes::registerBodyTypeDemand(std::string key)
{
    return bodyTypeDemandKeys_.insert(std::move(key)).second;
}

void ProjectCompilationIndexes::clearBodyTypeDemands() noexcept
{
    bodyTypeDemandKeys_.clear();
}

void ProjectCompilationIndexes::registerGenericTypeDemand(std::string key)
{
    genericTypeDemandKeys_.insert(std::move(key));
}

void ProjectCompilationIndexes::clearGenericTypeDemands() noexcept
{
    genericTypeDemandKeys_.clear();
}

void ProjectCompilationIndexes::rebuildMembers(std::span<const ClassMemberSymbol> members)
{
    membersByOrdinal_.clear();
    memberOrdinalsByName_.clear();
    memberOrdinalsBySignature_.clear();
    virtualMethodsByMember_.clear();
    virtualTablesByType_.clear();
    classLayoutsByType_.clear();
    structLayoutsByType_.clear();
    fieldLayoutsByMember_.clear();
    clearHirBindings();
    membersByOrdinal_.reserve(members.size());
    memberOrdinalsByName_.reserve(members.size());
    std::size_t typeCount = typesByOrdinal_.size();
    std::vector<std::size_t> memberCountsByType(typeCount, 0U);
    for (const ClassMemberSymbol& member : members)
    {
        typeCount = std::max(typeCount, member.declaringTypeOrdinal + 1U);
        if (member.declaringTypeOrdinal >= memberCountsByType.size())
        {
            memberCountsByType.resize(member.declaringTypeOrdinal + 1U, 0U);
        }
        ++memberCountsByType[member.declaringTypeOrdinal];
    }
    memberOrdinalsBySignature_.resize(typeCount);
    for (std::size_t typeOrdinal = 0; typeOrdinal < memberCountsByType.size(); ++typeOrdinal)
    {
        memberOrdinalsBySignature_[typeOrdinal].reserve(memberCountsByType[typeOrdinal]);
    }
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        const ClassMemberSymbol& member = members[ordinal];
        membersByOrdinal_.push_back(&member);
        memberOrdinalsByName_[member.name].push_back(ordinal);
        memberOrdinalsBySignature_[member.declaringTypeOrdinal][member.signatureKey].push_back(ordinal);
    }
}

void ProjectCompilationIndexes::rebuildLayouts(
    std::span<const ClassObjectLayout> classLayouts,
    std::span<const StructValueLayout> structLayouts,
    std::span<const ObjectFieldLayout> promotedFields,
    std::size_t memberCount,
    std::size_t typeCount)
{
    clearHirBindings();
    classLayoutsByType_.assign(typeCount, nullptr);
    structLayoutsByType_.assign(typeCount, nullptr);
    fieldLayoutsByMember_.assign(memberCount, nullptr);
    const auto registerField = [this](const ObjectFieldLayout& field)
    {
        if (field.memberOrdinal >= fieldLayoutsByMember_.size())
        {
            fieldLayoutsByMember_.resize(field.memberOrdinal + 1U, nullptr);
        }
        if (fieldLayoutsByMember_[field.memberOrdinal] == nullptr)
        {
            fieldLayoutsByMember_[field.memberOrdinal] = &field;
        }
    };
    for (const StructValueLayout& layout : structLayouts)
    {
        if (layout.typeOrdinal >= structLayoutsByType_.size())
        {
            structLayoutsByType_.resize(layout.typeOrdinal + 1U, nullptr);
        }
        structLayoutsByType_[layout.typeOrdinal] = &layout;
        for (const ObjectFieldLayout& field : layout.fields) registerField(field);
    }
    for (const ClassObjectLayout& layout : classLayouts)
    {
        if (layout.typeOrdinal >= classLayoutsByType_.size())
        {
            classLayoutsByType_.resize(layout.typeOrdinal + 1U, nullptr);
        }
        classLayoutsByType_[layout.typeOrdinal] = &layout;
        for (const ObjectFieldLayout& field : layout.fields) registerField(field);
    }
    for (const ObjectFieldLayout& field : promotedFields) registerField(field);
}

void ProjectCompilationIndexes::rebuildHirBindings(
    std::span<const MethodLocalSymbol> locals,
    std::span<const MethodLambdaBinding> lambdas,
    std::span<const MethodLambdaValueBinding> lambdaValues,
    std::span<const MethodCleanupRegion> cleanupRegions)
{
    clearHirBindings();
    hirLocalBySyntax_.reserve(locals.size());
    for (const MethodLocalSymbol& local : locals)
        if (local.syntax != nullptr) hirLocalBySyntax_.try_emplace(local.syntax, &local);
    hirLambdaBySyntax_.reserve(lambdas.size());
    for (const MethodLambdaBinding& lambda : lambdas)
        if (lambda.syntax != nullptr) hirLambdaBySyntax_.try_emplace(lambda.syntax, &lambda);
    hirLambdaValueBySyntax_.reserve(lambdaValues.size());
    for (const MethodLambdaValueBinding& value : lambdaValues)
        if (value.syntax != nullptr) hirLambdaValueBySyntax_.try_emplace(value.syntax, &value);
    hirCleanupRegionBySyntax_.reserve(cleanupRegions.size());
    for (const MethodCleanupRegion& region : cleanupRegions)
        if (region.syntax != nullptr) hirCleanupRegionBySyntax_.try_emplace(region.syntax, &region);
}

void ProjectCompilationIndexes::clearHirBindings() noexcept
{
    hirLocalBySyntax_.clear();
    hirLambdaBySyntax_.clear();
    hirLambdaValueBySyntax_.clear();
    hirCleanupRegionBySyntax_.clear();
    clearMirFunctions();
}

void ProjectCompilationIndexes::rebuildMirFunctions(
    std::span<const ProjectOopMirFunction> functions,
    std::size_t memberCount,
    std::size_t namespaceFunctionCount)
{
    clearMirFunctions();
    mirFunctionsByMember_.assign(memberCount, nullptr);
    mirFunctionsByNamespaceFunction_.assign(namespaceFunctionCount, nullptr);
    mirAsyncLocalsByFunction_.reserve(functions.size());
    mirAsyncParametersByFunction_.reserve(functions.size());
    for (const ProjectOopMirFunction& function : functions)
    {
        if (function.namespaceFunctionOrdinal.has_value())
        {
            const std::size_t ordinal = *function.namespaceFunctionOrdinal;
            if (ordinal >= mirFunctionsByNamespaceFunction_.size())
                mirFunctionsByNamespaceFunction_.resize(ordinal + 1U, nullptr);
            mirFunctionsByNamespaceFunction_[ordinal] = &function;
        }
        else if (!function.isLambda && !function.isSyntheticImplicitConstructor &&
                 function.memberOrdinal < mirFunctionsByMember_.size())
        {
            mirFunctionsByMember_[function.memberOrdinal] = &function;
        }
        MirAsyncFrameIndex& locals = mirAsyncLocalsByFunction_[&function];
        locals.reserve(function.asyncFrameLocals.size());
        for (const MirAsyncFrameSlot& slot : function.asyncFrameLocals)
            locals.emplace(std::get<0>(slot), &slot);
        MirAsyncFrameIndex& parameters = mirAsyncParametersByFunction_[&function];
        parameters.reserve(function.asyncFrameParameters.size());
        for (const MirAsyncFrameSlot& slot : function.asyncFrameParameters)
            parameters.emplace(std::get<0>(slot), &slot);
    }
}

void ProjectCompilationIndexes::rebuildMirLayouts(
    std::span<const ProjectOopMirStructLayout> structLayouts,
    std::span<const ProjectOopMirClassLayout> classLayouts,
    std::size_t typeCount)
{
    mirStructLayoutsByType_.assign(typeCount, nullptr);
    mirStructLayoutsByTypeKey_.clear();
    mirStructLayoutsByTypeKey_.reserve(structLayouts.size());
    for (const ProjectOopMirStructLayout& layout : structLayouts)
    {
        if (layout.typeOrdinal >= mirStructLayoutsByType_.size())
            mirStructLayoutsByType_.resize(layout.typeOrdinal + 1U, nullptr);
        mirStructLayoutsByType_[layout.typeOrdinal] = &layout;
        mirStructLayoutsByTypeKey_.try_emplace(layout.typeKey, &layout);
    }

    mirClassLayoutsByType_.assign(typeCount, nullptr);
    mirClassLayoutsByTypeKey_.clear();
    mirClassLayoutsByTypeKey_.reserve(classLayouts.size());
    for (const ProjectOopMirClassLayout& layout : classLayouts)
    {
        if (layout.typeOrdinal >= mirClassLayoutsByType_.size())
            mirClassLayoutsByType_.resize(layout.typeOrdinal + 1U, nullptr);
        mirClassLayoutsByType_[layout.typeOrdinal] = &layout;
        mirClassLayoutsByTypeKey_.try_emplace(layout.typeKey, &layout);
    }
}

void ProjectCompilationIndexes::clearMirFunctions() noexcept
{
    mirFunctionsByMember_.clear();
    mirFunctionsByNamespaceFunction_.clear();
    mirStructLayoutsByType_.clear();
    mirStructLayoutsByTypeKey_.clear();
    mirClassLayoutsByType_.clear();
    mirClassLayoutsByTypeKey_.clear();
    mirAsyncLocalsByFunction_.clear();
    mirAsyncParametersByFunction_.clear();
}

void ProjectCompilationIndexes::rebuildDispatch(
    std::span<const VirtualMethodInfo> virtualMethods,
    std::span<const ClassVirtualTable> classTables,
    std::size_t memberCount,
    std::size_t typeCount)
{
    virtualMethodsByMember_.assign(memberCount, nullptr);
    for (const VirtualMethodInfo& method : virtualMethods)
    {
        if (method.memberOrdinal >= virtualMethodsByMember_.size())
        {
            virtualMethodsByMember_.resize(method.memberOrdinal + 1U, nullptr);
        }
        virtualMethodsByMember_[method.memberOrdinal] = &method;
    }
    virtualTablesByType_.assign(typeCount, nullptr);
    for (const ClassVirtualTable& table : classTables)
    {
        if (table.typeOrdinal >= virtualTablesByType_.size())
        {
            virtualTablesByType_.resize(table.typeOrdinal + 1U, nullptr);
        }
        virtualTablesByType_[table.typeOrdinal] = &table;
    }
}

const NamedTypeDeclaration* ProjectCompilationIndexes::typeForOrdinal(
    std::size_t ordinal) const noexcept
{
    return ordinal < typesByOrdinal_.size() ? typesByOrdinal_[ordinal] : nullptr;
}

std::optional<std::size_t> ProjectCompilationIndexes::typeOrdinalForIdentity(
    std::string_view identity) const
{
    const auto iterator = typeOrdinalByIdentity_.find(identity);
    return iterator == typeOrdinalByIdentity_.end()
        ? std::nullopt : std::optional<std::size_t>(iterator->second);
}

std::span<const std::size_t> ProjectCompilationIndexes::typeOrdinalsForName(
    std::string_view name) const
{
    const auto iterator = typeOrdinalsByName_.find(name);
    return iterator == typeOrdinalsByName_.end()
        ? std::span<const std::size_t>{} : std::span<const std::size_t>(iterator->second);
}

const ClassMemberSymbol* ProjectCompilationIndexes::memberForOrdinal(
    std::size_t ordinal) const noexcept
{
    return ordinal < membersByOrdinal_.size() ? membersByOrdinal_[ordinal] : nullptr;
}

std::span<const std::size_t> ProjectCompilationIndexes::memberOrdinalsForName(
    std::string_view name) const noexcept
{
    const auto iterator = memberOrdinalsByName_.find(name);
    return iterator == memberOrdinalsByName_.end()
        ? std::span<const std::size_t>{} : std::span<const std::size_t>(iterator->second);
}

std::span<const std::size_t> ProjectCompilationIndexes::memberOrdinalsForSignature(
    std::size_t typeOrdinal,
    std::string_view signature) const noexcept
{
    if (typeOrdinal >= memberOrdinalsBySignature_.size())
    {
        return {};
    }
    const MemberSignatureIndex& index = memberOrdinalsBySignature_[typeOrdinal];
    const auto iterator = index.find(signature);
    return iterator == index.end()
        ? std::span<const std::size_t>{} : std::span<const std::size_t>(iterator->second);
}

const VirtualMethodInfo* ProjectCompilationIndexes::virtualMethodForMember(
    std::size_t memberOrdinal) const noexcept
{
    return memberOrdinal < virtualMethodsByMember_.size()
        ? virtualMethodsByMember_[memberOrdinal] : nullptr;
}

const ClassVirtualTable* ProjectCompilationIndexes::virtualTableForType(
    std::size_t typeOrdinal) const noexcept
{
    return typeOrdinal < virtualTablesByType_.size() ? virtualTablesByType_[typeOrdinal] : nullptr;
}

const ClassObjectLayout* ProjectCompilationIndexes::classLayoutForType(
    std::size_t typeOrdinal) const noexcept
{
    return typeOrdinal < classLayoutsByType_.size() ? classLayoutsByType_[typeOrdinal] : nullptr;
}

const StructValueLayout* ProjectCompilationIndexes::structLayoutForType(
    std::size_t typeOrdinal) const noexcept
{
    return typeOrdinal < structLayoutsByType_.size() ? structLayoutsByType_[typeOrdinal] : nullptr;
}

const ObjectFieldLayout* ProjectCompilationIndexes::fieldLayoutForMember(
    std::size_t memberOrdinal) const noexcept
{
    return memberOrdinal < fieldLayoutsByMember_.size()
        ? fieldLayoutsByMember_[memberOrdinal] : nullptr;
}

const GenericTemplateRecord* ProjectCompilationIndexes::genericTemplateForOrdinal(
    std::size_t ordinal) const noexcept
{
    return ordinal < genericTemplatesByOrdinal_.size() ? genericTemplatesByOrdinal_[ordinal] : nullptr;
}

const GenericTemplateRecord* ProjectCompilationIndexes::genericTemplateForCanonicalKey(
    std::string_view canonicalKey) const noexcept
{
    const auto iterator = genericTemplateByCanonicalKey_.find(canonicalKey);
    return iterator == genericTemplateByCanonicalKey_.end() ? nullptr : iterator->second;
}

std::span<const std::size_t> ProjectCompilationIndexes::genericTemplateOrdinalsForName(
    std::string_view name) const noexcept
{
    const auto iterator = genericTemplateOrdinalsByName_.find(name);
    return iterator == genericTemplateOrdinalsByName_.end()
        ? std::span<const std::size_t>{} : std::span<const std::size_t>(iterator->second);
}

const GenericSpecializationRecord* ProjectCompilationIndexes::genericSpecializationForOrdinal(
    std::size_t ordinal) const noexcept
{
    return ordinal < genericSpecializationsByOrdinal_.size()
        ? genericSpecializationsByOrdinal_[ordinal] : nullptr;
}

const GenericSpecializationRecord* ProjectCompilationIndexes::genericSpecializationForSyntax(
    const ast::AstNode* syntax) const noexcept
{
    const auto iterator = genericSpecializationBySyntax_.find(syntax);
    return iterator == genericSpecializationBySyntax_.end() ? nullptr : iterator->second;
}

const GenericSpecializationRecord* ProjectCompilationIndexes::genericSpecializationForKey(
    std::string_view key) const noexcept
{
    const auto iterator = genericSpecializationByKey_.find(key);
    return iterator == genericSpecializationByKey_.end() ? nullptr : iterator->second;
}

const GenericSpecializationRecord* ProjectCompilationIndexes::genericSpecializationForName(
    std::string_view name) const noexcept
{
    const auto iterator = genericSpecializationByName_.find(name);
    return iterator == genericSpecializationByName_.end() ? nullptr : iterator->second;
}

const MethodLocalSymbol* ProjectCompilationIndexes::hirLocalForSyntax(
    const ast::AstNode* syntax) const noexcept
{
    const auto iterator = hirLocalBySyntax_.find(syntax);
    return iterator == hirLocalBySyntax_.end() ? nullptr : iterator->second;
}

const MethodLambdaBinding* ProjectCompilationIndexes::hirLambdaForSyntax(
    const ast::AstNode* syntax) const noexcept
{
    const auto iterator = hirLambdaBySyntax_.find(syntax);
    return iterator == hirLambdaBySyntax_.end() ? nullptr : iterator->second;
}

const MethodLambdaValueBinding* ProjectCompilationIndexes::hirLambdaValueForSyntax(
    const ast::AstNode* syntax) const noexcept
{
    const auto iterator = hirLambdaValueBySyntax_.find(syntax);
    return iterator == hirLambdaValueBySyntax_.end() ? nullptr : iterator->second;
}

const MethodCleanupRegion* ProjectCompilationIndexes::hirCleanupRegionForSyntax(
    const ast::AstNode* syntax) const noexcept
{
    const auto iterator = hirCleanupRegionBySyntax_.find(syntax);
    return iterator == hirCleanupRegionBySyntax_.end() ? nullptr : iterator->second;
}

const ProjectOopMirFunction* ProjectCompilationIndexes::mirFunctionForMember(
    std::size_t memberOrdinal) const noexcept
{
    return memberOrdinal < mirFunctionsByMember_.size()
        ? mirFunctionsByMember_[memberOrdinal] : nullptr;
}

const ProjectOopMirFunction* ProjectCompilationIndexes::mirFunctionForNamespaceFunction(
    std::size_t functionOrdinal) const noexcept
{
    return functionOrdinal < mirFunctionsByNamespaceFunction_.size()
        ? mirFunctionsByNamespaceFunction_[functionOrdinal] : nullptr;
}

const ProjectOopMirStructLayout* ProjectCompilationIndexes::mirStructLayoutForType(
    std::size_t typeOrdinal) const noexcept
{
    return typeOrdinal < mirStructLayoutsByType_.size()
        ? mirStructLayoutsByType_[typeOrdinal] : nullptr;
}

const ProjectOopMirStructLayout* ProjectCompilationIndexes::mirStructLayoutForTypeKey(
    std::string_view typeKey) const noexcept
{
    const auto iterator = mirStructLayoutsByTypeKey_.find(typeKey);
    return iterator == mirStructLayoutsByTypeKey_.end() ? nullptr : iterator->second;
}

const ProjectOopMirClassLayout* ProjectCompilationIndexes::mirClassLayoutForType(
    std::size_t typeOrdinal) const noexcept
{
    return typeOrdinal < mirClassLayoutsByType_.size()
        ? mirClassLayoutsByType_[typeOrdinal] : nullptr;
}

const ProjectOopMirClassLayout* ProjectCompilationIndexes::mirClassLayoutForTypeKey(
    std::string_view typeKey) const noexcept
{
    const auto iterator = mirClassLayoutsByTypeKey_.find(typeKey);
    return iterator == mirClassLayoutsByTypeKey_.end() ? nullptr : iterator->second;
}

const ProjectCompilationIndexes::MirAsyncFrameSlot*
ProjectCompilationIndexes::mirAsyncLocalForOrdinal(
    const ProjectOopMirFunction& function,
    std::size_t localOrdinal) const noexcept
{
    const auto functionIndex = mirAsyncLocalsByFunction_.find(&function);
    if (functionIndex == mirAsyncLocalsByFunction_.end()) return nullptr;
    const auto slot = functionIndex->second.find(localOrdinal);
    return slot == functionIndex->second.end() ? nullptr : slot->second;
}

const ProjectCompilationIndexes::MirAsyncFrameSlot*
ProjectCompilationIndexes::mirAsyncParameterForOrdinal(
    const ProjectOopMirFunction& function,
    std::size_t parameterOrdinal,
    std::string_view typeKey) const noexcept
{
    const auto functionIndex = mirAsyncParametersByFunction_.find(&function);
    if (functionIndex == mirAsyncParametersByFunction_.end()) return nullptr;
    const auto [first, last] = functionIndex->second.equal_range(parameterOrdinal);
    for (auto slot = first; slot != last; ++slot)
        if (std::get<2>(*slot->second) == typeKey) return slot->second;
    return nullptr;
}
}
