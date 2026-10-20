#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectCallableBindingIndex.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisContext.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisStatistics.h"
#include "nativesharp/Frontend/MethodExpressionTypeBinding.h"
#include "nativesharp/Frontend/MethodEscapeOrigin.h"
#include "nativesharp/Frontend/OwnershipFlowSnapshot.h"
#include "nativesharp/Frontend/ProjectConstructionSite.h"
#include "nativesharp/Frontend/ProjectOwnershipFlowStatistics.h"
#include "nativesharp/Frontend/ProjectEscapePropagationStatistics.h"
#include "nativesharp/Frontend/ProjectMethodBodyBuildOptions.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Sema/ExpressionInfo.h"
#include "nativesharp/Frontend/MethodCoreOperationKind.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
struct MethodCoreBinding final
{
    MethodCoreOperationKind kind = MethodCoreOperationKind::ArrayAllocation;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string resultTypeKey;
    std::string elementTypeKey;
    bool assignable = false;
    // Stage 54.2 "Atomics" -- set only for AtomicInt32/AtomicInt64's Increment/Decrement/Add/
    // Exchange/CompareExchange core operations. These are INSTANCE methods with an IMPLICIT
    // "location" operand -- the address of the receiver's own private `value_` field -- rather
    // than an explicit argument the caller writes out. NativeSharp's OOP LLVM lowering pipeline
    // has no support for the `&`/`*` raw-pointer operators (see ProjectOopHirModel.cpp's
    // UnaryExpression case: "Stage 34 OOP lowering keeps raw pointer address/dereference in the
    // procedural backend boundary" -- a deliberate, hard boundary, discovered during this
    // stage's implementation, that rules out a direct C#-Interlocked-style `int*` parameter
    // approach entirely for any code going through the modern OOP pipeline every stage since 41
    // uses). Resolved once, during method-body recognition (ProjectMethodBodyModel.cpp), by
    // searching the declaring type's own members for its Field named "value_" -- HIR lowering
    // (ProjectOopHirModel.cpp) then builds a synthetic ProjectOopHirNodeKind::FieldAddress node
    // using this ordinal, with the call's receiver as its one child, reusing the EXACT SAME
    // field-address machinery (memberOrdinal -> ProjectObjectLayoutModel byte offset -> GEP)
    // every ordinary NativeSharp field access already relies on -- no new LLVM-lowering
    // mechanism was needed once this was understood.
    std::optional<std::size_t> implicitFieldMemberOrdinal;
    // Stage 54.4 "Condition Variables" -- set ONLY for Monitor::Wait. Every other core operation
    // that needs an implicit field address (AtomicInt32/64 above, and Monitor's own Enter/Exit/
    // Pulse/PulseAll) needs exactly ONE -- implicitFieldMemberOrdinal above is enough. Wait is the
    // sole exception: SleepConditionVariableSRW needs the addresses of TWO different fields on the
    // SAME receiver in a single call (the private `cv_` field, in implicitFieldMemberOrdinal
    // above, AND the private `lock_` field, here) -- resolved once here, during method-body
    // recognition, by the exact same by-name field search Stage 54.2 already established.
    // Deliberately NOT modeled as two separate FieldAddress HIR nodes each re-lowering the
    // receiver expression from its own AST syntax (the way Enter/Exit/Pulse/PulseAll each lower
    // their own single implicit field): a receiver expression is not guaranteed to be free of
    // side effects or even deterministic (nothing stops `GetMonitor().Wait(...)` from being valid
    // NativeSharp), so lowering it twice could evaluate two DIFFERENT objects and take the cv_
    // address of one and the lock_ address of the other -- a real, silent correctness bug this
    // round deliberately designs around. See ProjectOopHirModel.cpp's CoreMonitorWait case (the
    // receiver is lowered exactly ONCE, into a plain object-reference HIR node, with both field
    // ordinals carried as node metadata instead of pre-synthesized FieldAddress children) and
    // ProjectOopMirModel.h's ProjectOopMirNode::secondaryImmediate (the resolved BYTE OFFSET for
    // this ordinal, computed once during HIR->MIR conversion, mirroring how `immediate` already
    // holds the primary field's byte offset for an ordinary FieldAddress node).
    std::optional<std::size_t> secondaryImplicitFieldMemberOrdinal;
    // Tanda 5, items 304-306 -- present only for ordered AtomicInt32/AtomicInt64 operations.
    // overloads. The value is the stable NativeSharp MemoryOrder ABI value (0..4), deliberately
    // not an LLVMAtomicOrdering ordinal. HIR lowering consumes the source argument at compile
    // time and carries this value as node metadata, so an ordering can never become a runtime
    // integer passed accidentally to LLVM.
    std::optional<std::uint8_t> atomicMemoryOrder = std::nullopt;
    // Item 306 only: CompareExchange has an independent failure order. It remains absent on
    // every legacy overload and on all single-order atomic operations.
    std::optional<std::uint8_t> atomicFailureMemoryOrder = std::nullopt;
    // NS304-T09-F05: the CollectionBuilder core operation carries the CreateBuilder / Add /
    // ToCollection member ordinals so the HIR can emit the builder call sequence.
    std::array<std::size_t, 3> builderMemberOrdinals{};

};
}
