#pragma once

#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

namespace nativesharp::frontend
{
language::PointerContract pointerContractFromSyntax(const ast::TypeSyntax* syntax) noexcept;
bool isGenericValueTypeKey(std::string_view typeKey) noexcept;
OwnershipArgumentFit classifyOwnershipArgument(
    bool parameterOwnable,
    sema::OwnershipKind parameterOwnership,
    sema::OwnershipKind argumentOwnership,
    bool argumentTransfer,
    bool argumentIsFreshOwner,
    bool argumentIsNull) noexcept;
std::string ownershipArgumentFitMessage(
    OwnershipArgumentFit fit,
    std::string_view parameterName,
    std::string_view parameterTypeKey);
std::string typeCandidateList(
    std::span<const std::size_t> candidates,
    const ProjectNameLookup& lookup);
}
