#include "PrecompiledSurfaceReachability.h"

#include "../Driver/Internal/DriverInternal.h"

#include <algorithm>
#include <optional>

namespace nativesharp::compilation
{
namespace
{
constexpr std::string_view kTypeKeyPrefix = "type:";

bool isNameByte(char value) noexcept
{
    const unsigned char byte = static_cast<unsigned char>(value);
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') ||
        byte == '_' || byte >= 0x80U;
}

bool isQualifiedNameByte(char value) noexcept
{
    return isNameByte(value) || value == '.' || value == '+' || value == '`';
}

// The last segment of a qualified name without its generic arity suffix: `NS.Outer+List`1` gives
// `List`.
std::string_view simpleName(std::string_view qualifiedName) noexcept
{
    const std::size_t separator = qualifiedName.find_last_of(".+:");
    std::string_view name = separator == std::string_view::npos
        ? qualifiedName
        : qualifiedName.substr(separator + 1U);
    const std::size_t arity = name.find('`');
    if (arity != std::string_view::npos) name = name.substr(0, arity);
    return name;
}

bool allSet(const std::vector<bool>& flags)
{
    return std::find(flags.begin(), flags.end(), false) == flags.end();
}

template <typename Record>
void copyIncluded(const std::vector<Record>& records, const std::vector<bool>& included, std::vector<Record>& kept)
{
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (included[index]) kept.push_back(records[index]);
    }
}

// The identity (`Module::Qualified.Name`) of the first `type:` key inside `text`, or an empty view.
std::string_view firstTypeIdentity(std::string_view text) noexcept
{
    const std::size_t prefix = text.find(kTypeKeyPrefix);
    if (prefix == std::string_view::npos) return {};
    std::size_t cursor = prefix + kTypeKeyPrefix.size();
    const std::size_t begin = cursor;
    while (cursor < text.size() && isNameByte(text[cursor])) ++cursor;
    if (cursor + 1U >= text.size() || text[cursor] != ':' || text[cursor + 1U] != ':') return {};
    cursor += 2U;
    while (cursor < text.size() && isQualifiedNameByte(text[cursor])) ++cursor;
    return text.substr(begin, cursor - begin);
}
} // namespace

PrecompiledSurfaceReachability::PrecompiledSurfaceReachability(
    std::span<const std::shared_ptr<const interop::LibraryMetadata>> modules,
    const std::vector<bool>& fullModules)
    : modules_(modules.begin(), modules.end()), fullModules_(fullModules)
{
    includedTypes_.resize(modules_.size());
    includedMembers_.resize(modules_.size());
    includedTraitImpls_.resize(modules_.size());
    includedFunctions_.resize(modules_.size());
    includedStates_.resize(modules_.size());
    includedTemplates_.resize(modules_.size());
    for (std::size_t module = 0; module < modules_.size(); ++module)
    {
        const interop::LibraryMetadata& metadata = *modules_[module];
        includedTypes_[module].assign(metadata.types.size(), false);
        includedMembers_[module].assign(metadata.members.size(), false);
        includedTraitImpls_[module].assign(metadata.traitImpls.size(), false);
        includedFunctions_[module].assign(metadata.namespaceFunctions.size(), false);
        includedStates_[module].assign(metadata.namespaceStates.size(), false);
        includedTemplates_[module].assign(metadata.genericTemplates.size(), false);
        if (!fullModules_[module]) indexModule(module);
    }
    for (std::size_t module = 0; module < modules_.size(); ++module)
    {
        if (!fullModules_[module]) indexTraitImpls(module);
    }
    // Specialized types are indexed after every module, because a module may publish the concrete
    // declaration of a specialization whose template another module owns.
    for (std::size_t module = 0; module < modules_.size(); ++module)
    {
        if (fullModules_[module]) continue;
        const interop::LibraryMetadata& metadata = *modules_[module];
        for (std::size_t index = 0; index < metadata.genericSpecializations.size(); ++index)
        {
            const interop::LibraryMetadataGenericSpecialization& specialization =
                metadata.genericSpecializations[index];
            const std::optional<std::string> typeKey = driver::metadataSpecializedTypeKey(specialization);
            if (!typeKey.has_value()) continue;
            const std::string& identity = storage_.emplace_back(typeKey->substr(kTypeKeyPrefix.size()));
            specializationsByType_[identity].emplace_back(module, index);
            specializedTypesByTemplateName_[simpleName(specialization.templateKey)].push_back(identity);
        }
    }
}

void PrecompiledSurfaceReachability::indexModule(std::size_t module)
{
    const interop::LibraryMetadata& metadata = *modules_[module];
    for (std::size_t index = 0; index < metadata.types.size(); ++index)
    {
        const interop::LibraryMetadataType& type = metadata.types[index];
        typesByIdentity_[type.identityKey].emplace_back(module, index);
        typesByName_[simpleName(type.fullyQualifiedName)].emplace_back(module, index);
    }
    for (std::size_t index = 0; index < metadata.members.size(); ++index)
        membersByDeclaringType_[metadata.members[index].declaringTypeKey].emplace_back(module, index);
    for (std::size_t index = 0; index < metadata.namespaceFunctions.size(); ++index)
        functionsByName_[metadata.namespaceFunctions[index].name].emplace_back(module, index);
    for (std::size_t index = 0; index < metadata.namespaceStates.size(); ++index)
        statesByName_[metadata.namespaceStates[index].name].emplace_back(module, index);
    for (std::size_t index = 0; index < metadata.genericTemplates.size(); ++index)
    {
        const interop::LibraryMetadataGenericTemplate& generic = metadata.genericTemplates[index];
        templateByKey_.emplace(generic.canonicalKey, Location{module, index});
        templatesByName_[simpleName(generic.name)].emplace_back(module, index);
    }
}

void PrecompiledSurfaceReachability::indexTraitImpls(std::size_t module)
{
    // An implementation matters once its target type is reachable. A target that no pruned module
    // declares (a builtin or a standard-library type) is always present, so the implementation
    // matters as soon as its trait is reachable; without either key it is always kept.
    const interop::LibraryMetadata& metadata = *modules_[module];
    for (std::size_t index = 0; index < metadata.traitImpls.size(); ++index)
    {
        const interop::LibraryMetadataTraitImpl& traitImpl = metadata.traitImpls[index];
        const std::string_view target = firstTypeIdentity(traitImpl.targetTypeKey);
        if (!target.empty() && typesByIdentity_.contains(target))
        {
            traitImplsByType_[target].emplace_back(module, index);
            continue;
        }
        const std::string_view trait = firstTypeIdentity(traitImpl.traitTypeKey);
        if (!trait.empty())
        {
            traitImplsByType_[trait].emplace_back(module, index);
            continue;
        }
        unconditionalTraitImpls_.emplace_back(module, index);
    }
}

void PrecompiledSurfaceReachability::reach(const std::unordered_set<std::string>& names)
{
    for (const std::string& name : names)
    {
        reachName(name);
    }
    for (const Location& location : unconditionalTraitImpls_)
    {
        includeTraitImpl(location);
    }
    drain();
}

void PrecompiledSurfaceReachability::drain()
{
    while (!pendingNames_.empty() || !pendingTypes_.empty() || !pendingTemplates_.empty())
    {
        while (!pendingNames_.empty())
        {
            const std::string_view name = pendingNames_.back();
            pendingNames_.pop_back();
            processName(name);
        }
        while (!pendingTypes_.empty())
        {
            const std::string_view identity = pendingTypes_.back();
            pendingTypes_.pop_back();
            processType(identity);
        }
        while (!pendingTemplates_.empty())
        {
            const std::string_view key = pendingTemplates_.back();
            pendingTemplates_.pop_back();
            processTemplate(key);
        }
    }
}

void PrecompiledSurfaceReachability::reachName(std::string_view name)
{
    if (!name.empty() && reachedNames_.insert(name).second) pendingNames_.push_back(name);
}

void PrecompiledSurfaceReachability::reachType(std::string_view identity)
{
    if (!identity.empty() && reachedTypes_.insert(identity).second) pendingTypes_.push_back(identity);
}

void PrecompiledSurfaceReachability::reachTemplate(std::string_view key)
{
    if (!key.empty() && reachedTemplates_.insert(key).second) pendingTemplates_.push_back(key);
}

void PrecompiledSurfaceReachability::processName(std::string_view name)
{
    if (const auto types = typesByName_.find(name); types != typesByName_.end())
    {
        for (const Location& location : types->second)
            reachType(modules_[location.first]->types[location.second].identityKey);
    }
    if (const auto templates = templatesByName_.find(name); templates != templatesByName_.end())
    {
        for (const Location& location : templates->second)
            reachTemplate(modules_[location.first]->genericTemplates[location.second].canonicalKey);
    }
    if (const auto functions = functionsByName_.find(name); functions != functionsByName_.end())
    {
        for (const Location& location : functions->second) includeFunction(location);
    }
    if (const auto states = statesByName_.find(name); states != statesByName_.end())
    {
        for (const Location& location : states->second) includeState(location);
    }
    if (const auto specialized = specializedTypesByTemplateName_.find(name);
        specialized != specializedTypesByTemplateName_.end())
    {
        for (const std::string_view identity : specialized->second) reachType(identity);
    }
}

void PrecompiledSurfaceReachability::processType(std::string_view identity)
{
    // A nested type is declared inside its containing type, which must therefore be present too.
    const std::size_t moduleSeparator = identity.find("::");
    const std::size_t nesting = identity.find_last_of('+');
    if (moduleSeparator != std::string_view::npos && nesting != std::string_view::npos &&
        nesting > moduleSeparator)
    {
        reachType(identity.substr(0, nesting));
    }
    if (const auto types = typesByIdentity_.find(identity); types != typesByIdentity_.end())
    {
        for (const Location& location : types->second) includeType(location);
    }
    if (const auto specializations = specializationsByType_.find(identity);
        specializations != specializationsByType_.end())
    {
        for (const Location& location : specializations->second)
        {
            const interop::LibraryMetadataGenericSpecialization& specialization =
                modules_[location.first]->genericSpecializations[location.second];
            reachTemplate(specialization.templateKey);
            for (const std::string& argument : specialization.typeArguments) scanTypeKeys(argument);
        }
    }
    if (identity.find('`') != std::string_view::npos) reachTemplate(identity);
}

void PrecompiledSurfaceReachability::processTemplate(std::string_view key)
{
    const auto found = templateByKey_.find(key);
    if (found != templateByKey_.end()) includeTemplate(found->second);
}

void PrecompiledSurfaceReachability::includeType(const Location& location)
{
    std::vector<bool>::reference included = includedTypes_[location.first][location.second];
    if (included) return;
    included = true;
    const interop::LibraryMetadataType& type = modules_[location.first]->types[location.second];
    scanTypeKeys(type.baseTypeKey);
    for (const std::string& key : type.interfaceTypeKeys) scanTypeKeys(key);
    scanTypeKeys(type.enumUnderlyingTypeKey);
    scanTypeKeys(type.delegateReturnTypeKey);
    scanTypeKeys(type.delegateErrorTypeKey);
    for (const std::string& key : type.delegateParameterTypeKeys) scanTypeKeys(key);
    for (const interop::LibraryMetadataVariantCase& variantCase : type.variantCases)
        scanTypeKeys(variantCase.payloadTypeKey);
    for (const std::string& signature : type.traitMethodSignatures) scanTypeKeys(signature);
    for (const interop::LibraryMetadataAssociatedItem& item : type.associatedItems) scanTypeKeys(item.typeKey);
    for (const interop::LibraryMetadataField& field : type.fields) scanTypeKeys(field.typeKey);
    const std::string declaringTypeKey = std::string(kTypeKeyPrefix) + type.identityKey;
    if (const auto members = membersByDeclaringType_.find(declaringTypeKey);
        members != membersByDeclaringType_.end())
    {
        for (const Location& member : members->second) includeMember(member);
    }
    if (const auto traitImpls = traitImplsByType_.find(type.identityKey); traitImpls != traitImplsByType_.end())
    {
        for (const Location& traitImpl : traitImpls->second) includeTraitImpl(traitImpl);
    }
}

void PrecompiledSurfaceReachability::includeMember(const Location& location)
{
    std::vector<bool>::reference included = includedMembers_[location.first][location.second];
    if (included) return;
    included = true;
    scanCallable(modules_[location.first]->members[location.second]);
}

void PrecompiledSurfaceReachability::includeTraitImpl(const Location& location)
{
    std::vector<bool>::reference included = includedTraitImpls_[location.first][location.second];
    if (included) return;
    included = true;
    const interop::LibraryMetadataTraitImpl& traitImpl = modules_[location.first]->traitImpls[location.second];
    scanTypeKeys(traitImpl.targetTypeKey);
    scanTypeKeys(traitImpl.traitTypeKey);
    for (const interop::LibraryMetadataMember& method : traitImpl.methods) scanCallable(method);
}

void PrecompiledSurfaceReachability::includeFunction(const Location& location)
{
    std::vector<bool>::reference included = includedFunctions_[location.first][location.second];
    if (included) return;
    included = true;
    const interop::LibraryMetadataNamespaceFunction& function =
        modules_[location.first]->namespaceFunctions[location.second];
    scanTypeKeys(function.declaredTypeKey);
    scanTypeKeys(function.declaredErrorTypeKey);
    scanTypeKeys(function.signatureKey);
    scanParameters(function.parameters);
}

void PrecompiledSurfaceReachability::includeState(const Location& location)
{
    std::vector<bool>::reference included = includedStates_[location.first][location.second];
    if (included) return;
    included = true;
    const interop::LibraryMetadataNamespaceState& state = modules_[location.first]->namespaceStates[location.second];
    scanTypeKeys(state.declaredTypeKey);
    scanSourceNames(state.initializerText);
}

void PrecompiledSurfaceReachability::includeTemplate(const Location& location)
{
    std::vector<bool>::reference included = includedTemplates_[location.first][location.second];
    if (included) return;
    included = true;
    const interop::LibraryMetadata& metadata = *modules_[location.first];
    const interop::LibraryMetadataGenericTemplate& generic = metadata.genericTemplates[location.second];
    scanSourceNames(generic.sourceText);
    for (const std::string& dependency : generic.dependencies)
    {
        reachTemplate(dependency);
        scanTypeKeys(dependency);
    }
    for (const interop::LibraryMetadataGenericParameterContract& contract : generic.parameterContracts)
    {
        scanTypeKeys(contract.constantTypeKey);
        scanTypeKeys(contract.equalityTypeKey);
        scanTypeKeys(contract.associatedProjectionKey);
        for (const std::string& key : contract.typeConstraintKeys) scanTypeKeys(key);
    }
    // A generic method is materialized inside partial declarations of its declaring type.
    if (!generic.ownerName.empty())
    {
        std::string owner = metadata.moduleName + "::";
        if (!generic.namespaceName.empty()) owner += generic.namespaceName + ".";
        owner += generic.ownerName;
        reachType(storage_.emplace_back(std::move(owner)));
    }
}

void PrecompiledSurfaceReachability::scanCallable(const interop::LibraryMetadataMember& member)
{
    scanTypeKeys(member.declaringTypeKey);
    scanTypeKeys(member.declaredTypeKey);
    scanTypeKeys(member.declaredErrorTypeKey);
    scanTypeKeys(member.signatureKey);
    scanParameters(member.parameters);
    scanSourceNames(member.constantText);
    scanSourceNames(member.inlineSourceText);
}

void PrecompiledSurfaceReachability::scanParameters(std::span<const interop::LibraryMetadataParameter> parameters)
{
    for (const interop::LibraryMetadataParameter& parameter : parameters)
    {
        scanTypeKeys(parameter.typeKey);
        scanSourceNames(parameter.defaultValueText);
    }
}

void PrecompiledSurfaceReachability::scanTypeKeys(std::string_view text)
{
    std::size_t cursor = 0;
    while ((cursor = text.find(kTypeKeyPrefix, cursor)) != std::string_view::npos)
    {
        cursor += kTypeKeyPrefix.size();
        const std::size_t begin = cursor;
        while (cursor < text.size() && isNameByte(text[cursor])) ++cursor;
        if (cursor == begin || cursor + 1U >= text.size() || text[cursor] != ':' || text[cursor + 1U] != ':')
            continue;
        cursor += 2U;
        while (cursor < text.size() && isQualifiedNameByte(text[cursor])) ++cursor;
        reachType(text.substr(begin, cursor - begin));
    }
}

void PrecompiledSurfaceReachability::scanSourceNames(std::string_view text)
{
    std::size_t cursor = 0;
    while (cursor < text.size())
    {
        if (!isNameByte(text[cursor]))
        {
            ++cursor;
            continue;
        }
        const std::size_t begin = cursor;
        while (cursor < text.size() && isNameByte(text[cursor])) ++cursor;
        reachName(text.substr(begin, cursor - begin));
    }
}

bool PrecompiledSurfaceReachability::everythingReached(std::size_t module) const
{
    return allSet(includedTypes_[module]) && allSet(includedMembers_[module]) &&
        allSet(includedTraitImpls_[module]) && allSet(includedFunctions_[module]) &&
        allSet(includedStates_[module]) && allSet(includedTemplates_[module]);
}

std::shared_ptr<const interop::LibraryMetadata> PrecompiledSurfaceReachability::prunedMetadata(
    std::size_t module) const
{
    if (fullModules_[module] || everythingReached(module)) return modules_[module];
    const interop::LibraryMetadata& metadata = *modules_[module];
    auto pruned = std::make_shared<interop::LibraryMetadata>();
    pruned->formatMajor = metadata.formatMajor;
    pruned->formatMinor = metadata.formatMinor;
    pruned->payloadDecodedByCurrentReader = metadata.payloadDecodedByCurrentReader;
    pruned->targetId = metadata.targetId;
    pruned->pointerSize = metadata.pointerSize;
    pruned->pointerAlignment = metadata.pointerAlignment;
    pruned->languageFeatureFlags = metadata.languageFeatureFlags;
    pruned->producerVersion = metadata.producerVersion;
    pruned->moduleName = metadata.moduleName;
    pruned->targetTriple = metadata.targetTriple;
    pruned->nativeAbiMajor = metadata.nativeAbiMajor;
    pruned->nativeAbiMinor = metadata.nativeAbiMinor;
    pruned->runtimeAbiMajor = metadata.runtimeAbiMajor;
    pruned->runtimeAbiMinor = metadata.runtimeAbiMinor;
    pruned->manglingAbiVersion = metadata.manglingAbiVersion;
    pruned->objectAbiVersion = metadata.objectAbiVersion;
    pruned->delegateAbiVersion = metadata.delegateAbiVersion;
    pruned->exceptionAbiVersion = metadata.exceptionAbiVersion;
    pruned->managedDelegateCellSize = metadata.managedDelegateCellSize;
    pruned->abiFingerprint = metadata.abiFingerprint;
    // The kept records stay in their original order: lookups binary-search the sorted type table.
    copyIncluded(metadata.types, includedTypes_[module], pruned->types);
    copyIncluded(metadata.members, includedMembers_[module], pruned->members);
    copyIncluded(metadata.traitImpls, includedTraitImpls_[module], pruned->traitImpls);
    copyIncluded(metadata.namespaceFunctions, includedFunctions_[module], pruned->namespaceFunctions);
    copyIncluded(metadata.namespaceStates, includedStates_[module], pruned->namespaceStates);
    copyIncluded(metadata.genericTemplates, includedTemplates_[module], pruned->genericTemplates);
    pruned->genericSpecializations = metadata.genericSpecializations;
    pruned->externalTypeKeys = metadata.externalTypeKeys;
    pruned->dispatchTables = metadata.dispatchTables;
    pruned->exports = metadata.exports;
    pruned->binaryDependencies = metadata.binaryDependencies;
    pruned->platformImports = metadata.platformImports;
    return pruned;
}
} // namespace nativesharp::compilation
