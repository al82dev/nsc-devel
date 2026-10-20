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
struct MethodMoveBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::optional<std::size_t> sourceLocalOrdinal;
    std::optional<std::size_t> sourceParameterOrdinal;
    // A partial move keeps the containing owner alive while emptying one of its fields or
    // elements.  Retain that root owner separately from the moved slot so semantic validation
    // can enforce borrow exclusivity and dumps can make the decision observable.
    std::optional<std::size_t> partialSourceOwnerLocalOrdinal;
    std::optional<std::size_t> partialSourceOwnerParameterOrdinal;
    // Canonical source-level projection from the named root, e.g. {"Header", "Token"} for
    // `move(packet.Header.Token)`. Empty for whole-value moves and runtime-indexed elements.
    std::vector<std::string> partialFieldPath;
    std::string typeKey;
    // Stage 52: move(...) also accepts a native-array element (e.g. "move(items[i])"),
    // needed by generic collections (Queue<T>.Dequeue(), Stack<T>.Pop(), ...) to extract an
    // owned element out of backing storage. When true, sourceLocalOrdinal/sourceParameterOrdinal
    // are both unset -- HIR lowering re-derives the array/index sub-expressions directly from
    // `syntax->first()` (the IndexExpression being moved).
    //
    // `clearSourceStorage` records whether the SOURCE actually needs load+empty-store treatment.
    // Reference slots use null and move-only value aggregates use their zero/empty state; Copy
    // scalars degrade to a harmless plain read.
    bool sourceIsArrayElement = false;
    // Bloque 1: move(...) also accepts a direct `owned` field access -- `move(holder.slot)`, or
    // `move(slot)` through the implicit this. Same contract as the array-element case: both
    // source ordinals stay unset, HIR lowers the field access as an address and pairs the load
    // with a null-store into the vacated slot, so the container's own discharge (its destruction
    // entry, or a later overwrite's release) sees an empty slot instead of double-freeing the
    // value that was just moved out.
    bool sourceIsOwnedField = false;
    // NS304-T07-F04: `move(it.Current)` transfers a move-only item out of the iterator's item slot
    // exactly once. The HIR lowers it as a load of item@16 + a clear-store (no double drop).
    bool sourceIsIteratorCurrent = false;
    // The source storage must be cleared after the load whenever its bits carry a drop
    // obligation. Reference slots use null as their optimized drop flag; move-only value
    // aggregates are zeroed so every nested owning slot becomes empty as well.
    bool clearSourceStorage = false;
};
}
