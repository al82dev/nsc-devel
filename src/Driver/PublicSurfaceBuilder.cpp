#include "PublicSurfaceBuilder.h"
#include "PublicSurfaceEmitterSupport.h"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <utility>

namespace nativesharp::driver
{
using namespace public_surface_support;

constexpr std::uint8_t kPublicAccessibility = 3U;
constexpr std::uint8_t kGenericMethodKind = 3U;
constexpr std::uint32_t kAccessorStatic = 1U << 0U;
constexpr std::uint32_t kAccessorReadonly = 1U << 1U;
constexpr std::uint32_t kAccessorVirtual = 1U << 3U;
constexpr std::uint32_t kAccessorOverride = 1U << 4U;
constexpr std::uint32_t kAccessorAbstract = 1U << 5U;
constexpr std::uint32_t kAccessorSealed = 1U << 6U;
constexpr std::uint32_t kPropertyAccessor = 1U << 12U;
constexpr std::uint32_t kIndexerAccessor = 1U << 13U;
constexpr std::uint32_t kGetter = 1U << 14U;
constexpr std::uint32_t kSetter = 1U << 15U;
constexpr std::uint32_t kAccessorNewSlot = 1U << 19U;
constexpr std::uint32_t kAccessorDeclarationFlags =
    kAccessorStatic | kAccessorVirtual | kAccessorOverride | kAccessorAbstract | kAccessorSealed | kAccessorNewSlot;

PublicSurfaceBuilder::PublicSurfaceBuilder(const interop::LibraryMetadata &metadata, std::string &source,
                                           std::string &error)
    : metadata(metadata), source(source), error(error), generated(&generatedStorage)
{
}

bool PublicSurfaceBuilder::collectBaseAlias(const std::string &typeKey)
{
    const std::string baseKey = metadataBaseTypeKey(typeKey);
    if (!baseKey.starts_with("type:"))
        return true;
    if (baseAliases.contains(baseKey))
        return true;
    std::string sourceType;
    if (!sourceTypeForMetadataKey(metadata, baseKey, sourceType, error))
        return false;
    const std::string alias = "__NscMetadataBase" + std::to_string(baseAliases.size());
    baseAliases.emplace(baseKey, alias);
    (*generated) << "using " << alias << " = " << sourceType << ";\n";
    return true;
}

bool PublicSurfaceBuilder::sourceBaseType(const std::string &typeKey, std::string &sourceType)
{
    const std::string baseKey = metadataBaseTypeKey(typeKey);
    const auto alias = baseAliases.find(baseKey);
    if (alias != baseAliases.end())
    {
        sourceType = alias->second;
        return true;
    }
    return sourceTypeForMetadataKey(metadata, typeKey, sourceType, error);
}

void PublicSurfaceBuilder::emitLifetimeParameters(const std::vector<std::string> &lifetimes)
{
    if (lifetimes.empty())
        return;
    (*generated) << '<';
    for (std::size_t index = 0; index < lifetimes.size(); ++index)
    {
        if (index != 0U)
            (*generated) << ", ";
        (*generated) << "lifetime " << lifetimes[index];
    }
    (*generated) << '>';
}

void PublicSurfaceBuilder::emitLifetimeConstraints(const std::vector<std::pair<std::string, std::string>> &constraints)
{
    for (const auto &[longer, shorter] : constraints)
        (*generated) << " where " << longer << " : " << shorter;
}

bool PublicSurfaceBuilder::emitThrowsContract(const std::string &errorTypeKey)
{
    if (errorTypeKey.empty())
        return true;
    std::string errorType;
    if (!sourceTypeForMetadataKey(metadata, errorTypeKey, errorType, error))
        return false;
    (*generated) << " throws " << errorType;
    return true;
}

bool PublicSurfaceBuilder::emitParameter(const interop::LibraryMetadataParameter &parameter,
                                         const std::string &memberName, const std::size_t index)
{
    std::string parameterType;
    std::string sourceTypeKey = parameter.typeKey;
    if (parameter.passKind != interop::LibraryMetadataParameterPassKind::Value)
    {
        if (sourceTypeKey.empty() || !sourceTypeKey.ends_with('*'))
        {
            error = "metadata by-reference parameter has no pointer carrier in "
                    "member '" +
                    memberName + "'";
            return false;
        }
        sourceTypeKey.pop_back();
    }
    if (!sourceTypeForMetadataKey(metadata, sourceTypeKey, parameterType, error))
        return false;
    (*generated) << (parameter.isParameterPack ? "params " : "")
                 << parameterPassPrefix(parameter.passKind, parameter.lifetimeName)
                 << ownershipPrefix(parameter.ownership) << (parameter.isDynamicTrait ? "dyn " : "") << parameterType
                 << ' ' << (parameter.name.empty() ? "arg" + std::to_string(index) : parameter.name);
    if (parameter.ffiLengthParameter != UINT32_MAX)
    {
        if (parameter.ffiLengthParameterName.empty())
        {
            error = "metadata FFI length parameter has no source name";
            return false;
        }
        (*generated) << " length(" << parameter.ffiLengthParameterName << ')';
    }
    if (parameter.ffiCallbackLifetime != 0U)
        (*generated) << " callback(" << (parameter.ffiCallbackLifetime == 2U ? "escaping" : "scoped") << ')';
    std::string defaultValue;
    if (!sourceDefaultValue(parameter, defaultValue, error))
        return false;
    if (!defaultValue.empty())
        (*generated) << " = " << defaultValue;
    return true;
}

bool PublicSurfaceBuilder::emitParameterSlice(std::string_view memberName,
                                              std::span<const interop::LibraryMetadataParameter> parameters,
                                              const std::size_t begin, const std::size_t count)
{
    if (begin > parameters.size() || count > parameters.size() - begin)
    {
        error = "metadata member '" + std::string(memberName) +
            "' has an invalid parameter range";
        return false;
    }
    for (std::size_t index = 0; index < count; ++index)
    {
        if (index != 0)
            (*generated) << ", ";
        if (!emitParameter(
                parameters[begin + index], std::string(memberName), begin + index))
            return false;
    }
    return true;
}

bool PublicSurfaceBuilder::emitParameterRange(std::string_view memberName,
                                              std::span<const interop::LibraryMetadataParameter> parameters,
                                              const std::size_t count)
{
    return emitParameterSlice(memberName, parameters, 0U, count);
}

bool PublicSurfaceBuilder::emitParameters(const interop::LibraryMetadataMember &member)
{
    if (!emitParameterRange(member.name, member.parameters, member.parameters.size()))
        return false;
    if ((member.flags & interop::kMetadataMemberVariadic) != 0U)
    {
        if (!member.parameters.empty())
            (*generated) << ", ";
        (*generated) << "...";
    }
    return true;
}

bool PublicSurfaceBuilder::emitAssociatedItems(const interop::LibraryMetadataType &owner)
{
    for (const interop::LibraryMetadataAssociatedItem &item : owner.associatedItems)
    {
        (*generated) << "    ";
        if (owner.kind != interop::LibraryMetadataTypeKind::Interface)
            (*generated) << "associated ";
        if (item.kind == interop::LibraryMetadataAssociatedItemKind::Type)
        {
            (*generated) << "type " << item.name;
            if (item.hasDefault)
            {
                std::string associatedType;
                if (!sourceTypeForMetadataKey(metadata, item.typeKey, associatedType, error))
                    return false;
                (*generated) << " = " << associatedType;
            }
            (*generated) << ";\n";
            continue;
        }

        std::string constantType;
        if (!sourceTypeForMetadataKey(metadata, item.typeKey, constantType, error))
            return false;
        (*generated) << "const " << constantType << ' ' << item.name;
        if (item.hasDefault)
        {
            (*generated) << " = ";
            if (item.typeKey == "builtin:bool")
                (*generated) << (item.valueBits == 0U ? "false" : "true");
            else if (item.typeKey == "builtin:sbyte" || item.typeKey == "builtin:short" ||
                     item.typeKey == "builtin:int" || item.typeKey == "builtin:long")
                (*generated) << static_cast<std::int64_t>(item.valueBits);
            else
                (*generated) << item.valueBits;
        }
        (*generated) << ";\n";
    }
    return true;
}

bool PublicSurfaceBuilder::recoverAccessorIdentity(const interop::LibraryMetadataMember &member, bool &indexer,
                                                   bool &getter, std::string &associatedName, std::string &contractKey)
{
    const bool property = (member.flags & kPropertyAccessor) != 0U;
    indexer = (member.flags & kIndexerAccessor) != 0U;
    getter = (member.flags & kGetter) != 0U;
    const bool setter = (member.flags & kSetter) != 0U;
    if (property == indexer || getter == setter)
    {
        error = "metadata accessor '" + member.name + "' has inconsistent property/indexer flags";
        return false;
    }
    if (property)
    {
        const std::string_view prefix = getter ? "get_" : "set_";
        if (!member.name.starts_with(prefix) || member.name.size() == prefix.size())
        {
            error = "metadata property accessor '" + member.name + "' has no recoverable property name";
            return false;
        }
        associatedName = member.name.substr(prefix.size());
    }
    else
    {
        associatedName = "Item";
    }
    std::ostringstream key;
    key << (indexer ? "indexer:" : "property:") << associatedName;
    if (indexer)
    {
        if (setter && member.parameters.empty())
        {
            error = "metadata indexer setter '" + member.name + "' has no value parameter";
            return false;
        }
        const std::size_t indexCount = member.parameters.size() - (setter ? 1U : 0U);
        for (std::size_t index = 0; index < indexCount; ++index)
            key << ':' << static_cast<unsigned>(member.parameters[index].passKind) << ':'
                << member.parameters[index].typeKey;
    }
    contractKey = key.str();
    return true;
}

bool PublicSurfaceBuilder::collectAccessorContracts(const std::vector<const interop::LibraryMetadataMember *> &members,
                                                    std::vector<AccessorContract> &contracts)
{
    std::unordered_map<std::string, std::size_t> contractByKey;
    for (const interop::LibraryMetadataMember *member : members)
    {
        if ((member->flags & (kPropertyAccessor | kIndexerAccessor)) == 0U)
            continue;
        bool indexer = false;
        bool getter = false;
        std::string associatedName;
        std::string contractKey;
        if (!recoverAccessorIdentity(*member, indexer, getter, associatedName, contractKey))
            return false;
        const auto [position, inserted] = contractByKey.emplace(contractKey, contracts.size());
        if (inserted)
            contracts.push_back({});
        AccessorContract &contract = contracts[position->second];
        contract.isIndexer = indexer;
        contract.name = std::move(associatedName);
        const interop::LibraryMetadataMember *&slot = getter ? contract.getter : contract.setter;
        if (slot != nullptr)
        {
            error = "metadata contains duplicate accessor '" + member->name + "'";
            return false;
        }
        slot = member;
        contract.declarationOrder.push_back(member);
    }
    return true;
}

bool PublicSurfaceBuilder::validateAccessorContract(const AccessorContract &contract)
{
    if (contract.getter == nullptr)
    {
        error = "metadata-backed source import cannot materialize set-only accessor '" + contract.setter->name + "'";
        return false;
    }
    if (contract.setter == nullptr)
        return true;
    const interop::LibraryMetadataMember &shape = *contract.getter;
    if ((shape.flags & interop::kMetadataMemberReferenceReturn) != 0U ||
        (contract.setter->flags & interop::kMetadataMemberReferenceReturn) != 0U)
    {
        error = "metadata reference-returning indexer '" + contract.name + "' cannot declare a setter";
        return false;
    }
    if (contract.setter->parameters.empty() || contract.setter->parameters.back().typeKey != shape.declaredTypeKey ||
        (contract.setter->flags & kAccessorDeclarationFlags) != (shape.flags & kAccessorDeclarationFlags))
    {
        error = "metadata accessor pair for '" + contract.name + "' has inconsistent type or modifiers";
        return false;
    }
    if ((contract.setter->flags & kAccessorReadonly) != 0U)
    {
        error = "metadata setter for '" + contract.name + "' cannot be readonly";
        return false;
    }
    return true;
}

std::uint8_t PublicSurfaceBuilder::accessorAccessibility(const AccessorContract &contract)
{
    std::uint8_t accessibility = contract.getter->accessibility;
    if (contract.setter == nullptr)
        return accessibility;
    if (accessibility == 3U || contract.setter->accessibility == 3U)
        return std::uint8_t{3U};
    if (accessibility == 1U || contract.setter->accessibility == 1U)
        return std::uint8_t{1U};
    if (accessibility == 2U || contract.setter->accessibility == 2U)
        return std::uint8_t{2U};
    return std::uint8_t{0U};
}

void PublicSurfaceBuilder::emitAccessorModifiers(const interop::LibraryMetadataMember &shape, bool interfaceContext,
                                                 bool preserveStatic, bool declarationReadonly)
{
    if (preserveStatic && (shape.flags & kAccessorStatic) != 0U)
        (*generated) << "static ";
    if (interfaceContext)
        return;
    if (!preserveStatic && (shape.flags & kAccessorStatic) != 0U)
        (*generated) << "static ";
    if (declarationReadonly)
        (*generated) << "readonly ";
    if ((shape.flags & kAccessorSealed) != 0U)
        (*generated) << "sealed ";
    if ((shape.flags & kAccessorNewSlot) != 0U)
        (*generated) << "new ";
    if ((shape.flags & kAccessorOverride) != 0U)
        (*generated) << "override ";
    else if ((shape.flags & kAccessorAbstract) != 0U)
        (*generated) << "abstract ";
    else if ((shape.flags & kAccessorVirtual) != 0U)
        (*generated) << "virtual ";
}

bool PublicSurfaceBuilder::emitAccessorMember(const interop::LibraryMetadataMember &accessor,
                                              const AccessorContract &contract, std::uint8_t accessibility,
                                              bool interfaceContext, bool declarationReadonly)
{
    (*generated) << "        ";
    if (!interfaceContext && accessor.accessibility != accessibility)
        (*generated) << sourceAccessibility(accessor.accessibility) << ' ';
    if (!interfaceContext && !declarationReadonly && &accessor == contract.getter &&
        (accessor.flags & kAccessorReadonly) != 0U)
        (*generated) << "readonly ";
    (*generated) << (&accessor == contract.getter ? "get" : "set");
    if (!emitThrowsContract(accessor.declaredErrorTypeKey))
        return false;
    (*generated) << (((accessor.flags & kAccessorAbstract) != 0U) ? ";\n" : " { }\n");
    return true;
}

bool PublicSurfaceBuilder::emitAccessorContract(const AccessorContract &contract, bool interfaceContext,
                                                bool preserveStatic,
                                                std::set<const interop::LibraryMetadataMember *> &materialized)
{
    if (!validateAccessorContract(contract))
        return false;
    const interop::LibraryMetadataMember &shape = *contract.getter;
    const bool declarationReadonly = contract.setter == nullptr && (shape.flags & kAccessorReadonly) != 0U;
    const std::uint8_t accessibility = accessorAccessibility(contract);
    std::string declaredType;
    if (!sourceTypeForMetadataKey(metadata, shape.declaredTypeKey, declaredType, error))
        return false;
    auto declaredOwnership = shape.declaredOwnership;
    if (contract.setter != nullptr &&
        contract.setter->parameters.back().ownership != interop::LibraryMetadataOwnershipKind::None)
        declaredOwnership = contract.setter->parameters.back().ownership;
    (*generated) << "    " << sourceAccessibility(accessibility) << ' ';
    emitAccessorModifiers(shape, interfaceContext, preserveStatic, declarationReadonly);
    if ((shape.flags & interop::kMetadataMemberReferenceReturn) != 0U)
        (*generated) << "ref" << lifetimeAnnotation(shape.returnLifetimeName) << ' ';
    (*generated) << ownershipPrefix(declaredOwnership)
                 << ((shape.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << declaredType << ' ';
    if (contract.isIndexer)
    {
        (*generated) << "this[";
        if (!emitParameterRange(shape.name, shape.parameters, shape.parameters.size()))
            return false;
        (*generated) << ']';
    }
    else
    {
        (*generated) << contract.name;
    }
    (*generated) << "\n    {\n";
    for (const interop::LibraryMetadataMember *accessor : contract.declarationOrder)
    {
        if (!emitAccessorMember(*accessor, contract, accessibility, interfaceContext, declarationReadonly))
            return false;
        materialized.insert(accessor);
    }
    (*generated) << "    }\n";
    return true;
}

bool PublicSurfaceBuilder::emitAccessorContracts(const std::vector<const interop::LibraryMetadataMember *> &members,
                                                 bool interfaceContext, bool preserveStatic,
                                                 std::set<const interop::LibraryMetadataMember *> &materialized)
{
    std::vector<AccessorContract> contracts;
    if (!collectAccessorContracts(members, contracts))
        return false;
    for (const AccessorContract &contract : contracts)
        if (!emitAccessorContract(contract, interfaceContext, preserveStatic, materialized))
            return false;
    return true;
}

bool PublicSurfaceBuilder::emitDelegateType(const interop::LibraryMetadataType &type,
                                            std::string_view typeAccessibility, std::string_view typeName,
                                            bool topLevel, const std::string &namespaceName)
{
    std::string returnType;
    if (!sourceTypeForMetadataKey(metadata, type.delegateReturnTypeKey, returnType, error))
        return false;
    (*generated) << typeAccessibility << ' ';
    if ((type.delegateFlags & 1U) != 0U)
        (*generated) << (((type.delegateFlags & 2U) != 0U) ? "extern vectorcall " : "extern C ");
    (*generated) << "delegate ";
    if (type.delegateReferenceReturn)
        (*generated) << "ref" << lifetimeAnnotation(type.delegateReturnLifetimeName) << ' ';
    if (type.delegateReturnDynamicTrait)
        (*generated) << "dyn ";
    (*generated) << returnType << ' ' << typeName;
    emitLifetimeParameters(type.lifetimeParameters);
    (*generated) << '(';
    for (std::size_t parameterIndex = 0; parameterIndex < type.delegateParameterTypeKeys.size(); ++parameterIndex)
    {
        if (parameterIndex != 0)
            (*generated) << ", ";
        std::string parameterType;
        std::string parameterTypeKey = type.delegateParameterTypeKeys[parameterIndex];
        const interop::LibraryMetadataParameterPassKind passKind =
            parameterIndex < type.delegateParameterPassKinds.size() ? type.delegateParameterPassKinds[parameterIndex]
                                                                    : interop::LibraryMetadataParameterPassKind::Value;
        if (passKind != interop::LibraryMetadataParameterPassKind::Value)
        {
            if (parameterTypeKey.empty() || !parameterTypeKey.ends_with('*'))
            {
                error = "metadata delegate by-reference parameter has no pointer carrier";
                return false;
            }
            parameterTypeKey.pop_back();
        }
        if (!sourceTypeForMetadataKey(metadata, parameterTypeKey, parameterType, error))
            return false;
        (*generated) << parameterPassPrefix(passKind,
                                            parameterIndex < type.delegateParameterLifetimeNames.size()
                                                ? std::string_view(type.delegateParameterLifetimeNames[parameterIndex])
                                                : std::string_view{});
        if (parameterIndex < type.delegateParameterDynamicTraits.size() &&
            type.delegateParameterDynamicTraits[parameterIndex] != 0U)
            (*generated) << "dyn ";
        (*generated) << parameterType << " arg" << parameterIndex;
    }
    (*generated) << ')';
    if (!emitThrowsContract(type.delegateErrorTypeKey))
        return false;
    emitLifetimeConstraints(type.lifetimeOutlivesConstraints);
    (*generated) << ";\n";
    if (topLevel && !namespaceName.empty())
        (*generated) << "}\n";
    return true;
}

bool PublicSurfaceBuilder::emitEnumType(const interop::LibraryMetadataType &type, std::string_view typeAccessibility,
                                        std::string_view typeName, bool topLevel, const std::string &namespaceName)
{
    std::string underlyingType;
    if (!sourceTypeForMetadataKey(metadata,
                                  type.enumUnderlyingTypeKey.empty() ? "builtin:int" : type.enumUnderlyingTypeKey,
                                  underlyingType, error))
    {
        return false;
    }
    (*generated) << typeAccessibility << " enum " << typeName << " : " << underlyingType << "\n{\n";
    bool firstEnumerator = true;
    constexpr std::uint8_t kFieldKind = 0U;
    const std::string declaringTypeKey = "type:" + type.identityKey;
    for (const interop::LibraryMetadataMember &member : metadata.members)
    {
        if (member.kind != kFieldKind || member.declaringTypeKey != declaringTypeKey || member.constantKind == 0U)
        {
            continue;
        }
        if (!firstEnumerator)
            (*generated) << ",\n";
        (*generated) << "    " << member.name << " = " << enumConstantSource(type, member.constantBits);
        firstEnumerator = false;
    }
    if (!firstEnumerator)
        (*generated) << '\n';
    (*generated) << "}\n";
    if (topLevel && !namespaceName.empty())
        (*generated) << "}\n";
    return true;
}

bool PublicSurfaceBuilder::emitInterfaceMethod(
    const interop::LibraryMetadataMember &member, bool isTrait,
    const std::set<const interop::LibraryMetadataMember *> &materializedAccessors)
{
    constexpr std::uint8_t kMethodKind = 2U;
    constexpr std::uint32_t kStatic = 1U << 0U;
    constexpr std::uint32_t kAbstract = 1U << 5U;
    constexpr std::uint32_t kUserOperator = 1U << 16U;
    if (member.kind != kMethodKind || materializedAccessors.contains(&member))
        return true;
    std::string returnType;
    if (!sourceTypeForMetadataKey(metadata, member.declaredTypeKey, returnType, error))
        return false;
    (*generated) << "    public ";
    if (isTrait && (member.flags & kUserOperator) != 0U)
    {
        const std::string_view token = metadataOperatorToken(member.name);
        if (token.empty())
        {
            error = "metadata-backed trait cannot spell operator requirement '" + member.name + "'";
            return false;
        }
        (*generated) << "static " << ownershipPrefix(member.declaredOwnership)
                     << ((member.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << returnType
                     << " operator " << token << '(';
        if (!emitParameters(member))
            return false;
        (*generated) << ')';
        if (!emitThrowsContract(member.declaredErrorTypeKey))
            return false;
        (*generated) << ";\n";
        return true;
    }
    if (isTrait && (member.flags & kStatic) != 0U)
        (*generated) << "static ";
    if ((member.flags & interop::kMetadataMemberNoHeap) != 0U)
        (*generated) << "noheap ";
    if ((member.flags & interop::kMetadataMemberReferenceReturn) != 0U)
        (*generated) << "ref" << lifetimeAnnotation(member.returnLifetimeName) << ' ';
    (*generated) << ownershipPrefix(member.declaredOwnership)
                 << ((member.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << returnType << ' '
                 << member.name;
    emitLifetimeParameters(member.lifetimeParameters);
    (*generated) << '(';
    if (!emitParameters(member))
        return false;
    (*generated) << ')';
    if (!emitThrowsContract(member.declaredErrorTypeKey))
        return false;
    emitLifetimeConstraints(member.lifetimeOutlivesConstraints);
    (*generated) << (isTrait && (member.flags & kAbstract) == 0U ? " { }\n" : ";\n");
    return true;
}

bool PublicSurfaceBuilder::emitInterfaceType(const interop::LibraryMetadataType &type,
                                             const std::vector<const interop::LibraryMetadataMember *> &orderedMethods,
                                             std::string_view typeAccessibility, std::string_view typeName,
                                             bool topLevel, const std::string &namespaceName)
{
    const bool isTrait = metadata.formatMinor >= 28 && (type.flags & interop::kMetadataTypeTrait) != 0U;
    (*generated) << typeAccessibility << (isTrait ? " trait " : " interface ") << typeName;
    emitLifetimeParameters(type.lifetimeParameters);
    if (!type.interfaceTypeKeys.empty())
        (*generated) << " : ";
    for (std::size_t index = 0; index < type.interfaceTypeKeys.size(); ++index)
    {
        if (index != 0)
            (*generated) << ", ";
        std::string baseInterface;
        if (!sourceBaseType(type.interfaceTypeKeys[index], baseInterface))
            return false;
        (*generated) << baseInterface;
    }
    emitLifetimeConstraints(type.lifetimeOutlivesConstraints);
    (*generated) << "\n{\n";
    if (!emitAssociatedItems(type))
        return false;
    std::set<const interop::LibraryMetadataMember *> materializedAccessors;
    if (!emitAccessorContracts(orderedMethods, true, isTrait, materializedAccessors))
        return false;
    for (const interop::LibraryMetadataMember *member : orderedMethods)
        if (!emitInterfaceMethod(*member, isTrait, materializedAccessors))
            return false;
    (*generated) << "}\n";
    if (topLevel && !namespaceName.empty())
        (*generated) << "}\n";
    return true;
}

bool PublicSurfaceBuilder::emitVariantType(const interop::LibraryMetadataType &type, std::string_view typeAccessibility,
                                           std::string_view typeName, bool topLevel, const std::string &namespaceName)
{
    // Reparse the nominal declaration instead of exposing the generated tag/union
    // implementation. The ordered 1.27 catalogue is authoritative because its ordinal is
    // the observable CaseTag value; the ordinary public member table is signature-sorted
    // and cannot reconstruct that order safely.
    (*generated) << typeAccessibility << ' ';
    if (metadata.formatMinor >= 33 && (type.flags & interop::kMetadataTypeCopy) != 0U)
        (*generated) << "copy ";
    (*generated) << "variant " << typeName << "\n{\n";
    for (std::size_t caseIndex = 0; caseIndex < type.variantCases.size(); ++caseIndex)
    {
        const interop::LibraryMetadataVariantCase &variantCase = type.variantCases[caseIndex];
        (*generated) << "    " << variantCase.name;
        if (!variantCase.payloadTypeKey.empty())
        {
            std::string payloadType;
            if (!sourceTypeForMetadataKey(metadata, variantCase.payloadTypeKey, payloadType, error))
                return false;
            (*generated) << '(' << payloadType << ')';
        }
        if (caseIndex + 1U != type.variantCases.size())
            (*generated) << ',';
        (*generated) << '\n';
    }
    (*generated) << "}\n";
    if (topLevel && !namespaceName.empty())
        (*generated) << "}\n";
    return true;
}

void PublicSurfaceBuilder::emitNominalKindPrefix(const interop::LibraryMetadataType &type)
{
    if (type.kind == interop::LibraryMetadataTypeKind::Struct)
    {
        (*generated) << "pack(" << type.alignment << ") align(" << type.alignment << ") size(" << type.size << ") ";
        if ((type.flags & (1U << 6U)) != 0U)
            (*generated) << "readonly ";
        if ((type.flags & (1U << 4U)) != 0U)
            (*generated) << "borrowed ";
        if (metadata.formatMinor >= 33 && (type.flags & interop::kMetadataTypeCopy) != 0U)
            (*generated) << "copy ";
        if (metadata.formatMinor >= 26 && (type.flags & interop::kMetadataTypeRecord) != 0U)
            (*generated) << "record ";
        (*generated) << ((type.flags & (1U << 5U)) != 0U ? "union " : "struct ");
        return;
    }
    if (genericMethodOwners.contains(type.fullyQualifiedName))
        (*generated) << "partial ";
    if ((type.flags & (1U << 1U)) != 0U)
        (*generated) << "abstract ";
    if ((type.flags & (1U << 2U)) != 0U)
        (*generated) << "sealed ";
    if ((type.flags & (1U << 3U)) != 0U)
        (*generated) << "static ";
    (*generated) << "class ";
}

void PublicSurfaceBuilder::emitRecordDerives(const interop::LibraryMetadataType &type)
{
    if (type.kind != interop::LibraryMetadataTypeKind::Struct || metadata.formatMinor < 26 ||
        (type.flags & interop::kMetadataTypeRecord) == 0U)
        return;
    const std::array<std::pair<std::uint16_t, std::string_view>, 4> derives = {
        {{interop::kMetadataTypeDeriveEquality, "Equality"},
         {interop::kMetadataTypeDeriveHash, "Hash"},
         {interop::kMetadataTypeDerivePrint, "Print"},
         {interop::kMetadataTypeDeriveClone, "Clone"}}};
    bool firstDerive = true;
    for (const auto &[flag, name] : derives)
    {
        if ((type.flags & flag) == 0U)
            continue;
        (*generated) << (firstDerive ? " derive(" : ", ") << name;
        firstDerive = false;
    }
    if (!firstDerive)
        (*generated) << ')';
}

bool PublicSurfaceBuilder::emitNominalBases(const interop::LibraryMetadataType &type)
{
    const bool opaqueResultStruct = type.kind == interop::LibraryMetadataTypeKind::Struct &&
                                    metadata.formatMinor >= 30 &&
                                    (type.flags & interop::kMetadataTypeOpaqueResult) != 0U;
    if (!((type.kind == interop::LibraryMetadataTypeKind::Class && (type.flags & (1U << 3U)) == 0U) ||
          opaqueResultStruct) ||
        (type.baseTypeKey.empty() && type.interfaceTypeKeys.empty()))
        return true;
    (*generated) << " : ";
    bool wroteBase = false;
    if (!opaqueResultStruct && !type.baseTypeKey.empty())
    {
        std::string baseType;
        if (!sourceBaseType(type.baseTypeKey, baseType))
            return false;
        (*generated) << baseType;
        wroteBase = true;
    }
    for (const std::string &interfaceTypeKey : type.interfaceTypeKeys)
    {
        std::string interfaceType;
        if (!sourceBaseType(interfaceTypeKey, interfaceType))
            return false;
        if (wroteBase)
            (*generated) << ", ";
        (*generated) << interfaceType;
        wroteBase = true;
    }
    return true;
}

bool PublicSurfaceBuilder::emitNominalHeader(const interop::LibraryMetadataType &type,
                                             std::string_view typeAccessibility, const std::string &typeName)
{
    (*generated) << typeAccessibility << ' ';
    emitNominalKindPrefix(type);
    (*generated) << typeName;
    emitLifetimeParameters(type.lifetimeParameters);
    emitRecordDerives(type);
    if (!emitNominalBases(type))
        return false;
    emitLifetimeConstraints(type.lifetimeOutlivesConstraints);
    (*generated) << "\n{\n";
    return emitAssociatedItems(type);
}

bool PublicSurfaceBuilder::openAnonymousCarrier(AnonymousCarrierFrame &frame)
{
    if (!frame.carrier->typeKey.starts_with("type:"))
    {
        error = "metadata anonymous-member carrier '" + frame.carrier->name +
                "' does not refer to a nominal aggregate type";
        return false;
    }
    frame.nested = metadataTypeForKey(metadata, frame.carrier->typeKey);
    if (frame.nested == nullptr || frame.nested->kind != interop::LibraryMetadataTypeKind::Struct)
    {
        error = "metadata anonymous-member carrier '" + frame.carrier->name +
                "' refers to a missing or non-aggregate type '" + frame.carrier->typeKey + "'";
        return false;
    }
    const interop::LibraryMetadataMember *carrierMember =
        metadataFieldMember(metadata, *frame.owner, frame.carrier->name);
    (*generated) << frame.indentation;
    if (frame.owner->kind == interop::LibraryMetadataTypeKind::Struct)
        (*generated) << "at(" << frame.carrier->offset << ") ";
    (*generated) << sourceAccessibility(carrierMember == nullptr ? 0U : carrierMember->accessibility) << ' ';
    (*generated) << "pack(" << frame.nested->alignment << ") align(" << frame.nested->alignment << ") size("
                 << frame.nested->size << ") " << ((frame.nested->flags & (1U << 5U)) != 0U ? "union" : "struct")
                 << "\n"
                 << frame.indentation << "{\n";
    return true;
}

bool PublicSurfaceBuilder::emitAnonymousField(const AnonymousCarrierFrame &frame,
                                              const interop::LibraryMetadataField &child, const std::size_t childIndex)
{
    std::string childType;
    if (!sourceTypeForMetadataKey(metadata, child.typeKey, childType, error))
        return false;
    const interop::LibraryMetadataMember *childMember = metadataFieldMember(metadata, *frame.nested, child.name);
    const std::string childName = child.name.empty() ? "__nsc_layout_field_" + std::to_string(childIndex) : child.name;
    const bool childBitField = (child.flags & interop::kMetadataFieldBitField) != 0U;
    const bool childUnnamedBitField = (child.flags & interop::kMetadataFieldUnnamedBitField) != 0U;
    const std::string childIndentation = frame.indentation + "    ";
    (*generated) << childIndentation;
    if (!childBitField)
        (*generated) << "at(" << child.offset << ") align(" << child.alignment << ") ";
    (*generated) << sourceAccessibility(childMember == nullptr ? 0U : childMember->accessibility) << ' ';
    if ((child.flags & (1U << 1U)) != 0U || (frame.nested->flags & (1U << 6U)) != 0U)
        (*generated) << "readonly ";
    if ((child.flags & interop::kMetadataMemberVolatile) != 0U)
        (*generated) << "volatile ";
    (*generated) << ownershipPrefix(child.ownership, child.lifetimeName)
                 << ((child.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << childType;
    if (!childUnnamedBitField)
        (*generated) << ' ' << childName;
    if (childBitField)
        (*generated) << " : " << child.bitWidth;
    (*generated) << ";\n";
    return true;
}

bool PublicSurfaceBuilder::emitAnonymousCarrier(const interop::LibraryMetadataType &owner,
                                                const interop::LibraryMetadataField &carrier,
                                                std::string_view indentation)
{
    std::vector<AnonymousCarrierFrame> pending = {{&owner, &carrier, nullptr, std::string(indentation), 0U}};
    while (!pending.empty())
    {
        AnonymousCarrierFrame &frame = pending.back();
        if (frame.nested == nullptr && !openAnonymousCarrier(frame))
            return false;
        if (frame.nextChild == frame.nested->fields.size())
        {
            (*generated) << frame.indentation << "};\n";
            pending.pop_back();
            continue;
        }
        const std::size_t childIndex = frame.nextChild++;
        const interop::LibraryMetadataField &child = frame.nested->fields[childIndex];
        if ((child.flags & interop::kMetadataFieldAnonymousCarrier) != 0U)
        {
            pending.push_back({frame.nested, &child, nullptr, frame.indentation + "    ", 0U});
            continue;
        }
        if (!emitAnonymousField(frame, child, childIndex))
            return false;
    }
    return true;
}

bool PublicSurfaceBuilder::emitNominalField(const interop::LibraryMetadataType &owner,
                                            const interop::LibraryMetadataField &field, std::size_t fieldIndex,
                                            std::set<const interop::LibraryMetadataMember *> &materializedEvents)
{
    if ((field.flags & interop::kMetadataFieldAnonymousCarrier) != 0U)
        return emitAnonymousCarrier(owner, field, "    ");
    std::string fieldType;
    if (!sourceTypeForMetadataKey(metadata, field.typeKey, fieldType, error))
        return false;
    if (const interop::LibraryMetadataMember *event = metadataEventForBacking(metadata, owner, field); event != nullptr)
    {
        (*generated) << "    " << sourceAccessibility(event->accessibility) << ' ';
        if ((event->flags & (1U << 0U)) != 0U)
            (*generated) << "static ";
        (*generated) << "event " << ownershipPrefix(event->declaredOwnership)
                     << ((event->flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << fieldType << ' '
                     << event->name << ";\n";
        materializedEvents.insert(event);
        return true;
    }
    const interop::LibraryMetadataMember *member = metadataFieldMember(metadata, owner, field.name);
    constexpr std::uint32_t kCompilerGenerated = 1U << 10U;
    const bool propertyBacking = (field.flags & kCompilerGenerated) != 0U && field.name.starts_with("__nsc_backing_") &&
                                 field.name.ends_with("$stage38");
    const std::string fieldName = propertyBacking      ? "__nsc_layout_property_" + std::to_string(fieldIndex)
                                  : field.name.empty() ? "__nsc_layout_field_" + std::to_string(fieldIndex)
                                                       : field.name;
    const bool bitField = (field.flags & interop::kMetadataFieldBitField) != 0U;
    const bool unnamedBitField = (field.flags & interop::kMetadataFieldUnnamedBitField) != 0U;
    (*generated) << "    ";
    if (!bitField && owner.kind == interop::LibraryMetadataTypeKind::Struct)
        (*generated) << "at(" << field.offset << ") ";
    if (!bitField)
        (*generated) << "align(" << field.alignment << ") ";
    (*generated) << (member == nullptr ? std::string_view("private") : sourceAccessibility(member->accessibility))
                 << ' ';
    if ((field.flags & (1U << 1U)) != 0U || (owner.flags & (1U << 6U)) != 0U)
        (*generated) << "readonly ";
    if ((field.flags & interop::kMetadataMemberVolatile) != 0U)
        (*generated) << "volatile ";
    (*generated) << ownershipPrefix(field.ownership, field.lifetimeName)
                 << ((field.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << fieldType;
    if (!unnamedBitField)
        (*generated) << ' ' << fieldName;
    if (bitField)
        (*generated) << " : " << field.bitWidth;
    (*generated) << ";\n";
    return true;
}

bool PublicSurfaceBuilder::emitStaticFieldMember(const interop::LibraryMetadataMember &member)
{
    std::string fieldType;
    if (!sourceTypeForMetadataKey(metadata, member.declaredTypeKey, fieldType, error))
        return false;
    if ((member.flags & (1U << 2U)) != 0U)
    {
        std::string value;
        if (!metadataConstantSource(member, value, error))
            return false;
        (*generated) << "    " << sourceAccessibility(member.accessibility) << " const "
                     << ((member.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << fieldType << ' '
                     << member.name << " = " << value << ";\n";
        return true;
    }
    (*generated) << "    " << sourceAccessibility(member.accessibility) << " static ";
    if ((member.flags & (1U << 1U)) != 0U)
        (*generated) << "readonly ";
    if ((member.flags & interop::kMetadataMemberVolatile) != 0U)
        (*generated) << "volatile ";
    (*generated) << ((member.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << fieldType << ' '
                 << member.name << ";\n";
    return true;
}

bool PublicSurfaceBuilder::emitEventMember(const interop::LibraryMetadataMember &member,
                                           const std::set<const interop::LibraryMetadataMember *> &materializedEvents)
{
    if (materializedEvents.contains(&member))
        return true;
    std::string eventType;
    if (!sourceTypeForMetadataKey(metadata, member.declaredTypeKey, eventType, error))
        return false;
    (*generated) << "    " << sourceAccessibility(member.accessibility) << ' ';
    if ((member.flags & (1U << 0U)) != 0U)
        (*generated) << "static ";
    (*generated) << "event " << ownershipPrefix(member.declaredOwnership)
                 << ((member.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << eventType << ' '
                 << member.name << ";\n";
    return true;
}

bool PublicSurfaceBuilder::emitConstructorMember(const interop::LibraryMetadataMember &member,
                                                 const std::string &typeName)
{
    (*generated) << "    " << sourceAccessibility(member.accessibility) << ' ';
    if ((member.flags & interop::kMetadataMemberNoHeap) != 0U)
        (*generated) << "noheap ";
    (*generated) << typeName;
    emitLifetimeParameters(member.lifetimeParameters);
    (*generated) << '(';
    if (!emitParameters(member))
        return false;
    (*generated) << ')';
    emitLifetimeConstraints(member.lifetimeOutlivesConstraints);
    (*generated) << " { }\n";
    return true;
}

bool PublicSurfaceBuilder::emitImportedMethod(const interop::LibraryMetadataMember &member,
                                              const std::string &returnType)
{
    (*generated) << "    extern C";
    if (!member.importDllName.empty())
        (*generated) << " from " << escapedStringLiteral(member.importDllName);
    (*generated) << "\n    {\n        " << sourceAccessibility(member.accessibility) << ' '
                 << ownershipPrefix(member.declaredOwnership)
                 << ((member.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << returnType << ' '
                 << member.name << '(';
    if (!emitParameters(member))
        return false;
    (*generated) << ')';
    if (member.ffiUnwindPolicy != 0U)
        (*generated) << " unwind(" << (member.ffiUnwindPolicy == 2U ? "abort" : "forbidden") << ')';
    (*generated) << ";\n    }\n";
    return true;
}

bool PublicSurfaceBuilder::emitUserOperator(const interop::LibraryMetadataMember &member, const std::string &returnType)
{
    const std::string_view token = metadataOperatorToken(member.name);
    if (token.empty())
    {
        error = "metadata-backed source import cannot spell user operator '" + member.name + "'";
        return false;
    }
    (*generated) << "    " << sourceAccessibility(member.accessibility) << " static "
                 << ownershipPrefix(member.declaredOwnership)
                 << ((member.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << returnType
                 << " operator " << token << '(';
    if (!emitParameters(member))
        return false;
    (*generated) << ')';
    if (!emitThrowsContract(member.declaredErrorTypeKey))
        return false;
    (*generated) << " { }\n";
    return true;
}

bool PublicSurfaceBuilder::emitUserConversion(const interop::LibraryMetadataMember &member,
                                              const std::string &returnType)
{
    constexpr std::uint32_t kImplicitConversion = 1U << 18U;
    (*generated) << "    " << sourceAccessibility(member.accessibility) << " static "
                 << ((member.flags & kImplicitConversion) != 0U ? "implicit" : "explicit") << " operator "
                 << ownershipPrefix(member.declaredOwnership)
                 << ((member.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << returnType << '(';
    if (!emitParameters(member))
        return false;
    (*generated) << ')';
    if (!emitThrowsContract(member.declaredErrorTypeKey))
        return false;
    (*generated) << " { }\n";
    return true;
}

bool PublicSurfaceBuilder::emitOrdinaryMethod(const interop::LibraryMetadataMember &member,
                                              const std::string &returnType)
{
    constexpr std::uint32_t kStatic = 1U << 0U;
    constexpr std::uint32_t kReadonly = 1U << 1U;
    constexpr std::uint32_t kVirtual = 1U << 3U;
    constexpr std::uint32_t kOverride = 1U << 4U;
    constexpr std::uint32_t kAbstract = 1U << 5U;
    constexpr std::uint32_t kSealed = 1U << 6U;
    constexpr std::uint32_t kNewSlot = 1U << 19U;
    (*generated) << "    " << sourceAccessibility(member.accessibility) << ' ';
    if ((member.flags & kStatic) != 0U)
        (*generated) << "static ";
    if ((member.flags & kReadonly) != 0U)
        (*generated) << "readonly ";
    if ((member.flags & kSealed) != 0U)
        (*generated) << "sealed ";
    if ((member.flags & kNewSlot) != 0U)
        (*generated) << "new ";
    if ((member.flags & kOverride) != 0U)
        (*generated) << "override ";
    else if ((member.flags & kAbstract) != 0U)
        (*generated) << "abstract ";
    else if ((member.flags & kVirtual) != 0U)
        (*generated) << "virtual ";
    const std::uint32_t inlining =
        (member.flags & interop::kMetadataMemberInliningMask) >> interop::kMetadataMemberInliningShift;
    if (inlining == 3U)
        (*generated) << "noinline ";
    if ((member.flags & interop::kMetadataMemberNoHeap) != 0U)
        (*generated) << "noheap ";
    if ((member.flags & interop::kMetadataMemberReferenceReturn) != 0U)
        (*generated) << "ref" << lifetimeAnnotation(member.returnLifetimeName) << ' ';
    (*generated) << ownershipPrefix(member.declaredOwnership)
                 << ((member.flags & interop::kMetadataMemberDynamicTrait) != 0U ? "dyn " : "") << returnType << ' '
                 << member.name;
    emitLifetimeParameters(member.lifetimeParameters);
    (*generated) << '(';
    if (!emitParameters(member))
        return false;
    (*generated) << ')';
    if (!emitThrowsContract(member.declaredErrorTypeKey))
        return false;
    emitLifetimeConstraints(member.lifetimeOutlivesConstraints);
    (*generated) << (((member.flags & kAbstract) != 0U) ? ";\n" : " { }\n");
    return true;
}

bool PublicSurfaceBuilder::emitMethodMember(const interop::LibraryMetadataMember &member)
{
    if (!member.inlineSourceText.empty())
    {
        (*generated) << "    " << member.inlineSourceText << '\n';
        return true;
    }
    std::string returnType;
    if (!sourceTypeForMetadataKey(metadata, member.declaredTypeKey, returnType, error))
        return false;
    constexpr std::uint32_t kUserOperator = 1U << 16U;
    constexpr std::uint32_t kUserConversion = 1U << 17U;
    constexpr std::uint32_t kImportedC = 1U << 20U;
    if ((member.flags & kImportedC) != 0U)
        return emitImportedMethod(member, returnType);
    if ((member.flags & kUserOperator) != 0U)
        return emitUserOperator(member, returnType);
    if ((member.flags & kUserConversion) != 0U)
        return emitUserConversion(member, returnType);
    return emitOrdinaryMethod(member, returnType);
}

bool PublicSurfaceBuilder::emitNominalMember(
    const interop::LibraryMetadataMember &member, const std::string &nominalTypeName,
    const std::set<const interop::LibraryMetadataMember *> &materializedAccessors,
    const std::set<const interop::LibraryMetadataMember *> &materializedEvents)
{
    constexpr std::uint8_t kFieldKind = 0U;
    constexpr std::uint8_t kEventKind = 1U;
    constexpr std::uint8_t kMethodKind = 2U;
    constexpr std::uint8_t kConstructorKind = 3U;
    constexpr std::uint8_t kDestructorKind = 4U;
    if (materializedAccessors.contains(&member))
        return true;
    if (member.kind == kFieldKind)
        return emitStaticFieldMember(member);
    if (member.kind == kEventKind)
        return emitEventMember(member, materializedEvents);
    if (member.kind == kDestructorKind)
    {
        (*generated) << "    ~" << nominalTypeName << "() { }\n";
        return true;
    }
    if (member.kind == kConstructorKind)
        return emitConstructorMember(member, nominalTypeName);
    if (member.kind == kMethodKind)
        return emitMethodMember(member);
    return true;
}

bool PublicSurfaceBuilder::emitNominalType(const interop::LibraryMetadataType &type,
                                           const std::vector<const interop::LibraryMetadataMember *> &orderedMethods,
                                           std::string_view typeAccessibility, const std::string &typeName,
                                           bool topLevel, const std::string &namespaceName,
                                           const std::map<std::string, std::string> &renderedTypes)
{
    if (type.kind != interop::LibraryMetadataTypeKind::Struct && type.kind != interop::LibraryMetadataTypeKind::Class)
    {
        error = "metadata-backed source import cannot materialize declaration "
                "kind for '" +
                type.fullyQualifiedName + "'";
        return false;
    }
    const bool isVariant = type.kind == interop::LibraryMetadataTypeKind::Struct && metadata.formatMinor >= 27 &&
                           (type.flags & interop::kMetadataTypeVariant) != 0U;
    if (isVariant)
        return emitVariantType(type, typeAccessibility, typeName, topLevel, namespaceName);
    if (!emitNominalHeader(type, typeAccessibility, typeName))
        return false;

    std::set<const interop::LibraryMetadataMember *> materializedEvents;
    std::set<const interop::LibraryMetadataMember *> materializedAccessors;
    for (std::size_t fieldIndex = 0; fieldIndex < type.fields.size(); ++fieldIndex)
    {
        if (!emitNominalField(type, type.fields[fieldIndex], fieldIndex, materializedEvents))
            return false;
    }

    if (!emitAccessorContracts(orderedMethods, false, false, materializedAccessors))
        return false;

    for (const interop::LibraryMetadataMember *member : orderedMethods)
        if (!emitNominalMember(*member, typeName, materializedAccessors, materializedEvents))
            return false;
    const auto children = nestedChildren.find(type.fullyQualifiedName);
    if (children != nestedChildren.end())
    {
        for (const std::string &childIdentity : children->second)
        {
            const auto child = renderedTypes.find(childIdentity);
            if (child != renderedTypes.end())
                (*generated) << child->second;
        }
    }
    (*generated) << "}\n";
    if (topLevel && !namespaceName.empty())
        (*generated) << "}\n";
    return true;
}

std::vector<const interop::LibraryMetadataMember *> PublicSurfaceBuilder::orderedPublishedMethods(
    const interop::LibraryMetadataType &type, const std::vector<const interop::LibraryMetadataMember *> &methods)
{
    const std::string declaringTypeKey = "type:" + type.identityKey;
    const auto dispatch =
        std::ranges::find_if(metadata.dispatchTables, [&](const interop::LibraryMetadataDispatch &candidate)
                             { return candidate.typeKey == declaringTypeKey; });
    const interop::LibraryMetadataDispatch *dispatchTable =
        dispatch == metadata.dispatchTables.end() ? nullptr : &*dispatch;

    // The rank of each method is computed once instead of twice per comparison. A stable sort
    // of (rank, method) pairs that compares only the ranks keeps methods of equal rank in their
    // original order, so the result is the one of stable-sorting the methods by rank.
    std::vector<std::pair<std::size_t, const interop::LibraryMetadataMember *>> rankedMethods;
    rankedMethods.reserve(methods.size());
    for (const interop::LibraryMetadataMember *method : methods)
        rankedMethods.emplace_back(dispatchRank(type, declaringTypeKey, dispatchTable, method), method);
    std::stable_sort(rankedMethods.begin(), rankedMethods.end(),
                     [](const auto &left, const auto &right) { return left.first < right.first; });
    std::vector<const interop::LibraryMetadataMember *> orderedMethods;
    orderedMethods.reserve(rankedMethods.size());
    for (const auto &rankedMethod : rankedMethods)
        orderedMethods.push_back(rankedMethod.second);
    return orderedMethods;
}

bool PublicSurfaceBuilder::emitPublishedType(const std::string &identity, const bool topLevel,
                                             const std::map<std::string, std::string> &renderedTypes)
{
    const auto groupFound = published.find(identity);
    if (groupFound == published.end() || groupFound->second.type == nullptr)
        return true;
    const PublishedType &group = groupFound->second;
    const interop::LibraryMetadataType &type = *group.type;
    if (type.fullyQualifiedName.find('`') != std::string::npos)
    {
        error = "metadata-backed source import does not yet support generic declaring type '" +
                type.fullyQualifiedName + "'";
        return false;
    }
    const std::size_t topTypeEnd = type.fullyQualifiedName.find('+');
    const std::string_view topTypeName(type.fullyQualifiedName.data(),
                                       topTypeEnd == std::string::npos ? type.fullyQualifiedName.size() : topTypeEnd);
    const std::size_t separator = topTypeName.find_last_of('.');
    const std::string namespaceName =
        separator == std::string::npos ? std::string() : std::string(topTypeName.substr(0, separator));
    const std::size_t nestedSeparator = type.fullyQualifiedName.find_last_of('+');
    const std::string typeName = nestedSeparator == std::string::npos
                                     ? (separator == std::string::npos ? type.fullyQualifiedName
                                                                       : type.fullyQualifiedName.substr(separator + 1U))
                                     : type.fullyQualifiedName.substr(nestedSeparator + 1U);
    const auto orderedMethods = orderedPublishedMethods(type, group.methods);
    if (topLevel && !namespaceName.empty())
        (*generated) << "namespace " << namespaceName << "\n{\n";
    const std::string_view typeAccessibility =
        type.accessibility == kPublicAccessibility ? std::string_view("public") : std::string_view("internal");
    if (type.kind == interop::LibraryMetadataTypeKind::Delegate)
        return emitDelegateType(type, typeAccessibility, typeName, topLevel, namespaceName);
    if (type.kind == interop::LibraryMetadataTypeKind::Enum)
        return emitEnumType(type, typeAccessibility, typeName, topLevel, namespaceName);

    if (type.kind == interop::LibraryMetadataTypeKind::Interface)
        return emitInterfaceType(type, orderedMethods, typeAccessibility, typeName, topLevel, namespaceName);
    return emitNominalType(type, orderedMethods, typeAccessibility, typeName, topLevel, namespaceName, renderedTypes);
}

std::size_t PublicSurfaceBuilder::dispatchRank(const interop::LibraryMetadataType &type,
                                               const std::string &declaringTypeKey,
                                               const interop::LibraryMetadataDispatch *dispatch,
                                               const interop::LibraryMetadataMember *member)
{
    if ((type.flags & interop::kMetadataTypeTrait) != 0U)
    {
        const auto slot = std::ranges::find(type.traitMethodSignatures, member->signatureKey);
        if (slot != type.traitMethodSignatures.end())
            return static_cast<std::size_t>(slot - type.traitMethodSignatures.begin());
    }
    if (dispatch != nullptr)
    {
        const auto slot = std::ranges::find(dispatch->slotSignatures, member->signatureKey);
        if (slot != dispatch->slotSignatures.end())
            return static_cast<std::size_t>(slot - dispatch->slotSignatures.begin());
    }
    if (type.kind == interop::LibraryMetadataTypeKind::Interface)
    {
        for (const interop::LibraryMetadataDispatch &owner : metadata.dispatchTables)
        {
            for (const interop::LibraryMetadataDispatchInterface &interface : owner.interfaces)
            {
                if (interface.interfaceTypeKey != declaringTypeKey)
                    continue;
                const auto slot = std::ranges::find(interface.interfaceSignatures, member->signatureKey);
                if (slot != interface.interfaceSignatures.end())
                    return static_cast<std::size_t>(slot - interface.interfaceSignatures.begin());
            }
        }
    }
    return std::numeric_limits<std::size_t>::max();
}

void PublicSurfaceBuilder::collectGenericMethodOwners()
{
    for (const interop::LibraryMetadataGenericTemplate &generic : metadata.genericTemplates)
    {
        if (generic.kind != kGenericMethodKind)
            continue;
        std::string ownerPrefix;
        std::size_t begin = 0;
        while (begin <= generic.ownerName.size())
        {
            const std::size_t separator = generic.ownerName.find('+', begin);
            const std::size_t stop = separator == std::string::npos ? generic.ownerName.size() : separator;
            if (stop > begin)
            {
                if (!ownerPrefix.empty())
                    ownerPrefix.push_back('+');
                ownerPrefix.append(generic.ownerName.substr(begin, stop - begin));
                genericMethodOwners.insert(generic.namespaceName.empty() ? ownerPrefix
                                                                         : generic.namespaceName + "." + ownerPrefix);
            }
            if (separator == std::string::npos)
                break;
            begin = separator + 1U;
        }
    }
}

void PublicSurfaceBuilder::collectPublishedTypes()
{
    for (const interop::LibraryMetadataType &type : metadata.types)
    {
        // A public nominal type is API even when no exported method happens to mention it. This
        // matters when a consumer supplies a standalone struct or enum from one metadata module
        // as the type argument of a public template from another module: omitting the declaration
        // makes the generic pass fall back to the consumer as its origin and the cloned template
        // later looks for the argument in the wrong project. Generic template declarations live
        // in metadata.genericTemplates; every concrete type in this table can be reconstructed by
        // the kind-specific source emitter below.
        if (type.accessibility == kPublicAccessibility && isPubliclyReachableMetadataType(metadata, type))
        {
            published[type.identityKey].type = &type;
            collectMaterializedContractTypes(metadata, type.baseTypeKey, contractTypeIdentities);
            for (const std::string &interfaceTypeKey : type.interfaceTypeKeys)
                collectMaterializedContractTypes(metadata, interfaceTypeKey, contractTypeIdentities);
            for (const interop::LibraryMetadataField &field : type.fields)
                collectMaterializedContractTypes(metadata, field.typeKey, contractTypeIdentities);
            for (const interop::LibraryMetadataAssociatedItem &item : type.associatedItems)
                collectMaterializedContractTypes(metadata, item.typeKey, contractTypeIdentities);
        }
    }
}

bool PublicSurfaceBuilder::collectPublishedMembers()
{
    for (const interop::LibraryMetadataMember &member : metadata.members)
    {
        if (!canMaterializePrecompiledDeclaration(metadata, member))
        {
            continue;
        }
        const auto type = std::find_if(metadata.types.begin(), metadata.types.end(), [&](const auto &candidate)
                                       { return "type:" + candidate.identityKey == member.declaringTypeKey; });
        if (type == metadata.types.end())
        {
            error = "metadata member '" + member.name + "' refers to a declaring type that is not present";
            return false;
        }
        const bool contractDependency = contractTypeIdentities.contains(type->identityKey);
        if (!isPubliclyReachableMetadataType(metadata, *type) && !contractDependency)
            continue;
        if (type->accessibility != kPublicAccessibility)
        {
            constexpr std::uint32_t kVirtual = 1U << 3U;
            constexpr std::uint32_t kOverride = 1U << 4U;
            constexpr std::uint32_t kAbstract = 1U << 5U;
            constexpr std::uint8_t kDestructorKind = 4U;
            if (!contractDependency && (member.flags & (kVirtual | kOverride | kAbstract)) == 0U &&
                member.kind != kDestructorKind)
                continue;
        }
        PublishedType &group = published[type->identityKey];
        group.type = &*type;
        group.methods.push_back(&member);
        collectMaterializedContractTypes(metadata, member.declaredTypeKey, contractTypeIdentities);
        collectMaterializedContractTypes(metadata, member.declaredErrorTypeKey, contractTypeIdentities);
        for (const interop::LibraryMetadataParameter &parameter : member.parameters)
            collectMaterializedContractTypes(metadata, parameter.typeKey, contractTypeIdentities);
    }
    return true;
}

void PublicSurfaceBuilder::collectNamespaceContractTypes()
{
    for (const interop::LibraryMetadataNamespaceFunction &function : metadata.namespaceFunctions)
    {
        constexpr std::uint8_t kPublicNamespaceAccessibility = 1U;
        if (function.accessibility != kPublicNamespaceAccessibility)
            continue;
        collectMaterializedContractTypes(metadata, function.declaredTypeKey, contractTypeIdentities);
        collectMaterializedContractTypes(metadata, function.declaredErrorTypeKey, contractTypeIdentities);
        for (const interop::LibraryMetadataParameter &parameter : function.parameters)
            collectMaterializedContractTypes(metadata, parameter.typeKey, contractTypeIdentities);
    }
}

bool PublicSurfaceBuilder::publishContractTypes()
{
    for (const std::string &identity : contractTypeIdentities)
    {
        const auto type = std::ranges::find_if(metadata.types, [&](const auto &candidate)
                                               { return candidate.identityKey == identity; });
        if (type == metadata.types.end())
        {
            error = "metadata contract refers to missing type '" + identity + "'";
            return false;
        }
        published[identity].type = &*type;
    }
    return true;
}

bool PublicSurfaceBuilder::emitAliases()
{
    // Every named type spelling below already comes from its canonical NSMETA key and therefore
    // carries the complete namespace. Do not synthesize namespace imports here. In NativeSharp a
    // `using` name deliberately has precedence over a type in the producer's global namespace;
    // importing `NativeSharp.Glfw` would consequently rebind the producer's own global `Keys` to
    // `NativeSharp.Glfw.Keys` and change the callable signature before physical-name binding.
    // Base lists keep their private aliases below because that grammar requires a simple name.
    // Base lists in NativeSharp intentionally use a simple identifier. Give every
    // metadata base/interface an unambiguous private alias so two dependencies may
    // both publish (for example) `Base` without losing their module/namespace
    // identity while reconstructing the consumer declaration.

    for (const interop::LibraryMetadataType &type : metadata.types)
    {
        if (!collectBaseAlias(type.baseTypeKey))
            return false;
        for (const std::string &interfaceTypeKey : type.interfaceTypeKeys)
            if (!collectBaseAlias(interfaceTypeKey))
                return false;
    }
    for (const interop::LibraryMetadataNamespaceFunction &function : metadata.namespaceFunctions)
    {
        if ((function.flags & interop::kMetadataNamespaceFunctionExtension) == 0U)
            continue;
        if (function.parameters.empty())
        {
            error = "metadata extension method '" + function.namespaceName + "." + function.name + "' has no receiver";
            return false;
        }
        const interop::LibraryMetadataParameter &receiver = function.parameters.front();
        if (receiver.passKind == interop::LibraryMetadataParameterPassKind::Out)
        {
            error =
                "metadata extension method '" + function.namespaceName + "." + function.name + "' has an out receiver";
            return false;
        }
        std::string receiverTypeKey = receiver.typeKey;
        if (receiver.passKind != interop::LibraryMetadataParameterPassKind::Value)
        {
            if (receiverTypeKey.empty() || !receiverTypeKey.ends_with('*'))
            {
                error = "metadata extension method '" + function.namespaceName + "." + function.name +
                        "' has no pointer receiver carrier";
                return false;
            }
            receiverTypeKey.pop_back();
        }
        if (extensionReceiverAliases.contains(receiverTypeKey))
            continue;
        std::string sourceType;
        if (!sourceTypeForMetadataKey(metadata, receiverTypeKey, sourceType, error))
            return false;
        const std::string alias = "__NscMetadataExtensionReceiver" + std::to_string(extensionReceiverAliases.size());
        extensionReceiverAliases.emplace(receiverTypeKey, alias);
        (*generated) << "using " << alias << " = " << sourceType << ";\n";
    }
    if (!baseAliases.empty() || !extensionReceiverAliases.empty())
        (*generated) << '\n';
    return true;
}

void PublicSurfaceBuilder::collectNestedChildren()
{
    for (const auto &[identity, group] : published)
    {
        const std::size_t separator = group.type->fullyQualifiedName.find_last_of('+');
        if (separator != std::string::npos)
            nestedChildren[group.type->fullyQualifiedName.substr(0, separator)].push_back(identity);
    }
}

bool PublicSurfaceBuilder::renderPublishedTypes()
{
    std::map<std::string, std::string> renderedTypes;
    for (const auto &[identity, group] : published)
    {
        if (group.type == nullptr || group.type->fullyQualifiedName.find('+') != std::string::npos)
            continue;
        std::vector<PublishedTypeFrame> pending = {{identity, false}};
        while (!pending.empty())
        {
            PublishedTypeFrame frame = std::move(pending.back());
            pending.pop_back();
            const auto current = published.find(frame.identity);
            if (current == published.end() || current->second.type == nullptr)
                continue;
            if (!frame.expanded)
            {
                pending.push_back({frame.identity, true});
                const auto children = nestedChildren.find(current->second.type->fullyQualifiedName);
                if (children != nestedChildren.end())
                {
                    for (auto child = children->second.rbegin(); child != children->second.rend(); ++child)
                        pending.push_back({*child, false});
                }
                continue;
            }
            std::ostringstream rendered;
            generated = &rendered;
            const bool topLevel = current->second.type->fullyQualifiedName.find('+') == std::string::npos;
            if (!emitPublishedType(frame.identity, topLevel, renderedTypes))
            {
                generated = &generatedStorage;
                return false;
            }
            renderedTypes[frame.identity] = rendered.str();
        }
        generated = &generatedStorage;
        (*generated) << renderedTypes[identity];
    }
    return true;
}

bool PublicSurfaceBuilder::emitStaticNamespaceExtension(const interop::LibraryMetadataNamespaceFunction &function)
{
    // NS304-T08-F07: a static extension has no implicit receiver. The extended type is the
    // first parameter's type (a static operator's first operand) since the receiver type
    // is not published as a separate field.
    if (function.parameters.empty())
    {
        error = "metadata static extension '" + function.namespaceName + "." + function.name + "' has no receiver type";
        return false;
    }
    const std::string receiverTypeKey = function.parameters.front().typeKey;
    const auto receiverAlias = extensionReceiverAliases.find(receiverTypeKey);
    if (receiverAlias == extensionReceiverAliases.end())
    {
        error = "metadata static extension '" + function.namespaceName + "." + function.name +
                "' has no materialized receiver alias";
        return false;
    }
    (*generated) << "extension " << receiverAlias->second << std::endl << '{' << std::endl;
    return true;
}

bool PublicSurfaceBuilder::emitInstanceNamespaceExtension(const interop::LibraryMetadataNamespaceFunction &function)
{
    if (function.parameters.empty())
    {
        error = "metadata extension method '" + function.namespaceName + "." + function.name + "' has no receiver";
        return false;
    }
    const interop::LibraryMetadataParameter &receiver = function.parameters.front();
    if (receiver.passKind == interop::LibraryMetadataParameterPassKind::Out)
    {
        error = "metadata extension method '" + function.namespaceName + "." + function.name + "' has an out receiver";
        return false;
    }
    std::string receiverTypeKey = receiver.typeKey;
    if (receiver.passKind != interop::LibraryMetadataParameterPassKind::Value)
    {
        if (receiverTypeKey.empty() || !receiverTypeKey.ends_with('*'))
        {
            error = "metadata extension method '" + function.namespaceName + "." + function.name +
                    "' has no pointer receiver carrier";
            return false;
        }
        receiverTypeKey.pop_back();
    }
    const auto receiverAlias = extensionReceiverAliases.find(receiverTypeKey);
    if (receiverAlias == extensionReceiverAliases.end())
    {
        error = "metadata extension method '" + function.namespaceName + "." + function.name +
                "' has no materialized receiver alias";
        return false;
    }
    (*generated) << "extension " << parameterPassPrefix(receiver.passKind, receiver.lifetimeName)
                 << ownershipPrefix(receiver.ownership) << (receiver.isDynamicTrait ? "dyn " : "")
                 << receiverAlias->second << std::endl
                 << '{' << std::endl;
    return true;
}

bool PublicSurfaceBuilder::emitNamespaceFunctionDeclaration(const interop::LibraryMetadataNamespaceFunction &function,
                                                            const bool extension, const bool staticExtension,
                                                            const std::string &returnType)
{
    if ((function.flags & 8U) != 0U)
        (*generated) << "extern C" << std::endl << '{' << std::endl;
    (*generated) << "public ";
    if (staticExtension)
        (*generated) << "static ";
    if ((function.flags & 1U) != 0U)
        (*generated) << "ref" << lifetimeAnnotation(function.returnLifetimeName) << ' ';
    (*generated) << ownershipPrefix(function.declaredOwnership) << ((function.flags & 16U) != 0U ? "dyn " : "")
                 << returnType << ' ' << function.name;
    emitLifetimeParameters(function.lifetimeParameters);
    (*generated) << '(';
    const std::size_t firstExplicitParameter = (extension && !staticExtension) ? 1U : 0U;
    if (!emitParameterSlice(function.name, function.parameters, firstExplicitParameter,
                            function.parameters.size() - firstExplicitParameter))
        return false;
    if ((function.flags & 2U) != 0U)
    {
        if (function.parameters.size() > firstExplicitParameter)
            (*generated) << ", ";
        (*generated) << "...";
    }
    (*generated) << ')';
    if (!emitThrowsContract(function.declaredErrorTypeKey))
        return false;
    emitLifetimeConstraints(function.lifetimeOutlivesConstraints);
    (*generated) << ';' << std::endl;
    return true;
}

bool PublicSurfaceBuilder::emitNamespaceFunction(const interop::LibraryMetadataNamespaceFunction &function)
{
    if (function.accessibility != 1U)
        return true;
    const bool extension = (function.flags & interop::kMetadataNamespaceFunctionExtension) != 0U;
    const bool staticExtension = (function.flags & interop::kMetadataNamespaceFunctionStaticExtension) != 0U;
    std::string returnType;
    if (!sourceTypeForMetadataKey(metadata, function.declaredTypeKey, returnType, error))
    {
        return false;
    }
    if (!function.namespaceName.empty())
        (*generated) << "namespace " << function.namespaceName << std::endl << '{' << std::endl;
    if (staticExtension && !emitStaticNamespaceExtension(function))
        return false;
    if (extension && !staticExtension && !emitInstanceNamespaceExtension(function))
        return false;
    if (!emitNamespaceFunctionDeclaration(function, extension, staticExtension, returnType))
        return false;
    if ((function.flags & 8U) != 0U)
        (*generated) << '}' << std::endl;
    if (extension)
        (*generated) << '}' << std::endl;
    if (!function.namespaceName.empty())
        (*generated) << '}' << std::endl;
    return true;
}

bool PublicSurfaceBuilder::emitNamespaceFunctions()
{
    for (const interop::LibraryMetadataNamespaceFunction &function : metadata.namespaceFunctions)
    {
        if (!emitNamespaceFunction(function))
            return false;
    }
    return true;
}

bool PublicSurfaceBuilder::emitNamespaceState(const interop::LibraryMetadataNamespaceState &state)
{

    if (state.accessibility != 1U)
        return true;
    std::string typeSource;
    if (!sourceTypeForMetadataKey(metadata, state.declaredTypeKey, typeSource, error))
    {
        return false;
    }
    if (!state.namespaceName.empty())
        (*generated) << "namespace " << state.namespaceName << std::endl << '{' << std::endl;
    (*generated) << "public ";
    const bool isConst = (state.flags & 1U) != 0U;
    if (isConst)
        (*generated) << "const ";
    if ((state.flags & 2U) != 0U)
        (*generated) << "dyn ";
    (*generated) << typeSource << ' ' << state.name;
    if (isConst)
    {
        std::string initializer;
        using Kind = interop::LibraryMetadataNamespaceStateInitializerKind;
        if (state.initializerKind == Kind::Zero)
            initializer = "default(" + typeSource + ")";
        else if (state.initializerKind == Kind::Null)
            initializer = "null";
        else
        {
            interop::LibraryMetadataMember constant;
            constant.name = state.name;
            constant.declaredTypeKey = state.declaredTypeKey;
            constant.constantBits = state.initializerBits;
            constant.constantText = state.initializerText;
            if (state.initializerKind == Kind::Integer)
                constant.constantKind = 1U;
            else if (state.initializerKind == Kind::Boolean)
                constant.constantKind = 3U;
            else if (state.initializerKind == Kind::Floating)
                constant.constantKind = state.declaredTypeKey == "builtin:float" ? 4U : 5U;
            else if (state.initializerKind == Kind::String)
                constant.constantKind = 6U;
            if (!metadataConstantSource(constant, initializer, error))
                return false;
        }
        (*generated) << " = " << initializer;
    }
    (*generated) << ';' << std::endl;
    if (!state.namespaceName.empty())
        (*generated) << '}' << std::endl;
    return true;
}

bool PublicSurfaceBuilder::emitNamespaceStates()
{
    for (const interop::LibraryMetadataNamespaceState &state : metadata.namespaceStates)
    {
        if (!emitNamespaceState(state))
            return false;
    }
    return true;
}

bool PublicSurfaceBuilder::emitTraitImpl(const interop::LibraryMetadataTraitImpl &traitImpl)
{

    std::string traitSource;
    std::string targetSource;
    if (!sourceTypeForMetadataKey(metadata, traitImpl.traitTypeKey, traitSource, error) ||
        !sourceTypeForMetadataKey(metadata, traitImpl.targetTypeKey, targetSource, error))
    {
        return false;
    }
    (*generated) << "impl " << traitSource << " for " << targetSource << "\n{\n";
    for (const interop::LibraryMetadataMember &member : traitImpl.methods)
    {
        (*generated) << "    ";
        if (member.accessibility == kPublicAccessibility)
            (*generated) << "public ";
        std::string returnType;
        if (!sourceTypeForMetadataKey(metadata, member.declaredTypeKey, returnType, error))
            return false;
        (*generated) << returnType << ' ' << member.name << '(';
        if (!emitParameters(member))
            return false;
        (*generated) << ')';
        if (!emitThrowsContract(member.declaredErrorTypeKey))
            return false;
        (*generated) << ";\n";
    }
    (*generated) << "}\n";
    return true;
}

bool PublicSurfaceBuilder::emitTraitImpls()
{
    // NS304-T08-F05: external trait conformances become impl blocks so the consumer's orphan rule
    // + member registration rebuild the same static dispatch surface over the external target.
    for (const interop::LibraryMetadataTraitImpl &traitImpl : metadata.traitImpls)
    {
        if (!emitTraitImpl(traitImpl))
            return false;
    }
    return true;
}

bool PublicSurfaceBuilder::run()
{
    collectGenericMethodOwners();
    collectPublishedTypes();
    if (!collectPublishedMembers())
        return false;
    collectNamespaceContractTypes();
    if (!publishContractTypes() || !emitAliases())
        return false;
    collectNestedChildren();
    if (!renderPublishedTypes() || !emitNamespaceFunctions() || !emitNamespaceStates() || !emitTraitImpls())
        return false;
    source = generatedStorage.str();
    return true;
}
} // namespace nativesharp::driver
