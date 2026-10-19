#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ConstructorCandidateBinding.h"
#include "nativesharp/Frontend/ConstructorInitializerKind.h"
#include "nativesharp/Frontend/ConstructorInitializerResolutionKind.h"
#include "nativesharp/Frontend/ConstructorOwnershipTransfer.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace nativesharp::frontend
{
struct ConstructorInitializerBinding final
{
    ConstructorInitializerResolutionKind resolution = ConstructorInitializerResolutionKind::Invalid;
    ConstructorInitializerKind kind = ConstructorInitializerKind::ImplicitBase;
    const ast::AstNode* initializerSyntax = nullptr;
    std::size_t constructorMemberOrdinal = 0;
    std::size_t containingTypeOrdinal = 0;
    std::optional<std::size_t> targetTypeOrdinal;
    bool explicitInitializer = false;
    std::vector<std::size_t> explicitConstructorMemberOrdinals;
    std::vector<ConstructorCandidateBinding> evaluatedCandidates;
    std::optional<std::size_t> selectedConstructorMemberOrdinal;
    std::optional<std::size_t> selectedEntryConstructorMemberOrdinal;
    bool selectedImplicitDefault = false;
    std::vector<ProjectImplicitConversion> selectedConversions;
    std::vector<std::size_t> selectedArgumentParameterOrdinals;
    std::vector<bool> selectedParameterPackElements;
    std::optional<std::size_t> selectedParameterPackOrdinal;
    bool selectedUsesExpandedParameterPack = false;
    bool selectedSynthesizesEmptyParameterPack = false;
    std::vector<const ast::AstNode*> selectedDefaultArguments;
    std::vector<std::size_t> selectedDefaultArgumentParameterOrdinals;
    // NSC-LIMIT-010: `this(...)` y `base(...)` transfieren propiedad con las mismas reglas
    // que un `new`, asi que registran las mismas transferencias.
    std::vector<ConstructorOwnershipTransfer> selectedOwnershipTransfers;
    std::vector<std::size_t> selectedFullExpressionBorrowArgumentOrdinals;
};
}
