#include "nativesharp/Language/TraitSemantics.h"

#include <array>
#include <unordered_map>
#include <unordered_set>

namespace nativesharp::language
{
bool validateTrait(const TraitContract& trait, std::string& error)
{
    error.clear();
    if (trait.qualifiedName.empty()) { error = "trait requires a qualified identity"; return false; }
    if (trait.declaresInstanceStorage) { error = "traits cannot declare instance storage"; return false; }
    std::unordered_set<std::string> signatures;
    for (const TraitMethod& method : trait.methods)
        if (method.signature.empty() || !signatures.insert(method.signature).second)
        { error = "trait method signatures must be non-empty and unique"; return false; }
    return true;
}

bool validateImplCoherence(std::span<const ImplCandidate> implementations, std::string& error)
{
    error.clear();
    std::unordered_set<std::string> identities;
    for (const ImplCandidate& item : implementations)
    {
        if (!item.traitOwnedByModule && !item.typeOwnedByModule)
        { error = "orphan impl: the module owns neither trait nor type"; return false; }
        const std::string key = item.traitIdentity + " for " + item.typeIdentity;
        if (!identities.insert(key).second)
        { error = "duplicate coherent impl for " + key; return false; }
    }
    return true;
}

bool normalizeAssociatedType(
    std::string input,
    std::span<const AssociatedProjection> projections,
    std::string& result,
    std::string& error)
{
    error.clear();
    std::unordered_map<std::string, std::string> table;
    for (const auto& item : projections)
        if (!table.emplace(item.projection, item.replacement).second)
        { error = "ambiguous associated type projection"; return false; }
    std::unordered_set<std::string> seen;
    while (table.contains(input))
    {
        if (!seen.insert(input).second) { error = "cyclic associated type projection"; return false; }
        input = table.at(input);
    }
    result = std::move(input);
    return true;
}

bool validateOpaqueResultTypes(std::span<const std::string> concreteReturns, std::string& error)
{
    error.clear();
    if (concreteReturns.empty()) { error = "opaque result declaration has no return type"; return false; }
    for (const std::string& type : concreteReturns)
        if (type != concreteReturns.front())
        { error = "opaque result paths resolve to different concrete types"; return false; }
    return true;
}

bool isObjectSafe(std::span<const TraitMethod> methods, std::string& error)
{
    error.clear();
    for (const TraitMethod& method : methods)
        if (method.isGeneric || method.returnsSelfByValue)
        { error = "dyn trait methods cannot be generic or return Self by value"; return false; }
    return true;
}

TraitObjectLayout windowsX64TraitObjectLayout() noexcept
{
    // The reference points at the concrete object. Its first word points at the class descriptor,
    // whose interface map selects the stable trait method table. Conversion is therefore a
    // pointer-preserving view operation: no box, no retain cell, and no heap allocation.
    return {8, 8, true, true, false};
}

std::string dispatchMetadata(DispatchKind kind, std::string_view traitIdentity)
{
    return std::string(kind == DispatchKind::Static ? "static:" : "dyn:") + std::string(traitIdentity);
}

ExtensionResolution resolveExtension(bool hasInstanceMember, std::size_t visibleExtensions) noexcept
{
    if (hasInstanceMember) return ExtensionResolution::InstanceMember;
    if (visibleExtensions == 0) return ExtensionResolution::Missing;
    if (visibleExtensions == 1) return ExtensionResolution::UniqueExtension;
    return ExtensionResolution::Ambiguous;
}

namespace
{
constexpr std::array<OperatorTraitContract, 20> kOperatorTraits = {{
    {"+", 1, "lang:UnaryPlus", "op_UnaryPlus"},
    {"-", 1, "lang:Negate", "op_UnaryNegation"},
    {"!", 1, "lang:LogicalNot", "op_LogicalNot"},
    {"~", 1, "lang:OnesComplement", "op_OnesComplement"},
    {"+", 2, "lang:Add", "op_Addition"},
    {"-", 2, "lang:Subtract", "op_Subtraction"},
    {"*", 2, "lang:Multiply", "op_Multiply"},
    {"/", 2, "lang:Divide", "op_Division"},
    {"%", 2, "lang:Remainder", "op_Modulus"},
    {"&", 2, "lang:BitwiseAnd", "op_BitwiseAnd"},
    {"|", 2, "lang:BitwiseOr", "op_BitwiseOr"},
    {"^", 2, "lang:ExclusiveOr", "op_ExclusiveOr"},
    {"<<", 2, "lang:LeftShift", "op_LeftShift"},
    {">>", 2, "lang:RightShift", "op_RightShift"},
    {"==", 2, "lang:Equality", "op_Equality"},
    {"!=", 2, "lang:Inequality", "op_Inequality"},
    {"<", 2, "lang:LessThan", "op_LessThan"},
    {"<=", 2, "lang:LessThanOrEqual", "op_LessThanOrEqual"},
    {">", 2, "lang:GreaterThan", "op_GreaterThan"},
    {">=", 2, "lang:GreaterThanOrEqual", "op_GreaterThanOrEqual"},
}};
}

std::span<const OperatorTraitContract> operatorTraitContracts() noexcept
{
    return kOperatorTraits;
}

const OperatorTraitContract* operatorTraitContract(
    std::string_view spelling,
    std::size_t arity) noexcept
{
    for (const OperatorTraitContract& contract : kOperatorTraits)
        if (contract.spelling == spelling && contract.arity == arity) return &contract;
    return nullptr;
}

const OperatorTraitContract* operatorTraitContractForMethod(
    std::string_view methodName,
    std::size_t arity) noexcept
{
    for (const OperatorTraitContract& contract : kOperatorTraits)
        if (contract.methodName == methodName && contract.arity == arity) return &contract;
    return nullptr;
}

std::string_view operatorTrait(std::string_view spelling, std::size_t arity) noexcept
{
    const OperatorTraitContract* contract = operatorTraitContract(spelling, arity);
    return contract != nullptr ? contract->langItem : std::string_view{};
}

std::string_view operatorMethodName(std::string_view spelling, std::size_t arity) noexcept
{
    const OperatorTraitContract* contract = operatorTraitContract(spelling, arity);
    return contract != nullptr ? contract->methodName : std::string_view{};
}

std::string_view operatorTrait(std::string_view spelling) noexcept
{
    if (const OperatorTraitContract* binary = operatorTraitContract(spelling, 2))
        return binary->langItem;
    if (const OperatorTraitContract* unary = operatorTraitContract(spelling, 1))
        return unary->langItem;
    return std::string_view{};
}

bool operatorCanBeOverloaded(std::string_view spelling) noexcept
{
    // Short-circuiting is control flow, not an eager two-argument call. It stays intrinsic until
    // a dedicated truthiness/branching protocol is designed; ordinary trait operators must never
    // make the right operand eager. Unknown spellings are not silently accepted either.
    if (spelling == "&&" || spelling == "||" || spelling == "??") return false;
    for (const OperatorTraitContract& contract : kOperatorTraits)
        if (contract.spelling == spelling) return true;
    return false;
}
}
