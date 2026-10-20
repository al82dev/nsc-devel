#pragma once

#include "nativesharp/Frontend/ProjectOopMirNodeKind.h"

#include "nativesharp/Language/ErrorSemantics.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"
#include "nativesharp/Frontend/TemplateParseCache.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace nativesharp::frontend
{
struct ProjectOopMirNode final
{
    ~ProjectOopMirNode() noexcept;

    ProjectOopMirNodeKind kind = ProjectOopMirNodeKind::Block;
    const ast::AstNode* syntax = nullptr;
    std::string typeKey;
    std::string elementTypeKey;
    std::string stringValue;
    // Item 381. Source-facing name/shape retained for CodeView after HIR lowering.
    std::string debugDisplayName;
    lexer::TokenKind operation = lexer::TokenKind::Invalid;
    std::optional<std::size_t> memberOrdinal;
    std::optional<std::size_t> namespaceFunctionOrdinal;
    std::optional<std::size_t> namespaceStateOrdinal;
    // Stage 54.4 "Condition Variables" -- mirrors ProjectOopHirNode::secondaryMemberOrdinal (see
    // its comment there). Set only on a CoreMonitorWait node.
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
    std::optional<std::size_t> mirRegionOrdinal;
    std::optional<std::size_t> exceptionTargetRegionOrdinal;
    std::optional<std::size_t> targetScopeOrdinal;
    std::optional<std::size_t> disposeContractMemberOrdinal;
    std::optional<std::size_t> disposeImplementationMemberOrdinal;
    std::vector<std::size_t> cleanupRegionOrdinals;
    std::optional<MethodCleanupContinuationKind> cleanupContinuationKind;
    std::optional<MethodBorrowKind> borrowKind;
    std::optional<MethodLocalBorrowSourceKind> localBorrowSourceKind;
    language::CaptureMode captureMode = language::CaptureMode::Reference;
    std::optional<std::size_t> virtualSlot;
    std::optional<std::size_t> interfaceSlot;
    std::optional<ProjectConversionKind> conversionKind;
    // SwitchSection only -- carried through from the HIR unchanged. See ProjectOopHirModel.h.
    std::vector<std::int64_t> switchLabels;
    bool switchHasDefault = false;
    std::uint64_t immediate = 0;
    // Stage 54.4 "Condition Variables" -- set only on a CoreMonitorWait node, resolved during
    // HIR->MIR conversion exactly like `immediate` above (which, for THIS node kind only, holds
    // the `cv_` field's byte offset rather than an ordinary FieldAddress's own offset): the
    // `lock_` field's byte offset. See ProjectOopLlvmLowerer.cpp's CoreMonitorWait case for where
    // both offsets get consumed (two GEPs off one already-lowered receiver pointer).
    std::uint64_t secondaryImmediate = 0;
    // NS304-T13-F06: a RawElementAddress over a flexible array bounds-checks against its `Length`
    // member (byte offset in secondaryImmediate); this flag separates presence from value.
    bool hasTrailingLengthBounds = false;
    // NS304-T13-F06: Length's byte offset relative to the flexible-array base (can be negative).
    std::int64_t trailingLengthRelativeOffset = 0;
    // NS304-T13-F07: allocator provenance attached to a placement construction.
    std::optional<std::size_t> allocatorContextLocalOrdinal;
    std::optional<std::size_t> allocatorDeallocateMemberOrdinal;
    std::optional<std::size_t> allocatorTypeOrdinal;
    std::string allocatorDeallocateCalleeName;
    std::uint64_t resultSuccessTag = 0;
    std::uint64_t resultErrorTag = 0;
    std::uint64_t resultReturnErrorTag = 0;
    std::string resultSourceCarrierTypeKey;
    std::string resultSourceErrorTypeKey;
    std::string resultReturnCarrierTypeKey;
    std::string resultReturnErrorTypeKey;
    std::uint64_t alignment = 1;
    // Item 329. Present together on BitFieldLoad/BitFieldAddress. `immediate` remains the byte
    // offset of the allocation unit; these values describe that unit and the logical slice in it.
    std::optional<std::uint64_t> bitOffset;
    std::optional<std::uint64_t> bitWidth;
    std::uint64_t bitStorageSize = 0;
    std::uint64_t integerValue = 0;
    double floatingValue = 0.0;
    bool booleanValue = false;
    std::vector<ProjectCollectionSegmentKind> collectionSegments;
    // Item 312. Mirrors ProjectOopHirNode::referenceLocal for backend slot materialization.
    bool referenceLocal = false;
    // Mirrors ProjectOopHirNode::referenceMutable for cache/debug consumers. LLVM intentionally
    // uses the same address representation for readonly and mutable aliases.
    bool referenceMutable = false;
    // NativeSharp 3.0.4 T01-F06. Mirrors the HIR module-storage lifetime contract.
    bool staticLocal = false;
    // NativeSharp 3.0 T06-F06. Mirrors the HIR pin contract through MIR and cache identity.
    bool pinnedStorage = false;
    // Item 313. Mirrors ProjectOopHirNode::referenceResult for physical call ABI selection.
    bool referenceResult = false;
    // Mirrors the HIR governing-address contract for pattern ref/ref mut bindings.
    bool governingReference = false;
    // T01-F03. Mirrors ProjectOopHirNode::nullConditionalCall.
    bool nullConditionalCall = false;
    // Mirrors the setter-assignment value contract without changing the setter's void ABI.
    bool returnsAssignedValue = false;
    // NativeSharp 3.0 T08-F06. A semantic requirement, not an optimizer hint.
    bool mustTailCall = false;
    // Item 377. Retained for CodeView/PDB ownership and borrow visualization.
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    // Item 378. Scope-bound destruction remains visible to late debug-info lowering.
    bool automaticDestruction = false;
    bool variableLengthArray = false;
    std::size_t variableLengthArrayRank = 0;
    std::size_t nativeArrayRank = 0;
    bool placementConstructed = false;
    // Item 323. Mirrors the HIR proof; AllocateAndConstruct consumes a frame-owned object buffer.
    bool stackPromotedObject = false;
    // Item 308. Mirrors ProjectOopHirNode::volatileAccess without re-inferring declarations.
    bool volatileAccess = false;
    bool allocationFailureReturnsNull = false;
    bool zeroInitialize = false;
    bool baseReceiver = false;
    bool targetIsInterface = false;
    bool dynamicDestruction = false;
    bool valueTypeNoOpDeletion = false;
    sema::OwnershipKind objectLifetimeOwnership = sema::OwnershipKind::None;
    sema::OwnershipKind storageOwnership = sema::OwnershipKind::None;
    bool catchAll = false;
    bool catchHasFilter = false;
    bool catchExhaustive = false;
    std::vector<std::string> catchMatchedErrorTypeKeys;
    bool interfaceCleanupDispatch = false;
    // Item 300: explicit optimization contract for InlineAssembly. These are derived once from
    // the validated MIR children; late consumers do not guess barrier semantics from raw text.
    bool inlineAssemblyHasSideEffects = false;
    bool inlineAssemblyClobbersMemory = false;
    bool inlineAssemblyClobbersFlags = false;
    std::size_t argumentCount = 0;
    std::vector<std::size_t> argumentParameterOrdinals;
    std::string calleeName;
    std::string secondaryCalleeName;
    std::string resultSuccessTestCalleeName;
    std::string resultSuccessExtractorCalleeName;
    std::string resultErrorExtractorCalleeName;
    std::string resultErrorFactoryCalleeName;
    std::string resultErrorConversionCalleeName;
    std::vector<std::string> argumentTypeKeys;
    std::vector<std::unique_ptr<ProjectOopMirNode>> children;
};
}
