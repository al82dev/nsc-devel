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
struct MethodCallCandidate final
{
    std::size_t memberOrdinal = 0;
    bool viable = false;
    std::vector<ProjectImplicitConversion> conversions;
    // One entry per explicit argument, in source evaluation order.  The value is the selected
    // callable's parameter ordinal.
    std::vector<std::size_t> argumentParameterOrdinals;
    std::vector<std::size_t> defaultedParameterOrdinals;
    // NativeSharp 3.0 T01-F02. One bit per explicit source argument. A true bit means the
    // argument is an element of the final params ReadOnlySpan<T>, not a prebuilt span.
    std::vector<bool> parameterPackElements;
    std::optional<std::size_t> parameterPackOrdinal;
    // Source argument ordinals whose fresh owned string is borrowed only for the duration of
    // this call. Used by namespace/free functions as well as member calls.
    std::vector<std::size_t> fullExpressionBorrowArgumentOrdinals;
    bool usesExpandedParameterPack = false;
    bool synthesizesEmptyParameterPack = false;
    std::size_t defaultedParameterCount = 0;
    bool isVariadicCandidate = false;
    std::optional<std::size_t> invalidVariadicArgumentOrdinal;
    std::optional<std::size_t> invalidMoveOnlyArgumentOrdinal;
};
}
