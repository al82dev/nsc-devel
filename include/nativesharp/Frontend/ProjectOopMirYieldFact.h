#pragma once

#include "nativesharp/Frontend/MethodExpressionTypeBinding.h"
#include "nativesharp/Frontend/ProjectOopMirNodeKind.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
// Raw semantic observations, not a decision to destroy an expression result.
struct ProjectOopMirYieldFact final
{
    std::size_t yieldPreorder = 0;
    std::size_t nodePreorder = 0;
    std::optional<std::size_t> parentFact;
    std::size_t childIndex = 0;
    ProjectOopMirNodeKind nodeKind;
    bool hasBinding = false;
    std::string typeKey;
    sema::ValueCategory valueCategory = sema::ValueCategory::Value;
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    bool ownershipTransfer = false;
    bool ownershipFreshOwner = false;
    sema::OwnershipKind objectLifetimeOwnership = sema::OwnershipKind::None;
    sema::OwnershipKind storageOwnership = sema::OwnershipKind::None;
};
}
