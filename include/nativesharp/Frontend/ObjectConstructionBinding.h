#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ConstructionResolutionKind.h"
#include "nativesharp/Frontend/ConstructorCandidateBinding.h"
#include "nativesharp/Frontend/ConstructorOwnershipTransfer.h"
#include "nativesharp/Frontend/ProjectConstructionSite.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct ObjectConstructionBinding final
{
    ConstructionResolutionKind resolution = ConstructionResolutionKind::InvalidTarget;
    const ast::AstNode* syntax = nullptr;
    std::size_t callerMemberOrdinal = 0;
    std::size_t targetTypeOrdinal = 0;
    // C6 "struct members": `new Vec2(1, 2)` on a STRUCT means "initialize in place", not
    // "allocate". Everything about resolving which constructor runs is identical to a class --
    // same candidates, same overload rules, same argument conversions -- so only this flag and
    // the lowering differ. See ProjectOopHirModel's ObjectCreationExpression case.
    bool targetIsStruct = false;
    // NS304-T13-F02: `new (allocator) T(...)` where `allocator : IAllocator`. The HIR desugars to
    // `allocator.Allocate(size)` + placement construction on the returned pointer.
    bool allocatorSelection = false;
    std::string allocatorTypeKey;
    std::optional<std::size_t> allocatorAllocateMemberOrdinal;
    // NS304-T13-F07: the allocator's Deallocate member (provenance pair of Allocate).
    std::optional<std::size_t> allocatorDeallocateMemberOrdinal;
    // NS304-T13-F08: the explicit failure profile (None when `failure:` is absent).
    ProjectAllocatorFailureProfile allocatorFailureProfile = ProjectAllocatorFailureProfile::None;
    // NS304-T13-F08 `failure: result`: the Ok/Error factory members of Result<T,E>.
    std::optional<std::size_t> allocatorResultOkFactoryMemberOrdinal;
    std::optional<std::size_t> allocatorResultErrorFactoryMemberOrdinal;
    std::string allocatorResultCarrierTypeKey;
    std::string targetTypeKey;
    std::vector<std::size_t> explicitConstructorMemberOrdinals;
    std::vector<ConstructorCandidateBinding> evaluatedCandidates;
    std::optional<std::size_t> selectedConstructorMemberOrdinal;
    bool selectedImplicitDefault = false;
    std::optional<std::size_t> implicitDefaultBaseConstructorMemberOrdinal;
    std::vector<ProjectImplicitConversion> selectedConversions;
    std::vector<std::size_t> selectedArgumentParameterOrdinals;
    std::vector<bool> selectedParameterPackElements;
    std::optional<std::size_t> selectedParameterPackOrdinal;
    bool selectedUsesExpandedParameterPack = false;
    bool selectedSynthesizesEmptyParameterPack = false;
    std::vector<const ast::AstNode*> selectedDefaultArguments;
    std::vector<std::size_t> selectedDefaultArgumentParameterOrdinals;
    // NSC-LIMIT-010. Las transferencias del candidato ELEGIDO, copiadas aqui para que quien
    // lea el binding final no tenga que volver a recorrer los candidatos evaluados.
    std::vector<ConstructorOwnershipTransfer> selectedOwnershipTransfers;
    std::vector<std::size_t> selectedFullExpressionBorrowArgumentOrdinals;
};
}
