#pragma once

#include "nativesharp/Frontend/ProjectOopHirNodeKind.h"
#include "nativesharp/Frontend/ProjectCollectionSegmentKind.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>


namespace nativesharp::frontend
{
struct ProjectOopHirNode final
{
    ~ProjectOopHirNode();

    ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::Block;
    const ast::AstNode* syntax = nullptr;
    std::string typeKey;
    std::string elementTypeKey;
    std::string stringValue;
    // Item 381. Late debug lowering needs the source shape that the pointer-shaped internal VLA
    // representation and placement result would otherwise erase.
    std::string debugDisplayName;
    lexer::TokenKind operation = lexer::TokenKind::Invalid;
    std::optional<std::size_t> memberOrdinal;
    std::optional<std::size_t> namespaceFunctionOrdinal;
    std::optional<std::size_t> namespaceStateOrdinal;
    // Stage 54.4 "Condition Variables" -- set ONLY on a CoreMonitorWait node, alongside
    // memberOrdinal above (which, for THIS node kind only, holds the `cv_` field's ordinal rather
    // than an ordinary field access's own ordinal). Holds the `lock_` field's ordinal. Default-
    // constructed to nullopt for every other node kind -- adding this field required no update to
    // any existing construction site in this codebase, since ProjectOopHirNode is always built via
    // makeNode()+field assignment, never positional aggregate initialization (see the fresh
    // Stage 54.3 lesson about MethodCoreBinding, which WAS positionally aggregate-initialized and
    // needed all 16 call sites patched for exactly this reason -- this struct has no such risk).
    std::optional<std::size_t> secondaryMemberOrdinal;
    std::optional<std::size_t> resultSuccessTestMemberOrdinal;
    std::optional<std::size_t> resultSuccessExtractorMemberOrdinal;
    std::optional<std::size_t> resultErrorExtractorMemberOrdinal;
    std::optional<std::size_t> resultErrorFactoryMemberOrdinal;
    std::optional<std::size_t> resultErrorConversionMemberOrdinal;
    std::optional<std::size_t> localOrdinal;
    std::optional<std::size_t> parameterOrdinal;
    std::optional<std::size_t> constructionOrdinal;
    std::optional<std::size_t> targetTypeOrdinal;
    std::optional<std::size_t> exceptionTypeOrdinal;
    std::optional<std::size_t> cleanupRegionOrdinal;
    std::optional<std::size_t> hirRegionOrdinal;
    std::optional<std::size_t> exceptionTargetRegionOrdinal;
    std::optional<std::size_t> targetScopeOrdinal;
    std::optional<std::size_t> disposeContractMemberOrdinal;
    std::optional<std::size_t> disposeImplementationMemberOrdinal;
    std::vector<std::size_t> cleanupRegionOrdinals;
    std::optional<MethodCleanupContinuationKind> cleanupContinuationKind;
    std::optional<MethodBorrowKind> borrowKind;
    std::optional<MethodLocalBorrowSourceKind> localBorrowSourceKind;
    language::CaptureMode captureMode = language::CaptureMode::Reference;
    std::optional<ProjectImplicitConversion> conversion;
    // Set only on a SwitchSection node: the folded constant value of every label on that section,
    // in source order. Empty on a section whose only label is `default`. Signed even when the
    // governing type is unsigned -- the value is a bit pattern here, and the backend re-widens it
    // to the governing type when it builds the LLVM case constant.
    std::vector<std::int64_t> switchLabels;
    // Set only on a SwitchSection node: this section carries the `default` label.
    bool switchHasDefault = false;
    std::uint64_t integerValue = 0;
    // T01-F05 AggregateElement descriptors under AggregateSpread use this as the source-relative
    // byte offset while integerValue remains the destination-absolute offset.
    std::uint64_t secondaryIntegerValue = 0;
    // NS304-T13-F06: a RawElementAddress over a flexible array carries the byte offset of its
    // `Length` member in secondaryIntegerValue; this flag separates presence from value so a
    // Length field at offset 0 stays distinguishable from "no bounds check".
    bool hasTrailingLengthBounds = false;
    // NS304-T13-F06: Length's byte offset RELATIVE TO the flexible-array base address (which is
    // the Data field, not the struct head), so it can be negative.
    std::int64_t trailingLengthRelativeOffset = 0;
    // NS304-T13-F07: allocator provenance attached to a placement construction (`new (alloc) T(...)`).
    // contextOrdinal is the temp local holding the allocator reference; the member ordinal names its
    // Deallocate; typeOrdinal drives the generated per-allocator deallocate thunk.
    std::optional<std::size_t> allocatorContextLocalOrdinal;
    std::optional<std::size_t> allocatorDeallocateMemberOrdinal;
    std::optional<std::size_t> allocatorTypeOrdinal;
    std::uint64_t resultSuccessTag = 0;
    std::uint64_t resultErrorTag = 0;
    std::uint64_t resultReturnErrorTag = 0;
    std::string resultSourceCarrierTypeKey;
    std::string resultSourceErrorTypeKey;
    std::string resultReturnCarrierTypeKey;
    std::string resultReturnErrorTypeKey;
    // Item 329. AggregateElement uses these when a positional initializer targets a bit field.
    // The byte offset remains in integerValue, just like an ordinary aggregate element.
    std::optional<std::uint64_t> bitOffset;
    std::optional<std::uint64_t> bitWidth;
    std::uint64_t bitStorageSize = 0;
    std::uint64_t bitStorageAlignment = 1;
    double floatingValue = 0.0;
    bool booleanValue = false;
    // One entry per materialized collection child, preserving literal/spread source order.
    // Empty means this ArrayAllocation uses the ordinary count/initializer representation.
    std::vector<ProjectCollectionSegmentKind> collectionSegments;
    // Item 312. Set on LocalDeclaration when its physical slot stores an alias address rather
    // than a value of typeKey.
    bool referenceLocal = false;
    // Capability of a reference local. The backend representation remains an address slot;
    // semantic assignment checks have already enforced readonly `ref` before HIR construction.
    bool referenceMutable = false;
    // NativeSharp 3.0.4 T01-F06. LocalDeclaration uses module storage and a once guard.
    bool staticLocal = false;
    // NativeSharp 3.0 T06-F06. The local's storage/address is statically pinned for its lexical
    // lifetime. Kept in HIR so coroutine/debug/FFI consumers never infer it from source syntax.
    bool pinnedStorage = false;
    // Item 313. A call with this bit produces an address although typeKey names the referent.
    // Value contexts add PointerLoad; address contexts preserve the call itself.
    bool referenceResult = false;
    // MatchExpression only: its governing child is an address used by ref/ref mut bindings.
    bool governingReference = false;
    // T01-F03. Set only on an instance getter/method reached through `?.`.  The receiver is the
    // first child as for every other instance call, but the backend must test it before lowering
    // the remaining children so argument/default expressions stay lazy on the null edge.
    bool nullConditionalCall = false;
    // Property/indexer setters have a void ABI, while the assignment expression yields the
    // converted value passed to the setter. Late lowering must return that already-evaluated
    // argument after emitting the call instead of treating the void call as the expression value.
    bool returnsAssignedValue = false;
    // NativeSharp 3.0 T08-F06. Set only after the frontend proves ABI identity and an empty
    // cleanup path for `must_tail return call(...)`.
    bool mustTailCall = false;
    // Item 377. Debug information must distinguish an owner from a borrow even though both use
    // the same physical pointer representation. This is the declaration-level qualifier;
    // objectLifetimeOwnership/storageOwnership remain the independent placement axes.
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    // Item 378. Distinguishes a scope-bound RAII owner from a manually managed owner in PDB.
    bool automaticDestruction = false;
    bool variableLengthArray = false;
    std::size_t variableLengthArrayRank = 0;
    std::size_t nativeArrayRank = 0;
    bool placementConstructed = false;
    // Item 323. Set on a proven-nonescaping RAII class local and its ObjectConstruction child.
    // The object lifetime remains owned, but its backing bytes belong to the current stack frame.
    bool stackPromotedObject = false;
    // Item 308. Set on field value/address nodes and propagated to the store that consumes an
    // address. It means LLVM volatile memory access, never atomic synchronization.
    bool volatileAccess = false;
    bool baseReceiver = false;
    bool interfaceDispatch = false;
    bool targetIsInterface = false;
    bool dynamicDestruction = false;
    bool valueTypeNoOpDeletion = false;
    // Item 320. Independent lifetime domains; placement is owned/borrowed, heap new owned/owned.
    sema::OwnershipKind objectLifetimeOwnership = sema::OwnershipKind::None;
    sema::OwnershipKind storageOwnership = sema::OwnershipKind::None;
    bool catchAll = false;
    bool catchHasFilter = false;
    bool catchExhaustive = false;
    std::vector<std::string> catchMatchedErrorTypeKeys;
    bool interfaceCleanupDispatch = false;
    std::size_t argumentCount = 0;
    std::vector<std::string> argumentTypeKeys;
    // Children remain in source evaluation order.  Calls use this map to place the resulting
    // values in ABI parameter order only after every source expression has been evaluated.
    std::vector<std::size_t> argumentParameterOrdinals;
    std::vector<std::unique_ptr<ProjectOopHirNode>> children;
};
}
