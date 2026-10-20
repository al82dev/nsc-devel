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
#include "nativesharp/Frontend/MethodLambdaCaptureKind.h"
#include "nativesharp/Frontend/MethodLambdaCaptureSourceKind.h"

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
struct MethodLambdaCapture final
{
    MethodLambdaCaptureKind kind = MethodLambdaCaptureKind::Local;
    std::string name;
    std::string typeKey;
    std::optional<std::size_t> localOrdinal;
    std::optional<std::size_t> parameterOrdinal;
    MethodLambdaCaptureSourceKind sourceKind = MethodLambdaCaptureSourceKind::OriginalLocal;
    std::optional<std::size_t> sourceOrdinal;
    // T08-F03. Reference modes share the source cell. Copy/move modes materialize an
    // independent cell when the closure value is created.
    language::CaptureMode mode = language::CaptureMode::Reference;
    // True only for a move capture which transfers a real destruction obligation into the
    // closure environment (RAII pointer/array/string or a value with drop glue).
    bool ownsCapturedValue = false;
    bool clearMovedSource = false;
};
}
