#include "nativesharp/Language/MetaprogrammingSemantics.h"
#include "nativesharp/Language/MetaprogrammingGeneration.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace nativesharp::language
{
std::vector<IntrospectionMember> visibleIntrospectionMembers(
    std::span<const IntrospectionMember> members,
    bool sameModule)
{
    std::vector<IntrospectionMember> result;
    for (const auto& member : members)
        if (sameModule || member.isPublic) result.push_back(member);
    std::ranges::sort(result, {}, &IntrospectionMember::sourceOrdinal);
    return result;
}

bool introspectionRequiresRuntimeMetadata() noexcept { return false; }

bool validateAttributes(std::span<const AttributeUse> attributes, std::string& error)
{
    error.clear();
    std::unordered_set<std::string> seen;
    for (const AttributeUse& attribute : attributes)
    {
        if (attribute.identity.empty() || attribute.target.empty())
        { error = "attribute requires identity and target"; return false; }
        const std::string key = attribute.identity + "@" + attribute.target;
        if (!attribute.repeatable && !seen.insert(key).second)
        { error = "non-repeatable attribute appears more than once"; return false; }
    }
    return true;
}

std::string deriveDependencyKey(
    std::string_view generatorIdentity,
    std::span<const std::string> fieldIdentities)
{
    std::vector<std::string> sorted(fieldIdentities.begin(), fieldIdentities.end());
    std::ranges::sort(sorted);
    std::string result = "derive:" + std::string(generatorIdentity) + "|";
    for (const std::string& field : sorted) result += std::to_string(field.size()) + ":" + field + ";";
    return result;
}

bool sameBinding(const HygienicName& left, const HygienicName& right) noexcept
{
    return left.spelling == right.spelling && left.scopeId == right.scopeId;
}

bool capabilityAllowedInHermeticProfile(ComptimeCapability capability) noexcept
{
    return capability == ComptimeCapability::ReadDeclaredFile ||
        capability == ComptimeCapability::ReadDeclaredEnvironment;
}

std::string comptimeInputIdentity(std::span<const ComptimeInput> inputs)
{
    std::vector<ComptimeInput> sorted(inputs.begin(), inputs.end());
    std::ranges::sort(sorted, {}, &ComptimeInput::identity);
    std::string result = "comptime-input-v1|";
    for (const auto& item : sorted) result += item.identity + "=" + item.contentDigest + ";";
    return result;
}

bool validPackageDiagnosticCode(std::string_view package, std::string_view code) noexcept
{
    return !package.empty() && !code.empty() && !code.starts_with("NS3") && code.starts_with(package);
}

ComptimeBudget::ComptimeBudget(
    std::size_t steps,
    std::size_t bytes,
    std::size_t depth) noexcept
    : steps_(steps), bytes_(bytes), depth_(depth) {}

bool ComptimeBudget::consume(
    std::size_t steps,
    std::size_t bytes,
    std::size_t depth,
    std::string& error)
{
    error.clear();
    if (steps > steps_ || bytes > bytes_ || depth > depth_)
    { error = "compile-time evaluation exceeded its deterministic resource budget"; return false; }
    steps_ -= steps;
    bytes_ -= bytes;
    return true;
}

namespace
{
bool reservedGeneratedName(std::string_view spelling)
{
    return spelling.starts_with("__nsc_");
}

GeneratedDeclaration finish(GeneratedDeclaration declaration)
{
    if (declaration.name.spelling.empty())
        declaration.error = "generated declaration name must not be empty";
    else if (reservedGeneratedName(declaration.name.spelling))
        declaration.error = "generated declaration may not use the reserved __nsc_ namespace";
    else if (declaration.originPath.empty())
        declaration.error = "generated declaration must carry a source origin";
    return declaration;
}
}

GeneratedDeclaration MetaprogrammingAstBuilder::buildField(
    std::string name,
    std::string typeKey,
    std::size_t hygieneScope,
    std::string originPath,
    bool isPublic,
    bool isStatic,
    bool isReadonly) const
{
    GeneratedDeclaration declaration;
    declaration.kind = GeneratedDeclarationKind::Member;
    declaration.name = HygienicName{std::move(name), hygieneScope};
    declaration.typeKey = std::move(typeKey);
    declaration.originPath = std::move(originPath);
    declaration.isPublic = isPublic;
    declaration.isStatic = isStatic;
    declaration.isReadonly = isReadonly;
    if (declaration.typeKey.empty())
        declaration.error = "generated field type must not be empty";
    return finish(std::move(declaration));
}

GeneratedDeclaration MetaprogrammingAstBuilder::buildMethod(
    std::string name,
    std::string typeKey,
    std::size_t hygieneScope,
    std::string originPath,
    bool isPublic,
    bool isStatic) const
{
    GeneratedDeclaration declaration;
    declaration.kind = GeneratedDeclarationKind::Member;
    declaration.name = HygienicName{std::move(name), hygieneScope};
    declaration.typeKey = std::move(typeKey);
    declaration.originPath = std::move(originPath);
    declaration.isPublic = isPublic;
    declaration.isStatic = isStatic;
    if (declaration.typeKey.empty())
        declaration.error = "generated method return type must not be empty";
    return finish(std::move(declaration));
}

GeneratedDeclaration MetaprogrammingAstBuilder::buildImpl(
    std::string traitName,
    std::string targetTypeKey,
    std::size_t hygieneScope,
    std::string originPath) const
{
    GeneratedDeclaration declaration;
    declaration.kind = GeneratedDeclarationKind::Impl;
    declaration.name = HygienicName{std::move(traitName), hygieneScope};
    declaration.typeKey = std::move(targetTypeKey);
    declaration.originPath = std::move(originPath);
    declaration.isPublic = true;
    if (declaration.typeKey.empty())
        declaration.error = "generated impl target type must not be empty";
    return finish(std::move(declaration));
}

GeneratedDeclaration MetaprogrammingAstBuilder::buildHelper(
    std::string name,
    std::string typeKey,
    std::size_t hygieneScope,
    std::string originPath,
    bool isPublic) const
{
    GeneratedDeclaration declaration;
    declaration.kind = GeneratedDeclarationKind::Helper;
    declaration.name = HygienicName{std::move(name), hygieneScope};
    declaration.typeKey = std::move(typeKey);
    declaration.originPath = std::move(originPath);
    declaration.isPublic = isPublic;
    if (declaration.typeKey.empty())
        declaration.error = "generated helper type must not be empty";
    return finish(std::move(declaration));
}

bool MetaprogrammingAstBuilder::wouldCollide(
    const std::vector<HygienicName>& existing,
    const GeneratedDeclaration& declaration) noexcept
{
    return std::any_of(existing.begin(), existing.end(), [&](const HygienicName& other)
    {
        return sameBinding(other, declaration.name);
    });
}
}
